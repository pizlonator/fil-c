/*
 * GStreamer
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_CCCOMBINER_H__
#define __GST_CCCOMBINER_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>

#include "ccutils.h"

G_BEGIN_DECLS
#define GST_TYPE_CCCOMBINER \
  (gst_cc_combiner_get_type())
#define GST_CCCOMBINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCCOMBINER,GstCCCombiner))
#define GST_CCCOMBINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCCOMBINER,GstCCCombinerClass))
#define GST_IS_CCCOMBINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCCOMBINER))
#define GST_IS_CCCOMBINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCCOMBINER))

typedef struct _GstCCCombiner GstCCCombiner;
typedef struct _GstCCCombinerClass GstCCCombinerClass;

struct _GstCCCombiner
{
  GstAggregator parent;

  gint video_fps_n, video_fps_d;
  gboolean progressive;
  GstClockTime previous_video_running_time_end;
  GstClockTime current_video_running_time;
  GstClockTime current_video_running_time_end;
  GstBuffer *current_video_buffer;

  GArray *current_frame_captions;
  GstVideoCaptionType caption_type;

  gboolean prop_schedule;
  guint prop_max_scheduled;
  gboolean prop_output_padding;

  gboolean schedule;
  guint max_scheduled;
  gboolean output_padding;
  guint current_scheduled;

  CCBuffer *cc_buffer;
  guint16 cdp_hdr_sequence_cntr;
  const struct cdp_fps_entry *cdp_fps_entry;
};

struct _GstCCCombinerClass
{
  GstAggregatorClass parent_class;
};

GType gst_cc_combiner_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (cccombiner);

G_END_DECLS
#endif /* __GST_CCCOMBINER_H__ */
