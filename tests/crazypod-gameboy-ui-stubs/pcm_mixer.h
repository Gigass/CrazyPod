#include "pcm.h"
#define PCM_MIXER_CHAN_PLAYBACK 0
#define MIX_AMP_UNITY 0x10000
void mixer_channel_play_data(int channel, pcm_play_callback_type callback,
                             const void *data, size_t size);
void mixer_channel_stop(int channel);
void mixer_channel_set_amplitude(int channel, unsigned amplitude);
void mixer_set_frequency(unsigned frequency);
unsigned mixer_get_frequency(void);
