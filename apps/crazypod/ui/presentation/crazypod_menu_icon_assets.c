#include "config.h"

#ifdef IPOD_6G

#include "crazypod_menu_icon_assets.h"

/* Generated from Material Symbols Rounded. See the generator for sources. */
#include "crazypod_menu_icon_data.inc"

const lv_image_dsc_t *crazypod_menu_icon_asset(
    enum crazypod_menu_icon icon)
{
    if(icon <= CRAZYPOD_MENU_ICON_NONE ||
       icon >= CRAZYPOD_MENU_ICON_COUNT)
        return NULL;
    return crazypod_menu_icon_assets[icon];
}

#endif
