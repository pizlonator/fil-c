/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"

#include "pp-host.h"

#include <glib/gi18n.h>

#define BUFFER_LENGTH 1024

typedef struct
{
  gchar *hostname;
  gint   port;
} PpHostPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PpHost, pp_host, G_TYPE_OBJECT);

enum {
  PROP_0 = 0,
  PROP_HOSTNAME,
  PROP_PORT,
};

enum {
  AUTHENTICATION_REQUIRED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
pp_host_finalize (GObject *object)
{
  PpHost *self = PP_HOST (object);
  PpHostPrivate *priv = pp_host_get_instance_private (self);

  g_clear_pointer (&priv->hostname, g_free);

  G_OBJECT_CLASS (pp_host_parent_class)->finalize (object);
}

static void
pp_host_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *param_spec)
{
  PpHost *self = PP_HOST (object);
  PpHostPrivate *priv = pp_host_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_HOSTNAME:
        g_value_set_string (value, priv->hostname);
        break;
      case PROP_PORT:
        g_value_set_int (value, priv->port);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_host_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *param_spec)
{
  PpHost *self = PP_HOST (object);
  PpHostPrivate *priv = pp_host_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_HOSTNAME:
        g_free (priv->hostname);
        priv->hostname = g_value_dup_string (value);
        break;
      case PROP_PORT:
        priv->port = g_value_get_int (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_host_class_init (PpHostClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = pp_host_set_property;
  gobject_class->get_property = pp_host_get_property;
  gobject_class->finalize = pp_host_finalize;

  g_object_class_install_property (gobject_class, PROP_HOSTNAME,
    g_param_spec_string ("hostname",
                         "Hostname",
                         "The hostname",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PORT,
    g_param_spec_int ("port",
                      "Port",
                      "The port",
                      -1, G_MAXINT32, PP_HOST_UNSET_PORT,
                      G_PARAM_READWRITE));

  signals[AUTHENTICATION_REQUIRED] =
    g_signal_new ("authentication-required",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
pp_host_init (PpHost *self)
{
  PpHostPrivate *priv = pp_host_get_instance_private (self);
  priv->port = PP_HOST_UNSET_PORT;
}

PpHost *
pp_host_new (const gchar *hostname)
{
  return g_object_new (PP_TYPE_HOST,
                       "hostname", hostname,
                       NULL);
}

static gchar **
line_split (gchar *line)
{
  gboolean          escaped = FALSE;
  gboolean          quoted = FALSE;
  gboolean          in_word = FALSE;
  gchar           **words = NULL;
  gchar           **result = NULL;
  g_autofree gchar *buffer = NULL;
  gchar             ch;
  gint              n = 0;
  gint              i, j = 0, k = 0;

  if (line)
    {
      n = strlen (line);
      words = g_new0 (gchar *, n + 1);
      buffer = g_new0 (gchar, n + 1);

      for (i = 0; i < n; i++)
        {
          ch = line[i];

          if (escaped)
            {
              buffer[k++] = ch;
              escaped = FALSE;
              continue;
            }

          if (ch == '\\')
            {
              in_word = TRUE;
              escaped = TRUE;
              continue;
            }

          if (in_word)
            {
              if (quoted)
                {
                  if (ch == '"')
                    quoted = FALSE;
                  else
                    buffer[k++] = ch;
                }
              else if (g_ascii_isspace (ch))
                {
                  words[j++] = g_strdup (buffer);
                  memset (buffer, 0, n + 1);
                  k = 0;
                  in_word = FALSE;
                }
              else if (ch == '"')
                quoted = TRUE;
              else
                buffer[k++] = ch;
            }
          else
            {
              if (ch == '"')
                {
                  in_word = TRUE;
                  quoted = TRUE;
                }
              else if (!g_ascii_isspace (ch))
                {
                  in_word = TRUE;
                  buffer[k++] = ch;
                }
            }
        }
    }

  if (buffer && buffer[0] != '\0')
    words[j++] = g_strdup (buffer);

  result = g_strdupv (words);
  g_strfreev (words);

  return result;
}

static void
_pp_host_get_snmp_devices_thread (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  PpHost           *self = source_object;
  PpHostPrivate    *priv = pp_host_get_instance_private (self);
  g_autoptr(GPtrArray) devices = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv)     argv = NULL;
  g_autofree gchar *stdout_string = NULL;
  gint              exit_status;

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  argv = g_new0 (gchar *, 3);
  argv[0] = g_strdup ("/usr/lib/cups/backend/snmp");
  argv[1] = g_strdup (priv->hostname);

  /* Use SNMP to get printer's informations */
  g_spawn_sync (NULL,
                argv,
                NULL,
                G_SPAWN_STDERR_TO_DEV_NULL,
                NULL,
                NULL,
                &stdout_string,
                NULL,
                &exit_status,
                &error);

  if (exit_status == 0 && stdout_string)
    {
      g_auto(GStrv)     printer_informations = NULL;
      gint              length;

      printer_informations = line_split (stdout_string);
      length = g_strv_length (printer_informations);

      if (length >= 4)
        {
          g_autofree gchar *device_name = NULL;
          gboolean is_network_device;
          PpPrintDevice *device;

          device_name = g_strdup (printer_informations[3]);
          g_strcanon (device_name, ALLOWED_CHARACTERS, '-');
          is_network_device = g_strcmp0 (printer_informations[0], "network") == 0;

          device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                 "is-network-device", is_network_device,
                                 "device-uri", printer_informations[1],
                                 "device-make-and-model", printer_informations[2],
                                 "device-info", printer_informations[3],
                                 "acquisition-method", ACQUISITION_METHOD_SNMP,
                                 "device-name", device_name,
                                 NULL);

          if (length >= 5 && printer_informations[4][0] != '\0')
            g_object_set (device, "device-id", printer_informations[4], NULL);

          if (length >= 6 && printer_informations[5][0] != '\0')
            g_object_set (device, "device-location", printer_informations[5], NULL);

          g_ptr_array_add (devices, device);
        }
    }

  g_task_return_pointer (task, g_ptr_array_ref (devices), (GDestroyNotify) g_ptr_array_unref);
}

void
pp_host_get_snmp_devices_async (PpHost              *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, _pp_host_get_snmp_devices_thread);
}

GPtrArray *
pp_host_get_snmp_devices_finish (PpHost        *self,
                                 GAsyncResult  *res,
                                 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
_pp_host_get_remote_cups_devices_thread (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  cups_dest_t   *dests = NULL;
  PpHost        *self = (PpHost *) source_object;
  PpHostPrivate *priv = pp_host_get_instance_private (self);
  g_autoptr(GPtrArray) devices = NULL;
  http_t        *http;
  gint           num_of_devices = 0;
  gint           port;
  gint           i;

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  if (priv->port == PP_HOST_UNSET_PORT)
    port = PP_HOST_DEFAULT_IPP_PORT;
  else
    port = priv->port;

  /* Connect to remote CUPS server and get its devices */
#ifdef HAVE_CUPS_HTTPCONNECT2
  http = httpConnect2 (priv->hostname, port, NULL, AF_UNSPEC,
                       HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
#else
  http = httpConnect (priv->hostname, port);
#endif
  if (http)
    {
      num_of_devices = cupsGetDests2 (http, &dests);
      if (num_of_devices > 0)
        {
          for (i = 0; i < num_of_devices; i++)
            {
              g_autofree gchar *device_uri = NULL;
              const char *device_location;
              PpPrintDevice *device;

              device_uri = g_strdup_printf ("ipp://%s:%d/printers/%s",
                                            priv->hostname,
                                            port,
                                            dests[i].name);

              device_location = cupsGetOption ("printer-location",
                                               dests[i].num_options,
                                               dests[i].options);

              device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                     "is-network-device", TRUE,
                                     "device-uri", device_uri,
                                     "device-name", dests[i].name,
                                     "device-location", device_location,
                                     "host-name", priv->hostname,
                                     "host-port", port,
                                     "acquisition-method", ACQUISITION_METHOD_REMOTE_CUPS_SERVER,
                                     NULL);
              g_ptr_array_add (devices, device);
            }
        }

      httpClose (http);
    }

  g_task_return_pointer (task, g_ptr_array_ref (devices), (GDestroyNotify) g_ptr_array_unref);
}

void
pp_host_get_remote_cups_devices_async (PpHost              *self,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, _pp_host_get_remote_cups_devices_thread);
}

GPtrArray *
pp_host_get_remote_cups_devices_finish (PpHost        *self,
                                        GAsyncResult  *res,
                                        GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_task_propagate_pointer (G_TASK (res), error);
}

typedef struct
{
  PpHost *host;
  gint    port;
} JetDirectData;

static void
jetdirect_data_free (JetDirectData *data)
{
  if (data != NULL)
    {
      g_clear_object (&data->host);
      g_free (data);
    }
}

static void
jetdirect_connection_test_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  g_autoptr(GSocketConnection) connection = NULL;
  PpHostPrivate               *priv;
  JetDirectData               *data;
  g_autoptr(GPtrArray)         devices = NULL;
  g_autoptr(GError)            error = NULL;
  g_autoptr(GTask)             task = G_TASK (user_data);

  data = g_task_get_task_data (task);
  priv = pp_host_get_instance_private (data->host);

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  connection = g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source_object),
                                                       res,
                                                       &error);

  if (connection != NULL)
    {
      g_autofree gchar *device_uri = NULL;
      PpPrintDevice *device;

      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);

      device_uri = g_strdup_printf ("socket://%s:%d",
                                    priv->hostname,
                                    data->port);

      device = g_object_new (PP_TYPE_PRINT_DEVICE,
                             "is-network-device", TRUE,
                             "device-uri", device_uri,
                             /* Translators: The found device is a JetDirect printer */
                             "device-name", _("JetDirect Printer"),
                             "host-name", priv->hostname,
                             "host-port", data->port,
                             "acquisition-method", ACQUISITION_METHOD_JETDIRECT,
                             NULL);
      g_ptr_array_add (devices, device);
    }

  g_task_return_pointer (task, g_ptr_array_ref (devices), (GDestroyNotify) g_ptr_array_unref);
}

