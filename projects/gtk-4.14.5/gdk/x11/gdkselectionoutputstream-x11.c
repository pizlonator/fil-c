/* GIO - GLib Output, Output and Streaming Library
 * 
 * Copyright (C) 2017 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 *         Christian Kellner <gicmo@gnome.org> 
 */

#include "config.h"

#include "gdkselectionoutputstream-x11.h"

#include "gdkclipboard-x11.h"
#include "gdkdisplay-x11.h"
#include <glib/gi18n-lib.h>
#include "gdktextlistconverter-x11.h"
#include "gdkx11display.h"
#include "gdkx11property.h"
#include "gdkx11surface.h"

typedef struct _GdkX11PendingSelectionNotify GdkX11PendingSelectionNotify;
typedef struct _GdkX11SelectionOutputStreamPrivate  GdkX11SelectionOutputStreamPrivate;

struct _GdkX11SelectionOutputStreamPrivate {
  GdkDisplay *display;
  GdkX11PendingSelectionNotify *notify;
  Window xwindow;
  char *selection;
  Atom xselection;
  char *target;
  Atom xtarget;
  char *property;
  Atom xproperty;
  char *type;
  Atom xtype;
  int format;
  gulong timestamp;

  GMutex mutex;
  GCond cond;
  GByteArray *data;
  guint flush_requested : 1;

  GTask *pending_task;

  guint incr : 1;
  guint sent_end_of_stream : 1;
  guint delete_pending : 1; /* owns a reference */
};

struct _GdkX11PendingSelectionNotify
{
  gsize n_pending;
  
  XSelectionEvent xevent;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkX11SelectionOutputStream, gdk_x11_selection_output_stream, G_TYPE_OUTPUT_STREAM);

static GdkX11PendingSelectionNotify *
gdk_x11_pending_selection_notify_new (Window window,
                                      Atom   selection,
                                      Atom   target,
                                      Atom   property,
                                      Time   timestamp)
{
  GdkX11PendingSelectionNotify *pending;

  pending = g_new0 (GdkX11PendingSelectionNotify, 1);
  pending->n_pending = 1;

  pending->xevent.type = SelectionNotify;
  pending->xevent.serial = 0;
  pending->xevent.send_event = True;
  pending->xevent.requestor = window;
  pending->xevent.selection = selection;
  pending->xevent.target = target;
  pending->xevent.property = property;
  pending->xevent.time = timestamp;

  return pending;
}

static void
gdk_x11_pending_selection_notify_free (GdkX11PendingSelectionNotify *notify)
{
  g_free (notify);
}

static void
gdk_x11_pending_selection_notify_require (GdkX11PendingSelectionNotify *notify,
                                          guint                         n_sends)
{
  notify->n_pending += n_sends;
}

static void
gdk_x11_pending_selection_notify_send (GdkX11PendingSelectionNotify *notify,
                                       GdkDisplay                   *display,
                                       gboolean                      success)
{
  Display *xdisplay;
  int error;

  notify->n_pending--;
  if (notify->n_pending)
    {
      GDK_DISPLAY_DEBUG (display, SELECTION,
                         "%s:%s: not sending SelectionNotify yet, %zu streams still pending",
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.selection),
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.target),
                         notify->n_pending);
      return;
    }

  GDK_DISPLAY_DEBUG (display, SELECTION,
                     "%s:%s: sending SelectionNotify reporting %s",
                     gdk_x11_get_xatom_name_for_display (display, notify->xevent.selection),
                     gdk_x11_get_xatom_name_for_display (display, notify->xevent.target),
                     success ? "success" : "failure");
  if (!success)
    notify->xevent.property = None;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  if (XSendEvent (xdisplay, notify->xevent.requestor, False, NoEventMask, (XEvent*) &notify->xevent) == 0)
    {
      GDK_DISPLAY_DEBUG (display, SELECTION,
                         "%s:%s: failed to XSendEvent()",
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.selection),
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.target));
    }
  XSync (xdisplay, False);

  error = gdk_x11_display_error_trap_pop (display);
  if (error != Success)
    {
      GDK_DISPLAY_DEBUG (display, SELECTION,
                         "%s:%s: X error during write: %d",
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.selection),
                         gdk_x11_get_xatom_name_for_display (display, notify->xevent.target),
                         error);
    }

  gdk_x11_pending_selection_notify_free (notify);
}

