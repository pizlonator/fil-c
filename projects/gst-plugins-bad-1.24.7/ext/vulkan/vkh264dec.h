/* GStreamer
 * Copyright (C) 2023 Igalia, S.L.
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

#pragma once

#include <gst/codecs/gsth264decoder.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_H264_DECODER (gst_vulkan_h264_decoder_get_type())
G_DECLARE_FINAL_TYPE (GstVulkanH264Decoder, gst_vulkan_h264_decoder, GST, VULKAN_H264_DECODER, GstH264Decoder)

GST_ELEMENT_REGISTER_DECLARE (vulkanh264dec);

G_END_DECLS
