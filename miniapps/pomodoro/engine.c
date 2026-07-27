#include "engine.h"

#define POMODORO_DEFAULT_FOCUS_MINUTES 25u
#define POMODORO_DEFAULT_SHORT_MINUTES 5u
#define POMODORO_DEFAULT_LONG_MINUTES 15u
#define POMODORO_DEFAULT_ROUNDS 4u

static void bytes_clear(void *buffer, size_t size)
{
    uint8_t *bytes = buffer;
    size_t index;

    for(index = 0; index < size; ++index)
        bytes[index] = 0;
}

static uint32_t checksum_bytes(const void *buffer, size_t size)
{
    const uint8_t *bytes = buffer;
    uint32_t hash = 2166136261u;
    size_t index;

    for(index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t phase_duration_for(const struct pomodoro_config *config,
                                   enum pomodoro_phase phase)
{
    uint32_t minutes;

    if(phase == POMODORO_PHASE_SHORT_BREAK)
        minutes = config->short_break_minutes;
    else if(phase == POMODORO_PHASE_LONG_BREAK)
        minutes = config->long_break_minutes;
    else
        minutes = config->focus_minutes;
    return minutes * 60u;
}

static uint32_t next_token(struct pomodoro_model *model)
{
    uint32_t token = model->next_alarm_token;

    if(token == 0)
        token = 1;
    model->next_alarm_token = token + 1u;
    if(model->next_alarm_token == 0)
        model->next_alarm_token = 1;
    return token;
}

void pomodoro_model_init(struct pomodoro_model *model)
{
    if(model == NULL)
        return;
    bytes_clear(model, sizeof(*model));
    model->config.focus_minutes = POMODORO_DEFAULT_FOCUS_MINUTES;
    model->config.short_break_minutes =
        POMODORO_DEFAULT_SHORT_MINUTES;
    model->config.long_break_minutes = POMODORO_DEFAULT_LONG_MINUTES;
    model->config.rounds = POMODORO_DEFAULT_ROUNDS;
    model->phase = POMODORO_PHASE_FOCUS;
    model->run_state = POMODORO_READY;
    model->remaining_seconds = phase_duration_for(
        &model->config, POMODORO_PHASE_FOCUS);
    model->next_alarm_token = 1;
}

bool pomodoro_config_valid(const struct pomodoro_config *config)
{
    if(config == NULL)
        return false;
    return config->focus_minutes >= POMODORO_MIN_MINUTES &&
           config->focus_minutes <= POMODORO_MAX_MINUTES &&
           config->short_break_minutes >= POMODORO_MIN_MINUTES &&
           config->short_break_minutes <= POMODORO_MAX_MINUTES &&
           config->long_break_minutes >= POMODORO_MIN_MINUTES &&
           config->long_break_minutes <= POMODORO_MAX_MINUTES &&
           config->rounds >= POMODORO_MIN_ROUNDS &&
           config->rounds <= POMODORO_MAX_ROUNDS;
}

bool pomodoro_model_set_config(struct pomodoro_model *model,
                               const struct pomodoro_config *config)
{
    uint16_t maximum_focuses;

    if(model == NULL || !pomodoro_config_valid(config) ||
       model->run_state != POMODORO_READY)
        return false;

    model->config = *config;
    maximum_focuses = model->phase == POMODORO_PHASE_LONG_BREAK
        ? config->rounds : (uint16_t)(config->rounds - 1u);
    if(model->focuses_in_cycle > maximum_focuses)
        model->focuses_in_cycle = maximum_focuses;
    model->remaining_seconds = phase_duration_for(
        config, (enum pomodoro_phase)model->phase);
    return true;
}

uint32_t pomodoro_phase_duration(const struct pomodoro_model *model)
{
    if(model == NULL ||
       model->phase > POMODORO_PHASE_LONG_BREAK)
        return 0;
    return phase_duration_for(&model->config,
                              (enum pomodoro_phase)model->phase);
}

uint32_t pomodoro_remaining_at(const struct pomodoro_model *model,
                               uint32_t now_epoch)
{
    int32_t delta;

    if(model == NULL)
        return 0;
    if(model->run_state != POMODORO_RUNNING)
        return model->remaining_seconds;
    delta = (int32_t)(model->deadline_epoch - now_epoch);
    return delta > 0 ? (uint32_t)delta : 0;
}

unsigned pomodoro_round_number(const struct pomodoro_model *model)
{
    unsigned round;

    if(model == NULL || model->config.rounds == 0)
        return 0;
    if(model->phase == POMODORO_PHASE_FOCUS)
        round = (unsigned)model->focuses_in_cycle + 1u;
    else
        round = model->focuses_in_cycle == 0
            ? 1u : (unsigned)model->focuses_in_cycle;
    if(round > model->config.rounds)
        round = model->config.rounds;
    return round;
}

enum pomodoro_phase
pomodoro_next_phase(const struct pomodoro_model *model)
{
    if(model == NULL)
        return POMODORO_PHASE_FOCUS;
    if(model->phase != POMODORO_PHASE_FOCUS)
        return POMODORO_PHASE_FOCUS;
    if((unsigned)model->focuses_in_cycle + 1u >= model->config.rounds)
        return POMODORO_PHASE_LONG_BREAK;
    return POMODORO_PHASE_SHORT_BREAK;
}

bool pomodoro_start(struct pomodoro_model *model, uint32_t now_epoch)
{
    if(model == NULL || model->run_state != POMODORO_READY ||
       model->remaining_seconds == 0)
        return false;
    model->alarm_token = next_token(model);
    model->deadline_epoch = now_epoch + model->remaining_seconds;
    model->run_state = POMODORO_RUNNING;
    return true;
}

bool pomodoro_pause(struct pomodoro_model *model, uint32_t now_epoch)
{
    uint32_t remaining;

    if(model == NULL || model->run_state != POMODORO_RUNNING)
        return false;
    remaining = pomodoro_remaining_at(model, now_epoch);
    if(remaining == 0) {
        model->remaining_seconds = 0;
        model->deadline_epoch = 0;
        model->run_state = POMODORO_COMPLETE;
    }
    else {
        model->remaining_seconds = remaining;
        model->deadline_epoch = 0;
        model->alarm_token = 0;
        model->run_state = POMODORO_PAUSED;
    }
    return true;
}

bool pomodoro_resume(struct pomodoro_model *model, uint32_t now_epoch)
{
    if(model == NULL || model->run_state != POMODORO_PAUSED ||
       model->remaining_seconds == 0)
        return false;
    model->alarm_token = next_token(model);
    model->deadline_epoch = now_epoch + model->remaining_seconds;
    model->run_state = POMODORO_RUNNING;
    return true;
}

bool pomodoro_tick(struct pomodoro_model *model, uint32_t now_epoch)
{
    if(model == NULL || model->run_state != POMODORO_RUNNING ||
       pomodoro_remaining_at(model, now_epoch) != 0)
        return false;
    model->remaining_seconds = 0;
    model->deadline_epoch = 0;
    model->run_state = POMODORO_COMPLETE;
    return true;
}

bool pomodoro_alarm_fired(struct pomodoro_model *model, uint32_t token)
{
    if(model == NULL || model->run_state != POMODORO_RUNNING ||
       token == 0 || token != model->alarm_token)
        return false;
    model->remaining_seconds = 0;
    model->deadline_epoch = 0;
    model->run_state = POMODORO_COMPLETE;
    return true;
}

bool pomodoro_reset(struct pomodoro_model *model)
{
    if(model == NULL)
        return false;
    model->run_state = POMODORO_READY;
    model->deadline_epoch = 0;
    model->alarm_token = 0;
    model->remaining_seconds = pomodoro_phase_duration(model);
    return model->remaining_seconds != 0;
}

bool pomodoro_skip(struct pomodoro_model *model)
{
    enum pomodoro_phase next_phase;

    if(model == NULL ||
       (model->run_state != POMODORO_RUNNING &&
        model->run_state != POMODORO_PAUSED))
        return false;

    if(model->phase == POMODORO_PHASE_FOCUS)
        next_phase = POMODORO_PHASE_SHORT_BREAK;
    else {
        next_phase = POMODORO_PHASE_FOCUS;
        if(model->phase == POMODORO_PHASE_LONG_BREAK)
            model->focuses_in_cycle = 0;
    }
    model->phase = (uint8_t)next_phase;
    model->run_state = POMODORO_READY;
    model->deadline_epoch = 0;
    model->alarm_token = 0;
    model->remaining_seconds = pomodoro_phase_duration(model);
    return true;
}

bool pomodoro_advance(struct pomodoro_model *model)
{
    enum pomodoro_phase next_phase;

    if(model == NULL || model->run_state != POMODORO_COMPLETE)
        return false;

    if(model->phase == POMODORO_PHASE_FOCUS) {
        if(model->focuses_in_cycle < model->config.rounds)
            ++model->focuses_in_cycle;
        next_phase = model->focuses_in_cycle >= model->config.rounds
            ? POMODORO_PHASE_LONG_BREAK
            : POMODORO_PHASE_SHORT_BREAK;
    }
    else {
        next_phase = POMODORO_PHASE_FOCUS;
        if(model->phase == POMODORO_PHASE_LONG_BREAK)
            model->focuses_in_cycle = 0;
    }
    model->phase = (uint8_t)next_phase;
    model->run_state = POMODORO_READY;
    model->deadline_epoch = 0;
    model->alarm_token = 0;
    model->remaining_seconds = pomodoro_phase_duration(model);
    return true;
}

void pomodoro_pack(const struct pomodoro_model *model,
                   struct pomodoro_disk_state *disk)
{
    if(model == NULL || disk == NULL)
        return;
    bytes_clear(disk, sizeof(*disk));
    disk->magic = POMODORO_STATE_MAGIC;
    disk->version = POMODORO_STATE_VERSION;
    disk->size = (uint16_t)sizeof(*disk);
    disk->focus_minutes = model->config.focus_minutes;
    disk->short_break_minutes = model->config.short_break_minutes;
    disk->long_break_minutes = model->config.long_break_minutes;
    disk->rounds = model->config.rounds;
    disk->phase = model->phase;
    disk->run_state = model->run_state;
    disk->focuses_in_cycle = model->focuses_in_cycle;
    disk->deadline_epoch = model->deadline_epoch;
    disk->remaining_seconds = model->remaining_seconds;
    disk->alarm_token = model->alarm_token;
    disk->next_alarm_token = model->next_alarm_token;
    disk->checksum = checksum_bytes(disk, sizeof(*disk));
}

bool pomodoro_unpack(struct pomodoro_model *model,
                     const struct pomodoro_disk_state *disk,
                     size_t size)
{
    struct pomodoro_disk_state copy;
    struct pomodoro_model loaded;
    uint32_t checksum;
    uint32_t duration;

    if(model == NULL || disk == NULL ||
       size != sizeof(*disk) ||
       disk->magic != POMODORO_STATE_MAGIC ||
       disk->version != POMODORO_STATE_VERSION ||
       disk->size != sizeof(*disk))
        return false;

    copy = *disk;
    checksum = copy.checksum;
    copy.checksum = 0;
    if(checksum != checksum_bytes(&copy, sizeof(copy)))
        return false;

    bytes_clear(&loaded, sizeof(loaded));
    loaded.config.focus_minutes = disk->focus_minutes;
    loaded.config.short_break_minutes = disk->short_break_minutes;
    loaded.config.long_break_minutes = disk->long_break_minutes;
    loaded.config.rounds = disk->rounds;
    loaded.phase = disk->phase;
    loaded.run_state = disk->run_state;
    loaded.focuses_in_cycle = disk->focuses_in_cycle;
    loaded.deadline_epoch = disk->deadline_epoch;
    loaded.remaining_seconds = disk->remaining_seconds;
    loaded.alarm_token = disk->alarm_token;
    loaded.next_alarm_token = disk->next_alarm_token;

    if(!pomodoro_config_valid(&loaded.config) ||
       loaded.phase > POMODORO_PHASE_LONG_BREAK ||
       loaded.run_state > POMODORO_COMPLETE ||
       loaded.focuses_in_cycle > loaded.config.rounds)
        return false;

    duration = pomodoro_phase_duration(&loaded);
    if(duration == 0 || loaded.remaining_seconds > duration)
        return false;
    if(loaded.run_state == POMODORO_RUNNING &&
       loaded.alarm_token == 0)
        return false;
    if(loaded.run_state == POMODORO_READY)
        loaded.remaining_seconds = duration;
    else if(loaded.run_state == POMODORO_COMPLETE)
        loaded.remaining_seconds = 0;
    if(loaded.next_alarm_token == 0)
        loaded.next_alarm_token = 1;

    *model = loaded;
    return true;
}

