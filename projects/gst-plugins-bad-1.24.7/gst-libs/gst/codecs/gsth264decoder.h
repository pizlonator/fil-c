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

#ifndef __GST_H264_DECODER_H__
#define __GST_H264_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/codecs/gsth264picture.h>

G_BEGIN_DECLS

#define GST_TYPE_H264_DECODER            (gst_h264_decoder_get_type())
#define GST_H264_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_H264_DECODER,GstH264Decoder))
#define GST_H264_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_H264_DECODER,GstH264DecoderClass))
#define GST_H264_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_H264_DECODER,GstH264DecoderClass))
#define GST_IS_H264_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_H264_DECODER))
#define GST_IS_H264_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_H264_DECODER))
#define GST_H264_DECODER_CAST(obj)       ((GstH264Decoder*)obj)

/**
 * GstH264DecoderCompliance:
 * @GST_H264_DECODER_COMPLIANCE_AUTO: The decoder behavior is
 *     automatically choosen.
 * @GST_H264_DECODER_COMPLIANCE_STRICT: The decoder behavior strictly
 *     conforms to the SPEC. All the decoder behaviors conform to the
 *     SPEC, not including any nonstandard behavior which is not
 *     mentioned in the SPEC.
 * @GST_H264_DECODER_COMPLIANCE_NORMAL: The decoder behavior normally
 *     conforms to the SPEC. Most behaviors conform to the SPEC but
 *     including some nonstandard features which are widely used or
 *     often used in the industry practice. This meets the request of
 *     real streams and usages, but may not 100% conform to the
 *     SPEC. It has very low risk. E.g., we will output pictures
 *     without waiting DPB being full for the lower latency, which may
 *     cause B frame disorder when there are reference frames with
 *     smaller POC after it in decoder order. And the baseline profile
 *     may be mapped to the constrained-baseline profile, but it may
 *     have problems when a real baseline stream comes with FMO or
 *     ASO.
 * @GST_H264_DECODER_COMPLIANCE_FLEXIBLE: The decoder behavior
 *     flexibly conforms to the SPEC. It uses the nonstandard features
 *     more aggressively in order to get better performance(for
 *     example, lower latency). It may change the result of the
 *     decoder and should be used carefully. Besides including all
 *     risks in *normal* mode, it has more risks, such as frames
 *     disorder when reference frames POC decrease in decoder order.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_H264_DECODER_COMPLIANCE_AUTO,
  GST_H264_DECODER_COMPLIANCE_STRICT,
  GST_H264_DECODER_COMPLIANCE_NORMAL,
  GST_H264_DECODER_COMPLIANCE_FLEXIBLE
} GstH264DecoderCompliance;

#define GST_TYPE_H264_DECODER_COMPLIANCE (gst_h264_decoder_compliance_get_type())

GST_CODECS_API
GType gst_h264_decoder_compliance_get_type (void);

typedef struct _GstH264Decoder GstH264Decoder;
typedef struct _GstH264DecoderClass GstH264DecoderClass;
typedef struct _GstH264DecoderPrivate GstH264DecoderPrivate;

/**
 * GstH264Decoder:
 *
 * The opaque #GstH264Decoder data structure.
 */
struct _GstH264Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;

  /*< private >*/
  GstH264DecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstH264DecoderClass:
 *
 * The opaque #GstH264DecoderClass data structure.
 */
struct _GstH264DecoderClass
{
  /*< private >*/
  GstVideoDecoderClass parent_class;

  /**
   * GstH264DecoderClass::new_sequence:
   * @decoder: a #GstH264Decoder
   * @sps: a #GstH264SPS
   * @max_dpb_size: the size of dpb including preferred output delay
   *   by subclass reported via get_preferred_output_delay method.
   *
   * Notifies subclass of SPS update
   */
  GstFlowReturn (*new_sequence)     (GstH264Decoder * decoder,
                                     const GstH264SPS * sps,
                                     gint max_dpb_size);

  /**
   * GstH264DecoderClass::new_picture:
   * @decoder: a #GstH264Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstH264Picture
   *
   * Optional. Called whenever new #GstH264Picture is created.
   * Subclass can set implementation specific user data
   * on the #GstH264Picture via gst_h264_picture_set_user_data
   */
  GstFlowReturn (*new_picture)      (GstH264Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstH264Picture * picture);

