/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtkcolumnviewrowwidgetprivate.h"

#include "gtkbinlayout.h"
#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewcellwidgetprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewrowprivate.h"
#include "gtkcolumnviewtitleprivate.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkColumnViewRowWidget, gtk_column_view_row_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

static GtkColumnView *
gtk_column_view_row_widget_get_column_view (GtkColumnViewRowWidget *self)
{
  GtkWidget *parent = _gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_COLUMN_VIEW (parent))
    return GTK_COLUMN_VIEW (parent);

  parent = _gtk_widget_get_parent (parent);
  g_assert (GTK_IS_COLUMN_VIEW (parent));

  return GTK_COLUMN_VIEW (parent);
}

static gboolean
gtk_column_view_row_widget_is_header (GtkColumnViewRowWidget *self)
{
  return gtk_widget_get_css_name (GTK_WIDGET (self)) == g_intern_static_string ("header");
}

static GtkColumnViewColumn *
gtk_column_view_row_child_get_column (GtkWidget *child)
{
  if (GTK_IS_COLUMN_VIEW_CELL_WIDGET (child))
    return gtk_column_view_cell_widget_get_column (GTK_COLUMN_VIEW_CELL_WIDGET (child));
  else
    return gtk_column_view_title_get_column (GTK_COLUMN_VIEW_TITLE (child));

  g_return_val_if_reached (NULL);
}

static GtkWidget *
gtk_column_view_row_widget_find_child (GtkColumnViewRowWidget *self,
                                       GtkColumnViewColumn    *column)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_column_view_row_child_get_column (child) == column)
        return child;
    }

  return NULL;
}

static void
gtk_column_view_row_widget_update (GtkListItemBase *base,
                                   guint            position,
                                   gpointer         item,
                                   gboolean         selected)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (base);
  GtkWidget *child;

  if (gtk_column_view_row_widget_is_header (self))
    return;

  GTK_LIST_ITEM_BASE_CLASS (gtk_column_view_row_widget_parent_class)->update (base, position, item, selected);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (child), position, item, selected);
    }
}

static gpointer
gtk_column_view_row_widget_create_object (GtkListFactoryWidget *fw)
{
  return gtk_column_view_row_new ();
}

static void
gtk_column_view_row_widget_setup_object (GtkListFactoryWidget *fw,
                                         gpointer              object)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (fw);
  GtkColumnViewRow *row = object;

  g_assert (!gtk_column_view_row_widget_is_header (self));

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->setup_object (fw, object);

  row->owner = self;

  gtk_list_factory_widget_set_activatable (fw, row->activatable);
  gtk_list_factory_widget_set_selectable (fw, row->selectable);
  gtk_widget_set_focusable (GTK_WIDGET (self), row->focusable);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, row->accessible_label,
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, row->accessible_description,
                                  -1);

  gtk_column_view_row_do_notify (row,
                                 gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                 gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                                 gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_column_view_row_widget_teardown_object (GtkListFactoryWidget *fw,
                                            gpointer              object)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (fw);
  GtkColumnViewRow *row = object;

  g_assert (!gtk_column_view_row_widget_is_header (self));

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->teardown_object (fw, object);

  row->owner = NULL;

  gtk_list_factory_widget_set_activatable (fw, FALSE);
  gtk_list_factory_widget_set_selectable (fw, FALSE);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL);
  gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);

  gtk_column_view_row_do_notify (row,
                                 gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                 gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                                 gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_column_view_row_widget_update_object (GtkListFactoryWidget *fw,
                                          gpointer              object,
                                          guint                 position,
                                          gpointer              item,
                                          gboolean              selected)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (fw);
  GtkListItemBase *base = GTK_LIST_ITEM_BASE (self);
  GtkColumnViewRow *row = object;
  /* Track notify manually instead of freeze/thaw_notify for performance reasons. */
  gboolean notify_item = FALSE, notify_position = FALSE, notify_selected = FALSE;

  g_assert (!gtk_column_view_row_widget_is_header (self));

  /* FIXME: It's kinda evil to notify external objects from here... */
  notify_item = gtk_list_item_base_get_item (base) != item;
  notify_position = gtk_list_item_base_get_position (base) != position;
  notify_selected = gtk_list_item_base_get_selected (base) != selected;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->update_object (fw,
                                                                                          object,
                                                                                          position,
                                                                                          item,
                                                                                          selected);

  if (row)
    gtk_column_view_row_do_notify (row, notify_item, notify_position, notify_selected);
}

