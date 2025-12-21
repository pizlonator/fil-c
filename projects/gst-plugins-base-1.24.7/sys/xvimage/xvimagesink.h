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

#ifndef __GST_XV_IMAGE_SINK_H__
#define __GST_XV_IMAGE_SINK_H__

#include <gst/video/gstvideosink.h>

/* Helper functions */
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_XV_IMAGE_SINK \
  (gst_xv_image_sink_get_type())
#define GST_XV_IMAGE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_XV_IMAGE_SINK, GstXvImageSink))
#define GST_XV_IMAGE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_XV_IMAGE_SINK, GstXvImageSinkClass))
#define GST_IS_XV_IMAGE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_XV_IMAGE_SINK))
#define GST_IS_XV_IMAGE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_XV_IMAGE_SINK))

typedef struct _GstXvImageSink GstXvImageSink;
typedef struct _GstXvImageSinkClass GstXvImageSinkClass;

#include "xvimageallocator.h"
#include "xvimagepool.h"
#include "xvcontext.h"

/**
 * GstXvImageSink:
 * @display_name: the name of the Display we want to render to
 * @xvcontext: our instance's #GstXvContext
 * @xwindow: the #GstXWindow we are rendering to
 * @cur_image: a reference to the last #GstXvImage that was put to @xwindow. It
 * is used when Expose events are received to redraw the latest video frame
 * @event_thread: a thread listening for events on @xwindow and handling them
 * @running: used to inform @event_thread if it should run/shutdown
 * @fps_n: the framerate fraction numerator
 * @fps_d: the framerate fraction denominator
 * @x_lock: used to protect X calls as we are not using the XLib in threaded
 * mode
 * @flow_lock: used to protect data flow routines from external calls such as
 * events from @event_thread or methods from the #GstVideoOverlay interface
 * @par: used to override calculated pixel aspect ratio from @xvcontext
 * @pool_lock: used to protect the buffer pool
 * @image_pool: a list of #GstXvImageBuffer that could be reused at next buffer
 * allocation call
 * @synchronous: used to store if XSynchronous should be used or not (for
 * debugging purpose only)
 * @keep_aspect: used to remember if reverse negotiation scaling should respect
 * aspect ratio
 * @handle_events: used to know if we should handle select XEvents or not
 * @brightness: used to store the user settings for color balance brightness
 * @contrast: used to store the user settings for color balance contrast
 * @hue: used to store the user settings for color balance hue
 * @saturation: used to store the user settings for color balance saturation
 * @cb_changed: used to store if the color balance settings where changed
 * @video_width: the width of incoming video frames in pixels
 * @video_height: the height of incoming video frames in pixels
 *
 * The #GstXvImageSink data structure.
 */
struct _GstXvImageSink
{
  /* Our element stuff */
  GstVideoSink videosink;

  GstXvContextConfig config;
  GstXvContext *context;
  GstXvImageAllocator *allocator;
  GstXWindow *xwindow;
  GstBuffer *cur_image;

  GThread *event_thread;
  gboolean running;

  GstVideoInfo info;

  /* Framerate numerator and denominator */
  gint fps_n;
  gint fps_d;

  GMutex flow_lock;

  /* object-set pixel aspect ratio */
  GValue *par;

  /* the buffer pool */
  GstBufferPool *pool;

  gboolean synchronous;
  gboolean double_buffer;
  gboolean keep_aspect;
  gboolean redraw_border;
  gboolean handle_events;
  gboolean handle_expose;

  /* size of incoming video, used as the size for XvImage */
  guint video_width, video_height;

  gboolean draw_borders;

  /* stream metadata */
  gchar *media_title;

  /* saved render rectangle until we have a window */
  gboolean pending_render_rect;
  GstVideoRectangle render_rect;
};

struct _GstXvImageSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_xv_image_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (xvimagesink);

G_END_DECLS
#endif /* __GST_XV_IMAGE_SINK_H__ */
