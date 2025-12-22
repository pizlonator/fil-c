/* GStreamer
 * Copyright (C) 2020 Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
#include <config.h>
#endif

#include "gstv4l2codecallocator.h"
#include "gstv4l2codecalphadecodebin.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2codecvp8dec.h"
#include "gstv4l2format.h"

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define V4L2_MIN_KERNEL_VER_MAJOR 5
#define V4L2_MIN_KERNEL_VER_MINOR 13
#define V4L2_MIN_KERNEL_VERSION KERNEL_VERSION(V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR, 0)

GST_DEBUG_CATEGORY_STATIC (v4l2_vp8dec_debug);
#define GST_CAT_DEFAULT v4l2_vp8dec_debug

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8")
    );

static GstStaticPadTemplate alpha_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8, codec-alpha = (boolean) true")
    );

#define SRC_CAPS \
    GST_VIDEO_DMA_DRM_CAPS_MAKE " ; " \
    GST_VIDEO_CAPS_MAKE (GST_V4L2_DEFAULT_VIDEO_FORMATS)

#define SRC_CAPS_NO_DRM \
    GST_VIDEO_CAPS_MAKE (GST_V4L2_DEFAULT_VIDEO_FORMATS)

static GstStaticCaps static_src_caps = GST_STATIC_CAPS (SRC_CAPS);
static GstStaticCaps static_src_caps_no_drm = GST_STATIC_CAPS (SRC_CAPS_NO_DRM);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SRC_NAME,
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS));

struct _GstV4l2CodecVp8Dec
{
  GstVp8Decoder parent;
  GstV4l2Decoder *decoder;
  GstVideoCodecState *output_state;
  GstVideoInfo vinfo;
  GstVideoInfoDmaDrm vinfo_drm;
  gint width;
  gint height;

  GstV4l2CodecAllocator *sink_allocator;
  GstV4l2CodecAllocator *src_allocator;
  GstV4l2CodecPool *src_pool;
  gint min_pool_size;
  gboolean has_videometa;
  gboolean streaming;
  gboolean copy_frames;

  struct v4l2_ctrl_vp8_frame frame_header;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecVp8Dec, gst_v4l2_codec_vp8_dec,
    GST_TYPE_VP8_DECODER);

#define parent_class gst_v4l2_codec_vp8_dec_parent_class

static guint
gst_v4l2_codec_vp8_dec_get_preferred_output_delay (GstVp8Decoder * decoder,
    gboolean is_live)
{

  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  guint delay;

  if (is_live)
    delay = 0;
  else
    /* Just one for now, perhaps we can make this configurable in the future. */
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);
  return delay;
}

static gboolean
gst_v4l2_codec_vp8_dec_open (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  guint version;

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open VP8 decoder"),
        ("gst_v4l2_decoder_open() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  version = gst_v4l2_decoder_get_version (self->decoder);
  if (version < V4L2_MIN_KERNEL_VERSION)
    GST_WARNING_OBJECT (self,
        "V4L2 API v%u.%u too old, at least v%u.%u required",
        (version >> 16) & 0xff, (version >> 8) & 0xff,
        V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR);

  return TRUE;
}

static gboolean
gst_v4l2_codec_vp8_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  gst_v4l2_decoder_close (self->decoder);
  return TRUE;
}

static void
gst_v4l2_codec_vp8_dec_streamoff (GstV4l2CodecVp8Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_vp8_dec_reset_allocation (GstV4l2CodecVp8Dec * self)
{
  if (self->sink_allocator) {
    gst_v4l2_codec_allocator_detach (self->sink_allocator);
    g_clear_object (&self->sink_allocator);
  }

  if (self->src_allocator) {
    gst_v4l2_codec_allocator_detach (self->src_allocator);
    g_clear_object (&self->src_allocator);
    g_clear_object (&self->src_pool);
  }
}

static gboolean
gst_v4l2_codec_vp8_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);

  gst_v4l2_codec_vp8_dec_streamoff (self);
  gst_v4l2_codec_vp8_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gboolean
