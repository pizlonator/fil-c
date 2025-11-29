/* gtkboxlayout.c: Box layout manager
 *
 * Copyright 2019  GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkboxlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"

/**
 * GtkBoxLayout:
 *
 * `GtkBoxLayout` is a layout manager that arranges children in a single
 * row or column.
 *
 * Whether it is a row or column depends on the value of its
 * [property@Gtk.Orientable:orientation] property. Within the other dimension
 * all children all allocated the same size. The `GtkBoxLayout` will respect
 * the [property@Gtk.Widget:halign] and [property@Gtk.Widget:valign]
 * properties of each child widget.
 *
 * If you want all children to be assigned the same size, you can use
 * the [property@Gtk.BoxLayout:homogeneous] property.
 *
 * If you want to specify the amount of space placed between each child,
 * you can use the [property@Gtk.BoxLayout:spacing] property.
 */

struct _GtkBoxLayout
{
  GtkLayoutManager parent_instance;

  gboolean homogeneous;
  guint spacing;
  GtkOrientation orientation;
  GtkBaselinePosition baseline_position;
  int baseline_child;
};

G_DEFINE_TYPE_WITH_CODE (GtkBoxLayout, gtk_box_layout, GTK_TYPE_LAYOUT_MANAGER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

enum {
  PROP_HOMOGENEOUS = 1,
  PROP_SPACING,
  PROP_BASELINE_CHILD,
  PROP_BASELINE_POSITION,

  /* From GtkOrientable */
  PROP_ORIENTATION,

  N_PROPS = PROP_ORIENTATION
};

static GParamSpec *box_layout_props[N_PROPS];

static void
gtk_box_layout_set_orientation (GtkBoxLayout   *self,
                                GtkOrientation  orientation)
{
  GtkLayoutManager *layout_manager = GTK_LAYOUT_MANAGER (self);
  GtkWidget *widget;

  if (self->orientation == orientation)
    return;

  self->orientation = orientation;

  widget = gtk_layout_manager_get_widget (layout_manager);
  if (widget != NULL && GTK_IS_ORIENTABLE (widget))
    gtk_widget_update_orientation (widget, self->orientation);

  gtk_layout_manager_layout_changed (layout_manager);

  g_object_notify (G_OBJECT (self), "orientation");
}

static void
gtk_box_layout_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      gtk_box_layout_set_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_SPACING:
      gtk_box_layout_set_spacing (self, g_value_get_int (value));
      break;

    case PROP_BASELINE_CHILD:
      gtk_box_layout_set_baseline_child (self, g_value_get_int (value));
      break;

    case PROP_BASELINE_POSITION:
      gtk_box_layout_set_baseline_position (self, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      gtk_box_layout_set_orientation (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_box_layout_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, box_layout->homogeneous);
      break;

    case PROP_SPACING:
      g_value_set_int (value, box_layout->spacing);
      break;

    case PROP_BASELINE_CHILD:
      g_value_set_int (value, box_layout->baseline_child);
      break;

    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, box_layout->baseline_position);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, box_layout->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
count_expand_children (GtkWidget *widget,
                       GtkOrientation orientation,
                       int *visible_children,
                       int *expand_children)
{
  GtkWidget *child;

  *visible_children = *expand_children = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      *visible_children += 1;

      if (gtk_widget_compute_expand (child, orientation))
        *expand_children += 1;
    }
}

static int
get_spacing (GtkBoxLayout *self,
             GtkCssNode   *node)
{
  GtkCssStyle *style = gtk_css_node_get_style (node);
  GtkCssValue *border_spacing;
  int css_spacing;

  border_spacing = style->size->border_spacing;
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + self->spacing;
}

