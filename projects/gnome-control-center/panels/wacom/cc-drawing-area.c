/*
 * Copyright © 2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"
#include <cairo/cairo.h>
#include "cc-drawing-area.h"

typedef struct _CcDrawingArea CcDrawingArea;

struct _CcDrawingArea {
	GtkDrawingArea parent;
	GtkGesture *stylus_gesture;
	cairo_surface_t *surface;
	cairo_t *cr;
};

G_DEFINE_TYPE (CcDrawingArea, cc_drawing_area, GTK_TYPE_DRAWING_AREA)

static void
ensure_drawing_surface (CcDrawingArea *area,
			gint           width,
			gint           height)
{
	if (!area->surface ||
	    cairo_image_surface_get_width (area->surface) != width ||
	    cairo_image_surface_get_height (area->surface) != height) {
		cairo_surface_t *surface;

		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						      width, height);
		if (area->surface) {
			cairo_t *cr;

			cr = cairo_create (surface);
			cairo_set_source_surface (cr, area->surface, 0, 0);
			cairo_paint (cr);

			cairo_surface_destroy (area->surface);
			cairo_destroy (area->cr);
			cairo_destroy (cr);
		}

		area->surface = surface;
		area->cr = cairo_create (surface);
	}
}

static void
cc_drawing_area_map (GtkWidget *widget)
{
	GtkAllocation allocation;

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->map (widget);

	gtk_widget_get_allocation (widget, &allocation);
	ensure_drawing_surface (CC_DRAWING_AREA (widget),
				allocation.width, allocation.height);
}

static void
cc_drawing_area_unmap (GtkWidget *widget)
{
	CcDrawingArea *area = CC_DRAWING_AREA (widget);

	if (area->cr) {
		cairo_destroy (area->cr);
		area->cr = NULL;
	}

	if (area->surface) {
		cairo_surface_destroy (area->surface);
		area->surface = NULL;
	}

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->unmap (widget);
}

static void
draw_cb (GtkDrawingArea *drawing_area,
         cairo_t   *cr,
         gint       width,
         gint       height,
         gpointer   user_data)
{
	CcDrawingArea *area = CC_DRAWING_AREA (drawing_area);

	ensure_drawing_surface (area, width, height);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cairo_set_source_surface (cr, area->surface, 0, 0);
	cairo_paint (cr);

	cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_stroke (cr);
}

static void
stylus_down_cb (CcDrawingArea    *area,
		double            x,
		double            y)
{
	cairo_new_path (area->cr);
}

static void
stylus_motion_cb (CcDrawingArea    *area,
		  double            x,
		  double            y,
		  GtkGestureStylus *gesture)
{
	GdkDeviceTool *tool;
	gdouble pressure;

	tool = gtk_gesture_stylus_get_device_tool (gesture);
	gtk_gesture_stylus_get_axis (gesture,
				     GDK_AXIS_PRESSURE,
				     &pressure);

	if (gdk_device_tool_get_tool_type (tool) == GDK_DEVICE_TOOL_TYPE_ERASER) {
		cairo_set_line_width (area->cr, 10 * pressure);
		cairo_set_operator (area->cr, CAIRO_OPERATOR_DEST_OUT);
	} else {
		cairo_set_line_width (area->cr, 4 * pressure);
		cairo_set_operator (area->cr, CAIRO_OPERATOR_SATURATE);
	}

	cairo_set_source_rgba (area->cr, 0, 0, 0, pressure);
	cairo_line_to (area->cr, x, y);
	cairo_stroke (area->cr);

	cairo_move_to (area->cr, x, y);

	gtk_widget_queue_draw (GTK_WIDGET (area));
}

static void
cc_drawing_area_class_init (CcDrawingAreaClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->map = cc_drawing_area_map;
	widget_class->unmap = cc_drawing_area_unmap;
}

static void
cc_drawing_area_init (CcDrawingArea *area)
{
	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (area), draw_cb, NULL, NULL);
	area->stylus_gesture = gtk_gesture_stylus_new ();
	g_signal_connect_swapped (area->stylus_gesture, "down",
                                  G_CALLBACK (stylus_down_cb), area);
	g_signal_connect_swapped (area->stylus_gesture, "motion",
                                  G_CALLBACK (stylus_motion_cb), area);
	gtk_widget_add_controller (GTK_WIDGET (area),
				   GTK_EVENT_CONTROLLER (area->stylus_gesture));
}

GtkWidget *
cc_drawing_area_new (void)
{
	return g_object_new (CC_TYPE_DRAWING_AREA, NULL);
}
