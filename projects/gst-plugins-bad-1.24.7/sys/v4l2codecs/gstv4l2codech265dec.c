/* GStreamer
 * Copyright (C) 2020 Nicolas Dufresne <nicolas.dufresne@collabora.com>
 * Copyright (C) 2020 Safran Passenger Innovations LLC
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
#include "gstv4l2codech265dec.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2format.h"
#include "linux/v4l2-controls.h"

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define V4L2_MIN_KERNEL_VER_MAJOR 5
#define V4L2_MIN_KERNEL_VER_MINOR 20
#define V4L2_MIN_KERNEL_VERSION KERNEL_VERSION(V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR, 0)

GST_DEBUG_CATEGORY_STATIC (v4l2_h265dec_debug);
#define GST_CAT_DEFAULT v4l2_h265dec_debug

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, "
        "stream-format=(string) { hvc1, hev1, byte-stream }, "
        "alignment=(string) au")
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

struct _GstV4l2CodecH265Dec
{
  GstH265Decoder parent;
  GstV4l2Decoder *decoder;
  GstVideoCodecState *output_state;
  GstVideoInfo vinfo;
  GstVideoInfoDmaDrm vinfo_drm;
  gint display_width;
  gint display_height;
  gint coded_width;
  gint coded_height;
  guint bitdepth;
  guint chroma_format_idc;
  guint num_slices;
  gboolean first_slice;

  GstV4l2CodecAllocator *sink_allocator;
  GstV4l2CodecAllocator *src_allocator;
  GstV4l2CodecPool *src_pool;
  gint min_pool_size;
  gboolean has_videometa;
  gboolean streaming;
  gboolean copy_frames;
  gboolean need_sequence;

  struct v4l2_ctrl_hevc_sps sps;
  struct v4l2_ctrl_hevc_pps pps;
  struct v4l2_ctrl_hevc_scaling_matrix scaling_matrix;
  struct v4l2_ctrl_hevc_decode_params decode_params;
  GArray *slice_params;
  GArray *entry_point_offsets;

  enum v4l2_stateless_hevc_decode_mode decode_mode;
  enum v4l2_stateless_hevc_start_code start_code;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;

  gboolean support_scaling_matrix;
  gboolean support_slice_parameters;
  gboolean support_entry_point_offsets;

  GstVideoConverter *convert;
  gboolean need_crop;
  gint crop_rect_width, crop_rect_height;
  gint crop_rect_x, crop_rect_y;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecH265Dec, gst_v4l2_codec_h265_dec,
    GST_TYPE_H265_DECODER);

#define parent_class gst_v4l2_codec_h265_dec_parent_class

static gboolean
is_frame_based (GstV4l2CodecH265Dec * self)
{
  return (self->decode_mode ==
      V4L2_STATELESS_HEVC_DECODE_MODE_FRAME_BASED) &&
      !self->support_slice_parameters;
}

static gboolean
is_slice_based (GstV4l2CodecH265Dec * self)
{
  return self->decode_mode == V4L2_STATELESS_HEVC_DECODE_MODE_SLICE_BASED;
}

static gboolean
is_frame_based_with_slices (GstV4l2CodecH265Dec * self)
{
  return (self->decode_mode ==
      V4L2_STATELESS_HEVC_DECODE_MODE_FRAME_BASED) &&
      self->support_slice_parameters;
}

static gboolean
needs_start_codes (GstV4l2CodecH265Dec * self)
{
  return self->start_code == V4L2_STATELESS_HEVC_START_CODE_ANNEX_B;
}

static gboolean
gst_v4l2_decoder_h265_api_check (GstV4l2Decoder * decoder)
{
  guint i, ret_size;
  /* *INDENT-OFF* */
  #define SET_ID(cid) .id = (cid), .name = #cid
  struct
  {
    const gchar *name;
    unsigned int id;
    unsigned int size;
    gboolean optional;
  } controls[] = {
    {
      SET_ID (V4L2_CID_STATELESS_HEVC_SPS),
      .size = sizeof(struct v4l2_ctrl_hevc_sps),
    }, {
      SET_ID (V4L2_CID_STATELESS_HEVC_PPS),
      .size = sizeof(struct v4l2_ctrl_hevc_pps),
    }, {
      SET_ID (V4L2_CID_STATELESS_HEVC_SCALING_MATRIX),
      .size = sizeof(struct v4l2_ctrl_hevc_scaling_matrix),
      .optional = TRUE,
    }, {
      SET_ID (V4L2_CID_STATELESS_HEVC_DECODE_PARAMS),
      .size = sizeof(struct v4l2_ctrl_hevc_decode_params),
    }, {
      SET_ID (V4L2_CID_STATELESS_HEVC_SLICE_PARAMS),
      .size = sizeof(struct v4l2_ctrl_hevc_slice_params),
      .optional = TRUE,
    }
  };
  #undef SET_ID
  /* *INDENT-ON* */

  /*
   * Compatibility check: make sure the pointer controls are
   * the right size.
   */
  for (i = 0; i < G_N_ELEMENTS (controls); i++) {
    gboolean control_found;

    control_found = gst_v4l2_decoder_query_control_size (decoder,
        controls[i].id, &ret_size);

    if (!controls[i].optional && !control_found) {
      GST_WARNING ("Driver is missing %s support.", controls[i].name);
      return FALSE;
    }

    if (control_found && ret_size != controls[i].size) {
      GST_WARNING ("%s control size mismatch: got %d bytes but %d expected.",
          controls[i].name, ret_size, controls[i].size);
      return FALSE;
    }
  }

  return TRUE;
}


static gboolean
gst_v4l2_codec_h265_dec_open (GstVideoDecoder * decoder)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_HEVC_DECODE_MODE,
    },
    {
      .id = V4L2_CID_STATELESS_HEVC_START_CODE,
    },
  };
  struct v4l2_ext_control scaling_matrix[] = {
    {
      .id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX,
      .ptr = &self->scaling_matrix,
      .size = sizeof (self->scaling_matrix),
    },
  };
  /* *INDENT-ON* */

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open H265 decoder"),
        ("gst_v4l2_decoder_open() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  if (!gst_v4l2_decoder_get_controls (self->decoder, control,
          G_N_ELEMENTS (control))) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Driver did not report framing and start code method."),
        ("gst_v4l2_decoder_get_controls() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  self->support_scaling_matrix =
      gst_v4l2_decoder_get_controls (self->decoder, scaling_matrix,
      G_N_ELEMENTS (scaling_matrix));

  self->support_slice_parameters =
      gst_v4l2_decoder_query_control_size (self->decoder,
      V4L2_CID_STATELESS_HEVC_SLICE_PARAMS, NULL);

  self->support_entry_point_offsets =
      gst_v4l2_decoder_query_control_size (self->decoder,
      V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS, NULL);

  self->decode_mode = control[0].value;
  self->start_code = control[1].value;

  GST_INFO_OBJECT (self, "Opened H265 %s decoder %s",
      is_frame_based (self) ? "frame based" : is_slice_based (self) ?
      "slice based" : "frame based with slices",
      needs_start_codes (self) ? "using start-codes" : "without start-codes");
  gst_h265_decoder_set_process_ref_pic_lists (GST_H265_DECODER (self),
      is_slice_based (self) || is_frame_based_with_slices (self));

  return TRUE;
}

