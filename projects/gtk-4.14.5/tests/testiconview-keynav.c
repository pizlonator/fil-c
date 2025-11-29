/* testiconview-keynav.c
 * Copyright (C) 2010  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

/*
 * This example demonstrates how to use the keynav-failed signal to
 * extend arrow keynav over adjacent icon views. This can be used when
 * grouping items.
 */

#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkTreeModel *
get_model (void)
{
  static GtkListStore *store;
  GtkTreeIter iter;

  if (store)
    return (GtkTreeModel *) g_object_ref (store);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "One", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Two", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Three", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Four", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Five", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Six", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Seven", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Eight", -1);

  return (GtkTreeModel *) store;
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      data)
{
  gboolean first = GPOINTER_TO_INT (data);
  gboolean visible;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (model, iter);

  if (gtk_tree_path_get_indices (path)[0] < 4)
    visible = first;
  else
    visible = !first;

  gtk_tree_path_free (path);

  return visible;
}

static GtkTreeModel *
get_filter_model (gboolean first)
{
  GtkTreeModelFilter *model;

  model = (GtkTreeModelFilter *)gtk_tree_model_filter_new (get_model (), NULL);

  gtk_tree_model_filter_set_visible_func (model, visible_func, GINT_TO_POINTER (first), NULL);

  return (GtkTreeModel *) model;
}

static GtkWidget *
get_view (gboolean first)
{
  GtkWidget *view;

  view = gtk_icon_view_new_with_model (get_filter_model (first));
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (view), 0);
  gtk_widget_set_size_request (view, 0, -1);

  return view;
}

typedef struct
{
  GtkWidget *header1;
  GtkWidget *view1;
  GtkWidget *header2;
  GtkWidget *view2;
} Views;

static gboolean
keynav_failed (GtkWidget        *view,
               GtkDirectionType  direction,
               Views            *views)
{
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeIter iter;
  int col;
  GtkTreePath *sel;

  if (view == views->view1 && direction == GTK_DIR_DOWN)
    {
      if (gtk_icon_view_get_cursor (GTK_ICON_VIEW (views->view1), &path, NULL))
        {
          col = gtk_icon_view_get_item_column (GTK_ICON_VIEW (views->view1), path);
          gtk_tree_path_free (path);

          sel = NULL;
          model = gtk_icon_view_get_model (GTK_ICON_VIEW (views->view2));
          gtk_tree_model_get_iter_first (model, &iter);
          do {
            path = gtk_tree_model_get_path (model, &iter);
            if (gtk_icon_view_get_item_column (GTK_ICON_VIEW (views->view2), path) == col)
              {
                sel = path;
                break;
              }
          } while (gtk_tree_model_iter_next (model, &iter));

          gtk_icon_view_set_cursor (GTK_ICON_VIEW (views->view2), sel, NULL, FALSE);
          gtk_tree_path_free (sel);
        }
      gtk_widget_grab_focus (views->view2);
      return TRUE;
    }

  if (view == views->view2 && direction == GTK_DIR_UP)
    {
      if (gtk_icon_view_get_cursor (GTK_ICON_VIEW (views->view2), &path, NULL))
        {
          col = gtk_icon_view_get_item_column (GTK_ICON_VIEW (views->view2), path);
          gtk_tree_path_free (path);

          sel = NULL;
          model = gtk_icon_view_get_model (GTK_ICON_VIEW (views->view1));
          gtk_tree_model_get_iter_first (model, &iter);
          do {
            path = gtk_tree_model_get_path (model, &iter);
            if (gtk_icon_view_get_item_column (GTK_ICON_VIEW (views->view1), path) == col)
              {
                if (sel)
                  gtk_tree_path_free (sel);
                sel = path;
              }
            else
              gtk_tree_path_free (path);
          } while (gtk_tree_model_iter_next (model, &iter));

          gtk_icon_view_set_cursor (GTK_ICON_VIEW (views->view1), sel, NULL, FALSE);
          gtk_tree_path_free (sel);
        }
      gtk_widget_grab_focus (views->view1);
      return TRUE;
    }

  return FALSE;
}

static void
focus_changed (GtkWidget  *view,
               GParamSpec *pspec,
                gpointer   data)
{
  if (gtk_widget_has_focus (view))
    {
      GtkTreePath *path;

      if (!gtk_icon_view_get_cursor (GTK_ICON_VIEW (view), &path, NULL))
        {
          path = gtk_tree_path_new_from_indices (0, -1);
          gtk_icon_view_set_cursor (GTK_ICON_VIEW (view), path, NULL, FALSE);
        }

      gtk_icon_view_select_path (GTK_ICON_VIEW (view), path);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_icon_view_unselect_all (GTK_ICON_VIEW (view));
    }
}

#define CSS \
  "GtkWindow {\n" \
  "  background-color: @base_color;\n" \
  "}\n"

static void
set_styles (void)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();

  gtk_css_provider_load_from_data (provider, CSS, -1);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  Views views;

  gtk_init ();

  set_styles ();

  window = gtk_window_new ();
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  views.header1 = g_object_new (GTK_TYPE_LABEL,
                                "label", "<b>Group 1</b>",
                                "use-markup", TRUE,
                                "xalign", 0.0,
                                NULL);
  views.view1 = get_view (TRUE);
  views.header2 = g_object_new (GTK_TYPE_LABEL,
                                "label", "<b>Group 2</b>",
                                "use-markup", TRUE,
                                "xalign", 0.0,
                                NULL);
  views.view2 = get_view (FALSE);

  g_signal_connect (views.view1, "keynav-failed", G_CALLBACK (keynav_failed), &views);
  g_signal_connect (views.view2, "keynav-failed", G_CALLBACK (keynav_failed), &views);
  g_signal_connect (views.view1, "notify::has-focus", G_CALLBACK (focus_changed), &views);
  g_signal_connect (views.view2, "notify::has-focus", G_CALLBACK (focus_changed), &views);

  gtk_box_append (GTK_BOX (vbox), views.header1);
  gtk_box_append (GTK_BOX (vbox), views.view1);
  gtk_box_append (GTK_BOX (vbox), views.header2);
  gtk_box_append (GTK_BOX (vbox), views.view2);

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