static void
gtk_box_layout_compute_size (GtkBoxLayout *self,
                             GtkWidget    *widget,
                             int           for_size,
                             int          *minimum,
                             int          *natural,
                             int          *minimum_baseline,
                             int          *natural_baseline)
{
  GtkWidget *child;
  int n_visible_children = 0;
  int required_min = 0, required_nat = 0;
  int largest_min = 0, largest_nat = 0;
  int spacing = get_spacing (self, gtk_widget_get_css_node (widget));
  int child_above_min = 0, child_above_nat = 0;
  int above_min = 0, above_nat = 0;
  gboolean have_baseline = FALSE;
  int pos;

  for (child = gtk_widget_get_first_child (widget), pos = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), pos++)
    {
      int child_min = 0;
      int child_nat = 0;
      int child_min_baseline = -1;
      int child_nat_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, self->orientation,
                          for_size,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      largest_min = MAX (largest_min, child_min);
      largest_nat = MAX (largest_nat, child_nat);

      required_min += child_min;
      required_nat += child_nat;

      if (self->orientation == GTK_ORIENTATION_VERTICAL)
        {
          if (pos < self->baseline_child)
            {
              above_min += child_min;
              above_nat += child_nat;
            }
          else if (pos == self->baseline_child)
            {
              have_baseline = TRUE;
              if (child_min_baseline > -1)
                {
                  child_above_min = child_min_baseline;
                  child_above_nat = child_nat_baseline;
                }
              else
                {
                  child_above_min = child_min;
                  child_above_nat = child_nat;
                }
            }
        }

      n_visible_children += 1;
    }

  if (n_visible_children > 0)
    {
      if (self->homogeneous)
        {
          required_min = largest_min * n_visible_children;
          required_nat = largest_nat * n_visible_children;

          above_min = largest_min * MAX (self->baseline_child, 0);
          above_nat = largest_nat * MAX (self->baseline_child, 0);
        }

      required_min += (n_visible_children - 1) * spacing;
      required_nat += (n_visible_children - 1) * spacing;

      above_min += MAX (self->baseline_child, 0) * spacing;
      above_nat += MAX (self->baseline_child, 0) * spacing;
    }

  *minimum = required_min;
  *natural = required_nat;

  if (have_baseline)
    {
      *minimum_baseline = above_min + child_above_min;
      *natural_baseline = above_nat + child_above_nat;
    }
  else
    {
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
}

static void
gtk_box_layout_compute_opposite_size (GtkBoxLayout *self,
                                      GtkWidget    *widget,
                                      int          *minimum,
                                      int          *natural,
                                      int          *min_baseline,
                                      int          *nat_baseline)
{
  GtkWidget *child;
  int largest_min = 0, largest_nat = 0;
  int largest_min_above = -1, largest_min_below = -1;
  int largest_nat_above = -1, largest_nat_below = -1;
  gboolean have_baseline = FALSE;
  gboolean align_baseline = FALSE;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min = 0;
      int child_nat = 0;
      int child_min_baseline = -1;
      int child_nat_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child,
                          OPPOSITE_ORIENTATION (self->orientation),
                          -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      largest_min = MAX (largest_min, child_min);
      largest_nat = MAX (largest_nat, child_nat);

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (child_min_baseline > -1)
            {
              have_baseline = TRUE;
              if (gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_FILL ||
                  gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_CENTER)
                align_baseline = TRUE;

              largest_min_above = MAX (largest_min_above, child_min_baseline);
              largest_min_below = MAX (largest_min_below, child_min - child_min_baseline);
              largest_nat_above = MAX (largest_nat_above, child_nat_baseline);
              largest_nat_below = MAX (largest_nat_below, child_nat - child_nat_baseline);
            }
        }
    }

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL && align_baseline)
    {
      largest_min = MAX (largest_min, largest_min_above + largest_min_below);
      largest_nat = MAX (largest_nat, largest_nat_above + largest_nat_below);
    }

  *minimum = largest_min;
  *natural = largest_nat;

  if (have_baseline)
    {
      *min_baseline = largest_min_above;
      *nat_baseline = largest_nat_above;
    }
  else
    {
      *min_baseline = -1;
      *nat_baseline = -1;
    }
}

/* if widgets haven't reached their min opposite size at this
 * huge value, things went massively wrong and we need to bail to not
 * cause an infinite loop.
 */
#define MAX_ALLOWED_SIZE (1 << 20)

