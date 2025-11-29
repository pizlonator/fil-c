#include <gtk/gtk.h>

static void
stack_role (void)
{
  GtkWidget *stack = gtk_stack_new ();
  GtkWidget *child1 = gtk_label_new ("a");
  GtkWidget *child2 = gtk_label_new ("b");
  GtkWidget *switcher = gtk_stack_switcher_new ();
  GtkWidget *child;
  GtkStackPage *page;

  g_object_ref_sink (stack);
  g_object_ref_sink (switcher);

  gtk_stack_add_named (GTK_STACK (stack), child1, "a");
  gtk_stack_add_named (GTK_STACK (stack), child2, "b");

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (stack), GTK_ACCESSIBLE_ROLE_GROUP);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (switcher), GTK_ACCESSIBLE_ROLE_TAB_LIST);

  child = gtk_widget_get_first_child (switcher);
  page = gtk_stack_get_page (GTK_STACK (stack), child1);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_ROLE_TAB);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (page), GTK_ACCESSIBLE_ROLE_TAB_PANEL);

  child = gtk_widget_get_last_child (switcher);
  page = gtk_stack_get_page (GTK_STACK (stack), child2);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_ROLE_TAB);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (page), GTK_ACCESSIBLE_ROLE_TAB_PANEL);

  g_object_unref (stack);
  g_object_unref (switcher);
}

static void
stack_state (void)
{
  GtkWidget *stack = gtk_stack_new ();
  GtkWidget *child1 = gtk_label_new ("a");
  GtkWidget *child2 = gtk_label_new ("b");
  GtkWidget *switcher = gtk_stack_switcher_new ();
  GtkWidget *child;

  g_object_ref_sink (stack);
  g_object_ref_sink (switcher);

  gtk_stack_add_named (GTK_STACK (stack), child1, "a");
  gtk_stack_add_named (GTK_STACK (stack), child2, "b");

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  child = gtk_widget_get_first_child (switcher);
  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_STATE_SELECTED, TRUE);

  child = gtk_widget_get_last_child (switcher);
  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_STATE_SELECTED, FALSE);

  g_object_unref (stack);
  g_object_unref (switcher);
}

static void
stack_relations (void)
{
  GtkWidget *stack = gtk_stack_new ();
  GtkWidget *child1 = gtk_label_new ("a");
  GtkWidget *child2 = gtk_label_new ("b");
  GtkWidget *switcher = gtk_stack_switcher_new ();
  GtkWidget *child;
  GtkStackPage *page;

  g_object_ref_sink (stack);
  g_object_ref_sink (switcher);

  gtk_stack_add_named (GTK_STACK (stack), child1, "a");
  gtk_stack_add_named (GTK_STACK (stack), child2, "b");

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  child = gtk_widget_get_first_child (switcher);
  page = gtk_stack_get_page (GTK_STACK (stack), child1);
  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_RELATION_CONTROLS, page, NULL);

  child = gtk_widget_get_last_child (switcher);
  page = gtk_stack_get_page (GTK_STACK (stack), child2);
  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (child), GTK_ACCESSIBLE_RELATION_CONTROLS, page, NULL);

  g_object_unref (stack);
  g_object_unref (switcher);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/stack/role", stack_role);
  g_test_add_func ("/a11y/stack/state", stack_state);
  g_test_add_func ("/a11y/stack/relations", stack_relations);

  return g_test_run ();
}

