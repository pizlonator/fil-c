/* cc-input-list-box.c
 *
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Sergey Udaltsov   <svu@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

G_BEGIN_DECLS

#include <adwaita.h>

#define CC_TYPE_INPUT_LIST_BOX (cc_input_list_box_get_type ())
G_DECLARE_FINAL_TYPE (CcInputListBox, cc_input_list_box, CC, INPUT_LIST_BOX, AdwBin)

void cc_input_list_box_set_login            (CcInputListBox *box,
                                             gboolean        login);
void cc_input_list_box_set_login_auto_apply (CcInputListBox *box,
                                             gboolean        auto_apply);
void cc_input_list_box_set_localed          (CcInputListBox *box,
                                             GDBusProxy     *localed);
void cc_input_list_box_set_permission       (CcInputListBox *box,
                                             GPermission    *permission);

G_END_DECLS
