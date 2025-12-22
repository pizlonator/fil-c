/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstopenh264elements.h"
#include "gstopenh264dec.h"

#include <wels/codec_ver.h>
#define OPENH264_VERSION_CHECK(maj,min) ((OPENH264_MAJOR > (maj)) || (OPENH264_MAJOR == (maj) && OPENH264_MINOR >= (min)))

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <string.h>             /* for memcpy */

#if OPENH264_VERSION_CHECK (1,9)
#define HAVE_OPENH264_MAIN_PROFILE 1
#else
#define HAVE_OPENH264_MAIN_PROFILE 0
#endif

GST_DEBUG_CATEGORY_STATIC (gst_openh264dec_debug_category);
#define GST_CAT_DEFAULT gst_openh264dec_debug_category

/* prototypes */
static gboolean gst_openh264dec_start (GstVideoDecoder * decoder);
static gboolean gst_openh264dec_stop (GstVideoDecoder * decoder);

static gboolean gst_openh264dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_openh264dec_reset (GstVideoDecoder * decoder,
    gboolean hard);
static GstFlowReturn gst_openh264dec_finish (GstVideoDecoder * decoder);
static GstFlowReturn gst_openh264dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_openh264dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean openh264dec_element_init (GstPlugin * plugin);

#if HAVE_OPENH264_MAIN_PROFILE
#define SUPPORTED_PROFILE_STR "profile=(string){ constrained-baseline, baseline, main, high, constrained-high, progressive-high }"
#else
#define SUPPORTED_PROFILE_STR "profile=(string){ constrained-baseline, baseline }"
#endif

/* pad templates */
static GstStaticPadTemplate gst_openh264dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-h264, stream-format=(string)byte-stream, alignment=(string)au, "
      SUPPORTED_PROFILE_STR
    ));

static GstStaticPadTemplate gst_openh264dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420")));

/* class initialization */

G_DEFINE_TYPE (GstOpenh264Dec, gst_openh264dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (openh264dec, openh264dec_element_init);

static void
gst_openh264dec_class_init (GstOpenh264DecClass * klass)
{
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_openh264dec_sink_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_openh264dec_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenH264 video decoder", "Decoder/Video", "OpenH264 video decoder",
      "Ericsson AB, http://www.ericsson.com");

  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_openh264dec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_openh264dec_stop);

  video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_openh264dec_set_format);
  video_decoder_class->reset = GST_DEBUG_FUNCPTR (gst_openh264dec_reset);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_openh264dec_finish);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_openh264dec_handle_frame);
  video_decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_openh264dec_decide_allocation);
}

static void
gst_openh264dec_init (GstOpenh264Dec * openh264dec)
{
  openh264dec->decoder = NULL;

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (openh264dec), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (openh264dec), TRUE);
}

#ifndef GST_DISABLE_GST_DEBUG
static void
openh264_trace_cb (void *ctx, int level, const char *string)
{
  GObject *o = G_OBJECT (ctx);
  GstDebugLevel lvl = GST_LEVEL_WARNING;

  if (level >= WELS_LOG_DETAIL)
    lvl = GST_LEVEL_LOG;
  else if (level >= WELS_LOG_DEBUG)
    lvl = GST_LEVEL_DEBUG;
  else if (level >= WELS_LOG_INFO)
    lvl = GST_LEVEL_INFO;
  else if (level >= WELS_LOG_WARNING)
    lvl = GST_LEVEL_WARNING;
  else if (level >= WELS_LOG_ERROR)
    lvl = GST_LEVEL_ERROR;

  gst_debug_log (GST_CAT_DEFAULT, lvl, "", "", 0, o, "%s", string);
}
#endif

static gboolean
gst_openh264dec_start (GstVideoDecoder * decoder)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);
  gint ret;
  SDecodingParam dec_param = { 0 };

  if (openh264dec->decoder != NULL) {
    openh264dec->decoder->Uninitialize ();
    WelsDestroyDecoder (openh264dec->decoder);
    openh264dec->decoder = NULL;
  }

  if (WelsCreateDecoder (&(openh264dec->decoder)) != 0) {
    GST_ELEMENT_ERROR (openh264dec, LIBRARY, INIT, (NULL),
        ("Failed to create OpenH264 decoder."));
    return FALSE;
  }

#ifndef GST_DISABLE_GST_DEBUG
  {
    int log_level = WELS_LOG_WARNING;
    WelsTraceCallback log_cb = openh264_trace_cb;

    openh264dec->decoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &log_level);
    openh264dec->decoder->SetOption (DECODER_OPTION_TRACE_CALLBACK,
        (void *) &log_cb);
    openh264dec->decoder->SetOption (DECODER_OPTION_TRACE_CALLBACK_CONTEXT,
        (void *) &decoder);
  }
