/* gtkgridlayout.c: Layout manager for grid-like widgets
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 */

/**
 * GtkGridLayout:
 *
 * `GtkGridLayout` is a layout manager which arranges child widgets in
 * rows and columns.
 *
 * Children have an "attach point" defined by the horizontal and vertical
 * index of the cell they occupy; children can span multiple rows or columns.
 * The layout properties for setting the attach points and spans are set
 * using the [class@Gtk.GridLayoutChild] associated to each child widget.
 *
 * The behaviour of `GtkGridLayout` when several children occupy the same
 * grid cell is undefined.
 *
 * `GtkGridLayout` can be used like a `GtkBoxLayout` if all children are
 * attached to the same row or column; however, if you only ever need a
 * single row or column, you should consider using `GtkBoxLayout`.
 */

/**
 * GtkGridLayoutChild:
 *
 * `GtkLayoutChild` subclass for children in a `GtkGridLayout`.
 */
#include "config.h"

#include "gtkgridlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkdebug.h"
#include "gtklayoutchild.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"

/* {{{ GtkGridLayoutChild */
typedef struct {
  int pos;
  int span;
} GridChildAttach;

struct _GtkGridLayoutChild
{
  GtkLayoutChild parent_instance;

  GridChildAttach attach[2];
};

#define CHILD_COLUMN(child)     ((child)->attach[GTK_ORIENTATION_HORIZONTAL].pos)
#define CHILD_COL_SPAN(child)   ((child)->attach[GTK_ORIENTATION_HORIZONTAL].span)
#define CHILD_ROW(child)        ((child)->attach[GTK_ORIENTATION_VERTICAL].pos)
#define CHILD_ROW_SPAN(child)   ((child)->attach[GTK_ORIENTATION_VERTICAL].span)

enum {
  PROP_CHILD_COLUMN = 1,
  PROP_CHILD_ROW,
  PROP_CHILD_COLUMN_SPAN,
  PROP_CHILD_ROW_SPAN,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkGridLayoutChild, gtk_grid_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_grid_layout_child_set_property (GObject      *gobject,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_COLUMN:
      gtk_grid_layout_child_set_column (self, g_value_get_int (value));
      break;

    case PROP_CHILD_ROW :
      gtk_grid_layout_child_set_row (self, g_value_get_int (value));
      break;

    case PROP_CHILD_COLUMN_SPAN:
      gtk_grid_layout_child_set_column_span (self, g_value_get_int (value));
      break;

    case PROP_CHILD_ROW_SPAN:
      gtk_grid_layout_child_set_row_span (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_child_get_property (GObject    *gobject,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_COLUMN:
      g_value_set_int (value, CHILD_COLUMN (self));
      break;

    case PROP_CHILD_ROW:
      g_value_set_int (value, CHILD_ROW (self));
      break;

    case PROP_CHILD_COLUMN_SPAN:
      g_value_set_int (value, CHILD_COL_SPAN (self));
      break;

    case PROP_CHILD_ROW_SPAN:
      g_value_set_int (value, CHILD_ROW_SPAN (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_child_class_init (GtkGridLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_grid_layout_child_set_property;
  gobject_class->get_property = gtk_grid_layout_child_get_property;

  /**
   * GtkGridLayoutChild:column: (attributes org.gtk.Property.get=gtk_grid_layout_child_get_column org.gtk.Property.set=gtk_grid_layout_child_set_column)
   *
   * The column to place the child in.
   */
  child_props[PROP_CHILD_COLUMN] =
    g_param_spec_int ("column", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:row: (attributes org.gtk.Property.get=gtk_grid_layout_child_get_row org.gtk.Property.set=gtk_grid_layout_child_set_row)
   *
   * The row to place the child in.
   */
  child_props[PROP_CHILD_ROW] =
    g_param_spec_int ("row", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:column-span: (attributes org.gtk.Property.get=gtk_grid_layout_child_get_column_span org.gtk.Property.set=gtk_grid_layout_child_set_column_span)
   *
   * The number of columns the child spans to.
   */
  child_props[PROP_CHILD_COLUMN_SPAN] =
    g_param_spec_int ("column-span", NULL, NULL,
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:row-span: (attributes org.gtk.Property.get=gtk_grid_layout_child_get_row_span org.gtk.Property.set=gtk_grid_layout_child_set_row_span)
   *
   * The number of rows the child spans to.
   */
  child_props[PROP_CHILD_ROW_SPAN] =
    g_param_spec_int ("row-span", NULL, NULL,
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_grid_layout_child_init (GtkGridLayoutChild *self)
{
  CHILD_ROW_SPAN (self) = 1;
  CHILD_COL_SPAN (self) = 1;
}

/**
 * gtk_grid_layout_child_set_row: (attributes org.gtk.Method.set_property=row)
 * @child: a `GtkGridLayoutChild`
 * @row: the row for @child
 *
 * Sets the row to place @child in.
 */
void
gtk_grid_layout_child_set_row (GtkGridLayoutChild *child,
                               int                 row)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_ROW (child) == row)
    return;

  CHILD_ROW (child) = row;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_ROW]);
}

/**
 * gtk_grid_layout_child_get_row: (attributes org.gtk.Method.get_property=row)
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the row number to which @child attaches its top side.
 *
 * Returns: the row number
 */
int
gtk_grid_layout_child_get_row (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 0);

  return CHILD_ROW (child);
}

/**
 * gtk_grid_layout_child_set_column: (attributes org.gtk.Method.set_property=column)
 * @child: a `GtkGridLayoutChild`
 * @column: the attach point for @child
 *
 * Sets the column number to attach the left side of @child.
 */
void
gtk_grid_layout_child_set_column (GtkGridLayoutChild *child,
                                  int                 column)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_COLUMN (child) == column)
    return;

  CHILD_COLUMN (child) = column;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_COLUMN]);
}

/**
 * gtk_grid_layout_child_get_column: (attributes org.gtk.Method.get_property=column)
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the column number to which @child attaches its left side.
 *
 * Returns: the column number
 */
int
gtk_grid_layout_child_get_column (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 0);

  return CHILD_COLUMN (child);
}

/**
 * gtk_grid_layout_child_set_column_span: (attributes org.gtk.Method.set_property=column-span)
 * @child: a `GtkGridLayoutChild`
 * @span: the span of @child
 *
 * Sets the number of columns @child spans to.
 */
void
gtk_grid_layout_child_set_column_span (GtkGridLayoutChild *child,
                                       int                 span)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_COL_SPAN (child) == span)
    return;

  CHILD_COL_SPAN (child) = span;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_COLUMN_SPAN]);
}

