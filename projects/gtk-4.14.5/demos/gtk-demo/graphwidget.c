#include "graphwidget.h"

struct _GraphWidget
{
  GtkWidget parent_instance;

  GskPath *path;
  GskStroke *stroke;
  GdkRGBA color;

  guint tick_cb;
  guint64 start_time;

  double period;
  double amplitude;
};

struct _GraphWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GraphWidget, graph_widget, GTK_TYPE_WIDGET)

static void
update_path (GraphWidget *self,
             float        amplitude)
{
  graphene_point_t p[20];
  GskPathBuilder *builder;

  g_clear_pointer (&self->path, gsk_path_unref);

  for (int i = 0; i < 20; i++)
    {
      p[i].x = 10 * i;
      p[i].y = 50;

      if (i % 4 == 1 || i % 4 == 2)
        {
          if (i % 8 < 4)
            p[i].y += amplitude;
          else
            p[i].y -= amplitude;
        }
    }

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, p[0].x, p[0].y);

  for (int i = 0; i < 20; i += 4)
    gsk_path_builder_cubic_to (builder,
                               p[i+1].x, p[i+1].y,
                               p[i+2].x, p[i+2].y,
                               p[i+3].x, p[i+3].y);

  self->path = gsk_path_builder_free_to_path (builder);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  GraphWidget *self = GRAPH_WIDGET (widget);
  guint64 now;
  double angle;

  now = gdk_frame_clock_get_frame_time (frame_clock);

  if (self->start_time == 0)
    self->start_time = now;

  angle = 360 * (now - self->start_time) / (double)(self->period * G_TIME_SPAN_MINUTE);
  update_path (self, sin (angle) * self->amplitude);

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
graph_widget_init (GraphWidget *self)
{
  self->color.red = g_random_double_range (0, 1);
  self->color.green = g_random_double_range (0, 1);
  self->color.blue = g_random_double_range (0, 1);
  self->color.alpha = 1;

  self->period = g_random_double_range (0.5, 1);
  self->amplitude = g_random_double_range (10, 25);

  self->stroke = gsk_stroke_new (2);

  update_path (self, 0);

  self->start_time = 0;
  self->tick_cb = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
}

static void
graph_widget_dispose (GObject *object)
{
  GraphWidget *self = GRAPH_WIDGET (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  gsk_stroke_free (self->stroke);

  G_OBJECT_CLASS (graph_widget_parent_class)->dispose (object);
}

static void
graph_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GraphWidget *self = GRAPH_WIDGET (widget);

  gtk_snapshot_append_stroke (snapshot, self->path, self->stroke, &self->color);
}

static void
graph_widget_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = 200;
  else
    *minimum = *natural = 100;
}


static void
graph_widget_class_init (GraphWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = graph_widget_dispose;

  widget_class->snapshot = graph_widget_snapshot;
  widget_class->measure = graph_widget_measure;
}

GtkWidget *
graph_widget_new (void)
{
  return g_object_new (GRAPH_TYPE_WIDGET, NULL);
}
