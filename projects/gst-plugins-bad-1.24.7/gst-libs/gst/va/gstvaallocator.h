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

#include <gst/allocators/allocators.h>
#include <gst/va/gstva.h>
#include <gst/video/video.h>
#include <stdint.h>
#include <va/va.h>

G_BEGIN_DECLS

#define GST_TYPE_VA_DMABUF_ALLOCATOR (gst_va_dmabuf_allocator_get_type())
#define GST_VA_DMABUF_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_DMABUF_ALLOCATOR, GstVaDmabufAllocator))
#define GST_VA_DMABUF_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VA_DMABUF_ALLOCATOR, GstVaDmabufAllocatorClass))
#define GST_IS_VA_DMABUF_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_DMABUF_ALLOCATOR))
#define GST_IS_VA_DMABUF_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VA_DMABUF_ALLOCATOR))
#define GST_VA_DMABUF_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VA_DMABUF_ALLOCATOR, GstVaDmabufAllocatorClass))

GST_VA_API
GType                 gst_va_dmabuf_allocator_get_type    (void);
GST_VA_API
GstAllocator *        gst_va_dmabuf_allocator_new         (GstVaDisplay * display);
GST_VA_API
gboolean              gst_va_dmabuf_allocator_setup_buffer (GstAllocator * allocator,
                                                            GstBuffer * buffer);
GST_VA_API
gboolean              gst_va_dmabuf_allocator_prepare_buffer (GstAllocator * allocator,
                                                              GstBuffer * buffer);
GST_VA_API
void                  gst_va_dmabuf_allocator_flush       (GstAllocator * allocator);
GST_VA_API
gboolean              gst_va_dmabuf_allocator_set_format  (GstAllocator * allocator,
                                                           GstVideoInfoDmaDrm * info,
                                                           guint usage_hint);
GST_VA_API
gboolean              gst_va_dmabuf_allocator_get_format  (GstAllocator * allocator,
                                                           GstVideoInfoDmaDrm * info,
                                                           guint * usage_hint);

GST_VA_API
gboolean              gst_va_dmabuf_memories_setup        (GstVaDisplay * display,
                                                           GstVideoInfoDmaDrm * drm_info,
                                                           GstMemory * mem[GST_VIDEO_MAX_PLANES],
                                                           uintptr_t fds[GST_VIDEO_MAX_PLANES],
                                                           gsize offset[GST_VIDEO_MAX_PLANES],
                                                           guint usage_hint);

GST_VA_API
guint64               gst_va_dmabuf_get_modifier_for_format (GstVaDisplay * display,
                                                             GstVideoFormat format,
                                                             guint usage_hint);

#define GST_TYPE_VA_ALLOCATOR (gst_va_allocator_get_type())
#define GST_VA_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_ALLOCATOR, GstVaAllocator))
#define GST_VA_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VA_ALLOCATOR, GstVaAllocatorClass))
#define GST_IS_VA_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_ALLOCATOR))
#define GST_IS_VA_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VA_ALLOCATOR))
#define GST_VA_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VA_ALLOCATOR, GstVaAllocatorClass))

/**
 * GST_ALLOCATOR_VASURFACE:
 *
 * Since: 1.22
 */
#define GST_ALLOCATOR_VASURFACE   "VAMemory"

/**
 * GST_MAP_VA:
 *
 * Flag indicating that we should map the VASurfaceID instead of to
 * system memory, so users can use libva primitives to operate with
 * that surface.
 *
 * Since: 1.22
 */
#define GST_MAP_VA (GST_MAP_FLAG_LAST << 1)

GST_VA_API
GType                 gst_va_allocator_get_type           (void);
GST_VA_API
GstAllocator *        gst_va_allocator_new                (GstVaDisplay * display,
                                                           GArray * surface_formats);
GST_VA_API
GstMemory *           gst_va_allocator_alloc              (GstAllocator * allocator);
GST_VA_API
gboolean              gst_va_allocator_setup_buffer       (GstAllocator * allocator,
                                                           GstBuffer * buffer);
GST_VA_API
gboolean              gst_va_allocator_prepare_buffer     (GstAllocator * allocator,
                                                           GstBuffer * buffer);
GST_VA_API
void                  gst_va_allocator_flush              (GstAllocator * allocator);
GST_VA_API
gboolean              gst_va_allocator_set_format         (GstAllocator * allocator,
                                                           GstVideoInfo * info,
                                                           guint usage_hint,
                                                           GstVaFeature feat_use_derived);
GST_VA_API
gboolean              gst_va_allocator_get_format         (GstAllocator * allocator,
                                                           GstVideoInfo * info,
                                                           guint * usage_hint,
                                                           gboolean * use_derived);
GST_VA_API
void                  gst_va_allocator_set_hacks          (GstAllocator * allocator,
                                                           guint32 hacks);
GST_VA_API
GstVaDisplay *        gst_va_allocator_peek_display       (GstAllocator * allocator);

GST_VA_API
VASurfaceID           gst_va_memory_get_surface           (GstMemory * mem);
GST_VA_API
GstVaDisplay *        gst_va_memory_peek_display          (GstMemory * mem);
GST_VA_API
VASurfaceID           gst_va_buffer_get_surface           (GstBuffer * buffer);

GST_VA_API
gboolean              gst_va_buffer_create_aux_surface    (GstBuffer * buffer);
GST_VA_API
VASurfaceID           gst_va_buffer_get_aux_surface       (GstBuffer * buffer);
GST_VA_API
GstVaDisplay *        gst_va_buffer_peek_display          (GstBuffer * buffer);

G_END_DECLS
