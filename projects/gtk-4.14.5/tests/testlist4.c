#include <gtk/gtk.h>

typedef struct
{
  GtkApplication parent_instance;
} TestApp;

typedef GtkApplicationClass TestAppClass;

static GType test_app_get_type (void);
G_DEFINE_TYPE (TestApp, test_app, GTK_TYPE_APPLICATION)

static GtkWidget *create_row (const char *label);

static void
activate_first_row (GSimpleAction *simple,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  const char *text = "First row activated (no parameter action)";

  g_print ("%s\n", text);
  gtk_label_set_label (GTK_LABEL (user_data), text);
}

static void
activate_print_string (GSimpleAction *simple,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  const char *text = g_variant_get_string (parameter, NULL);

  g_print ("%s\n", text);
  gtk_label_set_label (GTK_LABEL (user_data), text);
}

static void
activate_print_int (GSimpleAction *simple,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  const int value = g_variant_get_int32 (parameter);
  char *text;

  text = g_strdup_printf ("Row %d activated (int action)", value);

  g_print ("%s\n", text);
  gtk_label_set_label (GTK_LABEL (user_data), text);
}

static void
row_without_gaction_activated_cb (GtkListBox    *list,
                                  GtkListBoxRow *row,
                                  gpointer       user_data)
{
  int index = gtk_list_box_row_get_index (row);
  char *text;

  text = g_strdup_printf ("Row %d activated (signal based)", index);

  g_print ("%s\n", text);
  gtk_label_set_label (GTK_LABEL (user_data), text);
}

static void
add_separator (GtkListBoxRow *row, GtkListBoxRow *before, gpointer data)
{
  if (!before)
    return;

  gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static GtkWidget *
create_row (const char *text)
{
  GtkWidget *row_content, *label;

  row_content = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  label = gtk_label_new (text);
  gtk_box_append (GTK_BOX (row_content), label);

  return row_content;
}

static void
new_window (GApplication *app)
{
  GtkWidget *window, *grid, *sw, *list, *label;
  GSimpleAction *action;

  GtkWidget *row_content;
  GtkListBoxRow *row;

  int i;
  char *text, *text2;

  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  /* widget creation */
  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);
  sw = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (GTK_WIDGET (sw), true);
  gtk_widget_set_vexpand (GTK_WIDGET (sw), true);
  gtk_grid_attach (GTK_GRID (grid), sw, 0, 0, 1, 1);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (list), add_separator, NULL, NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  label = gtk_label_new ("No row activated");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  /* no parameter action row */
  action = g_simple_action_new ("first-row-action", NULL);
  g_action_map_add_action (G_ACTION_MAP (window), G_ACTION (action));

  row_content = create_row ("First row (no parameter action)");
  gtk_list_box_insert (GTK_LIST_BOX (list), row_content, -1);

  row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), 0);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (row), "win.first-row-action");

  g_signal_connect (action, "activate", (GCallback) activate_first_row, label);

  /* string action rows */
  action = g_simple_action_new ("print-string", G_VARIANT_TYPE_STRING);
  g_action_map_add_action (G_ACTION_MAP (window), G_ACTION (action));

  for (i = 1; i < 3; i++)
    {
      text = g_strdup_printf ("Row %d (string action)", i);
      row_content = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row_content, -1);

      row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), i);
      text2 = g_strdup_printf ("Row %d activated (string action)", i);
      gtk_actionable_set_action_target (GTK_ACTIONABLE (row), "s", text2);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (row), "win.print-string");
    }

  g_signal_connect (action, "activate", (GCallback) activate_print_string, label);

  /* int action rows */
  action = g_simple_action_new ("print-int", G_VARIANT_TYPE_INT32);
  g_action_map_add_action (G_ACTION_MAP (window), G_ACTION (action));

  for (i = 3; i < 5; i++)
    {
      text = g_strdup_printf ("Row %d (int action)", i);
      row_content = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row_content, -1);

      row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), i);
      gtk_actionable_set_action_target (GTK_ACTIONABLE (row), "i", i);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (row), "win.print-int");
    }

  g_signal_connect (action, "activate", (GCallback) activate_print_int, label);

  /* signal based row */
  for (i = 5; i < 7; i++)
    {
      text = g_strdup_printf ("Row %d (signal based)", i);
      row_content = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row_content, -1);
    }

  g_signal_connect (list, "row-activated",
                    G_CALLBACK (row_without_gaction_activated_cb), label);

  /* let the show begin */
  gtk_window_present (GTK_WINDOW (window));
}

static void
test_app_activate (GApplication *application)
{
  new_window (application);
}

static void
test_app_init (TestApp *app)
{
}

static void
test_app_class_init (TestAppClass *class)
{
  G_APPLICATION_CLASS (class)->activate = test_app_activate;
}

static TestApp *
test_app_new (void)
{
  TestApp *test_app;

  g_set_application_name ("Test List 4");

  test_app = g_object_new (test_app_get_type (),
                           "application-id", "org.gtk.testlist4",
                           "flags", G_APPLICATION_FLAGS_NONE,
                           NULL);

  return test_app;
}

int
main (int argc, char **argv)
{
  TestApp *test_app;
  int status;

  test_app = test_app_new ();
  status = g_application_run (G_APPLICATION (test_app), argc, argv);

  g_object_unref (test_app);
  return status;
}
