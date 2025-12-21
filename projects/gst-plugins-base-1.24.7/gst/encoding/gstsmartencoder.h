/* GStreamer video re-encoder element
 * Copyright (C) <2010> Edward Hervey <bilboed@bilboed.com>
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
#ifndef __SMART_ENCODER_H__
#define __SMART_ENCODER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_SMART_ENCODER (gst_smart_encoder_get_type())
G_DECLARE_FINAL_TYPE (GstSmartEncoder, gst_smart_encoder, GST, SMART_ENCODER, GstBin)

struct _GstSmartEncoder {
  GstBin parent;

  GstPad *sinkpad, *srcpad;

  gboolean pushed_segment;

  /* Segment received upstream */
  GstSegment input_segment;

  /* The segment we pushed downstream */
  GstSegment output_segment;

  /* Internal segments to compute buffers running time before pushing
   * them downstream. It is the encoder segment when reecoding gops,
   * and the input segment when pushing them unmodified. */
  GstSegment internal_segment;
  GstClockTime last_dts;

  GstCaps *original_caps;
  gboolean push_original_caps;
  GstEvent *segment_event;
  GstEvent *stream_start_event;

  /* Pending GOP to be checked */
  GList* pending_gop;
  guint64 gop_start;	/* GOP start PTS in the `input_segment` scale. */
  guint64 gop_stop;		/* GOP end PTS in the `input_segment` scale. */

  /* Internal recoding elements */
  GstPad *internal_sinkpad;
  GstPad *internal_srcpad;
  GstElement *decoder;
  GstElement *encoder;

  GstFlowReturn internal_flow;
  GMutex internal_flow_lock;
  GCond internal_flow_cond;
};

gboolean gst_smart_encoder_set_encoder (GstSmartEncoder *self,
                                        GstCaps *format,
                                        GstElement *encoder);

G_END_DECLS

#endif /* __SMART_ENCODER_H__ */
