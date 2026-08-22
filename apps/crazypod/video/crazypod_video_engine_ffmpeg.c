#include "config.h"

#if defined(IPOD_6G) && defined(SIMULATOR)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "kernel.h"
#include "lcd.h"
#include "pcm.h"
#include "pcm_mixer.h"

#include "crazypod_video_engine_internal.h"

#define VIDEO_QUEUE_CAPACITY 12
#define VIDEO_BUFFER_SIZE (LCD_WIDTH * LCD_HEIGHT * 3 / 2)
#define AUDIO_QUEUE_CAPACITY 32
#define AUDIO_CHUNK_SIZE 16384
#define AUDIO_OUTPUT_RATE 48000
#define AUDIO_BYTES_PER_FRAME 4
#define ENGINE_PREFILL_MS 500

struct ffmpeg_video_slot {
    uint8_t pixels[VIDEO_BUFFER_SIZE];
    uint32_t pts_ms;
    int width;
    int height;
};

struct ffmpeg_audio_slot {
    uint8_t samples[AUDIO_CHUNK_SIZE];
    size_t size;
    uint32_t frames;
};

struct ffmpeg_engine {
    const struct crazypod_video_engine_host *host;
    AVFormatContext *format;
    AVCodecContext *video_codec;
    AVCodecContext *audio_codec;
    struct SwsContext *scaler;
    SwrContext *resampler;
    AVPacket *packet;
    AVFrame *frame;
    int video_stream;
    int audio_stream;
    int output_width;
    int output_height;
    int output_x;
    int output_y;
    uint32_t duration_ms;
    uint32_t seek_base_ms;
    long wall_start_tick;
    uint32_t paused_position_ms;
    enum crazypod_video_engine_status status;
    bool input_eof;
    bool video_flush_sent;
    bool audio_flush_sent;
    bool video_eof;
    bool audio_eof;
    bool frame_available;
    struct ffmpeg_video_slot video_queue[VIDEO_QUEUE_CAPACITY];
    unsigned video_head;
    unsigned video_tail;
    unsigned video_count;
    uint8_t last_video[VIDEO_BUFFER_SIZE];
    struct ffmpeg_audio_slot audio_queue[AUDIO_QUEUE_CAPACITY];
    unsigned audio_head;
    unsigned audio_tail;
    volatile unsigned audio_count;
    volatile bool audio_active;
    volatile uint32_t audio_active_frames;
    volatile size_t audio_active_size;
    volatile uint64_t audio_completed_frames;
    uint64_t decoded_audio_frames;
    uint32_t decoded_video_frames;
    uint32_t presented_video_frames;
    unsigned old_sample_rate;
};

static struct ffmpeg_engine engine;

static void report_error(const char *operation, int error)
{
    char detail[AV_ERROR_MAX_STRING_SIZE];
    char message[160];

    av_strerror(error, detail, sizeof(detail));
    snprintf(message, sizeof(message), "%s: %s", operation, detail);
    if(engine.host != NULL && engine.host->set_error != NULL)
        engine.host->set_error(message);
}

static void report_message(const char *message)
{
    if(engine.host != NULL && engine.host->set_error != NULL)
        engine.host->set_error(message);
}

static void simulator_media_path(
    char *destination, size_t size, const char *path)
{
    if(path != NULL && path[0] == '/')
        snprintf(destination, size, "simdisk%s", path);
    else
        snprintf(destination, size, "%s", path != NULL ? path : "");
}

static int open_decoder(int stream_index, AVCodecContext **context)
{
    const AVCodec *decoder;
    AVCodecParameters *parameters;
    int result;

    parameters = engine.format->streams[stream_index]->codecpar;
    decoder = avcodec_find_decoder(parameters->codec_id);
    if(decoder == NULL)
        return AVERROR_DECODER_NOT_FOUND;
    *context = avcodec_alloc_context3(decoder);
    if(*context == NULL)
        return AVERROR(ENOMEM);
    result = avcodec_parameters_to_context(*context, parameters);
    if(result < 0)
        return result;
    return avcodec_open2(*context, decoder, NULL);
}

