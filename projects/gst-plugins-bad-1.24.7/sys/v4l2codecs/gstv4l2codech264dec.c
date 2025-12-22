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
#include "gstv4l2codech264dec.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2format.h"
#include "linux/v4l2-controls.h"

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define V4L2_MIN_KERNEL_VER_MAJOR 5
#define V4L2_MIN_KERNEL_VER_MINOR 11
#define V4L2_MIN_KERNEL_VERSION KERNEL_VERSION(V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR, 0)

GST_DEBUG_CATEGORY_STATIC (v4l2_h264dec_debug);
#define GST_CAT_DEFAULT v4l2_h264dec_debug

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "stream-format=(string) { avc, avc3, byte-stream }, "
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

struct _GstV4l2CodecH264Dec
{
  GstH264Decoder parent;
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
  gboolean interlaced;
  gboolean need_sequence;
  gboolean copy_frames;
  gboolean scaling_matrix_present;

  struct v4l2_ctrl_h264_sps sps;
  struct v4l2_ctrl_h264_pps pps;
  struct v4l2_ctrl_h264_scaling_matrix scaling_matrix;
  struct v4l2_ctrl_h264_decode_params decode_params;
  struct v4l2_ctrl_h264_pred_weights pred_weight;
  GArray *slice_params;

  enum v4l2_stateless_h264_decode_mode decode_mode;
  enum v4l2_stateless_h264_start_code start_code;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecH264Dec, gst_v4l2_codec_h264_dec,
    GST_TYPE_H264_DECODER);

#define parent_class gst_v4l2_codec_h264_dec_parent_class

static gboolean
is_frame_based (GstV4l2CodecH264Dec * self)
{
  return self->decode_mode == V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED;
}

static gboolean
is_slice_based (GstV4l2CodecH264Dec * self)
{
  return self->decode_mode == V4L2_STATELESS_H264_DECODE_MODE_SLICE_BASED;
}

static gboolean
needs_start_codes (GstV4l2CodecH264Dec * self)
{
  return self->start_code == V4L2_STATELESS_H264_START_CODE_ANNEX_B;
}

static gboolean
gst_v4l2_decoder_h264_api_check (GstV4l2Decoder * decoder)
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
      SET_ID (V4L2_CID_STATELESS_H264_SPS),
      .size = sizeof(struct v4l2_ctrl_h264_sps),
    }, {
      SET_ID (V4L2_CID_STATELESS_H264_PPS),
      .size = sizeof(struct v4l2_ctrl_h264_pps),
    }, {
      SET_ID (V4L2_CID_STATELESS_H264_SCALING_MATRIX),
      .size = sizeof(struct v4l2_ctrl_h264_scaling_matrix),
      .optional = TRUE,
    }, {
      SET_ID (V4L2_CID_STATELESS_H264_DECODE_PARAMS),
      .size = sizeof(struct v4l2_ctrl_h264_decode_params),
    }, {
      SET_ID (V4L2_CID_STATELESS_H264_SLICE_PARAMS),
      .size = sizeof(struct v4l2_ctrl_h264_slice_params),
      .optional = TRUE,
    }, {
      SET_ID (V4L2_CID_STATELESS_H264_PRED_WEIGHTS),
      .size = sizeof(struct v4l2_ctrl_h264_pred_weights),
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
gst_v4l2_codec_h264_dec_open (GstVideoDecoder * decoder)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);

  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_H264_DECODE_MODE,
    },
    {
      .id = V4L2_CID_STATELESS_H264_START_CODE,
    },
  };
  /* *INDENT-ON* */

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open H264 decoder"),
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

  self->decode_mode = control[0].value;
  self->start_code = control[1].value;

  GST_INFO_OBJECT (self, "Opened H264 %s decoder %s",
      is_frame_based (self) ? "frame based" : "slice based",
      needs_start_codes (self) ? "using start-codes" : "without start-codes");
  gst_h264_decoder_set_process_ref_pic_lists (GST_H264_DECODER (self),
      is_slice_based (self));

  return TRUE;
}

static gboolean
gst_v4l2_codec_h264_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  gst_v4l2_decoder_close (self->decoder);
  return TRUE;
}

static void
gst_v4l2_codec_h264_dec_streamoff (GstV4l2CodecH264Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_h264_dec_reset_allocation (GstV4l2CodecH264Dec * self)
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
gst_v4l2_codec_h264_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);

  gst_v4l2_codec_h264_dec_streamoff (self);
  gst_v4l2_codec_h264_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gint
