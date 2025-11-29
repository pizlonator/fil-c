/* GDK HData Output Stream - a stream backed by a global memory buffer
 * 
 * Copyright (C) 2018 Руслан Ижбулатов
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
 * Author: Руслан Ижбулатов <lrn1986@gmail.com>
 */

#include "config.h"

#include <windows.h>

#include "gdkprivate-win32.h"
#include "gdkhdataoutputstream-win32.h"

#include "gdkclipboard-win32.h"
#include "gdkdisplay-win32.h"
#include <glib/gi18n-lib.h>
#include "gdkwin32display.h"
#include "gdkwin32surface.h"


typedef struct _GdkWin32HDataOutputStreamPrivate  GdkWin32HDataOutputStreamPrivate;

struct _GdkWin32HDataOutputStreamPrivate
{
  HANDLE                     handle;
  guchar                    *data;
  gsize                      data_allocated_size;
  gsize                      data_length;
  GdkWin32ContentFormatPair  pair;
  guint                      handle_is_buffer : 1;
  guint                      closed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkWin32HDataOutputStream, gdk_win32_hdata_output_stream, G_TYPE_OUTPUT_STREAM);

static gssize
write_stream (GdkWin32HDataOutputStream         *stream,
              GdkWin32HDataOutputStreamPrivate  *priv,
              const void                        *buffer,
              gsize                              count,
              GError                           **error)
{
  gsize spillover = (priv->data_length + count) - priv->data_allocated_size;
  gsize to_copy = count;

  if (priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("writing a closed stream"));
      return -1;
    }

  if (spillover > 0 && !priv->handle_is_buffer)
    {
      guchar *new_data;
      HANDLE new_handle = GlobalReAlloc (priv->handle, priv->data_allocated_size + spillover, 0);

      if (new_handle != NULL)
        {
          new_data = g_try_realloc (priv->data, priv->data_allocated_size + spillover);

          if (new_data != NULL)
            {
              priv->handle = new_handle;
              priv->data = new_data;
              priv->data_allocated_size += spillover;
            }
          else
            {
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                   _("g_try_realloc () failed"));
              return -1;
            }
        }
      else
        {
          DWORD error_code = GetLastError ();
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "%s%lu", _("GlobalReAlloc() failed: "), error_code);
          return -1;
        }
    }

  if (priv->handle_is_buffer)
    {
      to_copy = MIN (count, priv->data_allocated_size - priv->data_length);

      if (to_copy == 0)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Ran out of buffer space (buffer size is fixed)"));
          return -1;
        }

      memcpy (&((guchar *) priv->handle)[priv->data_length], buffer, to_copy);
    }
  else
    memcpy (&priv->data[priv->data_length], buffer, to_copy);

  priv->data_length += to_copy;

  return to_copy;
}

static gssize
gdk_win32_hdata_output_stream_write (GOutputStream  *output_stream,
                                     const void     *buffer,
                                     gsize           count,
                                     GCancellable   *cancellable,
                                     GError        **error)
{
  GdkWin32HDataOutputStream *stream = GDK_WIN32_HDATA_OUTPUT_STREAM (output_stream);
  GdkWin32HDataOutputStreamPrivate *priv = gdk_win32_hdata_output_stream_get_instance_private (stream);
  gssize result = write_stream (stream, priv, buffer, count, error);

  if (result != -1)
    GDK_NOTE(SELECTION, g_printerr ("CLIPBOARD: wrote %zd bytes, %" G_GSIZE_FORMAT " total now\n",
                                    result, priv->data_length));

  return result;
}

static void
gdk_win32_hdata_output_stream_write_async (GOutputStream        *output_stream,
                                           const void           *buffer,
                                           gsize                 count,
                                           int                   io_priority,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  GdkWin32HDataOutputStream *stream = GDK_WIN32_HDATA_OUTPUT_STREAM (output_stream);
  GdkWin32HDataOutputStreamPrivate *priv = gdk_win32_hdata_output_stream_get_instance_private (stream);
  GTask *task;
  gssize result;
  GError *error = NULL;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_win32_hdata_output_stream_write_async);
  g_task_set_priority (task, io_priority);

  result = write_stream (stream, priv, buffer, count, &error);

  if (result != -1)
    {
      GDK_NOTE (SELECTION, g_printerr ("CLIPBOARD async wrote %zd bytes, %" G_GSIZE_FORMAT " total now\n",
                                       result, priv->data_length));
      g_task_return_int (task, result);
    }
  else
    g_task_return_error (task, error);

  g_object_unref (task);

  return;
}

