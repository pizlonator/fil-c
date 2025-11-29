/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2017 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>

#include "gtkopenuriportal.h"
#include "xdp-dbus.h"
#include "gtkwindowprivate.h"
#include "gtkprivate.h"
#include "gtkdialogerror.h"

#ifdef G_OS_UNIX
#include <gio/gunixfdlist.h>
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif


static GtkXdpOpenURI *openuri;

static gboolean
init_openuri_portal (void)
{
  static gsize openuri_inited = 0;

  if (g_once_init_enter (&openuri_inited))
    {
      GError *error = NULL;
      GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

      if (connection != NULL)
        {
          openuri = gtk_xdp_open_uri_proxy_new_sync (connection, 0,
                                                     PORTAL_BUS_NAME,
                                                     PORTAL_OBJECT_PATH,
                                                     NULL, &error);
          if (openuri == NULL)
            {
              g_warning ("Cannot create OpenURI portal proxy: %s", error->message);
              g_error_free (error);
            }

          if (gtk_xdp_open_uri_get_version (openuri) < 3)
            {
              g_warning ("Not a supported version of the OpenURI portal: %u", gtk_xdp_open_uri_get_version (openuri));
              g_clear_object (&openuri);
            }

          g_object_unref (connection);
        }
      else
        {
          g_warning ("Cannot connect to session bus when initializing OpenURI portal: %s",
                     error->message);
          g_error_free (error);
        }

      g_once_init_leave (&openuri_inited, 1);
    }

  return openuri != NULL;
}

gboolean
gtk_openuri_portal_is_available (void)
{
  return init_openuri_portal ();
}

enum {
  XDG_DESKTOP_PORTAL_SUCCESS   = 0,
  XDG_DESKTOP_PORTAL_CANCELLED = 1,
  XDG_DESKTOP_PORTAL_FAILED    = 2
};

enum {
  OPEN_URI    = 0,
  OPEN_FILE   = 1,
  OPEN_FOLDER = 2
};

typedef struct {
  GtkWindow *parent;
  char *parent_handle;
  GFile *file;
  char *uri;
  gboolean open_folder;
  GtkOpenuriFlags flags;
  GDBusConnection *connection;
  GCancellable *cancellable;
  GTask *task;
  char *handle;
  guint signal_id;
  glong cancel_handler;
  int call;
} OpenUriData;

static void
open_uri_data_free (OpenUriData *data)
{
  if (data->signal_id)
    g_dbus_connection_signal_unsubscribe (data->connection, data->signal_id);
  g_clear_object (&data->connection);
  if (data->cancel_handler)
    g_signal_handler_disconnect (data->cancellable, data->cancel_handler);
  if (data->parent && data->parent_handle)
    gtk_window_unexport_handle (data->parent, data->parent_handle);
  g_free (data->parent_handle);
  g_clear_object (&data->parent);
  g_clear_object (&data->file);
  g_free (data->uri);
  g_clear_object (&data->cancellable);
  g_clear_object (&data->task);
  g_free (data->handle);
  g_free (data);
}

static void
response_received (GDBusConnection *connection,
                   const char      *sender_name,
                   const char      *object_path,
                   const char      *interface_name,
                   const char      *signal_name,
                   GVariant        *parameters,
                   gpointer         user_data)
{
  GTask *task = user_data;
  guint32 response;

  g_variant_get (parameters, "(u@a{sv})", &response, NULL);

  switch (response)
    {
    case XDG_DESKTOP_PORTAL_SUCCESS:
      g_task_return_boolean (task, TRUE);
      break;
    case XDG_DESKTOP_PORTAL_CANCELLED:
      g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED, "The portal dialog was dismissed by the user");
      break;
    case XDG_DESKTOP_PORTAL_FAILED:
    default:
      g_task_return_new_error (task, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED, "The application launch failed");
      break;
    }

  g_object_unref (task);
}

