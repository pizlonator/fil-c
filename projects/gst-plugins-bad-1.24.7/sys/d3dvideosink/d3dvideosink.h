/* GStreamer
 * Copyright (C) 2012 Roland Krikava <info@bluedigits.com>
 * Copyright (C) 2010-2011 David Hoyt <dhoyt@hoytsoft.org>
 * Copyright (C) 2010 Andoni Morales <ylatuya@gmail.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef _GSTD3DVIDEOSINK_H_
#define _GSTD3DVIDEOSINK_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/videooverlay.h>
#include <gst/video/navigation.h>

#include "d3dhelpers.h"

G_BEGIN_DECLS

#define GST_TYPE_D3DVIDEOSINK                     (gst_d3dvideosink_get_type())
#define GST_D3DVIDEOSINK(obj)                     (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3DVIDEOSINK,GstD3DVideoSink))
#define GST_D3DVIDEOSINK_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3DVIDEOSINK,GstD3DVideoSinkClass))
#define GST_D3DVIDEOSINK_GET_CLASS(obj)           (GST_D3DVIDEOSINK_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3DVIDEOSINK(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3DVIDEOSINK))
#define GST_IS_D3DVIDEOSINK_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3DVIDEOSINK))

typedef struct _GstD3DVideoSink GstD3DVideoSink;
typedef struct _GstD3DVideoSinkClass GstD3DVideoSinkClass;

struct _GstD3DVideoSink
{
  GstVideoSink       sink;
  GstD3DData         d3d;

  GstCaps *          supported_caps;

  GstVideoFormat     format;
  GstVideoInfo       info;
  gint               width;
  gint               height;
  GstBufferPool      *pool;
  GstBufferPool      *fallback_pool;
  GstBuffer          *fallback_buffer;

  GstVideoRectangle  crop_rect;
  GstVideoRectangle  render_rect;

  GRecMutex    lock;
  GThread *internal_window_thread;

  /* Properties */
  gboolean           force_aspect_ratio;
  gboolean           create_internal_window;
  gboolean           stream_stop_on_close;
  gboolean           enable_navigation_events;
};

struct _GstD3DVideoSinkClass
{
  GstVideoSinkClass parent_class;
  GstD3DDataClass   d3d;
  GRecMutex   lock;
  /* this count is incremented each time the sink is destroyed, so that
   * old queue events can be ignored */
  guint create_count;
};

#define LOCK_SINK(sink) G_STMT_START { \
    GST_TRACE_OBJECT(sink, "Locking sink from thread %p", g_thread_self()); \
    g_rec_mutex_lock(&sink->lock); \
    GST_TRACE_OBJECT(sink, "Locked sink from thread %p", g_thread_self()); \
} G_STMT_END
#define UNLOCK_SINK(sink) G_STMT_START { \
  GST_TRACE_OBJECT(sink, "Unlocking sink from thread %p", g_thread_self()); \
  g_rec_mutex_unlock(&sink->lock); \
} G_STMT_END
#define LOCK_CLASS(obj, klass) G_STMT_START { \
    GST_TRACE_OBJECT(obj, "Locking class from thread %p", g_thread_self()); \
    g_rec_mutex_lock(&klass->lock); \
    GST_TRACE_OBJECT(obj, "Locked class from thread %p", g_thread_self()); \
} G_STMT_END
#define UNLOCK_CLASS(obj, klass) G_STMT_START { \
  GST_TRACE_OBJECT(obj, "Unlocking class from thread %p", g_thread_self()); \
  g_rec_mutex_unlock(&klass->lock); \
} G_STMT_END

GType    gst_d3dvideosink_get_type (void);

G_END_DECLS


#endif /* _GSTD3DVIDEOSINK_H_ */
