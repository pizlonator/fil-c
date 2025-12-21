/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011-2012> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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

#ifndef __GST_OPUS_DEC_H__
#define __GST_OPUS_DEC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>
#include <opus_multistream.h>

G_BEGIN_DECLS

#define GST_TYPE_OPUS_DEC \
  (gst_opus_dec_get_type())
#define GST_OPUS_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPUS_DEC,GstOpusDec))
#define GST_OPUS_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPUS_DEC,GstOpusDecClass))
#define GST_IS_OPUS_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPUS_DEC))
#define GST_IS_OPUS_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPUS_DEC))

typedef struct _GstOpusDec GstOpusDec;
typedef struct _GstOpusDecClass GstOpusDecClass;

struct _GstOpusDec {
  GstAudioDecoder       element;

  OpusMSDecoder        *state;

  guint64               packetno;

  GstBuffer            *streamheader;
  GstBuffer            *vorbiscomment;

  guint32 sample_rate;
  guint n_channels;
  guint16 pre_skip;
  gint16 r128_gain;

  GstAudioChannelPosition opus_pos[64];
  GstAudioInfo info;

  guint8 n_streams;
  guint8 n_stereo_streams;
  guint8 channel_mapping_family;
  guint8 channel_mapping[256];

  gboolean apply_gain;
  double r128_gain_volume;

  gboolean use_inband_fec;
  GstBuffer *last_buffer;
  gboolean primed;

  guint64 leftover_plc_duration;

  GstClockTime last_known_buffer_duration;

  gboolean phase_inversion;

  /* Used to generate the 'stats' property. Protected by object lock */
  guint64 num_pushed;
  guint64 num_gap;
  guint64 plc_num_samples;
  guint64 plc_duration;
};

struct _GstOpusDecClass {
  GstAudioDecoderClass parent_class;
};

GType gst_opus_dec_get_type (void);

G_END_DECLS

#endif /* __GST_OPUS_DEC_H__ */
