/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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

#include "pp-samba.h"

#include "config.h"

#include <glib/gi18n.h>
#include <libsmbclient.h>
#include <errno.h>

#define POLL_DELAY 100000

struct _PpSamba
{
  PpHost    parent_instance;

  /* Auth info */
  gchar    *username;
  gchar    *password;
  gboolean  waiting;
};

G_DEFINE_TYPE (PpSamba, pp_samba, PP_TYPE_HOST);

static void
pp_samba_finalize (GObject *object)
{
  PpSamba *self = PP_SAMBA (object);

  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->password, g_free);

  G_OBJECT_CLASS (pp_samba_parent_class)->finalize (object);
}

static void
pp_samba_class_init (PpSambaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = pp_samba_finalize;
}

static void
pp_samba_init (PpSamba *samba)
{
}

PpSamba *
pp_samba_new (const gchar *hostname)
{
  return g_object_new (PP_TYPE_SAMBA,
                       "hostname", hostname,
                       NULL);
}

typedef struct
{
  PpSamba       *samba;
  GPtrArray     *devices;
  GMainContext  *context;
  gboolean       auth_if_needed;
  gboolean       hostname_set;
  gboolean       cancelled;
} SMBData;

static void
smb_data_free (SMBData *data)
{
  if (data)
    {
      g_ptr_array_unref (data->devices);

      g_free (data);
    }
}

static gboolean
get_auth_info (gpointer user_data)
{
  SMBData *data = (SMBData *) user_data;
  PpSamba *samba = PP_SAMBA (data->samba);

  g_signal_emit_by_name (samba, "authentication-required");

  return FALSE;
}

void
pp_samba_set_auth_info (PpSamba     *samba,
                        const gchar *username,
                        const gchar *password)
{
  g_free (samba->username);
  if ((username != NULL) && (username[0] != '\0'))
    samba->username = g_strdup (username);
  else
    samba->username = NULL;

  g_free (samba->password);
  if ((password != NULL) && (password[0] != '\0'))
    samba->password = g_strdup (password);
  else
    samba->password = NULL;

  samba->waiting = FALSE;
}

static void
auth_fn (SMBCCTX    *smb_context,
         const char *server,
         const char *share,
         char       *workgroup,
         int         wgmaxlen,
         char       *username,
         int         unmaxlen,
         char       *password,
         int         pwmaxlen)
{
  PpSamba           *samba;
  g_autoptr(GSource) source = NULL;
  SMBData           *data;

  data = (SMBData *) smbc_getOptionUserData (smb_context);
  samba = data->samba;

  if (!data->cancelled)
    {
      samba->username = g_strdup (username);
      samba->password = g_strdup (password);

      source = g_idle_source_new ();
      g_source_set_callback (source,
                             get_auth_info,
                             data,
                             NULL);
      g_source_attach (source, data->context);

      samba->waiting = TRUE;

      /*
       * smbclient needs to get authentication data
       * from this synchronous callback so we are blocking
       * until we get them
       */
      while (samba->waiting)
        {
          g_usleep (POLL_DELAY);
        }

      /* Samba tries to call the auth_fn again if we just set the values
       * to NULL when we want to cancel the authentication 
       */
      if (samba->username == NULL && samba->password == NULL)
        data->cancelled = TRUE;

      if (samba->username != NULL)
        {
          if (g_strcmp0 (username, samba->username) != 0)
            g_strlcpy (username, samba->username, unmaxlen);
        }
      else
        {
          username[0] = '\0';
        }

      if (samba->password != NULL)
        {
          if (g_strcmp0 (password, samba->password) != 0)
            g_strlcpy (password, samba->password, pwmaxlen);
        }
      else
        {
          password[0] = '\0';
        }

    }
}

static void
anonymous_auth_fn (SMBCCTX    *smb_context,
                   const char *server,
                   const char *share,
                   char       *workgroup,
                   int         wgmaxlen,
                   char       *username,
                   int         unmaxlen,
                   char       *password,
                   int         pwmaxlen)
{
  username[0] = '\0';
  password[0] = '\0';
}