get_pixel_bitdepth (GstV4l2CodecH264Dec * self)
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
gst_v4l2_codec_h264_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  GstH264Decoder *h264dec = GST_H264_DECODER (decoder);
  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_H264_SPS,
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

  gst_v4l2_codec_h264_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_H264_SLICE,
          self->coded_width, self->coded_height, get_pixel_bitdepth (self))) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure H264 decoder"),
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
      h264dec->input_state);

  if (self->interlaced)
    self->output_state->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;

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
gst_v4l2_codec_h264_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  GstCaps *caps = NULL;
  guint min = 0, num_bitstream;

  /* If we are streaming here, then it means there is nothing allocation
   * related in the new state and allocation can be ignored */
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
gst_v4l2_codec_h264_dec_fill_sequence (GstV4l2CodecH264Dec * self,
    const GstH264SPS * sps)
{
  gint i;

  /* *INDENT-OFF* */
  self->sps = (struct v4l2_ctrl_h264_sps) {
    .profile_idc = sps->profile_idc,
    .constraint_set_flags = (sps->constraint_set0_flag)
        | (sps->constraint_set1_flag << 1) | (sps->constraint_set2_flag << 2)
        | (sps->constraint_set3_flag << 3) | (sps->constraint_set4_flag << 4)
        | (sps->constraint_set5_flag << 5),
    .level_idc = sps->level_idc,
    .seq_parameter_set_id = sps->id,
    .chroma_format_idc = sps->chroma_format_idc,
    .bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
    .log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4,
    .pic_order_cnt_type = sps->pic_order_cnt_type,
    .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
    .max_num_ref_frames = sps->num_ref_frames,
    .num_ref_frames_in_pic_order_cnt_cycle = sps->num_ref_frames_in_pic_order_cnt_cycle,
    .offset_for_non_ref_pic = sps->offset_for_non_ref_pic,
    .offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field,
    .pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1,
    .pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1,
    .flags = (sps->separate_colour_plane_flag ? V4L2_H264_SPS_FLAG_SEPARATE_COLOUR_PLANE : 0)
        | (sps->qpprime_y_zero_transform_bypass_flag ? V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS : 0)
        | (sps->delta_pic_order_always_zero_flag ? V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO : 0)
        | (sps->gaps_in_frame_num_value_allowed_flag ? V4L2_H264_SPS_FLAG_GAPS_IN_FRAME_NUM_VALUE_ALLOWED : 0)
        | (sps->frame_mbs_only_flag ? V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY : 0)
        | (sps->mb_adaptive_frame_field_flag ? V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD : 0)
        | (sps->direct_8x8_inference_flag ? V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE : 0),
  };
  /* *INDENT-ON* */

  for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
    self->sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];
}

static void
gst_v4l2_codec_h264_dec_fill_pps (GstV4l2CodecH264Dec * self, GstH264PPS * pps)
{
  /* *INDENT-OFF* */
  self->pps = (struct v4l2_ctrl_h264_pps) {
    .pic_parameter_set_id = pps->id,
    .seq_parameter_set_id = pps->sequence->id,
    .num_slice_groups_minus1 = pps->num_slice_groups_minus1,
    .num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_active_minus1,
    .weighted_bipred_idc = pps->weighted_bipred_idc,
    .pic_init_qp_minus26 = pps->pic_init_qp_minus26,
    .pic_init_qs_minus26 = pps->pic_init_qs_minus26,
    .chroma_qp_index_offset = pps->chroma_qp_index_offset,
    .second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset,
    .flags = 0
        | (pps->entropy_coding_mode_flag ? V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE : 0)
        | (pps->pic_order_present_flag ? V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT : 0)
        | (pps->weighted_pred_flag ? V4L2_H264_PPS_FLAG_WEIGHTED_PRED : 0)
        | (pps->deblocking_filter_control_present_flag ? V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT : 0)
        | (pps->constrained_intra_pred_flag ? V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED : 0)
        | (pps->redundant_pic_cnt_present_flag ? V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT : 0)
        | (pps->transform_8x8_mode_flag ? V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE : 0)
        | (self->scaling_matrix_present ? V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT : 0),
  };
  /* *INDENT-ON* */
}