static int
distribute_remaining_size (GtkRequestedSize *sizes,
                           gsize             n_sizes,
                           GtkOrientation    orientation,
                           int               available,
                           int               min,
                           int               max)
{
  int total_size = 0;
  gsize i;

  if (n_sizes == 0)
    return available;

  for (i = 0; i < n_sizes; i++)
    {
      gtk_widget_measure (sizes[i].data,
                          orientation,
                          min,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);
      total_size += sizes[i].minimum_size;
    }

  if (total_size <= available)
    return available - total_size;

  /* total_size > available happens when we last ran for values too big,
   * rerun for the correct value min == max in that case */
  while (min < max || total_size > available)
    {
      int test;

      if (min > MAX_ALLOWED_SIZE)
        {
          /* sanity check! */
          for (i = 0; i < n_sizes; i++)
            {
              int check_min, check_nat;
              gtk_widget_measure (sizes[i].data,
                                  orientation,
                                  MAX_ALLOWED_SIZE,
                                  &sizes[i].minimum_size, &sizes[i].natural_size,
                                  NULL, NULL);
              gtk_widget_measure (sizes[i].data,
                                  orientation,
                                  -1,
                                  &check_min, &check_nat,
                                  NULL, NULL);
              if (check_min < sizes[i].minimum_size)
                {
                  g_critical ("%s %p reports a minimum %s of %u, but minimum %s for %s of %u is %u. Expect overlapping widgets.",
                              G_OBJECT_TYPE_NAME (sizes[i].data), sizes[i].data,
                              orientation == GTK_ORIENTATION_HORIZONTAL ? "width" : "height",
                              check_min,
                              orientation == GTK_ORIENTATION_HORIZONTAL ? "width" : "height",
                              orientation == GTK_ORIENTATION_HORIZONTAL ? "height" : "width",
                              MAX_ALLOWED_SIZE, sizes[i].minimum_size);
                  sizes[i].minimum_size = check_min;
                  sizes[i].natural_size = check_nat;
                }
              total_size += sizes[i].minimum_size;
            }
          return MAX (0, available - total_size);
        }

      if (max == MAX_ALLOWED_SIZE)
        test = min * 2;
      else
        test = (min + max) / 2;

      total_size = 0;
      for (i = 0; i < n_sizes; i++)
        {
          gtk_widget_measure (sizes[i].data,
                              orientation,
                              test,
                              &sizes[i].minimum_size, &sizes[i].natural_size,
                              NULL, NULL);
          total_size += sizes[i].minimum_size;
        }

      if (total_size > available)
        min = test + 1;
      else
        max = test;
    }
    
  return available - total_size;
}

