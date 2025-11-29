#include <gtk/gtk.h>

#define TEST_TYPE_OBJECT (test_object_get_type ())
G_DECLARE_FINAL_TYPE (TestObject, test_object, TEST, OBJECT, GObject)

struct _TestObject {
  GObject parent_instance;
  char *string;
  guint number;
  gboolean allow_children;
};

enum {
  PROP_STRING = 1,
  PROP_NUMBER,
  PROP_ALLOW_CHILDREN,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT);

static void
test_object_init (TestObject *obj)
{
}

static void
test_object_finalize (GObject *object)
{
  TestObject *obj = TEST_OBJECT (object);

  g_free (obj->string);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  TestObject *obj = TEST_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (obj->string);
      obj->string = g_value_dup_string (value);
      break;

    case PROP_NUMBER:
      obj->number = g_value_get_uint (value);
      break;

    case PROP_ALLOW_CHILDREN:
      obj->allow_children = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
test_object_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TestObject *obj = TEST_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, obj->string);
      break;

    case PROP_NUMBER:
      g_value_set_uint (value, obj->number);
      break;

    case PROP_ALLOW_CHILDREN:
      g_value_set_boolean (value, obj->allow_children);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
test_object_class_init (TestObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = test_object_finalize;
  object_class->set_property = test_object_set_property;
  object_class->get_property = test_object_get_property;

  g_object_class_install_property (object_class, PROP_STRING,
      g_param_spec_string ("string", "String", "String",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_NUMBER,
      g_param_spec_uint ("number", "Number", "Number",
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ALLOW_CHILDREN,
      g_param_spec_boolean ("allow-children", "Allow children", "Allow children",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static TestObject *
test_object_new (const char *string,
                 guint       number,
                 gboolean    allow_children)
{
  return g_object_new (TEST_TYPE_OBJECT,
                       "string", string,
                       "number", number,
                       "allow-children", allow_children,
                       NULL);
}

static const char *
test_object_get_string (TestObject *obj)
{
  return obj->string;
}

static guint
test_object_get_number (TestObject *obj)
{
  return obj->number;
}

static gboolean
test_object_get_allow_children (TestObject *obj)
{
  return obj->allow_children;
}

/* * * */

static GListModel *
create_model (guint base,
              guint n,
              guint increment,
              gboolean allow_children)
{
  GListStore *store;
  guint i;

  store = g_list_store_new (TEST_TYPE_OBJECT);
  for (i = 0; i < n; i++)
    {
      char *string;
      guint number;
      TestObject *obj;

      number = base + i * increment;
      string = g_strdup_printf ("%u", number);
      obj = test_object_new (string, number, allow_children);
      g_list_store_append (store, obj);
      g_object_unref (obj);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static GListModel *
create_child_model (gpointer item,
                    gpointer user_data)
{
  guint size = GPOINTER_TO_UINT (user_data);
  guint base = test_object_get_number (TEST_OBJECT (item));

  if (test_object_get_allow_children (TEST_OBJECT (item)))
    return create_model (base, size, 1, FALSE);
  else
    return NULL;
}

static GListModel *
create_tree_model (guint n, guint m)
{
  return G_LIST_MODEL (gtk_tree_list_model_new (create_model (0, n, m, TRUE),
                                                FALSE,
                                                FALSE,
                                                create_child_model,
                                                GUINT_TO_POINTER (m), NULL));
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *entry;

  entry = gtk_editable_label_new ("");
  gtk_editable_set_width_chars (GTK_EDITABLE (entry), 3);
  gtk_list_item_set_child (item, entry);
}

static void
text_changed (GObject    *object,
              GParamSpec *pspec,
              gpointer    data)
{
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (object));
g_print ("text changed to '%s'\n", text);
  g_object_set (data, "string", text, NULL);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  TestObject *obj;
  GtkWidget *entry;

  obj = gtk_list_item_get_item (item);
  entry = gtk_list_item_get_child (item);
  gtk_editable_set_text (GTK_EDITABLE (entry), test_object_get_string (obj));
  g_signal_connect (entry, "notify::text", G_CALLBACK (text_changed), obj);
}

static void
unbind_item (GtkSignalListItemFactory *factory,
             GtkListItem              *item)
{
  TestObject *obj;
  GtkWidget *entry;

  obj = gtk_list_item_get_item (item);
  entry = gtk_list_item_get_child (item);
  g_signal_handlers_disconnect_by_func (entry, text_changed, obj);
}

static void
setup_tree_item (GtkSignalListItemFactory *factory,
                 GtkListItem              *item)
{
  GtkWidget *expander;
  GtkWidget *entry;

  entry = gtk_editable_label_new ("");
  gtk_editable_set_width_chars (GTK_EDITABLE (entry), 3);
  expander = gtk_tree_expander_new ();
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), entry);
  gtk_list_item_set_child (item, expander);
}

static void
bind_tree_item (GtkSignalListItemFactory *factory,
                GtkListItem              *item)
{
  GtkTreeListRow *row;
  GtkTreeExpander *expander;
  TestObject *obj;
  GtkWidget *entry;

  row = gtk_list_item_get_item (item);
  expander = GTK_TREE_EXPANDER (gtk_list_item_get_child (item));
  gtk_tree_expander_set_list_row (expander, row);
  obj = gtk_tree_list_row_get_item (row);
  entry = gtk_tree_expander_get_child (expander);
  gtk_editable_set_text (GTK_EDITABLE (entry), test_object_get_string (obj));

  g_signal_connect (entry, "notify::text", G_CALLBACK (text_changed), obj);
}

static void
unbind_tree_item (GtkSignalListItemFactory *factory,
                  GtkListItem              *item)
{
  GtkTreeListRow *row;
  GtkTreeExpander *expander;
  TestObject *obj;
  GtkWidget *entry;

  row = gtk_list_item_get_item (item);
  expander = GTK_TREE_EXPANDER (gtk_list_item_get_child (item));
  obj = gtk_tree_list_row_get_item (row);
  entry = gtk_tree_expander_get_child (expander);
  g_signal_handlers_disconnect_by_func (entry, text_changed, obj);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *box2;
  GtkWidget *stack;
  GtkWidget *switcher;
  GtkWidget *sw;
  GtkWidget *grid;
  GtkWidget *list;
  GtkWidget *cv;
  GListModel *model;
  GtkListItemFactory *factory;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_window_set_child (GTK_WINDOW (window), box2);

  switcher = gtk_stack_switcher_new ();
  gtk_widget_set_halign (GTK_WIDGET (switcher), GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (GTK_WIDGET (switcher), 10);
  gtk_widget_set_margin_bottom (GTK_WIDGET (switcher), 10);
  gtk_box_append (GTK_BOX (box2), switcher);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_box_append (GTK_BOX (box2), box);

  label = gtk_editable_label_new ("Drag me");
  gtk_box_append (GTK_BOX (box), label);

  stack = gtk_stack_new ();
  gtk_widget_set_vexpand (stack, TRUE);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_box_append (GTK_BOX (box), stack);

  /* grid */
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "grid", "GtkGridView");

  model = create_model (0, 400, 1, FALSE);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item), NULL);

  grid = gtk_grid_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (model)), factory);
  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (grid), 20);
  gtk_grid_view_set_max_columns (GTK_GRID_VIEW (grid), 20);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), grid);

  /* list */
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "list", "GtkListView");

  list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (create_model (0, 400, 1, FALSE))), NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item), NULL);

  gtk_list_view_set_factory (GTK_LIST_VIEW (list), factory);
  g_object_unref (factory);

  /* columnview */
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "column", "GtkColumnView");

  cv = gtk_column_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (create_model (0, 400, 1, FALSE))));

  for (guint i = 0; i < 20; i++)
    {
      GtkColumnViewColumn *column;
      char *title;

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
      g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item), NULL);

      title = g_strdup_printf ("Column %u", i);
      column = gtk_column_view_column_new (title, factory);
      gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);
      g_object_unref (column);
      g_free (title);
    }

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), cv);

  /* tree */
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), sw, "tree", "Tree");

  list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (create_tree_model (20, 20))), NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_tree_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_tree_item), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_tree_item), NULL);

  gtk_list_view_set_factory (GTK_LIST_VIEW (list), factory);
  g_object_unref (factory);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}
