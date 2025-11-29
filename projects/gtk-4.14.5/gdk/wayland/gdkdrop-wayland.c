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

#include "gdkdropprivate.h"

#include "gdkprivate-wayland.h"
#include "gdkcontentformats.h"
#include "gdkdisplay-wayland.h"
#include <glib/gi18n-lib.h>
#include "gdkseat-wayland.h"

#include "gdkdeviceprivate.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <string.h>

#define GDK_TYPE_WAYLAND_DROP              (gdk_wayland_drop_get_type ())
#define GDK_WAYLAND_DROP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DROP, GdkWaylandDrop))
#define GDK_WAYLAND_DROP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DROP, GdkWaylandDropClass))
#define GDK_IS_WAYLAND_DROP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DROP))
#define GDK_IS_WAYLAND_DROP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DROP))
#define GDK_WAYLAND_DROP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DROP, GdkWaylandDropClass))

typedef struct _GdkWaylandDrop GdkWaylandDrop;
typedef struct _GdkWaylandDropClass GdkWaylandDropClass;

struct _GdkWaylandDrop
{
  GdkDrop drop;

  struct wl_data_offer *offer;
  uint32_t source_actions;
  uint32_t action;
  uint32_t serial;
};

struct _GdkWaylandDropClass
{
  GdkDropClass parent_class;
};

GType gdk_wayland_drop_get_type (void);

G_DEFINE_TYPE (GdkWaylandDrop, gdk_wayland_drop, GDK_TYPE_DROP)

static void
gdk_wayland_drop_finalize (GObject *object)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (object);

  g_clear_pointer (&wayland_drop->offer, wl_data_offer_destroy);

  G_OBJECT_CLASS (gdk_wayland_drop_parent_class)->finalize (object);
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
gdk_wayland_drop_drop_set_status (GdkWaylandDrop *drop_wayland,
                                  gboolean        accepted)
{
  if (accepted)
    {
      const char *const *mimetypes;
      gsize i, n_mimetypes;
      
      /* This is a local drag, treat it like that */
      if (gdk_drop_get_drag (GDK_DROP (drop_wayland)))
        {
          wl_data_offer_accept (drop_wayland->offer, drop_wayland->serial, GDK_WAYLAND_LOCAL_DND_MIME_TYPE);
          return;
        }

      mimetypes = gdk_content_formats_get_mime_types (gdk_drop_get_formats (GDK_DROP (drop_wayland)), &n_mimetypes);
      for (i = 0; i < n_mimetypes; i++)
        {
          if (mimetypes[i] != g_intern_static_string ("DELETE"))
            break;
        }

      if (i < n_mimetypes)
        {
          wl_data_offer_accept (drop_wayland->offer, drop_wayland->serial, mimetypes[i]);
          return;
        }
    }

  wl_data_offer_accept (drop_wayland->offer, drop_wayland->serial, NULL);
}

static void
gdk_wayland_drop_commit_status (GdkWaylandDrop *wayland_drop,
                                GdkDragAction   actions,
                                GdkDragAction   preferred)
{
  GdkDisplay *display;

  display = gdk_drop_get_display (GDK_DROP (wayland_drop));

  if (wl_data_device_manager_get_version (GDK_WAYLAND_DISPLAY (display)->data_device_manager) >=
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
    {
      uint32_t dnd_actions;
      uint32_t preferred_action;

      dnd_actions = gdk_to_wl_actions (actions);
      preferred_action = gdk_to_wl_actions (preferred);

      wl_data_offer_set_actions (wayland_drop->offer, dnd_actions, preferred_action);
    }

  gdk_wayland_drop_drop_set_status (wayland_drop, actions != 0);
}

static void
gdk_wayland_drop_status (GdkDrop       *drop,
                         GdkDragAction  actions,
                         GdkDragAction  preferred)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (drop);

  gdk_wayland_drop_commit_status (wayland_drop, actions, preferred);
}

