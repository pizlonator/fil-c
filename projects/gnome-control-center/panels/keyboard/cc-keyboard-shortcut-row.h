/* cc-keyboard-shortcut-row.h
 *
 * Copyright (C) 2020 System76, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include "cc-keyboard-item.h"
#include "cc-keyboard-manager.h"
#include "cc-keyboard-shortcut-editor.h"

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_SHORTCUT_ROW (cc_keyboard_shortcut_row_get_type())
G_DECLARE_FINAL_TYPE (CcKeyboardShortcutRow, cc_keyboard_shortcut_row, CC, KEYBOARD_SHORTCUT_ROW, AdwActionRow)

CcKeyboardShortcutRow *cc_keyboard_shortcut_row_new (CcKeyboardItem           *item,
                                                     CcKeyboardManager        *manager,
                                                     CcKeyboardShortcutEditor *editor,
                                                     GtkSizeGroup             *size_group);
CcKeyboardItem        *cc_keyboard_shortcut_row_get_item  (CcKeyboardShortcutRow   *self);

G_END_DECLS
