/* GStreamer Adaptive Multi-Rate Wide-Band (AMR-WB) plugin
 * Copyright (C) 2006 Edgard Lima <edgard.lima@gmail.com>
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

#ifndef __GST_VOAMRWBENC_H__
#define __GST_VOAMRWBENC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include <vo-amrwbenc/enc_if.h>

G_BEGIN_DECLS

#define GST_TYPE_VOAMRWBENC			\
  (gst_voamrwbenc_get_type())
#define GST_VOAMRWBENC(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VOAMRWBENC, GstVoAmrWbEnc))
#define GST_VOAMRWBENC_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VOAMRWBENC, GstVoAmrWbEncClass))
#define GST_IS_VOAMRWBENC(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VOAMRWBENC))
#define GST_IS_VOAMRWBENC_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VOAMRWBENC))

typedef struct _GstVoAmrWbEnc GstVoAmrWbEnc;
typedef struct _GstVoAmrWbEncClass GstVoAmrWbEncClass;

struct _GstVoAmrWbEnc {
  GstAudioEncoder element;

  /* library handle */
  void *handle;

  /* input settings */
  gint bandmode;
  gint channels, rate;
};

struct _GstVoAmrWbEncClass {
  GstAudioEncoderClass parent_class;
};

GType gst_voamrwbenc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (voamrwbenc);

G_END_DECLS

#endif /* __GST_VOAMRWBENC_H__ */
