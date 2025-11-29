/* GTK - The GIMP Toolkit
 * gtktrashmonitor.h: Monitor the trash:/// folder to see if there is trash or not
 * Copyright (C) 2011 Suse
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Federico Mena Quintero <federico@gnome.org>
 */

#include "config.h"

#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtktrashmonitorprivate.h"

#define UPDATE_RATE_SECONDS 1

struct _GtkTrashMonitor
{
  GObject parent;

  GFileMonitor *file_monitor;
  gulong file_monitor_changed_id;

  gboolean pending;
  int timeout_id;

  guint has_trash : 1;
};

struct _GtkTrashMonitorClass
{
  GObjectClass parent_class;

  void (* trash_state_changed) (GtkTrashMonitor *monitor);
};

enum {
  TRASH_STATE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkTrashMonitor, _gtk_trash_monitor, G_TYPE_OBJECT)

static GtkTrashMonitor *the_trash_monitor;

#define ICON_NAME_TRASH_EMPTY "user-trash-symbolic"
#define ICON_NAME_TRASH_FULL  "user-trash-full-symbolic"

static void
gtk_trash_monitor_dispose (GObject *object)
{
  GtkTrashMonitor *monitor;

  monitor = GTK_TRASH_MONITOR (object);

  if (monitor->file_monitor)
    {
      g_signal_handler_disconnect (monitor->file_monitor, monitor->file_monitor_changed_id);
      monitor->file_monitor_changed_id = 0;

      g_clear_object (&monitor->file_monitor);
    }

  if (monitor->timeout_id > 0)
    g_source_remove (monitor->timeout_id);
  monitor->timeout_id = 0;

  G_OBJECT_CLASS (_gtk_trash_monitor_parent_class)->dispose (object);
}

static void
_gtk_trash_monitor_class_init (GtkTrashMonitorClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) class;

  gobject_class->dispose = gtk_trash_monitor_dispose;

  signals[TRASH_STATE_CHANGED] =
    g_signal_new (I_("trash-state-changed"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkTrashMonitorClass, trash_state_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

/* Updates the internal has_trash flag and emits the "trash-state-changed" signal */
static void
update_has_trash_and_notify (GtkTrashMonitor *monitor,
                             gboolean has_trash)
{
  if (monitor->has_trash == !!has_trash)
    return;

  monitor->has_trash = !!has_trash;

  g_signal_emit (monitor, signals[TRASH_STATE_CHANGED], 0); 
}

/* Use G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT since we only want to know whether the
 * trash is empty or not, not access its children. This is available for the
 * trash backend since it uses a cache. In this way we prevent flooding the
 * trash backend with enumeration requests when trashing > 1000 files
 */
static void
trash_query_info_cb (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
  GtkTrashMonitor *monitor = GTK_TRASH_MONITOR (user_data);
  GFileInfo *info;
  guint32 item_count;
  gboolean has_trash = FALSE;

  info = g_file_query_info_finish (G_FILE (source), result, NULL);

  if (info != NULL)
    {
      item_count = g_file_info_get_attribute_uint32 (info,
                                                     G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
      has_trash = item_count > 0;

      g_object_unref (info);
    }

  update_has_trash_and_notify (monitor, has_trash);

  g_object_unref (monitor); /* was reffed in recompute_trash_state() */
}

static void recompute_trash_state (GtkTrashMonitor *monitor);

static gboolean
recompute_trash_state_cb (gpointer data)
{
  GtkTrashMonitor *monitor = data;

  monitor->timeout_id = 0;
  if (monitor->pending)
    {
      monitor->pending = FALSE;
      recompute_trash_state (monitor);
    }

  return G_SOURCE_REMOVE;
}

/* Asynchronously recomputes whether there is trash or not */
static void
recompute_trash_state (GtkTrashMonitor *monitor)
{
  GFile *file;

  /* Rate limit the updates to not flood the gvfsd-trash when too many changes
   * happened in a short time.
  */
  if (monitor->timeout_id > 0)
    {
      monitor->pending = TRUE;
      return;
    }

  file = g_file_new_for_uri ("trash:///");
  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT, NULL,
                           trash_query_info_cb, g_object_ref (monitor));

  monitor->timeout_id = g_timeout_add_seconds (UPDATE_RATE_SECONDS,
                                               recompute_trash_state_cb,
                                               monitor);
  gdk_source_set_static_name_by_id (monitor->timeout_id, "[gtk] recompute_trash_state_cb");

  g_object_unref (file);
}

/* Callback used when the "trash:///" file monitor changes; we just recompute the trash state
 * whenever something happens.
 */
static void
file_monitor_changed_cb (GFileMonitor      *file_monitor,
                         GFile             *child,
                         GFile             *other_file,
                         GFileMonitorEvent  event_type,
                         GtkTrashMonitor   *monitor)
{
  recompute_trash_state (monitor);
}

static void
_gtk_trash_monitor_init (GtkTrashMonitor *monitor)
{
  GFile *file;

  file = g_file_new_for_uri ("trash:///");

  monitor->file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
  monitor->pending = FALSE;
  monitor->timeout_id = 0;

  g_object_unref (file);

  if (monitor->file_monitor)
    monitor->file_monitor_changed_id = g_signal_connect (monitor->file_monitor, "changed",
                                                         G_CALLBACK (file_monitor_changed_cb), monitor);

  recompute_trash_state (monitor);
}

/*
 * _gtk_trash_monitor_get:
 *
 * Returns: (transfer full): a new reference to the singleton
 * GtkTrashMonitor object.  Be sure to call g_object_unref() on it when you are
 * done with the trash monitor.
 */
GtkTrashMonitor *
_gtk_trash_monitor_get (void)
{
  if (the_trash_monitor != NULL)
    {
      g_object_ref (the_trash_monitor);
    }
  else
    {
      the_trash_monitor = g_object_new (GTK_TYPE_TRASH_MONITOR, NULL);
      g_object_add_weak_pointer (G_OBJECT (the_trash_monitor), (gpointer *) &the_trash_monitor);
    }

  return the_trash_monitor;
}

/*
 * _gtk_trash_monitor_get_icon:
 * @monitor: a GtkTrashMonitor
 *
 * Returns: (transfer full): the GIcon that should be used to represent
 * the state of the trash folder on screen, based on whether there is trash or
 * not.
 */
GIcon *
_gtk_trash_monitor_get_icon (GtkTrashMonitor *monitor)
{
  const char *icon_name;

  g_return_val_if_fail (GTK_IS_TRASH_MONITOR (monitor), NULL);

  if (monitor->has_trash)
    icon_name = ICON_NAME_TRASH_FULL;
  else
    icon_name = ICON_NAME_TRASH_EMPTY;

  return g_themed_icon_new (icon_name);
}

/*
 * _gtk_trash_monitor_get_has_trash:
 * @monitor: a GtkTrashMonitor
 *
 * Returns: #TRUE if there is trash in the trash:/// folder, or #FALSE otherwise.
 */
gboolean
_gtk_trash_monitor_get_has_trash (GtkTrashMonitor *monitor)
{
  g_return_val_if_fail (GTK_IS_TRASH_MONITOR (monitor), FALSE);

  return monitor->has_trash;
}
