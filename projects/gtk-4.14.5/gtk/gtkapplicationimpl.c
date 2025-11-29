/*
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

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_MACOS
#include <gdk/macos/gdkmacos.h>
#endif

G_DEFINE_TYPE (GtkApplicationImpl, gtk_application_impl, G_TYPE_OBJECT)

static void
gtk_application_impl_init (GtkApplicationImpl *impl)
{
}

static guint do_nothing (void) { return 0; }
static gboolean return_false (void) { return FALSE; }

static void
gtk_application_impl_class_init (GtkApplicationImplClass *class)
{
  /* NB: can only 'do_nothing' for functions with integer or void return */
  class->startup = (gpointer) do_nothing;
  class->shutdown = (gpointer) do_nothing;
  class->before_emit = (gpointer) do_nothing;
  class->window_added = (gpointer) do_nothing;
  class->window_removed = (gpointer) do_nothing;
  class->active_window_changed = (gpointer) do_nothing;
  class->handle_window_realize = (gpointer) do_nothing;
  class->handle_window_map = (gpointer) do_nothing;
  class->set_app_menu = (gpointer) do_nothing;
  class->set_menubar = (gpointer) do_nothing;
  class->inhibit = (gpointer) do_nothing;
  class->uninhibit = (gpointer) do_nothing;
  class->prefers_app_menu = (gpointer) return_false;
}

void
gtk_application_impl_startup (GtkApplicationImpl *impl,
                              gboolean            register_session)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->startup (impl, register_session);
}

void
gtk_application_impl_shutdown (GtkApplicationImpl *impl)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->shutdown (impl);
}

void
gtk_application_impl_before_emit (GtkApplicationImpl *impl,
                                  GVariant           *platform_data)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->before_emit (impl, platform_data);
}

void
gtk_application_impl_window_added (GtkApplicationImpl *impl,
                                   GtkWindow          *window)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->window_added (impl, window);
}

void
gtk_application_impl_window_removed (GtkApplicationImpl *impl,
                                     GtkWindow          *window)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->window_removed (impl, window);
}

void
gtk_application_impl_active_window_changed (GtkApplicationImpl *impl,
                                            GtkWindow          *window)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->active_window_changed (impl, window);
}

void
gtk_application_impl_handle_window_realize (GtkApplicationImpl *impl,
                                            GtkWindow          *window)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->handle_window_realize (impl, window);
}

void
gtk_application_impl_handle_window_map (GtkApplicationImpl *impl,
                                        GtkWindow          *window)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->handle_window_map (impl, window);
}

void
gtk_application_impl_set_app_menu (GtkApplicationImpl *impl,
                                   GMenuModel         *app_menu)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->set_app_menu (impl, app_menu);
}

void
gtk_application_impl_set_menubar (GtkApplicationImpl *impl,
                                  GMenuModel         *menubar)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->set_menubar (impl, menubar);
}

guint
gtk_application_impl_inhibit (GtkApplicationImpl         *impl,
                              GtkWindow                  *window,
                              GtkApplicationInhibitFlags  flags,
                              const char                 *reason)
{
  return GTK_APPLICATION_IMPL_GET_CLASS (impl)->inhibit (impl, window, flags, reason);
}

void
gtk_application_impl_uninhibit (GtkApplicationImpl *impl,
                                guint               cookie)
{
  GTK_APPLICATION_IMPL_GET_CLASS (impl)->uninhibit (impl, cookie);
}

gboolean
gtk_application_impl_prefers_app_menu (GtkApplicationImpl *impl)
{
  return GTK_APPLICATION_IMPL_GET_CLASS (impl)->prefers_app_menu (impl);
}

GtkApplicationImpl *
gtk_application_impl_new (GtkApplication *application,
                          GdkDisplay     *display)
{
  GtkApplicationImpl *impl;
  GType impl_type;

  impl_type = gtk_application_impl_get_type ();

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    impl_type = gtk_application_impl_x11_get_type ();
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    impl_type = gtk_application_impl_wayland_get_type ();
#endif

#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (display))
    impl_type = gtk_application_impl_quartz_get_type ();
#endif

  impl = g_object_new (impl_type, NULL);
  impl->application = application;
  impl->display = display;

  return impl;
}
