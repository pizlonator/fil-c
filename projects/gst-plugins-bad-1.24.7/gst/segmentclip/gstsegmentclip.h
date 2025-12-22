/* GStreamer
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_SEGMENT_CLIP_H__
#define __GST_SEGMENT_CLIP_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_SEGMENT_CLIP \
  (gst_segment_clip_get_type())
#define GST_SEGMENT_CLIP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SEGMENT_CLIP,GstSegmentClip))
#define GST_SEGMENT_CLIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SEGMENT_CLIP,GstSegmentClipClass))
#define GST_SEGMENT_CLIP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_SEGMENT_CLIP,GstSegmentClipClass))
#define GST_IS_SEGMENT_CLIP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SEGMENT_CLIP))
#define GST_IS_SEGMENT_CLIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SEGMENT_CLIP))

typedef struct _GstSegmentClip GstSegmentClip;
typedef struct _GstSegmentClipClass GstSegmentClipClass;

struct _GstSegmentClip
{
  GstElement parent;

  /* < private > */
  GstPad *sinkpad;
  GstPad *srcpad;

  GstSegment segment;
};

struct _GstSegmentClipClass
{
  GstElementClass parent_class;

  gboolean      (*set_caps)    (GstSegmentClip *self, GstCaps *caps);
  GstFlowReturn (*clip_buffer) (GstSegmentClip *self, GstBuffer *buffer, GstBuffer **outbuf);
  void          (*reset)       (GstSegmentClip *self);
};

GType gst_segment_clip_get_type (void);

G_END_DECLS

#endif /* __GST_SEGMENT_CLIP_H__ */
