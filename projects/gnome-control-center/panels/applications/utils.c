/* utils.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <config.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ftw.h>

#include "utils.h"
#ifdef HAVE_SNAP
#include "cc-snapd-client.h"
#endif

static gint
ftw_remove_cb (const gchar       *path,
               const struct stat *sb,
               gint               typeflags,
               struct FTW        *ftwbuf)
{
  remove (path);
  return 0;
}

static void
file_remove_thread_func (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GFile *file = source_object;
  g_autofree gchar *path = g_file_get_path (file);

  nftw (path, ftw_remove_cb, 20, FTW_PHYS | FTW_DEPTH);

  if (g_task_set_return_on_cancel (task, FALSE))
    g_task_return_boolean (task, TRUE);
}

void
file_remove_async (GFile               *file,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             data)
{
  g_autoptr(GTask) task = g_task_new (file, cancellable, callback, data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, file_remove_thread_func);
}

gboolean
file_remove_finish (GFile        *file,
                    GAsyncResult *result,
                    GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (result, file), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

static GPrivate size_key = G_PRIVATE_INIT (g_free);

static gint
ftw_size_cb (const gchar       *path,
             const struct stat *sb,
             gint               typeflags,
             struct FTW        *ftwbuf)
{
  guint64 *size = (guint64*)g_private_get (&size_key);
  if (typeflags == FTW_F)
    *size += sb->st_size;
  return 0;
}

static void
file_size_thread_func (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GFile *file = source_object;
  g_autofree gchar *path = g_file_get_path (file);
  guint64 *total;

  g_private_replace (&size_key, g_new0 (guint64, 1));

  nftw (path, ftw_size_cb, 20, FTW_PHYS | FTW_DEPTH);

  total = g_new0 (guint64, 1);
  *total = *(guint64*)g_private_get (&size_key);

  if (g_task_set_return_on_cancel (task, FALSE))
    g_task_return_pointer (task, total, g_free);
}

void
file_size_async (GFile               *file,
                 GCancellable        *cancellable,
                 GAsyncReadyCallback  callback,
                 gpointer             data)
{
  g_autoptr(GTask) task = g_task_new (file, cancellable, callback, data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, file_size_thread_func);
}

gboolean
file_size_finish (GFile        *file,
                  GAsyncResult *result,
                  guint64      *size,
                  GError      **error)
{
  g_autofree guint64 *data = NULL;

  g_return_val_if_fail (g_task_is_valid (result, file), FALSE);
  data = g_task_propagate_pointer (G_TASK (result), error);
  if (data == NULL)
    return FALSE;
  if (size != NULL)
    *size = *data;
  return TRUE;
}

static gchar *
get_output_of (const gchar **argv)
{
  g_autofree gchar *output = NULL;
  int status;

  if (!g_spawn_sync (NULL,
                     (gchar**) argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL, NULL,
                     &output, NULL,
                     &status, NULL))
    return NULL;

  if (!g_spawn_check_wait_status (status, NULL))
    return NULL;

  return g_steal_pointer (&output);
}

GKeyFile *
get_flatpak_metadata (const gchar *app_id)
{
  const gchar *argv[5] = { "flatpak", "info", "-m", "app", NULL };
  g_autofree gchar *data = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;

  argv[3] = app_id;

  data = get_output_of (argv);
  if (data == NULL)
    return NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile, data, -1, 0, &error))
    {
      g_warning ("%s", error->message);
      return NULL;
    }

  return g_steal_pointer (&keyfile);
}

guint64
get_flatpak_app_size (const gchar *app_id)
{
  const gchar *argv[5] = { "flatpak", "info", "-s", "app", NULL };
  g_autofree gchar *data = NULL;
  guint64 factor;
  double val;

  argv[3] = app_id;

  data = get_output_of (argv);
  if (data == NULL)
    return 0;

  data = g_strstrip (data);

  if (g_str_has_suffix (data, "kB") || g_str_has_suffix (data, "kb"))
    factor = 1000;
  else if (g_str_has_suffix (data, "MB") || g_str_has_suffix (data, "Mb"))
    factor = 1000 * 1000;
  else if (g_str_has_suffix (data, "GB") || g_str_has_suffix (data, "Gb"))
    factor = 1000 * 1000 * 1000;
  else if (g_str_has_suffix (data, "KiB") || g_str_has_suffix (data, "Kib"))
    factor = 1024;
  else if (g_str_has_suffix (data, "MiB") || g_str_has_suffix (data, "Mib"))
    factor = 1024 * 1024;
  else if (g_str_has_suffix (data, "GiB") || g_str_has_suffix (data, "Gib"))
    factor = 1024 * 1024 * 1024;
  else
    factor = 1;

  val = g_ascii_strtod (data, NULL);

  return (guint64)(val * factor);
}

guint64
get_snap_app_size (const gchar *snap_name)
{
#ifdef HAVE_SNAP
  g_autoptr(CcSnapdClient) client = NULL;
  g_autoptr(JsonObject) snap = NULL;
  g_autoptr(GError) error = NULL;

  client = cc_snapd_client_new ();
  snap = cc_snapd_client_get_snap_sync (client, snap_name, NULL, &error);
  if (snap == NULL)
    {
      g_warning ("Failed to get snap size: %s", error->message);
      return 0;
    }

  return json_object_get_int_member (snap, "installed-size");
#else
  return 0;
#endif
}

char *
get_app_id (GAppInfo *info)
{
  gchar *app_id = g_strdup (g_app_info_get_id (info));

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  return app_id;
}
