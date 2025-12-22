/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
#include <gst/codecs/codecs-prelude.h>

G_BEGIN_DECLS

typedef struct _GstCodecPicture GstCodecPicture;
#define GST_CODEC_PICTURE(p) ((GstCodecPicture *) p)

/**
 * GST_CODEC_PICTURE_FRAME_NUMBER:
 * @picture: a #GstCodecPicture
 *
 * Gets access to the system_frame_number field of @picture
 *
 * Since: 1.24
 */
#define GST_CODEC_PICTURE_FRAME_NUMBER(picture) \
    GST_CODEC_PICTURE(picture)->system_frame_number

/**
 * GST_CODEC_PICTURE_COPY_FRAME_NUMBER:
 * @dst: a #GstCodecPicture
 * @src: a #GstCodecPicture
 *
 * Copy system_frame_number of @src to @dst
 *
 * Since: 1.24
 */
#define GST_CODEC_PICTURE_COPY_FRAME_NUMBER(dst,src) G_STMT_START { \
    GST_CODEC_PICTURE(dst)->system_frame_number = \
    GST_CODEC_PICTURE(src)->system_frame_number; \
} G_STMT_END

/**
 * GstCodecPicture:
 *
 * Base struct for coded picture representation
 *
 * Since: 1.24
 */
struct _GstCodecPicture
{
  /*< private >*/
  GstMiniObject parent;

  guint32 system_frame_number;
  GstVideoCodecState *discont_state;

  gpointer user_data;
  GDestroyNotify notify;

  gpointer _gst_reserved[GST_PADDING];
};

GST_CODECS_API
void      gst_codec_picture_set_user_data (GstCodecPicture * picture,
                                           gpointer user_data,
                                           GDestroyNotify notify);

GST_CODECS_API
gpointer  gst_codec_picture_get_user_data (GstCodecPicture * picture);

GST_CODECS_API
void      gst_codec_picture_set_discont_state (GstCodecPicture * picture,
                                               GstVideoCodecState * discont_state);

static inline GstCodecPicture *
gst_codec_picture_ref (GstCodecPicture * picture)
{
  return (GstCodecPicture *) gst_mini_object_ref ((GstMiniObject *) picture);
}

static inline void
gst_codec_picture_unref (GstCodecPicture * picture)
{
  gst_mini_object_unref ((GstMiniObject *) picture);
}

static inline void
gst_clear_codec_picture (GstCodecPicture ** picture)
{
  gst_clear_mini_object ((GstMiniObject **) picture);
}

G_END_DECLS
