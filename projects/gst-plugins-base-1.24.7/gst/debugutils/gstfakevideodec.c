/* GStreamer
 * Copyright (C) 2019 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2023 Thibault Saunier <tsaunier@igalia.com>
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
 *
 */
/**
 * element-fakevideodec:
 *
 * The fake video decoder ignores input bitstream except
 * to enforce caps restrictions. It reads video `width`,
 * `height` and `framerate` from caps. Then it just pushes
 * video frames without doing any decoding. It can Also
 * handle raw frames decoding them as they come, faking
 * that it is decoding them.
 *
 * When faking decoding encoded data, it draws a snake moving from
 * left to right in the middle of the frame. This is a
 * light weight drawing while it still provides an idea
 * about how smooth is the rendering.
 *
 * The fake video decoder inherits from GstVideoDecoder.
 * It is useful to measure how smooth will be the whole
 * rendering pipeline if you had the most efficient video
 * decoder. Also useful to bisect issues for example when
 * suspecting issues in a specific video decoder.
 *
 * It is also useful to to use it to test the #GstVideoDecoder base
 * class.
 *
 * ## Examples:
 *
 * ### Fake decoding raw frames
 *
 * ```
 * $ gst-launch-1.0 videotestsrc !  fakevideodec ! videoconvert ! autovideosink
 * ```
 *
 * ### False decoding encoded framers
 *
 * ```
 * $ GST_PLUGIN_FEATURE_RANK=fakevideodec:1000 gst-launch-1.0 playbin3 uri=file:///path/to/video
 *
 * ```
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstfakevideodec.h"

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <stdio.h>
#include <string.h>


GST_DEBUG_CATEGORY_STATIC (gst_fake_videodec_debug);
#define GST_CAT_DEFAULT gst_fake_videodec_debug

static gboolean gst_fake_video_dec_start (GstVideoDecoder * decoder);
static gboolean gst_fake_video_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_fake_video_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_fake_video_dec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_fake_video_dec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_fake_video_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_fake_video_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);

#define FAKE_VIDEO_DEC_CAPS_COMMON ", width=(int) [1, MAX], height=(int) [1, MAX], framerate=(fraction) [1, MAX]"
#define FAKE_VIDEO_DEC_CAPS_COMMON_PARSED FAKE_VIDEO_DEC_CAPS_COMMON ", parsed = (boolean) true"
#define FAKE_VIDEO_DEC_CAPS "video/x-h264" FAKE_VIDEO_DEC_CAPS_COMMON_PARSED ";" \
    "video/x-h263" FAKE_VIDEO_DEC_CAPS_COMMON_PARSED ";" \
    "video/x-theora" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-vp6" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-vp6-flash" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-vp8" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-vp9" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-divx" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-msmpeg" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/mpeg, mpegversion=(int) {1, 2, 4}, systemstream=(boolean) false" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-flash-video, flvversion=(int) 1" FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-raw,format={ RGBA, RGBx, BGRA, BGRx, RGB16} " FAKE_VIDEO_DEC_CAPS_COMMON ";" \
    "video/x-wmv, wmvversion=(int) {1, 2, 3}" FAKE_VIDEO_DEC_CAPS_COMMON

static GstStaticPadTemplate gst_fake_video_dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (FAKE_VIDEO_DEC_CAPS)
    );

static GstStaticPadTemplate gst_fake_video_dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGBA, RGBx, BGRA, BGRx, RGB16}"))
    );

#define parent_class gst_fake_video_dec_parent_class
G_DEFINE_TYPE (GstFakeVideoDec, gst_fake_video_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (fakevideodec, "fakevideodec",
    GST_RANK_NONE, gst_fake_video_dec_get_type ());

static void
gst_fake_video_dec_class_init (GstFakeVideoDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *base_video_decoder_class =
      GST_VIDEO_DECODER_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_fake_video_dec_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_fake_video_dec_sink_template));

  gst_element_class_set_static_metadata (element_class,
      "Fake Video Decoder",
      "Codec/Decoder/Video",
      "Fake video decoder", "Julien Isorce <julien.isorce@gmail.com>");

  base_video_decoder_class->start =
      GST_DEBUG_FUNCPTR (gst_fake_video_dec_start);
  base_video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_fake_video_dec_stop);
  base_video_decoder_class->flush =
      GST_DEBUG_FUNCPTR (gst_fake_video_dec_flush);
  base_video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_fake_video_dec_set_format);
  base_video_decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_fake_video_dec_negotiate);
  base_video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_fake_video_dec_handle_frame);
  base_video_decoder_class->decide_allocation =
      gst_fake_video_dec_decide_allocation;

  GST_DEBUG_CATEGORY_INIT (gst_fake_videodec_debug, "fakevideodec", 0,
      "Fake Video Decoder");
}

static void
gst_fake_video_dec_init (GstFakeVideoDec * dec)
{
  GstVideoDecoder *bdec = GST_VIDEO_DECODER (dec);

  GST_DEBUG_OBJECT (dec, "Initialize fake video decoder");

  gst_video_decoder_set_packetized (bdec, TRUE);
  dec->min_buffers = 0;
  dec->snake_current_step = 0;
  dec->snake_max_steps = 0;
  dec->snake_length = 0;
}

static gboolean
gst_fake_video_dec_start (GstVideoDecoder * decoder)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (decoder);

  GST_DEBUG_OBJECT (dec, "start");

  dec->min_buffers = 0;
  dec->snake_current_step = 0;

  return TRUE;
}

static gboolean
gst_fake_video_dec_stop (GstVideoDecoder * base_video_decoder)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (base_video_decoder);

  GST_DEBUG_OBJECT (dec, "stop");

  if (dec->output_state) {
    gst_video_codec_state_unref (dec->output_state);
    dec->output_state = NULL;
  }

  if (dec->input_state) {
    gst_video_codec_state_unref (dec->input_state);
    dec->input_state = NULL;
  }

  return TRUE;
}

static gboolean
gst_fake_video_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (decoder);
  GstCaps *templ_caps = NULL;
  GstCaps *intersection = NULL;
  GstVideoInfo info;
  gdouble fps = 0;

  GST_DEBUG_OBJECT (dec, "set format");

  /* select what downstream want of support the best */

  templ_caps = gst_pad_get_pad_template_caps (GST_VIDEO_DECODER_SRC_PAD (dec));
  intersection =
      gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (dec), templ_caps);
  gst_caps_unref (templ_caps);

  GST_DEBUG_OBJECT (dec, "Allowed downstream caps: %" GST_PTR_FORMAT,
      intersection);

  intersection = gst_caps_truncate (intersection);
  intersection = gst_caps_fixate (intersection);

  gst_video_info_init (&info);
  if (!gst_video_info_from_caps (&info, intersection)) {
    GST_WARNING_OBJECT (dec,
        "failed to parse intersection with downstream caps %" GST_PTR_FORMAT,
        intersection);
    gst_caps_unref (intersection);
    return FALSE;
  }
  gst_caps_unref (intersection);
  intersection = NULL;

  if (dec->input_state)
    gst_video_codec_state_unref (dec->input_state);
  dec->input_state = gst_video_codec_state_ref (state);

  dec->output_state =
      gst_video_decoder_set_output_state (GST_VIDEO_DECODER (dec),
      GST_VIDEO_INFO_FORMAT (&info), dec->input_state->info.width,
      dec->input_state->info.height, dec->input_state);
  gst_video_decoder_negotiate (GST_VIDEO_DECODER (dec));

  gst_util_fraction_to_double (dec->output_state->info.fps_n,
      dec->output_state->info.fps_d, &fps);

  if (fps < 1 || fps > 60) {
    GST_ERROR_OBJECT (dec, "unsupported framerate %d / %d",
        dec->output_state->info.fps_n, dec->output_state->info.fps_d);
    return FALSE;
  }

  dec->snake_max_steps = (guint) fps;
  dec->snake_length = dec->output_state->info.width / fps;

  if (dec->snake_length == 0) {
    GST_ERROR_OBJECT (dec,
        "unsupported framerate %d / %d or frame width too small %d",
        dec->output_state->info.fps_n, dec->output_state->info.fps_d,
        dec->output_state->info.width);
    return FALSE;
  }

  GST_DEBUG_OBJECT (dec,
      "width: %d, height: %d, fps_n: %d, fps_d: %d, snake length %d",
      dec->output_state->info.width, dec->output_state->info.height,
      dec->output_state->info.fps_n, dec->output_state->info.fps_d,
      dec->snake_length);

  return TRUE;
}

