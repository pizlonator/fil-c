/* testentrycompletion.c
 * Copyright (C) 2004  Red Hat, Inc.
 * Author: Matthias Clasen
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
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* Don't copy this bad example; inline RGB data is always a better
 * idea than inline XPMs.
 */
static const char *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "
};

static GtkWidget *window = NULL;


/* Creates a tree model containing the completions */
static GtkTreeModel *
create_simple_completion_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  
  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "GNOME", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "gnominious", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Gnomonic projection", -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "total", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totally", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "toto", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "tottery", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totterer", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Totten trust", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totipotent", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totipotency", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totemism", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totem pole", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Totara", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totalizer", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totalizator", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totalitarianism", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "total parenteral nutrition", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "total hysterectomy", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "total eclipse", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Totipresence", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "Totipalmi", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "zombie", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "a\303\246x", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "a\303\246y", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "a\303\246z", -1);
 
  return GTK_TREE_MODEL (store);
}

/* Creates a tree model containing the completions */
static GtkTreeModel *
create_completion_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)book_closed_xpm);

  store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "ambient", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "ambidextrously", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "ambidexter", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "ambiguity", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "American Party", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "American mountain ash", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "amelioration", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "Amelia Earhart", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "Totten trust", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, pixbuf, 1, "Laminated arch", -1);
 
  return GTK_TREE_MODEL (store);
}

static gboolean
match_func (GtkEntryCompletion *completion,
	    const char         *key,
	    GtkTreeIter        *iter,
	    gpointer            user_data)
{
  char *item = NULL;
  GtkTreeModel *model;

  gboolean ret = FALSE;

  model = gtk_entry_completion_get_model (completion);

  gtk_tree_model_get (model, iter, 1, &item, -1);

  if (item != NULL)
    {
      g_print ("compare %s %s\n", key, item);
      if (strncmp (key, item, strlen (key)) == 0)
	ret = TRUE;

      g_free (item);
    }

  return ret;
}

static int timer_count = 0;

static const char *dynamic_completions[] = {
  "GNOME",
  "gnominious",
  "Gnomonic projection",
  "total",
  "totally",
  "toto",
  "tottery",
  "totterer",
  "Totten trust",
  "totipotent",
  "totipotency",
  "totemism",
  "totem pole",
  "Totara",
  "totalizer",
  "totalizator",
  "totalitarianism",
  "total parenteral nutrition",
  "total hysterectomy",
  "total eclipse",
  "Totipresence",
  "Totipalmi",
  "zombie"
};

static int
animation_timer (GtkEntryCompletion *completion)
{
  GtkTreeIter iter;
  int n_completions = G_N_ELEMENTS (dynamic_completions);
  int n;
  static GtkListStore *old_store = NULL;
  GtkListStore *store = GTK_LIST_STORE (gtk_entry_completion_get_model (completion));

  if (timer_count % 10 == 0)
    {
      if (!old_store)
	{
	  g_print ("removing model!\n");

	  old_store = g_object_ref (store);
	  gtk_entry_completion_set_model (completion, NULL);
	}
      else
	{
	  g_print ("readding model!\n");
	  
	  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (old_store));
	  g_object_unref (old_store);
	  old_store = NULL;
	}

      timer_count ++;
      return TRUE;
    }

  if (!old_store)
    {
      if ((timer_count / n_completions) % 2 == 0)
	{
	  n = timer_count % n_completions;
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter, 0, dynamic_completions[n], -1);
	  
	}
      else
	{
	  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
	    gtk_list_store_remove (store, &iter);
	}
    }
  
  timer_count++;
  return TRUE;
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel       *model,
		   GtkTreeIter        *iter)
{
  char *str;
  GtkWidget *entry;

  entry = gtk_entry_completion_get_entry (completion);
  gtk_tree_model_get (GTK_TREE_MODEL (model), iter, 1, &str, -1);
  gtk_editable_set_text (GTK_EDITABLE (entry), str);
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_free (str);

  return TRUE;
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
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryCompletion *completion;
  GtkTreeModel *completion_model;
  GtkCellRenderer *cell;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_start (vbox, 5);
  gtk_widget_set_margin_end (vbox, 5);
  gtk_widget_set_margin_top (vbox, 5);
  gtk_widget_set_margin_bottom (vbox, 5);
  gtk_window_set_child (GTK_WINDOW (window), vbox);
    
  label = gtk_label_new (NULL);

  gtk_label_set_markup (GTK_LABEL (label), "Completion demo, try writing <b>total</b> or <b>gnome</b> for example.");
  gtk_box_append (GTK_BOX (vbox), label);

  /* Create our first entry */
  entry = gtk_entry_new ();
  
  /* Create the completion object */
  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  
  /* Assign the completion to the entry */
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);
  
  gtk_box_append (GTK_BOX (vbox), entry);

  /* Create a tree model and use it as the completion model */
  completion_model = create_simple_completion_model ();
  gtk_entry_completion_set_model (completion, completion_model);
  g_object_unref (completion_model);
  
  /* Use model column 0 as the text column */
  gtk_entry_completion_set_text_column (completion, 0);

  /* Create our second entry */
  entry = gtk_entry_new ();

  /* Create the completion object */
  completion = gtk_entry_completion_new ();
  
  /* Assign the completion to the entry */
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);
  
  gtk_box_append (GTK_BOX (vbox), entry);

  /* Create a tree model and use it as the completion model */
  completion_model = create_completion_model ();
  gtk_entry_completion_set_model (completion, completion_model);
  gtk_entry_completion_set_minimum_key_length (completion, 2);
  g_object_unref (completion_model);
  
  /* Use model column 1 as the text column */
  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), cell, 
				  "pixbuf", 0, NULL); 

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), cell, 
				  "text", 1, NULL); 
  
  gtk_entry_completion_set_match_func (completion, match_func, NULL, NULL);
  g_signal_connect (completion, "match-selected", 
		    G_CALLBACK (match_selected_cb), NULL);

  /* Create our third entry */
  entry = gtk_entry_new ();

  /* Create the completion object */
  completion = gtk_entry_completion_new ();
  
  /* Assign the completion to the entry */
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);
  
  gtk_box_append (GTK_BOX (vbox), entry);

  /* Create a tree model and use it as the completion model */
  completion_model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

  gtk_entry_completion_set_model (completion, completion_model);
  g_object_unref (completion_model);

  /* Use model column 0 as the text column */
  gtk_entry_completion_set_text_column (completion, 0);

  /* Fill the completion dynamically */
  g_timeout_add (1000, (GSourceFunc) animation_timer, completion);

  /* Fourth entry */
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Model-less entry completion"));

  entry = gtk_entry_new ();

  /* Create the completion object */
  completion = gtk_entry_completion_new ();

  /* Assign the completion to the entry */
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  gtk_box_append (GTK_BOX (vbox), entry);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}


