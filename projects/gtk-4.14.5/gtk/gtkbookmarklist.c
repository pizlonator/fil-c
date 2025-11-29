/*
 * Copyright © 2020 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkbookmarklist.h"

#include "gtksettings.h"
#include "gtkprivate.h"

/**
 * GtkBookmarkList:
 *
 * `GtkBookmarkList` is a list model that wraps `GBookmarkFile`.
 *
 * It presents a `GListModel` and fills it asynchronously with the
 * `GFileInfo`s returned from that function.
 *
 * The `GFileInfo`s in the list have some attributes in the recent
 * namespace added: `recent::private` (boolean) and `recent:applications`
 * (stringv).
 */

enum {
  PROP_0,
  PROP_FILENAME,
  PROP_ATTRIBUTES,
  PROP_IO_PRIORITY,
  PROP_ITEM_TYPE,
  PROP_LOADING,
  PROP_N_ITEMS,

  NUM_PROPERTIES
};

struct _GtkBookmarkList
{
  GObject parent_instance;

  char *attributes;
  char *filename;
  int io_priority;
  int loading;

  GCancellable *cancellable;
  GFileMonitor *monitor;
  GBookmarkFile *file;

  GSequence *items;
};

struct _GtkBookmarkListClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_bookmark_list_get_item_type (GListModel *list)
{
  return G_TYPE_FILE_INFO;
}

static guint
gtk_bookmark_list_get_n_items (GListModel *list)
{
  GtkBookmarkList *self = GTK_BOOKMARK_LIST (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_bookmark_list_get_item (GListModel *list,
                                guint       position)
{
  GtkBookmarkList *self = GTK_BOOKMARK_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_bookmark_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_bookmark_list_get_item_type;
  iface->get_n_items = gtk_bookmark_list_get_n_items;
  iface->get_item = gtk_bookmark_list_get_item;
}

static void     gtk_bookmark_list_start_loading (GtkBookmarkList *self);
static gboolean gtk_bookmark_list_stop_loading  (GtkBookmarkList *self);
static void     bookmark_file_changed (GFileMonitor       *monitor,
                                       GFile              *file,
                                       GFile              *other_file,
                                       GFileMonitorEvent   event,
                                       gpointer            data);
static void gtk_bookmark_list_set_filename (GtkBookmarkList *self,
                                                const char         *filename);

G_DEFINE_TYPE_WITH_CODE (GtkBookmarkList, gtk_bookmark_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_bookmark_list_model_init))

static void
gtk_bookmark_list_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkBookmarkList *self = GTK_BOOKMARK_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      gtk_bookmark_list_set_attributes (self, g_value_get_string (value));
      break;

    case PROP_IO_PRIORITY:
      gtk_bookmark_list_set_io_priority (self, g_value_get_int (value));
      break;

    case PROP_FILENAME:
      gtk_bookmark_list_set_filename (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_bookmark_list_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkBookmarkList *self = GTK_BOOKMARK_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      g_value_set_string (value, self->attributes);
      break;

    case PROP_FILENAME:
      g_value_set_string (value, self->filename);
      break;

    case PROP_IO_PRIORITY:
      g_value_set_int (value, self->io_priority);
      break;

    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, G_TYPE_FILE_INFO);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, gtk_bookmark_list_is_loading (self));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, g_sequence_get_length (self->items));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_bookmark_list_dispose (GObject *object)
{
  GtkBookmarkList *self = GTK_BOOKMARK_LIST (object);

  gtk_bookmark_list_stop_loading (self);

  g_clear_pointer (&self->attributes, g_free);
  g_clear_pointer (&self->filename, g_free);
  g_clear_pointer (&self->items, g_sequence_free);
  g_clear_pointer (&self->file, g_bookmark_file_free);

  g_signal_handlers_disconnect_by_func (self->monitor, G_CALLBACK (bookmark_file_changed), self);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gtk_bookmark_list_parent_class)->dispose (object);
}