/* Test whether given host has an AppSocket/HP JetDirect printer connected.
   See http://en.wikipedia.org/wiki/JetDirect
       http://www.cups.org/documentation.php/network.html */
void
pp_host_get_jetdirect_devices_async (PpHost              *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  PpHostPrivate    *priv = pp_host_get_instance_private (self);
  JetDirectData    *data;
  g_autoptr(GTask)  task = NULL;
  g_autofree gchar *address = NULL;

  data = g_new0 (JetDirectData, 1);
  data->host = g_object_ref (self);

  if (priv->port == PP_HOST_UNSET_PORT)
    data->port = PP_HOST_DEFAULT_JETDIRECT_PORT;
  else
    data->port = priv->port;

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
  g_task_set_task_data (task, data, (GDestroyNotify) jetdirect_data_free);

  address = g_strdup_printf ("%s:%d", priv->hostname, data->port);
  if (address != NULL && address[0] != '/')
    {
      g_autoptr(GSocketClient) client = NULL;

      client = g_socket_client_new ();

      g_socket_client_connect_to_host_async (client,
                                             address,
                                             data->port,
                                             cancellable,
                                             jetdirect_connection_test_cb,
                                             g_steal_pointer (&task));
    }
  else
    {
      GPtrArray *devices = g_ptr_array_new_with_free_func (g_object_unref);
      g_task_return_pointer (task, devices, (GDestroyNotify) g_ptr_array_unref);
    }
}