static void
list_dir (SMBCCTX      *smb_context,
          const gchar  *dirname,
          const gchar  *path,
          GCancellable *cancellable,
          SMBData      *data)
{
  struct smbc_dirent *dirent;
  smbc_closedir_fn    smbclient_closedir;
  smbc_readdir_fn     smbclient_readdir;
  smbc_opendir_fn     smbclient_opendir;
  const gchar        *host_name;
  SMBCFILE           *dir;

  if (!g_cancellable_is_cancelled (cancellable))
    {
      smbclient_closedir = smbc_getFunctionClosedir (smb_context);
      smbclient_readdir = smbc_getFunctionReaddir (smb_context);
      smbclient_opendir = smbc_getFunctionOpendir (smb_context);

      dir = smbclient_opendir (smb_context, dirname);
      if (!dir && errno == EACCES)
        {
          if (g_str_has_prefix (dirname, "smb://"))
            host_name = dirname + 6;
          else
            host_name = dirname;

          if (data->auth_if_needed)
            {
              data->cancelled = FALSE;
              smbc_setFunctionAuthDataWithContext (smb_context, auth_fn);
              dir = smbclient_opendir (smb_context, dirname);
              smbc_setFunctionAuthDataWithContext (smb_context, anonymous_auth_fn);

              if (data->cancelled)
                {
                  PpPrintDevice *device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                        "host-name", host_name,
                                                        "is-authenticated-server", TRUE,
                                                        NULL);
                  g_ptr_array_add (data->devices, device);

                  if (dir)
                    smbclient_closedir (smb_context, dir);
                  return;
                }
            }
          else
            {
              PpPrintDevice *device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                    "host-name", host_name,
                                                    "is-authenticated-server", TRUE,
                                                    NULL);
              g_ptr_array_add (data->devices, device);
            }
        }

      while (dir && (dirent = smbclient_readdir (smb_context, dir)))
        {
          g_autofree gchar *subdirname = NULL;
          g_autofree gchar *subpath = NULL;

          if (dirent->smbc_type == SMBC_WORKGROUP)
            {
              subdirname = g_strdup_printf ("%s%s", dirname, dirent->name);
              subpath = g_strdup_printf ("%s%s", path, dirent->name);
            }

          if (dirent->smbc_type == SMBC_SERVER)
            {
              subdirname = g_strdup_printf ("smb://%s", dirent->name);
              subpath = g_strdup_printf ("%s//%s", path, dirent->name);
            }

          if (dirent->smbc_type == SMBC_PRINTER_SHARE)
            {
              g_autofree gchar *uri = NULL;
              g_autofree gchar *device_name = NULL;
              g_autofree gchar *device_uri = NULL;
              PpPrintDevice *device;

              uri = g_strdup_printf ("%s/%s", dirname, dirent->name);
              device_uri = g_uri_escape_string (uri,
                                                G_URI_RESERVED_CHARS_GENERIC_DELIMITERS
                                                G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS,
                                                FALSE);

              device_name = g_strdup (dirent->name);
              g_strcanon (device_name, ALLOWED_CHARACTERS, '-');

              device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                     "device-uri", device_uri,
                                     "is-network-device", TRUE,
                                     "device-info", dirent->comment,
                                     "device-name", device_name,
                                     "acquisition-method", data->hostname_set ? ACQUISITION_METHOD_SAMBA_HOST : ACQUISITION_METHOD_SAMBA,
                                     "device-location", path,
                                     "host-name", dirname,
                                     NULL);

              g_ptr_array_add (data->devices, device);
            }

          if (subdirname)
            {
              list_dir (smb_context,
                        subdirname,
                        subpath,
                        cancellable,
                        data);
            }
        }

      if (dir)
        smbclient_closedir (smb_context, dir);
    }
}

static void
_pp_samba_get_devices_thread (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  static GMutex   mutex;
  SMBData        *data = (SMBData *) task_data;
  SMBCCTX        *smb_context;

  data->devices = g_ptr_array_new_with_free_func (g_object_unref);
  data->samba = PP_SAMBA (source_object);

  g_mutex_lock (&mutex);

  smb_context = smbc_new_context ();
  if (smb_context)
    {
      if (smbc_init_context (smb_context))
        {
          g_autofree gchar *hostname = NULL;
          g_autofree gchar *dirname = NULL;
          g_autofree gchar *path = NULL;

          smbc_setOptionUserData (smb_context, data);

          g_object_get (source_object, "hostname", &hostname, NULL);
          if (hostname != NULL)
            {
              dirname = g_strdup_printf ("smb://%s", hostname);
              path = g_strdup_printf ("//%s", hostname);
            }
          else
            {
              dirname = g_strdup_printf ("smb://");
              path = g_strdup_printf ("//");
            }

          smbc_setFunctionAuthDataWithContext (smb_context, anonymous_auth_fn);
          list_dir (smb_context, dirname, path, cancellable, data);
        }

      smbc_free_context (smb_context, 1);
    }

  g_mutex_unlock (&mutex);

  g_task_return_pointer (task, g_ptr_array_ref (data->devices), (GDestroyNotify) g_ptr_array_unref);
}

void
pp_samba_get_devices_async (PpSamba             *samba,
                            gboolean             auth_if_needed,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask)  task = NULL;
  SMBData          *data;
  g_autofree gchar *hostname = NULL;

  g_object_get (G_OBJECT (samba), "hostname", &hostname, NULL);

  task = g_task_new (samba, cancellable, callback, user_data);
  data = g_new0 (SMBData, 1);
  data->devices = NULL;
  data->context = g_main_context_default ();
  data->hostname_set = hostname != NULL;
  data->auth_if_needed = auth_if_needed;

  g_task_set_task_data (task, data, (GDestroyNotify) smb_data_free);
  g_task_run_in_thread (task, _pp_samba_get_devices_thread);
}

GPtrArray *
pp_samba_get_devices_finish (PpSamba       *samba,
                             GAsyncResult  *res,
                             GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, samba), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_task_propagate_pointer (G_TASK (res), error);
}
