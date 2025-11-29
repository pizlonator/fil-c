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

#include "gtknoselection.h"

#include "gtkbitset.h"
#include "gtksectionmodelprivate.h"
#include "gtkselectionmodel.h"

/**
 * GtkNoSelection:
 *
 * `GtkNoSelection` is a `GtkSelectionModel` that does not allow selecting
 * anything.
 *
 * This model is meant to be used as a simple wrapper around a `GListModel`
 * when a `GtkSelectionModel` is required.
 *
 * `GtkNoSelection` passes through sections from the underlying model.
 */
struct _GtkNoSelection
{
  GObject parent_instance;

  GListModel *model;
};

struct _GtkNoSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  PROP_N_ITEMS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_no_selection_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_no_selection_get_n_items (GListModel *list)
{
  GtkNoSelection *self = GTK_NO_SELECTION (list);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_no_selection_get_item (GListModel *list,
                           guint       position)
{
  GtkNoSelection *self = GTK_NO_SELECTION (list);

  if (self->model == NULL)
    return NULL;

  return g_list_model_get_item (self->model, position);
}

static void
gtk_no_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_no_selection_get_item_type;
  iface->get_n_items = gtk_no_selection_get_n_items;
  iface->get_item = gtk_no_selection_get_item;
}

static void
gtk_no_selection_get_section (GtkSectionModel *model,
                              guint            position,
                              guint           *out_start,
                              guint           *out_end)
{
  GtkNoSelection *self = GTK_NO_SELECTION (model);

  gtk_list_model_get_section (self->model, position, out_start, out_end);
}

static void
gtk_no_selection_section_model_init (GtkSectionModelInterface *iface)
{
  iface->get_section = gtk_no_selection_get_section;
}

static gboolean
gtk_no_selection_is_selected (GtkSelectionModel *model,
                              guint              position)
{
  return FALSE;
}

static GtkBitset *
gtk_no_selection_get_selection_in_range (GtkSelectionModel *model,
                                         guint              pos,
                                         guint              n_items)
{
  return gtk_bitset_new_empty ();
}

static void
gtk_no_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_no_selection_is_selected;
  iface->get_selection_in_range = gtk_no_selection_get_selection_in_range;
}

G_DEFINE_TYPE_EXTENDED (GtkNoSelection, gtk_no_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_no_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL,
                                               gtk_no_selection_section_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_no_selection_selection_model_init))

static void
gtk_no_selection_items_changed_cb (GListModel     *model,
                                   guint           position,
                                   guint           removed,
                                   guint           added,
                                   GtkNoSelection *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static void
gtk_no_selection_sections_changed_cb (GtkSectionModel *model,
                                      unsigned int     position,
                                      unsigned int     n_items,
                                      gpointer         user_data)
{
  GtkNoSelection *self = GTK_NO_SELECTION (user_data);

  gtk_section_model_sections_changed (GTK_SECTION_MODEL (self), position, n_items);
}

static void
gtk_no_selection_clear_model (GtkNoSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_no_selection_items_changed_cb,
                                        self);
  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_no_selection_sections_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_no_selection_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)

{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_no_selection_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_no_selection_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, gtk_no_selection_get_item_type (G_LIST_MODEL (self)));
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_no_selection_get_n_items (G_LIST_MODEL (self)));
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_no_selection_dispose (GObject *object)
{
  GtkNoSelection *self = GTK_NO_SELECTION (object);

  gtk_no_selection_clear_model (self);

  G_OBJECT_CLASS (gtk_no_selection_parent_class)->dispose (object);
}

static void
gtk_no_selection_class_init (GtkNoSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_no_selection_get_property;
  gobject_class->set_property = gtk_no_selection_set_property;
  gobject_class->dispose = gtk_no_selection_dispose;

  /**
   * GtkNoSelection:item-type:
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
   * GtkNoSelection:model: (attributes org.gtk.property.get=gtk_no_selection_get_model org.gtk.Property.set=gtk_no_selection_set_model)
   *
   * The model being managed.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                       G_TYPE_LIST_MODEL,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNoSelection:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_no_selection_init (GtkNoSelection *self)
{
}

/**
 * gtk_no_selection_new:
 * @model: (nullable) (transfer full): the `GListModel` to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkNoSelection): a new `GtkNoSelection`
 */
GtkNoSelection *
gtk_no_selection_new (GListModel *model)
{
  GtkNoSelection *self;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_NO_SELECTION,
                       "model", model,
                       NULL);

  /* consume the reference */
  g_clear_object (&model);

  return self;
}

/**
 * gtk_no_selection_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkNoSelection`
 *
 * Gets the model that @self is wrapping.
 *
 * Returns: (transfer none) (nullable): The model being wrapped
 */
GListModel *
gtk_no_selection_get_model (GtkNoSelection *self)
{
  g_return_val_if_fail (GTK_IS_NO_SELECTION (self), NULL);

  return self->model;
}

/**
 * gtk_no_selection_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkNoSelection`
 * @model: (nullable): A `GListModel` to wrap
 *
 * Sets the model that @self should wrap.
 *
 * If @model is %NULL, this model will be empty.
 */
void
gtk_no_selection_set_model (GtkNoSelection *self,
                            GListModel     *model)
{
  guint n_items_before, n_items_after;

  g_return_if_fail (GTK_IS_NO_SELECTION (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  n_items_before = self->model ? g_list_model_get_n_items (self->model) : 0;
  gtk_no_selection_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (self->model, "items-changed",
                        G_CALLBACK (gtk_no_selection_items_changed_cb), self);
      if (GTK_IS_SECTION_MODEL (self->model))
        g_signal_connect (self->model, "sections-changed",
                          G_CALLBACK (gtk_no_selection_sections_changed_cb), self);
      n_items_after = g_list_model_get_n_items (self->model);
    }
  else
    n_items_after = 0;

  g_list_model_items_changed (G_LIST_MODEL (self),
                              0,
                              n_items_before,
                              n_items_after);
  if (n_items_before != n_items_after)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}