static gboolean
gst_v4l2_codec_h265_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  gst_v4l2_decoder_close (self->decoder);
  return TRUE;
}

static void
gst_v4l2_codec_h265_dec_streamoff (GstV4l2CodecH265Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_h265_dec_reset_allocation (GstV4l2CodecH265Dec * self)
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
gst_v4l2_codec_h265_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);

  gst_v4l2_codec_h265_dec_streamoff (self);
  gst_v4l2_codec_h265_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gint
get_pixel_bitdepth (GstV4l2CodecH265Dec * self)
{
  gint depth;

  switch (self->chroma_format_idc) {
    case 0:
      /* 4:0:0 */
      depth = self->bitdepth;
      break;
    case 1:
      /* 4:2:0 */
      depth = self->bitdepth + self->bitdepth / 2;
      break;
    case 2:
      /* 4:2:2 */
      depth = 2 * self->bitdepth;
      break;
    case 3:
      /* 4:4:4 */
      depth = 3 * self->bitdepth;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported chroma format %i",
          self->chroma_format_idc);
      depth = 0;
      break;
  }

  return depth;
}

static gboolean
gst_v4l2_codec_h265_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  GstH265Decoder *h265dec = GST_H265_DECODER (decoder);
  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_HEVC_SPS,
      .ptr = &self->sps,
      .size = sizeof (self->sps),
    },
  };
  /* *INDENT-ON* */
  GstCaps *peer_caps, *filter, *caps;
  GstStaticCaps *static_filter;

  /* Ignore downstream renegotiation request. */
  if (self->streaming)
    goto done;

  GST_DEBUG_OBJECT (self, "Negotiate");

  gst_v4l2_codec_h265_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_HEVC_SLICE,
          self->coded_width, self->coded_height, get_pixel_bitdepth (self))) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure H265 decoder"),
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
        ("Unsupported bitdepth/chroma format"),
        ("No support for %ux%u %ubit chroma IDC %i", self->coded_width,
            self->coded_height, self->bitdepth, self->chroma_format_idc));
    gst_caps_unref (caps);
    return FALSE;
  }
  gst_caps_unref (caps);

done:
  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  self->output_state =
      gst_v4l2_decoder_set_output_state (GST_VIDEO_DECODER (self), &self->vinfo,
      &self->vinfo_drm, self->display_width, self->display_height,
      h265dec->input_state);

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
gst_v4l2_codec_h265_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  GstCaps *caps = NULL;
  guint min = 0;

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

  self->sink_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SINK, self->min_pool_size + 2);
  self->src_allocator = gst_v4l2_codec_allocator_new (self->decoder,
      GST_PAD_SRC, self->min_pool_size + min + 1);
  self->src_pool = gst_v4l2_codec_pool_new (self->src_allocator, &self->vinfo);

no_internal_changes:
  /* Our buffer pool is internal, we will let the base class create a video
   * pool, and use it if we are running out of buffers or if downstream does
   * not support GstVideoMeta */
  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static void
gst_v4l2_codec_h265_dec_fill_sequence (GstV4l2CodecH265Dec * self,
    const GstH265SPS * sps)
{
  /* Whenever we update teh sps, we need to send it again */
  self->need_sequence = TRUE;

  /* *INDENT-OFF* */
  self->sps = (struct v4l2_ctrl_hevc_sps) {
        .video_parameter_set_id = sps->vps->id,
        .seq_parameter_set_id = sps->id,
	.pic_width_in_luma_samples = sps->pic_width_in_luma_samples,
	.pic_height_in_luma_samples = sps->pic_height_in_luma_samples,
	.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
	.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
	.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
	.sps_max_dec_pic_buffering_minus1 = sps->max_dec_pic_buffering_minus1[0],
	.sps_max_num_reorder_pics = sps->max_num_reorder_pics[0],
	.sps_max_latency_increase_plus1 = sps->max_latency_increase_plus1[0],
	.log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3,
	.log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size,
	.log2_min_luma_transform_block_size_minus2 = sps->log2_min_transform_block_size_minus2,
	.log2_diff_max_min_luma_transform_block_size = sps->log2_diff_max_min_transform_block_size,
	.max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter,
	.max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra,
	.num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets,
	.num_long_term_ref_pics_sps = sps->num_long_term_ref_pics_sps,
        .chroma_format_idc = sps->chroma_format_idc,
        .sps_max_sub_layers_minus1 = sps->max_sub_layers_minus1,
        .flags = (sps->separate_colour_plane_flag ? V4L2_HEVC_SPS_FLAG_SEPARATE_COLOUR_PLANE : 0) |
            (sps->scaling_list_enabled_flag ? V4L2_HEVC_SPS_FLAG_SCALING_LIST_ENABLED : 0) |
            (sps->amp_enabled_flag ? V4L2_HEVC_SPS_FLAG_AMP_ENABLED : 0) |
            (sps->sample_adaptive_offset_enabled_flag ? V4L2_HEVC_SPS_FLAG_SAMPLE_ADAPTIVE_OFFSET : 0) |
            (sps->long_term_ref_pics_present_flag ? V4L2_HEVC_SPS_FLAG_LONG_TERM_REF_PICS_PRESENT : 0) |
            (sps->temporal_mvp_enabled_flag ? V4L2_HEVC_SPS_FLAG_SPS_TEMPORAL_MVP_ENABLED : 0) |
            (sps->strong_intra_smoothing_enabled_flag ? V4L2_HEVC_SPS_FLAG_STRONG_INTRA_SMOOTHING_ENABLED : 0),
  };
  /* *INDENT-ON* */
  if (sps->pcm_enabled_flag) {
    self->sps.pcm_sample_bit_depth_luma_minus1 =
        sps->pcm_sample_bit_depth_luma_minus1;
    self->sps.pcm_sample_bit_depth_chroma_minus1 =
        sps->pcm_sample_bit_depth_chroma_minus1;
    self->sps.log2_min_pcm_luma_coding_block_size_minus3 =
        sps->log2_min_pcm_luma_coding_block_size_minus3;
    self->sps.log2_diff_max_min_pcm_luma_coding_block_size =
        sps->log2_diff_max_min_pcm_luma_coding_block_size;
    self->sps.flags |=
        V4L2_HEVC_SPS_FLAG_PCM_ENABLED | (sps->pcm_loop_filter_disabled_flag ?
        V4L2_HEVC_SPS_FLAG_PCM_LOOP_FILTER_DISABLED : 0);
  }
}

