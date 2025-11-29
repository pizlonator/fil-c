/* GTK - The GIMP Toolkit
 * gtkfilesystemmodel.c: GtkTreeModel wrapping a GtkFileSystem
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "gtkfilesystemmodelprivate.h"

#include <stdlib.h>
#include <string.h>

#include "gtkfilechooserutils.h"
#include "gtkmarshalers.h"
#include "gtkfilter.h"
#include "gtkprivate.h"

/* priority used for all async callbacks in the main loop
 * This should be higher than redraw priorities so multiple callbacks
 * firing can be handled without intermediate redraws
 */
#define IO_PRIORITY G_PRIORITY_DEFAULT

/* random number that everyone else seems to use, too */
#define FILES_PER_QUERY 100

typedef struct _FileModelNode           FileModelNode;

struct _FileModelNode
{
  GFile *               file;           /* file represented by this node */
  GFileInfo *           info;           /* info for this file or NULL if unknown */

  guint                 row;            /* if valid (see model->n_valid_indexes), visible nodes before and including
                                         * this one - see the "Structure" comment above.
                                         */

  guint                 visible :1;     /* if the file is currently visible */
  guint                 filtered_out :1;/* if the file is currently filtered out (i.e. it didn't pass the filters) */
  guint                 frozen_add :1;  /* true if the model was frozen and the entry has not been added yet */
};

struct _GtkFileSystemModel
{
  GObject               parent_instance;

  GFile *               dir;            /* directory that's displayed */
  guint                 dir_thaw_source;/* GSource id for unfreezing the model */
  char *                attributes;     /* attributes the file info must contain, or NULL for all attributes */
  GFileMonitor *        dir_monitor;    /* directory that is monitored, or NULL if monitoring was not supported */

  GCancellable *        cancellable;    /* cancellable in use for all operations - cancelled on dispose */
  GArray *              files;          /* array of FileModelNode containing all our files */
  guint                 n_nodes_valid;  /* count of valid nodes (i.e. those whose node->row is accurate) */
  GHashTable *          file_lookup;    /* mapping of GFile => array index in model->files
                                         * This hash table doesn't always have the same number of entries
                                         * as the files array; it gets re-populated in node_get_for_file()
                                         * if this mismatch is detected.
                                         */

  GtkFileFilter *       filter;         /* filter to use for deciding which nodes are visible */

  guint                 frozen;         /* number of times we're frozen */

  unsigned int          filter_on_thaw   : 1; /* set when filtering needs to happen upon thawing */
  unsigned int          show_hidden      : 1; /* whether to show hidden files */
  unsigned int          show_folders     : 1; /* whether to show folders */
  unsigned int          show_files       : 1; /* whether to show files */
  unsigned int          filter_folders   : 1; /* whether filter applies to folders */
  unsigned int          can_select_files : 1;
};

static void freeze_updates (GtkFileSystemModel *model);
static void thaw_updates (GtkFileSystemModel *model);

/*** FileModelNode ***/

/* Get a FileModelNode structure given an index in the model->files array of nodes */
#define get_node(_model, _index) ((FileModelNode *) ((_model)->files->data + (_index) * sizeof (FileModelNode)))

/* Get an index within the model->files array of nodes, given a FileModelNode* */
#define node_index(_model, _node) (((char *) (_node) - (_model)->files->data) / sizeof (FileModelNode))

/* @up_to_index: smallest model->files array index that will be valid after this call
 * @up_to_row: smallest node->row that will be valid after this call
 *
 * If you want to validate up to an index or up to a row, specify the index or
 * the row you want and specify G_MAXUINT for the other argument.  Pass
 * G_MAXUINT for both arguments for “validate everything”.
 */
static void
node_validate_rows (GtkFileSystemModel *model,
                    guint               up_to_index,
                    guint               up_to_row)
{
  guint i, row;

  if (model->files->len == 0)
    return;

  up_to_index = MIN (up_to_index, model->files->len - 1);

  i = model->n_nodes_valid;
  if (i != 0)
    row = get_node (model, i - 1)->row;
  else
    row = 0;

  while (i <= up_to_index && row <= up_to_row)
    {
      FileModelNode *node = get_node (model, i);
      if (node->visible)
        row++;
      node->row = row;
      i++;
    }
  model->n_nodes_valid = i;
}

