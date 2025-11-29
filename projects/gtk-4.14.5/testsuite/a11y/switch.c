#include <gtk/gtk.h>

static void
switch_role (void)
{
  GtkWidget *widget = gtk_switch_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SWITCH);

  g_object_unref (widget);
}

static void
switch_state (void)
{
  GtkWidget *widget = gtk_switch_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_CHECKED, FALSE);

  gtk_switch_set_active (GTK_SWITCH (widget), TRUE);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_CHECKED, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/switch/role", switch_role);
  g_test_add_func ("/a11y/switch/state", switch_state);

  return g_test_run ();
}
