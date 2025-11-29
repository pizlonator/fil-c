/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtklistitemprivate.h"

#include "gtkcolumnviewcell.h"
#include "gtkaccessible.h"

/**
 * GtkListItem:
 *
 * `GtkListItem` is used by list widgets to represent items in a
 * [iface@Gio.ListModel].
 *
 * `GtkListItem` objects are managed by the list widget (with its factory)
 * and cannot be created by applications, but they need to be populated
 * by application code. This is done by calling [method@Gtk.ListItem.set_child].
 *
 * `GtkListItem` objects exist in 2 stages:
 *
 * 1. The unbound stage where the listitem is not currently connected to
 *    an item in the list. In that case, the [property@Gtk.ListItem:item]
 *    property is set to %NULL.
 *
 * 2. The bound stage where the listitem references an item from the list.
 *    The [property@Gtk.ListItem:item] property is not %NULL.
 */

enum
{
  PROP_0,
  PROP_ACCESSIBLE_DESCRIPTION,
  PROP_ACCESSIBLE_LABEL,
  PROP_ACTIVATABLE,
  PROP_CHILD,
  PROP_FOCUSABLE,
  PROP_ITEM,
  PROP_POSITION,
  PROP_SELECTABLE,
  PROP_SELECTED,

  N_PROPS
};

G_DEFINE_TYPE (GtkListItem, gtk_list_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_list_item_dispose (GObject *object)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  g_assert (self->owner == NULL); /* would hold a reference */
  g_clear_object (&self->child);

  g_clear_pointer (&self->accessible_description, g_free);
  g_clear_pointer (&self->accessible_label, g_free);

  G_OBJECT_CLASS (gtk_list_item_parent_class)->dispose (object);
}

