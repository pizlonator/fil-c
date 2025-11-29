/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>


#include "gtkcssboxesimplprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtknativeprivate.h"
#include "gtkwidgetprivate.h"

#include "gdk/gdksurfaceprivate.h"

typedef struct _GtkNativePrivate
{
  gulong update_handler_id;
  gulong layout_handler_id;
  gulong scale_changed_handler_id;
} GtkNativePrivate;

static GQuark quark_gtk_native_private;

/**
 * GtkNative:
 *
 * `GtkNative` is the interface implemented by all widgets that have
 * their own `GdkSurface`.
 *
 * The obvious example of a `GtkNative` is `GtkWindow`.
 *
 * Every widget that is not itself a `GtkNative` is contained in one,
 * and you can get it with [method@Gtk.Widget.get_native].
 *
 * To get the surface of a `GtkNative`, use [method@Gtk.Native.get_surface].
 * It is also possible to find the `GtkNative` to which a surface
 * belongs, with [func@Gtk.Native.get_for_surface].
 *
 * In addition to a [class@Gdk.Surface], a `GtkNative` also provides
 * a [class@Gsk.Renderer] for rendering on that surface. To get the
 * renderer, use [method@Gtk.Native.get_renderer].
 */

G_DEFINE_INTERFACE (GtkNative, gtk_native, GTK_TYPE_WIDGET)

static GskRenderer *
gtk_native_default_get_renderer (GtkNative *self)
{
  return NULL;
}

static void
gtk_native_default_get_surface_transform (GtkNative *self,
                                          double    *x,
                                          double    *y)
{
  *x = 0;
  *y = 0;
}

static void
gtk_native_default_layout (GtkNative *self,
                           int        width,
                           int        height)
{
}

static void
gtk_native_default_init (GtkNativeInterface *iface)
{
  iface->get_renderer = gtk_native_default_get_renderer;
  iface->get_surface_transform = gtk_native_default_get_surface_transform;
  iface->layout = gtk_native_default_layout;

  quark_gtk_native_private = g_quark_from_static_string ("gtk-native-private");
}

static void
frame_clock_update_cb (GdkFrameClock *clock,
                       GtkNative     *native)
{
  if (GTK_IS_ROOT (native))
    gtk_css_node_validate (gtk_widget_get_css_node (GTK_WIDGET (native)));
}

static void
gtk_native_layout (GtkNative *self,
                   int        width,
                   int        height)
{
  GTK_NATIVE_GET_IFACE (self)->layout (self, width, height);
}

static void
surface_layout_cb (GdkSurface *surface,
                   int         width,
                   int         height,
                   GtkNative  *native)
{
  gtk_native_layout (native, width, height);

  if (gtk_widget_needs_allocate (GTK_WIDGET (native)))
    gtk_native_queue_relayout (native);
}

static void
scale_changed_cb (GdkSurface *surface,
                  GParamSpec *pspec,
                  GtkNative  *native)
{
  _gtk_widget_scale_changed (GTK_WIDGET (native));
}

static void
verify_priv_unrealized (gpointer user_data)
{
  GtkNativePrivate *priv = user_data;

  g_warn_if_fail (priv->update_handler_id == 0);
  g_warn_if_fail (priv->layout_handler_id == 0);
  g_warn_if_fail (priv->scale_changed_handler_id == 0);

  g_free (priv);
}

/**
 * gtk_native_realize:
 * @self: a `GtkNative`
 *
 * Realizes a `GtkNative`.
 *
 * This should only be used by subclasses.
 */
void
gtk_native_realize (GtkNative *self)
{
  GdkSurface *surface;
  GdkFrameClock *clock;
  GtkNativePrivate *priv;

  g_return_if_fail (g_object_get_qdata (G_OBJECT (self),
                                        quark_gtk_native_private) == NULL);

  surface = gtk_native_get_surface (self);
  clock = gdk_surface_get_frame_clock (surface);
  g_return_if_fail (clock != NULL);

  priv = g_new0 (GtkNativePrivate, 1);
  priv->update_handler_id = g_signal_connect_after (clock, "update",
                                              G_CALLBACK (frame_clock_update_cb),
                                              self);
  priv->layout_handler_id = g_signal_connect (surface, "layout",
                                              G_CALLBACK (surface_layout_cb),
                                              self);

  priv->scale_changed_handler_id = g_signal_connect (surface, "notify::scale-factor",
                                                     G_CALLBACK (scale_changed_cb),
                                                     self);

  g_object_set_qdata_full (G_OBJECT (self),
                           quark_gtk_native_private,
                           priv,
                           verify_priv_unrealized);
}