static GstFlowReturn
gst_fake_video_dec_init_buffer (GstFakeVideoDec * dec, GstBuffer * buffer)
{
  GstMapInfo minfo;

  if (!gst_buffer_map (buffer, &minfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (dec, "Failed to map input buffer");
    return GST_FLOW_ERROR;
  }

  /* Make the frames entirely black just once. */
  memset (minfo.data, 0, minfo.maxsize);

  gst_buffer_unmap (buffer, &minfo);

  return GST_FLOW_OK;
}

static gboolean
gst_fake_video_dec_negotiate (GstVideoDecoder * decoder)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (decoder);
  GstVideoCodecFrame *frame = NULL;
  gboolean ret = TRUE;
  guint i = 0;

  if (!GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder))
    return FALSE;

  GST_DEBUG_OBJECT (dec, "negotiate");

  frame = g_slice_new0 (GstVideoCodecFrame);

  for (i = 0; i < dec->min_buffers; ++i) {
    ret =
        gst_video_decoder_allocate_output_frame (decoder, frame) == GST_FLOW_OK;
    if (!ret)
      break;
    gst_fake_video_dec_init_buffer (dec, frame->output_buffer);
    gst_buffer_replace (&frame->output_buffer, NULL);
  }

  g_slice_free (GstVideoCodecFrame, frame);

  return ret;
}

