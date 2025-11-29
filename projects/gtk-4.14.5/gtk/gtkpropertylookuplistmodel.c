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


/*
 * GtkPropertyLookupListModel:
 *
 * `GtkPropertyLookupListModel` is a `GListModel` implementation that takes an
 * object and a property and then recursively looks up the next element using
 * the property on the previous object.
 *
 * For example, one could use this list model with the GtkWidget:parent property
 * to get a list of a widgets and all its ancestors.
 **/

#include "config.h"

#include "gtkpropertylookuplistmodelprivate.h"

#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_N_ITEMS,
  PROP_OBJECT,
  PROP_PROPERTY,
  NUM_PROPERTIES
};

struct _GtkPropertyLookupListModel
{
  GObject parent_instance;

  GType item_type;
  char *property;
  GPtrArray *items; /* list of items - lazily expanded if there's a NULL sentinel */
};

struct _GtkPropertyLookupListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_property_lookup_list_model_get_item_type (GListModel *list)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (list);

  return self->item_type;
}

static gboolean
gtk_property_lookup_list_model_is_infinite (GtkPropertyLookupListModel *self)
{
  if (self->items->len == 0)
    return FALSE;

  return g_ptr_array_index (self->items, self->items->len - 1) == NULL;
}

static void
gtk_property_lookup_list_model_notify_cb (GObject                    *object,
                                          GParamSpec                 *pspec,
                                          GtkPropertyLookupListModel *self);

static guint
gtk_property_lookup_list_model_clear (GtkPropertyLookupListModel *self,
                                      guint                       remaining)
{
  guint i;

  for (i = remaining; i < self->items->len; i++)
    {
      gpointer object = g_ptr_array_index (self->items, i);
      if (object == NULL)
        break;

      g_signal_handlers_disconnect_by_func (object, gtk_property_lookup_list_model_notify_cb, self);
      g_object_unref (object);
    }

  /* keeps the sentinel, yay! */
  g_ptr_array_remove_range (self->items, remaining, i - remaining);

  return i - remaining;
}

static guint
gtk_property_lookup_list_model_append (GtkPropertyLookupListModel *self,
                                       guint                       n_items)
{
  gpointer last, next;
  guint i, start;

  g_assert (self->items->len > 0);
  g_assert (!gtk_property_lookup_list_model_is_infinite (self));

  last = g_ptr_array_index (self->items, self->items->len - 1);
  start = self->items->len;
  for (i = start; i < n_items; i++)
    {
      g_object_get (last, self->property, &next, NULL);
      if (next == NULL)
        return i - start;
      
      g_signal_connect_closure_by_id (next,
                                      g_signal_lookup ("notify", G_OBJECT_TYPE (next)),
                                      g_quark_from_static_string (self->property),
                                      g_cclosure_new (G_CALLBACK (gtk_property_lookup_list_model_notify_cb), G_OBJECT (self), NULL),
                                      FALSE);

      g_ptr_array_add (self->items, next);
      last = next;
    }

  return i - start;
}

static void
gtk_property_lookup_list_model_ensure (GtkPropertyLookupListModel *self,
                                       guint                       n_items)
{
  if (!gtk_property_lookup_list_model_is_infinite (self))
    return;

  if (self->items->len - 1 >= n_items)
    return;
  
  /* remove NULL sentinel */
  g_ptr_array_remove_index (self->items, self->items->len - 1);

  if (self->items->len == 0)
    return;

  if (gtk_property_lookup_list_model_append (self, n_items) == n_items)
    {
      /* re-add NULL sentinel */
      g_ptr_array_add (self->items, NULL);
    }
}

static void
gtk_property_lookup_list_model_notify_cb (GObject                    *object,
                                          GParamSpec                 *pspec,
                                          GtkPropertyLookupListModel *self)
{
  guint position, removed, added;

  if (!g_ptr_array_find (self->items, object, &position))
    {
      /* can only happen if we forgot to disconnect a signal handler */
      g_assert_not_reached ();
    }
  /* we found the position of the item that notified, but the first change
   * is its child.
   */
  position++;

  removed = gtk_property_lookup_list_model_clear (self, position);

  if (!gtk_property_lookup_list_model_is_infinite (self))
    added = gtk_property_lookup_list_model_append (self, G_MAXUINT);
  else
    added = 0;

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static guint
gtk_property_lookup_list_model_get_n_items (GListModel *list)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (list);

  gtk_property_lookup_list_model_ensure (self, G_MAXUINT);

  return self->items->len;
}

static gpointer
gtk_property_lookup_list_model_get_item (GListModel *list,
                                         guint       position)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (list);

  gtk_property_lookup_list_model_ensure (self, position + 1);

  if (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static void
gtk_property_lookup_list_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_property_lookup_list_model_get_item_type;
  iface->get_n_items = gtk_property_lookup_list_model_get_n_items;
  iface->get_item = gtk_property_lookup_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkPropertyLookupListModel, gtk_property_lookup_list_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_property_lookup_list_model_list_model_init))

static gboolean
check_pspec (GType       type,
             GParamSpec *pspec)
{
  if (pspec == NULL)
    return FALSE;

  if (!G_IS_PARAM_SPEC_OBJECT (pspec))
    return FALSE;

  if (!g_type_is_a (G_PARAM_SPEC (pspec)->value_type, type))
    return FALSE;

  return TRUE;
}

