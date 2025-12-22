/* GStreamer
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_RSVG_DEC_H__
#define __GST_RSVG_DEC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>

#include <cairo.h>

#include <librsvg/rsvg.h>

G_BEGIN_DECLS

#define GST_TYPE_RSVG_DEC \
  (gst_rsvg_dec_get_type())
#define GST_RSVG_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RSVG_DEC,GstRsvgDec))
#define GST_RSVG_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RSVG_DEC,GstRsvgDecClass))
#define GST_IS_RSVG_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RSVG_DEC))
#define GST_IS_RSVG_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RSVG_DEC))

typedef struct _GstRsvgDec GstRsvgDec;
typedef struct _GstRsvgDecClass GstRsvgDecClass;

struct _GstRsvgDec
{
  GstVideoDecoder  decoder;

  GstPad     *sinkpad;
  GstPad     *srcpad;

  GList *pending_events;

  GstClockTime first_timestamp;
  guint64 frame_count;

  GstVideoCodecState *input_state;

  GstSegment segment;
  gboolean need_newsegment;

  GstAdapter *adapter;
};

struct _GstRsvgDecClass
{
  GstVideoDecoderClass parent_class;
};

GType gst_rsvg_dec_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (rsvgdec);

G_END_DECLS

#endif /* __GST_RSVG_DEC_H__ */
