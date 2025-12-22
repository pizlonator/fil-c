
/* GStreamer
 * Copyright (C) 2021 Daniel Almeida <daniel.almeida@collabora.com>
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
#include "gstv4l2codecvp9dec.h"
#include "gstv4l2format.h"
#include "linux/v4l2-controls.h"
#include "linux/videodev2.h"

GST_DEBUG_CATEGORY_STATIC (v4l2_vp9dec_debug);
#define GST_CAT_DEFAULT v4l2_vp9dec_debug

/* Used to mark picture that have been outputed */
#define FLAG_PICTURE_HOLDS_BUFFER GST_MINI_OBJECT_FLAG_LAST

enum
{
  PROP_0,
  PROP_LAST = PROP_0
};

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9, " "alignment=(string) frame")
    );

static GstStaticPadTemplate alpha_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp9, codec-alpha = (boolean) true, "
        "alignment = frame")
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

struct _GstV4l2CodecVp9Dec
{
  GstVp9Decoder parent;
  GstV4l2Decoder *decoder;
  GstVideoCodecState *output_state;
  GstVideoInfo vinfo;
  GstVideoInfoDmaDrm vinfo_drm;
  gint width;
  gint height;

  GstV4l2CodecAllocator *sink_allocator;
  GstV4l2CodecAllocator *src_allocator;
  GstV4l2CodecPool *src_pool;
  gboolean has_videometa;
  gboolean streaming;
  gboolean copy_frames;

  struct v4l2_ctrl_vp9_frame v4l2_vp9_frame;
  struct v4l2_ctrl_vp9_compressed_hdr v4l2_delta_probs;

  GstMemory *bitstream;
  GstMapInfo bitstream_map;

  /* will renegotiate if parser reports new values */
  guint bit_depth;
  guint color_range;
  guint profile;
  guint color_space;
  guint subsampling_x;
  guint subsampling_y;
};

G_DEFINE_ABSTRACT_TYPE (GstV4l2CodecVp9Dec, gst_v4l2_codec_vp9_dec,
    GST_TYPE_VP9_DECODER);

#define parent_class gst_v4l2_codec_vp9_dec_parent_class

static guint
gst_v4l2_codec_vp9_dec_get_preferred_output_delay (GstVp9Decoder * decoder,
    gboolean is_live)
{

  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  guint delay;

  if (is_live)
    delay = 0;
  else
    delay = 1;

  gst_v4l2_decoder_set_render_delay (self->decoder, delay);
  return delay;
}

static void
gst_v4l2_codec_vp9_dec_fill_lf_params (GstV4l2CodecVp9Dec * self,
    const GstVp9LoopFilterParams * lf)
{
  gint i;

  G_STATIC_ASSERT (sizeof (self->v4l2_vp9_frame.lf.ref_deltas) ==
      sizeof (lf->loop_filter_ref_deltas));
  G_STATIC_ASSERT (sizeof (self->v4l2_vp9_frame.lf.mode_deltas) ==
      sizeof (lf->loop_filter_mode_deltas));

  for (i = 0; i < G_N_ELEMENTS (self->v4l2_vp9_frame.lf.ref_deltas); i++)
    self->v4l2_vp9_frame.lf.ref_deltas[i] = lf->loop_filter_ref_deltas[i];

  for (i = 0; i < G_N_ELEMENTS (self->v4l2_vp9_frame.lf.mode_deltas); i++)
    self->v4l2_vp9_frame.lf.mode_deltas[i] = lf->loop_filter_mode_deltas[i];
}