static gboolean
gdk_x11_selection_output_stream_xevent (GdkDisplay   *display,
                                        const XEvent *xevent,
                                        gpointer      data);

static gboolean
gdk_x11_selection_output_stream_can_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (priv->delete_pending)
    return FALSE;

  return TRUE;
}

static gboolean
gdk_x11_selection_output_stream_needs_flush_unlocked (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (priv->sent_end_of_stream)
    return FALSE;

  if (g_output_stream_is_closing (G_OUTPUT_STREAM (stream)) ||
      g_output_stream_is_closed (G_OUTPUT_STREAM (stream)))
    return TRUE;

  if (priv->data->len == 0 && priv->notify == NULL)
    return FALSE;

  if (priv->flush_requested)
    return TRUE;

  return priv->data->len >= gdk_x11_display_get_max_request_size (priv->display);
}

static gboolean
gdk_x11_selection_output_stream_needs_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  gboolean result;

  g_mutex_lock (&priv->mutex);

  result = gdk_x11_selection_output_stream_needs_flush_unlocked (stream);

  g_mutex_unlock (&priv->mutex);

  return result;
}

static gsize
get_element_size (int format)
{
  switch (format)
    {
    case 8:
      return 1;

    case 16:
      return sizeof (short);

    case 32:
      return sizeof (long);

    default:
      g_warning ("Unknown format %u", format);
      return 1;
    }
}

static void
gdk_x11_selection_output_stream_perform_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  Display *xdisplay;
  gsize element_size, n_elements;
  int error;

  g_assert (!priv->delete_pending);

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);

  /* We operate on a foreign window, better guard against catastrophe */
  gdk_x11_display_error_trap_push (priv->display);

  g_mutex_lock (&priv->mutex);

  element_size = get_element_size (priv->format);
  n_elements = priv->data->len / element_size;

  if (priv->notify && !g_output_stream_is_closing (G_OUTPUT_STREAM (stream)))
    {
      XWindowAttributes attrs;

      priv->incr = TRUE;
      GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                         "%s:%s: initiating INCR transfer",
                         priv->selection, priv->target);

      XGetWindowAttributes (xdisplay,
			    priv->xwindow,
			    &attrs);
      if (!(attrs.your_event_mask & PropertyChangeMask))
        {
          XSelectInput (xdisplay, priv->xwindow, attrs.your_event_mask | PropertyChangeMask);
        }

      XChangeProperty (GDK_DISPLAY_XDISPLAY (priv->display),
                       priv->xwindow,
                       priv->xproperty, 
                       gdk_x11_get_xatom_by_name_for_display (priv->display, "INCR"),
                       32,
                       PropModeReplace,
                       (guchar *) &(long) { n_elements },
                       1);
    }
  else
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (priv->display),
                       priv->xwindow,
                       priv->xproperty, 
                       priv->xtype,
                       priv->format,
                       PropModeReplace,
                       priv->data->data,
                       n_elements);
      GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                         "%s:%s: wrote %zu/%u bytes",
                         priv->selection, priv->target, n_elements * element_size, priv->data->len);
      g_byte_array_remove_range (priv->data, 0, n_elements * element_size);
      if (priv->data->len < element_size)
        priv->flush_requested = FALSE;
      if (!priv->incr || n_elements == 0)
        priv->sent_end_of_stream = TRUE;
    }

  if (priv->notify)
    {
      gdk_x11_pending_selection_notify_send (priv->notify, priv->display, TRUE);
      priv->notify = NULL;
    }

  g_object_ref (stream);
  priv->delete_pending = TRUE;
  g_cond_broadcast (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  /* XXX: handle failure here and report EPIPE for future operations on the stream? */
  error = gdk_x11_display_error_trap_pop (priv->display);
  if (error != Success)
    {
      GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                         "%s:%s: X error during write: %d",
                         priv->selection, priv->target, error);
    }

  if (priv->pending_task)
    {
      g_task_return_int (priv->pending_task, GPOINTER_TO_SIZE (g_task_get_task_data (priv->pending_task)));
      g_object_unref (priv->pending_task);
      priv->pending_task = NULL;
    }
}

