/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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

#include <string.h>

#include "gtkgrid.h"

#include "gtkbuildable.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkgridlayout.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"


/**
 * GtkGrid:
 *
 * `GtkGrid` is a container which arranges its child widgets in
 * rows and columns.
 *
 * ![An example GtkGrid](grid.png)
 *
 * It supports arbitrary positions and horizontal/vertical spans.
 *
 * Children are added using [method@Gtk.Grid.attach]. They can span multiple
 * rows or columns. It is also possible to add a child next to an existing
 * child, using [method@Gtk.Grid.attach_next_to]. To remove a child from the
 * grid, use [method@Gtk.Grid.remove].
 *
 * The behaviour of `GtkGrid` when several children occupy the same grid
 * cell is undefined.
 *
 * # GtkGrid as GtkBuildable
 *
 * Every child in a `GtkGrid` has access to a custom [iface@Gtk.Buildable]
 * element, called `<layout>`. It can by used to specify a position in the
 * grid and optionally spans. All properties that can be used in the `<layout>`
 * element are implemented by [class@Gtk.GridLayoutChild].
 *
 * It is implemented by `GtkWidget` using [class@Gtk.LayoutManager].
 *
 * To showcase it, here is a simple example:
 *
 * ```xml
 * <object class="GtkGrid" id="my_grid">
 *   <child>
 *     <object class="GtkButton" id="button1">
 *       <property name="label">Button 1</property>
 *       <layout>
 *         <property name="column">0</property>
 *         <property name="row">0</property>
 *       </layout>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkButton" id="button2">
 *       <property name="label">Button 2</property>
 *       <layout>
 *         <property name="column">1</property>
 *         <property name="row">0</property>
 *       </layout>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkButton" id="button3">
 *       <property name="label">Button 3</property>
 *       <layout>
 *         <property name="column">2</property>
 *         <property name="row">0</property>
 *         <property name="row-span">2</property>
 *       </layout>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkButton" id="button4">
 *       <property name="label">Button 4</property>
 *       <layout>
 *         <property name="column">0</property>
 *         <property name="row">1</property>
 *         <property name="column-span">2</property>
 *       </layout>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * It organizes the first two buttons side-by-side in one cell each.
 * The third button is in the last column but spans across two rows.
 * This is defined by the `row-span` property. The last button is
 * located in the second row and spans across two columns, which is
 * defined by the `column-span` property.
 *
 * # CSS nodes
 *
 * `GtkGrid` uses a single CSS node with name `grid`.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkGrid` used the `GTK_ACCESSIBLE_ROLE_GROUP` role.
 *
 * Starting from GTK 4.12, `GtkGrid` uses the `GTK_ACCESSIBLE_ROLE_GENERIC` role.
 */

typedef struct
{
  GtkLayoutManager *layout_manager;

  GtkOrientation orientation;
} GtkGridPrivate;

enum
{
  PROP_0,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,
  N_PROPERTIES,

  /* GtkOrientable */
  PROP_ORIENTATION
};

static void gtk_grid_buildable_iface_init (GtkBuildableIface *iface);

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkGrid, gtk_grid, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkGrid)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_grid_buildable_iface_init))


static void
gtk_grid_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkGrid *grid = GTK_GRID (object);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_ROW_SPACING:
      g_value_set_int (value, gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_set_orientation (GtkGrid        *grid,
                          GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;

      gtk_widget_update_orientation (GTK_WIDGET (grid), priv->orientation);

      g_object_notify (G_OBJECT (grid), "orientation");
    }
}