static void calculate_output_geometry(void)
{
    int width = engine.video_codec->width;
    int height = engine.video_codec->height;

    if(width * LCD_HEIGHT > height * LCD_WIDTH) {
        engine.output_width = LCD_WIDTH;
        engine.output_height = height * LCD_WIDTH / width;
    }
    else {
        engine.output_height = LCD_HEIGHT;
        engine.output_width = width * LCD_HEIGHT / height;
    }
    engine.output_width &= ~1;
    engine.output_height &= ~1;
    if(engine.output_width < 2)
        engine.output_width = 2;
    if(engine.output_height < 2)
        engine.output_height = 2;
    engine.output_x = (LCD_WIDTH - engine.output_width) / 2;
    engine.output_y = (LCD_HEIGHT - engine.output_height) / 2;
}

static uint32_t frame_timestamp_ms(const AVFrame *frame, int stream_index)
{
    int64_t timestamp = frame->best_effort_timestamp;

    if(timestamp == AV_NOPTS_VALUE)
        return engine.seek_base_ms;
    timestamp = av_rescale_q(
        timestamp, engine.format->streams[stream_index]->time_base,
        (AVRational){1, 1000});
    if(timestamp < 0)
        return 0;
    if(timestamp > UINT32_MAX)
        return UINT32_MAX;
    return (uint32_t)timestamp;
}

static int queue_video_frame(const AVFrame *frame)
{
    struct ffmpeg_video_slot *slot;
    uint8_t *planes[4];
    int strides[4];
    int result;

    if(engine.video_count >= VIDEO_QUEUE_CAPACITY)
        return AVERROR(EAGAIN);
    slot = &engine.video_queue[engine.video_tail];
    result = av_image_fill_arrays(
        planes, strides, slot->pixels, AV_PIX_FMT_YUV420P,
        engine.output_width, engine.output_height, 1);
    if(result < 0)
        return result;
    engine.scaler = sws_getCachedContext(
        engine.scaler,
        frame->width, frame->height, (enum AVPixelFormat)frame->format,
        engine.output_width, engine.output_height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL);
    if(engine.scaler == NULL)
        return AVERROR(ENOMEM);
    result = sws_scale(
        engine.scaler,
        (const uint8_t * const *)frame->data, frame->linesize,
        0, frame->height, planes, strides);
    if(result <= 0)
        return AVERROR_INVALIDDATA;
    slot->pts_ms = frame_timestamp_ms(frame, engine.video_stream);
    slot->width = engine.output_width;
    slot->height = engine.output_height;
    engine.video_tail =
        (engine.video_tail + 1) % VIDEO_QUEUE_CAPACITY;
    ++engine.video_count;
    ++engine.decoded_video_frames;
    return 0;
}

static void audio_get_more(const void **start, size_t *size)
{
    struct ffmpeg_audio_slot *slot;

    if(engine.audio_active) {
        engine.audio_completed_frames += engine.audio_active_frames;
        if(engine.audio_count > 0) {
            engine.audio_head =
                (engine.audio_head + 1) % AUDIO_QUEUE_CAPACITY;
            --engine.audio_count;
        }
    }
    engine.audio_active = false;
    engine.audio_active_frames = 0;
    engine.audio_active_size = 0;
    if(engine.audio_count == 0) {
        *start = NULL;
        *size = 0;
        return;
    }
    slot = &engine.audio_queue[engine.audio_head];
    engine.audio_active = true;
    engine.audio_active_frames = slot->frames;
    engine.audio_active_size = slot->size;
    *start = slot->samples;
    *size = slot->size;
}

static void start_audio_if_needed(void)
{
    if(engine.audio_count == 0 || engine.status !=
       CRAZYPOD_VIDEO_ENGINE_PLAYING)
        return;
    if(mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) == CHANNEL_STOPPED) {
        mixer_channel_set_amplitude(
            PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
        mixer_channel_play_data(
            PCM_MIXER_CHAN_PLAYBACK, audio_get_more, NULL, 0);
    }
}

