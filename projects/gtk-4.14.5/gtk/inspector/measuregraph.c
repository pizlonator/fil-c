/*
 * Copyright © 2021 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "measuregraph.h"

/* gdk_texture_new_for_surface() */
#include "gdk/gdktextureprivate.h"

#define MAX_SIZES 2048

typedef struct _Size Size;
struct _Size
{
  int min;
  int nat;
};

struct _GtkInspectorMeasureGraph
{
  GObject parent_instance;

  GdkPaintable *texture;
  Size width;
  Size height;
  Size width_for_height[MAX_SIZES];
  Size height_for_width[MAX_SIZES];
};

struct _GtkInspectorMeasureGraphClass
{
  GObjectClass parent_class;
};

static void
gtk_inspector_measure_graph_ensure_texture (GtkInspectorMeasureGraph *self)
{
  int i, width, height;
  cairo_surface_t *surface;
  cairo_t *cr;

  if (self->texture)
    return;

  if (self->width.nat == 0 || self->height.nat == 0)
    {
      self->texture = gdk_paintable_new_empty (0, 0);
      return;
    }

  width = self->width.nat;
  for (i = 0; i < MAX_SIZES; i++)
    width = MAX (width, self->width_for_height[i].nat);
  width = MIN (width, MAX_SIZES);
  height = self->height.nat;
  for (i = 0; i < MAX_SIZES; i++)
    height = MAX (height, self->height_for_width[i].nat);
  height = MIN (height, MAX_SIZES);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  
  cairo_set_source_rgba (cr, 0.5, 0, 0, 1);
  cairo_rectangle (cr, 0, 0, self->width.min, height);
  cairo_fill (cr);
  cairo_set_source_rgba (cr, 1, 0, 0, 1);
  for (i = self->width.min; i < width; i++)
    cairo_rectangle (cr, i, 0, 1, self->height_for_width[i].min);
  cairo_fill (cr);
  cairo_set_source_rgba (cr, 1, 0, 0, 0.3);
  for (i = self->width.min; i < width; i++)
    cairo_rectangle (cr, i, self->height_for_width[i].min, 1, self->height_for_width[i].nat - self->height_for_width[i].min);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0, 0, 0.5, 1);
  cairo_rectangle (cr, 0, 0, width, self->height.min);
  cairo_fill (cr);
  cairo_set_source_rgba (cr, 0, 0, 1, 1);
  for (i = self->height.min; i < height; i++)
    cairo_rectangle (cr, 0, i, self->width_for_height[i].min, 1);
  cairo_fill (cr);
  cairo_set_source_rgba (cr, 0, 0, 1, 0.3);
  for (i = self->height.min; i < height; i++)
    cairo_rectangle (cr, self->width_for_height[i].min, i, self->width_for_height[i].nat - self->width_for_height[i].min, 1);
  cairo_fill (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_rectangle (cr, self->width.nat, 0, 1, height);
  cairo_rectangle (cr, 0, self->height.nat, width, 1);
  cairo_fill (cr);

  cairo_destroy (cr);
  self->texture = GDK_PAINTABLE (gdk_texture_new_for_surface (surface));
  cairo_surface_destroy (surface);
}

static void
gtk_inspector_measure_graph_paintable_snapshot (GdkPaintable *paintable,
                                                GdkSnapshot  *snapshot,
                                                double        width,
                                                double        height)
{
  GtkInspectorMeasureGraph *self = GTK_INSPECTOR_MEASURE_GRAPH (paintable);

  gtk_inspector_measure_graph_ensure_texture (self);

  if (self->texture == NULL)
    return;

  gdk_paintable_snapshot (self->texture, snapshot, width, height);
}

static int
gtk_inspector_measure_graph_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkInspectorMeasureGraph *self = GTK_INSPECTOR_MEASURE_GRAPH (paintable);

  gtk_inspector_measure_graph_ensure_texture (self);

  return gdk_paintable_get_intrinsic_width (self->texture);
}

static int
gtk_inspector_measure_graph_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkInspectorMeasureGraph *self = GTK_INSPECTOR_MEASURE_GRAPH (paintable);

  gtk_inspector_measure_graph_ensure_texture (self);

  return gdk_paintable_get_intrinsic_height (self->texture);
}

static double
gtk_inspector_measure_graph_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkInspectorMeasureGraph *self = GTK_INSPECTOR_MEASURE_GRAPH (paintable);

  gtk_inspector_measure_graph_ensure_texture (self);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->texture);
}

static void
gtk_inspector_measure_graph_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_inspector_measure_graph_paintable_snapshot;
  iface->get_intrinsic_width = gtk_inspector_measure_graph_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_inspector_measure_graph_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_inspector_measure_graph_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (GtkInspectorMeasureGraph, gtk_inspector_measure_graph, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_inspector_measure_graph_paintable_init))

static void
gtk_inspector_measure_graph_dispose (GObject *object)
{
  GtkInspectorMeasureGraph *self = GTK_INSPECTOR_MEASURE_GRAPH (object);

  g_clear_object (&self->texture);

  G_OBJECT_CLASS (gtk_inspector_measure_graph_parent_class)->dispose (object);
}

static void
gtk_inspector_measure_graph_class_init (GtkInspectorMeasureGraphClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_inspector_measure_graph_dispose;
}

static void
gtk_inspector_measure_graph_init (GtkInspectorMeasureGraph *self)
{
}

GtkInspectorMeasureGraph *
gtk_inspector_measure_graph_new (void)
{
  return g_object_new (GTK_TYPE_INSPECTOR_MEASURE_GRAPH, NULL);
}

void
gtk_inspector_measure_graph_clear (GtkInspectorMeasureGraph *self)
{
  g_clear_object (&self->texture);

  memset (&self->width, 0, sizeof (self->width));
  memset (&self->height, 0, sizeof (self->height));
  memset (&self->width_for_height, 0, sizeof (self->width_for_height));
  memset (&self->height_for_width, 0, sizeof (self->height_for_width));

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

void
gtk_inspector_measure_graph_measure (GtkInspectorMeasureGraph *self,
                                     GtkWidget                *widget)
{
  int i;

  g_clear_object (&self->texture);

  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &self->width.min, &self->width.nat, NULL, NULL);
  gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1 ,&self->height.min, &self->height.nat, NULL, NULL);

  memset (&self->width_for_height, 0, sizeof (Size) * MIN (self->height.min, MAX_SIZES));
  for (i = self->height.min; i < MAX_SIZES; i++)
    gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, i, &self->width_for_height[i].min, &self->width_for_height[i].nat, NULL, NULL);
  memset (&self->height_for_width, 0, sizeof (Size) * MIN (self->width.min, MAX_SIZES));
  for (i = self->width.min; i < MAX_SIZES; i++)
    gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, i, &self->height_for_width[i].min, &self->height_for_width[i].nat, NULL, NULL);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

GdkTexture *
gtk_inspector_measure_graph_get_texture (GtkInspectorMeasureGraph *self)
{
  gtk_inspector_measure_graph_ensure_texture (self);

  if (!GDK_IS_TEXTURE (self->texture))
    return NULL;

  return GDK_TEXTURE (self->texture);
}

