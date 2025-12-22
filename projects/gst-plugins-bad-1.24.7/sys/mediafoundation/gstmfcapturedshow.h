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

#include <gst/gst.h>
#include "gstmfsourceobject.h"

G_BEGIN_DECLS

#define GST_TYPE_MF_CAPTURE_DSHOW (gst_mf_capture_dshow_get_type())
G_DECLARE_FINAL_TYPE (GstMFCaptureDShow, gst_mf_capture_dshow,
    GST, MF_CAPTURE_DSHOW, GstMFSourceObject);

GstMFSourceObject * gst_mf_capture_dshow_new (GstMFSourceType type,
                                              gint device_index,
                                              const gchar * device_name,
                                              const gchar * device_path);

GstMFSourceResult   gst_mf_capture_dshow_enumerate (gint device_index,
                                                    GstMFSourceObject ** object);

G_END_DECLS
