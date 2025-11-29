/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <AppKit/AppKit.h>

#include "gdkdisplayprivate.h"

#include "gdkmacosdisplay.h"
#include "gdkmacoskeymap.h"
#include "gdkmacossurface.h"

G_BEGIN_DECLS

/* Text Input Client */
#define TIC_MARKED_TEXT      "tic-marked-text"
#define TIC_SELECTED_POS     "tic-selected-pos"
#define TIC_SELECTED_LEN     "tic-selected-len"
#define TIC_INSERT_TEXT      "tic-insert-text"
#define TIC_IN_KEY_DOWN      "tic-in-key-down"

/* GtkIMContext */
#define GIC_CURSOR_RECT      "gic-cursor-rect"
#define GIC_FILTER_KEY       "gic-filter-key"
#define GIC_FILTER_PASSTHRU  0
#define GIC_FILTER_FILTERED  1

#define GDK_MACOS_EVENT_DROP (GdkEvent *)GSIZE_TO_POINTER(1)

struct _GdkMacosDisplay
{
  GdkDisplay parent_instance;

  char *name;
  GdkMacosKeymap *keymap;

  /* An list of GdkMacosMonitor. The first instance is always the primary
   * monitor. This contains the 0,0 coordinate in quartz coordinates, but may
   * not be 0,0 in GDK coordinates.
   */
  GListStore *monitors;

  /* A queue of surfaces that have been made "main" so that we can update the
   * main status to the next surface when a window has lost main status (such
   * as when destroyed). This uses the GdkMacosSurface main link.
   */
  GQueue main_surfaces;

  /* A queue of surfaces sorted by their front-to-back ordering on the screen.
   * This is updated occasionally when we know that the data we have cached
   * has been invalidated. This uses the GdkMacosSurface.sorted link.
   */
  GQueue sorted_surfaces;

  /* The surface that is receiving keyboard events */
  GdkMacosSurface *keyboard_surface;

  /* [NSDraggingInfo draggingSequenceNumber] to GdkMacosDr(ag,op) */
  GHashTable *active_drags;
  GHashTable *active_drops;

  /* Used to translate from quartz coordinate space to GDK */
  int width;
  int height;
  int min_x;
  int min_y;
  int max_x;
  int max_y;

  /* A GSource to select a new main/key window */
  guint select_key_in_idle;

  /* Note if we have a key window that is not a GdkMacosWindow
   * such as a NSPanel used for native dialogs.
   */
  guint key_window_is_foreign : 1;
};

struct _GdkMacosDisplayClass
{
  GdkDisplayClass parent_class;
};


GdkDisplay      *_gdk_macos_display_open                           (const char      *display_name);
int              _gdk_macos_display_get_fd                         (GdkMacosDisplay *self);
void             _gdk_macos_display_queue_events                   (GdkMacosDisplay *self);
void             _gdk_macos_display_to_display_coords              (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y,
                                                                    int             *out_x,
                                                                    int             *out_y);
void             _gdk_macos_display_from_display_coords            (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y,
                                                                    int             *out_x,
                                                                    int             *out_y);
NSScreen        *_gdk_macos_display_get_screen_at_display_coords   (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y);
GdkMonitor      *_gdk_macos_display_get_monitor_at_coords          (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y);
GdkMonitor      *_gdk_macos_display_get_monitor_at_display_coords  (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y);
void             _gdk_macos_display_surface_added                  (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
GdkEvent        *_gdk_macos_display_translate                      (GdkMacosDisplay *self,
                                                                    NSEvent         *event);
void             _gdk_macos_display_feedback_init                  (GdkMacosDisplay *self);
void             _gdk_macos_display_feedback_destroy               (GdkMacosDisplay *self);
void             _gdk_macos_display_break_all_grabs                (GdkMacosDisplay *self,
                                                                    guint32          time);
GdkModifierType  _gdk_macos_display_get_current_keyboard_modifiers (GdkMacosDisplay *self);
GdkModifierType  _gdk_macos_display_get_current_mouse_modifiers    (GdkMacosDisplay *self);
GdkMacosSurface *_gdk_macos_display_get_surface_at_display_coords  (GdkMacosDisplay *self,
                                                                    double           x,
                                                                    double           y,
                                                                    int             *surface_x,
                                                                    int             *surface_y);
void             _gdk_macos_display_reload_monitors                (GdkMacosDisplay *self);
void             _gdk_macos_display_surface_removed                (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
NSWindow        *_gdk_macos_display_find_native_under_pointer      (GdkMacosDisplay *self,
                                                                    int             *x,
                                                                    int             *y);
gboolean         _gdk_macos_display_get_setting                    (GdkMacosDisplay *self,
                                                                    const char      *setting,
                                                                    GValue          *value);
void             _gdk_macos_display_reload_settings                (GdkMacosDisplay *self);
void             _gdk_macos_display_surface_resigned_main          (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
void             _gdk_macos_display_surface_became_main            (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
void             _gdk_macos_display_surface_resigned_key           (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
void             _gdk_macos_display_surface_became_key             (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface);
void             _gdk_macos_display_clear_sorting                  (GdkMacosDisplay *self);
const GList     *_gdk_macos_display_get_surfaces                   (GdkMacosDisplay *self);
void             _gdk_macos_display_send_event                     (GdkMacosDisplay *self,
                                                                    NSEvent         *nsevent);
void             _gdk_macos_display_warp_pointer                   (GdkMacosDisplay *self,
                                                                    int              x,
                                                                    int              y);
NSEvent         *_gdk_macos_display_get_matching_nsevent           (GdkEvent        *event);
NSEvent         *_gdk_macos_display_get_exact_nsevent              (GdkEvent        *event);
NSEvent         *_gdk_macos_display_get_last_nsevent               (void);
GdkDrag         *_gdk_macos_display_find_drag                      (GdkMacosDisplay *self,
                                                                    NSInteger        sequence_number);
GdkDrop         *_gdk_macos_display_find_drop                      (GdkMacosDisplay *self,
                                                                    NSInteger        sequence_number);
void             _gdk_macos_display_set_drag                       (GdkMacosDisplay *self,
                                                                    NSInteger        sequence_number,
                                                                    GdkDrag         *drag);
void             _gdk_macos_display_set_drop                       (GdkMacosDisplay *self,
                                                                    NSInteger        sequence_number,
                                                                    GdkDrop         *drop);
void             _gdk_macos_display_position_surface               (GdkMacosDisplay *self,
                                                                    GdkMacosSurface *surface,
                                                                    GdkMonitor      *monitor,
                                                                    int             *x,
                                                                    int             *y);

G_END_DECLS

