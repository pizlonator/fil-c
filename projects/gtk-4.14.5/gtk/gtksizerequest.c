/* gtksizerequest.c
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Mathias Hasselmann <mathias@openismus.com>
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "gtksizerequest.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtksizegroup-private.h"
#include "gtksizerequestcacheprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtklayoutmanagerprivate.h"


#ifdef G_ENABLE_CONSISTENCY_CHECKS
static GQuark recursion_check_quark = 0;

static void
push_recursion_check (GtkWidget       *widget,
                      GtkOrientation   orientation)
{
  gboolean in_measure = FALSE;

  if (recursion_check_quark == 0)
    recursion_check_quark = g_quark_from_static_string ("gtk-size-request-in-progress");

  in_measure = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), recursion_check_quark));

  if (in_measure)
    {
      g_warning ("%s %p: widget tried to gtk_widget_measure inside "
                 " GtkWidget::measure implementation. "
                 "Should just invoke GTK_WIDGET_GET_CLASS(widget)->measure "
                 "directly rather than using gtk_widget_measure",
                 G_OBJECT_TYPE_NAME (widget), widget);
    }

  g_object_set_qdata (G_OBJECT (widget), recursion_check_quark, GINT_TO_POINTER(TRUE));
}

static void
pop_recursion_check (GtkWidget       *widget,
                     GtkOrientation   orientation)
{
  g_object_set_qdata (G_OBJECT (widget), recursion_check_quark, NULL);
}
#else
#define push_recursion_check(widget, orientation)
#define pop_recursion_check(widget, orientation)
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

static GtkSizeRequestMode
fetch_request_mode (GtkWidget *widget)
{
  GtkLayoutManager *layout_manager = gtk_widget_get_layout_manager (widget);

  if (layout_manager != NULL)
    return gtk_layout_manager_get_request_mode (layout_manager);
  else
    return GTK_WIDGET_GET_CLASS (widget)->get_request_mode (widget);
}

static int
get_number (GtkCssValue *value)
{
  double d = _gtk_css_number_value_get (value, 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

/* Special-case min-width|height to round upwards, to avoid underalloc by 1px */
static int
get_number_ceil (GtkCssValue *value)
{
  return ceil (_gtk_css_number_value_get (value, 100));
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style->size->margin_top);
  margin->left = get_number (style->size->margin_left);
  margin->bottom = get_number (style->size->margin_bottom);
  margin->right = get_number (style->size->margin_right);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style->border->border_top_width);
  border->left = get_number (style->border->border_left_width);
  border->bottom = get_number (style->border->border_bottom_width);
  border->right = get_number (style->border->border_right_width);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style->size->padding_top);
  border->left = get_number (style->size->padding_left);
  border->bottom = get_number (style->size->padding_bottom);
  border->right = get_number (style->size->padding_right);
}

