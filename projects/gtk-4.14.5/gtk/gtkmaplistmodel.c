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

#include "gtkmaplistmodel.h"

#include "gtkrbtreeprivate.h"
#include "gtksectionmodel.h"
#include "gtkprivate.h"

/**
 * GtkMapListModel:
 *
 * A `GtkMapListModel` maps the items in a list model to different items.
 *
 * `GtkMapListModel` uses a [callback@Gtk.MapListModelMapFunc].
 *
 * Example: Create a list of `GtkEventControllers`
 * ```c
 * static gpointer
 * map_to_controllers (gpointer widget,
 *                     gpointer data)
 * {
 *   gpointer result = gtk_widget_observe_controllers (widget);
 *   g_object_unref (widget);
 *   return result;
 * }
 *
 * widgets = gtk_widget_observe_children (widget);
 *
 * controllers = gtk_map_list_model_new (widgets,
 *                                       map_to_controllers,
 *                                       NULL, NULL);
 *
 * model = gtk_flatten_list_model_new (GTK_TYPE_EVENT_CONTROLLER,
 *                                     controllers);
 * ```
 *
 * `GtkMapListModel` will attempt to discard the mapped objects as soon as
 * they are no longer needed and recreate them if necessary.
 *
 * `GtkMapListModel` passes through sections from the underlying model.
 */

enum {
  PROP_0,
  PROP_HAS_MAP,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  PROP_N_ITEMS,

  NUM_PROPERTIES
};

typedef struct _MapNode MapNode;
typedef struct _MapAugment MapAugment;

struct _MapNode
{
  guint n_items;
  gpointer item; /* can only be set when n_items == 1 */
};

struct _MapAugment
{
  guint n_items;
};

struct _GtkMapListModel
{
  GObject parent_instance;

  GListModel *model;
  GtkMapListModelMapFunc map_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GtkRbTree *items; /* NULL if map_func == NULL */
};

struct _GtkMapListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static MapNode *
gtk_map_list_model_get_nth (GtkRbTree *tree,
                            guint      position,
                            guint     *out_start_pos)
{
  MapNode *node, *tmp;
  guint start_pos = position;

  node = gtk_rb_tree_get_root (tree);

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          MapAugment *aug = gtk_rb_tree_get_augment (tree, tmp);
          if (position < aug->n_items)
            {
              node = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      if (position < node->n_items)
        {
          start_pos -= position;
          break;
        }
      position -= node->n_items;

      node = gtk_rb_tree_node_get_right (node);
    }

  if (out_start_pos)
    *out_start_pos = start_pos;

  return node;
}

static GType
gtk_map_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_map_list_model_get_n_items (GListModel *list)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (list);
  
  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_map_list_model_get_item (GListModel *list,
                             guint       position)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (list);
  MapNode *node;
  guint offset;

  if (self->model == NULL)
    return NULL;

  if (self->items == NULL)
    return g_list_model_get_item (self->model, position);

  node = gtk_map_list_model_get_nth (self->items, position, &offset);
  if (node == NULL)
    return NULL;

  if (node->item)
    return g_object_ref (node->item);

  if (offset != position)
    {
      MapNode *before = gtk_rb_tree_insert_before (self->items, node);
      before->n_items = position - offset;
      node->n_items -= before->n_items;
      gtk_rb_tree_node_mark_dirty (node);
    }

  if (node->n_items > 1)
    {
      MapNode *after = gtk_rb_tree_insert_after (self->items, node);
      after->n_items = node->n_items - 1;
      node->n_items = 1;
      gtk_rb_tree_node_mark_dirty (node);
    }

  node->item = self->map_func (g_list_model_get_item (self->model, position), self->user_data);
  g_object_add_weak_pointer (node->item, &node->item);

  return node->item;
}

static void
gtk_map_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_map_list_model_get_item_type;
  iface->get_n_items = gtk_map_list_model_get_n_items;
  iface->get_item = gtk_map_list_model_get_item;
}

static void
gtk_map_list_model_get_section (GtkSectionModel *model,
                                guint            position,
                                guint           *out_start,
                                guint           *out_end)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (model);

  if (GTK_IS_SECTION_MODEL (self->model))
    {
      gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), position, out_start, out_end);
      return;
    }

  *out_start = 0;
  *out_end = self->model ? g_list_model_get_n_items (self->model) : 0;
}

static void
gtk_map_list_model_sections_changed_cb (GtkSectionModel *model,
                                        unsigned int     position,
                                        unsigned int     n_items,
                                        gpointer         user_data)
{
  gtk_section_model_sections_changed (GTK_SECTION_MODEL (user_data), position, n_items);
}