/**
 * gtk_grid_layout_child_get_column_span: (attributes org.gtk.Method.get_property=column-span)
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the number of columns that @child spans to.
 *
 * Returns: the number of columns
 */
int
gtk_grid_layout_child_get_column_span (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 1);

  return CHILD_COL_SPAN (child);
}

/**
 * gtk_grid_layout_child_set_row_span: (attributes org.gtk.Method.set_property=row-span)
 * @child: a `GtkGridLayoutChild`
 * @span: the span of @child
 *
 * Sets the number of rows @child spans to.
 */
void
gtk_grid_layout_child_set_row_span (GtkGridLayoutChild *child,
                                    int                 span)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_ROW_SPAN (child) == span)
    return;

  CHILD_ROW_SPAN (child) = span;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_ROW_SPAN]);
}

/**
 * gtk_grid_layout_child_get_row_span: (attributes org.gtk.Method.get_property=row-span)
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the number of rows that @child spans to.
 *
 * Returns: the number of row
 */
int
gtk_grid_layout_child_get_row_span (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 1);

  return CHILD_ROW_SPAN (child);
}

/* }}} */

/* {{{ GtkGridLayout */

typedef struct {
  int row;
  GtkBaselinePosition baseline_position;
} GridRowProperties;

static const GridRowProperties grid_row_properties_default = {
  0,
  GTK_BASELINE_POSITION_CENTER
};

/* A GridLineData struct contains row/column specific parts
 * of the grid.
 */
typedef struct {
  gint16 spacing;
  guint homogeneous : 1;
} GridLineData;

#define ROWS(layout)    (&(layout)->linedata[GTK_ORIENTATION_HORIZONTAL])
#define COLUMNS(layout) (&(layout)->linedata[GTK_ORIENTATION_VERTICAL])

/* A GridLine struct represents a single row or column
 * during size requests
 */
typedef struct {
  int minimum;
  int natural;
  int minimum_above;
  int minimum_below;
  int natural_above;
  int natural_below;

  int position;
  int allocation;
  int allocated_baseline;

  guint need_expand : 1;
  guint expand      : 1;
  guint empty       : 1;
} GridLine;

typedef struct {
  GridLine *lines;
  int min, max;
} GridLines;

typedef struct {
  GtkGridLayout *layout;
  GtkWidget *widget;

  GridLines lines[2];
} GridRequest;

struct _GtkGridLayout
{
  GtkLayoutManager parent_instance;

  /* Array<GridRowProperties> */
  GArray *row_properties;

  GtkOrientation orientation;
  int baseline_row;

  GridLineData linedata[2];
};

enum {
  PROP_ROW_SPACING = 1,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,

  N_PROPERTIES
};

static GParamSpec *layout_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkGridLayout, gtk_grid_layout, GTK_TYPE_LAYOUT_MANAGER)

static inline GtkGridLayoutChild *
get_grid_child (GtkGridLayout *self,
                GtkWidget     *child)
{
  GtkLayoutManager *manager = GTK_LAYOUT_MANAGER (self);

  return (GtkGridLayoutChild *) gtk_layout_manager_get_layout_child (manager, child);
}

static int
get_spacing (GtkGridLayout  *self,
             GtkWidget      *widget,
             GtkOrientation  orientation)
{
  GtkCssNode *node = gtk_widget_get_css_node (widget);
  GtkCssStyle *style = gtk_css_node_get_style (node);
  GtkCssValue *border_spacing;
  int css_spacing;

  border_spacing = style->size->border_spacing;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + self->linedata[orientation].spacing;
}

/* Calculates the min and max numbers for both orientations. */
static void
grid_request_count_lines (GridRequest *request)
{
  GtkWidget *child;
  int min[2];
  int max[2];

  min[0] = min[1] = G_MAXINT;
  max[0] = max[1] = G_MININT;

  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);
      GridChildAttach *attach = grid_child->attach;

      min[0] = MIN (min[0], attach[0].pos);
      max[0] = MAX (max[0], attach[0].pos + attach[0].span);
      min[1] = MIN (min[1], attach[1].pos);
      max[1] = MAX (max[1], attach[1].pos + attach[1].span);
    }

  request->lines[0].min = min[0];
  request->lines[0].max = max[0];
  request->lines[1].min = min[1];
  request->lines[1].max = max[1];
}

/* Sets line sizes to 0 and marks lines as expand
 * if they have a non-spanning expanding child.
 */
static void
grid_request_init (GridRequest    *request,
                   GtkOrientation  orientation)
{
  GtkWidget *child;
  GridLines *lines;
  int i;

  lines = &request->lines[orientation];

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = 0;
      lines->lines[i].natural = 0;
      lines->lines[i].minimum_above = -1;
      lines->lines[i].minimum_below = -1;
      lines->lines[i].natural_above = -1;
      lines->lines[i].natural_below = -1;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }


  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);
      GridChildAttach *attach;

      attach = &grid_child->attach[orientation];
      if (attach->span == 1 && gtk_widget_compute_expand (child, orientation))
        lines->lines[attach->pos - lines->min].expand = TRUE;
    }
}

