/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkdirectorylist.h"

#include "gtkprivate.h"

/**
 * GtkDirectoryList:
 *
 * `GtkDirectoryList` is a list model that wraps g_file_enumerate_children_async().
 *
 * It presents a `GListModel` and fills it asynchronously with the `GFileInfo`s
 * returned from that function.
 *
 * Enumeration will start automatically when the
 * [property@Gtk.DirectoryList:file] property is set.
 *
 * While the `GtkDirectoryList` is being filled, the
 * [property@Gtk.DirectoryList:loading] property will be set to %TRUE. You can
 * listen to that property if you want to show information like a `GtkSpinner`
 * or a "Loading..." text.
 *
 * If loading fails at any point, the [property@Gtk.DirectoryList:error]
 * property will be set to give more indication about the failure.
 *
 * The `GFileInfo`s returned from a `GtkDirectoryList` have the "standard::file"
 * attribute set to the `GFile` they refer to. This way you can get at the file
 * that is referred to in the same way you would via g_file_enumerator_get_child().
 * This means you do not need access to the `GtkDirectoryList`, but can access
 * the `GFile` directly from the `GFileInfo` when operating with a `GtkListView`
 * or similar.
 */

/* random number that everyone else seems to use, too */
#define FILES_PER_QUERY 100

enum {
  PROP_0,
  PROP_ATTRIBUTES,
  PROP_ERROR,
  PROP_FILE,
  PROP_IO_PRIORITY,
  PROP_ITEM_TYPE,
  PROP_LOADING,
  PROP_MONITORED,
  PROP_N_ITEMS,

  NUM_PROPERTIES
};

typedef struct _QueuedEvent QueuedEvent;
struct _QueuedEvent
{
  GtkDirectoryList *list;
  GFile *file;
  GFileInfo *info;
  GFileMonitorEvent event;
};

static void
free_queued_event (gpointer data)
{
  QueuedEvent *event = data;

  g_clear_object (&event->file);
  g_clear_object (&event->info);
  g_free (event);
}

struct _GtkDirectoryList
{
  GObject parent_instance;

  char *attributes;
  GFile *file;
  GFileMonitor *monitor;
  gboolean monitored;
  int io_priority;

  GCancellable *cancellable;
  GError *error; /* Error while loading */
  GSequence *items; /* Use GPtrArray or GListStore here? */
  GQueue events;
};

struct _GtkDirectoryListClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_directory_list_get_item_type (GListModel *list)
{
  return G_TYPE_FILE_INFO;
}

static guint
gtk_directory_list_get_n_items (GListModel *list)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_directory_list_get_item (GListModel *list,
                             guint       position)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_directory_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_directory_list_get_item_type;
  iface->get_n_items = gtk_directory_list_get_n_items;
  iface->get_item = gtk_directory_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkDirectoryList, gtk_directory_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_directory_list_model_init))

static void
gtk_directory_list_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      gtk_directory_list_set_attributes (self, g_value_get_string (value));
      break;
    case PROP_FILE:
      gtk_directory_list_set_file (self, g_value_get_object (value));
      break;

    case PROP_IO_PRIORITY:
      gtk_directory_list_set_io_priority (self, g_value_get_int (value));
      break;

    case PROP_MONITORED:
      gtk_directory_list_set_monitored (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_directory_list_get_property (GObject     *object,
                                 guint        prop_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      g_value_set_string (value, self->attributes);
      break;

    case PROP_ERROR:
      g_value_set_boxed (value, self->error);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_IO_PRIORITY:
      g_value_set_int (value, self->io_priority);
      break;

    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, G_TYPE_FILE_INFO);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, gtk_directory_list_is_loading (self));
      break;

    case PROP_MONITORED:
      g_value_set_boolean (value, gtk_directory_list_get_monitored (self));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, g_sequence_get_length (self->items));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_directory_list_stop_loading (GtkDirectoryList *self)
{
  if (self->cancellable == NULL)
    return FALSE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  return TRUE;
}

static void directory_changed (GFileMonitor       *monitor,
                               GFile              *file,
                               GFile              *other_file,
                               GFileMonitorEvent   event,
                               gpointer            data);

static void
gtk_directory_list_stop_monitoring (GtkDirectoryList *self)
{
  if (self->monitor)
    g_signal_handlers_disconnect_by_func (self->monitor, directory_changed, self);
  g_clear_object (&self->monitor);
}

