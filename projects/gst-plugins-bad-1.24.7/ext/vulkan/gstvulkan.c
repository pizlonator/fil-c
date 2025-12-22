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

/**
 * SECTION:plugin-vulkan
 * @title: vulkan
 *
 * Cross-platform Vulkan plugin.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vksink.h"
#include "vkupload.h"
#include "vkimageidentity.h"
#include "vkcolorconvert.h"
#include "vkshaderspv.h"
#include "vkdownload.h"
#include "vkviewconvert.h"
#include "vkdeviceprovider.h"
#include "gstvulkanelements.h"
#include "vkoverlaycompositor.h"

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
#include "vkh264dec.h"
#include "vkh265dec.h"
#endif

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_DEVICE_PROVIDER_REGISTER (vulkandeviceprovider, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkansink, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkanupload, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkandownload, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkancolorconvert, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkanimageidentity, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkanshaderspv, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkanviewconvert, plugin);

  ret |= GST_ELEMENT_REGISTER (vulkanoverlaycompositor, plugin);

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  GST_ELEMENT_REGISTER (vulkanh264dec, plugin);
  GST_ELEMENT_REGISTER (vulkanh265dec, plugin);
#endif

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vulkan,
    "Vulkan plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
