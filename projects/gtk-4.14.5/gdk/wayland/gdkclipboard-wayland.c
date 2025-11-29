/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "config.h"

#include "gdkclipboardprivate.h"
#include "gdkclipboard-wayland.h"

#include "gdkcontentformats.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-wayland.h"
#include "gdkprivate.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

typedef struct _GdkWaylandClipboardClass GdkWaylandClipboardClass;

struct _GdkWaylandClipboard
{
  GdkClipboard parent;

  struct wl_data_offer *offer;
  GdkContentFormats *offer_formats;

  struct wl_data_source *source;
};

struct _GdkWaylandClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandClipboard, gdk_wayland_clipboard, GDK_TYPE_CLIPBOARD)

static void
gdk_wayland_clipboard_discard_offer (GdkWaylandClipboard *cb)
{
  g_clear_pointer (&cb->offer_formats, gdk_content_formats_unref);
  g_clear_pointer (&cb->offer, wl_data_offer_destroy);
}

static void
gdk_wayland_clipboard_discard_source (GdkWaylandClipboard *cb)
{
  g_clear_pointer (&cb->source, wl_data_source_destroy);
}

static void
gdk_wayland_clipboard_finalize (GObject *object)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (object);

  gdk_wayland_clipboard_discard_offer (cb);
  gdk_wayland_clipboard_discard_source (cb);
  
  G_OBJECT_CLASS (gdk_wayland_clipboard_parent_class)->finalize (object);
}

static void
gdk_wayland_clipboard_data_source_target (void                  *data,
                                          struct wl_data_source *source,
                                          const char            *mime_type)
{
  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: Huh? data_source.target() events?", data);
}

static void
gdk_wayland_clipboard_write_done (GObject      *clipboard,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GError *error = NULL;

  if (!gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, &error))
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (clipboard)), CLIPBOARD,
                         "%p: failed to write stream: %s", clipboard, error->message);
      g_error_free (error);
    }
}

static void
gdk_wayland_clipboard_data_source_send (void                  *data,
                                        struct wl_data_source *source,
                                        const char            *mime_type,
                                        int32_t                fd)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (data);
  GOutputStream *stream;

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: data source send request for %s on fd %d",
                     source, mime_type, fd);

  mime_type = gdk_intern_mime_type (mime_type);
  stream = g_unix_output_stream_new (fd, TRUE);

  gdk_clipboard_write_async (GDK_CLIPBOARD (cb),
                             mime_type,
                             stream,
                             G_PRIORITY_DEFAULT,
                             NULL,
                             gdk_wayland_clipboard_write_done,
                             cb);
  g_object_unref (stream);
}

static void
gdk_wayland_clipboard_data_source_cancelled (void                  *data,
                                             struct wl_data_source *source)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (data);

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: data source cancelled", data);

  if (cb->source == source)
    {
      gdk_wayland_clipboard_discard_source (cb);
      gdk_wayland_clipboard_claim_remote (cb, NULL, gdk_content_formats_new (NULL, 0));
    }
}

static void
gdk_wayland_clipboard_data_source_dnd_drop_performed (void                  *data,
                                                      struct wl_data_source *source)
{
  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: Huh? data_source.dnd_drop_performed() events?", data);
}

static void
gdk_wayland_clipboard_data_source_dnd_finished (void                  *data,
                                               struct wl_data_source *source)
{
  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: Huh? data_source.dnd_finished() events?", data);
}

static void
gdk_wayland_clipboard_data_source_action (void                  *data,
                                          struct wl_data_source *source,
                                          uint32_t               action)
{
  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: Huh? data_source.action() events?", data);
}

static const struct wl_data_source_listener data_source_listener = {
  gdk_wayland_clipboard_data_source_target,
  gdk_wayland_clipboard_data_source_send,
  gdk_wayland_clipboard_data_source_cancelled,
  gdk_wayland_clipboard_data_source_dnd_drop_performed,
  gdk_wayland_clipboard_data_source_dnd_finished,
  gdk_wayland_clipboard_data_source_action,
};

