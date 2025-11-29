#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkWidget *
oriented_test_widget (const char *label, const char *color)
{
  GtkWidget *box;
  GtkWidget *widget;
  GtkCssProvider *provider;
  char *data;

  widget = gtk_label_new (label);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  provider = gtk_css_provider_new ();
  data = g_strdup_printf ("box { background: %s; }", color);
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (box),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_free (data);
  g_object_unref (provider);
  gtk_box_append (GTK_BOX (box), widget);

  return box;
}

static GtkWidget *
test_widget (const char *label, const char *color)
{
  return oriented_test_widget (label, color);
}

static GtkOrientation o;

static void
toggle_orientation (GtkGestureClick *gesture,
                    guint            n_press,
                    double           x,
                    double           y,
                    GtkGrid         *grid)
{
  o = 1 - o;
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), o);
}

static void
simple_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *test1, *test2, *test3, *test4, *test5, *test6;
  GtkGesture *gesture;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Orientation");
  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (toggle_orientation), grid);
  gtk_widget_add_controller (window, GTK_EVENT_CONTROLLER (gesture));

  gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
  test1 = test_widget ("1", "red");
  gtk_grid_attach (GTK_GRID (grid), test1, 0, 0, 1, 1);
  test2 = test_widget ("2", "green");
  gtk_grid_attach (GTK_GRID (grid), test2, 1, 0, 1, 1);
  test3 = test_widget ("3", "blue");
  gtk_grid_attach (GTK_GRID (grid), test3, 2, 0, 1, 1);
  test4 = test_widget ("4", "green");
  gtk_grid_attach (GTK_GRID (grid), test4, 0, 1, 1, 1);
  gtk_widget_set_vexpand (test4, TRUE);
  test5 = test_widget ("5", "blue");
  gtk_grid_attach_next_to (GTK_GRID (grid), test5, test4, GTK_POS_RIGHT, 2, 1);
  test6 = test_widget ("6", "yellow");
  gtk_grid_attach (GTK_GRID (grid), test6, -1, 0, 1, 2);
  gtk_widget_set_hexpand (test6, TRUE);

  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, -1) == NULL);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, 0) == test1);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 1, 0) == test2);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), 0, 1) == test4);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 0) == test6);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 1) == test6);
  g_assert (gtk_grid_get_child_at (GTK_GRID (grid), -1, 2) == NULL);
  gtk_window_present (GTK_WINDOW (window));
}

static void
text_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *paned1;
  GtkWidget *box;
  GtkWidget *label;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Height-for-Width");
  paned1 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_window_set_child (GTK_WINDOW (window), paned1);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_paned_set_start_child (GTK_PANED (paned1), box);
  gtk_paned_set_resize_start_child (GTK_PANED (paned1), TRUE);
  gtk_paned_set_shrink_start_child (GTK_PANED (paned1), FALSE);
  gtk_paned_set_end_child (GTK_PANED (paned1), gtk_label_new ("Space"));
  gtk_paned_set_resize_end_child (GTK_PANED (paned1), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (paned1), FALSE);

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (box), gtk_label_new ("Above"));
  gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (box), grid);
  gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (box), gtk_label_new ("Below"));

  label = gtk_label_new ("Some text that may wrap if it has to");
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("1", "red"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("2", "blue"), 0, 1, 1, 1);

  label = gtk_label_new ("Some text that may wrap if it has to");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
}

static void
box_comparison (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *grid;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Grid vs. Box");
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Above"));
  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (vbox), box);

  gtk_box_append (GTK_BOX (box), test_widget ("1", "white"));

  label = gtk_label_new ("Some ellipsizing text");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_box_append (GTK_BOX (box), label);

  gtk_box_append (GTK_BOX (box), test_widget ("2", "green"));

  label = gtk_label_new ("Some text that may wrap if needed");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_box_append (GTK_BOX (box), label);

  gtk_box_append (GTK_BOX (box), test_widget ("3", "red"));

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (vbox), grid);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("1", "white"), 0, 0, 1, 1);

  label = gtk_label_new ("Some ellipsizing text");
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("2", "green"), 2, 0, 1, 1);

  label = gtk_label_new ("Some text that may wrap if needed");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 10);
  gtk_grid_attach (GTK_GRID (grid), label, 3, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("3", "red"), 4, 0, 1, 1);

  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Below"));

  gtk_window_present (GTK_WINDOW (window));
}

static void
empty_line (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *child;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Empty row");
  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  child = test_widget ("(0, 0)", "red");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(10, 0)", "green"), 10, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(10, 1)", "magenta"), 10, 1, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
}

static void
empty_grid (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *child;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Empty grid");
  grid = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);

  child = test_widget ("(0, 0)", "red");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_hide (child);
}

static void
scrolling (void)
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *viewport;
  GtkWidget *grid;
  GtkWidget *child;
  int i;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Scrolling");
  sw = gtk_scrolled_window_new ();
  viewport = gtk_viewport_new (NULL, NULL);
  grid = gtk_grid_new ();

  gtk_window_set_child (GTK_WINDOW (window), sw);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), viewport);
  gtk_viewport_set_child (GTK_VIEWPORT (viewport), grid);

  child = oriented_test_widget ("#800080", "#800080");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);

  for (i = 1; i < 16; i++)
    {
      char *color;
      color = g_strdup_printf ("#%02x00%02x", 128 + 8*i, 128 - 8*i);
      child = test_widget (color, color);
      gtk_grid_attach (GTK_GRID (grid), child, 0, i, i + 1, 1);
      gtk_widget_set_hexpand (child, TRUE);
      g_free (color);
    }

  for (i = 1; i < 16; i++)
    {
      char *color;
      color = g_strdup_printf ("#%02x00%02x", 128 - 8*i, 128 + 8*i);
      child = oriented_test_widget (color, color);
      gtk_grid_attach (GTK_GRID (grid), child, i, 0, 1, i);
      gtk_widget_set_vexpand (child, TRUE);
      g_free (color);
    }

  gtk_window_present (GTK_WINDOW (window));
}

