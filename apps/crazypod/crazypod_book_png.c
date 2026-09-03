#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "file.h"
#include "inflate.h"

#include "crazypod_book_png.h"

#define PNG_SIGNATURE_SIZE 8
#define PNG_IHDR_SIZE 13
#define PNG_MAX_CHUNK_SIZE (16u * 1024u * 1024u)
#define PNG_MAX_WORKSPACE (96u * 1024u)

struct png_image {
    uint32_t width;
    uint32_t height;
    unsigned color_type;
    unsigned bit_depth;
    unsigned channels;
    unsigned filter_bpp;
    size_t row_bytes;
    size_t workspace_size;
    unsigned palette_count;
    unsigned char palette[256][4];
    bool transparent_key;
    uint32_t key_r;
    uint32_t key_g;
    uint32_t key_b;
};

struct png_stream {
    int fd;
    uint32_t chunk_remaining;
    uint32_t chunk_type;
    bool have_chunk;
    bool ended;
};

struct png_decode_context {
    const struct png_image *image;
    fb_data *pixels;
    int output_width;
    int output_height;
    unsigned char *current;
    unsigned char *previous;
    size_t row_used;
    uint32_t row_index;
    int output_row;
    bool failed;
};

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static uint32_t read_be32(const unsigned char *data)
{
    return ((uint32_t)data[0] << 24) |
        ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) | data[3];
}

static uint16_t read_be16(const unsigned char *data)
{
    return (uint16_t)(((unsigned)data[0] << 8) | data[1]);
}

static bool skip_bytes(int fd, uint32_t size)
{
    unsigned char buffer[64];

    while(size > 0) {
        uint32_t wanted = size < sizeof(buffer) ? size : sizeof(buffer);

        if(!read_exact(fd, buffer, wanted))
            return false;
        size -= wanted;
    }
    return true;
}

static bool read_chunk_header(
    int fd, uint32_t *length, uint32_t *type)
{
    unsigned char header[8];

    if(!read_exact(fd, header, sizeof(header)))
        return false;
    *length = read_be32(header);
    *type = read_be32(header + 4);
    return *length <= PNG_MAX_CHUNK_SIZE;
}

static bool read_chunk_crc(int fd)
{
    unsigned char crc[4];

    return read_exact(fd, crc, sizeof(crc));
}

static unsigned png_channels(unsigned color_type)
{
    switch(color_type) {
    case 0:
        return 1;
    case 2:
        return 3;
    case 3:
        return 1;
    case 4:
        return 2;
    case 6:
        return 4;
    default:
        return 0;
    }
}

static bool png_bit_depth_valid(unsigned color_type, unsigned bit_depth)
{
    if(color_type == 0 || color_type == 3)
        return bit_depth == 1 || bit_depth == 2 ||
            bit_depth == 4 || bit_depth == 8 ||
            (color_type == 0 && bit_depth == 16);
    return bit_depth == 8 || bit_depth == 16;
}

static unsigned png_filter_bpp(unsigned color_type, unsigned bit_depth)
{
    unsigned bytes = png_channels(color_type) * bit_depth;

    if(bytes < 8)
        return 1;
    return (bytes + 7) / 8;
}

static bool png_finish_header(struct png_image *image)
{
    size_t bits_per_row;
    size_t row_data;
    size_t workspace;

    if(image->width == 0 || image->height == 0 ||
       image->width > INT_MAX || image->height > INT_MAX ||
       image->width > (SIZE_MAX - 7) /
           (image->channels * image->bit_depth))
        return false;
    if(image->color_type == 3 && image->palette_count == 0)
        return false;
    bits_per_row = (size_t)image->width * image->channels *
        image->bit_depth;
    row_data = (bits_per_row + 7) / 8;
    if(row_data > SIZE_MAX - 1)
        return false;
    image->row_bytes = row_data + 1;
    if(image->row_bytes > (SIZE_MAX - inflate_size - inflate_align) / 2)
        return false;
    workspace = image->row_bytes * 2 + inflate_size + inflate_align;
    if(workspace > PNG_MAX_WORKSPACE || inflate_align == 0)
        return false;
    image->filter_bpp = png_filter_bpp(
        image->color_type, image->bit_depth);
    image->workspace_size = workspace;
    return true;
}