static void
gtk_box_layout_compute_opposite_size_for_size (GtkBoxLayout *self,
                                               GtkWidget    *widget,
                                               int           for_size,
                                               int          *minimum,
                                               int          *natural,
                                               int          *min_baseline,
                                               int          *nat_baseline)
{
  GtkWidget *child;
  int nvis_children;
  int nexpand_children;
  int computed_minimum = 0, computed_natural = 0;
  int computed_minimum_above = 0, computed_natural_above = 0;
  int computed_minimum_below = 0, computed_natural_below = 0;
  int computed_minimum_baseline = -1, computed_natural_baseline = -1;
  GtkRequestedSize *sizes;
  int available, size_given_to_child, i;
  int child_size, child_minimum, child_natural;
  int child_minimum_baseline, child_natural_baseline;
  int n_extra_widgets = 0;
  int spacing;
  gboolean have_baseline = FALSE;

  count_expand_children (widget, self->orientation, &nvis_children, &nexpand_children);

  if (nvis_children <= 0)
    return;

  spacing = get_spacing (self, gtk_widget_get_css_node (widget));
  sizes = g_newa (GtkRequestedSize, nvis_children);
  g_assert ((nvis_children - 1) * spacing <= for_size);
  available = for_size - (nvis_children - 1) * spacing;

  if (self->homogeneous)
    {
      size_given_to_child = available / nvis_children;
      n_extra_widgets = available % nvis_children;

      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_should_layout (child))
            continue;

          child_size = size_given_to_child;
          if (n_extra_widgets)
            {
              child_size++;
              n_extra_widgets--;
            }

          child_minimum_baseline = child_natural_baseline = -1;
          /* Assign the child's position. */
          gtk_widget_measure (child,
                              OPPOSITE_ORIENTATION (self->orientation),
                              child_size,
                              &child_minimum, &child_natural,
                              &child_minimum_baseline, &child_natural_baseline);

          if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              if (child_minimum_baseline > -1)
                {
                  have_baseline = TRUE;
                  computed_minimum_below = MAX (computed_minimum_below, child_minimum - child_minimum_baseline);
                  computed_natural_below = MAX (computed_natural_below, child_natural - child_natural_baseline);
                  computed_minimum_above = MAX (computed_minimum_above, child_minimum_baseline);
                  computed_natural_above = MAX (computed_natural_above, child_natural_baseline);
                }
              else
                {
                  computed_minimum = MAX (computed_minimum, child_minimum);
                  computed_natural = MAX (computed_natural, child_natural);
                }
            }
          else
            {
              computed_minimum = MAX (computed_minimum, child_minimum);
              computed_natural = MAX (computed_natural, child_natural);
            }
        }
    }
  else
    {
      int min_size = 0, child_min_size;
      int n_inconstant = 0;

      /* Retrieve desired size for visible children */
      for (i = 0, child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_should_layout (child))
            continue;

          if (gtk_widget_get_request_mode (child) == GTK_SIZE_REQUEST_CONSTANT_SIZE)
            {
              gtk_widget_measure (child,
                                  self->orientation,
                                  -1,
                                  &sizes[i].minimum_size, &sizes[i].natural_size,
                                  NULL, NULL);
              sizes[i].data = child;
              g_assert (available >= sizes[i].minimum_size);
              available -= sizes[i].minimum_size;
              i++;
            }
          else
            {
              gtk_widget_measure (child,
                                  OPPOSITE_ORIENTATION (self->orientation),
                                  -1,
                                  &child_min_size, NULL,
                                  NULL, NULL);
              min_size = MAX (min_size, child_min_size);
              n_inconstant++;
              sizes[nvis_children - n_inconstant].data = child;
            }
        }

      available = distribute_remaining_size (sizes + nvis_children - n_inconstant,
                                             n_inconstant,
                                             self->orientation,
                                             available,
                                             min_size,
                                             MAX_ALLOWED_SIZE);

      /* Bring children up to size first */
      available = gtk_distribute_natural_allocation (available, nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          size_given_to_child = available / nexpand_children;
          n_extra_widgets = available % nexpand_children;
        }
      else
        {
          size_given_to_child = 0;
        }

      i = 0;
      n_inconstant = 0;
      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_should_layout (child))
            continue;

          if (sizes[i].data == child)
            {
              child_size = sizes[i].minimum_size;
              i++;
            }
          else
            {
              n_inconstant++;
              g_assert (sizes[nvis_children - n_inconstant].data == child);
              child_size = sizes[nvis_children - n_inconstant].minimum_size;
            }

          if (gtk_widget_compute_expand (child, self->orientation))
            {
              child_size += size_given_to_child;

              if (n_extra_widgets > 0)
                {
                  child_size++;
                  n_extra_widgets--;
                }
            }

          child_minimum_baseline = child_natural_baseline = -1;
          /* Assign the child's position. */
          gtk_widget_measure (child,
                              OPPOSITE_ORIENTATION (self->orientation),
                              child_size,
                              &child_minimum, &child_natural,
                              &child_minimum_baseline, &child_natural_baseline);

          if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              if (child_minimum_baseline > -1)
                {
                  have_baseline = TRUE;
                  computed_minimum_below = MAX (computed_minimum_below, child_minimum - child_minimum_baseline);
                  computed_natural_below = MAX (computed_natural_below, child_natural - child_natural_baseline);
                  computed_minimum_above = MAX (computed_minimum_above, child_minimum_baseline);
                  computed_natural_above = MAX (computed_natural_above, child_natural_baseline);
                }
              else
                {
                  computed_minimum = MAX (computed_minimum, child_minimum);
                  computed_natural = MAX (computed_natural, child_natural);
                }
            }
          else
            {
              computed_minimum = MAX (computed_minimum, child_minimum);
              computed_natural = MAX (computed_natural, child_natural);
            }
        }
    }

  if (have_baseline)
    {
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          computed_minimum = MAX (computed_minimum, computed_minimum_below + computed_minimum_above);
          computed_natural = MAX (computed_natural, computed_natural_below + computed_natural_above);
          switch (self->baseline_position)
            {
            case GTK_BASELINE_POSITION_TOP:
              computed_minimum_baseline = computed_minimum_above;
              computed_natural_baseline = computed_natural_above;
            break;
            case GTK_BASELINE_POSITION_CENTER:
              computed_minimum_baseline = computed_minimum_above + MAX((computed_minimum - (computed_minimum_above + computed_minimum_below)) / 2, 0);
              computed_natural_baseline = computed_natural_above + MAX((computed_natural - (computed_natural_above + computed_natural_below)) / 2, 0);
              break;
            case GTK_BASELINE_POSITION_BOTTOM:
              computed_minimum_baseline = computed_minimum - computed_minimum_below;
              computed_natural_baseline = computed_natural - computed_natural_below;
              break;
            default:
              break;
            }
        }
    }

  *minimum = computed_minimum;
  *natural = MAX (computed_natural, computed_natural_below + computed_natural_above);
  *min_baseline = computed_minimum_baseline;
  *nat_baseline = computed_natural_baseline;
}

