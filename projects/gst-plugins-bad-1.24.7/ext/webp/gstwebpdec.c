/* GStreamer
 * Copyright (C) <2013> Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) <2013> Intel Corporation
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
#include <string.h>

#include "gstwebpdec.h"

#define MIN_WIDTH 1
#define MAX_WIDTH 16383
#define MIN_HEIGHT 1
#define MAX_HEIGHT 16383

enum
{
  PROP_0,
  PROP_BYPASS_FILTERING,
  PROP_NO_FANCY_UPSAMPLING,
  PROP_USE_THREADS
};

static GstStaticPadTemplate gst_webp_dec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/webp")
    );

/*Fixme: Add YUV support */
static GstStaticPadTemplate gst_webp_dec_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ RGB, RGBA, BGR, BGRA, ARGB, RGB16}"))
    );

GST_DEBUG_CATEGORY_STATIC (webp_dec_debug);
#define GST_CAT_DEFAULT webp_dec_debug

static void gst_webp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_webp_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_webp_dec_start (GstVideoDecoder * bdec);
static gboolean gst_webp_dec_stop (GstVideoDecoder * bdec);
static gboolean gst_webp_dec_set_format (GstVideoDecoder * dec,
    GstVideoCodecState * state);
static GstFlowReturn gst_webp_dec_parse (GstVideoDecoder * bdec,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos);
static GstFlowReturn gst_webp_dec_handle_frame (GstVideoDecoder * bdec,
    GstVideoCodecFrame * frame);
static gboolean gst_webp_dec_decide_allocation (GstVideoDecoder * bdec,
    GstQuery * query);
static gboolean gst_webp_dec_sink_event (GstVideoDecoder * bdec,
    GstEvent * event);

static gboolean gst_webp_dec_reset_frame (GstWebPDec * webpdec);

