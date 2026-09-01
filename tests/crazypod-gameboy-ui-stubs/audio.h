#define AUDIO_STATUS_PLAY 1
#define AUDIO_STATUS_PAUSE 2
struct mp3entry;
int audio_status(void);
struct mp3entry *audio_current_track(void);
void audio_hard_stop(void);
void audio_play(unsigned long elapsed, unsigned long offset);
void audio_pause(void);