static void
gtk_box_layout_measure (GtkLayoutManager *layout_manager,
                        GtkWidget        *widget,
                        GtkOrientation    orientation,
                        int               for_size,
                        int              *minimum,
                        int              *natural,
                        int              *min_baseline,
                        int              *nat_baseline)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (layout_manager);

  if (self->orientation != orientation)
    {
      if (for_size < 0)
        {
          gtk_box_layout_compute_opposite_size (self, widget,
                                                minimum, natural,
                                                min_baseline, nat_baseline);
        }
      else
        {
          gtk_box_layout_compute_opposite_size_for_size (self, widget, for_size,
                                                         minimum, natural,
                                                         min_baseline, nat_baseline);
        }
    }
  else
    {
      gtk_box_layout_compute_size (self, widget, for_size,
                                   minimum, natural,
                                   min_baseline, nat_baseline);
    }
}

static void
gtk_box_layout_allocate (GtkLayoutManager *layout_manager,
                         GtkWidget        *widget,
                         int               width,
                         int               height,
                         int               baseline)
{
  GtkBoxLayout *self = GTK_BOX_LAYOUT (layout_manager);
  GtkWidget *child;
  int nvis_children;
  int nexpand_children;
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GtkRequestedSize *sizes;
  int child_minimum_baseline, child_natural_baseline;
  int minimum_above, natural_above;
  int minimum_below, natural_below;
  gboolean have_baseline;
  int extra_space;
  int children_minimum_size = 0;
  int size_given_to_child;
  int n_extra_widgets = 0; /* Number of widgets that receive 1 extra px */
  int x = 0, y = 0, i;
  int child_size;
  int spacing;

  count_expand_children (widget, self->orientation, &nvis_children, &nexpand_children);

  /* If there is no visible child, simply return. */
  if (nvis_children <= 0)
    return;

  direction = _gtk_widget_get_direction (widget);
  sizes = g_newa (GtkRequestedSize, nvis_children);
  spacing = get_spacing (self, gtk_widget_get_css_node (widget));

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    extra_space = width - (nvis_children - 1) * spacing;
  else
    extra_space = height - (nvis_children - 1) * spacing;

  have_baseline = FALSE;
  minimum_above = natural_above = 0;
  minimum_below = natural_below = 0;

  /* Retrieve desired size for visible children. */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child,
                          self->orientation,
                          self->orientation == GTK_ORIENTATION_HORIZONTAL ? height : width,
                          &sizes[i].minimum_size, &sizes[i].natural_size,
                          NULL, NULL);

      children_minimum_size += sizes[i].minimum_size;

      sizes[i].data = child;

      i++;
    }

  if (self->homogeneous)
    {
      /* We still need to run the above loop to populate the minimum sizes for
       * children that aren't going to fill.
       */
      size_given_to_child = extra_space / nvis_children;
      n_extra_widgets = extra_space % nvis_children;
    }
  else
    {
      /* Bring children up to size first */
      extra_space -= children_minimum_size;
      extra_space = MAX (0, extra_space);
      extra_space = gtk_distribute_natural_allocation (extra_space, nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          size_given_to_child = extra_space / nexpand_children;
          n_extra_widgets = extra_space % nexpand_children;
        }
      else
        {
          size_given_to_child = 0;
        }
    }

  /* Allocate child sizes. */
  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      /* Assign the child's size. */
      if (self->homogeneous)
        {
          child_size = size_given_to_child;

          if (n_extra_widgets > 0)
            {
              child_size++;
              n_extra_widgets--;
            }
        }
      else
        {
          child_size = sizes[i].minimum_size;

          if (gtk_widget_compute_expand (child, self->orientation))
            {
              child_size += size_given_to_child;

              if (n_extra_widgets > 0)
                {
                  child_size++;
                  n_extra_widgets--;
                }
            }
        }

      sizes[i].natural_size = child_size;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
          (gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_FILL ||
           gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_CENTER))
        {
          int child_allocation_width;
          int child_minimum_height, child_natural_height;

          child_allocation_width = child_size;

          child_minimum_baseline = -1;
          child_natural_baseline = -1;
          gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                              child_allocation_width,
                              &child_minimum_height, &child_natural_height,
                              &child_minimum_baseline, &child_natural_baseline);

          if (child_minimum_baseline >= 0)
            {
              have_baseline = TRUE;
              minimum_below = MAX (minimum_below, child_minimum_height - child_minimum_baseline);
              natural_below = MAX (natural_below, child_natural_height - child_natural_baseline);
              minimum_above = MAX (minimum_above, child_minimum_baseline);
              natural_above = MAX (natural_above, child_natural_baseline);
            }
        }

      i++;
    }

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    baseline = -1;

  /* we only calculate our own baseline if we don't get one passed from the parent
   * and any of the child widgets explicitly request one */
  if (baseline == -1 && have_baseline)
    {
      /* TODO: This is purely based on the minimum baseline, when things fit we should
         use the natural one? */

      switch (self->baseline_position)
        {
        case GTK_BASELINE_POSITION_TOP:
          baseline = minimum_above;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          baseline = minimum_above + (height - (minimum_above + minimum_below)) / 2;
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          baseline = height - minimum_below;
          break;
        default:
          break;
        }
    }

  /* Allocate child positions. */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      child_allocation.y = 0;
      child_allocation.height = height;
      x = 0;
    }
  else
    {
      child_allocation.x = 0;
      child_allocation.width = width;
      y = 0;
    }

  for (i = 0, child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      child_size = sizes[i].natural_size;

      /* Assign the child's position. */
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.width = child_size;
          child_allocation.x = x;

          x += child_size + spacing;

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = width - child_allocation.x - child_allocation.width;

        }
      else /* (self->orientation == GTK_ORIENTATION_VERTICAL) */
        {
          child_allocation.height = child_size;
          child_allocation.y = y;

          y += child_size + spacing;
        }

      gtk_widget_size_allocate (child, &child_allocation, baseline);

      i++;
    }
}

