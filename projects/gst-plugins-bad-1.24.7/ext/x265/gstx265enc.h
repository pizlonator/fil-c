/* GStreamer H265 encoder plugin
 * Copyright (C) 2005 Michal Benes <michal.benes@itonis.tv>
 * Copyright (C) 2005 Josef Zlomek <josef.zlomek@itonis.tv>
 * Copyright (C) 2014 Thijs Vermeir <thijs.vermeir@barco.com>
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

#ifndef __GST_X265_ENC_H__
#define __GST_X265_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <x265.h>

G_BEGIN_DECLS
#define GST_TYPE_X265_ENC \
  (gst_x265_enc_get_type())
#define GST_X265_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_X265_ENC,GstX265Enc))
#define GST_X265_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_X265_ENC,GstX265EncClass))
#define GST_IS_X265_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_X265_ENC))
#define GST_IS_X265_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_X265_ENC))
typedef struct _GstX265Enc GstX265Enc;
typedef struct _GstX265EncClass GstX265EncClass;

struct _GstX265Enc
{
  GstVideoEncoder element;

  /*< private > */
  x265_encoder *x265enc;
  x265_param x265param;
  GstClockTime dts_offset;
  gboolean push_header;
  const x265_api *api;

  /* List of frame/buffer mapping structs for
   * pending frames */
  GList *pending_frames;

  /* properties */
  guint bitrate;
  gint qp;
  gint log_level;
  gint tune;
  gint speed_preset;
  gint keyintmax;
  GString *option_string_prop;  /* option-string property */
  /*GString *option_string; *//* used by set prop */

  /* input description */
  GstVideoCodecState *input_state;

  /* configuration changed  while playing */
  gboolean reconfig;

  /* from the downstream caps */
  GPtrArray *peer_profiles;
  /*const x265_level_t *peer_level; */
};

struct _GstX265EncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_x265_enc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (x265enc);

G_END_DECLS
#endif /* __GST_X265_ENC_H__ */