static guint G_GNUC_UNUSED
node_get_tree_row (GtkFileSystemModel *model,
                   guint               index)
{
  if (model->n_nodes_valid <= index)
    node_validate_rows (model, index, G_MAXUINT);

  return get_node (model, index)->row - 1;
}

static void
node_invalidate_index (GtkFileSystemModel *model,
                       guint               id)
{
  model->n_nodes_valid = MIN (model->n_nodes_valid, id);
}

static void
node_set_visible_and_filtered_out (GtkFileSystemModel *model,
                                   guint               id,
                                   gboolean            visible,
                                   gboolean            filtered_out,
                                   gboolean            selectable)
{
  FileModelNode *node = get_node (model, id);

  /* Filteredness */

  g_file_info_set_attribute_boolean (node->info, "filechooser::filtered-out", filtered_out);

  if (node->filtered_out != filtered_out)
    {
      node->filtered_out = filtered_out;
    }

  /* Selectability */

  g_file_info_set_attribute_boolean (node->info, "filechooser::selectable", selectable);

  /* Visibility */

  g_file_info_set_attribute_boolean (node->info, "filechooser::visible", visible);

  if (node->visible == visible ||
      node->frozen_add)
    return;

  if (visible)
    {
      node->visible = TRUE;
      node_invalidate_index (model, id);
    }
  else
    {
      g_assert (node_get_tree_row (model, id) < model->files->len);

      node->visible = FALSE;
      node_invalidate_index (model, id);
    }
}

static gboolean
node_should_be_filtered_out (GtkFileSystemModel *model,
                             guint               id)
{
  FileModelNode *node = get_node (model, id);

  if (node->info == NULL)
    return TRUE;

  if (model->filter == NULL)
    return FALSE;

  g_assert (g_file_info_has_attribute (node->info, "standard::file"));

  return !gtk_filter_match (GTK_FILTER (model->filter), node->info);
}

static gboolean
node_should_be_visible (GtkFileSystemModel *model,
                        guint               id,
                        gboolean            filtered_out)
{
  FileModelNode *node = get_node (model, id);
  gboolean has_is_hidden;
  gboolean has_is_backup;
  gboolean result;

  if (node->info == NULL)
    return FALSE;

  has_is_hidden = g_file_info_has_attribute (node->info, "standard::is-hidden");
  has_is_backup = g_file_info_has_attribute (node->info, "standard::is-backup");
  if (!model->show_hidden &&
      ((has_is_hidden && g_file_info_get_is_hidden (node->info)) ||
       (has_is_backup && g_file_info_get_is_backup (node->info))))
    return FALSE;

  if (_gtk_file_info_consider_as_directory (node->info))
    {
      if (!model->show_folders)
        return FALSE;

      if (!model->filter_folders)
        return TRUE;
    }
  else
    {
      if (!model->show_files)
        return FALSE;
    }

  result = !filtered_out;

  return result;
}

static gboolean
node_should_be_selectable (GtkFileSystemModel *model,
                          guint                id)
{
  FileModelNode *node = get_node (model, id);

  if (node->info == NULL)
    return TRUE;

  if (_gtk_file_info_consider_as_directory (node->info))
    return TRUE;
  else
    return model->can_select_files;
}

static void
node_compute_visibility_and_filters (GtkFileSystemModel *model,
                                     guint               id)
{
  gboolean filtered_out;
  gboolean visible;
  gboolean selectable;

  filtered_out = node_should_be_filtered_out (model, id);
  visible = node_should_be_visible (model, id, filtered_out);
  selectable = node_should_be_selectable (model, id);

  node_set_visible_and_filtered_out (model, id, visible, filtered_out, selectable);
}

