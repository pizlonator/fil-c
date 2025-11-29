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

#include "gdkprimary-wayland.h"

#include "gdkclipboardprivate.h"
#include "gdkcontentformats.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-wayland.h"
#include "gdkprivate.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

typedef struct _GdkWaylandPrimaryClass GdkWaylandPrimaryClass;

struct _GdkWaylandPrimary
{
  GdkClipboard parent;

  struct zwp_primary_selection_device_v1 *primary_data_device;

  struct zwp_primary_selection_offer_v1 *pending;
  GdkContentFormatsBuilder *pending_builder;

  struct zwp_primary_selection_offer_v1 *offer;
  GdkContentFormats *offer_formats;

  struct zwp_primary_selection_source_v1 *source;
};

struct _GdkWaylandPrimaryClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandPrimary, gdk_wayland_primary, GDK_TYPE_CLIPBOARD)

static void
gdk_wayland_primary_discard_pending (GdkWaylandPrimary *cb)
{
  if (cb->pending_builder)
    {
      GdkContentFormats *ignore = gdk_content_formats_builder_free_to_formats (cb->pending_builder);
      gdk_content_formats_unref (ignore);
      cb->pending_builder = NULL;
    }
  g_clear_pointer (&cb->pending, zwp_primary_selection_offer_v1_destroy);
}

static void
gdk_wayland_primary_discard_offer (GdkWaylandPrimary *cb)
{
  g_clear_pointer (&cb->offer_formats, gdk_content_formats_unref);
  g_clear_pointer (&cb->offer, zwp_primary_selection_offer_v1_destroy);
}

static void
gdk_wayland_primary_discard_source (GdkWaylandPrimary *cb)
{
  g_clear_pointer (&cb->source, zwp_primary_selection_source_v1_destroy);
}

static void
gdk_wayland_primary_finalize (GObject *object)
{
  GdkWaylandPrimary *cb = GDK_WAYLAND_PRIMARY (object);

  gdk_wayland_primary_discard_pending (cb);
  gdk_wayland_primary_discard_offer (cb);
  gdk_wayland_primary_discard_source (cb);
  
  G_OBJECT_CLASS (gdk_wayland_primary_parent_class)->finalize (object);
}

static void
gdk_wayland_primary_claim_remote (GdkWaylandPrimary                     *cb,
                                  struct zwp_primary_selection_offer_v1 *offer,
                                  GdkContentFormats                     *formats)
{
  g_return_if_fail (GDK_IS_WAYLAND_PRIMARY (cb));

  if (cb->source)
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD, "%p: Ignoring primary offer for self", cb);
      gdk_content_formats_unref (formats);
      g_clear_pointer (&offer, zwp_primary_selection_offer_v1_destroy);
      return;
    }

  gdk_wayland_primary_discard_offer (cb);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%p: remote primary claim for %s", cb, s);
      g_free (s);
    }

  cb->offer_formats = formats;
  cb->offer = offer;

  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), cb->offer_formats);
}

static void
primary_offer_offer (void                                  *data,
                     struct zwp_primary_selection_offer_v1 *offer,
                     const char                            *type)
{
  GdkWaylandPrimary *cb = data;

  if (cb->pending != offer)
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), SELECTION,
                         "%p: offer for unknown selection %p of %s",
                         cb, offer, type);
      return;
    }

  gdk_content_formats_builder_add_mime_type (cb->pending_builder, type);
}

static const struct zwp_primary_selection_offer_v1_listener primary_offer_listener = {
  primary_offer_offer,
};

static void
primary_selection_data_offer (void                                   *data,
                              struct zwp_primary_selection_device_v1 *device,
                              struct zwp_primary_selection_offer_v1  *offer)
{
  GdkWaylandPrimary *cb = data;

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), SELECTION,
                     "%p: new primary offer %p",
                     cb, offer);

  gdk_wayland_primary_discard_pending (cb);

  cb->pending = offer;
  zwp_primary_selection_offer_v1_add_listener (offer,
                                               &primary_offer_listener,
                                               cb);

  cb->pending_builder = gdk_content_formats_builder_new ();
}

static void
primary_selection_selection (void                                   *data,
                             struct zwp_primary_selection_device_v1 *device,
                             struct zwp_primary_selection_offer_v1  *offer)
{
  GdkWaylandPrimary *cb = data;
  GdkContentFormats *formats;

  if (offer == NULL)
    {
      gdk_wayland_primary_claim_remote (cb, NULL, gdk_content_formats_new (NULL, 0));
      return;
    }

  if (cb->pending != offer)
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), SELECTION,
                         "%p: ignoring unknown data offer %p",
                         cb, offer);
      return;
    }

  formats = gdk_content_formats_builder_free_to_formats (cb->pending_builder);
  cb->pending_builder = NULL;
  cb->pending = NULL;

  gdk_wayland_primary_claim_remote (cb, offer, formats);
}

static const struct zwp_primary_selection_device_v1_listener primary_selection_device_listener = {
  primary_selection_data_offer,
  primary_selection_selection,
};

static void
gdk_wayland_primary_write_done (GObject      *clipboard,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GError *error = NULL;

  if (!gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, &error))
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (clipboard)), SELECTION,
                         "%p: failed to write stream: %s",
                         clipboard, error->message);
      g_error_free (error);
    }
}

