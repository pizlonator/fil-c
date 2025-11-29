/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
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

#include "config.h"

#include <cairo-gobject.h>

#include "gdksurface.h"

#include "gdkprivate.h"
#include "gdkcontentprovider.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontextprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkmarshalers.h"
#include "gdkpopupprivate.h"
#include "gdkrectangle.h"
#include "gdktoplevelprivate.h"
#include "gdkvulkancontext.h"
#include "gdksubsurfaceprivate.h"

#include <math.h>

#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

/**
 * GdkSurface:
 *
 * A `GdkSurface` is a rectangular region on the screen.
 *
 * It’s a low-level object, used to implement high-level objects
 * such as [GtkWindow](../gtk4/class.Window.html).
 *
 * The surfaces you see in practice are either [iface@Gdk.Toplevel] or
 * [iface@Gdk.Popup], and those interfaces provide much of the required
 * API to interact with these surfaces. Other, more specialized surface
 * types exist, but you will rarely interact with them directly.
 */

typedef struct _GdkSurfacePrivate GdkSurfacePrivate;

struct _GdkSurfacePrivate
{
  gpointer egl_native_window;
#ifdef HAVE_EGL
  EGLSurface egl_surface;
  gboolean egl_surface_high_depth;
#endif

  gpointer widget;
};

enum {
  LAYOUT,
  RENDER,
  EVENT,
  ENTER_MONITOR,
  LEAVE_MONITOR,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CURSOR,
  PROP_DISPLAY,
  PROP_FRAME_CLOCK,
  PROP_MAPPED,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SCALE_FACTOR,
  PROP_SCALE,
  LAST_PROP
};

/* Global info */

static void gdk_surface_finalize     (GObject      *object);

static void gdk_surface_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);
static void gdk_surface_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec);

static void update_cursor               (GdkDisplay *display,
                                         GdkDevice  *device);

static void gdk_surface_queue_set_is_mapped (GdkSurface *surface,
                                             gboolean    is_mapped);


static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkSurface, gdk_surface, G_TYPE_OBJECT)

static gboolean
gdk_surface_real_beep (GdkSurface *surface)
{
  return FALSE;
}

static GdkDisplay *
get_display_for_surface (GdkSurface *primary,
                         GdkSurface *secondary)
{
  GdkDisplay *display = primary->display;

  if (display)
    return display;

  display = secondary->display;

  if (display)
    return display;

  g_warning ("no display for surface, using default");
  return gdk_display_get_default ();
}

static GdkMonitor *
get_monitor_for_rect (GdkDisplay          *display,
                      const GdkRectangle  *rect,
                      void               (*get_bounds) (GdkMonitor   *monitor,
                                                        GdkRectangle *bounds))
{
  int biggest_area = G_MININT;
  GdkMonitor *best_monitor = NULL;
  GdkMonitor *monitor;
  GdkRectangle workarea;
  GdkRectangle intersection;
  GListModel *monitors;
  guint i;

  monitors = gdk_display_get_monitors (display);
  for (i = 0; i < g_list_model_get_n_items (monitors); i++)
    {
      monitor = g_list_model_get_item (monitors, i);
      get_bounds (monitor, &workarea);

      if (gdk_rectangle_intersect (&workarea, rect, &intersection))
        {
          if (intersection.width * intersection.height > biggest_area)
            {
              biggest_area = intersection.width * intersection.height;
              best_monitor = monitor;
            }
        }
      g_object_unref (monitor);
    }

  return best_monitor;
}

static int
get_anchor_x_sign (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_SOUTH_WEST:
      return -1;

    default:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return 0;

    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_SOUTH_EAST:
      return 1;
    }
}

static int
get_anchor_y_sign (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_NORTH_EAST:
      return -1;

    default:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_EAST:
      return 0;

    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_SOUTH:
    case GDK_GRAVITY_SOUTH_EAST:
      return 1;
    }
}

static int
maybe_flip_position (int       bounds_pos,
                     int       bounds_size,
                     int       rect_pos,
                     int       rect_size,
                     int       surface_size,
                     int       rect_sign,
                     int       surface_sign,
                     int       offset,
                     gboolean  flip,
                     gboolean *flipped)
{
  int primary;
  int secondary;

  *flipped = FALSE;
  primary = rect_pos + (1 + rect_sign) * rect_size / 2 + offset - (1 + surface_sign) * surface_size / 2;

  if (!flip || (primary >= bounds_pos && primary + surface_size <= bounds_pos + bounds_size))
    return primary;

  *flipped = TRUE;
  secondary = rect_pos + (1 - rect_sign) * rect_size / 2 - offset - (1 - surface_sign) * surface_size / 2;

  if ((secondary >= bounds_pos && secondary + surface_size <= bounds_pos + bounds_size) || primary > bounds_pos + bounds_size)
    return secondary;

  *flipped = FALSE;
  return primary;
}

GdkMonitor *
gdk_surface_get_layout_monitor (GdkSurface      *surface,
                                GdkPopupLayout  *layout,
                                void           (*get_bounds) (GdkMonitor   *monitor,
                                                              GdkRectangle *bounds))
{
  GdkDisplay *display;
  GdkRectangle root_rect;

  root_rect = *gdk_popup_layout_get_anchor_rect (layout);
  gdk_surface_get_root_coords (surface->parent,
                               root_rect.x,
                               root_rect.y,
                               &root_rect.x,
                               &root_rect.y);

  root_rect.width = MAX (1, root_rect.width);
  root_rect.height = MAX (1, root_rect.height);

  display = get_display_for_surface (surface, surface->transient_for);
  return get_monitor_for_rect (display, &root_rect, get_bounds);
}

void
gdk_surface_layout_popup_helper (GdkSurface     *surface,
                                 int             width,
                                 int             height,
                                 int             shadow_left,
                                 int             shadow_right,
                                 int             shadow_top,
                                 int             shadow_bottom,
                                 GdkMonitor     *monitor,
                                 GdkRectangle   *bounds,
                                 GdkPopupLayout *layout,
                                 GdkRectangle   *out_final_rect)
{
  GdkRectangle root_rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  int rect_anchor_dx;
  int rect_anchor_dy;
  GdkAnchorHints anchor_hints;
  GdkRectangle final_rect;
  gboolean flipped_x;
  gboolean flipped_y;
  int x, y;

  g_return_if_fail (GDK_IS_POPUP (surface));

  root_rect = *gdk_popup_layout_get_anchor_rect (layout);
  gdk_surface_get_root_coords (surface->parent,
                               root_rect.x,
                               root_rect.y,
                               &root_rect.x,
                               &root_rect.y);

  rect_anchor = gdk_popup_layout_get_rect_anchor (layout);
  surface_anchor = gdk_popup_layout_get_surface_anchor (layout);
  gdk_popup_layout_get_offset (layout, &rect_anchor_dx, &rect_anchor_dy);
  anchor_hints = gdk_popup_layout_get_anchor_hints (layout);

  final_rect.width = width - shadow_left - shadow_right;
  final_rect.height = height - shadow_top - shadow_bottom;
  final_rect.x = maybe_flip_position (bounds->x,
                                      bounds->width,
                                      root_rect.x,
                                      root_rect.width,
                                      final_rect.width,
                                      get_anchor_x_sign (rect_anchor),
                                      get_anchor_x_sign (surface_anchor),
                                      rect_anchor_dx,
                                      anchor_hints & GDK_ANCHOR_FLIP_X,
                                      &flipped_x);
  final_rect.y = maybe_flip_position (bounds->y,
                                      bounds->height,
                                      root_rect.y,
                                      root_rect.height,
                                      final_rect.height,
                                      get_anchor_y_sign (rect_anchor),
                                      get_anchor_y_sign (surface_anchor),
                                      rect_anchor_dy,
                                      anchor_hints & GDK_ANCHOR_FLIP_Y,
                                      &flipped_y);

  if (anchor_hints & GDK_ANCHOR_SLIDE_X)
    {
      if (final_rect.x + final_rect.width > bounds->x + bounds->width)
        final_rect.x = bounds->x + bounds->width - final_rect.width;

      if (final_rect.x < bounds->x)
        final_rect.x = bounds->x;
    }

  if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
    {
      if (final_rect.y + final_rect.height > bounds->y + bounds->height)
        final_rect.y = bounds->y + bounds->height - final_rect.height;

      if (final_rect.y < bounds->y)
        final_rect.y = bounds->y;
    }

  if (anchor_hints & GDK_ANCHOR_RESIZE_X)
    {
      if (final_rect.x < bounds->x)
        {
          final_rect.width -= bounds->x - final_rect.x;
          final_rect.x = bounds->x;
        }

      if (final_rect.x + final_rect.width > bounds->x + bounds->width)
        final_rect.width = bounds->x + bounds->width - final_rect.x;
    }

  if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
    {
      if (final_rect.y < bounds->y)
        {
          final_rect.height -= bounds->y - final_rect.y;
          final_rect.y = bounds->y;
        }

      if (final_rect.y + final_rect.height > bounds->y + bounds->height)
        final_rect.height = bounds->y + bounds->height - final_rect.y;
    }

  final_rect.x -= shadow_left;
  final_rect.y -= shadow_top;
  final_rect.width += shadow_left + shadow_right;
  final_rect.height += shadow_top + shadow_bottom;

  gdk_surface_get_origin (surface->parent, &x, &y);
  final_rect.x -= x;
  final_rect.y -= y;

  if (flipped_x)
    {
      rect_anchor = gdk_gravity_flip_horizontally (rect_anchor);
      surface_anchor = gdk_gravity_flip_horizontally (surface_anchor);
    }
  if (flipped_y)
    {
      rect_anchor = gdk_gravity_flip_vertically (rect_anchor);
      surface_anchor = gdk_gravity_flip_vertically (surface_anchor);
    }

  surface->popup.rect_anchor = rect_anchor;
  surface->popup.surface_anchor = surface_anchor;

  *out_final_rect = final_rect;
}