static void
gtk_map_list_model_section_model_init (GtkSectionModelInterface *iface)
{
  iface->get_section = gtk_map_list_model_get_section;
}

G_DEFINE_TYPE_WITH_CODE (GtkMapListModel, gtk_map_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_map_list_model_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL, gtk_map_list_model_section_model_init))

static void
gtk_map_list_model_items_changed_cb (GListModel      *model,
                                     guint            position,
                                     guint            removed,
                                     guint            added,
                                     GtkMapListModel *self)
{
  MapNode *node;
  guint start, end;
  guint count;

  if (self->items == NULL)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      if (removed != added)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
      return;
    }

  node = gtk_map_list_model_get_nth (self->items, position, &start);
  g_assert (start <= position);

  count = removed;
  while (count > 0)
    {
      end = start + node->n_items;
      if (start == position && end <= position + count)
        {
          MapNode *next = gtk_rb_tree_node_get_next (node);
          count -= node->n_items;
          gtk_rb_tree_remove (self->items, node);
          node = next;
        }
      else
        {
          if (end >= position + count)
            {
              node->n_items -= count;
              count = 0;
              gtk_rb_tree_node_mark_dirty (node);
            }
          else if (start < position)
            {
              guint overlap = node->n_items - (position - start);
              node->n_items -= overlap;
              gtk_rb_tree_node_mark_dirty (node);
              count -= overlap;
              start = position;
              node = gtk_rb_tree_node_get_next (node);
            }
        }
    }

  if (added)
    {
      if (node == NULL)
        node = gtk_rb_tree_insert_before (self->items, NULL);
      else if (node->item)
        node = gtk_rb_tree_insert_after (self->items, node);

      node->n_items += added;
      gtk_rb_tree_node_mark_dirty (node);
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static void
gtk_map_list_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_map_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_map_list_model_get_property (GObject     *object,
                                 guint        prop_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_HAS_MAP:
      g_value_set_boolean (value, self->items != NULL);
      break;

    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, gtk_map_list_model_get_item_type (G_LIST_MODEL (self)));
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_map_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_map_list_model_clear_model (GtkMapListModel *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_map_list_model_sections_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->model, gtk_map_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_map_list_model_dispose (GObject *object)
{
  GtkMapListModel *self = GTK_MAP_LIST_MODEL (object);

  gtk_map_list_model_clear_model (self);
  if (self->user_destroy)
    self->user_destroy (self->user_data);
  self->map_func = NULL;
  self->user_data = NULL;
  self->user_destroy = NULL;
  g_clear_pointer (&self->items, gtk_rb_tree_unref);

  G_OBJECT_CLASS (gtk_map_list_model_parent_class)->dispose (object);
}

static void
gtk_map_list_model_class_init (GtkMapListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_map_list_model_set_property;
  gobject_class->get_property = gtk_map_list_model_get_property;
  gobject_class->dispose = gtk_map_list_model_dispose;

  /**
   * GtkMapListModel:has-map: (attributes org.gtk.Property.get=gtk_map_list_model_has_map)
   *
   * If a map is set for this model
   */
  properties[PROP_HAS_MAP] =
      g_param_spec_boolean ("has-map", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMapListModel:item-type:
   *
   * The type of items. See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.8
   **/
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMapListModel:model: (attributes org.gtk.Property.get=gtk_map_list_model_get_model org.gtk.Property.set=gtk_map_list_model_set_model)
   *
   * The model being mapped.
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMapListModel:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_map_list_model_init (GtkMapListModel *self)
{
}


static void
gtk_map_list_model_augment (GtkRbTree *map,
                            gpointer   _aug,
                            gpointer   _node,
                            gpointer   left,
                            gpointer   right)
{
  MapNode *node = _node;
  MapAugment *aug = _aug;

  aug->n_items = node->n_items;

  if (left)
    {
      MapAugment *left_aug = gtk_rb_tree_get_augment (map, left);
      aug->n_items += left_aug->n_items;
    }
  if (right)
    {
      MapAugment *right_aug = gtk_rb_tree_get_augment (map, right);
      aug->n_items += right_aug->n_items;
    }
}

/**
 * gtk_map_list_model_new:
 * @model: (transfer full) (nullable): The model to map
 * @map_func: (nullable): map function
 * @user_data: (closure): user data passed to @map_func
 * @user_destroy: destroy notifier for @user_data
 *
 * Creates a new `GtkMapListModel` for the given arguments.
 *
 * Returns: a new `GtkMapListModel`
 */
GtkMapListModel *
gtk_map_list_model_new (GListModel             *model,
                        GtkMapListModelMapFunc  map_func,
                        gpointer                user_data,
                        GDestroyNotify          user_destroy)
{
  GtkMapListModel *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  result = g_object_new (GTK_TYPE_MAP_LIST_MODEL,
                         "model", model,
                         NULL);

  /* consume the reference */
  g_clear_object (&model);

  if (map_func)
    gtk_map_list_model_set_map_func (result, map_func, user_data, user_destroy);

  return result;
}

static void
gtk_map_list_model_clear_node (gpointer _node)
{
  MapNode *node = _node;

  if (node->item)
    g_object_remove_weak_pointer (node->item, &node->item);
}

static void
gtk_map_list_model_init_items (GtkMapListModel *self)
{
  if (self->map_func && self->model)
    {
      guint n_items;

      if (self->items)
        {
          gtk_rb_tree_remove_all (self->items);
        }
      else
        {
          self->items = gtk_rb_tree_new (MapNode,
                                         MapAugment,
                                         gtk_map_list_model_augment,
                                         gtk_map_list_model_clear_node,
                                         NULL);
        }

      n_items = g_list_model_get_n_items (self->model);
      if (n_items)
        {
          MapNode *node = gtk_rb_tree_insert_before (self->items, NULL);
          node->n_items = g_list_model_get_n_items (self->model);
          gtk_rb_tree_node_mark_dirty (node);
        }
    }
  else
    {
      g_clear_pointer (&self->items, gtk_rb_tree_unref);
    }
}

/**
 * gtk_map_list_model_set_map_func:
 * @self: a `GtkMapListModel`
 * @map_func: (nullable): map function
 * @user_data: (closure): user data passed to @map_func
 * @user_destroy: destroy notifier for @user_data
 *
 * Sets the function used to map items.
 *
 * The function will be called whenever an item needs to be mapped
 * and must return the item to use for the given input item.
 *
 * Note that `GtkMapListModel` may call this function multiple times
 * on the same item, because it may delete items it doesn't need anymore.
 *
 * GTK makes no effort to ensure that @map_func conforms to the item type
 * of @self. It assumes that the caller knows what they are doing and the map
 * function returns items of the appropriate type.
 */
void
gtk_map_list_model_set_map_func (GtkMapListModel        *self,
                                 GtkMapListModelMapFunc  map_func,
                                 gpointer                user_data,
                                 GDestroyNotify          user_destroy)
{
  gboolean was_maped, will_be_maped;
  guint n_items;

  g_return_if_fail (GTK_IS_MAP_LIST_MODEL (self));
  g_return_if_fail (map_func != NULL || (user_data == NULL && !user_destroy));

  was_maped = self->map_func != NULL;
  will_be_maped = map_func != NULL;

  if (!was_maped && !will_be_maped)
    return;

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  self->map_func = map_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;
  
  gtk_map_list_model_init_items (self);

  if (self->model)
    n_items = g_list_model_get_n_items (self->model);
  else
    n_items = 0;
  if (n_items)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);

  if (was_maped != will_be_maped)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_MAP]);
}

/**
 * gtk_map_list_model_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkMapListModel`
 * @model: (nullable): The model to be mapped
 *
 * Sets the model to be mapped.
 *
 * GTK makes no effort to ensure that @model conforms to the item type
 * expected by the map function. It assumes that the caller knows what
 * they are doing and have set up an appropriate map function.
 */
void
gtk_map_list_model_set_model (GtkMapListModel *self,
                              GListModel      *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_MAP_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_map_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_map_list_model_items_changed_cb), self);
      added = g_list_model_get_n_items (model);

      if (GTK_IS_SECTION_MODEL (model))
        g_signal_connect (model, "sections-changed", G_CALLBACK (gtk_map_list_model_sections_changed_cb), self);
    }
  else
    {
      added = 0;
    }

  gtk_map_list_model_init_items (self);
  
  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_map_list_model_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkMapListModel`
 *
 * Gets the model that is currently being mapped or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets mapped
 */
GListModel *
gtk_map_list_model_get_model (GtkMapListModel *self)
{
  g_return_val_if_fail (GTK_IS_MAP_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_map_list_model_has_map: (attributes org.gtk.Method.get_property=has-map)
 * @self: a `GtkMapListModel`
 *
 * Checks if a map function is currently set on @self.
 *
 * Returns: %TRUE if a map function is set
 */
gboolean
gtk_map_list_model_has_map (GtkMapListModel *self)
{
  g_return_val_if_fail (GTK_IS_MAP_LIST_MODEL (self), FALSE);

  return self->map_func != NULL;
}
