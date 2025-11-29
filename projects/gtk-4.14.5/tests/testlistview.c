#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define FILE_INFO_TYPE_SELECTION (file_info_selection_get_type ())

G_DECLARE_FINAL_TYPE (FileInfoSelection, file_info_selection, FILE_INFO, SELECTION, GObject)

struct _FileInfoSelection
{
  GObject parent_instance;

  GListModel *model;
};

struct _FileInfoSelectionClass
{
  GObjectClass parent_class;
};

static GType
file_info_selection_get_item_type (GListModel *list)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
file_info_selection_get_n_items (GListModel *list)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
file_info_selection_get_item (GListModel *list,
                               guint       position)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
file_info_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = file_info_selection_get_item_type;
  iface->get_n_items = file_info_selection_get_n_items;
  iface->get_item = file_info_selection_get_item;
}

static gboolean
file_info_selection_is_selected (GtkSelectionModel *model,
                                 guint              position)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (model);
  gpointer item;

  item = g_list_model_get_item (self->model, position);
  if (item == NULL)
    return FALSE;

  if (GTK_IS_TREE_LIST_ROW (item))
    {
      GtkTreeListRow *row = item;
      item = gtk_tree_list_row_get_item (row);
      g_object_unref (row);
    }

  return g_file_info_get_attribute_boolean (item, "filechooser::selected");
}

static void
file_info_selection_set_selected (FileInfoSelection *self,
                                  guint              position,
                                  gboolean           selected)
{
  gpointer item;

  item = g_list_model_get_item (self->model, position);
  if (item == NULL)
    return;

  if (GTK_IS_TREE_LIST_ROW (item))
    {
      GtkTreeListRow *row = item;
      item = gtk_tree_list_row_get_item (row);
      g_object_unref (row);
    }

  g_file_info_set_attribute_boolean (item, "filechooser::selected", selected);
}

static gboolean
file_info_selection_select_item (GtkSelectionModel *model,
                                 guint              position,
                                 gboolean           exclusive)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (model);

  if (exclusive)
    {
      guint i;

      for (i = 0; i < g_list_model_get_n_items (self->model); i++)
        file_info_selection_set_selected (self, i, i == position);

      gtk_selection_model_selection_changed (model, 0, g_list_model_get_n_items (self->model));
    }
  else
    {
      file_info_selection_set_selected (self, position, TRUE);

      gtk_selection_model_selection_changed (model, position, 1);
    }

  return TRUE;
}

static gboolean
file_info_selection_unselect_item (GtkSelectionModel *model,
                                   guint              position)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (model);

  file_info_selection_set_selected (self, position, FALSE);

  gtk_selection_model_selection_changed (model, position, 1);

  return TRUE;
}

static gboolean
file_info_selection_select_range (GtkSelectionModel *model,
                                  guint              position,
                                  guint              n_items,
                                  gboolean           exclusive)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (model);
  guint i;

  if (exclusive)
    for (i = 0; i < position; i++)
      file_info_selection_set_selected (self, i, FALSE);

  for (i = position; i < position + n_items; i++)
    file_info_selection_set_selected (self, i, TRUE);

  if (exclusive)
    for (i = position + n_items; i < g_list_model_get_n_items (self->model); i++)
      file_info_selection_set_selected (self, i, FALSE);

  if (exclusive)
    gtk_selection_model_selection_changed (model, 0, g_list_model_get_n_items (self->model));
  else
    gtk_selection_model_selection_changed (model, position, n_items);

  return TRUE;
}

static gboolean
file_info_selection_unselect_range (GtkSelectionModel *model,
                                    guint              position,
                                    guint              n_items)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (model);
  guint i;

  for (i = position; i < position + n_items; i++)
    file_info_selection_set_selected (self, i, FALSE);

  gtk_selection_model_selection_changed (model, position, n_items);

  return TRUE;
}

static void
file_info_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = file_info_selection_is_selected; 
  iface->select_item = file_info_selection_select_item; 
  iface->unselect_item = file_info_selection_unselect_item; 
  iface->select_range = file_info_selection_select_range; 
  iface->unselect_range = file_info_selection_unselect_range; 
}

