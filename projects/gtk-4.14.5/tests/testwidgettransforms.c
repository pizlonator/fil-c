

#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static const char *css =
"test>button {"
"  all: unset; "
"  background-color: white;"
"  border: 30px solid teal;"
"  margin: 40px;"
"  padding: 40px;"
"}"
"test>button:hover {"
"  background-color: blue;"
"}"
"test image {"
"  background-color: purple;"
"}"
;

/* Just so we can avoid a signal */
GtkWidget *transform_tester;
GtkWidget *test_widget;
GtkWidget *test_child;
float scale = 1;
gboolean do_picking = TRUE;

static const GdkRGBA RED   = {1, 0, 0, 0.4};
static const GdkRGBA GREEN = {0, 1, 0, 0.7};
static const GdkRGBA BLUE  = {0, 0, 1, 0.4};
static const GdkRGBA BLACK = {0, 0, 0, 1  };



/* ######################################################################### */
/* ############################## MatrixChooser ############################ */
/* ######################################################################### */


#define GTK_TYPE_MATRIX_CHOOSER (gtk_matrix_chooser_get_type ())
G_DECLARE_FINAL_TYPE (GtkMatrixChooser, gtk_matrix_chooser, GTK, MATRIX_CHOOSER, GtkWidget)

struct _GtkMatrixChooser
{
  GtkWidget parent_instance;
};

G_DEFINE_TYPE (GtkMatrixChooser, gtk_matrix_chooser, GTK_TYPE_WIDGET)

static void
gtk_matrix_chooser_init (GtkMatrixChooser *self)
{
}

static void
gtk_matrix_chooser_class_init (GtkMatrixChooserClass *klass)
{

}


/* ######################################################################### */
/* ############################# TransformTester ########################### */
/* ######################################################################### */

#define TEST_WIDGET_MIN_SIZE 100

#define GTK_TYPE_TRANSFORM_TESTER (gtk_transform_tester_get_type ())
G_DECLARE_FINAL_TYPE (GtkTransformTester, gtk_transform_tester, GTK, TRANSFORM_TESTER, GtkWidget);

struct _GtkTransformTester
{
  GtkWidget parent_instance;

  GtkWidget *test_widget;
  int pick_increase;
};

G_DEFINE_TYPE (GtkTransformTester, gtk_transform_tester, GTK_TYPE_WIDGET);

static void
gtk_transform_tester_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;

  if (self->test_widget)
    {
      gtk_widget_measure (self->test_widget, orientation, for_size,
                          minimum, natural, NULL, NULL);
    }
}

static void
gtk_transform_tester_size_allocate (GtkWidget  *widget,
                                    int         width,
                                    int         height,
                                    int         baseline)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;
  GskTransform *global_transform;
  int w, h;

  if (!self->test_widget)
    return;

  scale += 2.5f;

  gtk_widget_measure (self->test_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      &w, NULL, NULL, NULL);
  gtk_widget_measure (self->test_widget, GTK_ORIENTATION_VERTICAL, w,
                      &h, NULL, NULL, NULL);

  g_message ("%s: %d, %d", __FUNCTION__, w, h);

  global_transform = NULL;

  global_transform = gsk_transform_translate (global_transform, &GRAPHENE_POINT_INIT (width / 2.0f, height / 2.0f));
  global_transform = gsk_transform_rotate (global_transform, scale);
  global_transform = gsk_transform_translate (global_transform, &GRAPHENE_POINT_INIT (-w / 2.0f, -h / 2.0f));

  gtk_widget_allocate (self->test_widget,
                       w, h,
                       -1,
                       global_transform);
}