static void
gst_v4l2_codec_h265_dec_fill_pps (GstV4l2CodecH265Dec * self, GstH265PPS * pps)
{
  gint i;

  /* *INDENT-OFF* */
  self->pps = (struct v4l2_ctrl_hevc_pps) {
        .pic_parameter_set_id = pps->id,
	.num_extra_slice_header_bits = pps->num_extra_slice_header_bits,
	.num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1,
	.num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1,
	.init_qp_minus26 = pps->init_qp_minus26,
	.diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth,
	.pps_cb_qp_offset = pps->cb_qp_offset,
	.pps_cr_qp_offset = pps->cr_qp_offset,
	.pps_beta_offset_div2 = pps->beta_offset_div2,
	.pps_tc_offset_div2 = pps->tc_offset_div2,
	.log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2,
	.flags = (pps->dependent_slice_segments_enabled_flag ? V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT_ENABLED : 0) |
            (pps->output_flag_present_flag ? V4L2_HEVC_PPS_FLAG_OUTPUT_FLAG_PRESENT : 0) |
            (pps->sign_data_hiding_enabled_flag ? V4L2_HEVC_PPS_FLAG_SIGN_DATA_HIDING_ENABLED : 0) |
            (pps->cabac_init_present_flag ? V4L2_HEVC_PPS_FLAG_CABAC_INIT_PRESENT : 0) |
            (pps->constrained_intra_pred_flag ? V4L2_HEVC_PPS_FLAG_CONSTRAINED_INTRA_PRED : 0) |
            (pps->transform_skip_enabled_flag ? V4L2_HEVC_PPS_FLAG_TRANSFORM_SKIP_ENABLED : 0) |
            (pps->cu_qp_delta_enabled_flag ? V4L2_HEVC_PPS_FLAG_CU_QP_DELTA_ENABLED : 0) |
            (pps->slice_chroma_qp_offsets_present_flag ? V4L2_HEVC_PPS_FLAG_PPS_SLICE_CHROMA_QP_OFFSETS_PRESENT : 0) |
            (pps->weighted_pred_flag ? V4L2_HEVC_PPS_FLAG_WEIGHTED_PRED : 0) |
            (pps->weighted_bipred_flag ? V4L2_HEVC_PPS_FLAG_WEIGHTED_BIPRED : 0) |
            (pps->transquant_bypass_enabled_flag ? V4L2_HEVC_PPS_FLAG_TRANSQUANT_BYPASS_ENABLED : 0) |
            (pps->tiles_enabled_flag ? V4L2_HEVC_PPS_FLAG_TILES_ENABLED : 0) |
            (pps->entropy_coding_sync_enabled_flag ? V4L2_HEVC_PPS_FLAG_ENTROPY_CODING_SYNC_ENABLED : 0) |
            (pps->loop_filter_across_tiles_enabled_flag ? V4L2_HEVC_PPS_FLAG_LOOP_FILTER_ACROSS_TILES_ENABLED : 0) |
            (pps->loop_filter_across_slices_enabled_flag ? V4L2_HEVC_PPS_FLAG_PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED : 0 ) |
            (pps->deblocking_filter_override_enabled_flag ? V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_OVERRIDE_ENABLED : 0) |
            (pps->deblocking_filter_disabled_flag ? V4L2_HEVC_PPS_FLAG_PPS_DISABLE_DEBLOCKING_FILTER : 0) |
            (pps->lists_modification_present_flag ? V4L2_HEVC_PPS_FLAG_LISTS_MODIFICATION_PRESENT : 0) |
            (pps->slice_segment_header_extension_present_flag ? V4L2_HEVC_PPS_FLAG_SLICE_SEGMENT_HEADER_EXTENSION_PRESENT : 0) |
            (pps->deblocking_filter_control_present_flag ? V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT : 0) |
            (pps->uniform_spacing_flag ? V4L2_HEVC_PPS_FLAG_UNIFORM_SPACING : 0),
  };
  /* *INDENT-ON* */

  if (pps->tiles_enabled_flag) {
    self->pps.num_tile_columns_minus1 = pps->num_tile_columns_minus1;
    self->pps.num_tile_rows_minus1 = pps->num_tile_rows_minus1;

    /* This should not be needed if we use uniform spacing, but Cedrus driver
     * depends on it for now. As GStreamer parser do set that, lets just pass
     * the information we have. */
    for (i = 0; i <= pps->num_tile_columns_minus1; i++)
      self->pps.column_width_minus1[i] = pps->column_width_minus1[i];
    for (i = 0; i <= pps->num_tile_rows_minus1; i++)
      self->pps.row_height_minus1[i] = pps->row_height_minus1[i];
  }
}

static void
gst_v4l2_codec_h265_dec_fill_scaling_matrix (GstV4l2CodecH265Dec * self,
    GstH265PPS * pps)
{
  GstH265ScalingList *sl;
  gint i;

  if (!pps->sps->scaling_list_enabled_flag) {
    memset (&self->scaling_matrix, 0, sizeof (self->scaling_matrix));
    return;
  }

  if (pps->scaling_list_data_present_flag)
    sl = &pps->scaling_list;
  else if (pps->sps->scaling_list_data_present_flag)
    sl = &pps->sps->scaling_list;
  /* The default scaling list is strored in the pps */
  else
    sl = &pps->scaling_list;

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_lists_4x4); i++)
    gst_h265_quant_matrix_4x4_get_raster_from_uprightdiagonal
        (self->scaling_matrix.scaling_list_4x4[i], sl->scaling_lists_4x4[i]);

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_lists_8x8); i++)
    gst_h265_quant_matrix_8x8_get_raster_from_uprightdiagonal
        (self->scaling_matrix.scaling_list_8x8[i], sl->scaling_lists_8x8[i]);

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_lists_16x16); i++)
    gst_h265_quant_matrix_16x16_get_raster_from_uprightdiagonal
        (self->scaling_matrix.scaling_list_16x16[i],
        sl->scaling_lists_16x16[i]);

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_lists_32x32); i++)
    gst_h265_quant_matrix_32x32_get_raster_from_uprightdiagonal
        (self->scaling_matrix.scaling_list_32x32[i],
        sl->scaling_lists_32x32[i]);

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_list_dc_coef_minus8_16x16); i++)
    self->scaling_matrix.scaling_list_dc_coef_16x16[i] =
        sl->scaling_list_dc_coef_minus8_16x16[i] + 8;

  for (i = 0; i < G_N_ELEMENTS (sl->scaling_list_dc_coef_minus8_32x32); i++)
    self->scaling_matrix.scaling_list_dc_coef_32x32[i] =
        sl->scaling_list_dc_coef_minus8_32x32[i] + 8;
}

