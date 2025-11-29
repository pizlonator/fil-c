#include <gtk/gtk.h>

enum
{
  PROP_LABEL = 1,
  PROP_ID,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

typedef struct
{
  GObject parent;

  char *label;
  int id;
} MyObject;

typedef struct
{
  GObjectClass parent_class;
} MyObjectClass;

static GType my_object_get_type (void);
G_DEFINE_TYPE (MyObject, my_object, G_TYPE_OBJECT)

static void
my_object_init (MyObject *obj)
{
}

static void
my_object_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MyObject *obj = (MyObject *)object;

  switch (property_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, obj->label);
      break;
    case PROP_ID:
      g_value_set_int (value, obj->id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
my_object_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MyObject *obj = (MyObject *)object;

  switch (property_id)
    {
    case PROP_LABEL:
      g_free (obj->label);
      obj->label = g_value_dup_string (value);
      break;
    case PROP_ID:
      obj->id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
my_object_finalize (GObject *obj)
{
  MyObject *object = (MyObject *)obj;

  g_free (object->label);

  G_OBJECT_CLASS (my_object_parent_class)->finalize (obj);
}

static void
my_object_class_init (MyObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = my_object_get_property;
  object_class->set_property = my_object_set_property;
  object_class->finalize = my_object_finalize;

  properties[PROP_LABEL] = g_param_spec_string ("label", "label", "label",
                                                NULL, G_PARAM_READWRITE);
  properties[PROP_ID] = g_param_spec_int ("id", "id", "id",
                                          0, G_MAXINT, 0, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}

static GtkWidget *
create_widget (gpointer item,
               gpointer user_data)
{
  MyObject *obj = (MyObject *)item;
  GtkWidget *label;

  label = gtk_label_new ("");
  g_object_bind_property (obj, "label", label, "label", G_BINDING_SYNC_CREATE);

  return label;
}

static int
compare_items (gconstpointer a, gconstpointer b, gpointer data)
{
  int id_a, id_b;

  g_object_get ((gpointer)a, "id", &id_a, NULL);
  g_object_get ((gpointer)b, "id", &id_b, NULL);

  return id_a - id_b;
}

static void
add_some (GtkButton *button, GListStore *store)
{
  int n, i;
  guint n_items;
  GObject *obj;
  char *label;

  for (n = 0; n < 50; n++)
    {
      n_items = g_list_model_get_n_items (G_LIST_MODEL (store));
      i = g_random_int_range (0, MAX (2 * n_items, 1));
      label = g_strdup_printf ("Added %d", i);
      obj = g_object_new (my_object_get_type (),
                          "id", i,
                          "label", label,
                          NULL);
      g_list_store_insert_sorted (store, obj, compare_items, NULL);
      g_object_unref (obj);
      g_free (label);
    }
}

static void
remove_some (GtkButton *button, GListStore *store)
{
  int n, i;
  guint n_items;

  for (n = 0; n < 50; n++)
    {
      n_items = g_list_model_get_n_items (G_LIST_MODEL (store));
      if (n_items == 0)
        return;
      i = g_random_int_range (0, n_items);
      g_list_store_remove (store, i);
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *grid, *sw, *box, *button;
  GListStore *store;
  int i;

  gtk_init ();

  store = g_list_store_new (my_object_get_type ());
  for (i = 0; i < 100; i++)
    {
      MyObject *obj;
      char *label;

      label = g_strdup_printf ("item %d", i);
      obj = g_object_new (my_object_get_type (),
                          "id", i,
                          "label", label,
                          NULL);
      g_list_store_append (store, obj);
      g_free (label);
      g_object_unref (obj);
    }

  window = gtk_window_new ();
  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_grid_attach (GTK_GRID (grid), sw, 0, 0, 1, 1);

  box = gtk_list_box_new ();
  gtk_list_box_bind_model (GTK_LIST_BOX (box), G_LIST_MODEL (store), create_widget, NULL, NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_grid_attach (GTK_GRID (grid), sw, 1, 0, 1, 1);

  box = gtk_flow_box_new ();
  gtk_flow_box_bind_model (GTK_FLOW_BOX (box), G_LIST_MODEL (store), create_widget, NULL, NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);

  button = gtk_button_new_with_label ("Add some");
  g_signal_connect (button, "clicked", G_CALLBACK (add_some), store);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  button = gtk_button_new_with_label ("Remove some");
  g_signal_connect (button, "clicked", G_CALLBACK (remove_some), store);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 2, 1, 1);

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