/* Sums allocations for lines spanned by child and their spacing.
 */
static int
compute_allocation_for_child (GridRequest        *request,
                              GtkGridLayoutChild *child,
                              GtkOrientation      orientation)
{
  GridLines *lines;
  GridLine *line;
  GridChildAttach *attach;
  int size;
  int i;

  lines = &request->lines[orientation];
  attach = &child->attach[orientation];

  size = (attach->span - 1) * get_spacing (request->layout, request->widget, orientation);
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      size += line->allocation;
    }

  return size;
}

static void
compute_request_for_child (GridRequest        *request,
                           GtkWidget          *child,
                           GtkGridLayoutChild *grid_child,
                           GtkOrientation      orientation,
                           gboolean            contextual,
                           int                *minimum,
                           int                *natural,
                           int                *minimum_baseline,
                           int                *natural_baseline)
{
  if (minimum_baseline != NULL)
    *minimum_baseline = -1;
  if (natural_baseline != NULL)
    *natural_baseline = -1;

  if (contextual)
    {
      int size;

      size = compute_allocation_for_child (request, grid_child, 1 - orientation);

      gtk_widget_measure (child,
                          orientation,
                          size,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
  else
    {
      gtk_widget_measure (child,
                          orientation,
                          -1,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
}

/* Sets requisition to max. of non-spanning children.
 * If contextual is TRUE, requires allocations of
 * lines in the opposite orientation to be set.
 */
static void
grid_request_non_spanning (GridRequest    *request,
                           GtkOrientation  orientation,
                             gboolean        contextual)
{
  GtkWidget *child;
  GridLines *lines;
  GridLine *line;
  int i;
  GtkBaselinePosition baseline_pos;
  int minimum, minimum_baseline;
  int natural, natural_baseline;

  lines = &request->lines[orientation];

  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);
      GridChildAttach *attach;

      if (!gtk_widget_should_layout (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      compute_request_for_child (request, child, grid_child, orientation, contextual, &minimum, &natural, &minimum_baseline, &natural_baseline);

      line = &lines->lines[attach->pos - lines->min];

      if (minimum_baseline != -1)
        {
          line->minimum_above = MAX (line->minimum_above, minimum_baseline);
          line->minimum_below = MAX (line->minimum_below, minimum - minimum_baseline);
          line->natural_above = MAX (line->natural_above, natural_baseline);
          line->natural_below = MAX (line->natural_below, natural - natural_baseline);
        }
      else
        {
          line->minimum = MAX (line->minimum, minimum);
          line->natural = MAX (line->natural, natural);
        }
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->minimum_above != -1)
        {
          line->minimum = MAX (line->minimum, line->minimum_above + line->minimum_below);
          line->natural = MAX (line->natural, line->natural_above + line->natural_below);

          baseline_pos = gtk_grid_layout_get_row_baseline_position (request->layout, i + lines->min);

          switch (baseline_pos)
            {
            case GTK_BASELINE_POSITION_TOP:
              line->minimum_above += 0;
              line->minimum_below += line->minimum - (line->minimum_above + line->minimum_below);
              line->natural_above += 0;
              line->natural_below += line->natural - (line->natural_above + line->natural_below);
              break;

            case GTK_BASELINE_POSITION_CENTER:
              line->minimum_above += (line->minimum - (line->minimum_above + line->minimum_below))/2;
              line->minimum_below += (line->minimum - (line->minimum_above + line->minimum_below))/2;
              line->natural_above += (line->natural - (line->natural_above + line->natural_below))/2;
              line->natural_below += (line->natural - (line->natural_above + line->natural_below))/2;
              break;

            case GTK_BASELINE_POSITION_BOTTOM:
              line->minimum_above += line->minimum - (line->minimum_above + line->minimum_below);
              line->minimum_below += 0;
              line->natural_above += line->natural - (line->natural_above + line->natural_below);
              line->natural_below += 0;
              break;

            default:
              break;
            }
        }
    }
}

/* Enforce homogeneous sizes */
static void
grid_request_homogeneous (GridRequest    *request,
                          GtkOrientation  orientation)
{
  GtkGridLayout *self = request->layout;
  GridLineData *linedata;
  GridLines *lines;
  int minimum, natural;
  int i;

  linedata = &self->linedata[orientation];
  lines = &request->lines[orientation];

  if (!linedata->homogeneous)
    return;

  minimum = 0;
  natural = 0;

  for (i = 0; i < lines->max - lines->min; i++)
    {
      minimum = MAX (minimum, lines->lines[i].minimum);
      natural = MAX (natural, lines->lines[i].natural);
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      lines->lines[i].minimum = minimum;
      lines->lines[i].natural = natural;

      /* TODO: Do we want to adjust the baseline here too?
       * And if so, also in the homogeneous resize.
       */
    }
}

/* Deals with spanning children.
 * Requires expand fields of lines to be set for
 * non-spanning children.
 */
static void
grid_request_spanning (GridRequest    *request,
                       GtkOrientation  orientation,
                       gboolean        contextual)
{
  GtkGridLayout *self = request->layout;
  GtkWidget *child;
  GridChildAttach *attach;
  GridLineData *linedata;
  GridLines *lines;
  GridLine *line;
  int minimum, natural;
  int span_minimum, span_natural;
  int span_expand;
  gboolean force_expand;
  int spacing;
  int extra;
  int expand;
  int line_extra;
  int i;

  linedata = &self->linedata[orientation];
  lines = &request->lines[orientation];
  spacing = get_spacing (request->layout, request->widget, orientation);

  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);

      if (!gtk_widget_should_layout (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      /* We ignore baselines for spanning children */
      compute_request_for_child (request, child, grid_child, orientation, contextual, &minimum, &natural, NULL, NULL);

      span_minimum = (attach->span - 1) * spacing;
      span_natural = (attach->span - 1) * spacing;
      span_expand = 0;
      force_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];
          span_minimum += line->minimum;
          span_natural += line->natural;
          if (line->expand)
            span_expand += 1;
        }
      if (span_expand == 0)
        {
          span_expand = attach->span;
          force_expand = TRUE;
        }

      /* If we need to request more space for this child to fill
       * its requisition, then divide up the needed space amongst the
       * lines it spans, favoring expandable lines if any.
       *
       * When doing homogeneous allocation though, try to keep the
       * line allocations even, since we're going to force them to
       * be the same anyway, and we don't want to introduce unnecessary
       * extra space.
       */
      if (span_minimum < minimum)
        {
          if (linedata->homogeneous)
            {
              int total, m;

              total = minimum - (attach->span - 1) * spacing;
              m = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->minimum = MAX (line->minimum, m);
                }
            }
          else
            {
              extra = minimum - span_minimum;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->minimum += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }

      if (span_natural < natural)
        {
          if (linedata->homogeneous)
            {
              int total, n;

              total = natural - (attach->span - 1) * spacing;
              n = total / attach->span + (total % attach->span ? 1 : 0);
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  line->natural = MAX (line->natural, n);
                }
            }
          else
            {
              extra = natural - span_natural;
              expand = span_expand;
              for (i = 0; i < attach->span; i++)
                {
                  line = &lines->lines[attach->pos - lines->min + i];
                  if (force_expand || line->expand)
                    {
                      line_extra = extra / expand;
                      line->natural += line_extra;
                      extra -= line_extra;
                      expand -= 1;
                    }
                }
            }
        }
    }
}

