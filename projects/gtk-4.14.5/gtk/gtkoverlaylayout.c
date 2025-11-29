/* gtkoverlaylayout.c: Overlay layout manager
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright 2019 Red Hat, Inc.
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

#include "gtkoverlaylayout.h"

#include "gtklayoutchild.h"
#include "gtkoverlay.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <graphene-gobject.h>


/**
 * GtkOverlayLayout:
 *
 * `GtkOverlayLayout` is the layout manager used by [class@Gtk.Overlay].
 *
 * It places widgets as overlays on top of the main child.
 *
 * This is not a reusable layout manager, since it expects its widget
 * to be a `GtkOverlay`. It is only listed here so that its layout
 * properties get documented.
 */

/**
 * GtkOverlayLayoutChild:
 *
 * `GtkLayoutChild` subclass for children in a `GtkOverlayLayout`.
 */

struct _GtkOverlayLayout
{
  GtkLayoutManager parent_instance;
};

struct _GtkOverlayLayoutChild
{
  GtkLayoutChild parent_instance;

  guint measure : 1;
  guint clip_overlay : 1;
};

enum
{
  PROP_MEASURE = 1,
  PROP_CLIP_OVERLAY,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkOverlayLayoutChild, gtk_overlay_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_overlay_layout_child_set_property (GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkOverlayLayoutChild *self = GTK_OVERLAY_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_MEASURE:
      gtk_overlay_layout_child_set_measure (self, g_value_get_boolean (value));
      break;

    case PROP_CLIP_OVERLAY:
      gtk_overlay_layout_child_set_clip_overlay (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_overlay_layout_child_get_property (GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkOverlayLayoutChild *self = GTK_OVERLAY_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_MEASURE:
      g_value_set_boolean (value, self->measure);
      break;

    case PROP_CLIP_OVERLAY:
      g_value_set_boolean (value, self->clip_overlay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_overlay_layout_child_class_init (GtkOverlayLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_overlay_layout_child_set_property;
  gobject_class->get_property = gtk_overlay_layout_child_get_property;

  /**
   * GtkOverlayLayoutChild:measure: (attributes org.gtk.Property.get=gtk_overlay_layout_child_get_measure org.gtk.Property.set=gtk_overlay_layout_child_set_measure)
   *
   * Whether the child size should contribute to the `GtkOverlayLayout`'s
   * measurement.
   */
  child_props[PROP_MEASURE] =
    g_param_spec_boolean ("measure", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkOverlayLayoutChild:clip-overlay: (attributes org.gtk.Property.get=gtk_overlay_layout_child_get_clip_overlay org.gtk.Property.set=gtk_overlay_layout_child_set_clip_overlay)
   *
   * Whether the child should be clipped to fit the parent's size.
   */
  child_props[PROP_CLIP_OVERLAY] =
    g_param_spec_boolean ("clip-overlay", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_overlay_layout_child_init (GtkOverlayLayoutChild *self)
{
}

/**
 * gtk_overlay_layout_child_set_measure: (attributes org.gtk.Method.set_property=measure)
 * @child: a `GtkOverlayLayoutChild`
 * @measure: whether to measure this child
 *
 * Sets whether to measure this child.
 */
void
gtk_overlay_layout_child_set_measure (GtkOverlayLayoutChild *child,
                                      gboolean               measure)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child));

  if (child->measure == measure)
    return;

  child->measure = measure;

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_MEASURE]);
}

/**
 * gtk_overlay_layout_child_get_measure: (attributes org.gtk.Method.get_property=measure)
 * @child: a `GtkOverlayLayoutChild`
 *
 * Retrieves whether the child is measured.
 *
 * Returns: whether the child is measured
 */
gboolean
gtk_overlay_layout_child_get_measure (GtkOverlayLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child), FALSE);

  return child->measure;
}

/**
 * gtk_overlay_layout_child_set_clip_overlay: (attributes org.gtk.Method.set_property=clip-overlay)
 * @child: a `GtkOverlayLayoutChild`
 * @clip_overlay: whether to clip this child
 *
 * Sets whether to clip this child.
 */
void
gtk_overlay_layout_child_set_clip_overlay (GtkOverlayLayoutChild *child,
                                           gboolean               clip_overlay)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child));

  if (child->clip_overlay == clip_overlay)
    return;

  child->clip_overlay = clip_overlay;

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CLIP_OVERLAY]);
}

/**
 * gtk_overlay_layout_child_get_clip_overlay: (attributes org.gtk.Method.get_property=clip-overlay)
 * @child: a `GtkOverlayLayoutChild`
 *
 * Retrieves whether the child is clipped.
 *
 * Returns: whether the child is clipped
 */
gboolean
gtk_overlay_layout_child_get_clip_overlay (GtkOverlayLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child), FALSE);

  return child->clip_overlay;
}

