/* GStreamer
 * Copyright (C) <2013> Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) <2013> Intel Corporation
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

#ifndef __GST_WEBP_DEC_H__
#define __GST_WEBP_DEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbytereader.h>

#include <stdio.h>
#include <webp/decode.h>

G_BEGIN_DECLS

#define GST_TYPE_WEBP_DEC \
  (gst_webp_dec_get_type())
#define GST_WEBP_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBP_DEC,GstWebPDec))
#define GST_WEBP_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WEBP_DEC,GstWebPDecClass))
#define GST_IS_WEBP_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBP_DEC))
#define GST_IS_WEBP_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WEBP_DEC))

typedef struct _GstWebPDec           GstWebPDec;
typedef struct _GstWebPDecClass      GstWebPDecClass;

struct _GstWebPDec {
  GstVideoDecoder decoder;

  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  gboolean saw_header;
  guint frame_size;

  /* properties */
  gboolean bypass_filtering;
  gboolean no_fancy_upsampling;
  gboolean use_threads;

  WEBP_CSP_MODE colorspace;
  WebPDecoderConfig config;
};

struct _GstWebPDecClass {
  GstVideoDecoderClass decoder_class;
};

GType gst_webp_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (webpdec);

G_END_DECLS

#endif /* __GST_WEBP_DEC_H__ */
