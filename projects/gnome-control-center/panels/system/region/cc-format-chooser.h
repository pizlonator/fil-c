/*
 * Copyright (C) 2013 Red Hat, Inc
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
 *
 * Written by:
 *     Matthias Clasen
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_FORMAT_CHOOSER (cc_format_chooser_get_type ())
G_DECLARE_FINAL_TYPE (CcFormatChooser, cc_format_chooser, CC, FORMAT_CHOOSER, GtkDialog)

CcFormatChooser *cc_format_chooser_new          (void);
void             cc_format_chooser_clear_filter (CcFormatChooser *chooser);
const gchar     *cc_format_chooser_get_region   (CcFormatChooser *chooser);
void             cc_format_chooser_set_region   (CcFormatChooser *chooser,
                                                 const gchar     *region);

G_END_DECLS