G_DEFINE_TYPE (GtkOverlayLayout, gtk_overlay_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_overlay_layout_measure (GtkLayoutManager *layout_manager,
                            GtkWidget        *widget,
                            GtkOrientation    orientation,
                            int               for_size,
                            int              *minimum,
                            int              *natural,
                            int              *minimum_baseline,
                            int              *natural_baseline)
{
  GtkOverlayLayoutChild *child_info;
  GtkWidget *child;
  int min, nat;
  GtkWidget *main_widget;

  main_widget = gtk_overlay_get_child (GTK_OVERLAY (widget));

  min = 0;
  nat = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      child_info = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

      if (child == main_widget || child_info->measure)
        {
          int child_min, child_nat, child_min_baseline, child_nat_baseline;

          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &child_min, &child_nat,
                              &child_min_baseline, &child_nat_baseline);

          min = MAX (min, child_min);
          nat = MAX (nat, child_nat);
        }
    }

  if (minimum != NULL)
    *minimum = min;
  if (natural != NULL)
    *natural = nat;
}

static void
gtk_overlay_compute_child_allocation (GtkOverlay            *overlay,
                                      GtkWidget             *widget,
                                      GtkOverlayLayoutChild *child,
                                      GtkAllocation         *widget_allocation)
{
  GtkAllocation allocation;
  gboolean result;

  g_signal_emit_by_name (overlay, "get-child-position",
                         widget, &allocation, &result);

  widget_allocation->x = allocation.x;
  widget_allocation->y = allocation.y;
  widget_allocation->width = allocation.width;
  widget_allocation->height = allocation.height;
}

static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    case GTK_ALIGN_FILL:
    case GTK_ALIGN_CENTER:
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_BASELINE_CENTER:
    default:
      return align;
    }
}

static void
gtk_overlay_child_update_style_classes (GtkOverlay *overlay,
                                        GtkWidget *child,
                                        GtkAllocation *child_allocation)
{
  GtkWidget *widget = GTK_WIDGET (overlay);
  int width, height;
  GtkAlign valign, halign;
  gboolean is_left, is_right, is_top, is_bottom;
  gboolean has_left, has_right, has_top, has_bottom;

  has_left = gtk_widget_has_css_class (child, "left");
  has_right = gtk_widget_has_css_class (child, "right");
  has_top = gtk_widget_has_css_class (child, "top");
  has_bottom = gtk_widget_has_css_class (child, "bottom");

  is_left = is_right = is_top = is_bottom = FALSE;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  halign = effective_align (gtk_widget_get_halign (child),
                            gtk_widget_get_direction (child));

  if (halign == GTK_ALIGN_START)
    is_left = (child_allocation->x == 0);
  else if (halign == GTK_ALIGN_END)
    is_right = (child_allocation->x + child_allocation->width == width);

  valign = gtk_widget_get_valign (child);

  if (valign == GTK_ALIGN_START)
    is_top = (child_allocation->y == 0);
  else if (valign == GTK_ALIGN_END)
    is_bottom = (child_allocation->y + child_allocation->height == height);

  if (has_left && !is_left)
    gtk_widget_remove_css_class (child, "left");
  else if (!has_left && is_left)
    gtk_widget_add_css_class (child, "left");

  if (has_right && !is_right)
    gtk_widget_remove_css_class (child, "right");
  else if (!has_right && is_right)
    gtk_widget_add_css_class (child, "right");

  if (has_top && !is_top)
    gtk_widget_remove_css_class (child, "top");
  else if (!has_top && is_top)
    gtk_widget_add_css_class (child, "top");

  if (has_bottom && !is_bottom)
    gtk_widget_remove_css_class (child, "bottom");
  else if (!has_bottom && is_bottom)
    gtk_widget_add_css_class (child, "bottom");
}

static void
gtk_overlay_child_allocate (GtkOverlay            *overlay,
                            GtkWidget             *widget,
                            GtkOverlayLayoutChild *child)
{
  GtkAllocation child_allocation;

  if (!gtk_widget_should_layout (widget))
    return;

  gtk_overlay_compute_child_allocation (overlay, widget, child, &child_allocation);

  gtk_overlay_child_update_style_classes (overlay, widget, &child_allocation);
  gtk_widget_size_allocate (widget, &child_allocation, -1);
}

static void
gtk_overlay_layout_allocate (GtkLayoutManager *layout_manager,
                             GtkWidget        *widget,
                             int               width,
                             int               height,
                             int               baseline)
{
  GtkWidget *child;
  GtkWidget *main_widget;

  main_widget = gtk_overlay_get_child (GTK_OVERLAY (widget));
  if (main_widget && gtk_widget_get_visible (main_widget))
    gtk_widget_size_allocate (main_widget,
                              &(GtkAllocation) { 0, 0, width, height },
                              -1);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        {
          GtkOverlayLayoutChild *child_data;
          child_data = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

          gtk_overlay_child_allocate (GTK_OVERLAY (widget), child, child_data);
        }
    }
}

static void
gtk_overlay_layout_class_init (GtkOverlayLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->layout_child_type = GTK_TYPE_OVERLAY_LAYOUT_CHILD;
  layout_class->measure = gtk_overlay_layout_measure;
  layout_class->allocate = gtk_overlay_layout_allocate;
}

static void
gtk_overlay_layout_init (GtkOverlayLayout *self)
{
}

/**
 * gtk_overlay_layout_new:
 *
 * Creates a new `GtkOverlayLayout` instance.
 *
 * Returns: the newly created instance
 */
GtkLayoutManager *
gtk_overlay_layout_new (void)
{
  return g_object_new (GTK_TYPE_OVERLAY_LAYOUT, NULL);
}
