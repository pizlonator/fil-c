/* GDK - The GIMP Drawing Kit
 * gdkdisplay.c
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

#include "config.h"

#include "gdkdisplay.h"
#include "gdkdisplayprivate.h"

#include <glib/gi18n-lib.h>
#include "gdkprivate.h"

#include "gdkapplaunchcontext.h"
#include "gdkclipboardprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkdmabufeglprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabufformatsprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkmonitorprivate.h"
#include "gdkrectangle.h"
#include "gdkvulkancontextprivate.h"

#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#include <math.h>
#include <stdlib.h>

/**
 * GdkDisplay:
 *
 * `GdkDisplay` objects are the GDK representation of a workstation.
 *
 * Their purpose are two-fold:
 *
 * - To manage and provide information about input devices (pointers, keyboards, etc)
 * - To manage and provide information about output devices (monitors, projectors, etc)
 *
 * Most of the input device handling has been factored out into separate
 * [class@Gdk.Seat] objects. Every display has a one or more seats, which
 * can be accessed with [method@Gdk.Display.get_default_seat] and
 * [method@Gdk.Display.list_seats].
 *
 * Output devices are represented by [class@Gdk.Monitor] objects, which can
 * be accessed with [method@Gdk.Display.get_monitor_at_surface] and similar APIs.
 */

enum
{
  PROP_0,
  PROP_COMPOSITED,
  PROP_RGBA,
  PROP_SHADOW_WIDTH,
  PROP_INPUT_SHAPES,
  PROP_DMABUF_FORMATS,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

enum {
  OPENED,
  CLOSED,
  SEAT_ADDED,
  SEAT_REMOVED,
  SETTING_CHANGED,
  LAST_SIGNAL
};

typedef struct _GdkDisplayPrivate GdkDisplayPrivate;

struct _GdkDisplayPrivate {
  /* The base context that all other contexts inherit from.
   * This context is never exposed to public API and is
   * allowed to have a %NULL surface.
   */
  GdkGLContext *gl_context;
  GError *gl_error;

#ifdef HAVE_EGL
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLConfig egl_config_high_depth;
#endif

  guint rgba : 1;
  guint composited : 1;
  guint shadow_width: 1;
  guint input_shapes : 1;

  GdkDebugFlags debug_flags;
};

static void gdk_display_dispose     (GObject         *object);
static void gdk_display_finalize    (GObject         *object);


static GdkAppLaunchContext *gdk_display_real_get_app_launch_context (GdkDisplay *display);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GdkDisplay, gdk_display, G_TYPE_OBJECT)

static void
gdk_display_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkDisplay *display = GDK_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_COMPOSITED:
      g_value_set_boolean (value, gdk_display_is_composited (display));
      break;

    case PROP_RGBA:
      g_value_set_boolean (value, gdk_display_is_rgba (display));
      break;

    case PROP_SHADOW_WIDTH:
      g_value_set_boolean (value, gdk_display_supports_shadow_width (display));
      break;

    case PROP_INPUT_SHAPES:
      g_value_set_boolean (value, gdk_display_supports_input_shapes (display));
      break;

    case PROP_DMABUF_FORMATS:
      g_value_set_boxed (value, gdk_display_get_dmabuf_formats (display));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_display_real_make_default (GdkDisplay *display)
{
}

static GdkGLContext *
gdk_display_default_init_gl (GdkDisplay  *display,
                             GError     **error)
{
  g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       _("The current backend does not support OpenGL"));

  return NULL;
}

static guint
gdk_display_default_rate_egl_config (GdkDisplay *display,
                                     gpointer    egl_display,
                                     gpointer    config)
{
  guint distance = 0;
#ifdef HAVE_EGL
  int tmp;

  if (!eglGetConfigAttrib (egl_display, config, EGL_SAMPLE_BUFFERS, &tmp) || tmp != 0)
    distance += 0x20000;

  if (!eglGetConfigAttrib (egl_display, config, EGL_DEPTH_SIZE, &tmp) || tmp != 0 ||
      !eglGetConfigAttrib (egl_display, config, EGL_STENCIL_SIZE, &tmp) || tmp != 0)
    distance += 0x10000;
#endif

  return distance;
}

static GdkSeat *
gdk_display_real_get_default_seat (GdkDisplay *display)
{
  if (!display->seats)
    return NULL;

  return display->seats->data;
}

