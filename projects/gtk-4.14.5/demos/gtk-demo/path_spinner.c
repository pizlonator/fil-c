/* Path/Spinner
 *
 * This demo shows how to use GskPath to draw a simple animation
 * that could be used as a spinner.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "paintable.h"

#undef SHOW_CONTROLS

#define GTK_TYPE_SPINNER_PAINTABLE (gtk_spinner_paintable_get_type ())
G_DECLARE_FINAL_TYPE (GtkSpinnerPaintable, gtk_spinner_paintable, GTK, SPINNER_PAINTABLE, GObject)

struct _GtkSpinnerPaintable
{
  GObject parent_instance;

  gint64 start_time;

  int width;
  double angle;
  double completion;

  GskPath *circle;
  GskPath *path;
  GskStroke *stroke;
  GdkRGBA color;
  GdkRGBA circle_color;
#ifdef SHOW_CONTROLS
  GskPath *controls;
  GdkRGBA control_color;
#endif
};

struct _GtkSpinnerPaintableClass
{
  GObjectClass parent_class;
};

static int
gtk_spinner_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkSpinnerPaintable *self = GTK_SPINNER_PAINTABLE (paintable);

  return self->width;
}

static int
gtk_spinner_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkSpinnerPaintable *self = GTK_SPINNER_PAINTABLE (paintable);

  return self->width;
}

static void
gtk_spinner_paintable_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  GtkSpinnerPaintable *self = GTK_SPINNER_PAINTABLE (paintable);

  gtk_snapshot_append_stroke (snapshot, self->circle, self->stroke, &self->circle_color);
  gtk_snapshot_append_stroke (snapshot, self->path, self->stroke, &self->color);
#ifdef SHOW_CONTROLS
  GskStroke *stroke = gsk_stroke_new (1);
  gtk_snapshot_append_stroke (snapshot, self->controls, stroke, &self->control_color);
  gsk_stroke_free (stroke);
#endif
}

static GdkPaintableFlags
gtk_spinner_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_spinner_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = gtk_spinner_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_spinner_paintable_get_intrinsic_height;
  iface->snapshot = gtk_spinner_paintable_snapshot;
  iface->get_flags = gtk_spinner_paintable_get_flags;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkSpinnerPaintable, gtk_spinner_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_spinner_paintable_paintable_init))

static void
gtk_spinner_paintable_dispose (GObject *object)
{
  GtkSpinnerPaintable *self = GTK_SPINNER_PAINTABLE (object);

  gsk_path_unref (self->circle);
  gsk_path_unref (self->path);
#ifdef SHOW_CONTROLS
  gsk_path_unref (self->controls);
#endif

  gsk_stroke_free (self->stroke);

  G_OBJECT_CLASS (gtk_spinner_paintable_parent_class)->dispose (object);
}

static void
gtk_spinner_paintable_class_init (GtkSpinnerPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_spinner_paintable_dispose;
}

static void
gtk_spinner_paintable_init (GtkSpinnerPaintable *self)
{
}

static GdkPaintable *
gtk_spinner_paintable_new (void)
{
  GtkSpinnerPaintable *self;
  GskPathBuilder *builder;

  self = g_object_new (GTK_TYPE_SPINNER_PAINTABLE, NULL);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (50, 50), 40);
  self->circle = gsk_path_builder_free_to_path (builder);

  self->width = 100;
  self->angle = 0;
  self->completion = 1;
  gdk_rgba_parse (&self->color, "green");
  gdk_rgba_parse (&self->circle_color, "lightgray");
#ifdef SHOW_CONTROLS
  gdk_rgba_parse (&self->control_color, "black");
#endif
  self->stroke = gsk_stroke_new (5);

  return GDK_PAINTABLE (self);
}

#ifdef SHOW_CONTROLS
static gboolean
add_controls (GskPathOperation        op,
              const graphene_point_t *pts,
              gsize                   n_pts,
              float                   weight,
              gpointer                data)
{
  GskPathBuilder *builder = data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builder, pts[2].x, pts[2].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builder, pts[2].x, pts[2].y);
      gsk_path_builder_line_to (builder, pts[3].x, pts[3].y);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}
#endif

static void
update_path (GtkSpinnerPaintable *self)
{
  GskPathBuilder *builder;
  GskPathPoint start, end;
  graphene_point_t p0, p1;
  float start_angle, end_angle;

  start_angle = self->angle;
  end_angle = fmod (self->angle + 360 * self->completion / 100, 360);

  p0 = GRAPHENE_POINT_INIT (50 + 40 * cos (M_PI * start_angle / 180),
                            50 + 40 * sin (M_PI * start_angle / 180));
  p1 = GRAPHENE_POINT_INIT (50 + 40 * cos (M_PI * end_angle / 180),
                            50 + 40 * sin (M_PI * end_angle / 180));

  g_clear_pointer (&self->path, gsk_path_unref);

  gsk_path_get_closest_point (self->circle, &p0, INFINITY, &start, NULL);
  gsk_path_get_closest_point (self->circle, &p1, INFINITY, &end, NULL);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_segment (builder, self->circle, &start, &end);
  self->path = gsk_path_builder_free_to_path (builder);

#ifdef SHOW_CONTROLS
  g_clear_pointer (&self->controls, gsk_path_unref);
  builder = gsk_path_builder_new ();
  gsk_path_foreach (self->path, -1, add_controls, builder);
  self->controls = gsk_path_builder_free_to_path (builder);
#endif

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
gtk_spinner_paintable_set_completion (GtkSpinnerPaintable *self,
                                   float             completion)
{
  self->completion = CLAMP (completion, 0, 100);
  update_path (self);
}

static float
gtk_spinner_paintable_get_completion (GtkSpinnerPaintable *self)
{
  return self->completion;
}

static void
gtk_spinner_paintable_set_frame_time (GtkSpinnerPaintable *self,
                                      gint64            time)
{
  double delta;

  if (self->start_time == 0)
    self->start_time = time;

  delta = (time - self->start_time) / (double) G_TIME_SPAN_SECOND;
  self->angle = fmod (60 * delta, 360);
  update_path (self);
}

static gboolean
tick_cb (GtkWidget *widget,
         GdkFrameClock *clock,
         gpointer data)
{
  GtkSpinnerPaintable *self = data;

  gtk_spinner_paintable_set_frame_time (self, gdk_frame_clock_get_frame_time (clock));
  return G_SOURCE_CONTINUE;
}

static gboolean
progress_timeout (gpointer data)
{
  GtkSpinnerPaintable *self = data;
  static float progress_delta = 0.5;
  float progress;

  progress = gtk_spinner_paintable_get_completion (self);
  if (progress >= 100 || progress <= 0)
    progress_delta = -progress_delta;

  gtk_spinner_paintable_set_completion (self, progress + progress_delta);

  return G_SOURCE_CONTINUE;
}

static void
unset_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

GtkWidget *
do_path_spinner (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picture;
      GdkPaintable *paintable;
      guint timeout_id;

      window = gtk_window_new ();
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_window_set_title (GTK_WINDOW (window), "Spinner");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      paintable = gtk_spinner_paintable_new ();
      picture = gtk_picture_new_for_paintable (paintable);
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
      g_object_unref (paintable);

      gtk_widget_add_tick_callback (picture, tick_cb, paintable, NULL);
      timeout_id = g_timeout_add (100, progress_timeout, paintable);

      g_object_set_data_full (G_OBJECT (picture), "timeout", GUINT_TO_POINTER (timeout_id), unset_timeout);

      gtk_window_set_child (GTK_WINDOW (window), picture);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