/* Marks empty and expanding lines and counts them */
static void
grid_request_compute_expand (GridRequest    *request,
                             GtkOrientation  orientation,
                             int             min,
                             int             max,
                             int            *nonempty_lines,
                             int            *expand_lines)
{
  GtkWidget *child;
  GridChildAttach *attach;
  int i;
  GridLines *lines;
  GridLine *line;
  gboolean has_expand;
  int expand;
  int empty;

  lines = &request->lines[orientation];

  min = MAX (min, lines->min);
  max = MIN (max, lines->max);

  for (i = min - lines->min; i < max - lines->min; i++)
    {
      lines->lines[i].need_expand = FALSE;
      lines->lines[i].expand = FALSE;
      lines->lines[i].empty = TRUE;
    }

  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);

      if (!gtk_widget_should_layout (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span != 1)
        continue;

      if (attach->pos >= max || attach->pos < min)
        continue;

      line = &lines->lines[attach->pos - lines->min];
      line->empty = FALSE;
      if (gtk_widget_compute_expand (child, orientation))
        line->expand = TRUE;
    }

  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);

      if (!gtk_widget_should_layout (child))
        continue;

      attach = &grid_child->attach[orientation];
      if (attach->span == 1)
        continue;

      has_expand = FALSE;
      for (i = 0; i < attach->span; i++)
        {
          line = &lines->lines[attach->pos - lines->min + i];

          if (line->expand)
            has_expand = TRUE;

          if (attach->pos + i >= max || attach->pos + 1 < min)
            continue;

          line->empty = FALSE;
        }

      if (!has_expand && gtk_widget_compute_expand (child, orientation))
        {
          for (i = 0; i < attach->span; i++)
            {
              if (attach->pos + i >= max || attach->pos + 1 < min)
                continue;

              line = &lines->lines[attach->pos - lines->min + i];
              line->need_expand = TRUE;
            }
        }
    }

  empty = 0;
  expand = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->need_expand)
        line->expand = TRUE;

      if (line->empty)
        empty += 1;

      if (line->expand)
        expand += 1;
    }

  if (nonempty_lines)
    *nonempty_lines = max - min - empty;

  if (expand_lines)
    *expand_lines = expand;
}

/* Sums the minimum and natural fields of lines and their spacing */
static void
grid_request_sum (GridRequest    *request,
                  GtkOrientation  orientation,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkGridLayout *self = request->layout;
  GridLines *lines;
  int i;
  int min, nat;
  int nonempty;
  int spacing;

  grid_request_compute_expand (request, orientation, G_MININT, G_MAXINT, &nonempty, NULL);

  lines = &request->lines[orientation];
  spacing = get_spacing (request->layout, request->widget, orientation);

  min = 0;
  nat = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      if (orientation == GTK_ORIENTATION_VERTICAL &&
          lines->min + i == self->baseline_row &&
          lines->lines[i].minimum_above != -1)
        {
          if (minimum_baseline)
            *minimum_baseline = min + lines->lines[i].minimum_above;
          if (natural_baseline)
            *natural_baseline = nat + lines->lines[i].natural_above;
        }

      min += lines->lines[i].minimum;
      nat += lines->lines[i].natural;

      if (!lines->lines[i].empty)
        {
          min += spacing;
          nat += spacing;
        }
    }

  /* Remove last spacing, if any was applied */
  if (nonempty > 0)
    {
      min -= spacing;
      nat -= spacing;
    }

  *minimum = min;
  *natural = nat;
}

/* Computes minimum and natural fields of lines.
 * When contextual is TRUE, requires allocation of
 * lines in the opposite orientation to be set.
 */
static void
grid_request_run (GridRequest    *request,
                  GtkOrientation  orientation,
                  gboolean        contextual)
{
  grid_request_init (request, orientation);
  grid_request_non_spanning (request, orientation, contextual);
  grid_request_homogeneous (request, orientation);
  grid_request_spanning (request, orientation, contextual);
  grid_request_homogeneous (request, orientation);
}

