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

#ifndef __GST_VP8_DECODER_H__
#define __GST_VP8_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gstvp8parser.h>
#include <gst/codecs/gstvp8picture.h>

G_BEGIN_DECLS

#define GST_TYPE_VP8_DECODER            (gst_vp8_decoder_get_type())
#define GST_VP8_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VP8_DECODER,GstVp8Decoder))
#define GST_VP8_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VP8_DECODER,GstVp8DecoderClass))
#define GST_VP8_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_VP8_DECODER,GstVp8DecoderClass))
#define GST_IS_VP8_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VP8_DECODER))
#define GST_IS_VP8_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VP8_DECODER))
#define GST_VP8_DECODER_CAST(obj)       ((GstVp8Decoder*)obj)

typedef struct _GstVp8Decoder GstVp8Decoder;
typedef struct _GstVp8DecoderClass GstVp8DecoderClass;
typedef struct _GstVp8DecoderPrivate GstVp8DecoderPrivate;

/**
 * GstVp8Decoder:
 *
 * The opaque #GstVp8Decoder data structure.
 */
struct _GstVp8Decoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;

  /* reference frames */
  GstVp8Picture *last_picture;
  GstVp8Picture *golden_ref_picture;
  GstVp8Picture *alt_ref_picture;

  /*< private >*/
  GstVp8DecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstVp8DecoderClass:
 * @new_sequence:      Notifies subclass of SPS update
 * @new_picture:       Optional.
 *                     Called whenever new #GstVp8Picture is created.
 *                     Subclass can set implementation specific user data
 *                     on the #GstVp8Picture via gst_vp8_picture_set_user_data
 * @start_picture:     Optional.
 *                     Called per one #GstVp8Picture to notify subclass to prepare
 *                     decoding process for the #GstVp8Picture
 * @decode_slice:      Provides per slice data with parsed slice header and
 *                     required raw bitstream for subclass to decode it
 * @end_picture:       Optional.
 *                     Called per one #GstVp8Picture to notify subclass to finish
 *                     decoding process for the #GstVp8Picture
 * @output_picture:    Called with a #GstVp8Picture which is required to be outputted.
 *                     Subclass can retrieve parent #GstVideoCodecFrame by using
 *                     gst_video_decoder_get_frame() with system_frame_number
 *                     and the #GstVideoCodecFrame must be consumed by subclass via
 *                     gst_video_decoder_{finish,drop,release}_frame().
 */
struct _GstVp8DecoderClass
{
  GstVideoDecoderClass parent_class;

  GstFlowReturn   (*new_sequence)      (GstVp8Decoder * decoder,
                                        const GstVp8FrameHdr * frame_hdr,
                                        gint max_dpb_size);

  /**
   * GstVp8DecoderClass:new_picture:
   * @decoder: a #GstVp8Decoder
   * @frame: (transfer none): a #GstVideoCodecFrame
   * @picture: (transfer none): a #GstVp8Picture
   */
  GstFlowReturn   (*new_picture)       (GstVp8Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstVp8Picture * picture);

  GstFlowReturn   (*start_picture)     (GstVp8Decoder * decoder,
                                        GstVp8Picture * picture);

  GstFlowReturn   (*decode_picture)    (GstVp8Decoder * decoder,
                                        GstVp8Picture * picture,
                                        GstVp8Parser * parser);

  GstFlowReturn   (*end_picture)       (GstVp8Decoder * decoder,
                                        GstVp8Picture * picture);

  /**
   * GstVp8DecoderClass:output_picture:
   * @decoder: a #GstVp8Decoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   * @picture: (transfer full): a #GstVp8Picture
   */
  GstFlowReturn   (*output_picture)    (GstVp8Decoder * decoder,
                                        GstVideoCodecFrame * frame,
                                        GstVp8Picture * picture);

  /**
   * GstVp8DecoderClass::get_preferred_output_delay:
   * @decoder: a #GstVp8Decoder
   * @is_live: whether upstream is live or not
   *
   * Optional. Called by baseclass to query whether delaying output is
   * preferred by subclass or not.
   *
   * Returns: the number of perferred delayed output frame
   *
   * Since: 1.20
   */
  guint           (*get_preferred_output_delay)   (GstVp8Decoder * decoder,
                                                   gboolean is_live);

  /*< private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVp8Decoder, gst_object_unref)

GST_CODECS_API
GType gst_vp8_decoder_get_type (void);

G_END_DECLS

#endif /* __GST_VP8_DECODER_H__ */