static GtkWidget *
gtk_column_view_next_focus_widget (GtkWidget        *widget,
                                   GtkWidget        *current,
                                   GtkDirectionType  direction)
{
  gboolean forward;

  switch (direction)
    {
      case GTK_DIR_TAB_FORWARD:
        forward = TRUE;
        break;
      case GTK_DIR_TAB_BACKWARD:
        forward = FALSE;
        break;
      case GTK_DIR_LEFT:
        forward = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
        break;
      case GTK_DIR_RIGHT:
        forward = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
        break;
      case GTK_DIR_UP:
      case GTK_DIR_DOWN:
        return NULL;
      default:
        g_return_val_if_reached (NULL);
    }

  if (forward)
    {
      if (current == NULL)
        return widget;
      else if (current == widget)
        return gtk_widget_get_first_child (widget);
      else
        return gtk_widget_get_next_sibling (current);
    }
  else
    {
      if (current == NULL)
        return gtk_widget_get_last_child (widget);
      else if (current == widget)
        return NULL;
      else
        {
          current = gtk_widget_get_prev_sibling (current);
          if (current)
            return current;
          else
            return widget;
        }
    }
}

static gboolean
gtk_column_view_row_widget_focus (GtkWidget        *widget,
                                  GtkDirectionType  direction)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);
  GtkWidget *child, *current;
  GtkColumnView *view;

  current = gtk_widget_get_focus_child (widget);

  view = gtk_column_view_row_widget_get_column_view (self);
  if (gtk_column_view_get_tab_behavior (view) == GTK_LIST_TAB_CELL &&
      (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD))
    {
      if (current || gtk_widget_is_focus (widget))
        return FALSE;
    }

  if (current == NULL)
    {
      GtkColumnViewColumn *focus_column = gtk_column_view_get_focus_column (view);
      if (focus_column)
        {
          current = gtk_column_view_row_widget_find_child (self, focus_column);
          if (current && gtk_widget_child_focus (current, direction))
            return TRUE;
        }
    }

  if (gtk_widget_is_focus (widget))
    current = widget;

  for (child = gtk_column_view_next_focus_widget (widget, current, direction);
       child;
       child = gtk_column_view_next_focus_widget (widget, child, direction))
    {
      if (child == widget)
        {
          if (gtk_widget_grab_focus_self (widget))
            {
              gtk_column_view_set_focus_column (view, NULL, FALSE);
              return TRUE;
            }
        }
      else if (child)
        {
          if (gtk_widget_child_focus (child, direction))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_column_view_row_widget_grab_focus (GtkWidget *widget)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);
  GtkWidget *child, *focus_child;
  GtkColumnViewColumn *focus_column;
  GtkColumnView *view;

  view = gtk_column_view_row_widget_get_column_view (self);
  focus_column = gtk_column_view_get_focus_column (view);
  if (focus_column)
    {
      focus_child = gtk_column_view_row_widget_find_child (self, focus_column);
      if (focus_child && gtk_widget_grab_focus (focus_child))
        return TRUE;
    }
  else
    focus_child = NULL;

  if (gtk_widget_grab_focus_self (widget))
    {
      gtk_column_view_set_focus_column (view, NULL, FALSE);
      return TRUE;
    }

  for (child = focus_child ? gtk_widget_get_next_sibling (focus_child) : gtk_widget_get_first_child (widget);
       child != focus_child;
       child = child ? gtk_widget_get_next_sibling (child) : gtk_widget_get_first_child (widget))
    {
      /* When we started iterating at focus_child, we want to iterate over the rest
       * of the children, too */
      if (child == NULL)
        continue;

      if (gtk_widget_grab_focus (child))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_column_view_row_widget_set_focus_child (GtkWidget *widget,
                                            GtkWidget *child)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->set_focus_child (widget, child);

  if (child)
    {
      gtk_column_view_set_focus_column (gtk_column_view_row_widget_get_column_view (self),
                                        gtk_column_view_row_child_get_column (child),
                                        TRUE);
    }
}

static void
gtk_column_view_row_widget_dispose (GObject *object)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    {
      gtk_column_view_row_widget_remove_child (self, child);
    }

  G_OBJECT_CLASS (gtk_column_view_row_widget_parent_class)->dispose (object);
}

