/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_MF_CAPTURE_WINRT_H__
#define __GST_MF_CAPTURE_WINRT_H__

#include <gst/gst.h>
#include "gstmfsourceobject.h"

G_BEGIN_DECLS

#define GST_TYPE_MF_CAPTURE_WINRT (gst_mf_capture_winrt_get_type())
G_DECLARE_FINAL_TYPE (GstMFCaptureWinRT, gst_mf_capture_winrt,
    GST, MF_CAPTURE_WINRT, GstMFSourceObject);

GstMFSourceObject * gst_mf_capture_winrt_new (GstMFSourceType type,
                                              gint device_index,
                                              const gchar * device_name,
                                              const gchar * device_path,
                                              gpointer dispatcher);

GstMFSourceResult   gst_mf_capture_winrt_enumerate (gint device_index,
                                                    GstMFSourceObject ** object);

G_END_DECLS

#endif /* __GST_MF_CAPTURE_WINRT_H__ */