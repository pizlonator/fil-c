/* cc-keyboard-shortcut-dialog.h
 *
 * Copyright (C) 2020 System76, Inc.
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
 * Author: Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_SHORTCUT_DIALOG (cc_keyboard_shortcut_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcKeyboardShortcutDialog, cc_keyboard_shortcut_dialog, CC, KEYBOARD_SHORTCUT_DIALOG, AdwWindow)

GtkWidget* cc_keyboard_shortcut_dialog_new (void);

G_END_DECLS
