#include "config.h"

#ifdef IPOD_6G

#include "adc.h"
#include "audio.h"
#include "backlight.h"
#include "bootdata.h"
#include "button.h"
#include "core_alloc.h"
#include "devicedata.h"
#include "disk.h"
#include "file_internal.h"
#include "gcc_extensions.h"
#include "i2c.h"
#include "kernel-internal.h"
#include "lcd.h"
#include "panic.h"
#include "pathfuncs.h"
#include "pcm.h"
#include "power.h"
#include "powermgmt.h"
#include "rtc.h"
#ifdef HAVE_SERIAL
#include "serial.h"
#endif
#include "storage.h"
#include "system.h"
#include "usb.h"

#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif

#include "dsp_core.h"
#include "playlist.h"

#ifdef HAVE_HARDWARE_CLICK
#include "piezo.h"
#endif

#if (CONFIG_PLATFORM & PLATFORM_SDL)
#include "sim_tasks.h"
#include "system-sdl.h"
#endif

#include "crazypod_audio_shims.h"
#include "crazypod_appearance.h"
#include "crazypod_books.h"
#include "crazypod_lcd.h"
#include "crazypod_music.h"
#include "crazypod_notes.h"
#include "crazypod_organizer.h"
#include "crazypod_presets.h"
#include "crazypod_runtime_limits.h"
#include "crazypod_state.h"
#include "crazypod_ui.h"
#include "crazypod_workouts.h"

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)

static void crazypod_platform_init(void)
{
    system_init();
    core_allocator_init();
    kernel_init();
    enable_irq();

    lcd_init();
    crazypod_lcd_show_boot_logo();
    button_init();
    powermgmt_init();
    backlight_init();
    backlight_set_brightness(DEFAULT_BRIGHTNESS_SETTING);
    backlight_set_timeout(30);
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_set_timeout_plugged(60);
#endif
    backlight_on();

#ifdef HAVE_MULTIVOLUME
    init_volume_names();
#endif
#ifdef SIMULATOR
    sim_tasks_init();
#endif

    storage_init();
    init_battery_tables();
    pcm_init();
    dsp_init();
    crazypod_audio_settings_init();
    playlist_init();
    audio_init();
    crazypod_state_load();
    crazypod_appearance_load();
    crazypod_presets_load();
    crazypod_notes_init();
    crazypod_books_init();
    crazypod_workouts_init();
    crazypod_music_init();

#ifndef USB_NONE
    usb_init();
    crazypod_ui_usb_prompt_init();
    usb_start_monitoring();
#endif
}

int main(int argc, char *argv[])
{
    sys_handle_argv(argc, argv);
    crazypod_platform_init();
    crazypod_ui_run();
}

#else

static unsigned char
    crazypod_ui_thread_stack[CRAZYPOD_UI_THREAD_STACK_SIZE]
    CACHEALIGN_ATTR;

static void crazypod_ui_thread_entry(void)
{
    crazypod_ui_run();
    panicf("CrazyPod UI exited");
}

static void crazypod_platform_init(void)
{
    int mount_count;

    system_init();
    core_allocator_init();
    kernel_init();

#ifdef HAVE_BOOTDATA
    verify_boot_data();
#endif
#ifdef HAVE_DEVICEDATA
    verify_device_data();
#endif

    filesystem_init();
    set_cpu_frequency(CPUFREQ_NORMAL);
    cpu_boost(true);

    i2c_init();
    power_init();
    enable_irq();
#ifdef CPU_ARM_CLASSIC
    enable_fiq();
#endif

    lcd_init();
    crazypod_lcd_show_boot_logo();
#if CONFIG_RTC
    rtc_init();
#endif
    adc_init();

#ifndef USB_NONE
    usb_init();
#endif
    backlight_init();
    backlight_set_brightness(DEFAULT_BRIGHTNESS_SETTING);
    backlight_set_timeout(30);
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_set_timeout_plugged(60);
#endif
    backlight_on();
    button_init();
#ifdef IPOD_ACCESSORY_PROTOCOL
    iap_setup(0);
#endif
#ifdef HAVE_SERIAL
    serial_setup();
#endif
    powermgmt_init();
#ifdef HAVE_ACCESSORY_SUPPLY
    accessory_supply_set(true);
#endif
#ifdef HAVE_HARDWARE_CLICK
    piezo_init();
#endif

    if(storage_init() != 0)
        panicf("storage init failed");

    mount_count = disk_mount_all();
    if(mount_count <= 0)
        panicf("no filesystem: %d", mount_count);

    init_battery_tables();
    pcm_init();
    dsp_init();
    crazypod_audio_settings_init();
    playlist_init();
    audio_init();
    crazypod_state_load();
    crazypod_appearance_load();
    crazypod_presets_load();
    crazypod_notes_init();
    crazypod_books_init();
    crazypod_workouts_init();
    crazypod_music_init();

#ifndef USB_NONE
    crazypod_ui_usb_prompt_init();
    usb_start_monitoring();
#endif
    cpu_boost(false);
}

int main(void) NORETURN_ATTR;
int main(void)
{
    unsigned int ui_thread;

    crazypod_platform_init();
    ui_thread = create_thread(
        crazypod_ui_thread_entry,
        crazypod_ui_thread_stack,
        sizeof(crazypod_ui_thread_stack),
        0, "crazypod_ui"
        IF_PRIO(, PRIORITY_USER_INTERFACE)
        IF_COP(, CPU));
    if(ui_thread == 0)
        panicf("CrazyPod UI thread");
    thread_wait(ui_thread);
    panicf("CrazyPod UI stopped");
}

#endif

#endif