static gboolean
gdk_wayland_clipboard_claim (GdkClipboard       *clipboard,
                             GdkContentFormats  *formats,
                             gboolean            local,
                             GdkContentProvider *content)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (clipboard);

  if (local)
    {
      GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (gdk_clipboard_get_display (clipboard));
      GdkDevice *device;
      const char * const *mime_types;
      gsize i, n_mime_types;

      gdk_wayland_clipboard_discard_offer (cb);
      gdk_wayland_clipboard_discard_source (cb);

      cb->source = wl_data_device_manager_create_data_source (wayland_display->data_device_manager);
      wl_data_source_add_listener (cb->source, &data_source_listener, cb);

      mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);
      for (i = 0; i < n_mime_types; i++)
        {
          wl_data_source_offer (cb->source, mime_types[i]);
        }

      device = gdk_seat_get_pointer (gdk_display_get_default_seat (GDK_DISPLAY (wayland_display)));
      gdk_wayland_device_set_selection (device, cb->source);
    }

  return GDK_CLIPBOARD_CLASS (gdk_wayland_clipboard_parent_class)->claim (clipboard, formats, local, content);
}

static void
gdk_wayland_clipboard_read_async (GdkClipboard        *clipboard,
                                  GdkContentFormats   *formats,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (clipboard);
  GInputStream *stream;
  const char *mime_type;
  int pipe_fd[2];
  GError *error = NULL;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_wayland_clipboard_read_async);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%p: read for %s", cb, s);
      g_free (s);
    }
  mime_type = gdk_content_formats_match_mime_type (formats, cb->offer_formats);
  if (mime_type == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      g_object_unref (task);
      return;
    }
  /* offer formats should be empty if we have no offer */
  g_assert (cb->offer);

  g_task_set_task_data (task, (gpointer) mime_type, NULL);

  if (!g_unix_open_pipe (pipe_fd, O_CLOEXEC, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  wl_data_offer_receive (cb->offer, mime_type, pipe_fd[1]);
  stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
  close (pipe_fd[1]);
  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

static GInputStream *
gdk_wayland_clipboard_read_finish (GdkClipboard  *clipboard,
                                   GAsyncResult  *result,
                                   const char   **out_mime_type,
                                   GError       **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_wayland_clipboard_read_async, NULL);

  if (out_mime_type)
    *out_mime_type = g_task_get_task_data (task);

  return g_task_propagate_pointer (task, error);
}

static void
gdk_wayland_clipboard_class_init (GdkWaylandClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_wayland_clipboard_finalize;

  clipboard_class->claim = gdk_wayland_clipboard_claim;
  clipboard_class->read_async = gdk_wayland_clipboard_read_async;
  clipboard_class->read_finish = gdk_wayland_clipboard_read_finish;
}

static void
gdk_wayland_clipboard_init (GdkWaylandClipboard *cb)
{
}

GdkClipboard *
gdk_wayland_clipboard_new (GdkDisplay *display)
{
  GdkWaylandClipboard *cb;

  cb = g_object_new (GDK_TYPE_WAYLAND_CLIPBOARD,
                     "display", display,
                     NULL);

  return GDK_CLIPBOARD (cb);
}

void
gdk_wayland_clipboard_claim_remote (GdkWaylandClipboard  *cb,
                                    struct wl_data_offer *offer,
                                    GdkContentFormats    *formats)
{
  g_return_if_fail (GDK_IS_WAYLAND_CLIPBOARD (cb));

  if (cb->source)
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD,
                         "%p: Ignoring clipboard offer for self", cb);
      gdk_content_formats_unref (formats);
      return;
    }

  gdk_wayland_clipboard_discard_offer (cb);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%p: remote clipboard claim for %s", cb, s);
      g_free (s);
    }
  cb->offer_formats = formats;
  cb->offer = offer;

  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb),
                              cb->offer_formats);
}

