/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "gtkcenterlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtklayoutchild.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"

/**
 * GtkCenterLayout:
 *
 * `GtkCenterLayout` is a layout manager that manages up to three children.
 *
 * The start widget is allocated at the start of the layout (left in
 * left-to-right locales and right in right-to-left ones), and the end
 * widget at the end.
 *
 * The center widget is centered regarding the full width of the layout's.
 */
struct _GtkCenterLayout
{
  GtkLayoutManager parent_instance;

  GtkBaselinePosition baseline_pos;
  GtkOrientation orientation;
  gboolean shrink_center_last;

  union {
    struct {
      GtkWidget *start_widget;
      GtkWidget *center_widget;
      GtkWidget *end_widget;
    };
    GtkWidget *children[3];
  };
};

enum {
  PROP_0,
  PROP_SHRINK_CENTER_LAST,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkCenterLayout, gtk_center_layout, GTK_TYPE_LAYOUT_MANAGER)

static int
get_spacing (GtkCenterLayout *self,
             GtkCssNode      *node)
{
  GtkCssStyle *style = gtk_css_node_get_style (node);
  GtkCssValue *border_spacing;
  int css_spacing;

  border_spacing = style->size->border_spacing;
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing;
}

static GtkSizeRequestMode
gtk_center_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                    GtkWidget        *widget)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  int count[3] = { 0, 0, 0 };

  if (self->start_widget)
    count[gtk_widget_get_request_mode (self->start_widget)]++;

  if (self->center_widget)
    count[gtk_widget_get_request_mode (self->center_widget)]++;

  if (self->end_widget)
    count[gtk_widget_get_request_mode (self->end_widget)]++;

  if (!count[GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH] &&
      !count[GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT])
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return count[GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT] > count[GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH]
           ? GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT
           : GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_center_layout_distribute (GtkCenterLayout  *self,
                              int               for_size,
                              int               size,
                              int               spacing,
                              GtkRequestedSize *sizes)
{
  int center_size = 0;
  int start_size = 0;
  int end_size = 0;
  gboolean center_expand = FALSE;
  gboolean start_expand = FALSE;
  gboolean end_expand = FALSE;
  int avail;
  int i;
  int needed_spacing = 0;

  /* Usable space is really less... */
  for (i = 0; i < 3; i++)
    {
      if (self->children[i])
        needed_spacing += spacing;
    }
  needed_spacing -= spacing;

  sizes[0].minimum_size = sizes[0].natural_size = 0;
  sizes[1].minimum_size = sizes[1].natural_size = 0;
  sizes[2].minimum_size = sizes[2].natural_size = 0;

  for (i = 0; i < 3; i ++)
    {
      if (self->children[i])
        gtk_widget_measure (self->children[i], self->orientation, for_size,
                            &sizes[i].minimum_size, &sizes[i].natural_size,
                            NULL, NULL);
    }

  if (self->center_widget)
    {
      int natural_size;

      avail = size - needed_spacing - (sizes[0].minimum_size + sizes[2].minimum_size);

      if (self->shrink_center_last)
        natural_size = sizes[1].natural_size;
      else
        natural_size = CLAMP (size - needed_spacing - (sizes[0].natural_size + sizes[2].natural_size), sizes[1].minimum_size, sizes[1].natural_size);

      center_size = CLAMP (avail, sizes[1].minimum_size, natural_size);
      center_expand = gtk_widget_compute_expand (self->center_widget, self->orientation);
    }

  if (self->start_widget)
    {
      avail = size - needed_spacing - (center_size + sizes[2].minimum_size);
      start_size = CLAMP (avail, sizes[0].minimum_size, sizes[0].natural_size);
      start_expand = gtk_widget_compute_expand (self->start_widget, self->orientation);
    }

   if (self->end_widget)
    {
      avail = size - needed_spacing - (center_size + sizes[0].minimum_size);
      end_size = CLAMP (avail, sizes[2].minimum_size, sizes[2].natural_size);
      end_expand = gtk_widget_compute_expand (self->end_widget, self->orientation);
    }

  if (self->center_widget)
    {
      int center_pos;

      center_pos = (size / 2) - (center_size / 2);

      /* Push in from start/end */
      if (start_size > 0 && start_size + spacing > center_pos)
        center_pos = start_size + spacing;
      else if (end_size > 0 && size - end_size - spacing < center_pos + center_size)
        center_pos = size - center_size - end_size - spacing;
      else if (center_expand)
        {
          center_size = size - 2 * (MAX (start_size, end_size) + spacing);
          center_pos = (size / 2) - (center_size / 2) + spacing;
        }

      if (start_expand)
        start_size = center_pos - spacing;

      if (end_expand)
        end_size = size - (center_pos + center_size) - spacing;
    }
  else
    {
      avail = size - needed_spacing - (start_size + end_size);
      if (start_expand && end_expand)
        {
          start_size += avail / 2;
          end_size += avail / 2;
        }
      else if (start_expand)
        {
          start_size += avail;
        }
      else if (end_expand)
        {
          end_size += avail;
        }
    }

  sizes[0].minimum_size = start_size;
  sizes[1].minimum_size = center_size;
  sizes[2].minimum_size = end_size;
}