static gboolean
gdk_x11_selection_output_stream_invoke_flush (gpointer data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (data);

  if (gdk_x11_selection_output_stream_needs_flush (stream) &&
      gdk_x11_selection_output_stream_can_flush (stream))
    gdk_x11_selection_output_stream_perform_flush (stream);

  return G_SOURCE_REMOVE;
}

static gssize
gdk_x11_selection_output_stream_write (GOutputStream  *output_stream,
                                       const void     *buffer,
                                       gsize           count,
                                       GCancellable   *cancellable,
                                       GError        **error)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  g_mutex_lock (&priv->mutex);
  g_byte_array_append (priv->data, buffer, count);
  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: wrote %zu bytes, %u total now",
                     priv->selection, priv->target, count, priv->data->len);
  g_mutex_unlock (&priv->mutex);

  g_main_context_invoke (NULL, gdk_x11_selection_output_stream_invoke_flush, stream);

  g_mutex_lock (&priv->mutex);
  if (gdk_x11_selection_output_stream_needs_flush_unlocked (stream))
    g_cond_wait (&priv->cond, &priv->mutex);
  g_mutex_unlock (&priv->mutex);

  return count;
}

static void
gdk_x11_selection_output_stream_write_async (GOutputStream        *output_stream,
                                             const void           *buffer,
                                             gsize                 count,
                                             int                   io_priority,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_output_stream_write_async);
  g_task_set_priority (task, io_priority);

  g_mutex_lock (&priv->mutex);
  g_byte_array_append (priv->data, buffer, count);
  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: async wrote %zu bytes, %u total now",
                     priv->selection, priv->target, count, priv->data->len);
  g_mutex_unlock (&priv->mutex);

  if (!gdk_x11_selection_output_stream_needs_flush (stream))
    {
      g_task_return_int (task, count);
      g_object_unref (task);
      return;
    }
  else if (!gdk_x11_selection_output_stream_can_flush (stream))
    {
      g_assert (priv->pending_task == NULL);
      priv->pending_task = task;
      g_task_set_task_data (task, GSIZE_TO_POINTER (count), NULL);
      return;
    }
  else
    {
      gdk_x11_selection_output_stream_perform_flush (stream);
      g_task_return_int (task, count);
      g_object_unref (task);
      return;
    }
}

static gssize
gdk_x11_selection_output_stream_write_finish (GOutputStream  *stream,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_x11_selection_output_stream_write_async, -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static gboolean
gdk_x11_selection_output_request_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  gboolean needs_flush;

  g_mutex_lock (&priv->mutex);

  if (priv->data->len >= get_element_size (priv->format))
    priv->flush_requested = TRUE;

  needs_flush = gdk_x11_selection_output_stream_needs_flush_unlocked (stream);
  g_mutex_unlock (&priv->mutex);

   GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                      "%s:%s: requested flush%s",
                      priv->selection, priv->target, needs_flush ?"" : ", but not needed");
  return needs_flush;
}

static gboolean
gdk_x11_selection_output_stream_flush (GOutputStream  *output_stream,
                                       GCancellable   *cancellable,
                                       GError        **error)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (!gdk_x11_selection_output_request_flush (stream))
    return TRUE;

  g_main_context_invoke (NULL, gdk_x11_selection_output_stream_invoke_flush, stream);

  g_mutex_lock (&priv->mutex);
  if (gdk_x11_selection_output_stream_needs_flush_unlocked (stream))
    g_cond_wait (&priv->cond, &priv->mutex);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
gdk_x11_selection_output_stream_flush_async (GOutputStream       *output_stream,
                                             int                  io_priority,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_output_stream_flush_async);
  g_task_set_priority (task, io_priority);

  if (!gdk_x11_selection_output_stream_can_flush (stream))
    {
      if (gdk_x11_selection_output_request_flush (stream))
        {
          g_assert (priv->pending_task == NULL);
          priv->pending_task = task;
          return;
        }
      else
        {
          g_task_return_boolean (task, TRUE);
          g_object_unref (task);
          return;
        }
    }

  gdk_x11_selection_output_stream_perform_flush (stream);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
  return;
}