static void
grid_distribute_non_homogeneous (GridLines *lines,
                                 int        nonempty,
                                 int        expand,
                                 int        size,
                                 int        min,
                                 int        max)
{
  GtkRequestedSize *sizes;
  GridLine *line;
  int extra;
  int rest;
  int i, j;

  if (nonempty == 0)
    return;

  sizes = g_newa (GtkRequestedSize, nonempty);

  j = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (line->empty)
        continue;

      size -= line->minimum;

      sizes[j].minimum_size = line->minimum;
      sizes[j].natural_size = line->natural;
      sizes[j].data = line;
      j++;
    }

  size = gtk_distribute_natural_allocation (MAX (0, size), nonempty, sizes);

  if (expand > 0)
    {
      extra = size / expand;
      rest = size % expand;
    }
  else
    {
      extra = 0;
      rest = 0;
    }

  j = 0;
  for (i = min - lines->min; i < max - lines->min; i++)
    {
      line = &lines->lines[i];
      if (line->empty)
        continue;

      g_assert (line == sizes[j].data);

      line->allocation = sizes[j].minimum_size;
      if (line->expand)
        {
          line->allocation += extra;
          if (rest > 0)
            {
              line->allocation += 1;
              rest -= 1;
            }
        }

      j++;
    }
}

/* Requires that the minimum and natural fields of lines
 * have been set, computes the allocation field of lines
 * by distributing total_size among lines.
 */
static void
grid_request_allocate (GridRequest    *request,
                       GtkOrientation  orientation,
                       int             total_size)
{
  GtkGridLayout *self = request->layout;
  GridLineData *linedata;
  GridLines *lines;
  GridLine *line;
  int nonempty1, nonempty2;
  int expand1, expand2;
  int i;
  GtkBaselinePosition baseline_pos;
  int baseline;
  int extra, extra2;
  int rest;
  int size1, size2;
  int split, split_pos;
  int spacing;

  linedata = &self->linedata[orientation];
  lines = &request->lines[orientation];
  spacing = get_spacing (request->layout, request->widget, orientation);

  baseline = gtk_widget_get_baseline (request->widget);

  if (orientation == GTK_ORIENTATION_VERTICAL && baseline != -1 &&
      self->baseline_row >= lines->min && self->baseline_row < lines->max &&
      lines->lines[self->baseline_row - lines->min].minimum_above != -1)
    {
      split = self->baseline_row;
      split_pos = baseline - lines->lines[self->baseline_row - lines->min].minimum_above;
      grid_request_compute_expand (request, orientation, lines->min, split, &nonempty1, &expand1);
      grid_request_compute_expand (request, orientation, split, lines->max, &nonempty2, &expand2);

      if (nonempty2 > 0)
        {
          size1 = split_pos - (nonempty1) * spacing;
          size2 = (total_size - split_pos) - (nonempty2 - 1) * spacing;
        }
      else
        {
          size1 = total_size - (nonempty1 - 1) * spacing;
          size2 = 0;
        }
    }
  else
    {
      grid_request_compute_expand (request, orientation, lines->min, lines->max, &nonempty1, &expand1);
      nonempty2 = expand2 = 0;
      split = lines->max;

      size1 = total_size - (nonempty1 - 1) * spacing;
      size2 = 0;
    }

  if (nonempty1 == 0 && nonempty2 == 0)
    return;

  if (linedata->homogeneous)
    {
      if (nonempty1 > 0)
        {
          extra = size1 / nonempty1;
          rest = size1 % nonempty1;
        }
      else
        {
          extra = 0;
          rest = 0;
        }
      if (nonempty2 > 0)
        {
          extra2 = size2 / nonempty2;
          if (extra2 < extra || nonempty1 == 0)
            {
              extra = extra2;
              rest = size2 % nonempty2;
            }
        }

      for (i = 0; i < lines->max - lines->min; i++)
        {
          line = &lines->lines[i];
          if (line->empty)
            continue;

          line->allocation = extra;
          if (rest > 0)
            {
              line->allocation += 1;
              rest -= 1;
            }
        }
    }
  else
    {
      grid_distribute_non_homogeneous (lines,
                                       nonempty1,
                                       expand1,
                                       size1,
                                       lines->min,
                                       split);
      grid_distribute_non_homogeneous (lines,
                                       nonempty2,
                                       expand2,
                                       size2,
                                       split,
                                       lines->max);
    }

  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (line->empty)
        continue;

      if (line->minimum_above != -1)
        {
          /* Note: This is overridden in grid_request_position for the allocated baseline */
          baseline_pos = gtk_grid_layout_get_row_baseline_position (request->layout, i + lines->min);

          switch (baseline_pos)
            {
            case GTK_BASELINE_POSITION_TOP:
              line->allocated_baseline = line->minimum_above;
              break;
            case GTK_BASELINE_POSITION_CENTER:
              line->allocated_baseline = line->minimum_above +
                                         (line->allocation - (line->minimum_above + line->minimum_below)) / 2;
              break;
            case GTK_BASELINE_POSITION_BOTTOM:
              line->allocated_baseline = line->allocation - line->minimum_below;
              break;
            default:
              break;
            }
        }
      else
        line->allocated_baseline = -1;
    }
}