static void reset_audio_queue(uint32_t position_ms)
{
    mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    pcm_play_lock();
    engine.audio_head = 0;
    engine.audio_tail = 0;
    engine.audio_count = 0;
    engine.audio_active = false;
    engine.audio_active_frames = 0;
    engine.audio_active_size = 0;
    engine.audio_completed_frames = 0;
    engine.seek_base_ms = position_ms;
    pcm_play_unlock();
}

static int queue_audio_frame(const AVFrame *frame)
{
    struct ffmpeg_audio_slot *slot;
    uint8_t *output;
    int capacity_frames;
    int output_frames;
    int output_size;

    if(engine.audio_count >= AUDIO_QUEUE_CAPACITY)
        return AVERROR(EAGAIN);
    slot = &engine.audio_queue[engine.audio_tail];
    output = slot->samples;
    capacity_frames = AUDIO_CHUNK_SIZE / AUDIO_BYTES_PER_FRAME;
    output_frames = swr_convert(
        engine.resampler, &output, capacity_frames,
        (const uint8_t **)frame->extended_data, frame->nb_samples);
    if(output_frames < 0)
        return output_frames;
    output_size = output_frames * AUDIO_BYTES_PER_FRAME;
    if(output_size <= 0)
        return 0;
    slot->size = (size_t)output_size;
    slot->frames = (uint32_t)output_frames;
    pcm_play_lock();
    engine.audio_tail =
        (engine.audio_tail + 1) % AUDIO_QUEUE_CAPACITY;
    ++engine.audio_count;
    engine.decoded_audio_frames += (uint32_t)output_frames;
    pcm_play_unlock();
    return 0;
}

static int drain_video_decoder(void)
{
    int result;

    while(engine.video_count < VIDEO_QUEUE_CAPACITY) {
        result = avcodec_receive_frame(engine.video_codec, engine.frame);
        if(result == AVERROR(EAGAIN))
            return 0;
        if(result == AVERROR_EOF) {
            engine.video_eof = true;
            return 0;
        }
        if(result < 0)
            return result;
        result = queue_video_frame(engine.frame);
        av_frame_unref(engine.frame);
        if(result < 0)
            return result;
    }
    return 0;
}

static int drain_audio_decoder(void)
{
    int result;

    while(engine.audio_count < AUDIO_QUEUE_CAPACITY) {
        result = avcodec_receive_frame(engine.audio_codec, engine.frame);
        if(result == AVERROR(EAGAIN))
            return 0;
        if(result == AVERROR_EOF) {
            engine.audio_eof = true;
            return 0;
        }
        if(result < 0)
            return result;
        result = queue_audio_frame(engine.frame);
        av_frame_unref(engine.frame);
        if(result < 0)
            return result;
    }
    return 0;
}

static uint32_t audio_queued_ms(void)
{
    unsigned index = engine.audio_head;
    unsigned count = engine.audio_count;
    uint64_t frames = 0;

    while(count-- > 0) {
        frames += engine.audio_queue[index].frames;
        index = (index + 1) % AUDIO_QUEUE_CAPACITY;
    }
    return (uint32_t)(frames * 1000u / AUDIO_OUTPUT_RATE);
}

static uint32_t wall_position_ms(void)
{
    long elapsed = current_tick - engine.wall_start_tick;

    if(elapsed < 0)
        elapsed = 0;
    return engine.seek_base_ms +
        (uint32_t)((uint64_t)elapsed * 1000u / HZ);
}

static uint32_t audio_position_ms(void)
{
    uint64_t frames = engine.audio_completed_frames;

    if(engine.audio_active) {
        size_t waiting = mixer_channel_get_bytes_waiting(
            PCM_MIXER_CHAN_PLAYBACK);

        if(waiting < engine.audio_active_size)
            frames += (engine.audio_active_size - waiting) /
                AUDIO_BYTES_PER_FRAME;
    }
    return engine.seek_base_ms +
        (uint32_t)(frames * 1000u / AUDIO_OUTPUT_RATE);
}

static uint32_t current_position_ms(void)
{
    uint32_t position;

    if(engine.status == CRAZYPOD_VIDEO_ENGINE_PAUSED)
        return engine.paused_position_ms;
    position = engine.audio_stream >= 0
        ? audio_position_ms() : wall_position_ms();
    if(position > engine.duration_ms)
        position = engine.duration_ms;
    return position;
}

