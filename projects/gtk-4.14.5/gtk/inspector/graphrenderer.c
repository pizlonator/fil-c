/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "graphrenderer.h"

#include "graphdata.h"

#include "gtksnapshot.h"

enum {
  PROP_0,
  PROP_DATA,
  PROP_MINIMUM,
  PROP_MAXIMUM
};

struct _GraphRenderer
{
  GtkWidget parent;

  GraphData *data;
  double minimum;
  double maximum;
};

G_DEFINE_TYPE (GraphRenderer, graph_renderer, GTK_TYPE_WIDGET)

static void
graph_renderer_dispose (GObject *object)
{
  GraphRenderer *self = GRAPH_RENDERER (object);

  g_clear_object (&self->data);

  G_OBJECT_CLASS (graph_renderer_parent_class)->dispose (object);
}

static void
graph_renderer_get_property (GObject    *object,
                             guint       param_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GraphRenderer *self = GRAPH_RENDERER (object);

  switch (param_id)
    {
      case PROP_DATA:
        g_value_set_object (value, self->data);
        break;
      case PROP_MINIMUM:
        g_value_set_double (value, self->minimum);
        break;
      case PROP_MAXIMUM:
        g_value_set_double (value, self->maximum);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
graph_renderer_set_property (GObject      *object,
                             guint         param_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GraphRenderer *self = GRAPH_RENDERER (object);

  switch (param_id)
    {
      case PROP_DATA:
        graph_renderer_set_data (self, g_value_get_object (value));
        break;
      case PROP_MINIMUM:
        if (self->minimum != g_value_get_double (value))
          {
            self->minimum = g_value_get_double (value);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      case PROP_MAXIMUM:
        if (self->maximum != g_value_get_double (value))
          {
            self->maximum = g_value_get_double (value);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

#define MIN_HEIGHT 24
#define MIN_WIDTH (3 * MIN_HEIGHT)

static void
graph_renderer_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = MIN_WIDTH;
  else
    *minimum = *natural = MIN_HEIGHT;
}

static void
graph_renderer_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GraphRenderer *self = GRAPH_RENDERER (widget);
  double minimum, maximum, diff;
  double x, y, width, height;
  cairo_t *cr;
  GdkRGBA color;
  guint i, n;

#define LINE_WIDTH 1.0

  if (self->data == NULL)
    return;

  if (self->minimum == -G_MAXDOUBLE)
    minimum = graph_data_get_minimum (self->data);
  else
    minimum = self->minimum;

  if (self->maximum == G_MAXDOUBLE)
    maximum = graph_data_get_maximum (self->data);
  else
    maximum = self->maximum;

  diff = maximum - minimum;

  gtk_widget_get_color (widget, &color);

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (
                                      0, 0,
                                      gtk_widget_get_width (widget),
                                      gtk_widget_get_height (widget)
                                  ));

  cairo_set_line_width (cr, 1.0);

  x = LINE_WIDTH / 2.0;
  y = LINE_WIDTH / 2.0;
  width = gtk_widget_get_width (widget) - LINE_WIDTH;
  height = gtk_widget_get_height (widget) - LINE_WIDTH;

  cairo_move_to (cr, x, y + height);

  if (diff > 0)
    {
      n = graph_data_get_n_values (self->data);
      for (i = 0; i < n; i++)
        {
          double val = graph_data_get_value (self->data, i);

          val = (val - minimum) / diff;
          val = y + height - val * height;

          cairo_line_to (cr, x + width * i / (n - 1), val);
        }
    }

  cairo_line_to (cr, x + width, y + height);
  cairo_close_path (cr);

  gdk_cairo_set_source_rgba (cr, &color);
  cairo_stroke_preserve (cr);

  color.alpha *= 0.2;
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static void
graph_renderer_class_init (GraphRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = graph_renderer_dispose;
  object_class->get_property = graph_renderer_get_property;
  object_class->set_property = graph_renderer_set_property;

  widget_class->measure = graph_renderer_measure;
  widget_class->snapshot = graph_renderer_snapshot;

  g_object_class_install_property (object_class,
                                   PROP_DATA,
                                   g_param_spec_object ("data", NULL, NULL,
                                                        graph_data_get_type (),
                                                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MINIMUM,
                                   g_param_spec_double ("minimum", NULL, NULL,
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, -G_MAXDOUBLE,
                                                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MINIMUM,
                                   g_param_spec_double ("maximum", NULL, NULL,
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE,
                                                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
graph_renderer_init (GraphRenderer *self)
{
  self->minimum = -G_MAXDOUBLE;
  self->maximum = G_MAXDOUBLE;
}

GraphRenderer *
graph_renderer_new (void)
{
  return g_object_new (graph_renderer_get_type (), NULL);
}

void
graph_renderer_set_data (GraphRenderer *self,
                         GraphData     *data)
{
  if (g_set_object (&self->data, data))
    g_object_notify (G_OBJECT (self), "data");

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
