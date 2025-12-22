/* GStreamer
 *  Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vaav1dec
 * @title: vaav1dec
 * @short_description: A VA-API based AV1 video decoder
 *
 * vaav1dec decodes AV1 bitstreams to VA surfaces using the
 * installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The decoding surfaces can be mapped onto main memory as video
 * frames.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=sample.av1 ! ivfparse ! av1parse ! vaav1dec ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/va/gstva.h>
#include <gst/va/vasurfaceimage.h>

#include "gstvaav1dec.h"
#include "gstvabasedec.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_av1dec_debug);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_va_av1dec_debug
#else
#define GST_CAT_DEFAULT NULL
#endif

#define GST_VA_AV1_DEC(obj)           ((GstVaAV1Dec *) obj)
#define GST_VA_AV1_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaAV1DecClass))
#define GST_VA_AV1_DEC_CLASS(klass)   ((GstVaAV1DecClass *) klass)

typedef struct _GstVaAV1Dec GstVaAV1Dec;
typedef struct _GstVaAV1DecClass GstVaAV1DecClass;

struct _GstVaAV1DecClass
{
  GstVaBaseDecClass parent_class;
};

struct _GstVaAV1Dec
{
  GstVaBaseDec parent;

  GstAV1SequenceHeaderOBU seq;
  GstVideoFormat preferred_format;
  /* Used for layers not output. */
  GstBufferPool *internal_pool;
};

static GstElementClass *parent_class = NULL;

/* *INDENT-OFF* */
static const gchar *src_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12, P010_10LE }");
/* *INDENT-ON* */

static const gchar *sink_caps_str = "video/x-av1";

static gboolean
gst_va_av1_dec_negotiate (GstVideoDecoder * decoder)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  /* Ignore downstream renegotiation request. */
  if (!base->need_negotiation)
    return TRUE;

  base->need_negotiation = FALSE;

  /* Do not re-create the context if only the frame size changes */
  if (!gst_va_decoder_config_is_equal (base->decoder, base->profile,
          base->rt_format, base->width, base->height)) {
    if (gst_va_decoder_is_open (base->decoder)
        && !gst_va_decoder_close (base->decoder))
      return FALSE;

    if (!gst_va_decoder_open (base->decoder, base->profile, base->rt_format))
      return FALSE;

    if (!gst_va_decoder_set_frame_size (base->decoder, base->width,
            base->height))
      return FALSE;
  }

  if (!gst_va_base_dec_set_output_state (base))
    return FALSE;

  if (self->preferred_format != GST_VIDEO_FORMAT_UNKNOWN &&
      self->preferred_format !=
      GST_VIDEO_INFO_FORMAT (&base->output_state->info)) {
    GST_WARNING_OBJECT (self, "The preferred_format is different from"
        " the last result");
    return FALSE;
  }
  self->preferred_format = GST_VIDEO_INFO_FORMAT (&base->output_state->info);

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static GstCaps *
_complete_sink_caps (GstCaps * sinkcaps)
{
  GstCaps *caps = gst_caps_copy (sinkcaps);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "frame");
  gst_caps_set_value (caps, "alignment", &val);
  g_value_unset (&val);

  return caps;
}

static VAProfile
_get_profile (GstVaAV1Dec * self, const GstAV1SequenceHeaderOBU * seq_hdr)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  VAProfile profile = VAProfileNone;

  switch (seq_hdr->seq_profile) {
    case GST_AV1_PROFILE_0:
      profile = VAProfileAV1Profile0;
      break;
    case GST_AV1_PROFILE_1:
      profile = VAProfileAV1Profile1;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unsupported av1 profile value %d",
          seq_hdr->seq_profile);
      return VAProfileNone;
  }

  if (!gst_va_decoder_has_profile (base->decoder, profile)) {
    GST_ERROR_OBJECT (self, "Profile %s is not supported by HW",
        gst_va_profile_name (profile));
    return VAProfileNone;
  }

  return profile;
}

