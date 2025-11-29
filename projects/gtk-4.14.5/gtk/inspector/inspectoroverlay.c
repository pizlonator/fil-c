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

#include "inspectoroverlay.h"


G_DEFINE_ABSTRACT_TYPE (GtkInspectorOverlay, gtk_inspector_overlay, G_TYPE_OBJECT)

static void
gtk_inspector_overlay_default_snapshot (GtkInspectorOverlay *self,
                                        GtkSnapshot         *snapshot,
                                        GskRenderNode       *node,
                                        GtkWidget           *widget)
{
}

static void
gtk_inspector_overlay_default_queue_draw (GtkInspectorOverlay *self)
{
}

static void
gtk_inspector_overlay_class_init (GtkInspectorOverlayClass *class)
{
  class->snapshot = gtk_inspector_overlay_default_snapshot;
  class->queue_draw = gtk_inspector_overlay_default_queue_draw;
}

static void
gtk_inspector_overlay_init (GtkInspectorOverlay *self)
{
}

void
gtk_inspector_overlay_snapshot (GtkInspectorOverlay *self,
                                GtkSnapshot         *snapshot,
                                GskRenderNode       *node,
                                GtkWidget           *widget)
{
  gtk_snapshot_push_debug (snapshot, "%s %p", G_OBJECT_TYPE_NAME (self), self);

  GTK_INSPECTOR_OVERLAY_GET_CLASS (self)->snapshot (self, snapshot, node, widget);

  gtk_snapshot_pop (snapshot);
}

void
gtk_inspector_overlay_queue_draw (GtkInspectorOverlay *self)
{
  GTK_INSPECTOR_OVERLAY_GET_CLASS (self)->queue_draw (self);
}

