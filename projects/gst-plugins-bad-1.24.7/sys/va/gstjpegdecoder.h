/* GStreamer
 * Copyright (C) 2022 Víctor Jáquez <vjaquez@igalia.com>
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

#ifndef __GST_JPEG_DECODER_H__
#define __GST_JPEG_DECODER_H__

#include <gst/codecs/codecs-prelude.h>

#include <gst/video/video.h>
#include <gst/codecparsers/gstjpegparser.h>

G_BEGIN_DECLS

#define GST_TYPE_JPEG_DECODER            (gst_jpeg_decoder_get_type())
#define GST_JPEG_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JPEG_DECODER,GstJpegDecoder))
#define GST_JPEG_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JPEG_DECODER,GstJpegDecoderClass))
#define GST_JPEG_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_JPEG_DECODER,GstJpegDecoderClass))
#define GST_IS_JPEG_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JPEG_DECODER))
#define GST_IS_JPEG_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JPEG_DECODER))
#define GST_JPEG_DECODER_CAST(obj)       ((GstJpegDecoder*)obj)

typedef struct _GstJpegDecoderScan GstJpegDecoderScan;

typedef struct _GstJpegDecoder GstJpegDecoder;
typedef struct _GstJpegDecoderClass GstJpegDecoderClass;
typedef struct _GstJpegDecoderPrivate GstJpegDecoderPrivate;


/**
 * GstJpegDecoderScan:
 *
 * Container for a SOS segment.
 */
struct _GstJpegDecoderScan
{
  GstJpegScanHdr *scan_hdr;
  GstJpegHuffmanTables *huffman_tables;
  GstJpegQuantTables *quantization_tables;
  guint restart_interval;
  guint mcus_per_row;
  guint mcu_rows_in_scan;
};

/**
 * GstJpegDecoder:
 *
 * The opaque #GstJpegDecoder data structure.
 *
 * Since: 1.22
 */
struct _GstJpegDecoder
{
  /*< private >*/
  GstVideoDecoder parent;

  /*< protected >*/
  GstVideoCodecState * input_state;

  /*< private >*/
  GstJpegDecoderPrivate *priv;
  gpointer padding[GST_PADDING_LARGE];
};

/**
 * GstJpegDecoderClass:
 *
 * The opaque #GstJpegDecoderClass data structure.
 */
struct _GstJpegDecoderClass
{
  /*< private >*/
  GstVideoDecoderClass parent_class;

 /**
  * GstJpegDecoderClass::new_picture:
  * @decoder: a #GstJpegDecoder
  * @frame: (transfer none): a #GstVideoCodecFrame
  * @frame_hdr: (transfer none): a #GstJpegFrameHdr
  *
  * Called whenever new picture is detected.
  */
  GstFlowReturn (*new_picture)      (GstJpegDecoder * decoder,
                                     GstVideoCodecFrame * frame,
                                     GstJpegMarker marker,
                                     GstJpegFrameHdr * frame_hdr);

  /**
   * GstJpegDecoderClass::decode_scan:
   * @decoder: a #GstJpegDecoder
   * @scan: (transfer none): a #GstJpegDecoderScan
   * @buffer: (transfer none): scan buffer
   * @size: size of @buffer
   *
   * Called whenever new scan is found.
   */
  GstFlowReturn (*decode_scan)     (GstJpegDecoder * decoder,
                                    GstJpegDecoderScan * scan,
                                    const guint8 * buffer,
                                    guint32 size);

  /**
   * GstJpegDecoderClass::end_picture:
   * @decoder: a #GstJpegDecoder
   *
   * Called whenever a picture end mark is found.
   */
  GstFlowReturn (*end_picture)     (GstJpegDecoder * decoder);

  /**
   * GstJpegDecoderClass::output_picture:
   * @decoder: a #GstJpegDecoder
   * @frame: (transfer full): a #GstVideoCodecFrame
   *
   * Called whenever a @frame is required to be outputted.  The
   * #GstVideoCodecFrame must be consumed by subclass.
   */
  GstFlowReturn   (*output_picture)    (GstJpegDecoder * decoder,
                                        GstVideoCodecFrame * frame);

  /*< Private >*/
  gpointer padding[GST_PADDING_LARGE];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstJpegDecoder, gst_object_unref)

GType gst_jpeg_decoder_get_type (void);

G_END_DECLS

#endif /* __GST_JPEG_DECODER_H__ */
