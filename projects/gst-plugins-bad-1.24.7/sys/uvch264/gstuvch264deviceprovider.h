/* GStreamer
 * Copyright (C) 2019 Thibault Saunier <tsaunier@igalia.com>
 *
 * gstuvc_h264deviceprovider.c: UvcH264 device probing and monitoring
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gst/gst.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (GstUvcH264Device, gst_uvc_h264_device, GST, UVC_H264_DEVICE, GstDevice)

G_DECLARE_FINAL_TYPE (GstUvcH264DeviceProvider, gst_uvc_h264_device_provider, GST, UVC_H264_DEVICE_PROVIDER, GstDeviceProvider)
GST_DEVICE_PROVIDER_REGISTER_DECLARE (uvch264deviceprovider);
G_END_DECLS
