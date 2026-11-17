/*
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Fil-C: vendored copy of gnome-settings-daemon's data/gsd-enums.h
 * (from gnome-settings-daemon 46.0), which gnome-settings-daemon
 * installs as <gnome-settings-daemon/gsd-enums.h>.  gnome-settings-daemon
 * is not part of the pizlonated system, so this header is shipped inside
 * gnome-control-center instead (see meson.build: gnome_settings_dep).
 */

#ifndef __gsd_enums_h__
#define __gsd_enums_h__

typedef enum
{
  GSD_FONT_ANTIALIASING_MODE_NONE,
  GSD_FONT_ANTIALIASING_MODE_GRAYSCALE,
  GSD_FONT_ANTIALIASING_MODE_RGBA
} GsdFontAntialiasingMode;

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

typedef enum
{
  GSD_SMARTCARD_REMOVAL_ACTION_NONE,
  GSD_SMARTCARD_REMOVAL_ACTION_LOCK_SCREEN,
  GSD_SMARTCARD_REMOVAL_ACTION_FORCE_LOGOUT
} GsdSmartcardRemovalAction;

typedef enum
{
  GSD_TOUCHPAD_SCROLL_METHOD_DISABLED,
  GSD_TOUCHPAD_SCROLL_METHOD_EDGE_SCROLLING,
  GSD_TOUCHPAD_SCROLL_METHOD_TWO_FINGER_SCROLLING
} GsdTouchpadScrollMethod;

typedef enum
{
  GSD_BELL_MODE_ON,
  GSD_BELL_MODE_OFF,
  GSD_BELL_MODE_CUSTOM
} GsdBellMode;

typedef enum
{
  GSD_TOUCHPAD_HANDEDNESS_RIGHT,
  GSD_TOUCHPAD_HANDEDNESS_LEFT,
  GSD_TOUCHPAD_HANDEDNESS_MOUSE
} GsdTouchpadHandedness;

typedef enum
{
  GSD_WACOM_ROTATION_NONE,
  GSD_WACOM_ROTATION_CW,
  GSD_WACOM_ROTATION_CCW,
  GSD_WACOM_ROTATION_HALF
} GsdWacomRotation;

typedef enum
{
  GSD_WACOM_ACTION_TYPE_NONE,
  GSD_WACOM_ACTION_TYPE_CUSTOM,
  GSD_WACOM_ACTION_TYPE_SWITCH_MONITOR,
  GSD_WACOM_ACTION_TYPE_HELP
} GsdWacomActionType;

typedef enum
{
  GSD_POWER_ACTION_BLANK,
  GSD_POWER_ACTION_SUSPEND,
  GSD_POWER_ACTION_SHUTDOWN,
  GSD_POWER_ACTION_HIBERNATE,
  GSD_POWER_ACTION_INTERACTIVE,
  GSD_POWER_ACTION_NOTHING,
  GSD_POWER_ACTION_LOGOUT
} GsdPowerActionType;

typedef enum
{
  GSD_POWER_BUTTON_ACTION_NOTHING,
  GSD_POWER_BUTTON_ACTION_SUSPEND,
  GSD_POWER_BUTTON_ACTION_HIBERNATE,
  GSD_POWER_BUTTON_ACTION_INTERACTIVE
} GsdPowerButtonActionType;

typedef enum
{
  GSD_UPDATE_TYPE_ALL,
  GSD_UPDATE_TYPE_SECURITY,
  GSD_UPDATE_TYPE_NONE
} GsdUpdateType;

typedef enum
{
  GSD_NUM_LOCK_STATE_UNKNOWN,
  GSD_NUM_LOCK_STATE_ON,
  GSD_NUM_LOCK_STATE_OFF
} GsdNumLockState;

#endif /* __gsd_enums_h__ */
