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

#include "gtkcolumnviewrowprivate.h"
#include "gtkaccessible.h"


/**
 * GtkColumnViewRow:
 *
 * `GtkColumnViewRow` is used by [class@Gtk.ColumnView] to allow configuring
 * how rows are displayed.
 *
 * It is not used to set the widgets displayed in the individual cells. For that
 * see [method@GtkColumnViewColumn.set_factory] and [class@GtkColumnViewCell].
 *
 * Since: 4.12
 */

struct _GtkColumnViewRowClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_ACCESSIBLE_DESCRIPTION,
  PROP_ACCESSIBLE_LABEL,
  PROP_ACTIVATABLE,
  PROP_FOCUSABLE,
  PROP_ITEM,
  PROP_POSITION,
  PROP_SELECTABLE,
  PROP_SELECTED,

  N_PROPS
};

G_DEFINE_TYPE (GtkColumnViewRow, gtk_column_view_row, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_column_view_row_dispose (GObject *object)
{
  GtkColumnViewRow *self = GTK_COLUMN_VIEW_ROW (object);

  g_clear_pointer (&self->accessible_description, g_free);
  g_clear_pointer (&self->accessible_label, g_free);

  G_OBJECT_CLASS (gtk_column_view_row_parent_class)->dispose (object);
}

static void
gtk_column_view_row_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkColumnViewRow *self = GTK_COLUMN_VIEW_ROW (object);

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
gtk_column_view_row_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkColumnViewRow *self = GTK_COLUMN_VIEW_ROW (object);

  switch (property_id)
    {
    case PROP_ACCESSIBLE_DESCRIPTION:
      gtk_column_view_row_set_accessible_description (self, g_value_get_string (value));
      break;

    case PROP_ACCESSIBLE_LABEL:
      gtk_column_view_row_set_accessible_label (self, g_value_get_string (value));
      break;

    case PROP_ACTIVATABLE:
      gtk_column_view_row_set_activatable (self, g_value_get_boolean (value));
      break;

    case PROP_FOCUSABLE:
      gtk_column_view_row_set_focusable (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTABLE:
      gtk_column_view_row_set_selectable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_row_class_init (GtkColumnViewRowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_row_dispose;
  gobject_class->get_property = gtk_column_view_row_get_property;
  gobject_class->set_property = gtk_column_view_row_set_property;

  /**
   * GtkColumnViewRow:accessible-description: (attributes org.gtk.Property.get=gtk_column_view_row_get_accessible_description org.gtk.Property.set=gtk_column_view_row_set_accessible_description)
   *
   * The accessible description to set on the row.
   *
   * Since: 4.12
   */
  properties[PROP_ACCESSIBLE_DESCRIPTION] =
    g_param_spec_string ("accessible-description", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:accessible-label: (attributes org.gtk.Property.get=gtk_column_view_row_get_accessible_label org.gtk.Property.set=gtk_column_view_row_set_accessible_label)
   *
   * The accessible label to set on the row.
   *
   * Since: 4.12
   */
  properties[PROP_ACCESSIBLE_LABEL] =
    g_param_spec_string ("accessible-label", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:activatable: (attributes org.gtk.Property.get=gtk_column_view_row_get_activatable org.gtk.Property.set=gtk_column_view_row_set_activatable)
   *
   * If the row can be activated by the user.
   * 
   * Since: 4.12
   */
  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:focusable: (attributes org.gtk.Property.get=gtk_column_view_row_get_focusable org.gtk.Property.set=gtk_column_view_row_set_focusable)
   *
   * If the row can be focused with the keyboard.
   *
   * Since: 4.12
   */
  properties[PROP_FOCUSABLE] =
    g_param_spec_boolean ("focusable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:item: (attributes org.gtk.Property.get=gtk_column_view_row_get_item)
   *
   * The item for this row.
   *
   * Since: 4.12
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:position: (attributes org.gtk.Property.get=gtk_column_view_row_get_position)
   *
   * Position of the row.
   *
   * Since: 4.12
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:selectable: (attributes org.gtk.Property.get=gtk_column_view_row_get_selectable org.gtk.Property.set=gtk_column_view_row_set_selectable)
   *
   * If the row can be selected by the user.
   *
   * Since: 4.12
   */
  properties[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewRow:selected: (attributes org.gtk.Property.get=gtk_column_view_row_get_selected)
   *
   * If the item in the row is currently selected.
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
gtk_column_view_row_init (GtkColumnViewRow *self)
{
  self->selectable = TRUE;
  self->activatable = TRUE;
  self->focusable = TRUE;
}

GtkColumnViewRow *
gtk_column_view_row_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_ROW, NULL);
}

void
gtk_column_view_row_do_notify (GtkColumnViewRow *column_view_row,
                               gboolean          notify_item,
                               gboolean          notify_position,
                               gboolean          notify_selected)
{
  GObject *object = G_OBJECT (column_view_row);

  if (notify_item)
    g_object_notify_by_pspec (object, properties[PROP_ITEM]);
  if (notify_position)
    g_object_notify_by_pspec (object, properties[PROP_POSITION]);
  if (notify_selected)
    g_object_notify_by_pspec (object, properties[PROP_SELECTED]);
}

/**
 * gtk_column_view_row_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkColumnViewRow`
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
gtk_column_view_row_get_item (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), NULL);

  if (self->owner == NULL)
    return NULL;

  return gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->owner));
}

/**
 * gtk_column_view_row_get_position: (attributes org.gtk.Method.get_property=position)
 * @self: a `GtkColumnViewRow`
 *
 * Gets the position in the model that @self currently displays.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The position of this row 
 *
 * Since: 4.12
 */
guint
gtk_column_view_row_get_position (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), GTK_INVALID_LIST_POSITION);

  if (self->owner == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->owner));
}

/**
 * gtk_column_view_row_get_selected: (attributes org.gtk.Method.get_property=selected)
 * @self: a `GtkColumnViewRow`
 *
 * Checks if the item is selected that this row corresponds to.
 *
 * The selected state is maintained by the list widget and its model
 * and cannot be set otherwise.
 *
 * Returns: %TRUE if the item is selected.
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_row_get_selected (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), FALSE);

  if (self->owner == NULL)
    return FALSE;

  return gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self->owner));
}

/**
 * gtk_column_view_row_get_selectable: (attributes org.gtk.Method.get_property=selectable)
 * @self: a `GtkColumnViewRow`
 *
 * Checks if the row has been set to be selectable via
 * gtk_column_view_row_set_selectable().
 *
 * Do not confuse this function with [method@Gtk.ColumnViewRow.get_selected].
 *
 * Returns: %TRUE if the row is selectable
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_row_get_selectable (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), FALSE);

  return self->selectable;
}

/**
 * gtk_column_view_row_set_selectable: (attributes org.gtk.Method.set_property=selectable)
 * @self: a `GtkColumnViewRow`
 * @selectable: if the row should be selectable
 *
 * Sets @self to be selectable.
 *
 * If a row is selectable, clicking on the row or using the keyboard
 * will try to select or unselect the row. Whether this succeeds is up to
 * the model to determine, as it is managing the selected state.
 *
 * Note that this means that making a row non-selectable has no
 * influence on the selected state at all. A non-selectable row
 * may still be selected.
 *
 * By default, rows are selectable.
 *
 * Since: 4.12
 */
void
gtk_column_view_row_set_selectable (GtkColumnViewRow *self,
                                    gboolean          selectable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_ROW (self));

  if (self->selectable == selectable)
    return;

  self->selectable = selectable;

  if (self->owner)
    gtk_list_factory_widget_set_selectable (GTK_LIST_FACTORY_WIDGET (self->owner), selectable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTABLE]);
}

/**
 * gtk_column_view_row_get_activatable: (attributes org.gtk.Method.get_property=activatable)
 * @self: a `GtkColumnViewRow`
 *
 * Checks if the row has been set to be activatable via
 * gtk_column_view_row_set_activatable().
 *
 * Returns: %TRUE if the row is activatable
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_row_get_activatable (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), FALSE);

  return self->activatable;
}

/**
 * gtk_column_view_row_set_activatable: (attributes org.gtk.Method.set_property=activatable)
 * @self: a `GtkColumnViewRow`
 * @activatable: if the row should be activatable
 *
 * Sets @self to be activatable.
 *
 * If a row is activatable, double-clicking on the row, using
 * the Return key or calling gtk_widget_activate() will activate
 * the row. Activating instructs the containing columnview to 
 * emit the [signal@Gtk.ColumnView::activate] signal.
 *
 * By default, row are activatable.
 *
 * Since: 4.12
 */
void
gtk_column_view_row_set_activatable (GtkColumnViewRow *self,
                                     gboolean          activatable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_ROW (self));

  if (self->activatable == activatable)
    return;

  self->activatable = activatable;

  if (self->owner)
    gtk_list_factory_widget_set_activatable (GTK_LIST_FACTORY_WIDGET (self->owner), activatable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATABLE]);
}

