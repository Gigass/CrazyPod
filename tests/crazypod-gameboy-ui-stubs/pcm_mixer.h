#include "pcm.h"
#include <stdint.h>
#define PCM_MIXER_CHAN_PLAYBACK 0
#define MIX_AMP_UNITY 0x10000
struct mixer_play_cbs {
    pcm_play_callback_type get_more;
    void (*sampr_changed)(uint32_t sampr);
};
void mixer_channel_play_data(int channel, const struct mixer_play_cbs *callbacks,
                             const void *data, size_t size);
void mixer_channel_stop(int channel);
void mixer_channel_set_amplitude(int channel, unsigned amplitude);
void mixer_set_frequency(unsigned frequency);
unsigned mixer_get_frequency(void);
