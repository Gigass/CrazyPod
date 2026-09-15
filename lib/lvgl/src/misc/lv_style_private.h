/**
 * @file lv_style_private.h
 *
 */

#ifndef LV_STYLE_PRIVATE_H
#define LV_STYLE_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_style.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Set once any style anywhere carries LV_STYLE_BLUR_RADIUS or
 * LV_STYLE_DROP_SHADOW_OPA. Invalidation has to walk every widget on the
 * display to find blurred widgets overlapping the dirty area; a UI that
 * uses neither can skip those walks, which on a slow CPU with a busy
 * screen is the difference between a few microseconds per invalidation
 * and a few milliseconds. Never cleared: once a blur exists the walks are
 * needed for the rest of the session.
 */
extern bool lv_style_blur_in_use;

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_STYLE_PRIVATE_H*/
