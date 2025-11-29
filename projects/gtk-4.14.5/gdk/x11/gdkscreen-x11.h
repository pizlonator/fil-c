/*
 * gdkscreen-x11.h
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#pragma once

#include "gdkx11screen.h"
#include <X11/X.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

struct _GdkX11Screen
{
  GObject parent_instance;

  GdkDisplay *display;
  Display *xdisplay;
  Screen *xscreen;
  Window xroot_window;
  int screen_num;

  int surface_scale;
  gboolean fixed_surface_scale;

  /* Xft resources for the display, used for default values for
   * the Xft/ XSETTINGS
   */
  int xft_hintstyle;
  int xft_rgba;
  int xft_dpi;

  /* Window manager */
  Window wmspec_check_window;
  char *window_manager_name;

  /* X Settings */
  Window xsettings_manager_window;
  Atom xsettings_selection_atom;
  GHashTable *xsettings; /* string of GDK settings name => GValue */

  /* TRUE if wmspec_check_window has changed since last
   * fetch of _NET_SUPPORTED
   */
  guint need_refetch_net_supported : 1;
  /* TRUE if wmspec_check_window has changed since last
   * fetch of window manager name
   */
  guint need_refetch_wm_name : 1;
  guint xft_init : 1; /* Whether we've initialized these values yet */
  guint xft_antialias : 1;
  guint xft_hinting : 1;

  /* cache for window->translate vfunc */
  GC subwindow_gcs[32];
};

struct _GdkX11ScreenClass
{
  GObjectClass parent_class;

  void (* window_manager_changed) (GdkX11Screen *x11_screen);
};

GType       _gdk_x11_screen_get_type (void);
GdkX11Screen *_gdk_x11_screen_new           (GdkDisplay   *display,
                                             int           screen_number);

void _gdk_x11_screen_window_manager_changed (GdkX11Screen *screen);
void _gdk_x11_screen_size_changed           (GdkX11Screen *screen,
                                             const XEvent *event);
void _gdk_x11_screen_get_edge_monitors      (GdkX11Screen *screen,
                                             int          *top,
                                             int          *bottom,
                                             int          *left,
                                             int          *right);
void _gdk_x11_screen_set_surface_scale      (GdkX11Screen *x11_screen,
                                             int           scale);
gboolean _gdk_x11_screen_get_monitor_work_area (GdkX11Screen *screen,
                                                GdkMonitor   *monitor,
                                                GdkRectangle *area);
void gdk_x11_screen_get_work_area           (GdkX11Screen *screen,
                                             GdkRectangle *area);
gboolean gdk_x11_screen_get_setting         (GdkX11Screen *screen,
                                             const char   *name,
                                             GValue       *value);
gboolean
_gdk_x11_screen_get_xft_setting             (GdkX11Screen *screen,
                                             const char   *name,
                                             GValue       *value);

G_END_DECLS