static bool png_open_stream(
    const char *path, struct png_image *image, struct png_stream *stream)
{
    static const unsigned char signature[PNG_SIGNATURE_SIZE] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };
    unsigned char data[PNG_IHDR_SIZE];
    unsigned char palette[256 * 3];
    unsigned char transparency[256];
    uint32_t length;
    uint32_t type;
    int fd;
    bool ihdr_seen = false;

    memset(image, 0, sizeof(*image));
    memset(stream, 0, sizeof(*stream));
    fd = open(path, O_RDONLY);
    if(fd < 0 || !read_exact(fd, data, PNG_SIGNATURE_SIZE) ||
       memcmp(data, signature, PNG_SIGNATURE_SIZE) != 0) {
        if(fd >= 0)
            close(fd);
        return false;
    }
    stream->fd = fd;
    while(read_chunk_header(fd, &length, &type)) {
        if(!ihdr_seen && type != 0x49484452u) {
            close(fd);
            return false;
        }
        if(type == 0x49484452u) {
            if(ihdr_seen || length != PNG_IHDR_SIZE ||
               !read_exact(fd, data, sizeof(data)) ||
               !read_chunk_crc(fd)) {
                close(fd);
                return false;
            }
            image->width = read_be32(data);
            image->height = read_be32(data + 4);
            image->bit_depth = data[8];
            image->color_type = data[9];
            image->channels = png_channels(image->color_type);
            ihdr_seen = image->channels != 0 &&
                png_bit_depth_valid(image->color_type, image->bit_depth) &&
                data[10] == 0 && data[11] == 0 && data[12] == 0;
            if(!ihdr_seen) {
                close(fd);
                return false;
            }
        }
        else if(type == 0x504c5445u) {
            unsigned i;

            if(length == 0 || length > sizeof(palette) ||
               length % 3 != 0 ||
               !read_exact(fd, palette, length) ||
               !read_chunk_crc(fd)) {
                close(fd);
                return false;
            }
            image->palette_count = length / 3;
            for(i = 0; i < image->palette_count; ++i) {
                image->palette[i][0] = palette[i * 3];
                image->palette[i][1] = palette[i * 3 + 1];
                image->palette[i][2] = palette[i * 3 + 2];
                image->palette[i][3] = 255;
            }
        }
        else if(type == 0x74524e53u) {
            if(image->color_type == 3) {
                unsigned i;

                if(length > sizeof(transparency) ||
                   !read_exact(fd, transparency, length) ||
                   !read_chunk_crc(fd)) {
                    close(fd);
                    return false;
                }
                for(i = 0; i < length && i < image->palette_count; ++i)
                    image->palette[i][3] = transparency[i];
            }
            else if(image->color_type == 0 && length == 2) {
                if(!read_exact(fd, data, 2) || !read_chunk_crc(fd)) {
                    close(fd);
                    return false;
                }
                image->transparent_key = true;
                image->key_r = read_be16(data);
            }
            else if(image->color_type == 2 && length == 6) {
                if(!read_exact(fd, data, 6) || !read_chunk_crc(fd)) {
                    close(fd);
                    return false;
                }
                image->transparent_key = true;
                image->key_r = read_be16(data);
                image->key_g = read_be16(data + 2);
                image->key_b = read_be16(data + 4);
            }
            else {
                close(fd);
                return false;
            }
        }
        else if(type == 0x49444154u) {
            if(!png_finish_header(image)) {
                close(fd);
                return false;
            }
            stream->chunk_remaining = length;
            stream->chunk_type = type;
            stream->have_chunk = true;
            return true;
        }
        else {
            /* Unknown critical chunks cannot be safely ignored. */
            if((type & 0x20000000u) == 0 ||
               !skip_bytes(fd, length) || !read_chunk_crc(fd)) {
                close(fd);
                return false;
            }
        }
    }
    close(fd);
    return false;
}