static void
open_call_done (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  GtkXdpOpenURI *portal = GTK_XDP_OPEN_URI (source);
  GTask *task = user_data;
  OpenUriData *data = g_task_get_task_data (task);
  GError *error = NULL;
  gboolean res;
  char *path = NULL;

  switch (data->call)
    {
    case OPEN_FILE:
      res = gtk_xdp_open_uri_call_open_file_finish (portal, &path, NULL, result, &error);
      break;
    case OPEN_FOLDER:
      res = gtk_xdp_open_uri_call_open_directory_finish (portal, &path, NULL, result, &error);
      break;
    case OPEN_URI:
      res = gtk_xdp_open_uri_call_open_uri_finish (portal, &path, result, &error);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  if (!res)
    {
      g_free (path);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (g_strcmp0 (data->handle, path) != 0)
    {
      g_dbus_connection_signal_unsubscribe (data->connection, data->signal_id);

      data->signal_id = g_dbus_connection_signal_subscribe (data->connection,
                                                            PORTAL_BUS_NAME,
                                                            PORTAL_REQUEST_INTERFACE,
                                                            "Response",
                                                            path,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            response_received,
                                                            task,
                                                            NULL);
      g_free (data->handle);
      data->handle = g_strdup (path);
    }

  g_free (path);
}

static void
send_close (OpenUriData *data)
{
  GDBusMessage *message;
  GError *error = NULL;

  message = g_dbus_message_new_method_call (PORTAL_BUS_NAME,
                                            data->handle,
                                            PORTAL_REQUEST_INTERFACE,
                                            "Close");

  if (!g_dbus_connection_send_message (data->connection,
                                       message,
                                       G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                       NULL, &error))
    {
      g_warning ("unable to send Close message: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (message);
}

static void
canceled (GCancellable *cancellable,
          GTask        *task)
{
  OpenUriData *data = g_task_get_task_data (task);

  send_close (data);

  g_task_return_new_error (task,
                           GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED,
                           "The OpenURI portal call was cancelled by the application");
  g_object_unref (task);
}

static void
open_uri (OpenUriData         *data,
          const char          *parent_window,
          const char          *activation_token,
          GAsyncReadyCallback  callback)
{
  GFile *file = data->file;
  gboolean open_folder = data->open_folder;
  GTask *task;
  GVariant *opts = NULL;
  int i;
  GDBusConnection *connection;
  GVariantBuilder opt_builder;
  char *token;
  char *sender;

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (openuri));
  data->connection = g_object_ref (connection);

  task = g_task_new (NULL, NULL, callback, data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_task_data (task, data, NULL);
  if (data->cancellable)
    data->cancel_handler = g_signal_connect (data->cancellable, "cancelled", G_CALLBACK (canceled), task);

  token = g_strdup_printf ("gtk%d", g_random_int_range (0, G_MAXINT));
  sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
  for (i = 0; sender[i]; i++)
    if (sender[i] == '.')
      sender[i] = '_';

  data->handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  g_free (sender);

  data->signal_id = g_dbus_connection_signal_subscribe (connection,
                                                        PORTAL_BUS_NAME,
                                                        PORTAL_REQUEST_INTERFACE,
                                                        "Response",
                                                        data->handle,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        response_received,
                                                        task,
                                                        NULL);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (token));
  g_free (token);

  if (activation_token)
    g_variant_builder_add (&opt_builder, "{sv}", "activation_token", g_variant_new_string (activation_token));

  if (!open_folder)
    {
      if (data->flags & GTK_OPENURI_FLAGS_ASK)
        g_variant_builder_add (&opt_builder, "{sv}", "ask", g_variant_new_boolean (TRUE));
      if (data->flags & GTK_OPENURI_FLAGS_WRITABLE)
        g_variant_builder_add (&opt_builder, "{sv}", "writable", g_variant_new_boolean (TRUE));
    }

  opts = g_variant_builder_end (&opt_builder);

  if (file && g_file_is_native (file))
    {
      const char *path = NULL;
      GUnixFDList *fd_list = NULL;
      int fd, fd_id, errsv;

      path = g_file_peek_path (file);
      fd = g_open (path, O_RDONLY | O_CLOEXEC);
      errsv = errno;
      if (fd == -1)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR, g_io_error_from_errno (errsv),
                                   "Failed to open file");
          g_object_unref (task);
          return;
        }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_id = 0;

      if (open_folder)
        {
          data->call = OPEN_FOLDER;
          gtk_xdp_open_uri_call_open_directory (openuri,
                                             parent_window ? parent_window : "",
                                             g_variant_new ("h", fd_id),
                                             opts,
                                             fd_list,
                                             NULL,
                                             open_call_done,
                                             task);
        }
      else
        {
          data->call = OPEN_FILE;
          gtk_xdp_open_uri_call_open_file (openuri,
                                        parent_window ? parent_window : "",
                                        g_variant_new ("h", fd_id),
                                        opts,
                                        fd_list,
                                        NULL,
                                        open_call_done,
                                        task);
        }

      g_object_unref (fd_list);
    }
  else
    {
      char *uri = NULL;

      if (file)
        uri = g_file_get_uri (file);

      data->call = OPEN_URI;
      gtk_xdp_open_uri_call_open_uri (openuri,
                                   parent_window ? parent_window : "",
                                   uri ? uri : data->uri,
                                   opts,
                                   NULL,
                                   open_call_done,
                                   task);
      g_free (uri);
    }
}

static void
open_uri_done (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
  OpenUriData *data = user_data;
  GError *error = NULL;

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          g_task_return_new_error (data->task,
                                   GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED,
                                   "The operation was cancelled by the application");
        }
      else
        g_task_return_error (data->task, error);
    }
  else
    g_task_return_boolean (data->task, TRUE);

  open_uri_data_free (data);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle,
                        gpointer    user_data)
{
  OpenUriData *data = user_data;
  GdkDisplay *display;
  GAppLaunchContext *context;
  char *activation_token = NULL;

  data->parent_handle = g_strdup (handle);

  if (window)
    display = gtk_widget_get_display (GTK_WIDGET (window));
  else
    display = gdk_display_get_default ();

  /* FIXME
   * Call the vfunc directly since g_app_launch_context_get_startup_notify_id
   * has NULL checks.
   *
   * We should have a more direct way to do this.
   */
  context = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (display));
  activation_token = G_APP_LAUNCH_CONTEXT_GET_CLASS (context)->get_startup_notify_id (context, NULL, NULL);
  g_object_unref (context);

  open_uri (data, handle, activation_token, open_uri_done);

  g_free (activation_token);
}