static void
gst_v4l2_codec_vp9_dec_fill_seg_params (GstV4l2CodecVp9Dec * self,
    const GstVp9SegmentationParams * s)
{
  guint i;
  struct v4l2_vp9_segmentation *v4l2_segmentation = &self->v4l2_vp9_frame.seg;

  G_STATIC_ASSERT (sizeof (v4l2_segmentation->tree_probs) ==
      sizeof (s->segmentation_tree_probs));
  G_STATIC_ASSERT (sizeof (v4l2_segmentation->pred_probs) ==
      sizeof (s->segmentation_pred_prob));
  G_STATIC_ASSERT (sizeof (v4l2_segmentation->feature_data) ==
      sizeof (s->feature_data));

  for (i = 0; i < G_N_ELEMENTS (v4l2_segmentation->tree_probs); i++) {
    v4l2_segmentation->tree_probs[i] = s->segmentation_tree_probs[i];
  }
  for (i = 0; i < G_N_ELEMENTS (v4l2_segmentation->pred_probs); i++) {
    v4l2_segmentation->pred_probs[i] = s->segmentation_pred_prob[i];
  }
  for (i = 0; i < G_N_ELEMENTS (v4l2_segmentation->feature_enabled); i++) {
    /* see 3. Symbols (and abbreviated terms) for reference */

    /* *INDENT-OFF* */
    v4l2_segmentation->feature_enabled[i] =
        (s->feature_enabled[i][GST_VP9_SEG_LVL_ALT_Q]     ? V4L2_VP9_SEGMENT_FEATURE_ENABLED(V4L2_VP9_SEG_LVL_ALT_Q) : 0) |
        (s->feature_enabled[i][GST_VP9_SEG_LVL_ALT_L]     ? V4L2_VP9_SEGMENT_FEATURE_ENABLED(V4L2_VP9_SEG_LVL_ALT_L) : 0) |
        (s->feature_enabled[i][GST_VP9_SEG_LVL_REF_FRAME] ? V4L2_VP9_SEGMENT_FEATURE_ENABLED(V4L2_VP9_SEG_LVL_REF_FRAME) : 0) |
        (s->feature_enabled[i][GST_VP9_SEG_SEG_LVL_SKIP]  ? V4L2_VP9_SEGMENT_FEATURE_ENABLED(V4L2_VP9_SEG_LVL_SKIP) : 0);
    /* *INDENT-ON* */
  }

  memcpy (v4l2_segmentation->feature_data, s->feature_data,
      sizeof (v4l2_segmentation->feature_data));
}

static void
gst_v4l2_codec_vp9_dec_fill_prob_updates (GstV4l2CodecVp9Dec * self,
    const GstVp9FrameHeader * h)
{
  struct v4l2_ctrl_vp9_compressed_hdr *probs = &self->v4l2_delta_probs;

  G_STATIC_ASSERT (sizeof (probs->tx8) ==
      sizeof (h->delta_probabilities.tx_probs_8x8));
  G_STATIC_ASSERT (sizeof (probs->tx16) ==
      sizeof (h->delta_probabilities.tx_probs_16x16));
  G_STATIC_ASSERT (sizeof (probs->tx32) ==
      sizeof (h->delta_probabilities.tx_probs_32x32));
  G_STATIC_ASSERT (sizeof (probs->coef) ==
      sizeof (h->delta_probabilities.coef));
  G_STATIC_ASSERT (sizeof (probs->skip) ==
      sizeof (h->delta_probabilities.skip));
  G_STATIC_ASSERT (sizeof (probs->inter_mode) ==
      sizeof (h->delta_probabilities.inter_mode));
  G_STATIC_ASSERT (sizeof (probs->interp_filter) ==
      sizeof (h->delta_probabilities.interp_filter));
  G_STATIC_ASSERT (sizeof (probs->is_inter) ==
      sizeof (h->delta_probabilities.is_inter));
  G_STATIC_ASSERT (sizeof (probs->comp_mode) ==
      sizeof (h->delta_probabilities.comp_mode));
  G_STATIC_ASSERT (sizeof (probs->single_ref) ==
      sizeof (h->delta_probabilities.single_ref));
  G_STATIC_ASSERT (sizeof (probs->comp_ref) ==
      sizeof (h->delta_probabilities.comp_ref));
  G_STATIC_ASSERT (sizeof (probs->y_mode) ==
      sizeof (h->delta_probabilities.y_mode));
  G_STATIC_ASSERT (sizeof (probs->partition) ==
      sizeof (h->delta_probabilities.partition));
  G_STATIC_ASSERT (sizeof (probs->mv.joint) ==
      sizeof (h->delta_probabilities.mv.joint));
  G_STATIC_ASSERT (sizeof (probs->mv.sign) ==
      sizeof (h->delta_probabilities.mv.sign));
  G_STATIC_ASSERT (sizeof (probs->mv.classes) ==
      sizeof (h->delta_probabilities.mv.klass));
  G_STATIC_ASSERT (sizeof (probs->mv.class0_bit) ==
      sizeof (h->delta_probabilities.mv.class0_bit));
  G_STATIC_ASSERT (sizeof (probs->mv.bits) ==
      sizeof (h->delta_probabilities.mv.bits));
  G_STATIC_ASSERT (sizeof (probs->mv.class0_fr) ==
      sizeof (h->delta_probabilities.mv.class0_fr));
  G_STATIC_ASSERT (sizeof (probs->mv.fr) ==
      sizeof (h->delta_probabilities.mv.fr));
  G_STATIC_ASSERT (sizeof (probs->mv.class0_hp) ==
      sizeof (h->delta_probabilities.mv.class0_hp));
  G_STATIC_ASSERT (sizeof (probs->mv.hp) ==
      sizeof (h->delta_probabilities.mv.hp));

  memset (probs, 0, sizeof (*probs));

  probs->tx_mode = h->tx_mode;
  memcpy (probs->tx8, h->delta_probabilities.tx_probs_8x8, sizeof (probs->tx8));
  memcpy (probs->tx16, h->delta_probabilities.tx_probs_16x16,
      sizeof (probs->tx16));
  memcpy (probs->tx32, h->delta_probabilities.tx_probs_32x32,
      sizeof (probs->tx32));
  memcpy (probs->coef, h->delta_probabilities.coef, sizeof (probs->coef));
  memcpy (probs->skip, h->delta_probabilities.skip, sizeof (probs->skip));
  memcpy (probs->inter_mode, h->delta_probabilities.inter_mode,
      sizeof (probs->inter_mode));
  memcpy (probs->interp_filter,
      h->delta_probabilities.interp_filter, sizeof (probs->interp_filter));
  memcpy (probs->is_inter, h->delta_probabilities.is_inter,
      sizeof (probs->is_inter));
  memcpy (probs->comp_mode, h->delta_probabilities.comp_mode,
      sizeof (probs->comp_mode));
  memcpy (probs->single_ref, h->delta_probabilities.single_ref,
      sizeof (probs->single_ref));
  memcpy (probs->comp_ref, h->delta_probabilities.comp_ref,
      sizeof (probs->comp_ref));
  memcpy (probs->y_mode, h->delta_probabilities.y_mode, sizeof (probs->y_mode));
  memcpy (probs->partition, h->delta_probabilities.partition,
      sizeof (probs->partition));

  memcpy (probs->mv.joint, h->delta_probabilities.mv.joint,
      sizeof (h->delta_probabilities.mv.joint));
  memcpy (probs->mv.sign, h->delta_probabilities.mv.sign,
      sizeof (h->delta_probabilities.mv.sign));
  memcpy (probs->mv.classes, h->delta_probabilities.mv.klass,
      sizeof (h->delta_probabilities.mv.klass));
  memcpy (probs->mv.class0_bit,
      h->delta_probabilities.mv.class0_bit,
      sizeof (h->delta_probabilities.mv.class0_bit));
  memcpy (probs->mv.bits, h->delta_probabilities.mv.bits,
      sizeof (h->delta_probabilities.mv.bits));
  memcpy (probs->mv.class0_fr, h->delta_probabilities.mv.class0_fr,
      sizeof (h->delta_probabilities.mv.class0_fr));
  memcpy (probs->mv.fr, h->delta_probabilities.mv.fr,
      sizeof (h->delta_probabilities.mv.fr));
  memcpy (probs->mv.class0_hp, h->delta_probabilities.mv.class0_hp,
      sizeof (h->delta_probabilities.mv.class0_hp));
  memcpy (probs->mv.hp, h->delta_probabilities.mv.hp,
      sizeof (h->delta_probabilities.mv.hp));
}