/* Computes the position fields from allocation and spacing */
static void
grid_request_position (GridRequest    *request,
                       GtkOrientation  orientation)
{
  GtkGridLayout *self = request->layout;
  GridLines *lines;
  GridLine *line;
  int position, old_position;
  int allocated_baseline;
  int spacing;
  int i, j;

  lines = &request->lines[orientation];
  spacing = get_spacing (request->layout, request->widget, orientation);

  allocated_baseline = gtk_widget_get_baseline (request->widget);

  position = 0;
  for (i = 0; i < lines->max - lines->min; i++)
    {
      line = &lines->lines[i];

      if (orientation == GTK_ORIENTATION_VERTICAL &&
          i + lines->min == self->baseline_row &&
          allocated_baseline != -1 &&
          lines->lines[i].minimum_above != -1)
        {
          old_position = position;
          position = allocated_baseline - line->minimum_above;

          /* Back-patch previous rows */
          for (j = 0; j < i; j++)
            {
              if (!lines->lines[j].empty)
                lines->lines[j].position += position - old_position;
            }
        }

      if (!line->empty)
        {
          line->position = position;
          position += line->allocation + spacing;

          if (orientation == GTK_ORIENTATION_VERTICAL &&
              i + lines->min == self->baseline_row &&
              allocated_baseline != -1 &&
              lines->lines[i].minimum_above != -1)
            line->allocated_baseline = allocated_baseline - line->position;
        }
    }
}

static void
gtk_grid_layout_get_size (GtkGridLayout  *self,
                          GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GridRequest request;
  GridLines *lines;

  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  if (_gtk_widget_get_first_child (widget) == NULL)
    return;

  request.layout = self;
  request.widget = widget;
  grid_request_count_lines (&request);

  lines = &request.lines[orientation];
  lines->lines = g_newa (GridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GridLine));

  grid_request_run (&request, orientation, FALSE);
  grid_request_sum (&request, orientation,
                    minimum, natural,
                    minimum_baseline, natural_baseline);
}

static void
gtk_grid_layout_get_size_for_size (GtkGridLayout  *self,
                                   GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline)
{
  GridRequest request;
  GridLines *lines;
  int min_size, nat_size;

  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  if (_gtk_widget_get_first_child (widget) == NULL)
    return;

  request.layout = self;
  request.widget = widget;
  grid_request_count_lines (&request);

  lines = &request.lines[0];
  lines->lines = g_newa (GridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (GridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GridLine));

  grid_request_run (&request, 1 - orientation, FALSE);
  grid_request_sum (&request, 1 - orientation, &min_size, &nat_size, NULL, NULL);
  grid_request_allocate (&request, 1 - orientation, MAX (size, min_size));

  grid_request_run (&request, orientation, TRUE);
  grid_request_sum (&request, orientation,
                    minimum, natural,
                    minimum_baseline, natural_baseline);
}

static void
gtk_grid_layout_measure (GtkLayoutManager *manager,
                         GtkWidget        *widget,
                         GtkOrientation    orientation,
                         int               for_size,
                         int              *minimum,
                         int              *natural,
                         int              *minimum_baseline,
                         int              *natural_baseline)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (manager);

  if ((orientation == GTK_ORIENTATION_HORIZONTAL &&
       gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT) ||
      (orientation == GTK_ORIENTATION_VERTICAL &&
       gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH))
    gtk_grid_layout_get_size_for_size (self, widget, orientation, for_size,
                                       minimum, natural,
                                       minimum_baseline, natural_baseline);
  else
    gtk_grid_layout_get_size (self, widget, orientation,
                              minimum, natural,
                              minimum_baseline, natural_baseline);
}

static void
allocate_child (GridRequest        *request,
                GtkOrientation      orientation,
                GtkWidget          *child,
                GtkGridLayoutChild *grid_child,
                int                *position,
                int                *size,
                int                *baseline)
{
  GridLines *lines;
  GridLine *line;
  GridChildAttach *attach;
  int i;

  lines = &request->lines[orientation];
  attach = &grid_child->attach[orientation];

  *position = lines->lines[attach->pos - lines->min].position;
  if (attach->span == 1 &&
      (gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_CENTER ||
       gtk_widget_get_valign (child) == GTK_ALIGN_BASELINE_FILL))
    *baseline = lines->lines[attach->pos - lines->min].allocated_baseline;
  else
    *baseline = -1;

  *size = (attach->span - 1) * get_spacing (request->layout, request->widget, orientation);
  for (i = 0; i < attach->span; i++)
    {
      line = &lines->lines[attach->pos - lines->min + i];
      *size += line->allocation;
    }
}

static void
grid_request_allocate_children (GridRequest *request,
                                int          grid_width,
                                int          grid_height)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  int x, y, width, height, baseline, ignore;


  for (child = _gtk_widget_get_first_child (request->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child = get_grid_child (request->layout, child);

      if (!gtk_widget_should_layout (child))
        continue;

      allocate_child (request, GTK_ORIENTATION_HORIZONTAL, child, grid_child, &x, &width, &ignore);
      allocate_child (request, GTK_ORIENTATION_VERTICAL, child, grid_child, &y, &height, &baseline);

      child_allocation.x = x;
      child_allocation.y = y;
      child_allocation.width = width;
      child_allocation.height = height;

      if (_gtk_widget_get_direction (request->widget) == GTK_TEXT_DIR_RTL)
        child_allocation.x = grid_width - child_allocation.x - child_allocation.width;

      gtk_widget_size_allocate (child, &child_allocation, baseline);
    }
}

#define GET_SIZE(width, height, orientation) (orientation == GTK_ORIENTATION_HORIZONTAL ? width : height)