static void
gtk_center_layout_measure_orientation (GtkCenterLayout *self,
                                       GtkWidget       *widget,
                                       GtkOrientation   orientation,
                                       int              for_size,
                                       int             *minimum,
                                       int             *natural,
                                       int             *minimum_baseline,
                                       int             *natural_baseline)
{
  int min[3];
  int nat[3];
  int n_visible_children = 0;
  int spacing;
  int i;

  spacing = get_spacing (self, gtk_widget_get_css_node (widget));

  for (i = 0; i < 3; i ++)
    {
      GtkWidget *child = self->children[i];

      if (child)
        {
          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &min[i], &nat[i], NULL, NULL);

          if (_gtk_widget_get_visible (child))
            n_visible_children ++;
        }
      else
        {
          min[i] = 0;
          nat[i] = 0;
        }
    }

  *minimum = min[0] + min[1] + min[2];
  *natural = nat[1] + 2 * MAX (nat[0], nat[2]);

  if (n_visible_children > 0)
    {
      *minimum += (n_visible_children - 1) * spacing;
      *natural += (n_visible_children - 1) * spacing;
    }
}

static void
gtk_center_layout_measure_opposite (GtkCenterLayout *self,
                                    GtkOrientation   orientation,
                                    int              for_size,
                                    int             *minimum,
                                    int             *natural,
                                    int             *minimum_baseline,
                                    int             *natural_baseline)
{
  int child_min, child_nat;
  int child_min_baseline, child_nat_baseline;
  int total_min, above_min, below_min;
  int total_nat, above_nat, below_nat;
  GtkWidget *child[3];
  GtkRequestedSize sizes[3];
  gboolean have_baseline = FALSE;
  gboolean align_baseline = FALSE;
  int i;

  child[0] = self->start_widget;
  child[1] = self->center_widget;
  child[2] = self->end_widget;

  if (for_size >= 0)
    gtk_center_layout_distribute (self, -1, for_size, 0, sizes);

  above_min = below_min = above_nat = below_nat = -1;
  total_min = total_nat = 0;

  for (i = 0; i < 3; i++)
    {
      if (child[i] == NULL)
        continue;

      gtk_widget_measure (child[i],
                          orientation,
                          for_size >= 0 ? sizes[i].minimum_size : -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      total_min = MAX (total_min, child_min);
      total_nat = MAX (total_nat, child_nat);

      if (orientation == GTK_ORIENTATION_VERTICAL && child_min_baseline >= 0)
        {
          have_baseline = TRUE;
          if (gtk_widget_get_valign (child[i]) == GTK_ALIGN_BASELINE_FILL ||
              gtk_widget_get_valign (child[i]) == GTK_ALIGN_BASELINE_CENTER)
            align_baseline = TRUE;

          below_min = MAX (below_min, child_min - child_min_baseline);
          above_min = MAX (above_min, child_min_baseline);
          below_nat = MAX (below_nat, child_nat - child_nat_baseline);
          above_nat = MAX (above_nat, child_nat_baseline);
        }
   }

  if (have_baseline)
    {
      int min_baseline = -1;
      int nat_baseline = -1;

      if (align_baseline)
        {
          total_min = MAX (total_min, above_min + below_min);
          total_nat = MAX (total_nat, above_nat + below_nat);
        }

      switch (self->baseline_pos)
        {
        case GTK_BASELINE_POSITION_TOP:
          min_baseline = above_min;
          nat_baseline = above_nat;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          min_baseline = above_min + (total_min - (above_min + below_min)) / 2;
          nat_baseline = above_nat + (total_nat - (above_nat + below_nat)) / 2;
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          min_baseline = total_min - below_min;
          nat_baseline = total_nat - below_nat;
          break;
        default:
          break;
        }

      if (minimum_baseline)
        *minimum_baseline = min_baseline;
      if (natural_baseline)
        *natural_baseline = nat_baseline;
    }

  *minimum = total_min;
  *natural = total_nat;
}



static void
gtk_center_layout_measure (GtkLayoutManager *layout_manager,
                           GtkWidget        *widget,
                           GtkOrientation    orientation,
                           int               for_size,
                           int              *minimum,
                           int              *natural,
                           int              *minimum_baseline,
                           int              *natural_baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);

  if (self->orientation == orientation)
    gtk_center_layout_measure_orientation (self, widget, orientation, for_size,
                                           minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_center_layout_measure_opposite (self, orientation, for_size,
                                        minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_center_layout_allocate (GtkLayoutManager *layout_manager,
                            GtkWidget        *widget,
                            int               width,
                            int               height,
                            int               baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  GtkWidget *child[3];
  int child_size[3];
  int child_pos[3];
  GtkRequestedSize sizes[3];
  int size;
  int for_size;
  int i;
  int spacing;

  spacing = get_spacing (self, gtk_widget_get_css_node (widget));

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      size = width;
      for_size = height;
    }
  else
    {
      size = height;
      for_size = width;
      baseline = -1;
    }

  /* Allocate child sizes */

  gtk_center_layout_distribute (self, for_size, size, spacing, sizes);

  child[1] = self->center_widget;
  child_size[1] = sizes[1].minimum_size;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      child[0] = self->end_widget;
      child[2] = self->start_widget;
      child_size[0] = sizes[2].minimum_size;
      child_size[2] = sizes[0].minimum_size;
    }
  else
    {
      child[0] = self->start_widget;
      child[2] = self->end_widget;
      child_size[0] = sizes[0].minimum_size;
      child_size[2] = sizes[2].minimum_size;
    }

  /* Determine baseline */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      baseline == -1)
    {
      int min_above, nat_above;
      int min_below, nat_below;
      gboolean have_baseline;

      have_baseline = FALSE;
      min_above = nat_above = 0;
      min_below = nat_below = 0;

      for (i = 0; i < 3; i++)
        {
          if (child[i] &&
              (gtk_widget_get_valign (child[i]) == GTK_ALIGN_BASELINE_FILL ||
               gtk_widget_get_valign (child[i]) == GTK_ALIGN_BASELINE_CENTER))
            {
              int child_min_height, child_nat_height;
              int child_min_baseline, child_nat_baseline;

              child_min_baseline = child_nat_baseline = -1;

              gtk_widget_measure (child[i], GTK_ORIENTATION_VERTICAL,
                                  child_size[i],
                                  &child_min_height, &child_nat_height,
                                  &child_min_baseline, &child_nat_baseline);

              if (child_min_baseline >= 0)
                {
                  have_baseline = TRUE;
                  min_below = MAX (min_below, child_min_height - child_min_baseline);
                  nat_below = MAX (nat_below, child_nat_height - child_nat_baseline);
                  min_above = MAX (min_above, child_min_baseline);
                  nat_above = MAX (nat_above, child_nat_baseline);
                }
            }
        }

      if (have_baseline)
        {
          /* TODO: This is purely based on the minimum baseline.
           * When things fit we should use the natural one
           */
          switch (self->baseline_pos)
            {
            default:
            case GTK_BASELINE_POSITION_TOP:
              baseline = min_above;
              break;
            case GTK_BASELINE_POSITION_CENTER:
              baseline = min_above + (height - (min_above + min_below)) / 2;
              break;
            case GTK_BASELINE_POSITION_BOTTOM:
              baseline = height - min_below;
              break;
            }
        }
    }

  /* Allocate child positions */

  child_pos[0] = 0;
  child_pos[1] = (size / 2) - (child_size[1] / 2);
  child_pos[2] = size - child_size[2];

  if (child[1])
    {
      /* Push in from start/end */
      if (child_size[0] > 0 && child_size[0] + spacing > child_pos[1])
        child_pos[1] = child_size[0] + spacing;
      else if (child_size[2] > 0 && size - child_size[2] - spacing < child_pos[1] + child_size[1])
        child_pos[1] = size - child_size[1] - child_size[2] - spacing;
    }

  for (i = 0; i < 3; i++)
    {
      GtkAllocation child_allocation;

      if (child[i] == NULL)
        continue;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.x = child_pos[i];
          child_allocation.y = 0;
          child_allocation.width = child_size[i];
          child_allocation.height = height;
        }
      else
        {
          child_allocation.x = 0;
          child_allocation.y = child_pos[i];
          child_allocation.width = width;
          child_allocation.height = child_size[i];
        }

      gtk_widget_size_allocate (child[i], &child_allocation, baseline);
    }
}

