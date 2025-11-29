/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcolumnviewcellwidgetprivate.h"

#include "gtkcolumnviewcellprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewrowwidgetprivate.h"
#include "gtkcssboxesprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtklistfactorywidgetprivate.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"


struct _GtkColumnViewCellWidget
{
  GtkListItemWidget parent_instance;

  GtkColumnViewColumn *column;

  /* This list isn't sorted - next/prev refer to list elements, not rows in the list */
  GtkColumnViewCellWidget *next_cell;
  GtkColumnViewCellWidget *prev_cell;
};

struct _GtkColumnViewCellWidgetClass
{
  GtkListItemWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewCellWidget, gtk_column_view_cell_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

static gboolean
gtk_column_view_cell_widget_focus (GtkWidget        *widget,
                                   GtkDirectionType  direction)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (gtk_widget_get_focus_child (widget))
    {
      /* focus is in the child */
      if (direction == GTK_DIR_TAB_BACKWARD)
        return gtk_widget_grab_focus_self (widget);
      else
        return FALSE;
    }
  else if (gtk_widget_is_focus (widget))
    {
      /* The widget has focus */
      if (direction == GTK_DIR_TAB_FORWARD)
        {
          if (child)
            return gtk_widget_child_focus (child, direction);
        }

      return FALSE;
    }
  else
    {
      /* focus coming in from the outside */
      if (direction == GTK_DIR_TAB_BACKWARD)
        {
          if (child &&
              gtk_widget_child_focus (child, direction))
            return TRUE;

          return gtk_widget_grab_focus_self (widget);
        }
      else
        {
          if (gtk_widget_grab_focus_self (widget))
            return TRUE;

          if (child &&
              gtk_widget_child_focus (child, direction))
            return TRUE;

          return FALSE;
        }
    }
}

static gboolean
gtk_column_view_cell_widget_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  if (GTK_WIDGET_CLASS (gtk_column_view_cell_widget_parent_class)->grab_focus (widget))
    return TRUE;

  child = gtk_widget_get_first_child (widget);
  if (child && gtk_widget_grab_focus (child))
    return TRUE;

  return FALSE;
}

static gpointer
gtk_column_view_cell_widget_create_object (GtkListFactoryWidget *fw)
{
  return gtk_column_view_cell_new ();
}

static void
gtk_column_view_cell_widget_setup_object (GtkListFactoryWidget *fw,
                                          gpointer              object)
{
  GtkColumnViewCellWidget *self = GTK_COLUMN_VIEW_CELL_WIDGET (fw);
  GtkColumnViewCell *cell = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_cell_widget_parent_class)->setup_object (fw, object);

  cell->cell = self;

  gtk_column_view_cell_widget_set_child (GTK_COLUMN_VIEW_CELL_WIDGET (self), cell->child);

  gtk_widget_set_focusable (GTK_WIDGET (self), cell->focusable);

  gtk_column_view_cell_do_notify (cell,
                                  gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                  gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                                  gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_column_view_cell_widget_teardown_object (GtkListFactoryWidget *fw,
                                             gpointer              object)
{
  GtkColumnViewCellWidget *self = GTK_COLUMN_VIEW_CELL_WIDGET (fw);
  GtkColumnViewCell *cell = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_cell_widget_parent_class)->teardown_object (fw, object);

  cell->cell = NULL;

  gtk_column_view_cell_widget_set_child (GTK_COLUMN_VIEW_CELL_WIDGET (self), NULL);

  gtk_widget_set_focusable (GTK_WIDGET (self), FALSE);

  gtk_column_view_cell_do_notify (cell,
                                  gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                  gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                                  gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_column_view_cell_widget_update_object (GtkListFactoryWidget *fw,
                                           gpointer              object,
                                           guint                 position,
                                           gpointer              item,
                                           gboolean              selected)
{
  GtkColumnViewCellWidget *self = GTK_COLUMN_VIEW_CELL_WIDGET (fw);
  GtkListItemBase *base = GTK_LIST_ITEM_BASE (self);
  GtkColumnViewCell *cell = object;
  /* Track notify manually instead of freeze/thaw_notify for performance reasons. */
  gboolean notify_item = FALSE, notify_position = FALSE, notify_selected = FALSE;

  /* FIXME: It's kinda evil to notify external objects from here... */
  notify_item = gtk_list_item_base_get_item (base) != item;
  notify_position = gtk_list_item_base_get_position (base) != position;
  notify_selected = gtk_list_item_base_get_selected (base) != selected;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_cell_widget_parent_class)->update_object (fw,
                                                                                           object,
                                                                                           position,
                                                                                           item,
                                                                                           selected);

  if (cell)
    gtk_column_view_cell_do_notify (cell, notify_item, notify_position, notify_selected);
}

static int
unadjust_width (GtkWidget *widget,
                int        width)
{
  GtkCssBoxes boxes;

  if (width <= -1)
    return -1;

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_css_node_get_style (gtk_widget_get_css_node (widget)),
                                 0, 0,
                                 width, 100000);
  return MAX (0, floor (gtk_css_boxes_get_content_rect (&boxes)->size.width));
}