static void
gdk_display_real_opened (GdkDisplay *display)
{
  _gdk_display_manager_add_display (gdk_display_manager_get (), display);
}

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;
  object_class->get_property = gdk_display_get_property;

  class->make_default = gdk_display_real_make_default;
  class->get_app_launch_context = gdk_display_real_get_app_launch_context;
  class->init_gl = gdk_display_default_init_gl;
  class->rate_egl_config = gdk_display_default_rate_egl_config;
  class->get_default_seat = gdk_display_real_get_default_seat;
  class->opened = gdk_display_real_opened;

  /**
   * GdkDisplay:composited: (attributes org.gtk.Property.get=gdk_display_is_composited)
   *
   * %TRUE if the display properly composites the alpha channel.
   */
  props[PROP_COMPOSITED] =
    g_param_spec_boolean ("composited", NULL, NULL,
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDisplay:rgba: (attributes org.gtk.Property.get=gdk_display_is_rgba)
   *
   * %TRUE if the display supports an alpha channel.
   */
  props[PROP_RGBA] =
    g_param_spec_boolean ("rgba", NULL, NULL,
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDisplay:shadow-width: (attributes org.gtk.Property.get=gdk_display_supports_shadow_width)
   *
   * %TRUE if the display supports extensible frames.
   *
   * Since: 4.14
   */
  props[PROP_SHADOW_WIDTH] =
    g_param_spec_boolean ("shadow-width", NULL, NULL,
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDisplay:input-shapes: (attributes org.gtk.Property.get=gdk_display_supports_input_shapes)
   *
   * %TRUE if the display supports input shapes.
   */
  props[PROP_INPUT_SHAPES] =
    g_param_spec_boolean ("input-shapes", NULL, NULL,
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDisplay:dmabuf-formats: (attributes org.gtk.Property.get=gdk_display_get_dmabuf_formats)
   *
   * The dma-buf formats that are supported on this display
   *
   * Since: 4.14
   */
  props[PROP_DMABUF_FORMATS] =
    g_param_spec_boxed ("dmabuf-formats", NULL, NULL,
                        GDK_TYPE_DMABUF_FORMATS,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /**
   * GdkDisplay::opened:
   * @display: the object on which the signal is emitted
   *
   * Emitted when the connection to the windowing system for @display is opened.
   */
  signals[OPENED] =
    g_signal_new (g_intern_static_string ("opened"),
		  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, opened),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkDisplay::closed:
   * @display: the object on which the signal is emitted
   * @is_error: %TRUE if the display was closed due to an error
   *
   * Emitted when the connection to the windowing system for @display is closed.
   */
  signals[CLOSED] =
    g_signal_new (g_intern_static_string ("closed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, closed),
		  NULL, NULL,
                  NULL,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_BOOLEAN);

  /**
   * GdkDisplay::seat-added:
   * @display: the object on which the signal is emitted
   * @seat: the seat that was just added
   *
   * Emitted whenever a new seat is made known to the windowing system.
   */
  signals[SEAT_ADDED] =
    g_signal_new (g_intern_static_string ("seat-added"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  NULL,
		  G_TYPE_NONE, 1, GDK_TYPE_SEAT);

  /**
   * GdkDisplay::seat-removed:
   * @display: the object on which the signal is emitted
   * @seat: the seat that was just removed
   *
   * Emitted whenever a seat is removed by the windowing system.
   */
  signals[SEAT_REMOVED] =
    g_signal_new (g_intern_static_string ("seat-removed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  NULL,
		  G_TYPE_NONE, 1, GDK_TYPE_SEAT);

  /**
   * GdkDisplay::setting-changed:
   * @display: the object on which the signal is emitted
   * @setting: the name of the setting that changed
   *
   * Emitted whenever a setting changes its value.
   */
  signals[SETTING_CHANGED] =
    g_signal_new (g_intern_static_string ("setting-changed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL,
                  NULL,
		  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
free_pointer_info (GdkPointerSurfaceInfo *info)
{
  g_clear_object (&info->surface_under_pointer);
  g_free (info);
}

static void
free_device_grab (GdkDeviceGrabInfo *info)
{
  g_object_unref (info->surface);
  g_free (info);
}

static gboolean
free_device_grabs_foreach (gpointer key,
                           gpointer value,
                           gpointer user_data)
{
  GList *list = value;

  g_list_free_full (list, (GDestroyNotify) free_device_grab);

  return TRUE;
}

static void
gdk_display_init (GdkDisplay *display)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  display->double_click_time = 250;
  display->double_click_distance = 5;

  display->device_grabs = g_hash_table_new (NULL, NULL);

  display->pointers_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                  (GDestroyNotify) free_pointer_info);

  g_queue_init (&display->queued_events);

  priv->debug_flags = _gdk_debug_flags;

  priv->composited = TRUE;
  priv->rgba = TRUE;
  priv->shadow_width = TRUE;
  priv->input_shapes = TRUE;
}

static void
gdk_display_dispose (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY (object);
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (display->dmabuf_downloaders); i++)
    {
      if (display->dmabuf_downloaders[i] == NULL)
        continue;

      gdk_dmabuf_downloader_close (display->dmabuf_downloaders[i]);
      g_clear_object (&display->dmabuf_downloaders[i]);
    }

  _gdk_display_manager_remove_display (gdk_display_manager_get (), display);

  g_queue_clear (&display->queued_events);

  g_clear_pointer (&display->egl_dmabuf_formats, gdk_dmabuf_formats_unref);
  g_clear_pointer (&display->egl_external_formats, gdk_dmabuf_formats_unref);
#ifdef GDK_RENDERING_VULKAN
  if (display->vk_dmabuf_formats)
    {
      gdk_display_unref_vulkan (display);
      g_assert (display->vk_dmabuf_formats == NULL);
    }
#endif

  g_clear_object (&priv->gl_context);
#ifdef HAVE_EGL
  g_clear_pointer (&priv->egl_display, eglTerminate);
#endif
  g_clear_error (&priv->gl_error);

  for (GList *l = display->seats; l; l = l->next)
    g_object_run_dispose (G_OBJECT (l->data));

  G_OBJECT_CLASS (gdk_display_parent_class)->dispose (object);
}

static void
gdk_display_finalize (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY (object);

  g_hash_table_foreach_remove (display->device_grabs,
                               free_device_grabs_foreach,
                               NULL);
  g_hash_table_destroy (display->device_grabs);

  g_hash_table_destroy (display->pointers_info);

  g_list_free_full (display->seats, g_object_unref);

  g_clear_pointer (&display->dmabuf_formats, gdk_dmabuf_formats_unref);

  G_OBJECT_CLASS (gdk_display_parent_class)->finalize (object);
}

/**
 * gdk_display_close:
 * @display: a `GdkDisplay`
 *
 * Closes the connection to the windowing system for the given display.
 *
 * This cleans up associated resources.
 */
void
gdk_display_close (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    {
      display->closed = TRUE;

      g_signal_emit (display, signals[CLOSED], 0, FALSE);
      g_object_run_dispose (G_OBJECT (display));

      g_object_unref (display);
    }
}

/**
 * gdk_display_is_closed:
 * @display: a `GdkDisplay`
 *
 * Finds out if the display has been closed.
 *
 * Returns: %TRUE if the display is closed.
 */
gboolean
gdk_display_is_closed  (GdkDisplay  *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return display->closed;
}

/*<private>
 * gdk_display_get_event:
 * @display: a `GdkDisplay`
 *
 * Gets the next `GdkEvent` to be processed for @display,
 * fetching events from the windowing system if necessary.
 *
 * Returns: (nullable) (transfer full): the next `GdkEvent`
 *   to be processed
 */
GdkEvent *
gdk_display_get_event (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->event_pause_count == 0)
    GDK_DISPLAY_GET_CLASS (display)->queue_events (display);

  return _gdk_event_unqueue (display);
}

/**
 * gdk_display_put_event:
 * @display: a `GdkDisplay`
 * @event: (transfer none): a `GdkEvent`
 *
 * Adds the given event to the event queue for @display.
 *
 * Deprecated: 4.10: This function is only useful in very
 * special situations and should not be used by applications.
 **/
void
gdk_display_put_event (GdkDisplay *display,
                       GdkEvent   *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  _gdk_event_queue_append (display, gdk_event_ref ((GdkEvent *)event));
}

static void
generate_grab_broken_event (GdkDisplay *display,
                            GdkSurface  *surface,
                            GdkDevice  *device,
			    gboolean    implicit,
			    GdkSurface  *grab_surface)
{
  g_return_if_fail (surface != NULL);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkEvent *event;

      event = gdk_grab_broken_event_new (surface,
                                         device,
                                         grab_surface,
                                         implicit);

      _gdk_event_queue_append (display, event);
    }
}

GdkDeviceGrabInfo *
_gdk_display_get_last_device_grab (GdkDisplay *display,
                                   GdkDevice  *device)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);

  if (l)
    {
      l = g_list_last (l);
      return l->data;
    }

  return NULL;
}

GdkDeviceGrabInfo *
_gdk_display_add_device_grab (GdkDisplay       *display,
                              GdkDevice        *device,
                              GdkSurface        *surface,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              unsigned long     serial_start,
                              guint32           time,
                              gboolean          implicit)
{
  GdkDeviceGrabInfo *info, *other_info;
  GList *grabs, *l;

  info = g_new0 (GdkDeviceGrabInfo, 1);

  info->surface = g_object_ref (surface);
  info->serial_start = serial_start;
  info->serial_end = G_MAXULONG;
  info->owner_events = owner_events;
  info->event_mask = event_mask;
  info->time = time;
  info->implicit = implicit;

  grabs = g_hash_table_lookup (display->device_grabs, device);

  /* Find the first grab that has a larger start time (if any) and insert
   * before that. I.E we insert after already existing grabs with same
   * start time */
  for (l = grabs; l != NULL; l = l->next)
    {
      other_info = l->data;

      if (info->serial_start < other_info->serial_start)
	break;
    }

  grabs = g_list_insert_before (grabs, l, info);

  /* Make sure the new grab end before next grab */
  if (l)
    {
      other_info = l->data;
      info->serial_end = other_info->serial_start;
    }

  /* Find any previous grab and update its end time */
  l = g_list_find (grabs, info);
  l = l->prev;
  if (l)
    {
      other_info = l->data;
      other_info->serial_end = serial_start;
    }

  g_hash_table_insert (display->device_grabs, device, grabs);

  return info;
}

static GdkSurface *
get_current_toplevel (GdkDisplay      *display,
                      GdkDevice       *device,
                      int             *x_out,
                      int             *y_out,
		      GdkModifierType *state_out)
{
  GdkSurface *pointer_surface;
  double x, y;
  GdkModifierType state;

  pointer_surface = _gdk_device_surface_at_position (device, &x, &y, &state);

  if (pointer_surface != NULL &&
      GDK_SURFACE_DESTROYED (pointer_surface))
    pointer_surface = NULL;

  *x_out = round (x);
  *y_out = round (y);
  *state_out = state;

  return pointer_surface;
}

static void
switch_to_pointer_grab (GdkDisplay        *display,
                        GdkDevice         *device,
			GdkDeviceGrabInfo *grab,
			GdkDeviceGrabInfo *last_grab,
			guint32            time,
			gulong             serial)
{
  GdkSurface *new_toplevel;
  GdkPointerSurfaceInfo *info;
  GList *old_grabs;
  GdkModifierType state;
  int x = 0, y = 0;

  /* Temporarily unset pointer to make sure we send the crossing events below */
  old_grabs = g_hash_table_lookup (display->device_grabs, device);
  g_hash_table_steal (display->device_grabs, device);
  info = _gdk_display_get_pointer_info (display, device);

  if (grab)
    {
      /* New grab is in effect */
      if (!grab->implicit)
	{
	  /* !owner_event Grabbing a surface that we're not inside, current status is
	     now NULL (i.e. outside grabbed surface) */
	  if (!grab->owner_events && info->surface_under_pointer != grab->surface)
	    _gdk_display_set_surface_under_pointer (display, device, NULL);
	}

      grab->activated = TRUE;
    }

  if (last_grab)
    {
      new_toplevel = NULL;

      if (grab == NULL /* ungrab */ ||
	  (!last_grab->owner_events && grab->owner_events) /* switched to owner_events */ )
	{
          new_toplevel = get_current_toplevel (display, device, &x, &y, &state);

	  if (new_toplevel)
	    {
	      /* w is now toplevel and x,y in toplevel coords */
              _gdk_display_set_surface_under_pointer (display, device, new_toplevel);
	      info->toplevel_x = x;
	      info->toplevel_y = y;
	      info->state = state;
	    }
	}

      if (grab == NULL) /* Ungrabbed, send events */
	{
	  /* We're now ungrabbed, update the surface_under_pointer */
	  _gdk_display_set_surface_under_pointer (display, device, new_toplevel);
	}
    }

  g_hash_table_insert (display->device_grabs, device, old_grabs);
}

void
_gdk_display_update_last_event (GdkDisplay     *display,
                                GdkEvent       *event)
{
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    display->last_event_time = gdk_event_get_time (event);
}

void
_gdk_display_device_grab_update (GdkDisplay *display,
                                 GdkDevice  *device,
                                 gulong      current_serial)
{
  GdkDeviceGrabInfo *current_grab, *next_grab;
  GList *grabs;
  guint32 time;

  time = display->last_event_time;
  grabs = g_hash_table_lookup (display->device_grabs, device);

  while (grabs != NULL)
    {
      current_grab = grabs->data;

      if (current_grab->serial_start > current_serial)
	return; /* Hasn't started yet */

      if (current_grab->serial_end > current_serial)
	{
	  /* This one hasn't ended yet.
	     its the currently active one or scheduled to be active */

	  if (!current_grab->activated)
            {
              if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
                switch_to_pointer_grab (display, device, current_grab, NULL, time, current_serial);
            }

	  break;
	}

      next_grab = NULL;
      if (grabs->next)
	{
	  /* This is the next active grab */
	  next_grab = grabs->next->data;

	  if (next_grab->serial_start > current_serial)
	    next_grab = NULL; /* Actually its not yet active */
	}

      if ((next_grab == NULL && current_grab->implicit_ungrab) ||
          (next_grab != NULL && current_grab->surface != next_grab->surface))
        generate_grab_broken_event (display, GDK_SURFACE (current_grab->surface),
                                    device,
                                    current_grab->implicit,
                                    next_grab? next_grab->surface : NULL);

      /* Remove old grab */
      grabs = g_list_delete_link (grabs, grabs);
      g_hash_table_insert (display->device_grabs, device, grabs);

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        switch_to_pointer_grab (display, device,
                                next_grab, current_grab,
                                time, current_serial);

      free_device_grab (current_grab);
    }
}

static GList *
grab_list_find (GList  *grabs,
                gulong  serial)
{
  GdkDeviceGrabInfo *grab;

  while (grabs)
    {
      grab = grabs->data;

      if (serial >= grab->serial_start && serial < grab->serial_end)
	return grabs;

      grabs = grabs->next;
    }

  return NULL;
}

static GList *
find_device_grab (GdkDisplay *display,
                   GdkDevice  *device,
                   gulong      serial)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);
  return grab_list_find (l, serial);
}

GdkDeviceGrabInfo *
_gdk_display_has_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial)
{
  GList *l;

  l = find_device_grab (display, device, serial);
  if (l)
    return l->data;

  return NULL;
}

