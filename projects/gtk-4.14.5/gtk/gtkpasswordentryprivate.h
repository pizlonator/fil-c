/* gtkpasswordentryprivate.h: Private API for GtkPasswordEntry
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkpasswordentry.h"
#include "gtktextprivate.h"

G_BEGIN_DECLS

GtkText *       gtk_password_entry_get_text_widget      (GtkPasswordEntry *entry);

void            gtk_password_entry_toggle_peek          (GtkPasswordEntry *entry);

G_END_DECLS