static void
gst_v4l2_codecs_vp9_dec_fill_refs (GstV4l2CodecVp9Dec * self,
    const GstVp9FrameHeader * h, const GstVp9Dpb * reference_frames)
{
  GstVp9Picture *ref_pic;

  if (!reference_frames)
    return;

  if (reference_frames->pic_list[h->ref_frame_idx[0]]) {
    ref_pic = reference_frames->pic_list[h->ref_frame_idx[0]];
    self->v4l2_vp9_frame.last_frame_ts = GST_CODEC_PICTURE_TS_NS (ref_pic);
  }

  if (reference_frames->pic_list[h->ref_frame_idx[1]]) {
    ref_pic = reference_frames->pic_list[h->ref_frame_idx[1]];
    self->v4l2_vp9_frame.golden_frame_ts = GST_CODEC_PICTURE_TS_NS (ref_pic);
  }

  if (reference_frames->pic_list[h->ref_frame_idx[2]]) {
    ref_pic = reference_frames->pic_list[h->ref_frame_idx[2]];
    self->v4l2_vp9_frame.alt_frame_ts = GST_CODEC_PICTURE_TS_NS (ref_pic);
  }
}

static void
gst_v4l2_codec_vp9_dec_fill_dec_params (GstV4l2CodecVp9Dec * self,
    const GstVp9FrameHeader * h, const GstVp9Dpb * reference_frames)
{
  /* *INDENT-OFF* */
  self->v4l2_vp9_frame = (struct v4l2_ctrl_vp9_frame) {
    .flags =
        (h->frame_type == GST_VP9_KEY_FRAME ? V4L2_VP9_FRAME_FLAG_KEY_FRAME : 0) |
        (h->show_frame ? V4L2_VP9_FRAME_FLAG_SHOW_FRAME : 0) |
        (h->error_resilient_mode ? V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT : 0) |
        (h->intra_only ? V4L2_VP9_FRAME_FLAG_INTRA_ONLY : 0) |
        (h->allow_high_precision_mv ? V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV : 0) |
        (h->refresh_frame_context ? V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX : 0) |
        (h->frame_parallel_decoding_mode ? V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE : 0) |
        (self->subsampling_x ? V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING : 0) |
        (self->subsampling_y ? V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING : 0) |
        (self->color_range ? V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING : 0),

    .compressed_header_size = h->header_size_in_bytes,
    .uncompressed_header_size = h->frame_header_length_in_bytes,
    .profile = h->profile,
    .frame_context_idx = h->frame_context_idx,
    .bit_depth = self->bit_depth,
    .interpolation_filter = h->interpolation_filter,
    .tile_cols_log2 = h->tile_cols_log2,
    .tile_rows_log2 = h->tile_rows_log2,
    .reference_mode = h->reference_mode,
    .frame_width_minus_1 = h->width - 1,
    .frame_height_minus_1 = h->height - 1,
    .render_width_minus_1 = h->render_width ? h->render_width - 1 : h->width - 1,
    .render_height_minus_1 = h->render_height ? h->render_height - 1: h->height - 1,
    .ref_frame_sign_bias =
      (h->ref_frame_sign_bias[GST_VP9_REF_FRAME_LAST] ? V4L2_VP9_SIGN_BIAS_LAST : 0) |
      (h->ref_frame_sign_bias[GST_VP9_REF_FRAME_GOLDEN] ? V4L2_VP9_SIGN_BIAS_GOLDEN : 0) |
      (h->ref_frame_sign_bias[GST_VP9_REF_FRAME_ALTREF] ? V4L2_VP9_SIGN_BIAS_ALT : 0),

    .lf = (struct v4l2_vp9_loop_filter) {
      .flags =
        (h->loop_filter_params.loop_filter_delta_enabled ? V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED : 0) |
        (h->loop_filter_params.loop_filter_delta_update ? V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE : 0),
      .level = h->loop_filter_params.loop_filter_level,
      .sharpness = h->loop_filter_params.loop_filter_sharpness
    },

    .quant = (struct v4l2_vp9_quantization) {
      .base_q_idx = h->quantization_params.base_q_idx,      /* used for Y (luma) AC coefficients */
      .delta_q_y_dc = h->quantization_params.delta_q_y_dc,
      .delta_q_uv_dc = h->quantization_params.delta_q_uv_dc,
      .delta_q_uv_ac = h->quantization_params.delta_q_uv_ac,
    },

    .seg = (struct v4l2_vp9_segmentation) {
      .flags =
        (h->segmentation_params.segmentation_enabled ? V4L2_VP9_SEGMENTATION_FLAG_ENABLED : 0) |
        (h->segmentation_params.segmentation_update_map ? V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP : 0) |
        (h->segmentation_params.segmentation_temporal_update ? V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE : 0) |
        (h->segmentation_params.segmentation_update_data ? V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA : 0) |
        (h->segmentation_params.segmentation_abs_or_delta_update ? V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE : 0),
    }
  };
  /* *INDENT-ON* */

  switch (h->reset_frame_context) {
    case 0:
    case 1:
      self->v4l2_vp9_frame.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
    case 2:
      self->v4l2_vp9_frame.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_SPEC;
      break;
    case 3:
      self->v4l2_vp9_frame.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_ALL;
      break;
    default:
      break;
  }

  gst_v4l2_codecs_vp9_dec_fill_refs (self, h, reference_frames);
  gst_v4l2_codec_vp9_dec_fill_lf_params (self, &h->loop_filter_params);
  gst_v4l2_codec_vp9_dec_fill_seg_params (self, &h->segmentation_params);
}