/* Since GdkEvent is a GTypeInstance, GValue can only store it as a pointer,
 * and GClosure does not know how to handle its memory management. To avoid
 * the event going away in the middle of the signal emission, we provide a
 * marshaller that keeps the event alive for the duration of the closure.
 */
static void
gdk_surface_event_marshaller (GClosure     *closure,
                              GValue       *return_value,
                              guint         n_param_values,
                              const GValue *param_values,
                              gpointer      invocation_hint,
                              gpointer      marshal_data)
{
  GdkEvent *event = g_value_get_pointer (&param_values[1]);

  gdk_event_ref (event);

  _gdk_marshal_BOOLEAN__POINTER (closure,
                                 return_value,
                                 n_param_values,
                                 param_values,
                                 invocation_hint,
                                 marshal_data);


  gdk_event_unref (event);
}

static void
gdk_surface_event_marshallerv (GClosure *closure,
                               GValue   *return_value,
                               gpointer  instance,
                               va_list   args,
                               gpointer  marshal_data,
                               int       n_params,
                               GType    *param_types)
{
  va_list args_copy;
  GdkEvent *event;

  G_VA_COPY (args_copy, args);
  event = va_arg (args_copy, gpointer);

  gdk_event_ref (event);

  _gdk_marshal_BOOLEAN__POINTERv (closure,
                                  return_value,
                                  instance,
                                  args,
                                  marshal_data,
                                  n_params,
                                  param_types);

  gdk_event_unref (event);

  va_end (args_copy);
}

static void
gdk_surface_init (GdkSurface *surface)
{
  /* 0-initialization is good for all other fields. */

  surface->state = 0;
  surface->fullscreen_mode = GDK_FULLSCREEN_ON_CURRENT_MONITOR;
  surface->width = 1;
  surface->height = 1;

  surface->alpha = 255;

  surface->device_cursor = g_hash_table_new_full (NULL, NULL,
                                                 NULL, g_object_unref);

  surface->subsurfaces = g_ptr_array_new ();
}

static double
gdk_surface_real_get_scale (GdkSurface *surface)
{
  return 1.0;
}

static GdkSubsurface *
gdk_surface_real_create_subsurface (GdkSurface *surface)
{
  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), OFFLOAD,
                     "Subsurfaces not supported for %s", G_OBJECT_TYPE_NAME (surface));
  return NULL;
}

static void
gdk_surface_constructed (GObject *object)
{
  G_GNUC_UNUSED GdkSurface *surface = GDK_SURFACE (object);

  g_assert (surface->frame_clock != NULL);

  G_OBJECT_CLASS (gdk_surface_parent_class)->constructed (object);
}

