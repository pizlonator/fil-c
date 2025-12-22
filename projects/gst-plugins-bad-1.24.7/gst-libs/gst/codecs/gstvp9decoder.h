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

#ifndef __GST_VP9_DECODER_H__
#define __GST_VP9_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecs/gstvp9picture.h>

G_BEGIN_DECLS

#define GST_TYPE_VP9_DECODER            (gst_vp9_decoder_get_type())
#define GST_VP9_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VP9_DECODER,GstVp9Decoder))
#define GST_VP9_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VP9_DECODER,GstVp9DecoderClass))
#define GST_VP9_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VP9_DECODER,GstVp9DecoderClass))
#define GST_IS_VP9_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VP9_DECODER))
#define GST_IS_VP9_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VP9_DECODER))
#define GST_VP9_DECODER_CAST(obj)       ((GstVP9Decoder*)obj)

typedef struct _GstVp9Decoder GstVp9Decoder;
typedef struct _GstVp9DecoderClass GstVp9DecoderClass;
typedef struct _GstVp9DecoderPrivate GstVp9DecoderPrivate;

/**
 * GstVp9Decoder:
 *
 * The opaque #GstVp9Decoder data structure.
 */
struct _GstVp9Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;
  gboolean parse_compressed_headers;

  /*< private >*/
  GstVp9DecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstVp9DecoderClass:
 */
struct _GstVp9DecoderClass
{
  GstVideoDecoderClass parent_class;

  /**
   * GstVp9DecoderClass::new_sequence:
   * @decoder: a #GstVp9Decoder
   * @frame_hdr: a #GstVp9FrameHeader
   * @max_dpb_size: the size of dpb including preferred output delay
   *   by subclass reported via get_preferred_output_delay method.
   *
   * Notifies subclass of video sequence update such as resolution, bitdepth,
   * profile.
   *
   * Since: 1.18
   */
  GstFlowReturn   (*new_sequence)      (GstVp9Decoder * decoder,
                                        const GstVp9FrameHeader *frame_hdr,
                                        gint max_dpb_size);

  /**
   * GstVp9DecoderClass::new_picture:
   * @decoder: a #GstVp9Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstVp9Picture
   *
   * Optional. Called whenever new #GstVp9Picture is created.
   * Subclass can set implementation specific user data on the #GstVp9Picture
   * via gst_vp9_picture_set_user_data
   *
   * Since: 1.18
   */
  GstFlowReturn   (*new_picture)       (GstVp9Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstVp9Picture * picture);

  /**
   * GstVp9DecoderClass::duplicate_picture:
   * @decoder: a #GstVp9Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstVp9Picture to be duplicated
   *
   * Optional. Called to duplicate @picture when show_existing_frame flag is set
   * in the parsed vp9 frame header. Returned #GstVp9Picture from this method
   * should hold already decoded picture data corresponding to the @picture,
   * since the returned #GstVp9Picture from this method will be passed to
   * the output_picture method immediately without additional decoding process.
   *
   * If this method is not implemented by subclass, baseclass will drop
   * current #GstVideoCodecFrame without additional processing for the current
   * frame.
   *
   * Returns: (transfer full) (nullable): a #GstVp9Picture or %NULL if failed to duplicate
   * @picture.
   *
   * Since: 1.18
   */
  GstVp9Picture * (*duplicate_picture) (GstVp9Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstVp9Picture * picture);

  /**
   * GstVp9DecoderClass::start_picture:
   * @decoder: a #GstVp9Decoder
   * @picture: (transfer none): a #GstVp9Picture
   *
   * Optional. Called to notify subclass to prepare decoding process for
   * @picture
   *
   * Since: 1.18
   */
  GstFlowReturn   (*start_picture)     (GstVp9Decoder * decoder,
                                        GstVp9Picture * picture);

  /**
   * GstVp9DecoderClass::decode_picture:
   * @decoder: a #GstVp9Decoder
   * @picture: (transfer none): a #GstVp9Picture to decoder
   * @dpb: (transfer none): a #GstVp9Dpb
   *
   * Called to notify decoding for subclass to decoder given @picture with
   * given @dpb
   *
   * Since: 1.18
   */
  GstFlowReturn   (*decode_picture)    (GstVp9Decoder * decoder,
                                        GstVp9Picture * picture,
                                        GstVp9Dpb * dpb);

  /**
   * GstVp9DecoderClass::end_picture:
   * @decoder: a #GstVp9Decoder
   * @picture: (transfer none): a #GstVp9Picture
   *
   * Optional. Called per one #GstVp9Picture to notify subclass to finish
   * decoding process for the #GstVp9Picture
   *
   * Since: 1.18
   */
  GstFlowReturn   (*end_picture)       (GstVp9Decoder * decoder,
                                        GstVp9Picture * picture);

  /**
   * GstVp9DecoderClass::output_picture:
   * @decoder: a #GstVp9Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstVp9Picture
   *
   * Called to notify @picture is ready to be outputted.
   *
   * Since: 1.18
   */
  GstFlowReturn   (*output_picture)    (GstVp9Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstVp9Picture * picture);

  /**
   * GstVp9DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstVp9Decoder
   * @is_live: whether upstream is live or not
   *
   * Optional. Retrieve the preferred output delay from child classes.
   * controls how many frames to delay when calling
   * GstVp9DecoderClass::output_picture
   *
   * Returns: the number of perferred delayed output frame
   *
   * Since: 1.20
   */
  guint           (*get_preferred_output_delay)   (GstVp9Decoder * decoder,
                                                   gboolean is_live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVp9Decoder, gst_object_unref)

GST_CODECS_API
GType gst_vp9_decoder_get_type (void);

GST_CODECS_API
void gst_vp9_decoder_set_non_keyframe_format_change_support (GstVp9Decoder * decoder,
                                                             gboolean support);

G_END_DECLS

#endif /* __GST_VP9_DECODER_H__ */