static guint
_get_rtformat (GstVaAV1Dec * self, VAProfile profile,
    const GstAV1SequenceHeaderOBU * seq_header)
{
  /* 6.4.1:
     seq_profile  Bit depth  Monochrome support  Chroma subsampling
     0            8 or 10    Yes                 YUV 4:2:0
     1            8 or 10    No                  YUV 4:4:4
     2            8 or 10    Yes                 YUV 4:2:2
     2            12         Yes                 YUV 4:2:0,YUV 4:2:2,YUV 4:4:4
   */

  /* TODO: consider Monochrome case. Just return 4:2:0 for Monochrome now. */
  switch (profile) {
    case VAProfileAV1Profile0:
      if (seq_header->bit_depth == 8) {
        return VA_RT_FORMAT_YUV420;
      } else if (seq_header->bit_depth == 10) {
        return VA_RT_FORMAT_YUV420_10;
      }
      break;
    case VAProfileAV1Profile1:
      if (seq_header->bit_depth == 8) {
        return VA_RT_FORMAT_YUV444;
      } else if (seq_header->bit_depth == 10) {
        return VA_RT_FORMAT_YUV444_10;
      }
      break;
    default:
      break;
  }

  GST_ERROR_OBJECT (self, "Fail to find rtformat for profile:%s, bit_depth:%d",
      gst_va_profile_name (profile), seq_header->bit_depth);
  return 0;
}

static GstCaps *
gst_va_av1_dec_getcaps (GstVideoDecoder * decoder, GstCaps * filter)
{
  GstCaps *sinkcaps, *caps = NULL, *tmp;
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  if (base->decoder)
    caps = gst_va_decoder_get_sinkpad_caps (base->decoder);

  if (caps) {
    sinkcaps = _complete_sink_caps (caps);
    gst_caps_unref (caps);
    if (filter) {
      tmp = gst_caps_intersect_full (filter, sinkcaps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (sinkcaps);
      caps = tmp;
    } else {
      caps = sinkcaps;
    }
    GST_LOG_OBJECT (base, "Returning caps %" GST_PTR_FORMAT, caps);
  } else if (!caps) {
    caps = gst_video_decoder_proxy_getcaps (decoder, NULL, filter);
  }

  return caps;
}

static void
_clear_internal_pool (GstVaAV1Dec * self)
{
  if (self->internal_pool)
    gst_buffer_pool_set_active (self->internal_pool, FALSE);

  gst_clear_object (&self->internal_pool);
}

static GstBufferPool *
_create_internal_pool (GstVaAV1Dec * self, gint width, gint height)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  GstVideoInfo info;
  GArray *surface_formats;
  GstAllocator *allocator;
  GstCaps *caps = NULL;
  GstBufferPool *pool;
  guint usage_hint;
  GstAllocationParams params = { 0, };

  gst_allocation_params_init (&params);

  /* We may come here before the negotiation, make sure all pools
     use the same video format. */
  if (self->preferred_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GstVideoFormat format;

    gst_va_base_dec_get_preferred_format_and_caps_features (base,
        &format, NULL, NULL);
    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      GST_WARNING_OBJECT (self, "Failed to get format for internal pool");
      return NULL;
    }

    self->preferred_format = format;
  }

  gst_video_info_set_format (&info, self->preferred_format, width, height);

  caps = gst_video_info_to_caps (&info);
  if (!caps) {
    GST_WARNING_OBJECT (self, "Failed to create caps for internal pool");
    return NULL;
  }

  gst_caps_set_features_simple (caps,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA));

  surface_formats = gst_va_decoder_get_surface_formats (base->decoder);
  allocator = gst_va_allocator_new (base->display, surface_formats);

  usage_hint = va_get_surface_usage_hint (base->display,
      VAEntrypointVLD, GST_PAD_SRC, FALSE);

  pool = gst_va_pool_new_with_config (caps, 1, 0, usage_hint,
      GST_VA_FEATURE_AUTO, allocator, &params);

  gst_clear_caps (&caps);
  gst_object_unref (allocator);

  if (!pool) {
    GST_WARNING_OBJECT (self, "Failed to create internal pool");
    return NULL;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_WARNING_OBJECT (self, "Failed to activate internal pool");
    gst_object_unref (pool);
    return NULL;
  }

  return pool;
}

static GstFlowReturn
gst_va_av1_dec_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVideoInfo *info = &base->output_info;
  VAProfile profile;
  guint rt_format;
  gint width, height;

  GST_LOG_OBJECT (self, "new sequence");

  profile = _get_profile (self, seq_hdr);
  if (profile == VAProfileNone)
    return GST_FLOW_NOT_NEGOTIATED;

  rt_format = _get_rtformat (self, profile, seq_hdr);
  if (!rt_format)
    return GST_FLOW_NOT_NEGOTIATED;

  self->seq = *seq_hdr;

  width = seq_hdr->max_frame_width_minus_1 + 1;
  height = seq_hdr->max_frame_height_minus_1 + 1;

  if (!gst_va_decoder_config_is_equal (base->decoder, profile,
          rt_format, width, height)) {
    _clear_internal_pool (self);
    self->preferred_format = GST_VIDEO_FORMAT_UNKNOWN;

    base->profile = profile;
    base->rt_format = rt_format;
    GST_VIDEO_INFO_WIDTH (info) = base->width = width;
    GST_VIDEO_INFO_HEIGHT (info) = base->height = height;
    base->need_negotiation = TRUE;
    base->min_buffers = 7 + 4;  /* dpb size + scratch surfaces */
    base->need_valign = FALSE;
  }

  g_clear_pointer (&base->input_state, gst_video_codec_state_unref);
  base->input_state = gst_video_codec_state_ref (decoder->input_state);

  return GST_FLOW_OK;
}