static void
gdk_surface_class_init (GdkSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gdk_surface_constructed;
  object_class->finalize = gdk_surface_finalize;
  object_class->set_property = gdk_surface_set_property;
  object_class->get_property = gdk_surface_get_property;

  klass->beep = gdk_surface_real_beep;
  klass->get_scale = gdk_surface_real_get_scale;
  klass->create_subsurface = gdk_surface_real_create_subsurface;

  /**
   * GdkSurface:cursor: (attributes org.gtk.Property.get=gdk_surface_get_cursor org.gtk.Property.set=gdk_surface_set_cursor)
   *
   * The mouse pointer for the `GdkSurface`.
   */
  properties[PROP_CURSOR] =
      g_param_spec_object ("cursor", NULL, NULL,
                           GDK_TYPE_CURSOR,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:display: (attributes org.gtk.Property.get=gdk_surface_get_display)
   *
   * The `GdkDisplay` connection of the surface.
   */
  properties[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:frame-clock: (attributes org.gtk.Property.get=gdk_surface_get_frame_clock)
   *
   * The `GdkFrameClock` of the surface.
   */
  properties[PROP_FRAME_CLOCK] =
      g_param_spec_object ("frame-clock", NULL, NULL,
                           GDK_TYPE_FRAME_CLOCK,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:mapped: (attributes org.gtk.Property.get=gdk_surface_get_mapped)
   *
   * Whether the surface is mapped.
   */
  properties[PROP_MAPPED] =
      g_param_spec_boolean ("mapped", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:width: (attributes org.gtk.Property.get=gdk_surface_get_width)
   *
   * The width of the surface in pixels.
   */
  properties[PROP_WIDTH] =
      g_param_spec_int ("width", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:height: (attributes org.gtk.Property.get=gdk_surface_get_height)
   *
   * The height of the surface, in pixels.
   */
  properties[PROP_HEIGHT] =
      g_param_spec_int ("height", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:scale-factor: (attributes org.gtk.Property.get=gdk_surface_get_scale_factor)
   *
   * The scale factor of the surface.
   *
   * The scale factor is the next larger integer,
   * compared to [property@Gdk.Surface:scale].
   */
  properties[PROP_SCALE_FACTOR] =
      g_param_spec_int ("scale-factor", NULL, NULL,
                        1, G_MAXINT, 1,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:scale: (attributes org.gtk.Property.get=gdk_surface_get_scale)
   *
   * The scale of the surface.
   *
   * Since: 4.12
   */
  properties[PROP_SCALE] =
      g_param_spec_double ("scale", NULL, NULL,
                        1., G_MAXDOUBLE, 1.,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * GdkSurface::layout:
   * @surface: the `GdkSurface`
   * @width: the current width
   * @height: the current height
   *
   * Emitted when the size of @surface is changed, or when relayout should
   * be performed.
   *
   * Surface size is reported in ”application pixels”, not
   * ”device pixels” (see gdk_surface_get_scale_factor()).
   */
  signals[LAYOUT] =
    g_signal_new (g_intern_static_string ("layout"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  _gdk_marshal_VOID__INT_INT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[LAYOUT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_VOID__INT_INTv);

  /**
   * GdkSurface::render:
   * @surface: the `GdkSurface`
   * @region: the region that needs to be redrawn
   *
   * Emitted when part of the surface needs to be redrawn.
   *
   * Returns: %TRUE to indicate that the signal has been handled
   */
  signals[RENDER] =
    g_signal_new (g_intern_static_string ("render"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL,
                  _gdk_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN,
                  1,
                  CAIRO_GOBJECT_TYPE_REGION);
  g_signal_set_va_marshaller (signals[RENDER],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_BOOLEAN__BOXEDv);

  /**
   * GdkSurface::event:
   * @surface: the `GdkSurface`
   * @event: (type Gdk.Event): an input event
   *
   * Emitted when GDK receives an input event for @surface.
   *
   * Returns: %TRUE to indicate that the event has been handled
   */
  signals[EVENT] =
    g_signal_new (g_intern_static_string ("event"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL,
                  gdk_surface_event_marshaller,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_POINTER);
  g_signal_set_va_marshaller (signals[EVENT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              gdk_surface_event_marshallerv);

  /**
   * GdkSurface::enter-monitor:
   * @surface: the `GdkSurface`
   * @monitor: the monitor
   *
   * Emitted when @surface starts being present on the monitor.
   */
  signals[ENTER_MONITOR] =
    g_signal_new (g_intern_static_string ("enter-monitor"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_MONITOR);

  /**
   * GdkSurface::leave-monitor:
   * @surface: the `GdkSurface`
   * @monitor: the monitor
   *
   * Emitted when @surface stops being present on the monitor.
   */
  signals[LEAVE_MONITOR] =
    g_signal_new (g_intern_static_string ("leave-monitor"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_MONITOR);
}

static void
seat_removed_cb (GdkDisplay *display,
                 GdkSeat    *seat,
                 GdkSurface  *surface)
{
  GdkDevice *device = gdk_seat_get_pointer (seat);

  surface->devices_inside = g_list_remove (surface->devices_inside, device);
  g_hash_table_remove (surface->device_cursor, device);
}

static void
gdk_surface_finalize (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);

  g_clear_handle_id (&surface->request_motion_id, g_source_remove);

  g_signal_handlers_disconnect_by_func (surface->display,
                                        seat_removed_cb, surface);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("losing last reference to undestroyed surface");
      _gdk_surface_destroy (surface, FALSE);
    }

  g_clear_pointer (&surface->input_region, cairo_region_destroy);
  g_clear_object (&surface->cursor);
  g_clear_pointer (&surface->device_cursor, g_hash_table_destroy);
  g_clear_pointer (&surface->devices_inside, g_list_free);

  g_clear_object (&surface->display);

  g_clear_pointer (&surface->opaque_region, cairo_region_destroy);

  if (surface->parent)
    surface->parent->children = g_list_remove (surface->parent->children, surface);

  g_assert (surface->subsurfaces->len == 0);

  g_ptr_array_unref (surface->subsurfaces);

  G_OBJECT_CLASS (gdk_surface_parent_class)->finalize (object);
}

static void
gdk_surface_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      gdk_surface_set_cursor (surface, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      surface->display = g_value_dup_object (value);
      g_assert (surface->display != NULL);
      g_signal_connect (surface->display, "seat-removed",
                        G_CALLBACK (seat_removed_cb), surface);
      break;

    case PROP_FRAME_CLOCK:
      gdk_surface_set_frame_clock (surface, GDK_FRAME_CLOCK (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#define GDK_SURFACE_IS_STICKY(surface) (((surface)->state & GDK_TOPLEVEL_STATE_STICKY))

static void
gdk_surface_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      g_value_set_object (value, gdk_surface_get_cursor (surface));
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, surface->display);
      break;

    case PROP_FRAME_CLOCK:
      g_value_set_object (value, surface->frame_clock);
      break;

    case PROP_MAPPED:
      g_value_set_boolean (value, GDK_SURFACE_IS_MAPPED (surface));
      break;

    case PROP_WIDTH:
      g_value_set_int (value, surface->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, surface->height);
      break;

    case PROP_SCALE_FACTOR:
      g_value_set_int (value, gdk_surface_get_scale_factor (surface));
      break;

    case PROP_SCALE:
      g_value_set_double (value, gdk_surface_get_scale (surface));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
_gdk_surface_update_size (GdkSurface *surface)
{
  GSList *l;

  for (l = surface->draw_contexts; l; l = l->next)
    gdk_draw_context_surface_resized (l->data);

  g_object_notify (G_OBJECT (surface), "width");
  g_object_notify (G_OBJECT (surface), "height");
}

/**
 * gdk_surface_new_toplevel: (constructor)
 * @display: the display to create the surface on
 *
 * Creates a new toplevel surface.
 *
 * Returns: (transfer full): the new `GdkSurface`
 */
GdkSurface *
gdk_surface_new_toplevel (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_object_new (GDK_DISPLAY_GET_CLASS (display)->toplevel_type,
                       "display", display,
                       NULL);
}

/**
 * gdk_surface_new_popup: (constructor)
 * @parent: the parent surface to attach the surface to
 * @autohide: whether to hide the surface on outside clicks
 *
 * Create a new popup surface.
 *
 * The surface will be attached to @parent and can be positioned
 * relative to it using [method@Gdk.Popup.present].
 *
 * Returns: (transfer full): a new `GdkSurface`
 */
GdkSurface *
gdk_surface_new_popup (GdkSurface *parent,
                       gboolean    autohide)
{
  GdkSurface *surface;
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SURFACE (parent), NULL);

  display = gdk_surface_get_display (parent);

  surface = g_object_new (GDK_DISPLAY_GET_CLASS (display)->popup_type,
                         "display", display,
                         "parent", parent,
                         NULL);

  surface->autohide = autohide;

  return surface;
}

static void
update_pointer_info_foreach (GdkDisplay           *display,
                             GdkDevice            *device,
                             GdkPointerSurfaceInfo *pointer_info,
                             gpointer              user_data)
{
  GdkSurface *surface = user_data;

  if (pointer_info->surface_under_pointer == surface)
    {
      g_object_unref (pointer_info->surface_under_pointer);
      pointer_info->surface_under_pointer = NULL;
    }
}

static void
surface_remove_from_pointer_info (GdkSurface  *surface,
                                  GdkDisplay *display)
{
  _gdk_display_pointer_info_foreach (display,
                                     update_pointer_info_foreach,
                                     surface);
}

/**
 * _gdk_surface_destroy_hierarchy:
 * @surface: a `GdkSurface`
 * @recursing_native: If %TRUE, then this is being called because a native
 *   parent was destroyed. This generally means that the call to the windowing
 *   system to destroy the surface can be omitted, since it will be destroyed
 *   as a result of the parent being destroyed. Unless @foreign_destroy.
 * @foreign_destroy: If %TRUE, the surface or a parent was destroyed by some
 *   external agency. The surface has already been destroyed and no windowing
 *   system calls should be made. (This may never happen for some windowing
 *   systems.)
 *
 * Internal function to destroy a surface. Like gdk_surface_destroy(),
 * but does not drop the reference count created by gdk_surface_new().
 */
static void
_gdk_surface_destroy_hierarchy (GdkSurface *surface,
                                gboolean    foreign_destroy)
{
  G_GNUC_UNUSED GdkSurfacePrivate *priv = gdk_surface_get_instance_private (surface);

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_SURFACE_GET_CLASS (surface)->destroy (surface, foreign_destroy);

  /* backend must have unset this */
  g_assert (priv->egl_native_window == NULL);

  if (surface->gl_paint_context)
    {
      /* Make sure to destroy if current */
      g_object_run_dispose (G_OBJECT (surface->gl_paint_context));
      g_object_unref (surface->gl_paint_context);
      surface->gl_paint_context = NULL;
    }

  if (surface->frame_clock)
    {
      if (surface->parent == NULL)
        g_object_run_dispose (G_OBJECT (surface->frame_clock));
      gdk_surface_set_frame_clock (surface, NULL);
    }

  _gdk_surface_clear_update_area (surface);

  g_clear_handle_id (&surface->set_is_mapped_source_id, g_source_remove);
  surface->is_mapped = FALSE;
  surface->pending_is_mapped = FALSE;

  surface->destroyed = TRUE;

  surface_remove_from_pointer_info (surface, surface->display);

  if (GDK_IS_TOPLEVEL (surface))
    g_object_notify (G_OBJECT (surface), "state");
  g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
}

/**
 * _gdk_surface_destroy:
 * @surface: a `GdkSurface`
 * @foreign_destroy: If %TRUE, the surface or a parent was destroyed by some
 *   external agency. The surface has already been destroyed and no windowing
 *   system calls should be made. (This may never happen for some windowing
 *   systems.)
 *
 * Internal function to destroy a surface. Like gdk_surface_destroy(),
 * but does not drop the reference count created by gdk_surface_new().
 */
void
_gdk_surface_destroy (GdkSurface *surface,
                      gboolean   foreign_destroy)
{
  _gdk_surface_destroy_hierarchy (surface, foreign_destroy);
}

/**
 * gdk_surface_destroy:
 * @surface: a `GdkSurface`
 *
 * Destroys the window system resources associated with @surface and
 * decrements @surface's reference count.
 *
 * The window system resources for all children of @surface are also
 * destroyed, but the children’s reference counts are not decremented.
 *
 * Note that a surface will not be destroyed automatically when its
 * reference count reaches zero. You must call this function yourself
 * before that happens.
 */
void
gdk_surface_destroy (GdkSurface *surface)
{
  _gdk_surface_destroy_hierarchy (surface, FALSE);
  g_object_unref (surface);
}

void
gdk_surface_set_widget (GdkSurface *self,
                        gpointer    widget)
{
  GdkSurfacePrivate *priv = gdk_surface_get_instance_private (self);

  priv->widget = widget;
}

gpointer
gdk_surface_get_widget (GdkSurface *self)
{
  GdkSurfacePrivate *priv = gdk_surface_get_instance_private (self);

  return priv->widget;
}

/**
 * gdk_surface_get_display: (attributes org.gtk.Method.get_property=display)
 * @surface: a `GdkSurface`
 *
 * Gets the `GdkDisplay` associated with a `GdkSurface`.
 *
 * Returns: (transfer none): the `GdkDisplay` associated with @surface
 */
GdkDisplay *
gdk_surface_get_display (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->display;
}
/**
 * gdk_surface_is_destroyed:
 * @surface: a `GdkSurface`
 *
 * Check to see if a surface is destroyed.
 *
 * Returns: %TRUE if the surface is destroyed
 */
gboolean
gdk_surface_is_destroyed (GdkSurface *surface)
{
  return GDK_SURFACE_DESTROYED (surface);
}

/**
 * gdk_surface_get_mapped: (attributes org.gtk.Method.get_property=mapped)
 * @surface: a `GdkSurface`
 *
 * Checks whether the surface has been mapped.
 *
 * A surface is mapped with [method@Gdk.Toplevel.present]
 * or [method@Gdk.Popup.present].
 *
 * Returns: %TRUE if the surface is mapped
 */
gboolean
gdk_surface_get_mapped (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return GDK_SURFACE_IS_MAPPED (surface);
}

void
gdk_surface_set_egl_native_window (GdkSurface *self,
                                   gpointer    native_window)
{
#ifdef HAVE_EGL
  GdkSurfacePrivate *priv = gdk_surface_get_instance_private (self);

  /* This checks that all EGL platforms we support conform to the same struct sizes.
   * When this ever fails, there will be some fun times happening for whoever tries
   * this weird EGL backend... */
  G_STATIC_ASSERT (sizeof (gpointer) == sizeof (EGLNativeWindowType));

  if (priv->egl_surface != NULL)
    {
      GdkDisplay *display = gdk_surface_get_display (self);

      gdk_gl_context_clear_current_if_surface (self);
      eglDestroySurface (gdk_display_get_egl_display (display), priv->egl_surface);
      priv->egl_surface = NULL;
    }

  priv->egl_native_window = native_window;
}

gpointer /* EGLSurface */
gdk_surface_get_egl_surface (GdkSurface *self)
{
  GdkSurfacePrivate *priv = gdk_surface_get_instance_private (self);

  return priv->egl_surface;
}

void
gdk_surface_ensure_egl_surface (GdkSurface *self,
                                gboolean    high_depth)
{
  GdkSurfacePrivate *priv = gdk_surface_get_instance_private (self);
  GdkDisplay *display = gdk_surface_get_display (self);

  g_return_if_fail (priv->egl_native_window != NULL);

  if (priv->egl_surface_high_depth != high_depth &&
      priv->egl_surface != NULL &&
      gdk_display_get_egl_config_high_depth (display) != gdk_display_get_egl_config (display))
    {
      gdk_gl_context_clear_current_if_surface (self);
      eglDestroySurface (gdk_display_get_egl_display (display), priv->egl_surface);
      priv->egl_surface = NULL;
    }

  if (priv->egl_surface == NULL)
    {
      priv->egl_surface = eglCreateWindowSurface (gdk_display_get_egl_display (display),
                                                  high_depth ? gdk_display_get_egl_config_high_depth (display)
                                                             : gdk_display_get_egl_config (display),
                                                  (EGLNativeWindowType) priv->egl_native_window,
                                                  NULL);
      priv->egl_surface_high_depth = high_depth;
    }
#endif
}

GdkGLContext *
gdk_surface_get_paint_gl_context (GdkSurface  *surface,
                                  GError     **error)
{
  if (!gdk_display_prepare_gl (surface->display, error))
    return NULL;

  if (surface->gl_paint_context == NULL)
    {
      surface->gl_paint_context = gdk_surface_create_gl_context (surface, error);
      if (surface->gl_paint_context == NULL)
        return NULL;
    }

  if (!gdk_gl_context_realize (surface->gl_paint_context, error))
    {
      g_clear_object (&surface->gl_paint_context);
      return NULL;
    }

  return surface->gl_paint_context;
}

/**
 * gdk_surface_create_gl_context:
 * @surface: a `GdkSurface`
 * @error: return location for an error
 *
 * Creates a new `GdkGLContext` for the `GdkSurface`.
 *
 * The context is disconnected from any particular surface or surface.
 * If the creation of the `GdkGLContext` failed, @error will be set.
 * Before using the returned `GdkGLContext`, you will need to
 * call [method@Gdk.GLContext.make_current] or [method@Gdk.GLContext.realize].
 *
 * Returns: (transfer full): the newly created `GdkGLContext`
 */
GdkGLContext *
gdk_surface_create_gl_context (GdkSurface   *surface,
                               GError      **error)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!gdk_display_prepare_gl (surface->display, error))
    return NULL;

  return gdk_gl_context_new (surface->display, surface);
}

/**
 * gdk_surface_create_cairo_context:
 * @surface: a `GdkSurface`
 *
 * Creates a new `GdkCairoContext` for rendering on @surface.
 *
 * Returns: (transfer full): the newly created `GdkCairoContext`
 */
GdkCairoContext *
gdk_surface_create_cairo_context (GdkSurface *surface)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  display = surface->display;

  return g_object_new (GDK_DISPLAY_GET_CLASS (display)->cairo_context_type,
                       "surface", surface,
                       NULL);
}

/**
 * gdk_surface_create_vulkan_context:
 * @surface: a `GdkSurface`
 * @error: return location for an error
 *
 * Sets an error and returns %NULL.
 *
 * Returns: (transfer full): %NULL
 *
 * Deprecated: 4.14: GTK does not expose any Vulkan internals. This
 *   function is a leftover that was accidentally exposed.
 */
GdkVulkanContext *
gdk_surface_create_vulkan_context (GdkSurface  *surface,
                                   GError    **error)
{
  g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
               "GTK does not expose Vulkan internals.");
  return FALSE;
}

static gboolean
gdk_surface_is_toplevel_frozen (GdkSurface *surface)
{
  return surface->update_and_descendants_freeze_count > 0;
}

static void
gdk_surface_schedule_update (GdkSurface *surface)
{
  GdkFrameClock *frame_clock;

  g_return_if_fail (surface);

  surface->pending_phases |= GDK_FRAME_CLOCK_PHASE_PAINT;

  if (surface->update_freeze_count ||
      gdk_surface_is_toplevel_frozen (surface))
    return;

  /* If there's no frame clock (a foreign surface), then the invalid
   * region will just stick around unless gdk_surface_process_updates()
   * is called. */
  frame_clock = gdk_surface_get_frame_clock (surface);
  if (frame_clock)
    gdk_frame_clock_request_phase (gdk_surface_get_frame_clock (surface),
                                   GDK_FRAME_CLOCK_PHASE_PAINT);
}

static void
gdk_surface_layout_on_clock (GdkFrameClock *clock,
                             void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkSurfaceClass *class;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    return;

  surface->pending_phases &= ~GDK_FRAME_CLOCK_PHASE_LAYOUT;

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->compute_size)
    {
      if (class->compute_size (surface))
        return;
    }

  g_signal_emit (surface, signals[LAYOUT], 0, surface->width, surface->height);
}

/**
 * gdk_surface_request_layout:
 * @surface: a `GdkSurface`
 *
 * Request a layout phase from the surface's frame clock.
 *
 * See [method@Gdk.FrameClock.request_phase].
 */
void
gdk_surface_request_layout (GdkSurface *surface)
{
  GdkSurfaceClass *class;
  GdkFrameClock *frame_clock;

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->request_layout)
    class->request_layout (surface);

  frame_clock = gdk_surface_get_frame_clock (surface);
  g_return_if_fail (frame_clock);

  gdk_frame_clock_request_phase (frame_clock,
                                 GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

static void
gdk_surface_paint_on_clock (GdkFrameClock *clock,
                            void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);
  cairo_region_t *expose_region;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !surface->update_area ||
      surface->update_freeze_count ||
      gdk_surface_is_toplevel_frozen (surface))
    return;

  surface->pending_phases &= ~GDK_FRAME_CLOCK_PHASE_PAINT;
  expose_region = surface->update_area;
  surface->update_area = NULL;

  if (GDK_SURFACE_IS_MAPPED (surface))
    {
      gboolean handled;

      g_object_ref (surface);

      g_signal_emit (surface, signals[RENDER], 0, expose_region, &handled);

      g_object_unref (surface);
    }

  cairo_region_destroy (expose_region);
}

/*
 * gdk_surface_invalidate_rect:
 * @surface: a `GdkSurface`
 * @rect: (nullable): rectangle to invalidate or %NULL to
 *   invalidate the whole surface
 *
 * Invalidate a rectangular region of @surface.
 *
 * This is a convenience wrapper around
 * [method@Gdk.Surface.invalidate_region].
 */
void
gdk_surface_invalidate_rect (GdkSurface        *surface,
                             const GdkRectangle *rect)
{
  GdkRectangle surface_rect;
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (!GDK_SURFACE_IS_MAPPED (surface))
    return;

  if (!rect)
    {
      surface_rect.x = 0;
      surface_rect.y = 0;
      surface_rect.width = surface->width;
      surface_rect.height = surface->height;
      rect = &surface_rect;
    }

  region = cairo_region_create_rectangle (rect);
  gdk_surface_invalidate_region (surface, region);
  cairo_region_destroy (region);
}

static void
impl_surface_add_update_area (GdkSurface     *impl_surface,
                              cairo_region_t *region)
{
  if (impl_surface->update_area)
    cairo_region_union (impl_surface->update_area, region);
  else
    {
      impl_surface->update_area = cairo_region_copy (region);
      gdk_surface_schedule_update (impl_surface);
    }
}

/**
 * gdk_surface_queue_render:
 * @surface: a `GdkSurface`
 *
 * Forces a [signal@Gdk.Surface::render] signal emission for @surface
 * to be scheduled.
 *
 * This function is useful for implementations that track invalid
 * regions on their own.
 */
void
gdk_surface_queue_render (GdkSurface *surface)
{
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  region = cairo_region_create ();
  impl_surface_add_update_area (surface, region);
  cairo_region_destroy (region);
}

/*
 * gdk_surface_invalidate_region:
 * @surface: a `GdkSurface`
 * @region: a `cairo_region_t`
 *
 * Adds @region to the update area for @surface.
 *
 * The update area is the region that needs to be redrawn,
 * or “dirty region.”
 *
 * GDK will process all updates whenever the frame clock schedules
 * a redraw, so there’s no need to do forces redraws manually, you
 * just need to invalidate regions that you know should be redrawn.
 */
void
gdk_surface_invalidate_region (GdkSurface          *surface,
                               const cairo_region_t *region)
{
  cairo_region_t *visible_region;
  cairo_rectangle_int_t r;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (!GDK_SURFACE_IS_MAPPED (surface))
    return;

  if (cairo_region_is_empty (region))
    return;

  r.x = 0;
  r.y = 0;
  r.width = surface->width;
  r.height = surface->height;

  visible_region = cairo_region_copy (region);

  cairo_region_intersect_rectangle (visible_region, &r);
  impl_surface_add_update_area (surface, visible_region);

  cairo_region_destroy (visible_region);
}

/*
 * _gdk_surface_clear_update_area:
 * @surface: a `GdkSurface`
 *
 * Internal function to clear the update area for a surface.
 * This is called when the surface is hidden or destroyed.
 */
void
_gdk_surface_clear_update_area (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->update_area)
    {
      cairo_region_destroy (surface->update_area);
      surface->update_area = NULL;
    }
}

/*
 * gdk_surface_freeze_updates:
 * @surface: a `GdkSurface`
 *
 * Temporarily freezes a surface such that it won’t receive expose
 * events.  The surface will begin receiving expose events again when
 * gdk_surface_thaw_updates() is called. If gdk_surface_freeze_updates()
 * has been called more than once, gdk_surface_thaw_updates() must be
 * called an equal number of times to begin processing exposes.
 */
void
gdk_surface_freeze_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_DEBUG_CHECK (NO_VSYNC))
    return;

  surface->update_freeze_count++;
  if (surface->update_freeze_count == 1)
    _gdk_frame_clock_uninhibit_freeze (surface->frame_clock);
}

static gboolean
request_motion_cb (void *data)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);

  if (clock)
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS);
  surface->request_motion_id = 0;

  return G_SOURCE_REMOVE;
}


/*
 * gdk_surface_thaw_updates:
 * @surface: a `GdkSurface`
 *
 * Thaws a surface frozen with gdk_surface_freeze_updates(). Note that this
 * will not necessarily schedule updates if the surface freeze count reaches
 * zero.
 */
void
gdk_surface_thaw_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_DEBUG_CHECK (NO_VSYNC))
    return;

  g_return_if_fail (surface->update_freeze_count > 0);

  if (--surface->update_freeze_count == 0)
    {
      GdkFrameClock *frame_clock = surface->frame_clock;

      _gdk_frame_clock_inhibit_freeze (frame_clock);

      if (surface->pending_phases)
        gdk_frame_clock_request_phase (frame_clock, surface->pending_phases);

      if (surface->request_motion && surface->request_motion_id == 0)
        {
          surface->request_motion_id =
            g_idle_add_full (GDK_PRIORITY_REDRAW + 20, request_motion_cb, surface, NULL);
          gdk_source_set_static_name_by_id (surface->request_motion_id, "[gtk] request_motion_cb");
        }
    }
}