static void
gdk_wayland_primary_data_source_send (void                                   *data,
                                      struct zwp_primary_selection_source_v1 *source,
                                      const char                             *mime_type,
                                      int32_t                                 fd)
{
  GdkWaylandPrimary *cb = GDK_WAYLAND_PRIMARY (data);
  GOutputStream *stream;

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), SELECTION,
                     "%p: data source send request for %s on fd %d",
                     source, mime_type, fd);

  mime_type = gdk_intern_mime_type (mime_type);
  stream = g_unix_output_stream_new (fd, TRUE);

  gdk_clipboard_write_async (GDK_CLIPBOARD (cb),
                             mime_type,
                             stream,
                             G_PRIORITY_DEFAULT,
                             NULL,
                             gdk_wayland_primary_write_done,
                             cb);
  g_object_unref (stream);
}

static void
gdk_wayland_primary_data_source_cancelled (void                                   *data,
                                           struct zwp_primary_selection_source_v1 *source)
{
  GdkWaylandPrimary *cb = GDK_WAYLAND_PRIMARY (data);

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (data)), CLIPBOARD,
                     "%p: data source cancelled", data);

  if (cb->source == source)
    {
      gdk_wayland_primary_discard_source (cb);
      gdk_wayland_primary_claim_remote (cb, NULL, gdk_content_formats_new (NULL, 0));
    }
}

static const struct zwp_primary_selection_source_v1_listener primary_source_listener = {
  gdk_wayland_primary_data_source_send,
  gdk_wayland_primary_data_source_cancelled,
};

static gboolean
gdk_wayland_primary_claim (GdkClipboard       *clipboard,
                           GdkContentFormats  *formats,
                           gboolean            local,
                           GdkContentProvider *content)
{
  GdkWaylandPrimary *cb = GDK_WAYLAND_PRIMARY (clipboard);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_clipboard_get_display (clipboard), CLIPBOARD))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%p: claim primary (%s) for %s", cb, local ? "local" : "remote", s);
      g_free (s);
    }
  if (local)
    {
      GdkWaylandDisplay *wdisplay = GDK_WAYLAND_DISPLAY (gdk_clipboard_get_display (clipboard));
      const char * const *mime_types;
      gsize i, n_mime_types;
      GdkSeat *seat;
      guint32 serial;

      gdk_wayland_primary_discard_offer (cb);
      gdk_wayland_primary_discard_source (cb);

      cb->source = zwp_primary_selection_device_manager_v1_create_source (wdisplay->primary_selection_manager);
      zwp_primary_selection_source_v1_add_listener (cb->source, &primary_source_listener, cb);

      mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);
      for (i = 0; i < n_mime_types; i++)
        {
          zwp_primary_selection_source_v1_offer (cb->source, mime_types[i]);
        }

      seat = gdk_display_get_default_seat (GDK_DISPLAY (wdisplay));
      serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (seat),
                                                                NULL);
      zwp_primary_selection_device_v1_set_selection (cb->primary_data_device,
                                                     cb->source,
                                                     serial);
    }

  return GDK_CLIPBOARD_CLASS (gdk_wayland_primary_parent_class)->claim (clipboard, formats, local, content);
}

static void
gdk_wayland_primary_read_async (GdkClipboard        *clipboard,
                                GdkContentFormats   *formats,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GdkWaylandPrimary *cb = GDK_WAYLAND_PRIMARY (clipboard);
  GInputStream *stream;
  const char *mime_type;
  int pipe_fd[2];
  GError *error = NULL;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_wayland_primary_read_async);

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_clipboard_get_display (clipboard), CLIPBOARD))
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

  zwp_primary_selection_offer_v1_receive (cb->offer, mime_type, pipe_fd[1]);
  stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
  close (pipe_fd[1]);
  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

static GInputStream *
gdk_wayland_primary_read_finish (GdkClipboard  *clipboard,
                                 GAsyncResult  *result,
                                 const char   **out_mime_type,
                                 GError       **error)
{
  GInputStream *stream;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_wayland_primary_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream)
    {
      if (out_mime_type)
        *out_mime_type = g_task_get_task_data (task);
      g_object_ref (stream);
    }
  else
    {
      if (out_mime_type)
        *out_mime_type = NULL;
    }

  return stream;
}

static void
gdk_wayland_primary_class_init (GdkWaylandPrimaryClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_wayland_primary_finalize;

  clipboard_class->claim = gdk_wayland_primary_claim;
  clipboard_class->read_async = gdk_wayland_primary_read_async;
  clipboard_class->read_finish = gdk_wayland_primary_read_finish;
}

static void
gdk_wayland_primary_init (GdkWaylandPrimary *cb)
{
}

GdkClipboard *
gdk_wayland_primary_new (GdkWaylandSeat *seat)
{
  GdkWaylandDisplay *wdisplay;
  GdkWaylandPrimary *cb;

  wdisplay = GDK_WAYLAND_DISPLAY (gdk_seat_get_display (GDK_SEAT (seat)));

  cb = g_object_new (GDK_TYPE_WAYLAND_PRIMARY,
                     "display", wdisplay,
                     NULL);

  cb->primary_data_device =
        zwp_primary_selection_device_manager_v1_get_device (wdisplay->primary_selection_manager,
                                                            gdk_wayland_seat_get_wl_seat (GDK_SEAT (seat)));
  zwp_primary_selection_device_v1_add_listener (cb->primary_data_device,
                                                &primary_selection_device_listener, cb);

  return GDK_CLIPBOARD (cb);
}
