/* Layout Manager/Transition
 * #Keywords: GtkLayoutManager
 *
 * This demo shows a simple example of a custom layout manager
 * and a widget using it. The layout manager places the children
 * of the widget in a grid or a circle.
 *
 * The widget is animating the transition between the two layouts.
 *
 * Click to start the transition.
 */

#include <gtk/gtk.h>

#include "demowidget.h"
#include "demochild.h"


GtkWidget *
do_layoutmanager (GtkWidget *parent)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *widget;
      GtkWidget *child;
      const char *color[] = {
        "red", "orange", "yellow", "green",
        "blue", "grey", "magenta", "lime",
        "yellow", "firebrick", "aqua", "purple",
        "tomato", "pink", "thistle", "maroon"
      };
      int i;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Layout Manager — Transition");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      widget = demo_widget_new ();

      for (i = 0; i < 16; i++)
        {
          child = demo_child_new (color[i]);
          gtk_widget_set_margin_start (child, 4);
          gtk_widget_set_margin_end (child, 4);
          gtk_widget_set_margin_top (child, 4);
          gtk_widget_set_margin_bottom (child, 4);
          demo_widget_add_child (DEMO_WIDGET (widget), child);
        }

      gtk_window_set_child (GTK_WINDOW (window), widget);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;

}
