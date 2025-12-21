/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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

#ifndef __GST_XVIMAGEALLOCATOR_H__
#define __GST_XVIMAGEALLOCATOR_H__

typedef struct _GstXvImageMemory GstXvImageMemory;

typedef struct _GstXvImageAllocator GstXvImageAllocator;
typedef struct _GstXvImageAllocatorClass GstXvImageAllocatorClass;

#include "xvcontext.h"

G_BEGIN_DECLS

/* allocator functions */
#define GST_TYPE_XVIMAGE_ALLOCATOR      (gst_xvimage_allocator_get_type())
#define GST_IS_XVIMAGE_ALLOCATOR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_XVIMAGE_ALLOCATOR))
#define GST_XVIMAGE_ALLOCATOR(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_XVIMAGE_ALLOCATOR, GstXvImageAllocator))
#define GST_XVIMAGE_ALLOCATOR_CAST(obj) ((GstXvImageAllocator*)(obj))

GType gst_xvimage_allocator_get_type (void);

/* the allocator */
GstXvImageAllocator * gst_xvimage_allocator_new         (GstXvContext * context);

GstXvContext *        gst_xvimage_allocator_peek_context (GstXvImageAllocator * allocator);

GstMemory *           gst_xvimage_allocator_alloc       (GstXvImageAllocator * allocator,
                                                         gint im_format,
                                                         const GstVideoInfo * info,
                                                         gint padded_width,
                                                         gint padded_height,
                                                         const GstVideoRectangle *crop,
                                                         GError ** error);

/* memory from the allocator */
gboolean              gst_xvimage_memory_is_from_context (GstMemory *mem,
                                                          GstXvContext * context);

gint                  gst_xvimage_memory_get_format     (GstXvImageMemory *mem);
XvImage *             gst_xvimage_memory_get_xvimage    (GstXvImageMemory *mem);
gboolean              gst_xvimage_memory_get_crop       (GstXvImageMemory *mem,
                                                         GstVideoRectangle *crop);

void                  gst_xvimage_memory_render         (GstXvImageMemory *mem,
                                                         GstVideoRectangle *src_crop,
                                                         GstXWindow *window,
                                                         GstVideoRectangle *dst_crop,
                                                         gboolean draw_border);

G_END_DECLS

#endif /*__GST_XVIMAGEALLOCATOR_H__*/
