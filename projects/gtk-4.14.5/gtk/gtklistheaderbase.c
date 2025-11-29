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

#include "gtklistheaderbaseprivate.h"

typedef struct _GtkListHeaderBasePrivate GtkListHeaderBasePrivate;
struct _GtkListHeaderBasePrivate
{
  GObject *item;
  guint start;
  guint end;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkListHeaderBase, gtk_list_header_base, GTK_TYPE_WIDGET)

static void
gtk_list_header_base_default_update (GtkListHeaderBase *self,
                                     gpointer           item,
                                     guint              start,
                                     guint              end)
{
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  g_set_object (&priv->item, item);
  priv->start = start;
  priv->end = end;
}

static void
gtk_list_header_base_dispose (GObject *object)
{
  GtkListHeaderBase *self = GTK_LIST_HEADER_BASE (object);
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  g_clear_object (&priv->item);

  G_OBJECT_CLASS (gtk_list_header_base_parent_class)->dispose (object);
}

static void
gtk_list_header_base_class_init (GtkListHeaderBaseClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->update = gtk_list_header_base_default_update;

  gobject_class->dispose = gtk_list_header_base_dispose;
}

static void
gtk_list_header_base_init (GtkListHeaderBase *self)
{
}

void
gtk_list_header_base_update (GtkListHeaderBase *self,
                             gpointer           item,
                             guint              start,
                             guint              end)
{
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  if (priv->item == item &&
      priv->start == start && 
      priv->end == end)
    return;

  GTK_LIST_HEADER_BASE_GET_CLASS (self)->update (self, item, start, end);
}

guint
gtk_list_header_base_get_start (GtkListHeaderBase *self)
{
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  return priv->start;
}

guint
gtk_list_header_base_get_end (GtkListHeaderBase *self)
{
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  return priv->end;
}

gpointer
gtk_list_header_base_get_item (GtkListHeaderBase *self)
{
  GtkListHeaderBasePrivate *priv = gtk_list_header_base_get_instance_private (self);

  return priv->item;
}

