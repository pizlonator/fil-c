/*
 * Copyright © 2020 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkjoinedmenuprivate.h"

typedef struct
{
  GMenuModel *model;
  gulong      items_changed_handler;
} Menu;

struct _GtkJoinedMenu
{
  GMenuModel  parent_instance;
  GArray     *menus;
};

G_DEFINE_TYPE (GtkJoinedMenu, gtk_joined_menu, G_TYPE_MENU_MODEL)

static void
clear_menu (gpointer data)
{
  Menu *menu = data;

  g_clear_signal_handler (&menu->items_changed_handler, menu->model);
  g_clear_object (&menu->model);
}

static gint
gtk_joined_menu_get_offset_at_index (GtkJoinedMenu *self,
                                     gint           index)
{
  gint offset = 0;

  for (guint i = 0; i < index; i++)
    offset += g_menu_model_get_n_items (g_array_index (self->menus, Menu, i).model);

  return offset;
}

static gint
gtk_joined_menu_get_offset_at_model (GtkJoinedMenu *self,
                                     GMenuModel    *model)
{
  gint offset = 0;

  for (guint i = 0; i < self->menus->len; i++)
    {
      const Menu *menu = &g_array_index (self->menus, Menu, i);

      if (menu->model == model)
        break;

      offset += g_menu_model_get_n_items (menu->model);
    }

  return offset;
}

static gboolean
gtk_joined_menu_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static gint
gtk_joined_menu_get_n_items (GMenuModel *model)
{
  GtkJoinedMenu *self = (GtkJoinedMenu *)model;

  if (self->menus->len == 0)
    return 0;

  return gtk_joined_menu_get_offset_at_index (self, self->menus->len);
}

static const Menu *
gtk_joined_menu_get_item (GtkJoinedMenu *self,
                          gint          *item_index)
{
  g_assert (GTK_IS_JOINED_MENU (self));

  for (guint i = 0; i < self->menus->len; i++)
    {
      const Menu *menu = &g_array_index (self->menus, Menu, i);
      gint n_items = g_menu_model_get_n_items (menu->model);

      if (n_items > *item_index)
        return menu;

      (*item_index) -= n_items;
    }

  g_return_val_if_reached (NULL);
}

static void
gtk_joined_menu_get_item_attributes (GMenuModel  *model,
                                     gint         item_index,
                                     GHashTable **attributes)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  G_MENU_MODEL_GET_CLASS (menu->model)->get_item_attributes (menu->model, item_index, attributes);
}

static GMenuAttributeIter *
gtk_joined_menu_iterate_item_attributes (GMenuModel *model,
                                         gint        item_index)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  return G_MENU_MODEL_GET_CLASS (menu->model)->iterate_item_attributes (menu->model, item_index);
}

static GVariant *
gtk_joined_menu_get_item_attribute_value (GMenuModel         *model,
                                          gint                item_index,
                                          const gchar        *attribute,
                                          const GVariantType *expected_type)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  return G_MENU_MODEL_GET_CLASS (menu->model)->get_item_attribute_value (menu->model, item_index, attribute, expected_type);
}

static void
gtk_joined_menu_get_item_links (GMenuModel  *model,
                                gint         item_index,
                                GHashTable **links)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  G_MENU_MODEL_GET_CLASS (menu->model)->get_item_links (menu->model, item_index, links);
}

static GMenuLinkIter *
gtk_joined_menu_iterate_item_links (GMenuModel *model,
                                    gint        item_index)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  return G_MENU_MODEL_GET_CLASS (menu->model)->iterate_item_links (menu->model, item_index);
}

static GMenuModel *
gtk_joined_menu_get_item_link (GMenuModel  *model,
                               gint         item_index,
                               const gchar *link)
{
  const Menu *menu = gtk_joined_menu_get_item (GTK_JOINED_MENU (model), &item_index);
  return G_MENU_MODEL_GET_CLASS (menu->model)->get_item_link (menu->model, item_index, link);
}

static void
gtk_joined_menu_finalize (GObject *object)
{
  GtkJoinedMenu *self = (GtkJoinedMenu *)object;

  g_clear_pointer (&self->menus, g_array_unref);

  G_OBJECT_CLASS (gtk_joined_menu_parent_class)->finalize (object);
}

static void
gtk_joined_menu_class_init (GtkJoinedMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->finalize = gtk_joined_menu_finalize;

  menu_model_class->is_mutable = gtk_joined_menu_is_mutable;
  menu_model_class->get_n_items = gtk_joined_menu_get_n_items;
  menu_model_class->get_item_attributes = gtk_joined_menu_get_item_attributes;
  menu_model_class->iterate_item_attributes = gtk_joined_menu_iterate_item_attributes;
  menu_model_class->get_item_attribute_value = gtk_joined_menu_get_item_attribute_value;
  menu_model_class->get_item_links = gtk_joined_menu_get_item_links;
  menu_model_class->iterate_item_links = gtk_joined_menu_iterate_item_links;
  menu_model_class->get_item_link = gtk_joined_menu_get_item_link;
}

static void
gtk_joined_menu_init (GtkJoinedMenu *self)
{
  self->menus = g_array_new (FALSE, FALSE, sizeof (Menu));
  g_array_set_clear_func (self->menus, clear_menu);
}

static void
gtk_joined_menu_on_items_changed (GtkJoinedMenu *self,
                                  guint          offset,
                                  guint          removed,
                                  guint          added,
                                  GMenuModel    *model)
{
  g_assert (GTK_IS_JOINED_MENU (self));
  g_assert (G_IS_MENU_MODEL (model));

  offset += gtk_joined_menu_get_offset_at_model (self, model);
  g_menu_model_items_changed (G_MENU_MODEL (self), offset, removed, added);
}

GtkJoinedMenu *
gtk_joined_menu_new (void)
{
  return g_object_new (GTK_TYPE_JOINED_MENU, NULL);
}

static void
gtk_joined_menu_insert (GtkJoinedMenu *self,
                        GMenuModel    *model,
                        gint           index)
{
  Menu menu = { 0 };
  gint offset;
  gint n_items;

  g_assert (GTK_IS_JOINED_MENU (self));
  g_assert (G_IS_MENU_MODEL (model));
  g_assert (index >= 0);
  g_assert (index <= self->menus->len);

  menu.model = g_object_ref (model);
  menu.items_changed_handler =
    g_signal_connect_swapped (menu.model,
                              "items-changed",
                              G_CALLBACK (gtk_joined_menu_on_items_changed),
                              self);
  g_array_insert_val (self->menus, index, menu);

  n_items = g_menu_model_get_n_items (model);
  offset = gtk_joined_menu_get_offset_at_index (self, index);
  g_menu_model_items_changed (G_MENU_MODEL (self), offset, 0, n_items);
}

void
gtk_joined_menu_append_menu (GtkJoinedMenu *self,
                             GMenuModel    *model)
{

  g_return_if_fail (GTK_IS_JOINED_MENU (self));
  g_return_if_fail (G_MENU_MODEL (model));

  gtk_joined_menu_insert (self, model, self->menus->len);
}

void
gtk_joined_menu_prepend_menu (GtkJoinedMenu *self,
                              GMenuModel    *model)
{
  g_return_if_fail (GTK_IS_JOINED_MENU (self));
  g_return_if_fail (G_MENU_MODEL (model));

  gtk_joined_menu_insert (self, model, 0);
}

void
gtk_joined_menu_remove_index (GtkJoinedMenu *self,
                              guint          index)
{
  const Menu *menu;
  gint n_items;
  gint offset;

  g_return_if_fail (GTK_IS_JOINED_MENU (self));
  g_return_if_fail (index < self->menus->len);

  menu = &g_array_index (self->menus, Menu, index);

  offset = gtk_joined_menu_get_offset_at_index (self, index);
  n_items = g_menu_model_get_n_items (menu->model);

  g_array_remove_index (self->menus, index);

  g_menu_model_items_changed (G_MENU_MODEL (self), offset, n_items, 0);
}

void
gtk_joined_menu_remove_menu (GtkJoinedMenu *self,
                             GMenuModel    *model)
{
  g_return_if_fail (GTK_IS_JOINED_MENU (self));
  g_return_if_fail (G_IS_MENU_MODEL (model));

  for (guint i = 0; i < self->menus->len; i++)
    {
      if (g_array_index (self->menus, Menu, i).model == model)
        {
          gtk_joined_menu_remove_index (self, i);
          break;
        }
    }
}

guint
gtk_joined_menu_get_n_joined (GtkJoinedMenu *self)
{
  g_return_val_if_fail (GTK_IS_JOINED_MENU (self), 0);

  return self->menus->len;
}
