/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_H264_PICTURE_H__
#define __GST_H264_PICTURE_H__

#include <gst/codecs/codecs-prelude.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_H264_PICTURE     (gst_h264_picture_get_type())
#define GST_IS_H264_PICTURE(obj)  (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_H264_PICTURE))
#define GST_H264_PICTURE(obj)     ((GstH264Picture *)obj)
#define GST_H264_PICTURE_CAST(obj) (GST_H264_PICTURE(obj))

typedef struct _GstH264Slice GstH264Slice;
typedef struct _GstH264Picture GstH264Picture;

/* As specified in A.3.1 h) and A.3.2 f) */
#define GST_H264_DPB_MAX_SIZE 16

/**
 * GST_H264_PICTURE_IS_REF:
 * @picture: a #GstH264Picture
 *
 * Check whether @picture is used for short-term or long-term reference
 *
 * Since: 1.20
 */
#define GST_H264_PICTURE_IS_REF(picture) \
    ((picture)->ref != GST_H264_PICTURE_REF_NONE)

/**
 * GST_H264_PICTURE_IS_SHORT_TERM_REF:
 * @picture: a #GstH264Picture
 *
 * Check whether @picture is used for short-term reference
 *
 * Since: 1.20
 */
#define GST_H264_PICTURE_IS_SHORT_TERM_REF(picture) \
    ((picture)->ref == GST_H264_PICTURE_REF_SHORT_TERM)

/**
 * GST_H264_PICTURE_IS_LONG_TERM_REF:
 * @picture: a #GstH264Picture
 *
 * Check whether @picture is used for long-term reference
 *
 * Since: 1.20
 */
#define GST_H264_PICTURE_IS_LONG_TERM_REF(picture) \
    ((picture)->ref == GST_H264_PICTURE_REF_LONG_TERM)

/**
 * GST_H264_PICTURE_IS_FRAME:
 * @picture: a #GstH264Picture
 *
 * Check whether @picture is a frame (not a field picture)
 *
 * Since: 1.20
 */
#define GST_H264_PICTURE_IS_FRAME(picture) \
    ((picture)->field == GST_H264_PICTURE_FIELD_FRAME)

struct _GstH264Slice
{
  GstH264SliceHdr header;

  /* parsed nal unit (doesn't take ownership of raw data) */
  GstH264NalUnit nalu;
};

typedef enum
{
  GST_H264_PICTURE_FIELD_FRAME,
  GST_H264_PICTURE_FIELD_TOP_FIELD,
  GST_H264_PICTURE_FIELD_BOTTOM_FIELD,
} GstH264PictureField;

/**
 * GstH264PictureReference:
 * @GST_H264_PICTURE_REF_NONE: Not used for reference picture
 * @GST_H264_PICTURE_REF_SHORT_TERM: Used for short-term reference picture
 * @GST_H264_PICTURE_REF_LONG_TERM: Used for long-term reference picture
 *
 * Since: 1.20
 */
typedef enum
{
  GST_H264_PICTURE_REF_NONE = 0,
  GST_H264_PICTURE_REF_SHORT_TERM,
  GST_H264_PICTURE_REF_LONG_TERM,
} GstH264PictureReference;

struct _GstH264Picture
{
  /*< private >*/
  GstCodecPicture parent;

  GstH264SliceType type;

  guint8 pic_order_cnt_type;  /* SPS */
  gint32 top_field_order_cnt;
  gint32 bottom_field_order_cnt;

  gint pic_order_cnt;
  gint pic_order_cnt_msb;
  gint pic_order_cnt_lsb;
  gint delta_pic_order_cnt_bottom;
  gint delta_pic_order_cnt0;
  gint delta_pic_order_cnt1;

  gint pic_num;
  gint long_term_pic_num;
  gint frame_num;
  gint frame_num_offset;
  gint frame_num_wrap;
  gint long_term_frame_idx;

  gint nal_ref_idc;
  gboolean idr;
  gint idr_pic_id;
  gboolean field_pic_flag;
  GstH264PictureReference ref;
  /* Whether a reference picture. */
  gboolean ref_pic;
  gboolean needed_for_output;
  gboolean mem_mgmt_5;

  gboolean nonexisting;

  GstH264PictureField field;

  GstH264DecRefPicMarking dec_ref_pic_marking;

  /* Set by decoder to trace the number of delayed output pictures */
  guint32 reorder_frame_number;

  /* For interlaced decoding */
  gboolean second_field;
  GstH264Picture * other_field;

  GstVideoBufferFlags buffer_flags;
};

