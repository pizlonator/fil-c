/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#define GST_TYPE_QSV_DECODER            (gst_qsv_decoder_get_type())
#define GST_QSV_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_QSV_DECODER, GstQsvDecoder))
#define GST_QSV_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_QSV_DECODER, GstQsvDecoderClass))
#define GST_IS_QSV_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_QSV_DECODER))
#define GST_IS_QSV_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_QSV_DECODER))
#define GST_QSV_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_QSV_DECODER, GstQsvDecoderClass))
#define GST_QSV_DECODER_CAST(obj)       ((GstQsvDecoder *)obj)

typedef struct _GstQsvDecoder GstQsvDecoder;
typedef struct _GstQsvDecoderClass GstQsvDecoderClass;
typedef struct _GstQsvDecoderPrivate GstQsvDecoderPrivate;

typedef struct _GstQsvDecoderClassData
{
  guint impl_index;
  gint64 adapter_luid;
  gchar *display_path;
  gchar *description;

  GstCaps *sink_caps;
  GstCaps *src_caps;
} GstQsvDecoderClassData;

struct _GstQsvDecoder
{
  GstVideoDecoder parent;

  GstQsvDecoderPrivate *priv;
};

struct _GstQsvDecoderClass
{
  GstVideoDecoderClass parent_class;

  mfxU32 codec_id;
  mfxU32 impl_index;

  /* DXGI adapter LUID, for Windows */
  gint64 adapter_luid;

  /* VA display device path, for Linux */
  gchar *display_path;

  gboolean    (*set_format)    (GstQsvDecoder * decoder,
                                GstVideoCodecState * state);

  GstBuffer * (*process_input) (GstQsvDecoder * decoder,
                                gboolean need_codec_data,
                                GstBuffer * buffer);
};

GType gst_qsv_decoder_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstQsvDecoder, gst_object_unref)

G_END_DECLS
