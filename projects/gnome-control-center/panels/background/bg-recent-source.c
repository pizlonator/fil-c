/* bg-recent-source.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "bg-recent-source"

#include "bg-recent-source.h"
#include "cc-background-item.h"

#define ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_NAME "," \
                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
                   G_FILE_ATTRIBUTE_TIME_MODIFIED

struct _BgRecentSource
{
  BgSource      parent;

  GFile        *backgrounds_folder;
  GFileMonitor *monitor;

  GCancellable *cancellable;
  GHashTable   *items;
};

G_DEFINE_TYPE (BgRecentSource, bg_recent_source, BG_TYPE_SOURCE)

static int
sort_func (gconstpointer a,
           gconstpointer b,
           gpointer      user_data)
{
  CcBackgroundItem *item_a;
  CcBackgroundItem *item_b;
  guint64 modified_a;
  guint64 modified_b;
  int retval;

  item_a = (CcBackgroundItem *) a;
  item_b = (CcBackgroundItem *) b;
  modified_a = cc_background_item_get_modified (item_a);
  modified_b = cc_background_item_get_modified (item_b);

  retval = modified_b - modified_a;

  return retval;
}

static void
add_file_from_info (BgRecentSource *self,
                    GFile          *file,
                    GFileInfo      *info)
{
  g_autoptr(CcBackgroundItem) item = NULL;
  g_autofree gchar *source_uri = NULL;
  g_autofree gchar *uri = NULL;
  GListStore *store;
  const gchar *content_type;
  guint64 mtime;

  content_type = g_file_info_get_content_type (info);
  mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  if (!content_type || !g_content_type_is_a (content_type, "image/*"))
    return;

  uri = g_file_get_uri (file);
  item = cc_background_item_new (uri);
  g_object_set (G_OBJECT (item),
                "shading", G_DESKTOP_BACKGROUND_SHADING_SOLID,
                "placement", G_DESKTOP_BACKGROUND_STYLE_ZOOM,
                "modified", mtime,
                "needs-download", FALSE,
                "source-url", source_uri,
                NULL);

  store = bg_source_get_liststore (BG_SOURCE (self));
  g_list_store_insert_sorted (store, item, sort_func, self);

  g_hash_table_insert (self->items, g_strdup (uri), g_object_ref (item));
}

static void
remove_item (BgRecentSource   *self,
             CcBackgroundItem *item)
{
  GListStore *store;
  const gchar *uri;
  guint i;

  g_return_if_fail (BG_IS_RECENT_SOURCE (self));
  g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

  uri = cc_background_item_get_uri (item);
  store = bg_source_get_liststore (BG_SOURCE (self));

  g_debug ("Removing wallpaper %s", uri);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      g_autoptr(CcBackgroundItem) tmp = NULL;

      tmp = g_list_model_get_item (G_LIST_MODEL (store), i);

      if (tmp == item)
        {
          g_list_store_remove (store, i);
          break;
        }
    }

  g_hash_table_remove (self->items, cc_background_item_get_uri (item));
}

static void
query_info_finished_cb (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  BgRecentSource *self;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  GFile *file = NULL;

  file = G_FILE (source);
  file_info = g_file_query_info_finish (file, result, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get pictures file information: %s", error->message);
      return;
    }

  self = BG_RECENT_SOURCE (user_data);

  g_debug ("Adding wallpaper %s (%d)",
           g_file_info_get_name (file_info),
           G_IS_FILE (self->backgrounds_folder));

  add_file_from_info (self, file, file_info);
}

static void
on_file_changed_cb (BgRecentSource    *self,
                    GFile             *file,
                    GFile             *other_file,
                    GFileMonitorEvent  event_type)
{
  g_autofree gchar *uri = NULL;

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      g_file_query_info_async (file,
                               ATTRIBUTES,
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                               G_PRIORITY_DEFAULT,
                               self->cancellable,
                               query_info_finished_cb,
                               self);
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
      uri = g_file_get_uri (file);
      remove_item (self, g_hash_table_lookup (self->items, uri));
      break;

    default:
      return;
    }
}

static int
file_sort_func (gconstpointer a,
                gconstpointer b)
{
  GFileInfo *file_a = G_FILE_INFO (a);
  GFileInfo *file_b = G_FILE_INFO (b);
  guint64 modified_a, modified_b;

  modified_a = g_file_info_get_attribute_uint64 (file_a, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  modified_b = g_file_info_get_attribute_uint64 (file_b, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  return modified_b - modified_a;
}

static void
file_info_async_ready_cb (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  BgRecentSource *self;
  g_autolist(GFileInfo) file_infos = NULL;
  g_autoptr(GError) error = NULL;
  GFile *parent = NULL;
  GList *l;

  file_infos = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source),
                                                    result,
                                                    &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get pictures file information: %s", error->message);
      return;
    }

  self = BG_RECENT_SOURCE (user_data);
  parent = g_file_enumerator_get_container (G_FILE_ENUMERATOR (source));

  file_infos = g_list_sort (file_infos, file_sort_func);

  for (l = file_infos; l; l = l->next)
    {
      g_autoptr(GFile) file = NULL;
      GFileInfo *info;

      info = l->data;
      file = g_file_get_child (parent, g_file_info_get_name (info));

      g_debug ("Found recent wallpaper %s", g_file_info_get_name (info));

      add_file_from_info (self, file, info);
    }

  g_file_enumerator_close (G_FILE_ENUMERATOR (source), self->cancellable, &error);

  if (error)
    g_warning ("Error closing file enumerator: %s", error->message);
}

static void
enumerate_children_finished_cb (GObject      *source,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  BgRecentSource *self;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;

  enumerator = g_file_enumerate_children_finish (G_FILE (source), result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not fill pictures source: %s", error->message);
      return;
    }

  self = BG_RECENT_SOURCE (user_data);
  g_file_enumerator_next_files_async (enumerator,
                                      G_MAXINT,
                                      G_PRIORITY_DEFAULT,
                                      self->cancellable,
                                      file_info_async_ready_cb,
                                      self);
}

static void
load_backgrounds (BgRecentSource *self)
{
  g_autofree gchar *backgrounds_path = NULL;
  g_autoptr(GError) error = NULL;

  if (!g_file_make_directory_with_parents (self->backgrounds_folder, self->cancellable, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      g_critical ("Failed to create local background directory: %s", error->message);
      return;
    }

  backgrounds_path = g_file_get_path (self->backgrounds_folder);
  g_debug ("Enumerating wallpapers under %s", backgrounds_path);

  g_file_enumerate_children_async (self->backgrounds_folder,
                                   ATTRIBUTES,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   G_PRIORITY_DEFAULT,
                                   self->cancellable,
                                   enumerate_children_finished_cb,
                                   self);

  self->monitor = g_file_monitor_directory (self->backgrounds_folder,
                                            G_FILE_MONITOR_WATCH_MOVES,
                                            self->cancellable,
                                            &error);

  if (!self->monitor)
    {
      g_critical ("Failed to monitor background directory: %s", error->message);
      return;
    }

  g_signal_connect_object (self->monitor, "changed", G_CALLBACK (on_file_changed_cb), self, G_CONNECT_SWAPPED);
}

/* Callbacks */

