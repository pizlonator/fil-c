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

#include "gtkcolumnviewcellprivate.h"

#include "gtklistitembaseprivate.h"

/**
 * GtkColumnViewCell:
 *
 * `GtkColumnViewCell` is used by [class@Gtk.ColumnViewColumn] to represent items
 * in a cell in [class@Gtk.ColumnView].
 *
 * The `GtkColumnViewCell`s are managed by the columnview widget (with its factory)
 * and cannot be created by applications, but they need to be populated
 * by application code. This is done by calling [method@Gtk.ColumnViewCell.set_child].
 *
 * `GtkColumnViewCell`s exist in 2 stages:
 *
 * 1. The unbound stage where the listitem is not currently connected to
 *    an item in the list. In that case, the [property@Gtk.ColumnViewCell:item]
 *    property is set to %NULL.
 *
 * 2. The bound stage where the listitem references an item from the list.
 *    The [property@Gtk.ColumnViewCell:item] property is not %NULL.
 *
 * Since: 4.12
 */

struct _GtkColumnViewCellClass
{
  GtkListItemClass parent_class;
};

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_FOCUSABLE,
  PROP_ITEM,
  PROP_POSITION,
  PROP_SELECTED,

  N_PROPS
};

G_DEFINE_TYPE (GtkColumnViewCell, gtk_column_view_cell, GTK_TYPE_LIST_ITEM)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_column_view_cell_dispose (GObject *object)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (object);

  g_assert (self->cell == NULL); /* would hold a reference */
  g_clear_object (&self->child);

  G_OBJECT_CLASS (gtk_column_view_cell_parent_class)->dispose (object);
}

static void
gtk_column_view_cell_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    case PROP_FOCUSABLE:
      g_value_set_boolean (value, self->focusable);
      break;

    case PROP_ITEM:
      if (self->cell)
        g_value_set_object (value, gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->cell)));
      break;

    case PROP_POSITION:
      if (self->cell)
        g_value_set_uint (value, gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->cell)));
      else
        g_value_set_uint (value, GTK_INVALID_LIST_POSITION);
      break;

    case PROP_SELECTED:
      if (self->cell)
        g_value_set_boolean (value, gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self->cell)));
      else
        g_value_set_boolean (value, FALSE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_cell_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_column_view_cell_set_child (self, g_value_get_object (value));
      break;

    case PROP_FOCUSABLE:
      gtk_column_view_cell_set_focusable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_cell_class_init (GtkColumnViewCellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_cell_dispose;
  gobject_class->get_property = gtk_column_view_cell_get_property;
  gobject_class->set_property = gtk_column_view_cell_set_property;

  /**
   * GtkColumnViewCell:child: (attributes org.gtk.Property.get=gtk_column_view_cell_get_child org.gtk.Property.set=gtk_column_view_cell_set_child)
   *
   * Widget used for display.
   *
   * Since: 4.12
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewCell:focusable: (attributes org.gtk.Property.get=gtk_column_view_cell_get_focusable org.gtk.Property.set=gtk_column_view_cell_set_focusable)
   *
   * If the item can be focused with the keyboard.
   *
   * Since: 4.12
   */
  properties[PROP_FOCUSABLE] =
    g_param_spec_boolean ("focusable", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewCell:item: (attributes org.gtk.Property.get=gtk_column_view_cell_get_item)
   *
   * Displayed item.
   *
   * Since: 4.12
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewCell:position: (attributes org.gtk.Property.get=gtk_column_view_cell_get_position)
   *
   * Position of the item.
   *
   * Since: 4.12
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewCell:selected: (attributes org.gtk.Property.get=gtk_column_view_cell_get_selected)
   *
   * If the item is currently selected.
   *
   * Since: 4.12
   */
  properties[PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_column_view_cell_init (GtkColumnViewCell *self)
{
  self->focusable = FALSE;
}

GtkColumnViewCell *
gtk_column_view_cell_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_CELL, NULL);
}

void
gtk_column_view_cell_do_notify (GtkColumnViewCell *column_view_cell,
                                gboolean notify_item,
                                gboolean notify_position,
                                gboolean notify_selected)
{
  GObject *object = G_OBJECT (column_view_cell);

  if (notify_item)
    g_object_notify_by_pspec (object, properties[PROP_ITEM]);
  if (notify_position)
    g_object_notify_by_pspec (object, properties[PROP_POSITION]);
  if (notify_selected)
    g_object_notify_by_pspec (object, properties[PROP_SELECTED]);
}

/**
 * gtk_column_view_cell_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkColumnViewCell`
 *
 * Gets the model item that associated with @self.
 *
 * If @self is unbound, this function returns %NULL.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 *
 * Since: 4.12
 **/
gpointer
gtk_column_view_cell_get_item (GtkColumnViewCell *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_CELL (self), NULL);

  if (self->cell == NULL)
    return NULL;

  return gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->cell));
}

