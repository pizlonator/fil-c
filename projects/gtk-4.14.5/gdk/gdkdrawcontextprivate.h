/* GDK - The GIMP Drawing Kit
 *
 * gdkdrawcontext.h: base class for rendering system support
 *
 * Copyright © 2016  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkdrawcontext.h"

#include "gdkmemoryformatprivate.h"

G_BEGIN_DECLS

#define GDK_DRAW_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAW_CONTEXT, GdkDrawContextClass))
#define GDK_IS_DRAW_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAW_CONTEXT))
#define GDK_DRAW_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAW_CONTEXT, GdkDrawContextClass))

typedef struct _GdkDrawContextClass GdkDrawContextClass;

struct _GdkDrawContext
{
  GObject parent_instance;
};

struct _GdkDrawContextClass
{
  GObjectClass parent_class;

  void                  (* begin_frame)                         (GdkDrawContext         *context,
                                                                 GdkMemoryDepth          depth,
                                                                 cairo_region_t         *update_area);
  void                  (* end_frame)                           (GdkDrawContext         *context,
                                                                 cairo_region_t         *painted);
  void                  (* empty_frame)                         (GdkDrawContext         *context);
  void                  (* surface_resized)                     (GdkDrawContext         *context);
};

void                    gdk_draw_context_surface_resized        (GdkDrawContext         *context);

void                    gdk_draw_context_begin_frame_full       (GdkDrawContext         *context,
                                                                 GdkMemoryDepth          depth,
                                                                 const cairo_region_t   *region);

void                    gdk_draw_context_empty_frame            (GdkDrawContext         *context);


G_END_DECLS