#endif

  dec_param.uiTargetDqLayer = 255;
  dec_param.eEcActiveIdc = ERROR_CON_FRAME_COPY;
#if OPENH264_MAJOR == 1 && OPENH264_MINOR < 6
  dec_param.eOutputColorFormat = videoFormatI420;
#endif
  dec_param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;

  ret = openh264dec->decoder->Initialize (&dec_param);

  GST_DEBUG_OBJECT (openh264dec,
      "openh264_dec_start called, openh264dec %sinitialized OK!",
      (ret != cmResultSuccess) ? "NOT " : "");

  return (ret == cmResultSuccess);
}

static gboolean
gst_openh264dec_stop (GstVideoDecoder * decoder)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);

  if (openh264dec->decoder) {
    openh264dec->decoder->Uninitialize ();
    WelsDestroyDecoder (openh264dec->decoder);
    openh264dec->decoder = NULL;
  }

  if (openh264dec->input_state) {
    gst_video_codec_state_unref (openh264dec->input_state);
    openh264dec->input_state = NULL;
  }
  openh264dec->width = openh264dec->height = 0;

  return TRUE;
}

static gboolean
gst_openh264dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);

  GST_DEBUG_OBJECT (openh264dec, "input caps: %" GST_PTR_FORMAT, state->caps);

  if (openh264dec->input_state) {
    gst_video_codec_state_unref (openh264dec->input_state);
    openh264dec->input_state = NULL;
  }
  openh264dec->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static gboolean
gst_openh264dec_reset (GstVideoDecoder * decoder, gboolean hard)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);

  GST_DEBUG_OBJECT (openh264dec, "reset");

  return TRUE;
}