static gboolean
gdk_x11_selection_output_stream_flush_finish (GOutputStream  *stream,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, gdk_x11_selection_output_stream_flush_async), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_x11_selection_output_stream_finalize (GObject *object)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (object);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  /* not sending a notify is terrible */
  g_assert (priv->notify == NULL);

  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: finalizing",
                     priv->selection, priv->target);
  GDK_X11_DISPLAY (priv->display)->streams = g_slist_remove (GDK_X11_DISPLAY (priv->display)->streams, stream);
  g_signal_handlers_disconnect_by_func (priv->display,
                                        gdk_x11_selection_output_stream_xevent,
                                        stream);

  g_byte_array_unref (priv->data);
  g_cond_clear (&priv->cond);
  g_mutex_clear (&priv->mutex);

  g_free (priv->selection);
  g_free (priv->target);
  g_free (priv->property);
  g_free (priv->type);

  G_OBJECT_CLASS (gdk_x11_selection_output_stream_parent_class)->finalize (object);
}

static void
gdk_x11_selection_output_stream_class_init (GdkX11SelectionOutputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *output_stream_class = G_OUTPUT_STREAM_CLASS (klass);

  object_class->finalize = gdk_x11_selection_output_stream_finalize;
  
  output_stream_class->write_fn = gdk_x11_selection_output_stream_write;
  output_stream_class->flush = gdk_x11_selection_output_stream_flush;

  output_stream_class->write_async = gdk_x11_selection_output_stream_write_async;
  output_stream_class->write_finish = gdk_x11_selection_output_stream_write_finish;
  output_stream_class->flush_async = gdk_x11_selection_output_stream_flush_async;
  output_stream_class->flush_finish = gdk_x11_selection_output_stream_flush_finish;
}

static void
gdk_x11_selection_output_stream_init (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);
  priv->data = g_byte_array_new ();
}

static gboolean
gdk_x11_selection_output_stream_xevent (GdkDisplay   *display,
                                        const XEvent *xevent,
                                        gpointer      data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (data);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  Display *xdisplay;

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);

  if (xevent->xany.display != xdisplay ||
      xevent->xany.window != priv->xwindow)
    return FALSE;

  switch (xevent->type)
  {
    case PropertyNotify:
      if (!priv->incr ||
          xevent->xproperty.atom != priv->xproperty ||
          xevent->xproperty.state != PropertyDelete)
        return FALSE;

      GDK_DISPLAY_DEBUG (display, SELECTION,
                         "%s:%s: got PropertyNotify Delete during INCR",
                         priv->selection, priv->target);
      priv->delete_pending = FALSE;
      if (gdk_x11_selection_output_stream_needs_flush (stream) &&
          gdk_x11_selection_output_stream_can_flush (stream))
        gdk_x11_selection_output_stream_perform_flush (stream);
      g_object_unref (stream); /* from unsetting the delete_pending */
      return FALSE;

    default:
      return FALSE;
    }
}

static GOutputStream *
gdk_x11_selection_output_stream_new (GdkDisplay                   *display,
                                     GdkX11PendingSelectionNotify *notify,
                                     Window                        window,
                                     const char                   *selection,
                                     const char                   *target,
                                     const char                   *property,
                                     const char                   *type,
                                     int                           format,
                                     gulong                        timestamp)
{
  GdkX11SelectionOutputStream *stream;
  GdkX11SelectionOutputStreamPrivate *priv;

  stream = g_object_new (GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, NULL);
  priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  priv->display = display;
  GDK_X11_DISPLAY (display)->streams = g_slist_prepend (GDK_X11_DISPLAY (display)->streams, stream);
  priv->notify = notify;
  priv->xwindow = window;
  priv->selection = g_strdup (selection);
  priv->xselection = gdk_x11_get_xatom_by_name_for_display (display, priv->selection);
  priv->target = g_strdup (target);
  priv->xtarget = gdk_x11_get_xatom_by_name_for_display (display, priv->target);
  priv->property = g_strdup (property);
  priv->xproperty = gdk_x11_get_xatom_by_name_for_display (display, priv->property);
  priv->type = g_strdup (type);
  priv->xtype = gdk_x11_get_xatom_by_name_for_display (display, priv->type);
  priv->format = format;
  priv->timestamp = timestamp;

  g_signal_connect (display,
                    "xevent",
                    G_CALLBACK (gdk_x11_selection_output_stream_xevent),
                    stream);
  
  return G_OUTPUT_STREAM (stream);
}