/*
 * gdk_surface_constrain_size:
 * @geometry: a `GdkGeometry` structure
 * @flags: a mask indicating what portions of @geometry are set
 * @width: desired width of surface
 * @height: desired height of the surface
 * @new_width: (out): location to store resulting width
 * @new_height: (out): location to store resulting height
 *
 * Constrains a desired width and height according to a
 * set of geometry hints (such as minimum and maximum size).
 */
void
gdk_surface_constrain_size (GdkGeometry    *geometry,
                            GdkSurfaceHints  flags,
                            int             width,
                            int             height,
                            int            *new_width,
                            int            *new_height)
{
  /* This routine is partially borrowed from fvwm.
   *
   * Copyright 1993, Robert Nation
   *     You may use this code for any purpose, as long as the original
   *     copyright remains in the source code and all documentation
   *
   * which in turn borrows parts of the algorithm from uwm
   */
  int min_width = 0;
  int min_height = 0;
  int max_width = G_MAXINT;
  int max_height = G_MAXINT;

  if (flags & GDK_HINT_MIN_SIZE)
    {
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      max_width = geometry->max_width ;
      max_height = geometry->max_height;
    }

  /* clamp width and height to min and max values
   */
  width = CLAMP (width, min_width, max_width);
  height = CLAMP (height, min_height, max_height);

  *new_width = width;
  *new_height = height;
}

