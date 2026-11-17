/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 */

#pragma once

#include <glib-object.h>

#include "cc-keyboard-item.h"

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_MANAGER (cc_keyboard_manager_get_type ())
G_DECLARE_FINAL_TYPE (CcKeyboardManager, cc_keyboard_manager, CC, KEYBOARD_MANAGER, GObject)

CcKeyboardManager*   cc_keyboard_manager_new                     (void);

void                 cc_keyboard_manager_load_shortcuts          (CcKeyboardManager  *self);

CcKeyboardItem*      cc_keyboard_manager_create_custom_shortcut  (CcKeyboardManager  *self);

void                 cc_keyboard_manager_add_custom_shortcut     (CcKeyboardManager  *self,
                                                                  CcKeyboardItem     *item);

void                 cc_keyboard_manager_remove_custom_shortcut  (CcKeyboardManager  *self,
                                                                  CcKeyboardItem     *item);

CcKeyboardItem*      cc_keyboard_manager_get_collision           (CcKeyboardManager  *self,
                                                                  CcKeyboardItem     *item,
                                                                  CcKeyCombo         *combo);

void                 cc_keyboard_manager_reset_shortcut          (CcKeyboardManager  *self,
                                                                  CcKeyboardItem     *item);

G_END_DECLS

