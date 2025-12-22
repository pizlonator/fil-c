/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2encstreamwriter.cc: GStreamer/mpeg2enc output wrapper
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmpeg2enc.hh"
#include "gstmpeg2encstreamwriter.hh"
#include <string.h>

/*
 * Class init stuff.
 */

GstMpeg2EncStreamWriter::GstMpeg2EncStreamWriter (GstVideoEncoder * venc,
    EncoderParams * params)
{
  video_encoder = GST_VIDEO_ENCODER_CAST (gst_object_ref (venc));
}

GstMpeg2EncStreamWriter::~GstMpeg2EncStreamWriter ()
{
  gst_object_unref (video_encoder);
}

void
GstMpeg2EncStreamWriter::WriteOutBufferUpto (const guint8 * buffer,
    const guint32 flush_upto)
{
  GstVideoCodecFrame *frame;
  GstBuffer *buf;
  GstMpeg2enc *enc = GST_MPEG2ENC (video_encoder);
  GstFlowReturn ret;

  frame = gst_video_encoder_get_oldest_frame (video_encoder);
  g_assert (frame != NULL);

  buf = gst_buffer_new_and_alloc (flush_upto);
  gst_buffer_fill (buf, 0, buffer, flush_upto);
  flushed += flush_upto;
  frame->output_buffer = buf;

  /* this should not block anything else (e.g. handle_frame), but if it does,
   * it's ok as mpeg2enc is not really a loop-based element, but push-based */
  ret = gst_video_encoder_finish_frame (video_encoder, frame);
  gst_video_codec_frame_unref (frame);
  GST_MPEG2ENC_MUTEX_LOCK (enc);
  enc->srcresult = ret;
  GST_MPEG2ENC_MUTEX_UNLOCK (enc);
}

guint64
GstMpeg2EncStreamWriter::BitCount ()
{
  return flushed * 8ll;
}