static void
gtk_widget_query_size_for_orientation (GtkWidget        *widget,
                                       GtkOrientation    orientation,
                                       int               for_size,
                                       int              *minimum,
                                       int              *natural,
                                       int              *minimum_baseline,
                                       int              *natural_baseline)
{
  SizeRequestCache *cache;
  int min_size = 0;
  int nat_size = 0;
  int min_baseline = -1;
  int nat_baseline = -1;
  gboolean found_in_cache;

  gtk_widget_ensure_resize (widget);

  /* We check the request mode first, to determine whether the widget even does
   * any wfh/hfw handling. If it doesn't, we reset for_size to -1 and ensure
   * that we only cache one size for the widget (i.e. a lot more cache hits). */
  cache = _gtk_widget_peek_request_cache (widget);
  if (G_UNLIKELY (!cache->request_mode_valid))
    {
      cache->request_mode = fetch_request_mode (widget);
      cache->request_mode_valid = TRUE;
    }

  if (cache->request_mode == GTK_SIZE_REQUEST_CONSTANT_SIZE)
    for_size = -1;

  found_in_cache = _gtk_size_request_cache_lookup (cache,
                                                   orientation,
                                                   for_size,
                                                   &min_size,
                                                   &nat_size,
                                                   &min_baseline,
                                                   &nat_baseline);

  if (!found_in_cache)
    {
      GtkWidgetClass *widget_class;
      GtkCssStyle *style;
      GtkBorder margin, border, padding;
      int adjusted_min, adjusted_natural;
      int reported_min_size = 0;
      int reported_nat_size = 0;
      int css_min_size;
      int css_min_for_size;
      int css_extra_for_size;
      int css_extra_size;
      int widget_margins_for_size;

      style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
      get_box_margin (style, &margin);
      get_box_border (style, &border);
      get_box_padding (style, &padding);

      widget_class = GTK_WIDGET_GET_CLASS (widget);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          css_extra_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
          css_extra_for_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
          css_min_size = get_number_ceil (style->size->min_width);
          css_min_for_size = get_number_ceil (style->size->min_height);
          widget_margins_for_size = widget->priv->margin.top + widget->priv->margin.bottom;
        }
      else
        {
          css_extra_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
          css_extra_for_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
          css_min_size = get_number_ceil (style->size->min_height);
          css_min_for_size = get_number_ceil (style->size->min_width);
          widget_margins_for_size = widget->priv->margin.left + widget->priv->margin.right;
        }

      GtkLayoutManager *layout_manager = gtk_widget_get_layout_manager (widget);

      if (layout_manager != NULL)
        {
          if (for_size < 0)
            {
              push_recursion_check (widget, orientation);
              gtk_layout_manager_measure (layout_manager, widget,
                                          orientation, -1,
                                          &reported_min_size, &reported_nat_size,
                                          &min_baseline, &nat_baseline);
              pop_recursion_check (widget, orientation);
            }
          else
            {
              int adjusted_for_size;
              int minimum_for_size = 0;
              int natural_for_size = 0;

              /* Pull the minimum for_size from the cache as it's needed to adjust
               * the proposed 'for_size' */
              gtk_layout_manager_measure (layout_manager, widget,
                                          OPPOSITE_ORIENTATION (orientation), -1,
                                          &minimum_for_size, &natural_for_size,
                                          NULL, NULL);

              if (minimum_for_size < css_min_for_size)
                minimum_for_size = css_min_for_size;

              if (for_size < minimum_for_size)
                for_size = minimum_for_size;

              adjusted_for_size = for_size - widget_margins_for_size;
              adjusted_for_size -= css_extra_for_size;
              if (adjusted_for_size < 0)
                adjusted_for_size = minimum_for_size;

              push_recursion_check (widget, orientation);
              gtk_layout_manager_measure (layout_manager, widget,
                                          orientation,
                                          adjusted_for_size,
                                          &reported_min_size, &reported_nat_size,
                                          &min_baseline, &nat_baseline);
              pop_recursion_check (widget, orientation);
            }
        }
      else
        {
          if (for_size < 0)
            {
              push_recursion_check (widget, orientation);
              widget_class->measure (widget, orientation, -1,
                                     &reported_min_size, &reported_nat_size,
                                     &min_baseline, &nat_baseline);
              pop_recursion_check (widget, orientation);
            }
          else
            {
              int adjusted_for_size;
              int minimum_for_size = 0;
              int natural_for_size = 0;

              /* Pull the minimum for_size from the cache as it's needed to adjust
               * the proposed 'for_size' */
              gtk_widget_measure (widget, OPPOSITE_ORIENTATION (orientation), -1,
                                  &minimum_for_size, &natural_for_size, NULL, NULL);

              if (minimum_for_size < css_min_for_size)
                minimum_for_size = css_min_for_size;

              if (for_size < minimum_for_size)
                for_size = minimum_for_size;

              adjusted_for_size = for_size - widget_margins_for_size;
              adjusted_for_size -= css_extra_for_size;
              if (adjusted_for_size < 0)
                adjusted_for_size = minimum_for_size;

              push_recursion_check (widget, orientation);
              widget_class->measure (widget,
                                     orientation,
                                     adjusted_for_size,
                                     &reported_min_size, &reported_nat_size,
                                     &min_baseline, &nat_baseline);
              pop_recursion_check (widget, orientation);
            }
        }

      min_size = MAX (0, MAX (reported_min_size, css_min_size)) + css_extra_size;
      nat_size = MAX (0, MAX (reported_nat_size, css_min_size)) + css_extra_size;

      if (G_UNLIKELY (min_size > nat_size))
        {
          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              g_warning ("%s %p (%s) reported min width %d and natural width %d in measure() with for_size=%d; natural size must be >= min size",
                         G_OBJECT_TYPE_NAME (widget), widget,
                         g_quark_to_string (gtk_css_node_get_name (gtk_widget_get_css_node (widget))),
                         min_size, nat_size, for_size);
            }
          else
            {
              g_warning ("%s %p (%s) reported min height %d and natural height %d in measure() with for_size=%d; natural size must be >= min size",
                         G_OBJECT_TYPE_NAME (widget), widget,
                         g_quark_to_string (gtk_css_node_get_name (gtk_widget_get_css_node (widget))),
                         min_size, nat_size, for_size);

            }

          nat_size = min_size;
        }
      else if (G_UNLIKELY (min_size < 0))
        {
          g_warning ("%s %p (%s) reported min %s %d, but sizes must be >= 0",
                     G_OBJECT_TYPE_NAME (widget), widget,
                     g_quark_to_string (gtk_css_node_get_name (gtk_widget_get_css_node (widget))),
                     orientation == GTK_ORIENTATION_HORIZONTAL ? "width" : "height",
                     min_size);
          min_size = 0;
          nat_size = MAX (0, min_size);
        }

      adjusted_min     = min_size;
      adjusted_natural = nat_size;
      gtk_widget_adjust_size_request (widget, orientation,
                                      &adjusted_min, &adjusted_natural);

      if (adjusted_min < min_size ||
          adjusted_natural < nat_size)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d must not decrease below min %d natural %d",
                     G_OBJECT_TYPE_NAME (widget), widget,
                     orientation == GTK_ORIENTATION_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     min_size, nat_size);
          /* don't use the adjustment */
        }
      else if (adjusted_min > adjusted_natural)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d original min %d natural %d has min greater than natural",
                     G_OBJECT_TYPE_NAME (widget), widget,
                     orientation == GTK_ORIENTATION_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     min_size, nat_size);
          /* don't use the adjustment */
        }
      else
        {
          /* adjustment looks good */
          min_size = adjusted_min;
          nat_size = adjusted_natural;
        }

      if (min_baseline != -1 || nat_baseline != -1)
	{
	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      g_warning ("%s %p reported a horizontal baseline",
			 G_OBJECT_TYPE_NAME (widget), widget);
	      min_baseline = -1;
	      nat_baseline = -1;
	    }
	  else if (min_baseline == -1 || nat_baseline == -1)
	    {
	      g_warning ("%s %p reported baseline for only one of min/natural (min: %d, natural: %d)",
			 G_OBJECT_TYPE_NAME (widget), widget,
			 min_baseline, nat_baseline);
	      min_baseline = -1;
	      nat_baseline = -1;
	    }
          else if (min_baseline > reported_min_size ||
                   nat_baseline > reported_nat_size ||
                   min_baseline < 0 ||
                   nat_baseline < 0)
            {
              g_warning ("%s %p reported baselines of minimum %d and natural %d, but sizes of "
                         "minimum %d and natural %d. Baselines must be inside the widget size.",
                         G_OBJECT_TYPE_NAME (widget), widget, min_baseline, nat_baseline,
                         reported_min_size, reported_nat_size);
              min_baseline = -1;
              nat_baseline = -1;
            }
	  else
            {
              if (css_min_size > reported_min_size)
                {
                  min_baseline += (css_min_size - reported_min_size) / 2;
                  nat_baseline += (css_min_size - reported_min_size) / 2;
                }

              min_baseline += margin.top + border.top + padding.top;
              nat_baseline += margin.top + border.top + padding.top;

              gtk_widget_adjust_baseline_request (widget, &min_baseline, &nat_baseline);
            }
        }

      _gtk_size_request_cache_commit (cache,
                                      orientation,
                                      for_size,
                                      min_size,
                                      nat_size,
				      min_baseline,
				      nat_baseline);
    }

  if (minimum)
    *minimum = min_size;

  if (natural)
    *natural = nat_size;

  if (minimum_baseline)
    *minimum_baseline = min_baseline;

  if (natural_baseline)
    *natural_baseline = nat_baseline;

  g_assert (min_size <= nat_size);

  if (GTK_DISPLAY_DEBUG_CHECK (_gtk_widget_get_display (widget), SIZE_REQUEST))
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, "[%p] %s\t%s: %d is minimum %d and natural: %d",
                              widget, G_OBJECT_TYPE_NAME (widget),
                              orientation == GTK_ORIENTATION_HORIZONTAL
                              ? "width for height"
                              : "height for width",
                              for_size, min_size, nat_size);
      if (min_baseline != -1 || nat_baseline != -1)
        {
          g_string_append_printf (s, ", baseline %d/%d",
                                  min_baseline, nat_baseline);
        }
      g_string_append_printf (s, " (hit cache: %s)\n",
                              found_in_cache ? "yes" : "no");
      g_printerr ("%s", s->str);
      g_string_free (s, TRUE);
    }
}