static void
gtk_grid_layout_allocate (GtkLayoutManager *manager,
                          GtkWidget        *widget,
                          int               width,
                          int               height,
                          int              baseline)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (manager);
  GridRequest request;
  GridLines *lines;
  GtkOrientation orientation;

  if (_gtk_widget_get_first_child (widget) == NULL)
    return;

  request.layout = self;
  request.widget = widget;

  grid_request_count_lines (&request);
  lines = &request.lines[0];
  lines->lines = g_newa (GridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GridLine));
  lines = &request.lines[1];
  lines->lines = g_newa (GridLine, lines->max - lines->min);
  memset (lines->lines, 0, (lines->max - lines->min) * sizeof (GridLine));

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    orientation = GTK_ORIENTATION_HORIZONTAL;
  else
    orientation = GTK_ORIENTATION_VERTICAL;

  grid_request_run (&request, OPPOSITE_ORIENTATION (orientation), FALSE);
  grid_request_allocate (&request, OPPOSITE_ORIENTATION (orientation),
                         GET_SIZE (width, height, OPPOSITE_ORIENTATION (orientation)));

  grid_request_run (&request, orientation, TRUE);
  grid_request_allocate (&request, orientation, GET_SIZE (width, height, orientation));

  grid_request_position (&request, 0);
  grid_request_position (&request, 1);
  grid_request_allocate_children (&request, width, height);
}

static void
gtk_grid_layout_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_SPACING:
      gtk_grid_layout_set_row_spacing (self, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_layout_set_column_spacing (self, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_layout_set_row_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_layout_set_column_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_layout_set_baseline_row (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_SPACING:
      g_value_set_int (value, COLUMNS (self)->spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, ROWS (self)->spacing);
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, COLUMNS (self)->homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, ROWS (self)->homogeneous);
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, self->baseline_row);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_finalize (GObject *gobject)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  g_clear_pointer (&self->row_properties, g_array_unref);

  G_OBJECT_CLASS (gtk_grid_layout_parent_class)->finalize (gobject);
}

static void
gtk_grid_layout_class_init (GtkGridLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->layout_child_type = GTK_TYPE_GRID_LAYOUT_CHILD;
  layout_class->measure = gtk_grid_layout_measure;
  layout_class->allocate = gtk_grid_layout_allocate;

  gobject_class->set_property = gtk_grid_layout_set_property;
  gobject_class->get_property = gtk_grid_layout_get_property;
  gobject_class->finalize = gtk_grid_layout_finalize;

  /**
   * GtkGridLayout:row-spacing: (attributes org.gtk.Property.get=gtk_grid_layout_get_row_spacing org.gtk.Property.set=gtk_grid_layout_set_row_spacing)
   *
   * The amount of space between to consecutive rows.
   */
  layout_props[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:column-spacing: (attributes org.gtk.Property.get=gtk_grid_layout_get_column_spacing org.gtk.Property.set=gtk_grid_layout_set_column_spacing)
   *
   * The amount of space between to consecutive columns.
   */
  layout_props[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:row-homogeneous: (attributes org.gtk.Property.get=gtk_grid_layout_get_row_homogeneous org.gtk.Property.set=gtk_grid_layout_set_row_homogeneous)
   *
   * Whether all the rows in the grid have the same height.
   */
  layout_props[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:column-homogeneous: (attributes org.gtk.Property.get=gtk_grid_layout_get_column_homogeneous org.gtk.Property.set=gtk_grid_layout_set_column_homogeneous)
   *
   * Whether all the columns in the grid have the same width.
   */
  layout_props[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:baseline-row: (attributes org.gtk.Property.get=gtk_grid_layout_get_baseline_row org.gtk.Property.set=gtk_grid_layout_set_baseline_row)
   *
   * The row to align to the baseline, when `GtkWidget:valign` is set
   * to %GTK_ALIGN_BASELINE.
   */
  layout_props[PROP_BASELINE_ROW] =
    g_param_spec_int ("baseline-row", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, layout_props);
}

static void
gtk_grid_layout_init (GtkGridLayout *self)
{
}

/**
 * gtk_grid_layout_new:
 *
 * Creates a new `GtkGridLayout`.
 *
 * Returns: the newly created `GtkGridLayout`
 */
GtkLayoutManager *
gtk_grid_layout_new (void)
{
  return g_object_new (GTK_TYPE_GRID_LAYOUT, NULL);
}

/**
 * gtk_grid_layout_set_row_homogeneous: (attributes org.gtk.Method.set_property=row-homogeneous)
 * @grid: a `GtkGridLayout`
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @grid should have the same height.
 */
void
gtk_grid_layout_set_row_homogeneous (GtkGridLayout *grid,
                                     gboolean       homogeneous)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  /* Yes, homogeneous rows means all the columns have the same size */
  if (COLUMNS (grid)->homogeneous == !!homogeneous)
    return;

  COLUMNS (grid)->homogeneous = !!homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_ROW_HOMOGENEOUS]);
}

/**
 * gtk_grid_layout_get_row_homogeneous: (attributes org.gtk.Method.get_property=row-homogeneous)
 * @grid: a `GtkGridLayout`
 *
 * Checks whether all rows of @grid should have the same height.
 *
 * Returns: %TRUE if the rows are homogeneous, and %FALSE otherwise
 */
gboolean
gtk_grid_layout_get_row_homogeneous (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), FALSE);

  return COLUMNS (grid)->homogeneous;
}

/**
 * gtk_grid_layout_set_row_spacing: (attributes org.gtk.Method.set_property=row-spacing)
 * @grid: a `GtkGridLayout`
 * @spacing: the amount of space between rows, in pixels
 *
 * Sets the amount of space to insert between consecutive rows.
 */
void
gtk_grid_layout_set_row_spacing (GtkGridLayout *grid,
                                 guint          spacing)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (COLUMNS (grid)->spacing == spacing)
    return;

  COLUMNS (grid)->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_ROW_SPACING]);
}

/**
 * gtk_grid_layout_get_row_spacing: (attributes org.gtk.Method.get_property=row-spacing)
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the spacing set with gtk_grid_layout_set_row_spacing().
 *
 * Returns: the spacing between consecutive rows
 */