static gboolean
gst_v4l2_codec_vp9_dec_open (GstVideoDecoder * decoder)
{
  GstVp9Decoder *vp9dec = GST_VP9_DECODER (decoder);
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);

  if (!gst_v4l2_decoder_open (self->decoder)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to open VP9 decoder"),
        ("gst_v4l2_decoder_open() failed: %s", g_strerror (errno)));
    return FALSE;
  }

  vp9dec->parse_compressed_headers =
      gst_v4l2_decoder_query_control_size (self->decoder,
      V4L2_CID_STATELESS_VP9_COMPRESSED_HDR, NULL);

  /* V4L2 does not support non-keyframe resolution change, this will ask the
   * base class to drop frame until the next keyframe as a workaround. */
  gst_vp9_decoder_set_non_keyframe_format_change_support (vp9dec, FALSE);

  return TRUE;
}

static gboolean
gst_v4l2_codec_vp9_dec_close (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  gst_v4l2_decoder_close (self->decoder);
  return TRUE;
}

static void
gst_v4l2_codec_vp9_dec_streamoff (GstV4l2CodecVp9Dec * self)
{
  if (self->streaming) {
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
    gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);
    self->streaming = FALSE;
  }
}

static void
gst_v4l2_codec_vp9_dec_reset_allocation (GstV4l2CodecVp9Dec * self)
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
gst_v4l2_codec_vp9_dec_stop (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);

  gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SINK);
  gst_v4l2_decoder_streamoff (self->decoder, GST_PAD_SRC);

  gst_v4l2_codec_vp9_dec_reset_allocation (self);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state = NULL;

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gboolean
gst_v4l2_codec_vp9_dec_negotiate (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  GstVp9Decoder *vp9dec = GST_VP9_DECODER (decoder);

  /* *INDENT-OFF* */
  struct v4l2_ext_control control[] = {
    {
      .id = V4L2_CID_STATELESS_VP9_FRAME,
      .ptr = &self->v4l2_vp9_frame,
      .size = sizeof (self->v4l2_vp9_frame),
    },
  };
  /* *INDENT-ON* */

  GstCaps *peer_caps, *filter, *caps;
  GstStaticCaps *static_filter;

  /* Ignore downstream renegotiation request. */
  if (self->streaming)
    goto done;

  GST_DEBUG_OBJECT (self, "Negotiate");

  gst_v4l2_codec_vp9_dec_reset_allocation (self);

  if (!gst_v4l2_decoder_set_sink_fmt (self->decoder, V4L2_PIX_FMT_VP9_FRAME,
          self->width, self->height, self->bit_depth)) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to configure VP9 decoder"),
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
      &self->vinfo_drm, self->width, self->height, vp9dec->input_state);

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
gst_v4l2_codec_vp9_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  GstCaps *caps = NULL;
  guint min = 0;
  guint num_bitstream;

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
      GST_PAD_SRC, GST_VP9_REF_FRAMES + min + 4);
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

