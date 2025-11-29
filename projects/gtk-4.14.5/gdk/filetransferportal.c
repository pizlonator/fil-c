/*
 * Copyright (C) 2018 Matthias Clasen
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

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gdkcontentformats.h"
#include "gdkcontentserializer.h"
#include "gdkcontentdeserializer.h"
#include "gdkdebugprivate.h"

#include <gio/gio.h>

#ifdef G_OS_UNIX

#include <gio/gunixfdlist.h>

#ifndef O_PATH
#define O_PATH 0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif

#include "filetransferportalprivate.h"

static GDBusProxy *file_transfer_proxy = NULL;

typedef struct {
  GTask *task;
  const char **files;
  int len;
  int start;
} AddFileData;

static void add_files (GDBusProxy  *proxy,
                       AddFileData *afd);

static void
add_files_done (GObject      *object,
                GAsyncResult *result,
                gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  AddFileData *afd = data;
  GError *error = NULL;
  GVariant *ret;

  ret = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, NULL, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (afd->task, error);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  g_variant_unref (ret);

  if (afd->start >= afd->len)
    {
      g_task_return_boolean (afd->task, TRUE);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  add_files (proxy, afd);
}

/* We call AddFiles in chunks of 16 to avoid running into
 * the per-message fd limit of the bus.
 */
static void
add_files (GDBusProxy  *proxy,
           AddFileData *afd)
{
  GUnixFDList *fd_list;
  GVariantBuilder fds, options;
  int i;
  char *key;

  g_variant_builder_init (&fds, G_VARIANT_TYPE ("ah"));
  fd_list = g_unix_fd_list_new ();

  for (i = 0; afd->files[afd->start + i]; i++)
    {
      int fd;
      int fd_in;
      GError *error = NULL;

      if (i == 16)
        break;

      fd = open (afd->files[afd->start + i], O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_task_return_new_error (afd->task, G_IO_ERROR, g_io_error_from_errno (errno),
                                   "Failed to open %s", afd->files[afd->start + i]);
          g_object_unref (afd->task);
          g_free (afd);
          g_object_unref (fd_list);
          return;
         }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_in = g_unix_fd_list_append (fd_list, fd, &error);
      close (fd);

      if (fd_in == -1)
        {
          g_task_return_error (afd->task, error);
          g_object_unref (afd->task);
          g_free (afd);
          g_object_unref (fd_list);
          return;
        }

      g_variant_builder_add (&fds, "h", fd_in);
    }

   afd->start += 16;

  key = (char *)g_object_get_data (G_OBJECT (afd->task), "key");

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call_with_unix_fd_list (proxy,
                                       "AddFiles",
                                       g_variant_new ("(saha{sv})", key, &fds, &options),
                                       0, -1,
                                       fd_list,
                                       NULL,
                                       add_files_done, afd);

  g_object_unref (fd_list);
}

static void
start_session_done (GObject      *object,
                    GAsyncResult *result,
                    gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  AddFileData *afd = data;
  GError *error = NULL;
  GVariant *ret;
  const char *key;

  ret = g_dbus_proxy_call_finish (proxy, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (afd->task, error);
      g_object_unref (afd->task);
      g_free (afd);
      return;
    }

  g_variant_get (ret, "(&s)", &key);

  g_object_set_data_full (G_OBJECT (afd->task), "key", g_strdup (key), g_free);

  g_variant_unref (ret);

  add_files (proxy, afd);
}

void
file_transfer_portal_register_files (const char          **files,
                                     gboolean              writable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              data)
{
  GTask *task;
  AddFileData *afd;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  if (file_transfer_proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  afd = g_new (AddFileData, 1);
  afd->task = task;
  afd->files = files;
  afd->len = g_strv_length ((char **)files);
  afd->start = 0;

  GDK_DEBUG (DND, "file transfer portal: registering %d files", afd->len);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "writable", g_variant_new_boolean (writable));
  g_variant_builder_add (&options, "{sv}", "autostop", g_variant_new_boolean (TRUE));

  g_dbus_proxy_call (file_transfer_proxy, "StartTransfer",
                     g_variant_new ("(a{sv})", &options),
                     0, -1, NULL, start_session_done, afd);
}

