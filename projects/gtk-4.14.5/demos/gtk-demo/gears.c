/* OpenGL/Gears
 *
 * This is a classic OpenGL demo, running in a GtkGLArea.
 */


#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtkgears.h"

/************************************************************************
 *                 DEMO CODE                                            *
 ************************************************************************/

static void
on_axis_value_change (GtkAdjustment *adjustment,
                      gpointer       data)
{
  GtkGears *gears = GTK_GEARS (data);
  int axis = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (adjustment), "axis"));

  gtk_gears_set_axis (gears, axis, gtk_adjustment_get_value (adjustment));
}


static GtkWidget *
create_axis_slider (GtkGears *gears,
                    int axis)
{
  GtkWidget *box, *label, *slider;
  GtkAdjustment *adj;
  const char *text;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);

  switch (axis)
    {
    case GTK_GEARS_X_AXIS:
      text = "X";
      break;

    case GTK_GEARS_Y_AXIS:
      text = "Y";
      break;

    case GTK_GEARS_Z_AXIS:
      text = "Z";
      break;

    default:
      g_assert_not_reached ();
    }

  label = gtk_label_new (text);
  gtk_box_append (GTK_BOX (box), label);

  adj = gtk_adjustment_new (gtk_gears_get_axis (gears, axis), 0.0, 360.0, 1.0, 12.0, 0.0);
  g_object_set_data (G_OBJECT (adj), "axis", GINT_TO_POINTER (axis));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (on_axis_value_change),
                    gears);
  slider = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adj);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_box_append (GTK_BOX (box), slider);
  gtk_widget_set_vexpand (slider, TRUE);

  return box;
}

GtkWidget *
do_gears (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box, *hbox, *fps_label, *gears, *overlay, *frame;
  int i;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Gears");
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 640);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      overlay = gtk_overlay_new ();
      gtk_widget_set_margin_start (overlay, 12);
      gtk_widget_set_margin_end (overlay, 12);
      gtk_widget_set_margin_top (overlay, 12);
      gtk_widget_set_margin_bottom (overlay, 12);

      gtk_window_set_child (GTK_WINDOW (window), overlay);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_START);
      gtk_widget_set_valign (frame, GTK_ALIGN_START);
      gtk_widget_add_css_class (frame, "app-notification");
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), frame);

      fps_label = gtk_label_new ("");
      gtk_widget_set_halign (fps_label, GTK_ALIGN_START);
      gtk_frame_set_child (GTK_FRAME (frame), fps_label);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
      gtk_box_set_spacing (GTK_BOX (box), 6);
      gtk_overlay_set_child (GTK_OVERLAY (overlay), box);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
      gtk_box_set_spacing (GTK_BOX (box), 6);
      gtk_box_append (GTK_BOX (box), hbox);

      gears = gtk_gears_new ();
      gtk_widget_set_hexpand (gears, TRUE);
      gtk_widget_set_vexpand (gears, TRUE);
      gtk_box_append (GTK_BOX (hbox), gears);

      for (i = 0; i < GTK_GEARS_N_AXIS; i++)
        gtk_box_append (GTK_BOX (hbox), create_axis_slider (GTK_GEARS (gears), i));

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
      gtk_box_set_spacing (GTK_BOX (hbox), 6);
      gtk_box_append (GTK_BOX (box), hbox);

      gtk_gears_set_fps_label (GTK_GEARS (gears), GTK_LABEL (fps_label));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
