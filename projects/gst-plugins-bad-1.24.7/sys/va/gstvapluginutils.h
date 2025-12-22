/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
#include <gst/va/gstva.h>
#include "gstvadevice.h"

G_BEGIN_DECLS

#ifdef G_OS_WIN32
#define GST_IS_VA_DISPLAY_PLATFORM(dpy) GST_IS_VA_DISPLAY_WIN32(dpy)
#define GST_VA_DEVICE_PATH_PROP_DESC "DXGI Adapter LUID"
#else
#define GST_IS_VA_DISPLAY_PLATFORM(dpy) GST_IS_VA_DISPLAY_DRM(dpy)
#define GST_VA_DEVICE_PATH_PROP_DESC "DRM device path"
#endif

GstVaDisplay * gst_va_display_platform_new (const gchar * path);

void gst_va_create_feature_name (GstVaDevice * device,
                                 const gchar * type_name_default,
                                 const gchar * type_name_templ,
                                 gchar ** type_name,
                                 const gchar * feature_name_default,
                                 const gchar * feature_name_templ,
                                 gchar ** feature_name,
                                 gchar ** desc,
                                 guint * rank);

GstBufferPool * gst_va_create_other_pool (GstAllocator * allocator,
                                          GstAllocationParams * params,
                                          GstCaps * caps,
                                          guint size);

G_END_DECLS