gst_v4l2_codec_vp8_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  GstVp8Decoder *vp8dec = GST_VP8_DECODER (decoder);
  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_VP8_FRAME,
      .ptr = &self->frame_header,
      .size = sizeof (self->frame_header),
    },
  };
  /* *INDENT-ON* */
  GstCaps *peer_caps, *filter, *caps;
  GstStaticCaps *static_filter;

  /* Ignore downstream renegotiation request. */
  if (self->streaming)
    goto done;

  GST_DEBUG_OBJECT (self, "Negotiate");

  gst_v4l2_codec_vp8_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_VP8_FRAME,
          self->width, self->height, 12 /* 8 bits 4:2:0 */ )) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure VP8 decoder"),
        ("gst_v4l2_decoder_set_sink_fmt() failed: %s", g_strerror (errno)));
    gst_v4l2_decoder_close (self->decoder);
    return FALSE;
  }

  if (!gst_v4l2_decoder_set_controls (self->decoder, NULL, control,
          G_N_ELEMENTS (control))) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver does not support the selected stream."), (NULL));
    return FALSE;
  }

  /* If the peer has ANY caps only advertise system memory caps */
  peer_caps = gst_pad_peer_query_caps (decoder->srcpad, NULL);
  static_filter =
      gst_caps_is_any (peer_caps) ? &static_src_caps_no_drm : &static_src_caps;
  gst_caps_unref (peer_caps);

  filter = gst_v4l2_decoder_enum_src_formats (self->decoder, static_filter);
  if (!filter) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("No supported decoder output formats"), (NULL));
    return FALSE;
  }
  GST_DEBUG_OBJECT (self, "Supported output formats: %" GST_PTR_FORMAT, filter);

  caps = gst_pad_peer_query_caps (decoder->srcpad, filter);
  gst_caps_unref (filter);
  GST_DEBUG_OBJECT (self, "Peer supported formats: %" GST_PTR_FORMAT, caps);

  if (!gst_v4l2_decoder_select_src_format (self->decoder, caps, &self->vinfo,
          &self->vinfo_drm)) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Unsupported pixel format"),
        ("No support for %ux%u format %s", self->width, self->height,
            gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&self->vinfo))));
    gst_caps_unref (caps);
    return FALSE;
  }
  gst_caps_unref (caps);

done:
  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  self->output_state =
      gst_v4l2_decoder_set_output_state (GST_VIDEO_DECODER (self), &self->vinfo,
      &self->vinfo_drm, self->width, self->height, vp8dec->input_state);

  if (GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder)) {
    if (self->streaming)
      return TRUE;

    if (!gst_v4l2_decoder_streamon (self->decoder, GST_PAD_SINK)) {
      GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
          ("Could not enable the decoder driver."),
          ("VIDIOC_STREAMON(SINK) failed: %s", g_strerror (errno)));
      return FALSE;
    }

    if (!gst_v4l2_decoder_streamon (self->decoder, GST_PAD_SRC)) {
      GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
          ("Could not enable the decoder driver."),
          ("VIDIOC_STREAMON(SRC) failed: %s", g_strerror (errno)));
      return FALSE;
    }

    self->streaming = TRUE;

    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_v4l2_codec_vp8_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  GstCaps *caps = NULL;
  guint min = 0;
  guint num_bitstream;

  if (self->streaming)
    goto no_internal_changes;

  g_clear_object (&self->src_pool);
  g_clear_object (&self->src_allocator);

  self->has_videometa = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, NULL);

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (self, "No valid caps");
    return FALSE;
  }

  if (gst_video_is_dma_drm_caps (caps) && !self->has_videometa) {
    GST_ERROR_OBJECT (self,
        "DMABuf caps negotiated without the mandatory support of VideoMeta");
    return FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_parse_nth_allocation_pool (query, 0, NULL, NULL, &min, NULL);

  min = MAX (2, min);

  num_bitstream = 1 +
      MAX (1, gst_v4l2_decoder_get_render_delay (self->decoder));

  self->sink_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SINK, num_bitstream);
  if (!self->sink_allocator) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to allocate sink buffers."), (NULL));
    return FALSE;
  }

  self->src_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SRC, self->min_pool_size + min + 4);
  if (!self->src_allocator) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to allocate source buffers."), (NULL));
    g_clear_object (&self->sink_allocator);
    return FALSE;
  }

  self->src_pool = gst_v4l2_codec_pool_new (self->src_allocator, &self->vinfo);

no_internal_changes:
  /* Our buffer pool is internal, we will let the base class create a video
   * pool, and use it if we are running out of buffers or if downstream does
   * not support GstVideoMeta */
  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static void
