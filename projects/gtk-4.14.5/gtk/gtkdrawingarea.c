/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkdrawingarea.h"
#include "gtkmarshalers.h"
#include "gdk/gdkmarshalers.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

typedef struct _GtkDrawingAreaPrivate GtkDrawingAreaPrivate;

struct _GtkDrawingAreaPrivate {
  int content_width;
  int content_height;

  GtkDrawingAreaDrawFunc draw_func;
  gpointer draw_func_target;
  GDestroyNotify draw_func_target_destroy_notify;
};

enum {
  PROP_0,
  PROP_CONTENT_WIDTH,
  PROP_CONTENT_HEIGHT,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

enum {
  RESIZE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

/**
 * GtkDrawingArea:
 *
 * `GtkDrawingArea` is a widget that allows drawing with cairo.
 *
 * ![An example GtkDrawingArea](drawingarea.png)
 *
 * It’s essentially a blank widget; you can draw on it. After
 * creating a drawing area, the application may want to connect to:
 *
 * - The [signal@Gtk.Widget::realize] signal to take any necessary actions
 *   when the widget is instantiated on a particular display.
 *   (Create GDK resources in response to this signal.)
 *
 * - The [signal@Gtk.DrawingArea::resize] signal to take any necessary
 *   actions when the widget changes size.
 *
 * - Call [method@Gtk.DrawingArea.set_draw_func] to handle redrawing the
 *   contents of the widget.
 *
 * The following code portion demonstrates using a drawing
 * area to display a circle in the normal widget foreground
 * color.
 *
 * ## Simple GtkDrawingArea usage
 *
 * ```c
 * static void
 * draw_function (GtkDrawingArea *area,
 *                cairo_t        *cr,
 *                int             width,
 *                int             height,
 *                gpointer        data)
 * {
 *   GdkRGBA color;
 *
 *   cairo_arc (cr,
 *              width / 2.0, height / 2.0,
 *              MIN (width, height) / 2.0,
 *              0, 2 * G_PI);
 *
 *   gtk_widget_get_color (GTK_WIDGET (area),
 *                         &color);
 *   gdk_cairo_set_source_rgba (cr, &color);
 *
 *   cairo_fill (cr);
 * }
 *
 * int
 * main (int argc, char **argv)
 * {
 *   gtk_init ();
 *
 *   GtkWidget *area = gtk_drawing_area_new ();
 *   gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (area), 100);
 *   gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (area), 100);
 *   gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (area),
 *                                   draw_function,
 *                                   NULL, NULL);
 *   return 0;
 * }
 * ```
 *
 * The draw function is normally called when a drawing area first comes
 * onscreen, or when it’s covered by another window and then uncovered.
 * You can also force a redraw by adding to the “damage region” of the
 * drawing area’s window using [method@Gtk.Widget.queue_draw].
 * This will cause the drawing area to call the draw function again.
 *
 * The available routines for drawing are documented in the
 * [Cairo documentation](https://www.cairographics.org/manual/); GDK
 * offers additional API to integrate with Cairo, like [func@Gdk.cairo_set_source_rgba]
 * or [func@Gdk.cairo_set_source_pixbuf].
 *
 * To receive mouse events on a drawing area, you will need to use
 * event controllers. To receive keyboard events, you will need to set
 * the “can-focus” property on the drawing area, and you should probably
 * draw some user-visible indication that the drawing area is focused.
 *
 * If you need more complex control over your widget, you should consider
 * creating your own `GtkWidget` subclass.
 */

G_DEFINE_TYPE_WITH_PRIVATE (GtkDrawingArea, gtk_drawing_area, GTK_TYPE_WIDGET)

static void
gtk_drawing_area_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkDrawingArea *self = GTK_DRAWING_AREA (gobject);