static guint
node_get_for_file (GtkFileSystemModel *model,
                   GFile              *file)
{
  gpointer position;
  int i;

  if (g_hash_table_lookup_extended (model->file_lookup,
                                    file, NULL,
                                    &position))
    return GPOINTER_TO_UINT (position);

  /* The invariant here is that the files in model->files[n] for n < g_hash_table_size (model->file_lookup)
   * are already added to the hash table. this loop merely rebuilds our (file -> index) mapping on demand.
   *
   * If we exit the loop, the next pending batch of mappings will be resolved when this function gets called again
   * with another file that is not yet in the mapping.
   */
  for (i = g_hash_table_size (model->file_lookup); i < model->files->len; i++)
    {
      FileModelNode *node = get_node (model, i);

      g_hash_table_insert (model->file_lookup, node->file, GUINT_TO_POINTER (i));
      if (g_file_equal (node->file, file))
        return i;
    }

  return GTK_INVALID_LIST_POSITION;
}

/*** GListModel ***/

static GType
list_model_get_item_type (GListModel *list_model)
{
  return G_TYPE_FILE_INFO;
}

static guint
list_model_get_n_items (GListModel *list_model)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (list_model);

  return model->files->len;
}

static gpointer
list_model_get_item (GListModel *list_model,
                     guint       position)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (list_model);
  FileModelNode *node;

  if (position >= model->files->len)
    return NULL;

  node = get_node (model, position);
  return g_object_ref (node->info);
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items = list_model_get_n_items;
  iface->get_item = list_model_get_item;
}

/*** GtkFileSystemModel ***/

/* Signal IDs */
enum {
  FINISHED_LOADING,
  LAST_SIGNAL
};

static guint file_system_model_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkFileSystemModel, gtk_file_system_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                g_list_model_iface_init))

static void
freeze_updates (GtkFileSystemModel *model)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  model->frozen++;
}

static void
gtk_file_system_model_refilter_all (GtkFileSystemModel *model)
{
  guint i;

  if (model->frozen)
    {
      model->filter_on_thaw = TRUE;
      return;
    }

  freeze_updates (model);

  for (i = 0; i < model->files->len; i++)
    node_compute_visibility_and_filters (model, i);

  g_list_model_items_changed (G_LIST_MODEL (model), 0, model->files->len, model->files->len);
  model->filter_on_thaw = FALSE;
  thaw_updates (model);
}

static void
thaw_updates (GtkFileSystemModel *model)
{
  gboolean stuff_added;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (model->frozen > 0);

  model->frozen--;
  if (model->frozen > 0)
    return;

  stuff_added = model->files->len > 0 &&
                get_node (model, model->files->len - 1)->frozen_add;

  if (model->filter_on_thaw)
    gtk_file_system_model_refilter_all (model);
  if (stuff_added)
    {
      guint i;
      guint changed_idx = G_MAXUINT;

      for (i = 0; i < model->files->len; i++)
        {
          FileModelNode *node = get_node (model, i);

          if (!node->frozen_add)
            continue;

          node->frozen_add = FALSE;
          node_compute_visibility_and_filters (model, i);
          if (changed_idx == G_MAXUINT)
            changed_idx = i;
        }

      if (changed_idx != G_MAXUINT)
        g_list_model_items_changed (G_LIST_MODEL (model), changed_idx,
                                    model->files->len - changed_idx,
                                    model->files->len - changed_idx);
    }
}

static void
add_file (GtkFileSystemModel *model,
          GFile              *file,
          GFileInfo          *info)
{
  FileModelNode *node;
  guint position;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  node = g_new0 (FileModelNode, 1);
  node->file = g_object_ref (file);
  if (info)
    {
      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
      node->info = g_object_ref (info);
    }
  node->frozen_add = model->frozen ? TRUE : FALSE;

  g_array_append_vals (model->files, node, 1);
  g_free (node);

  position = model->files->len - 1;

  if (!model->frozen)
    {
      node_compute_visibility_and_filters (model, position);
      g_list_model_items_changed (G_LIST_MODEL (model), position, 0, 1);
    }
}

static void
adjust_file_lookup (GtkFileSystemModel *model, guint id, int increment)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, model->file_lookup);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      guint index = GPOINTER_TO_UINT (value);

      if (index >= id)
        {
          index += increment;
          g_hash_table_iter_replace (&iter, GUINT_TO_POINTER (index));
        }
    }
}