static void
gdk_wayland_drop_finish (GdkDrop       *drop,
                         GdkDragAction  action)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (drop);
  GdkDisplay *display = gdk_drop_get_display (drop);

  if (action)
    {
      gdk_wayland_drop_commit_status (wayland_drop, action, action);

      if (wl_data_device_manager_get_version (GDK_WAYLAND_DISPLAY (display)->data_device_manager) >=
          WL_DATA_OFFER_FINISH_SINCE_VERSION)
        wl_data_offer_finish (wayland_drop->offer);
    }

  g_clear_pointer (&wayland_drop->offer, wl_data_offer_destroy);
}

static void
gdk_wayland_drop_read_async (GdkDrop             *drop,
                             GdkContentFormats   *formats,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (drop);
  GInputStream *stream;
  const char *mime_type;
  int pipe_fd[2];
  GError *error = NULL;
  GTask *task;

  task = g_task_new (drop, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_wayland_drop_read_async);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_drop_get_display (drop), DND))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%p: read for %s", drop, s);
      g_free (s);
    }
  mime_type = gdk_content_formats_match_mime_type (formats,
                                                   gdk_drop_get_formats (drop));
  if (mime_type == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      g_object_unref (task);
      return;
    }

  g_task_set_task_data (task, (gpointer) mime_type, NULL);

  if (!g_unix_open_pipe (pipe_fd, O_CLOEXEC, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  wl_data_offer_receive (wayland_drop->offer, mime_type, pipe_fd[1]);
  stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
  close (pipe_fd[1]);
  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

static GInputStream *
gdk_wayland_drop_read_finish (GdkDrop       *drop,
                              GAsyncResult  *result,
                              const char   **out_mime_type,
                              GError       **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (drop)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_wayland_drop_read_async, NULL);

  if (out_mime_type)
    *out_mime_type = g_task_get_task_data (task);

  return g_task_propagate_pointer (task, error);
}

static void
gdk_wayland_drop_class_init (GdkWaylandDropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);

  object_class->finalize = gdk_wayland_drop_finalize;

  drop_class->status = gdk_wayland_drop_status;
  drop_class->finish = gdk_wayland_drop_finish;
  drop_class->read_async = gdk_wayland_drop_read_async;
  drop_class->read_finish = gdk_wayland_drop_read_finish;
}

static void
gdk_wayland_drop_init (GdkWaylandDrop *drop)
{
}

GdkDrop *
gdk_wayland_drop_new (GdkDevice            *device,
                      GdkDrag              *drag,
                      GdkContentFormats    *formats,
                      GdkSurface           *surface,
                      struct wl_data_offer *offer,
                      uint32_t              serial)

{
  GdkWaylandDrop *drop;

  drop = g_object_new (GDK_TYPE_WAYLAND_DROP,
                       "device", device,
                       "drag", drag,
                       "formats", formats,
                       "surface", surface,
                       NULL);
  drop->offer = offer;
  drop->serial = serial;

  return GDK_DROP (drop);
}

static void
gdk_wayland_drop_update_actions (GdkWaylandDrop *drop)
{
  GdkDragAction gdk_actions = 0;
  uint32_t wl_actions;

  if (drop->action == 0 ||
      drop->action & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    wl_actions = drop->source_actions;
  else
    wl_actions = drop->action;

  if (wl_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    gdk_actions |= GDK_ACTION_COPY;
  if (wl_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    gdk_actions |= GDK_ACTION_MOVE;

  gdk_drop_set_actions (GDK_DROP (drop), gdk_actions);
}

void
gdk_wayland_drop_set_source_actions (GdkDrop *drop,
                                     uint32_t source_actions)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (drop);

  wayland_drop->source_actions = source_actions;

  gdk_wayland_drop_update_actions (wayland_drop);
}

void
gdk_wayland_drop_set_action (GdkDrop *drop,
                             uint32_t action)
{
  GdkWaylandDrop *wayland_drop = GDK_WAYLAND_DROP (drop);

  wayland_drop->action = action;

  gdk_wayland_drop_update_actions (wayland_drop);
}

