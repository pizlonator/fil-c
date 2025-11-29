/* List Box/Controls
 *
 * GtkListBox is well-suited for creating “button strips” — lists of
 * controls for use in preference dialogs or settings panels. To create
 * this style of list, use the .rich-list style class.
 */

#include <gtk/gtk.h>

static GtkWidget *window;
static GtkWidget *switch_widget;
static GtkWidget *check;
static GtkWidget *image;

static void
row_activated (GtkListBox    *list,
               GtkListBoxRow *row)
{
  if (gtk_widget_is_ancestor (switch_widget, GTK_WIDGET (row)))
    {
      gtk_switch_set_active (GTK_SWITCH (switch_widget),
                             !gtk_switch_get_active (GTK_SWITCH (switch_widget)));
    }
  else if (gtk_widget_is_ancestor (check, GTK_WIDGET (row)))
    {
      gtk_check_button_set_active (GTK_CHECK_BUTTON (check),
                                   !gtk_check_button_get_active (GTK_CHECK_BUTTON (check)));
    }
  else if (gtk_widget_is_ancestor (image, GTK_WIDGET (row)))
    {
      gtk_widget_set_opacity (image,
                              1.0 - gtk_widget_get_opacity (image));
    }
}

GtkWidget *
do_listbox_controls (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, row_activated);
      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);

      gtk_builder_add_from_resource (builder, "/listbox_controls/listbox_controls.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      switch_widget = GTK_WIDGET (gtk_builder_get_object (builder, "switch"));
      check = GTK_WIDGET (gtk_builder_get_object (builder, "check"));
      image = GTK_WIDGET (gtk_builder_get_object (builder, "image"));

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