static void
gtk_box_layout_class_init (GtkBoxLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = gtk_box_layout_set_property;
  gobject_class->get_property = gtk_box_layout_get_property;

  layout_manager_class->measure = gtk_box_layout_measure;
  layout_manager_class->allocate = gtk_box_layout_allocate;

  /**
   * GtkBoxLayout:homogeneous: (attributes org.gtk.Property.get=gtk_box_layout_get_homogeneous org.gtk.Property.set=gtk_box_layout_set_homogeneous)
   *
   * Whether the box layout should distribute the available space
   * equally among the children.
   */
  box_layout_props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBoxLayout:spacing: (attributes org.gtk.Property.get=gtk_box_layout_get_spacing org.gtk.Property.set=gtk_box_layout_set_spacing)
   *
   * The space to put between the children.
   */
  box_layout_props[PROP_SPACING] =
    g_param_spec_int ("spacing", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBoxLayout:baseline-child: (attributes org.gtk.Property.get=gtk_box_layout_get_baseline_child org.gtk.Property.set=gtk_box_layout_set_baseline_child)
   *
   * The child that determines the baseline of the box
   * in vertical layout.
   *
   * If the child does baseline positioning, then its baseline
   * is lined up with the baseline of the box. If it doesn't, then
   * the bottom edge of the child is used.
   *
   * Since: 4.12
   */
  box_layout_props[PROP_BASELINE_CHILD] =
    g_param_spec_int ("baseline-child", NULL, NULL,
                      -1, G_MAXINT, -1,
                      GTK_PARAM_READWRITE |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkBoxLayout:baseline-position: (attributes org.gtk.Property.get=gtk_box_layout_get_baseline_position org.gtk.Property.set=gtk_box_layout_set_baseline_position)
   *
   * The position of the allocated baseline within the extra space
   * allocated to each child.
   *
   * This property is only relevant for horizontal layouts containing
   * at least one child with a baseline alignment.
   */
  box_layout_props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position", NULL, NULL,
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       GTK_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, box_layout_props);
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");
}

static void
gtk_box_layout_init (GtkBoxLayout *self)
{
  self->homogeneous = FALSE;
  self->spacing = 0;
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_position = GTK_BASELINE_POSITION_CENTER;
  self->baseline_child = -1;
}

/**
 * gtk_box_layout_new:
 * @orientation: the orientation for the new layout
 *
 * Creates a new `GtkBoxLayout`.
 *
 * Returns: a new box layout
 */
GtkLayoutManager *
gtk_box_layout_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_BOX_LAYOUT,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_box_layout_set_homogeneous: (attributes org.gtk.Method.set_property=homogeneous)
 * @box_layout: a `GtkBoxLayout`
 * @homogeneous: %TRUE to set the box layout as homogeneous
 *
 * Sets whether the box layout will allocate the same
 * size to all children.
 */
void
gtk_box_layout_set_homogeneous (GtkBoxLayout *box_layout,
                                gboolean      homogeneous)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  homogeneous = !!homogeneous;
  if (box_layout->homogeneous == homogeneous)
    return;

  box_layout->homogeneous = homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_HOMOGENEOUS]);
}