#define gst_webp_dec_parent_class parent_class
G_DEFINE_TYPE (GstWebPDec, gst_webp_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (webpdec, "webpdec",
    GST_RANK_PRIMARY, GST_TYPE_WEBP_DEC);

static void
gst_webp_dec_class_init (GstWebPDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoDecoderClass *vdec_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  vdec_class = (GstVideoDecoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_webp_dec_set_property;
  gobject_class->get_property = gst_webp_dec_get_property;

  gst_element_class_add_static_pad_template (element_class,
      &gst_webp_dec_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_webp_dec_sink_pad_template);
  gst_element_class_set_static_metadata (element_class, "WebP image decoder",
      "Codec/Decoder/Image", "Decode images from WebP format",
      "Sreerenj Balachandran <sreerenj.balachandrn@intel.com>");

  g_object_class_install_property (gobject_class, PROP_BYPASS_FILTERING,
      g_param_spec_boolean ("bypass-filtering", "Bypass Filtering",
          "When enabled, skip the in-loop filtering", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NO_FANCY_UPSAMPLING,
      g_param_spec_boolean ("no-fancy-upsampling", "No Fancy Upsampling",
          "When enabled, use faster pointwise upsampler", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_THREADS,
      g_param_spec_boolean ("use-threads", "Use Threads",
          "When enabled, use multi-threaded decoding", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  vdec_class->start = gst_webp_dec_start;
  vdec_class->stop = gst_webp_dec_stop;
  vdec_class->parse = gst_webp_dec_parse;
  vdec_class->set_format = gst_webp_dec_set_format;
  vdec_class->handle_frame = gst_webp_dec_handle_frame;
  vdec_class->decide_allocation = gst_webp_dec_decide_allocation;
  vdec_class->sink_event = gst_webp_dec_sink_event;

  GST_DEBUG_CATEGORY_INIT (webp_dec_debug, "webpdec", 0, "WebP decoder");
}

static void
gst_webp_dec_init (GstWebPDec * dec)
{
  GST_DEBUG ("Initialize the webp decoder");

  memset (&dec->config, 0, sizeof (dec->config));
  dec->saw_header = FALSE;

  dec->bypass_filtering = FALSE;
  dec->no_fancy_upsampling = FALSE;
  dec->use_threads = FALSE;
  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (dec));
}

static gboolean
gst_webp_dec_reset_frame (GstWebPDec * webpdec)
{
  GST_DEBUG ("Reset the current frame properties");

  webpdec->saw_header = FALSE;

  if (!WebPInitDecoderConfig (&webpdec->config)) {
    GST_WARNING_OBJECT (webpdec,
        "Failed to configure the WebP image decoding libraray");
    return FALSE;
  }
  return TRUE;
}

static void
gst_webp_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebPDec *dec;

  dec = GST_WEBP_DEC (object);

  switch (prop_id) {
    case PROP_BYPASS_FILTERING:
      dec->bypass_filtering = g_value_get_boolean (value);
      break;
    case PROP_NO_FANCY_UPSAMPLING:
      dec->no_fancy_upsampling = g_value_get_boolean (value);
      break;
    case PROP_USE_THREADS:
      dec->use_threads = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webp_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWebPDec *dec;

  dec = GST_WEBP_DEC (object);

  switch (prop_id) {
    case PROP_BYPASS_FILTERING:
      g_value_set_boolean (value, dec->bypass_filtering);
      break;
    case PROP_NO_FANCY_UPSAMPLING:
      g_value_set_boolean (value, dec->no_fancy_upsampling);
      break;
    case PROP_USE_THREADS:
      g_value_set_boolean (value, dec->use_threads);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_webp_dec_start (GstVideoDecoder * decoder)
{
  GstWebPDec *webpdec = (GstWebPDec *) decoder;

  return gst_webp_dec_reset_frame (webpdec);
}

static gboolean
gst_webp_dec_stop (GstVideoDecoder * bdec)
{
  GstWebPDec *webpdec = (GstWebPDec *) bdec;

  if (webpdec->input_state) {
    gst_video_codec_state_unref (webpdec->input_state);
    webpdec->input_state = NULL;
  }
  if (webpdec->output_state) {
    gst_video_codec_state_unref (webpdec->output_state);
    webpdec->output_state = NULL;
  }
  return TRUE;
}

static gboolean
gst_webp_dec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstWebPDec *webpdec = (GstWebPDec *) decoder;

  if (webpdec->input_state)
    gst_video_codec_state_unref (webpdec->input_state);
  webpdec->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static gboolean
gst_webp_dec_decide_allocation (GstVideoDecoder * bdec, GstQuery * query)
{
  GstBufferPool *pool = NULL;
  GstStructure *config;

  if (!GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (bdec, query))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);

  if (pool == NULL)
    return FALSE;

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_webp_dec_sink_event (GstVideoDecoder * bdec, GstEvent * event)
{
  const GstSegment *segment;

  if (GST_EVENT_TYPE (event) != GST_EVENT_SEGMENT)
    goto done;

  gst_event_parse_segment (event, &segment);

  if (segment->format == GST_FORMAT_TIME)
    gst_video_decoder_set_packetized (bdec, TRUE);
  else
    gst_video_decoder_set_packetized (bdec, FALSE);

done:
  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (bdec, event);
}

static GstFlowReturn
gst_webp_dec_parse (GstVideoDecoder * decoder, GstVideoCodecFrame * frame,
    GstAdapter * adapter, gboolean at_eos)
{
  gsize toadd = 0;
  gsize size;
  gconstpointer data;
  GstByteReader reader;
  GstWebPDec *webpdec = (GstWebPDec *) decoder;

  size = gst_adapter_available (adapter);
  GST_DEBUG_OBJECT (decoder,
      "parsing webp image data (%" G_GSIZE_FORMAT " bytes)", size);

  if (at_eos) {
    GST_DEBUG ("Flushing all data out");
    toadd = size;

    /* If we have leftover data, throw it away */
    if (!webpdec->saw_header)
      goto drop_frame;
    goto have_full_frame;
  }

  if (!webpdec->saw_header) {
    guint32 code;

    if (size < 12)
      goto need_more_data;

    data = gst_adapter_map (adapter, size);
    gst_byte_reader_init (&reader, data, size);

    if (!gst_byte_reader_get_uint32_le (&reader, &code))
      goto error;

    if (code == GST_MAKE_FOURCC ('R', 'I', 'F', 'F')) {
      if (!gst_byte_reader_get_uint32_le (&reader, &webpdec->frame_size))
        goto error;

      if (!gst_byte_reader_get_uint32_le (&reader, &code))
        goto error;

      if (code == GST_MAKE_FOURCC ('W', 'E', 'B', 'P'))
        webpdec->saw_header = TRUE;

    }
  }

  if (!webpdec->saw_header)
    goto error;

  if (size >= (webpdec->frame_size + 8)) {
    toadd = webpdec->frame_size + 8;
    webpdec->saw_header = FALSE;
    goto have_full_frame;
  }

need_more_data:
  return GST_VIDEO_DECODER_FLOW_NEED_DATA;

have_full_frame:
  if (toadd)
    gst_video_decoder_add_to_frame (decoder, toadd);
  return gst_video_decoder_have_frame (decoder);

drop_frame:
  gst_adapter_flush (adapter, size);
  return GST_FLOW_OK;

error:
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_webp_dec_update_src_caps (GstWebPDec * dec, GstMapInfo * map_info)
{
  WebPBitstreamFeatures features;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  if (WebPGetFeatures (map_info->data, map_info->size,
          &features) != VP8_STATUS_OK) {
    GST_ERROR_OBJECT (dec, "Failed to execute WebPGetFeatures");
    return GST_FLOW_ERROR;
  }

  if (features.width < MIN_WIDTH || features.width > MAX_WIDTH
      || features.height < MIN_HEIGHT || features.height > MAX_HEIGHT) {
    GST_ERROR_OBJECT (dec, "Dimensions of the frame is unsupported by libwebp");
    return GST_FLOW_ERROR;
  }

  /* TODO: Add support for other formats */
  if (features.has_alpha) {
    format = GST_VIDEO_FORMAT_ARGB;
    dec->colorspace = MODE_ARGB;
  } else {
    format = GST_VIDEO_FORMAT_RGB;
    dec->colorspace = MODE_RGB;
  }

  /* Check if output state changed */
  if (dec->output_state) {
    GstVideoInfo *info = &dec->output_state->info;

    if (features.width == GST_VIDEO_INFO_WIDTH (info) &&
        features.height == GST_VIDEO_INFO_HEIGHT (info) &&
        GST_VIDEO_INFO_FORMAT (info) == format) {
      goto beach;
    }
    gst_video_codec_state_unref (dec->output_state);
  }

  dec->output_state =
      gst_video_decoder_set_output_state (GST_VIDEO_DECODER (dec), format,
      features.width, features.height, dec->input_state);

  if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (dec)))
    return GST_FLOW_NOT_NEGOTIATED;

beach:
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_webp_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstWebPDec *webpdec = (GstWebPDec *) decoder;
  GstMapInfo map_info;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFrame vframe;

  gst_buffer_map (frame->input_buffer, &map_info, GST_MAP_READ);

  ret = gst_webp_dec_update_src_caps (webpdec, &map_info);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unmap (frame->input_buffer, &map_info);
    gst_video_codec_frame_unref (frame);
    goto done;
  }

  ret = gst_video_decoder_allocate_output_frame (decoder, frame);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_ERROR_OBJECT (decoder, "failed to allocate output frame");
    ret = GST_FLOW_ERROR;
    gst_buffer_unmap (frame->input_buffer, &map_info);
    gst_video_codec_frame_unref (frame);

    goto done;
  }

  if (!gst_video_frame_map (&vframe, &webpdec->output_state->info,
          frame->output_buffer, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (decoder, "Failed to map output videoframe");
    ret = GST_FLOW_ERROR;
    gst_buffer_unmap (frame->input_buffer, &map_info);
    gst_video_codec_frame_unref (frame);

    goto done;
  }

  /* configure output buffer parameteres */
  webpdec->config.options.bypass_filtering = webpdec->bypass_filtering;
  webpdec->config.options.no_fancy_upsampling = webpdec->no_fancy_upsampling;
  webpdec->config.options.use_threads = webpdec->use_threads;
  webpdec->config.output.colorspace = webpdec->colorspace;
  webpdec->config.output.u.RGBA.rgba = (uint8_t *) vframe.map[0].data;
  webpdec->config.output.u.RGBA.stride =
      GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 0);
  webpdec->config.output.u.RGBA.size = GST_VIDEO_FRAME_SIZE (&vframe);
  webpdec->config.output.is_external_memory = 1;

  if (WebPDecode (map_info.data, map_info.size,
          &webpdec->config) != VP8_STATUS_OK) {
    GST_ERROR_OBJECT (decoder, "Failed to decode the webp frame");
    ret = GST_FLOW_ERROR;
    gst_video_frame_unmap (&vframe);
    gst_buffer_unmap (frame->input_buffer, &map_info);
    gst_video_codec_frame_unref (frame);

    goto done;
  }

  gst_video_frame_unmap (&vframe);
  gst_buffer_unmap (frame->input_buffer, &map_info);

  ret = gst_video_decoder_finish_frame (decoder, frame);

  if (!gst_webp_dec_reset_frame (webpdec)) {
    ret = GST_FLOW_ERROR;
    goto done;
  }

done:
  return ret;
}
