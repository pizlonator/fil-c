/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2019 Red Hat, Inc.
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

/* Uninstalled header defining types and functions internal to GDK */

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdkenumtypes.h"
#include "gdksurface.h"
#include "gdktoplevel.h"
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GdkSubsurface GdkSubsurface;

typedef struct _GskRenderNode GskRenderNode;

struct _GdkSurface
{
  GObject parent_instance;

  GdkDisplay *display;

  GdkSurface *transient_for; /* for toplevels */
  GdkSurface *parent;        /* for popups */
  GList *children;           /* popups */

  guint set_is_mapped_source_id;
  gboolean pending_is_mapped;
  gboolean is_mapped;

  int x;
  int y;

  GdkGLContext *gl_paint_context;

  cairo_region_t *update_area;
  guint update_freeze_count;
  GdkFrameClockPhase pending_phases;

  GdkToplevelState pending_set_flags;
  GdkToplevelState pending_unset_flags;
  GdkToplevelState state;

  guint8 resize_count;

  guint8 alpha;
  guint8 fullscreen_mode;

  guint modal_hint : 1;
  guint destroyed : 2;
  guint frame_clock_events_paused : 1;
  guint autohide : 1;
  guint shortcuts_inhibited : 1;
  guint request_motion : 1;
  guint has_pointer : 1;

  guint request_motion_id;

  struct {
    GdkGravity surface_anchor;
    GdkGravity rect_anchor;
  } popup;

  guint update_and_descendants_freeze_count;

  int width, height;

  GdkCursor *cursor;
  GHashTable *device_cursor;

  cairo_region_t *input_region;

  GList *devices_inside;

  GdkFrameClock *frame_clock; /* NULL to use from parent or default */

  GSList *draw_contexts;
  GdkDrawContext *paint_context;

  cairo_region_t *opaque_region;

  GdkSeat *current_shortcuts_inhibited_seat;

  GPtrArray *subsurfaces;

  /* We keep the subsurfaces above and below the surface in two linked
   * lists, which start here.
   */
  GdkSubsurface *subsurfaces_above;
  GdkSubsurface *subsurfaces_below;
};

struct _GdkSurfaceClass
{
  GObjectClass parent_class;

  void         (* hide)                 (GdkSurface      *surface);
  void         (* get_geometry)         (GdkSurface      *surface,
                                         int             *x,
                                         int             *y,
                                         int             *width,
                                         int             *height);
  void         (* get_root_coords)      (GdkSurface      *surface,
                                         int              x,
                                         int              y,
                                         int             *root_x,
                                         int             *root_y);
  gboolean     (* get_device_state)     (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         double          *x,
                                         double          *y,
                                         GdkModifierType *mask);
  void         (* set_input_region)     (GdkSurface      *surface,
                                         cairo_region_t  *shape_region);

  /* Called to do the windowing system specific part of gdk_surface_destroy(),
   *
   * surface: The window being destroyed
   * foreign_destroy: If TRUE, the surface or a parent was destroyed by some
   *     external agency. The surface has already been destroyed and no
   *     windowing system calls should be made. (This may never happen
   *     for some windowing systems.)
   */
  void         (* destroy)              (GdkSurface       *surface,
                                         gboolean         foreign_destroy);


  /* optional */
  gboolean     (* beep)                 (GdkSurface       *surface);

  void         (* destroy_notify)       (GdkSurface *surface);
  GdkDrag *    (* drag_begin)           (GdkSurface         *surface,
                                         GdkDevice          *device,
                                         GdkContentProvider *content,
                                         GdkDragAction       actions,
                                         double              dx,
                                         double              dy);

  double       (* get_scale)              (GdkSurface      *surface);

  void         (* set_opaque_region)      (GdkSurface      *surface,
                                           cairo_region_t *region);
  void         (* request_layout)         (GdkSurface     *surface);
  gboolean     (* compute_size)           (GdkSurface     *surface);

  GdkSubsurface *
               (* create_subsurface)      (GdkSurface          *surface);
};

#define GDK_SURFACE_DESTROYED(d) (((GdkSurface *)(d))->destroyed)

#define GDK_SURFACE_IS_MAPPED(surface) ((surface)->pending_is_mapped)

void gdk_surface_set_state (GdkSurface      *surface,
                            GdkToplevelState  new_state);

void gdk_surface_set_is_mapped (GdkSurface *surface,
                                gboolean    is_mapped);

GdkMonitor * gdk_surface_get_layout_monitor (GdkSurface      *surface,
                                             GdkPopupLayout  *layout,
                                             void           (*get_bounds) (GdkMonitor   *monitor,
                                                                           GdkRectangle *bounds));