static void
gst_v4l2_codec_h264_dec_fill_scaling_matrix (GstV4l2CodecH264Dec * self,
    GstH264PPS * pps)
{
  gint i, n;

  for (i = 0; i < G_N_ELEMENTS (pps->scaling_lists_4x4); i++)
    gst_h264_quant_matrix_4x4_get_raster_from_zigzag (self->
        scaling_matrix.scaling_list_4x4[i], pps->scaling_lists_4x4[i]);

  /* Avoid uninitialize data passed into ioctl() */
  memset (self->scaling_matrix.scaling_list_8x8, 0,
      sizeof (self->scaling_matrix.scaling_list_8x8));

  /* We need the first 2 entries (Y intra and Y inter for YCbCr 4:2:2 and
   * less, and the full 6 entries for 4:4:4, see Table 7-2 of the spec for
   * more details */
  n = (pps->sequence->chroma_format_idc == 3) ? 6 : 2;
  for (i = 0; i < n; i++)
    gst_h264_quant_matrix_8x8_get_raster_from_zigzag (self->
        scaling_matrix.scaling_list_8x8[i], pps->scaling_lists_8x8[i]);
}

static void
gst_v4l2_codec_h264_dec_fill_decoder_params (GstV4l2CodecH264Dec * self,
    GstH264SliceHdr * slice_hdr, GstH264Picture * picture, GstH264Dpb * dpb)
{
  GArray *refs = gst_h264_dpb_get_pictures_all (dpb);
  gint i, entry_id = 0;

  /* *INDENT-OFF* */
  self->decode_params = (struct v4l2_ctrl_h264_decode_params) {
    .nal_ref_idc = picture->nal_ref_idc,
    .frame_num = slice_hdr->frame_num,
    .idr_pic_id = slice_hdr->idr_pic_id,
    .pic_order_cnt_lsb = slice_hdr->pic_order_cnt_lsb,
    .delta_pic_order_cnt_bottom = slice_hdr->delta_pic_order_cnt_bottom,
    .delta_pic_order_cnt0 = slice_hdr->delta_pic_order_cnt[0],
    .delta_pic_order_cnt1 = slice_hdr->delta_pic_order_cnt[1],
    .dec_ref_pic_marking_bit_size = slice_hdr->dec_ref_pic_marking.bit_size,
    .pic_order_cnt_bit_size = slice_hdr->pic_order_cnt_bit_size,
    .slice_group_change_cycle = slice_hdr->slice_group_change_cycle,
    .flags = (picture->idr ? V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC : 0) |
             (slice_hdr->field_pic_flag ? V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC : 0) |
             (slice_hdr->bottom_field_flag ? V4L2_H264_DECODE_PARAM_FLAG_BOTTOM_FIELD : 0),
  };
  /* *INDENT-ON* */

  switch (picture->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      self->decode_params.top_field_order_cnt = picture->top_field_order_cnt;
      self->decode_params.bottom_field_order_cnt =
          picture->bottom_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      self->decode_params.top_field_order_cnt = picture->top_field_order_cnt;
      self->decode_params.bottom_field_order_cnt = 0;
      if (picture->other_field)
        self->decode_params.bottom_field_order_cnt =
            picture->other_field->bottom_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      self->decode_params.top_field_order_cnt = 0;
      if (picture->other_field)
        self->decode_params.top_field_order_cnt =
            picture->other_field->top_field_order_cnt;
      self->decode_params.bottom_field_order_cnt =
          picture->bottom_field_order_cnt;
      break;
  }

  for (i = 0; i < refs->len; i++) {
    GstH264Picture *ref_pic = g_array_index (refs, GstH264Picture *, i);
    gint pic_num = ref_pic->pic_num;
    gint frame_num = ref_pic->frame_num;
    struct v4l2_h264_dpb_entry *entry;

    /* Skip non-reference as they are not useful to decoding */
    if (!GST_H264_PICTURE_IS_REF (ref_pic))
      continue;

    /* The second field picture will be handled differently */
    if (ref_pic->second_field)
      continue;

    /* V4L2 uAPI uses pic_num for both PicNum and LongTermPicNum, and
     * frame_num for both FrameNum and LongTermFrameIdx */
    if (GST_H264_PICTURE_IS_LONG_TERM_REF (ref_pic)) {
      pic_num = ref_pic->long_term_pic_num;
      frame_num = ref_pic->long_term_frame_idx;
    }

    entry = &self->decode_params.dpb[entry_id++];
    /* *INDENT-OFF* */
    *entry = (struct v4l2_h264_dpb_entry) {
      /*
       * The reference is multiplied by 1000 because it's was set as micro
       * seconds and this TS is nanosecond.
       */
      .reference_ts = GST_CODEC_PICTURE_TS_NS (ref_pic),
      .frame_num = frame_num,
      .pic_num = pic_num,
      .flags = V4L2_H264_DPB_ENTRY_FLAG_VALID
          | (GST_H264_PICTURE_IS_REF (ref_pic) ? V4L2_H264_DPB_ENTRY_FLAG_ACTIVE : 0)
          | (GST_H264_PICTURE_IS_LONG_TERM_REF (ref_pic) ? V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM : 0)
          | (ref_pic->field_pic_flag ? V4L2_H264_DPB_ENTRY_FLAG_FIELD : 0),
    };
    /* *INDENT-ON* */

    switch (ref_pic->field) {
      case GST_H264_PICTURE_FIELD_FRAME:
        entry->top_field_order_cnt = ref_pic->top_field_order_cnt;
        entry->bottom_field_order_cnt = ref_pic->bottom_field_order_cnt;
        entry->fields = V4L2_H264_FRAME_REF;
        break;
      case GST_H264_PICTURE_FIELD_TOP_FIELD:
        entry->top_field_order_cnt = ref_pic->top_field_order_cnt;
        entry->fields = V4L2_H264_TOP_FIELD_REF;

        if (ref_pic->other_field) {
          entry->bottom_field_order_cnt =
              ref_pic->other_field->bottom_field_order_cnt;
          entry->fields |= V4L2_H264_BOTTOM_FIELD_REF;
        }
        break;
      case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
        entry->bottom_field_order_cnt = ref_pic->bottom_field_order_cnt;
        entry->fields = V4L2_H264_BOTTOM_FIELD_REF;

        if (ref_pic->other_field) {
          entry->top_field_order_cnt =
              ref_pic->other_field->top_field_order_cnt;
          entry->fields |= V4L2_H264_TOP_FIELD_REF;
        }
        break;
    }
  }

  g_array_unref (refs);
}

