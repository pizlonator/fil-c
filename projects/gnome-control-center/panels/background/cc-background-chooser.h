/* cc-background-chooser.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_CHOOSER (cc_background_chooser_get_type())
G_DECLARE_FINAL_TYPE (CcBackgroundChooser, cc_background_chooser, CC, BACKGROUND_CHOOSER, GtkBox)

void cc_background_chooser_select_file     (CcBackgroundChooser *self);
void cc_background_chooser_set_active_item (CcBackgroundChooser *self,
                                            CcBackgroundItem    *active_item);

G_END_DECLS
