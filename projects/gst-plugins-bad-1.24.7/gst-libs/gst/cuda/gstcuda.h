/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#ifndef GST_USE_UNSTABLE_API
#pragma message ("The Cuda library from gst-plugins-bad is unstable API and may change in future.")
#pragma message ("You can define GST_USE_UNSTABLE_API to avoid this warning.")
#endif

#include <gst/gst.h>
#include <gst/cuda/cuda-prelude.h>
#include <gst/cuda/cuda-gst.h>
#include <gst/cuda/gstcudabufferpool.h>
#include <gst/cuda/gstcudacontext.h>
#include <gst/cuda/gstcudaloader.h>
#include <gst/cuda/gstcudamemory.h>
#include <gst/cuda/gstcudanvrtc.h>
#include <gst/cuda/gstcudastream.h>
#include <gst/cuda/gstcudautils.h>