static void
gtk_grid_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkGrid *grid = GTK_GRID (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_grid_set_orientation (grid, g_value_get_enum (value));
      break;

    case PROP_ROW_SPACING:
      gtk_grid_set_row_spacing (grid, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_set_column_spacing (grid, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_set_row_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_set_column_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_set_baseline_row (grid, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
grid_attach (GtkGrid   *grid,
             GtkWidget *widget,
             int        column,
             int        row,
             int        width,
             int        height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_child;

  gtk_widget_set_parent (widget, GTK_WIDGET (grid));

  grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, widget));
  gtk_grid_layout_child_set_column (grid_child, column);
  gtk_grid_layout_child_set_row (grid_child, row);
  gtk_grid_layout_child_set_column_span (grid_child, width);
  gtk_grid_layout_child_set_row_span (grid_child, height);
}

/* Find the position 'touching' existing
 * children. @orientation and @max determine
 * from which direction to approach (horizontal
 * + max = right, vertical + !max = top, etc).
 * @op_pos, @op_span determine the rows/columns
 * in which the touching has to happen.
 */
static int
find_attach_position (GtkGrid         *grid,
                      GtkOrientation   orientation,
                      int              op_pos,
                      int              op_span,
                      gboolean         max)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;
  gboolean hit;
  int pos;

  if (max)
    pos = -G_MAXINT;
  else
    pos = G_MAXINT;

  hit = FALSE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int attach_pos = 0, attach_span = 0;
      int opposite_pos = 0, opposite_span = 0;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

      switch (orientation)
        {
        case GTK_ORIENTATION_HORIZONTAL:
          attach_pos = gtk_grid_layout_child_get_column (grid_child);
          attach_span = gtk_grid_layout_child_get_column_span (grid_child);
          opposite_pos = gtk_grid_layout_child_get_row (grid_child);
          opposite_span = gtk_grid_layout_child_get_row_span (grid_child);
          break;

        case GTK_ORIENTATION_VERTICAL:
          attach_pos = gtk_grid_layout_child_get_row (grid_child);
          attach_span = gtk_grid_layout_child_get_row_span (grid_child);
          opposite_pos = gtk_grid_layout_child_get_column (grid_child);
          opposite_span = gtk_grid_layout_child_get_column_span (grid_child);
          break;

        default:
          break;
        }

      /* check if the ranges overlap */
      if (opposite_pos <= op_pos + op_span && op_pos <= opposite_pos + opposite_span)
        {
          hit = TRUE;

          if (max)
            pos = MAX (pos, attach_pos + attach_span);
          else
            pos = MIN (pos, attach_pos);
        }
     }

  if (!hit)
    pos = 0;

  return pos;
}

static void
gtk_grid_compute_expand (GtkWidget *widget,
                         gboolean  *hexpand_p,
                         gboolean  *vexpand_p)
{
  GtkWidget *w;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      hexpand = hexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static GtkSizeRequestMode
gtk_grid_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_grid_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_grid_remove (GTK_GRID (object), child);

  G_OBJECT_CLASS (gtk_grid_parent_class)->dispose (object);
}

