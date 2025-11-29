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

#include "gtkrendererpaintableprivate.h"

#include <gtk/gtk.h>

struct _GtkRendererPaintable
{
  GObject parent_instance;

  GskRenderer *renderer;
  GdkPaintable *paintable;
};

struct _GtkRendererPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_PAINTABLE,
  PROP_RENDERER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_renderer_paintable_paintable_snapshot (GdkPaintable *paintable,
                                           GdkSnapshot  *snapshot,
                                           double        width,
                                           double        height)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (paintable);
  GtkSnapshot *node_snapshot;
  GskRenderNode *node;
  GdkTexture *texture;

  if (self->paintable == NULL)
    return;

  if (self->renderer == NULL ||
      !gsk_renderer_is_realized (self->renderer))
    {
      gdk_paintable_snapshot (self->paintable, snapshot, width, height);
      return;
    }

  node_snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (self->paintable, node_snapshot, width, height);
  node = gtk_snapshot_free_to_node (node_snapshot);
  if (node == NULL)
    return;

  texture = gsk_renderer_render_texture (self->renderer,
                                         node,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height));

  gdk_paintable_snapshot (GDK_PAINTABLE (texture), snapshot, width, height);
  g_object_unref (texture);

  gsk_render_node_unref (node);
}

static int
gtk_renderer_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (paintable);

  if (self->paintable == NULL)
    return 0;

  return gdk_paintable_get_intrinsic_width (self->paintable);
}

static int
gtk_renderer_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (paintable);

  if (self->paintable == NULL)
    return 0;

  return gdk_paintable_get_intrinsic_height (self->paintable);
}

static double
gtk_renderer_paintable_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (paintable);

  if (self->paintable == NULL)
    return 0.0;

  return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
}

static void
gtk_renderer_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_renderer_paintable_paintable_snapshot;
  iface->get_intrinsic_width = gtk_renderer_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_renderer_paintable_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_renderer_paintable_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (GtkRendererPaintable, gtk_renderer_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_renderer_paintable_paintable_init))

static void
gtk_renderer_paintable_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)

{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      gtk_renderer_paintable_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_RENDERER:
      gtk_renderer_paintable_set_renderer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_renderer_paintable_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_RENDERER:
      g_value_set_object (value, self->renderer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_renderer_paintable_unset_paintable (GtkRendererPaintable *self)
{
  guint flags;
  
  if (self->paintable == NULL)
    return;

  flags = gdk_paintable_get_flags (self->paintable);

  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_handlers_disconnect_by_func (self->paintable,
                                          gdk_paintable_invalidate_contents,
                                          self);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_handlers_disconnect_by_func (self->paintable,
                                          gdk_paintable_invalidate_size,
                                          self);

  g_clear_object (&self->paintable);
}

static void
gtk_renderer_paintable_dispose (GObject *object)
{
  GtkRendererPaintable *self = GTK_RENDERER_PAINTABLE (object);

  g_clear_object (&self->renderer);
  gtk_renderer_paintable_unset_paintable (self);

  G_OBJECT_CLASS (gtk_renderer_paintable_parent_class)->dispose (object);
}

static void
gtk_renderer_paintable_class_init (GtkRendererPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_renderer_paintable_get_property;
  gobject_class->set_property = gtk_renderer_paintable_set_property;
  gobject_class->dispose = gtk_renderer_paintable_dispose;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable",
                         "Paintable",
                         "The paintable to be shown",
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_RENDERER] =
    g_param_spec_object ("renderer",
                         "Renderer",
                         "Renderer used to render the paintable",
                         GSK_TYPE_RENDERER,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_renderer_paintable_init (GtkRendererPaintable *self)
{
}

GdkPaintable *
gtk_renderer_paintable_new (GskRenderer  *renderer,
                            GdkPaintable *paintable)
{
  g_return_val_if_fail (renderer == NULL || GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable), NULL);

  return g_object_new (GTK_TYPE_RENDERER_PAINTABLE,
                       "renderer", renderer,
                       "paintable", paintable,
                       NULL);
}

void
gtk_renderer_paintable_set_renderer (GtkRendererPaintable *self,
                                     GskRenderer          *renderer)
{
  g_return_if_fail (GTK_IS_RENDERER_PAINTABLE (self));
  g_return_if_fail (renderer == NULL || GSK_IS_RENDERER (renderer));

  if (!g_set_object (&self->renderer, renderer))
    return;

  if (self->paintable)
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RENDERER]);
}

GskRenderer *
gtk_renderer_paintable_get_renderer (GtkRendererPaintable *self)
{
  g_return_val_if_fail (GTK_IS_RENDERER_PAINTABLE (self), NULL);

  return self->renderer;
}

void
gtk_renderer_paintable_set_paintable (GtkRendererPaintable *self,
                                      GdkPaintable         *paintable)
{
  g_return_if_fail (GTK_IS_RENDERER_PAINTABLE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (self->paintable == paintable)
    return;

  gtk_renderer_paintable_unset_paintable (self);

  if (paintable)
    {
      const guint flags = gdk_paintable_get_flags (paintable);

      self->paintable = g_object_ref (paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect_swapped (paintable,
                                  "invalidate-contents",
                                  G_CALLBACK (gdk_paintable_invalidate_contents),
                                  self);
      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect_swapped (paintable,
                                  "invalidate-size",
                                  G_CALLBACK (gdk_paintable_invalidate_size),
                                  self);
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

GdkPaintable *
gtk_renderer_paintable_get_paintable (GtkRendererPaintable *self)
{
  g_return_val_if_fail (GTK_IS_RENDERER_PAINTABLE (self), NULL);

  return self->paintable;
}

