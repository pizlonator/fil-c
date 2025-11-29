/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtkicontheme.h>
#include <gtk/gtkcssstyleprivate.h>

typedef struct _GtkStringSet GtkStringSet;
const char *gtk_string_set_add (GtkStringSet *set,
                                const char   *string);

#define IMAGE_MISSING_RESOURCE_PATH "/org/gtk/libgtk/icons/16x16/status/image-missing.png"

void gtk_icon_theme_lookup_symbolic_colors   (GtkCssStyle      *style,
                                              GdkRGBA           color_out[4]);

int gtk_icon_theme_get_serial (GtkIconTheme *self);

