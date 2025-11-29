/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#pragma once

#include "gdksurfaceprivate.h"
#include "gdkx11surface.h"

#include <X11/Xlib.h>

#ifdef HAVE_XSYNC
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#endif

G_BEGIN_DECLS

GType gdk_x11_toplevel_get_type (void) G_GNUC_CONST;
GType gdk_x11_popup_get_type (void) G_GNUC_CONST;
GType gdk_x11_drag_surface_get_type (void) G_GNUC_CONST;

#define GDK_TYPE_X11_TOPLEVEL (gdk_x11_toplevel_get_type ())
#define GDK_TYPE_X11_POPUP (gdk_x11_popup_get_type ())
#define GDK_TYPE_X11_DRAG_SURFACE (gdk_x11_drag_surface_get_type ())

typedef struct _GdkToplevelX11 GdkToplevelX11;
typedef struct _GdkXPositionInfo GdkXPositionInfo;

struct _GdkX11Surface
{
  GdkSurface parent_instance;

  Window xid;

  GdkToplevelX11 *toplevel;	/* Toplevel-specific information */
  GdkCursor *cursor;

  guint no_bg : 1;        /* Set when the window background is temporarily
                           * unset during resizing and scaling */
  guint override_redirect : 1;
  guint frame_clock_connected : 1;
  guint frame_sync_enabled : 1;
  guint tracking_damage: 1;

  int surface_scale;

  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  /* Width and height not divided by surface_scale - this matters in the
   * corner-case where the window manager assigns us a size that isn't
   * a multiple of surface_scale - for example for a maximized window
   * with an odd-sized title-bar.
   */
  int unscaled_width;
  int unscaled_height;

  int last_computed_width;
  int last_computed_height;

  GdkToplevelLayout *toplevel_layout;

  struct {
    int configured_width;
    int configured_height;
    gboolean configure_pending;
    gboolean surface_geometry_dirty;
  } next_layout;

  guint compute_size_source_id;

  cairo_surface_t *cairo_surface;
  /* GLXDrawable */ XID glx_drawable;
  guint32 glx_frame_counter;

  int abs_x;
  int abs_y;

  guint64 map_time;

  GList *surface_is_on_monitor;
};
 
struct _GdkX11SurfaceClass 
{
  GdkSurfaceClass parent_class;
};

struct _GdkToplevelX11
{

  /* Set if the window, or any descendent of it, is the server's focus window
   */
  guint has_focus_window : 1;

  /* Set if window->has_focus_window and the focus isn't grabbed elsewhere.
   */
  guint has_focus : 1;

  /* Set if the pointer is inside this window. (This is needed for
   * for focus tracking)
   */
  guint has_pointer : 1;
  
  /* Set if the window is a descendent of the focus window and the pointer is
   * inside it. (This is the case where the window will receive keystroke
   * events even window->has_focus_window is FALSE)
   */
  guint has_pointer_focus : 1;

  /* Set if we are requesting these hints */
  guint skip_taskbar_hint : 1;
  guint skip_pager_hint : 1;
  guint urgency_hint : 1;

  guint on_all_desktops : 1;   /* _NET_WM_STICKY == 0xFFFFFFFF */

  guint have_maxvert : 1;       /* _NET_WM_STATE_MAXIMIZED_VERT */
  guint have_maxhorz : 1;       /* _NET_WM_STATE_MAXIMIZED_HORZ */
  guint have_fullscreen : 1;    /* _NET_WM_STATE_FULLSCREEN */
  guint have_hidden : 1;	/* _NET_WM_STATE_HIDDEN */

  guint is_leader : 1;

  /* Set if the WM is presenting us as focused, i.e. with active decorations
   */
  guint have_focused : 1;

  guint in_frame : 1;

  /* If we're waiting for damage from the X server after painting a frame */
  guint frame_still_painting : 1;

  /* If we're expecting a response from the compositor after painting a frame */
  guint frame_pending : 1;

  /* Whether pending_counter_value/configure_counter_value are updates
   * to the extended update counter */
  guint pending_counter_value_is_extended : 1;
  guint configure_counter_value_is_extended : 1;

  gulong map_serial;	/* Serial of last transition from unmapped */
  
  cairo_surface_t *icon_pixmap;
  cairo_surface_t *icon_mask;
  GdkSurface *group_leader;

  /* Time of most recent user interaction. */
  gulong user_time;

  /* We use an extra X window for toplevel windows that we XSetInputFocus()
   * to in order to avoid getting keyboard events redirected to subwindows
   * that might not even be part of this app
   */
  Window focus_window;

  GdkSurfaceHints last_geometry_hints_mask;
  GdkGeometry last_geometry_hints;
  
  /* Constrained edge information */
  guint edge_constraints;

#ifdef HAVE_XSYNC
  XID update_counter;
  XID extended_update_counter;
  gint64 pending_counter_value; /* latest _NET_WM_SYNC_REQUEST value received */
  gint64 configure_counter_value; /* Latest _NET_WM_SYNC_REQUEST value received
				 * where we have also seen the corresponding
				 * ConfigureNotify
				 */
  gint64 current_counter_value;

  /* After a _NET_WM_FRAME_DRAWN message, this is the soonest that we think
   * frame after will be presented */
  gint64 throttled_presentation_time;
#endif
};

GdkSurface     *gdk_x11_drag_surface_new             (GdkDisplay *display);

GdkToplevelX11 *_gdk_x11_surface_get_toplevel        (GdkSurface *window);

GdkCursor      *_gdk_x11_surface_get_cursor          (GdkSurface *window);

void            _gdk_x11_surface_set_surface_scale   (GdkSurface *window,
                                                      int        scale);
gboolean        _gdk_x11_surface_syncs_frames        (GdkSurface *surface);

void            gdk_x11_surface_pre_damage           (GdkSurface *surface);

void            gdk_x11_surface_move                 (GdkSurface *surface,
                                                      int         x,
                                                      int         y);
void            gdk_x11_surface_check_monitor        (GdkSurface *surface,
                                                      GdkMonitor *monitor);


G_END_DECLS