static void
print_atoms (GdkDisplay *display,
             const char *selection,
             const char *prefix,
             const Atom *atoms,
             gsize       n_atoms)
{
  if (GDK_DISPLAY_DEBUG_CHECK (display, CLIPBOARD))
    {
      GString *str;

      str = g_string_new ("");
      g_string_append_printf (str, "%s: %s [ ", selection, prefix);
      for (gsize i = 0; i < n_atoms; i++)
        g_string_append_printf (str, "%s%s", i > 0 ? ", " : "", gdk_x11_get_xatom_name_for_display (display , atoms[i]));
      g_string_append (str, " ]");
      gdk_debug_message ("%s", str->str);
      g_string_free (str, TRUE);
    }
}

static void
handle_targets_done (GObject      *stream,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GError *error = NULL;
  gsize bytes_written;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream), result, &bytes_written, &error))
    {
      GDK_DEBUG (CLIPBOARD, "---: failed to send targets after %zu bytes: %s",
                            bytes_written, error->message);
      g_error_free (error);
    }

  g_free (user_data);
}

static void
handle_targets (GOutputStream                *stream,
                GdkDisplay                   *display,
                GdkContentFormats            *formats,
                const char                   *target,
                const char                   *encoding,
                int                           format,
                gulong                        timestamp,
                GdkX11SelectionOutputHandler  handler,
                gpointer                      user_data)
{
  Atom *atoms;
  gsize n_atoms;

  atoms = gdk_x11_clipboard_formats_to_atoms (display,
                                              TRUE,
                                              formats,
                                              &n_atoms);
  print_atoms (display, "---", "sending targets", atoms, n_atoms);
  g_output_stream_write_all_async (stream,
                                   atoms,
                                   n_atoms * sizeof (Atom),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   handle_targets_done,
                                   atoms);
  g_object_unref (stream);
}

static void
handle_timestamp_done (GObject      *stream,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GError *error = NULL;
  gsize bytes_written;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream), result, &bytes_written, &error))
    {
      GDK_DEBUG (CLIPBOARD, "---: failed to send timestamp after %zu bytes: %s",
                            bytes_written, error->message);
      g_error_free (error);
    }

  g_free (user_data);
}

static void
handle_timestamp (GOutputStream                *stream,
                  GdkDisplay                   *display,
                  GdkContentFormats            *formats,
                  const char                   *target,
                  const char                   *encoding,
                  int                           format,
                  gulong                        timestamp,
                  GdkX11SelectionOutputHandler  handler,
                  gpointer                      user_data)
{
  gulong *time_;

  time_ = g_new (gulong, 1);
  *time_ = timestamp;

  g_output_stream_write_all_async (stream,
                                   time_,
                                   sizeof (gulong),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   handle_timestamp_done,
                                   time_);
  g_object_unref (stream);
}

static void
handle_save_targets (GOutputStream                *stream,
                     GdkDisplay                   *display,
                     GdkContentFormats            *formats,
                     const char                   *target,
                     const char                   *encoding,
                     int                           format,
                     gulong                        timestamp,
                     GdkX11SelectionOutputHandler  handler,
                     gpointer                      user_data)
{
  /* Don't do anything */

  g_object_unref (stream);
}

