#include <assert.h>

#define IPOD_6G
#define AUDIOHW_HAVE_LINEOUT
#define AUDIOHW_HAVE_BASS
#define AUDIOHW_HAVE_TREBLE
#define TONE_PRESCALER
#define SOUND_VOLUME 0
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct { int volume, balance, bass, treble; } sound_prescaler;
static int hp_l, hp_r, dock_l, dock_r, attenuation;
static int sound_min(int setting) { (void)setting; return -60; }
static int sound_max(int setting) { (void)setting; return 12; }
static int sound_value_to_cb(int setting, int value)
{ (void)setting; return value * 10; }
static void audiohw_set_prescaler(int value) { attenuation = value; }
static void audiohw_set_volume(int l, int r) { hp_l = l; hp_r = r; }
static void audiohw_set_lineout_volume(int l, int r)
{ dock_l = l; dock_r = r; }

#include "sound_volume_under_test.c"

int main(void)
{
    sound_prescaler.volume = -250;
    set_prescaled_volume();
    assert(hp_l == -250 && hp_r == -250);
    assert(dock_l == 10 && dock_r == 10);

    sound_prescaler.volume = -600;
    set_prescaled_volume();
    assert(hp_l == -600 && dock_l == -600 && dock_r == -600);

    sound_prescaler.volume = 100;
    set_prescaled_volume();
    assert(hp_l == 100 && hp_r == 100);
    assert(dock_l == 120 && dock_r == 120);

    sound_prescaler.volume = -250;
    sound_prescaler.balance = 100;
    set_prescaled_volume();
    assert(hp_l == -600 && dock_l == -600 && dock_r == 10);
    sound_prescaler.balance = -100;
    set_prescaled_volume();
    assert(hp_r == -600 && dock_r == -600 && dock_l == 10);

    sound_prescaler.balance = 0;
    sound_prescaler.bass = 120;
    set_prescaled_volume();
    assert(attenuation == 120 && hp_l == -130 && dock_l == 120);
    sound_prescaler.volume = -600;
    set_prescaled_volume();
    assert(dock_l == -600 && dock_r == -600);
    return 0;
}