static void
remove_file (GtkFileSystemModel *model,
             GFile              *file)
{
  FileModelNode *node;
  guint id;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));

  id = node_get_for_file (model, file);
  if (id == GTK_INVALID_LIST_POSITION)
    return;

  node = get_node (model, id);

  node_invalidate_index (model, id);

  g_hash_table_remove (model->file_lookup, file);
  g_clear_object (&node->file);
  adjust_file_lookup (model, id, -1);

  g_clear_object (&node->info);

  g_array_remove_index (model->files, id);

  g_list_model_items_changed (G_LIST_MODEL (model), id, 1, 0);
}

static void
gtk_file_system_model_dispose (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);

  g_clear_handle_id (&model->dir_thaw_source, g_source_remove);

  g_cancellable_cancel (model->cancellable);
  if (model->dir_monitor)
    g_file_monitor_cancel (model->dir_monitor);

  G_OBJECT_CLASS (gtk_file_system_model_parent_class)->dispose (object);
}


static void
gtk_file_system_model_finalize (GObject *object)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (object);
  guint i;

  for (i = 0; i < model->files->len; i++)
    {
      FileModelNode *node = get_node (model, i);
      g_clear_object (&node->file);
      g_clear_object (&node->info);
    }
  g_array_free (model->files, TRUE);

  g_clear_object (&model->cancellable);
  g_clear_pointer (&model->attributes, g_free);
  g_clear_object (&model->dir);
  g_clear_object (&model->dir_monitor);
  g_clear_pointer (&model->file_lookup, g_hash_table_destroy);
  g_clear_object (&model->filter);

  G_OBJECT_CLASS (gtk_file_system_model_parent_class)->finalize (object);
}

static void
gtk_file_system_model_class_init (GtkFileSystemModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_system_model_finalize;
  gobject_class->dispose = gtk_file_system_model_dispose;

  file_system_model_signals[FINISHED_LOADING] =
    g_signal_new (I_("finished-loading"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, G_TYPE_ERROR);
}

static void
gtk_file_system_model_init (GtkFileSystemModel *model)
{
  model->show_files = TRUE;
  model->show_folders = TRUE;
  model->show_hidden = FALSE;
  model->filter_folders = FALSE;
  model->can_select_files = TRUE;

  model->file_lookup = g_hash_table_new (g_file_hash, (GEqualFunc) g_file_equal);
  model->cancellable = g_cancellable_new ();
}

/*** API ***/

static void
gtk_file_system_model_closed_enumerator (GObject      *object,
                                         GAsyncResult *res,
                                         gpointer      data)
{
  g_file_enumerator_close_finish (G_FILE_ENUMERATOR (object), res, NULL);
}

static gboolean
thaw_func (gpointer data)
{
  GtkFileSystemModel *model = data;

  thaw_updates (model);
  model->dir_thaw_source = 0;

  return FALSE;
}

static void
gtk_file_system_model_got_files (GObject      *object,
                                 GAsyncResult *res,
                                 gpointer      data)
{
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (object);
  GtkFileSystemModel *model = data;
  GList *walk, *files;
  GError *error = NULL;

  files = g_file_enumerator_next_files_finish (enumerator, res, &error);

  if (files)
    {
      if (model->dir_thaw_source == 0)
        {
          freeze_updates (model);
          model->dir_thaw_source = g_timeout_add_full (IO_PRIORITY + 1, 50,
                                                       thaw_func,
                                                       model,
                                                       NULL);
          gdk_source_set_static_name_by_id (model->dir_thaw_source, "[gtk] thaw_func");
        }

      for (walk = files; walk; walk = walk->next)
        {
          const char *name;
          GFileInfo *info;
          GFile *file;

          info = walk->data;
          name = g_file_info_get_name (info);
          if (name == NULL)
            {
              /* Shouldn't happen, but the APIs allow it */
              g_object_unref (info);
              continue;
            }
          file = g_file_get_child (model->dir, name);
          add_file (model, file, info);
          g_object_unref (file);
          g_object_unref (info);
        }
      g_list_free (files);

      g_file_enumerator_next_files_async (enumerator,
                                          g_file_is_native (model->dir) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
                                          IO_PRIORITY,
                                          model->cancellable,
                                          gtk_file_system_model_got_files,
                                          model);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_file_enumerator_close_async (enumerator,
                                         IO_PRIORITY,
                                         model->cancellable,
                                         gtk_file_system_model_closed_enumerator,
                                         NULL);
          if (model->dir_thaw_source != 0)
            {
              g_clear_handle_id (&model->dir_thaw_source, g_source_remove);
              thaw_updates (model);
            }

          g_signal_emit (model, file_system_model_signals[FINISHED_LOADING], 0, error);
        }

      if (error)
        g_error_free (error);
    }
}