  /**
   * GstH264DecoderClass::new_field_picture:
   * @decoder: a #GstH264Decoder
   * @first_field: (transfer none): the first field #GstH264Picture already decoded
   * @second_field: (transfer none): a #GstH264Picture for the second field
   *
   * Called when a new field picture is created for interlaced field picture.
   * Subclass can attach implementation specific user data on @second_field via
   * gst_h264_picture_set_user_data
   *
   * Since: 1.20
   */
  GstFlowReturn (*new_field_picture)  (GstH264Decoder * decoder,
                                       GstH264Picture * first_field,
                                       GstH264Picture * second_field);

  /**
   * GstH264DecoderClass::start_picture:
   * @decoder: a #GstH264Decoder
   * @picture: (transfer none): a #GstH264Picture
   * @slice: (transfer none): a #GstH264Slice
   * @dpb: (transfer none): a #GstH264Dpb
   *
   * Optional. Called per one #GstH264Picture to notify subclass to prepare
   * decoding process for the #GstH264Picture
   */
  GstFlowReturn (*start_picture)    (GstH264Decoder * decoder,
                                     GstH264Picture * picture,
                                     GstH264Slice * slice,
                                     GstH264Dpb * dpb);

  /**
   * GstH264DecoderClass::decode_slice:
   * @decoder: a #GstH264Decoder
   * @picture: (transfer none): a #GstH264Picture
   * @slice: (transfer none): a #GstH264Slice
   * @ref_pic_list0: (element-type GstH264Picture) (transfer none):
   *    an array of #GstH264Picture pointers
   * @ref_pic_list1: (element-type GstH264Picture) (transfer none):
   *    an array of #GstH264Picture pointers
   *
   * Provides per slice data with parsed slice header and required raw bitstream
   * for subclass to decode it. If gst_h264_decoder_set_process_ref_pic_lists()
   * is called with %TRUE by the subclass, @ref_pic_list0 and @ref_pic_list1
   * are non-%NULL.
   * In case of interlaced stream, @ref_pic_list0 and @ref_pic_list1 will
   * contain only the first field of complementary reference field pair
   * if currently being decoded picture is a frame picture. Subclasses might
   * need to retrive the other field (i.e., the second field) of the picture
   * if needed.
   */
  GstFlowReturn (*decode_slice)     (GstH264Decoder * decoder,
                                     GstH264Picture * picture,
                                     GstH264Slice * slice,
                                     GArray * ref_pic_list0,
                                     GArray * ref_pic_list1);

  /**
   * GstH264DecoderClass::end_picture:
   * @decoder: a #GstH264Decoder
   * @picture: (transfer none): a #GstH264Picture
   *
   * Optional. Called per one #GstH264Picture to notify subclass to finish
   * decoding process for the #GstH264Picture
   */
  GstFlowReturn (*end_picture)      (GstH264Decoder * decoder,
                                     GstH264Picture * picture);

  /**
   * GstH264DecoderClass::output_picture:
   * @decoder: a #GstH264Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstH264Picture
   *
   * Called with a #GstH264Picture which is required to be outputted.
   * The #GstVideoCodecFrame must be consumed by subclass.
   */
  GstFlowReturn (*output_picture)   (GstH264Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstH264Picture * picture);

  /**
   * GstH264DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstH264Decoder
   * @live: whether upstream is live or not
   *
   * Optional. Called by baseclass to query whether delaying output is
   * preferred by subclass or not.
   *
   * Returns: the number of perferred delayed output frame
   *
   * Since: 1.20
   */
  guint (*get_preferred_output_delay)   (GstH264Decoder * decoder,
                                         gboolean live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstH264Decoder, gst_object_unref)

GST_CODECS_API
GType gst_h264_decoder_get_type (void);

GST_CODECS_API
void gst_h264_decoder_set_process_ref_pic_lists (GstH264Decoder * decoder,
                                                 gboolean process);

GST_CODECS_API
GstH264Picture * gst_h264_decoder_get_picture   (GstH264Decoder * decoder,
                                                 guint32 system_frame_number);

G_END_DECLS

#endif /* __GST_H264_DECODER_H__ */
