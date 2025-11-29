/* GIO - GLib Input, Output and Streaming Library
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

#include "gdkselectioninputstream-x11.h"

#include "gdkdisplay-x11.h"
#include <glib/gi18n-lib.h>
#include "gdkx11display.h"
#include "gdkx11property.h"
#include "gdkx11surface.h"

typedef struct GdkX11SelectionInputStreamPrivate  GdkX11SelectionInputStreamPrivate;

struct GdkX11SelectionInputStreamPrivate {
  GdkDisplay *display;
  GAsyncQueue *chunks;
  char *selection;
  Atom xselection;
  char *target;
  Atom xtarget;
  char *property;
  Atom xproperty;
  const char *type;
  Atom xtype;
  int format;

  GTask *pending_task;
  guchar *pending_data;
  gsize pending_size;

  guint complete : 1;
  guint incr : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkX11SelectionInputStream, gdk_x11_selection_input_stream, G_TYPE_INPUT_STREAM);

static gboolean
gdk_x11_selection_input_stream_xevent (GdkDisplay   *display,
                                       const XEvent *xevent,
                                       gpointer      data);

static gboolean
gdk_x11_selection_input_stream_has_data (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  return g_async_queue_length (priv->chunks) > 0 || priv->complete;
}

/* NB: blocks when no data is in buffer */
static gsize
gdk_x11_selection_input_stream_fill_buffer (GdkX11SelectionInputStream *stream,
                                            guchar                     *buffer,
                                            gsize                       count)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  GBytes *bytes;
  
  gsize result = 0;

  g_async_queue_lock (priv->chunks);

  for (bytes = g_async_queue_pop_unlocked (priv->chunks);
       bytes != NULL && count > 0;
       bytes = g_async_queue_try_pop_unlocked (priv->chunks))
  {
    gsize size = g_bytes_get_size (bytes);

    if (size == 0)
      {
        /* EOF marker, put it back */
        g_async_queue_push_front_unlocked (priv->chunks, g_steal_pointer (&bytes));
        break;
      }
    else if (size > count)
      {
        GBytes *subbytes;
        if (buffer)
          memcpy (buffer, g_bytes_get_data (bytes, NULL), count);
        subbytes = g_bytes_new_from_bytes (bytes, count, size - count);
        g_async_queue_push_front_unlocked (priv->chunks, subbytes);
        size = count;
      }
    else
      {
        if (buffer)
          memcpy (buffer, g_bytes_get_data (bytes, NULL), size);
      }

    g_bytes_unref (g_steal_pointer (&bytes));
    result += size;
    if (buffer)
      buffer += size;
    count -= size;
  }

  if (bytes)
    g_async_queue_push_front_unlocked (priv->chunks, bytes);

  g_async_queue_unlock (priv->chunks);

  return result;
}

static void
gdk_x11_selection_input_stream_flush (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  gssize written;

  if (!gdk_x11_selection_input_stream_has_data (stream))
    return;

  if (priv->pending_task == NULL)
    return;

  written = gdk_x11_selection_input_stream_fill_buffer (stream, priv->pending_data, priv->pending_size);
  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: finishing read of %zd/%zu bytes",
                     priv->selection, priv->target,
                     written, priv->pending_size);
  g_task_return_int (priv->pending_task, written);

  g_clear_object (&priv->pending_task);
  priv->pending_data = NULL;
  priv->pending_size = 0;
}

static void
gdk_x11_selection_input_stream_complete (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  if (priv->complete)
    return;

  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: transfer complete",
                     priv->selection, priv->target);
  priv->complete = TRUE;

  g_async_queue_push (priv->chunks, g_bytes_new (NULL, 0));
  gdk_x11_selection_input_stream_flush (stream);

  GDK_X11_DISPLAY (priv->display)->streams = g_slist_remove (GDK_X11_DISPLAY (priv->display)->streams, stream);
  g_signal_handlers_disconnect_by_func (priv->display,
                                        gdk_x11_selection_input_stream_xevent,
                                        g_steal_pointer (&stream));
}

static gssize
gdk_x11_selection_input_stream_read (GInputStream  *input_stream,
                                     void          *buffer,
                                     gsize          count,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (input_stream);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  gssize written;

  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: starting sync read of %zu bytes",
                     priv->selection, priv->target, count);
  written = gdk_x11_selection_input_stream_fill_buffer (stream, buffer, count);
  GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                     "%s:%s: finishing sync read of %zd/%zu bytes",
                     priv->selection, priv->target, written, count);
  return written;
}

static gboolean
gdk_x11_selection_input_stream_close (GInputStream  *stream,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  return TRUE;
}