void
gtk_openuri_portal_open_async (GFile               *file,
                               gboolean             open_folder,
                               GtkOpenuriFlags      flags,
                               GtkWindow           *parent,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  OpenUriData *data;

  if (!init_openuri_portal ())
    {
      g_task_report_new_error (NULL, callback, user_data, NULL,
                               GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                               "The OpenURI portal is not available");
      return;
    }

  data = g_new0 (OpenUriData, 1);
  data->parent = parent ? g_object_ref (parent) : NULL;
  data->file = g_object_ref (file);
  data->open_folder = open_folder;
  data->flags = flags;
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  data->task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_check_cancellable (data->task, FALSE);
  g_task_set_source_tag (data->task, gtk_openuri_portal_open_async);

  if (!parent || !gtk_window_export_handle (parent, window_handle_exported, data))
    window_handle_exported (parent, NULL, data);
}

gboolean
gtk_openuri_portal_open_finish (GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_openuri_portal_open_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
gtk_openuri_portal_open_uri_async (const char          *uri,
                                   GtkWindow           *parent,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  OpenUriData *data;

  if (!init_openuri_portal ())
    {
      g_task_report_new_error (NULL, callback, user_data, NULL,
                               GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED,
                               "The OpenURI portal is not available");
      return;
    }

  data = g_new0 (OpenUriData, 1);
  data->parent = parent ? g_object_ref (parent) : NULL;
  data->uri = g_strdup (uri);
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  data->task = g_task_new (parent, cancellable, callback, user_data);
  g_task_set_check_cancellable (data->task, FALSE);
  g_task_set_source_tag (data->task, gtk_openuri_portal_open_uri_async);

  if (!parent || !gtk_window_export_handle (parent, window_handle_exported, data))
    window_handle_exported (parent, NULL, data);
}

gboolean
gtk_openuri_portal_open_uri_finish (GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gtk_openuri_portal_open_uri_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