gboolean
file_transfer_portal_register_files_finish (GAsyncResult  *result,
                                            char         **key,
                                            GError       **error)
{
  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      *key = g_strdup (g_object_get_data (G_OBJECT (result), "key"));
      return TRUE;
    }

  return FALSE;
}

static void
retrieve_files_done (GObject      *object,
                     GAsyncResult *result,
                     gpointer      data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  GTask *task = data;
  GError *error = NULL;
  GVariant *ret;
  char **files;

  ret = g_dbus_proxy_call_finish (proxy, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (ret, "(^a&s)", &files);

  g_object_set_data_full (G_OBJECT (task), "files", g_strdupv (files), (GDestroyNotify)g_strfreev);

  g_variant_unref (ret);

  g_task_return_boolean (task, TRUE);
}

void
file_transfer_portal_retrieve_files (const char          *key,
                                     GAsyncReadyCallback  callback,
                                     gpointer             data)
{
  GTask *task;
  GVariantBuilder options;

  task = g_task_new (NULL, NULL, callback, data);

  if (file_transfer_proxy == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No portal found");
      g_object_unref (task);
      return;
    }

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call (file_transfer_proxy,
                     "RetrieveFiles",
                     g_variant_new ("(sa{sv})", key, &options),
                     0, -1, NULL,
                     retrieve_files_done, task);
}

gboolean
file_transfer_portal_retrieve_files_finish (GAsyncResult   *result,
                                            char         ***files,
                                            GError        **error)
{
  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      *files = g_strdupv (g_object_get_data (G_OBJECT (result), "files"));
      return TRUE;
    }

  return FALSE;
}


/* serializer */

static void
file_serializer_finish (GObject      *source,
                        GAsyncResult *result,
                        gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
portal_ready (GObject *object,
              GAsyncResult *result,
              gpointer serializer)
{
  GError *error = NULL;
  char *key;

  if (!file_transfer_portal_register_files_finish (result, &key, &error))
    {
      gdk_content_serializer_return_error (serializer, error);
      return;
    }

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   key,
                                   strlen (key) + 1,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   file_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, key, g_free);
}

static void
portal_file_serializer (GdkContentSerializer *serializer)
{
  GPtrArray *files;
  const GValue *value;

  files = g_ptr_array_new_with_free_func (g_free);

  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      GFile *file;

      file = g_value_get_object (gdk_content_serializer_get_value (serializer));

      if (file && g_file_peek_path (file))
        {
          GDK_DEBUG (DND, "file transfer portal: Adding %s", g_file_peek_path (file));
          g_ptr_array_add (files, g_file_get_path (file));
        }
      else if (GDK_DEBUG_CHECK (DND))
        {
          char *uri = g_file_get_uri (file);
          gdk_debug_message ("file transfer portal: %s has no path, dropping\n", uri);
          g_free (uri);
        }

      g_ptr_array_add (files, NULL);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *l;

      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          GFile *file = l->data;

          if (file && g_file_peek_path (file))
            {
              GDK_DEBUG (DND, "file transfer portal: Adding %s", g_file_peek_path (file));
              g_ptr_array_add (files, g_file_get_path (file));
            }
          else
            {
              char *uri = g_file_get_uri (file);
              gdk_debug_message ("file transfer portal: %s has no path, dropping", uri);
              g_free (uri);
            }
        }

      g_ptr_array_add (files, NULL);
    }

  /* this call doesn't copy the strings, so keep the array around until the registration is done */
  file_transfer_portal_register_files ((const char **)files->pdata, TRUE, portal_ready, serializer);
  gdk_content_serializer_set_task_data (serializer, files, (GDestroyNotify)g_ptr_array_unref);
}

/* deserializer */