/**
 * gtk_column_view_row_get_focusable: (attributes org.gtk.Method.get_property=focusable)
 * @self: a `GtkColumnViewRow`
 *
 * Checks if a row item has been set to be focusable via
 * gtk_column_view_row_set_focusable().
 *
 * Returns: %TRUE if the row is focusable
 *
 * Since: 4.12
 */
gboolean
gtk_column_view_row_get_focusable (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), FALSE);

  return self->focusable;
}

/**
 * gtk_column_view_row_set_focusable: (attributes org.gtk.Method.set_property=focusable)
 * @self: a `GtkColumnViewRow`
 * @focusable: if the row should be focusable
 *
 * Sets @self to be focusable.
 *
 * If a row is focusable, it can be focused using the keyboard.
 * This works similar to [method@Gtk.Widget.set_focusable].
 *
 * Note that if row are not focusable, the contents of cells can still be focused if
 * they are focusable.
 *
 * By default, rows are focusable.
 *
 * Since: 4.12
 */
void
gtk_column_view_row_set_focusable (GtkColumnViewRow *self,
                                   gboolean          focusable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_ROW (self));

  if (self->focusable == focusable)
    return;

  self->focusable = focusable;

  if (self->owner)
    gtk_widget_set_focusable (GTK_WIDGET (self->owner), focusable);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOCUSABLE]);
}

