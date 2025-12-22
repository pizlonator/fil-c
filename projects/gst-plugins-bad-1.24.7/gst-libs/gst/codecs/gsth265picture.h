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

#ifndef __GST_H265_PICTURE_H__
#define __GST_H265_PICTURE_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The CODECs library is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>
#include <gst/codecs/codecs-prelude.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_H265_PICTURE     (gst_h265_picture_get_type())
#define GST_IS_H265_PICTURE(obj)  (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_H265_PICTURE))
#define GST_H265_PICTURE(obj)     ((GstH265Picture *)obj)
#define GST_H265_PICTURE_CAST(obj) (GST_H265_PICTURE(obj))

typedef struct _GstH265Slice GstH265Slice;
typedef struct _GstH265Picture GstH265Picture;

#define GST_H265_DPB_MAX_SIZE 16

struct _GstH265Slice
{
  GstH265SliceHdr header;

  /* parsed nal unit (doesn't take ownership of raw data) */
  GstH265NalUnit nalu;

  /*< private >*/
  gboolean rap_pic_flag;
  gboolean no_rasl_output_flag;
  gboolean no_output_of_prior_pics_flag;
  gboolean clear_dpb;
  gboolean intra_pic_flag;
};

struct _GstH265Picture
{
  /*< private >*/
  GstCodecPicture parent;

  GstH265SliceType type;

  gint pic_order_cnt;
  gint pic_order_cnt_msb;
  gint pic_order_cnt_lsb;

  guint32 pic_latency_cnt;      /* PicLatencyCount */

  gboolean output_flag;
  gboolean NoRaslOutputFlag;
  gboolean NoOutputOfPriorPicsFlag;
  gboolean RapPicFlag;           /* nalu type between 16 and 21 */
  gboolean IntraPicFlag;         /* Intra pic (only Intra slices) */

  gboolean ref;
  gboolean long_term;
  gboolean needed_for_output;

  /* from picture timing SEI */
  GstH265SEIPicStructType pic_struct;
  guint8 source_scan_type;
  guint8 duplicate_flag;

  GstVideoBufferFlags buffer_flags;
};

GST_CODECS_API
GType gst_h265_picture_get_type (void);

GST_CODECS_API
GstH265Picture * gst_h265_picture_new (void);

static inline GstH265Picture *
gst_h265_picture_ref (GstH265Picture * picture)
{
  return (GstH265Picture *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (picture));
}

static inline void
gst_h265_picture_unref (GstH265Picture * picture)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (picture));
}

static inline gboolean
gst_h265_picture_replace (GstH265Picture ** old_picture,
    GstH265Picture * new_picture)
{
  return gst_mini_object_replace ((GstMiniObject **) old_picture,
      (GstMiniObject *) new_picture);
}

static inline void
gst_clear_h265_picture (GstH265Picture ** picture)
{
  if (picture && *picture) {
    gst_h265_picture_unref (*picture);
    *picture = NULL;
  }
}

static inline void
gst_h265_picture_set_user_data (GstH265Picture * picture, gpointer user_data,
    GDestroyNotify notify)
{
  gst_codec_picture_set_user_data (GST_CODEC_PICTURE (picture),
      user_data, notify);
}

static inline gpointer
gst_h265_picture_get_user_data (GstH265Picture * picture)
{
  return gst_codec_picture_get_user_data (GST_CODEC_PICTURE (picture));
}

static inline void
gst_h265_picture_set_discont_state (GstH265Picture * picture,
    GstVideoCodecState * discont_state)
{
  gst_codec_picture_set_discont_state (GST_CODEC_PICTURE (picture),
      discont_state);
}

/*******************
 * GstH265Dpb *
 *******************/
typedef struct _GstH265Dpb GstH265Dpb;

GST_CODECS_API
GstH265Dpb * gst_h265_dpb_new (void);

GST_CODECS_API
void  gst_h265_dpb_set_max_num_pics (GstH265Dpb * dpb,
                                     gint max_num_pics);

GST_CODECS_API
gint gst_h265_dpb_get_max_num_pics  (GstH265Dpb * dpb);

GST_CODECS_API
void  gst_h265_dpb_free             (GstH265Dpb * dpb);

GST_CODECS_API
void  gst_h265_dpb_clear            (GstH265Dpb * dpb);

GST_CODECS_API
void  gst_h265_dpb_add              (GstH265Dpb * dpb,
                                     GstH265Picture * picture);

GST_CODECS_API
void  gst_h265_dpb_delete_unused    (GstH265Dpb * dpb);

GST_CODECS_API
gint  gst_h265_dpb_num_ref_pictures (GstH265Dpb * dpb);

GST_CODECS_API
void  gst_h265_dpb_mark_all_non_ref (GstH265Dpb * dpb);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_get_ref_by_poc       (GstH265Dpb * dpb,
                                                    gint poc);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_get_ref_by_poc_lsb   (GstH265Dpb * dpb,
                                                    gint poc_lsb);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_get_short_ref_by_poc (GstH265Dpb * dpb,
                                                    gint poc);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_get_long_ref_by_poc  (GstH265Dpb * dpb,
                                                    gint poc);

GST_CODECS_API
GArray * gst_h265_dpb_get_pictures_all         (GstH265Dpb * dpb);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_get_picture      (GstH265Dpb * dpb,
                                                guint32 system_frame_number);

GST_CODECS_API
gint  gst_h265_dpb_get_size   (GstH265Dpb * dpb);

GST_CODECS_API
gboolean gst_h265_dpb_needs_bump (GstH265Dpb * dpb,
                                  guint max_num_reorder_pics,
                                  guint max_latency_increase,
                                  guint max_dec_pic_buffering);

GST_CODECS_API
GstH265Picture * gst_h265_dpb_bump (GstH265Dpb * dpb,
                                    gboolean drain);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstH265Picture, gst_h265_picture_unref)

G_END_DECLS

#endif /* __GST_H265_PICTURE_H__ */
