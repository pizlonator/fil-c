/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2006  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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

static gconstpointer GROUP_A = "GROUP_A";
static gconstpointer GROUP_B = "GROUP_B";

const char *tabs1 [] = {
  "aaaaaaaaaa",
  "bbbbbbbbbb",
  "cccccccccc",
  "dddddddddd",
  NULL
};

const char *tabs2 [] = {
  "1",
  "2",
  "3",
  "4",
  "55555",
  NULL
};

const char *tabs3 [] = {
  "foo",
  "bar",
  NULL
};

const char *tabs4 [] = {
  "beer",
  "water",
  "lemonade",
  "coffee",
  "tea",
  NULL
};

static GtkNotebook*
window_creation_function (GtkNotebook *source_notebook,
                          GtkWidget   *child,
                          int          x,
                          int          y,
                          gpointer     data)
{
  GtkWidget *window, *notebook;

  window = gtk_window_new ();
  notebook = gtk_notebook_new ();
  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook),
                               gtk_notebook_get_group_name (source_notebook));

  gtk_window_set_child (GTK_WINDOW (window), notebook);

  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  gtk_window_present (GTK_WINDOW (window));

  return GTK_NOTEBOOK (notebook);
}

static void
on_page_reordered (GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer data)
{
  g_print ("page %d reordered\n", page_num);
}

static gboolean
remove_in_idle (gpointer data)
{
  GtkNotebookPage *page = data;
  GtkWidget *child = gtk_notebook_page_get_child (page);
  GtkWidget *parent = gtk_widget_get_ancestor (child, GTK_TYPE_NOTEBOOK);
  GtkWidget *tab_label;

  tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (parent), child);
  g_print ("Removing tab: %s\n", gtk_label_get_text (GTK_LABEL (tab_label)));
  gtk_box_remove (GTK_BOX (parent), child);

  return G_SOURCE_REMOVE;
}

static gboolean
on_button_drag_drop (GtkDropTarget *dest,
                     const GValue  *value,
                     double         x,
                     double         y,
                     gpointer       user_data)
{
  GtkNotebookPage *page;

  page = g_value_get_object (value);
  g_idle_add (remove_in_idle, page);

  return TRUE;
}

static void
action_clicked_cb (GtkWidget *button,
                   GtkWidget *notebook)
{
  GtkWidget *page, *title;

  page = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (page), "Addition");

  title = gtk_label_new ("Addition");

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);
}

static GtkWidget*
create_notebook (const char     **labels,
                 const char      *group,
                 GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page, *action_widget;

  notebook = gtk_notebook_new ();
  gtk_widget_set_vexpand (notebook, TRUE);
  gtk_widget_set_hexpand (notebook, TRUE);

  action_widget = gtk_button_new_from_icon_name ("list-add-symbolic");
  g_signal_connect (action_widget, "clicked", G_CALLBACK (action_clicked_cb), notebook);
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), action_widget, GTK_PACK_END);

  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      page = gtk_entry_new ();
      gtk_editable_set_text (GTK_EDITABLE (page), *labels);

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  return notebook;
}

static GtkWidget*
create_notebook_non_dragable_content (const char      **labels,
                                      const char       *group,
                                      GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page, *action_widget;

  notebook = gtk_notebook_new ();
  gtk_widget_set_vexpand (notebook, TRUE);
  gtk_widget_set_hexpand (notebook, TRUE);

  action_widget = gtk_button_new_from_icon_name ("list-add-symbolic");
  g_signal_connect (action_widget, "clicked", G_CALLBACK (action_clicked_cb), notebook);
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), action_widget, GTK_PACK_END);

  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      GtkWidget *button;
      button = gtk_button_new_with_label ("example content");
      /* Use GtkListBox since it bubbles up motion notify event, which can
       * experience more issues than GtkBox. */
      page = gtk_list_box_new ();
      gtk_box_append (GTK_BOX (page), button);

      button = gtk_button_new_with_label ("row 2");
      gtk_box_append (GTK_BOX (page), button);

      button = gtk_button_new_with_label ("third row");
      gtk_box_append (GTK_BOX (page), button);

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  return notebook;
}

static GtkWidget*
create_notebook_with_notebooks (const char      **labels,
                                const char       *group,
                                GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page;

  notebook = gtk_notebook_new ();
  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      page = create_notebook (labels, group, pos);
      gtk_notebook_popup_enable (GTK_NOTEBOOK (page));

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  return notebook;
}

static GtkWidget*
create_trash_button (void)
{
  GtkWidget *button;
  GtkDropTarget *dest;

  button = gtk_button_new_with_mnemonic ("_Delete");

  dest = gtk_drop_target_new (GTK_TYPE_NOTEBOOK_PAGE, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drop", G_CALLBACK (on_button_drag_drop), NULL);
  gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (dest));

  return button;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *grid;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  grid = gtk_grid_new ();

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook_non_dragable_content (tabs1, GROUP_A, GTK_POS_TOP),
                   0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook (tabs2, GROUP_B, GTK_POS_BOTTOM),
                   0, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook (tabs3, GROUP_B, GTK_POS_LEFT),
                   1, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook_with_notebooks (tabs4, GROUP_A, GTK_POS_RIGHT),
                   1, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_trash_button (),
                   1, 2, 1, 1);

  gtk_window_set_child (GTK_WINDOW (window), grid);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
