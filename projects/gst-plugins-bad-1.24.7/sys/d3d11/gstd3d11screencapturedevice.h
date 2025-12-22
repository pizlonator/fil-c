/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include "gstd3d11pluginutils.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D11_SCREEN_CAPTURE_DEVICE (gst_d3d11_screen_capture_device_get_type())
G_DECLARE_FINAL_TYPE (GstD3D11ScreenCaptureDevice, gst_d3d11_screen_capture_device,
    GST, D3D11_SCREEN_CAPTURE_DEVICE, GstDevice);

#define GST_TYPE_D3D11_SCREEN_CAPTURE_DEVICE_PROVIDER (gst_d3d11_screen_capture_device_provider_get_type())
G_DECLARE_FINAL_TYPE (GstD3D11ScreenCaptureDeviceProvider,
    gst_d3d11_screen_capture_device_provider,
    GST, D3D11_SCREEN_CAPTURE_DEVICE_PROVIDER, GstDeviceProvider);

G_END_DECLS

