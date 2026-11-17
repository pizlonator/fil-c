/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010 Intel, Inc
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author(s):
 *   Thomas Wood <thomas.wood@intel.com>
 *   Rodrigo Moya <rodrigo@gnome.org>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#pragma once


#define DPI_FACTOR_LARGE             1.25
#define DPI_FACTOR_NORMAL            1.0
#define HIGH_CONTRAST_THEME          "HighContrast"

/* shell settings */
#define A11Y_SETTINGS                "org.gnome.desktop.a11y"
#define KEY_ALWAYS_SHOW_STATUS       "always-show-universal-access-status"

/* a11y interface settings */
#define A11Y_INTERFACE_SETTINGS      "org.gnome.desktop.a11y.interface"
#define KEY_HIGH_CONTRAST            "high-contrast"
#define KEY_STATUS_SHAPES            "show-status-shapes"

/* interface settings */
#define INTERFACE_SETTINGS           "org.gnome.desktop.interface"
#define KEY_CURSOR_BLINKING          "cursor-blink"
#define KEY_CURSOR_BLINKING_TIME     "cursor-blink-time"
#define KEY_ENABLE_ANIMATIONS        "enable-animations"
#define KEY_GTK_THEME                "gtk-theme"
#define KEY_ICON_THEME               "icon-theme"
#define KEY_LOCATE_POINTER           "locate-pointer"
#define KEY_MOUSE_CURSOR_SIZE        "cursor-size"
#define KEY_OVERLAY_SCROLLING        "overlay-scrolling"
#define KEY_TEXT_SCALING_FACTOR      "text-scaling-factor"

/* application settings */
#define APPLICATION_SETTINGS         "org.gnome.desktop.a11y.applications"
#define KEY_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_SCREEN_READER_ENABLED    "screen-reader-enabled"

/* sound settings */
#define SOUND_SETTINGS               "org.gnome.desktop.sound"
#define KEY_SOUND_OVERAMPLIFY        "allow-volume-above-100-percent"

/* wm settings */
#define WM_SETTINGS                  "org.gnome.desktop.wm.preferences"
#define KEY_VISUAL_BELL              "visual-bell"
#define KEY_VISUAL_BELL_TYPE         "visual-bell-type"

/* keyboard settings */
#define KEYBOARD_SETTINGS            "org.gnome.desktop.a11y.keyboard"
#define KEY_BOUNCEKEYS_BEEP_REJECT   "bouncekeys-beep-reject"
#define KEY_BOUNCEKEYS_DELAY         "bouncekeys-delay"
#define KEY_BOUNCEKEYS_ENABLED       "bouncekeys-enable"
#define KEY_KEYBOARD_TOGGLE          "enable"
#define KEY_MOUSEKEYS_ENABLED        "mousekeys-enable"
#define KEY_SLOWKEYS_BEEP_ACCEPT     "slowkeys-beep-accept"
#define KEY_SLOWKEYS_BEEP_PRESS      "slowkeys-beep-press"
#define KEY_SLOWKEYS_BEEP_REJECT     "slowkeys-beep-reject"
#define KEY_SLOWKEYS_DELAY           "slowkeys-delay"
#define KEY_SLOWKEYS_ENABLED         "slowkeys-enable"
#define KEY_STICKYKEYS_ENABLED       "stickykeys-enable"
#define KEY_STICKYKEYS_MODIFIER_BEEP "stickykeys-modifier-beep"
#define KEY_STICKYKEYS_TWO_KEY_OFF   "stickykeys-two-key-off"
#define KEY_TOGGLEKEYS_ENABLED       "togglekeys-enable"

/* keyboard desktop settings */
#define KEYBOARD_DESKTOP_SETTINGS    "org.gnome.desktop.peripherals.keyboard"
#define KEY_REPEAT_DELAY             "delay"
#define KEY_REPEAT_KEYS              "repeat"
#define KEY_REPEAT_INTERVAL          "repeat-interval"

/* mouse settings */
#define MOUSE_SETTINGS               "org.gnome.desktop.a11y.mouse"
#define KEY_SECONDARY_CLICK_ENABLED  "secondary-click-enabled"
#define KEY_SECONDARY_CLICK_TIME     "secondary-click-time"
#define KEY_DWELL_CLICK_ENABLED      "dwell-click-enabled"
#define KEY_DWELL_TIME               "dwell-time"
#define KEY_DWELL_THRESHOLD          "dwell-threshold"

#define MOUSE_PERIPHERAL_SETTINGS    "org.gnome.desktop.peripherals.mouse"
#define KEY_DOUBLE_CLICK_DELAY       "double-click"