static void
gtk_directory_list_dispose (GObject *object)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (object);

  gtk_directory_list_stop_loading (self);
  gtk_directory_list_stop_monitoring (self);

  g_clear_object (&self->file);
  g_clear_pointer (&self->attributes, g_free);

  g_clear_error (&self->error);
  g_clear_pointer (&self->items, g_sequence_free);

  g_queue_foreach (&self->events, (GFunc) free_queued_event, NULL);
  g_queue_clear (&self->events);

  G_OBJECT_CLASS (gtk_directory_list_parent_class)->dispose (object);
}

static void
gtk_directory_list_class_init (GtkDirectoryListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_directory_list_set_property;
  gobject_class->get_property = gtk_directory_list_get_property;
  gobject_class->dispose = gtk_directory_list_dispose;

  /**
   * GtkDirectoryList:attributes: (attributes org.gtk.Property.get=gtk_directory_list_get_attributes org.gtk.Property.set=gtk_directory_list_set_attributes)
   *
   * The attributes to query.
   */
  properties[PROP_ATTRIBUTES] =
      g_param_spec_string ("attributes", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:error: (attributes org.gtk.Property.get=gtk_directory_list_get_error)
   *
   * Error encountered while loading files.
   */
  properties[PROP_ERROR] =
      g_param_spec_boxed ("error", NULL, NULL,
                          G_TYPE_ERROR,
                          GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:file: (attributes org.gtk.Property.get=gtk_directory_list_get_file org.gtk.Property.set=gtk_directory_list_set_file)
   *
   * File to query.
   */
  properties[PROP_FILE] =
      g_param_spec_object ("file", NULL, NULL,
                           G_TYPE_FILE,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:io-priority: (attributes org.gtk.Property.get=gtk_directory_list_get_io_priority org.gtk.Property.set=gtk_directory_list_set_io_priority)
   *
   * Priority used when loading.
   */
  properties[PROP_IO_PRIORITY] =
      g_param_spec_int ("io-priority", NULL, NULL,
                        -G_MAXINT, G_MAXINT, G_PRIORITY_DEFAULT,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:item-type:
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
   * GtkDirectoryList:loading: (attributes org.gtk.Property.get=gtk_directory_list_is_loading)
   *
   * %TRUE if files are being loaded.
   */
  properties[PROP_LOADING] =
      g_param_spec_boolean ("loading", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:monitored: (attributes org.gtk.Property.get=gtk_directory_list_get_monitored org.gtk.Property.set=gtk_directory_list_set_monitored)
   *
   * %TRUE if the directory is monitored for changed.
   */
  properties[PROP_MONITORED] =
      g_param_spec_boolean ("monitored", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDirectoryList:n-items:
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
gtk_directory_list_init (GtkDirectoryList *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->io_priority = G_PRIORITY_DEFAULT;
  self->monitored = TRUE;
  g_queue_init (&self->events);
}

/**
 * gtk_directory_list_new:
 * @file: (nullable): The file to query
 * @attributes: (nullable): The attributes to query with
 *
 * Creates a new `GtkDirectoryList`.
 *
 * The `GtkDirectoryList` is querying the given @file
 * with the given @attributes.
 *
 * Returns: a new `GtkDirectoryList`
 **/
GtkDirectoryList *
gtk_directory_list_new (const char *attributes,
                        GFile      *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (GTK_TYPE_DIRECTORY_LIST,
                       "attributes", attributes,
                       "file", file,
                       NULL);
}

static void
gtk_directory_list_clear_items (GtkDirectoryList *self)
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

  if (self->error)
    {
      g_clear_error (&self->error);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);
    }
}

static void
gtk_directory_list_enumerator_closed_cb (GObject      *source,
                                         GAsyncResult *res,
                                         gpointer      user_data)
{
  g_file_enumerator_close_finish (G_FILE_ENUMERATOR (source), res, NULL);
}

static void
gtk_directory_list_got_files_cb (GObject      *source,
                                 GAsyncResult *res,
                                 gpointer      user_data)
{
  GtkDirectoryList *self = user_data; /* invalid if cancelled */
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source);
  GError *error = NULL;
  GList *l, *files;
  guint n;

  files = g_file_enumerator_next_files_finish (enumerator, res, &error);

  if (files == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          return;
        }

      g_file_enumerator_close_async (enumerator,
                                     self->io_priority,
                                     NULL,
                                     gtk_directory_list_enumerator_closed_cb,
                                     NULL);

      g_object_freeze_notify (G_OBJECT (self));

      g_clear_object (&self->cancellable);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

      if (error)
        {
          self->error = error;
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);
        }

      g_object_thaw_notify (G_OBJECT (self));
      return;
    }

  n = 0;
  for (l = files; l; l = l->next)
    {
      GFileInfo *info;
      GFile *file;

      info = l->data;
      file = g_file_enumerator_get_child (enumerator, info);
      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
      g_object_unref (file);
      g_sequence_append (self->items, info);
      n++;
    }
  g_list_free (files);

  g_file_enumerator_next_files_async (enumerator,
                                      g_file_is_native (self->file) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
                                      self->io_priority,
                                      self->cancellable,
                                      gtk_directory_list_got_files_cb,
                                      self);

  if (n > 0)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), g_sequence_get_length (self->items) - n, 0, n);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }
}