/* Returns true if last grab was ended
 * If if_child is non-NULL, end the grab only if the grabbed
 * surface is the same as if_child or a descendant of it */
gboolean
_gdk_display_end_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial,
                              GdkSurface  *if_child,
                              gboolean    implicit)
{
  GdkDeviceGrabInfo *grab;
  GList *l;

  l = find_device_grab (display, device, serial);

  if (l == NULL)
    return FALSE;

  grab = l->data;
  if (grab && (if_child == NULL || if_child == grab->surface))
    {
      grab->serial_end = serial;
      grab->implicit_ungrab = implicit;
      return l->next == NULL;
    }

  return FALSE;
}

GdkPointerSurfaceInfo *
_gdk_display_get_pointer_info (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkPointerSurfaceInfo *info;
  GdkSeat *seat;

  if (device)
    {
      seat = gdk_device_get_seat (device);

      if (device == gdk_seat_get_keyboard (seat))
        device = gdk_seat_get_pointer (seat);
    }

  if (G_UNLIKELY (!device))
    return NULL;

  info = g_hash_table_lookup (display->pointers_info, device);

  if (G_UNLIKELY (!info))
    {
      info = g_new0 (GdkPointerSurfaceInfo, 1);
      g_hash_table_insert (display->pointers_info, device, info);
    }

  return info;
}

void
_gdk_display_pointer_info_foreach (GdkDisplay                   *display,
                                   GdkDisplayPointerInfoForeach  func,
                                   gpointer                      user_data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, display->pointers_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkPointerSurfaceInfo *info = value;
      GdkDevice *device = key;

      (func) (display, device, info, user_data);
    }
}

/*< private >
 * gdk_device_grab_info:
 * @display: the display for which to get the grab information
 * @device: device to get the grab information from
 * @grab_surface: (out) (transfer none): location to store current grab surface
 * @owner_events: (out): location to store boolean indicating whether
 *   the @owner_events flag to gdk_device_grab() was %TRUE.
 *
 * Determines information about the current keyboard grab.
 * This is not public API and must not be used by applications.
 *
 * Returns: %TRUE if this application currently has the
 *  keyboard grabbed.
 */
gboolean
gdk_device_grab_info (GdkDisplay  *display,
                      GdkDevice   *device,
                      GdkSurface  **grab_surface,
                      gboolean    *owner_events)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    {
      if (grab_surface)
        *grab_surface = info->surface;
      if (owner_events)
        *owner_events = info->owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gdk_display_device_is_grabbed:
 * @display: a `GdkDisplay`
 * @device: a `GdkDevice`
 *
 * Returns %TRUE if there is an ongoing grab on @device for @display.
 *
 * Returns: %TRUE if there is a grab in effect for @device.
 */
gboolean
gdk_display_device_is_grabbed (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), TRUE);

  /* What we're interested in is the steady state (ie last grab),
     because we're interested e.g. if we grabbed so that we
     can ungrab, even if our grab is not active just yet. */
  info = _gdk_display_get_last_device_grab (display, device);

  return (info && !info->implicit);
}

/**
 * gdk_display_get_name:
 * @display: a `GdkDisplay`
 *
 * Gets the name of the display.
 *
 * Returns: a string representing the display name. This string is owned
 *   by GDK and should not be modified or freed.
 */
const char *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_name (display);
}

/**
 * gdk_display_beep:
 * @display: a `GdkDisplay`
 *
 * Emits a short beep on @display
 */
void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->beep (display);
}

/**
 * gdk_display_sync:
 * @display: a `GdkDisplay`
 *
 * Flushes any requests queued for the windowing system and waits until all
 * requests have been handled.
 *
 * This is often used for making sure that the display is synchronized
 * with the current state of the program. Calling [method@Gdk.Display.sync]
 * before [method@GdkX11.Display.error_trap_pop] makes sure that any errors
 * generated from earlier requests are handled before the error trap is removed.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 */