static gboolean
lookup_pspec (GType       type,
              const char *name)
{
  gboolean result;

  if (g_type_is_a (type, G_TYPE_INTERFACE))
    {
      gpointer iface;
      GParamSpec *pspec;

      iface = g_type_default_interface_ref (type);
      pspec = g_object_interface_find_property (iface, name);
      result = check_pspec (type, pspec);
      g_type_default_interface_unref (iface);
    }
  else
    {
      GObjectClass *klass;
      GParamSpec *pspec;

      klass = g_type_class_ref (type);
      g_return_val_if_fail (klass != NULL, FALSE);
      pspec = g_object_class_find_property (klass, name);
      result = check_pspec (type, pspec);
      g_type_class_unref (klass);
    }

  return result;
}

static void
gtk_property_lookup_list_model_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      self->item_type = g_value_get_gtype (value);
      g_return_if_fail (self->item_type != 0);
      break;

    case PROP_OBJECT:
      gtk_property_lookup_list_model_set_object (self, g_value_get_object (value));
      break;

    case PROP_PROPERTY:
      self->property = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  if (self->property && self->item_type &&
      !lookup_pspec (self->item_type, self->property))
    {
      g_critical ("type %s has no property named \"%s\"", g_type_name (self->item_type), self->property);
    }
}

static void 
gtk_property_lookup_list_model_get_property (GObject     *object,
                                             guint        prop_id,
                                             GValue      *value,
                                             GParamSpec  *pspec)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_property_lookup_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    case PROP_OBJECT:
      g_value_set_object (value, gtk_property_lookup_list_model_get_object (self));
      break;

    case PROP_PROPERTY:
      g_value_set_string (value, self->property);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_property_lookup_list_model_dispose (GObject *object)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (object);

  gtk_property_lookup_list_model_clear (self, 0);

  G_OBJECT_CLASS (gtk_property_lookup_list_model_parent_class)->dispose (object);
}

static void
gtk_property_lookup_list_model_finalize (GObject *object)
{
  GtkPropertyLookupListModel *self = GTK_PROPERTY_LOOKUP_LIST_MODEL (object);

  g_ptr_array_unref (self->items);
  g_free (self->property);

  G_OBJECT_CLASS (gtk_property_lookup_list_model_parent_class)->finalize (object);
}

static void
gtk_property_lookup_list_model_class_init (GtkPropertyLookupListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_property_lookup_list_model_get_property;
  object_class->set_property = gtk_property_lookup_list_model_set_property;
  object_class->dispose = gtk_property_lookup_list_model_dispose;
  object_class->finalize = gtk_property_lookup_list_model_finalize;

  /**
   * GtkPropertyLookupListModel:item-type:
   *
   * The `GType` for elements of this object. See [method@Gio.ListModel.get_item_type].
   */
  properties[PROP_ITEM_TYPE] =
      g_param_spec_gtype ("item-type", NULL, NULL,
                          G_TYPE_OBJECT,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPropertyLookupListModel:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkPropertyLookupListModel:property:
   *
   * Name of the property used for lookups
   */
  properties[PROP_PROPERTY] =
      g_param_spec_string ("property", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPropertyLookupListModel:object:
   *
   * The root object
   */
  properties[PROP_OBJECT] =
      g_param_spec_object ("object", NULL, NULL,
                           G_TYPE_OBJECT,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_property_lookup_list_model_init (GtkPropertyLookupListModel *self)
{
  self->item_type = G_TYPE_OBJECT;
  self->items = g_ptr_array_new ();
  /* add sentinel */
  g_ptr_array_add (self->items, NULL);
}

GtkPropertyLookupListModel *
gtk_property_lookup_list_model_new (GType       item_type,
                                    const char *property_name)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_object_new (GTK_TYPE_PROPERTY_LOOKUP_LIST_MODEL,
                       "item-type", item_type,
                       "property", property_name,
                       NULL);
}

void
gtk_property_lookup_list_model_set_object (GtkPropertyLookupListModel *self,
                                           gpointer                    object)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_PROPERTY_LOOKUP_LIST_MODEL (self));

  if (object)
    {
      if (self->items->len != 0 &&
          g_ptr_array_index (self->items, 0) == object)
        return;

      removed = gtk_property_lookup_list_model_clear (self, 0);

      g_ptr_array_insert (self->items, 0, g_object_ref (object));
      g_signal_connect_closure_by_id (object,
                                      g_signal_lookup ("notify", G_OBJECT_TYPE (object)),
                                      g_quark_from_static_string (self->property),
                                      g_cclosure_new (G_CALLBACK (gtk_property_lookup_list_model_notify_cb), G_OBJECT (self), NULL),
                                      FALSE);
      added = 1;
      if (!gtk_property_lookup_list_model_is_infinite (self))
        added += gtk_property_lookup_list_model_append (self, G_MAXUINT);
    }
  else
    {
      if (self->items->len == 0 ||
          g_ptr_array_index (self->items, 0) == NULL)
        return;

      removed = gtk_property_lookup_list_model_clear (self, 0);
      added = 0;
    }

  g_assert (removed != 0 || added != 0);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

gpointer
gtk_property_lookup_list_model_get_object (GtkPropertyLookupListModel *self)
{
  g_return_val_if_fail (GTK_IS_PROPERTY_LOOKUP_LIST_MODEL (self), NULL);

  if (self->items->len == 0)
    return NULL;
  
  return g_ptr_array_index (self->items, 0);
}