gst_v4l2_codec_vp8_dec_fill_segment (struct v4l2_vp8_segment
    *segment, const GstVp8Segmentation * segmentation)
{
  gint i;

  /* *INDENT-OFF* */
  segment->flags =
    (segmentation->segmentation_enabled ? V4L2_VP8_SEGMENT_FLAG_ENABLED : 0) |
    (segmentation->update_mb_segmentation_map ? V4L2_VP8_SEGMENT_FLAG_UPDATE_MAP : 0) |
    (segmentation->update_segment_feature_data ? V4L2_VP8_SEGMENT_FLAG_UPDATE_FEATURE_DATA : 0) |
    (segmentation->segment_feature_mode ? 0 : V4L2_VP8_SEGMENT_FLAG_DELTA_VALUE_MODE);
  /* *INDENT-ON* */

  for (i = 0; i < 4; i++) {
    segment->quant_update[i] = segmentation->quantizer_update_value[i];
    segment->lf_update[i] = segmentation->lf_update_value[i];
  }

  for (i = 0; i < 3; i++)
    segment->segment_probs[i] = segmentation->segment_prob[i];

  segment->padding = 0;
}

static void
gst_v4l2_codec_vp8_dec_fill_lf (struct v4l2_vp8_loop_filter
    *lf, const GstVp8MbLfAdjustments * lf_adj)
{
  gint i;

  lf->flags |=
      (lf_adj->loop_filter_adj_enable ? V4L2_VP8_LF_ADJ_ENABLE : 0) |
      (lf_adj->mode_ref_lf_delta_update ? V4L2_VP8_LF_DELTA_UPDATE : 0);

  for (i = 0; i < 4; i++) {
    lf->ref_frm_delta[i] = lf_adj->ref_frame_delta[i];
    lf->mb_mode_delta[i] = lf_adj->mb_mode_delta[i];
  }
}

static void
gst_v4l2_codec_vp8_dec_fill_entropy (struct v4l2_vp8_entropy
    *entropy, const GstVp8FrameHdr * frame_hdr)
{
  memcpy (entropy->coeff_probs, frame_hdr->token_probs.prob,
      sizeof (frame_hdr->token_probs.prob));
  memcpy (entropy->y_mode_probs, frame_hdr->mode_probs.y_prob,
      sizeof (frame_hdr->mode_probs.y_prob));
  memcpy (entropy->uv_mode_probs, frame_hdr->mode_probs.uv_prob,
      sizeof (frame_hdr->mode_probs.uv_prob));
  memcpy (entropy->mv_probs, frame_hdr->mv_probs.prob,
      sizeof (frame_hdr->mv_probs.prob));
}

static void
gst_v4l2_codec_vp8_dec_fill_frame_header (GstV4l2CodecVp8Dec * self,
    const GstVp8FrameHdr * frame_hdr)
{
  gint i;

  /* *INDENT-OFF* */
  self->frame_header = (struct v4l2_ctrl_vp8_frame) {
    .lf = (struct v4l2_vp8_loop_filter) {
      .sharpness_level = frame_hdr->sharpness_level,
      .level = frame_hdr->loop_filter_level,
      .flags = (frame_hdr->filter_type == 1 ? V4L2_VP8_LF_FILTER_TYPE_SIMPLE : 0)
    },
    .quant = (struct v4l2_vp8_quantization) {
      .y_ac_qi = frame_hdr->quant_indices.y_ac_qi,
      .y_dc_delta = frame_hdr->quant_indices.y_dc_delta,
      .y2_dc_delta = frame_hdr->quant_indices.y2_dc_delta,
      .y2_ac_delta = frame_hdr->quant_indices.y2_ac_delta,
      .uv_dc_delta = frame_hdr->quant_indices.uv_dc_delta,
      .uv_ac_delta = frame_hdr->quant_indices.uv_ac_delta
    },
    .coder_state = (struct v4l2_vp8_entropy_coder_state) {
      .range = frame_hdr->rd_range,
      .value = frame_hdr->rd_value,
      .bit_count = frame_hdr->rd_count
    },

    .width = self->width,
    .height = self->height,

    .horizontal_scale = frame_hdr->horiz_scale_code,
    .vertical_scale = frame_hdr->vert_scale_code,

    .version = frame_hdr->version,
    .prob_skip_false = frame_hdr->prob_skip_false,
    .prob_intra = frame_hdr->prob_intra,
    .prob_last = frame_hdr->prob_last,
    .prob_gf = frame_hdr->prob_gf,
    .num_dct_parts = 1 << frame_hdr->log2_nbr_of_dct_partitions,

    .first_part_size = frame_hdr->first_part_size,
    .first_part_header_bits = frame_hdr->header_size,

    .flags = (frame_hdr->key_frame ? V4L2_VP8_FRAME_FLAG_KEY_FRAME : 0) |
             (frame_hdr->show_frame ? V4L2_VP8_FRAME_FLAG_SHOW_FRAME : 0) |
             (frame_hdr->mb_no_skip_coeff ? V4L2_VP8_FRAME_FLAG_MB_NO_SKIP_COEFF : 0) |
             (frame_hdr->sign_bias_golden ? V4L2_VP8_FRAME_FLAG_SIGN_BIAS_GOLDEN : 0) |
             (frame_hdr->sign_bias_alternate ? V4L2_VP8_FRAME_FLAG_SIGN_BIAS_ALT : 0),
  };
  /* *INDENT-ON* */

  for (i = 0; i < 8; i++)
    self->frame_header.dct_part_sizes[i] = frame_hdr->partition_size[i];

  gst_v4l2_codec_vp8_dec_fill_entropy (&self->frame_header.entropy, frame_hdr);
}