static void
on_file_copied_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(BgRecentSource) self = BG_RECENT_SOURCE (user_data);
  g_autofree gchar *original_file = NULL;
  g_autoptr(GError) error = NULL;

  g_file_copy_finish (G_FILE (source), result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_critical ("Failed to copy file: %s", error->message);
      return;
    }

   original_file = g_file_get_path (G_FILE (source));
   g_debug ("Successfully copied wallpaper: %s", original_file);
}

static void
on_file_deleted_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(BgRecentSource) self = BG_RECENT_SOURCE (user_data);
  g_autofree gchar *original_file = NULL;
  g_autoptr(GError) error = NULL;

  g_file_delete_finish (G_FILE (source), result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_critical ("Failed to delete wallpaper: %s", error->message);
      return;
    }

  original_file = g_file_get_path (G_FILE (source));
  g_debug ("Successfully deleted wallpaper: %s", original_file);
}

/* GObject overrides */

static void
bg_recent_source_finalize (GObject *object)
{
  BgRecentSource *self = (BgRecentSource *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (bg_recent_source_parent_class)->finalize (object);
}

static void
bg_recent_source_class_init (BgRecentSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bg_recent_source_finalize;
}

static void
bg_recent_source_init (BgRecentSource *self)
{
  g_autofree gchar *backgrounds_path = NULL;

  backgrounds_path = g_build_filename (g_get_user_data_dir (), "backgrounds", NULL);

  self->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->cancellable = g_cancellable_new ();
  self->backgrounds_folder = g_file_new_for_path (backgrounds_path);

  load_backgrounds (self);
}

BgRecentSource*
bg_recent_source_new (void)
{
  return g_object_new (BG_TYPE_RECENT_SOURCE, NULL);
}

void
bg_recent_source_add_file (BgRecentSource *self,
                           const gchar    *path)
{
  g_autoptr(GDateTime) now = NULL;
  g_autofree gchar *destination_name = NULL;
  g_autofree gchar *formatted_now = NULL;
  g_autofree gchar *basename = NULL;
  g_autoptr(GFile) destination = NULL;
  g_autoptr(GFile) file = NULL;

  g_return_if_fail (BG_IS_RECENT_SOURCE (self));
  g_return_if_fail (path && *path);

  g_debug ("Importing wallpaper %s", path);

  now = g_date_time_new_now_local ();
  formatted_now = g_date_time_format (now, "%Y-%m-%d-%H-%M-%S");

  file = g_file_new_for_path (path);

  basename = g_file_get_basename (file);
  destination_name = g_strdup_printf ("%s-%s", formatted_now, basename);
  destination = g_file_get_child (self->backgrounds_folder, destination_name);

  g_file_copy_async (file,
                     destination,
                     G_FILE_COPY_NONE,
                     G_PRIORITY_DEFAULT,
                     self->cancellable,
                     NULL, NULL,
                     on_file_copied_cb,
                     g_object_ref (self));
}

void
bg_recent_source_remove_item (BgRecentSource   *self,
                              CcBackgroundItem *item)
{
  g_autoptr(GFile) file = NULL;
  const gchar *uri;

  g_return_if_fail (BG_IS_RECENT_SOURCE (self));
  g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

  uri = cc_background_item_get_uri (item);
  file = g_file_new_for_uri (uri);

  g_file_delete_async (file,
                       G_PRIORITY_DEFAULT,
                       self->cancellable,
                       on_file_deleted_cb,
                       g_object_ref (self));
}