static gssize
gdk_win32_hdata_output_stream_write_finish (GOutputStream  *stream,
                                            GAsyncResult   *result,
                                            GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_win32_hdata_output_stream_write_async, -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static gboolean
gdk_win32_hdata_output_stream_close (GOutputStream  *output_stream,
                                     GCancellable   *cancellable,
                                     GError        **error)
{
  GdkWin32HDataOutputStream *stream = GDK_WIN32_HDATA_OUTPUT_STREAM (output_stream);
  GdkWin32HDataOutputStreamPrivate *priv = gdk_win32_hdata_output_stream_get_instance_private (stream);
  guchar *hdata;

  if (priv->closed)
    return TRUE;

  if (priv->pair.transmute)
    {
      guchar *transmuted_data = NULL;
      gsize   transmuted_data_length;

      if (priv->handle_is_buffer)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Can’t transmute a single handle"));
          return FALSE;
        }

      if (!_gdk_win32_transmute_contentformat (priv->pair.contentformat,
                                               priv->pair.w32format,
                                               priv->data,
                                               priv->data_length,
                                               &transmuted_data,
                                               &transmuted_data_length))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       _("Failed to transmute %zu bytes of data from %s to %u"),
                       priv->data_length,
                       priv->pair.contentformat,
                       priv->pair.w32format);
          return FALSE;
        }
      else
        {
          HANDLE new_handle;

          new_handle = GlobalReAlloc (priv->handle, transmuted_data_length, 0);

          if (new_handle == NULL)
            {
              DWORD error_code = GetLastError ();
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "%s%lu", _("GlobalReAlloc() failed: "), error_code);
              return FALSE;
            }

          priv->handle = new_handle;
          priv->data_length = transmuted_data_length;
          g_clear_pointer (&priv->data, g_free);
          priv->data = transmuted_data;
        }
    }

  if (!priv->handle_is_buffer)
    {
      hdata = GlobalLock (priv->handle);

      if (hdata == NULL)
        {
          DWORD error_code = GetLastError ();
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "%s%lu", _("GlobalLock() failed: "), error_code);
          return FALSE;
        }

      memcpy (hdata, priv->data, priv->data_length);
      GlobalUnlock (priv->handle);
      g_clear_pointer (&priv->data, g_free);
    }

  priv->closed = 1;

  return TRUE;
}

static void
gdk_win32_hdata_output_stream_close_async (GOutputStream       *stream,
                                           int                  io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GTask *task;
  GError *error = NULL;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_win32_hdata_output_stream_close_async);
  g_task_set_priority (task, io_priority);

  if (gdk_win32_hdata_output_stream_close (stream, NULL, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
}

static gboolean
gdk_win32_hdata_output_stream_close_finish (GOutputStream  *stream,
                                            GAsyncResult   *result,
                                            GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, gdk_win32_hdata_output_stream_close_async), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_win32_hdata_output_stream_finalize (GObject *object)
{
  GdkWin32HDataOutputStream *stream = GDK_WIN32_HDATA_OUTPUT_STREAM (object);
  GdkWin32HDataOutputStreamPrivate *priv = gdk_win32_hdata_output_stream_get_instance_private (stream);

  g_clear_pointer (&priv->data, g_free);

  /* We deliberately don't close the memory object,
   * as it will be used elsewhere (it's a shame that
   * MS global memory handles are not refcounted and
   * not duplicateable).
   * Except when the stream isn't closed, which means
   * that the caller never bothered to get the handle.
   */
  if (!priv->closed && priv->handle)
    {
      if (_gdk_win32_format_uses_hdata (priv->pair.w32format))
        GlobalFree (priv->handle);
      else
        CloseHandle (priv->handle);
    }

  G_OBJECT_CLASS (gdk_win32_hdata_output_stream_parent_class)->finalize (object);
}

static void
gdk_win32_hdata_output_stream_class_init (GdkWin32HDataOutputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *output_stream_class = G_OUTPUT_STREAM_CLASS (klass);

  object_class->finalize = gdk_win32_hdata_output_stream_finalize;

  output_stream_class->write_fn = gdk_win32_hdata_output_stream_write;
  output_stream_class->close_fn = gdk_win32_hdata_output_stream_close;

  output_stream_class->write_async = gdk_win32_hdata_output_stream_write_async;
  output_stream_class->write_finish = gdk_win32_hdata_output_stream_write_finish;
  output_stream_class->close_async = gdk_win32_hdata_output_stream_close_async;
  output_stream_class->close_finish = gdk_win32_hdata_output_stream_close_finish;
}

static void
gdk_win32_hdata_output_stream_init (GdkWin32HDataOutputStream *stream)
{
}

GOutputStream *
gdk_win32_hdata_output_stream_new (GdkWin32ContentFormatPair  *pair,
                                   GError                    **error)
{
  GdkWin32HDataOutputStream *stream;
  GdkWin32HDataOutputStreamPrivate *priv;
  HANDLE handle;
  gboolean hmem;

  hmem = _gdk_win32_format_uses_hdata (pair->w32format);

  if (hmem)
    {
      handle = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, 0);

      if (handle == NULL)
        {
          DWORD error_code = GetLastError ();
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "%s%lu", _("GlobalAlloc() failed: "), error_code);

          return NULL;
        }
    }

  stream = g_object_new (GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM, NULL);
  priv = gdk_win32_hdata_output_stream_get_instance_private (stream);
  priv->pair = *pair;

  if (hmem)
    {
      priv->handle = handle;
    }
  else
    {
      priv->data_allocated_size = sizeof (priv->handle);
      priv->handle_is_buffer = 1;
    }

  return G_OUTPUT_STREAM (stream);
}

HANDLE
gdk_win32_hdata_output_stream_get_handle (GdkWin32HDataOutputStream *stream,
                                          gboolean                  *is_hdata)
{
  GdkWin32HDataOutputStreamPrivate *priv;
  priv = gdk_win32_hdata_output_stream_get_instance_private (stream);

  if (!priv->closed)
    return NULL;

  if (is_hdata)
    *is_hdata = _gdk_win32_format_uses_hdata (priv->pair.w32format);

  return priv->handle;
}