/**
 * gtk_column_view_cell_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkColumnViewCell`
 *
 * Gets the child previously set via gtk_column_view_cell_set_child() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The child
 *
 * Since: 4.12
 */
GtkWidget *
gtk_column_view_cell_get_child (GtkColumnViewCell *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_CELL (self), NULL);

  return self->child;
}

/**
 * gtk_column_view_cell_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkColumnViewCell`
 * @child: (nullable): The list item's child or %NULL to unset
 *
 * Sets the child to be used for this listitem.
 *
 * This function is typically called by applications when
 * setting up a listitem so that the widget can be reused when
 * binding it multiple times.
 *
 * Since: 4.12
 */
void
gtk_column_view_cell_set_child (GtkColumnViewCell *self,
                                GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_CELL (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  g_clear_object (&self->child);

  if (child)
    {
      g_object_ref_sink (child);
      self->child = child;
    }

  if (self->cell)
    gtk_column_view_cell_widget_set_child (self->cell, child);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_column_view_cell_get_position: (attributes org.gtk.Method.get_property=position)
 * @self: a `GtkColumnViewCell`
 *
 * Gets the position in the model that @self currently displays.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The position of this item
 *
 * Since: 4.12
 */
guint
gtk_column_view_cell_get_position (GtkColumnViewCell *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_CELL (self), GTK_INVALID_LIST_POSITION);

  if (self->cell == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->cell));
}

/**
 * gtk_column_view_cell_get_selected: (attributes org.gtk.Method.get_property=selected)
 * @self: a `GtkColumnViewCell`
 *
 * Checks if the item is displayed as selected.
 *
 * The selected state is maintained by the liste widget and its model
 * and cannot be set otherwise.
 *
 * Returns: %TRUE if the item is selected.
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_cell_get_selected (GtkColumnViewCell *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_CELL (self), FALSE);

  if (self->cell == NULL)
    return FALSE;

  return gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self->cell));
}

/**
 * gtk_column_view_cell_get_focusable: (attributes org.gtk.Method.get_property=focusable)
 * @self: a `GtkColumnViewCell`
 *
 * Checks if a list item has been set to be focusable via
 * gtk_column_view_cell_set_focusable().
 *
 * Returns: %TRUE if the item is focusable
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_cell_get_focusable (GtkColumnViewCell *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_CELL (self), FALSE);

  return self->focusable;
}

/**
 * gtk_column_view_cell_set_focusable: (attributes org.gtk.Method.set_property=focusable)
 * @self: a `GtkColumnViewCell`
 * @focusable: if the item should be focusable
 *
 * Sets @self to be focusable.
 *
 * If an item is focusable, it can be focused using the keyboard.
 * This works similar to [method@Gtk.Widget.set_focusable].
 *
 * Note that if items are not focusable, the keyboard cannot be used to activate
 * them and selecting only works if one of the listitem's children is focusable.
 *
 * By default, list items are focusable.
 *
 * Since: 4.12
 */
void
gtk_column_view_cell_set_focusable (GtkColumnViewCell *self,
                                    gboolean     focusable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_CELL (self));

  if (self->focusable == focusable)
    return;

  self->focusable = focusable;

  if (self->cell)
    gtk_widget_set_focusable (GTK_WIDGET (self->cell), focusable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOCUSABLE]);
}