  switch (prop_id)
    {
    case PROP_CONTENT_WIDTH:
      gtk_drawing_area_set_content_width (self, g_value_get_int (value));
      break;

    case PROP_CONTENT_HEIGHT:
      gtk_drawing_area_set_content_height (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_drawing_area_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkDrawingArea *self = GTK_DRAWING_AREA (gobject);
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTENT_WIDTH:
      g_value_set_int (value, priv->content_width);
      break;

    case PROP_CONTENT_HEIGHT:
      g_value_set_int (value, priv->content_height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_drawing_area_dispose (GObject *object)
{
  GtkDrawingArea *self = GTK_DRAWING_AREA (object);
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  if (priv->draw_func_target_destroy_notify != NULL)
    priv->draw_func_target_destroy_notify (priv->draw_func_target);

  priv->draw_func = NULL;
  priv->draw_func_target = NULL;
  priv->draw_func_target_destroy_notify = NULL;

  G_OBJECT_CLASS (gtk_drawing_area_parent_class)->dispose (object);
}

static void
gtk_drawing_area_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkDrawingArea *self = GTK_DRAWING_AREA (widget);
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = *natural = priv->content_width;
    }
  else
    {
      *minimum = *natural = priv->content_height;
    }
}

static void
gtk_drawing_area_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  g_signal_emit (widget, signals[RESIZE], 0, width, height);
}

static void
gtk_drawing_area_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkDrawingArea *self = GTK_DRAWING_AREA (widget);
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);
  cairo_t *cr;
  int width, height;

  if (!priv->draw_func)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);


  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (
                                      0, 0,
                                      width, height
                                  ));
  priv->draw_func (self,
                   cr,
                   width, height,
                   priv->draw_func_target);
  cairo_destroy (cr);
}

static void
gtk_drawing_area_class_init (GtkDrawingAreaClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_drawing_area_set_property;
  gobject_class->get_property = gtk_drawing_area_get_property;
  gobject_class->dispose = gtk_drawing_area_dispose;

  widget_class->measure = gtk_drawing_area_measure;
  widget_class->size_allocate = gtk_drawing_area_size_allocate;
  widget_class->snapshot = gtk_drawing_area_snapshot;

  /**
   * GtkDrawingArea:content-width: (attributes org.gtk.Property.get=gtk_drawing_area_get_content_width org.gtk.Property.set=gtk_drawing_area_set_content_width)
   *
   * The content width.
   */
  props[PROP_CONTENT_WIDTH] =
    g_param_spec_int ("content-width", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDrawingArea:content-height: (attributes org.gtk.Property.get=gtk_drawing_area_get_content_height org.gtk.Property.set=gtk_drawing_area_set_content_height)
   *
   * The content height.
   */
  props[PROP_CONTENT_HEIGHT] =
    g_param_spec_int ("content-height", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  /**
   * GtkDrawingArea::resize:
   * @area: the `GtkDrawingArea` that emitted the signal
   * @width: the width of the viewport
   * @height: the height of the viewport
   *
   * Emitted once when the widget is realized, and then each time the widget
   * is changed while realized.
   *
   * This is useful in order to keep state up to date with the widget size,
   * like for instance a backing surface.
   */
  signals[RESIZE] =
    g_signal_new (I_("resize"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkDrawingAreaClass, resize),
                  NULL, NULL,
                  _gdk_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  g_signal_set_va_marshaller (signals[RESIZE],
                              G_TYPE_FROM_CLASS (class),
                              _gdk_marshal_VOID__INT_INTv);
}

static void
gtk_drawing_area_init (GtkDrawingArea *darea)
{
  gtk_widget_set_focusable (GTK_WIDGET (darea), FALSE);
}

/**
 * gtk_drawing_area_new:
 *
 * Creates a new drawing area.
 *
 * Returns: a new `GtkDrawingArea`
 */
GtkWidget*
gtk_drawing_area_new (void)
{
  return g_object_new (GTK_TYPE_DRAWING_AREA, NULL);
}

/**
 * gtk_drawing_area_set_content_width: (attributes org.gtk.Method.set_property=content-width)
 * @self: a `GtkDrawingArea`
 * @width: the width of contents
 *
 * Sets the desired width of the contents of the drawing area.
 *
 * Note that because widgets may be allocated larger sizes than they
 * requested, it is possible that the actual width passed to your draw
 * function is larger than the width set here. You can use
 * [method@Gtk.Widget.set_halign] to avoid that.
 *
 * If the width is set to 0 (the default), the drawing area may disappear.
 */
void
gtk_drawing_area_set_content_width (GtkDrawingArea *self,
                                    int             width)
{
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  g_return_if_fail (GTK_IS_DRAWING_AREA (self));
  g_return_if_fail (width >= 0);

  if (priv->content_width == width)
    return;

  priv->content_width = width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_WIDTH]);
}

/**
 * gtk_drawing_area_get_content_width: (attributes org.gtk.Method.get_property=content-width)
 * @self: a `GtkDrawingArea`
 *
 * Retrieves the content width of the `GtkDrawingArea`.
 *
 * Returns: The width requested for content of the drawing area
 */
int
gtk_drawing_area_get_content_width (GtkDrawingArea *self)
{
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_DRAWING_AREA (self), 0);

  return priv->content_width;
}

