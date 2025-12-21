/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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
#include <gst/base/gstbasetransform.h>
#include <gst/audio/gstdsd.h>

G_BEGIN_DECLS

#define GST_TYPE_DSD_CONVERT (gst_dsd_convert_get_type())
G_DECLARE_FINAL_TYPE (GstDsdConvert, gst_dsd_convert,
    GST, DSD_CONVERT, GstBaseTransform)
#define GST_DSD_CONVERT_CAST(obj) ((GstDsdConvert *)(obj))

GST_ELEMENT_REGISTER_DECLARE (dsdconvert);

G_END_DECLS