static void
handle_text_list (GOutputStream                *stream,
                  GdkDisplay                   *display,
                  GdkContentFormats            *formats,
                  const char                   *target,
                  const char                   *encoding,
                  int                           format,
                  gulong                        timestamp,
                  GdkX11SelectionOutputHandler  handler,
                  gpointer                      user_data)
{
  GOutputStream *converter_stream;
  GConverter *converter;

  converter = gdk_x11_text_list_converter_to_utf8_new (display,
                                                       encoding,
                                                       format);
  converter_stream = g_converter_output_stream_new (stream, converter);

  g_object_unref (converter);
  g_object_unref (stream);

  handler (converter_stream, gdk_intern_mime_type ("text/plain;charset=utf-8"), user_data);
}

static void
handle_utf8 (GOutputStream                *stream,
             GdkDisplay                   *display,
             GdkContentFormats            *formats,
             const char                   *target,
             const char                   *encoding,
             int                           format,
             gulong                        timestamp,
             GdkX11SelectionOutputHandler  handler,
             gpointer                      user_data)
{
  handler (stream, gdk_intern_mime_type ("text/plain;charset=utf-8"), user_data);
}

typedef void (* MimeTypeHandleFunc) (GOutputStream *, GdkDisplay *, GdkContentFormats *, const char *, const char *, int, gulong, GdkX11SelectionOutputHandler, gpointer);

static const struct {
  const char *x_target;
  const char *mime_type;
  const char *type;
  int format;
  MimeTypeHandleFunc handler;
} special_targets[] = {
  { "UTF8_STRING",   "text/plain;charset=utf-8", "UTF8_STRING",   8,  handle_utf8 },
  { "COMPOUND_TEXT", "text/plain;charset=utf-8", "COMPOUND_TEXT", 8,  handle_text_list },
  { "TEXT",          "text/plain;charset=utf-8", "STRING",        8,  handle_text_list },
  { "STRING",        "text/plain;charset=utf-8", "STRING",        8,  handle_text_list },
  { "TARGETS",       NULL,                       "ATOM",          32, handle_targets },
  { "TIMESTAMP",     NULL,                       "INTEGER",       32, handle_timestamp },
  { "SAVE_TARGETS",  NULL,                       "NULL",          32, handle_save_targets }
};

