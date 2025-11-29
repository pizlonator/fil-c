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

#pragma once

#include <gtk/gtksnapshot.h>

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_OVERLAY             (gtk_inspector_overlay_get_type ())

G_DECLARE_DERIVABLE_TYPE (GtkInspectorOverlay, gtk_inspector_overlay, GTK, INSPECTOR_OVERLAY, GObject)

struct _GtkInspectorOverlayClass
{
  GObjectClass parent_class;

  void                  (* snapshot)                            (GtkInspectorOverlay    *self,
                                                                 GtkSnapshot            *snapshot,
                                                                 GskRenderNode          *node,
                                                                 GtkWidget              *widget);
  void                  (* queue_draw)                          (GtkInspectorOverlay    *self);
};

void                    gtk_inspector_overlay_snapshot          (GtkInspectorOverlay    *self,
                                                                 GtkSnapshot            *snapshot,
                                                                 GskRenderNode          *node,
                                                                 GtkWidget              *widget);
void                    gtk_inspector_overlay_queue_draw        (GtkInspectorOverlay    *self);

G_END_DECLS