static GstFlowReturn
gst_v4l2_codec_vp9_dec_new_sequence (GstVp9Decoder * decoder,
    const GstVp9FrameHeader * frame_hdr, gint max_dpb_size)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
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

  if (self->subsampling_x != frame_hdr->subsampling_x ||
      self->subsampling_y != frame_hdr->subsampling_y) {
    GST_DEBUG_OBJECT (self,
        "subsampling changed from x: %d, y: %d to x: %d, y: %d",
        self->subsampling_x, self->subsampling_y,
        frame_hdr->subsampling_x, frame_hdr->subsampling_y);
    self->subsampling_x = frame_hdr->subsampling_x;
    self->subsampling_y = frame_hdr->subsampling_y;
    negotiation_needed = TRUE;
  }

  if (frame_hdr->color_space != GST_VP9_CS_UNKNOWN &&
      frame_hdr->color_space != GST_VP9_CS_RESERVED_2 &&
      frame_hdr->color_space != self->color_space) {
    GST_DEBUG_OBJECT (self, "colorspace changed from %d to %d",
        self->color_space, frame_hdr->color_space);
    self->color_space = frame_hdr->color_space;
    negotiation_needed = TRUE;
  }

  if (frame_hdr->color_range != self->color_range) {
    GST_DEBUG_OBJECT (self, "color range changed from %d to %d",
        self->color_range, frame_hdr->color_range);
    self->color_range = frame_hdr->color_range;
    negotiation_needed = TRUE;
  }

  if (frame_hdr->profile != GST_VP9_PROFILE_UNDEFINED &&
      frame_hdr->profile != self->profile) {
    GST_DEBUG_OBJECT (self, "profile changed from %d to %d", self->profile,
        frame_hdr->profile);
    self->profile = frame_hdr->profile;
    negotiation_needed = TRUE;
  }

  if (frame_hdr->bit_depth != self->bit_depth) {
    GST_DEBUG_OBJECT (self, "bit-depth changed from %d to %d",
        self->bit_depth, frame_hdr->bit_depth);
    self->bit_depth = frame_hdr->bit_depth;
    negotiation_needed = TRUE;
  }

  gst_v4l2_codec_vp9_dec_fill_dec_params (self, frame_hdr, NULL);

  if (decoder->parse_compressed_headers)
    gst_v4l2_codec_vp9_dec_fill_prob_updates (self, frame_hdr);

  if (negotiation_needed) {
    gst_v4l2_codec_vp9_dec_streamoff (self);
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_ERROR;
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
gst_v4l2_codec_vp9_dec_start_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);

  /* FIXME base class should not call us if negotiation failed */
  if (!self->sink_allocator)
    return GST_FLOW_ERROR;

  /* Ensure we have a bitstream to write into */
  if (!self->bitstream) {
    self->bitstream = gst_v4l2_codec_allocator_alloc (self->sink_allocator);

    if (!self->bitstream) {
      GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
          ("Not enough memory to decode VP9 stream."), (NULL));
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

static void
gst_v4l2_codec_vp9_dec_reset_picture (GstV4l2CodecVp9Dec * self)
{
  if (self->bitstream) {
    if (self->bitstream_map.memory)
      gst_memory_unmap (self->bitstream, &self->bitstream_map);
    g_clear_pointer (&self->bitstream, gst_memory_unref);
    self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  }
}

static GstFlowReturn
gst_v4l2_codec_vp9_dec_decode_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture, GstVp9Dpb * dpb)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  guint8 *bitstream_data = self->bitstream_map.data;

  if (self->bitstream_map.maxsize < picture->size) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Not enough space to send picture bitstream."), (NULL));
    gst_v4l2_codec_vp9_dec_reset_picture (self);
    return GST_FLOW_ERROR;
  }

  gst_v4l2_codec_vp9_dec_fill_dec_params (self, &picture->frame_hdr, dpb);

  if (decoder->parse_compressed_headers)
    gst_v4l2_codec_vp9_dec_fill_prob_updates (self, &picture->frame_hdr);

  memcpy (bitstream_data, picture->data, picture->size);
  self->bitstream_map.size = picture->size;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_v4l2_codec_vp9_dec_end_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  GstVideoCodecFrame *frame;
  GstV4l2Request *request = NULL;
  GstFlowReturn flow_ret;
  gsize bytesused;
  guint num_controls = 1;

  /* *INDENT-OFF* */
  struct v4l2_ext_control decode_params_control[] = {
    {
      .id = V4L2_CID_STATELESS_VP9_FRAME,
      .ptr = &self->v4l2_vp9_frame,
      .size = sizeof(self->v4l2_vp9_frame),
    },
    {
      /* V4L2_CID_STATELESS_VP9_COMPRESSED_HDR */
    },
  };

  if (decoder->parse_compressed_headers) {
    decode_params_control[num_controls++] = (struct v4l2_ext_control) {
      .id = V4L2_CID_STATELESS_VP9_COMPRESSED_HDR,
      .ptr = &self->v4l2_delta_probs,
      .size = sizeof (self->v4l2_delta_probs),
    };
  }
  /* *INDENT-ON* */

  bytesused = self->bitstream_map.size;
  gst_memory_unmap (self->bitstream, &self->bitstream_map);
  self->bitstream_map = (GstMapInfo) GST_MAP_INFO_INIT;
  gst_memory_resize (self->bitstream, 0, bytesused);

  frame = gst_video_decoder_get_frame (GST_VIDEO_DECODER (self),
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));
  g_return_val_if_fail (frame, FALSE);

  flow_ret = gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (self->src_pool),
      &frame->output_buffer, NULL);
  if (flow_ret != GST_FLOW_OK) {
    if (flow_ret == GST_FLOW_FLUSHING)
      GST_DEBUG_OBJECT (self, "Frame decoding aborted, we are flushing.");
    else
      GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
          ("No more picture buffer available."), (NULL));
    goto fail;
  }

  request = gst_v4l2_decoder_alloc_request (self->decoder,
      GST_CODEC_PICTURE_FRAME_NUMBER (picture), self->bitstream,
      frame->output_buffer);

  gst_video_codec_frame_unref (frame);

  if (!request) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NO_SPACE_LEFT,
        ("Failed to allocate a media request object."), (NULL));
    goto fail;
  }

  if (!gst_v4l2_decoder_set_controls (self->decoder, request,
          decode_params_control, num_controls)) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver did not accept the bitstream parameters."), (NULL));
    goto fail;
  }

  if (!gst_v4l2_request_queue (request, 0)) {
    GST_ELEMENT_ERROR (decoder, RESOURCE, WRITE,
        ("Driver did not accept the decode request."), (NULL));
    goto fail;
  }

  gst_vp9_picture_set_user_data (picture, request,
      (GDestroyNotify) gst_v4l2_request_unref);
  gst_v4l2_codec_vp9_dec_reset_picture (self);
  return GST_FLOW_OK;