guint
gtk_grid_layout_get_row_spacing (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), 0);

  return COLUMNS (grid)->spacing;
}

/**
 * gtk_grid_layout_set_column_homogeneous: (attributes org.gtk.Method.set_property=column-homogeneous)
 * @grid: a `GtkGridLayout`
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @grid should have the same width.
 */
void
gtk_grid_layout_set_column_homogeneous (GtkGridLayout *grid,
                                        gboolean       homogeneous)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  /* Yes, homogeneous columns means all the rows have the same size */
  if (ROWS (grid)->homogeneous == !!homogeneous)
    return;

  ROWS (grid)->homogeneous = !!homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_COLUMN_HOMOGENEOUS]);
}

/**
 * gtk_grid_layout_get_column_homogeneous: (attributes org.gtk.Method.get_property=column-homogeneous)
 * @grid: a `GtkGridLayout`
 *
 * Checks whether all columns of @grid should have the same width.
 *
 * Returns: %TRUE if the columns are homogeneous, and %FALSE otherwise
 */
gboolean
gtk_grid_layout_get_column_homogeneous (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), FALSE);

  return ROWS (grid)->homogeneous;
}

/**
 * gtk_grid_layout_set_column_spacing: (attributes org.gtk.Method.set_property=column-spacing)
 * @grid: a `GtkGridLayout`
 * @spacing: the amount of space between columns, in pixels
 *
 * Sets the amount of space to insert between consecutive columns.
 */
void
gtk_grid_layout_set_column_spacing (GtkGridLayout *grid,
                                    guint          spacing)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (ROWS (grid)->spacing == spacing)
    return;

  ROWS (grid)->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_COLUMN_SPACING]);
}

/**
 * gtk_grid_layout_get_column_spacing: (attributes org.gtk.Method.get_property=column-spacing)
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the spacing set with gtk_grid_layout_set_column_spacing().
 *
 * Returns: the spacing between consecutive columns
 */
guint
gtk_grid_layout_get_column_spacing (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), 0);

  return ROWS (grid)->spacing;
}

static GridRowProperties *
find_row_properties (GtkGridLayout *self,
		     int            row)
{
  int i;

  if (self->row_properties == NULL)
    return NULL;

  for (i = 0; i < self->row_properties->len; i++)
    {
      GridRowProperties *prop = &g_array_index (self->row_properties, GridRowProperties, i);

      if (prop->row == row)
        return prop;
    }

  return NULL;
}

static GridRowProperties *
get_row_properties_or_create (GtkGridLayout *self,
			      int            row)
{
  GridRowProperties *props;

  props = find_row_properties (self, row);
  if (props != NULL)
    return props;

  /* This is the only place where we create the row properties array;
   * find_row_properties() is used by getters, so we should not create
   * the array there.
   */
  if (self->row_properties == NULL)
    self->row_properties = g_array_new (FALSE, FALSE, sizeof (GridRowProperties));

  g_array_append_vals (self->row_properties, &grid_row_properties_default, 1);
  props = &g_array_index (self->row_properties, GridRowProperties, self->row_properties->len - 1);
  props->row = row;

  return props;
}

static const GridRowProperties *
get_row_properties_or_default (GtkGridLayout *self,
			       int            row)
{
  GridRowProperties *props;

  props = find_row_properties (self, row);
  if (props != NULL)
    return props;

  return &grid_row_properties_default;
}

/**
 * gtk_grid_layout_set_row_baseline_position:
 * @grid: a `GtkGridLayout`
 * @row: a row index
 * @pos: a `GtkBaselinePosition`
 *
 * Sets how the baseline should be positioned on @row of the
 * grid, in case that row is assigned more space than is requested.
 */
void
gtk_grid_layout_set_row_baseline_position (GtkGridLayout       *grid,
                                           int                  row,
                                           GtkBaselinePosition  pos)
{
  GridRowProperties *props;

  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  props = get_row_properties_or_create (grid, row);

  if (props->baseline_position == pos)
    return;

  props->baseline_position = pos;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
}

/**
 * gtk_grid_layout_get_row_baseline_position:
 * @grid: a `GtkGridLayout`
 * @row: a row index
 *
 * Returns the baseline position of @row.
 *
 * If no value has been set with
 * [method@Gtk.GridLayout.set_row_baseline_position],
 * the default value of %GTK_BASELINE_POSITION_CENTER
 * is returned.
 *
 * Returns: the baseline position of @row
 */
GtkBaselinePosition
gtk_grid_layout_get_row_baseline_position (GtkGridLayout *grid,
                                           int            row)
{
  const GridRowProperties *props;

  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), GTK_BASELINE_POSITION_CENTER);

  props = get_row_properties_or_default (grid, row);

  return props->baseline_position;
}

/**
 * gtk_grid_layout_set_baseline_row: (attributes org.gtk.Method.set_property=baseline-row)
 * @grid: a `GtkGridLayout`
 * @row: the row index
 *
 * Sets which row defines the global baseline for the entire grid.
 *
 * Each row in the grid can have its own local baseline, but only
 * one of those is global, meaning it will be the baseline in the
 * parent of the @grid.
 */
void
gtk_grid_layout_set_baseline_row (GtkGridLayout *grid,
                                  int            row)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  if (grid->baseline_row == row)
    return;

  grid->baseline_row = row;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_BASELINE_ROW]);
}

/**
 * gtk_grid_layout_get_baseline_row: (attributes org.gtk.Method.get_property=baseline-row)
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the row set with gtk_grid_layout_set_baseline_row().
 *
 * Returns: the global baseline row
 */
int
gtk_grid_layout_get_baseline_row (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), GTK_BASELINE_POSITION_CENTER);

  return grid->baseline_row;
}
/* }}} */