static void
portal_finish (GObject *object,
               GAsyncResult *result,
               gpointer deserializer)
{
  char **files = NULL;
  GError *error = NULL;
  GValue *value;

  if (!file_transfer_portal_retrieve_files_finish (result, &files, &error))
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  if (GDK_DEBUG_CHECK (DND))
    {
      char *s = g_strjoinv (", ", files);
      gdk_debug_message ("file transfer portal: Receiving files: %s", s);
      g_free (s);
    }

  value = gdk_content_deserializer_get_value (deserializer);
  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      if (files[0] != NULL)
        g_value_take_object (value, g_file_new_for_path (files[0]));
    }
  else
    {
      GSList *l = NULL;
      gsize i;

      for (i = 0; files[i] != NULL; i++)
        l = g_slist_prepend (l, g_file_new_for_path (files[i]));
      g_value_take_boxed (value, g_slist_reverse (l));
    }
  g_strfreev (files);

  gdk_content_deserializer_return_success (deserializer);
}

static void
portal_file_deserializer_finish (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  char *key;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  /* write terminating NULL */
  if (!g_output_stream_write (stream, "", 1, NULL, &error))
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  key = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (stream));
  if (key == NULL)
    {
      GError *gerror = g_error_new (G_IO_ERROR,
                                    G_IO_ERROR_NOT_FOUND,
                                    "Could not convert data from %s to %s",
                                    gdk_content_deserializer_get_mime_type (deserializer),
                                    g_type_name (gdk_content_deserializer_get_gtype (deserializer)));
      gdk_content_deserializer_return_error (deserializer, gerror);
      return;
    }

  file_transfer_portal_retrieve_files (key, portal_finish, deserializer);
  gdk_content_deserializer_set_task_data (deserializer, key, g_free);
}

static void
portal_file_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                portal_file_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
connection_closed (GDBusConnection *connection,
                   gboolean         remote_peer_vanished,
                   GError          *error)
{
  g_clear_object (&file_transfer_proxy);
}

static void
finish_registration (void)
{
  gdk_content_register_serializer (G_TYPE_FILE,
                                   "application/vnd.portal.filetransfer",
                                   portal_file_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "application/vnd.portal.filetransfer",
                                   portal_file_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_deserializer ("application/vnd.portal.filetransfer",
                                     GDK_TYPE_FILE_LIST,
                                     portal_file_deserializer,
                                     NULL,
                                     NULL);

  gdk_content_register_deserializer ("application/vnd.portal.filetransfer",
                                     G_TYPE_FILE,
                                     portal_file_deserializer,
                                     NULL,
                                     NULL);

  /* FIXME: I missed up and used the wrong mime type here when
   * I implemented my own protocol. Keep these around for a while
   * so we can interoperate with existing flatpaks using GTK 4.6
   */
  gdk_content_register_serializer (G_TYPE_FILE,
                                   "application/vnd.portal.files",
                                   portal_file_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "application/vnd.portal.files",
                                   portal_file_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_deserializer ("application/vnd.portal.files",
                                     GDK_TYPE_FILE_LIST,
                                     portal_file_deserializer,
                                     NULL,
                                     NULL);

  gdk_content_register_deserializer ("application/vnd.portal.files",
                                     G_TYPE_FILE,
                                     portal_file_deserializer,
                                     NULL,
                                     NULL);

  /* Free the singleton when the connection closes, important for test */
  g_signal_connect (g_dbus_proxy_get_connection (G_DBUS_PROXY (file_transfer_proxy)),
                    "closed", G_CALLBACK (connection_closed), NULL);
}

static gboolean
proxy_has_owner (GDBusProxy *proxy)
{
  char *owner;

  owner = g_dbus_proxy_get_name_owner (proxy);
  if (owner)
    {
      g_free (owner);
      return TRUE;
    }

  return FALSE;
}

void
file_transfer_portal_register (void)
{
  static gboolean called;

  if (!called)
    {
      called = TRUE;

      file_transfer_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS
                                | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                NULL,
                                "org.freedesktop.portal.Documents",
                                "/org/freedesktop/portal/documents",
                                "org.freedesktop.portal.FileTransfer",
                                NULL,
                                NULL);

      if (file_transfer_proxy && !proxy_has_owner (file_transfer_proxy))
        g_clear_object (&file_transfer_proxy);

      if (file_transfer_proxy)
        finish_registration ();
    }
}


#endif /* G_OS_UNIX */
