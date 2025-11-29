/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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
#include "gtk/gtk.h"
#include "gtkmagnifierprivate.h"
#include "gtkwidgetprivate.h"
#include "gtksnapshot.h"
#include "gtkcssboxesprivate.h"

enum {
  PROP_INSPECTED = 1,
  PROP_RESIZE,
  PROP_MAGNIFICATION
};

struct _GtkMagnifier
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  double magnification;
  int x;
  int y;
  gboolean resize;
};

typedef struct
{
  GtkWidgetClass parent_class;
} GtkMagnifierClass;

G_DEFINE_TYPE (GtkMagnifier, gtk_magnifier, GTK_TYPE_WIDGET)

static void
_gtk_magnifier_set_property (GObject      *object,
                             guint         param_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (object);

  switch (param_id)
    {
    case PROP_INSPECTED:
      _gtk_magnifier_set_inspected (magnifier, g_value_get_object (value));
      break;

    case PROP_MAGNIFICATION:
      _gtk_magnifier_set_magnification (magnifier, g_value_get_double (value));
      break;

    case PROP_RESIZE:
      _gtk_magnifier_set_resize (magnifier, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
_gtk_magnifier_get_property (GObject    *object,
                             guint       param_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (object);

  switch (param_id)
    {
    case PROP_INSPECTED:
      g_value_set_object (value, gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (magnifier->paintable)));
      break;

    case PROP_MAGNIFICATION:
      g_value_set_double (value, magnifier->magnification);
      break;

    case PROP_RESIZE:
      g_value_set_boolean (value, magnifier->resize);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_magnifier_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (widget);
  double width, height, paintable_width, paintable_height;
  GtkWidget *inspected;
  GtkCssBoxes boxes;
  const graphene_rect_t *content_rect;
  const graphene_rect_t *border_rect;
  graphene_point_t offset;

  inspected = gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (magnifier->paintable));
  if (inspected == NULL)
    return;

  gtk_css_boxes_init (&boxes, inspected);
  content_rect = gtk_css_boxes_get_content_rect (&boxes);
  border_rect = gtk_css_boxes_get_border_rect (&boxes);

  offset.x = content_rect->origin.x - border_rect->origin.x;
  offset.y = content_rect->origin.y - border_rect->origin.y;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  paintable_width = gdk_paintable_get_intrinsic_width (magnifier->paintable);
  paintable_height = gdk_paintable_get_intrinsic_height (magnifier->paintable);
  if (paintable_width <= 0.0 || paintable_height <= 0.0)
    return;

  gtk_snapshot_save (snapshot);
  if (!magnifier->resize)
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2, height / 2));
  gtk_snapshot_scale (snapshot, magnifier->magnification, magnifier->magnification);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                          - CLAMP (magnifier->x + offset.x, 0, paintable_width),
                          - CLAMP (magnifier->y + offset.y, 0, paintable_height)));

  gdk_paintable_snapshot (magnifier->paintable, snapshot, paintable_width, paintable_height);
  gtk_snapshot_restore (snapshot);
}

static void
gtk_magnifier_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (widget);
  int size;

  if (magnifier->resize)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        size = magnifier->magnification * gdk_paintable_get_intrinsic_width (magnifier->paintable);
      else
        size = magnifier->magnification * gdk_paintable_get_intrinsic_height (magnifier->paintable);
    }
  else
    size = 0;

  *minimum = size;
  *natural = size;
}

static void
gtk_magnifier_dispose (GObject *object)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (object);

  if (magnifier->paintable)
    _gtk_magnifier_set_inspected (magnifier, NULL);

  g_clear_object (&magnifier->paintable);

  G_OBJECT_CLASS (gtk_magnifier_parent_class)->dispose (object);
}

static void
gtk_magnifier_class_init (GtkMagnifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = _gtk_magnifier_set_property;
  object_class->get_property = _gtk_magnifier_get_property;
  object_class->dispose = gtk_magnifier_dispose;

  widget_class->snapshot = gtk_magnifier_snapshot;
  widget_class->measure = gtk_magnifier_measure;

  g_object_class_install_property (object_class,
                                   PROP_INSPECTED,
                                   g_param_spec_object ("inspected", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MAGNIFICATION,
                                   g_param_spec_double ("magnification", NULL, NULL,
                                                        1, G_MAXDOUBLE, 1,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_RESIZE,
                                   g_param_spec_boolean ("resize", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  gtk_widget_class_set_css_name (widget_class, "magnifier");
}

static void
gtk_magnifier_init (GtkMagnifier *magnifier)
{
  GtkWidget *widget = GTK_WIDGET (magnifier);

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  magnifier->magnification = 1;
  magnifier->resize = FALSE;
  magnifier->paintable = gtk_widget_paintable_new (NULL);
  g_signal_connect_swapped (magnifier->paintable, "invalidate-contents", G_CALLBACK (gtk_widget_queue_draw), magnifier);
  g_signal_connect_swapped (magnifier->paintable, "invalidate-size", G_CALLBACK (gtk_widget_queue_resize), magnifier);
}

GtkWidget *
_gtk_magnifier_new (GtkWidget *inspected)
{
  g_return_val_if_fail (GTK_IS_WIDGET (inspected), NULL);

  return g_object_new (GTK_TYPE_MAGNIFIER,
                       "inspected", inspected,
                       NULL);
}

GtkWidget *
_gtk_magnifier_get_inspected (GtkMagnifier *magnifier)
{
  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), NULL);

  return gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (magnifier->paintable));
}

void
_gtk_magnifier_set_inspected (GtkMagnifier *magnifier,
                              GtkWidget    *inspected)
{
  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));
  g_return_if_fail (inspected == NULL || GTK_IS_WIDGET (inspected));

  gtk_widget_paintable_set_widget (GTK_WIDGET_PAINTABLE (magnifier->paintable), inspected);

  g_object_notify (G_OBJECT (magnifier), "inspected");
}

void
_gtk_magnifier_set_coords (GtkMagnifier *magnifier,
                           double        x,
                           double        y)
{
  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  if (magnifier->x == x && magnifier->y == y)
    return;

  magnifier->x = x;
  magnifier->y = y;

  gtk_widget_queue_draw (GTK_WIDGET (magnifier));
}

void
_gtk_magnifier_get_coords (GtkMagnifier *magnifier,
                           double       *x,
                           double       *y)
{
  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  if (x)
    *x = magnifier->x;

  if (y)
    *y = magnifier->y;
}

void
_gtk_magnifier_set_magnification (GtkMagnifier *magnifier,
                                  double        magnification)
{
  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  if (magnifier->magnification == magnification)
    return;

  magnifier->magnification = magnification;
  g_object_notify (G_OBJECT (magnifier), "magnification");

  if (magnifier->resize)
    gtk_widget_queue_resize (GTK_WIDGET (magnifier));

  gtk_widget_queue_draw (GTK_WIDGET (magnifier));
}

double
_gtk_magnifier_get_magnification (GtkMagnifier *magnifier)
{
  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), 1);

  return magnifier->magnification;
}

void
_gtk_magnifier_set_resize (GtkMagnifier *magnifier,
                           gboolean      resize)
{
  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  if (magnifier->resize == resize)
    return;

  magnifier->resize = resize;

  gtk_widget_queue_resize (GTK_WIDGET (magnifier));
}

gboolean
_gtk_magnifier_get_resize (GtkMagnifier *magnifier)
{
  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), FALSE);

  return magnifier->resize;
}