static void
gtk_list_item_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    case PROP_ACCESSIBLE_DESCRIPTION:
      g_value_set_string (value, self->accessible_description);
      break;

    case PROP_ACCESSIBLE_LABEL:
      g_value_set_string (value, self->accessible_label);
      break;

    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, self->activatable);
      break;

    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    case PROP_FOCUSABLE:
      g_value_set_boolean (value, self->focusable);
      break;

    case PROP_ITEM:
      if (self->owner)
        g_value_set_object (value, gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->owner)));
      break;

    case PROP_POSITION:
      if (self->owner)
        g_value_set_uint (value, gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->owner)));
      else
        g_value_set_uint (value, GTK_INVALID_LIST_POSITION);
      break;

    case PROP_SELECTABLE:
      g_value_set_boolean (value, self->selectable);
      break;

    case PROP_SELECTED:
      if (self->owner)
        g_value_set_boolean (value, gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self->owner)));
      else
        g_value_set_boolean (value, FALSE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    case PROP_ACCESSIBLE_DESCRIPTION:
      gtk_list_item_set_accessible_description (self, g_value_get_string (value));
      break;

    case PROP_ACCESSIBLE_LABEL:
      gtk_list_item_set_accessible_label (self, g_value_get_string (value));
      break;

    case PROP_ACTIVATABLE:
      gtk_list_item_set_activatable (self, g_value_get_boolean (value));
      break;

    case PROP_CHILD:
      gtk_list_item_set_child (self, g_value_get_object (value));
      break;

    case PROP_FOCUSABLE:
      gtk_list_item_set_focusable (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTABLE:
      gtk_list_item_set_selectable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_class_init (GtkListItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_list_item_dispose;
  gobject_class->get_property = gtk_list_item_get_property;
  gobject_class->set_property = gtk_list_item_set_property;

  /**
   * GtkListItem:accessible-description: (attributes org.gtk.Property.get=gtk_list_item_get_accessible_description org.gtk.Property.set=gtk_list_item_set_accessible_description)
   *
   * The accessible description to set on the list item.
   *
   * Since: 4.12
   */
  properties[PROP_ACCESSIBLE_DESCRIPTION] =
    g_param_spec_string ("accessible-description", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:accessible-label: (attributes org.gtk.Property.get=gtk_list_item_get_accessible_label org.gtk.Property.set=gtk_list_item_set_accessible_label)
   *
   * The accessible label to set on the list item.
   *
   * Since: 4.12
   */
  properties[PROP_ACCESSIBLE_LABEL] =
    g_param_spec_string ("accessible-label", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:activatable: (attributes org.gtk.Property.get=gtk_list_item_get_activatable org.gtk.Property.set=gtk_list_item_set_activatable)
   *
   * If the item can be activated by the user.
   */
  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:child: (attributes org.gtk.Property.get=gtk_list_item_get_child org.gtk.Property.set=gtk_list_item_set_child)
   *
   * Widget used for display.
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:focusable: (attributes org.gtk.Property.get=gtk_list_item_get_focusable org.gtk.Property.set=gtk_list_item_set_focusable)
   *
   * If the item can be focused with the keyboard.
   *
   * Since: 4.12
   */
  properties[PROP_FOCUSABLE] =
    g_param_spec_boolean ("focusable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:item: (attributes org.gtk.Property.get=gtk_list_item_get_item)
   *
   * Displayed item.
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:position: (attributes org.gtk.Property.get=gtk_list_item_get_position)
   *
   * Position of the item.
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selectable: (attributes org.gtk.Property.get=gtk_list_item_get_selectable org.gtk.Property.set=gtk_list_item_set_selectable)
   *
   * If the item can be selected by the user.
   */
  properties[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:selected: (attributes org.gtk.Property.get=gtk_list_item_get_selected)
   *
   * If the item is currently selected.
   */
  properties[PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_list_item_init (GtkListItem *self)
{
  self->selectable = TRUE;
  self->activatable = TRUE;
  self->focusable = TRUE;
}

GtkListItem *
gtk_list_item_new (void)
{
  return g_object_new (GTK_TYPE_LIST_ITEM, NULL);
}

void
gtk_list_item_do_notify (GtkListItem *list_item,
                         gboolean notify_item,
                         gboolean notify_position,
                         gboolean notify_selected)
{
  GObject *object = G_OBJECT (list_item);

  if (notify_item)
    g_object_notify_by_pspec (object, properties[PROP_ITEM]);
  if (notify_position)
    g_object_notify_by_pspec (object, properties[PROP_POSITION]);
  if (notify_selected)
    g_object_notify_by_pspec (object, properties[PROP_SELECTED]);
}

/**
 * gtk_list_item_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkListItem`
 *
 * Gets the model item that associated with @self.
 *
 * If @self is unbound, this function returns %NULL.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 **/
gpointer
gtk_list_item_get_item (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  if (self->owner)
    return gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->owner));
  else if (GTK_IS_COLUMN_VIEW_CELL (self))
    return gtk_column_view_cell_get_item (GTK_COLUMN_VIEW_CELL (self));
  else
    return NULL;
}

/**
 * gtk_list_item_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkListItem`
 *
 * Gets the child previously set via gtk_list_item_set_child() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The child
 */
GtkWidget *
gtk_list_item_get_child (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  if (GTK_IS_COLUMN_VIEW_CELL (self))
    return gtk_column_view_cell_get_child (GTK_COLUMN_VIEW_CELL (self));

  return self->child;
}

/**
 * gtk_list_item_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkListItem`
 * @child: (nullable): The list item's child or %NULL to unset
 *
 * Sets the child to be used for this listitem.
 *
 * This function is typically called by applications when
 * setting up a listitem so that the widget can be reused when
 * binding it multiple times.
 */
void
gtk_list_item_set_child (GtkListItem *self,
                         GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));
  g_return_if_fail (child == NULL || gtk_widget_get_parent (child) == NULL);

  if (GTK_IS_COLUMN_VIEW_CELL (self))
    {
      gtk_column_view_cell_set_child (GTK_COLUMN_VIEW_CELL (self), child);
      return;
    }

  if (self->child == child)
    return;

  g_clear_object (&self->child);

  if (child)
    {
      g_object_ref_sink (child);
      self->child = child;

      /* Workaround that hopefully achieves good enough backwards
       * compatibility with people using expanders.
       */
      if (!self->focusable_set)
        gtk_list_item_set_focusable (self, !gtk_widget_get_focusable (child));
    }

  if (self->owner)
    gtk_list_item_widget_set_child (self->owner, child);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_list_item_get_position: (attributes org.gtk.Method.get_property=position)
 * @self: a `GtkListItem`
 *
 * Gets the position in the model that @self currently displays.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The position of this item
 */
guint
gtk_list_item_get_position (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), GTK_INVALID_LIST_POSITION);

  if (self->owner)
    return gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->owner));
  else if (GTK_IS_COLUMN_VIEW_CELL (self))
    return gtk_column_view_cell_get_position (GTK_COLUMN_VIEW_CELL (self));
  else
    return GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_list_item_get_selected: (attributes org.gtk.Method.get_property=selected)
 * @self: a `GtkListItem`
 *
 * Checks if the item is displayed as selected.
 *
 * The selected state is maintained by the liste widget and its model
 * and cannot be set otherwise.
 *
 * Returns: %TRUE if the item is selected.
 */
gboolean
gtk_list_item_get_selected (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  if (self->owner)
    return gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self->owner));
  else if (GTK_IS_COLUMN_VIEW_CELL (self))
    return gtk_column_view_cell_get_selected (GTK_COLUMN_VIEW_CELL (self));
  else
    return FALSE;
}

/**
 * gtk_list_item_get_selectable: (attributes org.gtk.Method.get_property=selectable)
 * @self: a `GtkListItem`
 *
 * Checks if a list item has been set to be selectable via
 * gtk_list_item_set_selectable().
 *
 * Do not confuse this function with [method@Gtk.ListItem.get_selected].
 *
 * Returns: %TRUE if the item is selectable
 */
