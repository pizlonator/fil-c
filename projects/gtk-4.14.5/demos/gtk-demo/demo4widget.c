#include <math.h>
#include "demo4widget.h"
#include "hsla.h"

enum
{
  PROP_0,
  PROP_PROGRESS,
};

struct _Demo4Widget
{
  GtkWidget parent_instance;
  PangoLayout *layout;
  GskColorStop stops[8];
  gsize n_stops;
  double progress;

  guint tick;
};

struct _Demo4WidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Demo4Widget, demo4_widget, GTK_TYPE_WIDGET)

static void
rotate_color (GdkRGBA *rgba)
{
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, rgba);
  hsla.hue -= 1;
  _gdk_rgba_init_from_hsla (rgba, &hsla);
}

static gboolean
rotate_colors (GtkWidget     *widget,
               GdkFrameClock *clock,
               gpointer       user_data)
{
  Demo4Widget *self = DEMO4_WIDGET (widget);

  for (unsigned int i = 0; i < self->n_stops; i++)
    rotate_color (&self->stops[i].color);

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
demo4_widget_init (Demo4Widget *self)
{
  PangoFontDescription *desc;

  self->progress = 0.5;

  self->n_stops = 8;
  self->stops[0].offset = 0;
  self->stops[0].color = (GdkRGBA) { 1, 0, 0, 1 };

  for (unsigned int i = 1; i < self->n_stops; i++)
    {
      GdkHSLA hsla;

      self->stops[i].offset = i / (double)(self->n_stops - 1);
      _gdk_hsla_init_from_rgba (&hsla, &self->stops[i - 1].color);
      hsla.hue += 360.0 / (double)(self->n_stops - 1);
      _gdk_rgba_init_from_hsla (&self->stops[i].color, &hsla);
    }

  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "123");
  desc = pango_font_description_from_string ("Cantarell Bold 210");
  pango_layout_set_font_description (self->layout, desc);
  pango_font_description_free (desc);

  self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), rotate_colors, NULL, NULL);
}

static void
demo4_widget_dispose (GObject *object)
{
  Demo4Widget *self = DEMO4_WIDGET (object);

  g_clear_object (&self->layout);
  gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);

  G_OBJECT_CLASS (demo4_widget_parent_class)->dispose (object);
}

static void
demo4_widget_snapshot_content (GtkWidget   *widget,
                               GtkSnapshot *snapshot,
                               GskMaskMode  mode)
{
  Demo4Widget *self = DEMO4_WIDGET (widget);
  int width, height, layout_width, layout_height;
  double scale;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_push_mask (snapshot, mode);
  pango_layout_get_pixel_size (self->layout, &layout_width, &layout_height);
  scale = MIN ((double) width / layout_width, (double) height / layout_height);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT ((width - scale * layout_width) / 2,
                                                (height - scale * layout_height) / 2));
  gtk_snapshot_scale (snapshot, scale, scale);
  gtk_snapshot_append_layout (snapshot, self->layout, &(GdkRGBA) { 0, 0, 0, 1 });
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (width, height),
                                       self->stops,
                                       self->n_stops);
  gtk_snapshot_pop (snapshot);
}

static void
demo4_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  Demo4Widget *self = DEMO4_WIDGET (widget);
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_LUMINANCE);
  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (width, 0),
                                       (GskColorStop[2]) {
                                         { MAX (0.0, self->progress - 5.0 / width), { 1, 1, 1, 1 } },
                                         { MIN (1.0, self->progress + 5.0 / width), { 0, 0, 0, 1 } }
                                       }, 2);
  gtk_snapshot_pop (snapshot);
  demo4_widget_snapshot_content (widget, snapshot, GSK_MASK_MODE_INVERTED_ALPHA);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_LUMINANCE);
  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (width, 0),
                                       (GskColorStop[2]) {
                                         { MAX (0.0, self->progress - 5.0 / width), { 1, 1, 1, 1 } },
                                         { MIN (1.0, self->progress + 5.0 / width), { 0, 0, 0, 1 } }
                                       }, 2);
  gtk_snapshot_pop (snapshot);
  demo4_widget_snapshot_content (widget, snapshot, GSK_MASK_MODE_ALPHA);
  gtk_snapshot_pop (snapshot);
}

static void
demo4_widget_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  Demo4Widget *self = DEMO4_WIDGET (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      self->progress = g_value_get_double (value);
      gtk_widget_queue_draw (GTK_WIDGET (object));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo4_widget_get_property (GObject     *object,
                           guint        prop_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  Demo4Widget *self = DEMO4_WIDGET (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, self->progress);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo4_widget_class_init (Demo4WidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo4_widget_dispose;
  object_class->get_property = demo4_widget_get_property;
  object_class->set_property = demo4_widget_set_property;

  widget_class->snapshot = demo4_widget_snapshot;

  g_object_class_install_property (object_class, PROP_PROGRESS,
      g_param_spec_double ("progress", NULL, NULL,
                           0.0, 1.0, 0.5,
                           G_PARAM_READWRITE));
}

GtkWidget *
demo4_widget_new (void)
{
  return g_object_new (DEMO4_TYPE_WIDGET, NULL);
}