GPtrArray *
pp_host_get_jetdirect_devices_finish (PpHost        *self,
                                      GAsyncResult  *res,
                                      GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_task_propagate_pointer (G_TASK (res), error);
}

static gboolean
test_lpd_queue (GSocketClient *client,
                gchar         *address,
                gint           port,
                GCancellable  *cancellable,
                gchar         *queue_name)
{
  g_autoptr(GSocketConnection) connection = NULL;
  gboolean                     result = FALSE;
  g_autoptr(GError)            error = NULL;

  connection = g_socket_client_connect_to_host (client,
                                                address,
                                                port,
                                                cancellable,
                                                &error);

  if (connection != NULL)
    {
      if (G_IS_TCP_CONNECTION (connection))
        {
          GOutputStream *output;
          GInputStream  *input;
          gssize         bytes_read, bytes_written;
          gchar          buffer[BUFFER_LENGTH];
          gint           length;

          output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
          input = g_io_stream_get_input_stream (G_IO_STREAM (connection));

          /* This LPD command is explained in RFC 1179, section 5.2 */
          length = g_snprintf (buffer, BUFFER_LENGTH, "\2%s\n", queue_name);

          bytes_written = g_output_stream_write (output,
                                                 buffer,
                                                 length,
                                                 NULL,
                                                 &error);

          if (bytes_written != -1)
            {
              bytes_read = g_input_stream_read (input,
                                                buffer,
                                                BUFFER_LENGTH,
                                                NULL,
                                                &error);

              if (bytes_read != -1)
                {
                  if (bytes_read > 0 && buffer[0] == 0)
                    {
                      /* This LPD command is explained in RFC 1179, section 6.1 */
                      length = g_snprintf (buffer, BUFFER_LENGTH, "\1\n");

                      bytes_written = g_output_stream_write (output,
                                                             buffer,
                                                             length,
                                                             NULL,
                                                             &error);

                      result = TRUE;
                    }
                }
            }
        }

      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
    }

  return result;
}