static void
gst_v4l2_codec_vp8_dec_fill_references (GstV4l2CodecVp8Dec * self)
{
  GstVp8Decoder *decoder = &self->parent;

  if (decoder->last_picture) {
    self->frame_header.last_frame_ts =
        GST_CODEC_PICTURE_TS_NS (decoder->last_picture);
  }

  if (decoder->golden_ref_picture) {
    self->frame_header.golden_frame_ts =
        GST_CODEC_PICTURE_TS_NS (decoder->golden_ref_picture);
  }

  if (decoder->alt_ref_picture) {
    self->frame_header.alt_frame_ts =
        GST_CODEC_PICTURE_TS_NS (decoder->alt_ref_picture);
  }

  GST_DEBUG_OBJECT (self, "Passing references: last %u, golden %u, alt %u",
      (guint32) self->frame_header.last_frame_ts / 1000,
      (guint32) self->frame_header.golden_frame_ts / 1000,
      (guint32) self->frame_header.alt_frame_ts / 1000);
}

static GstFlowReturn
gst_v4l2_codec_vp8_dec_new_sequence (GstVp8Decoder * decoder,
    const GstVp8FrameHdr * frame_hdr, gint max_dpb_size)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  gboolean negotiation_needed = FALSE;

  if (self->vinfo.finfo->format == GST_VIDEO_FORMAT_UNKNOWN)
    negotiation_needed = TRUE;

  /* TODO Check if current buffers are large enough, and reuse them */
  if (self->width != frame_hdr->width || self->height != frame_hdr->height) {
    self->width = frame_hdr->width;
    self->height = frame_hdr->height;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Resolution changed to %dx%d",
        self->width, self->height);
  }

  gst_v4l2_codec_vp8_dec_fill_frame_header (self, frame_hdr);

  if (negotiation_needed) {
    gst_v4l2_codec_vp8_dec_streamoff (self);
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  /* Check if we can zero-copy buffers */
  if (!self->has_videometa) {
    GstVideoInfo ref_vinfo;
    gint i;

    gst_video_info_set_format (&ref_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
        self->width, self->height);

    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&self->vinfo); i++) {
      if (self->vinfo.stride[i] != ref_vinfo.stride[i] ||
          self->vinfo.offset[i] != ref_vinfo.offset[i]) {
        GST_WARNING_OBJECT (self,
            "GstVideoMeta support required, copying frames.");
        self->copy_frames = TRUE;
        break;
      }
    }
  } else {
    self->copy_frames = FALSE;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_vp8_dec_start_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);

  /* FIXME base class should not call us if negotiation failed */
  if (!self->sink_allocator)
    return GST_FLOW_NOT_NEGOTIATED;

  /* Ensure we have a bitstream to write into */
  if (!self->bitstream) {
    self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

    if (!self->bitstream) {
      GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
          ("Not enough memory to decode VP8 stream."), (NULL));
      return GST_FLOW_ERROR;
    }

    if (!gst_memory_map (self->bitstream, &self->bitstream_map, GST_MAP_WRITE)) {
      GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
          ("Could not access bitstream memory for writing"), (NULL));
      g_clear_pointer (&self->bitstream, gst_memory_unref);
      return GST_FLOW_ERROR;
    }
  }

  /* We use this field to track how much we have written */
  self->bitstream_map.size = 0;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_vp8_dec_decode_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture, GstVp8Parser * parser)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  guint8 *bitstream_data = self->bitstream_map.data;

  if (self->bitstream_map.maxsize < picture->size) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space to send picture bitstream."), (NULL));
    return GST_FLOW_ERROR;
  }

  gst_v4l2_codec_vp8_dec_fill_frame_header (self, &picture->frame_hdr);
  gst_v4l2_codec_vp8_dec_fill_segment (&self->frame_header.segment,
      &parser->segmentation);
  gst_v4l2_codec_vp8_dec_fill_lf (&self->frame_header.lf,
      &parser->mb_lf_adjust);
  gst_v4l2_codec_vp8_dec_fill_references (self);

  memcpy (bitstream_data, picture->data, picture->size);
  self->bitstream_map.size = picture->size;

  return GST_FLOW_OK;
}

