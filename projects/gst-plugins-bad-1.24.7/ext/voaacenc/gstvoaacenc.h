/* GStreamer AAC encoder plugin
 * Copyright (C) 2011 Kan Hu <kan.hu@linaro.org>
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

#ifndef __GST_VOAACENC_H__
#define __GST_VOAACENC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include <vo-aacenc/voAAC.h>
#include <vo-aacenc/cmnMemory.h>

G_BEGIN_DECLS

#define GST_TYPE_VOAACENC \
  (gst_voaacenc_get_type())
#define GST_VOAACENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VOAACENC, GstVoAacEnc))
#define GST_VOAACENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VOAACENC, GstVoAacEncClass))
#define GST_IS_VOAACENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VOAACENC))
#define GST_IS_VOAACENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VOAACENC))

typedef struct _GstVoAacEnc GstVoAacEnc;
typedef struct _GstVoAacEncClass GstVoAacEncClass;

struct _GstVoAacEnc {
  GstAudioEncoder element;

  gboolean discont;

  /* desired bitrate */
  gint bitrate;

  /* caps */
  gint channels;
  gint rate;
  gint output_format;

  gint inbuf_size;

  /* library handle */
  VO_AUDIO_CODECAPI codec_api;
  VO_HANDLE handle;
  VO_MEM_OPERATOR mem_operator;

};

struct _GstVoAacEncClass {
  GstAudioEncoderClass parent_class;
};

GType gst_voaacenc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (voaacenc);

G_END_DECLS

#endif /* __GST_VOAACENC_H__ */
