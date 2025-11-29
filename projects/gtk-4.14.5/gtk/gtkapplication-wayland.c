/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 * Copyright © 2020 Emmanuel Gil Peyrot
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

#include <gdk/wayland/gdkwayland.h>
#include <gdk/wayland/gdkdisplay-wayland.h>
#include <gdk/wayland/gdksurface-wayland.h>
#include <gdk/wayland/gdktoplevel-wayland-private.h>
#include <gdk/wayland/idle-inhibit-unstable-v1-client-protocol.h>

typedef struct
{
  GtkApplicationImplDBusClass parent_class;

  /* stores the dbus version of the overridden methods */
  guint (*dbus_inhibit) (GtkApplicationImpl         *impl,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const char                 *reason);
  void (*dbus_uninhibit) (GtkApplicationImpl *impl,
                          guint               cookie);
} GtkApplicationImplWaylandClass;

typedef struct
{
  guint cookie;
  guint dbus_cookie;
  GtkApplicationInhibitFlags flags;
  GdkSurface *surface;

} GtkApplicationWaylandInhibitor;

static void
gtk_application_wayland_inhibitor_free (GtkApplicationWaylandInhibitor *inhibitor)
{
  g_free (inhibitor);
}

typedef struct
{
  GtkApplicationImplDBus dbus;
  GSList *inhibitors;
  guint next_cookie;

} GtkApplicationImplWayland;

G_DEFINE_TYPE (GtkApplicationImplWayland, gtk_application_impl_wayland, GTK_TYPE_APPLICATION_IMPL_DBUS)

static void
gtk_application_impl_wayland_handle_window_realize (GtkApplicationImpl *impl,
                                                    GtkWindow          *window)
{
  GtkApplicationImplClass *impl_class =
    GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class);
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GdkSurface *gdk_surface;
  char *window_path;

  gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));

  if (!GDK_IS_WAYLAND_TOPLEVEL (gdk_surface))
    return;

  window_path = gtk_application_impl_dbus_get_window_path (dbus, window);

  gdk_wayland_toplevel_set_dbus_properties (GDK_TOPLEVEL (gdk_surface),
                                            dbus->application_id,
                                            dbus->app_menu_path,
                                            dbus->menubar_path,
                                            window_path,
                                            dbus->object_path,
                                            dbus->unique_name);

  g_free (window_path);

  impl_class->handle_window_realize (impl, window);
}

static void
gtk_application_impl_wayland_before_emit (GtkApplicationImpl *impl,
                                          GVariant           *platform_data)
{
  const char *startup_notification_id = NULL;

  g_variant_lookup (platform_data, "activation-token", "&s", &startup_notification_id);
  if (!startup_notification_id)
    g_variant_lookup (platform_data, "desktop-startup-id", "&s", &startup_notification_id);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_wayland_display_set_startup_notification_id (gdk_display_get_default (), startup_notification_id);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static guint
gtk_application_impl_wayland_inhibit (GtkApplicationImpl         *impl,
                                      GtkWindow                  *window,
                                      GtkApplicationInhibitFlags  flags,
                                      const char                 *reason)
{
  GtkApplicationImplWayland *wayland = (GtkApplicationImplWayland *) impl;
  GdkSurface *surface;
  GtkApplicationWaylandInhibitor *inhibitor;
  gboolean success;

  if (!flags)
    return 0;

  inhibitor = g_new0 (GtkApplicationWaylandInhibitor, 1);
  inhibitor->cookie = ++wayland->next_cookie;
  inhibitor->flags = flags;
  wayland->inhibitors = g_slist_prepend (wayland->inhibitors, inhibitor);

  if (flags & GTK_APPLICATION_INHIBIT_IDLE)
    {
      surface = gtk_native_get_surface (GTK_NATIVE (window));
      if (GDK_IS_WAYLAND_TOPLEVEL (surface))
        {
          success = gdk_wayland_toplevel_inhibit_idle (GDK_TOPLEVEL (surface));
          if (success)
            {
              flags &= ~GTK_APPLICATION_INHIBIT_IDLE;
              inhibitor->surface = surface;
            }
        }
    }

  if (flags)
    inhibitor->dbus_cookie = ((GtkApplicationImplWaylandClass *) G_OBJECT_GET_CLASS (wayland))->dbus_inhibit (impl, window, flags, reason);

  return inhibitor->cookie;
}

static void
gtk_application_impl_wayland_uninhibit (GtkApplicationImpl *impl,
                                        guint               cookie)
{
  GtkApplicationImplWayland *wayland = (GtkApplicationImplWayland *) impl;
  GSList *iter;

  for (iter = wayland->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationWaylandInhibitor *inhibitor = iter->data;

      if (inhibitor->cookie == cookie)
        {
          if (inhibitor->dbus_cookie)
            ((GtkApplicationImplWaylandClass *) G_OBJECT_GET_CLASS (wayland))->dbus_uninhibit (impl, inhibitor->dbus_cookie);
          if (inhibitor->surface)
            gdk_wayland_toplevel_uninhibit_idle (GDK_TOPLEVEL (inhibitor->surface));
          gtk_application_wayland_inhibitor_free (inhibitor);
          wayland->inhibitors = g_slist_delete_link (wayland->inhibitors, iter);
          return;
        }
    }

  g_warning ("Invalid inhibitor cookie");
}

static void
gtk_application_impl_wayland_init (GtkApplicationImplWayland *wayland)
{
}

static void
gtk_application_impl_wayland_class_init (GtkApplicationImplWaylandClass *class)
{
  GtkApplicationImplClass *impl_class = GTK_APPLICATION_IMPL_CLASS (class);

  class->dbus_inhibit = impl_class->inhibit;
  class->dbus_uninhibit = impl_class->uninhibit;

  impl_class->handle_window_realize =
    gtk_application_impl_wayland_handle_window_realize;
  impl_class->before_emit =
    gtk_application_impl_wayland_before_emit;
  impl_class->inhibit =
    gtk_application_impl_wayland_inhibit;
  impl_class->uninhibit =
    gtk_application_impl_wayland_uninhibit;
}
