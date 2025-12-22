/* GStreamer
 * Copyright (C) 2020 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include <gst/gst.h>

#include "gstv4l2codecallocator.h"

#ifndef __GST_V4L2_CODEC_POOL_H__
#define __GST_V4L2_CODEC_POOL_H__

#define GST_TYPE_V4L2_CODEC_POOL gst_v4l2_codec_pool_get_type()
G_DECLARE_FINAL_TYPE(GstV4l2CodecPool, gst_v4l2_codec_pool, GST,
    V4L2_CODEC_POOL, GstBufferPool)

GstV4l2CodecPool *gst_v4l2_codec_pool_new  (GstV4l2CodecAllocator *allocator,
                                            const GstVideoInfo * vinfo);

guint32           gst_v4l2_codec_buffer_get_index (GstBuffer * buffer);

#endif /* __GST_V4L2_CODEC_POOL_H__ */