static void
gst_v4l2_codec_h264_dec_fill_pred_weight (GstV4l2CodecH264Dec * self,
    GstH264SliceHdr * slice_hdr)
{
  gint i, j;

  /* *INDENT-OFF* */
  self->pred_weight = (struct v4l2_ctrl_h264_pred_weights) {
    .luma_log2_weight_denom = slice_hdr->pred_weight_table.luma_log2_weight_denom,
    .chroma_log2_weight_denom = slice_hdr->pred_weight_table.chroma_log2_weight_denom,
  };
  /* *INDENT-ON* */

  for (i = 0; i <= slice_hdr->num_ref_idx_l0_active_minus1; i++) {
    self->pred_weight.weight_factors[0].luma_weight[i] =
        slice_hdr->pred_weight_table.luma_weight_l0[i];
    self->pred_weight.weight_factors[0].luma_offset[i] =
        slice_hdr->pred_weight_table.luma_offset_l0[i];
  }

  if (slice_hdr->pps->sequence->chroma_array_type != 0) {
    for (i = 0; i <= slice_hdr->num_ref_idx_l0_active_minus1; i++) {
      for (j = 0; j < 2; j++) {
        self->pred_weight.weight_factors[0].chroma_weight[i][j] =
            slice_hdr->pred_weight_table.chroma_weight_l0[i][j];
        self->pred_weight.weight_factors[0].chroma_offset[i][j] =
            slice_hdr->pred_weight_table.chroma_offset_l0[i][j];
      }
    }
  }

  /* Skip l1 if this is not a B-Frames. */
  if (slice_hdr->type % 5 != GST_H264_B_SLICE)
    return;

  for (i = 0; i <= slice_hdr->num_ref_idx_l1_active_minus1; i++) {
    self->pred_weight.weight_factors[1].luma_weight[i] =
        slice_hdr->pred_weight_table.luma_weight_l1[i];
    self->pred_weight.weight_factors[1].luma_offset[i] =
        slice_hdr->pred_weight_table.luma_offset_l1[i];
  }

  if (slice_hdr->pps->sequence->chroma_array_type != 0) {
    for (i = 0; i <= slice_hdr->num_ref_idx_l1_active_minus1; i++) {
      for (j = 0; j < 2; j++) {
        self->pred_weight.weight_factors[1].chroma_weight[i][j] =
            slice_hdr->pred_weight_table.chroma_weight_l1[i][j];
        self->pred_weight.weight_factors[1].chroma_offset[i][j] =
            slice_hdr->pred_weight_table.chroma_offset_l1[i][j];
      }
    }
  }
}

