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

#ifndef __GST_AV1_DECODER_H__
#define __GST_AV1_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gstav1parser.h>
#include <gst/codecs/gstav1picture.h>

G_BEGIN_DECLS

#define GST_TYPE_AV1_DECODER            (gst_av1_decoder_get_type())
#define GST_AV1_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AV1_DECODER,GstAV1Decoder))
#define GST_AV1_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AV1_DECODER,GstAV1DecoderClass))
#define GST_AV1_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AV1_DECODER,GstAV1DecoderClass))
#define GST_IS_AV1_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AV1_DECODER))
#define GST_IS_AV1_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AV1_DECODER))

typedef struct _GstAV1Decoder GstAV1Decoder;
typedef struct _GstAV1DecoderClass GstAV1DecoderClass;
typedef struct _GstAV1DecoderPrivate GstAV1DecoderPrivate;

/**
 * GstAV1Decoder:
 *
 * The opaque #GstAV1Decoder data structure.
 *
 * Since: 1.20
 */
struct _GstAV1Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;
  guint highest_spatial_layer;

  /*< private >*/
  GstAV1DecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstAV1DecoderClass:
 */
struct _GstAV1DecoderClass
{
  GstVideoDecoderClass parent_class;

  /**
   * GstAV1DecoderClass::new_sequence:
   * @decoder: a #GstAV1Decoder
   * @seq_hdr: a #GstAV1SequenceHeaderOBU
   * @max_dpb_size: the size of dpb including preferred output delay
   *   by subclass reported via get_preferred_output_delay method.
   *
   * Notifies subclass of SPS update
   *
   * Since: 1.20
   */
  GstFlowReturn   (*new_sequence)      (GstAV1Decoder * decoder,
                                        const GstAV1SequenceHeaderOBU * seq_hdr,
                                        gint max_dpb_size);
  /**
   * GstAV1DecoderClass::new_picture:
   * @decoder: a #GstAV1Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstAV1Picture
   *
   * Optional. Called whenever new #GstAV1Picture is created.
   * Subclass can set implementation specific user data
   * on the #GstAV1Picture via gst_av1_picture_set_user_data
   *
   * Since: 1.20
   */
  GstFlowReturn   (*new_picture)       (GstAV1Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstAV1Picture * picture);
  /**
   * GstAV1DecoderClass::duplicate_picture:
   * @decoder: a #GstAV1Decoder
   * @picture: (transfer none): a #GstAV1Picture
   * @frame: (transfer none): the current #GstVideoCodecFrame
   *
   * Called when need to duplicate an existing #GstAV1Picture. As
   * duplicated key-frame will populate the DPB, this virtual
   * function is not optional.
   *
   * Since: 1.22
   */
  GstAV1Picture * (*duplicate_picture) (GstAV1Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstAV1Picture * picture);
  /**
   * GstAV1DecoderClass::start_picture:
   * @decoder: a #GstAV1Decoder
   * @picture: (transfer none): a #GstAV1Picture
   * @dpb: (transfer none): a #GstAV1Dpb
   *
   * Optional. Called per one #GstAV1Picture to notify subclass to prepare
   * decoding process for the #GstAV1Picture
   *
   * Since: 1.20
   */
  GstFlowReturn   (*start_picture)     (GstAV1Decoder * decoder,
                                        GstAV1Picture * picture,
                                        GstAV1Dpb * dpb);
  /**
   * GstAV1DecoderClass::decode_tile:
   * @decoder: a #GstAV1Decoder
   * @picture: (transfer none): a #GstAV1Picture
   * @tile: (transfer none): a #GstAV1Tile
   *
   * Provides the tile data with tile group header and required raw
   * bitstream for subclass to decode it.
   *
   * Since: 1.20
   */
  GstFlowReturn   (*decode_tile)       (GstAV1Decoder * decoder,
                                        GstAV1Picture * picture,
                                        GstAV1Tile * tile);
  /**
   * GstAV1DecoderClass::end_picture:
   * @decoder: a #GstAV1Decoder
   * @picture: (transfer none): a #GstAV1Picture
   *
   * Optional. Called per one #GstAV1Picture to notify subclass to finish
   * decoding process for the #GstAV1Picture
   *
   * Since: 1.20
   */
  GstFlowReturn   (*end_picture)       (GstAV1Decoder * decoder,
                                        GstAV1Picture * picture);
  /**
   * GstAV1DecoderClass::output_picture:
   * @decoder: a #GstAV1Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstAV1Picture
   *
   * Called with a #GstAV1Picture which is required to be outputted.
   * The #GstVideoCodecFrame must be consumed by subclass.
   *
   * Since: 1.20
   */
  GstFlowReturn   (*output_picture)    (GstAV1Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstAV1Picture * picture);

  /**
   * GstAV1DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstAV1Decoder
   * @live: whether upstream is live or not
   *
   * Optional. Called by baseclass to query whether delaying output is
   * preferred by subclass or not.
   *
   * Returns: the number of perferred delayed output frame
   *
   * Since: 1.22
   */
  guint (*get_preferred_output_delay)   (GstAV1Decoder * decoder,
                                         gboolean live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstAV1Decoder, gst_object_unref)

GST_CODECS_API
GType gst_av1_decoder_get_type (void);

G_END_DECLS

#endif /* __GST_AV1_DECODER_H__ */
