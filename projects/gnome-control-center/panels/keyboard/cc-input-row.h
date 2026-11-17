/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#include "cc-input-source.h"

G_BEGIN_DECLS

#define CC_TYPE_INPUT_ROW (cc_input_row_get_type ())
G_DECLARE_FINAL_TYPE (CcInputRow, cc_input_row, CC, INPUT_ROW, AdwActionRow)

CcInputRow      *cc_input_row_new           (CcInputSource *source);

CcInputSource   *cc_input_row_get_source    (CcInputRow    *row);

void             cc_input_row_set_removable (CcInputRow    *row,
                                             gboolean       removable);

void             cc_input_row_set_draggable (CcInputRow    *row,
                                             gboolean       draggable);

G_END_DECLS
