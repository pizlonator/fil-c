#include <gtk/gtk.h>
#include <math.h>

GtkAdjustment *adjustment;
int cursor_x, cursor_y;

static void
motion_cb (GtkEventControllerMotion *motion,
           double                    x,
           double                    y,
           GtkWidget                *widget)
{
  float processing_ms = gtk_adjustment_get_value (adjustment);
  g_usleep (processing_ms * 1000);

  cursor_x = x;
  cursor_y = y;
  gtk_widget_queue_draw (widget);
}

static void
on_draw (GtkDrawingArea *da,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        data)
{
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0, 0.5, 0.5);

  cairo_arc (cr, cursor_x, cursor_y, 10, 0, 2 * M_PI);
  cairo_stroke (cr);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *da;
  GtkEventController *controller;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), on_draw, NULL, NULL);
  gtk_widget_set_vexpand (da, TRUE);
  gtk_box_append (GTK_BOX (vbox), da);

  label = gtk_label_new ("Event processing time (ms):");
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), label);

  adjustment = gtk_adjustment_new (20, 0, 200, 1, 10, 0);
  scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_box_append (GTK_BOX (vbox), scale);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion",
                    G_CALLBACK (motion_cb), da);
  gtk_widget_add_controller (da, controller);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (quit_cb), &done);

  gtk_window_present (GTK_WINDOW (window));
  while (!done)
    g_main_context_iteration (NULL, TRUE); 

  return 0;
}
