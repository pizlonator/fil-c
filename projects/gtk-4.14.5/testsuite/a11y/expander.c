#include <gtk/gtk.h>

static void
expander_role (void)
{
  GtkWidget *widget = gtk_expander_new ("Hello");
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_BUTTON);

  g_object_unref (widget);
}

static void
expander_state (void)
{
  GtkWidget *widget = gtk_expander_new ("Hello");
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_EXPANDED, FALSE);

  gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_EXPANDED, TRUE);

  g_object_unref (widget);
}

static void
expander_relations (void)
{
  GtkWidget *widget = gtk_expander_new ("Hello");
  GtkWidget *child = gtk_label_new ("Child");

  g_object_ref_sink (widget);

  gtk_expander_set_child (GTK_EXPANDER (widget), child);

  gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);

  gtk_test_accessible_assert_relation (widget, GTK_ACCESSIBLE_RELATION_CONTROLS, child, NULL);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/expander/role", expander_role);
  g_test_add_func ("/a11y/expander/state", expander_state);
  g_test_add_func ("/a11y/expander/relations", expander_relations);

  return g_test_run ();
}
