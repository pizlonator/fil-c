#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef GtkListStore MyModel;
typedef GtkListStoreClass MyModelClass;

static void my_model_drag_source_init (GtkTreeDragSourceIface *iface);

static GType my_model_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyModel, my_model, GTK_TYPE_LIST_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                my_model_drag_source_init))

static void
my_model_class_init (MyModelClass *class)
{
}

static void
my_model_init (MyModel *object)
{
  GType types[1] = { G_TYPE_STRING };

  gtk_list_store_set_column_types (GTK_LIST_STORE (object), G_N_ELEMENTS (types), types);
}

static GdkContentProvider *
my_model_drag_data_get (GtkTreeDragSource *source,
                        GtkTreePath       *path)
{
  GdkContentProvider *content;
  GtkTreeIter iter;
  char *text;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (source), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (source), &iter, 0, &text, -1);
  content = gdk_content_provider_new_typed (G_TYPE_STRING, text);
  g_free (text);

  return content;
}

static void
my_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  static GtkTreeDragSourceIface *parent;

  parent = g_type_interface_peek_parent (iface);

  iface->row_draggable = parent->row_draggable;
  iface->drag_data_delete = parent->drag_data_delete;
  iface->drag_data_get = my_model_drag_data_get;
}

static GtkTreeModel *
get_model (void)
{
  MyModel *model;

  model = g_object_new (my_model_get_type (), NULL);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 1", -1);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 2", -1);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 3", -1);

  return GTK_TREE_MODEL (model);
}

static GtkWidget *
get_dragsource (void)
{
  GtkTreeView *tv;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GdkContentFormats *targets;

  tv = (GtkTreeView*) gtk_tree_view_new ();
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Text", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (tv, column);

  gtk_tree_view_set_model (tv, get_model ());
  targets = gdk_content_formats_new_for_gtype (G_TYPE_STRING);
  gtk_tree_view_enable_model_drag_source (tv, GDK_BUTTON1_MASK, targets, GDK_ACTION_COPY);
  gdk_content_formats_unref (targets);

  return GTK_WIDGET (tv);
}

static void
drag_drop (GtkDropTarget *dest,
           const GValue  *value,
           int            x,
           int            y,
           gpointer       dada)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));

  gtk_label_set_label (GTK_LABEL (widget), g_value_get_string (value));
}

static GtkWidget *
get_droptarget (void)
{
  GtkWidget *label;
  GtkDropTarget *dest;

  label = gtk_label_new ("Drop here");
  dest = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (drag_drop), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (dest));

  return label;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;

  gtk_init ();

  window = gtk_window_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), get_dragsource ());
  gtk_box_append (GTK_BOX (box), get_droptarget ());

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