/**
 * gtk_native_unrealize:
 * @self: a `GtkNative`
 *
 * Unrealizes a `GtkNative`.
 *
 * This should only be used by subclasses.
 */
void
gtk_native_unrealize (GtkNative *self)
{
  GtkNativePrivate *priv;
  GdkSurface *surface;
  GdkFrameClock *clock;

  priv = g_object_get_qdata (G_OBJECT (self), quark_gtk_native_private);
  g_return_if_fail (priv != NULL);

  surface = gtk_native_get_surface (self);
  clock = gdk_surface_get_frame_clock (surface);
  g_return_if_fail (clock != NULL);

  g_clear_signal_handler (&priv->update_handler_id, clock);
  g_clear_signal_handler (&priv->layout_handler_id, surface);
  g_clear_signal_handler (&priv->scale_changed_handler_id, surface);

  g_object_set_qdata (G_OBJECT (self), quark_gtk_native_private, NULL);
}

/**
 * gtk_native_get_surface:
 * @self: a `GtkNative`
 *
 * Returns the surface of this `GtkNative`.
 *
 * Returns: (transfer none) (nullable): the surface of @self
 */
GdkSurface *
gtk_native_get_surface (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_surface (self);
}

/**
 * gtk_native_get_renderer:
 * @self: a `GtkNative`
 *
 * Returns the renderer that is used for this `GtkNative`.
 *
 * Returns: (transfer none) (nullable): the renderer for @self
 */
GskRenderer *
gtk_native_get_renderer (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_renderer (self);
}

/**
 * gtk_native_get_surface_transform:
 * @self: a `GtkNative`
 * @x: (out): return location for the x coordinate
 * @y: (out): return location for the y coordinate
 *
 * Retrieves the surface transform of @self.
 *
 * This is the translation from @self's surface coordinates into
 * @self's widget coordinates.
 */
void
gtk_native_get_surface_transform (GtkNative *self,
                                  double    *x,
                                  double    *y)
{
  g_return_if_fail (GTK_IS_NATIVE (self));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  GTK_NATIVE_GET_IFACE (self)->get_surface_transform (self, x, y);
}

/**
 * gtk_native_get_for_surface:
 * @surface: a `GdkSurface`
 *
 * Finds the `GtkNative` associated with the surface.
 *
 * Returns: (transfer none) (nullable): the `GtkNative` that is associated with @surface
 */
GtkNative *
gtk_native_get_for_surface (GdkSurface *surface)
{
  GtkWidget *widget;

  widget = (GtkWidget *)gdk_surface_get_widget (surface);

  if (widget && GTK_IS_NATIVE (widget))
    return GTK_NATIVE (widget);

  return NULL;
}

void
gtk_native_queue_relayout (GtkNative *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GdkSurface *surface;
  GdkFrameClock *clock;

  surface = gtk_widget_get_surface (widget);
  clock = gtk_widget_get_frame_clock (widget);
  if (clock == NULL)
    return;

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_UPDATE);
  gdk_surface_request_layout (surface);
}

static void
corner_rect (const GtkCssValue     *value,
             cairo_rectangle_int_t *rect)
{
  rect->width = _gtk_css_corner_value_get_x (value, 100);
  rect->height = _gtk_css_corner_value_get_y (value, 100);
}

static void
subtract_decoration_corners_from_region (cairo_region_t        *region,
                                         cairo_rectangle_int_t *extents,
                                         const GtkCssStyle     *style)
{
  cairo_rectangle_int_t rect;

  corner_rect (style->border->border_top_left_radius, &rect);
  rect.x = extents->x;
  rect.y = extents->y;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (style->border->border_top_right_radius, &rect);
  rect.x = extents->x + extents->width - rect.width;
  rect.y = extents->y;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (style->border->border_bottom_left_radius, &rect);
  rect.x = extents->x;
  rect.y = extents->y + extents->height - rect.height;
  cairo_region_subtract_rectangle (region, &rect);

  corner_rect (style->border->border_bottom_right_radius, &rect);
  rect.x = extents->x + extents->width - rect.width;
  rect.y = extents->y + extents->height - rect.height;
  cairo_region_subtract_rectangle (region, &rect);
}

static int
get_translucent_border_edge (const GtkCssValue *color,
                             const GtkCssValue *border_color,
                             const GtkCssValue *border_width)
{
  if (border_color == NULL)
    border_color = color;

  if (!gdk_rgba_is_opaque (gtk_css_color_value_get_rgba (border_color)))
    return round (_gtk_css_number_value_get (border_width, 100));

  return 0;
}