static GstFlowReturn
gst_openh264dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);
  GstMapInfo map_info;
  GstVideoCodecState *state;
  SBufferInfo dst_buf_info;
  DECODING_STATE ret;
  guint8 *yuvdata[3];
  GstFlowReturn flow_status;
  GstVideoFrame video_frame;
  guint actual_width, actual_height;
  guint i;
  guint8 *p;
  guint row_stride, component_width, component_height, src_width, row;

  if (frame == NULL) {
#if OPENH264_VERSION_CHECK (1,9)
    /* Called with no videoframe for EOS logic. Drain out */
    int end_of_stream = 1;
    memset (&dst_buf_info, 0, sizeof (SBufferInfo));

    openh264dec->decoder->SetOption (DECODER_OPTION_END_OF_STREAM,
        &end_of_stream);
    ret = openh264dec->decoder->FlushFrame (yuvdata, &dst_buf_info);

    if (ret != dsErrorFree || dst_buf_info.iBufferStatus != 1) {
      GST_DEBUG_OBJECT (decoder, "No more frames to retrieve at EOS");
      return GST_FLOW_EOS;
    }
#else
    return GST_FLOW_EOS;
#endif
  } else {
    if (!gst_buffer_map (frame->input_buffer, &map_info, GST_MAP_READ)) {
      GST_ERROR_OBJECT (openh264dec, "Cannot map input buffer!");
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_ERROR;
    }

    GST_LOG_OBJECT (openh264dec, "handle frame, 1st NAL type %d",
        map_info.size > 4 ? map_info.data[4] & 0x1f : -1);

    memset (&dst_buf_info, 0, sizeof (SBufferInfo));
    /* Use the unsigned long long OpenH264 timestamp to store the system_frame_number
     * to track the original frame through any OpenH264 reordering */
    dst_buf_info.uiInBsTimeStamp = frame->system_frame_number;

    GST_LOG_OBJECT (decoder, "Submitting frame with PTS %" GST_TIME_FORMAT
        " and frame ref %" G_GUINT64_FORMAT,
        GST_TIME_ARGS (frame->pts), (guint64) frame->system_frame_number);

    ret =
        openh264dec->decoder->DecodeFrameNoDelay (map_info.data, map_info.size,
        yuvdata, &dst_buf_info);
    gst_buffer_unmap (frame->input_buffer, &map_info);

    if (ret != dsErrorFree) {
      /* Request a key unit from upstream */
      GST_DEBUG_OBJECT (openh264dec, "Requesting a key unit");

      gst_video_decoder_request_sync_point (decoder, frame,
          (GstVideoDecoderRequestSyncPointFlags) 0);

      GST_LOG_OBJECT (openh264dec, "error decoding nal, return code: %d", ret);
      gst_video_codec_frame_unref (frame);

      /* Get back the frame that was reported as errored */
      frame =
          gst_video_decoder_get_frame (decoder, dst_buf_info.uiOutYuvTimeStamp);
      if (frame) {
        GST_LOG_OBJECT (decoder,
            "Dropping errored frame ref %" G_GUINT64_FORMAT,
            (guint64) dst_buf_info.uiOutYuvTimeStamp);
        return gst_video_decoder_drop_frame (decoder, frame);
      }
      return GST_FLOW_OK;
    }

    gst_video_codec_frame_unref (frame);
    frame = NULL;

    /* No output available yet */
    if (dst_buf_info.iBufferStatus != 1) {
      GST_LOG_OBJECT (decoder, "No buffer decoded yet");
      return GST_FLOW_OK;
    }
  }

  GST_LOG_OBJECT (decoder, "Got back frame with frame ref %" G_GUINT64_FORMAT,
      (guint64) dst_buf_info.uiOutYuvTimeStamp);

  /* OpenH264 lets us pass an int reference through
   * so we can retrieve the input frame now */
  frame = gst_video_decoder_get_frame (decoder, dst_buf_info.uiOutYuvTimeStamp);
  if (!frame) {
    /* Where did our frame go? This is a reference tracking error. */
    GST_WARNING_OBJECT (decoder,
        "Failed to look up frame ref %" G_GUINT64_FORMAT,
        (guint64) dst_buf_info.uiOutYuvTimeStamp);
    return GST_FLOW_OK;
  }

  actual_width = dst_buf_info.UsrData.sSystemBuffer.iWidth;
  actual_height = dst_buf_info.UsrData.sSystemBuffer.iHeight;

  if (!gst_pad_has_current_caps (GST_VIDEO_DECODER_SRC_PAD (openh264dec))
      || actual_width != openh264dec->width
      || actual_height != openh264dec->height) {
    state =
        gst_video_decoder_set_output_state (decoder, GST_VIDEO_FORMAT_I420,
        actual_width, actual_height, openh264dec->input_state);
    openh264dec->width = actual_width;
    openh264dec->height = actual_height;

    if (!gst_video_decoder_negotiate (decoder)) {
      GST_ERROR_OBJECT (openh264dec,
          "Failed to negotiate with downstream elements");
      gst_video_codec_state_unref (state);
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_NOT_NEGOTIATED;
    }
  } else {
    state = gst_video_decoder_get_output_state (decoder);
  }

  flow_status = gst_video_decoder_allocate_output_frame (decoder, frame);
  if (flow_status != GST_FLOW_OK) {
    gst_video_codec_state_unref (state);
    gst_video_codec_frame_unref (frame);
    return flow_status;
  }

  if (!gst_video_frame_map (&video_frame, &state->info, frame->output_buffer,
          GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (openh264dec, "Cannot map output buffer!");
    gst_video_codec_state_unref (state);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  for (i = 0; i < 3; i++) {
    p = GST_VIDEO_FRAME_COMP_DATA (&video_frame, i);
    row_stride = GST_VIDEO_FRAME_COMP_STRIDE (&video_frame, i);
    component_width = GST_VIDEO_FRAME_COMP_WIDTH (&video_frame, i);
    component_height = GST_VIDEO_FRAME_COMP_HEIGHT (&video_frame, i);
    src_width =
        i <
        1 ? dst_buf_info.UsrData.sSystemBuffer.
        iStride[0] : dst_buf_info.UsrData.sSystemBuffer.iStride[1];
    for (row = 0; row < component_height; row++) {
      memcpy (p, yuvdata[i], component_width);
      p += row_stride;
      yuvdata[i] += src_width;
    }
  }
  gst_video_codec_state_unref (state);
  gst_video_frame_unmap (&video_frame);

  return gst_video_decoder_finish_frame (decoder, frame);
}

static GstFlowReturn
gst_openh264dec_finish (GstVideoDecoder * decoder)
{
  GstOpenh264Dec *openh264dec = GST_OPENH264DEC (decoder);

  GST_DEBUG_OBJECT (openh264dec, "finish");

  /* Decoder not negotiated yet */
  if (openh264dec->width == 0)
    return GST_FLOW_OK;

  /* Drain all pending frames */
  while ((gst_openh264dec_handle_frame (decoder, NULL)) == GST_FLOW_OK);

  return GST_FLOW_OK;
}

static gboolean
gst_openh264dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstVideoCodecState *state;
  GstBufferPool *pool;
  guint size, min, max;
  GstStructure *config;

  if (!GST_VIDEO_DECODER_CLASS (gst_openh264dec_parent_class)->decide_allocation
      (decoder, query))
    return FALSE;

  state = gst_video_decoder_get_output_state (decoder);

  gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }

  gst_buffer_pool_set_config (pool, config);

  gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);

  gst_object_unref (pool);
  gst_video_codec_state_unref (state);

  return TRUE;
}

static gboolean
openh264dec_element_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_openh264dec_debug_category, "openh264dec", 0,
      "debug category for openh264dec element");

  return gst_element_register (plugin, "openh264dec", GST_RANK_MARGINAL,
      GST_TYPE_OPENH264DEC);
}