static inline GstFlowReturn
_acquire_internal_buffer (GstVaAV1Dec * self, GstVideoCodecFrame * frame)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (self);
  GstFlowReturn ret;

  if (!self->internal_pool) {
    self->internal_pool =
        _create_internal_pool (self, base->width, base->height);
    if (!self->internal_pool)
      return GST_FLOW_ERROR;
  }

  if (base->need_negotiation) {
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self)))
      return GST_FLOW_NOT_NEGOTIATED;
  }

  ret = gst_buffer_pool_acquire_buffer (self->internal_pool,
      &frame->output_buffer, NULL);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self,
        "Failed to allocated output buffer from internal pool, return %s",
        gst_flow_get_name (ret));
  }

  return ret;
}

static GstFlowReturn
gst_va_av1_dec_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstAV1FrameHeaderOBU *frame_hdr = &picture->frame_hdr;
  GstVaDecodePicture *pic;
  GstVideoInfo *info = &base->output_info;
  GstFlowReturn ret;

  /* Only output the highest spatial layer. For non output pictures,
     we just use internal pool, then no negotiation needed. */
  if (picture->spatial_id < decoder->highest_spatial_layer) {
    ret = _acquire_internal_buffer (self, frame);
    if (ret != GST_FLOW_OK)
      return ret;
  } else {
    if (frame_hdr->upscaled_width != GST_VIDEO_INFO_WIDTH (info)
        || frame_hdr->frame_height != GST_VIDEO_INFO_HEIGHT (info)) {
      GST_VIDEO_INFO_WIDTH (info) = frame_hdr->upscaled_width;
      GST_VIDEO_INFO_HEIGHT (info) = frame_hdr->frame_height;

      if (GST_VIDEO_INFO_WIDTH (info) < base->width
          || GST_VIDEO_INFO_HEIGHT (info) < base->height) {
        base->need_valign = TRUE;
        /* *INDENT-OFF* */
        base->valign = (GstVideoAlignment) {
          .padding_bottom = base->height - GST_VIDEO_INFO_HEIGHT (info),
          .padding_right = base->width - GST_VIDEO_INFO_WIDTH (info),
        };
        /* *INDENT-ON* */
      }

      base->need_negotiation = TRUE;
    }

    ret = gst_va_base_dec_prepare_output_frame (base, frame);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to allocated output buffer, return %s",
          gst_flow_get_name (ret));
      return ret;
    }
  }

  if (picture->apply_grain) {
    if (!gst_va_buffer_create_aux_surface (frame->output_buffer)) {
      GST_WARNING_OBJECT (self,
          "Failed to allocated aux surface for buffer %p",
          frame->output_buffer);
      return GST_FLOW_ERROR;
    }
  }

  pic = gst_va_decode_picture_new (base->decoder, frame->output_buffer);

  gst_av1_picture_set_user_data (picture, pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  if (picture->apply_grain) {
    GST_LOG_OBJECT (self, "New va decode picture %p - %#x(aux: %#x)", pic,
        gst_va_decode_picture_get_surface (pic),
        gst_va_decode_picture_get_aux_surface (pic));
  } else {
    GST_LOG_OBJECT (self, "New va decode picture %p - %#x", pic,
        gst_va_decode_picture_get_surface (pic));
  }

  return GST_FLOW_OK;
}

static GstAV1Picture *
gst_va_av1_dec_duplicate_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *pic;
  GstVaDecodePicture *new_pic;
  GstAV1Picture *new_picture;

  pic = gst_av1_picture_get_user_data (picture);
  if (!pic) {
    GST_ERROR_OBJECT (self, "Parent picture does not have a va picture");
    return NULL;
  }

  new_picture = gst_av1_picture_new ();
  g_assert (pic->gstbuffer);
  new_pic = gst_va_decode_picture_new (base->decoder, pic->gstbuffer);

  GST_LOG_OBJECT (self, "Duplicate output with buffer %" GST_PTR_FORMAT
      " (surface %#x)", pic, gst_va_decode_picture_get_surface (pic));

  gst_av1_picture_set_user_data (new_picture, new_pic,
      (GDestroyNotify) gst_va_decode_picture_free);

  return new_picture;
}