static gboolean
gdk_x11_selection_output_streams_request (GdkDisplay                   *display,
                                          GdkX11PendingSelectionNotify *notify,
                                          GdkContentFormats            *formats,
                                          Window                        requestor,
                                          Atom                          xselection,
                                          Atom                          xtarget,
                                          Atom                          xproperty,
                                          gulong                        timestamp,
                                          GdkX11SelectionOutputHandler  handler,
                                          gpointer                      user_data)
{
  const char *mime_type, *selection, *target, *property;

  selection = gdk_x11_get_xatom_name_for_display (display, xselection);
  target = gdk_x11_get_xatom_name_for_display (display, xtarget);
  property = gdk_x11_get_xatom_name_for_display (display, xproperty);
  mime_type = gdk_intern_mime_type (target);

  if (mime_type)
    {
      if (gdk_content_formats_contain_mime_type (formats, mime_type))
        {
          GOutputStream *stream;

          stream = gdk_x11_selection_output_stream_new (display,
                                                        notify,
                                                        requestor,
                                                        selection,
                                                        target,
                                                        property,
                                                        target,
                                                        8,
                                                        timestamp);
          handler (stream, mime_type, user_data);
          return TRUE;
        }
    }
  else if (g_str_equal (target, "MULTIPLE"))
    {
      gulong n_atoms;
      gulong nbytes;
      Atom prop_type;
      int prop_format;
      Atom *atoms = NULL;
      int error;

      error = XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
                                  requestor,
                                  xproperty,
                                  0, 0x1FFFFFFF, False,
                                  AnyPropertyType,
                                  &prop_type, &prop_format,
                                  &n_atoms, &nbytes, (guchar **) &atoms);
      if (error != Success)
        {
          GDK_DISPLAY_DEBUG (display, SELECTION,
                             "%s: XGetProperty() during MULTIPLE failed with %d",
                             selection, error);
        }
      else if (prop_format != 32 ||
               prop_type != gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
        {
          GDK_DISPLAY_DEBUG (display, SELECTION,
                             "%s: XGetProperty() type/format should be ATOM_PAIR/32 but is %s/%d",
                             selection, gdk_x11_get_xatom_name_for_display (display, prop_type), prop_format);
        }
      else if (n_atoms < 2)
        {
          print_atoms (display, selection, "ignoring MULTIPLE request with too little elements", atoms, n_atoms);
        }
      else
        {
          gulong i;

          print_atoms (display, selection, "MULTIPLE request", atoms, n_atoms);
          if (n_atoms % 2)
            {
              GDK_DISPLAY_DEBUG (display, SELECTION,
                                 "%s: Number of atoms is uneven at %lu, ignoring last element",
                                 selection, n_atoms);
              n_atoms &= ~1;
            }

          gdk_x11_pending_selection_notify_require (notify, n_atoms / 2);

          for (i = 0; i < n_atoms / 2; i++)
            {
              gboolean success;

              if (atoms[2 * i] == None || atoms[2 * i + 1] == None)
                {
                  success = FALSE;
                  GDK_DISPLAY_DEBUG (display, SELECTION,
                                     "%s: None not allowed as atom in MULTIPLE request",
                                     selection);
                  gdk_x11_pending_selection_notify_send (notify, display, FALSE);
                }
              else if (atoms[2 * i] == gdk_x11_get_xatom_by_name_for_display (display, "MULTIPLE"))
                {
                  success = FALSE;
                  GDK_DISPLAY_DEBUG (display, SELECTION,
                                     "%s: MULTIPLE as target in MULTIPLE request would cause recursion",
                                     selection);
                  gdk_x11_pending_selection_notify_send (notify, display, FALSE);
                }
              else
                {
                  success = gdk_x11_selection_output_streams_request (display,
                                                                      notify,
                                                                      formats,
                                                                      requestor,
                                                                      xselection,
                                                                      atoms[2 * i],
                                                                      atoms[2 * i + 1],
                                                                      timestamp,
                                                                      handler,
                                                                      user_data);
                }

              if (!success)
                atoms[2 * i + 1] = None;
            }
        }

      XChangeProperty (gdk_x11_display_get_xdisplay (display),
                       requestor,
                       xproperty,
                       prop_type, 32,
                       PropModeReplace, (guchar *)atoms, n_atoms);

      if (atoms)
        XFree (atoms);

      gdk_x11_pending_selection_notify_send (notify, display, TRUE);
      return TRUE;
    }
  else
    {
      gsize i;

      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (g_str_equal (target, special_targets[i].x_target) &&
              special_targets[i].handler)
            {
              GOutputStream *stream;

              if (special_targets[i].mime_type)
                gdk_intern_mime_type (special_targets[i].mime_type);
              stream = gdk_x11_selection_output_stream_new (display,
                                                            notify,
                                                            requestor,
                                                            selection,
                                                            target,
                                                            property,
                                                            special_targets[i].type,
                                                            special_targets[i].format,
                                                            timestamp);
              special_targets[i].handler (stream,
                                          display,
                                          formats,
                                          target,
                                          special_targets[i].type,
                                          special_targets[i].format,
                                          timestamp,
                                          handler,
                                          user_data);
              return TRUE;
            }
        }
    }

  gdk_x11_pending_selection_notify_send (notify, display, FALSE);
  return FALSE;
}

void
gdk_x11_selection_output_streams_create (GdkDisplay                   *display,
                                         GdkContentFormats            *formats,
                                         Window                        requestor,
                                         Atom                          selection,
                                         Atom                          target,
                                         Atom                          property,
                                         gulong                        timestamp,
                                         GdkX11SelectionOutputHandler  handler,
                                         gpointer                      user_data)
{
  GdkX11PendingSelectionNotify *notify;

  notify = gdk_x11_pending_selection_notify_new (requestor,
                                                 selection,
                                                 target,
                                                 property,
                                                 timestamp);
  gdk_x11_selection_output_streams_request (display,
                                            notify,
                                            formats,
                                            requestor,
                                            selection,
                                            target,
                                            property,
                                            timestamp,
                                            handler,
                                            user_data);
}