G_DEFINE_TYPE_EXTENDED (FileInfoSelection, file_info_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               file_info_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               file_info_selection_selection_model_init))

static void
file_info_selection_items_changed_cb (GListModel        *model,
                                      guint              position,
                                      guint              removed,
                                      guint              added,
                                      FileInfoSelection *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
file_info_selection_clear_model (FileInfoSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, 
                                        file_info_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
file_info_selection_dispose (GObject *object)
{
  FileInfoSelection *self = FILE_INFO_SELECTION (object);

  file_info_selection_clear_model (self);

  G_OBJECT_CLASS (file_info_selection_parent_class)->dispose (object);
}

static void
file_info_selection_class_init (FileInfoSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = file_info_selection_dispose;
}

static void
file_info_selection_init (FileInfoSelection *self)
{
}

static FileInfoSelection *
file_info_selection_new (GListModel *model)
{
  FileInfoSelection *result;

  result = g_object_new (FILE_INFO_TYPE_SELECTION, NULL);

  result->model = g_object_ref (model);
  g_signal_connect (result->model, "items-changed",
                    G_CALLBACK (file_info_selection_items_changed_cb), result);

  return result;
}

/*** ---------------------- ***/

GSList *pending = NULL;
guint active = 0;

static void
loading_cb (GtkDirectoryList *dir,
            GParamSpec       *pspec,
            gpointer          unused)
{
  if (gtk_directory_list_is_loading (dir))
    {
      active++;
      /* HACK: ensure loading finishes and the dir doesn't get destroyed */
      g_object_ref (dir);
    }
  else
    {
      active--;
      g_object_unref (dir);

      while (active < 20 && pending)
        {
          GtkDirectoryList *dir2 = pending->data;
          pending = g_slist_remove (pending, dir2);
          gtk_directory_list_set_file (dir2, g_object_get_data (G_OBJECT (dir2), "file"));
          g_object_unref (dir2);
        }
    }
}

static GtkDirectoryList *
create_directory_list (GFile *file)
{
  GtkDirectoryList *dir;

  dir = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_TYPE
                                "," G_FILE_ATTRIBUTE_STANDARD_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                NULL);
  gtk_directory_list_set_io_priority (dir, G_PRIORITY_DEFAULT_IDLE);
  g_signal_connect (dir, "notify::loading", G_CALLBACK (loading_cb), NULL);
  g_assert (!gtk_directory_list_is_loading (dir));

  if (active > 20)
    {
      g_object_set_data_full (G_OBJECT (dir), "file", g_object_ref (file), g_object_unref);
      pending = g_slist_prepend (pending, g_object_ref (dir));
    }
  else
    {
      gtk_directory_list_set_file (dir, file);
    }

  return dir;
}

static char *
get_file_path (GFileInfo *info)
{
  GFile *file;

  file = G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
  return g_file_get_path (file);
}

static GListModel *
create_list_model_for_directory (gpointer file)
{
  GtkDirectoryList *dir;
  GtkSorter *sorter;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  dir = create_directory_list (file);
  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback) get_file_path, NULL, NULL)));

  return G_LIST_MODEL (gtk_sort_list_model_new (G_LIST_MODEL (dir), sorter));
}

typedef struct _RowData RowData;
struct _RowData
{
  GtkWidget *expander;
  GtkWidget *icon;
  GtkWidget *name;
  GCancellable *cancellable;

  GtkTreeListRow *current_item;
};

static void row_data_notify_item (GtkListItem *item,
                                  GParamSpec  *pspec,
                                  RowData     *data);
static void
row_data_unbind (RowData *data)
{
  if (data->current_item == NULL)
    return;

  if (data->cancellable)
    {
      g_cancellable_cancel (data->cancellable);
      g_clear_object (&data->cancellable);
    }

  g_clear_object (&data->current_item);
}