static guint
get_slice_header_byte_offset (GstH265Slice * slice)
{
  guint epb_count, nal_header_bytes;

  epb_count = slice->header.n_emulation_prevention_bytes;
  nal_header_bytes = slice->nalu.header_bytes;

  return nal_header_bytes + (slice->header.header_size + 7) / 8 - epb_count;
}

static void
gst_v4l2_codec_h265_dec_fill_slice_params (GstV4l2CodecH265Dec * self,
    GstH265Slice * slice, GstH265Picture * picture)
{
  gint n = self->num_slices++;
  GstH265SliceHdr *slice_hdr = &slice->header;
  GstH265PPS *pps = slice_hdr->pps;
  gsize slice_size = slice->nalu.size;
  gsize sc_offset = 0;
  struct v4l2_ctrl_hevc_slice_params *params;
  gint i, j;
  gint chroma_weight, chroma_log2_weight_denom;
  /* TODO adjust this if sps_ext is later supported */
  const gint32 WpOffsetHalfRangeC = 1 << 7;

  /* Ensure array is large enough */
  if (self->slice_params->len < self->num_slices)
    g_array_set_size (self->slice_params, self->slice_params->len * 2);

  if (needs_start_codes (self))
    sc_offset = 3;

  /* *INDENT-OFF* */
  params = &g_array_index (self->slice_params, struct v4l2_ctrl_hevc_slice_params, n);
  *params = (struct v4l2_ctrl_hevc_slice_params) {
    .bit_size = (slice_size + sc_offset) * 8,
    .data_byte_offset = get_slice_header_byte_offset (slice) + sc_offset,
    .num_entry_point_offsets = slice_hdr->num_entry_point_offsets,
    .nal_unit_type = slice->nalu.type,
    .nuh_temporal_id_plus1 = slice->nalu.temporal_id_plus1,
    .slice_type = slice_hdr->type,
    .colour_plane_id = slice_hdr->colour_plane_id,
    .slice_pic_order_cnt = picture->pic_order_cnt,
    .num_ref_idx_l0_active_minus1 = slice_hdr->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = slice_hdr->num_ref_idx_l1_active_minus1,
    .collocated_ref_idx = slice_hdr->collocated_ref_idx,
    .five_minus_max_num_merge_cand = slice_hdr->five_minus_max_num_merge_cand,
    .slice_qp_delta = slice_hdr->qp_delta,
    .slice_cb_qp_offset = slice_hdr->cb_qp_offset,
    .slice_cr_qp_offset = slice_hdr->cr_qp_offset,
    .slice_act_y_qp_offset = slice_hdr->slice_act_y_qp_offset,
    .slice_act_cb_qp_offset = slice_hdr->slice_act_cb_qp_offset,
    .slice_act_cr_qp_offset = slice_hdr->slice_act_cr_qp_offset,
    .slice_beta_offset_div2 = slice_hdr->beta_offset_div2,
    .slice_tc_offset_div2 = slice_hdr->tc_offset_div2,
    .pic_struct = picture->pic_struct,
    .slice_segment_addr = slice_hdr->segment_address,
    .short_term_ref_pic_set_size = slice_hdr->short_term_ref_pic_set_size,
    .long_term_ref_pic_set_size = slice_hdr->long_term_ref_pic_set_size,
    .pred_weight_table = (struct v4l2_hevc_pred_weight_table) {
        .luma_log2_weight_denom = slice_hdr->pred_weight_table.luma_log2_weight_denom,
        .delta_chroma_log2_weight_denom = slice_hdr->pred_weight_table.delta_chroma_log2_weight_denom,
    },
    .flags =
        (slice_hdr->sao_luma_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_SAO_LUMA : 0) |
        (slice_hdr->sao_chroma_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_SAO_CHROMA : 0) |
        (slice_hdr->temporal_mvp_enabled_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_TEMPORAL_MVP_ENABLED : 0) |
        (slice_hdr->mvd_l1_zero_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_MVD_L1_ZERO : 0) |
        (slice_hdr->cabac_init_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_CABAC_INIT : 0) |
        (slice_hdr->collocated_from_l0_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_COLLOCATED_FROM_L0 : 0) |
        (slice_hdr->use_integer_mv_flag ? V4L2_HEVC_SLICE_PARAMS_FLAG_USE_INTEGER_MV : 0) |
        (slice_hdr->deblocking_filter_disabled_flag ?
             V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_DEBLOCKING_FILTER_DISABLED : 0) |
        (slice_hdr->loop_filter_across_slices_enabled_flag ?
             V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED : 0) |
        (slice_hdr->dependent_slice_segment_flag ?
             V4L2_HEVC_SLICE_PARAMS_FLAG_DEPENDENT_SLICE_SEGMENT : 0),
  };
  /* *INDENT-ON* */

  for (i = 0; i < slice_hdr->num_entry_point_offsets; i++) {
    guint32 entry_point_offset = slice_hdr->entry_point_offset_minus1[i] + 1;
    g_array_append_val (self->entry_point_offsets, entry_point_offset);
  }

  if (GST_H265_IS_I_SLICE (slice_hdr) ||
      (!pps->weighted_pred_flag && GST_H265_IS_P_SLICE (slice_hdr)) ||
      (!pps->weighted_bipred_flag && GST_H265_IS_B_SLICE (slice_hdr)))
    return;

  for (i = 0; i <= slice_hdr->num_ref_idx_l0_active_minus1; i++) {
    if (!slice_hdr->pred_weight_table.luma_weight_l0_flag[i])
      continue;

    params->pred_weight_table.delta_luma_weight_l0[i] =
        slice_hdr->pred_weight_table.delta_luma_weight_l0[i];
    params->pred_weight_table.luma_offset_l0[i] =
        slice_hdr->pred_weight_table.luma_offset_l0[i];
  }

  chroma_log2_weight_denom =
      slice_hdr->pred_weight_table.luma_log2_weight_denom +
      slice_hdr->pred_weight_table.delta_chroma_log2_weight_denom;

  if (slice_hdr->pps->sps->chroma_array_type != 0) {

    for (i = 0; i <= slice_hdr->num_ref_idx_l0_active_minus1; i++) {
      if (!slice_hdr->pred_weight_table.chroma_weight_l0_flag[i])
        continue;

      for (j = 0; j < 2; j++) {
        gint16 delta_chroma_offset_l0 =
            slice_hdr->pred_weight_table.delta_chroma_offset_l0[i][j];
        gint chroma_offset;

        params->pred_weight_table.delta_chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table.delta_chroma_weight_l0[i][j];

        /* Find  ChromaWeightL0 */
        chroma_weight = (1 << chroma_log2_weight_denom) +
            slice_hdr->pred_weight_table.delta_chroma_weight_l0[i][j];
        chroma_offset = WpOffsetHalfRangeC + delta_chroma_offset_l0 -
            ((WpOffsetHalfRangeC * chroma_weight) >> chroma_log2_weight_denom);

        /* 7-56 */
        params->pred_weight_table.chroma_offset_l0[i][j] =
            CLAMP (chroma_offset, -WpOffsetHalfRangeC, WpOffsetHalfRangeC - 1);
      }
    }
  }

  /* Skip l1 if this is not a B-Frame. */
  if (!GST_H265_IS_B_SLICE (slice_hdr))
    return;

  for (i = 0; i <= slice_hdr->num_ref_idx_l1_active_minus1; i++) {
    if (!slice_hdr->pred_weight_table.luma_weight_l1_flag[i])
      continue;

    params->pred_weight_table.delta_luma_weight_l1[i] =
        slice_hdr->pred_weight_table.delta_luma_weight_l1[i];
    params->pred_weight_table.luma_offset_l1[i] =
        slice_hdr->pred_weight_table.luma_offset_l1[i];
  }

  if (slice_hdr->pps->sps->chroma_array_type != 0) {
    for (i = 0; i <= slice_hdr->num_ref_idx_l1_active_minus1; i++) {
      if (!slice_hdr->pred_weight_table.chroma_weight_l1_flag[i])
        continue;

      for (j = 0; j < 2; j++) {
        gint16 delta_chroma_offset_l1 =
            slice_hdr->pred_weight_table.delta_chroma_offset_l1[i][j];
        gint chroma_offset;

        params->pred_weight_table.delta_chroma_weight_l1[i][j] =
            slice_hdr->pred_weight_table.delta_chroma_weight_l1[i][j];

        /* Find  ChromaWeightL1 */
        chroma_weight = (1 << chroma_log2_weight_denom) +
            slice_hdr->pred_weight_table.delta_chroma_weight_l1[i][j];

        chroma_offset = WpOffsetHalfRangeC + delta_chroma_offset_l1 -
            ((WpOffsetHalfRangeC * chroma_weight) >> chroma_log2_weight_denom);

        /* 7-56 */
        params->pred_weight_table.chroma_offset_l1[i][j] =
            CLAMP (chroma_offset, -WpOffsetHalfRangeC, WpOffsetHalfRangeC - 1);
      }
    }
  }
}

