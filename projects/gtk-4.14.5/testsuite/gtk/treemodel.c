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

#include "treemodel.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  register_list_store_tests ();
  register_tree_store_tests ();
  register_model_ref_count_tests ();
  register_sort_model_tests ();
  register_filter_model_tests ();

  return g_test_run ();
}

/*
 * Signal monitor
 */

static const char *
signal_name_to_string (SignalName signal)
{
  switch (signal)
    {
      case ROW_INSERTED:
          return "row-inserted";

      case ROW_DELETED:
          return "row-deleted";

      case ROW_CHANGED:
          return "row-changed";

      case ROW_HAS_CHILD_TOGGLED:
          return "row-has-child-toggled";

      case ROWS_REORDERED:
          return "rows-reordered";

      case LAST_SIGNAL:
      default:
        g_assert_not_reached ();
    }

  return "(unknown)";
}

typedef struct
{
  SignalName signal;
  GtkTreePath *path;

  /* For rows-reordered */
  int *new_order;
  int len;
}
Signal;


static Signal *
signal_new (SignalName signal, GtkTreePath *path)
{
  Signal *s;

  s = g_new0 (Signal, 1);
  s->signal = signal;
  s->path = gtk_tree_path_copy (path);
  s->new_order = NULL;

  return s;
}

static Signal *
signal_new_with_order (SignalName signal, GtkTreePath *path,
                       int *new_order, int len)
{
  Signal *s = signal_new (signal, path);

  s->new_order = new_order;
  s->len = len;

  return s;
}

static void
signal_free (Signal *s)
{
  if (s->path)
    gtk_tree_path_free (s->path);

  g_free (s);
}


struct _SignalMonitor
{
  GQueue *queue;
  GtkTreeModel *client;
  gulong signal_ids[LAST_SIGNAL];
};


static void
signal_monitor_generic_handler (SignalMonitor *m,
                                SignalName     signal,
                                GtkTreeModel  *model,
                                GtkTreeIter   *iter,
                                GtkTreePath   *path,
                                int           *new_order)
{
  Signal *s;

  if (g_queue_is_empty (m->queue))
    {
      char *path_str;

      path_str = gtk_tree_path_to_string (path);
      g_error ("Signal queue empty, got signal %s path %s",
               signal_name_to_string (signal), path_str);
      g_free (path_str);

      g_assert_not_reached ();
    }

  if (m->client != model)
    {
      g_error ("Model mismatch; expected %p, got %p",
               m->client, model);
      g_assert_not_reached ();
    }

  s = g_queue_peek_tail (m->queue);

#if 0
  /* For debugging: output signals that are coming in.  Leaks memory. */
  g_print ("signal=%s path=%s\n", signal_name_to_string (signal),
           gtk_tree_path_to_string (path));
#endif

  if (s->signal != signal ||
      (gtk_tree_path_get_depth (s->path) == 0 &&
       gtk_tree_path_get_depth (path) != 0) ||
      (gtk_tree_path_get_depth (s->path) != 0 &&
       gtk_tree_path_compare (s->path, path) != 0))
    {
      char *path_str, *s_path_str;

      s_path_str = gtk_tree_path_to_string (s->path);
      path_str = gtk_tree_path_to_string (path);

      g_error ("Signals don't match; expected signal %s path %s, got signal %s path %s",
               signal_name_to_string (s->signal), s_path_str,
               signal_name_to_string (signal), path_str);

      g_free (s_path_str);
      g_free (path_str);

      g_assert_not_reached ();
    }

  if (signal == ROWS_REORDERED && s->new_order != NULL)
    {
      int i, len;

      g_assert_nonnull (new_order);

      len = gtk_tree_model_iter_n_children (model, iter);
      g_assert_cmpint (s->len, ==, len);

      for (i = 0; i < len; i++)
        g_assert_cmpint (s->new_order[i], ==, new_order[i]);
    }

  s = g_queue_pop_tail (m->queue);

  signal_free (s);
}

