/* GTK - The GIMP Toolkit
 * Copyright 2015  Emmanuele Bassi 
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

#include "gtktooltipwindowprivate.h"

#include "gtkbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknativeprivate.h"
#include "gtkcssboxesimplprivate.h"

#include "gdk/gdksurfaceprivate.h"

struct _GtkTooltipWindow
{
  GtkWidget parent_instance;

  GdkSurface *surface;
  GskRenderer *renderer;

  GtkWidget *relative_to;
  GdkRectangle rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  int dx;
  int dy;
  guint surface_transform_changed_cb;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *custom_widget;
};

struct _GtkTooltipWindowClass
{
  GtkWidgetClass parent_class;
};

static void gtk_tooltip_window_native_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTooltipWindow, gtk_tooltip_window, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_tooltip_window_native_init))


static GdkSurface *
gtk_tooltip_window_native_get_surface (GtkNative *native)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (native);

  return window->surface;
}

static GskRenderer *
gtk_tooltip_window_native_get_renderer (GtkNative *native)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (native);

  return window->renderer;
}

static void
gtk_tooltip_window_native_get_surface_transform (GtkNative *native,
                                                 double    *x,
                                                 double    *y)
{
  GtkCssBoxes css_boxes;
  const graphene_rect_t *margin_rect;

  gtk_css_boxes_init (&css_boxes, GTK_WIDGET (native));
  margin_rect = gtk_css_boxes_get_margin_rect (&css_boxes);

  *x = - margin_rect->origin.x;
  *y = - margin_rect->origin.y;
}

static GdkPopupLayout *
create_popup_layout (GtkTooltipWindow *window)
{
  GdkPopupLayout *layout;

  layout = gdk_popup_layout_new (&window->rect,
                                 window->rect_anchor,
                                 window->surface_anchor);
  gdk_popup_layout_set_anchor_hints (layout, window->anchor_hints);
  gdk_popup_layout_set_offset (layout, window->dx, window->dy);

  return layout;
}

static void
gtk_tooltip_window_relayout (GtkTooltipWindow *window)
{
  GtkRequisition req;
  GdkPopupLayout *layout;

  if (!gtk_widget_get_visible (GTK_WIDGET (window)) ||
      window->surface == NULL)
    return;

  gtk_widget_get_preferred_size (GTK_WIDGET (window), NULL, &req);
  layout = create_popup_layout (window);
  gdk_popup_present (GDK_POPUP (window->surface),
                     MAX (req.width, 1),
                     MAX (req.height, 1),
                     layout);
  gdk_popup_layout_unref (layout);
}

void
gtk_tooltip_window_present (GtkTooltipWindow *window)
{
  GtkWidget *widget = GTK_WIDGET (window);

  if (!_gtk_widget_get_alloc_needed (widget))
    {
      gtk_widget_ensure_allocate (widget);
    }
  else if (gtk_widget_get_visible (widget))
    {
      gtk_tooltip_window_relayout (window);
    }
}

static void
gtk_tooltip_window_native_layout (GtkNative *native,
                                  int        width,
                                  int        height)
{
  GtkWidget *widget = GTK_WIDGET (native);

  if (gtk_widget_needs_allocate (widget))
    gtk_widget_allocate (widget, width, height, -1, NULL);
  else
    gtk_widget_ensure_allocate (widget);

}

static void
gtk_tooltip_window_native_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_tooltip_window_native_get_surface;
  iface->get_renderer = gtk_tooltip_window_native_get_renderer;
  iface->get_surface_transform = gtk_tooltip_window_native_get_surface_transform;
  iface->layout = gtk_tooltip_window_native_layout;
}

static void
mapped_changed (GdkSurface *surface,
                GParamSpec *pspec,
                GtkWidget  *widget)
{
  if (!gdk_surface_get_mapped (surface))
    gtk_widget_set_visible (widget, FALSE);
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkWidget      *widget)
{
  gtk_widget_render (widget, surface, region);
  return TRUE;
}

static gboolean
surface_event (GdkSurface *surface,
               GdkEvent   *event,
               GtkWidget  *widget)
{
  return gtk_main_do_event (event);
}

static void
gtk_tooltip_window_realize (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GdkSurface *parent;

  parent = gtk_native_get_surface (gtk_widget_get_native (window->relative_to));
  window->surface = gdk_surface_new_popup (parent, FALSE);

  gdk_surface_set_widget (window->surface, widget);

  g_signal_connect (window->surface, "notify::mapped", G_CALLBACK (mapped_changed), widget);
  g_signal_connect (window->surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (window->surface, "event", G_CALLBACK (surface_event), widget);

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->realize (widget);

  window->renderer = gsk_renderer_new_for_surface (window->surface);

  gtk_native_realize (GTK_NATIVE (window));
}

static void
gtk_tooltip_window_unrealize (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  gtk_native_unrealize (GTK_NATIVE (window));

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (window->renderer);
  g_clear_object (&window->renderer);

  g_signal_handlers_disconnect_by_func (window->surface, mapped_changed, widget);
  g_signal_handlers_disconnect_by_func (window->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (window->surface, surface_event, widget);
  gdk_surface_set_widget (window->surface, NULL);
  g_clear_pointer (&window->surface, gdk_surface_destroy);
}


static void
unset_surface_transform_changed_cb (gpointer data)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (data);

  window->surface_transform_changed_cb = 0;
}

static gboolean
surface_transform_changed_cb (GtkWidget               *widget,
                              const graphene_matrix_t *transform,
                              gpointer                 user_data)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (user_data);

  gtk_tooltip_window_relayout (window);

  return G_SOURCE_CONTINUE;
}


static void
gtk_tooltip_window_map (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GdkPopupLayout *layout;

  layout = create_popup_layout (window);
  gdk_popup_present (GDK_POPUP (window->surface),
                     gdk_surface_get_width (window->surface),
                     gdk_surface_get_height (window->surface),
                     layout);
  gdk_popup_layout_unref (layout);

  window->surface_transform_changed_cb =
    gtk_widget_add_surface_transform_changed_callback (window->relative_to,
                                                       surface_transform_changed_cb,
                                                       window,
                                                       unset_surface_transform_changed_cb);

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->map (widget);

  if (gtk_widget_get_visible (window->box))
    gtk_widget_map (window->box);
}

static void
gtk_tooltip_window_unmap (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  gtk_widget_remove_surface_transform_changed_callback (window->relative_to,
                                                        window->surface_transform_changed_cb);
  window->surface_transform_changed_cb = 0;

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->unmap (widget);
  gdk_surface_hide (window->surface);

  gtk_widget_unmap (window->box);
}

static void
gtk_tooltip_window_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  if (window->box)
    gtk_widget_measure (window->box,
                        orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_tooltip_window_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  if (window->box)
    gtk_widget_allocate (window->box, width, height, baseline, NULL);
}

static void
gtk_tooltip_window_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_widget_realize (widget);
  gtk_tooltip_window_present (GTK_TOOLTIP_WINDOW (widget));
  gtk_widget_map (widget);
}

static void
gtk_tooltip_window_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void
gtk_tooltip_window_dispose (GObject *object)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (object);

  if (window->relative_to)
    gtk_widget_unparent (GTK_WIDGET (window));

  g_clear_pointer (&window->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_tooltip_window_parent_class)->dispose (object);
}

static void
gtk_tooltip_window_class_init (GtkTooltipWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_tooltip_window_dispose;
  widget_class->realize = gtk_tooltip_window_realize;
  widget_class->unrealize = gtk_tooltip_window_unrealize;
  widget_class->map = gtk_tooltip_window_map;
  widget_class->unmap = gtk_tooltip_window_unmap;
  widget_class->measure = gtk_tooltip_window_measure;
  widget_class->size_allocate = gtk_tooltip_window_size_allocate;
  widget_class->show = gtk_tooltip_window_show;
  widget_class->hide = gtk_tooltip_window_hide;

  gtk_widget_class_set_css_name (widget_class, I_("tooltip"));
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtktooltipwindow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, box);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, image);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, label);
}

static void
gtk_tooltip_window_init (GtkTooltipWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gtk_tooltip_window_new (void)
{
  return g_object_new (GTK_TYPE_TOOLTIP_WINDOW, NULL);
}

void
gtk_tooltip_window_set_label_markup (GtkTooltipWindow *window,
                                     const char       *markup)
{
  if (markup != NULL)
    gtk_label_set_markup (GTK_LABEL (window->label), markup);

  gtk_widget_set_visible (window->label, markup != NULL);
}

void
gtk_tooltip_window_set_label_text (GtkTooltipWindow *window,
                                   const char       *text)
{
  if (text != NULL)
    gtk_label_set_text (GTK_LABEL (window->label), text);

  gtk_widget_set_visible (window->label, text != NULL);
}

void
gtk_tooltip_window_set_image_icon (GtkTooltipWindow *window,
                                   GdkPaintable     *paintable)
{
  if (paintable != NULL)
    gtk_image_set_from_paintable (GTK_IMAGE (window->image), paintable);

  gtk_widget_set_visible (window->image, paintable != NULL);
}

void
gtk_tooltip_window_set_image_icon_from_name (GtkTooltipWindow *window,
                                             const char       *icon_name)
{
  gtk_widget_set_visible (window->image, icon_name != NULL);
  if (icon_name)
    gtk_image_set_from_icon_name (GTK_IMAGE (window->image), icon_name);
}

void
gtk_tooltip_window_set_image_icon_from_gicon (GtkTooltipWindow *window,
                                              GIcon            *gicon)
{
  gtk_widget_set_visible (window->image, gicon != NULL);
  if (gicon != NULL)
    gtk_image_set_from_gicon (GTK_IMAGE (window->image), gicon);
}

void
gtk_tooltip_window_set_custom_widget (GtkTooltipWindow *window,
                                      GtkWidget        *custom_widget)
{
  /* No need to do anything if the custom widget stays the same */
  if (window->custom_widget == custom_widget)
    return;

  if (window->custom_widget != NULL)
    {
      GtkWidget *custom = window->custom_widget;

      /* Note: We must reset window->custom_widget first,
       * since gtk_container_remove() will recurse into
       * gtk_tooltip_set_custom()
       */
      window->custom_widget = NULL;
      gtk_box_remove (GTK_BOX (window->box), custom);
      g_object_unref (custom);
    }

  if (custom_widget != NULL)
    {
      window->custom_widget = g_object_ref (custom_widget);

      gtk_box_append (GTK_BOX (window->box), custom_widget);
      gtk_widget_set_visible (custom_widget, TRUE);
      gtk_widget_set_visible (window->image, FALSE);
      gtk_widget_set_visible (window->label, FALSE);
    }
}

void
gtk_tooltip_window_set_relative_to (GtkTooltipWindow *window,
                                    GtkWidget        *relative_to)
{
  g_return_if_fail (GTK_WIDGET (window) != relative_to);

  if (window->relative_to == relative_to)
    return;

  g_object_ref (window);

  if (window->relative_to)
    gtk_widget_unparent (GTK_WIDGET (window));

  window->relative_to = relative_to;

  if (window->relative_to)
    gtk_widget_set_parent (GTK_WIDGET (window), relative_to);

  g_object_unref (window);
}

void
gtk_tooltip_window_position (GtkTooltipWindow *window,
                             GdkRectangle     *rect,
                             GdkGravity        rect_anchor,
                             GdkGravity        surface_anchor,
                             GdkAnchorHints    anchor_hints,
                             int               dx,
                             int               dy)
{
  window->rect = *rect;
  window->rect_anchor = rect_anchor;
  window->surface_anchor = surface_anchor;
  window->anchor_hints = anchor_hints;
  window->dx = dx;
  window->dy = dy;

  gtk_tooltip_window_relayout (window);
}