gboolean
gtk_list_item_get_selectable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->selectable;
}

/**
 * gtk_list_item_set_selectable: (attributes org.gtk.Method.set_property=selectable)
 * @self: a `GtkListItem`
 * @selectable: if the item should be selectable
 *
 * Sets @self to be selectable.
 *
 * If an item is selectable, clicking on the item or using the keyboard
 * will try to select or unselect the item. If this succeeds is up to
 * the model to determine, as it is managing the selected state.
 *
 * Note that this means that making an item non-selectable has no
 * influence on the selected state at all. A non-selectable item
 * may still be selected.
 *
 * By default, list items are selectable. When rebinding them to
 * a new item, they will also be reset to be selectable by GTK.
 */
void
gtk_list_item_set_selectable (GtkListItem *self,
                              gboolean     selectable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->selectable == selectable)
    return;

  self->selectable = selectable;

  if (self->owner)
    gtk_list_factory_widget_set_selectable (GTK_LIST_FACTORY_WIDGET (self->owner), selectable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTABLE]);
}

/**
 * gtk_list_item_get_activatable: (attributes org.gtk.Method.get_property=activatable)
 * @self: a `GtkListItem`
 *
 * Checks if a list item has been set to be activatable via
 * gtk_list_item_set_activatable().
 *
 * Returns: %TRUE if the item is activatable
 */
gboolean
gtk_list_item_get_activatable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->activatable;
}

/**
 * gtk_list_item_set_activatable: (attributes org.gtk.Method.set_property=activatable)
 * @self: a `GtkListItem`
 * @activatable: if the item should be activatable
 *
 * Sets @self to be activatable.
 *
 * If an item is activatable, double-clicking on the item, using
 * the Return key or calling gtk_widget_activate() will activate
 * the item. Activating instructs the containing view to handle
 * activation. `GtkListView` for example will be emitting the
 * [signal@Gtk.ListView::activate] signal.
 *
 * By default, list items are activatable.
 */
void
gtk_list_item_set_activatable (GtkListItem *self,
                               gboolean     activatable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->activatable == activatable)
    return;

  self->activatable = activatable;

  if (self->owner)
    gtk_list_factory_widget_set_activatable (GTK_LIST_FACTORY_WIDGET (self->owner), activatable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATABLE]);
}

/**
 * gtk_list_item_get_focusable: (attributes org.gtk.Method.get_property=focusable)
 * @self: a `GtkListItem`
 *
 * Checks if a list item has been set to be focusable via
 * gtk_list_item_set_focusable().
 *
 * Returns: %TRUE if the item is focusable
 *
 * Since: 4.12
 */
gboolean
gtk_list_item_get_focusable (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), FALSE);

  return self->focusable;
}

/**
 * gtk_list_item_set_focusable: (attributes org.gtk.Method.set_property=focusable)
 * @self: a `GtkListItem`
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
gtk_list_item_set_focusable (GtkListItem *self,
                             gboolean     focusable)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  self->focusable_set = TRUE;

  if (self->focusable == focusable)
    return;

  self->focusable = focusable;

  if (self->owner)
    gtk_widget_set_focusable (GTK_WIDGET (self->owner), focusable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOCUSABLE]);
}

/**
 * gtk_list_item_set_accessible_description:
 * @self: a `GtkListItem`
 * @description: the description
 *
 * Sets the accessible description for the list item,
 * which may be used by e.g. screen readers.
 *
 * Since: 4.12
 */
void
gtk_list_item_set_accessible_description (GtkListItem *self,
                                          const char  *description)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (!g_set_str (&self->accessible_description, description))
    return;

  if (self->owner)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->owner),
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, self->accessible_description,
                                    -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCESSIBLE_DESCRIPTION]);
}

/**
 * gtk_list_item_get_accessible_description:
 * @self: a `GtkListItem`
 *
 * Gets the accessible description of @self.
 *
 * Returns: the accessible description
 *
 * Since: 4.12
 */
const char *
gtk_list_item_get_accessible_description (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->accessible_description;
}

/**
 * gtk_list_item_set_accessible_label:
 * @self: a `GtkListItem`
 * @label: the label
 *
 * Sets the accessible label for the list item,
 * which may be used by e.g. screen readers.
 *
 * Since: 4.12
 */
void
gtk_list_item_set_accessible_label (GtkListItem *self,
                                    const char  *label)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (!g_set_str (&self->accessible_label, label))
    return;

  if (self->owner)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->owner),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL, self->accessible_label,
                                    -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCESSIBLE_LABEL]);
}

/**
 * gtk_list_item_get_accessible_label:
 * @self: a `GtkListItem`
 *
 * Gets the accessible label of @self.
 *
 * Returns: the accessible label
 *
 * Since: 4.12
 */
const char *
gtk_list_item_get_accessible_label (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->accessible_label;
}