void
gdk_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->sync (display);
}

/**
 * gdk_display_flush:
 * @display: a `GdkDisplay`
 *
 * Flushes any requests queued for the windowing system.
 *
 * This happens automatically when the main loop blocks waiting for new events,
 * but if your application is drawing without returning control to the main loop,
 * you may need to call this function explicitly. A common case where this function
 * needs to be called is when an application is executing drawing commands
 * from a thread other than the thread where the main loop is running.
 *
 * This is most useful for X11. On windowing systems where requests are
 * handled synchronously, this function will do nothing.
 */
void
gdk_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->flush (display);
}

/**
 * gdk_display_get_clipboard:
 * @display: a `GdkDisplay`
 *
 * Gets the clipboard used for copy/paste operations.
 *
 * Returns: (transfer none): the display's clipboard
 */
GdkClipboard *
gdk_display_get_clipboard (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->clipboard == NULL)
    display->clipboard = gdk_clipboard_new (display);

  return display->clipboard;
}

/**
 * gdk_display_get_primary_clipboard:
 * @display: a `GdkDisplay`
 *
 * Gets the clipboard used for the primary selection.
 *
 * On backends where the primary clipboard is not supported natively,
 * GDK emulates this clipboard locally.
 *
 * Returns: (transfer none): the primary clipboard
 */
GdkClipboard *
gdk_display_get_primary_clipboard (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->primary_clipboard == NULL)
    display->primary_clipboard = gdk_clipboard_new (display);

  return display->primary_clipboard;
}

/**
 * gdk_display_supports_input_shapes: (attributes org.gtk.Method.get_property=input-shapes)
 * @display: a `GdkDisplay`
 *
 * Returns %TRUE if the display supports input shapes.
 *
 * This means that [method@Gdk.Surface.set_input_region] can
 * be used to modify the input shape of surfaces on @display.
 *
 * On modern displays, this value is always %TRUE.
 *
 * Returns: %TRUE if surfaces with modified input shape are supported
 */
gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return priv->input_shapes;
}

void
gdk_display_set_input_shapes (GdkDisplay *display,
                              gboolean    input_shapes)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->input_shapes == input_shapes)
    return;

  priv->input_shapes = input_shapes;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_INPUT_SHAPES]);
}

static GdkAppLaunchContext *
gdk_display_real_get_app_launch_context (GdkDisplay *display)
{
  GdkAppLaunchContext *ctx;

  ctx = g_object_new (GDK_TYPE_APP_LAUNCH_CONTEXT,
                      "display", display,
                      NULL);

  return ctx;
}

/**
 * gdk_display_get_app_launch_context:
 * @display: a `GdkDisplay`
 *
 * Returns a `GdkAppLaunchContext` suitable for launching
 * applications on the given display.
 *
 * Returns: (transfer full): a new `GdkAppLaunchContext` for @display
 */
GdkAppLaunchContext *
gdk_display_get_app_launch_context (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_app_launch_context (display);
}

/**
 * gdk_display_open:
 * @display_name: (nullable): the name of the display to open
 *
 * Opens a display.
 *
 * If opening the display fails, `NULL` is returned.
 *
 * Returns: (nullable) (transfer none): a `GdkDisplay`
 */
GdkDisplay *
gdk_display_open (const char *display_name)
{
  return gdk_display_manager_open_display (gdk_display_manager_get (),
                                           display_name);
}

gulong
_gdk_display_get_next_serial (GdkDisplay *display)
{
  return GDK_DISPLAY_GET_CLASS (display)->get_next_serial (display);
}

/**
 * gdk_display_notify_startup_complete:
 * @display: a `GdkDisplay`
 * @startup_id: a startup-notification identifier, for which
 *   notification process should be completed
 *
 * Indicates to the GUI environment that the application has
 * finished loading, using a given identifier.
 *
 * GTK will call this function automatically for [GtkWindow](../gtk4/class.Window.html)
 * with custom startup-notification identifier unless
 * [gtk_window_set_auto_startup_notification()](../gtk4/method.Window.set_auto_startup_notification.html)
 * is called to disable that feature.
 *
 * Deprecated: 4.10: Using [method@Gdk.Toplevel.set_startup_id] is sufficient
 */
void
gdk_display_notify_startup_complete (GdkDisplay  *display,
                                     const char *startup_id)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)->notify_startup_complete (display, startup_id);
}

/**
 * gdk_display_get_startup_notification_id:
 * @display: a `GdkDisplay`
 *
 * Gets the startup notification ID for a Wayland display, or %NULL
 * if no ID has been defined.
 *
 * Returns: (nullable): the startup notification ID for @display
 *
 * Deprecated: 4.10
 */
const char *
gdk_display_get_startup_notification_id (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (GDK_DISPLAY_GET_CLASS (display)->get_startup_notification_id == NULL)
    return NULL;

  return GDK_DISPLAY_GET_CLASS (display)->get_startup_notification_id (display);
}

void
_gdk_display_pause_events (GdkDisplay *display)
{
  display->event_pause_count++;
}

void
_gdk_display_unpause_events (GdkDisplay *display)
{
  g_return_if_fail (display->event_pause_count > 0);

  display->event_pause_count--;
}

/*< private >
 * gdk_display_get_keymap:
 * @display: the `GdkDisplay`
 *
 * Returns the `GdkKeymap` attached to @display.
 *
 * Returns: (transfer none): the `GdkKeymap` attached to @display.
 */
GdkKeymap *
gdk_display_get_keymap (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_keymap (display);
}

/*<private>
 * gdk_display_create_vulkan_context:
 * @self: a `GdkDisplay`
 * @surface: (nullable): the `GdkSurface` to use or %NULL for a surfaceless
 *   context
 * @error: return location for an error
 *
 * Creates a new `GdkVulkanContext` for use with @display.
 *
 * If @surface is NULL, the context can not be used to draw to surfaces,
 * it can only be used for custom rendering or compute.
 *
 * If the creation of the `GdkVulkanContext` failed, @error will be set.
 *
 * Returns: (transfer full): the newly created `GdkVulkanContext`, or
 *   %NULL on error
 */
GdkVulkanContext *
gdk_display_create_vulkan_context (GdkDisplay  *self,
                                   GdkSurface  *surface,
                                   GError     **error)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (self), NULL);
  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (gdk_display_get_debug_flags (self) & GDK_DEBUG_VULKAN_DISABLE)
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           _("Vulkan support disabled via GDK_DEBUG"));
      return NULL;
    }

  if (GDK_DISPLAY_GET_CLASS (self)->vk_extension_name == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The %s backend has no Vulkan support.", G_OBJECT_TYPE_NAME (self));
      return FALSE;
    }

  if (surface)
    {
      return g_initable_new (GDK_DISPLAY_GET_CLASS (self)->vk_context_type,
                             NULL,
                             error,
                             "surface", surface,
                             NULL);
    }
  else
    {
      return g_initable_new (GDK_DISPLAY_GET_CLASS (self)->vk_context_type,
                             NULL,
                             error,
                             "display", self,
                             NULL);
    }
}

gboolean
gdk_display_has_vulkan_feature (GdkDisplay        *self,
                                GdkVulkanFeatures  feature)
{
#ifdef GDK_RENDERING_VULKAN
  return !!(self->vulkan_features & feature);
#else
  return FALSE;
#endif
}

static void
gdk_display_init_gl (GdkDisplay *self)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);
  GdkGLContext *context;
  gint64 before G_GNUC_UNUSED;
  gint64 before2 G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  if (gdk_display_get_debug_flags (self) & GDK_DEBUG_GL_DISABLE)
    {
      g_set_error_literal (&priv->gl_error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("GL support disabled via GDK_DEBUG"));
      return;
    }

  context = GDK_DISPLAY_GET_CLASS (self)->init_gl (self, &priv->gl_error);
  if (context == NULL)
    return;

  before2 = GDK_PROFILER_CURRENT_TIME;

  if (!gdk_gl_context_realize (context, &priv->gl_error))
    {
      g_object_unref (context);
      return;
    }

  gdk_profiler_end_mark (before2, "Realize OpenGL context", NULL);

  /* Only assign after realize, so GdkGLContext::realize() can use
   * gdk_display_get_gl_context() == NULL to differentiate between
   * the display's context and any other context.
   */
  priv->gl_context = context;

  gdk_gl_backend_use (GDK_GL_CONTEXT_GET_CLASS (context)->backend_type);

  gdk_profiler_end_mark (before, "Init OpenGL", NULL);
}

