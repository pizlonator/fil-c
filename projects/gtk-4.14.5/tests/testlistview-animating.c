#include <gtk/gtk.h>

#ifdef SMALL
#define AVERAGE 15
#define VARIANCE 10
#else
#define AVERAGE 300
#define VARIANCE 200
#endif

static void
setup_list_item (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *label = gtk_label_new ("");

  gtk_list_item_set_child (list_item, label);
}

static void
bind_list_item (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *label;
  gpointer item;
  char *s;

  item = gtk_list_item_get_item (list_item);

  if (item)
    s = g_strdup_printf ("%u: %s",
                         gtk_list_item_get_position (list_item),
                         (const char *) g_object_get_data (item, "message"));
  else
    s = NULL;

  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), s);

  g_free (s);
}

static GtkWidget *
create_widget_for_listbox (gpointer item,
                           gpointer unused)
{
  const char *message = g_object_get_data (item, "message");
  GtkWidget *widget;

  widget = gtk_label_new (message);

  return widget;
}

static guint
get_number (GObject *item)
{
  return GPOINTER_TO_UINT (g_object_get_data (item, "counter")) % 1000;
}

static void
add (GListStore *store)
{
  static guint counter;
  GObject *o;
  char *message;
  guint pos;

  counter++;
  o = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data (o, "counter", GUINT_TO_POINTER (counter));
  message = g_strdup_printf ("Item %u", counter);
  g_object_set_data_full (o, "message", message, g_free);

  pos = g_random_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
  g_list_store_insert (store, pos, o);
  g_object_unref (o);
}

static void
delete (GListStore *store)
{
  guint pos;

  pos = g_random_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)));
  g_list_store_remove (store, pos);
}

static gboolean
do_stuff (gpointer store)
{
  if (g_random_int_range (AVERAGE - VARIANCE, AVERAGE + VARIANCE) < g_list_model_get_n_items (store))
    delete (store);
  else
    add (store);

  return G_SOURCE_CONTINUE;
}

static gboolean
revert_sort (gpointer sorter)
{
  if (gtk_numeric_sorter_get_sort_order (sorter) == GTK_SORT_ASCENDING)
    gtk_numeric_sorter_set_sort_order (sorter, GTK_SORT_DESCENDING);
  else
    gtk_numeric_sorter_set_sort_order (sorter, GTK_SORT_ASCENDING);

  return G_SOURCE_CONTINUE;
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *win, *hbox, *vbox, *sw, *listview, *listbox, *label;
  GListStore *store;
  GListModel *toplevels;
  GtkSortListModel *sort;
  GtkSorter *sorter;
  guint i;
  GtkListItemFactory *factory;
  GtkSelectionModel *selection;

  gtk_init ();

  store = g_list_store_new (G_TYPE_OBJECT);
  for (i = 0; i < AVERAGE; i++)
    add (store);
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL)));
  sort = gtk_sort_list_model_new (G_LIST_MODEL (store), sorter);

  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_window_set_child (GTK_WINDOW (win), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append (GTK_BOX (hbox), vbox);

  label = gtk_label_new ("GtkListView");
  gtk_box_append (GTK_BOX (vbox), label);

  sw = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (vbox), sw);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_list_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_list_item), NULL);
  listview = gtk_list_view_new (NULL, factory);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append (GTK_BOX (hbox), vbox);

  label = gtk_label_new ("GtkListBox");
  gtk_box_append (GTK_BOX (vbox), label);

  sw = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (vbox), sw);

  listbox = gtk_list_box_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listbox);

  selection = GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (sort)));
  gtk_list_view_set_model (GTK_LIST_VIEW (listview), selection);
  g_object_unref (selection);
  gtk_list_box_bind_model (GTK_LIST_BOX (listbox),
                           G_LIST_MODEL (sort),
                           create_widget_for_listbox,
                           NULL, NULL);

  g_timeout_add (100, do_stuff, store);
  g_timeout_add_seconds (3, revert_sort, sorter);

  gtk_window_present (GTK_WINDOW (win));

  toplevels = gtk_window_get_toplevels ();
  while (g_list_model_get_n_items (toplevels))
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