static void
gtk_column_view_row_widget_measure_along (GtkColumnViewRowWidget *self,
                                          int                     for_size,
                                          int                    *minimum,
                                          int                    *natural,
                                          int                    *minimum_baseline,
                                          int                    *natural_baseline)
{
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
  GtkColumnView *view;
  GtkWidget *child;
  guint i, n;
  GtkRequestedSize *sizes = NULL;

  view = gtk_column_view_row_widget_get_column_view (self);

  if (for_size > -1)
    {
      n = g_list_model_get_n_items (gtk_column_view_get_columns (view));
      sizes = g_newa (GtkRequestedSize, n);
      gtk_column_view_distribute_width (view, for_size, sizes);
    }

  for (child = _gtk_widget_get_first_child (GTK_WIDGET (self)), i = 0;
       child != NULL;
       child = _gtk_widget_get_next_sibling (child), i++)
    {
      int child_min = 0;
      int child_nat = 0;
      int child_min_baseline = -1;
      int child_nat_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation,
                          for_size > -1 ? sizes[i].minimum_size : -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);

      if (child_min_baseline > -1)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (child_nat_baseline > -1)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
}

static void
gtk_column_view_row_widget_measure (GtkWidget      *widget,
                                    GtkOrientation  orientation,
                                    int             for_size,
                                    int            *minimum,
                                    int            *natural,
                                    int            *minimum_baseline,
                                    int            *natural_baseline)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_column_view_measure_across (gtk_column_view_row_widget_get_column_view (self),
                                      minimum,
                                      natural);
    }
  else
    {
      gtk_column_view_row_widget_measure_along (self,
                                                for_size,
                                                minimum,
                                                natural,
                                                minimum_baseline,
                                                natural_baseline);
    }
}

static void
gtk_column_view_row_widget_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkColumnViewColumn *column;
      int col_x, col_width, min;

      if (!gtk_widget_should_layout (child))
        continue;

      column = gtk_column_view_row_child_get_column (child);
      gtk_column_view_column_get_header_allocation (column, &col_x, &col_width);

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

      gtk_widget_size_allocate (child, &(GtkAllocation) { col_x, 0, MAX (min, col_width), height }, baseline);
    }
}

static void
add_arrow_bindings (GtkWidgetClass   *widget_class,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_widget_class_add_binding_signal (widget_class, keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
}

static void
gtk_column_view_row_widget_class_init (GtkColumnViewRowWidgetClass *klass)
{
  GtkListFactoryWidgetClass *factory_class = GTK_LIST_FACTORY_WIDGET_CLASS (klass);
  GtkListItemBaseClass *base_class = GTK_LIST_ITEM_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  factory_class->create_object = gtk_column_view_row_widget_create_object;
  factory_class->setup_object = gtk_column_view_row_widget_setup_object;
  factory_class->update_object = gtk_column_view_row_widget_update_object;
  factory_class->teardown_object = gtk_column_view_row_widget_teardown_object;

  base_class->update = gtk_column_view_row_widget_update;

  widget_class->focus = gtk_column_view_row_widget_focus;
  widget_class->grab_focus = gtk_column_view_row_widget_grab_focus;
  widget_class->set_focus_child = gtk_column_view_row_widget_set_focus_child;
  widget_class->measure = gtk_column_view_row_widget_measure;
  widget_class->size_allocate = gtk_column_view_row_widget_allocate;

  object_class->dispose = gtk_column_view_row_widget_dispose;

  add_arrow_bindings (widget_class, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (widget_class, GDK_KEY_Right, GTK_DIR_RIGHT);

  /* This gets overwritten by gtk_column_view_row_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, g_intern_static_string ("row"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_ROW);
}

static void
gtk_column_view_row_widget_init (GtkColumnViewRowWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

GtkWidget *
gtk_column_view_row_widget_new (GtkListItemFactory *factory,
                                gboolean            is_header)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_ROW_WIDGET,
                       "factory", factory,
                       "css-name", is_header ? "header" : "row",
                       "selectable", TRUE,
                       "activatable", TRUE,
                       NULL);
}

void
gtk_column_view_row_widget_add_child (GtkColumnViewRowWidget *self,
                                      GtkWidget              *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_column_view_row_widget_reorder_child (GtkColumnViewRowWidget *self,
                                          GtkWidget              *child,
                                          guint                   position)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *sibling = NULL;

  if (position > 0)
    {
      GtkWidget *c;
      guint i;

      for (c = gtk_widget_get_first_child (widget), i = 0;
           c;
           c = gtk_widget_get_next_sibling (c), i++)
        {
          if (i + 1 == position)
            {
              sibling = c;
              break;
            }
        }
    }

  if (child != sibling)
    gtk_widget_insert_after (child, widget, sibling);
}

void
gtk_column_view_row_widget_remove_child (GtkColumnViewRowWidget *self,
                                         GtkWidget              *child)
{
  if (GTK_IS_COLUMN_VIEW_CELL_WIDGET (child))
    gtk_column_view_cell_widget_unset_column (GTK_COLUMN_VIEW_CELL_WIDGET (child));

  gtk_widget_unparent (child);
}

