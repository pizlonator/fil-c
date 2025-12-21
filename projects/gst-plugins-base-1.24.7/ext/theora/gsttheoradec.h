/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <in7y118@public.uni-hamburg.de>
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

#ifndef __GST_THEORADEC_H__
#define __GST_THEORADEC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include <theora/theoradec.h>
#include <string.h>

G_BEGIN_DECLS

#define GST_TYPE_THEORA_DEC \
  (gst_theora_dec_get_type())
#define GST_THEORA_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_THEORA_DEC,GstTheoraDec))
#define GST_THEORA_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_THEORA_DEC,GstTheoraDecClass))
#define GST_IS_THEORA_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_THEORA_DEC))
#define GST_IS_THEORA_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_THEORA_DEC))

typedef struct _GstTheoraDec GstTheoraDec;
typedef struct _GstTheoraDecClass GstTheoraDecClass;

/**
 * GstTheoraDec:
 *
 * Opaque object data structure.
 */
struct _GstTheoraDec
{
  GstVideoDecoder element;

  /* theora decoder state */
  th_dec_ctx *decoder;
  //theora_state state;
  th_setup_info *setup;
  th_info info;
  th_comment comment;

  gboolean have_header;

  gboolean need_keyframe;
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  /* telemetry debugging options */
  gint telemetry_mv;
  gint telemetry_mbmode;
  gint telemetry_qi;
  gint telemetry_bits;
};

struct _GstTheoraDecClass
{
  GstVideoDecoderClass parent_class;
};

GType gst_theora_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (theoradec);

G_END_DECLS

#endif /* __GST_THEORADEC_H__ */
