/*
 * Copyright © 2016  Benjamin Otte
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
 */

#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrenderer.h>

G_BEGIN_DECLS

#define GSK_TYPE_GPU_RENDERER (gsk_gpu_renderer_get_type ())

#define GSK_GPU_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_GPU_RENDERER, GskGpuRenderer))
#define GSK_IS_GPU_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_GPU_RENDERER))

typedef struct _GskGpuRenderer                GskGpuRenderer;

GDK_AVAILABLE_IN_ALL
GType                   gsk_gpu_renderer_get_type            (void) G_GNUC_CONST;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuRenderer, g_object_unref)

G_END_DECLS