static void png_close_stream(struct png_stream *stream)
{
    if(stream->fd >= 0) {
        close(stream->fd);
        stream->fd = -1;
    }
}

static uint32_t png_reader(
    void *block, uint32_t block_size, void *context)
{
    struct png_stream *stream = context;
    unsigned char header[8];

    if(block_size == 0)
        return 0;
    while(!stream->ended) {
        if(stream->have_chunk && stream->chunk_remaining == 0) {
            if(!read_chunk_crc(stream->fd))
                return 0;
            stream->have_chunk = false;
        }
        if(!stream->have_chunk) {
            uint32_t length;
            uint32_t type;

            if(!read_exact(stream->fd, header, sizeof(header))) {
                stream->ended = true;
                return 0;
            }
            length = read_be32(header);
            type = read_be32(header + 4);
            if(length > PNG_MAX_CHUNK_SIZE) {
                stream->ended = true;
                return 0;
            }
            if(type == 0x49454e44u) {
                stream->ended = true;
                return 0;
            }
            stream->chunk_remaining = length;
            stream->chunk_type = type;
            stream->have_chunk = true;
        }
        if(stream->chunk_type != 0x49444154u) {
            if(!skip_bytes(stream->fd, stream->chunk_remaining))
                return 0;
            stream->chunk_remaining = 0;
            continue;
        }
        if(stream->chunk_remaining > 0) {
            uint32_t wanted = block_size < stream->chunk_remaining
                ? block_size : stream->chunk_remaining;
            ssize_t count = read(stream->fd, block, wanted);

            if(count <= 0)
                return 0;
            stream->chunk_remaining -= (uint32_t)count;
            return (uint32_t)count;
        }
    }
    return 0;
}

static unsigned png_sample(
    const unsigned char *row, unsigned index, unsigned bit_depth)
{
    unsigned mask;
    unsigned shift;

    if(bit_depth >= 8)
        return bit_depth == 16
            ? ((unsigned)row[index * 2] << 8) | row[index * 2 + 1]
            : row[index];
    mask = (1u << bit_depth) - 1;
    shift = 8 - bit_depth - (index % (8 / bit_depth)) * bit_depth;
    return (row[index / (8 / bit_depth)] >> shift) & mask;
}

static unsigned png_scale_sample(unsigned value, unsigned bit_depth)
{
    unsigned max = (1u << bit_depth) - 1;

    if(bit_depth == 16)
        return value >> 8;
    return value * 255 / max;
}

static unsigned char png_paeth(unsigned char left,
                               unsigned char up,
                               unsigned char up_left)
{
    int estimate = left + up - up_left;
    int left_distance = estimate - left;
    int up_distance = estimate - up;
    int up_left_distance = estimate - up_left;

    if(left_distance < 0)
        left_distance = -left_distance;
    if(up_distance < 0)
        up_distance = -up_distance;
    if(up_left_distance < 0)
        up_left_distance = -up_left_distance;
    if(left_distance <= up_distance && left_distance <= up_left_distance)
        return left;
    if(up_distance <= up_left_distance)
        return up;
    return up_left;
}