/**
 * gtk_widget_measure:
 * @widget: A `GtkWidget` instance
 * @orientation: the orientation to measure
 * @for_size: Size for the opposite of @orientation, i.e.
 *   if @orientation is %GTK_ORIENTATION_HORIZONTAL, this is
 *   the height the widget should be measured with. The %GTK_ORIENTATION_VERTICAL
 *   case is analogous. This way, both height-for-width and width-for-height
 *   requests can be implemented. If no size is known, -1 can be passed.
 * @minimum: (out) (optional): location to store the minimum size
 * @natural: (out) (optional): location to store the natural size
 * @minimum_baseline: (out) (optional): location to store the baseline
 *   position for the minimum size, or -1 to report no baseline
 * @natural_baseline: (out) (optional): location to store the baseline
 *   position for the natural size, or -1 to report no baseline
 *
 * Measures @widget in the orientation @orientation and for the given @for_size.
 *
 * As an example, if @orientation is %GTK_ORIENTATION_HORIZONTAL and @for_size
 * is 300, this functions will compute the minimum and natural width of @widget
 * if it is allocated at a height of 300 pixels.
 *
 * See [GtkWidget’s geometry management section](class.Widget.html#height-for-width-geometry-management) for
 * a more details on implementing `GtkWidgetClass.measure()`.
 */