static void
gtk_grid_class_init (GtkGridClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_grid_dispose;
  object_class->get_property = gtk_grid_get_property;
  object_class->set_property = gtk_grid_set_property;

  widget_class->compute_expand = gtk_grid_compute_expand;
  widget_class->get_request_mode = gtk_grid_get_request_mode;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkGrid:row-spacing: (attributes org.gtk.Property.get=gtk_grid_get_row_spacing org.gtk.Property.set=gtk_grid_set_row_spacing)
   *
   * The amount of space between two consecutive rows.
   */
  obj_properties[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGrid:column-spacing: (attributes org.gtk.Property.get=gtk_grid_get_column_spacing org.gtk.Property.set=gtk_grid_set_column_spacing)
   *
   * The amount of space between two consecutive columns.
   */
  obj_properties[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGrid:row-homogeneous: (attributes org.gtk.Property.get=gtk_grid_get_row_homogeneous org.gtk.Property.set=gtk_grid_set_row_homogeneous)
   *
   * If %TRUE, the rows are all the same height.
   */
  obj_properties[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGrid:column-homogeneous: (attributes org.gtk.Property.get=gtk_grid_get_column_homogeneous org.gtk.Property.set=gtk_grid_set_column_homogeneous)
   *
   * If %TRUE, the columns are all the same width.
   */
  obj_properties[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGrid:baseline-row: (attributes org.gtk.Property.get=gtk_grid_get_baseline_row org.gtk.Property.set=gtk_grid_set_baseline_row)
   *
   * The row to align to the baseline when valign is using baseline alignment.
   */
  obj_properties[PROP_BASELINE_ROW] =
    g_param_spec_int ("baseline-row", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_css_name (widget_class, I_("grid"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_grid_buildable_add_child (GtkBuildable *buildable,
                              GtkBuilder   *builder,
                              GObject      *child,
                              const char   *type)
{
  if (GTK_IS_WIDGET (child))
    {
      GtkGrid *grid = GTK_GRID ( buildable);
      GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
      int pos[2] = { 0, 0 };

      pos[priv->orientation] = find_attach_position (grid, priv->orientation, 0, 1, TRUE);
      grid_attach (grid, GTK_WIDGET (child), pos[0], pos[1], 1, 1);
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_grid_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_grid_buildable_add_child;
}

static void
gtk_grid_init (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  priv->layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (grid));

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  gtk_widget_update_orientation (GTK_WIDGET (grid), priv->orientation);
}

/**
 * gtk_grid_new:
 *
 * Creates a new grid widget.
 *
 * Returns: the new `GtkGrid`
 */
GtkWidget *
gtk_grid_new (void)
{
  return g_object_new (GTK_TYPE_GRID, NULL);
}

/**
 * gtk_grid_attach:
 * @grid: a `GtkGrid`
 * @child: the widget to add
 * @column: the column number to attach the left side of @child to
 * @row: the row number to attach the top side of @child to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The position of @child is determined by @column and @row.
 * The number of “cells” that @child will occupy is determined
 * by @width and @height.
 */
void
gtk_grid_attach (GtkGrid   *grid,
                 GtkWidget *child,
                 int        column,
                 int        row,
                 int        width,
                 int        height)
{
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  grid_attach (grid, child, column, row, width, height);
}

/**
 * gtk_grid_attach_next_to:
 * @grid: a `GtkGrid`
 * @child: the widget to add
 * @sibling: (nullable): the child of @grid that @child will be placed
 *   next to, or %NULL to place @child at the beginning or end
 * @side: the side of @sibling that @child is positioned next to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The widget is placed next to @sibling, on the side determined by
 * @side. When @sibling is %NULL, the widget is placed in row (for
 * left or right placement) or column 0 (for top or bottom placement),
 * at the end indicated by @side.
 *
 * Attaching widgets labeled `[1]`, `[2]`, `[3]` with `@sibling == %NULL` and
 * `@side == %GTK_POS_LEFT` yields a layout of `[3][2][1]`.
 */
void
gtk_grid_attach_next_to (GtkGrid         *grid,
                         GtkWidget       *child,
                         GtkWidget       *sibling,
                         GtkPositionType  side,
                         int              width,
                         int              height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_sibling;
  int left, top;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (sibling == NULL || _gtk_widget_get_parent (sibling) == (GtkWidget*)grid);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  if (sibling != NULL)
    {
      grid_sibling = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, sibling));

      switch (side)
        {
        case GTK_POS_LEFT:
          left = gtk_grid_layout_child_get_column (grid_sibling) - width;
          top = gtk_grid_layout_child_get_row (grid_sibling);
          break;
        case GTK_POS_RIGHT:
          left = gtk_grid_layout_child_get_column (grid_sibling) +
                 gtk_grid_layout_child_get_column_span (grid_sibling);
          top = gtk_grid_layout_child_get_row (grid_sibling);
          break;
        case GTK_POS_TOP:
          left = gtk_grid_layout_child_get_column (grid_sibling);
          top = gtk_grid_layout_child_get_row (grid_sibling) - height;
          break;
        case GTK_POS_BOTTOM:
          left = gtk_grid_layout_child_get_column (grid_sibling);
          top = gtk_grid_layout_child_get_row (grid_sibling) +
                gtk_grid_layout_child_get_row_span (grid_sibling);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      switch (side)
        {
        case GTK_POS_LEFT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, FALSE);
          left -= width;
          top = 0;
          break;
        case GTK_POS_RIGHT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, TRUE);
          top = 0;
          break;
        case GTK_POS_TOP:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, FALSE);
          top -= height;
          break;
        case GTK_POS_BOTTOM:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, TRUE);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  grid_attach (grid, child, left, top, width, height);
}

/**
 * gtk_grid_get_child_at:
 * @grid: a `GtkGrid`
 * @column: the left edge of the cell
 * @row: the top edge of the cell
 *
 * Gets the child of @grid whose area covers the grid
 * cell at @column, @row.
 *
 * Returns: (transfer none) (nullable): the child at the given position
 */
GtkWidget *
gtk_grid_get_child_at (GtkGrid *grid,
                       int      column,
                       int      row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_GRID (grid), NULL);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int child_column, child_row, child_width, child_height;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      child_column = gtk_grid_layout_child_get_column (grid_child);
      child_row = gtk_grid_layout_child_get_row (grid_child);
      child_width = gtk_grid_layout_child_get_column_span (grid_child);
      child_height = gtk_grid_layout_child_get_row_span (grid_child);

      if (child_column <= column && child_column + child_width > column &&
          child_row <= row && child_row + child_height > row)
        return child;
    }

  return NULL;
}

/**
 * gtk_grid_remove:
 * @grid: a `GtkGrid`
 * @child: the child widget to remove
 *
 * Removes a child from @grid.
 *
 * The child must have been added with
 * [method@Gtk.Grid.attach] or [method@Gtk.Grid.attach_next_to].
 */
void
gtk_grid_remove (GtkGrid   *grid,
                 GtkWidget *child)
{
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (grid));

  gtk_widget_unparent (child);
}

/**
 * gtk_grid_insert_row:
 * @grid: a `GtkGrid`
 * @position: the position to insert the row at
 *
 * Inserts a row at the specified position.
 *
 * Children which are attached at or below this position
 * are moved one row down. Children which span across this
 * position are grown to span the new row.
 */
void
gtk_grid_insert_row (GtkGrid *grid,
                     int      position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;
  int top, height;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      
      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      top = gtk_grid_layout_child_get_row (grid_child);
      height = gtk_grid_layout_child_get_row_span (grid_child);

      if (top >= position)
        gtk_grid_layout_child_set_row (grid_child, top + 1);
      else if (top + height > position)
        gtk_grid_layout_child_set_row_span (grid_child, height + 1);
    }
}

/**
 * gtk_grid_remove_row:
 * @grid: a `GtkGrid`
 * @position: the position of the row to remove
 *
 * Removes a row from the grid.
 *
 * Children that are placed in this row are removed,
 * spanning children that overlap this row have their
 * height reduced by one, and children below the row
 * are moved up.
 */
void
gtk_grid_remove_row (GtkGrid *grid,
                     int      position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      GtkGridLayoutChild *grid_child;
      int top, height;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      top = gtk_grid_layout_child_get_row (grid_child);
      height = gtk_grid_layout_child_get_row_span (grid_child);

      if (top <= position && top + height > position)
        height--;
      if (top > position)
        top--;

      if (height <= 0)
        {
          gtk_grid_remove (grid, child);
        }
      else
        {
          gtk_grid_layout_child_set_row_span (grid_child, height);
          gtk_grid_layout_child_set_row (grid_child, top);
        }

      child = next;
    }
}