fail:
  if (request)
    gst_v4l2_request_unref (request);

  gst_v4l2_codec_vp9_dec_reset_picture (self);
  return GST_FLOW_ERROR;
}

static gboolean
gst_v4l2_codec_vp9_dec_copy_output_buffer (GstV4l2CodecVp9Dec * self,
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


static GstVp9Picture *
gst_v4l2_codec_vp9_dec_duplicate_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstVp9Picture *new_picture;

  GST_DEBUG_OBJECT (decoder, "Duplicate picture %u",
      GST_CODEC_PICTURE_FRAME_NUMBER (picture));

  new_picture = gst_vp9_picture_new ();
  new_picture->frame_hdr = picture->frame_hdr;
  GST_CODEC_PICTURE_FRAME_NUMBER (new_picture) = frame->system_frame_number;

  if (GST_MINI_OBJECT_FLAG_IS_SET (picture, FLAG_PICTURE_HOLDS_BUFFER)) {
    GstBuffer *output_buffer = gst_vp9_picture_get_user_data (picture);

    if (output_buffer) {
      frame->output_buffer = gst_buffer_ref (output_buffer);

      /* We need to also hold on the picture so it stays alive, but also to
       * ensure we can duplicate it too. */
      gst_vp9_picture_set_user_data (new_picture,
          gst_buffer_ref (frame->output_buffer),
          (GDestroyNotify) gst_buffer_unref);
    }

    /* Flag regardless if the buffer is null, so we don't start thinking it
     * should hold a request unconditionally. */
    GST_MINI_OBJECT_FLAG_SET (new_picture, FLAG_PICTURE_HOLDS_BUFFER);
  } else {
    GstV4l2Request *request = gst_vp9_picture_get_user_data (picture);
    gst_vp9_picture_set_user_data (new_picture, gst_v4l2_request_ref (request),
        (GDestroyNotify) gst_v4l2_request_unref);
    frame->output_buffer = gst_v4l2_request_dup_pic_buf (request);
  }

  return new_picture;
}