/**
 * gdk_display_prepare_gl:
 * @self: a `GdkDisplay`
 * @error: return location for a `GError`
 *
 * Checks that OpenGL is available for @self and ensures that it is
 * properly initialized.
 * When this fails, an @error will be set describing the error and this
 * function returns %FALSE.
 *
 * Note that even if this function succeeds, creating a `GdkGLContext`
 * may still fail.
 *
 * This function is idempotent. Calling it multiple times will just
 * return the same value or error.
 *
 * You never need to call this function, GDK will call it automatically
 * as needed. But you can use it as a check when setting up code that
 * might make use of OpenGL.
 *
 * Returns: %TRUE if the display supports OpenGL
 *
 * Since: 4.4
 **/
gboolean
gdk_display_prepare_gl (GdkDisplay  *self,
                        GError     **error)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DISPLAY (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  for (;;)
    {
      if (priv->gl_context)
        return TRUE;

      if (priv->gl_error != NULL)
        {
          if (error)
            *error = g_error_copy (priv->gl_error);


          return FALSE;
        }

      gdk_display_init_gl (self);

      /* try again */
    }
}

/**
 * gdk_display_create_gl_context:
 * @self: a `GdkDisplay`
 * @error: return location for an error
 *
 * Creates a new `GdkGLContext` for the `GdkDisplay`.
 *
 * The context is disconnected from any particular surface or surface
 * and cannot be used to draw to any surface. It can only be used to
 * draw to non-surface framebuffers like textures.
 *
 * If the creation of the `GdkGLContext` failed, @error will be set.
 * Before using the returned `GdkGLContext`, you will need to
 * call [method@Gdk.GLContext.make_current] or [method@Gdk.GLContext.realize].
 *
 * Returns: (transfer full): the newly created `GdkGLContext`
 *
 * Since: 4.6
 */
GdkGLContext *
gdk_display_create_gl_context (GdkDisplay  *self,
                               GError     **error)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!gdk_display_prepare_gl (self, error))
    return NULL;

  return gdk_gl_context_new (self, NULL);
}

/*< private >
 * gdk_display_get_gl_context:
 * @self: the `GdkDisplay`
 *
 * Gets the GL context returned from [vfunc@Gdk.Display.init_gl]
 * previously.
 *
 * If that function has not been called yet or did fail, %NULL is
 * returned.
 * Call [method@Gdk.Display.prepare_gl] to avoid this.
 *
 * Returns: The `GdkGLContext`
 */
GdkGLContext *
gdk_display_get_gl_context (GdkDisplay *self)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);

  return priv->gl_context;
}

#ifdef HAVE_EGL
static int
strvcmp (gconstpointer p1,
         gconstpointer p2)
{
  const char * const *s1 = p1;
  const char * const *s2 = p2;

  return strcmp (*s1, *s2);
}

static char *
describe_extensions (EGLDisplay egl_display)
{
  const char *extensions;
  char **exts;
  char *ext;

  extensions = eglQueryString (egl_display, EGL_EXTENSIONS);

  exts = g_strsplit (extensions, " ", -1);
  qsort (exts, g_strv_length (exts), sizeof (char *), strvcmp);

  ext = g_strjoinv ("\n\t", exts);
  if (ext[0] == '\n')
    ext[0] = ' ';

  g_strfreev (exts);

  return g_strstrip (ext);
}

static char *
describe_egl_config (EGLDisplay egl_display,
                     EGLConfig  egl_config)
{
  EGLint red, green, blue, alpha, type;
  EGLint depth, stencil;

  if (egl_config == NULL)
    return g_strdup ("-");

  if (!eglGetConfigAttrib (egl_display, egl_config, EGL_RED_SIZE, &red) ||
      !eglGetConfigAttrib (egl_display, egl_config, EGL_GREEN_SIZE, &green) ||
      !eglGetConfigAttrib (egl_display, egl_config, EGL_BLUE_SIZE, &blue) ||
      !eglGetConfigAttrib (egl_display, egl_config, EGL_ALPHA_SIZE, &alpha))
    return g_strdup ("Unknown");

  if (epoxy_has_egl_extension (egl_display, "EGL_EXT_pixel_format_float"))
    {
      if (!eglGetConfigAttrib (egl_display, egl_config, EGL_COLOR_COMPONENT_TYPE_EXT, &type))
        type = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;
    }
  else
    type = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

  if (!eglGetConfigAttrib (egl_display, egl_config, EGL_DEPTH_SIZE, &depth))
    depth = 0;

  if (!eglGetConfigAttrib (egl_display, egl_config, EGL_STENCIL_SIZE, &stencil))
    stencil = 0;

  return g_strdup_printf ("R%dG%dB%dA%d%s, depth %d, stencil %d", red, green, blue, alpha,
                          type == EGL_COLOR_COMPONENT_TYPE_FIXED_EXT ? "" : " float",
                          depth,
                          stencil);
}

gpointer
gdk_display_get_egl_config (GdkDisplay *self)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);

  return priv->egl_config;
}

gpointer
gdk_display_get_egl_config_high_depth (GdkDisplay *self)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);

  return priv->egl_config_high_depth;
}

static EGLDisplay
gdk_display_create_egl_display (EGLenum  platform,
                                gpointer native_display)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  EGLDisplay egl_display = NULL;

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay != NULL)
        egl_display = getPlatformDisplay (platform, native_display, NULL);
      if (egl_display != NULL)
        goto out;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay != NULL)
        egl_display = getPlatformDisplay (platform, native_display, NULL);
      if (egl_display != NULL)
        goto out;
    }

  egl_display = eglGetDisplay ((EGLNativeDisplayType) native_display);

out:
  gdk_profiler_end_mark (start_time, "Create EGL display", NULL);

  return egl_display;
}

#define MAX_EGL_ATTRS 30

typedef enum {
  GDK_EGL_CONFIG_PERFECT = (1 << 0),
  GDK_EGL_CONFIG_HDR     = (1 << 1),
} GdkEGLConfigCreateFlags;

static EGLConfig
gdk_display_create_egl_config (GdkDisplay               *self,
                               GdkEGLConfigCreateFlags   flags,
                               GError                  **error)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  EGLint attrs[MAX_EGL_ATTRS];
  EGLConfig *configs;
  EGLint count, alloced;
  EGLConfig best_config;
  guint best_score;

  int i = 0;

  attrs[i++] = EGL_SURFACE_TYPE;
  attrs[i++] = EGL_WINDOW_BIT;

  attrs[i++] = EGL_COLOR_BUFFER_TYPE;
  attrs[i++] = EGL_RGB_BUFFER;

  attrs[i++] = EGL_RED_SIZE;
  attrs[i++] = (flags & GDK_EGL_CONFIG_HDR) ? 9 : 8;
  attrs[i++] = EGL_GREEN_SIZE;
  attrs[i++] = (flags & GDK_EGL_CONFIG_HDR) ? 9 : 8;
  attrs[i++] = EGL_BLUE_SIZE;
  attrs[i++] = (flags & GDK_EGL_CONFIG_HDR) ? 9 : 8;
  attrs[i++] = EGL_ALPHA_SIZE;
  attrs[i++] = 8;

  if (flags & GDK_EGL_CONFIG_HDR &&
      self->have_egl_pixel_format_float)
    {
      attrs[i++] = EGL_COLOR_COMPONENT_TYPE_EXT;
      attrs[i++] = EGL_DONT_CARE;
    }

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  if (!eglChooseConfig (priv->egl_display, attrs, NULL, -1, &alloced) || alloced == 0)
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No EGL configuration available"));
      return NULL;
    }

  configs = g_new (EGLConfig, alloced);
  if (!eglChooseConfig (priv->egl_display, attrs, configs, alloced, &count))
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Failed to get EGL configurations"));
      return NULL;
    }
  g_warn_if_fail (alloced == count);

  best_score = G_MAXUINT;
  best_config = NULL;

  for (i = 0; i < count; i++)
    {
      guint score = GDK_DISPLAY_GET_CLASS (self)->rate_egl_config (self, priv->egl_display, configs[i]);

      if (score < best_score)
        {
          best_score = score;
          best_config = configs[i];
        }

      if (score == 0)
        break;
    }

  g_free (configs);

  gdk_profiler_end_mark (start_time, "Create EGL config", NULL);

  if (best_score == G_MAXUINT)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No EGL configuration with required features found"));
      return NULL;
    }
  else if ((flags & GDK_EGL_CONFIG_PERFECT) && best_score != 0)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No perfect EGL configuration found"));
      return NULL;
    }

  return best_config;
}

