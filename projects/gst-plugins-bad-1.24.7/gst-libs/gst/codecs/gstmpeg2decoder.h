/* GStreamer
 * Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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

#ifndef __GST_MPEG2_DECODER_H__
#define __GST_MPEG2_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gstmpegvideoparser.h>
#include <gst/codecs/gstmpeg2picture.h>

G_BEGIN_DECLS

#define GST_TYPE_MPEG2_DECODER            (gst_mpeg2_decoder_get_type())
#define GST_MPEG2_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEG2_DECODER,GstMpeg2Decoder))
#define GST_MPEG2_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEG2_DECODER,GstMpeg2DecoderClass))
#define GST_MPEG2_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_MPEG2_DECODER,GstMpeg2DecoderClass))
#define GST_IS_MPEG2_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEG2_DECODER))
#define GST_IS_MPEG2_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEG2_DECODER))

typedef struct _GstMpeg2Decoder GstMpeg2Decoder;
typedef struct _GstMpeg2DecoderClass GstMpeg2DecoderClass;
typedef struct _GstMpeg2DecoderPrivate GstMpeg2DecoderPrivate;

/**
 * GstMpeg2Decoder:
 *
 * The opaque #GstMpeg2Decoder data structure.
 *
 * Since: 1.20
 */
struct _GstMpeg2Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;

  /*< private >*/
  GstMpeg2DecoderPrivate *priv;

  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstMpeg2DecoderClass:
 */
struct _GstMpeg2DecoderClass
{
  GstVideoDecoderClass parent_class;

  /**
   * GstMpeg2DecoderClass::new_sequence:
   * @decoder: a #GstMpeg2Decoder
   * @seq: a #GstMpegVideoSequenceHdr
   * @seq_ext: a #GstMpegVideoSequenceExt
   * @max_dpb_size: the size of dpb including preferred output delay
   *   by subclass reported via get_preferred_output_delay method.
   *
   * Notifies subclass of SPS update
   *
   * Since: 1.20
   */
  GstFlowReturn (*new_sequence)     (GstMpeg2Decoder * decoder,
                                     const GstMpegVideoSequenceHdr * seq,
                                     const GstMpegVideoSequenceExt * seq_ext,
                                     const GstMpegVideoSequenceDisplayExt * seq_display_ext,
                                     const GstMpegVideoSequenceScalableExt * seq_scalable_ext,
                                     gint max_dpb_size);

  /**
   * GstMpeg2DecoderClass::new_picture:
   * @decoder: a #GstMpeg2Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstMpeg2Picture
   *
   * Optional. Called whenever new #GstMpeg2Picture is created.
   * Subclass can set implementation specific user data
   * on the #GstMpeg2Picture via gst_mpeg2_picture_set_user_data
   *
   * Since: 1.20
   */
  GstFlowReturn (*new_picture)      (GstMpeg2Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstMpeg2Picture * picture);

  /**
   * GstMpeg2DecoderClass::new_field_picture:
   * @decoder: a #GstMpeg2Decoder
   * @first_field: (transfer none): the first field #GstMpeg2Picture already decoded
   * @second_field: (transfer none): a #GstMpeg2Picture for the second field
   *
   * Called when a new field picture is created for interlaced field picture.
   * Subclass can attach implementation specific user data on @second_field via
   * gst_mpeg2_picture_set_user_data
   *
   * Since: 1.20
   */
  GstFlowReturn (*new_field_picture)  (GstMpeg2Decoder * decoder,
                                       GstMpeg2Picture * first_field,
                                       GstMpeg2Picture * second_field);

  /**
   * GstMpeg2DecoderClass::start_picture:
   * @decoder: a #GstMpeg2Decoder
   * @picture: (transfer none): a #GstMpeg2Picture
   * @slice: (transfer none): a #GstMpeg2Slice
   * @prev_picture: (transfer none): a #GstMpeg2Picture
   * @next_picture: (transfer none): a #GstMpeg2Picture
   *
   * Optional. Called per one #GstMpeg2Picture to notify subclass to prepare
   * decoding process for the #GstMpeg2Picture
   *
   * Since: 1.20
   */
  GstFlowReturn (*start_picture)    (GstMpeg2Decoder * decoder,
                                     GstMpeg2Picture * picture,
                                     GstMpeg2Slice * slice,
                                     GstMpeg2Picture * prev_picture,
                                     GstMpeg2Picture * next_picture);

  /**
   * GstMpeg2DecoderClass::decode_slice:
   * @decoder: a #GstMpeg2Decoder
   * @picture: (transfer none): a #GstMpeg2Picture
   * @slice: (transfer none): a #GstMpeg2Slice
   *
   * Provides per slice data with parsed slice header and required raw bitstream
   * for subclass to decode it.
   *
   * Since: 1.20
   */
  GstFlowReturn (*decode_slice)     (GstMpeg2Decoder * decoder,
                                     GstMpeg2Picture * picture,
                                     GstMpeg2Slice * slice);

  /**
   * GstMpeg2DecoderClass::end_picture:
   * @decoder: a #GstMpeg2Decoder
   * @picture: (transfer none): a #GstMpeg2Picture
   *
   * Optional. Called per one #GstMpeg2Picture to notify subclass to finish
   * decoding process for the #GstMpeg2Picture
   *
   * Since: 1.20
   */
  GstFlowReturn (*end_picture)      (GstMpeg2Decoder * decoder,
                                     GstMpeg2Picture * picture);

  /**
   * GstMpeg2DecoderClass::output_picture:
   * @decoder: a #GstMpeg2Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstMpeg2Picture
   *
   * Called with a #GstMpeg2Picture which is required to be outputted.
   * The #GstVideoCodecFrame must be consumed by subclass.
   *
   * Since: 1.20
   */
  GstFlowReturn (*output_picture)   (GstMpeg2Decoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstMpeg2Picture * picture);

  /**
   * GstMpeg2DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstMpeg2Decoder
   * @is_live: whether upstream is live or not
   *
   * Optional. Called by baseclass to query whether delaying output is
   * preferred by subclass or not.
   *
   * Returns: the number of perferred delayed output frames
   *
   * Since: 1.20
   */
  guint (*get_preferred_output_delay) (GstMpeg2Decoder * decoder,
                                       gboolean is_live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstMpeg2Decoder, gst_object_unref)

GST_CODECS_API
GType gst_mpeg2_decoder_get_type (void);

G_END_DECLS

#endif /* __GST_MPEG2_DECODER_H__ */
