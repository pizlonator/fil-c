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

#ifndef _GST_V4L2_CODEC_ALLOCATOR_H_
#define _GST_V4L2_CODEC_ALLOCATOR_H_

#include <gst/allocators/allocators.h>
#include <gst/gst.h>

#include "gstv4l2codecdevice.h"
#include "gstv4l2decoder.h"

#define GST_TYPE_V4L2_CODEC_ALLOCATOR gst_v4l2_codec_allocator_get_type ()
G_DECLARE_FINAL_TYPE (GstV4l2CodecAllocator, gst_v4l2_codec_allocator,
    GST_V4L2_CODEC, ALLOCATOR, GstDmaBufAllocator);


GstV4l2CodecAllocator  *gst_v4l2_codec_allocator_new (GstV4l2Decoder * decoder,
                                                      GstPadDirection direction,
                                                      guint num_buffers);

GstMemory              *gst_v4l2_codec_allocator_alloc (GstV4l2CodecAllocator * allocator);



gboolean                gst_v4l2_codec_allocator_create_buffer (GstV4l2CodecAllocator * self);

gboolean                gst_v4l2_codec_allocator_wait_for_buffer (GstV4l2CodecAllocator * self);

gboolean                gst_v4l2_codec_allocator_prepare_buffer (GstV4l2CodecAllocator * allocator,
                                                                 GstBuffer * buffer);

guint                   gst_v4l2_codec_allocator_get_pool_size (GstV4l2CodecAllocator *self);

void                    gst_v4l2_codec_allocator_detach (GstV4l2CodecAllocator * self);

void                    gst_v4l2_codec_allocator_set_flushing (GstV4l2CodecAllocator * self,
                                                               gboolean flushing);

guint32                 gst_v4l2_codec_memory_get_index (GstMemory * mem);

#endif /* _GST_V4L2_CODECS_ALLOCATOR_H_ */