static void
_setup_segment_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header)
{
  guint i, j;
  uint8_t feature_mask;

  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++)
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++)
      pic_param->seg_info.feature_data[i][j] =
          frame_header->segmentation_params.feature_data[i][j];

  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    feature_mask = 0;
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      if (frame_header->segmentation_params.feature_enabled[i][j])
        feature_mask |= 1 << j;
    }
    pic_param->seg_info.feature_mask[i] = feature_mask;
  }
}

static void
_setup_film_grain_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header)
{
  guint i;

  if (!frame_header->film_grain_params.apply_grain)
    return;

  pic_param->film_grain_info.num_y_points =
      frame_header->film_grain_params.num_y_points;
  for (i = 0; i < frame_header->film_grain_params.num_y_points; i++) {
    pic_param->film_grain_info.point_y_value[i] =
        frame_header->film_grain_params.point_y_value[i];
    pic_param->film_grain_info.point_y_scaling[i] =
        frame_header->film_grain_params.point_y_scaling[i];
  }

  pic_param->film_grain_info.num_cb_points =
      frame_header->film_grain_params.num_cb_points;
  for (i = 0; i < frame_header->film_grain_params.num_cb_points; i++) {
    pic_param->film_grain_info.point_cb_value[i] =
        frame_header->film_grain_params.point_cb_value[i];
    pic_param->film_grain_info.point_cb_scaling[i] =
        frame_header->film_grain_params.point_cb_scaling[i];
  }

  pic_param->film_grain_info.num_cr_points =
      frame_header->film_grain_params.num_cr_points;
  for (i = 0; i < frame_header->film_grain_params.num_cr_points; i++) {
    pic_param->film_grain_info.point_cr_value[i] =
        frame_header->film_grain_params.point_cr_value[i];
    pic_param->film_grain_info.point_cr_scaling[i] =
        frame_header->film_grain_params.point_cr_scaling[i];
  }


  if (pic_param->film_grain_info.num_y_points) {
    for (i = 0; i < 24; i++) {
      pic_param->film_grain_info.ar_coeffs_y[i] =
          frame_header->film_grain_params.ar_coeffs_y_plus_128[i] - 128;
    }
  }
  if (frame_header->film_grain_params.chroma_scaling_from_luma
      || pic_param->film_grain_info.num_cb_points) {
    for (i = 0; i < GST_AV1_MAX_NUM_POS_LUMA; i++) {
      pic_param->film_grain_info.ar_coeffs_cb[i] =
          frame_header->film_grain_params.ar_coeffs_cb_plus_128[i] - 128;
    }
  }
  if (frame_header->film_grain_params.chroma_scaling_from_luma
      || pic_param->film_grain_info.num_cr_points) {
    for (i = 0; i < GST_AV1_MAX_NUM_POS_LUMA; i++) {
      pic_param->film_grain_info.ar_coeffs_cr[i] =
          frame_header->film_grain_params.ar_coeffs_cr_plus_128[i] - 128;
    }
  }
}

static void
_setup_loop_filter_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header)
{
  guint i;

  pic_param->filter_level[0] =
      frame_header->loop_filter_params.loop_filter_level[0];
  pic_param->filter_level[1] =
      frame_header->loop_filter_params.loop_filter_level[1];
  pic_param->filter_level_u =
      frame_header->loop_filter_params.loop_filter_level[2];
  pic_param->filter_level_v =
      frame_header->loop_filter_params.loop_filter_level[3];

  for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++)
    pic_param->ref_deltas[i] =
        frame_header->loop_filter_params.loop_filter_ref_deltas[i];
  for (i = 0; i < 2; i++)
    pic_param->mode_deltas[i] =
        frame_header->loop_filter_params.loop_filter_mode_deltas[i];
}