static void
gdk_x11_selection_input_stream_read_async (GInputStream        *input_stream,
                                           void                *buffer,
                                           gsize                count,
                                           int                  io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (input_stream);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_input_stream_read_async);
  g_task_set_priority (task, io_priority);

  if (gdk_x11_selection_input_stream_has_data (stream))
    {
      gssize size;

      size = gdk_x11_selection_input_stream_fill_buffer (stream, buffer, count);
      GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                         "%s:%s: async read of %zd/%zu bytes",
                         priv->selection, priv->target, size, count);
      g_task_return_int (task, size);
      g_object_unref (task);
    }
  else
    {
      priv->pending_data = buffer;
      priv->pending_size = count;
      priv->pending_task = task;
      GDK_DISPLAY_DEBUG (priv->display, SELECTION,
                         "%s:%s: async read of %zu bytes pending",
                         priv->selection, priv->target, count);
    }
}

static gssize
gdk_x11_selection_input_stream_read_finish (GInputStream  *stream,
                                            GAsyncResult  *result,
                                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_x11_selection_input_stream_read_async, -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
gdk_x11_selection_input_stream_close_async (GInputStream        *stream,
                                            int                  io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_input_stream_close_async);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static gboolean
gdk_x11_selection_input_stream_close_finish (GInputStream  *stream,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  return TRUE;
}

static void
gdk_x11_selection_input_stream_finalize (GObject *object)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (object);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  gdk_x11_selection_input_stream_complete (stream);

  g_async_queue_unref (priv->chunks);

  g_free (priv->selection);
  g_free (priv->target);
  g_free (priv->property);

  G_OBJECT_CLASS (gdk_x11_selection_input_stream_parent_class)->finalize (object);
}

static void
gdk_x11_selection_input_stream_class_init (GdkX11SelectionInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

  object_class->finalize = gdk_x11_selection_input_stream_finalize;
  
  istream_class->read_fn = gdk_x11_selection_input_stream_read;
  istream_class->close_fn = gdk_x11_selection_input_stream_close;

  istream_class->read_async = gdk_x11_selection_input_stream_read_async;
  istream_class->read_finish = gdk_x11_selection_input_stream_read_finish;
  istream_class->close_async = gdk_x11_selection_input_stream_close_async;
  istream_class->close_finish = gdk_x11_selection_input_stream_close_finish;
}

static void
gdk_x11_selection_input_stream_init (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  priv->chunks = g_async_queue_new_full ((GDestroyNotify) g_bytes_unref);
}

static void
XFree_without_return_value (gpointer data)
{
  XFree (data);
}

static GBytes *
get_selection_property (Display  *display,
                        Window    owner,
                        Atom      property,
                        Atom     *ret_type,
                        int      *ret_format)
{
  gulong nitems;
  gulong nbytes;
  Atom prop_type;
  int prop_format;
  guchar *data = NULL;

  if (XGetWindowProperty (display, owner, property,
                          0, 0x1FFFFFFF, False,
                          AnyPropertyType, &prop_type, &prop_format,
                          &nitems, &nbytes, &data) != Success)
    goto err;

  if (prop_type != None)
    {
      gsize length;

      switch (prop_format)
        {
        case 8:
          length = nitems;
          break;
        case 16:
          length = sizeof (short) * nitems;
          break;
        case 32:
          length = sizeof (long) * nitems;
          break;
        default:
          g_warning ("Unknown XGetWindowProperty() format %u", prop_format);
          goto err;
        }

      *ret_type = prop_type;
      *ret_format = prop_format;

      return g_bytes_new_with_free_func (data,
                                         length,
                                         XFree_without_return_value,
                                         data);
    }

err:
  if (data)
    XFree (data);

  *ret_type = None;
  *ret_format = 0;

  return NULL;
}