static void
gtk_file_system_model_update_file (GtkFileSystemModel *model,
                                   GFile              *file,
                                   GFileInfo          *info)
{
  FileModelNode *node;
  guint id;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  id = node_get_for_file (model, file);
  if (id == GTK_INVALID_LIST_POSITION)
    {
      add_file (model, file, info);
      id = node_get_for_file (model, file);
    }

  node = get_node (model, id);

  g_set_object (&node->info, info);

  g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
}

/* Helper for gtk_file_system_model_query_done and
 * gtk_file_system_model_one_query_done */
static void
query_done_helper (GObject      *object,
                   GAsyncResult *res,
                   gpointer      data,
                   gboolean      do_thaw_updates)
{
  GtkFileSystemModel *model = GTK_FILE_SYSTEM_MODEL (data);
  GFile *file = G_FILE (object);
  GFileInfo *info;

  info = g_file_query_info_finish (file, res, NULL);
  if (info)
    {
      gtk_file_system_model_update_file (model, file, info);
      g_object_unref (info);
    }

  if (do_thaw_updates)
    thaw_updates (model);
}

static void
gtk_file_system_model_query_done (GObject *     object,
                                  GAsyncResult *res,
                                  gpointer      data)
{
  query_done_helper (object, res, data, FALSE);
}

static void
gtk_file_system_model_monitor_change (GFileMonitor *      monitor,
                                      GFile *             file,
                                      GFile *             other_file,
                                      GFileMonitorEvent   type,
                                      GtkFileSystemModel *model)
{
  switch (type)
    {
      case G_FILE_MONITOR_EVENT_CREATED:
      case G_FILE_MONITOR_EVENT_CHANGED:
      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        if (g_file_equal (file, model->dir))
          return;
        /* We can treat all children the same way */
        g_file_query_info_async (file,
                                 model->attributes,
                                 G_FILE_QUERY_INFO_NONE,
                                 IO_PRIORITY,
                                 model->cancellable,
                                 gtk_file_system_model_query_done,
                                 model);
        break;
      case G_FILE_MONITOR_EVENT_DELETED:
        remove_file (model, file);
        break;
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        /* FIXME: use freeze/thaw with this somehow? */
      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
      case G_FILE_MONITOR_EVENT_UNMOUNTED:
      case G_FILE_MONITOR_EVENT_MOVED:
      case G_FILE_MONITOR_EVENT_RENAMED:
      case G_FILE_MONITOR_EVENT_MOVED_IN:
      case G_FILE_MONITOR_EVENT_MOVED_OUT:
      default:
        /* ignore these */
        break;
    }
}

static void
gtk_file_system_model_got_enumerator (GObject *dir, GAsyncResult *res, gpointer data)
{
  GtkFileSystemModel *model = data;
  GFileEnumerator *enumerator;
  GError *error = NULL;

  enumerator = g_file_enumerate_children_finish (G_FILE (dir), res, &error);
  if (enumerator == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      {
        g_signal_emit (model, file_system_model_signals[FINISHED_LOADING], 0, error);
        g_error_free (error);
      }
    }
  else
    {
      g_file_enumerator_next_files_async (enumerator,
                                          g_file_is_native (model->dir) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
                                          IO_PRIORITY,
                                          model->cancellable,
                                          gtk_file_system_model_got_files,
                                          model);
      g_object_unref (enumerator);
      model->dir_monitor = g_file_monitor_directory (model->dir,
                                                     G_FILE_MONITOR_NONE,
                                                     model->cancellable,
                                                     NULL); /* we don't mind if directory monitoring isn't supported, so the GError is NULL here */
      if (model->dir_monitor)
        g_signal_connect (model->dir_monitor,
                          "changed",
                          G_CALLBACK (gtk_file_system_model_monitor_change),
                          model);
    }
}