static GstFlowReturn
gst_v4l2_codec_vp9_dec_output_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstV4l2Request *request = NULL;
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

  if (!GST_MINI_OBJECT_FLAG_IS_SET (picture, FLAG_PICTURE_HOLDS_BUFFER))
    request = gst_vp9_picture_get_user_data (picture);

  if (request) {
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
    gst_vp9_picture_set_user_data (picture,
        gst_buffer_ref (frame->output_buffer),
        (GDestroyNotify) gst_buffer_unref);

    GST_MINI_OBJECT_FLAG_SET (picture, FLAG_PICTURE_HOLDS_BUFFER);
  }

  /* This may happen if we duplicate a picture witch failed to decode */
  if (!frame->output_buffer) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE,
        ("Failed to decode frame %u", codec_picture->system_frame_number),
        (NULL));
    goto error;
  }

  if (self->copy_frames)
    gst_v4l2_codec_vp9_dec_copy_output_buffer (self, frame);

  gst_vp9_picture_unref (picture);

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_drop_frame (vdec, frame);
  gst_vp9_picture_unref (picture);

  return GST_FLOW_ERROR;
}

static void
gst_v4l2_codec_vp9_dec_set_flushing (GstV4l2CodecVp9Dec * self,
    gboolean flushing)
{
  if (self->sink_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->sink_allocator, flushing);
  if (self->src_allocator)
    gst_v4l2_codec_allocator_set_flushing (self->src_allocator, flushing);
}

static gboolean
gst_v4l2_codec_vp9_dec_flush (GstVideoDecoder * decoder)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder state.");

  gst_v4l2_decoder_flush (self->decoder);
  gst_v4l2_codec_vp9_dec_set_flushing (self, FALSE);

  return GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
}

static gboolean
gst_v4l2_codec_vp9_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_codec_vp9_dec_set_flushing (self, TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstStateChangeReturn
gst_v4l2_codec_vp9_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    gst_v4l2_codec_vp9_dec_set_flushing (self, TRUE);

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_vp9_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_set_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_vp9_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (object);
  GObject *dec = G_OBJECT (self->decoder);

  switch (prop_id) {
    default:
      gst_v4l2_decoder_get_property (dec, prop_id - PROP_LAST, value, pspec);
      break;
  }
}

