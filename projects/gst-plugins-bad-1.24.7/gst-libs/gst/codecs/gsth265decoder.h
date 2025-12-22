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

#ifndef __GST_H265_DECODER_H__
#define __GST_H265_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/codecs/gsth265picture.h>

G_BEGIN_DECLS

#define GST_TYPE_H265_DECODER            (gst_h265_decoder_get_type())
#define GST_H265_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_H265_DECODER,GstH265Decoder))
#define GST_H265_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_H265_DECODER,GstH265DecoderClass))
#define GST_H265_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_H265_DECODER,GstH265DecoderClass))
#define GST_IS_H265_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_H265_DECODER))
#define GST_IS_H265_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_H265_DECODER))
#define GST_H265_DECODER_CAST(obj)       ((GstH265Decoder*)obj)

typedef struct _GstH265Decoder GstH265Decoder;
typedef struct _GstH265DecoderClass GstH265DecoderClass;
typedef struct _GstH265DecoderPrivate GstH265DecoderPrivate;

/**
 * GstH265Decoder:
 *
 * The opaque #GstH265Decoder data structure.
 */
struct _GstH265Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;

  GstH265Picture *RefPicSetStCurrBefore[16];
  GstH265Picture *RefPicSetStCurrAfter[16];
  GstH265Picture *RefPicSetStFoll[16];
  GstH265Picture *RefPicSetLtCurr[16];
  GstH265Picture *RefPicSetLtFoll[16];

  guint NumPocStCurrBefore;
  guint NumPocStCurrAfter;
  guint NumPocStFoll;
  guint NumPocLtCurr;
  guint NumPocLtFoll;
  guint NumPicTotalCurr;

  /*< private >*/
  GstH265DecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstH265DecoderClass:
 *
 * The opaque #GstH265DecoderClass data structure.
 */
struct _GstH265DecoderClass
{
  GstVideoDecoderClass parent_class;

  /**
   * GstH265DecoderClass::new_sequence:
   * @decoder: a #GstH265Decoder
   * @sps: a #GstH265SPS
   * @max_dpb_size: the size of dpb including preferred output delay
   *   by subclass reported via get_preferred_output_delay method.
   *
   * Notifies subclass of video sequence update
   */
  GstFlowReturn (*new_sequence)     (GstH265Decoder * decoder,
                                     const GstH265SPS * sps,
                                     gint max_dpb_size);

  /**
   * GstH265DecoderClass::new_picture:
   * @decoder: a #GstH265Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstH265Picture
   *
   * Optional. Called whenever new #GstH265Picture is created.
   * Subclass can set implementation specific user data
   * on the #GstH265Picture via gst_h265_picture_set_user_data
   */
  GstFlowReturn (*new_picture)      (GstH265Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstH265Picture * picture);

  /**
   * GstH265DecoderClass::start_picture:
   * @decoder: a #GstH265Decoder
   * @picture: (transfer none): a #GstH265Picture
   * @slice: (transfer none): a #GstH265Slice
   * @dpb: (transfer none): a #GstH265Dpb
   *
   * Optional. Called per one #GstH265Picture to notify subclass to prepare
   * decoding process for the #GstH265Picture
   */
  GstFlowReturn (*start_picture)    (GstH265Decoder * decoder,
                                     GstH265Picture * picture,
                                     GstH265Slice * slice,
                                     GstH265Dpb * dpb);

  /**
   * GstH265DecoderClass::decode_slice:
   * @decoder: a #GstH265Decoder
   * @picture: (transfer none): a #GstH265Picture
   * @slice: (transfer none): a #GstH265Slice
   * @ref_pic_list0: (element-type GstH265Picture) (transfer none):
   *    an array of #GstH265Picture pointers
   * @ref_pic_list1: (element-type GstH265Picture) (transfer none):
   *    an array of #GstH265Picture pointers
   *
   * Provides per slice data with parsed slice header and required raw bitstream
   * for subclass to decode it. If gst_h265_decoder_set_process_ref_pic_lists()
   * is called with %TRUE by the subclass, @ref_pic_list0 and @ref_pic_list1
   * are non-%NULL.
   */
  GstFlowReturn (*decode_slice)     (GstH265Decoder * decoder,
                                     GstH265Picture * picture,
                                     GstH265Slice * slice,
                                     GArray * ref_pic_list0,
                                     GArray * ref_pic_list1);

  /**
   * GstH265DecoderClass::end_picture:
   * @decoder: a #GstH265Decoder
   * @picture: (transfer none): a #GstH265Picture
   *
   * Optional. Called per one #GstH265Picture to notify subclass to finish
   * decoding process for the #GstH265Picture
   */
  GstFlowReturn (*end_picture)      (GstH265Decoder * decoder,
                                     GstH265Picture * picture);

  /**
   * GstH265DecoderClass:output_picture:
   * @decoder: a #GstH265Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstH265Picture
   *
   * Called with a #GstH265Picture which is required to be outputted.
   */
  GstFlowReturn (*output_picture)   (GstH265Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstH265Picture * picture);

  /**
   * GstH265DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstH265Decoder
   * @live: whether upstream is live or not
   *
   * Optional. Called by baseclass to query whether delaying output is
   * preferred by subclass or not.
   *
   * Returns: the number of perferred delayed output frame
   *
   * Since: 1.22
   */
  guint (*get_preferred_output_delay)   (GstH265Decoder * decoder,
                                         gboolean live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstH265Decoder, gst_object_unref)

GST_CODECS_API
GType gst_h265_decoder_get_type (void);

GST_CODECS_API
void gst_h265_decoder_set_process_ref_pic_lists (GstH265Decoder * decoder,
                                                 gboolean process);

GST_CODECS_API
GstH265Picture * gst_h265_decoder_get_picture   (GstH265Decoder * decoder,
                                                 guint32 system_frame_number);

G_END_DECLS

#endif /* __GST_H265_DECODER_H__ */