static bool png_unfilter_row(struct png_decode_context *context)
{
    unsigned char *row = context->current;
    const unsigned char *previous = context->previous;
    unsigned filter = row[0];
    size_t i;

    if(filter > 4)
        return false;
    for(i = 1; i < context->image->row_bytes; ++i) {
        unsigned left = i > context->image->filter_bpp
            ? row[i - context->image->filter_bpp] : 0;
        unsigned up = context->row_index > 0 ? previous[i] : 0;
        unsigned up_left = context->row_index > 0 &&
            i > context->image->filter_bpp
                ? previous[i - context->image->filter_bpp] : 0;

        switch(filter) {
        case 1:
            row[i] = (unsigned char)(row[i] + left);
            break;
        case 2:
            row[i] = (unsigned char)(row[i] + up);
            break;
        case 3:
            row[i] = (unsigned char)(row[i] + (left + up) / 2);
            break;
        case 4:
            row[i] = (unsigned char)(row[i] +
                png_paeth((unsigned char)left, (unsigned char)up,
                          (unsigned char)up_left));
            break;
        default:
            break;
        }
    }
    return true;
}

static void png_pixel(const struct png_image *image,
                      const unsigned char *row, unsigned x,
                      unsigned char *red, unsigned char *green,
                      unsigned char *blue, unsigned char *alpha)
{
    unsigned r = 255;
    unsigned g = 255;
    unsigned b = 255;
    unsigned a = 255;
    unsigned sample;

    switch(image->color_type) {
    case 0:
        sample = png_sample(row, x, image->bit_depth);
        r = g = b = png_scale_sample(sample, image->bit_depth);
        if(image->transparent_key && sample == image->key_r)
            a = 0;
        break;
    case 2:
        sample = png_sample(row, x * 3, image->bit_depth);
        r = png_scale_sample(sample, image->bit_depth);
        g = png_scale_sample(
            png_sample(row, x * 3 + 1, image->bit_depth),
            image->bit_depth);
        b = png_scale_sample(
            png_sample(row, x * 3 + 2, image->bit_depth),
            image->bit_depth);
        if(image->transparent_key && sample == image->key_r &&
           png_sample(row, x * 3 + 1, image->bit_depth) == image->key_g &&
           png_sample(row, x * 3 + 2, image->bit_depth) == image->key_b)
            a = 0;
        break;
    case 3:
        sample = png_sample(row, x, image->bit_depth);
        if(sample >= image->palette_count) {
            r = g = b = 255;
            a = 0;
        }
        else {
            r = image->palette[sample][0];
            g = image->palette[sample][1];
            b = image->palette[sample][2];
            a = image->palette[sample][3];
        }
        break;
    case 4:
        sample = png_sample(row, x * 2, image->bit_depth);
        r = g = b = png_scale_sample(sample, image->bit_depth);
        a = png_scale_sample(
            png_sample(row, x * 2 + 1, image->bit_depth),
            image->bit_depth);
        break;
    case 6:
        sample = png_sample(row, x * 4, image->bit_depth);
        r = png_scale_sample(sample, image->bit_depth);
        g = png_scale_sample(
            png_sample(row, x * 4 + 1, image->bit_depth),
            image->bit_depth);
        b = png_scale_sample(
            png_sample(row, x * 4 + 2, image->bit_depth),
            image->bit_depth);
        a = png_scale_sample(
            png_sample(row, x * 4 + 3, image->bit_depth),
            image->bit_depth);
        break;
    default:
        break;
    }
    *red = (unsigned char)((r * a + 255 * (255 - a) + 127) / 255);
    *green = (unsigned char)((g * a + 255 * (255 - a) + 127) / 255);
    *blue = (unsigned char)((b * a + 255 * (255 - a) + 127) / 255);
    *alpha = (unsigned char)a;
}

