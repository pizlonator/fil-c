/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include "gskrenderer.h"
#include "gskprofilerprivate.h"
#include "gskdebugprivate.h"
#include "gskoffloadprivate.h"

G_BEGIN_DECLS

#define GSK_RENDERER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_RENDERER, GskRendererClass))
#define GSK_IS_RENDERER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_RENDERER))
#define GSK_RENDERER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDERER, GskRendererClass))

struct _GskRenderer
{
  GObject parent_instance;
};

struct _GskRendererClass
{
  GObjectClass parent_class;

  gboolean supports_offload;

  gboolean             (* realize)                              (GskRenderer            *renderer,
                                                                 GdkDisplay             *display,
                                                                 GdkSurface             *surface,
                                                                 GError                **error);
  void                 (* unrealize)                            (GskRenderer            *renderer);

  GdkTexture *         (* render_texture)                       (GskRenderer            *renderer,
                                                                 GskRenderNode          *root,
                                                                 const graphene_rect_t  *viewport);
  void                 (* render)                               (GskRenderer            *renderer,
                                                                 GskRenderNode          *root,
                                                                 const cairo_region_t   *invalid);
};

GskProfiler *           gsk_renderer_get_profiler               (GskRenderer    *renderer);

GskDebugFlags           gsk_renderer_get_debug_flags            (GskRenderer    *renderer);
void                    gsk_renderer_set_debug_flags            (GskRenderer    *renderer,
                                                                 GskDebugFlags   flags);

G_END_DECLS