void
gtk_widget_measure (GtkWidget        *widget,
                    GtkOrientation    orientation,
                    int               for_size,
                    int              *minimum,
                    int              *natural,
                    int              *minimum_baseline,
                    int              *natural_baseline)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (for_size >= -1);
  g_return_if_fail (orientation == GTK_ORIENTATION_HORIZONTAL ||
                    orientation == GTK_ORIENTATION_VERTICAL);

  if (for_size >= 0)
    {
      int min_opposite_size;
      gtk_widget_measure (widget, OPPOSITE_ORIENTATION (orientation), -1, &min_opposite_size, NULL, NULL, NULL);
      if (for_size < min_opposite_size)
        for_size = min_opposite_size;
    }

  /* This is the main function that checks for a cached size and
   * possibly queries the widget class to compute the size if it's
   * not cached.
   */
  if (!_gtk_widget_get_visible (widget) && !GTK_IS_ROOT (widget))
    {
      if (minimum)
        *minimum = 0;
      if (natural)
        *natural = 0;
      if (minimum_baseline)
        *minimum_baseline = -1;
      if (natural_baseline)
        *natural_baseline = -1;
      return;
    }

  if (G_LIKELY (!_gtk_widget_get_sizegroups (widget)))
    {
      gtk_widget_query_size_for_orientation (widget, orientation, for_size, minimum, natural,
                                             minimum_baseline, natural_baseline);
    }
  else
    {
      GHashTable *widgets;
      GHashTableIter iter;
      gpointer key;
      int min_result = 0, nat_result = 0;

      widgets = _gtk_size_group_get_widget_peers (widget, orientation);

      g_hash_table_iter_init (&iter, widgets);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        {
          GtkWidget *tmp_widget = key;
          int min_dimension, nat_dimension;

          gtk_widget_query_size_for_orientation (tmp_widget, orientation, for_size,
                                                 &min_dimension, &nat_dimension, NULL, NULL);

          min_result = MAX (min_result, min_dimension);
          nat_result = MAX (nat_result, nat_dimension);
        }

      g_hash_table_destroy (widgets);

      /* Baselines make no sense with sizegroups really */
      if (minimum_baseline)
        *minimum_baseline = -1;

      if (natural_baseline)
        *natural_baseline = -1;

      if (minimum)
        *minimum = min_result;

      if (natural)
        *natural = nat_result;
    }
}

