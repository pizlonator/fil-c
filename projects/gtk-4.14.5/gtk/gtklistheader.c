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

#include "gtklistheaderprivate.h"

/**
 * GtkListHeader:
 *
 * `GtkListHeader` is used by list widgets to represent the headers they
 * display.
 *
 * `GtkListHeader` objects are managed just like [class@Gtk.ListItem]
 * objects via their factory, but provide a different set of properties suitable
 * for managing the header instead of individual items.
 *
 * Since: 4.12
 */

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_END,
  PROP_ITEM,
  PROP_N_ITEMS,
  PROP_START,

  N_PROPS
};

G_DEFINE_TYPE (GtkListHeader, gtk_list_header, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_list_header_dispose (GObject *object)
{
  GtkListHeader *self = GTK_LIST_HEADER (object);

  g_assert (self->owner == NULL); /* would hold a reference */
  g_clear_object (&self->child);

  G_OBJECT_CLASS (gtk_list_header_parent_class)->dispose (object);
}

static void
gtk_list_header_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkListHeader *self = GTK_LIST_HEADER (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    case PROP_END:
      if (self->owner)
        g_value_set_uint (value, gtk_list_header_base_get_end (GTK_LIST_HEADER_BASE (self->owner)));
      else
        g_value_set_uint (value, GTK_INVALID_LIST_POSITION);
      break;

    case PROP_ITEM:
      if (self->owner)
        g_value_set_object (value, gtk_list_header_base_get_item (GTK_LIST_HEADER_BASE (self->owner)));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_list_header_get_n_items (self));
      break;

    case PROP_START:
      if (self->owner)
        g_value_set_uint (value, gtk_list_header_base_get_start (GTK_LIST_HEADER_BASE (self->owner)));
      else
        g_value_set_uint (value, GTK_INVALID_LIST_POSITION);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_header_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkListHeader *self = GTK_LIST_HEADER (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_list_header_set_child (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_header_class_init (GtkListHeaderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_list_header_dispose;
  gobject_class->get_property = gtk_list_header_get_property;
  gobject_class->set_property = gtk_list_header_set_property;

  /**
   * GtkListHeader:child: (attributes org.gtk.Property.get=gtk_list_header_get_child org.gtk.Property.set=gtk_list_header_set_child)
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
   * GtkListHeader:end: (attributes org.gtk.Property.get=gtk_list_header_get_end)
   *
   * The first position no longer part of this section.
   *
   * Since: 4.12
   */
  properties[PROP_END] =
    g_param_spec_uint ("end", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListHeader:item: (attributes org.gtk.Property.get=gtk_list_header_get_item)
   *
   * The item at the start of the section.
   *
   * Since: 4.12
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListHeader:n-items: (attributes org.gtk.Property.get=gtk_list_header_get_n_items)
   *
   * Number of items in this section.
   *
   * Since: 4.12
   */
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListHeader:start: (attributes org.gtk.Property.get=gtk_list_header_get_start)
   *
   * First position of items in this section.
   *
   * Since: 4.12
   */
  properties[PROP_START] =
    g_param_spec_uint ("start", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_list_header_init (GtkListHeader *self)
{
}

GtkListHeader *
gtk_list_header_new (void)
{
  return g_object_new (GTK_TYPE_LIST_HEADER, NULL);
}

void
gtk_list_header_do_notify (GtkListHeader *list_header,
                           gboolean notify_item,
                           gboolean notify_start,
                           gboolean notify_end,
                           gboolean notify_n_items)
{
  GObject *object = G_OBJECT (list_header);

  if (notify_item)
    g_object_notify_by_pspec (object, properties[PROP_ITEM]);
  if (notify_start)
    g_object_notify_by_pspec (object, properties[PROP_START]);
  if (notify_end)
    g_object_notify_by_pspec (object, properties[PROP_END]);
  if (notify_n_items)
    g_object_notify_by_pspec (object, properties[PROP_N_ITEMS]);
}

/**
 * gtk_list_header_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkListHeader`
 *
 * Gets the model item at the start of the section.
 * This is the item that occupies the list model at position
 * [property@Gtk.ListHeader:start].
 *
 * If @self is unbound, this function returns %NULL.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 *
 * Since: 4.12
 **/
gpointer
gtk_list_header_get_item (GtkListHeader *self)
{
  g_return_val_if_fail (GTK_IS_LIST_HEADER (self), NULL);

  if (self->owner)
    return gtk_list_header_base_get_item (GTK_LIST_HEADER_BASE (self->owner));
  else
    return NULL;
}

/**
 * gtk_list_header_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkListHeader`
 *
 * Gets the child previously set via gtk_list_header_set_child() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The child
 *
 * Since: 4.12
 */
GtkWidget *
gtk_list_header_get_child (GtkListHeader *self)
{
  g_return_val_if_fail (GTK_IS_LIST_HEADER (self), NULL);

  return self->child;
}

/**
 * gtk_list_header_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkListHeader`
 * @child: (nullable): The list item's child or %NULL to unset
 *
 * Sets the child to be used for this listitem.
 *
 * This function is typically called by applications when
 * setting up a header so that the widget can be reused when
 * binding it multiple times.
 *
 * Since: 4.12
 */
void
gtk_list_header_set_child (GtkListHeader *self,
                           GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_LIST_HEADER (self));
  g_return_if_fail (child == NULL || gtk_widget_get_parent (child) == NULL);

  if (self->child == child)
    return;

  g_clear_object (&self->child);

  if (child)
    {
      g_object_ref_sink (child);
      self->child = child;
    }

  if (self->owner)
    gtk_list_header_widget_set_child (self->owner, child);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_list_header_get_start: (attributes org.gtk.Method.get_property=start)
 * @self: a `GtkListHeader`
 *
 * Gets the start position in the model of the section that @self is
 * currently the header for.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The start position of the section
 *
 * Since: 4.12
 */
guint
gtk_list_header_get_start (GtkListHeader *self)
{
  g_return_val_if_fail (GTK_IS_LIST_HEADER (self), GTK_INVALID_LIST_POSITION);

  if (self->owner)
    return gtk_list_header_base_get_start (GTK_LIST_HEADER_BASE (self->owner));
  else
    return GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_list_header_get_end: (attributes org.gtk.Method.get_property=end)
 * @self: a `GtkListHeader`
 *
 * Gets the end position in the model of the section that @self is
 * currently the header for.
 *
 * If @self is unbound, %GTK_INVALID_LIST_POSITION is returned.
 *
 * Returns: The end position of the section
 *
 * Since: 4.12
 */
guint
gtk_list_header_get_end (GtkListHeader *self)
{
  g_return_val_if_fail (GTK_IS_LIST_HEADER (self), GTK_INVALID_LIST_POSITION);

  if (self->owner)
    return gtk_list_header_base_get_end (GTK_LIST_HEADER_BASE (self->owner));
  else
    return GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_list_header_get_n_items: (attributes org.gtk.Method.get_property=n-items)
 * @self: a `GtkListHeader`
 *
 * Gets the the number of items in the section.
 *
 * If @self is unbound, 0 is returned.
 *
 * Returns: The number of items in the section
 *
 * Since: 4.12
 */
guint
gtk_list_header_get_n_items (GtkListHeader *self)
{
  g_return_val_if_fail (GTK_IS_LIST_HEADER (self), GTK_INVALID_LIST_POSITION);

  if (self->owner)
    return gtk_list_header_base_get_end (GTK_LIST_HEADER_BASE (self->owner)) -
           gtk_list_header_base_get_start (GTK_LIST_HEADER_BASE (self->owner));
  else
    return 0;
}