#undef MAX_EGL_ATTRS

static gboolean
gdk_display_check_egl_extensions (EGLDisplay   egl_display,
                                  const char **extensions,
                                  GError     **error)
{
  GString *missing = NULL;
  gsize i, n_missing;

  n_missing = 0;

  for (i = 0; extensions[i] != NULL; i++)
    {
      if (!epoxy_has_egl_extension (egl_display, extensions[i]))
        {
          if (missing == NULL)
            {
              missing = g_string_new (extensions[i]);
            }
          else
            {
              g_string_append (missing, ", ");
              g_string_append (missing, extensions[i]);
            }
          n_missing++;
        }
    }

  if (n_missing)
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                   /* translators: Arguments are the number of missing extensions
                    * followed by a comma-separated list of their names */
                   g_dngettext (GETTEXT_PACKAGE,
                                "EGL implementation is missing extension %s",
                                "EGL implementation is missing %2$d extensions: %1$s",
                                n_missing),
                   missing->str, (int) n_missing);

      g_string_free (missing, TRUE);
      return FALSE;
    }

  return TRUE;
}

static const char *
find_egl_device (EGLDisplay egl_display)
{
  EGLAttrib value;
  EGLDeviceEXT egl_device;

  eglQueryDisplayAttribEXT (egl_display, EGL_DEVICE_EXT, &value);

  egl_device = (EGLDeviceEXT)value;

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

  return eglQueryDeviceStringEXT (egl_device, EGL_DRM_RENDER_NODE_FILE_EXT);
}

gboolean
gdk_display_init_egl (GdkDisplay  *self,
                      int          platform,
                      gpointer     native_display,
                      gboolean     allow_any,
                      GError     **error)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  int major, minor;

  if (!gdk_gl_backend_can_be_used (GDK_GL_EGL, error))
    return FALSE;

  if (!epoxy_has_egl ())
    {
      gboolean sandboxed = gdk_running_in_sandbox ();

      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           sandboxed ? _("libEGL not available in this sandbox")
                                     : _("libEGL not available"));
      return FALSE;
    }

  priv->egl_display = gdk_display_create_egl_display (platform, native_display);

  if (priv->egl_display == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Failed to create EGL display"));
      return FALSE;
    }

  if (!eglInitialize (priv->egl_display, &major, &minor))
    {
      priv->egl_display = NULL;
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Could not initialize EGL display"));
      return FALSE;
    }

  if (major < GDK_EGL_MIN_VERSION_MAJOR ||
      (major == GDK_EGL_MIN_VERSION_MAJOR && minor < GDK_EGL_MIN_VERSION_MINOR))
    {
      g_clear_pointer (&priv->egl_display, eglTerminate);
      g_set_error (error, GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   _("EGL version %d.%d is too old. GTK requires %d.%d"),
                   major, minor, GDK_EGL_MIN_VERSION_MAJOR, GDK_EGL_MIN_VERSION_MINOR);
      return FALSE;
    }

  if (!gdk_display_check_egl_extensions (priv->egl_display,
                                         (const char *[]) {
                                           "EGL_KHR_create_context",
                                           "EGL_KHR_surfaceless_context",
                                           NULL
                                         },
                                         error))
    {
      g_clear_pointer (&priv->egl_display, eglTerminate);
      return FALSE;
    }

  priv->egl_config = gdk_display_create_egl_config (self,
                                                    allow_any ? 0 : GDK_EGL_CONFIG_PERFECT,
                                                    error);
  if (priv->egl_config == NULL)
    {
      g_clear_pointer (&priv->egl_display, eglTerminate);
      return FALSE;
    }

  self->have_egl_buffer_age =
    epoxy_has_egl_extension (priv->egl_display, "EGL_EXT_buffer_age");
  self->have_egl_no_config_context =
    epoxy_has_egl_extension (priv->egl_display, "EGL_KHR_no_config_context");
  self->have_egl_pixel_format_float =
    epoxy_has_egl_extension (priv->egl_display, "EGL_EXT_pixel_format_float");
  self->have_egl_dma_buf_import =
    epoxy_has_egl_extension (priv->egl_display, "EGL_EXT_image_dma_buf_import_modifiers");
  self->have_egl_dma_buf_export =
    epoxy_has_egl_extension (priv->egl_display, "EGL_MESA_image_dma_buf_export");

  if (self->have_egl_no_config_context)
    priv->egl_config_high_depth = gdk_display_create_egl_config (self,
                                                          GDK_EGL_CONFIG_HDR,
                                                          error);
  if (priv->egl_config_high_depth == NULL)
    priv->egl_config_high_depth = priv->egl_config;

  if (GDK_DISPLAY_DEBUG_CHECK (self, OPENGL))
    {
      char *ext = describe_extensions (priv->egl_display);
      char *std_cfg = describe_egl_config (priv->egl_display, priv->egl_config);
      char *hd_cfg = describe_egl_config (priv->egl_display, priv->egl_config_high_depth);
      const char *path;
      struct stat buf = { .st_rdev = 0, };

      path = find_egl_device (priv->egl_display);
      if (path)
        stat (path, &buf);

      gdk_debug_message ("EGL API version %d.%d found\n"
                         " - Vendor: %s\n"
                         " - Version: %s\n"
                         " - Device: %s, %d %d\n"
                         " - Client APIs: %s\n"
                         " - Extensions:\n"
                         "\t%s\n"
                         " - Selected fbconfig: %s\n"
                         "          high depth: %s",
                         major, minor,
                         eglQueryString (priv->egl_display, EGL_VENDOR),
                         eglQueryString (priv->egl_display, EGL_VERSION),
                         path ? path : "unknown",
#ifdef HAVE_SYS_SYSMACROS_H
                         major (buf.st_rdev), minor (buf.st_rdev),
#else
                         0, 0,
#endif
                         eglQueryString (priv->egl_display, EGL_CLIENT_APIS),
                         ext, std_cfg,
                         priv->egl_config_high_depth == priv->egl_config ? "none" : hd_cfg);
      g_free (hd_cfg);
      g_free (std_cfg);
      g_free (ext);
    }

  gdk_profiler_end_mark (start_time, "Init EGL", NULL);

  return TRUE;
}
#endif

/*<private>
 * gdk_display_get_egl_display:
 * @self: a display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * This function returns `NULL` if GL is not supported or GDK is using
 * a different OpenGL framework than EGL.
 *
 * Returns: (nullable): the EGL display object
 */
gpointer
gdk_display_get_egl_display (GdkDisplay *self)
{
#ifdef HAVE_EGL
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DISPLAY (self), NULL);

  if (!priv->egl_display &&
      !gdk_display_prepare_gl (self, NULL))
    return NULL;

  return priv->egl_display;
