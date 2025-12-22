/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdxvaav1decoder.h"
#include <string.h>
#include <vector>

#include "gstdxvatypedef.h"

GST_DEBUG_CATEGORY_STATIC (gst_dxva_av1_decoder_debug);
#define GST_CAT_DEFAULT gst_dxva_av1_decoder_debug

/* *INDENT-OFF* */
struct _GstDxvaAV1DecoderPrivate
{
  GstAV1SequenceHeaderOBU seq_hdr;
  DXVA_PicParams_AV1 pic_params;

  std::vector<DXVA_Tile_AV1> tile_list;
  std::vector<guint8> bitstream_buffer;
  GPtrArray *ref_pics = nullptr;

  guint max_width = 0;
  guint max_height = 0;
  guint bitdepth = 0;

  gboolean configured = FALSE;
};
/* *INDENT-ON* */

static void gst_dxva_av1_decoder_finalize (GObject * object);

static gboolean gst_dxva_av1_decoder_start (GstVideoDecoder * decoder);

static GstFlowReturn gst_dxva_av1_decoder_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size);
static GstFlowReturn gst_dxva_av1_decoder_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture);
static GstAV1Picture *gst_dxva_av1_decoder_duplicate_picture (GstAV1Decoder *
    decoder, GstVideoCodecFrame * frame, GstAV1Picture * picture);
static GstFlowReturn
gst_dxva_av1_decoder_start_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Dpb * dpb);
static GstFlowReturn gst_dxva_av1_decoder_decode_tile (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Tile * tile);
static GstFlowReturn gst_dxva_av1_decoder_end_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture);
static GstFlowReturn gst_dxva_av1_decoder_output_picture (GstAV1Decoder *
    decoder, GstVideoCodecFrame * frame, GstAV1Picture * picture);

#define gst_dxva_av1_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstDxvaAV1Decoder,
    gst_dxva_av1_decoder, GST_TYPE_AV1_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_dxva_av1_decoder_debug, "dxvaav1decoder",
        0, "dxvaav1decoder"));

static void
gst_dxva_av1_decoder_class_init (GstDxvaAV1DecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstAV1DecoderClass *av1decoder_class = GST_AV1_DECODER_CLASS (klass);

  gobject_class->finalize = gst_dxva_av1_decoder_finalize;

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_start);

  av1decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_new_sequence);
  av1decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_new_picture);
  av1decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_duplicate_picture);
  av1decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_start_picture);
  av1decoder_class->decode_tile =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_decode_tile);
  av1decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_end_picture);
  av1decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_dxva_av1_decoder_output_picture);
}

static void
gst_dxva_av1_decoder_init (GstDxvaAV1Decoder * self)
{
  self->priv = new GstDxvaAV1DecoderPrivate ();
  self->priv->ref_pics = g_ptr_array_new ();
}

static void
gst_dxva_av1_decoder_finalize (GObject * object)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (object);
  GstDxvaAV1DecoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->ref_pics);
  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dxva_av1_decoder_reset (GstDxvaAV1Decoder * self)
{
  GstDxvaAV1DecoderPrivate *priv = self->priv;

  priv->max_width = 0;
  priv->max_height = 0;
  priv->bitdepth = 0;
  priv->configured = FALSE;
}