/**
 * gtk_drawing_area_set_content_height: (attributes org.gtk.Method.set_property=content-height)
 * @self: a `GtkDrawingArea`
 * @height: the height of contents
 *
 * Sets the desired height of the contents of the drawing area.
 *
 * Note that because widgets may be allocated larger sizes than they
 * requested, it is possible that the actual height passed to your draw
 * function is larger than the height set here. You can use
 * [method@Gtk.Widget.set_valign] to avoid that.
 *
 * If the height is set to 0 (the default), the drawing area may disappear.
 */
void
gtk_drawing_area_set_content_height (GtkDrawingArea *self,
                                     int             height)
{
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  g_return_if_fail (GTK_IS_DRAWING_AREA (self));
  g_return_if_fail (height >= 0);

  if (priv->content_height == height)
    return;

  priv->content_height = height;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_HEIGHT]);
}

/**
 * gtk_drawing_area_get_content_height: (attributes org.gtk.Method.get_property=content-height)
 * @self: a `GtkDrawingArea`
 *
 * Retrieves the content height of the `GtkDrawingArea`.
 *
 * Returns: The height requested for content of the drawing area
 */
int
gtk_drawing_area_get_content_height (GtkDrawingArea *self)
{
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_DRAWING_AREA (self), 0);

  return priv->content_height;
}

/**
 * gtk_drawing_area_set_draw_func:
 * @self: a `GtkDrawingArea`
 * @draw_func: (nullable): callback that lets you draw
 *   the drawing area's contents
 * @user_data: (closure): user data passed to @draw_func
 * @destroy: destroy notifier for @user_data
 *
 * Setting a draw function is the main thing you want to do when using
 * a drawing area.
 *
 * The draw function is called whenever GTK needs to draw the contents
 * of the drawing area to the screen.
 *
 * The draw function will be called during the drawing stage of GTK.
 * In the drawing stage it is not allowed to change properties of any
 * GTK widgets or call any functions that would cause any properties
 * to be changed. You should restrict yourself exclusively to drawing
 * your contents in the draw function.
 *
 * If what you are drawing does change, call [method@Gtk.Widget.queue_draw]
 * on the drawing area. This will cause a redraw and will call @draw_func again.
 */
void
gtk_drawing_area_set_draw_func (GtkDrawingArea         *self,
                                GtkDrawingAreaDrawFunc  draw_func,
                                gpointer                user_data,
                                GDestroyNotify          destroy)
{
  GtkDrawingAreaPrivate *priv = gtk_drawing_area_get_instance_private (self);

  g_return_if_fail (GTK_IS_DRAWING_AREA (self));

  if (priv->draw_func_target_destroy_notify != NULL)
    priv->draw_func_target_destroy_notify (priv->draw_func_target);

  priv->draw_func = draw_func;
  priv->draw_func_target = user_data;
  priv->draw_func_target_destroy_notify = destroy;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