static void
gtk_bookmark_list_class_init (GtkBookmarkListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_bookmark_list_set_property;
  gobject_class->get_property = gtk_bookmark_list_get_property;
  gobject_class->dispose = gtk_bookmark_list_dispose;

  /**
   * GtkBookmarkList:filename: (attributes org.gtk.Property.get=gtk_bookmark_list_get_filename)
   *
   * The bookmark file to load.
   */
  properties[PROP_FILENAME] =
      g_param_spec_string ("filename", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GtkBookmarkList:attributes: (attributes org.gtk.Property.get=gtk_bookmark_list_get_attributes org.gtk.Property.set=gtk_bookmark_list_set_attributes)
   *
   * The attributes to query.
   */
  properties[PROP_ATTRIBUTES] =
      g_param_spec_string ("attributes", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBookmarkList:io-priority: (attributes org.gtk.Property.get=gtk_bookmark_list_get_io_priority org.gtk.Property.set=gtk_bookmark_list_set_io_priority)
   *
   * Priority used when loading.
   */
  properties[PROP_IO_PRIORITY] =
      g_param_spec_int ("io-priority", NULL, NULL,
                        -G_MAXINT, G_MAXINT, G_PRIORITY_DEFAULT,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBookmarkList:item-type:
   *
   * The type of items. See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.8
   **/
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_FILE_INFO,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkBookmarkList:loading: (attributes org.gtk.Property.get=gtk_bookmark_list_is_loading)
   *
   * %TRUE if files are being loaded.
   */
  properties[PROP_LOADING] =
      g_param_spec_boolean ("loading", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBookmarkList:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_bookmark_list_init (GtkBookmarkList *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->io_priority = G_PRIORITY_DEFAULT;
  self->file = g_bookmark_file_new ();
}

static gboolean
gtk_bookmark_list_stop_loading (GtkBookmarkList *self)
{
  if (self->cancellable == NULL)
    return FALSE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->loading = 0;

  return TRUE;
}

static void
got_file_info (GObject      *source,
               GAsyncResult *res,
               gpointer      user_data)
{
  GtkBookmarkList *self = user_data;
  GFile *file = G_FILE (source);
  GFileInfo *info;
  GError *error = NULL;

  info = g_file_query_info_finish (file, res, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (info)
    {
      char *uri;
      gboolean is_private;
      char **apps;

      uri = g_file_get_uri (file);
      is_private = g_bookmark_file_get_is_private (self->file, uri, NULL);
      apps = g_bookmark_file_get_applications (self->file, uri, NULL, NULL);

      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
      g_file_info_set_attribute_boolean (info, "recent::private", is_private);
      g_file_info_set_attribute_stringv (info, "recent::applications", apps);

      g_strfreev (apps);

      g_sequence_append (self->items, info);
      g_list_model_items_changed (G_LIST_MODEL (self), g_sequence_get_length (self->items) - 1, 0, 1);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

      g_free (uri);
    }

  self->loading--;

  if (self->loading == 0)
    {
      g_clear_object (&self->cancellable);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
}

static void
gtk_bookmark_list_clear_items (GtkBookmarkList *self)
{
  guint n_items;

  n_items = g_sequence_get_length (self->items);
  if (n_items > 0)
    {
      g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                               g_sequence_get_end_iter (self->items));

      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }
}

static void
gtk_bookmark_list_start_loading (GtkBookmarkList *self)
{
  gboolean was_loading;
  GError *error = NULL;

  was_loading = gtk_bookmark_list_stop_loading (self);
  gtk_bookmark_list_clear_items (self);

  if (g_bookmark_file_load_from_file (self->file, self->filename, &error))
    {
      char **uris;
      gsize len;
      int i;

      uris = g_bookmark_file_get_uris (self->file, &len);
      if (len > 0)
        {
          self->cancellable = g_cancellable_new ();
          self->loading = len;
        }

      for (i = 0; i < len; i++)
        {
          const char *uri = uris[i];
          GFile *file;

          /* add this item */
          file = g_file_new_for_uri (uri);
          g_file_query_info_async (file,
                                   self->attributes,
                                   0,
                                   self->io_priority,
                                   self->cancellable,
                                   got_file_info,
                                   self);
          g_object_unref (file);
        }

      g_strfreev (uris);
    }
  else
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Failed to load %s: %s", self->filename, error->message);
      g_clear_error (&error);
    }

  if (was_loading != (self->cancellable != NULL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
bookmark_file_changed (GFileMonitor      *monitor,
                       GFile             *file,
                       GFile             *other_file,
                       GFileMonitorEvent  event_type,
                       gpointer           data)
{
  GtkBookmarkList *self = data;

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
      gtk_bookmark_list_start_loading (self);
      break;

    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:

    default:
      break;
    }
}

static void
gtk_bookmark_list_set_filename (GtkBookmarkList *self,
                                    const char         *filename)
{
  GFile *file;

  if (filename)
    self->filename = g_strdup (filename);
  else
    self->filename = g_build_filename (g_get_user_data_dir (), "recently-used.xbel", NULL);

  file = g_file_new_for_path (self->filename);
  self->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
  g_signal_connect (self->monitor, "changed",
                    G_CALLBACK (bookmark_file_changed), self);
  g_object_unref (file);

  gtk_bookmark_list_start_loading (self);
}

/**
 * gtk_bookmark_list_get_filename: (attributes org.gtk.Method.get_property=filename)
 * @self: a `GtkBookmarkList`
 *
 * Returns the filename of the bookmark file that
 * this list is loading.
 *
 * Returns: (type filename): the filename of the .xbel file
 */
const char *
gtk_bookmark_list_get_filename (GtkBookmarkList *self)
{
  g_return_val_if_fail (GTK_IS_BOOKMARK_LIST (self), NULL);

  return self->filename;
}

/**
 * gtk_bookmark_list_new:
 * @filename: (type filename) (nullable): The bookmark file to load
 * @attributes: (nullable): The attributes to query
 *
 * Creates a new `GtkBookmarkList` with the given @attributes.
 *
 * Returns: a new `GtkBookmarkList`
 */
GtkBookmarkList *
gtk_bookmark_list_new (const char *filename,
                       const char *attributes)
{
  return g_object_new (GTK_TYPE_BOOKMARK_LIST,
                       "filename", filename,
                       "attributes", attributes,
                       NULL);
}

/**
 * gtk_bookmark_list_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @self: a `GtkBookmarkList`
 * @attributes: (nullable): the attributes to enumerate
 *
 * Sets the @attributes to be enumerated and starts the enumeration.
 *
 * If @attributes is %NULL, no attributes will be queried, but a list
 * of `GFileInfo`s will still be created.
 */
void
gtk_bookmark_list_set_attributes (GtkBookmarkList *self,
                                      const char         *attributes)
{
  g_return_if_fail (GTK_IS_BOOKMARK_LIST (self));

  if (g_strcmp0 (self->attributes, attributes) == 0)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_free (self->attributes);
  self->attributes = g_strdup (attributes);

  gtk_bookmark_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTRIBUTES]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_bookmark_list_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @self: a `GtkBookmarkList`
 *
 * Gets the attributes queried on the children.
 *
 * Returns: (nullable) (transfer none): The queried attributes
 */
const char *
gtk_bookmark_list_get_attributes (GtkBookmarkList *self)
{
  g_return_val_if_fail (GTK_IS_BOOKMARK_LIST (self), NULL);

  return self->attributes;
}

/**
 * gtk_bookmark_list_set_io_priority: (attributes org.gtk.Method.set_property=io-priority)
 * @self: a `GtkBookmarkList`
 * @io_priority: IO priority to use
 *
 * Sets the IO priority to use while loading files.
 *
 * The default IO priority is %G_PRIORITY_DEFAULT.
 */
void
gtk_bookmark_list_set_io_priority (GtkBookmarkList *self,
                                       int                 io_priority)
{
  g_return_if_fail (GTK_IS_BOOKMARK_LIST (self));

  if (self->io_priority == io_priority)
    return;

  self->io_priority = io_priority;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IO_PRIORITY]);
}

/**
 * gtk_bookmark_list_get_io_priority: (attributes org.gtk.Method.get_property=io-priority)
 * @self: a `GtkBookmarkList`
 *
 * Gets the IO priority to use while loading file.
 *
 * Returns: The IO priority.
 */
int
gtk_bookmark_list_get_io_priority (GtkBookmarkList *self)
{
  g_return_val_if_fail (GTK_IS_BOOKMARK_LIST (self), G_PRIORITY_DEFAULT);

  return self->io_priority;
}

/**
 * gtk_bookmark_list_is_loading: (attributes org.gtk.Method.get_property=loading)
 * @self: a `GtkBookmarkList`
 *
 * Returns %TRUE if the files are currently being loaded.
 *
 * Files will be added to @self from time to time while loading is
 * going on. The order in which are added is undefined and may change
 * in between runs.
 *
 * Returns: %TRUE if @self is loading
 */
gboolean
gtk_bookmark_list_is_loading (GtkBookmarkList *self)
{
  g_return_val_if_fail (GTK_IS_BOOKMARK_LIST (self), FALSE);

  return self->cancellable != NULL;
}