static void
gtk_center_layout_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (object);

  switch (prop_id)
    {
    case PROP_SHRINK_CENTER_LAST:
      gtk_center_layout_set_shrink_center_last (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_layout_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (object);

  switch (prop_id)
    {
    case PROP_SHRINK_CENTER_LAST:
      g_value_set_boolean (value, self->shrink_center_last);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_layout_dispose (GObject *object)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (object);

  g_clear_object (&self->start_widget);
  g_clear_object (&self->center_widget);
  g_clear_object (&self->end_widget);

  G_OBJECT_CLASS (gtk_center_layout_parent_class)->dispose (object);
}

static void
gtk_center_layout_class_init (GtkCenterLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  object_class->get_property = gtk_center_layout_get_property;
  object_class->set_property = gtk_center_layout_set_property;
  object_class->dispose = gtk_center_layout_dispose;

  layout_class->get_request_mode = gtk_center_layout_get_request_mode;
  layout_class->measure = gtk_center_layout_measure;
  layout_class->allocate = gtk_center_layout_allocate;

  /**
   * GtkCenterLayout:shrink-center-last: (attributes org.gtk.Property.get=gtk_center_layout_get_shrink_center_last org.gtk.Property.set=gtk_center_layout_set_shrink_center_last)
   *
   * Whether to shrink the center widget after other children.
   *
   * By default, when there's no space to give all three children their
   * natural widths, the start and end widgets start shrinking and the
   * center child keeps natural width until they reach minimum width.
   *
   * If set to `FALSE`, start and end widgets keep natural width and the
   * center widget starts shrinking instead.
   *
   * Since: 4.12
   */
  props[PROP_SHRINK_CENTER_LAST] =
      g_param_spec_boolean ("shrink-center-last", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
gtk_center_layout_init (GtkCenterLayout *self)
{
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_pos = GTK_BASELINE_POSITION_CENTER;
  self->shrink_center_last = TRUE;
}

/**
 * gtk_center_layout_new:
 *
 * Creates a new `GtkCenterLayout`.
 *
 * Returns: the newly created `GtkCenterLayout`
 */
GtkLayoutManager *
gtk_center_layout_new (void)
{
  return g_object_new (GTK_TYPE_CENTER_LAYOUT, NULL);
}

/**
 * gtk_center_layout_set_orientation:
 * @self: a `GtkCenterLayout`
 * @orientation: the new orientation
 *
 * Sets the orientation of @self.
 */
void
gtk_center_layout_set_orientation (GtkCenterLayout *self,
                                   GtkOrientation   orientation)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));

  if (orientation != self->orientation)
    {
      self->orientation = orientation;
      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
    }
}

/**
 * gtk_center_layout_get_orientation:
 * @self: a `GtkCenterLayout`
 *
 * Gets the current orienration of the layout manager.
 *
 * Returns: The current orientation of @self
 */
GtkOrientation
gtk_center_layout_get_orientation (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), GTK_ORIENTATION_HORIZONTAL);

  return self->orientation;
}