/**
 * gtk_grid_insert_column:
 * @grid: a `GtkGrid`
 * @position: the position to insert the column at
 *
 * Inserts a column at the specified position.
 *
 * Children which are attached at or to the right of this position
 * are moved one column to the right. Children which span across this
 * position are grown to span the new column.
 */
void
gtk_grid_insert_column (GtkGrid *grid,
                        int      position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int left, width;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      left = gtk_grid_layout_child_get_column (grid_child);
      width = gtk_grid_layout_child_get_column_span (grid_child);

      if (left >= position)
        gtk_grid_layout_child_set_column (grid_child, left + 1);
      else if (left + width > position)
        gtk_grid_layout_child_set_column_span (grid_child, width + 1);
    }
}

/**
 * gtk_grid_remove_column:
 * @grid: a `GtkGrid`
 * @position: the position of the column to remove
 *
 * Removes a column from the grid.
 *
 * Children that are placed in this column are removed,
 * spanning children that overlap this column have their
 * width reduced by one, and children after the column
 * are moved to the left.
 */
void
gtk_grid_remove_column (GtkGrid *grid,
                        int      position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      GtkGridLayoutChild *grid_child;
      int left, width;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

      left = gtk_grid_layout_child_get_column (grid_child);
      width = gtk_grid_layout_child_get_column_span (grid_child);

      if (left <= position && left + width > position)
        width--;
      if (left > position)
        left--;

      if (width <= 0)
        {
          gtk_grid_remove (grid, child);
        }
      else
        {
          gtk_grid_layout_child_set_column_span (grid_child, width);
          gtk_grid_layout_child_set_column (grid_child, left);
        }

      child = next;
    }
}