static guint8
lookup_dpb_index (struct v4l2_hevc_dpb_entry dpb[16], GstH265Picture * ref_pic)
{
  guint64 ref_ts;
  gint i;

  /* Reference list may have wholes in case a ref is missing, we should mark
   * the whole and avoid moving items in the list */
  if (!ref_pic)
    return 0xff;

  ref_ts = GST_CODEC_PICTURE_TS_NS (ref_pic);
  for (i = 0; i < 16; i++) {
    if (dpb[i].timestamp == ref_ts)
      return i;
  }

  return 0xff;
}

static void
gst_v4l2_codec_h265_dec_fill_references (GstV4l2CodecH265Dec * self,
    GArray * ref_pic_list0, GArray * ref_pic_list1)
{
  struct v4l2_ctrl_hevc_slice_params *slice_params;
  struct v4l2_ctrl_hevc_decode_params *decode_params = &self->decode_params;
  gint i;

  slice_params = &g_array_index (self->slice_params,
      struct v4l2_ctrl_hevc_slice_params, self->num_slices - 1);

  memset (slice_params->ref_idx_l0, 0xff, sizeof (slice_params->ref_idx_l0));
  memset (slice_params->ref_idx_l1, 0xff, sizeof (slice_params->ref_idx_l1));

  for (i = 0; i < ref_pic_list0->len; i++) {
    GstH265Picture *ref_pic =
        g_array_index (ref_pic_list0, GstH265Picture *, i);
    slice_params->ref_idx_l0[i] =
        lookup_dpb_index (decode_params->dpb, ref_pic);
  }

  for (i = 0; i < ref_pic_list1->len; i++) {
    GstH265Picture *ref_pic =
        g_array_index (ref_pic_list1, GstH265Picture *, i);
    slice_params->ref_idx_l1[i] =
        lookup_dpb_index (decode_params->dpb, ref_pic);
  }
}

static GstFlowReturn
gst_v4l2_codec_h265_dec_new_sequence (GstH265Decoder * decoder,
    const GstH265SPS * sps, gint max_dpb_size)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  gint crop_width = sps->width;
  gint crop_height = sps->height;
  gboolean negotiation_needed = FALSE;

  if (self->vinfo.finfo->format == GST_VIDEO_FORMAT_UNKNOWN)
    negotiation_needed = TRUE;

  /* TODO check if CREATE_BUFS is supported, and simply grow the pool */
  if (self->min_pool_size < max_dpb_size) {
    self->min_pool_size = max_dpb_size;
    negotiation_needed = TRUE;
  }

  self->need_crop = FALSE;
  if (sps->conformance_window_flag) {
    crop_width = sps->crop_rect_width;
    crop_height = sps->crop_rect_height;
    self->crop_rect_width = sps->crop_rect_width;
    self->crop_rect_height = sps->crop_rect_height;
    self->crop_rect_x = sps->crop_rect_x;
    self->crop_rect_y = sps->crop_rect_y;

    /* conformance_window_flag could be set but with zeroed
     * parameters so check if we really need to crop */
    self->need_crop |= self->crop_rect_width != sps->width;
    self->need_crop |= self->crop_rect_height != sps->height;
    self->need_crop |= self->crop_rect_x != 0;
    self->need_crop |= self->crop_rect_y != 0;
  }

  /* TODO Check if current buffers are large enough, and reuse them */
  if (self->display_width != crop_width || self->display_height != crop_height
      || self->coded_width != sps->width || self->coded_height != sps->height) {
    self->display_width = crop_width;
    self->display_height = crop_height;
    self->coded_width = sps->width;
    self->coded_height = sps->height;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Resolution changed to %dx%d (%ix%i)",
        self->display_width, self->display_height,
        self->coded_width, self->coded_height);
  }

  if (self->bitdepth != sps->bit_depth_luma_minus8 + 8) {
    self->bitdepth = sps->bit_depth_luma_minus8 + 8;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Bitdepth changed to %u", self->bitdepth);
  }

  if (self->chroma_format_idc != sps->chroma_format_idc) {
    self->chroma_format_idc = sps->chroma_format_idc;
    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Chroma format changed to %i",
        self->chroma_format_idc);
  }

  gst_v4l2_codec_h265_dec_fill_sequence (self, sps);

  if (negotiation_needed) {
    gst_v4l2_codec_h265_dec_streamoff (self);
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
        self->display_width, self->display_height);

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
    self->copy_frames = self->need_crop;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_h265_dec_ensure_bitstream (GstV4l2CodecH265Dec * self)
{
  if (self->bitstream)
    goto done;

  self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

  if (!self->bitstream) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to decode H265 stream."), (NULL));
    return FALSE;
  }

  if (!gst_memory_map (self->bitstream, &self->bitstream_map, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Could not access bitstream memory for writing"), (NULL));
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    return FALSE;
  }

done:
  /* We use this field to track how much we have written */
  self->bitstream_map.size = 0;

  return TRUE;
}