/**
 * gtk_widget_get_request_mode:
 * @widget: a `GtkWidget` instance
 *
 * Gets whether the widget prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * Single-child widgets generally propagate the preference of
 * their child, more complex widgets need to request something
 * either in context of their children or in context of their
 * allocation capabilities.
 *
 * Returns: The `GtkSizeRequestMode` preferred by @widget.
 */
GtkSizeRequestMode
gtk_widget_get_request_mode (GtkWidget *widget)
{
  SizeRequestCache *cache;

  cache = _gtk_widget_peek_request_cache (widget);

  if (G_UNLIKELY (!cache->request_mode_valid))
    {
      cache->request_mode = fetch_request_mode (widget);
      cache->request_mode_valid = TRUE;
    }

  return cache->request_mode;
}

/**
 * gtk_widget_get_preferred_size:
 * @widget: a `GtkWidget` instance
 * @minimum_size: (out) (optional): location for storing the minimum size
 * @natural_size: (out) (optional): location for storing the natural size
 *
 * Retrieves the minimum and natural size of a widget, taking
 * into account the widget’s preference for height-for-width management.
 *
 * This is used to retrieve a suitable size by container widgets which do
 * not impose any restrictions on the child placement. It can be used
 * to deduce toplevel window and menu sizes as well as child widgets in
 * free-form containers such as `GtkFixed`.
 *
 * Handle with care. Note that the natural height of a height-for-width
 * widget will generally be a smaller size than the minimum height, since
 * the required height for the natural width is generally smaller than the
 * required height for the minimum width.
 *
 * Use [method@Gtk.Widget.measure] if you want to support baseline alignment.
 */
