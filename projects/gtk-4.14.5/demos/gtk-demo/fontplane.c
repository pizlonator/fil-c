/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "fontplane.h"

enum {
  PROP_0,
  PROP_WEIGHT_ADJUSTMENT,
  PROP_WIDTH_ADJUSTMENT
};

G_DEFINE_TYPE (GtkFontPlane, gtk_font_plane, GTK_TYPE_WIDGET)

static double
adjustment_get_normalized_value (GtkAdjustment *adj)
{
  return (gtk_adjustment_get_value (adj) - gtk_adjustment_get_lower (adj)) /
        (gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj));
}

static void
val_to_xy (GtkFontPlane *plane,
           int           *x,
           int           *y)
{
  double u, v;
  int width, height;

  width = gtk_widget_get_width (GTK_WIDGET (plane));
  height = gtk_widget_get_height (GTK_WIDGET (plane));

  u = adjustment_get_normalized_value (plane->width_adj);
  v = adjustment_get_normalized_value (plane->weight_adj);

  *x = CLAMP (width * u, 0, width - 1);
  *y = CLAMP (height * (1 - v), 0, height - 1);
}

static void
plane_snapshot (GtkWidget   *widget,
                GtkSnapshot *snapshot)
{
  GtkFontPlane *plane = GTK_FONT_PLANE (widget);
  int x, y;
  int width, height;
  cairo_t *cr;

  val_to_xy (plane, &x, &y);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_paint (cr);

  cairo_move_to (cr, 0,     y + 0.5);
  cairo_line_to (cr, width, y + 0.5);

  cairo_move_to (cr, x + 0.5, 0);
  cairo_line_to (cr, x + 0.5, height);

  if (gtk_widget_has_visible_focus (widget))
    {
      cairo_set_line_width (cr, 3.0);
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6);
      cairo_stroke_preserve (cr);

      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8);
      cairo_stroke (cr);
    }
  else
    {
      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.8);
      cairo_stroke (cr);
    }

  cairo_destroy (cr);
}

static void
set_cross_cursor (GtkWidget *widget,
                  gboolean   enabled)
{
  if (enabled)
    gtk_widget_set_cursor_from_name (widget, "crosshair");
  else
    gtk_widget_set_cursor (widget, NULL);
}

static void
adj_changed (GtkFontPlane *plane)
{
  gtk_widget_queue_draw (GTK_WIDGET (plane));
}

static void
adjustment_set_normalized_value (GtkAdjustment *adj,
                                 double         val)
{
  gtk_adjustment_set_value (adj,
      gtk_adjustment_get_lower (adj) +
          val * (gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj)));
}

static void
update_value (GtkFontPlane *plane,
              int            x,
              int            y)
{
  GtkWidget *widget = GTK_WIDGET (plane);
  double u, v;

  u = CLAMP (x * (1.0 / gtk_widget_get_width (widget)), 0, 1);
  v = CLAMP (1 - y * (1.0 / gtk_widget_get_height (widget)), 0, 1);

  adjustment_set_normalized_value (plane->width_adj, u);
  adjustment_set_normalized_value (plane->weight_adj, v);

  gtk_widget_queue_draw (widget);
}

static void
plane_drag_gesture_begin (GtkGestureDrag *gesture,
                          double          start_x,
                          double          start_y,
                          GtkFontPlane  *plane)
{
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  set_cross_cursor (GTK_WIDGET (plane), TRUE);
  update_value (plane, start_x, start_y);
  gtk_widget_grab_focus (GTK_WIDGET (plane));
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
plane_drag_gesture_update (GtkGestureDrag *gesture,
                           double          offset_x,
                           double          offset_y,
                           GtkFontPlane  *plane)
{
  double start_x, start_y;

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture),
                                    &start_x, &start_y);
  update_value (plane, start_x + offset_x, start_y + offset_y);
}

static void
plane_drag_gesture_end (GtkGestureDrag *gesture,
                        double          offset_x,
                        double          offset_y,
                        GtkFontPlane  *plane)
{
  set_cross_cursor (GTK_WIDGET (plane), FALSE);
}

static void
gtk_font_plane_init (GtkFontPlane *plane)
{
  GtkGesture *gesture;

  gesture = gtk_gesture_drag_new ();
  g_signal_connect (gesture, "drag-begin",
		    G_CALLBACK (plane_drag_gesture_begin), plane);
  g_signal_connect (gesture, "drag-update",
		    G_CALLBACK (plane_drag_gesture_update), plane);
  g_signal_connect (gesture, "drag-end",
		    G_CALLBACK (plane_drag_gesture_end), plane);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_widget_add_controller (GTK_WIDGET (plane), GTK_EVENT_CONTROLLER (gesture));
}

static void
plane_finalize (GObject *object)
{
  GtkFontPlane *plane = GTK_FONT_PLANE (object);

  g_clear_object (&plane->weight_adj);
  g_clear_object (&plane->width_adj);

  G_OBJECT_CLASS (gtk_font_plane_parent_class)->finalize (object);
}

static void
plane_set_property (GObject      *object,
		    guint         prop_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
  GtkFontPlane *plane = GTK_FONT_PLANE (object);
  GtkAdjustment *adjustment;

  switch (prop_id)
    {
    case PROP_WEIGHT_ADJUSTMENT:
      adjustment = GTK_ADJUSTMENT (g_value_get_object (value));
      if (adjustment)
	{
	  plane->weight_adj = g_object_ref_sink (adjustment);
	  g_signal_connect_swapped (adjustment, "value-changed", G_CALLBACK (adj_changed), plane);
	}
      break;
    case PROP_WIDTH_ADJUSTMENT:
      adjustment = GTK_ADJUSTMENT (g_value_get_object (value));
      if (adjustment)
	{
	  plane->width_adj = g_object_ref_sink (adjustment);
	  g_signal_connect_swapped (adjustment, "value-changed", G_CALLBACK (adj_changed), plane);
	}
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_font_plane_class_init (GtkFontPlaneClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = plane_finalize;
  object_class->set_property = plane_set_property;

  widget_class->snapshot = plane_snapshot;

  g_object_class_install_property (object_class,
                                   PROP_WEIGHT_ADJUSTMENT,
                                   g_param_spec_object ("weight-adjustment",
                                                        NULL,
                                                        NULL,
							GTK_TYPE_ADJUSTMENT,
							G_PARAM_WRITABLE |
							G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_WIDTH_ADJUSTMENT,
                                   g_param_spec_object ("width-adjustment",
                                                        NULL,
                                                        NULL,
							GTK_TYPE_ADJUSTMENT,
							G_PARAM_WRITABLE |
							G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
gtk_font_plane_new (GtkAdjustment *weight_adj,
                     GtkAdjustment *width_adj)
{
  return g_object_new (GTK_TYPE_FONT_PLANE,
                       "weight-adjustment", weight_adj,
                       "width-adjustment", width_adj,
                       NULL);
}