static void
gtk_column_view_cell_widget_measure (GtkWidget      *widget,
                                     GtkOrientation  orientation,
                                     int             for_size,
                                     int            *minimum,
                                     int            *natural,
                                     int            *minimum_baseline,
                                     int            *natural_baseline)
{
  GtkColumnViewCellWidget *cell = GTK_COLUMN_VIEW_CELL_WIDGET (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  int fixed_width, unadj_width;

  fixed_width = gtk_column_view_column_get_fixed_width (cell->column);
  unadj_width = unadjust_width (widget, fixed_width);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (fixed_width > -1)
        {
          int min;

          if (for_size == -1)
            for_size = unadj_width;
          else
            for_size = MIN (for_size, unadj_width);

          gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
          for_size = MAX (for_size, min);
        }
    }

  if (child)
    gtk_widget_measure (child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (fixed_width > -1)
        {
          *minimum = 0;
          *natural = unadj_width;
        }
    }
}

static void
gtk_column_view_cell_widget_size_allocate (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           int        baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    {
      int min;

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, height, &min, NULL, NULL, NULL);

      gtk_widget_allocate (child, MAX (min, width), height, baseline, NULL);
    }
}

/* This should be to be called when unsetting the parent, but we have no
 * set_parent vfunc().
 */
void
gtk_column_view_cell_widget_unset_column (GtkColumnViewCellWidget *self)
{
  if (self->column)
    {
      gtk_column_view_column_remove_cell (self->column, self);

      if (self->prev_cell)
        self->prev_cell->next_cell = self->next_cell;
      if (self->next_cell)
        self->next_cell->prev_cell = self->prev_cell;

      self->prev_cell = NULL;
      self->next_cell = NULL;

      g_clear_object (&self->column);
    }
}

static void
gtk_column_view_cell_widget_dispose (GObject *object)
{
  GtkColumnViewCellWidget *self = GTK_COLUMN_VIEW_CELL_WIDGET (object);

  /* unset_parent() forgot to call this. Be very angry. */
  g_warn_if_fail (self->column == NULL);

  G_OBJECT_CLASS (gtk_column_view_cell_widget_parent_class)->dispose (object);
}

static GtkSizeRequestMode
gtk_column_view_cell_widget_get_request_mode (GtkWidget *widget)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    return gtk_widget_get_request_mode (child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_column_view_cell_widget_class_init (GtkColumnViewCellWidgetClass *klass)
{
  GtkListFactoryWidgetClass *factory_class = GTK_LIST_FACTORY_WIDGET_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  factory_class->create_object = gtk_column_view_cell_widget_create_object;
  factory_class->setup_object = gtk_column_view_cell_widget_setup_object;
  factory_class->update_object = gtk_column_view_cell_widget_update_object;
  factory_class->teardown_object = gtk_column_view_cell_widget_teardown_object;

  widget_class->focus = gtk_column_view_cell_widget_focus;
  widget_class->grab_focus = gtk_column_view_cell_widget_grab_focus;
  widget_class->measure = gtk_column_view_cell_widget_measure;
  widget_class->size_allocate = gtk_column_view_cell_widget_size_allocate;
  widget_class->get_request_mode = gtk_column_view_cell_widget_get_request_mode;

  gobject_class->dispose = gtk_column_view_cell_widget_dispose;

  gtk_widget_class_set_css_name (widget_class, I_("cell"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GRID_CELL);
}

static void
gtk_column_view_cell_widget_resize_func (GtkWidget *widget)
{
  GtkColumnViewCellWidget *self = GTK_COLUMN_VIEW_CELL_WIDGET (widget);

  if (self->column)
    gtk_column_view_column_queue_resize (self->column);
}

static void
gtk_column_view_cell_widget_init (GtkColumnViewCellWidget *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_focusable (widget, FALSE);
  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);
  /* FIXME: Figure out if setting the manager class to INVALID should work */
  gtk_widget_set_layout_manager (widget, NULL);
  widget->priv->resize_func = gtk_column_view_cell_widget_resize_func;
}

GtkWidget *
gtk_column_view_cell_widget_new (GtkColumnViewColumn *column,
                                 gboolean             inert)
{
  GtkColumnViewCellWidget *self;

  self = g_object_new (GTK_TYPE_COLUMN_VIEW_CELL_WIDGET,
                       "factory", inert ? NULL : gtk_column_view_column_get_factory (column),
                       NULL);

  self->column = g_object_ref (column);

  self->next_cell = gtk_column_view_column_get_first_cell (self->column);
  if (self->next_cell)
    self->next_cell->prev_cell = self;

  gtk_column_view_column_add_cell (self->column, self);

  return GTK_WIDGET (self);
}

void
gtk_column_view_cell_widget_remove (GtkColumnViewCellWidget *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_column_view_row_widget_remove_child (GTK_COLUMN_VIEW_ROW_WIDGET (gtk_widget_get_parent (widget)), widget);
}

GtkColumnViewCellWidget *
gtk_column_view_cell_widget_get_next (GtkColumnViewCellWidget *self)
{
  return self->next_cell;
}

GtkColumnViewCellWidget *
gtk_column_view_cell_widget_get_prev (GtkColumnViewCellWidget *self)
{
  return self->prev_cell;
}

GtkColumnViewColumn *
gtk_column_view_cell_widget_get_column (GtkColumnViewCellWidget *self)
{
  return self->column;
}

void
gtk_column_view_cell_widget_set_child (GtkColumnViewCellWidget *self,
                                       GtkWidget               *child)
{
  GtkWidget *cur_child = gtk_widget_get_first_child (GTK_WIDGET (self));

  if (cur_child == child)
    return;

  g_clear_pointer (&cur_child, gtk_widget_unparent);

  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}
