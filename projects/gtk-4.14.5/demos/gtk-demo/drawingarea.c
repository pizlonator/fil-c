/* Drawing Area
 * #Keywords: GtkDrawingArea
 *
 * GtkDrawingArea is a blank area where you can draw custom displays
 * of various kinds.
 *
 * This demo has two drawing areas. The checkerboard area shows
 * how you can just draw something; all you have to do is set a function
 * via gtk_drawing_area_set_draw_func(), as shown here.
 *
 * The "scribble" area is a bit more advanced, and shows how to handle
 * events such as button presses and mouse motion. Click the mouse
 * and drag in the scribble area to draw squiggles. Resize the window
 * to clear the area.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
/* Pixmap for scribble area, to store current scribbles */
static cairo_surface_t *surface = NULL;

/* Create a new surface of the appropriate size to store our scribbles */
static void
create_surface (GtkWidget *widget)
{
  cairo_t *cr;

  if (surface)
    cairo_surface_destroy (surface);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gtk_widget_get_width (widget),
                                        gtk_widget_get_height (widget));

  /* Initialize the surface to white */
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_destroy (cr);
}

static void
scribble_resize (GtkWidget *widget,
                 int        width,
                 int        height)
{
  create_surface (widget);
}

/* Redraw the screen from the surface */
static void
scribble_draw (GtkDrawingArea *da,
               cairo_t        *cr,
               int             width,
               int             height,
               gpointer        data)
{
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
}

/* Draw a rectangle on the screen */
static void
draw_brush (GtkWidget *widget,
            double     x,
            double     y)
{
  GdkRectangle update_rect;
  cairo_t *cr;

  if (surface == NULL ||
      cairo_image_surface_get_width (surface) != gtk_widget_get_width (widget) ||
      cairo_image_surface_get_height (surface) != gtk_widget_get_height (widget))
    create_surface (widget);

  update_rect.x = x - 3;
  update_rect.y = y - 3;
  update_rect.width = 6;
  update_rect.height = 6;

  /* Paint to the surface, where we store our state */
  cr = cairo_create (surface);

  gdk_cairo_rectangle (cr, &update_rect);
  cairo_fill (cr);

  cairo_destroy (cr);

  gtk_widget_queue_draw (widget);
}

static double start_x;
static double start_y;

static void
drag_begin (GtkGestureDrag *gesture,
            double          x,
            double          y,
            GtkWidget      *area)
{
  start_x = x;
  start_y = y;

  draw_brush (area, x, y);
}

static void
drag_update (GtkGestureDrag *gesture,
             double          x,
             double          y,
             GtkWidget      *area)
{
  draw_brush (area, start_x + x, start_y + y);
}

static void
drag_end (GtkGestureDrag *gesture,
          double          x,
          double          y,
          GtkWidget      *area)
{
  draw_brush (area, start_x + x, start_y + y);
}

static void
oval_path (cairo_t *cr,
           double xc, double yc,
           double xr, double yr)
{
  cairo_save (cr);

  cairo_translate (cr, xc, yc);
  cairo_scale (cr, 1.0, yr / xr);
  cairo_move_to (cr, xr, 0.0);
  cairo_arc (cr,
             0, 0,
             xr,
             0, 2 * G_PI);
  cairo_close_path (cr);

  cairo_restore (cr);
}

/* Fill the given area with checks in the standard style
 * for showing compositing effects.
 *
 * It would make sense to do this as a repeating surface,
 * but most implementations of RENDER currently have broken
 * implementations of repeat + transform, even when the
 * transform is a translation.
 */
static void
fill_checks (cairo_t *cr,
             int x,     int y,
             int width, int height)
{
  int i, j;

#define CHECK_SIZE 16

  cairo_rectangle (cr, x, y, width, height);
  cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
  cairo_fill (cr);

  /* Only works for CHECK_SIZE a power of 2 */
  j = x & (-CHECK_SIZE);

  for (; j < height; j += CHECK_SIZE)
    {
      i = y & (-CHECK_SIZE);
      for (; i < width; i += CHECK_SIZE)
        if ((i / CHECK_SIZE + j / CHECK_SIZE) % 2 == 0)
          cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
    }

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_fill (cr);

#undef CHECK_SIZE
}

/* Draw a red, green, and blue circle equally spaced inside
 * the larger circle of radius r at (xc, yc)
 */