/**
 * gtk_center_layout_set_baseline_position:
 * @self: a `GtkCenterLayout`
 * @baseline_position: the new baseline position
 *
 * Sets the new baseline position of @self
 */
void
gtk_center_layout_set_baseline_position (GtkCenterLayout     *self,
                                         GtkBaselinePosition  baseline_position)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));

  if (baseline_position != self->baseline_pos)
    {
      self->baseline_pos = baseline_position;
      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
    }
}

/**
 * gtk_center_layout_get_baseline_position:
 * @self: a `GtkCenterLayout`
 *
 * Returns the baseline position of the layout.
 *
 * Returns: The current baseline position of @self.
 */
GtkBaselinePosition
gtk_center_layout_get_baseline_position (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), GTK_BASELINE_POSITION_TOP);

  return self->baseline_pos;
}

/**
 * gtk_center_layout_set_start_widget:
 * @self: a `GtkCenterLayout`
 * @widget: (nullable): the new start widget
 *
 * Sets the new start widget of @self.
 *
 * To remove the existing start widget, pass %NULL.
 */
void
gtk_center_layout_set_start_widget (GtkCenterLayout *self,
                                    GtkWidget       *widget)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (g_set_object (&self->start_widget, widget))
    gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
}

/**
 * gtk_center_layout_get_start_widget:
 * @self: a `GtkCenterLayout`
 *
 * Returns the start widget of the layout.
 *
 * Returns: (nullable) (transfer none): The current start widget of @self
 */
