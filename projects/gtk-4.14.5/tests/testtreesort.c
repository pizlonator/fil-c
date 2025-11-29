/* testtreesort.c
 * Copyright (C) 2001 Red Hat, Inc
 * Author: Jonathan Blandford
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
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct _ListSort ListSort;
struct _ListSort
{
  const char *word_1;
  const char *word_2;
  const char *word_3;
  const char *word_4;
  int number_1;
};

static ListSort data[] =
{
  { "Apples", "Transmogrify long word to demonstrate weirdness", "Exculpatory", "Gesundheit", 30 },
  { "Oranges", "Wicker", "Adamantine", "Convivial", 10 },
  { "Bovine Spongiform Encephilopathy", "Sleazebucket", "Mountaineer", "Pander", 40 },
  { "Foot and Mouth", "Lampshade", "Skim Milk\nFull Milk", "Viewless", 20 },
  { "Blood,\nsweat,\ntears", "The Man", "Horses", "Muckety-Muck", 435 },
  { "Rare Steak", "Siam", "Watchdog", "Xantippe" , 99999 },
  { "SIGINT", "Rabbit Breath", "Alligator", "Bloodstained", 4123 },
  { "Google", "Chrysanthemums", "Hobnob", "Leapfrog", 1 },
  { "Technology fibre optic", "Turtle", "Academe", "Lonely", 3 },
  { "Freon", "Harpes", "Quidditch", "Reagan", 6},
  { "Transposition", "Fruit Basket", "Monkey Wort", "Glogg", 54 },
  { "Fern", "Glasnost and Perestroika", "Latitude", "Bomberman!!!", 2 },
  {NULL, }
};

static ListSort childdata[] =
{
  { "Heineken", "Nederland", "Wanda de vis", "Electronische post", 2},
  { "Hottentottententententoonstelling", "Rotterdam", "Ionentransport", "Palm", 45},
  { "Fruitvlieg", "Eigenfrequentie", "Supernoodles", "Ramen", 2002},
  { "Gereedschapskist", "Stelsel van lineaire vergelijkingen", "Tulpen", "Badlaken", 1311},
  { "Stereoinstallatie", "Rood tapijt", "Het periodieke systeem der elementen", "Laaste woord", 200},
  {NULL, }
};
  

enum
{
  WORD_COLUMN_1 = 0,
  WORD_COLUMN_2,
  WORD_COLUMN_3,
  WORD_COLUMN_4,
  NUMBER_COLUMN_1,
  NUM_COLUMNS
};

static void
switch_search_method (GtkWidget *button,
		      gpointer   tree_view)
{
  if (!gtk_tree_view_get_search_entry (GTK_TREE_VIEW (tree_view)))
    {
      gpointer search_entry = g_object_get_data (tree_view, "my-search-entry");
      gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), GTK_EDITABLE (search_entry));
    }
  else
    gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), NULL);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeStore *model;
  GtkTreeModel *smodel = NULL;
  GtkTreeModel *ssmodel = NULL;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  int i;

  GtkWidget *entry, *button;
  GtkWidget *window2, *vbox2, *scrolled_window2, *tree_view2;
  GtkWidget *window3, *vbox3, *scrolled_window3, *tree_view3;
  gboolean done = FALSE;

  gtk_init ();

  /**
   * First window - Just a GtkTreeStore
   */

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Words, words, words - Window 1");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Jonathan and Kristian's list of cool words. (And Anders' cool list of numbers) \n\nThis is just a GtkTreeStore"));
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  entry = gtk_entry_new ();
  gtk_box_append (GTK_BOX (vbox), entry);

  button = gtk_button_new_with_label ("Switch search method");
  gtk_box_append (GTK_BOX (vbox), button);

  scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window), TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_box_append (GTK_BOX (vbox), scrolled_window);

  model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

