/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_AMC_AUDIO_DEC_H__
#define __GST_AMC_AUDIO_DEC_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiodecoder.h>

#include "gstamc.h"

G_BEGIN_DECLS

#define GST_TYPE_AMC_AUDIO_DEC \
  (gst_amc_audio_dec_get_type())
#define GST_AMC_AUDIO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMC_AUDIO_DEC,GstAmcAudioDec))
#define GST_AMC_AUDIO_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMC_AUDIO_DEC,GstAmcAudioDecClass))
#define GST_AMC_AUDIO_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AMC_AUDIO_DEC,GstAmcAudioDecClass))
#define GST_IS_AMC_AUDIO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMC_AUDIO_DEC))
#define GST_IS_AMC_AUDIO_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMC_AUDIO_DEC))

typedef struct _GstAmcAudioDec GstAmcAudioDec;
typedef struct _GstAmcAudioDecClass GstAmcAudioDecClass;

struct _GstAmcAudioDec
{
  GstAudioDecoder parent;

  /* < private > */
  GstAmcCodec *codec;

  GstCaps *input_caps;
  GList *codec_datas;
  gboolean input_caps_changed;
  gint spf;

  /* For collecting complete frames for the output */
  GstAdapter *output_adapter;

  /* Output format of the codec */
  GstAudioInfo info;
  /* AMC positions, might need reordering */
  GstAudioChannelPosition positions[64];
  gboolean needs_reorder;
  gint reorder_map[64];

  /* TRUE if the component is configured and saw
   * the first buffer */
  gboolean started;
  gboolean flushing;

  GstClockTime last_upstream_ts;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining;
  /* TRUE if the component is drained currently */
  gboolean drained;

  GstFlowReturn downstream_flow_ret;
};

struct _GstAmcAudioDecClass
{
  GstAudioDecoderClass parent_class;

  const GstAmcCodecInfo *codec_info;
};

GType gst_amc_audio_dec_get_type (void);

G_END_DECLS

#endif /* __GST_AMC_AUDIO_DEC_H__ */