/**
 * GstH264DpbBumpMode:
 * @GST_H264_DPB_BUMP_NORMAL_LATENCY: No latency requirement for DBP bumping.
 * @GST_H264_DPB_BUMP_LOW_LATENCY: Low-latency requirement for DBP bumping.
 * @GST_H264_DPB_BUMP_VERY_LOW_LATENCY: Very low-latency requirement for DBP bumping.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_H264_DPB_BUMP_NORMAL_LATENCY,
  GST_H264_DPB_BUMP_LOW_LATENCY,
  GST_H264_DPB_BUMP_VERY_LOW_LATENCY
} GstH264DpbBumpMode;

GST_CODECS_API
GType gst_h264_picture_get_type (void);

GST_CODECS_API
GstH264Picture * gst_h264_picture_new (void);

static inline GstH264Picture *
gst_h264_picture_ref (GstH264Picture * picture)
{
  return (GstH264Picture *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (picture));
}

static inline void
gst_h264_picture_unref (GstH264Picture * picture)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (picture));
}

static inline gboolean
gst_h264_picture_replace (GstH264Picture ** old_picture,
    GstH264Picture * new_picture)
{
  return gst_mini_object_replace ((GstMiniObject **) old_picture,
      (GstMiniObject *) new_picture);
}

static inline void
gst_clear_h264_picture (GstH264Picture ** picture)
{
  if (picture && *picture) {
    gst_h264_picture_unref (*picture);
    *picture = NULL;
  }
}

static inline void
gst_h264_picture_set_user_data (GstH264Picture * picture, gpointer user_data,
    GDestroyNotify notify)
{
  gst_codec_picture_set_user_data (GST_CODEC_PICTURE (picture),
      user_data, notify);
}

static inline gpointer
gst_h264_picture_get_user_data (GstH264Picture * picture)
{
  return gst_codec_picture_get_user_data (GST_CODEC_PICTURE (picture));
}

static inline void
gst_h264_picture_set_discont_state (GstH264Picture * picture,
    GstVideoCodecState * discont_state)
{
  gst_codec_picture_set_discont_state (GST_CODEC_PICTURE (picture),
      discont_state);
}

/*******************
 * GstH264Dpb *
 *******************/
typedef struct _GstH264Dpb GstH264Dpb;

GST_CODECS_API
GstH264Dpb * gst_h264_dpb_new (void);

GST_CODECS_API
void  gst_h264_dpb_set_max_num_frames (GstH264Dpb * dpb,
                                       gint max_num_frames);

GST_CODECS_API
gint gst_h264_dpb_get_max_num_frames  (GstH264Dpb * dpb);

GST_CODECS_API
void gst_h264_dpb_set_interlaced      (GstH264Dpb * dpb,
                                       gboolean interlaced);

GST_CODECS_API
void gst_h264_dpb_set_max_num_reorder_frames (GstH264Dpb * dpb,
                                              guint32 max_num_reorder_frames);

GST_CODECS_API
guint32 gst_h264_dpb_get_max_num_reorder_frames (GstH264Dpb * dpb);

GST_CODECS_API
gboolean gst_h264_dpb_get_interlaced  (GstH264Dpb * dpb);

GST_CODECS_API
void  gst_h264_dpb_free             (GstH264Dpb * dpb);

GST_CODECS_API
void  gst_h264_dpb_clear            (GstH264Dpb * dpb);

GST_CODECS_API
void  gst_h264_dpb_add              (GstH264Dpb * dpb,
                                     GstH264Picture * picture);

GST_CODECS_API
void  gst_h264_dpb_delete_unused    (GstH264Dpb * dpb);

GST_CODECS_API
gint  gst_h264_dpb_num_ref_frames (GstH264Dpb * dpb);

GST_CODECS_API
void  gst_h264_dpb_mark_all_non_ref (GstH264Dpb * dpb);

GST_CODECS_API
GstH264Picture * gst_h264_dpb_get_short_ref_by_pic_num (GstH264Dpb * dpb,
                                                        gint pic_num);

GST_CODECS_API
GstH264Picture * gst_h264_dpb_get_long_ref_by_long_term_pic_num (GstH264Dpb * dpb,
                                                                 gint long_term_pic_num);

GST_CODECS_API
GstH264Picture * gst_h264_dpb_get_lowest_frame_num_short_ref (GstH264Dpb * dpb);

GST_CODECS_API
void  gst_h264_dpb_get_pictures_short_term_ref (GstH264Dpb * dpb,
                                                gboolean include_non_existing,
                                                gboolean include_second_field,
                                                GArray * out);

GST_CODECS_API
void  gst_h264_dpb_get_pictures_long_term_ref  (GstH264Dpb * dpb,
                                                gboolean include_second_field,
                                                GArray * out);

GST_CODECS_API
GArray * gst_h264_dpb_get_pictures_all         (GstH264Dpb * dpb);

GST_CODECS_API
GstH264Picture * gst_h264_dpb_get_picture      (GstH264Dpb * dpb,
                                                guint32 system_frame_number);

GST_CODECS_API
gint  gst_h264_dpb_get_size   (GstH264Dpb * dpb);

GST_CODECS_API
gboolean gst_h264_dpb_has_empty_frame_buffer   (GstH264Dpb * dpb);

GST_CODECS_API
gboolean gst_h264_dpb_needs_bump (GstH264Dpb * dpb,
                                  GstH264Picture * to_insert,
                                  GstH264DpbBumpMode latency_mode);

GST_CODECS_API
GstH264Picture * gst_h264_dpb_bump (GstH264Dpb * dpb,
                                    gboolean drain);

GST_CODECS_API
void gst_h264_dpb_set_last_output (GstH264Dpb * dpb,
                                   GstH264Picture * picture);

GST_CODECS_API
gboolean         gst_h264_dpb_perform_memory_management_control_operation (GstH264Dpb * dpb,
                                                                           GstH264RefPicMarking *ref_pic_marking,
                                                                           GstH264Picture * picture);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstH264Picture, gst_h264_picture_unref)

G_END_DECLS

#endif /* __GST_H264_PICTURE_H__ */
