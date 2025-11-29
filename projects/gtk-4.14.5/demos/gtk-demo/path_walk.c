/* Path/Walk
 *
 * This demo draws a world map and shows how to animate objects along a GskPath.
 *
 * The world map that is used here is a path with 211 lines and 1569 cubic
 * Bėzier segments in 121 contours.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define GTK_TYPE_PATH_WALK (gtk_path_walk_get_type ())
G_DECLARE_FINAL_TYPE (GtkPathWalk, gtk_path_walk, GTK, PATH_WALK, GtkWidget)

#define POINT_SIZE 8

enum {
  PROP_0,
  PROP_N_POINTS,
  PROP_PATH,
  N_PROPS
};

struct _GtkPathWalk
{
  GtkWidget parent_instance;

  GskPath *path;
  GskPathMeasure *measure;
  graphene_rect_t bounds;
  GskPath *arrow_path;
  guint n_points;
};

struct _GtkPathWalkClass
{
  GtkWidgetClass parent_class;
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkPathWalk, gtk_path_walk, GTK_TYPE_WIDGET)

static void
rgba_init_from_hsla (GdkRGBA *rgba,
                     float    hue,
                     float    saturation,
                     float    lightness,
                     float    alpha)
{
  float m1, m2;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  rgba->alpha = alpha;

  if (saturation == 0)
    {
      rgba->red = lightness;
      rgba->green = lightness;
      rgba->blue = lightness;
    }
  else
    {
      hue = hue + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->red = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->red = m2;
      else if (hue < 240)
        rgba->red = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->red = m1;

      hue -= 120;
      if (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->green = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->green = m2;
      else if (hue < 240)
        rgba->green = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->green = m1;

      hue -= 120;
      if (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->blue = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->blue = m2;
      else if (hue < 240)
        rgba->blue = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->blue = m1;
    }
}

static void
gtk_path_walk_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkPathWalk *self = GTK_PATH_WALK (widget);
  double width = gtk_widget_get_width (widget);
  double height = gtk_widget_get_height (widget);
  float length, progress;
  GskStroke *stroke;
  guint i;

  if (self->path == NULL)
    return;

  gtk_snapshot_save (snapshot);

  stroke = gsk_stroke_new (2.0);
  gtk_snapshot_push_stroke (snapshot, self->path, stroke);
  gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot);
  gsk_stroke_free (stroke);

  length = gsk_path_measure_get_length (self->measure);
  progress = 25.f * gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget)) / G_USEC_PER_SEC;

  stroke = gsk_stroke_new (1.0);
  for (i = 0; i < self->n_points; i++)
    {
      GskPathPoint point;
      graphene_point_t position;
      float angle;
      GdkRGBA color;
      float distance;

      distance = i * length / self->n_points;
      distance = fmod (distance + progress, length);

      gsk_path_measure_get_point (self->measure, distance, &point);
      gsk_path_point_get_position (&point, self->path, &position);
      angle = gsk_path_point_get_rotation (&point, self->path, GSK_PATH_FROM_START);
      rgba_init_from_hsla (&color, 360.f * i / self->n_points, 1, 0.5, 1);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &position);
      gtk_snapshot_rotate (snapshot, angle);
      gtk_snapshot_append_fill (snapshot, self->arrow_path, GSK_FILL_RULE_EVEN_ODD, &color);
      gtk_snapshot_append_stroke (snapshot, self->arrow_path, stroke, &(GdkRGBA) { 0, 0, 0, 1 });
      gtk_snapshot_restore (snapshot);
    }

  gsk_stroke_free (stroke);
  gtk_snapshot_restore (snapshot);
}

static void
gtk_path_walk_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkPathWalk *self = GTK_PATH_WALK (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) ceilf (self->bounds.size.width);
  else
    *minimum = *natural = (int) ceilf (self->bounds.size.height);
}

static void
gtk_path_walk_set_n_points (GtkPathWalk *self,
                            gsize        n_points)
{
  if (self->n_points == n_points)
    return;

  self->n_points = n_points;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_POINTS]);
}

static void
gtk_path_walk_set_path (GtkPathWalk *self,
                        GskPath     *path)
{
  if (self->path == path)
    return;

  g_clear_pointer (&self->path, gsk_path_unref);
  graphene_rect_init (&self->bounds, 0, 0, 0, 0);
  if (path)
    {
      GskStroke *stroke;

      self->path = gsk_path_ref (path);
      stroke = gsk_stroke_new (2.0);
      gsk_path_get_stroke_bounds (path, stroke, &self->bounds);
      gsk_stroke_free (stroke);
      self->measure = gsk_path_measure_new (self->path);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

static void
gtk_path_walk_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)

{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  switch (prop_id)
    {
    case PROP_N_POINTS:
      gtk_path_walk_set_n_points (self, g_value_get_uint (value));
      break;

    case PROP_PATH:
      gtk_path_walk_set_path (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_path_walk_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  switch (prop_id)
    {
    case PROP_N_POINTS:
      g_value_set_uint (value, self->n_points);
      break;

    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_path_walk_dispose (GObject *object)
{
  GtkPathWalk *self = GTK_PATH_WALK (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->measure, gsk_path_measure_unref);
  g_clear_pointer (&self->arrow_path, gsk_path_unref);

  G_OBJECT_CLASS (gtk_path_walk_parent_class)->dispose (object);
}

static void
gtk_path_walk_class_init (GtkPathWalkClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_path_walk_dispose;
  object_class->set_property = gtk_path_walk_set_property;
  object_class->get_property = gtk_path_walk_get_property;

  widget_class->snapshot = gtk_path_walk_snapshot;
  widget_class->measure = gtk_path_walk_measure;

  properties[PROP_N_POINTS] =
    g_param_spec_uint ("n-points",
                       NULL, NULL,
                       1, G_MAXUINT,
                       500,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PATH] =
    g_param_spec_boxed ("path",
                        NULL, NULL,
                        GSK_TYPE_PATH,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static gboolean
tick_tick_tick (GtkWidget     *self,
                GdkFrameClock *frame_clock,
                gpointer       unused)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));

  return G_SOURCE_CONTINUE;
}

static void
gtk_path_walk_init (GtkPathWalk *self)
{
  /* Data taken from
   * https://commons.wikimedia.org/wiki/Maps_of_the_world#/media/File:Simplified_blank_world_map_without_Antartica_(no_borders).svg
   */
  GBytes *data = g_resources_lookup_data ("/path_walk/path_world.txt", 0, NULL);
  GskPath *path = gsk_path_parse (g_bytes_get_data (data, NULL));
  g_bytes_unref (data);
  gtk_path_walk_set_path (self, path);
  gsk_path_unref (path);
  self->arrow_path = gsk_path_parse ("M 5 0 L 0 -5. 0 -2, -5 -2, -5 2, 0 2, 0 5 Z");
  self->n_points = 500;
  gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_tick_tick, NULL, NULL);
}

GtkWidget *
gtk_path_walk_new (void)
{
  GtkPathWalk *self;

  self = g_object_new (GTK_TYPE_PATH_WALK, NULL);

  return GTK_WIDGET (self);
}

GtkWidget *
do_path_walk (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;

      g_type_ensure (GTK_TYPE_PATH_WALK);

      builder = gtk_builder_new_from_resource ("/path_walk/path_walk.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