static void
gst_v4l2_codec_vp8_dec_reset_picture (GstV4l2CodecVp8Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }
}

static GstFlowReturn
gst_v4l2_codec_vp8_dec_end_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  GstVideoCodecFrame *frame;
  GstV4l2Request *request;
  GstBuffer *buffer;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  gsize bytesused;

  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_VP8_FRAME,
      .ptr = &self->frame_header,
      .size = sizeof(self->frame_header),
    },
  };
  /* *INDENT-ON* */

  bytesused = self->bitstream_map.size;
  gst_memory_unmap (self->bitstream, &self->bitstream_map);
  self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  gst_memory_resize (self->bitstream, 0, bytesused);

  flow_ret = gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (self->src_pool),
      &buffer, NULL);
  if (flow_ret != GST_FLOW_OK) {
    if (flow_ret == GST_FLOW_FLUSHING)
      GST_DEBUG_OBJECT (self, "Frame decoding aborted, we are flushing.");
    else
      GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
          ("No more picture buffer available."), (NULL));
    goto fail;
  }

  frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_warn_if_fail (frame->output_buffer == NULL);
  frame->output_buffer = buffer;
  gst_video_codec_frame_unref (frame);

  request = gst_v4l2_decoder_alloc_request (self->decoder,
      GST_CODEC_PICTURE_FRAME_NUMBER (picture), self->bitstream, buffer);
  if (!request) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Failed to allocate a media request object."), (NULL));
    goto fail;
  }

  gst_vp8_picture_set_user_data (picture, request,
      (GDestroyNotify) gst_v4l2_request_unref);

  if (!gst_v4l2_decoder_set_controls (self->decoder, request, control,
          G_N_ELEMENTS (control))) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver did not accept the bitstream parameters."), (NULL));
    goto fail;
  }

  if (!gst_v4l2_request_queue (request, 0)) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver did not accept the decode request."), (NULL));
    goto fail;
  }

  gst_v4l2_codec_vp8_dec_reset_picture (self);

  return GST_FLOW_OK;

fail:
  gst_v4l2_codec_vp8_dec_reset_picture (self);

  if (flow_ret != GST_FLOW_OK)
    return flow_ret;

  return GST_FLOW_ERROR;
}

static gboolean
gst_v4l2_codec_vp8_dec_copy_output_buffer (GstV4l2CodecVp8Dec * self,
    GstVideoCodecFrame * codec_frame)
{
  GstVideoFrame src_frame;
  GstVideoFrame dest_frame;
  GstVideoInfo dest_vinfo;
  GstBuffer *buffer;

  gst_video_info_set_format (&dest_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
      self->width, self->height);

  buffer = gst_video_decoder_allocate_output_buffer (GST_VIDEO_DECODER (self));
  if (!buffer)
    goto fail;

  if (!gst_video_frame_map (&src_frame, &self->vinfo,
          codec_frame->output_buffer, GST_MAP_READ))
    goto fail;

  if (!gst_video_frame_map (&dest_frame, &dest_vinfo, buffer, GST_MAP_WRITE)) {
    gst_video_frame_unmap (&dest_frame);
    goto fail;
  }

  /* gst_video_frame_copy can crop this, but does not know, so let make it
   * think it's all right */
  GST_VIDEO_INFO_WIDTH (&src_frame.info) = self->width;
  GST_VIDEO_INFO_HEIGHT (&src_frame.info) = self->height;

  if (!gst_video_frame_copy (&dest_frame, &src_frame)) {
    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&dest_frame);
    goto fail;
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dest_frame);
  gst_buffer_replace (&codec_frame->output_buffer, buffer);
  gst_buffer_unref (buffer);

  return TRUE;

fail:
  GST_ERROR_OBJECT (self, "Failed copy output buffer.");
  return FALSE;
}

