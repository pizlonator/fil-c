
/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gst/codecs/gstav1decoder.h>
#include <gst/codecs/gsth264decoder.h>
#include <gst/codecs/gsth265decoder.h>
#include <gst/codecs/gstmpeg2decoder.h>
#include <gst/codecs/gstvp8decoder.h>
#include <gst/codecs/gstvp9decoder.h>

#include "gstjpegdecoder.h"
#include "gstvadecoder.h"
#include "gstvadevice.h"
#include "gstvaprofile.h"
#include "gstvapluginutils.h"

G_BEGIN_DECLS

#define GST_VA_BASE_DEC(obj) ((GstVaBaseDec *)(obj))
#define GST_VA_BASE_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaBaseDecClass))
#define GST_VA_BASE_DEC_CLASS(klass) ((GstVaBaseDecClass *)(klass))

enum {
  GST_VA_DEC_PROP_DEVICE_PATH = 1,
  GST_VA_DEC_PROP_LAST,
};

typedef struct _GstVaBaseDec GstVaBaseDec;
typedef struct _GstVaBaseDecClass GstVaBaseDecClass;

struct _GstVaBaseDec
{
  /* <private> */
  union
  {
    GstH264Decoder h264;
    GstH265Decoder h265;
    GstMpeg2Decoder mpeg2;
    GstVp8Decoder vp8;
    GstVp9Decoder vp9;
    GstAV1Decoder av1;
    GstJpegDecoder jpeg;
  } parent;

  GstDebugCategory *debug_category;

  GstVaDisplay *display;
  GstVaDecoder *decoder;

  VAProfile profile;
  guint rt_format;
  /* coded or max resolution */
  gint width;
  gint height;

  guint min_buffers;

  GstVideoInfo output_info;
  GstVideoCodecState *output_state;
  GstVideoCodecState *input_state;
  GstBufferPool *other_pool;

  gboolean need_valign;
  GstVideoAlignment valign;

  gboolean copy_frames;

  gboolean apply_video_crop;
  GstVideoConverter *convert;

  gboolean need_negotiation;

  guint32 hacks;
};

struct _GstVaBaseDecClass
{
  /* <private> */
  union
  {
    GstH264DecoderClass h264;
    GstH265DecoderClass h265;
    GstMpeg2DecoderClass mpeg2;
    GstVp8DecoderClass vp8;
    GstVp9DecoderClass vp9;
    GstAV1DecoderClass av1;
  } parent_class;

  GstVaCodecs codec;
  gchar *render_device_path;
  /* The parent class in GType hierarchy */
  GstObjectClass *parent_decoder_class;
};

struct CData
{
  gchar *render_device_path;
  gchar *description;
  GstCaps *sink_caps;
  GstCaps *src_caps;
};

void                  gst_va_base_dec_init                (GstVaBaseDec * base,
                                                           GstDebugCategory * cat);
void                  gst_va_base_dec_class_init          (GstVaBaseDecClass * klass,
                                                           GstVaCodecs codec,
                                                           const gchar * render_device_path,
                                                           GstCaps * sink_caps,
                                                           GstCaps * src_caps,
                                                           GstCaps * doc_src_caps,
                                                           GstCaps * doc_sink_caps);

gboolean              gst_va_base_dec_close               (GstVideoDecoder * decoder);
void                  gst_va_base_dec_get_preferred_format_and_caps_features (GstVaBaseDec * base,
                                                           GstVideoFormat * format,
                                                           GstCapsFeatures ** capsfeatures,
                                                           guint64 * modifier);
gboolean              gst_va_base_dec_copy_output_buffer  (GstVaBaseDec * base,
                                                           GstVideoCodecFrame * codec_frame);
gboolean              gst_va_base_dec_process_output      (GstVaBaseDec * base,
                                                           GstVideoCodecFrame * frame,
                                                           GstVideoCodecState * input_state,
                                                           GstVideoBufferFlags buffer_flags);
GstFlowReturn         gst_va_base_dec_prepare_output_frame (GstVaBaseDec * base,
                                                            GstVideoCodecFrame * frame);
gboolean              gst_va_base_dec_set_output_state    (GstVaBaseDec * base);

G_END_DECLS
