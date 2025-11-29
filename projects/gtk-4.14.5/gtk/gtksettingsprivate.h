/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include <gtk/gtksettings.h>
#include "gtkstylecascadeprivate.h"

G_BEGIN_DECLS

#define DEFAULT_THEME_NAME      "Default"
#define DEFAULT_ICON_THEME      "Adwaita"

const cairo_font_options_t *
                    gtk_settings_get_font_options            (GtkSettings            *settings);
GdkDisplay         *_gtk_settings_get_display                (GtkSettings            *settings);
GtkStyleCascade    *_gtk_settings_get_style_cascade          (GtkSettings            *settings,
                                                              int                     scale);

typedef enum
{
  GTK_SETTINGS_SOURCE_DEFAULT,
  GTK_SETTINGS_SOURCE_THEME,
  GTK_SETTINGS_SOURCE_XSETTING,
  GTK_SETTINGS_SOURCE_APPLICATION
} GtkSettingsSource;

GtkSettingsSource  _gtk_settings_get_setting_source (GtkSettings *settings,
                                                     const char *name);

gboolean gtk_settings_get_enable_animations  (GtkSettings *settings);
int      gtk_settings_get_dnd_drag_threshold (GtkSettings *settings);
const char *gtk_settings_get_font_family    (GtkSettings *settings);
int          gtk_settings_get_font_size      (GtkSettings *settings);
gboolean     gtk_settings_get_font_size_is_absolute (GtkSettings *settings);

G_END_DECLS