static gboolean
gst_fake_video_dec_flush (GstVideoDecoder * decoder)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (decoder);

  GST_DEBUG_OBJECT (dec, "flush");

  return TRUE;
}

static void
gst_fake_video_dec_snake_next_step (GstFakeVideoDec * dec)
{
  if (dec->snake_current_step < dec->snake_max_steps) {
    ++dec->snake_current_step;
    return;
  }

  dec->snake_current_step = 0;
}

static GstFlowReturn
gst_fake_video_dec_fill_buffer (GstFakeVideoDec * dec, GstBuffer * buffer)
{
  gint height = 0;
  guint offset = 0;
  gint stride = 0;
  guint depth = 0;
  guint8 *data = NULL;
  GstVideoFrame frame;
  GstVideoInfo *info = &dec->output_state->info;

  if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (dec, "Could not map video buffer");
    return GST_FLOW_ERROR;
  }

  if (GST_VIDEO_FRAME_N_PLANES (&frame) != 1) {
    GST_ERROR_OBJECT (dec, "Currently only support one video frame plane");
    gst_video_frame_unmap (&frame);
    return GST_FLOW_ERROR;
  }

  height = GST_VIDEO_FRAME_HEIGHT (&frame);
  data = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
  offset = GST_VIDEO_FRAME_PLANE_OFFSET (&frame, 0);
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);
  depth = GST_VIDEO_FRAME_COMP_DEPTH (&frame, 0);

  switch (GST_VIDEO_FRAME_FORMAT (&frame)) {
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGB16:
    {
      /* Erase the entire line where the snake is drawn. */
      memset (data + offset + (height / 2) * stride, 0, stride * depth);
      /* Draw a snake moving from left to right. */
      memset (data + offset + (height / 2) * stride +
          dec->snake_current_step * dec->snake_length * depth, 0xff,
          dec->snake_length * depth);
      break;
    }
    default:
      GST_WARNING_OBJECT (dec, "Not supported video format %s",
          gst_video_format_to_string (GST_VIDEO_FRAME_FORMAT (&frame)));
      break;
  }

  gst_video_frame_unmap (&frame);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_fake_video_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo minfo;

  GST_DEBUG_OBJECT (dec, "handle frame");

  if (dec->input_state->info.finfo->format == GST_VIDEO_FORMAT_ENCODED) {
    if (!gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ)) {
      GST_ERROR_OBJECT (dec, "Failed to map input buffer");
      return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT (dec,
        "input data size %" G_GSIZE_FORMAT ", PTS: %" GST_TIME_FORMAT,
        minfo.size, GST_TIME_ARGS (frame->pts));

    gst_buffer_unmap (frame->input_buffer, &minfo);

    gst_fake_video_dec_snake_next_step (dec);
    ret = gst_video_decoder_allocate_output_frame (decoder, frame);
    if (ret != GST_FLOW_OK)
      goto drop;
    ret = gst_fake_video_dec_fill_buffer (dec, frame->output_buffer);
    if (ret != GST_FLOW_OK)
      goto drop;
  } else {
    frame->output_buffer = gst_buffer_ref (frame->input_buffer);
  }

  ret = gst_video_decoder_finish_frame (decoder, frame);
  return ret;

drop:
  gst_video_decoder_drop_frame (decoder, frame);
  return ret;
}

static gboolean
gst_fake_video_dec_decide_allocation (GstVideoDecoder * bdec, GstQuery * query)
{
  GstFakeVideoDec *dec = GST_FAKE_VIDEO_DEC (bdec);
  GstBufferPool *pool = NULL;
  GstStructure *config = NULL;
  guint min_buffers = 0;

  if (!GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (bdec, query))
    return FALSE;

  GST_DEBUG_OBJECT (dec, "decide allocation");

  g_assert (gst_query_get_n_allocation_pools (query) > 0);
  gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, &min_buffers,
      NULL);
  g_assert (pool != NULL);

  /* Initialize at least 2 buffers. */
  dec->min_buffers = MIN (min_buffers, 2);

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return TRUE;
}