/**
 * gdk_surface_get_device_position:
 * @surface: a `GdkSurface`
 * @device: pointer `GdkDevice` to query to
 * @x: (out) (optional): return location for the X coordinate of @device
 * @y: (out) (optional): return location for the Y coordinate of @device
 * @mask: (out) (optional): return location for the modifier mask
 *
 * Obtains the current device position and modifier state.
 *
 * The position is given in coordinates relative to the upper
 * left corner of @surface.
 *
 * Return: %TRUE if the device is over the surface
 */
gboolean
gdk_surface_get_device_position (GdkSurface       *surface,
                                 GdkDevice       *device,
                                 double          *x,
                                 double          *y,
                                 GdkModifierType *mask)
{
  double tmp_x, tmp_y;
  GdkModifierType tmp_mask;
  gboolean ret;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, FALSE);

  tmp_x = 0;
  tmp_y = 0;
  tmp_mask = 0;

  ret = GDK_SURFACE_GET_CLASS (surface)->get_device_state (surface,
                                                           device,
                                                           &tmp_x, &tmp_y,
                                                           &tmp_mask);

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;

  return ret;
}

/**
 * gdk_surface_hide:
 * @surface: a `GdkSurface`
 *
 * Hide the surface.
 *
 * For toplevel surfaces, withdraws them, so they will no longer be
 * known to the window manager; for all surfaces, unmaps them, so
 * they won’t be displayed. Normally done automatically as
 * part of [gtk_widget_hide()](../gtk4/method.Widget.hide.html).
 */
void
gdk_surface_hide (GdkSurface *surface)
{
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  gdk_surface_queue_set_is_mapped (surface, FALSE);

  if (was_mapped)
    {
      GdkDisplay *display;
      GdkSeat *seat;
      GList *devices, *d;

      /* May need to break grabs on children */
      display = surface->display;
      seat = gdk_display_get_default_seat (display);
      if (seat)
        {
          devices = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);
          devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
          devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));
        }
      else
        devices = NULL;

      for (d = devices; d; d = d->next)
        {
          GdkDevice *device = d->data;

          if (_gdk_display_end_device_grab (display,
                                            device,
                                            _gdk_display_get_next_serial (display),
                                            surface,
                                            TRUE))
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
              gdk_device_ungrab (device, GDK_CURRENT_TIME);
G_GNUC_END_IGNORE_DEPRECATIONS
            }
        }

      g_list_free (devices);
    }

  GDK_SURFACE_GET_CLASS (surface)->hide (surface);

  surface->popup.rect_anchor = 0;
  surface->popup.surface_anchor = 0;
  surface->x = 0;
  surface->y = 0;
}

static void
gdk_surface_set_cursor_internal (GdkSurface *surface,
                                 GdkDevice *device,
                                 GdkCursor *cursor)
{
  GdkPointerSurfaceInfo *pointer_info;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (surface->display == gdk_device_get_display (device));

  pointer_info = _gdk_display_get_pointer_info (surface->display, device);

  if (surface == pointer_info->surface_under_pointer)
    update_cursor (surface->display, device);
}

/**
 * gdk_surface_get_cursor: (attributes org.gtk.Method.get_property=cursor)
 * @surface: a `GdkSurface`
 *
 * Retrieves a `GdkCursor` pointer for the cursor currently set on the
 * `GdkSurface`.
 *
 * If the return value is %NULL then there is no custom cursor set on
 * the surface, and it is using the cursor for its parent surface.
 *
 * Use [method@Gdk.Surface.set_cursor] to unset the cursor of the surface.
 *
 * Returns: (nullable) (transfer none): a `GdkCursor`
 */
GdkCursor *
gdk_surface_get_cursor (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->cursor;
}

/**
 * gdk_surface_set_cursor: (attributes org.gtk.Method.set_property=cursor)
 * @surface: a `GdkSurface`
 * @cursor: (nullable): a `GdkCursor`
 *
 * Sets the default mouse pointer for a `GdkSurface`.
 *
 * Passing %NULL for the @cursor argument means that @surface will use
 * the cursor of its parent surface. Most surfaces should use this default.
 * Note that @cursor must be for the same display as @surface.
 *
 * Use [ctor@Gdk.Cursor.new_from_name] or [ctor@Gdk.Cursor.new_from_texture]
 * to create the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR.
 */
void
gdk_surface_set_cursor (GdkSurface *surface,
                        GdkCursor  *cursor)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->cursor)
    {
      g_object_unref (surface->cursor);
      surface->cursor = NULL;
    }

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkDevice *device;
      GList *seats, *s;

      if (cursor)
        surface->cursor = g_object_ref (cursor);

      seats = gdk_display_list_seats (surface->display);

      for (s = seats; s; s = s->next)
        {
          GList *devices, *d;

          device = gdk_seat_get_pointer (s->data);
          gdk_surface_set_cursor_internal (surface, device, surface->cursor);

          devices = gdk_seat_get_devices (s->data, GDK_SEAT_CAPABILITY_TABLET_STYLUS);
          for (d = devices; d; d = d->next)
            {
              device = d->data;
              gdk_surface_set_cursor_internal (surface, device, surface->cursor);
            }
          g_list_free (devices);
        }

      g_list_free (seats);
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_CURSOR]);
    }
}

/**
 * gdk_surface_get_device_cursor:
 * @surface: a `GdkSurface`
 * @device: a pointer `GdkDevice`
 *
 * Retrieves a `GdkCursor` pointer for the @device currently set on the
 * specified `GdkSurface`.
 *
 * If the return value is %NULL then there is no custom cursor set on the
 * specified surface, and it is using the cursor for its parent surface.
 *
 * Use [method@Gdk.Surface.set_cursor] to unset the cursor of the surface.
 *
 * Returns: (nullable) (transfer none): a `GdkCursor`
 */
GdkCursor *
gdk_surface_get_device_cursor (GdkSurface *surface,
                               GdkDevice  *device)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  return g_hash_table_lookup (surface->device_cursor, device);
}

/**
 * gdk_surface_set_device_cursor:
 * @surface: a `GdkSurface`
 * @device: a pointer `GdkDevice`
 * @cursor: a `GdkCursor`
 *
 * Sets a specific `GdkCursor` for a given device when it gets inside @surface.
 *
 * Passing %NULL for the @cursor argument means that @surface will use the
 * cursor of its parent surface. Most surfaces should use this default.
 *
 * Use [ctor@Gdk.Cursor.new_from_name] or [ctor@Gdk.Cursor.new_from_texture]
 * to create the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR.
 */
void
gdk_surface_set_device_cursor (GdkSurface *surface,
                               GdkDevice  *device,
                               GdkCursor  *cursor)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);

  if (!cursor)
    g_hash_table_remove (surface->device_cursor, device);
  else
    g_hash_table_replace (surface->device_cursor, device, g_object_ref (cursor));

  gdk_surface_set_cursor_internal (surface, device, cursor);
}

/*
 * gdk_surface_get_geometry:
 * @surface: a `GdkSurface`
 * @x: (out) (optional): return location for X coordinate of surface (relative to its parent)
 * @y: (out) (optional): return location for Y coordinate of surface (relative to its parent)
 * @width: (out) (optional): return location for width of surface
 * @height: (out) (optional): return location for height of surface
 *
 * Get the geometry of the surface.
 *
 * The X and Y coordinates returned are relative to the parent surface
 * of @surface, which for toplevels usually means relative to the
 * surface decorations (titlebar, etc.) rather than relative to the
 * root window (screen-size background window).
 *
 * On the X11 platform, the geometry is obtained from the X server, so
 * reflects the latest position of @surface; this may be out-of-sync with
 * the position of @surface delivered in the most-recently-processed
 * `GdkEventConfigure`. [method@Gdk.Surface.get_position] in contrast gets
 * the position from the most recent configure event.
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren’t interested in getting the value of that field.
 *
 * Note: If @surface is not a toplevel, it is much better to call
 * [method@Gdk.Surface.get_position], [method@Gdk.Surface.get_width] and
 * [method@Gdk.Surface.get_height] instead, because it avoids the roundtrip
 * to the X server and because these functions support the full 32-bit
 * coordinate space, whereas gdk_surface_get_geometry() is restricted to
 * the 16-bit coordinates of X11.
 */