/**
 * gtk_column_view_row_get_accessible_description:
 * @self: a `GtkColumnViewRow`
 *
 * Gets the accessible description of @self.
 *
 * Returns: the accessible description
 *
 * Since: 4.12
 */
const char *
gtk_column_view_row_get_accessible_description (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), NULL);

  return self->accessible_description;
}

/**
 * gtk_column_view_row_set_accessible_description:
 * @self: a `GtkColumnViewRow`
 * @description: the description
 *
 * Sets the accessible description for the row,
 * which may be used by e.g. screen readers.
 *
 * Since: 4.12
 */
void
gtk_column_view_row_set_accessible_description (GtkColumnViewRow *self,
                                                const char       *description)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_ROW (self));

  if (!g_set_str (&self->accessible_description, description))
    return;

  if (self->owner)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->owner),
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, self->accessible_description,
                                    -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCESSIBLE_DESCRIPTION]);
}

/**
 * gtk_column_view_row_get_accessible_label:
 * @self: a `GtkColumnViewRow`
 *
 * Gets the accessible label of @self.
 *
 * Returns: the accessible label
 *
 * Since: 4.12
 */
const char *
gtk_column_view_row_get_accessible_label (GtkColumnViewRow *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_ROW (self), NULL);

  return self->accessible_label;
}

/**
 * gtk_column_view_row_set_accessible_label:
 * @self: a `GtkColumnViewRow`
 * @label: the label
 *
 * Sets the accessible label for the row,
 * which may be used by e.g. screen readers.
 *
 * Since: 4.12
 */
void
gtk_column_view_row_set_accessible_label (GtkColumnViewRow *self,
                                    const char  *label)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_ROW (self));

  if (!g_set_str (&self->accessible_label, label))
    return;

  if (self->owner)
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->owner),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL, self->accessible_label,
                                    -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCESSIBLE_LABEL]);
}
