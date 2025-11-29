#include <gtk/gtk.h>

static void
toggle_button_role (void)
{
  GtkWidget *button = gtk_toggle_button_new ();
  g_object_ref_sink (button);

  gtk_test_accessible_assert_role (button, GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);

  /* Simple command buttons have a "pressed" state set to "undefined" */
  gtk_test_accessible_assert_state (button, GTK_ACCESSIBLE_STATE_PRESSED, GTK_ACCESSIBLE_VALUE_UNDEFINED);

  g_object_unref (button);
}

static void
toggle_button_label (void)
{
  GtkWidget *button = gtk_toggle_button_new_with_label ("Hello");
  g_object_ref_sink (button);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (button),
                                       GTK_ACCESSIBLE_RELATION_LABELLED_BY, gtk_widget_get_first_child (button), NULL);

  g_object_unref (button);
}

/* Check that we set up a labelled_by relationship between a button
 * and its label.
 */
static void
toggle_button_relation (void)
{
  GtkWidget *button = gtk_toggle_button_new_with_mnemonic ("_Hello");

  g_object_ref_sink (button);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (button),
                                       GTK_ACCESSIBLE_RELATION_LABELLED_BY, gtk_widget_get_first_child (button), NULL);

  g_object_unref (button);
}

static void
toggle_button_generic (void)
{
  GtkBuilder *builder;
  GtkWidget *button;
  GError *error = NULL;

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_GENERIC,
                         NULL);

  g_object_ref_sink (button);

  gtk_test_accessible_assert_role (button, GTK_ACCESSIBLE_ROLE_GENERIC);

  g_object_unref (button);

  builder = gtk_builder_new ();

  gtk_builder_add_from_string (builder,
    "<interface>"
    "  <object class='GtkToggleButton' id='togglebutton'>"
    "    <property name='accessible-role'>generic</property>"
    "  </object>"
    "</interface>",
    -1, &error);
  g_assert_no_error (error);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "togglebutton"));

  gtk_test_accessible_assert_role (button, GTK_ACCESSIBLE_ROLE_GENERIC);

  g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/togglebutton/role", toggle_button_role);
  g_test_add_func ("/a11y/togglebutton/label", toggle_button_label);
  g_test_add_func ("/a11y/togglebutton/relation", toggle_button_relation);
  g_test_add_func ("/a11y/togglebutton/generic", toggle_button_generic);

  return g_test_run ();
}