void
gdk_surface_get_geometry (GdkSurface *surface,
                          int        *x,
                          int        *y,
                          int        *width,
                          int        *height)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_SURFACE_GET_CLASS (surface)->get_geometry (surface, x, y, width, height);
}

/**
 * gdk_surface_get_width: (attributes org.gtk.Method.get_property=width)
 * @surface: a `GdkSurface`
 *
 * Returns the width of the given @surface.
 *
 * Surface size is reported in ”application pixels”, not
 * ”device pixels” (see [method@Gdk.Surface.get_scale_factor]).
 *
 * Returns: The width of @surface
 */
int
gdk_surface_get_width (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  return surface->width;
}

/**
 * gdk_surface_get_height: (attributes org.gtk.Method.get_property=height)
 * @surface: a `GdkSurface`
 *
 * Returns the height of the given @surface.
 *
 * Surface size is reported in ”application pixels”, not
 * ”device pixels” (see [method@Gdk.Surface.get_scale_factor]).
 *
 * Returns: The height of @surface
 */
int
gdk_surface_get_height (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  return surface->height;
}

/*
 * gdk_surface_get_origin:
 * @surface: a `GdkSurface`
 * @x: (out): return location for X coordinate
 * @y: (out): return location for Y coordinate
 *
 * Obtains the position of a surface in root window coordinates.
 *
 * (Compare with gdk_surface_get_position() and
 * gdk_surface_get_geometry() which return the position
 * of a surface relative to its parent surface.)
 */
void
gdk_surface_get_origin (GdkSurface *surface,
                        int        *x,
                        int        *y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  gdk_surface_get_root_coords (surface, 0, 0, x, y);
}

/*
 * gdk_surface_get_root_coords:
 * @surface: a `GdkSurface`
 * @x: X coordinate in surface
 * @y: Y coordinate in surface
 * @root_x: (out): return location for X coordinate
 * @root_y: (out): return location for Y coordinate
 *
 * Obtains the position of a surface position in root
 * window coordinates. This is similar to
 * gdk_surface_get_origin() but allows you to pass
 * in any position in the surface, not just the origin.
 */
void
gdk_surface_get_root_coords (GdkSurface *surface,
                             int        x,
                             int        y,
                             int       *root_x,
                             int       *root_y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    {
      *root_x = 0;
      *root_y = 0;
      return;
    }

  GDK_SURFACE_GET_CLASS (surface)->get_root_coords (surface, x, y, root_x, root_y);
}

/**
 * gdk_surface_set_input_region:
 * @surface: a `GdkSurface`
 * @region: region of surface to be reactive
 *
 * Apply the region to the surface for the purpose of event
 * handling.
 *
 * Mouse events which happen while the pointer position corresponds
 * to an unset bit in the mask will be passed on the surface below
 * @surface.
 *
 * An input region is typically used with RGBA surfaces. The alpha
 * channel of the surface defines which pixels are invisible and
 * allows for nicely antialiased borders, and the input region
 * controls where the surface is “clickable”.
 *
 * Use [method@Gdk.Display.supports_input_shapes] to find out if
 * a particular backend supports input regions.
 */
void
gdk_surface_set_input_region (GdkSurface     *surface,
                              cairo_region_t *region)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (cairo_region_equal (surface->input_region, region))
    return;

  if (surface->input_region)
    cairo_region_destroy (surface->input_region);

  if (region)
    surface->input_region = cairo_region_copy (region);
  else
    surface->input_region = NULL;

  GDK_SURFACE_GET_CLASS (surface)->set_input_region (surface, surface->input_region);
}

static void
update_cursor (GdkDisplay *display,
               GdkDevice  *device)
{
  GdkSurface *cursor_surface;
  GdkSurface *pointer_surface;
  GdkPointerSurfaceInfo *pointer_info;
  GdkDeviceGrabInfo *grab;
  GdkCursor *cursor;

  g_assert (display);
  g_assert (device);

  pointer_info = _gdk_display_get_pointer_info (display, device);
  pointer_surface = pointer_info->surface_under_pointer;

  /* We ignore the serials here and just pick the last grab
     we've sent, as that would shortly be used anyway. */
  grab = _gdk_display_get_last_device_grab (display, device);
  if (grab != NULL)
    {
      /* use the cursor from the grab surface */
      cursor_surface = grab->surface;
    }
  else
    {
      /* otherwise use the cursor from the pointer surface */
      cursor_surface = pointer_surface;
    }

  cursor = g_hash_table_lookup (cursor_surface->device_cursor, device);

  if (!cursor)
    cursor = cursor_surface->cursor;

  GDK_DEVICE_GET_CLASS (device)->set_surface_cursor (device, pointer_surface, cursor);
}

/**
 * gdk_surface_beep:
 * @surface: a toplevel `GdkSurface`
 *
 * Emits a short beep associated to @surface.
 *
 * If the display of @surface does not support per-surface beeps,
 * emits a short beep on the display just as [method@Gdk.Display.beep].
 */
void
gdk_surface_beep (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_GET_CLASS (surface)->beep (surface))
    return;

  gdk_display_beep (surface->display);
}

void
_gdk_display_set_surface_under_pointer (GdkDisplay *display,
                                        GdkDevice  *device,
                                        GdkSurface  *surface)
{
  GdkPointerSurfaceInfo *device_info;

  device_info = _gdk_display_get_pointer_info (display, device);

  if (device_info->surface_under_pointer)
    g_object_unref (device_info->surface_under_pointer);
  device_info->surface_under_pointer = surface;

  if (surface)
    {
      g_object_ref (surface);
      update_cursor (display, device);
    }
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
                             GDK_BUTTON2_MASK | \
                             GDK_BUTTON3_MASK | \
                             GDK_BUTTON4_MASK | \
                             GDK_BUTTON5_MASK)

void
_gdk_windowing_got_event (GdkDisplay *display,
                          GList      *event_link,
                          GdkEvent   *event,
                          gulong      serial)
{
  GdkSurface *event_surface = NULL;
  gboolean unlink_event = FALSE;
  GdkDeviceGrabInfo *button_release_grab;
  GdkPointerSurfaceInfo *pointer_info = NULL;
  GdkDevice *device;
  GdkEventType type;
  guint32 timestamp;

  _gdk_display_update_last_event (display, event);

  device = gdk_event_get_device (event);
  timestamp = gdk_event_get_time (event);

  if (device)
    {
      if (timestamp != GDK_CURRENT_TIME)
        gdk_device_set_timestamp (device, timestamp);

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD &&
          gdk_device_get_source (device) != GDK_SOURCE_TABLET_PAD)
        {
          pointer_info = _gdk_display_get_pointer_info (display, device);
          pointer_info->last_physical_device = device;
        }

      _gdk_display_device_grab_update (display, device, serial);
    }

  event_surface = gdk_event_get_surface (event);
  if (!event_surface)
    goto out;

  type = gdk_event_get_event_type (event);
  if (type == GDK_ENTER_NOTIFY)
    _gdk_display_set_surface_under_pointer (display, device, event_surface);
  else if (type == GDK_LEAVE_NOTIFY)
    _gdk_display_set_surface_under_pointer (display, device, NULL);

  if (type == GDK_BUTTON_PRESS)
    {
      GdkSurface *grab_surface;
      gboolean owner_events;

      if (!gdk_device_grab_info (display, device, &grab_surface, &owner_events))
        {
          _gdk_display_add_device_grab (display,
                                        device,
                                        event_surface,
                                        FALSE,
                                        GDK_ALL_EVENTS_MASK,
                                        serial,
                                        gdk_event_get_time (event),
                                        TRUE);
          _gdk_display_device_grab_update (display, device, serial);
        }
    }
  else if (type == GDK_BUTTON_RELEASE ||
           type == GDK_TOUCH_CANCEL ||
           type == GDK_TOUCH_END)
    {
      if (type == GDK_BUTTON_RELEASE ||
          gdk_event_get_pointer_emulated (event))
        {
          button_release_grab =
            _gdk_display_has_device_grab (display, device, serial);

          if (button_release_grab &&
              button_release_grab->implicit &&
              (gdk_event_get_modifier_state (event) & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (gdk_button_event_get_button (event) - 1))) == 0)
            {
              button_release_grab->serial_end = serial;
              button_release_grab->implicit_ungrab = FALSE;
              _gdk_display_device_grab_update (display, device, serial);
            }
        }
    }

 out:
  if (unlink_event)
    {
      _gdk_event_queue_remove_link (display, event_link);
      g_list_free_1 (event_link);
      gdk_event_unref (event);
    }

  /* This does two things - first it sees if there are motions at the
   * end of the queue that can be compressed. Second, if there is just
   * a single motion that won't be dispatched because it is a compression
   * candidate it queues up flushing the event queue.
   */
  _gdk_event_queue_handle_motion_compression (display);
  gdk_event_queue_handle_scroll_compression (display);

  if (event_surface)
    {
      GdkFrameClock *clock = gdk_surface_get_frame_clock (event_surface);

      if (clock) /* might be NULL if surface was destroyed */
        gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS);
    }
}

/**
 * gdk_surface_create_similar_surface:
 * @surface: surface to make new surface similar to
 * @content: the content for the new surface
 * @width: width of the new surface
 * @height: height of the new surface
 *
 * Create a new Cairo surface that is as compatible as possible with the
 * given @surface.
 *
 * For example the new surface will have the same fallback resolution
 * and font options as @surface. Generally, the new surface will also
 * use the same backend as @surface, unless that is not possible for
 * some reason. The type of the returned surface may be examined with
 * cairo_surface_get_type().
 *
 * Initially the surface contents are all 0 (transparent if contents
 * have transparency, black otherwise.)
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a “nil” surface if @other is already in an error state
 * or any other error occurs.
 *
 * Returns: a pointer to the newly allocated surface. The caller
 *   owns the surface and should call cairo_surface_destroy() when done
 *   with it.
 *
 * Deprecated: 4.12: Create a suitable cairo image surface yourself
 */