static guint
get_slice_header_bit_size (GstH264Slice * slice)
{
  return 8 * slice->nalu.header_bytes + slice->header.header_size
      - 8 * slice->header.n_emulation_prevention_bytes;
}

static void
gst_v4l2_codec_h264_dec_fill_slice_params (GstV4l2CodecH264Dec * self,
    GstH264Slice * slice)
{
  gint n = self->num_slices++;
  struct v4l2_ctrl_h264_slice_params *params;

  /* Ensure array is large enough */
  if (self->slice_params->len < self->num_slices)
    g_array_set_size (self->slice_params, self->slice_params->len * 2);

  /* *INDENT-OFF* */
  params = &g_array_index (self->slice_params, struct v4l2_ctrl_h264_slice_params, n);
  *params = (struct v4l2_ctrl_h264_slice_params) {
    .header_bit_size = get_slice_header_bit_size (slice),
    .first_mb_in_slice = slice->header.first_mb_in_slice,
    .slice_type = slice->header.type % 5,
    .colour_plane_id = slice->header.colour_plane_id,
    .redundant_pic_cnt = slice->header.redundant_pic_cnt,
    .cabac_init_idc = slice->header.cabac_init_idc,
    .slice_qp_delta = slice->header.slice_qp_delta,
    .slice_qs_delta = slice->header.slice_qs_delta,
    .disable_deblocking_filter_idc = slice->header.disable_deblocking_filter_idc,
    .slice_alpha_c0_offset_div2 = slice->header.slice_alpha_c0_offset_div2,
    .slice_beta_offset_div2 = slice->header.slice_beta_offset_div2,
    .num_ref_idx_l0_active_minus1 = slice->header.num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = slice->header.num_ref_idx_l1_active_minus1,
    .flags = (slice->header.direct_spatial_mv_pred_flag ? V4L2_H264_SLICE_FLAG_DIRECT_SPATIAL_MV_PRED : 0) |
             (slice->header.sp_for_switch_flag ? V4L2_H264_SLICE_FLAG_SP_FOR_SWITCH : 0),
  };
  /* *INDENT-ON* */
}

static guint8
lookup_dpb_index (struct v4l2_h264_dpb_entry dpb[16], GstH264Picture * ref_pic)
{
  guint64 ref_ts;
  gint i;

  /* Reference list may have wholes in case a ref is missing, we should mark
   * the whole and avoid moving items in the list */
  if (!ref_pic)
    return 0xff;

  /* DPB entries only stores first field in a merged fashion */
  if (ref_pic->second_field && ref_pic->other_field)
    ref_pic = ref_pic->other_field;

  ref_ts = (guint64) GST_CODEC_PICTURE_FRAME_NUMBER (ref_pic) * 1000;
  for (i = 0; i < 16; i++) {
    if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE
        && dpb[i].reference_ts == ref_ts)
      return i;
  }

  return 0xff;
}

static guint
_get_v4l2_fields_ref (GstH264Picture * ref_pic, gboolean merge)
{
  if (merge && ref_pic->other_field)
    return V4L2_H264_FRAME_REF;

  switch (ref_pic->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      return V4L2_H264_FRAME_REF;
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      return V4L2_H264_TOP_FIELD_REF;
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      return V4L2_H264_BOTTOM_FIELD_REF;
      break;
  }

  return V4L2_H264_FRAME_REF;
}

