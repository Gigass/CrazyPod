#include "config.h"

#ifdef IPOD_6G

#include "backlight.h"
#include "file.h"
#include "kernel.h"
#include "powermgmt.h"
#include "usb.h"

#include "../../crazypod_artwork.h"
#include "../../crazypod_books.h"
#include "../../crazypod_music.h"
#include "../../crazypod_organizer.h"
#include "../../crazypod_photos.h"
#include "../../crazypod_state.h"
#include "../../crazypod_videos.h"
#include "../app/crazypod_choice_coordinator.h"
#include "../app/crazypod_menu_preview.h"
#include "../features/customize/crazypod_customize_feature.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../features/music/crazypod_music_feature.h"
#include "../features/now_playing/crazypod_now_playing_feature.h"
#include "../presentation/crazypod_overlay_glass.h"
#include "../presentation/crazypod_popup_motion.h"
#include "../presentation/crazypod_preview_motion.h"
#include "crazypod_desktop.h"
#include "crazypod_desktop_native.h"
#include "crazypod_power_prompt.h"
#include "crazypod_system_prompts.h"
#include "crazypod_usb_prompt.h"

#define POWER_HOLD_TICKS (3 * HZ)
#define MEDIA_INVALID_PATH "/.crazypod/cache/media.invalid"

static struct {
    struct crazypod_system_prompts_host host;
    bool storage_active;
} prompts;

static void before_show(void)
{
    if(crazypod_choice_coordinator_visible())
        crazypod_choice_coordinator_dismiss(false);
    if(crazypod_now_playing_overlay_visible())
        crazypod_now_playing_overlay_dismiss(false);
    crazypod_preview_motion_clear_media_refresh();
    if(crazypod_preview_motion_active())
        crazypod_menu_preview_settle();
    backlight_on();
    crazypod_overlay_glass_prepare(false);
    crazypod_desktop_native_invalidate(true);
}

static void dismissed(void)
{
    crazypod_desktop_native_invalidate(true);
}

static void execute(enum shutdown_type type)
{
    if(crazypod_miniapps_feature_is_open()) {
        crazypod_miniapps_feature_reset_input();
        crazypod_miniapps_feature_close();
    }
    crazypod_state_save(true);
    shutdown_hw(type);
}

static void power_execute(int selected)
{
    execute(selected == 0
        ? SHUTDOWN_POWER_OFF : SHUTDOWN_REBOOT);
}

static void configure_power(void)
{
    const struct crazypod_power_prompt_callbacks callbacks = {
        .before_hold_show =
            crazypod_customize_feature_clear_input_holds,
        .before_show = before_show,
        .create_panel = crazypod_overlay_glass_panel,
        .animate_panel = crazypod_popup_animate,
        .dismissed = dismissed,
        .execute = power_execute,
    };

    crazypod_power_prompt_configure(
        crazypod_desktop_screen(), &callbacks);
}

#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
static void usb_before_show(void)
{
    crazypod_power_prompt_dismiss();
    before_show();
}

static void configure_usb(void)
{
    const struct crazypod_usb_prompt_callbacks callbacks = {
        .before_show = usb_before_show,
        .create_panel = crazypod_overlay_glass_panel,
        .animate_panel = crazypod_popup_animate,
        .dismissed = dismissed,
    };

    crazypod_usb_prompt_configure(
        crazypod_desktop_screen(), &callbacks);
}
#endif

void crazypod_system_prompts_configure(
    const struct crazypod_system_prompts_host *host)
{
    if(host != NULL)
        prompts.host = *host;
}

void crazypod_system_prompts_initialize_usb(void)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    crazypod_usb_prompt_register();
#endif
}

void crazypod_system_prompts_set_ui_ready(void)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    crazypod_usb_prompt_set_ui_ready(true);
#endif
}

bool crazypod_system_prompts_storage_active(void)
{
    return prompts.storage_active;
}

bool crazypod_system_prompts_power_visible(void)
{
    return crazypod_power_prompt_visible();
}

bool crazypod_system_prompts_handle_power(
    long base, bool repeated, intptr_t data)
{
    return crazypod_power_prompt_handle_button(
        base, repeated, data);
}

bool crazypod_system_prompts_handle_power_hold(long button)
{
    configure_power();
    return crazypod_power_prompt_handle_play_hold(
        button, prompts.host.now(), POWER_HOLD_TICKS);
}

void crazypod_system_prompts_dismiss_power(void)
{
    crazypod_power_prompt_dismiss();
}

#ifdef SIMULATOR
void crazypod_system_prompts_show_power(void)
{
    configure_power();
    crazypod_power_prompt_show();
}
#endif

bool crazypod_system_prompts_usb_visible(void)
{
    return crazypod_usb_prompt_visible();
}

bool crazypod_system_prompts_handle_usb(
    long base, bool repeated, intptr_t data)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    return crazypod_usb_prompt_handle_button(
        base, repeated, data);
#else
    (void)base;
    (void)repeated;
    (void)data;
    return false;
#endif
}

void crazypod_system_prompts_show_usb(unsigned request)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    configure_usb();
    crazypod_usb_prompt_show(request);
#else
    (void)request;
#endif
}

void crazypod_system_prompts_usb_done(unsigned request)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(crazypod_usb_prompt_matches_request(request) &&
       !crazypod_usb_prompt_data_blocking())
        crazypod_usb_prompt_dismiss();
#else
    (void)request;
#endif
}

void crazypod_system_prompts_usb_connected(intptr_t data)
{
    /*
     * The host may replace any storage-backed catalog while it owns mass
     * storage. Drop every product route now so stale numeric selections
     * cannot target a different book, contact or calendar event afterward.
     * close_product() is a no-op when Home is already visible.
     */
    prompts.host.close_product();
    prompts.storage_active = true;
    crazypod_music_library_schedule_rescan(
        prompts.host.now() + HZ / 2);
    crazypod_artwork_suspend();
    crazypod_photos_suspend();
    crazypod_videos_suspend();
    crazypod_music_cancel_scan();
    crazypod_music_require_catalog_validation();
    crazypod_photos_invalidate_catalog();
    crazypod_videos_invalidate_catalog();
    crazypod_state_save(true);
    /* Every catalog and decoded-media cache is now invalidated. Removing
     * the transaction marker here prevents newly rebuilt photo/video
     * catalogs from being rejected on the next boot when Music was never
     * opened. A crash before this point leaves the marker for boot cleanup. */
    remove(MEDIA_INVALID_PATH);
    usb_acknowledge(SYS_USB_CONNECTED_ACK, data);
}

void crazypod_system_prompts_usb_disconnected(void)
{
    prompts.storage_active = false;
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(crazypod_usb_prompt_data_blocking())
        crazypod_usb_prompt_dismiss();
#endif
    crazypod_books_invalidate_scan();
    crazypod_organizer_invalidate();
    crazypod_artwork_resume();
    crazypod_photos_resume();
    crazypod_videos_resume();
    crazypod_miniapps_feature_rescan();
    crazypod_music_library_schedule_rescan(
        prompts.host.now() + HZ / 2);
}

void crazypod_system_prompts_power_off(void)
{
#if defined(HAVE_USB_POWER) && !defined(USB_NONE)
    if(crazypod_usb_prompt_visible())
        crazypod_usb_prompt_finish(USB_MODE_CHARGE);
#endif
    crazypod_power_prompt_dismiss();
    execute(SHUTDOWN_POWER_OFF);
}

void crazypod_system_prompts_reboot(void)
{
    crazypod_power_prompt_dismiss();
    execute(SHUTDOWN_REBOOT);
}

#endif