#else
  return NULL;
#endif
}

#ifdef HAVE_DMABUF
static void
gdk_display_add_dmabuf_downloader (GdkDisplay          *display,
                                   GdkDmabufDownloader *downloader)
{
  gsize i;

  if (downloader == NULL)
    return;

  /* dmabuf_downloaders is NULL-terminated */
  for (i = 0; i < G_N_ELEMENTS (display->dmabuf_downloaders) - 1; i++)
    {
      if (display->dmabuf_downloaders[i] == NULL)
        break;
    }

  g_assert (i < G_N_ELEMENTS (display->dmabuf_downloaders) - 1);

  display->dmabuf_downloaders[i] = downloader;
}
#endif

/* To support a drm format, we must be able to import it into GL
 * using the relevant EGL extensions, and download it into a memory
 * texture, possibly doing format conversion with shaders (in GSK).
 */
void
gdk_display_init_dmabuf (GdkDisplay *self)
{
  GdkDmabufFormatsBuilder *builder;

  if (self->dmabuf_formats != NULL)
    return;

  GDK_DISPLAY_DEBUG (self, DMABUF,
                     "Beginning initialization of dmabuf support");

  builder = gdk_dmabuf_formats_builder_new ();

#ifdef HAVE_DMABUF
  if (!GDK_DISPLAY_DEBUG_CHECK (self, DMABUF_DISABLE))
    {
#ifdef GDK_RENDERING_VULKAN
      gdk_display_add_dmabuf_downloader (self, gdk_vulkan_get_dmabuf_downloader (self, builder));
#endif

#ifdef HAVE_EGL
      gdk_display_add_dmabuf_downloader (self, gdk_dmabuf_get_egl_downloader (self, builder));
#endif

      gdk_dmabuf_formats_builder_add_formats (builder,
                                              gdk_dmabuf_get_mmap_formats ());
    }
#endif

  self->dmabuf_formats = gdk_dmabuf_formats_builder_free_to_formats (builder);

  GDK_DISPLAY_DEBUG (self, DMABUF,
                     "Initialized support for %zu dmabuf formats",
                     gdk_dmabuf_formats_get_n_formats (self->dmabuf_formats));
}

/**
 * gdk_display_get_dmabuf_formats:
 * @display: a `GdkDisplay`
 *
 * Returns the dma-buf formats that are supported on this display.
 *
 * GTK may use OpenGL or Vulkan to support some formats.
 * Calling this function will then initialize them if they aren't yet.
 *
 * The formats returned by this function can be used for negotiating
 * buffer formats with producers such as v4l, pipewire or GStreamer.
 *
 * To learn more about dma-bufs, see [class@Gdk.DmabufTextureBuilder].
 *
 * Returns: (transfer none): a `GdkDmabufFormats` object
 *
 * Since: 4.14
 */
GdkDmabufFormats *
gdk_display_get_dmabuf_formats (GdkDisplay *display)
{
  gdk_display_init_dmabuf (display);

  return display->dmabuf_formats;
}

GdkDebugFlags
gdk_display_get_debug_flags (GdkDisplay *display)
{
  if (display == NULL)
    return _gdk_debug_flags;

  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  return priv->debug_flags;
}

void
gdk_display_set_debug_flags (GdkDisplay    *display,
                             GdkDebugFlags  flags)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  priv->debug_flags = flags;
}

/**
 * gdk_display_is_composited: (attributes org.gtk.Method.get_property=composited)
 * @display: a `GdkDisplay`
 *
 * Returns whether surfaces can reasonably be expected to have
 * their alpha channel drawn correctly on the screen.
 *
 * Check [method@Gdk.Display.is_rgba] for whether the display
 * supports an alpha channel.
 *
 * On X11 this function returns whether a compositing manager is
 * compositing on @display.
 *
 * On modern displays, this value is always %TRUE.
 *
 * Returns: Whether surfaces with RGBA visuals can reasonably
 *   be expected to have their alpha channels drawn correctly
 *   on the screen.
 */
gboolean
gdk_display_is_composited (GdkDisplay *display)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return priv->composited;
}

void
gdk_display_set_composited (GdkDisplay *display,
                            gboolean    composited)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->composited == composited)
    return;

  priv->composited = composited;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_COMPOSITED]);
}

/**
 * gdk_display_is_rgba: (attributes org.gtk.Method.get_property=rgba)
 * @display: a `GdkDisplay`
 *
 * Returns whether surfaces on this @display are created with an
 * alpha channel.
 *
 * Even if a %TRUE is returned, it is possible that the
 * surface’s alpha channel won’t be honored when displaying the
 * surface on the screen: in particular, for X an appropriate
 * windowing manager and compositing manager must be running to
 * provide appropriate display. Use [method@Gdk.Display.is_composited]
 * to check if that is the case.
 *
 * On modern displays, this value is always %TRUE.
 *
 * Returns: %TRUE if surfaces are created with an alpha channel or
 *   %FALSE if the display does not support this functionality.
 */
gboolean
gdk_display_is_rgba (GdkDisplay *display)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return priv->rgba;
}

void
gdk_display_set_rgba (GdkDisplay *display,
                      gboolean    rgba)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->rgba == rgba)
    return;

  priv->rgba = rgba;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_RGBA]);
}

/**
 * gdk_display_supports_shadow_width: (attributes org.gtk.Method.get_property=shadow-width)
 * @display: a `GdkDisplay`
 *
 * Returns whether it's possible for a surface to draw outside of the window area.
 *
 * If %TRUE is returned the application decides if it wants to draw shadows.
 * If %FALSE is returned, the compositor decides if it wants to draw shadows.
 *
 * Returns: %TRUE if surfaces can draw shadows or
 *   %FALSE if the display does not support this functionality.
 *
 * Since: 4.14
 */
gboolean
gdk_display_supports_shadow_width (GdkDisplay *display)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return priv->shadow_width;
}

void
gdk_display_set_shadow_width (GdkDisplay *display,
                              gboolean    shadow_width)
{
  GdkDisplayPrivate *priv = gdk_display_get_instance_private (display);

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->shadow_width == shadow_width)
    return;

  priv->shadow_width = shadow_width;

  g_object_notify_by_pspec (G_OBJECT (display), props[PROP_SHADOW_WIDTH]);
}

static void
device_removed_cb (GdkSeat    *seat,
                   GdkDevice  *device,
                   GdkDisplay *display)
{
  g_hash_table_remove (display->device_grabs, device);
  g_hash_table_remove (display->pointers_info, device);

  /* FIXME: change core pointer and remove from device list */
}

void
gdk_display_add_seat (GdkDisplay *display,
                      GdkSeat    *seat)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_SEAT (seat));

  display->seats = g_list_append (display->seats, g_object_ref (seat));
  g_signal_emit (display, signals[SEAT_ADDED], 0, seat);

  g_signal_connect (seat, "device-removed", G_CALLBACK (device_removed_cb), display);
}

void
gdk_display_remove_seat (GdkDisplay *display,
                         GdkSeat    *seat)
{
  GList *link;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_SEAT (seat));

  g_signal_handlers_disconnect_by_func (seat, G_CALLBACK (device_removed_cb), display);

  link = g_list_find (display->seats, seat);

  if (link)
    {
      display->seats = g_list_remove_link (display->seats, link);
      g_signal_emit (display, signals[SEAT_REMOVED], 0, seat);
      g_object_unref (link->data);
      g_list_free (link);
    }
}

/**
 * gdk_display_get_default_seat:
 * @display: a `GdkDisplay`
 *
 * Returns the default `GdkSeat` for this display.
 *
 * Note that a display may not have a seat. In this case,
 * this function will return %NULL.
 *
 * Returns: (transfer none) (nullable): the default seat.
 **/
GdkSeat *
gdk_display_get_default_seat (GdkDisplay *display)
{
  GdkDisplayClass *display_class;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  display_class = GDK_DISPLAY_GET_CLASS (display);

  return display_class->get_default_seat (display);
}