static gboolean
gst_dxva_av1_decoder_start (GstVideoDecoder * decoder)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);

  gst_dxva_av1_decoder_reset (self);

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static GstFlowReturn
gst_dxva_av1_decoder_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderPrivate *priv = self->priv;
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);
  gboolean modified = FALSE;
  guint max_width, max_height;
  GstVideoInfo info;
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstFlowReturn ret;

  GST_LOG_OBJECT (self, "new sequence");

  if (seq_hdr->seq_profile != GST_AV1_PROFILE_0) {
    GST_WARNING_OBJECT (self, "Unsupported profile %d", seq_hdr->seq_profile);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (seq_hdr->num_planes != 3) {
    GST_WARNING_OBJECT (self, "Monochrome is not supported");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  priv->seq_hdr = *seq_hdr;

  if (priv->bitdepth != seq_hdr->bit_depth) {
    GST_INFO_OBJECT (self, "Bitdepth changed %d -> %d", priv->bitdepth,
        seq_hdr->bit_depth);
    priv->bitdepth = seq_hdr->bit_depth;
    modified = TRUE;
  }

  max_width = seq_hdr->max_frame_width_minus_1 + 1;
  max_height = seq_hdr->max_frame_height_minus_1 + 1;

  if (priv->max_width != max_width || priv->max_height != max_height) {
    GST_INFO_OBJECT (self, "Resolution changed %dx%d -> %dx%d",
        priv->max_width, priv->max_height, max_width, max_height);
    priv->max_width = max_width;
    priv->max_height = max_height;
    modified = TRUE;
  }

  if (!modified && priv->configured)
    return GST_FLOW_OK;

  if (priv->bitdepth == 8) {
    out_format = GST_VIDEO_FORMAT_NV12;
  } else if (priv->bitdepth == 10) {
    out_format = GST_VIDEO_FORMAT_P010_10LE;
  } else {
    GST_WARNING_OBJECT (self, "Invalid bit-depth %d", seq_hdr->bit_depth);
    priv->configured = FALSE;
    return GST_FLOW_NOT_NEGOTIATED;
  }

  gst_video_info_set_format (&info,
      out_format, priv->max_width, priv->max_height);

  g_assert (klass->configure);
  ret = klass->configure (self, decoder->input_state, &info, 0, 0,
      priv->max_width, priv->max_height, max_dpb_size);

  if (ret == GST_FLOW_OK) {
    priv->configured = TRUE;
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_WARNING_OBJECT (self, "Couldn't negotiate with new sequence");
      ret = GST_FLOW_NOT_NEGOTIATED;
    }
  } else {
    priv->configured = FALSE;
  }

  return ret;
}

static GstFlowReturn
gst_dxva_av1_decoder_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);

  g_assert (klass->new_picture);

  return klass->new_picture (self, GST_CODEC_PICTURE (picture));
}

static GstAV1Picture *
gst_dxva_av1_decoder_duplicate_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);
  GstAV1Picture *new_picture;

  g_assert (klass->duplicate_picture);

  new_picture = gst_av1_picture_new ();
  if (klass->duplicate_picture (self, GST_CODEC_PICTURE (picture),
          GST_CODEC_PICTURE (new_picture)) != GST_FLOW_OK) {
    gst_av1_picture_unref (new_picture);
    return nullptr;
  }

  return new_picture;
}