static void
gst_v4l2_codec_h264_dec_fill_references (GstV4l2CodecH264Dec * self,
    gboolean cur_is_frame, GArray * ref_pic_list0, GArray * ref_pic_list1)
{
  struct v4l2_ctrl_h264_slice_params *slice_params;
  gint i;

  slice_params = &g_array_index (self->slice_params,
      struct v4l2_ctrl_h264_slice_params, 0);

  memset (slice_params->ref_pic_list0, 0xff,
      sizeof (slice_params->ref_pic_list0));
  memset (slice_params->ref_pic_list1, 0xff,
      sizeof (slice_params->ref_pic_list1));

  for (i = 0; i < ref_pic_list0->len; i++) {
    GstH264Picture *ref_pic =
        g_array_index (ref_pic_list0, GstH264Picture *, i);
    slice_params->ref_pic_list0[i].index =
        lookup_dpb_index (self->decode_params.dpb, ref_pic);
    slice_params->ref_pic_list0[i].fields =
        _get_v4l2_fields_ref (ref_pic, cur_is_frame);
  }

  for (i = 0; i < ref_pic_list1->len; i++) {
    GstH264Picture *ref_pic =
        g_array_index (ref_pic_list1, GstH264Picture *, i);
    slice_params->ref_pic_list1[i].index =
        lookup_dpb_index (self->decode_params.dpb, ref_pic);
    slice_params->ref_pic_list1[i].fields =
        _get_v4l2_fields_ref (ref_pic, cur_is_frame);
  }
}