static void
insert_cb (GtkButton *button, GtkWidget *window)
{
  GtkGrid *g, *g1, *g2, *g3, *g4;
  GtkWidget *child;
  gboolean inserted;

  g = GTK_GRID (gtk_window_get_child (GTK_WINDOW (window)));
  g1 = GTK_GRID (gtk_grid_get_child_at (g, 0, 0));
  g2 = GTK_GRID (gtk_grid_get_child_at (g, 1, 0));
  g3 = GTK_GRID (gtk_grid_get_child_at (g, 0, 1));
  g4 = GTK_GRID (gtk_grid_get_child_at (g, 1, 1));

  inserted = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "inserted"));

  if (inserted)
    {
      gtk_grid_remove_row (g1, 1);
      gtk_grid_remove_column (g2, 1);
      gtk_grid_remove_row (g3, 1);
      gtk_grid_remove_column (g4, 1);
    }
  else
    {
      gtk_grid_insert_row (g1, 1);
      gtk_grid_attach (g1, test_widget ("(0, 1)", "red"), 0, 1, 1, 1);
      gtk_grid_attach (g1, test_widget ("(2, 1)", "red"), 2, 1, 1, 1);

      gtk_grid_insert_column (g2, 1);
      gtk_grid_attach (g2, test_widget ("(1, 0)", "red"), 1, 0, 1, 1);
      gtk_grid_attach (g2, test_widget ("(1, 2)", "red"), 1, 2, 1, 1);

      child = gtk_grid_get_child_at (g3, 0, 0);
      gtk_grid_insert_next_to (g3, child, GTK_POS_BOTTOM);
      gtk_grid_attach (g3, test_widget ("(0, 1)", "red"), 0, 1, 1, 1);
      gtk_grid_attach (g3, test_widget ("(2, 1)", "red"), 2, 1, 1, 1);

      child = gtk_grid_get_child_at (g4, 0, 0);
      gtk_grid_insert_next_to (g4, child, GTK_POS_RIGHT);
      gtk_grid_attach (g4, test_widget ("(1, 0)", "red"), 1, 0, 1, 1);
      gtk_grid_attach (g4, test_widget ("(1, 2)", "red"), 1, 2, 1, 1);
    }

  gtk_button_set_label (button, inserted ? "Insert" : "Remove");
  g_object_set_data (G_OBJECT (button), "inserted", GINT_TO_POINTER (!inserted));
}

static void
insert (void)
{
  GtkWidget *window;
  GtkWidget *g;
  GtkWidget *grid;
  GtkWidget *child;
  GtkWidget *button;

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Insertion / Removal");

  g = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (g), 10);
  gtk_grid_set_column_spacing (GTK_GRID (g), 10);
  gtk_window_set_child (GTK_WINDOW (window), g);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 0)", "blue"), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "green"), 1, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 0)", "yellow"), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "yellow"), 2, 1, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 1, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 0)", "blue"), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "blue"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "green"), 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 2)", "yellow"), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "yellow"), 1, 2, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 0, 1, 1, 1);

  child = test_widget ("(0, 0)", "blue");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "blue"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "green"), 1, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 0)", "yellow"), 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(2, 1)", "yellow"), 2, 1, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (g), grid, 1, 1, 1, 1);

  child = test_widget ("(0, 0)", "blue");
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 0)", "blue"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 1)", "green"), 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(0, 2)", "yellow"), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), test_widget ("(1, 2)", "yellow"), 1, 2, 1, 1);

  button = gtk_button_new_with_label ("Insert");
  g_signal_connect (button, "clicked", G_CALLBACK (insert_cb), window);
  gtk_grid_attach (GTK_GRID (g), button, 0, 2, 2, 1);

  gtk_window_present (GTK_WINDOW (window));
}

static void
spanning_grid (void)
{
  GtkWidget *window;
  GtkWidget *g;
  GtkWidget *c;

  /* inspired by bug 698660
   * the row/column that are empty except for the spanning
   * child need to stay collapsed
   */

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Spanning");

  g = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), g);

  c = test_widget ("0", "blue");
  gtk_widget_set_hexpand (c, TRUE);
  gtk_grid_attach (GTK_GRID (g), c, 0, 4, 4, 1);

  c = test_widget ("1", "green");
  gtk_widget_set_vexpand (c, TRUE);
  gtk_grid_attach (GTK_GRID (g), c, 4, 0, 1, 4);

  c = test_widget ("2", "red");
  gtk_widget_set_hexpand (c, TRUE);
  gtk_widget_set_vexpand (c, TRUE);
  gtk_grid_attach (GTK_GRID (g), c, 3, 3, 1, 1);

  c = test_widget ("3", "yellow");
  gtk_grid_attach (GTK_GRID (g), c, 0, 3, 2, 1);

  c = test_widget ("4", "orange");
  gtk_grid_attach (GTK_GRID (g), c, 3, 0, 1, 2);

  c = test_widget ("5", "purple");
  gtk_grid_attach (GTK_GRID (g), c, 1, 1, 1, 1);

  c = test_widget ("6", "white");
  gtk_grid_attach (GTK_GRID (g), c, 0, 1, 1, 1);

  c = test_widget ("7", "cyan");
  gtk_grid_attach (GTK_GRID (g), c, 1, 0, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  simple_grid ();
  text_grid ();
  box_comparison ();
  empty_line ();
  scrolling ();
  insert ();
  empty_grid ();
  spanning_grid ();

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