static void
gtk_transform_tester_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  GtkTransformTester *self = (GtkTransformTester *)widget;
  const int width = gtk_widget_get_width (widget);
  const int height = gtk_widget_get_height (widget);
  const int inc = self->pick_increase;
  graphene_rect_t child_bounds;
  graphene_rect_t self_bounds;
  int x, y;

  GTK_WIDGET_CLASS (gtk_transform_tester_parent_class)->snapshot (widget, snapshot);

  if (!do_picking ||
      !gtk_widget_compute_bounds (self->test_widget, widget, &child_bounds) ||
      !gtk_widget_compute_bounds (self->test_widget, self->test_widget, &self_bounds))
    return;

  {
    const struct {
      graphene_point_t coords;
      GdkRGBA color;
    } points[4] = {
      { self_bounds.origin, {1, 0, 0, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x + self_bounds.size.width, self_bounds.origin.y), {0, 1, 0, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x + self_bounds.size.width, self_bounds.origin.y + self_bounds.size.height), {0, 0, 1, 1} },
      { GRAPHENE_POINT_INIT (self_bounds.origin.x, self_bounds.origin.y + self_bounds.size.height), {1, 0, 1, 1} }
    };

    for (x = 0; x < G_N_ELEMENTS (points); x ++)
      {
        double px, py;

        gtk_widget_translate_coordinates (self->test_widget, widget,
                                          points[x].coords.x, points[x].coords.y,
                                          &px, &py);

        gtk_snapshot_append_color (snapshot, &points[x].color,
                                   &GRAPHENE_RECT_INIT (px, py,
                                                        4,
                                                        4));
      }
  }

  /* Now add custom drawing */
  for (x = 0; x < width; x += inc)
    {
      for (y = 0; y < height; y += inc)
        {
          const float px = x;
          const float py = y;
          GtkWidget *picked;
#if 1
          picked = gtk_widget_pick (widget, px, py, GTK_PICK_DEFAULT);
#else
          {
            int dx, dy;
            gtk_widget_translate_coordinates (widget, self->test_widget, px, py, &dx, &dy);
            picked = gtk_widget_pick (self->test_widget, dx, dy, GTK_PICK_DEFAULT);
          }
#endif

          if (picked == self->test_widget)
            gtk_snapshot_append_color (snapshot, &GREEN,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));
          else if (picked == test_child)
            gtk_snapshot_append_color (snapshot, &BLUE,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));

          else
            gtk_snapshot_append_color (snapshot, &RED,
                                       &GRAPHENE_RECT_INIT (px - (inc / 2), py - (inc / 2), inc, inc));
        }
    }

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y,
                                                  child_bounds.size.width,
                                                  1));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x + child_bounds.size.width,
                                                  child_bounds.origin.y,
                                                  1,
                                                  child_bounds.size.height));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y + child_bounds.size.height,
                                                  child_bounds.size.width,
                                                  1));

  gtk_snapshot_append_color (snapshot, &BLACK,
                             &GRAPHENE_RECT_INIT (child_bounds.origin.x,
                                                  child_bounds.origin.y,
                                                  1,
                                                  child_bounds.size.height));
}

static void
gtk_transform_tester_init (GtkTransformTester *self)
{
  self->pick_increase = 4;
}

static void
gtk_transform_tester_class_init (GtkTransformTesterClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  widget_class->measure = gtk_transform_tester_measure;
  widget_class->size_allocate = gtk_transform_tester_size_allocate;
  widget_class->snapshot = gtk_transform_tester_snapshot;

  gtk_widget_class_set_css_name (widget_class, "test");
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  gtk_widget_queue_allocate (widget);

  return G_SOURCE_CONTINUE;
}

static void
gtk_transform_tester_set_test_widget (GtkTransformTester *self,
                                      GtkWidget          *widget)
{
  g_assert (!self->test_widget);

  self->test_widget = widget;
  gtk_widget_set_parent (widget, (GtkWidget *)self);

  gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
}

static void
toggled_cb (GtkToggleButton *source,
            gpointer         user_data)
{
  do_picking = gtk_toggle_button_get_active (source);
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
  GtkWidget *matrix_chooser;
  GtkWidget *box;
  GtkWidget *titlebar;
  GtkWidget *toggle_button;
  GtkCssProvider *provider;
  gboolean done = FALSE;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new ();
  matrix_chooser = g_object_new (GTK_TYPE_MATRIX_CHOOSER, NULL);
  transform_tester = g_object_new (GTK_TYPE_TRANSFORM_TESTER, NULL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  titlebar = gtk_header_bar_new ();

  gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);

  toggle_button = gtk_toggle_button_new ();
  gtk_button_set_label (GTK_BUTTON (toggle_button), "Picking");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_button), do_picking);
  g_signal_connect (toggle_button, "toggled", G_CALLBACK (toggled_cb), NULL);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), toggle_button);

  test_widget = gtk_button_new ();
  gtk_widget_set_size_request (test_widget, TEST_WIDGET_MIN_SIZE, TEST_WIDGET_MIN_SIZE);
  gtk_widget_set_halign (test_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (test_widget, GTK_ALIGN_CENTER);


  test_child = gtk_image_new_from_icon_name ("weather-clear");
  gtk_widget_set_halign (test_child, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (test_child, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request (test_child, TEST_WIDGET_MIN_SIZE / 2, TEST_WIDGET_MIN_SIZE / 2);
  gtk_button_set_child (GTK_BUTTON (test_widget), test_child);


  gtk_transform_tester_set_test_widget (GTK_TRANSFORM_TESTER (transform_tester), test_widget);

  gtk_widget_set_vexpand (transform_tester, TRUE);
  gtk_box_append (GTK_BOX (box), transform_tester);
  gtk_box_append (GTK_BOX (box), matrix_chooser);
  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_set_default_size ((GtkWindow *)window, 200, 200);
  g_signal_connect (window, "close-request", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
