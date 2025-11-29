#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())

G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
demo_widget_init (DemoWidget *self)
{
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
}

static void
test_custom_widget_role (void)
{
  GtkWidget *widget;

  widget = g_object_new (DEMO_TYPE_WIDGET, NULL);
  g_object_ref_sink (widget);

  g_assert_true (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget)) == GTK_ACCESSIBLE_ROLE_GENERIC);

  g_object_unref (widget);
}

static void
test_custom_widget_role_explicit (void)
{
  GtkWidget *widget;

  widget = g_object_new (DEMO_TYPE_WIDGET,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_LABEL,
                         NULL);
  g_object_ref_sink (widget);

  g_assert_true (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget)) == GTK_ACCESSIBLE_ROLE_LABEL);

  g_object_unref (widget);
}

static void
test_custom_widget_ui (void)
{
  GtkBuilder *builder;
  GtkWidget *widget;

  g_type_ensure (DEMO_TYPE_WIDGET);

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
    "<interface>"
    "  <object class='DemoWidget' id='test'>"
    "  </object>"
    "</interface>", -1, NULL);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "test"));

  g_assert_true (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget)) == GTK_ACCESSIBLE_ROLE_GENERIC);

  g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/custom-widget/role", test_custom_widget_role);
  g_test_add_func ("/a11y/custom-widget/explicit-role", test_custom_widget_role_explicit);
  g_test_add_func ("/a11y/custom-widget/ui", test_custom_widget_ui);

  return g_test_run ();
}
