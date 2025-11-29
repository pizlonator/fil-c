/* GSK - The GTK Scene Kit
 *
 * Copyright 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <pango/pango.h>
#include "gtk/gtksnapshot.h"

G_BEGIN_DECLS

#define GSK_TYPE_PANGO_RENDERER            (gsk_pango_renderer_get_type ())
#define GSK_PANGO_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_PANGO_RENDERER, GskPangoRenderer))
#define GSK_IS_PANGO_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_PANGO_RENDERER))
#define GSK_PANGO_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))
#define GSK_IS_PANGO_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_PANGO_RENDERER))
#define GSK_PANGO_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))

typedef struct _GskPangoRenderer      GskPangoRenderer;
typedef struct _GskPangoRendererClass GskPangoRendererClass;

typedef enum
{
  GSK_PANGO_RENDERER_NORMAL,
  GSK_PANGO_RENDERER_SELECTED,
  GSK_PANGO_RENDERER_CURSOR
} GskPangoRendererState;

typedef gboolean (*GskPangoShapeHandler) (PangoAttrShape         *attr,
                                          GdkSnapshot            *snapshot,
                                          double                  width,
                                          double                  height);

/*
 * This is a PangoRenderer implementation that translates all the draw calls to
 * gsk render nodes, using the GtkSnapshot helper class. Glyphs are translated
 * to text nodes, other draw calls may fall back to cairo nodes.
 */

struct _GskPangoRenderer
{
  PangoRenderer          parent_instance;

  GtkWidget             *widget;
  GtkSnapshot           *snapshot;
  const GdkRGBA         *fg_color;

  /* Error underline color for this widget */
  GdkRGBA               *error_color;

  GskPangoRendererState  state;

  guint                  is_cached_renderer : 1;

  GskPangoShapeHandler   shape_handler;
};

struct _GskPangoRendererClass
{
  PangoRendererClass parent_class;
};

GType             gsk_pango_renderer_get_type  (void) G_GNUC_CONST;
void              gsk_pango_renderer_set_state (GskPangoRenderer      *crenderer,
                                                GskPangoRendererState  state);
void              gsk_pango_renderer_set_shape_handler (GskPangoRenderer      *crenderer,
                                                        GskPangoShapeHandler handler);
GskPangoRenderer *gsk_pango_renderer_acquire   (void);
void              gsk_pango_renderer_release   (GskPangoRenderer      *crenderer);

G_END_DECLS