cairo_surface_t *
gdk_surface_create_similar_surface (GdkSurface      *surface,
                                    cairo_content_t  content,
                                    int              width,
                                    int              height)
{
  cairo_surface_t *similar_surface;
  int scale;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  scale = gdk_surface_get_scale_factor (surface);

  similar_surface = cairo_image_surface_create (content == CAIRO_CONTENT_COLOR ? CAIRO_FORMAT_RGB24 :
                                                content == CAIRO_CONTENT_ALPHA ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_ARGB32,
                                                width * scale, height * scale);
  cairo_surface_set_device_scale (similar_surface, scale, scale);

  return similar_surface;
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_surface_destroy_notify (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->destroy_notify (surface);
}

/**
 * gdk_drag_begin:
 * @surface: the source surface for this drag
 * @device: the device that controls this drag
 * @content: (transfer none): the offered content
 * @actions: the actions supported by this drag
 * @dx: the x offset to @device's position where the drag nominally started
 * @dy: the y offset to @device's position where the drag nominally started
 *
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source. After this call, you
 * probably want to set up the drag icon using the surface returned
 * by [method@Gdk.Drag.get_drag_surface].
 *
 * This function returns a reference to the [class@Gdk.Drag] object,
 * but GTK keeps its own reference as well, as long as the DND operation
 * is going on.
 *
 * Note: if @actions include %GDK_ACTION_MOVE, you need to listen for
 * the [signal@Gdk.Drag::dnd-finished] signal and delete the data at
 * the source if [method@Gdk.Drag.get_selected_action] returns
 * %GDK_ACTION_MOVE.
 *
 * Returns: (transfer full) (nullable): a newly created `GdkDrag`
 */
GdkDrag *
gdk_drag_begin (GdkSurface          *surface,
                GdkDevice          *device,
                GdkContentProvider *content,
                GdkDragAction       actions,
                double              dx,
                double              dy)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (surface->display == gdk_device_get_display (device), NULL);
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (content), NULL);

  return GDK_SURFACE_GET_CLASS (surface)->drag_begin (surface, device, content, actions, dx, dy);
}

static void
gdk_surface_ensure_motion (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;
  double x, y;
  GdkModifierType state;
  GdkSurface *grab_surface;

  if (!surface->request_motion)
    return;

  surface->request_motion = FALSE;

  display = gdk_surface_get_display (surface);
  seat = gdk_display_get_default_seat (display);
  if (!seat)
    return;

  device = gdk_seat_get_pointer (seat);

  if (!gdk_surface_get_device_position (surface, device, &x, &y, &state))
    return;

  if (gdk_device_grab_info (display, device, &grab_surface, NULL))
    {
      if (grab_surface != surface)
        return;
    }

  event = gdk_motion_event_new (surface,
                                device,
                                NULL,
                                GDK_CURRENT_TIME,
                                state,
                                x, y,
                                NULL);

  gdk_surface_handle_event (event);
  gdk_event_unref (event);
}

static void
gdk_surface_flush_events (GdkFrameClock *clock,
                          void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);

  _gdk_event_queue_flush (surface->display);
  gdk_surface_ensure_motion (surface);
  _gdk_display_pause_events (surface->display);

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS);
  surface->frame_clock_events_paused = TRUE;
}

static void
gdk_surface_resume_events (GdkFrameClock *clock,
                           void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);

  if (surface->frame_clock_events_paused)
    {
      _gdk_display_unpause_events (surface->display);
      surface->frame_clock_events_paused = FALSE;
    }
}

void
gdk_surface_set_frame_clock (GdkSurface     *surface,
                             GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (clock == NULL || GDK_IS_FRAME_CLOCK (clock));

  if (clock == surface->frame_clock)
    return;

  if (clock)
    {
      g_object_ref (clock);

      g_signal_connect (G_OBJECT (clock),
                        "flush-events",
                        G_CALLBACK (gdk_surface_flush_events),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "resume-events",
                        G_CALLBACK (gdk_surface_resume_events),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "layout",
                        G_CALLBACK (gdk_surface_layout_on_clock),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "paint",
                        G_CALLBACK (gdk_surface_paint_on_clock),
                        surface);

      if (surface->update_freeze_count == 0)
        _gdk_frame_clock_inhibit_freeze (clock);
    }

  if (surface->frame_clock)
    {
      if (surface->frame_clock_events_paused)
        gdk_surface_resume_events (surface->frame_clock, G_OBJECT (surface));

      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_flush_events),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_resume_events),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_layout_on_clock),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_paint_on_clock),
                                            surface);

      if (surface->update_freeze_count == 0)
        _gdk_frame_clock_uninhibit_freeze (surface->frame_clock);

      g_object_unref (surface->frame_clock);
    }

  surface->frame_clock = clock;
}

/**
 * gdk_surface_get_frame_clock: (attributes org.gtk.Method.get_property=frame-clock)
 * @surface: surface to get frame clock for
 *
 * Gets the frame clock for the surface.
 *
 * The frame clock for a surface never changes unless the surface is
 * reparented to a new toplevel surface.
 *
 * Returns: (transfer none): the frame clock
 */
GdkFrameClock *
gdk_surface_get_frame_clock (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->frame_clock;
}

/**
 * gdk_surface_get_scale_factor: (attributes org.gtk.Method.get_property=scale-factor)
 * @surface: surface to get scale factor for
 *
 * Returns the internal scale factor that maps from surface coordinates
 * to the actual device pixels.
 *
 * On traditional systems this is 1, but on very high density outputs
 * this can be a higher value (often 2). A higher value means that drawing
 * is automatically scaled up to a higher resolution, so any code doing
 * drawing will automatically look nicer. However, if you are supplying
 * pixel-based data the scale value can be used to determine whether to
 * use a pixel resource with higher resolution data.
 *
 * The scale factor may change during the lifetime of the surface.
 *
 * Returns: the scale factor
 */
int
gdk_surface_get_scale_factor (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 1);

  return (int) ceil (gdk_surface_get_scale (surface));
}

/**
 * gdk_surface_get_scale: (attributes org.gtk.Method.get_property=scale)
 * @surface: surface to get scale for
 *
 * Returns the internal scale that maps from surface coordinates
 * to the actual device pixels.
 *
 * When the scale is bigger than 1, the windowing system prefers to get
 * buffers with a resolution that is bigger than the surface size (e.g.
 * to show the surface on a high-resolution display, or in a magnifier).
 *
 * Compare with [method@Gdk.Surface.get_scale_factor], which returns the
 * next larger integer.
 *
 * The scale may change during the lifetime of the surface.
 *
 * Returns: the scale
 *
 * Since: 4.12
 */
double
gdk_surface_get_scale (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 1.);

  if (GDK_SURFACE_DESTROYED (surface))
    return 1.;

  return GDK_SURFACE_GET_CLASS (surface)->get_scale (surface);
}

/**
 * gdk_surface_set_opaque_region:
 * @surface: a top-level `GdkSurface`
 * @region: (nullable): a region, or %NULL to make the entire
 *   surface opaque
 *
 * Marks a region of the `GdkSurface` as opaque.
 *
 * For optimisation purposes, compositing window managers may
 * like to not draw obscured regions of surfaces, or turn off blending
 * during for these regions. With RGB windows with no transparency,
 * this is just the shape of the window, but with ARGB32 windows, the
 * compositor does not know what regions of the window are transparent
 * or not.
 *
 * This function only works for toplevel surfaces.
 *
 * GTK will update this property automatically if the @surface background
 * is opaque, as we know where the opaque regions are. If your surface
 * background is not opaque, please update this property in your
 * [GtkWidgetClass.css_changed](../gtk4/vfunc.Widget.css_changed.html) handler.
 */
void
gdk_surface_set_opaque_region (GdkSurface      *surface,
                               cairo_region_t *region)
{
  GdkSurfaceClass *class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (!GDK_SURFACE_DESTROYED (surface));

  if (cairo_region_equal (surface->opaque_region, region))
    return;

  g_clear_pointer (&surface->opaque_region, cairo_region_destroy);

  if (region != NULL)
    surface->opaque_region = cairo_region_reference (region);

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->set_opaque_region)
    class->set_opaque_region (surface, region);
}

void
gdk_surface_set_state (GdkSurface      *surface,
                       GdkToplevelState new_state)
{
  gboolean was_sticky, sticky;
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (new_state == surface->state)
    return; /* No actual work to do, nothing changed. */

  /* Actually update the field in GdkSurface, this is sort of an odd
   * place to do it, but seems like the safest since it ensures we expose no
   * inconsistent state to the user.
   */

  was_sticky = GDK_SURFACE_IS_STICKY (surface);

  surface->state = new_state;

  sticky = GDK_SURFACE_IS_STICKY (surface);

  if (GDK_IS_TOPLEVEL (surface))
    g_object_notify (G_OBJECT (surface), "state");

  if (was_sticky != sticky)
    g_object_notify (G_OBJECT (surface), "sticky");
}

void
gdk_synthesize_surface_state (GdkSurface       *surface,
                              GdkToplevelState  unset_flags,
                              GdkToplevelState  set_flags)
{
  gdk_surface_set_state (surface, (surface->state | set_flags) & ~unset_flags);
}

void
gdk_surface_queue_state_change (GdkSurface       *surface,
                                GdkToplevelState  unset_flags,
                                GdkToplevelState  set_flags)
{
  surface->pending_unset_flags |= unset_flags;
  surface->pending_set_flags &= ~unset_flags;

  surface->pending_set_flags |= set_flags;
  surface->pending_unset_flags &= ~set_flags;
}

