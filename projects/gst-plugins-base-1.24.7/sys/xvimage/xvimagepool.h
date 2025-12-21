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

#ifndef __GST_XVIMAGEPOOL_H__
#define __GST_XVIMAGEPOOL_H__

#include <gst/gst.h>

#include "xvimageallocator.h"

G_BEGIN_DECLS

typedef struct _GstXvImageBufferPool GstXvImageBufferPool;
typedef struct _GstXvImageBufferPoolClass GstXvImageBufferPoolClass;

/* buffer pool functions */
#define GST_TYPE_XVIMAGE_BUFFER_POOL      (gst_xvimage_buffer_pool_get_type())
#define GST_IS_XVIMAGE_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_XVIMAGE_BUFFER_POOL))
#define GST_XVIMAGE_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_XVIMAGE_BUFFER_POOL, GstXvImageBufferPool))
#define GST_XVIMAGE_BUFFER_POOL_CAST(obj) ((GstXvImageBufferPool*)(obj))

struct _GstXvImageBufferPool
{
  GstBufferPool bufferpool;

  GstXvImageAllocator *allocator;

  GstCaps *caps;
  gint im_format;
  GstVideoRectangle crop;
  GstVideoInfo info;
  GstVideoAlignment align;
  guint padded_width;
  guint padded_height;
  gboolean add_metavideo;
  gboolean need_alignment;

  /* used for calculating actual size, stride, and offset */
  GstMemory *pre_alloc_mem;
};

struct _GstXvImageBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType gst_xvimage_buffer_pool_get_type (void);

GstBufferPool *    gst_xvimage_buffer_pool_new     (GstXvImageAllocator *allocator);

G_END_DECLS

#endif /*__GST_XVIMAGEPOOL_H__*/