static void
row_data_update_info (RowData   *data,
                      GFileInfo *info)
{
  GIcon *icon;
  const char *thumbnail_path;

  thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (thumbnail_path)
    {
      /* XXX: not async */
      GFile *thumbnail_file = g_file_new_for_path (thumbnail_path);
      icon = g_file_icon_new (thumbnail_file);
      g_object_unref (thumbnail_file);
    }
  else if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_ICON))
    {
      icon = g_file_info_get_icon (info);
    }
  else
    {
      icon = NULL;
    }

  gtk_widget_set_visible (data->icon, icon != NULL);
  gtk_image_set_from_gicon (GTK_IMAGE (data->icon), icon);
}

static void
copy_attribute (GFileInfo   *to,
                GFileInfo   *from,
                const char *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static void
row_data_got_thumbnail_info_cb (GObject      *source,
                                GAsyncResult *res,
                                gpointer      _data)
{
  RowData *data = _data; /* invalid if operation was cancelled */
  GFile *file = G_FILE (source);
  GFileInfo *queried, *info;

  queried = g_file_query_info_finish (file, res, NULL);
  if (queried == NULL)
    return;

  /* now we know row is valid */

  info = gtk_tree_list_row_get_item (data->current_item);

  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  g_object_unref (queried);

  row_data_update_info (data, info);
  
  g_clear_object (&data->cancellable);
}

static void
row_data_bind (RowData        *data,
               GtkTreeListRow *item)
{
  GFileInfo *info;

  row_data_unbind (data);

  if (item == NULL)
    return;

  data->current_item = g_object_ref (item);

  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (data->expander), item);

  info = gtk_tree_list_row_get_item (item);

  if (!g_file_info_has_attribute (info, "filechooser::queried"))
    {
      data->cancellable = g_cancellable_new ();
      g_file_info_set_attribute_boolean (info, "filechooser::queried", TRUE);
      g_file_query_info_async (G_FILE (g_file_info_get_attribute_object (info, "standard::file")),
                               G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                               G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                               G_FILE_ATTRIBUTE_STANDARD_ICON,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               data->cancellable,
                               row_data_got_thumbnail_info_cb,
                               data);
    }

  row_data_update_info (data, info);

  gtk_label_set_label (GTK_LABEL (data->name), g_file_info_get_display_name (info));

  g_object_unref (info);
}

static void
row_data_notify_item (GtkListItem *item,
                      GParamSpec  *pspec,
                      RowData     *data)
{
  row_data_bind (data, gtk_list_item_get_item (item));
}

static void
row_data_free (gpointer _data)
{
  RowData *data = _data;

  row_data_unbind (data);

  g_free (data);
}

static void
setup_widget (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *box, *child;
  RowData *data;

  data = g_new0 (RowData, 1);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_list_item_set_child (list_item, box);

  child = gtk_label_new (NULL);
  gtk_label_set_width_chars (GTK_LABEL (child), 5);
  gtk_box_append (GTK_BOX (box), child);

  data->expander = gtk_tree_expander_new ();
  gtk_box_append (GTK_BOX (box), data->expander);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (data->expander), box);

  data->icon = gtk_image_new ();
  gtk_box_append (GTK_BOX (box), data->icon);

  data->name = gtk_label_new (NULL);
  gtk_label_set_max_width_chars (GTK_LABEL (data->name), 25);
  gtk_label_set_ellipsize (GTK_LABEL (data->name), PANGO_ELLIPSIZE_END);
  gtk_box_append (GTK_BOX (box), data->name);

  g_signal_connect (list_item, "notify::item", G_CALLBACK (row_data_notify_item), data);
  g_object_set_data_full (G_OBJECT (list_item), "row-data", data, row_data_free);
}

static GListModel *
create_list_model_for_file_info (gpointer file_info,
                                 gpointer unused)
{
  GFile *file = G_FILE (g_file_info_get_attribute_object (file_info, "standard::file"));

  if (file == NULL)
    return NULL;

  return create_list_model_for_directory (file);
}

