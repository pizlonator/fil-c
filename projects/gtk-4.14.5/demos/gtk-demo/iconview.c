/* Icon View/Icon View Basics
 *
 * The GtkIconView widget is used to display and manipulate icons.
 * It uses a GtkTreeModel for data storage, so the list store
 * example might be helpful.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkWidget *window = NULL;

#define FOLDER_NAME "/iconview/gnome-fs-directory.png"
#define FILE_NAME "/iconview/gnome-fs-regular.png"

enum
{
  COL_PATH,
  COL_DISPLAY_NAME,
  COL_PIXBUF,
  COL_IS_DIRECTORY,
  NUM_COLS
};


static GdkPixbuf *file_pixbuf, *folder_pixbuf;
char *parent;
GtkWidget *up_button;

/* Loads the images for the demo and returns whether the operation succeeded */
static void
load_pixbufs (void)
{
  if (file_pixbuf)
    return; /* already loaded earlier */

  file_pixbuf = gdk_pixbuf_new_from_resource (FILE_NAME, NULL);
  /* resources must load successfully */
  g_assert (file_pixbuf);

  folder_pixbuf = gdk_pixbuf_new_from_resource (FOLDER_NAME, NULL);
  g_assert (folder_pixbuf);
}

static void
fill_store (GtkListStore *store)
{
  GDir *dir;
  const char *name;
  GtkTreeIter iter;

  /* First clear the store */
  gtk_list_store_clear (store);

  /* Now go through the directory and extract all the file
   * information */
  dir = g_dir_open (parent, 0, NULL);
  if (!dir)
    return;

  name = g_dir_read_name (dir);
  while (name != NULL)
    {
      char *path, *display_name;
      gboolean is_dir;

      /* We ignore hidden files that start with a '.' */
      if (name[0] != '.')
        {
          path = g_build_filename (parent, name, NULL);

          is_dir = g_file_test (path, G_FILE_TEST_IS_DIR);

          display_name = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              COL_PATH, path,
                              COL_DISPLAY_NAME, display_name,
                              COL_IS_DIRECTORY, is_dir,
                              COL_PIXBUF, is_dir ? folder_pixbuf : file_pixbuf,
                              -1);
          g_free (path);
          g_free (display_name);
        }

      name = g_dir_read_name (dir);
    }
  g_dir_close (dir);
}

static int
sort_func (GtkTreeModel *model,
           GtkTreeIter  *a,
           GtkTreeIter  *b,
           gpointer      user_data)
{
  gboolean is_dir_a, is_dir_b;
  char *name_a, *name_b;
  int ret;

  /* We need this function because we want to sort
   * folders before files.
   */


  gtk_tree_model_get (model, a,
                      COL_IS_DIRECTORY, &is_dir_a,
                      COL_DISPLAY_NAME, &name_a,
                      -1);

  gtk_tree_model_get (model, b,
                      COL_IS_DIRECTORY, &is_dir_b,
                      COL_DISPLAY_NAME, &name_b,
                      -1);

  if (!is_dir_a && is_dir_b)
    ret = 1;
  else if (is_dir_a && !is_dir_b)
    ret = -1;
  else
    {
      ret = g_utf8_collate (name_a, name_b);
    }

  g_free (name_a);
  g_free (name_b);

  return ret;
}

static GtkListStore *
create_store (void)
{
  GtkListStore *store;

  store = gtk_list_store_new (NUM_COLS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_BOOLEAN);

  /* Set sort column and function */
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                           sort_func,
                                           NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);

  return store;
}

static void
item_activated (GtkIconView *icon_view,
                GtkTreePath *tree_path,
                gpointer     user_data)
{
  GtkListStore *store;
  char *path;
  GtkTreeIter iter;
  gboolean is_dir;

  store = GTK_LIST_STORE (user_data);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (store),
                           &iter, tree_path);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      COL_PATH, &path,
                      COL_IS_DIRECTORY, &is_dir,
                      -1);

  if (!is_dir)
    {
      g_free (path);
      return;
    }

  /* Replace parent with path and re-fill the model*/
  g_free (parent);
  parent = path;

  fill_store (store);

  /* Sensitize the up button */
  gtk_widget_set_sensitive (GTK_WIDGET (up_button), TRUE);
}

static void
up_clicked (GtkButton *item,
            gpointer   user_data)
{
  GtkListStore *store;
  char *dir_name;

  store = GTK_LIST_STORE (user_data);

  dir_name = g_path_get_dirname (parent);
  g_free (parent);

  parent = dir_name;

  fill_store (store);

  /* Maybe de-sensitize the up button */
  gtk_widget_set_sensitive (GTK_WIDGET (up_button),
                            strcmp (parent, "/") != 0);
}

static void
home_clicked (GtkButton *item,
              gpointer   user_data)
{
  GtkListStore *store;

  store = GTK_LIST_STORE (user_data);

  g_free (parent);
  parent = g_strdup (g_get_home_dir ());

  fill_store (store);

  /* Sensitize the up button */
  gtk_widget_set_sensitive (GTK_WIDGET (up_button),
                            TRUE);
}

static void close_window(void)
{
  gtk_window_destroy (GTK_WINDOW (window));
  window = NULL;

  g_object_unref (file_pixbuf);
  file_pixbuf = NULL;

  g_object_unref (folder_pixbuf);
  folder_pixbuf = NULL;
}

GtkWidget *
do_iconview (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *sw;
      GtkWidget *icon_view;
      GtkListStore *store;
      GtkWidget *vbox;
      GtkWidget *tool_bar;
      GtkWidget *home_button;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 650, 400);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Icon View Basics");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (close_window), NULL);

      load_pixbufs ();

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      tool_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (vbox), tool_bar);

      up_button = gtk_button_new_with_mnemonic ("_Up");
      gtk_widget_set_sensitive (GTK_WIDGET (up_button), FALSE);
      gtk_box_append (GTK_BOX (tool_bar), up_button);

      home_button = gtk_button_new_with_mnemonic ("_Home");
      gtk_box_append (GTK_BOX (tool_bar), home_button);


      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand (sw, TRUE);

      gtk_box_append (GTK_BOX (vbox), sw);

      /* Create the store and fill it with the contents of '/' */
      parent = g_strdup ("/");
      store = create_store ();
      fill_store (store);

      icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (store));
      gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (icon_view),
                                        GTK_SELECTION_MULTIPLE);
      g_object_unref (store);

      /* Connect to the "clicked" signal of the "Up" tool button */
      g_signal_connect (up_button, "clicked",
                        G_CALLBACK (up_clicked), store);

      /* Connect to the "clicked" signal of the "Home" tool button */
      g_signal_connect (home_button, "clicked",
                        G_CALLBACK (home_clicked), store);

      /* We now set which model columns that correspond to the text
       * and pixbuf of each item
       */
      gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_view), COL_DISPLAY_NAME);
      gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_view), COL_PIXBUF);

      /* Connect to the "item-activated" signal */
      g_signal_connect (icon_view, "item-activated",
                        G_CALLBACK (item_activated), store);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), icon_view);

      gtk_widget_grab_focus (icon_view);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
