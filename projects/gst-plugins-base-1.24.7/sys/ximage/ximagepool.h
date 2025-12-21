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

#ifndef __GST_XIMAGEPOOL_H__
#define __GST_XIMAGEPOOL_H__

#ifdef HAVE_XSHM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* HAVE_XSHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM */

#include <string.h>
#include <math.h>


G_BEGIN_DECLS

typedef struct _GstXImageMemory GstXImageMemory;

typedef struct _GstXImageBufferPool GstXImageBufferPool;
typedef struct _GstXImageBufferPoolClass GstXImageBufferPoolClass;

#include "ximagesink.h"

/**
 * GstXImageMemory:
 * @sink: a reference to the our #GstXImageSink
 * @ximage: the XImage of this buffer
 * @width: the width in pixels of XImage @ximage
 * @height: the height in pixels of XImage @ximage
 * @size: the size in bytes of XImage @ximage
 *
 * Subclass of #GstMemory containing additional information about an XImage.
 */
struct _GstXImageMemory
{
  GstMemory parent;

  /* Reference to the ximagesink we belong to */
  GstXImageSink *sink;

  XImage *ximage;

#ifdef HAVE_XSHM
  XShmSegmentInfo SHMInfo;
#endif                          /* HAVE_XSHM */

  gint x, y;
  gint width, height;
  size_t size;
};

/* buffer pool functions */
#define GST_TYPE_XIMAGE_BUFFER_POOL      (gst_ximage_buffer_pool_get_type())
#define GST_IS_XIMAGE_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_XIMAGE_BUFFER_POOL))
#define GST_XIMAGE_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_XIMAGE_BUFFER_POOL, GstXImageBufferPool))
#define GST_XIMAGE_BUFFER_POOL_CAST(obj) ((GstXImageBufferPool*)(obj))

struct _GstXImageBufferPool
{
  GstBufferPool bufferpool;

  GstXImageSink *sink;
  GstAllocator *allocator;

  GstCaps *caps;
  GstVideoInfo info;
  GstVideoAlignment align;
  guint    padded_width;
  guint    padded_height;
  gboolean add_metavideo;
  gboolean need_alignment;
};

struct _GstXImageBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType gst_ximage_buffer_pool_get_type (void);

GstBufferPool * gst_ximage_buffer_pool_new     (GstXImageSink * ximagesink);

gboolean gst_x_image_sink_check_xshm_calls (GstXImageSink * ximagesink,
        GstXContext * xcontext);

G_END_DECLS

#endif /* __GST_XIMAGEPOOL_H__ */