/**
 * gdk_display_list_seats:
 * @display: a `GdkDisplay`
 *
 * Returns the list of seats known to @display.
 *
 * Returns: (transfer container) (element-type GdkSeat): the
 *   list of seats known to the `GdkDisplay`
 */
GList *
gdk_display_list_seats (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_list_copy (display->seats);
}

/**
 * gdk_display_get_monitors:
 * @self: a `GdkDisplay`
 *
 * Gets the list of monitors associated with this display.
 *
 * Subsequent calls to this function will always return the
 * same list for the same display.
 *
 * You can listen to the GListModel::items-changed signal on
 * this list to monitor changes to the monitor of this display.
 *
 * Returns: (transfer none): a `GListModel` of `GdkMonitor`
 */
GListModel *
gdk_display_get_monitors (GdkDisplay *self)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (self), NULL);

  return GDK_DISPLAY_GET_CLASS (self)->get_monitors (self);
}

/**
 * gdk_display_get_monitor_at_surface:
 * @display: a `GdkDisplay`
 * @surface: a `GdkSurface`
 *
 * Gets the monitor in which the largest area of @surface
 * resides.
 *
 * Returns: (transfer none) (nullable): the monitor with the largest
 *   overlap with @surface
 */
GdkMonitor *
gdk_display_get_monitor_at_surface (GdkDisplay *display,
                                   GdkSurface  *surface)
{
  GdkRectangle win;
  GListModel *monitors;
  guint i;
  int area = 0;
  GdkMonitor *best = NULL;
  GdkDisplayClass *class;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  class = GDK_DISPLAY_GET_CLASS (display);
  if (class->get_monitor_at_surface)
    {
      best = class->get_monitor_at_surface (display, surface);

      if (best)
        return best;
    }

  /* the fallback implementation requires global coordinates */
  gdk_surface_get_geometry (surface, &win.x, &win.y, &win.width, &win.height);
  gdk_surface_get_origin (surface, &win.x, &win.y);

  monitors = gdk_display_get_monitors (display);
  for (i = 0; i < g_list_model_get_n_items (monitors); i++)
    {
      GdkMonitor *monitor;
      GdkRectangle mon, intersect;
      int overlap;

      monitor = g_list_model_get_item (monitors, i);
      gdk_monitor_get_geometry (monitor, &mon);
      gdk_rectangle_intersect (&win, &mon, &intersect);
      overlap = intersect.width *intersect.height;
      if (overlap > area)
        {
          area = overlap;
          best = monitor;
        }
      g_object_unref (monitor);
    }

  return best;
}

void
gdk_display_emit_opened (GdkDisplay *display)
{
  g_signal_emit (display, signals[OPENED], 0);
}

/**
 * gdk_display_get_setting:
 * @display: a `GdkDisplay`
 * @name: the name of the setting
 * @value: location to store the value of the setting
 *
 * Retrieves a desktop-wide setting such as double-click time
 * for the @display.
 *
 * Returns: %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise
 */
gboolean
gdk_display_get_setting (GdkDisplay *display,
                         const char *name,
                         GValue     *value)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return GDK_DISPLAY_GET_CLASS (display)->get_setting (display, name, value);
}

void
gdk_display_setting_changed (GdkDisplay       *display,
                             const char       *name)
{
  g_signal_emit (display, signals[SETTING_CHANGED], 0, name);
}

void
gdk_display_set_double_click_time (GdkDisplay *display,
                                   guint       msec)
{
  display->double_click_time = msec;
}

void
gdk_display_set_double_click_distance (GdkDisplay *display,
                                       guint       distance)
{
  display->double_click_distance = distance;
}

void
gdk_display_set_cursor_theme (GdkDisplay *display,
                              const char *name,
                              int         size)
{
  if (GDK_DISPLAY_GET_CLASS (display)->set_cursor_theme)
    GDK_DISPLAY_GET_CLASS (display)->set_cursor_theme (display, name, size);
}

/**
 * gdk_display_map_keyval:
 * @display: a `GdkDisplay`
 * @keyval: a keyval, such as %GDK_KEY_a, %GDK_KEY_Up, %GDK_KEY_Return, etc.
 * @keys: (out) (array length=n_keys) (transfer full): return location
 *   for an array of `GdkKeymapKey`
 * @n_keys: return location for number of elements in returned array
 *
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval.
 *
 * Groups and levels are two kinds of keyboard mode; in general, the level
 * determines whether the top or bottom symbol on a key is used, and the
 * group determines whether the left or right symbol is used.
 *
 * On US keyboards, the shift key changes the keyboard level, and there
 * are no groups. A group switch key might convert a keyboard between
 * Hebrew to English modes, for example.
 *
 * `GdkEventKey` contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 *
 * The returned array should be freed with g_free().
 *
 * Returns: %TRUE if keys were found and returned
 */
gboolean
gdk_display_map_keyval (GdkDisplay    *display,
                        guint          keyval,
                        GdkKeymapKey **keys,
                        int           *n_keys)
{
  return gdk_keymap_get_entries_for_keyval (gdk_display_get_keymap (display),
                                            keyval,
                                            keys,
                                            n_keys);
}

/**
 * gdk_display_map_keycode:
 * @display: a `GdkDisplay`
 * @keycode: a keycode
 * @keys: (out) (array length=n_entries) (transfer full) (optional): return
 *   location for array of `GdkKeymapKey`
 * @keyvals: (out) (array length=n_entries) (transfer full) (optional): return
 *   location for array of keyvals
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @keycode.
 *
 * The Nth `GdkKeymapKey` in @keys is bound to the Nth keyval in @keyvals.
 *
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level.
 *
 * Free the returned arrays with g_free().
 *
 * Returns: %TRUE if there were any entries
 */
gboolean
gdk_display_map_keycode (GdkDisplay    *display,
                         guint          keycode,
                         GdkKeymapKey **keys,
                         guint        **keyvals,
                         int           *n_entries)
{
  return gdk_keymap_get_entries_for_keycode (gdk_display_get_keymap (display),
                                             keycode,
                                             keys,
                                             keyvals,
                                             n_entries);
}

/**
 * gdk_display_translate_key:
 * @display: a `GdkDisplay`
 * @keycode: a keycode
 * @state: a modifier state
 * @group: active keyboard group
 * @keyval: (out) (optional): return location for keyval
 * @effective_group: (out) (optional): return location for effective group
 * @level: (out) (optional): return location for level
 * @consumed: (out) (optional): return location for modifiers that were used
 *   to determine the group or level
 *
 * Translates the contents of a `GdkEventKey` into a keyval, effective group,
 * and level.
 *
 * Modifiers that affected the translation and are thus unavailable for
 * application use are returned in @consumed_modifiers.
 *
 * The @effective_group is the group that was actually used for the
 * translation; some keys such as Enter are not affected by the active
 * keyboard group. The @level is derived from @state.
 *
 * @consumed_modifiers gives modifiers that should be masked out
 * from @state when comparing this key press to a keyboard shortcut.
 * For instance, on a US keyboard, the `plus` symbol is shifted, so
 * when comparing a key press to a `<Control>plus` accelerator `<Shift>`
 * should be masked out.
 *
 * This function should rarely be needed, since `GdkEventKey` already
 * contains the translated keyval. It is exported for the benefit of
 * virtualized test environments.
 *
 * Returns: %TRUE if there was a keyval bound to keycode/state/group.
 */
gboolean
gdk_display_translate_key (GdkDisplay      *display,
                           guint            keycode,
                           GdkModifierType  state,
                           int              group,
                           guint           *keyval,
                           int             *effective_group,
                           int             *level,
                           GdkModifierType *consumed)
{
  return gdk_keymap_translate_keyboard_state (gdk_display_get_keymap (display),
                                              keycode, state, group,
                                              keyval,
                                              effective_group,
                                              level,
                                              consumed);
}
