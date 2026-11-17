/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2022 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "cc-keyboard-manager.h"
#include "cc-keyboard-shortcut-editor.h"

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_SHORTCUT_GROUP (cc_keyboard_shortcut_group_get_type ())

G_DECLARE_FINAL_TYPE (CcKeyboardShortcutGroup, cc_keyboard_shortcut_group, CC, KEYBOARD_SHORTCUT_GROUP, AdwPreferencesGroup)

GtkWidget  *cc_keyboard_shortcut_group_new          (GListModel               *shortcut_items,
                                                     const char               *section_id,
                                                     const char               *section_title,
                                                     CcKeyboardManager        *manager,
                                                     CcKeyboardShortcutEditor *shortcut_editor,
                                                     GtkSizeGroup             *size_group);
GListModel *cc_keyboard_shortcut_group_get_model    (CcKeyboardShortcutGroup  *self);
void        cc_keyboard_shortcut_group_set_filter   (CcKeyboardShortcutGroup  *self,
                                                     GStrv                     search_terms);

G_END_DECLS