/**
 * gtk_grid_insert_next_to:
 * @grid: a `GtkGrid`
 * @sibling: the child of @grid that the new row or column will be
 *   placed next to
 * @side: the side of @sibling that @child is positioned next to
 *
 * Inserts a row or column at the specified position.
 *
 * The new row or column is placed next to @sibling, on the side
 * determined by @side. If @side is %GTK_POS_TOP or %GTK_POS_BOTTOM,
 * a row is inserted. If @side is %GTK_POS_LEFT of %GTK_POS_RIGHT,
 * a column is inserted.
 */
void
gtk_grid_insert_next_to (GtkGrid         *grid,
                         GtkWidget       *sibling,
                         GtkPositionType  side)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (sibling));
  g_return_if_fail (_gtk_widget_get_parent (sibling) == (GtkWidget*)grid);

  child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, sibling));

  switch (side)
    {
    case GTK_POS_LEFT:
      gtk_grid_insert_column (grid, gtk_grid_layout_child_get_column (child));
      break;
    case GTK_POS_RIGHT:
      {
        int col = gtk_grid_layout_child_get_column (child) +
                  gtk_grid_layout_child_get_column_span (child);
        gtk_grid_insert_column (grid, col);
      }
      break;
    case GTK_POS_TOP:
      gtk_grid_insert_row (grid, gtk_grid_layout_child_get_row (child));
      break;
    case GTK_POS_BOTTOM:
      {
        int row = gtk_grid_layout_child_get_row (child) +
                  gtk_grid_layout_child_get_row_span (child);
        gtk_grid_insert_row (grid, row);
      }
      break;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gtk_grid_set_row_homogeneous: (attributes org.gtk.Method.set_property=row-homogeneous)
 * @grid: a `GtkGrid`
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @grid will have the same height.
 */
void
gtk_grid_set_row_homogeneous (GtkGrid  *grid,
                              gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gboolean old_val;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_val = gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_val != !!homogeneous)
    {
      gtk_grid_layout_set_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager), homogeneous);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_row_homogeneous: (attributes org.gtk.Method.get_property=row-homogeneous)
 * @grid: a `GtkGrid`
 *
 * Returns whether all rows of @grid have the same height.
 *
 * Returns: whether all rows of @grid have the same height.
 */
gboolean
gtk_grid_get_row_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_column_homogeneous: (attributes org.gtk.Method.set_property=column-homogeneous)
 * @grid: a `GtkGrid`
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @grid will have the same width.
 */
void
gtk_grid_set_column_homogeneous (GtkGrid  *grid,
                                 gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gboolean old_val;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_val = gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_val != !!homogeneous)
    {
      gtk_grid_layout_set_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager), homogeneous);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_column_homogeneous: (attributes org.gtk.Method.get_property=column-homogeneous)
 * @grid: a `GtkGrid`
 *
 * Returns whether all columns of @grid have the same width.
 *
 * Returns: whether all columns of @grid have the same width.
 */
gboolean
gtk_grid_get_column_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_row_spacing: (attributes org.gtk.Method.set_property=row-spacing)
 * @grid: a `GtkGrid`
 * @spacing: the amount of space to insert between rows
 *
 * Sets the amount of space between rows of @grid.
 */
void
gtk_grid_set_row_spacing (GtkGrid *grid,
                          guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  guint old_spacing;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  old_spacing = gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_spacing != spacing)
    {
      gtk_grid_layout_set_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager), spacing);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_SPACING]);
    }
}

/**
 * gtk_grid_get_row_spacing: (attributes org.gtk.Method.get_property=row-spacing)
 * @grid: a `GtkGrid`
 *
 * Returns the amount of space between the rows of @grid.
 *
 * Returns: the row spacing of @grid
 */