/**
 * gtk_box_layout_get_homogeneous: (attributes org.gtk.Method.get_property=homogeneous)
 * @box_layout: a `GtkBoxLayout`
 *
 * Returns whether the layout is set to be homogeneous.
 *
 * Return: %TRUE if the layout is homogeneous
 */
gboolean
gtk_box_layout_get_homogeneous (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), FALSE);

  return box_layout->homogeneous;
}

/**
 * gtk_box_layout_set_spacing: (attributes org.gtk.Method.set_property=spacing)
 * @box_layout: a `GtkBoxLayout`
 * @spacing: the spacing to apply between children
 *
 * Sets how much spacing to put between children.
 */
void
gtk_box_layout_set_spacing (GtkBoxLayout *box_layout,
                            guint         spacing)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->spacing == spacing)
    return;

  box_layout->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_SPACING]);
}

/**
 * gtk_box_layout_get_spacing: (attributes org.gtk.Method.get_property=spacing)
 * @box_layout: a `GtkBoxLayout`
 *
 * Returns the space that @box_layout puts between children.
 *
 * Returns: the spacing of the layout
 */
guint
gtk_box_layout_get_spacing (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), 0);

  return box_layout->spacing;
}

/**
 * gtk_box_layout_set_baseline_position: (attributes org.gtk.Method.set_property=baseline-position)
 * @box_layout: a `GtkBoxLayout`
 * @position: a `GtkBaselinePosition`
 *
 * Sets the baseline position of a box layout.
 *
 * The baseline position affects only horizontal boxes with at least one
 * baseline aligned child. If there is more vertical space available than
 * requested, and the baseline is not allocated by the parent then the
 * given @position is used to allocate the baseline within the extra
 * space available.
 */
void
gtk_box_layout_set_baseline_position (GtkBoxLayout        *box_layout,
                                      GtkBaselinePosition  position)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->baseline_position != position)
    {
      box_layout->baseline_position = position;

      g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_BASELINE_POSITION]);

      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
    }
}

/**
 * gtk_box_layout_get_baseline_position: (attributes org.gtk.Method.get_property=baseline-position)
 * @box_layout: a `GtkBoxLayout`
 *
 * Gets the value set by gtk_box_layout_set_baseline_position().
 *
 * Returns: the baseline position
 */
GtkBaselinePosition
gtk_box_layout_get_baseline_position (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), GTK_BASELINE_POSITION_CENTER);

  return box_layout->baseline_position;
}

/**
 * gtk_box_layout_set_baseline_child: (attributes org.gtk.Method.set_property=baseline-child)
 * @box_layout: a `GtkBoxLayout`
 * @child: the child position, or -1
 *
 * Sets the index of the child that determines the baseline
 * in vertical layout.
 *
 * Since: 4.12
 */
void
gtk_box_layout_set_baseline_child (GtkBoxLayout *box_layout,
                                   int           child)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));
  g_return_if_fail (child >= -1);

  if (box_layout->baseline_child == child)
    return;

  box_layout->baseline_child = child;

  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_BASELINE_CHILD]);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
}

/**
 * gtk_box_layout_get_baseline_child:
 * @box_layout: a `GtkBoxLayout`
 *
 * Gets the value set by gtk_box_layout_set_baseline_child().
 *
 * Returns: the index of the child that determines the baseline
 *     in vertical layout, or -1
 *
 * Since: 4.12
 */
int
gtk_box_layout_get_baseline_child (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), -1);

  return box_layout->baseline_child;
}
