/*
 * GStreamer
 * Copyright (C) 2018 Edward Hervey <edward@centricular.com>
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

#ifndef __GST_LINE21DECODER_H__
#define __GST_LINE21DECODER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-anc.h>
#include "decoder.h"

G_BEGIN_DECLS
#define GST_TYPE_LINE21DECODER \
  (gst_line_21_decoder_get_type())
#define GST_LINE21DECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LINE21DECODER,GstLine21Decoder))
#define GST_LINE21DECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LINE21DECODER,GstLine21DecoderClass))
#define GST_IS_LINE21DECODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LINE21DECODER))
#define GST_IS_LINE21DECODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LINE21DECODER))

typedef struct _GstLine21Decoder GstLine21Decoder;
typedef struct _GstLine21DecoderClass GstLine21DecoderClass;

typedef enum {
  GST_LINE_21_DECODER_MODE_ADD,
  GST_LINE_21_DECODER_MODE_DROP,
  GST_LINE_21_DECODER_MODE_REPLACE,
} GstLine21DecoderMode;

struct _GstLine21Decoder
{
  GstVideoFilter parent;

  gboolean compatible_format;
  vbi_raw_decoder zvbi_decoder;

  /* Offset (in lines) to "line 21" in the incoming stream */
  gint line21_offset;

  /* Maximum number of lines to probe when looking for CC */
  gint max_line_probes;

  /* Whether input data is v210 and needs to be converted before
   * processing */
  gboolean convert_v210;
  guint8 *converted_lines;
  
  GstVideoInfo *info;

  gboolean ntsc_only;
  GstLine21DecoderMode mode;
};

struct _GstLine21DecoderClass
{
  GstVideoFilterClass parent_class;
};

GType gst_line_21_decoder_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (line21decoder);

G_END_DECLS
#endif /* __GST_LINE21DECODER_H__ */