static void
gtk_file_system_model_set_directory (GtkFileSystemModel *model,
                                     GFile *             dir,
                                     const char *       attributes)
{
  g_assert (G_IS_FILE (dir));

  model->dir = g_object_ref (dir);
  model->attributes = g_strdup (attributes);

  g_file_enumerate_children_async (model->dir,
                                   attributes,
                                   G_FILE_QUERY_INFO_NONE,
                                   IO_PRIORITY,
                                   model->cancellable,
                                   gtk_file_system_model_got_enumerator,
                                   model);

}

/**
 * _gtk_file_system_model_new:
 *
 * Creates a new `GtkFileSystemModel` object. You need to add files
 * to the list using _gtk_file_system_model_add_and_query_file()
 * or _gtk_file_system_model_update_file().
 *
 * Returns: the newly created `GtkFileSystemModel`
 **/
GtkFileSystemModel *
_gtk_file_system_model_new (void)
{
  GtkFileSystemModel *model;

  model = g_object_new (GTK_TYPE_FILE_SYSTEM_MODEL, NULL);

  model->files = g_array_sized_new (FALSE, FALSE, sizeof (FileModelNode), FILES_PER_QUERY);

  return model;
}

/**
 * _gtk_file_system_model_new_for_directory:
 * @directory: the directory to show.
 * @attributes: (nullable): attributes to immediately load or %NULL for all
 *
 * Creates a new `GtkFileSystemModel` object.
 *
 * The `GtkFileSystemModel` object wraps the given @directory as a
 * `GtkTreeModel`. The model will query the given directory with the
 * given @attributes and add all files inside the directory automatically.
 * If supported, it will also monitor the drectory and update the model's
 * contents to reflect changes, if the @directory supports monitoring.
 *
 * Returns: the newly created `GtkFileSystemModel`
 **/
GtkFileSystemModel *
_gtk_file_system_model_new_for_directory (GFile      *dir,
                                          const char *attributes)
{
  GtkFileSystemModel *model;

  g_return_val_if_fail (G_IS_FILE (dir), NULL);

  model = _gtk_file_system_model_new ();
  gtk_file_system_model_set_directory (model, dir, attributes);

  return model;
}

/**
 * _gtk_file_system_model_set_show_hidden:
 * @model: a `GtkFileSystemModel`
 * @show_hidden: whether hidden files should be displayed
 *
 * Sets whether hidden files should be included in the `GtkTreeModel`
 * for display.
 **/
void
_gtk_file_system_model_set_show_hidden (GtkFileSystemModel *model,
                                        gboolean            show_hidden)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_hidden = show_hidden != FALSE;

  if (show_hidden != model->show_hidden)
    {
      model->show_hidden = show_hidden;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_folders:
 * @model: a `GtkFileSystemModel`
 * @show_folders: whether folders should be displayed
 *
 * Sets whether folders should be included in the `GtkTreeModel`
 * for display.
 */
void
_gtk_file_system_model_set_show_folders (GtkFileSystemModel *model,
                                         gboolean            show_folders)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_folders = show_folders != FALSE;

  if (show_folders != model->show_folders)
    {
      model->show_folders = show_folders;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_show_files:
 * @model: a `GtkFileSystemModel`
 * @show_files: whether files (as opposed to folders) should be displayed.
 *
 * Sets whether files (as opposed to folders) should be included
 * in the `GtkTreeModel` for display.
 */
void
_gtk_file_system_model_set_show_files (GtkFileSystemModel *model,
                                       gboolean            show_files)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  show_files = show_files != FALSE;

  if (show_files != model->show_files)
    {
      model->show_files = show_files;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_set_filter_folders:
 * @model: a `GtkFileSystemModel`
 * @filter_folders: whether the filter applies to folders
 *
 * Sets whether the filter set by _gtk_file_system_model_set_filter()
 * applies to folders. By default, it does not and folders are always
 * visible.
 */
void
_gtk_file_system_model_set_filter_folders (GtkFileSystemModel *model,
                                           gboolean            filter_folders)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  filter_folders = filter_folders != FALSE;

  if (filter_folders != model->filter_folders)
    {
      model->filter_folders = filter_folders;
      gtk_file_system_model_refilter_all (model);
    }
}

/**
 * _gtk_file_system_model_get_cancellable:
 * @model: the model
 *
 * Gets the cancellable used by the @model. This is the cancellable used
 * internally by the @model that will be cancelled when @model is
 * disposed. So you can use it for operations that should be cancelled
 * when the model goes away.
 *
 * Returns: The cancellable used by @model
 **/
GCancellable *
_gtk_file_system_model_get_cancellable (GtkFileSystemModel *model)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);

  return model->cancellable;
}