static GstFlowReturn
gst_dxva_av1_decoder_start_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Dpb * dpb)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderPrivate *priv = self->priv;
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);
  const GstAV1SequenceHeaderOBU *seq_hdr = &priv->seq_hdr;
  const GstAV1FrameHeaderOBU *frame_hdr = &picture->frame_hdr;
  DXVA_PicParams_AV1 *pic_params = &priv->pic_params;
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  guint i, j;
  GstFlowReturn ret;
  guint8 picture_id;

  g_assert (klass->start_picture);
  g_assert (klass->get_picture_id);

  ret = klass->start_picture (self, codec_picture, &picture_id);
  if (ret != GST_FLOW_OK)
    return ret;

  priv->bitstream_buffer.resize (0);
  priv->tile_list.resize (0);
  g_ptr_array_set_size (priv->ref_pics, 0);

  memset (pic_params, 0, sizeof (DXVA_PicParams_AV1));

  pic_params->width = frame_hdr->frame_width;
  pic_params->height = frame_hdr->frame_height;

  pic_params->max_width = seq_hdr->max_frame_width_minus_1 + 1;
  pic_params->max_height = seq_hdr->max_frame_height_minus_1 + 1;

  pic_params->CurrPicTextureIndex = picture_id;
  pic_params->superres_denom = frame_hdr->superres_denom;
  pic_params->bitdepth = seq_hdr->bit_depth;
  pic_params->seq_profile = seq_hdr->seq_profile;

  /* TILES */
  pic_params->tiles.cols = frame_hdr->tile_info.tile_cols;
  pic_params->tiles.rows = frame_hdr->tile_info.tile_rows;
  pic_params->tiles.context_update_id =
      frame_hdr->tile_info.context_update_tile_id;

  for (i = 0; i < pic_params->tiles.cols; i++) {
    pic_params->tiles.widths[i] =
        frame_hdr->tile_info.width_in_sbs_minus_1[i] + 1;
  }

  for (i = 0; i < pic_params->tiles.rows; i++) {
    pic_params->tiles.heights[i] =
        frame_hdr->tile_info.height_in_sbs_minus_1[i] + 1;
  }

  /* CODING TOOLS */
  pic_params->coding.use_128x128_superblock = seq_hdr->use_128x128_superblock;
  pic_params->coding.intra_edge_filter = seq_hdr->enable_filter_intra;
  pic_params->coding.interintra_compound = seq_hdr->enable_interintra_compound;
  pic_params->coding.masked_compound = seq_hdr->enable_masked_compound;
  pic_params->coding.warped_motion = frame_hdr->allow_warped_motion;
  pic_params->coding.dual_filter = seq_hdr->enable_dual_filter;
  pic_params->coding.jnt_comp = seq_hdr->enable_jnt_comp;
  pic_params->coding.screen_content_tools =
      frame_hdr->allow_screen_content_tools;
  pic_params->coding.integer_mv = frame_hdr->force_integer_mv;
  pic_params->coding.cdef = seq_hdr->enable_cdef;
  pic_params->coding.restoration = seq_hdr->enable_restoration;
  pic_params->coding.film_grain = seq_hdr->film_grain_params_present;
  pic_params->coding.intrabc = frame_hdr->allow_intrabc;
  pic_params->coding.high_precision_mv = frame_hdr->allow_high_precision_mv;
  pic_params->coding.switchable_motion_mode =
      frame_hdr->is_motion_mode_switchable;
  pic_params->coding.filter_intra = seq_hdr->enable_filter_intra;
  pic_params->coding.disable_frame_end_update_cdf =
      frame_hdr->disable_frame_end_update_cdf;
  pic_params->coding.disable_cdf_update = frame_hdr->disable_cdf_update;
  pic_params->coding.reference_mode = frame_hdr->reference_select;
  pic_params->coding.skip_mode = frame_hdr->skip_mode_present;
  pic_params->coding.reduced_tx_set = frame_hdr->reduced_tx_set;
  pic_params->coding.superres = frame_hdr->use_superres;
  pic_params->coding.tx_mode = frame_hdr->tx_mode;
  pic_params->coding.use_ref_frame_mvs = frame_hdr->use_ref_frame_mvs;
  pic_params->coding.enable_ref_frame_mvs = seq_hdr->enable_ref_frame_mvs;
  pic_params->coding.reference_frame_update = 1;

  /* FORMAT */
  pic_params->format.frame_type = frame_hdr->frame_type;
  pic_params->format.show_frame = frame_hdr->show_frame;
  pic_params->format.showable_frame = frame_hdr->showable_frame;
  pic_params->format.subsampling_x = seq_hdr->color_config.subsampling_x;
  pic_params->format.subsampling_y = seq_hdr->color_config.subsampling_y;
  pic_params->format.mono_chrome = seq_hdr->color_config.mono_chrome;

  /* REFERENCES */
  pic_params->primary_ref_frame = frame_hdr->primary_ref_frame;
  pic_params->order_hint = frame_hdr->order_hint;
  if (seq_hdr->enable_order_hint) {
    pic_params->order_hint_bits = seq_hdr->order_hint_bits_minus_1 + 1;
  } else {
    pic_params->order_hint_bits = 0;
  }

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    if (dpb->pic_list[i]) {
      GstAV1Picture *other_pic = dpb->pic_list[i];
      const GstAV1GlobalMotionParams *gmp = &frame_hdr->global_motion_params;

      pic_params->frame_refs[i].width = other_pic->frame_hdr.frame_width;
      pic_params->frame_refs[i].height = other_pic->frame_hdr.frame_height;
      for (j = 0; j < 6; j++) {
        pic_params->frame_refs[i].wmmat[j] =
            gmp->gm_params[GST_AV1_REF_LAST_FRAME + i][j];
      }
      pic_params->frame_refs[i].wminvalid =
          (gmp->gm_type[GST_AV1_REF_LAST_FRAME + i] ==
          GST_AV1_WARP_MODEL_IDENTITY);
      pic_params->frame_refs[i].wmtype =
          gmp->gm_type[GST_AV1_REF_LAST_FRAME + i];
      pic_params->frame_refs[i].Index = frame_hdr->ref_frame_idx[i];
    } else {
      pic_params->frame_refs[i].Index = 0xff;
    }
  }

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    pic_params->RefFrameMapTextureIndex[i] = 0xff;

    if (dpb->pic_list[i]) {
      GstAV1Picture *other_pic = dpb->pic_list[i];
      guint8 id;

      id = klass->get_picture_id (self, GST_CODEC_PICTURE (other_pic));
      if (id != 0xff) {
        pic_params->RefFrameMapTextureIndex[i] = id;
        g_ptr_array_add (priv->ref_pics, other_pic);
      }
    }
  }

  /* LOOP FILTER PARAMS */
  pic_params->loop_filter.filter_level[0] =
      frame_hdr->loop_filter_params.loop_filter_level[0];
  pic_params->loop_filter.filter_level[1] =
      frame_hdr->loop_filter_params.loop_filter_level[1];
  pic_params->loop_filter.filter_level_u =
      frame_hdr->loop_filter_params.loop_filter_level[2];
  pic_params->loop_filter.filter_level_v =
      frame_hdr->loop_filter_params.loop_filter_level[3];
  pic_params->loop_filter.sharpness_level =
      frame_hdr->loop_filter_params.loop_filter_sharpness;
  pic_params->loop_filter.mode_ref_delta_enabled =
      frame_hdr->loop_filter_params.loop_filter_delta_enabled;
  pic_params->loop_filter.mode_ref_delta_update =
      frame_hdr->loop_filter_params.loop_filter_delta_update;
  pic_params->loop_filter.delta_lf_multi =
      frame_hdr->loop_filter_params.delta_lf_multi;
  pic_params->loop_filter.delta_lf_present =
      frame_hdr->loop_filter_params.delta_lf_present;

  for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
    pic_params->loop_filter.ref_deltas[i] =
        frame_hdr->loop_filter_params.loop_filter_ref_deltas[i];
  }

  for (i = 0; i < 2; i++) {
    pic_params->loop_filter.mode_deltas[i] =
        frame_hdr->loop_filter_params.loop_filter_mode_deltas[i];
  }

  pic_params->loop_filter.delta_lf_res =
      frame_hdr->loop_filter_params.delta_lf_res;

  for (i = 0; i < GST_AV1_MAX_NUM_PLANES; i++) {
    pic_params->loop_filter.frame_restoration_type[i] =
        frame_hdr->loop_restoration_params.frame_restoration_type[i];
  }

  if (frame_hdr->loop_restoration_params.uses_lr) {
    pic_params->loop_filter.log2_restoration_unit_size[0] =
        (6 + frame_hdr->loop_restoration_params.lr_unit_shift);
    pic_params->loop_filter.log2_restoration_unit_size[1] =
        pic_params->loop_filter.log2_restoration_unit_size[2] =
        (6 + frame_hdr->loop_restoration_params.lr_unit_shift -
        frame_hdr->loop_restoration_params.lr_uv_shift);
  } else {
    pic_params->loop_filter.log2_restoration_unit_size[0] =
        pic_params->loop_filter.log2_restoration_unit_size[1] =
        pic_params->loop_filter.log2_restoration_unit_size[2] = 8;
  }

  /* QUANTIZATION */
  pic_params->quantization.delta_q_present =
      frame_hdr->quantization_params.delta_q_present;
  pic_params->quantization.delta_q_res =
      frame_hdr->quantization_params.delta_q_res;
  pic_params->quantization.base_qindex =
      frame_hdr->quantization_params.base_q_idx;
  pic_params->quantization.y_dc_delta_q =
      frame_hdr->quantization_params.delta_q_y_dc;
  pic_params->quantization.u_dc_delta_q =
      frame_hdr->quantization_params.delta_q_u_dc;
  pic_params->quantization.v_dc_delta_q =
      frame_hdr->quantization_params.delta_q_v_dc;
  pic_params->quantization.u_ac_delta_q =
      frame_hdr->quantization_params.delta_q_u_ac;
  pic_params->quantization.v_ac_delta_q =
      frame_hdr->quantization_params.delta_q_v_ac;
  if (frame_hdr->quantization_params.using_qmatrix) {
    pic_params->quantization.qm_y = frame_hdr->quantization_params.qm_y;
    pic_params->quantization.qm_u = frame_hdr->quantization_params.qm_u;
    pic_params->quantization.qm_v = frame_hdr->quantization_params.qm_v;
  } else {
    pic_params->quantization.qm_y = 0xff;
    pic_params->quantization.qm_u = 0xff;
    pic_params->quantization.qm_v = 0xff;
  }

  /* Cdef params */
  pic_params->cdef.damping = frame_hdr->cdef_params.cdef_damping - 3;
  pic_params->cdef.bits = frame_hdr->cdef_params.cdef_bits;

  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    guint8 secondary;

    pic_params->cdef.y_strengths[i].primary =
        frame_hdr->cdef_params.cdef_y_pri_strength[i];
    secondary = frame_hdr->cdef_params.cdef_y_sec_strength[i];
    if (secondary == 4)
      secondary--;
    pic_params->cdef.y_strengths[i].secondary = secondary;

    pic_params->cdef.uv_strengths[i].primary =
        frame_hdr->cdef_params.cdef_uv_pri_strength[i];
    secondary = frame_hdr->cdef_params.cdef_uv_sec_strength[i];
    if (secondary == 4)
      secondary--;
    pic_params->cdef.uv_strengths[i].secondary = secondary;
  }

  pic_params->interp_filter = frame_hdr->interpolation_filter;

  /* SEGMENTATION */
  pic_params->segmentation.enabled =
      frame_hdr->segmentation_params.segmentation_enabled;
  pic_params->segmentation.update_map =
      frame_hdr->segmentation_params.segmentation_update_map;
  pic_params->segmentation.update_data =
      frame_hdr->segmentation_params.segmentation_update_data;
  pic_params->segmentation.temporal_update =
      frame_hdr->segmentation_params.segmentation_temporal_update;

  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      pic_params->segmentation.feature_mask[i].mask |=
          (frame_hdr->segmentation_params.feature_enabled[i][j] << j);
      pic_params->segmentation.feature_data[i][j] =
          frame_hdr->segmentation_params.feature_data[i][j];
    }
  }

  /* FILM GRAIN */
  if (frame_hdr->film_grain_params.apply_grain) {
    pic_params->film_grain.apply_grain = 1;
    pic_params->film_grain.scaling_shift_minus8 =
        frame_hdr->film_grain_params.grain_scaling_minus_8;
    pic_params->film_grain.chroma_scaling_from_luma =
        frame_hdr->film_grain_params.chroma_scaling_from_luma;
    pic_params->film_grain.ar_coeff_lag =
        frame_hdr->film_grain_params.ar_coeff_lag;
    pic_params->film_grain.ar_coeff_shift_minus6 =
        frame_hdr->film_grain_params.ar_coeff_shift_minus_6;
    pic_params->film_grain.grain_scale_shift =
        frame_hdr->film_grain_params.grain_scale_shift;
    pic_params->film_grain.overlap_flag =
        frame_hdr->film_grain_params.overlap_flag;
    pic_params->film_grain.clip_to_restricted_range =
        frame_hdr->film_grain_params.clip_to_restricted_range;
    pic_params->film_grain.matrix_coeff_is_identity =
        (seq_hdr->color_config.matrix_coefficients == GST_AV1_MC_IDENTITY);
    pic_params->film_grain.grain_seed = frame_hdr->film_grain_params.grain_seed;
    for (i = 0; i < frame_hdr->film_grain_params.num_y_points && i < 14; i++) {
      pic_params->film_grain.scaling_points_y[i][0] =
          frame_hdr->film_grain_params.point_y_value[i];
      pic_params->film_grain.scaling_points_y[i][1] =
          frame_hdr->film_grain_params.point_y_scaling[i];
    }
    pic_params->film_grain.num_y_points =
        frame_hdr->film_grain_params.num_y_points;

    for (i = 0; i < frame_hdr->film_grain_params.num_cb_points && i < 10; i++) {
      pic_params->film_grain.scaling_points_cb[i][0] =
          frame_hdr->film_grain_params.point_cb_value[i];
      pic_params->film_grain.scaling_points_cb[i][1] =
          frame_hdr->film_grain_params.point_cb_scaling[i];
    }
    pic_params->film_grain.num_cb_points =
        frame_hdr->film_grain_params.num_cb_points;

    for (i = 0; i < frame_hdr->film_grain_params.num_cr_points && i < 10; i++) {
      pic_params->film_grain.scaling_points_cr[i][0] =
          frame_hdr->film_grain_params.point_cr_value[i];
      pic_params->film_grain.scaling_points_cr[i][1] =
          frame_hdr->film_grain_params.point_cr_scaling[i];
    }
    pic_params->film_grain.num_cr_points =
        frame_hdr->film_grain_params.num_cr_points;

    for (i = 0; i < 24; i++) {
      pic_params->film_grain.ar_coeffs_y[i] =
          frame_hdr->film_grain_params.ar_coeffs_y_plus_128[i];
    }

    for (i = 0; i < 25; i++) {
      pic_params->film_grain.ar_coeffs_cb[i] =
          frame_hdr->film_grain_params.ar_coeffs_cb_plus_128[i];
      pic_params->film_grain.ar_coeffs_cr[i] =
          frame_hdr->film_grain_params.ar_coeffs_cr_plus_128[i];
    }

    pic_params->film_grain.cb_mult = frame_hdr->film_grain_params.cb_mult;
    pic_params->film_grain.cb_luma_mult =
        frame_hdr->film_grain_params.cb_luma_mult;
    pic_params->film_grain.cr_mult = frame_hdr->film_grain_params.cr_mult;
    pic_params->film_grain.cr_luma_mult =
        frame_hdr->film_grain_params.cr_luma_mult;
    pic_params->film_grain.cb_offset = frame_hdr->film_grain_params.cb_offset;
    pic_params->film_grain.cr_offset = frame_hdr->film_grain_params.cr_offset;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_av1_decoder_decode_tile (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Tile * tile)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderPrivate *priv = self->priv;
  GstAV1TileGroupOBU *tile_group = &tile->tile_group;

  if (tile_group->num_tiles > priv->tile_list.size ())
    priv->tile_list.resize (tile_group->num_tiles);

  g_assert (tile_group->tg_end < priv->tile_list.size ());

  GST_LOG_OBJECT (self, "Decode tile, tile count %d (start: %d - end: %d)",
      tile_group->num_tiles, tile_group->tg_start, tile_group->tg_end);

  for (guint i = tile_group->tg_start; i <= tile_group->tg_end; i++) {
    DXVA_Tile_AV1 *dxva_tile = &priv->tile_list[i];

    GST_TRACE_OBJECT (self,
        "Tile offset %d, size %d, row %d, col %d",
        tile_group->entry[i].tile_offset, tile_group->entry[i].tile_size,
        tile_group->entry[i].tile_row, tile_group->entry[i].tile_col);

    dxva_tile->DataOffset = priv->bitstream_buffer.size () +
        tile_group->entry[i].tile_offset;
    dxva_tile->DataSize = tile_group->entry[i].tile_size;
    dxva_tile->row = tile_group->entry[i].tile_row;
    dxva_tile->column = tile_group->entry[i].tile_col;
    /* TODO: used for tile list OBU */
    dxva_tile->anchor_frame = 0xff;
  }

  GST_TRACE_OBJECT (self, "OBU size %d", tile->obu.obu_size);

  size_t pos = priv->bitstream_buffer.size ();
  priv->bitstream_buffer.resize (pos + tile->obu.obu_size);

  memcpy (&priv->bitstream_buffer[0] + pos, tile->obu.data, tile->obu.obu_size);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dxva_av1_decoder_end_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderPrivate *priv = self->priv;
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);
  size_t bitstream_buffer_size;
  size_t bitstream_pos;
  GstDxvaDecodingArgs args;

  if (priv->bitstream_buffer.empty () || priv->tile_list.empty ()) {
    GST_ERROR_OBJECT (self, "No bitstream buffer to submit");
    return GST_FLOW_ERROR;
  }

  memset (&args, 0, sizeof (GstDxvaDecodingArgs));

  bitstream_pos = priv->bitstream_buffer.size ();
  bitstream_buffer_size = GST_ROUND_UP_128 (bitstream_pos);

  if (bitstream_buffer_size > bitstream_pos) {
    size_t padding = bitstream_buffer_size - bitstream_pos;

    /* As per DXVA spec, total amount of bitstream buffer size should be
     * 128 bytes aligned. If actual data is not multiple of 128 bytes,
     * the last slice data needs to be zero-padded */
    priv->bitstream_buffer.resize (bitstream_buffer_size, 0);

    DXVA_Tile_AV1 & tile = priv->tile_list.back ();
    tile.DataSize += padding;
  }

  args.picture_params = &priv->pic_params;
  args.picture_params_size = sizeof (DXVA_PicParams_AV1);
  args.slice_control = &priv->tile_list[0];
  args.slice_control_size = sizeof (DXVA_Tile_AV1) * priv->tile_list.size ();
  args.bitstream = &priv->bitstream_buffer[0];
  args.bitstream_size = priv->bitstream_buffer.size ();

  g_assert (klass->end_picture);

  return klass->end_picture (self, GST_CODEC_PICTURE (picture),
      priv->ref_pics, &args);
}

static GstFlowReturn
gst_dxva_av1_decoder_output_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstDxvaAV1Decoder *self = GST_DXVA_AV1_DECODER (decoder);
  GstDxvaAV1DecoderClass *klass = GST_DXVA_AV1_DECODER_GET_CLASS (self);

  g_assert (klass->output_picture);

  GST_LOG_OBJECT (self, "Outputting picture %p, %dx%d", picture,
      picture->frame_hdr.render_width, picture->frame_hdr.render_height);

  return klass->output_picture (self, frame, GST_CODEC_PICTURE (picture),
      (GstVideoBufferFlags) 0, picture->frame_hdr.render_width,
      picture->frame_hdr.render_height);
}
