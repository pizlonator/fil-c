/* GStreamer
 * Copyright (C) <2014> Sreerenj Balachandran <sreerenjb@gnome.org>
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
#ifndef __GST_WEBPENC_H__
#define __GST_WEBPENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <webp/encode.h>
#include <webp/mux.h>

G_BEGIN_DECLS

#define GST_TYPE_WEBP_ENC \
  (gst_webp_enc_get_type())
#define GST_WEBP_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBP_ENC,GstWebpEnc))
#define GST_WEBP_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WEBP_ENC,GstWebpEncClass))
#define GST_IS_WEBP_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBP_ENC))
#define GST_IS_WEBP_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WEBP_ENC))

typedef struct _GstWebpEnc GstWebpEnc;
typedef struct _GstWebpEncClass GstWebpEncClass;

/**
 * GstWebpEnc:
 *
 * Opaque data structure.
 */
struct _GstWebpEnc
{
  GstVideoEncoder element;

  GstVideoCodecState *input_state;

  gboolean lossless;
  gfloat quality;
  guint speed;
  gint preset;
  gboolean animated;
  guint animation_loops;
  guint32 animation_background_color;

  gboolean use_argb;
  GstVideoFormat rgb_format;

  WebPAnimEncoder *anim_enc;
  int next_timestamp;

  WebPEncCSP webp_color_space;
  struct WebPConfig webp_config;
  struct WebPPicture webp_picture;
  WebPMemoryWriter webp_writer;
};

struct _GstWebpEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_webp_enc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (webpenc);

G_END_DECLS
#endif /* __GST_WEBPENC_H__ */
