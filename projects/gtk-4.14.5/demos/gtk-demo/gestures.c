/* Gestures
 * #Keywords: GtkGesture
 *
 * Perform gestures on touchscreens and other input devices. This
 * demo reacts to long presses and swipes from all devices, plus
 * multi-touch rotate and zoom gestures.
 */

#include <gtk/gtk.h>

static GtkGesture *rotate = NULL;
static GtkGesture *zoom = NULL;
static double swipe_x = 0;
static double swipe_y = 0;
static gboolean long_pressed = FALSE;

static gboolean
touchpad_swipe_gesture_begin (GtkGesture       *gesture,
                              GdkEventSequence *sequence,
                              GtkWidget        *widget)
{
  /* Disallow touchscreen events here */
  if (sequence != NULL)
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
  return sequence == NULL;
}

static void
swipe_gesture_swept (GtkGestureSwipe *gesture,
                     double           velocity_x,
                     double           velocity_y,
                     GtkWidget       *widget)
{
  swipe_x = velocity_x / 10;
  swipe_y = velocity_y / 10;
  gtk_widget_queue_draw (widget);
}

static void
long_press_gesture_pressed (GtkGestureLongPress *gesture,
                            double               x,
                            double               y,
                            GtkWidget           *widget)
{
  long_pressed = TRUE;
  gtk_widget_queue_draw (widget);
}

static void
long_press_gesture_end (GtkGesture       *gesture,
                        GdkEventSequence *sequence,
                        GtkWidget        *widget)
{
  long_pressed = FALSE;
  gtk_widget_queue_draw (widget);
}

static void
rotation_angle_changed (GtkGestureRotate *gesture,
                        double            angle,
                        double            delta,
                        GtkWidget        *widget)
{
  gtk_widget_queue_draw (widget);
}

static void
zoom_scale_changed (GtkGestureZoom *gesture,
                    double          scale,
                    GtkWidget      *widget)
{
  gtk_widget_queue_draw (widget);
}

static void
drawing_area_draw (GtkDrawingArea *area,
                   cairo_t        *cr,
                   int             width,
                   int             height,
                   gpointer        data)
{
  if (swipe_x != 0 || swipe_y != 0)
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 6);
      cairo_move_to (cr, width / 2, height / 2);
      cairo_rel_line_to (cr, swipe_x, swipe_y);
      cairo_set_source_rgba (cr, 1, 0, 0, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  if (gtk_gesture_is_recognized (rotate) || gtk_gesture_is_recognized (zoom))
    {
      cairo_pattern_t *pat;
      cairo_matrix_t matrix;
      double angle, scale;
      double x_center, y_center;

      gtk_gesture_get_bounding_box_center (GTK_GESTURE (zoom), &x_center, &y_center);

      cairo_get_matrix (cr, &matrix);
      cairo_matrix_translate (&matrix, x_center, y_center);

      cairo_save (cr);

      angle = gtk_gesture_rotate_get_angle_delta (GTK_GESTURE_ROTATE (rotate));
      cairo_matrix_rotate (&matrix, angle);

      scale = gtk_gesture_zoom_get_scale_delta (GTK_GESTURE_ZOOM (zoom));
      cairo_matrix_scale (&matrix, scale, scale);

      cairo_set_matrix (cr, &matrix);
      cairo_rectangle (cr, -100, -100, 200, 200);

      pat = cairo_pattern_create_linear (-100, 0, 200, 0);
      cairo_pattern_add_color_stop_rgb (pat, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgb (pat, 1, 1, 0, 0);
      cairo_set_source (cr, pat);
      cairo_fill (cr);

      cairo_restore (cr);

      cairo_pattern_destroy (pat);
    }

  if (long_pressed)
    {
      cairo_save (cr);
      cairo_arc (cr,
                 width / 2, height / 2,
                 50, 0, 2 * G_PI);

      cairo_set_source_rgba (cr, 0, 1, 0, 0.5);
      cairo_stroke (cr);

      cairo_restore (cr);
    }
}

GtkWidget *
do_gestures (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *drawing_area;
  GtkGesture *gesture;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
      gtk_window_set_title (GTK_WINDOW (window), "Gestures");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      drawing_area = gtk_drawing_area_new ();
      gtk_window_set_child (GTK_WINDOW (window), drawing_area);

      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area),
                                      drawing_area_draw,
                                      NULL, NULL);

      /* Swipe */
      gesture = gtk_gesture_swipe_new ();
      g_signal_connect (gesture, "swipe",
                        G_CALLBACK (swipe_gesture_swept), drawing_area);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                  GTK_PHASE_BUBBLE);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));

      /* 3fg swipe for touchpads */
      gesture = g_object_new (GTK_TYPE_GESTURE_SWIPE,
                              "n-points", 3,
                              NULL);
      g_signal_connect (gesture, "begin",
                        G_CALLBACK (touchpad_swipe_gesture_begin), drawing_area);
      g_signal_connect (gesture, "swipe",
                        G_CALLBACK (swipe_gesture_swept), drawing_area);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                  GTK_PHASE_BUBBLE);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));


      /* Long press */
      gesture = gtk_gesture_long_press_new ();
      g_signal_connect (gesture, "pressed",
                        G_CALLBACK (long_press_gesture_pressed), drawing_area);
      g_signal_connect (gesture, "end",
                        G_CALLBACK (long_press_gesture_end), drawing_area);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                  GTK_PHASE_BUBBLE);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));

      /* Rotate */
      rotate = gesture = gtk_gesture_rotate_new ();
      g_signal_connect (gesture, "angle-changed",
                        G_CALLBACK (rotation_angle_changed), drawing_area);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                  GTK_PHASE_BUBBLE);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));

      /* Zoom */
      zoom = gesture = gtk_gesture_zoom_new ();
      g_signal_connect (gesture, "scale-changed",
                        G_CALLBACK (zoom_scale_changed), drawing_area);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                  GTK_PHASE_BUBBLE);
      gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