static void
gst_v4l2_codec_h265_dec_fill_decode_params (GstV4l2CodecH265Dec * self,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb)
{
  GstH265Decoder *decoder = (GstH265Decoder *) self;
  GArray *refs = gst_h265_dpb_get_pictures_all (dpb);
  gint i;

  /* *INDENT-OFF* */
  self->decode_params = (struct v4l2_ctrl_hevc_decode_params) {
    .pic_order_cnt_val = picture->pic_order_cnt,
    .num_poc_st_curr_before = decoder->NumPocStCurrBefore,
    .num_poc_st_curr_after = decoder->NumPocStCurrAfter,
    .num_poc_lt_curr = decoder->NumPocLtCurr,
    .num_delta_pocs_of_ref_rps_idx = slice->header.short_term_ref_pic_sets.NumDeltaPocsOfRefRpsIdx,
    .flags =
      (GST_H265_IS_NAL_TYPE_IRAP (slice->nalu.type) ? V4L2_HEVC_DECODE_PARAM_FLAG_IRAP_PIC : 0) |
      (GST_H265_IS_NAL_TYPE_IDR (slice->nalu.type) ? V4L2_HEVC_DECODE_PARAM_FLAG_IDR_PIC : 0) |
      (slice->header.no_output_of_prior_pics_flag ? V4L2_HEVC_DECODE_PARAM_FLAG_NO_OUTPUT_OF_PRIOR : 0),
  };
  /* *INDENT-ON* */

  for (i = 0; i < refs->len; i++) {
    GstH265Picture *ref_pic = g_array_index (refs, GstH265Picture *, i);

    if (!ref_pic->ref)
      continue;

    /* *INDENT-OFF* */
    self->decode_params.dpb[self->decode_params.num_active_dpb_entries++] =
        (struct v4l2_hevc_dpb_entry) {
      /*
       * The reference is multiplied by 1000 because it's wassed as micro
       * seconds and this TS is nanosecond.
       */
      .timestamp = (guint64) GST_CODEC_PICTURE_FRAME_NUMBER (ref_pic) * 1000,
      .flags = ref_pic->long_term ? V4L2_HEVC_DPB_ENTRY_LONG_TERM_REFERENCE : 0,
      .field_pic = ref_pic->pic_struct,
      .pic_order_cnt_val = ref_pic->pic_order_cnt,
    };
    /* *INDENT-ON* */
  }

  for (i = 0; i < 16; i++) {
    self->decode_params.poc_st_curr_before[i] =
        lookup_dpb_index (self->decode_params.dpb,
        decoder->RefPicSetStCurrBefore[i]);
    self->decode_params.poc_st_curr_after[i] =
        lookup_dpb_index (self->decode_params.dpb,
        decoder->RefPicSetStCurrAfter[i]);
    self->decode_params.poc_lt_curr[i] =
        lookup_dpb_index (self->decode_params.dpb, decoder->RefPicSetLtCurr[i]);
  }

  g_array_unref (refs);
}

static GstFlowReturn
gst_v4l2_codec_h265_dec_start_picture (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);

  /* FIXME base class should not call us if negotiation failed */
  if (!self->sink_allocator)
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_v4l2_codec_h265_dec_ensure_bitstream (self))
    return GST_FLOW_ERROR;

  /* The base class will only emit new_sequence for allocation related changes
   * in the SPS, make sure to keep the SPS upt-to-date */
  if (slice->header.pps->sps->id != self->sps.seq_parameter_set_id)
    gst_v4l2_codec_h265_dec_fill_sequence (self, slice->header.pps->sps);

  gst_v4l2_codec_h265_dec_fill_pps (self, slice->header.pps);
  gst_v4l2_codec_h265_dec_fill_scaling_matrix (self, slice->header.pps);
  gst_v4l2_codec_h265_dec_fill_decode_params (self, picture, slice, dpb);

  self->first_slice = TRUE;
  self->num_slices = 0;
  g_array_set_size (self->entry_point_offsets, 0);

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_h265_dec_crop_output_buffer (GstV4l2CodecH265Dec * self,
    GstVideoFrame * dest_frame, GstVideoFrame * src_frame)
{
  GstVideoInfo dst_info = dest_frame->info;

  dst_info.fps_n = src_frame->info.fps_n;
  dst_info.fps_d = src_frame->info.fps_d;

  if (self->convert) {
    gboolean new_convert = FALSE;
    gint x = 0, y = 0, width = 0, height = 0;
    const GstStructure *config = gst_video_converter_get_config (self->convert);

    if (!gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_X, &x)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_Y, &y)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_WIDTH,
            &width)
        || !gst_structure_get_int (config, GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT,
            &height))
      new_convert = TRUE;

    new_convert |= (self->crop_rect_x != x);
    new_convert |= (self->crop_rect_y != y);
    new_convert |= (self->crop_rect_width != width);
    new_convert |= (self->crop_rect_height != height);

    /* No need to check dest, it always has (0,0) -> (width, height) */

    if (new_convert)
      g_clear_pointer (&self->convert, gst_video_converter_free);
  }

  if (!self->convert) {
    self->convert = gst_video_converter_new (&src_frame->info, &dst_info,
        gst_structure_new ("options",
            GST_VIDEO_CONVERTER_OPT_DITHER_METHOD,
            GST_TYPE_VIDEO_DITHER_METHOD, GST_VIDEO_DITHER_NONE,
            GST_VIDEO_CONVERTER_OPT_DITHER_QUANTIZATION,
            G_TYPE_UINT, 0,
            GST_VIDEO_CONVERTER_OPT_CHROMA_MODE,
            GST_TYPE_VIDEO_CHROMA_MODE, GST_VIDEO_CHROMA_MODE_NONE,
            GST_VIDEO_CONVERTER_OPT_MATRIX_MODE,
            GST_TYPE_VIDEO_MATRIX_MODE, GST_VIDEO_MATRIX_MODE_NONE,
            GST_VIDEO_CONVERTER_OPT_SRC_X, G_TYPE_INT, self->crop_rect_x,
            GST_VIDEO_CONVERTER_OPT_SRC_Y, G_TYPE_INT, self->crop_rect_y,
            GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, G_TYPE_INT,
            self->crop_rect_width, GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT,
            G_TYPE_INT, self->crop_rect_height, GST_VIDEO_CONVERTER_OPT_DEST_X,
            G_TYPE_INT, 0, GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 0,
            GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, self->display_width,
            GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT,
            self->display_height, NULL));

    if (!self->convert) {
      GST_WARNING_OBJECT (self, "failed to create a video convert");
      return FALSE;
    }
  }

  gst_video_converter_frame (self->convert, src_frame, dest_frame);

  return TRUE;
}

