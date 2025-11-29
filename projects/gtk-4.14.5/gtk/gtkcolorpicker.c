/* GTK - The GIMP Toolkit
 * Copyright (C) 2018, Red Hat, Inc
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

#include "config.h"

#include "gtkcolorpickerprivate.h"
#include "gtkcolorpickerportalprivate.h"
#include "gtkcolorpickershellprivate.h"
#include "gtkcolorpickerkwinprivate.h"

#ifdef G_OS_WIN32
#include "gtkcolorpickerwin32private.h"
#endif

#include <gio/gio.h>


G_DEFINE_INTERFACE_WITH_CODE (GtkColorPicker, gtk_color_picker, G_TYPE_OBJECT,
                              g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_INITABLE);)

static void
gtk_color_picker_default_init (GtkColorPickerInterface *iface)
{
}

void
gtk_color_picker_pick (GtkColorPicker      *picker,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  GTK_COLOR_PICKER_GET_INTERFACE (picker)->pick (picker, callback, user_data);
}

GdkRGBA *
gtk_color_picker_pick_finish (GtkColorPicker  *picker,
                              GAsyncResult    *res,
                              GError         **error)
{
  return GTK_COLOR_PICKER_GET_INTERFACE (picker)->pick_finish (picker, res, error);
}

GtkColorPicker *
gtk_color_picker_new (void)
{
  GtkColorPicker *picker = NULL;

#if defined (G_OS_UNIX)
  if (!picker)
    picker = gtk_color_picker_portal_new ();
  if (!picker)
    picker = gtk_color_picker_shell_new ();
  if (!picker)
    picker = gtk_color_picker_kwin_new ();
#elif defined (G_OS_WIN32)
  if (!picker)
    picker = gtk_color_picker_win32_new ();
#endif

  if (!picker)
    g_debug ("No suitable GtkColorPicker implementation");
  else
    g_debug ("Using %s for picking colors", G_OBJECT_TYPE_NAME (picker));

  return picker;
}