static void
gst_v4l2_codec_vp9_dec_init (GstV4l2CodecVp9Dec * self)
{
}

static void
gst_v4l2_codec_vp9_dec_subinit (GstV4l2CodecVp9Dec * self,
    GstV4l2CodecVp9DecClass * klass)
{
  self->decoder = gst_v4l2_decoder_new (klass->device);
  gst_video_info_init (&self->vinfo);
  gst_video_info_dma_drm_init (&self->vinfo_drm);
}

static void
gst_v4l2_codec_vp9_dec_dispose (GObject * object)
{
  GstV4l2CodecVp9Dec *self = GST_V4L2_CODEC_VP9_DEC (object);

  g_clear_object (&self->decoder);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_codec_vp9_dec_class_init (GstV4l2CodecVp9DecClass * klass)
{
}

static void
gst_v4l2_codec_vp9_dec_subclass_init (GstV4l2CodecVp9DecClass * klass,
    GstV4l2CodecDevice * device)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp9DecoderClass *vp9decoder_class = GST_VP9_DECODER_CLASS (klass);

  gobject_class->set_property = gst_v4l2_codec_vp9_dec_set_property;
  gobject_class->get_property = gst_v4l2_codec_vp9_dec_get_property;
  gobject_class->dispose = gst_v4l2_codec_vp9_dec_dispose;

  gst_element_class_set_static_metadata (element_class,
      "V4L2 Stateless VP9 Video Decoder",
      "Codec/Decoder/Video/Hardware",
      "A V4L2 based VP9 video decoder",
      "Daniel Almeida <daniel.almeida@collabora.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_change_state);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_decide_allocation);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_flush);
  decoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_sink_event);

  vp9decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_new_sequence);
  vp9decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_start_picture);
  vp9decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_decode_picture);
  vp9decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_end_picture);
  vp9decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_output_picture);
  vp9decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_duplicate_picture);
  vp9decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_vp9_dec_get_preferred_output_delay);

  klass->device = device;
  gst_v4l2_decoder_install_properties (gobject_class, PROP_LAST, device);
}

static void gst_v4l2_codec_vp9_alpha_decode_bin_subclass_init
    (GstV4l2CodecAlphaDecodeBinClass * klass, gchar * decoder_name)
{
  GstV4l2CodecAlphaDecodeBinClass *adbin_class =
      (GstV4l2CodecAlphaDecodeBinClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  adbin_class->decoder_name = decoder_name;
  gst_element_class_add_static_pad_template (element_class, &alpha_template);

  gst_element_class_set_static_metadata (element_class,
      "VP9 Alpha Decoder", "Codec/Decoder/Video",
      "Wrapper bin to decode VP9 with alpha stream.",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");
}

void
gst_v4l2_codec_vp9_dec_register (GstPlugin * plugin, GstV4l2Decoder * decoder,
    GstV4l2CodecDevice * device, guint rank)
{
  gchar *element_name;
  GstCaps *src_caps, *alpha_caps;

  GST_DEBUG_CATEGORY_INIT (v4l2_vp9dec_debug, "v4l2codecs-vp9dec", 0,
      "V4L2 stateless VP9 decoder");

  if (!gst_v4l2_decoder_set_sink_fmt (decoder, V4L2_PIX_FMT_VP9_FRAME,
          320, 240, 8))
    return;
  src_caps = gst_v4l2_decoder_enum_src_formats (decoder, &static_src_caps);

  if (gst_caps_is_empty (src_caps)) {
    GST_WARNING ("Not registering VP9 decoder since it produces no "
        "supported format");
    goto done;
  }

  gst_v4l2_decoder_register (plugin, GST_TYPE_V4L2_CODEC_VP9_DEC,
      (GClassInitFunc) gst_v4l2_codec_vp9_dec_subclass_init,
      gst_mini_object_ref (GST_MINI_OBJECT (device)),
      (GInstanceInitFunc) gst_v4l2_codec_vp9_dec_subinit,
      "v4l2sl%svp9dec", device, rank, &element_name);

  if (!element_name)
    goto done;

  alpha_caps = gst_caps_from_string ("video/x-raw,format={I420, NV12}");

  if (gst_caps_can_intersect (src_caps, alpha_caps))
    gst_v4l2_codec_alpha_decode_bin_register (plugin,
        (GClassInitFunc) gst_v4l2_codec_vp9_alpha_decode_bin_subclass_init,
        element_name, "v4l2slvp9%salphadecodebin", device, rank);

  gst_caps_unref (alpha_caps);

done:
  gst_caps_unref (src_caps);
}
