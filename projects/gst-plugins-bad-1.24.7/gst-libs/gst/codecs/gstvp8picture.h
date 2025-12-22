/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_VP8_PICTURE_H__
#define __GST_VP8_PICTURE_H__

#include <gst/codecs/codecs-prelude.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/codecparsers/gstvp8parser.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_VP8_PICTURE     (gst_vp8_picture_get_type())
#define GST_IS_VP8_PICTURE(obj)  (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_VP8_PICTURE))
#define GST_VP8_PICTURE(obj)     ((GstVp8Picture *)obj)
#define GST_VP8_PICTURE_CAST(obj) (GST_VP8_PICTURE(obj))

typedef struct _GstVp8Picture GstVp8Picture;

struct _GstVp8Picture
{
  /*< private >*/
  GstCodecPicture parent;

  GstVp8FrameHdr frame_hdr;

  /* raw data and size (does not have ownership) */
  const guint8 * data;
  gsize size;
};

GST_CODECS_API
GType gst_vp8_picture_get_type (void);

GST_CODECS_API
GstVp8Picture * gst_vp8_picture_new (void);

static inline GstVp8Picture *
gst_vp8_picture_ref (GstVp8Picture * picture)
{
  return (GstVp8Picture *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (picture));
}

static inline void
gst_vp8_picture_unref (GstVp8Picture * picture)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (picture));
}

static inline gboolean
gst_vp8_picture_replace (GstVp8Picture ** old_picture,
    GstVp8Picture * new_picture)
{
  return gst_mini_object_replace ((GstMiniObject **) old_picture,
      (GstMiniObject *) new_picture);
}

static inline void
gst_clear_vp8_picture (GstVp8Picture ** picture)
{
  if (picture && *picture) {
    gst_vp8_picture_unref (*picture);
    *picture = NULL;
  }
}

static inline void
gst_vp8_picture_set_user_data (GstVp8Picture * picture, gpointer user_data,
    GDestroyNotify notify)
{
  gst_codec_picture_set_user_data (GST_CODEC_PICTURE (picture),
      user_data, notify);
}

static inline gpointer
gst_vp8_picture_get_user_data (GstVp8Picture * picture)
{
  return gst_codec_picture_get_user_data (GST_CODEC_PICTURE (picture));
}

static inline void
gst_vp8_picture_set_discont_state (GstVp8Picture * picture,
    GstVideoCodecState * discont_state)
{
  gst_codec_picture_set_discont_state (GST_CODEC_PICTURE (picture),
      discont_state);
}

G_END_DECLS

#endif /* __GST_VP8_PICTURE_H__ */