static void
_setup_quantization_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header)
{
  pic_param->qmatrix_fields.bits.using_qmatrix =
      frame_header->quantization_params.using_qmatrix;
  if (frame_header->quantization_params.using_qmatrix) {
    pic_param->qmatrix_fields.bits.qm_y =
        frame_header->quantization_params.qm_y;
    pic_param->qmatrix_fields.bits.qm_u =
        frame_header->quantization_params.qm_u;
    pic_param->qmatrix_fields.bits.qm_v =
        frame_header->quantization_params.qm_v;
  } else {
    pic_param->qmatrix_fields.bits.qm_y = 0;
    pic_param->qmatrix_fields.bits.qm_u = 0;
    pic_param->qmatrix_fields.bits.qm_v = 0;
  }
}

static void
_setup_cdef_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header, guint8 num_planes)
{
  guint8 sec_strength;
  guint i;

  pic_param->cdef_damping_minus_3 = frame_header->cdef_params.cdef_damping - 3;
  pic_param->cdef_bits = frame_header->cdef_params.cdef_bits;
  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    sec_strength = frame_header->cdef_params.cdef_y_sec_strength[i];
    g_assert (sec_strength <= 4);
    /* may need to minus 1 in order to merge with primary value. */
    if (sec_strength == 4)
      sec_strength--;

    pic_param->cdef_y_strengths[i] =
        ((frame_header->cdef_params.cdef_y_pri_strength[i] & 0xf) << 2) |
        (sec_strength & 0x03);
  }
  if (num_planes > 1) {
    for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
      sec_strength = frame_header->cdef_params.cdef_uv_sec_strength[i];
      g_assert (sec_strength <= 4);
      /* may need to minus 1 in order to merge with primary value. */
      if (sec_strength == 4)
        sec_strength--;

      pic_param->cdef_uv_strengths[i] =
          ((frame_header->cdef_params.cdef_uv_pri_strength[i] & 0xf) << 2) |
          (sec_strength & 0x03);
    }
  } else {
    for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
      pic_param->cdef_uv_strengths[i] = 0;
    }
  }
}

static void
_setup_global_motion_info (VADecPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_header)
{
  guint i, j;

  for (i = 0; i < 7; i++) {
    /* assuming VAAV1TransformationType and GstAV1WarpModelType are
     * equivalent */
    pic_param->wm[i].wmtype = (VAAV1TransformationType)
        frame_header->global_motion_params.gm_type[GST_AV1_REF_LAST_FRAME + i];

    for (j = 0; j < 6; j++)
      pic_param->wm[i].wmmat[j] =
          frame_header->global_motion_params.gm_params
          [GST_AV1_REF_LAST_FRAME + i][j];

    pic_param->wm[i].wmmat[6] = 0;
    pic_param->wm[i].wmmat[7] = 0;

    pic_param->wm[i].invalid =
        frame_header->global_motion_params.invalid[GST_AV1_REF_LAST_FRAME + i];
  }
}