void
gdk_surface_apply_state_change (GdkSurface *surface)
{
  if (!surface->pending_unset_flags && !surface->pending_set_flags)
    return;

  gdk_synthesize_surface_state (surface,
                                surface->pending_unset_flags,
                                surface->pending_set_flags);
  surface->pending_unset_flags = 0;
  surface->pending_set_flags = 0;
}

static gboolean
set_is_mapped_idle (gpointer user_data)
{
  GdkSurface *surface = GDK_SURFACE (user_data);

  surface->set_is_mapped_source_id = 0;

  g_return_val_if_fail (surface->pending_is_mapped != surface->is_mapped,
                        G_SOURCE_REMOVE);

  surface->is_mapped = surface->pending_is_mapped;
  if (surface->is_mapped)
    gdk_surface_invalidate_rect (surface, NULL);

  g_object_notify (G_OBJECT (surface), "mapped");

  return G_SOURCE_REMOVE;
}

void
gdk_surface_set_is_mapped (GdkSurface *surface,
                           gboolean    is_mapped)
{
  gboolean was_mapped;

  if (surface->pending_is_mapped != surface->is_mapped)
    g_clear_handle_id (&surface->set_is_mapped_source_id, g_source_remove);

  surface->pending_is_mapped = is_mapped;

  was_mapped = surface->is_mapped;
  surface->is_mapped = is_mapped;
  if (surface->is_mapped)
    gdk_surface_invalidate_rect (surface, NULL);

  if (was_mapped != is_mapped)
    g_object_notify (G_OBJECT (surface), "mapped");
}

static void
gdk_surface_queue_set_is_mapped (GdkSurface *surface,
                                 gboolean    is_mapped)
{
  if (surface->pending_is_mapped == is_mapped)
    return;

  surface->pending_is_mapped = is_mapped;

  if (surface->is_mapped == surface->pending_is_mapped)
    {
      g_clear_handle_id (&surface->set_is_mapped_source_id, g_source_remove);
    }
  else
    {
      g_return_if_fail (!surface->set_is_mapped_source_id);

      surface->set_is_mapped_source_id =
        g_idle_add_full (G_PRIORITY_HIGH - 10, set_is_mapped_idle, surface, NULL);
      gdk_source_set_static_name_by_id (surface->set_is_mapped_source_id, "[gtk] set_is_mapped_idle");
    }
}

static gboolean
check_autohide (GdkEvent *event)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkSurface *grab_surface, *event_surface;
  GdkEventType evtype = gdk_event_get_event_type (event);

 switch ((guint) evtype)
    {
    case GDK_BUTTON_PRESS:
#if 0
    // FIXME: we need to ignore the release that is paired
    // with the press starting the grab - due to implicit
    // grabs, it will be delivered to the same place as the
    // press, and will cause the auto dismissal to be triggered.
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
#endif
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      display = gdk_event_get_display (event);
      device = gdk_event_get_device (event);
      if (gdk_device_grab_info (display, device, &grab_surface, NULL))
        {
          event_surface = gdk_event_get_surface (event);
          if (event_surface->autohide &&
              !event_surface->has_pointer)
            event_surface = NULL;

          if (grab_surface->autohide &&
              (!event_surface ||
               (grab_surface != event_surface &&
                grab_surface != event_surface->parent)))
            {
              GdkSurface *surface = grab_surface;

              do
                {
                  gdk_surface_hide (surface);
                  surface = surface->parent;
                }
              while (surface->autohide && surface != event_surface);

              return TRUE;
            }
        }
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event_surface = gdk_event_get_surface (event);
      if (event_surface->autohide &&
          gdk_crossing_event_get_mode (event) == GDK_CROSSING_NORMAL)
        event_surface->has_pointer = evtype == GDK_ENTER_NOTIFY;
      break;
    default:;
    }

  return FALSE;
}

static inline void
add_event_mark (GdkEvent *event,
                gint64    time,
                gint64    end_time)
{
#ifdef HAVE_SYSPROF
  char *message = NULL;
  const char *kind;
  GEnumClass *class;
  GEnumValue *value;
  GdkEventType event_type;

  event_type = gdk_event_get_event_type (event);
  class = g_type_class_ref (GDK_TYPE_EVENT_TYPE);
  value = g_enum_get_value (class, event_type);
  g_type_class_unref (class);
  kind = value ? value->value_nick : "Event";

  switch ((int) event_type)
    {
    case GDK_MOTION_NOTIFY:
      {
        double x, y;
        gdk_event_get_position (event, &x, &y);
        message = g_strdup_printf ("%s {x=%lf, y=%lf, state=0x%x}",
                                   kind,
                                   x, y,
                                   gdk_event_get_modifier_state (event));
        break;
      }

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      {
        double x, y;
        gdk_event_get_position (event, &x, &y);
        message = g_strdup_printf ("%s {button=%u, x=%lf, y=%lf, state=0x%x}",
                                   kind,
                                   gdk_button_event_get_button (event),
                                   x, y,
                                   gdk_event_get_modifier_state (event));
        break;
      }

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      {
        message = g_strdup_printf ("%s {keyval=%u, state=0x%x, keycode=%u layout=%u level=%u is_modifier=%u}",
                                   kind,
                                   gdk_key_event_get_keyval (event),
                                   gdk_event_get_modifier_state (event),
                                   gdk_key_event_get_keycode (event),
                                   gdk_key_event_get_layout (event),
                                   gdk_key_event_get_level (event),
                                   gdk_key_event_is_modifier (event));
        break;
      }

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_SCROLL:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
    case GDK_PAD_GROUP_MODE:
    case GDK_GRAB_BROKEN:
    case GDK_DELETE:
    case GDK_FOCUS_CHANGE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_EVENT_LAST:
    default:
      break;
    }

  gdk_profiler_add_mark (time, end_time - time, "Event", message ? message : kind);

  g_free (message);
#endif
}

gboolean
gdk_surface_handle_event (GdkEvent *event)
{
  GdkSurface *surface = gdk_event_get_surface (event);
  gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  gboolean handled = FALSE;

  if (check_autohide (event))
    return TRUE;


  if (gdk_event_get_event_type (event) == GDK_MOTION_NOTIFY)
    surface->request_motion = FALSE;

  g_signal_emit (surface, signals[EVENT], 0, event, &handled);

  if (GDK_PROFILER_IS_RUNNING)
    add_event_mark (event, begin_time, GDK_PROFILER_CURRENT_TIME);

  return handled;
}

/*
 * gdk_surface_request_motion:
 * @surface: a `GdkSurface`
 *
 * Request that the next frame cycle should deliver a motion
 * event for @surface.
 *
 * The motion event will be delivered if the pointer is over the
 * surface, regardless whether the pointer has moved or not. This
 * is used by GTK after moving widgets around.
 */
void
gdk_surface_request_motion (GdkSurface *surface)
{
  surface->request_motion = TRUE;
}

/**
 * gdk_surface_translate_coordinates:
 * @from: the origin surface
 * @to: the target surface
 * @x: (inout): coordinates to translate
 * @y: (inout): coordinates to translate
 *
 * Translates coordinates between two surfaces.
 *
 * Note that this only works if @to and @from are popups or
 * transient-for to the same toplevel (directly or indirectly).
 *
 * Returns: %TRUE if the coordinates were successfully translated
 */
gboolean
gdk_surface_translate_coordinates (GdkSurface *from,
                                   GdkSurface *to,
                                   double     *x,
                                   double     *y)
{
  double in_x, in_y, out_x, out_y;
  int x1, y1, x2, y2;
  GdkSurface *f, *t;

  g_return_val_if_fail (GDK_IS_SURFACE (from), FALSE);
  g_return_val_if_fail (GDK_IS_SURFACE (to), FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  in_x = *x;
  in_y = *y;

  x1 = 0;
  y1 = 0;
  f = from;
  while (f->parent)
    {
      x1 += f->x;
      y1 += f->y;
      f = f->parent;
    }

  x2 = 0;
  y2 = 0;
  t = to;
  while (t->parent)
    {
      x2 += t->x;
      y2 += t->y;
      t = t->parent;
    }

  if (f != t)
    return FALSE;

  out_x = in_x + (x1 - x2);
  out_y = in_y + (y1 - y2);

  *x = out_x;
  *y = out_y;

  return TRUE;
}

GdkSeat *
gdk_surface_get_seat_from_event (GdkSurface *surface,
                                 GdkEvent   *event)
{
  if (event)
    {
      GdkSeat *seat = NULL;

      seat = gdk_event_get_seat (event);

      if (seat)
        return seat;
    }
  return gdk_display_get_default_seat (surface->display);
}

void
gdk_surface_enter_monitor (GdkSurface *surface,
                           GdkMonitor *monitor)
{
  g_signal_emit (surface, signals[ENTER_MONITOR], 0, monitor);
}

void
gdk_surface_leave_monitor (GdkSurface *surface,
                           GdkMonitor *monitor)
{
  g_signal_emit (surface, signals[LEAVE_MONITOR], 0, monitor);
}

GdkSubsurface *
gdk_surface_create_subsurface (GdkSurface *surface)
{
  GdkSubsurface *subsurface;

  subsurface = GDK_SURFACE_GET_CLASS (surface)->create_subsurface (surface);

  if (subsurface)
    {
      subsurface->parent = g_object_ref (surface);
      g_ptr_array_add (surface->subsurfaces, subsurface);
    }

  return subsurface;
}

gsize
gdk_surface_get_n_subsurfaces (GdkSurface *surface)
{
  return surface->subsurfaces->len;
}

GdkSubsurface *
gdk_surface_get_subsurface (GdkSurface *surface,
                            gsize       idx)
{
  return g_ptr_array_index (surface->subsurfaces, idx);
}