GFileInfo *
_gtk_file_system_model_get_info_for_file (GtkFileSystemModel *model,
                                          GFile              *file)
{
  FileModelNode *node;
  guint i;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  i = node_get_for_file (model, file);

  if (i == GTK_INVALID_LIST_POSITION)
    return NULL;

  node = get_node (model, i);
  return node->info;
}

/**
 * _gtk_file_system_model_update_files:
 * @model: the model
 * @files: the files
 * @infos: the new file infos
 *
 * Tells the file system model that the files changed and that the
 * new @infos should be used for it now.  If these files are not
 * part of @model, it will get added automatically.
 **/
void
_gtk_file_system_model_update_files (GtkFileSystemModel *model,
                                     GList              *files,
                                     GList              *infos)
{
  GList *l, *i;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  freeze_updates (model);

  for (l = files, i = infos; l; l = l->next, i = i->next)
    gtk_file_system_model_update_file (model, (GFile *)l->data, (GFileInfo *)i->data);

  thaw_updates (model);
}

/**
 * _gtk_file_system_model_set_filter:
 * @mode: a `GtkFileSystemModel`
 * @filter: (nullable): %NULL or filter to use
 *
 * Sets a filter to be used for deciding if a row should be visible or not.
 * Whether this filter applies to directories can be toggled with
 * _gtk_file_system_model_set_filter_folders().
 **/
void
_gtk_file_system_model_set_filter (GtkFileSystemModel      *model,
                                   GtkFileFilter *          filter)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (filter == NULL || GTK_IS_FILE_FILTER (filter));

  g_set_object (&model->filter, filter);

  gtk_file_system_model_refilter_all (model);
}

/**
 * _gtk_file_system_model_add_and_query_file:
 * @model: a `GtkFileSystemModel`
 * @file: the file to add
 * @attributes: attributes to query before adding the file
 *
 * This is a convenience function that calls g_file_query_info_async() on
 * the given file, and when successful, adds it to the model.
 * Upon failure, the @file is discarded.
 **/
void
_gtk_file_system_model_add_and_query_file (GtkFileSystemModel *model,
                                           GFile *             file,
                                           const char *        attributes)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (attributes != NULL);

  g_file_query_info_async (file,
                           attributes,
                           G_FILE_QUERY_INFO_NONE,
                           IO_PRIORITY,
                           model->cancellable,
                           gtk_file_system_model_query_done,
                           model);
}

static void
gtk_file_system_model_one_query_done (GObject *     object,
                                      GAsyncResult *res,
                                      gpointer      data)
{
  query_done_helper (object, res, data, TRUE);
}

void
_gtk_file_system_model_add_and_query_files (GtkFileSystemModel *model,
                                            GList              *list,
                                            const char         *attributes)
{
  GList *l;
  GFile *file;

  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));
  g_return_if_fail (attributes != NULL);

  for (l = list; l; l = l->next)
    {
      file = (GFile *)l->data;
      freeze_updates (model);
      g_file_query_info_async (file,
                               attributes,
                               G_FILE_QUERY_INFO_NONE,
                               IO_PRIORITY,
                               model->cancellable,
                               gtk_file_system_model_one_query_done,
                               model);
    }
}

GFile *
_gtk_file_system_model_get_directory (GtkFileSystemModel *model)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model), NULL);

  return model->dir;
}

void
_gtk_file_system_model_set_can_select_files (GtkFileSystemModel *model,
                                             gboolean            can_select)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_MODEL (model));

  model->can_select_files = can_select;
}
