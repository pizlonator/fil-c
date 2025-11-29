/* gdkvulkancontext-wayland.c
 *
 * gdkvulkancontext-wayland.c: Wayland specific Vulkan wrappers
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

#include "config.h"

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#include "gdkvulkancontext-wayland.h"

#include "gdkwaylanddisplay.h"
#include "gdkwaylandsurface.h"
#include "gdksurface-wayland-private.h"
#include "gdkprivate-wayland.h"

G_DEFINE_TYPE (GdkWaylandVulkanContext, gdk_wayland_vulkan_context, GDK_TYPE_VULKAN_CONTEXT)

static VkResult
gdk_wayland_vulkan_context_create_surface (GdkVulkanContext *context,
                                           VkSurfaceKHR     *vr_surface)
{
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  /* This is necessary so that Vulkan sees the Surface.
   * Usually, vkCreateXlibSurfaceKHR() will not cause a problem to happen as
   * it just creates resources, but further calls with the resulting surface
   * do cause issues.
   */
  gdk_display_sync (display);

  return GDK_VK_CHECK (vkCreateWaylandSurfaceKHR, gdk_vulkan_context_get_instance (context),
                                                   &(VkWaylandSurfaceCreateInfoKHR) {
                                                       VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                                                       NULL,
                                                       0,
                                                       gdk_wayland_display_get_wl_display (display),
                                                       gdk_wayland_surface_get_wl_surface (surface)
                                                   },
                                                   NULL,
                                                   vr_surface);
}

static void
gdk_vulkan_context_wayland_end_frame (GdkDrawContext *context,
                                      cairo_region_t *painted)
{
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));

  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_request_frame (surface);

  GDK_DRAW_CONTEXT_CLASS (gdk_wayland_vulkan_context_parent_class)->end_frame (context, painted);

  gdk_wayland_surface_notify_committed (surface);
}

static void
gdk_vulkan_context_wayland_empty_frame (GdkDrawContext *context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->has_pending_subsurface_commits)
    return;

  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_request_frame (surface);

  gdk_wayland_surface_commit (surface);
  gdk_wayland_surface_notify_committed (surface);
}

static void
gdk_wayland_vulkan_context_class_init (GdkWaylandVulkanContextClass *klass)
{
  GdkVulkanContextClass *vulkan_context_class = GDK_VULKAN_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  vulkan_context_class->create_surface = gdk_wayland_vulkan_context_create_surface;
  draw_context_class->end_frame = gdk_vulkan_context_wayland_end_frame;
  draw_context_class->empty_frame = gdk_vulkan_context_wayland_empty_frame;
}

static void
gdk_wayland_vulkan_context_init (GdkWaylandVulkanContext *self)
{
}

#endif /* GDK_RENDERING_VULKAN */