/*
  smodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (model));
  ssmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (smodel));
*/
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), GTK_EDITABLE (entry));
  g_object_set_data (G_OBJECT (tree_view), "my-search-entry", entry);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (switch_search_method), tree_view);

 /* gtk_tree_selection_set_select_function (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), select_func, NULL, NULL);*/

  /* 12 iters now, 12 later... */
  for (i = 0; data[i].word_1 != NULL; i++)
    {
      int k;
      GtkTreeIter child_iter;


      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  NUMBER_COLUMN_1, data[i].number_1,
			  -1);

      gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
      gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  NUMBER_COLUMN_1, data[i].number_1,
			  -1);

      for (k = 0; childdata[k].word_1 != NULL; k++)
	{
	  gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
	  gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			      WORD_COLUMN_1, childdata[k].word_1,
			      WORD_COLUMN_2, childdata[k].word_2,
			      WORD_COLUMN_3, childdata[k].word_3,
			      WORD_COLUMN_4, childdata[k].word_4,
			      NUMBER_COLUMN_1, childdata[k].number_1,
			      -1);

	}

    }
  
  smodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (model));
  ssmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (smodel));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
						     "text", WORD_COLUMN_1,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
						     "text", WORD_COLUMN_2,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
						     "text", WORD_COLUMN_3,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
						     "text", WORD_COLUMN_4,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("First Number", renderer,
						     "text", NUMBER_COLUMN_1,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, NUMBER_COLUMN_1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /*  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (smodel),
					WORD_COLUMN_1,
					GTK_SORT_ASCENDING);*/

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_window_present (GTK_WINDOW (window));

  /**
   * Second window - GtkTreeModelSort wrapping the GtkTreeStore
   */

  if (smodel)
    {
      window2 = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window2), 
			    "Words, words, words - window 2");
      g_signal_connect (window2, "destroy", G_CALLBACK (quit_cb), &done);
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (vbox2), 
			  gtk_label_new ("Jonathan and Kristian's list of words.\n\nA GtkTreeModelSort wrapping the GtkTreeStore of window 1"));
      gtk_window_set_child (GTK_WINDOW (window2), vbox2);
      
      scrolled_window2 = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window2), TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window2),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand (scrolled_window2, TRUE);
      gtk_box_append (GTK_BOX (vbox2), scrolled_window2);


      tree_view2 = gtk_tree_view_new_with_model (smodel);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
							 "text", WORD_COLUMN_1,
							 NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
							 "text", WORD_COLUMN_2,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
							 "text", WORD_COLUMN_3,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
							 "text", WORD_COLUMN_4,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      /*      gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (smodel),
					       (GtkTreeIterCompareFunc)gtk_tree_data_list_compare_func,
					       NULL, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (smodel),
					    WORD_COLUMN_1,
					    GTK_SORT_DESCENDING);*/

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window2), tree_view2);
      gtk_window_set_default_size (GTK_WINDOW (window2), 400, 400);
      gtk_window_present (GTK_WINDOW (window2));
    }
  
  /**
   * Third window - GtkTreeModelSort wrapping the GtkTreeModelSort which
   * is wrapping the GtkTreeStore.
   */
  
  if (ssmodel)
    {
      window3 = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window3), 
			    "Words, words, words - Window 3");
      g_signal_connect (window3, "destroy", G_CALLBACK (quit_cb), &done);
      vbox3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (vbox3), 
			  gtk_label_new ("Jonathan and Kristian's list of words.\n\nA GtkTreeModelSort wrapping the GtkTreeModelSort of window 2"));
      gtk_window_set_child (GTK_WINDOW (window3), vbox3);
      
      scrolled_window3 = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled_window3), TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window3),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand (scrolled_window3, TRUE);
      gtk_box_append (GTK_BOX (vbox3), scrolled_window3);


      tree_view3 = gtk_tree_view_new_with_model (ssmodel);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
							 "text", WORD_COLUMN_1,
							 NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
							 "text", WORD_COLUMN_2,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
							 "text", WORD_COLUMN_3,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
							 "text", WORD_COLUMN_4,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      /*      gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ssmodel),
					       (GtkTreeIterCompareFunc)gtk_tree_data_list_compare_func,
					       NULL, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ssmodel),
					    WORD_COLUMN_1,
					    GTK_SORT_ASCENDING);*/

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window3), tree_view3);
      gtk_window_set_default_size (GTK_WINDOW (window3), 400, 400);
      gtk_window_present (GTK_WINDOW (window3));
    }

  for (i = 0; data[i].word_1 != NULL; i++)
    {
      int k;
      
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  -1);
      for (k = 0; childdata[k].word_1 != NULL; k++)
	{
	  GtkTreeIter child_iter;
	  
	  gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
	  gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			      WORD_COLUMN_1, childdata[k].word_1,
			      WORD_COLUMN_2, childdata[k].word_2,
			      WORD_COLUMN_3, childdata[k].word_3,
			      WORD_COLUMN_4, childdata[k].word_4,
			      -1);
	}
    }

  while (!done)
    g_main_context_iteration (NULL, TRUE);
  
  return 0;
}