guint
gtk_grid_get_row_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_column_spacing: (attributes org.gtk.Method.set_property=column-spacing)
 * @grid: a `GtkGrid`
 * @spacing: the amount of space to insert between columns
 *
 * Sets the amount of space between columns of @grid.
 */
void
gtk_grid_set_column_spacing (GtkGrid *grid,
                             guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  guint old_spacing;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  old_spacing = gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_spacing != spacing)
    {
      gtk_grid_layout_set_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager), spacing);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_SPACING]);
    }
}

/**
 * gtk_grid_get_column_spacing: (attributes org.gtk.Method.get_property=column-spacing)
 * @grid: a `GtkGrid`
 *
 * Returns the amount of space between the columns of @grid.
 *
 * Returns: the column spacing of @grid
 */
guint
gtk_grid_get_column_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_row_baseline_position:
 * @grid: a `GtkGrid`
 * @row: a row index
 * @pos: a `GtkBaselinePosition`
 *
 * Sets how the baseline should be positioned on @row of the
 * grid, in case that row is assigned more space than is requested.
 *
 * The default baseline position is %GTK_BASELINE_POSITION_CENTER.
 */
void
gtk_grid_set_row_baseline_position (GtkGrid            *grid,
				    int                 row,
				    GtkBaselinePosition pos)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_if_fail (GTK_IS_GRID (grid));

  gtk_grid_layout_set_row_baseline_position (GTK_GRID_LAYOUT (priv->layout_manager),
                                             row,
                                             pos);
}

/**
 * gtk_grid_get_row_baseline_position:
 * @grid: a `GtkGrid`
 * @row: a row index
 *
 * Returns the baseline position of @row.
 *
 * See [method@Gtk.Grid.set_row_baseline_position].
 *
 * Returns: the baseline position of @row
 */
GtkBaselinePosition
gtk_grid_get_row_baseline_position (GtkGrid      *grid,
				    int           row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), GTK_BASELINE_POSITION_CENTER);

  return gtk_grid_layout_get_row_baseline_position (GTK_GRID_LAYOUT (priv->layout_manager), row);
}

/**
 * gtk_grid_set_baseline_row: (attributes org.gtk.Method.set_property=baseline-row)
 * @grid: a `GtkGrid`
 * @row: the row index
 *
 * Sets which row defines the global baseline for the entire grid.
 *
 * Each row in the grid can have its own local baseline, but only
 * one of those is global, meaning it will be the baseline in the
 * parent of the @grid.
 */
void
gtk_grid_set_baseline_row (GtkGrid *grid,
			   int      row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  int old_row;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_row = gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_row != row)
    {
      gtk_grid_layout_set_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager), row);
      g_object_notify (G_OBJECT (grid), "baseline-row");
    }
}

/**
 * gtk_grid_get_baseline_row: (attributes org.gtk.Method.get_property=baseline-row)
 * @grid: a `GtkGrid`
 *
 * Returns which row defines the global baseline of @grid.
 *
 * Returns: the row index defining the global baseline
 */
int
gtk_grid_get_baseline_row (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_query_child:
 * @grid: a `GtkGrid`
 * @child: a `GtkWidget` child of @grid
 * @column: (out) (optional): the column used to attach the left side of @child
 * @row: (out) (optional): the row used to attach the top side of @child
 * @width: (out) (optional): the number of columns @child spans
 * @height: (out) (optional): the number of rows @child spans
 *
 * Queries the attach points and spans of @child inside the given `GtkGrid`.
 */
void
gtk_grid_query_child (GtkGrid   *grid,
                      GtkWidget *child,
                      int       *column,
                      int       *row,
                      int       *width,
                      int       *height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == (GtkWidget *) grid);

  grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

  if (column != NULL)
    *column = gtk_grid_layout_child_get_column (grid_child);
  if (row != NULL)
    *row = gtk_grid_layout_child_get_row (grid_child);
  if (width != NULL)
    *width = gtk_grid_layout_child_get_column_span (grid_child);
  if (height != NULL)
    *height = gtk_grid_layout_child_get_row_span (grid_child);
}
