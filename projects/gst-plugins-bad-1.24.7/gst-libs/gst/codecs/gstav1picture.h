/* GStreamer
 * Copyright (C) 2020 He Junyan <junyan.he@intel.com>
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

#ifndef __GST_AV1_PICTURE_H__
#define __GST_AV1_PICTURE_H__

#include <gst/codecs/codecs-prelude.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/codecparsers/gstav1parser.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

/**
 * GST_TYPE_AV1_PICTURE:
 *
 * Since: 1.20
 */
#define GST_TYPE_AV1_PICTURE     (gst_av1_picture_get_type())
/**
 * GST_IS_AV1_PICTURE:
 *
 * Since: 1.20
 */
#define GST_IS_AV1_PICTURE(obj)  (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_AV1_PICTURE))
/**
 * GST_AV1_PICTURE:
 *
 * Since: 1.20
 */
#define GST_AV1_PICTURE(obj)     ((GstAV1Picture *)obj)

typedef struct _GstAV1Picture GstAV1Picture;
typedef struct _GstAV1Tile GstAV1Tile;

/**
 * GstAV1Tile:
 *
 * Since: 1.20
 */
struct _GstAV1Tile
{
  GstAV1TileGroupOBU tile_group;
  /* raw data and size of tile group (does not have ownership) */
  GstAV1OBU obu;
};

/**
 * GstAV1Picture:
 *
 * Since: 1.20
 */
struct _GstAV1Picture
{
  /*< private >*/
  GstCodecPicture parent;

  GstAV1FrameHeaderOBU frame_hdr;

  /* from OBU header */
  guint8 temporal_id;
  guint8 spatial_id;

  /* copied from parser */
  guint32 display_frame_id;
  gboolean show_frame;
  gboolean showable_frame;
  gboolean apply_grain;
};

GST_CODECS_API
GType gst_av1_picture_get_type (void);

GST_CODECS_API
GstAV1Picture * gst_av1_picture_new (void);

static inline GstAV1Picture *
gst_av1_picture_ref (GstAV1Picture * picture)
{
  return (GstAV1Picture *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (picture));
}

static inline void
gst_av1_picture_unref (GstAV1Picture * picture)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (picture));
}

static inline gboolean
gst_av1_picture_replace (GstAV1Picture ** old_picture,
    GstAV1Picture * new_picture)
{
  return gst_mini_object_replace ((GstMiniObject **) old_picture,
      (GstMiniObject *) new_picture);
}

static inline void
gst_clear_av1_picture (GstAV1Picture ** picture)
{
  if (picture && *picture) {
    gst_av1_picture_unref (*picture);
    *picture = NULL;
  }
}

static inline void
gst_av1_picture_set_user_data (GstAV1Picture * picture, gpointer user_data,
    GDestroyNotify notify)
{
  gst_codec_picture_set_user_data (GST_CODEC_PICTURE (picture),
      user_data, notify);
}

static inline gpointer
gst_av1_picture_get_user_data (GstAV1Picture * picture)
{
  return gst_codec_picture_get_user_data (GST_CODEC_PICTURE (picture));
}

static inline void
gst_av1_picture_set_discont_state (GstAV1Picture * picture,
    GstVideoCodecState * discont_state)
{
  gst_codec_picture_set_discont_state (GST_CODEC_PICTURE (picture),
      discont_state);
}

/*******************
 * GstAV1Dpb *
 *******************/
typedef struct _GstAV1Dpb GstAV1Dpb;

/**
 * GstAV1Dpb:
 *
 * Since: 1.20
 */
struct _GstAV1Dpb
{
  GstAV1Picture *pic_list[GST_AV1_NUM_REF_FRAMES];
};

GST_CODECS_API
GstAV1Dpb * gst_av1_dpb_new (void);

GST_CODECS_API
void  gst_av1_dpb_free             (GstAV1Dpb * dpb);

GST_CODECS_API
void  gst_av1_dpb_clear            (GstAV1Dpb * dpb);

GST_CODECS_API
void  gst_av1_dpb_add              (GstAV1Dpb * dpb,
                                    GstAV1Picture * picture);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstAV1Picture, gst_av1_picture_unref)

G_END_DECLS

#endif /* __GST_AV1_PICTURE_H__ */
