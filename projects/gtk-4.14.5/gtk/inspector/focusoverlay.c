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

#include "focusoverlay.h"

#include "gtkwidget.h"
#include "gtkroot.h"
#include "gtknative.h"

struct _GtkFocusOverlay
{
  GtkInspectorOverlay parent_instance;

  GdkRGBA color;
};

struct _GtkFocusOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkFocusOverlay, gtk_focus_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
gtk_focus_overlay_snapshot (GtkInspectorOverlay *overlay,
                            GtkSnapshot         *snapshot,
                            GskRenderNode       *node,
                            GtkWidget           *widget)
{
  GtkFocusOverlay *self = GTK_FOCUS_OVERLAY (overlay);
  GtkWidget *focus;
  graphene_rect_t bounds;

  if (!GTK_IS_NATIVE (widget))
    return;

  focus = gtk_root_get_focus (GTK_ROOT (gtk_widget_get_root (widget)));
  if (!focus)
    return;

  if (!gtk_widget_is_ancestor (focus, widget))
    return;

  if (GTK_WIDGET (gtk_widget_get_native (focus)) != widget)
    return;

  if (!gtk_widget_compute_bounds (focus, widget, &bounds))
    return;

  gtk_snapshot_append_color (snapshot, &self->color, &bounds);
}

static void
gtk_focus_overlay_queue_draw (GtkInspectorOverlay *overlay)
{
}

static void
gtk_focus_overlay_class_init (GtkFocusOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);

  overlay_class->snapshot = gtk_focus_overlay_snapshot;
  overlay_class->queue_draw = gtk_focus_overlay_queue_draw;
}

static void
gtk_focus_overlay_init (GtkFocusOverlay *self)
{
  self->color = (GdkRGBA) { 0.5, 0.0, 1.0, 0.2 };
}

GtkInspectorOverlay *
gtk_focus_overlay_new (void)
{
  GtkFocusOverlay *self;

  self = g_object_new (GTK_TYPE_FOCUS_OVERLAY, NULL);

  return GTK_INSPECTOR_OVERLAY (self);
}

void
gtk_focus_overlay_set_color (GtkFocusOverlay *self,
                             const GdkRGBA   *color)
{
  if (gdk_rgba_equal (&self->color, color))
    return;

  self->color = *color;
  gtk_inspector_overlay_queue_draw (GTK_INSPECTOR_OVERLAY (self));
}