static bool png_write_row(struct png_decode_context *context)
{
    const unsigned char *row = context->current + 1;
    uint32_t wanted_source;
    unsigned x;

    if(!png_unfilter_row(context) ||
       context->row_index >= context->image->height)
        return false;
    wanted_source = (uint32_t)context->output_row *
        context->image->height / (uint32_t)context->output_height;
    if(context->output_row < context->output_height &&
       context->row_index == wanted_source) {
        for(x = 0; x < (unsigned)context->output_width; ++x) {
            unsigned source_x = x * context->image->width /
                (unsigned)context->output_width;
            unsigned char red;
            unsigned char green;
            unsigned char blue;
            unsigned char alpha;

            png_pixel(context->image, row, source_x,
                      &red, &green, &blue, &alpha);
            context->pixels[context->output_row * context->output_width + x] =
                LCD_RGBPACK(red, green, blue);
        }
        ++context->output_row;
    }
    ++context->row_index;
    {
        unsigned char *swap = context->previous;

        context->previous = context->current;
        context->current = swap;
    }
    return true;
}

static uint32_t png_writer(
    const void *block, uint32_t block_size, void *context)
{
    struct png_decode_context *decoder = context;
    const unsigned char *input = block;

    while(block_size > 0) {
        size_t available = decoder->image->row_bytes - decoder->row_used;
        size_t count = block_size < available ? block_size : available;

        memcpy(decoder->current + decoder->row_used, input, count);
        decoder->row_used += count;
        input += count;
        block_size -= (uint32_t)count;
        if(decoder->row_used == decoder->image->row_bytes) {
            if(!png_write_row(decoder)) {
                decoder->failed = true;
                return 0;
            }
            decoder->row_used = 0;
        }
    }
    return block_size == 0 ? (uint32_t)(input - (const unsigned char *)block)
                           : 0;
}

static unsigned char *align_pointer(void *pointer, size_t alignment)
{
    uintptr_t value = (uintptr_t)pointer;

    value = (value + alignment - 1) & ~(alignment - 1);
    return (unsigned char *)value;
}

bool crazypod_book_png_inspect(
    const char *path, struct crazypod_book_png_info *info)
{
    struct png_image image;
    struct png_stream stream;

    if(path == NULL || info == NULL ||
       !png_open_stream(path, &image, &stream))
        return false;
    png_close_stream(&stream);
    info->width = (int)image.width;
    info->height = (int)image.height;
    info->workspace_size = image.workspace_size;
    return true;
}

bool crazypod_book_png_decode(
    const char *path, int max_width, int max_height,
    fb_data *pixels, int *width, int *height,
    void *workspace, size_t workspace_size)
{
    struct png_image image;
    struct png_stream stream;
    struct png_decode_context decoder;
    unsigned char *inflate_memory;
    uint64_t candidate_height;
    int output_width;
    int output_height;
    int result;

    if(path == NULL || pixels == NULL || width == NULL || height == NULL ||
       workspace == NULL || max_width <= 0 || max_height <= 0 ||
       !png_open_stream(path, &image, &stream))
        return false;
    output_width = image.width < (uint32_t)max_width
        ? (int)image.width : max_width;
    candidate_height = (uint64_t)image.height * output_width /
        image.width;
    output_height = candidate_height > 0 ? (int)candidate_height : 1;
    if(output_height > max_height) {
        output_height = max_height;
        output_width = (int)((uint64_t)image.width * output_height /
                             image.height);
        if(output_width < 1)
            output_width = 1;
    }
    if(workspace_size < image.workspace_size) {
        png_close_stream(&stream);
        return false;
    }
    memset(&decoder, 0, sizeof(decoder));
    decoder.image = &image;
    decoder.pixels = pixels;
    decoder.output_width = output_width;
    decoder.output_height = output_height;
    decoder.current = (unsigned char *)workspace;
    decoder.previous = decoder.current + image.row_bytes;
    inflate_memory = align_pointer(
        decoder.previous + image.row_bytes, inflate_align);
    result = inflate((struct inflate *)inflate_memory, INFLATE_ZLIB,
                     png_reader, &stream, png_writer, &decoder);
    png_close_stream(&stream);
    if(result != 0 || decoder.failed || decoder.row_used != 0 ||
       decoder.row_index != image.height ||
       decoder.output_row != output_height) {
        return false;
    }
    *width = output_width;
    *height = output_height;
    return true;
}

#endif