static void
draw_3circles (cairo_t *cr,
               double xc, double yc,
               double radius,
               double alpha)
{
  double subradius = radius * (2 / 3. - 0.1);

  cairo_set_source_rgba (cr, 1., 0., 0., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5)),
             yc - radius / 3. * sin (G_PI * (0.5)),
             subradius, subradius);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0., 1., 0., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5 + 2/.3)),
             yc - radius / 3. * sin (G_PI * (0.5 + 2/.3)),
             subradius, subradius);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0., 0., 1., alpha);
  oval_path (cr,
             xc + radius / 3. * cos (G_PI * (0.5 + 4/.3)),
             yc - radius / 3. * sin (G_PI * (0.5 + 4/.3)),
             subradius, subradius);
  cairo_fill (cr);
}

static void
groups_draw (GtkDrawingArea *darea,
             cairo_t        *cr,
             int             width,
             int             height,
             gpointer        data)
{
  cairo_surface_t *overlay, *punch, *circles;
  cairo_t *overlay_cr, *punch_cr, *circles_cr;

  /* Fill the background */
  double radius = 0.5 * (width < height ? width : height) - 10;
  double xc = width / 2.;
  double yc = height / 2.;

  overlay = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  punch = cairo_surface_create_similar (cairo_get_target (cr),
                                        CAIRO_CONTENT_ALPHA,
                                        width, height);

  circles = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  fill_checks (cr, 0, 0, width, height);

  /* Draw a black circle on the overlay
   */
  overlay_cr = cairo_create (overlay);
  cairo_set_source_rgb (overlay_cr, 0., 0., 0.);
  oval_path (overlay_cr, xc, yc, radius, radius);
  cairo_fill (overlay_cr);

  /* Draw 3 circles to the punch surface, then cut
   * that out of the main circle in the overlay
   */
  punch_cr = cairo_create (punch);
  draw_3circles (punch_cr, xc, yc, radius, 1.0);
  cairo_destroy (punch_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_DEST_OUT);
  cairo_set_source_surface (overlay_cr, punch, 0, 0);
  cairo_paint (overlay_cr);

  /* Now draw the 3 circles in a subgroup again
   * at half intensity, and use OperatorAdd to join up
   * without seams.
   */
  circles_cr = cairo_create (circles);

  cairo_set_operator (circles_cr, CAIRO_OPERATOR_OVER);
  draw_3circles (circles_cr, xc, yc, radius, 0.5);
  cairo_destroy (circles_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_ADD);
  cairo_set_source_surface (overlay_cr, circles, 0, 0);
  cairo_paint (overlay_cr);

  cairo_destroy (overlay_cr);

  cairo_set_source_surface (cr, overlay, 0, 0);
  cairo_paint (cr);

  cairo_surface_destroy (overlay);
  cairo_surface_destroy (punch);
  cairo_surface_destroy (circles);
}

static void
close_window (void)
{
  window = NULL;

  if (surface)
    cairo_surface_destroy (surface);
  surface = NULL;
}

GtkWidget *
do_drawingarea (GtkWidget *do_widget)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *da;
  GtkWidget *label;
  GtkGesture *drag;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drawing Area");
      gtk_window_set_default_size (GTK_WINDOW (window), 250, -1);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (close_window), NULL);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 16);
      gtk_widget_set_margin_end (vbox, 16);
      gtk_widget_set_margin_top (vbox, 16);
      gtk_widget_set_margin_bottom (vbox, 16);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      /*
       * Create the groups area
       */
      label = gtk_label_new ("Knockout groups");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_vexpand (frame, TRUE);
      gtk_box_append (GTK_BOX (vbox), frame);

      da = g_object_new (GTK_TYPE_DRAWING_AREA,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_IMG,
                         NULL);
      gtk_frame_set_child (GTK_FRAME (frame), da);
      gtk_accessible_update_relation (GTK_ACCESSIBLE (da),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                      -1);

      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), groups_draw, NULL, NULL);

      /*
       * Create the scribble area
       */

      label = gtk_label_new ("Scribble area");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_vexpand (frame, TRUE);
      gtk_box_append (GTK_BOX (vbox), frame);

      da = g_object_new (GTK_TYPE_DRAWING_AREA,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_IMG,
                         NULL);
      gtk_frame_set_child (GTK_FRAME (frame), da);
      gtk_accessible_update_relation (GTK_ACCESSIBLE (da),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                      -1);

      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), scribble_draw, NULL, NULL);

      g_signal_connect (da, "resize",
                        G_CALLBACK (scribble_resize), NULL);

      drag = gtk_gesture_drag_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), 0);
      gtk_widget_add_controller (da, GTK_EVENT_CONTROLLER (drag));

      g_signal_connect (drag, "drag-begin", G_CALLBACK (drag_begin), da);
      g_signal_connect (drag, "drag-update", G_CALLBACK (drag_update), da);
      g_signal_connect (drag, "drag-end", G_CALLBACK (drag_end), da);

    }

  if (!gtk_widget_get_visible (window))
      gtk_window_present (GTK_WINDOW (window));
  else
      gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
