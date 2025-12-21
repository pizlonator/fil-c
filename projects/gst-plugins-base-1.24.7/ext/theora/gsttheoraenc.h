/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (c) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
 *      Author : Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
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

#ifndef __GST_THEORAENC_H__
#define __GST_THEORAENC_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/video/gstvideoencoder.h>
#include <theora/theoraenc.h>

G_BEGIN_DECLS

#define GST_TYPE_THEORA_ENC \
  (gst_theora_enc_get_type())
#define GST_THEORA_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_THEORA_ENC,GstTheoraEnc))
#define GST_THEORA_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_THEORA_ENC,GstTheoraEncClass))
#define GST_IS_THEORA_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_THEORA_ENC))
#define GST_IS_THEORA_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_THEORA_ENC))

typedef struct _GstTheoraEnc GstTheoraEnc;
typedef struct _GstTheoraEncClass GstTheoraEncClass;

/**
 * GstTheoraEncMultipassMode:
 * @MULTIPASS_MODE_SINGLE_PASS: Single pass encoding
 * @MULTIPASS_MODE_FIRST_PASS: First pass of two pass encoding
 * @MULTIPASS_MODE_SECOND_PASS: Second pass of two pass encoding
 *
 */
typedef enum
{
  MULTIPASS_MODE_SINGLE_PASS,
  MULTIPASS_MODE_FIRST_PASS,
  MULTIPASS_MODE_SECOND_PASS
} GstTheoraEncMultipassMode;

/**
 * GstTheoraEnc:
 *
 * Opaque data structure.
 */
struct _GstTheoraEnc
{
  GstVideoEncoder element;

  ogg_stream_state to;

  th_enc_ctx *encoder;
  th_info info;
  th_comment comment;
  gboolean initialised;

  gint video_bitrate;           /* bitrate target for Theora video */
  gboolean bitrate_changed;
  gint video_quality;           /* Theora quality selector 0 = low, 63 = high */
  gboolean quality_changed;
  gboolean keyframe_auto;
  gint keyframe_freq;
  gint keyframe_force;

  GstVideoCodecState *input_state;

  gint width, height;
  gint fps_n, fps_d;

  guint packetno;
  guint64 bytes_out;
  guint64 granulepos_offset;
  guint64 timestamp_offset;
  guint64 pfn_offset;

  gint speed_level;
  gboolean vp3_compatible;
  gboolean drop_frames;
  gboolean cap_overflow;
  gboolean cap_underflow;
  int rate_buffer;

  GstTheoraEncMultipassMode multipass_mode;
  GIOChannel *multipass_cache_fd;
  GstAdapter *multipass_cache_adapter;
  gchar *multipass_cache_file;
};

struct _GstTheoraEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_theora_enc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (theoraenc);

G_END_DECLS

#endif /* __GST_THEORAENC_H__ */