static void
get_translucent_border_width (GtkWidget *widget,
                              GtkBorder *border)
{
  GtkCssNode *css_node = gtk_widget_get_css_node (widget);
  GtkCssStyle *style = gtk_css_node_get_style (css_node);

  border->top = get_translucent_border_edge (style->core->color,
                                             style->border->border_top_color,
                                             style->border->border_top_width);
  border->bottom = get_translucent_border_edge (style->core->color,
                                                style->border->border_bottom_color,
                                                style->border->border_bottom_width);
  border->left = get_translucent_border_edge (style->core->color,
                                              style->border->border_left_color,
                                              style->border->border_left_width);
  border->right = get_translucent_border_edge (style->core->color,
                                               style->border->border_right_color,
                                               style->border->border_right_width);
}

static gboolean
get_opaque_rect (GtkWidget             *widget,
                 const GtkCssStyle     *style,
                 cairo_rectangle_int_t *rect)
{
  gboolean is_opaque = gdk_rgba_is_opaque (gtk_css_color_value_get_rgba (style->background->background_color));

  if (is_opaque && gtk_widget_get_opacity (widget) < 1.0)
    is_opaque = FALSE;

  if (is_opaque)
    {
      const graphene_rect_t *border_rect;
      GtkCssBoxes css_boxes;
      GtkBorder border;

      gtk_css_boxes_init (&css_boxes, widget);
      border_rect = gtk_css_boxes_get_border_rect (&css_boxes);
      get_translucent_border_width (widget, &border);

      rect->x = border_rect->origin.x + border.left;
      rect->y = border_rect->origin.y + border.top;
      rect->width = border_rect->size.width - border.left - border.right;
      rect->height = border_rect->size.height - border.top - border.bottom;
    }

  return is_opaque;
}

static void
get_shadow_width (GtkWidget *widget,
                  GtkBorder *shadow_width,
                  int        resize_handle_size)
{
  GtkCssNode *css_node = gtk_widget_get_css_node (widget);
  const GtkCssStyle *style = gtk_css_node_get_style (css_node);

  gtk_css_shadow_value_get_extents (style->background->box_shadow, shadow_width);

  shadow_width->left = MAX (shadow_width->left, resize_handle_size);
  shadow_width->top = MAX (shadow_width->top, resize_handle_size);
  shadow_width->bottom = MAX (shadow_width->bottom, resize_handle_size);
  shadow_width->right = MAX (shadow_width->right, resize_handle_size);
}

void
gtk_native_update_opaque_region (GtkNative  *native,
                                 GtkWidget  *contents,
                                 gboolean    subtract_decoration_corners,
                                 gboolean    subtract_shadow,
                                 int         resize_handle_size)
{
  cairo_rectangle_int_t rect;
  cairo_region_t *opaque_region = NULL;
  const GtkCssStyle *style;
  GtkCssNode *css_node;
  GdkSurface *surface;
  GtkBorder shadow;

  g_return_if_fail (GTK_IS_NATIVE (native));
  g_return_if_fail (!contents || GTK_IS_WIDGET (contents));

  if (contents == NULL)
    contents = GTK_WIDGET (native);

  if (!_gtk_widget_get_realized (GTK_WIDGET (native)) ||
      !_gtk_widget_get_realized (contents))
    return;

  css_node = gtk_widget_get_css_node (contents);

  if (subtract_shadow)
    get_shadow_width (contents, &shadow, resize_handle_size);
  else
    shadow = (GtkBorder) {0, 0, 0, 0};

  surface = gtk_native_get_surface (native);
  style = gtk_css_node_get_style (css_node);

  if (get_opaque_rect (contents, style, &rect))
    {
      double native_x, native_y;

      gtk_native_get_surface_transform (native, &native_x, &native_y);
      rect.x += native_x;
      rect.y += native_y;

      if (contents != GTK_WIDGET (native))
        {
          graphene_point_t p;

          if (!gtk_widget_compute_point (contents, GTK_WIDGET (native),
                                         &GRAPHENE_POINT_INIT (0, 0), &p))
            graphene_point_init (&p, 0, 0);
          rect.x += p.x;
          rect.y += p.y;
        }

      opaque_region = cairo_region_create_rectangle (&rect);

      if (subtract_decoration_corners)
        subtract_decoration_corners_from_region (opaque_region, &rect, style);
    }

  gdk_surface_set_opaque_region (surface, opaque_region);

  cairo_region_destroy (opaque_region);
}