void
gtk_widget_get_preferred_size (GtkWidget      *widget,
                               GtkRequisition *minimum_size,
                               GtkRequisition *natural_size)
{
  int min_width, nat_width;
  int min_height, nat_height;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          &min_width, &nat_width, NULL, NULL);

      if (minimum_size)
        {
          minimum_size->width = min_width;
          gtk_widget_measure (widget,
                              GTK_ORIENTATION_VERTICAL, min_width,
                              &minimum_size->height, NULL, NULL, NULL);
        }

      if (natural_size)
        {
          natural_size->width = nat_width;
          gtk_widget_measure (widget,
                              GTK_ORIENTATION_VERTICAL, nat_width,
                              NULL, &natural_size->height, NULL, NULL);
        }
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT or CONSTANT_SIZE */
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          -1, &min_height, &nat_height, NULL, NULL);

      if (minimum_size)
        {
          minimum_size->height = min_height;
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, min_height,
                              &minimum_size->width, NULL, NULL, NULL);
        }

      if (natural_size)
        {
          natural_size->height = nat_height;
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, nat_height,
                              NULL, &natural_size->width, NULL, NULL);
        }
    }
}

static int
compare_gap (gconstpointer p1,
	     gconstpointer p2,
	     gpointer      data)
{
  GtkRequestedSize *sizes = data;
  const guint *c1 = p1;
  const guint *c2 = p2;

  const int d1 = MAX (sizes[*c1].natural_size -
                       sizes[*c1].minimum_size,
                       0);
  const int d2 = MAX (sizes[*c2].natural_size -
                       sizes[*c2].minimum_size,
                       0);

  int delta = (d2 - d1);

  if (0 == delta)
    delta = (*c2 - *c1);

  return delta;
}

/**
 * gtk_distribute_natural_allocation:
 * @extra_space: Extra space to redistribute among children after subtracting
 *   minimum sizes and any child padding from the overall allocation
 * @n_requested_sizes: Number of requests to fit into the allocation
 * @sizes: (array length=n_requested_sizes): An array of structs with a client pointer and a minimum/natural size
 *  in the orientation of the allocation.
 *
 * Distributes @extra_space to child @sizes by bringing smaller
 * children up to natural size first.
 *
 * The remaining space will be added to the @minimum_size member of the
 * `GtkRequestedSize` struct. If all sizes reach their natural size then
 * the remaining space is returned.
 *
 * Returns: The remainder of @extra_space after redistributing space
 * to @sizes.
 */
int
gtk_distribute_natural_allocation (int               extra_space,
				   guint             n_requested_sizes,
				   GtkRequestedSize *sizes)
{
  guint *spreading;
  int    i;

  g_return_val_if_fail (extra_space >= 0, 0);

  if (n_requested_sizes == 0)
    return extra_space;

  spreading = g_newa (guint, n_requested_sizes);

  for (i = 0; i < n_requested_sizes; i++)
    spreading[i] = i;

  /* Distribute the container's extra space c_gap. We want to assign
   * this space such that the sum of extra space assigned to children
   * (c^i_gap) is equal to c_cap. The case that there's not enough
   * space for all children to take their natural size needs some
   * attention. The goals we want to achieve are:
   *
   *   a) Maximize number of children taking their natural size.
   *   b) The allocated size of children should be a continuous
   *   function of c_gap.  That is, increasing the container size by
   *   one pixel should never make drastic changes in the distribution.
   *   c) If child i takes its natural size and child j doesn't,
   *   child j should have received at least as much gap as child i.
   *
   * The following code distributes the additional space by following
   * these rules.
   */

  /* Sort descending by gap and position. */
  g_qsort_with_data (spreading,
		     n_requested_sizes, sizeof (guint),
		     compare_gap, sizes);

  /* Distribute available space.
   * This masterpiece of a loop was conceived by Behdad Esfahbod.
   */
  for (i = n_requested_sizes - 1; extra_space > 0 && i >= 0; --i)
    {
      /* Divide remaining space by number of remaining children.
       * Sort order and reducing remaining space by assigned space
       * ensures that space is distributed equally.
       */
      int glue = (extra_space + i) / (i + 1);
      int gap = sizes[(spreading[i])].natural_size
	- sizes[(spreading[i])].minimum_size;

      int extra = MIN (glue, gap);

      sizes[spreading[i]].minimum_size += extra;

      extra_space -= extra;
    }

  return extra_space;
}
