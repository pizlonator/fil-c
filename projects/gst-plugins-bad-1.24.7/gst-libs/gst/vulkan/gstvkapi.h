/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_VULKAN_API_H__
#define __GST_VULKAN_API_H__

/**
 * VK_PROTOTYPES: (attributes doc.skip=true)
 */
#define VK_PROTOTYPES

#include <gst/vulkan/gstvkconfig.h>
#include <gst/vulkan/vulkan-prelude.h>
#include <gst/vulkan/vulkan_fwd.h>
#include <gst/vulkan/vulkan-enumtypes.h>

/**
 * VK_DEFINE_NON_DISPATCHABLE_HANDLE:
 *
 * Allow applications to override the VK_DEFINE_NON_DISPATCHABLE_HANDLE
 * but provide our own version otherwise. The default vulkan define
 * provides a different symbol type depending on the architecture and
 * this causes multilib problems because the generated .gir files are
 * different.
 *
 * Also make sure to provide a suitable GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT
 * implementation when redefining VK_DEFINE_NON_DISPATCHABLE_HANDLE.
 *
 * Since: 1.20
 */
#if !defined(VK_DEFINE_NON_DISPATCHABLE_HANDLE)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif

#include <vulkan/vulkan_core.h>

#endif /* __GST_VULKAN_API_H__ */