GtkWidget *
gtk_center_layout_get_start_widget (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), NULL);

  return self->start_widget;
}

/**
 * gtk_center_layout_set_center_widget:
 * @self: a `GtkCenterLayout`
 * @widget: (nullable): the new center widget
 *
 * Sets the new center widget of @self.
 *
 * To remove the existing center widget, pass %NULL.
 */
void
gtk_center_layout_set_center_widget (GtkCenterLayout *self,
                                     GtkWidget       *widget)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (g_set_object (&self->center_widget, widget))
    gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
}

/**
 * gtk_center_layout_get_center_widget:
 * @self: a `GtkCenterLayout`
 *
 * Returns the center widget of the layout.
 *
 * Returns: (nullable) (transfer none): the current center widget of @self
 */
GtkWidget *
gtk_center_layout_get_center_widget (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), NULL);

  return self->center_widget;
}

/**
 * gtk_center_layout_set_end_widget:
 * @self: a `GtkCenterLayout`
 * @widget: (nullable): the new end widget
 *
 * Sets the new end widget of @self.
 *
 * To remove the existing center widget, pass %NULL.
 */
void
gtk_center_layout_set_end_widget (GtkCenterLayout *self,
                                  GtkWidget       *widget)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (g_set_object (&self->end_widget, widget))
    gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
}

/**
 * gtk_center_layout_get_end_widget:
 * @self: a `GtkCenterLayout`
 *
 * Returns the end widget of the layout.
 *
 * Returns: (nullable) (transfer none): the current end widget of @self
 */
GtkWidget *
gtk_center_layout_get_end_widget (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), NULL);

  return self->end_widget;
}

/**
 * gtk_center_layout_set_shrink_center_last: (attributes org.gtk.Method.set_property=shrink-center-last)
 * @self: a `GtkCenterLayout`
 * @shrink_center_last: whether to shrink the center widget after others
 *
 * Sets whether to shrink the center widget after other children.
 *
 * By default, when there's no space to give all three children their
 * natural widths, the start and end widgets start shrinking and the
 * center child keeps natural width until they reach minimum width.
 *
 * If set to `FALSE`, start and end widgets keep natural width and the
 * center widget starts shrinking instead.
 *
 * Since: 4.12
 */
void
gtk_center_layout_set_shrink_center_last (GtkCenterLayout *self,
                                          gboolean         shrink_center_last)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (self));

  shrink_center_last = !!shrink_center_last;

  if (shrink_center_last == self->shrink_center_last)
    return;

  self->shrink_center_last = shrink_center_last;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SHRINK_CENTER_LAST]);
}

/**
 * gtk_center_layout_get_shrink_center_last: (attributes org.gtk.Method.get_property=shrink-center-last)
 * @self: a `GtkCenterLayout`
 *
 * Gets whether @self shrinks the center widget after other children.
 *
 * Returns: whether to shrink the center widget after others
 *
 * Since: 4.12
 */
gboolean
gtk_center_layout_get_shrink_center_last (GtkCenterLayout *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (self), FALSE);

  return self->shrink_center_last;
}