static gboolean
gst_v4l2_codec_h265_dec_copy_output_buffer (GstV4l2CodecH265Dec * self,
    GstVideoCodecFrame * codec_frame)
{
  GstVideoFrame src_frame;
  GstVideoFrame dest_frame;
  GstVideoInfo dest_vinfo;
  GstBuffer *buffer;

  gst_video_info_set_format (&dest_vinfo, GST_VIDEO_INFO_FORMAT (&self->vinfo),
      self->display_width, self->display_height);

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

  if (self->need_crop) {
    if (!gst_v4l2_codec_h265_dec_crop_output_buffer (self, &dest_frame,
            &src_frame)) {
      gst_video_frame_unmap (&src_frame);
      gst_video_frame_unmap (&dest_frame);
      GST_ERROR_OBJECT (self, "fail to apply the video crop.");
      goto fail;
    }
  } else {
    /* gst_video_frame_copy can crop this, but does not know, so let make it
     * think it's all right */
    GST_VIDEO_INFO_WIDTH (&src_frame.info) = self->display_width;
    GST_VIDEO_INFO_HEIGHT (&src_frame.info) = self->display_height;

    if (!gst_video_frame_copy (&dest_frame, &src_frame)) {
      gst_video_frame_unmap (&src_frame);
      gst_video_frame_unmap (&dest_frame);
      goto fail;
    }
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
gst_v4l2_codec_h265_dec_output_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = gst_h265_picture_get_user_data (picture);
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
        ("Decoding frame %u took too long", codec_picture->system_frame_number),
        (NULL));
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
  gst_h265_picture_set_user_data (picture,
      gst_buffer_ref (frame->output_buffer), (GDestroyNotify) gst_buffer_unref);

  if (self->copy_frames)
    gst_v4l2_codec_h265_dec_copy_output_buffer (self, frame);

  gst_h265_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_h265_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_h265_dec_reset_picture (GstV4l2CodecH265Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }

  self->num_slices = 0;
  g_array_set_size (self->entry_point_offsets, 0);
}

static gboolean
gst_v4l2_codec_h265_dec_ensure_output_buffer (GstV4l2CodecH265Dec * self,
    GstVideoCodecFrame * frame)
{
  GstBuffer *buffer;
  GstFlowReturn flow_ret;

  if (frame->output_buffer)
    return TRUE;

  flow_ret = gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (self->src_pool),
      &buffer, NULL);
  if (flow_ret != GST_FLOW_OK) {
    if (flow_ret == GST_FLOW_FLUSHING)
      GST_DEBUG_OBJECT (self, "Frame decoding aborted, we are flushing.");
    else
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
          ("No more picture buffer available."), (NULL));
    return FALSE;
  }

  frame->output_buffer = buffer;
  return TRUE;
}

static gboolean
gst_v4l2_codec_h265_dec_submit_bitstream (GstV4l2CodecH265Dec * self,
    GstH265Picture * picture, guint flags)
{
  GstV4l2Request *prev_request, *request = NULL;
  gsize bytesused;
  gboolean ret = FALSE;
  gint num_controls = 0;

  /* *INDENT-OFF* */
  /* Reserve space for controls */
  struct v4l2_ext_control control[] = {
    { }, /* SPS */
    { }, /* PPS */
    { }, /* DECODE_PARAMS */
    { }, /* SLICE_PARAMS */
    { }, /* SCALING_MATRIX */
    { }, /* ENTRY_POINT_OFFSETS */
  };
  /* *INDENT-ON* */

  prev_request = gst_h265_picture_get_user_data (picture);

  bytesused = self->bitstream_map.size;
  gst_memory_unmap (self->bitstream, &self->bitstream_map);
  self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  gst_memory_resize (self->bitstream, 0, bytesused);

  if (prev_request) {
    request = gst_v4l2_decoder_alloc_sub_request (self->decoder, prev_request,
        self->bitstream);
  } else {
    GstVideoCodecFrame *frame;
    guint32 system_frame_number = GST_CODEC_PICTURE_FRAME_NUMBER (picture);

    frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
        system_frame_number);
    g_return_val_if_fail (frame, FALSE);

    if (!gst_v4l2_codec_h265_dec_ensure_output_buffer (self, frame))
      goto done;

    request = gst_v4l2_decoder_alloc_request (self->decoder,
        system_frame_number, self->bitstream, frame->output_buffer);

    gst_video_codec_frame_unref (frame);
  }

  if (!request) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Failed to allocate a media request object."), (NULL));
    goto done;
  }

  if (self->need_sequence) {
    control[num_controls].id = V4L2_CID_STATELESS_HEVC_SPS;
    control[num_controls].ptr = &self->sps;
    control[num_controls].size = sizeof (self->sps);
    num_controls++;
    self->need_sequence = FALSE;
  }

  if (self->first_slice) {
    control[num_controls].id = V4L2_CID_STATELESS_HEVC_PPS;
    control[num_controls].ptr = &self->pps;
    control[num_controls].size = sizeof (self->pps);
    num_controls++;

    if (self->support_scaling_matrix) {
      control[num_controls].id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX;
      control[num_controls].ptr = &self->scaling_matrix;
      control[num_controls].size = sizeof (self->scaling_matrix);
      num_controls++;
    }

    control[num_controls].id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS;
    control[num_controls].ptr = &self->decode_params;
    control[num_controls].size = sizeof (self->decode_params);
    num_controls++;

    self->first_slice = FALSE;
  }

  /* Slice parameters are only filled for slice based or frame based with
   * slices decoders */
  if (self->num_slices && !is_frame_based (self)) {
    control[num_controls].id = V4L2_CID_STATELESS_HEVC_SLICE_PARAMS;
    control[num_controls].ptr = self->slice_params->data;
    control[num_controls].size = g_array_get_element_size (self->slice_params)
        * self->num_slices;
    num_controls++;

    if (self->support_entry_point_offsets && self->entry_point_offsets->len) {
      control[num_controls].id = V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS;
      control[num_controls].ptr = self->entry_point_offsets->data;
      control[num_controls].size =
          g_array_get_element_size (self->entry_point_offsets)
          * self->entry_point_offsets->len;
      num_controls++;
    }
  }

  if (num_controls > G_N_ELEMENTS (control))
    g_error ("Set too many controls, increase control[] size");

  if (!gst_v4l2_decoder_set_controls (self->decoder, request, control,
          num_controls)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the bitstream parameters."), (NULL));
    goto done;
  }

  if (!gst_v4l2_request_queue (request, flags)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Driver did not accept the decode request."), (NULL));
    goto done;
  }

  gst_h265_picture_set_user_data (picture, g_steal_pointer (&request),
      (GDestroyNotify) gst_v4l2_request_unref);
  ret = TRUE;

