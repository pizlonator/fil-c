#include <gtk/gtk.h>
#include "gtk/gtkatcontextprivate.h"
#include "gtk/gtkwidgetprivate.h"

static void
test_name_content (void)
{
  GtkWidget *window, *label1, *label2, *box, *button;
  GtkATContext *context;
  char *name;

  label1 = gtk_label_new ("a");
  label2 = gtk_label_new ("b");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_box_append (GTK_BOX (box), label1);
  gtk_box_append (GTK_BOX (box), label2);
  gtk_button_set_child (GTK_BUTTON (button), box);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), button);
  gtk_window_present (GTK_WINDOW (window));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (label1));
  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "a");
  g_free (name);
  g_object_unref (context);

  /* this is because generic doesn't allow naming */
  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (box));
  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "");
  g_free (name);
  g_object_unref (context);

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (button));
  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "a b");
  g_free (name);
  g_object_unref (context);

  gtk_widget_set_visible (label2, FALSE);

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (button));
  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "a");
  g_free (name);
  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_name_tooltip (void)
{
  GtkWidget *window, *image;
  GtkATContext *context;
  char *name;

  image = gtk_image_new ();

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), image);
  gtk_window_present (GTK_WINDOW (window));

  gtk_widget_set_tooltip_text (image, "tooltip");

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (image));

  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "tooltip");
  g_free (name);

  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_name_menubutton (void)
{
  GtkWidget *window, *widget;
  GtkATContext *context;
  char *name;

  widget = gtk_menu_button_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (widget), gtk_popover_new ());

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  gtk_widget_set_tooltip_text (widget, "tooltip");

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget));

  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "tooltip");
  g_free (name);

  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_name_label (void)
{
  GtkWidget *window, *image;
  GtkATContext *context;
  char *name;
  char *desc;

  image = gtk_image_new ();

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), image);
  gtk_window_present (GTK_WINDOW (window));

  g_object_ref_sink (image);
  gtk_widget_realize_at_context (image);

  gtk_widget_set_tooltip_text (image, "tooltip");

  gtk_accessible_update_property (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "label",
                                  -1);

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (image));

  name = gtk_at_context_get_name (context);
  desc = gtk_at_context_get_description (context);

  g_assert_cmpstr (name, ==, "label");
  g_assert_cmpstr (desc, ==, "tooltip");

  g_free (name);
  g_free (desc);

  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_name_prohibited (void)
{
  GtkWidget *window, *widget;
  GtkATContext *context;
  char *name;
  char *desc;

  widget = g_object_new (GTK_TYPE_BUTTON,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_TIME,
                         "label", "too late",
                         NULL);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget));

  name = gtk_at_context_get_name (context);
  desc = gtk_at_context_get_description (context);

  g_assert_cmpstr (name, ==, "");
  g_assert_cmpstr (desc, ==, "");

  g_free (name);
  g_free (desc);

  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_name_range (void)
{
  GtkWidget *window, *scale;
  GtkATContext *context;
  char *name;

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 10);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), scale);
  gtk_window_present (GTK_WINDOW (window));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (scale));

  g_assert_true (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (scale)) == GTK_ACCESSIBLE_ROLE_SLIDER);
  g_assert_true (gtk_at_context_get_accessible_role (context) == GTK_ACCESSIBLE_ROLE_SLIDER);

  gtk_range_set_value (GTK_RANGE (scale), 50);

  name = gtk_at_context_get_name (context);
  g_assert_cmpstr (name, ==, "");

  g_free (name);

  g_object_unref (context);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/name/content", test_name_content);
  g_test_add_func ("/a11y/name/tooltip", test_name_tooltip);
  g_test_add_func ("/a11y/name/menubutton", test_name_menubutton);
  g_test_add_func ("/a11y/name/label", test_name_label);
  g_test_add_func ("/a11y/name/prohibited", test_name_prohibited);
  g_test_add_func ("/a11y/name/range", test_name_range);

  return g_test_run ();
}
