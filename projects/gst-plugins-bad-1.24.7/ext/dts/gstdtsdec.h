/* GStreamer DTS decoder plugin based on libdtsdec
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
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


#ifndef __GST_DTSDEC_H__
#define __GST_DTSDEC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

G_BEGIN_DECLS

#define GST_TYPE_DTSDEC \
  (gst_dtsdec_get_type())
#define GST_DTSDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTSDEC,GstDtsDec))
#define GST_DTSDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DTSDEC,GstDtsDecClass))
#define GST_IS_DTSDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTSDEC))
#define GST_IS_DTSDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DTSDEC))

typedef struct _GstDtsDec GstDtsDec;
typedef struct _GstDtsDecClass GstDtsDecClass;

struct _GstDtsDec {
  GstAudioDecoder	 element;

  GstPadChainFunction base_chain;

  gboolean       dvdmode;
  gboolean       flag_update;
  gboolean       prev_flags;

  /* stream properties */
  gint 	         bit_rate;
  gint 	         sample_rate;
  gint 	         stream_channels;
  gint 	         request_channels;
  gint 	         using_channels;

  gint           channel_reorder_map[6];

  /* decoding properties */
  sample_t 	 level;
  sample_t 	 bias;
  gboolean 	 dynamic_range_compression;
  sample_t 	*samples;
#ifndef DTS_OLD
  dca_state_t   *state;
#else
  dts_state_t 	*state;
#endif
};

struct _GstDtsDecClass {
  GstAudioDecoderClass parent_class;

  guint32 dts_cpuflags;
};

GType gst_dtsdec_get_type(void);

GST_ELEMENT_REGISTER_DECLARE (dtsdec);

G_END_DECLS

#endif /* __GST_DTSDEC_H__ */