static GstFlowReturn
gst_va_av1_dec_start_picture (GstAV1Decoder * decoder, GstAV1Picture * picture,
    GstAV1Dpb * dpb)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstAV1FrameHeaderOBU *frame_header = &picture->frame_hdr;
  GstAV1SequenceHeaderOBU *seq_header = &self->seq;
  VADecPictureParameterBufferAV1 pic_param = { 0, };
  GstVaDecodePicture *va_pic;
  guint i;

  va_pic = gst_av1_picture_get_user_data (picture);
  g_assert (va_pic);

  /* *INDENT-OFF* */
  pic_param = (VADecPictureParameterBufferAV1){
    .profile = seq_header->seq_profile,
    .order_hint_bits_minus_1 = seq_header->order_hint_bits_minus_1,
    .matrix_coefficients = seq_header->color_config.matrix_coefficients,
    .seq_info_fields.fields = {
      .still_picture = seq_header->still_picture,
      .use_128x128_superblock = seq_header->use_128x128_superblock,
      .enable_filter_intra = seq_header->enable_filter_intra,
      .enable_intra_edge_filter = seq_header->enable_intra_edge_filter,
      .enable_interintra_compound = seq_header->enable_interintra_compound,
      .enable_masked_compound = seq_header->enable_masked_compound,
      .enable_dual_filter = seq_header->enable_dual_filter,
      .enable_order_hint = seq_header->enable_order_hint,
      .enable_jnt_comp = seq_header->enable_jnt_comp,
      .enable_cdef = seq_header->enable_cdef,
      .mono_chrome = seq_header->color_config.mono_chrome,
      .color_range = seq_header->color_config.color_range,
      .subsampling_x = seq_header->color_config.subsampling_x,
      .subsampling_y = seq_header->color_config.subsampling_y,
      .film_grain_params_present = seq_header->film_grain_params_present,
    },
    .anchor_frames_num = 0,
    .anchor_frames_list = NULL,
    .frame_width_minus1 = frame_header->upscaled_width - 1,
    .frame_height_minus1 = frame_header->frame_height - 1,
    .output_frame_width_in_tiles_minus_1 = 0,
    .output_frame_height_in_tiles_minus_1 = 0,
    .order_hint = frame_header->order_hint,
    /* Segmentation */
    .seg_info.segment_info_fields.bits = {
      .enabled = frame_header->segmentation_params.segmentation_enabled,
      .update_map = frame_header->segmentation_params.segmentation_update_map,
      .temporal_update =
        frame_header->segmentation_params.segmentation_temporal_update,
      .update_data =
        frame_header->segmentation_params.segmentation_update_data,
    },
    /* FilmGrain */
    .film_grain_info = {
      .film_grain_info_fields.bits = {
        .apply_grain = frame_header->film_grain_params.apply_grain,
        .chroma_scaling_from_luma =
          frame_header->film_grain_params.chroma_scaling_from_luma,
        .grain_scaling_minus_8 =
          frame_header->film_grain_params.grain_scaling_minus_8,
        .ar_coeff_lag = frame_header->film_grain_params.ar_coeff_lag,
        .ar_coeff_shift_minus_6 =
          frame_header->film_grain_params.ar_coeff_shift_minus_6,
        .grain_scale_shift = frame_header->film_grain_params.grain_scale_shift,
        .overlap_flag = frame_header->film_grain_params.overlap_flag,
        .clip_to_restricted_range =
          frame_header->film_grain_params.clip_to_restricted_range,
      },
      .grain_seed = frame_header->film_grain_params.grain_seed,
      .cb_mult = frame_header->film_grain_params.cb_mult,
      .cb_luma_mult = frame_header->film_grain_params.cb_luma_mult,
      .cb_offset = frame_header->film_grain_params.cb_offset,
      .cr_mult = frame_header->film_grain_params.cr_mult,
      .cr_luma_mult = frame_header->film_grain_params.cr_luma_mult,
      .cr_offset = frame_header->film_grain_params.cr_offset,
    },
    .tile_cols = frame_header->tile_info.tile_cols,
    .tile_rows = frame_header->tile_info.tile_rows,
    .context_update_tile_id = frame_header->tile_info.context_update_tile_id,
    .pic_info_fields.bits = {
      .frame_type = frame_header->frame_type,
      .show_frame = frame_header->show_frame,
      .showable_frame = frame_header->showable_frame,
      .error_resilient_mode = frame_header->error_resilient_mode,
      .disable_cdf_update = frame_header->disable_cdf_update,
      .allow_screen_content_tools = frame_header->allow_screen_content_tools,
      .force_integer_mv = frame_header->force_integer_mv,
      .allow_intrabc = frame_header->allow_intrabc,
      .use_superres = frame_header->use_superres,
      .allow_high_precision_mv = frame_header->allow_high_precision_mv,
      .is_motion_mode_switchable = frame_header->is_motion_mode_switchable,
      .use_ref_frame_mvs = frame_header->use_ref_frame_mvs,
      .disable_frame_end_update_cdf =
        frame_header->disable_frame_end_update_cdf,
      .uniform_tile_spacing_flag =
        frame_header->tile_info.uniform_tile_spacing_flag,
      .allow_warped_motion = frame_header->allow_warped_motion,
    },
    .superres_scale_denominator = frame_header->superres_denom,
    .interp_filter = frame_header->interpolation_filter,
    /* loop filter */
    .loop_filter_info_fields.bits = {
      .sharpness_level =
        frame_header->loop_filter_params.loop_filter_sharpness,
      .mode_ref_delta_enabled =
        frame_header->loop_filter_params.loop_filter_delta_enabled,
      .mode_ref_delta_update =
        frame_header->loop_filter_params.loop_filter_delta_update,
    },
    .mode_control_fields.bits = {
      .delta_lf_present_flag =
        frame_header->loop_filter_params.delta_lf_present,
      .log2_delta_lf_res = frame_header->loop_filter_params.delta_lf_res,
      .delta_lf_multi = frame_header->loop_filter_params.delta_lf_multi,
      .delta_q_present_flag =
        frame_header->quantization_params.delta_q_present,
      .log2_delta_q_res = frame_header->quantization_params.delta_q_res,
      .tx_mode = frame_header->tx_mode,
      .reference_select = frame_header->reference_select,
      .reduced_tx_set_used = frame_header->reduced_tx_set,
      .skip_mode_present = frame_header->skip_mode_present,
    },
    /* quantization */
    .base_qindex = frame_header->quantization_params.base_q_idx,
    .y_dc_delta_q = frame_header->quantization_params.delta_q_y_dc,
    .u_dc_delta_q = frame_header->quantization_params.delta_q_u_dc,
    .u_ac_delta_q = frame_header->quantization_params.delta_q_u_ac,
    .v_dc_delta_q = frame_header->quantization_params.delta_q_v_dc,
    .v_ac_delta_q = frame_header->quantization_params.delta_q_v_ac,
    /* loop restoration */
    .loop_restoration_fields.bits = {
      .yframe_restoration_type =
        frame_header->loop_restoration_params.frame_restoration_type[0],
      .cbframe_restoration_type =
        frame_header->loop_restoration_params.frame_restoration_type[1],
      .crframe_restoration_type =
        frame_header->loop_restoration_params.frame_restoration_type[2],
      .lr_unit_shift = frame_header->loop_restoration_params.lr_unit_shift,
      .lr_uv_shift = frame_header->loop_restoration_params.lr_uv_shift,
    },
  };
  /* *INDENT-ON* */

  if (seq_header->bit_depth == 8) {
    pic_param.bit_depth_idx = 0;
  } else if (seq_header->bit_depth == 10) {
    pic_param.bit_depth_idx = 1;
  } else if (seq_header->bit_depth == 12) {
    pic_param.bit_depth_idx = 2;
  } else {
    g_assert_not_reached ();
  }

  if (frame_header->film_grain_params.apply_grain) {
    pic_param.current_frame = gst_va_decode_picture_get_aux_surface (va_pic);
    pic_param.current_display_picture =
        gst_va_decode_picture_get_surface (va_pic);
  } else {
    pic_param.current_frame = gst_va_decode_picture_get_surface (va_pic);
    pic_param.current_display_picture = VA_INVALID_SURFACE;
  }

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (dpb->pic_list[i]) {
      if (dpb->pic_list[i]->apply_grain) {
        pic_param.ref_frame_map[i] = gst_va_decode_picture_get_aux_surface
            (gst_av1_picture_get_user_data (dpb->pic_list[i]));
      } else {
        pic_param.ref_frame_map[i] = gst_va_decode_picture_get_surface
            (gst_av1_picture_get_user_data (dpb->pic_list[i]));
      }
    } else {
      pic_param.ref_frame_map[i] = VA_INVALID_SURFACE;
    }
  }
  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    pic_param.ref_frame_idx[i] = frame_header->ref_frame_idx[i];
  }
  pic_param.primary_ref_frame = frame_header->primary_ref_frame;

  _setup_segment_info (&pic_param, frame_header);
  _setup_film_grain_info (&pic_param, frame_header);

  for (i = 0; i < 63; i++) {
    pic_param.width_in_sbs_minus_1[i] =
        frame_header->tile_info.width_in_sbs_minus_1[i];
    pic_param.height_in_sbs_minus_1[i] =
        frame_header->tile_info.height_in_sbs_minus_1[i];
  }

  _setup_loop_filter_info (&pic_param, frame_header);
  _setup_quantization_info (&pic_param, frame_header);
  _setup_cdef_info (&pic_param, frame_header, seq_header->num_planes);
  _setup_global_motion_info (&pic_param, frame_header);

  if (!gst_va_decoder_add_param_buffer (base->decoder, va_pic,
          VAPictureParameterBufferType, &pic_param, sizeof (pic_param)))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_av1_dec_decode_tile (GstAV1Decoder * decoder, GstAV1Picture * picture,
    GstAV1Tile * tile)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstAV1TileGroupOBU *tile_group = &tile->tile_group;
  GstVaDecodePicture *va_pic;
  guint i;
  VASliceParameterBufferAV1 slice_param[GST_AV1_MAX_TILE_COUNT] = { 0, };

  GST_TRACE_OBJECT (self, "-");

  for (i = 0; i < tile_group->tg_end - tile_group->tg_start + 1; i++) {
    slice_param[i].slice_data_size =
        tile_group->entry[tile_group->tg_start + i].tile_size;
    slice_param[i].slice_data_offset =
        tile_group->entry[tile_group->tg_start + i].tile_offset;
    slice_param[i].tile_row =
        tile_group->entry[tile_group->tg_start + i].tile_row;
    slice_param[i].tile_column =
        tile_group->entry[tile_group->tg_start + i].tile_col;
    slice_param[i].slice_data_flag = 0;
  }

  va_pic = gst_av1_picture_get_user_data (picture);

  if (!gst_va_decoder_add_slice_buffer_with_n_params (base->decoder, va_pic,
          slice_param, sizeof (VASliceParameterBufferAV1), i, tile->obu.data,
          tile->obu.obu_size)) {
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_av1_dec_end_picture (GstAV1Decoder * decoder, GstAV1Picture * picture)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaDecodePicture *va_pic;

  GST_LOG_OBJECT (self, "end picture %p, (system_frame_number %d)",
      picture, GST_CODEC_PICTURE_FRAME_NUMBER (picture));

  va_pic = gst_av1_picture_get_user_data (picture);

  if (!gst_va_decoder_decode_with_aux_surface (base->decoder, va_pic,
          picture->apply_grain)) {
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_av1_dec_output_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstCodecPicture *codec_picture = GST_CODEC_PICTURE (picture);
  gboolean ret;

  g_assert (picture->frame_hdr.show_frame ||
      picture->frame_hdr.show_existing_frame);

  GST_LOG_OBJECT (self,
      "Outputting picture %p (system_frame_number %d)",
      picture, codec_picture->system_frame_number);

  if (picture->frame_hdr.show_existing_frame) {
    GstVaDecodePicture *pic;

    g_assert (!frame->output_buffer);
    pic = gst_av1_picture_get_user_data (picture);
    frame->output_buffer = gst_buffer_ref (pic->gstbuffer);
  }

  ret = gst_va_base_dec_process_output (base,
      frame, codec_picture->discont_state, 0);
  gst_av1_picture_unref (picture);

  if (ret)
    return gst_video_decoder_finish_frame (vdec, frame);
  return GST_FLOW_ERROR;
}

static gboolean
gst_va_av1_dec_start (GstVideoDecoder * decoder)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);

  self->preferred_format = GST_VIDEO_FORMAT_UNKNOWN;

  return GST_VIDEO_DECODER_CLASS (parent_class)->start (decoder);
}