static void
gtk_directory_list_got_enumerator_cb (GObject      *source,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  GtkDirectoryList *self = user_data; /* invalid if cancelled */
  GFile *file = G_FILE (source);
  GFileEnumerator *enumerator;
  GError *error = NULL;

  enumerator = g_file_enumerate_children_finish (file, res, &error);
  if (enumerator == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          return;
        }

      g_object_freeze_notify (G_OBJECT (self));
      self->error = error;
      g_clear_object (&self->cancellable);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ERROR]);
      g_object_thaw_notify (G_OBJECT (self));
      return;
    }

  g_file_enumerator_next_files_async (enumerator,
                                      g_file_is_native (file) ? 50 * FILES_PER_QUERY : FILES_PER_QUERY,
                                      self->io_priority,
                                      self->cancellable,
                                      gtk_directory_list_got_files_cb,
                                      self);
  g_object_unref (enumerator);
}

static void
gtk_directory_list_start_loading (GtkDirectoryList *self)
{
  gboolean was_loading;
  char *glib_apis_suck;

  was_loading = gtk_directory_list_stop_loading (self);
  gtk_directory_list_clear_items (self);

  if (self->file == NULL)
    {
      if (was_loading)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
      return;
    }

  glib_apis_suck = g_strconcat ("standard::name,", self->attributes, NULL);
  self->cancellable = g_cancellable_new ();
  g_file_enumerate_children_async (self->file,
                                   glib_apis_suck,
                                   G_FILE_QUERY_INFO_NONE,
                                   self->io_priority,
                                   self->cancellable,
                                   gtk_directory_list_got_enumerator_cb,
                                   self);
  g_free (glib_apis_suck);

  if (!was_loading)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static GSequenceIter *
find_file (GSequence *sequence,
           GFile     *file)
{
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (sequence);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GFileInfo *item = G_FILE_INFO (g_sequence_get (iter));
      GFile *f = G_FILE (g_file_info_get_attribute_object (item, "standard::file"));

      if (g_file_equal (f, file))
        return iter;
    }

  return NULL;
}

static gboolean
handle_event (QueuedEvent *event)
{
  GtkDirectoryList *self = event->list;
  GFile *file = event->file;
  GFileInfo *info = event->info;
  GSequenceIter *iter;
  unsigned int position;

  switch ((int)event->event)
    {
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_CREATED:
      if (!info)
        return FALSE;

      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));

      iter = find_file (self->items, file);
      if (iter)
        {
          position = g_sequence_iter_get_position (iter);
          g_sequence_set (iter, g_object_ref (info));
          g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 1);
        }
      else
        {
          position = g_sequence_get_length (self->items);
          g_sequence_append (self->items, g_object_ref (info));
          g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
        }
      break;

    case G_FILE_MONITOR_EVENT_MOVED_OUT:
    case G_FILE_MONITOR_EVENT_DELETED:
      iter = find_file (self->items, file);
      if (iter)
        {
          position = g_sequence_iter_get_position (iter);
          g_sequence_remove (iter);
          g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
        }
      break;

    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      if (!info)
        return FALSE;

      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));

      iter = find_file (self->items, file);
      if (iter)
        {
          position = g_sequence_iter_get_position (iter);
          g_sequence_set (iter, g_object_ref (info));
          g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 1);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
handle_events (GtkDirectoryList *self)
{
  QueuedEvent *event;

  do
    {
      event = g_queue_peek_tail (&self->events);
      if (!event)
        return;

      if (!handle_event (event))
        return;

      event = g_queue_pop_tail (&self->events);
      free_queued_event (event);
    }
  while (TRUE);
}

static void
got_new_file_info_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      data)
{
  QueuedEvent *event = data;
  GtkDirectoryList *self = event->list;
  GFile *file = event->file;

  event->info = g_file_query_info_finish (file, res, NULL);
  handle_events (self);
}

