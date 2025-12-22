/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
#include <gst/video/video.h>
#include <gst/va/gstva.h>

G_BEGIN_DECLS

#define GST_TYPE_VA_POOL (gst_va_pool_get_type())
#define GST_VA_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_POOL, GstVaPool))
#define GST_VA_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VA_POOL, GstVaPoolClass))
#define GST_IS_VA_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_POOL))
#define GST_IS_VA_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VA_POOL))
#define GST_VA_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VA_POOL, GstVaPoolClass))

GST_VA_API
GType                gst_va_pool_get_type                 (void);
GST_VA_API
GstBufferPool *      gst_va_pool_new                      (void);
GST_VA_API
gboolean             gst_va_pool_requires_video_meta      (GstBufferPool * pool);
GST_VA_API
void                 gst_buffer_pool_config_set_va_allocation_params (GstStructure * config,
                                                                      guint usage_hint,
                                                                      GstVaFeature use_derived);
GST_VA_API
void                 gst_buffer_pool_config_set_va_alignment (GstStructure * config,
                                                              const GstVideoAlignment * align);
GST_VA_API
GstBufferPool *      gst_va_pool_new_with_config          (GstCaps * caps,
                                                           guint min_buffers,
                                                           guint max_buffers,
                                                           guint usage_hint,
                                                           GstVaFeature use_derived,
                                                           GstAllocator * allocator,
                                                           GstAllocationParams * alloc_params);
GST_VA_API
gboolean             gst_va_pool_get_buffer_size         (GstBufferPool * pool,
                                                          guint * size);

G_END_DECLS
