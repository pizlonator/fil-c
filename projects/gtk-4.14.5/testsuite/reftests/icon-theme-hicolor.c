/*
 * Copyright (C) 2021 Red Hat, Inc
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>


G_MODULE_EXPORT void
set_icon_theme_hicolor (void)
{
  GtkSettings *settings;

  g_test_message ("Attention: setting icon-theme to hicolor");

  settings = gtk_settings_get_for_display (gdk_display_get_default ());
  g_object_set (settings, "gtk-icon-theme-name", "hicolor", NULL);
}