static void
got_existing_file_info_cb (GObject      *source,
                           GAsyncResult *res,
                           gpointer      data)
{
  QueuedEvent *event = data;
  GtkDirectoryList *self = event->list;
  GFile *file = event->file;

  event->info = g_file_query_info_finish (file, res, NULL);
  handle_events (self);
}

static void
directory_changed (GFileMonitor       *monitor,
                   GFile              *file,
                   GFile              *other_file,
                   GFileMonitorEvent   event,
                   gpointer            data)
{
  GtkDirectoryList *self = GTK_DIRECTORY_LIST (data);
  QueuedEvent *ev;

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_CREATED:
      ev = g_new0 (QueuedEvent, 1);
      ev->list = self;
      ev->event = event;
      ev->file = g_object_ref (file);
      g_queue_push_head (&self->events, ev);

      g_file_query_info_async (file,
                               self->attributes,
                               G_FILE_QUERY_INFO_NONE,
                               self->io_priority,
                               self->cancellable,
                               got_new_file_info_cb,
                               ev);
      break;

    case G_FILE_MONITOR_EVENT_MOVED_OUT:
    case G_FILE_MONITOR_EVENT_DELETED:
      ev = g_new0 (QueuedEvent, 1);
      ev->list = self;
      ev->event = event;
      ev->file = g_object_ref (file);
      g_queue_push_head (&self->events, ev);

      handle_events (self);
      break;

    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      ev = g_new0 (QueuedEvent, 1);
      ev->list = self;
      ev->event = event;
      ev->file = g_object_ref (file);
      g_queue_push_head (&self->events, ev);

      g_file_query_info_async (file,
                               self->attributes,
                               G_FILE_QUERY_INFO_NONE,
                               self->io_priority,
                               self->cancellable,
                               got_existing_file_info_cb,
                               ev);
      break;

    case G_FILE_MONITOR_EVENT_RENAMED:
      ev = g_new0 (QueuedEvent, 1);
      ev->list = self;
      ev->event = G_FILE_MONITOR_EVENT_DELETED;
      ev->file = g_object_ref (file);
      g_queue_push_head (&self->events, ev);

      ev = g_new0 (QueuedEvent, 1);
      ev->list = self;
      ev->event = G_FILE_MONITOR_EVENT_CREATED;
      ev->file = g_object_ref (other_file);
      g_queue_push_head (&self->events, ev);

      g_file_query_info_async (other_file,
                               self->attributes,
                               G_FILE_QUERY_INFO_NONE,
                               self->io_priority,
                               self->cancellable,
                               got_existing_file_info_cb,
                               ev);
      break;

    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_MOVED:
    default:
      break;
    }
}

static void
gtk_directory_list_start_monitoring (GtkDirectoryList *self)
{
  g_assert (self->monitor == NULL);
  self->monitor = g_file_monitor_directory (self->file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
  g_signal_connect (self->monitor, "changed", G_CALLBACK (directory_changed), self);
}

static void
gtk_directory_list_update_monitoring (GtkDirectoryList *self)
{
  gtk_directory_list_stop_monitoring (self);
  if (self->file && self->monitored)
    gtk_directory_list_start_monitoring (self);
}

/**
 * gtk_directory_list_set_file: (attributes org.gtk.Method.set_property=file)
 * @self: a `GtkDirectoryList`
 * @file: (nullable): the `GFile` to be enumerated
 *
 * Sets the @file to be enumerated and starts the enumeration.
 *
 * If @file is %NULL, the result will be an empty list.
 */
void
gtk_directory_list_set_file (GtkDirectoryList *self,
                             GFile            *file)
{
  g_return_if_fail (GTK_IS_DIRECTORY_LIST (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (self->file == file ||
      (self->file && file && g_file_equal (self->file, file)))
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_set_object (&self->file, file);

  gtk_directory_list_update_monitoring (self);
  gtk_directory_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_directory_list_get_file: (attributes org.gtk.Method.get_property=file)
 * @self: a `GtkDirectoryList`
 *
 * Gets the file whose children are currently enumerated.
 *
 * Returns: (nullable) (transfer none): The file whose children are enumerated
 **/
GFile *
gtk_directory_list_get_file (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), NULL);

  return self->file;
}

/**
 * gtk_directory_list_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @self: a `GtkDirectoryList`
 * @attributes: (nullable): the attributes to enumerate
 *
 * Sets the @attributes to be enumerated and starts the enumeration.
 *
 * If @attributes is %NULL, the list of file infos will still be created, it will just
 * not contain any extra attributes.
 */
void
gtk_directory_list_set_attributes (GtkDirectoryList *self,
                                   const char       *attributes)
{
  g_return_if_fail (GTK_IS_DIRECTORY_LIST (self));

  if (self->attributes == attributes)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_free (self->attributes);
  self->attributes = g_strdup (attributes);

  gtk_directory_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTRIBUTES]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_directory_list_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @self: a `GtkDirectoryList`
 *
 * Gets the attributes queried on the children.
 *
 * Returns: (nullable) (transfer none): The queried attributes
 */
const char *
gtk_directory_list_get_attributes (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), NULL);

  return self->attributes;
}