static gboolean
gdk_x11_selection_input_stream_xevent (GdkDisplay   *display,
                                       const XEvent *xevent,
                                       gpointer      data)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (data);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  Display *xdisplay;
  Window xwindow;
  GBytes *bytes;
  Atom type;
  int format;

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);
  xwindow = GDK_X11_DISPLAY (priv->display)->leader_window;

  if (xevent->xany.display != xdisplay ||
      xevent->xany.window != xwindow)
    return FALSE;

  switch (xevent->type)
    {
      case PropertyNotify:
        if (!priv->incr ||
            xevent->xproperty.atom != priv->xproperty ||
            xevent->xproperty.state != PropertyNewValue)
          return FALSE;

      bytes = get_selection_property (xdisplay, xwindow, xevent->xproperty.atom, &type, &format);
      if (bytes == NULL)
        { 
          GDK_DISPLAY_DEBUG (display, SELECTION,
                             "%s:%s: got PropertyNotify erroring out of INCR",
                             priv->selection, priv->target);
          /* error, should we signal one? */
          g_clear_pointer (&stream, gdk_x11_selection_input_stream_complete);
        }
      else if (g_bytes_get_size (bytes) == 0 || type == None)
        {
          GDK_DISPLAY_DEBUG (display, SELECTION,
                             "%s:%s: got PropertyNotify ending INCR",
                             priv->selection, priv->target);
          g_bytes_unref (bytes);
          g_clear_pointer (&stream, gdk_x11_selection_input_stream_complete);
        }
      else
        {
          GDK_DISPLAY_DEBUG (display, SELECTION,
                             "%s:%s: got PropertyNotify during INCR with %zu bytes",
                             priv->selection, priv->target,
                             g_bytes_get_size (bytes));
          g_async_queue_push (priv->chunks, bytes);
          gdk_x11_selection_input_stream_flush (stream);
        }

      XDeleteProperty (xdisplay, xwindow, xevent->xproperty.atom);

      return FALSE;

    case SelectionNotify:
      {
        GTask *task;

        /* selection is not for us */
        if (priv->xselection != xevent->xselection.selection ||
            priv->xtarget != xevent->xselection.target)
          return FALSE;

        /* We already received a selectionNotify before */
        if (priv->pending_task == NULL || 
            g_task_get_source_tag (priv->pending_task) != gdk_x11_selection_input_stream_new_async)
          return FALSE;

        GDK_DISPLAY_DEBUG (display, SELECTION,
                           "%s:%s: got SelectionNotify",
                           priv->selection, priv->target);

        task = priv->pending_task;
        priv->pending_task = NULL;

        if (xevent->xselection.property == None)
          {
            g_task_return_new_error (task,
                                     G_IO_ERROR,
                                     G_IO_ERROR_NOT_FOUND,
                                     _("Format %s not supported"), priv->target);
            g_clear_pointer (&stream, gdk_x11_selection_input_stream_complete);
          }
        else
          {
            bytes = get_selection_property (xdisplay, xwindow, xevent->xselection.property, &priv->xtype, &priv->format);
            priv->type = gdk_x11_get_xatom_name_for_display (priv->display, priv->xtype);

            g_task_return_pointer (task, g_object_ref (stream), g_object_unref);

            if (bytes == NULL)
              {
                g_clear_pointer (&stream, gdk_x11_selection_input_stream_complete);
              }
            else
              {
                if (priv->xtype == gdk_x11_get_xatom_by_name_for_display (priv->display, "INCR"))
                  {
                    /* The remainder of the selection will come through PropertyNotify
                       events on xwindow */
                    GDK_DISPLAY_DEBUG (display, SELECTION,
                                       "%s:%s: initiating INCR transfer",
                                       priv->selection, priv->target);
                    priv->incr = TRUE;
                    gdk_x11_selection_input_stream_flush (stream);
                  }
                else
                  {
                    GDK_DISPLAY_DEBUG (display, SELECTION,
                                       "%s:%s: reading %zu bytes",
                                       priv->selection, priv->target,
                                       g_bytes_get_size (bytes));
                    g_async_queue_push (priv->chunks, bytes);

                    g_clear_pointer (&stream, gdk_x11_selection_input_stream_complete);
                  }
              }

            XDeleteProperty (xdisplay, xwindow, xevent->xselection.property);
          }

        g_object_unref (task);
      }
      return TRUE;

    default:
      return FALSE;
    }
}

void
gdk_x11_selection_input_stream_new_async (GdkDisplay          *display,
                                          const char          *selection,
                                          const char          *target,
                                          guint32              timestamp,
                                          int                  io_priority,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GdkX11SelectionInputStream *stream;
  GdkX11SelectionInputStreamPrivate *priv;

  stream = g_object_new (GDK_TYPE_X11_SELECTION_INPUT_STREAM, NULL);
  priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  priv->display = display;
  GDK_X11_DISPLAY (display)->streams = g_slist_prepend (GDK_X11_DISPLAY (display)->streams, stream);
  priv->selection = g_strdup (selection);
  priv->xselection = gdk_x11_get_xatom_by_name_for_display (display, priv->selection);
  priv->target = g_strdup (target);
  priv->xtarget = gdk_x11_get_xatom_by_name_for_display (display, priv->target);
  priv->property = g_strdup_printf ("GDK_SELECTION_%p", stream); 
  priv->xproperty = gdk_x11_get_xatom_by_name_for_display (display, priv->property);

  g_signal_connect_data (display, "xevent",
                         G_CALLBACK (gdk_x11_selection_input_stream_xevent),
                         g_steal_pointer (&stream),
                         (GClosureNotify) g_object_unref, 0);

  XConvertSelection (GDK_DISPLAY_XDISPLAY (display),
                     priv->xselection,
                     priv->xtarget,
                     priv->xproperty,
                     GDK_X11_DISPLAY (display)->leader_window,
                     timestamp);

  priv->pending_task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (priv->pending_task, gdk_x11_selection_input_stream_new_async);
  g_task_set_priority (priv->pending_task, io_priority);
}

GInputStream *
gdk_x11_selection_input_stream_new_finish (GAsyncResult  *result,
                                           const char   **type,
                                           int           *format,
                                           GError       **error)
{
  GdkX11SelectionInputStream *stream;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_x11_selection_input_stream_new_async, NULL);

  stream = g_task_propagate_pointer (task, error);
  if (stream)
    {
      GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

      if (type)
        *type = priv->type;
      if (format)
        *format = priv->format;
    }

  return G_INPUT_STREAM (stream);
}

