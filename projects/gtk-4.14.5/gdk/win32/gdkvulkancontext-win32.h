/* GDK - The GIMP Drawing Kit
 *
 * gdkvulkancontext-win32.h: Win32 specific Vulkan wrappers
 *
 * Copyright © 2016  Benjamin Otte
 * Copyright © 2016  Chun-wei Fan
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

#define VK_USE_PLATFORM_WIN32_KHR

#include "gdkvulkancontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_VULKAN_CONTEXT            (gdk_win32_vulkan_context_get_type ())
#define GDK_WIN32_VULKAN_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_VULKAN_CONTEXT, GdkWin32VulkanContext))
#define GDK_IS_WIN32_VULKAN_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_VULKAN_CONTEXT))
#define GDK_WIN32_VULKAN_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_VULKAN_CONTEXT, GdkWin32VulkanContextClass))
#define GDK_IS_WIN32_VULKAN_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_VULKAN_CONTEXT))
#define GDK_WIN32_VULKAN_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_VULKAN_CONTEXT, GdkWin32VulkanContextClass))

typedef struct _GdkWin32VulkanContext      GdkWin32VulkanContext;
typedef struct _GdkWin32VulkanContextClass GdkWin32VulkanContextClass;

struct _GdkWin32VulkanContext
{
  GdkVulkanContext parent_instance;
};

struct _GdkWin32VulkanContextClass
{
  GdkVulkanContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gdk_win32_vulkan_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !GDK_RENDERING_VULKAN */

