/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <mfx.h>
#include "gstqsvutils.h"

G_BEGIN_DECLS

#define GST_TYPE_QSV_ENCODER            (gst_qsv_encoder_get_type())
#define GST_QSV_ENCODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_QSV_ENCODER, GstQsvEncoder))
#define GST_QSV_ENCODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_QSV_ENCODER, GstQsvEncoderClass))
#define GST_IS_QSV_ENCODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_QSV_ENCODER))
#define GST_IS_QSV_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_QSV_ENCODER))
#define GST_QSV_ENCODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_QSV_ENCODER, GstQsvEncoderClass))
#define GST_QSV_ENCODER_CAST(obj)       ((GstQsvEncoder *)obj)

typedef struct _GstQsvEncoder GstQsvEncoder;
typedef struct _GstQsvEncoderClass GstQsvEncoderClass;
typedef struct _GstQsvEncoderPrivate GstQsvEncoderPrivate;

#define GST_TYPE_QSV_CODING_OPTION (gst_qsv_coding_option_get_type())
GType gst_qsv_coding_option_get_type (void);

typedef enum
{
  GST_QSV_ENCODER_RECONFIGURE_NONE,
  GST_QSV_ENCODER_RECONFIGURE_BITRATE,
  GST_QSV_ENCODER_RECONFIGURE_FULL,
} GstQsvEncoderReconfigure;

struct _GstQsvEncoder
{
  GstVideoEncoder parent;

  GstQsvEncoderPrivate *priv;
};

struct _GstQsvEncoderClass
{
  GstVideoEncoderClass parent_class;

  mfxU32 codec_id;
  mfxU32 impl_index;

  /* DXGI adapter LUID, for Windows */
  gint64 adapter_luid;

  /* VA display device path, for Linux */
  gchar *display_path;

  gboolean (*set_format)       (GstQsvEncoder * encoder,
                                GstVideoCodecState * state,
                                mfxVideoParam * param,
                                GPtrArray * extra_params);

  gboolean (*set_output_state) (GstQsvEncoder * encoder,
                                GstVideoCodecState * state,
                                mfxSession session);

  gboolean (*attach_payload)   (GstQsvEncoder * encoder,
                                GstVideoCodecFrame * frame,
                                GPtrArray * payload);

  GstBuffer * (*create_output_buffer) (GstQsvEncoder * encoder,
                                       mfxBitstream * bitstream);

  GstQsvEncoderReconfigure (*check_reconfigure) (GstQsvEncoder * encoder,
                                                 mfxSession session,
                                                 mfxVideoParam * param,
                                                 GPtrArray * extra_params);
};

GType gst_qsv_encoder_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstQsvEncoder, gst_object_unref)

G_END_DECLS