static GstFlowReturn
gst_v4l2_codec_h264_dec_new_sequence (GstH264Decoder * decoder,
    const GstH264SPS * sps, gint max_dpb_size)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  gint crop_width = sps->width;
  gint crop_height = sps->height;
  gboolean negotiation_needed = FALSE;
  gboolean interlaced;

  if (self->vinfo.finfo->format == GST_VIDEO_FORMAT_UNKNOWN)
    negotiation_needed = TRUE;

  /* TODO check if CREATE_BUFS is supported, and simply grow the pool */
  if (self->min_pool_size < max_dpb_size) {
    self->min_pool_size = max_dpb_size;
    negotiation_needed = TRUE;
  }

  if (sps->frame_cropping_flag) {
    crop_width = sps->crop_rect_width;
    crop_height = sps->crop_rect_height;
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

  interlaced = !sps->frame_mbs_only_flag;
  if (self->interlaced != interlaced) {
    self->interlaced = interlaced;

    negotiation_needed = TRUE;
    GST_INFO_OBJECT (self, "Interlaced mode changed to %d", interlaced);
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

  gst_v4l2_codec_h264_dec_fill_sequence (self, sps);
  self->need_sequence = TRUE;

  if (negotiation_needed) {
    gst_v4l2_codec_h264_dec_streamoff (self);
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
    self->copy_frames = FALSE;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_h264_dec_ensure_bitstream (GstV4l2CodecH264Dec * self)
{
  if (self->bitstream)
    goto done;

  self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

  if (!self->bitstream) {
    GST_ELEMENT_ERROR (self, RESOURCE, NO_SPACE_LEFT,
        ("Not enough memory to decode H264 stream."), (NULL));
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

static GstFlowReturn
gst_v4l2_codec_h264_dec_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);

  /* FIXME base class should not call us if negotiation failed */
  if (!self->sink_allocator)
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_v4l2_codec_h264_dec_ensure_bitstream (self))
    return GST_FLOW_ERROR;

  /*
   * Scaling matrix is present if there's one provided
   * by either the SPS or the PPS. This flag must be
   * set to true or false, before filling the PPS V4L2 control.
   */
  self->scaling_matrix_present =
      slice->header.pps->sequence->scaling_matrix_present_flag ||
      slice->header.pps->pic_scaling_matrix_present_flag;

  gst_v4l2_codec_h264_dec_fill_pps (self, slice->header.pps);

  if (self->scaling_matrix_present)
    gst_v4l2_codec_h264_dec_fill_scaling_matrix (self, slice->header.pps);

  gst_v4l2_codec_h264_dec_fill_decoder_params (self, &slice->header, picture,
      dpb);

  self->first_slice = TRUE;
  self->num_slices = 0;

  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_codec_h264_dec_copy_output_buffer (GstV4l2CodecH264Dec * self,
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

  /* gst_video_frame_copy can crop this, but does not know, so let make it
   * think it's all right */
  GST_VIDEO_INFO_WIDTH (&src_frame.info) = self->display_width;
  GST_VIDEO_INFO_HEIGHT (&src_frame.info) = self->display_height;

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
gst_v4l2_codec_h264_dec_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = gst_h264_picture_get_user_data (picture);
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
  gst_h264_picture_set_user_data (picture,
      gst_buffer_ref (frame->output_buffer), (GDestroyNotify) gst_buffer_unref);

  if (self->copy_frames)
    gst_v4l2_codec_h264_dec_copy_output_buffer (self, frame);

  gst_h264_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_h264_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_h264_dec_reset_picture (GstV4l2CodecH264Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }

  self->num_slices = 0;
}

static gboolean
gst_v4l2_codec_h264_dec_ensure_output_buffer (GstV4l2CodecH264Dec * self,
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
gst_v4l2_codec_h264_dec_submit_bitstream (GstV4l2CodecH264Dec * self,
    GstH264Picture * picture, guint flags)
{
  GstV4l2Request *prev_request, *request = NULL;
  gsize bytesused;
  gboolean ret = FALSE;
  guint num_controls = 0;

  /* *INDENT-OFF* */
  /* Reserve space for controls */
  struct v4l2_ext_control control[] = {
    { }, /* SPS */
    { }, /* PPS */
    { }, /* DECODE_PARAMS */
    { }, /* SLICE_PARAMS */
    { }, /* SCALING_MATRIX */
    { }, /* PRED_WEIGHTS */
  };
  /* *INDENT-ON* */

  prev_request = gst_h264_picture_get_user_data (picture);

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

    if (!gst_v4l2_codec_h264_dec_ensure_output_buffer (self, frame))
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
    control[num_controls].id = V4L2_CID_STATELESS_H264_SPS;
    control[num_controls].ptr = &self->sps;
    control[num_controls].size = sizeof (self->sps);
    num_controls++;
    self->need_sequence = FALSE;
  }

  if (self->first_slice) {
    control[num_controls].id = V4L2_CID_STATELESS_H264_PPS;
    control[num_controls].ptr = &self->pps;
    control[num_controls].size = sizeof (self->pps);
    num_controls++;

    if (self->scaling_matrix_present) {
      control[num_controls].id = V4L2_CID_STATELESS_H264_SCALING_MATRIX;
      control[num_controls].ptr = &self->scaling_matrix;
      control[num_controls].size = sizeof (self->scaling_matrix);
      num_controls++;
    }

    control[num_controls].id = V4L2_CID_STATELESS_H264_DECODE_PARAMS;
    control[num_controls].ptr = &self->decode_params;
    control[num_controls].size = sizeof (self->decode_params);
    num_controls++;

    self->first_slice = FALSE;
  }

  /* If it's not slice-based then it doesn't support per-slice controls. */
  if (is_slice_based (self)) {
    control[num_controls].id = V4L2_CID_STATELESS_H264_SLICE_PARAMS;
    control[num_controls].ptr = self->slice_params->data;
    control[num_controls].size = g_array_get_element_size (self->slice_params)
        * self->num_slices;
    num_controls++;

    control[num_controls].id = V4L2_CID_STATELESS_H264_PRED_WEIGHTS;
    control[num_controls].ptr = &self->pred_weight;
    control[num_controls].size = sizeof (self->pred_weight);
    num_controls++;
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

  gst_h264_picture_set_user_data (picture, g_steal_pointer (&request),
      (GDestroyNotify) gst_v4l2_request_unref);
  ret = TRUE;

done:
  if (request)
    gst_v4l2_request_unref (request);

  gst_v4l2_codec_h264_dec_reset_picture (self);

  return ret;
}

static GstFlowReturn
gst_v4l2_codec_h264_dec_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  gsize sc_off = 0;
  gsize nal_size;
  guint8 *bitstream_data;

  if (is_slice_based (self)) {
    if (self->bitstream_map.size) {
      /* In slice mode, we submit the pending slice asking the accelerator to
       * hold on the picture */
      if (!gst_v4l2_codec_h264_dec_submit_bitstream (self, picture,
              V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF)
          || !gst_v4l2_codec_h264_dec_ensure_bitstream (self))
        return GST_FLOW_ERROR;
    }

    gst_v4l2_codec_h264_dec_fill_slice_params (self, slice);
    gst_v4l2_codec_h264_dec_fill_pred_weight (self, &slice->header);
    gst_v4l2_codec_h264_dec_fill_references (self,
        GST_H264_PICTURE_IS_FRAME (picture), ref_pic_list0, ref_pic_list1);
  }

  bitstream_data = self->bitstream_map.data + self->bitstream_map.size;

  if (needs_start_codes (self))
    sc_off = 3;
  nal_size = sc_off + slice->nalu.size;

  if (self->bitstream_map.size + nal_size > self->bitstream_map.maxsize) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space to send all slice of an H264 frame."), (NULL));
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

  switch (slice->header.type % 5) {
    case GST_H264_P_SLICE:
      self->decode_params.flags |= V4L2_H264_DECODE_PARAM_FLAG_PFRAME;
      break;

    case GST_H264_B_SLICE:
      self->decode_params.flags |= V4L2_H264_DECODE_PARAM_FLAG_BFRAME;
      break;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_h264_dec_end_picture (GstH264Decoder * decoder,
    GstH264Picture * picture)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  guint flags = 0;

  /* Hold on the output frame if this is first field of a pair */
  if (picture->field != GST_H264_PICTURE_FIELD_FRAME && !picture->second_field)
    flags = V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF;

  if (!gst_v4l2_codec_h264_dec_submit_bitstream (self, picture, flags))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_h264_dec_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  GstV4l2Request *request = gst_h264_picture_get_user_data (first_field);

  if (!request) {
    GST_WARNING_OBJECT (self,
        "First picture does not have an associated request");
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (self, "Assigned request %i to second field.",
      gst_v4l2_request_get_fd (request));

  /* Associate the previous request with the new picture so that
   * submit_bitstream can create sub-request */
  gst_h264_picture_set_user_data (second_field, gst_v4l2_request_ref (request),
      (GDestroyNotify) gst_v4l2_request_unref);

  return GST_FLOW_OK;
}

static guint
gst_v4l2_codec_h264_dec_get_preferred_output_delay (GstH264Decoder * decoder,
    gboolean live)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);
  guint delay;

  if (live)
    delay = 0;
  else
    /* Just one for now, perhaps we can make this configurable in the future. */
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);

  return delay;
}

static void
gst_v4l2_codec_h264_dec_set_flushing (GstV4l2CodecH264Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_h264_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_h264_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_h264_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_h264_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_h264_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_h264_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_h264_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_h264_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_h264_dec_init (GstV4l2CodecH264Dec * self)
{
}

static void
gst_v4l2_codec_h264_dec_subinit (GstV4l2CodecH264Dec * self,
    GstV4l2CodecH264DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
  self->slice_params = g_array_sized_new (FALSE, TRUE,
      sizeof (struct v4l2_ctrl_h264_slice_params), 4);
  g_array_set_size (self->slice_params, 4);
}

static void
gst_v4l2_codec_h264_dec_dispose (GObject * object)
{
  GstV4l2CodecH264Dec *self = GST_V4L2_CODEC_H264_DEC (object);

  g_clear_object (&self->decoder);
  g_clear_pointer (&self->slice_params, g_array_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_h264_dec_class_init (GstV4l2CodecH264DecClass * klass)
{
}

static void
gst_v4l2_codec_h264_dec_subclass_init (GstV4l2CodecH264DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH264DecoderClass *h264decoder_class = GST_H264_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_h264_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_h264_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_h264_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless H.264 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based H.264 video decoder",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_sink_event);

  h264decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_new_sequence);
  h264decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_output_picture);
  h264decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_start_picture);
  h264decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_decode_slice);
  h264decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_end_picture);
  h264decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_new_field_picture);
  h264decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_h264_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

