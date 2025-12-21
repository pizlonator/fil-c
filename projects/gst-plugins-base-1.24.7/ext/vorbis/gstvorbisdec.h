/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_VORBIS_DEC_H__
#define __GST_VORBIS_DEC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>
#include "gstvorbisdeclib.h"

G_BEGIN_DECLS

#define GST_TYPE_VORBIS_DEC \
  (gst_vorbis_dec_get_type())
#define GST_VORBIS_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VORBIS_DEC,GstVorbisDec))
#define GST_VORBIS_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VORBIS_DEC,GstVorbisDecClass))
#define GST_IS_VORBIS_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VORBIS_DEC))
#define GST_IS_VORBIS_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VORBIS_DEC))

typedef struct _GstVorbisDec GstVorbisDec;
typedef struct _GstVorbisDecClass GstVorbisDecClass;

/**
 * GstVorbisDec:
 *
 * Opaque data structure.
 */
struct _GstVorbisDec {
  GstAudioDecoder element;

  vorbis_dsp_state  vd;
  vorbis_info       vi;
  vorbis_comment    vc;
#ifndef USE_TREMOLO
  vorbis_block      vb;
#endif

  gboolean          initialized;
  GstAudioInfo      info;

  CopySampleFunc    copy_samples;

  GList            *pending_headers;
};

struct _GstVorbisDecClass {
  GstAudioDecoderClass parent_class;
};

GType gst_vorbis_dec_get_type(void);

G_END_DECLS

#endif /* __GST_VORBIS_DEC_H__ */