static gboolean
update_statusbar (GtkStatusbar *statusbar)
{
  GListModel *model = g_object_get_data (G_OBJECT (statusbar), "model");
  GString *string = g_string_new (NULL);
  guint n;
  gboolean result = G_SOURCE_REMOVE;

  gtk_statusbar_remove_all (statusbar, 0);

  n = g_list_model_get_n_items (model);
  g_string_append_printf (string, "%u", n);
  if (GTK_IS_FILTER_LIST_MODEL (model))
    {
      guint n_unfiltered = g_list_model_get_n_items (gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (model)));
      if (n != n_unfiltered)
        g_string_append_printf (string, "/%u", n_unfiltered);
    }
  g_string_append (string, " items");

  if (pending || active)
    {
      g_string_append_printf (string, " (%u directories remaining)", active + g_slist_length (pending));
      result = G_SOURCE_CONTINUE;
    }

  gtk_statusbar_push (statusbar, 0, string->str);
  g_free (string->str);

  return result;
}

static gboolean
match_file (gpointer item, gpointer data)
{
  GtkWidget *search_entry = data;
  GFileInfo *info = gtk_tree_list_row_get_item (item);
  GFile *file = G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
  char *path;
  gboolean result;
  
  path = g_file_get_path (file);

  result = strstr (path, gtk_editable_get_text (GTK_EDITABLE (search_entry))) != NULL;

  g_object_unref (info);
  g_free (path);

  return result;
}

static void
search_changed_cb (GtkSearchEntry *entry,
                   GtkFilter      *custom_filter)
{
  gtk_filter_changed (custom_filter, GTK_FILTER_CHANGE_DIFFERENT);
}

int
main (int argc, char *argv[])
{
  GtkWidget *win, *vbox, *sw, *listview, *search_entry, *statusbar;
  GtkTreeListModel *tree;
  GtkFilterListModel *filter;
  GtkFilter *custom_filter;
  FileInfoSelection *selectionmodel;
  GFile *root;
  GListModel *toplevels;
  GtkListItemFactory *factory;

  gtk_init ();

  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (win), vbox);

  search_entry = gtk_search_entry_new ();
  gtk_box_append (GTK_BOX (vbox), search_entry);

  sw = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (search_entry), sw);
  gtk_box_append (GTK_BOX (vbox), sw);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_widget), NULL);
  listview = gtk_list_view_new (NULL, factory);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);

  if (argc > 1)
    root = g_file_new_for_commandline_arg (argv[1]);
  else
    root = g_file_new_for_path (g_get_current_dir ());
  tree = gtk_tree_list_model_new (create_list_model_for_directory (root),
                                  FALSE,
                                  TRUE,
                                  create_list_model_for_file_info,
                                  NULL, NULL);
  g_object_unref (root);

  custom_filter = GTK_FILTER (gtk_custom_filter_new (match_file, search_entry, NULL));
  filter = gtk_filter_list_model_new (G_LIST_MODEL (tree), custom_filter);
  g_signal_connect (search_entry, "search-changed", G_CALLBACK (search_changed_cb), custom_filter);

  selectionmodel = file_info_selection_new (G_LIST_MODEL (filter));
  g_object_unref (filter);

  gtk_list_view_set_model (GTK_LIST_VIEW (listview), GTK_SELECTION_MODEL (selectionmodel));

  statusbar = gtk_statusbar_new ();
  gtk_widget_add_tick_callback (statusbar, (GtkTickCallback) update_statusbar, NULL, NULL);
  g_object_set_data (G_OBJECT (statusbar), "model", filter);
  g_signal_connect_swapped (filter, "items-changed", G_CALLBACK (update_statusbar), statusbar);
  update_statusbar (GTK_STATUSBAR (statusbar));
  gtk_box_append (GTK_BOX (vbox), statusbar);

  g_object_unref (selectionmodel);

  gtk_window_present (GTK_WINDOW (win));

  toplevels = gtk_window_get_toplevels ();
  while (g_list_model_get_n_items (toplevels))
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
