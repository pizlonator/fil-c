#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

GtkWidget *stack;
GtkWidget *switcher;
GtkWidget *sidebar;
GtkWidget *w1;

static void
set_visible_child (GtkWidget *button, gpointer data)
{
  gtk_stack_set_visible_child (GTK_STACK (stack), GTK_WIDGET (data));
}

static void
set_visible_child_name (GtkWidget *button, gpointer data)
{
  gtk_stack_set_visible_child_name (GTK_STACK (stack), (const char *)data);
}

static void
toggle_hhomogeneous (GtkWidget *button, gpointer data)
{
  gboolean active = gtk_check_button_get_active (GTK_CHECK_BUTTON (button));
  gtk_stack_set_hhomogeneous (GTK_STACK (stack), active);
}

static void
toggle_vhomogeneous (GtkWidget *button, gpointer data)
{
  gboolean active = gtk_check_button_get_active (GTK_CHECK_BUTTON (button));
  gtk_stack_set_vhomogeneous (GTK_STACK (stack), active);
}

static void
toggle_icon_name (GtkWidget *button, gpointer data)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  GtkStackPage *page;

  page = gtk_stack_get_page (GTK_STACK (stack), w1);
  g_object_set (page, "icon-name", active ? "edit-find-symbolic" : NULL, NULL);
}

static void
toggle_transitions (GtkWidget *combo, gpointer data)
{
  int id = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  gtk_stack_set_transition_type (GTK_STACK (stack), id);
}

static void
on_back_button_clicked (GtkButton *button, gpointer user_data)
{
  const char *seq[] = { "1", "2", "3" };
  const char *vis;
  int i;

  vis = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  for (i = 1; i < G_N_ELEMENTS (seq); i++)
    {
      if (g_strcmp0 (vis, seq[i]) == 0)
        {
          gtk_stack_set_visible_child_full (GTK_STACK (stack), seq[i - 1], GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
          break;
        }
    }
}

static void
on_forward_button_clicked (GtkButton *button, gpointer user_data)
{
  const char *seq[] = { "1", "2", "3" };
  const char *vis;
  int i;

  vis = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  for (i = 0; i < G_N_ELEMENTS (seq) - 1; i++)
    {
      if (g_strcmp0 (vis, seq[i]) == 0)
        {
          gtk_stack_set_visible_child_full (GTK_STACK (stack), seq[i + 1], GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
          break;
        }
    }
}

static void
update_back_button_sensitivity (GtkStack *_stack, GParamSpec *pspec, GtkWidget *button)
{
  const char *vis;

  vis = gtk_stack_get_visible_child_name (GTK_STACK (stack));
  gtk_widget_set_sensitive (button, g_strcmp0 (vis, "1") != 0);
}

static void
update_forward_button_sensitivity (GtkStack *_stack, GParamSpec *pspec, GtkWidget *button)
{
  const char *vis;

  vis = gtk_stack_get_visible_child_name (GTK_STACK (stack));
  gtk_widget_set_sensitive (button, g_strcmp0 (vis, "3") != 0);
}

int
main (int argc,
      char ** argv)
{
  GtkWidget *window, *box, *button, *hbox, *combo, *layout;
  GtkWidget *w2, *w3;
  GtkListStore* store;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkWidget *scrolled_win;
  int i;
  GtkTreeIter iter;
  GEnumClass *class;
  GtkStackPage *page;

  gtk_init ();

  window = gtk_window_new ();
  gtk_widget_set_size_request (window, 300, 300);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  switcher = gtk_stack_switcher_new ();
  gtk_box_append (GTK_BOX (box), switcher);

  stack = gtk_stack_new ();

  /* Make transitions longer so we can see that they work */
  gtk_stack_set_transition_duration (GTK_STACK (stack), 1500);

  gtk_widget_set_halign (stack, GTK_ALIGN_START);
  gtk_widget_set_vexpand (stack, TRUE);

  /* Add sidebar before stack */
  sidebar = gtk_stack_sidebar_new ();
  gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));
  layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (layout), sidebar);
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_box_append (GTK_BOX (layout), stack);

  gtk_box_append (GTK_BOX (box), layout);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  w1 = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (w1)),
			    "This is a\nTest\nBalh!", -1);

  gtk_stack_add_titled (GTK_STACK (stack), w1, "1", "1");

  w2 = gtk_button_new_with_label ("Gazoooooooooooooooonk");
  gtk_stack_add_titled (GTK_STACK (stack), w2, "2", "2");
  page = gtk_stack_get_page (GTK_STACK (stack), w2);
  g_object_set (page, "needs-attention", TRUE, NULL);


  scrolled_win = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request (scrolled_win, 100, 200);


  store = gtk_list_store_new (1, G_TYPE_STRING);

  for (i = 0; i < 40; i++)
    gtk_list_store_insert_with_values (store, &iter, i, 0,  "Testvalule", -1);

  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_win), tree_view);
  w3 = scrolled_win;

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Target", renderer,
						     "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  gtk_stack_add_titled (GTK_STACK (stack), w3, "3", "3");

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (box), hbox);

  button = gtk_button_new_with_label ("1");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w1);

  button = gtk_button_new_with_label ("2");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w2);

  button = gtk_button_new_with_label ("3");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w3);

  button = gtk_button_new_with_label ("1");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "1");

  button = gtk_button_new_with_label ("2");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "2");

  button = gtk_button_new_with_label ("3");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "3");

  button = gtk_check_button_new ();
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button),
                               gtk_stack_get_hhomogeneous (GTK_STACK (stack)));
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) toggle_hhomogeneous, NULL);

  button = gtk_check_button_new_with_label ("homogeneous");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (button),
                               gtk_stack_get_vhomogeneous (GTK_STACK (stack)));
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) toggle_vhomogeneous, NULL);

  button = gtk_toggle_button_new_with_label ("Add icon");
  g_signal_connect (button, "toggled", (GCallback) toggle_icon_name, NULL);
  gtk_box_append (GTK_BOX (hbox), button);

  combo = gtk_combo_box_text_new ();
  class = g_type_class_ref (GTK_TYPE_STACK_TRANSITION_TYPE);
  for (i = 0; i < class->n_values; i++)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), class->values[i].value_nick);
  g_type_class_unref (class);

  gtk_box_append (GTK_BOX (hbox), combo);
  g_signal_connect (combo, "changed", (GCallback) toggle_transitions, NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (box), hbox);

  button = gtk_button_new_with_label ("<");
  g_signal_connect (button, "clicked", (GCallback) on_back_button_clicked, stack);
  g_signal_connect (stack, "notify::visible-child-name",
                    (GCallback)update_back_button_sensitivity, button);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label (">");
  gtk_box_append (GTK_BOX (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) on_forward_button_clicked, stack);
  g_signal_connect (stack, "notify::visible-child-name",
                    (GCallback)update_forward_button_sensitivity, button);


  gtk_window_present (GTK_WINDOW (window));
  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}