/**
 * gtk_directory_list_set_io_priority: (attributes org.gtk.Method.set_property=io-priority)
 * @self: a `GtkDirectoryList`
 * @io_priority: IO priority to use
 *
 * Sets the IO priority to use while loading directories.
 *
 * Setting the priority while @self is loading will reprioritize the
 * ongoing load as soon as possible.
 *
 * The default IO priority is %G_PRIORITY_DEFAULT, which is higher than
 * the GTK redraw priority. If you are loading a lot of directories in
 * parallel, lowering it to something like %G_PRIORITY_DEFAULT_IDLE
 * may increase responsiveness.
 */
void
gtk_directory_list_set_io_priority (GtkDirectoryList *self,
                                    int               io_priority)
{
  g_return_if_fail (GTK_IS_DIRECTORY_LIST (self));

  if (self->io_priority == io_priority)
    return;

  self->io_priority = io_priority;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IO_PRIORITY]);
}

/**
 * gtk_directory_list_get_io_priority: (attributes org.gtk.Method.get_property=io-priority)
 * @self: a `GtkDirectoryList`
 *
 * Gets the IO priority set via gtk_directory_list_set_io_priority().
 *
 * Returns: The IO priority.
 */
int
gtk_directory_list_get_io_priority (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), G_PRIORITY_DEFAULT);

  return self->io_priority;
}

/**
 * gtk_directory_list_is_loading: (attributes org.gtk.Method.get_property=loading)
 * @self: a `GtkDirectoryList`
 *
 * Returns %TRUE if the children enumeration is currently in
 * progress.
 *
 * Files will be added to @self from time to time while loading is
 * going on. The order in which are added is undefined and may change
 * in between runs.
 *
 * Returns: %TRUE if @self is loading
 */
gboolean
gtk_directory_list_is_loading (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), FALSE);

  return self->cancellable != NULL;
}

/**
 * gtk_directory_list_get_error: (attributes org.gtk.Method.get_property=error)
 * @self: a `GtkDirectoryList`
 *
 * Gets the loading error, if any.
 *
 * If an error occurs during the loading process, the loading process
 * will finish and this property allows querying the error that happened.
 * This error will persist until a file is loaded again.
 *
 * An error being set does not mean that no files were loaded, and all
 * successfully queried files will remain in the list.
 *
 * Returns: (nullable) (transfer none): The loading error or %NULL if
 *   loading finished successfully
 */
const GError *
gtk_directory_list_get_error (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), FALSE);

  return self->error;
}

/**
 * gtk_directory_list_set_monitored: (attributes org.gtk.Method.set_property=monitored)
 * @self: a `GtkDirectoryList`
 * @monitored: %TRUE to monitor the directory for changes
 *
 * Sets whether the directory list will monitor the directory
 * for changes.
 *
 * If monitoring is enabled, the ::items-changed signal will
 * be emitted when the directory contents change.
 *
 *
 * When monitoring is turned on after the initial creation
 * of the directory list, the directory is reloaded to avoid
 * missing files that appeared between the initial loading
 * and when monitoring was turned on.
 */
void
gtk_directory_list_set_monitored (GtkDirectoryList *self,
                                  gboolean          monitored)
{
  g_return_if_fail (GTK_IS_DIRECTORY_LIST (self));

  if (self->monitored == monitored)
    return;

  self->monitored = monitored;

  gtk_directory_list_update_monitoring (self);
  if (monitored)
    gtk_directory_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MONITORED]);
}

/**
 * gtk_directory_list_get_monitored: (attributes org.gtk.Method.get_property=monitored)
 * @self: a `GtkDirectoryList`
 *
 * Returns whether the directory list is monitoring
 * the directory for changes.
 *
 * Returns: %TRUE if the directory is monitored
 */
gboolean
gtk_directory_list_get_monitored (GtkDirectoryList *self)
{
  g_return_val_if_fail (GTK_IS_DIRECTORY_LIST (self), TRUE);

  return self->monitored;
}
