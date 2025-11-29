/* gdkvulkancontext-wayland.h
 *
 * gdkvulkancontext-wayland.h: Wayland specific Vulkan wrappers
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#define VK_USE_PLATFORM_WAYLAND_KHR

#include "gdkvulkancontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_WAYLAND_VULKAN_CONTEXT		(gdk_wayland_vulkan_context_get_type ())
#define GDK_WAYLAND_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WAYLAND_VULKAN_CONTEXT, GdkWaylandVulkanContext))
#define GDK_IS_WAYLAND_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WAYLAND_VULKAN_CONTEXT))
#define GDK_WAYLAND_VULKAN_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_VULKAN_CONTEXT, GdkWaylandVulkanContextClass))
#define GDK_IS_WAYLAND_VULKAN_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_VULKAN_CONTEXT))
#define GDK_WAYLAND_VULKAN_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_VULKAN_CONTEXT, GdkWaylandVulkanContextClass))

typedef struct _GdkWaylandVulkanContext GdkWaylandVulkanContext;
typedef struct _GdkWaylandVulkanContextClass GdkWaylandVulkanContextClass;

struct _GdkWaylandVulkanContext
{
  GdkVulkanContext parent_instance;
};

struct _GdkWaylandVulkanContextClass
{
  GdkVulkanContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_wayland_vulkan_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !GDK_RENDERING_VULKAN */