void gdk_surface_layout_popup_helper (GdkSurface     *surface,
                                      int             width,
                                      int             height,
                                      int             shadow_left,
                                      int             shadow_right,
                                      int             shadow_top,
                                      int             shadow_bottom,
                                      GdkMonitor     *monitor,
                                      GdkRectangle   *bounds,
                                      GdkPopupLayout *layout,
                                      GdkRectangle   *out_final_rect);

static inline GdkGravity
gdk_gravity_flip_horizontally (GdkGravity anchor)
{
  switch (anchor)
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
    }

  g_assert_not_reached ();
}

static inline GdkGravity
gdk_gravity_flip_vertically (GdkGravity anchor)
{
  switch (anchor)
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_SOUTH_WEST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_NORTH_EAST;
    }

  g_assert_not_reached ();
}

void       _gdk_surface_destroy           (GdkSurface      *surface,
                                           gboolean        foreign_destroy);
void       gdk_surface_invalidate_rect    (GdkSurface           *surface,
                                           const GdkRectangle   *rect);
void       gdk_surface_invalidate_region  (GdkSurface           *surface,
                                           const cairo_region_t *region);
void       _gdk_surface_clear_update_area (GdkSurface      *surface);
void       _gdk_surface_update_size       (GdkSurface      *surface);

GdkGLContext * gdk_surface_get_paint_gl_context (GdkSurface *surface,
                                                 GError   **error);

gboolean gdk_surface_handle_event (GdkEvent       *event);
GdkSeat * gdk_surface_get_seat_from_event (GdkSurface *surface,
                                           GdkEvent    *event);

void       gdk_surface_enter_monitor (GdkSurface *surface,
                                      GdkMonitor *monitor);
void       gdk_surface_leave_monitor (GdkSurface *surface,
                                      GdkMonitor *monitor);

void gdk_surface_destroy_notify       (GdkSurface *surface);

void gdk_synthesize_surface_state (GdkSurface     *surface,
                                   GdkToplevelState unset_flags,
                                   GdkToplevelState set_flags);

void gdk_surface_get_root_coords (GdkSurface *surface,
                                  int         x,
                                  int         y,
                                  int        *root_x,
                                  int        *root_y);
void gdk_surface_get_origin      (GdkSurface *surface,
                                  int        *x,
                                  int        *y);


void gdk_surface_get_geometry (GdkSurface *surface,
                               int        *x,
                               int        *y,
                               int        *width,
                               int        *height);

void                    gdk_surface_set_frame_clock             (GdkSurface             *surface,
                                                                 GdkFrameClock          *clock);
void                    gdk_surface_set_egl_native_window       (GdkSurface             *self,
                                                                 gpointer                native_window);
void                    gdk_surface_ensure_egl_surface          (GdkSurface             *self,
                                                                 gboolean                hdr);
gpointer /*EGLSurface*/ gdk_surface_get_egl_surface             (GdkSurface             *self);

void                    gdk_surface_set_widget                  (GdkSurface             *self,
                                                                 gpointer                widget);
gpointer                gdk_surface_get_widget                  (GdkSurface             *self);

void                    gdk_surface_freeze_updates              (GdkSurface             *surface);
void                    gdk_surface_thaw_updates                (GdkSurface             *surface);


typedef enum
{
  GDK_HINT_MIN_SIZE    = 1 << 1,
  GDK_HINT_MAX_SIZE    = 1 << 2,
} GdkSurfaceHints;

typedef struct _GdkGeometry GdkGeometry;

struct _GdkGeometry
{
  int min_width;
  int min_height;
  int max_width;
  int max_height;
};

void       gdk_surface_constrain_size      (GdkGeometry    *geometry,
                                            GdkSurfaceHints  flags,
                                            int             width,
                                            int             height,
                                            int            *new_width,
                                            int            *new_height);

void       gdk_surface_queue_state_change  (GdkSurface       *surface,
                                            GdkToplevelState  unset_flags,
                                            GdkToplevelState  set_flags);

void       gdk_surface_apply_state_change  (GdkSurface       *surface);

void       gdk_surface_emit_size_changed   (GdkSurface       *surface,
                                            int               width,
                                            int               height);

void       gdk_surface_request_compute_size (GdkSurface      *surface);

GDK_AVAILABLE_IN_ALL
void           gdk_surface_request_motion (GdkSurface *surface);

gboolean       gdk_surface_supports_edge_constraints    (GdkSurface *surface);

GdkSubsurface * gdk_surface_create_subsurface  (GdkSurface          *surface);
gsize           gdk_surface_get_n_subsurfaces  (GdkSurface          *surface);
GdkSubsurface * gdk_surface_get_subsurface     (GdkSurface          *surface,
                                                gsize                idx);

G_END_DECLS