static void present_due_video(void)
{
    uint32_t clock = current_position_ms();

    while(engine.video_count > 0) {
        struct ffmpeg_video_slot *slot =
            &engine.video_queue[engine.video_head];
        unsigned char *planes[3];

        if(slot->pts_ms > clock + 10 && engine.frame_available)
            break;
        memcpy(engine.last_video, slot->pixels, VIDEO_BUFFER_SIZE);
        engine.frame_available = true;
        planes[0] = engine.last_video;
        planes[1] = planes[0] + slot->width * slot->height;
        planes[2] = planes[1] + slot->width * slot->height / 4;
        if(engine.host != NULL && engine.host->present_yuv != NULL)
            engine.host->present_yuv(
                planes, 0, 0, slot->width,
                engine.output_x, engine.output_y,
                slot->width, slot->height);
        ++engine.presented_video_frames;
        engine.video_head =
            (engine.video_head + 1) % VIDEO_QUEUE_CAPACITY;
        --engine.video_count;
        if(slot->pts_ms > clock + 10)
            break;
    }
}

static int pump_packets(void)
{
    int iterations = 0;

    while(iterations++ < 96 &&
          engine.video_count < VIDEO_QUEUE_CAPACITY &&
          (engine.audio_stream < 0 ||
           audio_queued_ms() < ENGINE_PREFILL_MS)) {
        int result;

        if(engine.input_eof) {
            if(!engine.video_flush_sent) {
                result = avcodec_send_packet(engine.video_codec, NULL);
                if(result < 0 && result != AVERROR_EOF)
                    return result;
                engine.video_flush_sent = true;
            }
            result = drain_video_decoder();
            if(result < 0 && result != AVERROR(EAGAIN))
                return result;
            if(engine.audio_codec != NULL && !engine.audio_flush_sent) {
                result = avcodec_send_packet(engine.audio_codec, NULL);
                if(result < 0 && result != AVERROR_EOF)
                    return result;
                engine.audio_flush_sent = true;
            }
            if(engine.audio_codec != NULL) {
                result = drain_audio_decoder();
                if(result < 0 && result != AVERROR(EAGAIN))
                    return result;
            }
            break;
        }

        result = av_read_frame(engine.format, engine.packet);
        if(result == AVERROR_EOF) {
            engine.input_eof = true;
            continue;
        }
        if(result < 0)
            return result;
        if(engine.packet->stream_index == engine.video_stream) {
            result = avcodec_send_packet(engine.video_codec, engine.packet);
            av_packet_unref(engine.packet);
            if(result < 0 && result != AVERROR(EAGAIN))
                return result;
            result = drain_video_decoder();
        }
        else if(engine.packet->stream_index == engine.audio_stream) {
            result = avcodec_send_packet(engine.audio_codec, engine.packet);
            av_packet_unref(engine.packet);
            if(result < 0 && result != AVERROR(EAGAIN))
                return result;
            result = drain_audio_decoder();
        }
        else {
            av_packet_unref(engine.packet);
            result = 0;
        }
        if(result < 0 && result != AVERROR(EAGAIN))
            return result;
    }
    return 0;
}

static void release_engine(void)
{
    mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    if(getenv("CRAZYPOD_SIM_VIDEO_DIAGNOSTICS") != NULL &&
       engine.format != NULL) {
        fprintf(stderr,
                "CrazyPod FFmpeg: decoded_video=%u presented_video=%u "
                "decoded_audio_frames=%llu position_ms=%u\n",
                engine.decoded_video_frames,
                engine.presented_video_frames,
                (unsigned long long)engine.decoded_audio_frames,
                current_position_ms());
    }
    if(engine.old_sample_rate != 0)
        mixer_set_frequency(engine.old_sample_rate);
    swr_free(&engine.resampler);
    sws_freeContext(engine.scaler);
    av_frame_free(&engine.frame);
    av_packet_free(&engine.packet);
    avcodec_free_context(&engine.audio_codec);
    avcodec_free_context(&engine.video_codec);
    avformat_close_input(&engine.format);
    memset(&engine, 0, sizeof(engine));
    engine.video_stream = -1;
    engine.audio_stream = -1;
}

