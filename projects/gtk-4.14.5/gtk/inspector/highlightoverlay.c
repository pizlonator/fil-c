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

#include "highlightoverlay.h"

#include "gtkwidget.h"

struct _GtkHighlightOverlay
{
  GtkInspectorOverlay parent_instance;

  GtkWidget *widget;
  GdkRGBA color;
};

struct _GtkHighlightOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkHighlightOverlay, gtk_highlight_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
gtk_highlight_overlay_snapshot (GtkInspectorOverlay *overlay,
                                GtkSnapshot         *snapshot,
                                GskRenderNode       *node,
                                GtkWidget           *widget)
{
  GtkHighlightOverlay *self = GTK_HIGHLIGHT_OVERLAY (overlay);
  graphene_rect_t bounds;

  if (!gtk_widget_compute_bounds (self->widget, widget, &bounds))
    return;

  gtk_snapshot_append_color (snapshot,
                             &self->color,
                             &bounds);
}

static void
gtk_highlight_overlay_queue_draw (GtkInspectorOverlay *overlay)
{
  GtkHighlightOverlay *self = GTK_HIGHLIGHT_OVERLAY (overlay);

  gtk_widget_queue_draw (self->widget);
}

static void
gtk_highlight_overlay_dispose (GObject *object)
{
  GtkHighlightOverlay *self = GTK_HIGHLIGHT_OVERLAY (object);

  g_clear_object (&self->widget);

  G_OBJECT_CLASS (gtk_highlight_overlay_parent_class)->dispose (object);
}

static void
gtk_highlight_overlay_class_init (GtkHighlightOverlayClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);

  overlay_class->snapshot = gtk_highlight_overlay_snapshot;
  overlay_class->queue_draw = gtk_highlight_overlay_queue_draw;

  gobject_class->dispose = gtk_highlight_overlay_dispose;
}

static void
gtk_highlight_overlay_init (GtkHighlightOverlay *self)
{
  self->color = (GdkRGBA) { 0.0, 0.0, 1.0, 0.2 };
}

GtkInspectorOverlay *
gtk_highlight_overlay_new (GtkWidget *widget)
{
  GtkHighlightOverlay *self;

  self = g_object_new (GTK_TYPE_HIGHLIGHT_OVERLAY, NULL);

  self->widget = g_object_ref (widget);

  return GTK_INSPECTOR_OVERLAY (self);
}

GtkWidget *
gtk_highlight_overlay_get_widget (GtkHighlightOverlay *self)
{
  return self->widget;
}

void
gtk_highlight_overlay_set_color (GtkHighlightOverlay *self,
                                 const GdkRGBA       *color)
{
  if (gdk_rgba_equal (&self->color, color))
    return;

  self->color = *color;
  gtk_inspector_overlay_queue_draw (GTK_INSPECTOR_OVERLAY (self));
}
