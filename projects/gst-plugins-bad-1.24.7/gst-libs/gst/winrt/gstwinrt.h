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

#ifndef __GST_WINRT_H__
#define __GST_WINRT_H__

#ifndef GST_USE_UNSTABLE_API
#pragma message ("The winrt library from gst-plugins-bad is unstable API and may change in future.")
#pragma message ("You can define GST_USE_UNSTABLE_API to avoid this warning.")
#endif

#include <gst/gst.h>
#include <gst/winrt/gstwinrtdevicewatcher.h>

#endif /* __GST_WINRT_H__ */