void
gst_v4l2_codec_h264_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  GstCaps *src_caps;
  guint version;

  GST_DEBUG_CATEGORY_INIT (v4l2_h264dec_debug, "v4l2codecs-h264dec", 0,
      "V4L2 stateless h264 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_H264_SLICE,
          320, 240, 8))
    return;
  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering H264 decoder since it produces no "
        "supported format");
    goto done;
  }

  version = gst_v4l2_decoder_get_version (decoder);
  if (version < V4L2_MIN_KERNEL_VERSION)
    GST_WARNING ("V4L2 API v%u.%u too old, at least v%u.%u required",
        (version >> 16) & 0xff, (version >> 8) & 0xff,
        V4L2_MIN_KERNEL_VER_MAJOR, V4L2_MIN_KERNEL_VER_MINOR);

  if (!gst_v4l2_decoder_h264_api_check (decoder)) {
    GST_WARNING ("Not registering H264 decoder as it failed ABI check.");
    goto done;
  }

  gst_v4l2_decoder_register (plugin,
      GST_TYPE_V4L2_CODEC_H264_DEC,
      (GClassInitFunc) gst_v4l2_codec_h264_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_h264_dec_subinit,
      "v4l2sl%sh264dec", device, rank, NULL);

done:
  gst_caps_unref (src_caps);
}
