/* Main wrapper for TreeModel test suite.
 * Copyright (C) 2011  Kristian Rietveld  <kris@gtk.org>
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

#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

void register_list_store_tests (void);
void register_tree_store_tests (void);
void register_sort_model_tests (void);
void register_filter_model_tests (void);
void register_model_ref_count_tests (void);

/*
 * Signal monitor
 */
typedef struct _SignalMonitor           SignalMonitor;
typedef enum _SignalName                SignalName;

enum _SignalName
{
  ROW_INSERTED,
  ROW_DELETED,
  ROW_CHANGED,
  ROW_HAS_CHILD_TOGGLED,
  ROWS_REORDERED,
  LAST_SIGNAL
};


SignalMonitor *signal_monitor_new                     (GtkTreeModel  *client);
void           signal_monitor_free                    (SignalMonitor *m);

void           signal_monitor_assert_is_empty         (SignalMonitor *m);

void           signal_monitor_append_signal_reordered (SignalMonitor *m,
                                                       SignalName     signal,
                                                       GtkTreePath   *path,
                                                       int           *new_order,
                                                       int            len);
void           signal_monitor_append_signal_path      (SignalMonitor *m,
                                                       SignalName     signal,
                                                       GtkTreePath   *path);
void           signal_monitor_append_signal           (SignalMonitor *m,
                                                       SignalName     signal,
                                                       const char    *path_string);
