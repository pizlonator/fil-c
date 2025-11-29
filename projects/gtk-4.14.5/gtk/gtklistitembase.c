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

#include "gtklistitembaseprivate.h"

#include "gtkaccessible.h"

typedef struct _GtkListItemBasePrivate GtkListItemBasePrivate;
struct _GtkListItemBasePrivate
{
  GObject *item;
  guint position;
  gboolean selected;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkListItemBase, gtk_list_item_base, GTK_TYPE_WIDGET)

static void
gtk_list_item_base_default_update (GtkListItemBase *self,
                                   guint            position,
                                   gpointer         item,
                                   gboolean         selected)
{
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);

  g_set_object (&priv->item, item);
  priv->position = position;
  priv->selected = selected;
}

static void
gtk_list_item_base_dispose (GObject *object)
{
  GtkListItemBase *self = GTK_LIST_ITEM_BASE (object);
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);

  g_clear_object (&priv->item);

  G_OBJECT_CLASS (gtk_list_item_base_parent_class)->dispose (object);
}

static void
gtk_list_item_base_class_init (GtkListItemBaseClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->update = gtk_list_item_base_default_update;

  gobject_class->dispose = gtk_list_item_base_dispose;
}

static void
gtk_list_item_base_init (GtkListItemBase *self)
{
}

void
gtk_list_item_base_update (GtkListItemBase *self,
                           guint            position,
                           gpointer         item,
                           gboolean         selected)
{
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);
  gboolean was_selected;

  if (priv->position == position &&
      priv->item == item &&
      priv->selected == selected)
    return;

  was_selected = priv->selected;

  GTK_LIST_ITEM_BASE_GET_CLASS (self)->update (self, position, item, selected);

  /* don't look at selected variable, it's not reentrancy safe */
  if (was_selected != priv->selected)
    {
      if (priv->selected)
        gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_SELECTED);

      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_STATE_SELECTED, priv->selected,
                                   -1);
    }
}

guint
gtk_list_item_base_get_position (GtkListItemBase *self)
{
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);

  return priv->position;
}

gpointer
gtk_list_item_base_get_item (GtkListItemBase *self)
{
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);

  return priv->item;
}

gboolean
gtk_list_item_base_get_selected (GtkListItemBase *self)
{
  GtkListItemBasePrivate *priv = gtk_list_item_base_get_instance_private (self);

  return priv->selected;
}

