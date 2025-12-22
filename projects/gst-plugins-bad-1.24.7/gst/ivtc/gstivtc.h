/* GStreamer
 * Copyright (C) 2013 FIXME <fixme@example.com>
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

#ifndef _GST_IVTC_H_
#define _GST_IVTC_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

GST_ELEMENT_REGISTER_DECLARE (ivtc);

#define GST_TYPE_IVTC   (gst_ivtc_get_type())
#define GST_IVTC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IVTC,GstIvtc))
#define GST_IVTC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IVTC,GstIvtcClass))
#define GST_IS_IVTC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IVTC))
#define GST_IS_IVTC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IVTC))

typedef struct _GstIvtc GstIvtc;
typedef struct _GstIvtcClass GstIvtcClass;
typedef struct _GstIvtcField GstIvtcField;

struct _GstIvtcField
{
  GstBuffer *buffer;
  int parity;
  GstVideoFrame frame;
  GstClockTime ts;
};

#define GST_IVTC_MAX_FIELDS 10

struct _GstIvtc
{
  GstBaseTransform base_ivtc;

  GstSegment segment;

  GstVideoInfo sink_video_info;
  GstVideoInfo src_video_info;
  GstClockTime current_ts;
  GstClockTime field_duration;

  int n_fields;
  GstIvtcField fields[GST_IVTC_MAX_FIELDS];
};

struct _GstIvtcClass
{
  GstBaseTransformClass base_ivtc_class;
};

GType gst_ivtc_get_type (void);

G_END_DECLS

#endif