static void
signal_monitor_row_inserted (GtkTreeModel *model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  signal_monitor_generic_handler (data, ROW_INSERTED,
                                  model, iter, path, NULL);
}

static void
signal_monitor_row_deleted (GtkTreeModel *model,
                            GtkTreePath  *path,
                            gpointer      data)
{
  signal_monitor_generic_handler (data, ROW_DELETED,
                                  model, NULL, path, NULL);
}

static void
signal_monitor_row_changed (GtkTreeModel *model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter,
                            gpointer      data)
{
  signal_monitor_generic_handler (data, ROW_CHANGED,
                                  model, iter, path, NULL);
}

static void
signal_monitor_row_has_child_toggled (GtkTreeModel *model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter,
                                      gpointer      data)
{
  signal_monitor_generic_handler (data, ROW_HAS_CHILD_TOGGLED,
                                  model, iter, path, NULL);
}

static void
signal_monitor_rows_reordered (GtkTreeModel *model,
                               GtkTreePath  *path,
                               GtkTreeIter  *iter,
                               int          *new_order,
                               gpointer      data)
{
  signal_monitor_generic_handler (data, ROWS_REORDERED,
                                  model, iter, path, new_order);
}

SignalMonitor *
signal_monitor_new (GtkTreeModel *client)
{
  SignalMonitor *m;

  m = g_new0 (SignalMonitor, 1);
  m->client = g_object_ref (client);
  m->queue = g_queue_new ();

  m->signal_ids[ROW_INSERTED] = g_signal_connect (client,
                                                  "row-inserted",
                                                  G_CALLBACK (signal_monitor_row_inserted),
                                                  m);
  m->signal_ids[ROW_DELETED] = g_signal_connect (client,
                                                 "row-deleted",
                                                 G_CALLBACK (signal_monitor_row_deleted),
                                                 m);
  m->signal_ids[ROW_CHANGED] = g_signal_connect (client,
                                                 "row-changed",
                                                 G_CALLBACK (signal_monitor_row_changed),
                                                 m);
  m->signal_ids[ROW_HAS_CHILD_TOGGLED] = g_signal_connect (client,
                                                           "row-has-child-toggled",
                                                           G_CALLBACK (signal_monitor_row_has_child_toggled),
                                                           m);
  m->signal_ids[ROWS_REORDERED] = g_signal_connect (client,
                                                    "rows-reordered",
                                                    G_CALLBACK (signal_monitor_rows_reordered),
                                                    m);

  return m;
}

void
signal_monitor_free (SignalMonitor *m)
{
  int i;

  for (i = 0; i < LAST_SIGNAL; i++)
    g_signal_handler_disconnect (m->client, m->signal_ids[i]);

  g_object_unref (m->client);

  if (m->queue)
    g_queue_free (m->queue);

  g_free (m);
}

void
signal_monitor_assert_is_empty (SignalMonitor *m)
{
  g_assert_true (g_queue_is_empty (m->queue));
}

void
signal_monitor_append_signal_path (SignalMonitor *m,
                                   SignalName     signal,
                                   GtkTreePath   *path)
{
  Signal *s;

  s = signal_new (signal, path);
  g_queue_push_head (m->queue, s);
}

void
signal_monitor_append_signal_reordered (SignalMonitor *m,
                                        SignalName     signal,
                                        GtkTreePath   *path,
                                        int           *new_order,
                                        int            len)
{
  Signal *s;

  s = signal_new_with_order (signal, path, new_order, len);
  g_queue_push_head (m->queue, s);
}

void
signal_monitor_append_signal (SignalMonitor *m,
                              SignalName     signal,
                              const char    *path_string)
{
  Signal *s;
  GtkTreePath *path;

  path = gtk_tree_path_new_from_string (path_string);

  s = signal_new (signal, path);
  g_queue_push_head (m->queue, s);

  gtk_tree_path_free (path);
}