static enum crazypod_video_engine_result ffmpeg_open(
    const char *path, const struct crazypod_video_engine_host *host)
{
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    char media_path[1024];
    int result;

    release_engine();
    engine.host = host;
    simulator_media_path(media_path, sizeof(media_path), path);
    result = avformat_open_input(&engine.format, media_path, NULL, NULL);
    if(result < 0) {
        report_error("Could not open video", result);
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_OPEN_FAILED;
    }
    result = avformat_find_stream_info(engine.format, NULL);
    if(result < 0) {
        report_error("Could not read video tracks", result);
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_OPEN_FAILED;
    }
    engine.video_stream = av_find_best_stream(
        engine.format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(engine.video_stream < 0) {
        report_message("This file has no video track");
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED;
    }
    if(engine.format->streams[engine.video_stream]->codecpar->codec_id !=
       AV_CODEC_ID_H264) {
        report_message("Only H.264 video is supported");
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED;
    }
    result = open_decoder(engine.video_stream, &engine.video_codec);
    if(result < 0) {
        report_error("Could not start H.264 decoder", result);
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_ERROR;
    }
    engine.audio_stream = av_find_best_stream(
        engine.format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if(engine.audio_stream >= 0) {
        if(engine.format->streams[engine.audio_stream]->codecpar->codec_id !=
           AV_CODEC_ID_AAC) {
            report_message("Only AAC audio is supported");
            release_engine();
            return CRAZYPOD_VIDEO_ENGINE_UNSUPPORTED;
        }
        result = open_decoder(engine.audio_stream, &engine.audio_codec);
        if(result < 0) {
            report_error("Could not start AAC decoder", result);
            release_engine();
            return CRAZYPOD_VIDEO_ENGINE_ERROR;
        }
        result = swr_alloc_set_opts2(
            &engine.resampler, &stereo, AV_SAMPLE_FMT_S16,
            AUDIO_OUTPUT_RATE, &engine.audio_codec->ch_layout,
            engine.audio_codec->sample_fmt, engine.audio_codec->sample_rate,
            0, NULL);
        if(result < 0 || engine.resampler == NULL ||
           swr_init(engine.resampler) < 0) {
            report_message("Could not configure AAC audio output");
            release_engine();
            return CRAZYPOD_VIDEO_ENGINE_ERROR;
        }
    }
    else {
        engine.audio_stream = -1;
        engine.audio_eof = true;
    }
    engine.packet = av_packet_alloc();
    engine.frame = av_frame_alloc();
    if(engine.packet == NULL || engine.frame == NULL) {
        release_engine();
        return CRAZYPOD_VIDEO_ENGINE_NO_MEMORY;
    }
    calculate_output_geometry();
    if(engine.format->duration > 0)
        engine.duration_ms = (uint32_t)FFMIN(
            engine.format->duration / (AV_TIME_BASE / 1000), UINT32_MAX);
    engine.old_sample_rate = mixer_get_frequency();
    mixer_set_frequency(AUDIO_OUTPUT_RATE);
    engine.status = CRAZYPOD_VIDEO_ENGINE_STOPPED;
    return CRAZYPOD_VIDEO_ENGINE_OK;
}

static enum crazypod_video_engine_result ffmpeg_play(void)
{
    if(engine.format == NULL)
        return CRAZYPOD_VIDEO_ENGINE_ERROR;
    engine.wall_start_tick = current_tick;
    engine.status = CRAZYPOD_VIDEO_ENGINE_PLAYING;
    return CRAZYPOD_VIDEO_ENGINE_OK;
}

static void ffmpeg_service(void)
{
    int result;

    if(engine.status != CRAZYPOD_VIDEO_ENGINE_PLAYING)
        return;
    result = pump_packets();
    if(result < 0) {
        report_error("Video decoding failed", result);
        engine.status = CRAZYPOD_VIDEO_ENGINE_STATUS_FAILED;
        return;
    }
    start_audio_if_needed();
    present_due_video();
    if(engine.input_eof && engine.video_eof && engine.audio_eof &&
       engine.video_count == 0 && engine.audio_count == 0 &&
       !engine.audio_active)
        engine.status = CRAZYPOD_VIDEO_ENGINE_ENDED;
}

static void ffmpeg_pause(void)
{
    if(engine.status != CRAZYPOD_VIDEO_ENGINE_PLAYING)
        return;
    engine.paused_position_ms = current_position_ms();
    mixer_channel_play_pause(PCM_MIXER_CHAN_PLAYBACK, false);
    engine.status = CRAZYPOD_VIDEO_ENGINE_PAUSED;
}

static void ffmpeg_resume(void)
{
    if(engine.status != CRAZYPOD_VIDEO_ENGINE_PAUSED)
        return;
    if(engine.audio_stream < 0)
        engine.seek_base_ms = engine.paused_position_ms;
    engine.wall_start_tick = current_tick;
    engine.status = CRAZYPOD_VIDEO_ENGINE_PLAYING;
    if(mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) == CHANNEL_PAUSED)
        mixer_channel_play_pause(PCM_MIXER_CHAN_PLAYBACK, true);
    else
        start_audio_if_needed();
}

static void ffmpeg_seek(uint32_t position_ms)
{
    int64_t timestamp;
    int result;
    bool was_paused =
        engine.status == CRAZYPOD_VIDEO_ENGINE_PAUSED;

    if(engine.format == NULL)
        return;
    if(position_ms > engine.duration_ms)
        position_ms = engine.duration_ms;
    timestamp = av_rescale_q(
        position_ms, (AVRational){1, 1000},
        engine.format->streams[engine.video_stream]->time_base);
    result = av_seek_frame(
        engine.format, engine.video_stream, timestamp, AVSEEK_FLAG_BACKWARD);
    if(result < 0) {
        report_error("Video seek failed", result);
        return;
    }
    avcodec_flush_buffers(engine.video_codec);
    if(engine.audio_codec != NULL)
        avcodec_flush_buffers(engine.audio_codec);
    reset_audio_queue(position_ms);
    engine.video_head = 0;
    engine.video_tail = 0;
    engine.video_count = 0;
    engine.input_eof = false;
    engine.video_flush_sent = false;
    engine.audio_flush_sent = false;
    engine.video_eof = false;
    engine.audio_eof = engine.audio_codec == NULL;
    engine.wall_start_tick = current_tick;
    engine.paused_position_ms = position_ms;
    engine.status = was_paused
        ? CRAZYPOD_VIDEO_ENGINE_PAUSED
        : CRAZYPOD_VIDEO_ENGINE_PLAYING;
}

static void ffmpeg_redraw(void)
{
    unsigned char *planes[3];

    if(!engine.frame_available || engine.host == NULL ||
       engine.host->present_yuv == NULL)
        return;
    planes[0] = engine.last_video;
    planes[1] = planes[0] +
        engine.output_width * engine.output_height;
    planes[2] = planes[1] +
        engine.output_width * engine.output_height / 4;
    engine.host->present_yuv(
        planes, 0, 0, engine.output_width,
        engine.output_x, engine.output_y,
        engine.output_width, engine.output_height);
}

static void ffmpeg_close(void)
{
    release_engine();
}

static enum crazypod_video_engine_status ffmpeg_status(void)
{
    return engine.status;
}

static uint32_t ffmpeg_position_ms(void)
{
    return current_position_ms();
}

static uint32_t ffmpeg_duration_ms(void)
{
    return engine.duration_ms;
}

const struct crazypod_video_engine_ops *
crazypod_video_engine_ffmpeg_ops(void)
{
    static const struct crazypod_video_engine_ops ops = {
        .open = ffmpeg_open,
        .play = ffmpeg_play,
        .service = ffmpeg_service,
        .pause = ffmpeg_pause,
        .resume = ffmpeg_resume,
        .seek = ffmpeg_seek,
        .redraw = ffmpeg_redraw,
        .close = ffmpeg_close,
        .status = ffmpeg_status,
        .position_ms = ffmpeg_position_ms,
        .duration_ms = ffmpeg_duration_ms,
    };

    return &ops;
}

#endif
