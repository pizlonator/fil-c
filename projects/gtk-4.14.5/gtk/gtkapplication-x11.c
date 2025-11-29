/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationprivate.h"
#include "gtknative.h"

#include <gdk/x11/gdkx.h>

typedef GtkApplicationImplDBusClass GtkApplicationImplX11Class;

typedef struct
{
  GtkApplicationImplDBus dbus;

} GtkApplicationImplX11;

G_DEFINE_TYPE (GtkApplicationImplX11, gtk_application_impl_x11, GTK_TYPE_APPLICATION_IMPL_DBUS)

static void
gtk_application_impl_x11_handle_window_realize (GtkApplicationImpl *impl,
                                                GtkWindow          *window)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GdkSurface *gdk_surface;
  char *window_path;

  gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));

  if (!GDK_IS_X11_SURFACE (gdk_surface))
    return;

  window_path = gtk_application_impl_dbus_get_window_path (dbus, window);

  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_APPLICATION_ID", dbus->application_id);
  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_UNIQUE_BUS_NAME", dbus->unique_name);
  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_APPLICATION_OBJECT_PATH", dbus->object_path);
  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_WINDOW_OBJECT_PATH", window_path);
  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_APP_MENU_OBJECT_PATH", dbus->app_menu_path);
  gdk_x11_surface_set_utf8_property (gdk_surface, "_GTK_MENUBAR_OBJECT_PATH", dbus->menubar_path);

  g_free (window_path);
}

static GVariant *
gtk_application_impl_x11_get_window_system_id (GtkApplicationImplDBus *dbus,
                                               GtkWindow              *window)
{
  GdkSurface *gdk_surface;

  gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));

  if (GDK_IS_X11_SURFACE (gdk_surface))
    return g_variant_new_uint32 (GDK_SURFACE_XID (gdk_surface));

  return GTK_APPLICATION_IMPL_DBUS_CLASS (gtk_application_impl_x11_parent_class)->get_window_system_id (dbus, window);
}

static void
gtk_application_impl_x11_init (GtkApplicationImplX11 *x11)
{
}

static void
gtk_application_impl_x11_before_emit (GtkApplicationImpl *impl,
                                      GVariant           *platform_data)
{
  const char *startup_notification_id = NULL;

  g_variant_lookup (platform_data, "desktop-startup-id", "&s", &startup_notification_id);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_x11_display_set_startup_notification_id (gdk_display_get_default (), startup_notification_id);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_application_impl_x11_class_init (GtkApplicationImplX11Class *class)
{
  GtkApplicationImplDBusClass *dbus_class = GTK_APPLICATION_IMPL_DBUS_CLASS (class);
  GtkApplicationImplClass *impl_class = GTK_APPLICATION_IMPL_CLASS (class);

  impl_class->handle_window_realize = gtk_application_impl_x11_handle_window_realize;
  dbus_class->get_window_system_id = gtk_application_impl_x11_get_window_system_id;
  impl_class->before_emit = gtk_application_impl_x11_before_emit;
}