static GstFlowReturn
gst_v4l2_codec_vp8_dec_output_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = gst_vp8_picture_get_user_data (picture);
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  gint ret;

  if (codec_picture->discont_state) {
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (vdec, "Could not re-negotiate with updated state");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "Output picture %u",
      codec_picture->system_frame_number);

  ret = gst_v4l2_request_set_done (request);
  if (ret == 0) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Decoding frame took too long"), (NULL));
    goto error;
  } else if (ret < 0) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Decoding request failed: %s", g_strerror (errno)), (NULL));
    goto error;
  }

  g_return_val_if_fail (frame->output_buffer, GST_FLOW_ERROR);

  if (gst_v4l2_request_failed (request)) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Failed to decode frame %u", codec_picture->system_frame_number),
        (NULL));
    goto error;
  }

  /* Hold on reference buffers for the rest of the picture lifetime */
  gst_vp8_picture_set_user_data (picture,
      gst_buffer_ref (frame->output_buffer), (GDestroyNotify) gst_buffer_unref);

  if (self->copy_frames)
    gst_v4l2_codec_vp8_dec_copy_output_buffer (self, frame);

  gst_vp8_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_vp8_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_vp8_dec_set_flushing (GstV4l2CodecVp8Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_vp8_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_vp8_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_vp8_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_vp8_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_vp8_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_vp8_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_vp8_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_vp8_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_vp8_dec_init (GstV4l2CodecVp8Dec * self)
{
}

static void
gst_v4l2_codec_vp8_dec_subinit (GstV4l2CodecVp8Dec * self,
    GstV4l2CodecVp8DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
}

static void
gst_v4l2_codec_vp8_dec_dispose (GObject * object)
{
  GstV4l2CodecVp8Dec *self = GST_V4L2_CODEC_VP8_DEC (object);

  g_clear_object (&self->decoder);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_vp8_dec_class_init (GstV4l2CodecVp8DecClass * klass)
{
}

static void
gst_v4l2_codec_vp8_dec_subclass_init (GstV4l2CodecVp8DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp8DecoderClass *vp8decoder_class = GST_VP8_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_vp8_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_vp8_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_vp8_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless VP8 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based VP8 video decoder",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_sink_event);

  vp8decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_new_sequence);
  vp8decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_start_picture);
  vp8decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_decode_picture);
  vp8decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_end_picture);
  vp8decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_output_picture);
  vp8decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp8_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

static void gst_v4l2_codec_vp8_alpha_decode_bin_subclass_init
    (GstV4l2CodecAlphaDecodeBinClass * klass, gchar * decoder_name)
{
  GstV4l2CodecAlphaDecodeBinClass *adbin_class =
      (GstV4l2CodecAlphaDecodeBinClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  adbin_class->decoder_name = decoder_name;
  gst_element_class_add_static_pad_template (element_class, &alpha_template);

  gst_element_class_set_static_metadata (element_class,
      "VP8 Alpha Decoder", "Codec/Decoder/Video",
      "Wrapper bin to decode VP8 with alpha stream.",
      "Daniel Almeida <daniel.almeida@collabora.com>");
}

void
gst_v4l2_codec_vp8_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  gchar *element_name;
  GstCaps *src_caps, *alpha_caps;

  GST_DEBUG_CATEGORY_INIT (v4l2_vp8dec_debug, "v4l2codecs-vp8dec", 0,
      "V4L2 stateless VP8 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_VP8_FRAME,
          320, 240, 8))
    return;
  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering VP8 decoder since it produces no "
        "supported format");
    goto done;
  }

  gst_v4l2_decoder_register (plugin, GST_TYPE_V4L2_CODEC_VP8_DEC,
      (GClassInitFunc) gst_v4l2_codec_vp8_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_vp8_dec_subinit,
      "v4l2sl%svp8dec", device, rank, &element_name);

  if (!element_name)
    goto done;

  alpha_caps = gst_caps_from_string ("video/x-raw,format={I420, NV12}");

  if (gst_caps_can_intersect (src_caps, alpha_caps))
    gst_v4l2_codec_alpha_decode_bin_register (plugin,
        (GClassInitFunc) gst_v4l2_codec_vp8_alpha_decode_bin_subclass_init,
        element_name, "v4l2slvp8%salphadecodebin", device, rank);

  gst_caps_unref (alpha_caps);

done:
  gst_caps_unref (src_caps);
}
