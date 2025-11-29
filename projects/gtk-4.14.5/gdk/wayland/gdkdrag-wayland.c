/*
 * Copyright © 2010 Intel Corporation
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

#include "gdkdragprivate.h"

#include "gdkprivate-wayland.h"
#include "gdkcontentformats.h"
#include "gdkdisplay-wayland.h"
#include <glib/gi18n-lib.h>
#include "gdkseat-wayland.h"
#include "gdksurface-wayland-private.h"

#include "gdkdeviceprivate.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <string.h>

#define GDK_TYPE_WAYLAND_DRAG              (gdk_wayland_drag_get_type ())
#define GDK_WAYLAND_DRAG(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DRAG, GdkWaylandDrag))
#define GDK_WAYLAND_DRAG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DRAG, GdkWaylandDragClass))
#define GDK_IS_WAYLAND_DRAG(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DRAG))
#define GDK_IS_WAYLAND_DRAG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DRAG))
#define GDK_WAYLAND_DRAG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DRAG, GdkWaylandDragClass))

typedef struct _GdkWaylandDrag GdkWaylandDrag;
typedef struct _GdkWaylandDragClass GdkWaylandDragClass;

struct _GdkWaylandDrag
{
  GdkDrag drag;
  GdkSurface *dnd_surface;
  struct wl_data_source *data_source;
  struct wl_data_offer *offer;
  uint32_t serial;
  int hot_x;
  int hot_y;
};

struct _GdkWaylandDragClass
{
  GdkDragClass parent_class;
};

static GList *drags;

GType gdk_wayland_drag_get_type (void);

G_DEFINE_TYPE (GdkWaylandDrag, gdk_wayland_drag, GDK_TYPE_DRAG)

static void
gdk_wayland_drag_finalize (GObject *object)
{
  GdkWaylandDrag *wayland_drag = GDK_WAYLAND_DRAG (object);
  GdkDrag *drag = GDK_DRAG (object);
  GdkSurface *dnd_surface;

  drags = g_list_remove (drags, drag);

  gdk_drag_set_cursor (drag, NULL);

  g_clear_pointer (&wayland_drag->data_source, wl_data_source_destroy);
  g_clear_pointer (&wayland_drag->offer, wl_data_offer_destroy);

  dnd_surface = wayland_drag->dnd_surface;

  G_OBJECT_CLASS (gdk_wayland_drag_parent_class)->finalize (object);

  if (dnd_surface)
    gdk_surface_destroy (dnd_surface);
}

static inline uint32_t
gdk_to_wl_actions (GdkDragAction action)
{
  uint32_t dnd_actions = 0;

  if (action & (GDK_ACTION_COPY | GDK_ACTION_LINK))
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  if (action & GDK_ACTION_MOVE)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  if (action & GDK_ACTION_ASK)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

  return dnd_actions;
}

static void
gdk_wayland_drag_init (GdkWaylandDrag *drag_wayland)
{
  GdkDrag *drag;

  drag = GDK_DRAG (drag_wayland);
  drags = g_list_prepend (drags, drag);

  gdk_drag_set_selected_action (drag, GDK_ACTION_COPY);
}

static GdkSurface *
gdk_wayland_drag_get_drag_surface (GdkDrag *drag)
{
  return GDK_WAYLAND_DRAG (drag)->dnd_surface;
}

static void
gdk_wayland_drag_set_hotspot (GdkDrag *drag,
                              int      hot_x,
                              int      hot_y)
{
  GdkWaylandDrag *drag_wayland = GDK_WAYLAND_DRAG (drag);
  int prev_hot_x = drag_wayland->hot_x;
  int prev_hot_y = drag_wayland->hot_y;
  const GdkRectangle damage_rect = { .width = 1, .height = 1 };

  drag_wayland->hot_x = hot_x;
  drag_wayland->hot_y = hot_y;

  if (prev_hot_x == hot_x && prev_hot_y == hot_y)
    return;

  _gdk_wayland_surface_offset_next_wl_buffer (drag_wayland->dnd_surface,
                                             prev_hot_x - hot_x, prev_hot_y - hot_y);
  gdk_surface_invalidate_rect (drag_wayland->dnd_surface, &damage_rect);
}

static void
gdk_wayland_drag_set_cursor (GdkDrag   *drag,
                             GdkCursor *cursor)
{
  GdkDevice *device = gdk_drag_get_device (drag);

  if (device != NULL)
    gdk_wayland_seat_set_global_cursor (gdk_device_get_seat (device), cursor);
}

static void
gdk_wayland_drag_drop_performed (GdkDrag *drag,
                                 guint32  time_)
{
  gdk_drag_set_cursor (drag, NULL);
}

static void
gdk_wayland_drag_cancel (GdkDrag             *drag,
                         GdkDragCancelReason  reason)
{
  gdk_drag_set_cursor (drag, NULL);
  gdk_drag_drop_done (drag, FALSE);
}

static void
gdk_wayland_drag_drop_done (GdkDrag  *drag,
                            gboolean  success)
{
  GdkWaylandDrag *drag_wayland = GDK_WAYLAND_DRAG (drag);
  GdkDevice *device = gdk_drag_get_device (drag);

  gdk_wayland_seat_set_drag (gdk_device_get_seat (device), NULL);

  if (success)
    {
      if (drag_wayland->dnd_surface)
        gdk_surface_hide (drag_wayland->dnd_surface);
    }
}

static void
gdk_wayland_drag_class_init (GdkWaylandDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_wayland_drag_finalize;

  drag_class->get_drag_surface = gdk_wayland_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_wayland_drag_set_hotspot;
  drag_class->drop_done = gdk_wayland_drag_drop_done;
  drag_class->set_cursor = gdk_wayland_drag_set_cursor;
  drag_class->drop_performed = gdk_wayland_drag_drop_performed;
  drag_class->cancel = gdk_wayland_drag_cancel;
}

static inline GdkDragAction
_wl_to_gdk_actions (uint32_t dnd_actions)
{
  GdkDragAction actions = 0;

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions |= GDK_ACTION_COPY;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions |= GDK_ACTION_MOVE;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions |= GDK_ACTION_ASK;

  return actions;
}

static void
data_source_target (void                  *data,
                    struct wl_data_source *source,
                    const char            *mime_type)
{
  GDK_DEBUG (EVENTS, "data source target, source = %p, mime_type = %s",
                     source, mime_type);
}

static void
gdk_wayland_drag_write_done (GObject      *drag,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GError *error = NULL;

  if (!gdk_drag_write_finish (GDK_DRAG (drag), result, &error))
    {
      GDK_DISPLAY_DEBUG (gdk_drag_get_display (GDK_DRAG (drag)), DND,
                         "%p: failed to write stream: %s", drag, error->message);

      g_error_free (error);
    }
}

static void
data_source_send (void                  *data,
                  struct wl_data_source *source,
                  const char            *mime_type,
                  int32_t                fd)
{
  GdkDrag *drag = data;
  GOutputStream *stream;

  GDK_DISPLAY_DEBUG (gdk_drag_get_display (drag), DND,
                     "%p: data source send request for %s on fd %d\n",
                     source, mime_type, fd);

  //mime_type = gdk_intern_mime_type (mime_type);
  mime_type = g_intern_string (mime_type);
  stream = g_unix_output_stream_new (fd, TRUE);

  gdk_drag_write_async (drag,
                        mime_type,
                        stream,
                        G_PRIORITY_DEFAULT,
                        NULL,
                        gdk_wayland_drag_write_done,
                        drag);
  g_object_unref (stream);
}

static void
data_source_cancelled (void                  *data,
                       struct wl_data_source *source)
{
  GdkDrag *drag = data;

  GDK_DISPLAY_DEBUG (gdk_drag_get_display (drag), EVENTS,
                     "data source cancelled, source = %p", source);

  gdk_drag_cancel (drag, GDK_DRAG_CANCEL_ERROR);
}

static void
data_source_dnd_drop_performed (void                  *data,
                                struct wl_data_source *source)
{
  GdkDrag *drag = data;

  g_signal_emit_by_name (drag, "drop-performed");
}

static void
data_source_dnd_finished (void                  *data,
                          struct wl_data_source *source)
{
  GdkDrag *drag = data;

  g_object_ref (drag);
  g_signal_emit_by_name (drag, "dnd-finished");
  gdk_drag_drop_done (drag, TRUE);
  g_object_unref (drag);
}

static void
data_source_action (void                  *data,
                    struct wl_data_source *source,
                    uint32_t               action)
{
  GdkDrag *drag = data;

  GDK_DISPLAY_DEBUG (gdk_drag_get_display (drag), EVENTS,
                     "data source action, source = %p action=%x",
                     source, action);

  gdk_drag_set_selected_action (drag, _wl_to_gdk_actions (action));
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled,
  data_source_dnd_drop_performed,
  data_source_dnd_finished,
  data_source_action,
};

static void
gdk_wayland_drag_create_data_source (GdkDrag *drag)
{
  GdkWaylandDrag *drag_wayland = GDK_WAYLAND_DRAG (drag);
  GdkDisplay *display = gdk_drag_get_display (drag);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  const char *const *mimetypes;
  GdkContentFormats *formats;
  gsize i, n_mimetypes;

  drag_wayland->data_source = wl_data_device_manager_create_data_source (display_wayland->data_device_manager);
  wl_data_source_add_listener (drag_wayland->data_source,
                               &data_source_listener,
                               drag);

  formats = gdk_content_formats_ref (gdk_drag_get_formats (drag));
  formats = gdk_content_formats_union_serialize_mime_types (formats);

  mimetypes = gdk_content_formats_get_mime_types (formats, &n_mimetypes);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_drag_get_display (drag), EVENTS))
    {
      char *s = g_strjoinv (" ", (char **)mimetypes);
      gdk_debug_message ("create data source, mime types=%s", s);
      g_free (s);
    }

  wl_data_source_offer (drag_wayland->data_source, GDK_WAYLAND_LOCAL_DND_MIME_TYPE);
  for (i = 0; i < n_mimetypes; i++)
    wl_data_source_offer (drag_wayland->data_source, mimetypes[i]);

  gdk_content_formats_unref (formats);
}

GdkDrag *
_gdk_wayland_surface_drag_begin (GdkSurface         *surface,
                                 GdkDevice          *device,
                                 GdkContentProvider *content,
                                 GdkDragAction       actions,
                                 double              dx,
                                 double              dy)
{
  GdkWaylandDrag *drag_wayland;
  GdkDrag *drag;
  GdkSeat *seat;
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_device_get_display (device);
  seat = gdk_device_get_seat (device);

  drag_wayland = g_object_new (GDK_TYPE_WAYLAND_DRAG,
                               "surface", surface,
                               "device", device,
                               "content", content,
                               "actions", actions,
                               NULL);

  drag = GDK_DRAG (drag_wayland);

  drag_wayland->dnd_surface = g_object_new (GDK_TYPE_WAYLAND_DRAG_SURFACE,
                                            "display", display,
                                            NULL);

  gdk_wayland_drag_create_data_source (drag);

  if (wl_data_device_manager_get_version (GDK_WAYLAND_DISPLAY (display)->data_device_manager) >= WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION)
    wl_data_source_set_actions (drag_wayland->data_source, gdk_to_wl_actions (actions));

  gdk_wayland_seat_set_drag (seat, drag);

  wl_data_device_start_drag (gdk_wayland_device_get_data_device (device),
                             drag_wayland->data_source,
                             gdk_wayland_surface_get_wl_surface (surface),
                             gdk_wayland_surface_get_wl_surface (drag_wayland->dnd_surface),
                             _gdk_wayland_seat_get_implicit_grab_serial (seat, device, NULL));

  cursor = gdk_drag_get_cursor (drag, gdk_drag_get_selected_action (drag));
  gdk_drag_set_cursor (drag, cursor);

  gdk_seat_ungrab (seat);

  return drag;
}
