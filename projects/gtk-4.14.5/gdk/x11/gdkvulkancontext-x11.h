/* GDK - The GIMP Drawing Kit
 *
 * gdkvulkancontext-x11.h: X11 specific Vulkan wrappers
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

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#define VK_USE_PLATFORM_XLIB_KHR

#include "gdkvulkancontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_X11_VULKAN_CONTEXT		(gdk_x11_vulkan_context_get_type ())
#define GDK_X11_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_X11_VULKAN_CONTEXT, GdkX11VulkanContext))
#define GDK_IS_X11_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_X11_VULKAN_CONTEXT))
#define GDK_X11_VULKAN_CONTEXT_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_X11_VULKAN_CONTEXT, GdkX11VulkanContextClass))
#define GDK_IS_X11_VULKAN_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_X11_VULKAN_CONTEXT))
#define GDK_X11_VULKAN_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_X11_VULKAN_CONTEXT, GdkX11VulkanContextClass))

typedef struct _GdkX11VulkanContext GdkX11VulkanContext;
typedef struct _GdkX11VulkanContextClass GdkX11VulkanContextClass;

struct _GdkX11VulkanContext
{
  GdkVulkanContext parent_instance;
};

struct _GdkX11VulkanContextClass
{
  GdkVulkanContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_x11_vulkan_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !GDK_RENDERING_VULKAN */