static void
_pp_host_get_lpd_devices_thread (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  g_autoptr(GSocketConnection) connection = NULL;
  PpHost                      *self = source_object;
  PpHostPrivate               *priv = pp_host_get_instance_private (self);
  g_autoptr(GPtrArray)         devices = NULL;
  g_autoptr(GSocketClient)     client = NULL;
  g_autoptr(GError)            error = NULL;
  GList                       *candidates = NULL;
  GList                       *iter;
  gchar                       *found_queue = NULL;
  gchar                       *candidate;
  g_autofree gchar            *address = NULL;
  gint                         port;
  gint                         i;

  if (priv->port == PP_HOST_UNSET_PORT)
    port = PP_HOST_DEFAULT_LPD_PORT;
  else
    port = priv->port;

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  address = g_strdup_printf ("%s:%d", priv->hostname, port);
  if (address == NULL || address[0] == '/')
    {
      g_task_return_pointer (task, g_ptr_array_ref (devices), (GDestroyNotify) g_ptr_array_unref);
      return;
    }

  client = g_socket_client_new ();

  connection = g_socket_client_connect_to_host (client,
                                                address,
                                                port,
                                                cancellable,
                                                &error);

  if (connection != NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);

      /* Most of this list is taken from system-config-printer */
      candidates = g_list_append (candidates, g_strdup ("PASSTHRU"));
      candidates = g_list_append (candidates, g_strdup ("AUTO"));
      candidates = g_list_append (candidates, g_strdup ("BINPS"));
      candidates = g_list_append (candidates, g_strdup ("RAW"));
      candidates = g_list_append (candidates, g_strdup ("TEXT"));
      candidates = g_list_append (candidates, g_strdup ("ps"));
      candidates = g_list_append (candidates, g_strdup ("lp"));
      candidates = g_list_append (candidates, g_strdup ("PORT1"));

      for (i = 0; i < 8; i++)
        {
          candidates = g_list_append (candidates, g_strdup_printf ("LPT%d", i));
          candidates = g_list_append (candidates, g_strdup_printf ("LPT%d_PASSTHRU", i));
          candidates = g_list_append (candidates, g_strdup_printf ("COM%d", i));
          candidates = g_list_append (candidates, g_strdup_printf ("COM%d_PASSTHRU", i));
        }

      for (i = 0; i < 50; i++)
        candidates = g_list_append (candidates, g_strdup_printf ("pr%d", i));

      for (iter = candidates; iter != NULL; iter = iter->next)
        {
          candidate = (gchar *) iter->data;

          if (test_lpd_queue (client,
                              address,
                              port,
                              cancellable,
                              candidate))
            {
              found_queue = g_strdup (candidate);
              break;
            }
        }

      if (found_queue != NULL)
        {
          g_autofree gchar *device_uri = NULL;
          PpPrintDevice *device;

          device_uri = g_strdup_printf ("lpd://%s:%d/%s",
                                        priv->hostname,
                                        port,
                                        found_queue);

          device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                 "is-network-device", TRUE,
                                 "device-uri", device_uri,
                                 /* Translators: The found device is a Line Printer Daemon printer */
                                 "device-name", _("LPD Printer"),
                                 "host-name", priv->hostname,
                                 "host-port", port,
                                 "acquisition-method", ACQUISITION_METHOD_LPD,
                                 NULL);
          g_ptr_array_add (devices, device);
        }

      g_list_free_full (candidates, g_free);
    }

  g_task_return_pointer (task, g_ptr_array_ref (devices), (GDestroyNotify) g_ptr_array_unref);
}

void
pp_host_get_lpd_devices_async (PpHost              *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (G_OBJECT (self), cancellable, callback, user_data);
  g_task_run_in_thread (task, _pp_host_get_lpd_devices_thread);
}

GPtrArray *
pp_host_get_lpd_devices_finish (PpHost        *self,
                                GAsyncResult  *res,
                                GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_task_propagate_pointer (G_TASK (res), error);
}
