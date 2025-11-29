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
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_RENDERER (gsk_vulkan_renderer_get_type ())

#define GSK_VULKAN_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_VULKAN_RENDERER, GskVulkanRenderer))
#define GSK_IS_VULKAN_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_VULKAN_RENDERER))
#define GSK_VULKAN_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_VULKAN_RENDERER, GskVulkanRendererClass))
#define GSK_IS_VULKAN_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_VULKAN_RENDERER))
#define GSK_VULKAN_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_VULKAN_RENDERER, GskVulkanRendererClass))

/**
 * GskVulkanRenderer:
 *
 * A GSK renderer that is using Vulkan.
 *
 * This renderer will fail to realize if Vulkan is not supported.
 */
typedef struct _GskVulkanRenderer                GskVulkanRenderer;
typedef struct _GskVulkanRendererClass           GskVulkanRendererClass;

GDK_AVAILABLE_IN_ALL
GType                   gsk_vulkan_renderer_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GskRenderer *           gsk_vulkan_renderer_new                 (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskVulkanRenderer, g_object_unref)

G_END_DECLS