done:
  if (request)
    gst_v4l2_request_unref (request);

  gst_v4l2_codec_h265_dec_reset_picture (self);

  return ret;
}

static GstFlowReturn
gst_v4l2_codec_h265_dec_decode_slice (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice,
    GArray * ref_pic_list0, GArray * ref_pic_list1)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  gsize sc_off = 0;
  gsize nal_size;
  guint8 *bitstream_data;

  if (!is_frame_based (self)) {
    if (is_slice_based (self)) {
      if (self->bitstream_map.size) {
        /* In slice mode, we submit the pending slice asking the accelerator to
         * hold on the picture */
        if (!gst_v4l2_codec_h265_dec_submit_bitstream (self, picture,
                V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF)
            || !gst_v4l2_codec_h265_dec_ensure_bitstream (self))
          return GST_FLOW_ERROR;
      }
    }
    /* in frame based mode with slices, we need to provide the required data for
     * the whole frame, therefore we don't submit the bitstream here */
    gst_v4l2_codec_h265_dec_fill_slice_params (self, slice, picture);
    gst_v4l2_codec_h265_dec_fill_references (self, ref_pic_list0,
        ref_pic_list1);
  }

  /* if it is the first slice segment provide the short and long term
   * reference pictures set size */
  if (slice->header.first_slice_segment_in_pic_flag) {
    struct v4l2_ctrl_hevc_decode_params *decode_params = &self->decode_params;

    decode_params->short_term_ref_pic_set_size =
        slice->header.short_term_ref_pic_set_size;
    decode_params->long_term_ref_pic_set_size =
        slice->header.long_term_ref_pic_set_size;
  }

  bitstream_data = self->bitstream_map.data + self->bitstream_map.size;

  if (needs_start_codes (self))
    sc_off = 3;
  nal_size = sc_off + slice->nalu.size;

  if (self->bitstream_map.size + nal_size > self->bitstream_map.maxsize) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space to send all slice of an H265 frame."), (NULL));
    return GST_FLOW_ERROR;
  }

  if (needs_start_codes (self)) {
    bitstream_data[0] = 0x00;
    bitstream_data[1] = 0x00;
    bitstream_data[2] = 0x01;
  }

  memcpy (bitstream_data + sc_off, slice->nalu.data + slice->nalu.offset,
      slice->nalu.size);
  self->bitstream_map.size += nal_size;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_h265_dec_end_picture (GstH265Decoder * decoder,
    GstH265Picture * picture)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);

  if (!gst_v4l2_codec_h265_dec_submit_bitstream (self, picture, 0))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static guint
gst_v4l2_codec_h265_dec_get_preferred_output_delay (GstH265Decoder * decoder,
    gboolean is_live)
{

  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);
  guint delay;

  if (is_live)
    delay = 0;
  else
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);
  return delay;
}

static void
gst_v4l2_codec_h265_dec_set_flushing (GstV4l2CodecH265Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_h265_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_h265_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_h265_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_h265_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_h265_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_h265_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_h265_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_h265_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_h265_dec_init (GstV4l2CodecH265Dec * self)
{
}

static void
gst_v4l2_codec_h265_dec_subinit (GstV4l2CodecH265Dec * self,
    GstV4l2CodecH265DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
  self->slice_params = g_array_sized_new (FALSE, TRUE,
      sizeof (struct v4l2_ctrl_hevc_slice_params), 4);
  g_array_set_size (self->slice_params, 4);
  self->entry_point_offsets = g_array_sized_new (FALSE, TRUE,
      sizeof (guint32), 4);
}

static void
gst_v4l2_codec_h265_dec_dispose (GObject * object)
{
  GstV4l2CodecH265Dec *self = GST_V4L2_CODEC_H265_DEC (object);

  g_clear_object (&self->decoder);
  g_clear_pointer (&self->slice_params, g_array_unref);
  g_clear_pointer (&self->entry_point_offsets, g_array_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_h265_dec_class_init (GstV4l2CodecH265DecClass * klass)
{
}

static void
gst_v4l2_codec_h265_dec_subclass_init (GstV4l2CodecH265DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH265DecoderClass *h265decoder_class = GST_H265_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_h265_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_h265_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_h265_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless H.265 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based H.265 video decoder",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_sink_event);

  h265decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_new_sequence);
  h265decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_output_picture);
  h265decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_start_picture);
  h265decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_decode_slice);
  h265decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_end_picture);
  h265decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h265_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

void
gst_v4l2_codec_h265_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  GstCaps *src_caps;
  guint version;

  GST_DEBUG_CATEGORY_INIT (v4l2_h265dec_debug, "v4l2codecs-h265dec", 0,
      "V4L2 stateless h265 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_HEVC_SLICE,
          320, 240, 8))
    return;
  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering H265 decoder since it produces no "
        "supported format");
    goto done;
  }

  version = gst_v4l2_decoder_get_version (decoder);
  if (version < V4L2_MIN_KERNEL_VERSION)
    GST_WARNING ("V4L2 API v%u.%u too old, at least v%u.%u required",
        (version >> 16) & 0xff, (version >> 8) & 0xff,
        V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR);

  if (!gst_v4l2_decoder_h265_api_check (decoder)) {
    GST_WARNING ("Not registering H265 decoder as it failed ABI check.");
    goto done;
  }

  gst_v4l2_decoder_register (plugin, GST_TYPE_V4L2_CODEC_H265_DEC,
      (GClassInitFunc) gst_v4l2_codec_h265_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_h265_dec_subinit,
      "v4l2sl%sh265dec", device, rank, NULL);

done:
  gst_caps_unref (src_caps);
}