static gboolean
gst_va_av1_dec_close (GstVideoDecoder * decoder)
{
  GstVaAV1Dec *self = GST_VA_AV1_DEC (decoder);

  _clear_internal_pool (self);

  return gst_va_base_dec_close (GST_VIDEO_DECODER (decoder));
}

static void
gst_va_av1_dec_init (GTypeInstance * instance, gpointer g_class)
{
  gst_va_base_dec_init (GST_VA_BASE_DEC (instance), GST_CAT_DEFAULT);
}

static void
gst_va_av1_dec_dispose (GObject * object)
{
  gst_va_base_dec_close (GST_VIDEO_DECODER (object));
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_av1_dec_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstAV1DecoderClass *av1decoder_class = GST_AV1_DECODER_CLASS (g_class);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (g_class);
  struct CData *cdata = class_data;
  gchar *long_name;

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API AV1 Decoder in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API AV1 Decoder");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Video/Hardware",
      "VA-API based AV1 video decoder", "He Junyan <junyan.he@intel.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_class);

  /**
   * GstVaAV1Dec:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   *
   * Since: 1.22
   */
  gst_va_base_dec_class_init (GST_VA_BASE_DEC_CLASS (g_class), AV1,
      cdata->render_device_path, cdata->sink_caps, cdata->src_caps,
      src_doc_caps, sink_doc_caps);

  gobject_class->dispose = gst_va_av1_dec_dispose;

  decoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_va_av1_dec_getcaps);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_va_av1_dec_negotiate);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_va_av1_dec_close);
  decoder_class->start = GST_DEBUG_FUNCPTR (gst_va_av1_dec_start);

  av1decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_new_sequence);
  av1decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_new_picture);
  av1decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_duplicate_picture);
  av1decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_start_picture);
  av1decoder_class->decode_tile =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_decode_tile);
  av1decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_end_picture);
  av1decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_va_av1_dec_output_picture);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_av1dec_debug, "vaav1dec", 0,
      "VA AV1 decoder");

  return NULL;
}

gboolean
gst_va_av1_dec_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaAV1DecClass),
    .class_init = gst_va_av1_dec_class_init,
    .instance_size = sizeof (GstVaAV1Dec),
    .instance_init = gst_va_av1_dec_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (sink_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (src_caps), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);
  cdata->sink_caps = _complete_sink_caps (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaAV1Dec", "GstVa%sAV1Dec",
      &type_name, "vaav1dec", "va%sav1dec", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_AV1_DECODER,
      type_name, &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
