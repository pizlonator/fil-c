/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Oblong Industries, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION: element-msdkh264enc
 * @title: msdkh264enc
 * @short_description: Intel MSDK H264 encoder
 *
 * H264 video encoder based on Intel MFX
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=90 ! msdkh264enc ! h264parse ! filesink location=output.h264
 * ```
 *
 * Since: 1.12
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstmsdkh264enc.h"

#include <gst/base/base.h>
#include <gst/pbutils/pbutils.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_msdkh264enc_debug);
#define GST_CAT_DEFAULT gst_msdkh264enc_debug

#define GST_MSDKH264ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkH264Enc))
#define GST_MSDKH264ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkH264EncClass))
#define GST_IS_MSDKH264ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKH264ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

enum
{
  PROP_CABAC = GST_MSDKENC_PROP_MAX,
#ifndef GST_REMOVE_DEPRECATED
  PROP_LOW_POWER,
#endif
  PROP_FRAME_PACKING,
  PROP_RC_LA_DOWNSAMPLING,
  PROP_TRELLIS,
  PROP_MAX_SLICE_SIZE,
  PROP_B_PYRAMID,
  PROP_TUNE_MODE,
  PROP_P_PYRAMID,
  PROP_MIN_QP,
  PROP_MIN_QP_I,
  PROP_MIN_QP_P,
  PROP_MIN_QP_B,
  PROP_MAX_QP,
  PROP_MAX_QP_I,
  PROP_MAX_QP_P,
  PROP_MAX_QP_B,
  PROP_INTRA_REFRESH_TYPE,
  PROP_INTRA_REFRESH_CYCLE_SIZE,
  PROP_INTRA_REFRESH_QP_DELTA,
  PROP_INTRA_REFRESH_CYCLE_DIST,
  PROP_DBLK_IDC,
  PROP_PIC_TIMING_SEI,
};

enum
{
  GST_MSDK_FLAG_LOW_POWER = 1 << 0,
  GST_MSDK_FLAG_TUNE_MODE = 1 << 1,
};

#define PROP_CABAC_DEFAULT                    TRUE
#define PROP_LOWPOWER_DEFAULT                 FALSE
#define PROP_FRAME_PACKING_DEFAULT            -1
#define PROP_RC_LA_DOWNSAMPLING_DEFAULT       MFX_LOOKAHEAD_DS_UNKNOWN
#define PROP_TRELLIS_DEFAULT                  _MFX_TRELLIS_NONE
#define PROP_MAX_SLICE_SIZE_DEFAULT           0
#define PROP_B_PYRAMID_DEFAULT                FALSE
#define PROP_TUNE_MODE_DEFAULT                MFX_CODINGOPTION_UNKNOWN
#define PROP_P_PYRAMID_DEFAULT                FALSE
#define PROP_MIN_QP_DEFAULT                   0
#define PROP_MAX_QP_DEFAULT                   0
#define PROP_INTRA_REFRESH_TYPE_DEFAULT       MFX_REFRESH_NO
#define PROP_INTRA_REFRESH_CYCLE_SIZE_DEFAULT 0
#define PROP_INTRA_REFRESH_QP_DELTA_DEFAULT   0
#define PROP_INTRA_REFRESH_CYCLE_DIST_DEFAULT 0
#define PROP_DBLK_IDC_DEFAULT                 0
#define PROP_PIC_TIMING_SEI_DEFAULT           TRUE

/* *INDENT-OFF* */
static const gchar *doc_sink_caps_str =
    GST_VIDEO_CAPS_MAKE ("{ NV12, YUY2, BGRA, VUYA }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, YUY2, BGRA, VUYA }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory", "{ NV12 }");
/* *INDENT-ON* */

static const gchar *doc_src_caps_str = "video/x-h264";

static GstElementClass *parent_class = NULL;

static GType
gst_msdkh264enc_frame_packing_get_type (void)
{
  static GType format_type = 0;
  static const GEnumValue format_types[] = {
    {GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE, "None (default)", "none"},
    {GST_VIDEO_MULTIVIEW_FRAME_PACKING_SIDE_BY_SIDE, "Side by Side",
        "side-by-side"},
    {GST_VIDEO_MULTIVIEW_FRAME_PACKING_TOP_BOTTOM, "Top Bottom", "top-bottom"},
    {0, NULL, NULL}
  };

  if (!format_type) {
    format_type =
        g_enum_register_static ("GstMsdkH264EncFramePacking", format_types);
  }

  return format_type;
}

static void
gst_msdkh264enc_insert_sei (GstMsdkH264Enc * thiz, GstVideoCodecFrame * frame,
    GstMemory * sei_mem)
{
  GstBuffer *new_buffer;

  if (!thiz->parser)
    thiz->parser = gst_h264_nal_parser_new ();

  new_buffer = gst_h264_parser_insert_sei (thiz->parser,
      frame->output_buffer, sei_mem);

  if (!new_buffer) {
    GST_WARNING_OBJECT (thiz, "Cannot insert SEI nal into AU buffer");
    return;
  }

  gst_buffer_unref (frame->output_buffer);
  frame->output_buffer = new_buffer;
}

static void
gst_msdkh264enc_add_cc (GstMsdkH264Enc * thiz, GstVideoCodecFrame * frame)
{
  GstVideoCaptionMeta *cc_meta;
  gpointer iter = NULL;
  GstBuffer *in_buf = frame->input_buffer;
  GstMemory *mem = NULL;

  if (thiz->cc_sei_array)
    g_array_set_size (thiz->cc_sei_array, 0);

  while ((cc_meta =
          (GstVideoCaptionMeta *) gst_buffer_iterate_meta_filtered (in_buf,
              &iter, GST_VIDEO_CAPTION_META_API_TYPE))) {
    GstH264SEIMessage sei;
    GstH264RegisteredUserData *rud;
    guint8 *data;

    if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
      continue;

    memset (&sei, 0, sizeof (GstH264SEIMessage));
    sei.payloadType = GST_H264_SEI_REGISTERED_USER_DATA;
    rud = &sei.payload.registered_user_data;

    rud->country_code = 181;
    rud->size = cc_meta->size + 10;

    data = g_malloc (rud->size);
    memcpy (data + 9, cc_meta->data, cc_meta->size);

    data[0] = 0;                /* 16-bits itu_t_t35_provider_code */
    data[1] = 49;
    data[2] = 'G';              /* 32-bits ATSC_user_identifier */
    data[3] = 'A';
    data[4] = '9';
    data[5] = '4';
    data[6] = 3;                /* 8-bits ATSC1_data_user_data_type_code */
    /* 8-bits:
     * 1 bit process_em_data_flag (0)
     * 1 bit process_cc_data_flag (1)
     * 1 bit additional_data_flag (0)
     * 5-bits cc_count
     */
    data[7] = ((cc_meta->size / 3) & 0x1f) | 0x40;
    data[8] = 255;              /* 8 bits em_data, unused */
    data[cc_meta->size + 9] = 255;      /* 8 marker bits */

    rud->data = data;

    if (!thiz->cc_sei_array) {
      thiz->cc_sei_array =
          g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));
      g_array_set_clear_func (thiz->cc_sei_array,
          (GDestroyNotify) gst_h264_sei_clear);
    }

    g_array_append_val (thiz->cc_sei_array, sei);
  }

  if (!thiz->cc_sei_array || !thiz->cc_sei_array->len)
    return;

  mem = gst_h264_create_sei_memory (4, thiz->cc_sei_array);

  if (!mem) {
    GST_WARNING_OBJECT (thiz, "Cannot create SEI nal unit");
    return;
  }

  GST_DEBUG_OBJECT (thiz,
      "Inserting %d closed caption SEI message(s)", thiz->cc_sei_array->len);

  gst_msdkh264enc_insert_sei (thiz, frame, mem);
  gst_memory_unref (mem);
}

static GstFlowReturn
gst_msdkh264enc_pre_push (GstVideoEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (encoder);

  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame) && thiz->frame_packing_sei) {
    /* Insert frame packing SEI
     * FIXME: This assumes it does not exist in the stream, which is not
     * going to be true anymore once this is fixed:
     * https://github.com/Intel-Media-SDK/MediaSDK/issues/13
     */
    GST_DEBUG_OBJECT (thiz, "Inserting SEI Frame Packing for multiview");
    gst_msdkh264enc_insert_sei (thiz, frame, thiz->frame_packing_sei);
  }

  gst_msdkh264enc_add_cc (thiz, frame);

  return GST_FLOW_OK;
}

static gboolean
gst_msdkh264enc_set_format (GstMsdkEnc * encoder)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (encoder);
  GstPad *srcpad;
  GstCaps *template_caps;
  GstCaps *allowed_caps = NULL;

  thiz->profile = 0;
  thiz->level = 0;

  srcpad = GST_VIDEO_ENCODER_SRC_PAD (encoder);
  template_caps = gst_pad_get_pad_template_caps (srcpad);
  allowed_caps = gst_pad_get_allowed_caps (srcpad);

  /* If downstream has ANY caps let encoder decide profile and level */
  if (allowed_caps == template_caps) {
    GST_INFO_OBJECT (thiz,
        "downstream has ANY caps, profile/level set to auto");
  } else if (allowed_caps) {
    GstStructure *s;
    const gchar *profile;
    const gchar *level;

    if (gst_caps_is_empty (allowed_caps)) {
      gst_caps_unref (allowed_caps);
      gst_caps_unref (template_caps);
      return FALSE;
    }

    s = gst_caps_get_structure (allowed_caps, 0);

    profile = gst_structure_get_string (s, "profile");
    if (profile) {
      if (!g_strcmp0 (profile, "high")) {
        thiz->profile = MFX_PROFILE_AVC_HIGH;
      } else if (!g_strcmp0 (profile, "main")) {
        thiz->profile = MFX_PROFILE_AVC_MAIN;
      } else if (!g_strcmp0 (profile, "baseline")) {
        thiz->profile = MFX_PROFILE_AVC_BASELINE;
      } else if (!g_strcmp0 (profile, "constrained-baseline")) {
        thiz->profile = MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
      } else {
        thiz->profile = MFX_PROFILE_UNKNOWN;
      }
    }

    level = gst_structure_get_string (s, "level");
    if (level) {
      thiz->level = gst_codec_utils_h264_get_level_idc (level);
    }

    gst_caps_unref (allowed_caps);
  }

  gst_caps_unref (template_caps);

  if (thiz->frame_packing_sei) {
    gst_memory_unref (thiz->frame_packing_sei);
    thiz->frame_packing_sei = NULL;
  }

  /* prepare frame packing SEI message */
  if (encoder->input_state) {
    GstVideoMultiviewMode mode = GST_VIDEO_MULTIVIEW_MODE_NONE;

    /* use property value if any */
    if (thiz->frame_packing != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      mode = (GstVideoMultiviewMode) thiz->frame_packing;
    } else {
      mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&encoder->input_state->info);
    }

    if (mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ||
        mode == GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM) {
      GstH264SEIMessage sei;
      GstH264FramePacking *frame_packing;
      GArray *array = g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));

      g_array_set_clear_func (thiz->cc_sei_array,
          (GDestroyNotify) gst_h264_sei_clear);

      GST_DEBUG_OBJECT (thiz,
          "Prepare frame packing SEI data for multiview mode %d", mode);

      memset (&sei, 0, sizeof (GstH264SEIMessage));

      sei.payloadType = GST_H264_SEI_FRAME_PACKING;
      frame_packing = &sei.payload.frame_packing;
      frame_packing->frame_packing_id = 0;
      frame_packing->frame_packing_cancel_flag = 0;
      frame_packing->frame_packing_type =
          (mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ?
          GST_H264_FRAME_PACKING_SIDE_BY_SIDE :
          GST_H264_FRAME_PACKING_TOP_BOTTOM);
      /* we don't do this */
      frame_packing->quincunx_sampling_flag = 0;
      /* 0: unspecified */
      /* 1: frame 0 will be left view and frame 1 will be right view */
      frame_packing->content_interpretation_type = 1;
      /* we didn't do flip */
      frame_packing->spatial_flipping_flag = 0;
      frame_packing->frame0_flipped_flag = 0;
      /* must be zero for frame_packing_type != 2 */
      frame_packing->field_views_flag = 0;
      /* must be zero for frame_packing_type != 5 */
      frame_packing->current_frame_is_frame0_flag = 0;
      /* may or may not used to reference each other */
      frame_packing->frame0_self_contained_flag = 0;
      frame_packing->frame1_self_contained_flag = 0;

      frame_packing->frame0_grid_position_x = 0;
      frame_packing->frame0_grid_position_y = 0;
      frame_packing->frame1_grid_position_x = 0;
      frame_packing->frame1_grid_position_y = 0;

      /* will be applied to this GOP */
      frame_packing->frame_packing_repetition_period = 1;

      g_array_append_val (array, sei);

      thiz->frame_packing_sei = gst_h264_create_sei_memory (4, array);
      g_array_unref (array);
    }
  }

  return TRUE;
}

static gboolean
gst_msdkh264enc_configure (GstMsdkEnc * encoder)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (encoder);

  encoder->param.mfx.LowPower = thiz->tune_mode;
  encoder->param.mfx.CodecId = MFX_CODEC_AVC;
  encoder->param.mfx.CodecProfile = thiz->profile;
  encoder->param.mfx.CodecLevel = thiz->level;

  thiz->option.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
  thiz->option.Header.BufferSz = sizeof (thiz->option);
  if (thiz->profile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE ||
      thiz->profile == MFX_PROFILE_AVC_BASELINE ||
      thiz->profile == MFX_PROFILE_AVC_EXTENDED) {
    thiz->option.CAVLC = MFX_CODINGOPTION_ON;
  } else {
    thiz->option.CAVLC =
        (thiz->cabac ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);
  }

  if (encoder->option3.LowDelayBRC == MFX_CODINGOPTION_ON) {
    thiz->option.NalHrdConformance = MFX_CODINGOPTION_OFF;
  }

  thiz->option.PicTimingSEI =
      (thiz->pic_timing_sei ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);

  gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & thiz->option);

  encoder->option2.Trellis = thiz->trellis ? thiz->trellis : MFX_TRELLIS_OFF;
  encoder->option2.MaxSliceSize = thiz->max_slice_size;
  encoder->option2.MinQPI = thiz->min_qp_i;
  encoder->option2.MinQPP = thiz->min_qp_p;
  encoder->option2.MinQPB = thiz->min_qp_b;
  encoder->option2.MaxQPI = thiz->max_qp_i;
  encoder->option2.MaxQPP = thiz->max_qp_p;
  encoder->option2.MaxQPB = thiz->max_qp_b;
  encoder->option2.IntRefType = thiz->intra_refresh_type;
  encoder->option2.IntRefCycleSize = thiz->intra_refresh_cycle_size;
  encoder->option2.IntRefQPDelta = thiz->intra_refresh_qp_delta;
  encoder->option2.DisableDeblockingIdc = thiz->dblk_idc;

  if (encoder->rate_control == MFX_RATECONTROL_LA ||
      encoder->rate_control == MFX_RATECONTROL_LA_HRD ||
      encoder->rate_control == MFX_RATECONTROL_LA_ICQ)
    encoder->option2.LookAheadDS = thiz->lookahead_ds;

  if (thiz->b_pyramid) {
    encoder->option2.BRefType = MFX_B_REF_PYRAMID;
    /* Don't define Gop structure for B-pyramid, otherwise EncodeInit
     * will throw Invalid param error */
    encoder->param.mfx.GopRefDist = 0;
  }

  if (thiz->p_pyramid) {
    encoder->option3.PRefType = MFX_P_REF_PYRAMID;
    /* MFX_P_REF_PYRAMID is available for GopRefDist = 1 */
    encoder->param.mfx.GopRefDist = 1;
    /* SDK decides the DPB size for P pyramid */
    encoder->param.mfx.NumRefFrame = 0;
  }
  if (thiz->intra_refresh_cycle_dist) {
    encoder->option3.IntRefCycleDist = thiz->intra_refresh_cycle_dist;
    encoder->enable_extopt3 = TRUE;
  }

  /* Enable Extended coding options */
  gst_msdkenc_ensure_extended_coding_options (encoder);

  return TRUE;
}

static inline const gchar *
profile_to_string (gint profile)
{
  switch (profile) {
    case MFX_PROFILE_AVC_HIGH:
      return "high";
    case MFX_PROFILE_AVC_MAIN:
      return "main";
    case MFX_PROFILE_AVC_BASELINE:
      return "baseline";
    case MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
      return "constrained-baseline";
    default:
      break;
  }

  return NULL;
}

static inline const gchar *
level_to_string (gint level)
{
  switch (level) {
    case MFX_LEVEL_AVC_1:
      return "1";
    case MFX_LEVEL_AVC_1b:
      return "1.1";
    case MFX_LEVEL_AVC_11:
      return "1.1";
    case MFX_LEVEL_AVC_12:
      return "1.2";
    case MFX_LEVEL_AVC_13:
      return "1.3";
    case MFX_LEVEL_AVC_2:
      return "2";
    case MFX_LEVEL_AVC_21:
      return "2.1";
    case MFX_LEVEL_AVC_22:
      return "2.2";
    case MFX_LEVEL_AVC_3:
      return "3";
    case MFX_LEVEL_AVC_31:
      return "3.1";
    case MFX_LEVEL_AVC_32:
      return "3.2";
    case MFX_LEVEL_AVC_4:
      return "4";
    case MFX_LEVEL_AVC_41:
      return "4.1";
    case MFX_LEVEL_AVC_42:
      return "4.2";
    case MFX_LEVEL_AVC_5:
      return "5";
    case MFX_LEVEL_AVC_51:
      return "5.1";
    case MFX_LEVEL_AVC_52:
      return "5.2";
    default:
      break;
  }

  return NULL;
}

static GstCaps *
gst_msdkh264enc_set_src_caps (GstMsdkEnc * encoder)
{
  GstCaps *caps;
  GstStructure *structure;
  const gchar *profile;
  const gchar *level;

  caps = gst_caps_new_empty_simple ("video/x-h264");
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_set (structure, "stream-format", G_TYPE_STRING, "byte-stream",
      NULL);

  gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);

  profile = profile_to_string (encoder->param.mfx.CodecProfile);
  if (profile)
    gst_structure_set (structure, "profile", G_TYPE_STRING, profile, NULL);

  level = level_to_string (encoder->param.mfx.CodecLevel);
  if (level)
    gst_structure_set (structure, "level", G_TYPE_STRING, level, NULL);

  return caps;
}

static void
gst_msdkh264enc_dispose (GObject * object)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (object);

  if (thiz->frame_packing_sei) {
    gst_memory_unref (thiz->frame_packing_sei);
    thiz->frame_packing_sei = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_msdkh264enc_finalize (GObject * object)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (object);

  if (thiz->parser)
    gst_h264_nal_parser_free (thiz->parser);
  if (thiz->cc_sei_array)
    g_array_unref (thiz->cc_sei_array);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_msdkh264enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkEnc *enc = GST_MSDKENC (object);
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (object);

  if (gst_msdkenc_set_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);

  switch (prop_id) {
    case PROP_CABAC:
      thiz->cabac = g_value_get_boolean (value);
      break;
#ifndef GST_REMOVE_DEPRECATED
    case PROP_LOW_POWER:
      thiz->lowpower = g_value_get_boolean (value);
      thiz->prop_flag |= GST_MSDK_FLAG_LOW_POWER;

      /* Ignore it if user set tune mode explicitly */
      if (!(thiz->prop_flag & GST_MSDK_FLAG_TUNE_MODE))
        thiz->tune_mode =
            thiz->lowpower ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;

      break;
#endif
    case PROP_FRAME_PACKING:
      thiz->frame_packing = g_value_get_enum (value);
      break;
    case PROP_RC_LA_DOWNSAMPLING:
      thiz->lookahead_ds = g_value_get_enum (value);
      break;
    case PROP_TRELLIS:
      thiz->trellis = g_value_get_flags (value);
      break;
    case PROP_MAX_SLICE_SIZE:
      thiz->max_slice_size = g_value_get_uint (value);
      break;
    case PROP_B_PYRAMID:
      thiz->b_pyramid = g_value_get_boolean (value);
      break;
    case PROP_TUNE_MODE:
      thiz->tune_mode = g_value_get_enum (value);
      thiz->prop_flag |= GST_MSDK_FLAG_TUNE_MODE;
      break;
    case PROP_P_PYRAMID:
      thiz->p_pyramid = g_value_get_boolean (value);
      break;
    case PROP_MIN_QP:
      thiz->min_qp = g_value_get_uint (value);
      thiz->min_qp_i = thiz->min_qp_p = thiz->min_qp_b = thiz->min_qp;
      break;
    case PROP_MIN_QP_I:
      if (check_update_property_uint (enc, &thiz->min_qp_i,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed min-qp-i to %u", thiz->min_qp_i);
      }
      break;
    case PROP_MIN_QP_P:
      if (check_update_property_uint (enc, &thiz->min_qp_p,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed min-qp-p to %u", thiz->min_qp_p);
      }
      break;
    case PROP_MIN_QP_B:
      if (check_update_property_uint (enc, &thiz->min_qp_b,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed min-qp-b to %u", thiz->min_qp_b);
      }
      break;
    case PROP_MAX_QP:
      thiz->max_qp = g_value_get_uint (value);
      thiz->max_qp_i = thiz->max_qp_p = thiz->max_qp_b = thiz->max_qp;
      break;
    case PROP_MAX_QP_I:
      if (check_update_property_uint (enc, &thiz->max_qp_i,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-qp-i to %u", thiz->max_qp_i);
      }
      break;
    case PROP_MAX_QP_P:
      if (check_update_property_uint (enc, &thiz->max_qp_p,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-qp-p to %u", thiz->max_qp_p);
      }
      break;
    case PROP_MAX_QP_B:
      if (check_update_property_uint (enc, &thiz->max_qp_b,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-qp-b to %u", thiz->max_qp_b);
      }
      break;
    case PROP_INTRA_REFRESH_TYPE:
      if (check_update_property_uint (enc, &thiz->intra_refresh_type,
              g_value_get_enum (value))) {
        GST_DEBUG_OBJECT (thiz, "changed intra-refresh-type to %u",
            thiz->intra_refresh_type);
      }
      break;
    case PROP_INTRA_REFRESH_CYCLE_SIZE:
      if (check_update_property_uint (enc, &thiz->intra_refresh_cycle_size,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed intra-refresh-cycle-size to %u",
            thiz->intra_refresh_cycle_size);
      }
      break;
    case PROP_INTRA_REFRESH_QP_DELTA:
      if (check_update_property_int (enc, &thiz->intra_refresh_qp_delta,
              g_value_get_int (value))) {
        GST_DEBUG_OBJECT (thiz, "changed intra-refresh-qp-delta to %d",
            thiz->intra_refresh_qp_delta);
      }
      break;
    case PROP_INTRA_REFRESH_CYCLE_DIST:
      if (check_update_property_uint (enc, &thiz->intra_refresh_cycle_dist,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed intra-refresh-cycle-dist to %u",
            thiz->intra_refresh_cycle_dist);
      }
      break;
    case PROP_DBLK_IDC:
      thiz->dblk_idc = g_value_get_uint (value);
      break;
    case PROP_PIC_TIMING_SEI:
      if (check_update_property_bool (enc, &thiz->pic_timing_sei,
              g_value_get_boolean (value))) {
        GST_DEBUG_OBJECT (thiz, "changed pic-timimg-sei to %d",
            thiz->pic_timing_sei);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
  return;
}

static void
gst_msdkh264enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (object);

  if (gst_msdkenc_get_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);
  switch (prop_id) {
    case PROP_CABAC:
      g_value_set_boolean (value, thiz->cabac);
      break;
#ifndef GST_REMOVE_DEPRECATED
    case PROP_LOW_POWER:
      g_value_set_boolean (value, thiz->lowpower);
      break;
#endif
    case PROP_FRAME_PACKING:
      g_value_set_enum (value, thiz->frame_packing);
      break;
    case PROP_RC_LA_DOWNSAMPLING:
      g_value_set_enum (value, thiz->lookahead_ds);
      break;
    case PROP_TRELLIS:
      g_value_set_flags (value, thiz->trellis);
      break;
    case PROP_MAX_SLICE_SIZE:
      g_value_set_uint (value, thiz->max_slice_size);
      break;
    case PROP_B_PYRAMID:
      g_value_set_boolean (value, thiz->b_pyramid);
      break;
    case PROP_TUNE_MODE:
      g_value_set_enum (value, thiz->tune_mode);
      break;
    case PROP_P_PYRAMID:
      g_value_set_boolean (value, thiz->p_pyramid);
      break;
    case PROP_MIN_QP:
      g_value_set_uint (value, thiz->min_qp);
      break;
    case PROP_MIN_QP_I:
      g_value_set_uint (value, thiz->min_qp_i);
      break;
    case PROP_MIN_QP_P:
      g_value_set_uint (value, thiz->min_qp_p);
      break;
    case PROP_MIN_QP_B:
      g_value_set_uint (value, thiz->min_qp_b);
      break;
    case PROP_MAX_QP:
      g_value_set_uint (value, thiz->max_qp);
      break;
    case PROP_MAX_QP_I:
      g_value_set_uint (value, thiz->max_qp_i);
      break;
    case PROP_MAX_QP_P:
      g_value_set_uint (value, thiz->max_qp_p);
      break;
    case PROP_MAX_QP_B:
      g_value_set_uint (value, thiz->max_qp_b);
      break;
    case PROP_INTRA_REFRESH_TYPE:
      g_value_set_enum (value, thiz->intra_refresh_type);
      break;
    case PROP_INTRA_REFRESH_CYCLE_SIZE:
      g_value_set_uint (value, thiz->intra_refresh_cycle_size);
      break;
    case PROP_INTRA_REFRESH_QP_DELTA:
      g_value_set_int (value, thiz->intra_refresh_qp_delta);
      break;
    case PROP_INTRA_REFRESH_CYCLE_DIST:
      g_value_set_uint (value, thiz->intra_refresh_cycle_dist);
      break;
    case PROP_DBLK_IDC:
      g_value_set_uint (value, thiz->dblk_idc);
      break;
    case PROP_PIC_TIMING_SEI:
      g_value_set_boolean (value, thiz->pic_timing_sei);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
}

static gboolean
gst_msdkh264enc_need_reconfig (GstMsdkEnc * encoder, GstVideoCodecFrame * frame)
{
  GstMsdkH264Enc *h264enc = GST_MSDKH264ENC (encoder);

  return gst_msdkenc_get_roi_params (encoder, frame, h264enc->roi);
}

static void
gst_msdkh264enc_set_extra_params (GstMsdkEnc * encoder,
    GstVideoCodecFrame * frame)
{
  GstMsdkH264Enc *h264enc = GST_MSDKH264ENC (encoder);

  if (h264enc->roi[0].NumROI)
    gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & h264enc->roi[0]);
}

static gboolean
gst_msdkh264enc_is_format_supported (GstMsdkEnc * encoder,
    GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      return TRUE;
    default:
      return FALSE;
  }
}

static void
_msdkh264enc_install_properties (GObjectClass * gobject_class,
    GstMsdkEncClass * encoder_class)
{
  gst_msdkenc_install_common_properties (encoder_class);

  g_object_class_install_property (gobject_class, PROP_CABAC,
      g_param_spec_boolean ("cabac", "CABAC", "Enable CABAC entropy coding",
          PROP_CABAC_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifndef GST_REMOVE_DEPRECATED
  g_object_class_install_property (gobject_class, PROP_LOW_POWER,
      g_param_spec_boolean ("low-power", "Low power",
          "Enable low power mode (DEPRECATED, use tune instead)",
          PROP_LOWPOWER_DEFAULT,
          G_PARAM_DEPRECATED | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_FRAME_PACKING,
      g_param_spec_enum ("frame-packing", "Frame Packing",
          "Set frame packing mode for Stereoscopic content",
          gst_msdkh264enc_frame_packing_get_type (), PROP_FRAME_PACKING_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RC_LA_DOWNSAMPLING,
      g_param_spec_enum ("rc-lookahead-ds", "Look-ahead Downsampling",
          "Down sampling mode in look ahead bitrate control",
          gst_msdkenc_rc_lookahead_ds_get_type (),
          PROP_RC_LA_DOWNSAMPLING_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TRELLIS,
      g_param_spec_flags ("trellis", "Trellis",
          "Enable Trellis Quantization",
          gst_msdkenc_trellis_quantization_get_type (), _MFX_TRELLIS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_SLICE_SIZE,
      g_param_spec_uint ("max-slice-size", "Max Slice Size",
          "Maximum slice size in bytes (if enabled MSDK will ignore the control over num-slices)",
          0, G_MAXUINT32, PROP_MAX_SLICE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_B_PYRAMID,
      g_param_spec_boolean ("b-pyramid", "B-pyramid",
          "Enable B-Pyramid Reference structure", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TUNE_MODE,
      g_param_spec_enum ("tune", "Encoder tuning",
          "Encoder tuning option",
          gst_msdkenc_tune_mode_get_type (), PROP_TUNE_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_P_PYRAMID,
      g_param_spec_boolean ("p-pyramid", "P-pyramid",
          "Enable P-Pyramid Reference structure", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MIN_QP,
      g_param_spec_uint ("min-qp", "Min QP",
          "Minimal quantizer scale for I/P/B frames",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:min-qp-i:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MIN_QP_I,
      g_param_spec_uint ("min-qp-i", "Min QP I",
          "Minimal quantizer scale for I frame",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:min-qp-p:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MIN_QP_P,
      g_param_spec_uint ("min-qp-p", "Min QP P",
          "Minimal quantizer scale for P frame",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:min-qp-b:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MIN_QP_B,
      g_param_spec_uint ("min-qp-b", "Min QP B",
          "Minimal quantizer scale for B frame",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_QP,
      g_param_spec_uint ("max-qp", "Max QP",
          "Maximum quantizer scale for I/P/B frames",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:max-qp-i:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_QP_I,
      g_param_spec_uint ("max-qp-i", "Max QP I",
          "Maximum quantizer scale for I frame",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:max-qp-p:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_QP_P,
      g_param_spec_uint ("max-qp-p", "Max QP P",
          "Maximum quantizer scale for P frame",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:max-qp-b:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_QP_B,
      g_param_spec_uint ("max-qp-b", "Max QP B",
          "Maximum quantizer scale for B frame",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_TYPE,
      g_param_spec_enum ("intra-refresh-type", "Intra refresh type",
          "Set intra refresh type",
          gst_msdkenc_intra_refresh_type_get_type (),
          PROP_INTRA_REFRESH_TYPE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_CYCLE_SIZE,
      g_param_spec_uint ("intra-refresh-cycle-size", "Intra refresh cycle size",
          "Set intra refresh cycle size, valid value starts from 2",
          0, G_MAXUINT16, PROP_INTRA_REFRESH_CYCLE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_QP_DELTA,
      g_param_spec_int ("intra-refresh-qp-delta", "Intra refresh qp delta",
          "Set intra refresh qp delta",
          -51, 51, PROP_INTRA_REFRESH_QP_DELTA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_CYCLE_DIST,
      g_param_spec_uint ("intra-refresh-cycle-dist", "Intra refresh cycle dist",
          "Set intra refresh cycle dist",
          0, G_MAXUINT16, PROP_INTRA_REFRESH_CYCLE_DIST_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DBLK_IDC,
      g_param_spec_uint ("dblk-idc", "Disable Deblocking Idc",
          "Option of disable deblocking idc",
          0, 2, PROP_DBLK_IDC_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH264Enc:pic-timing-sei:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_PIC_TIMING_SEI,
      g_param_spec_boolean ("pic-timing-sei", "Picture Timing SEI",
          "Insert picture timing SEI with pic_struct syntax",
          PROP_PIC_TIMING_SEI_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_msdkh264enc_class_init (gpointer klass, gpointer data)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *videoencoder_class;
  GstMsdkEncClass *encoder_class;
  MsdkEncCData *cdata = data;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  videoencoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  encoder_class = GST_MSDKENC_CLASS (klass);

  gobject_class->dispose = gst_msdkh264enc_dispose;
  gobject_class->finalize = gst_msdkh264enc_finalize;
  gobject_class->set_property = gst_msdkh264enc_set_property;
  gobject_class->get_property = gst_msdkh264enc_get_property;

  videoencoder_class->pre_push = gst_msdkh264enc_pre_push;

  encoder_class->set_format = gst_msdkh264enc_set_format;
  encoder_class->configure = gst_msdkh264enc_configure;
  encoder_class->set_src_caps = gst_msdkh264enc_set_src_caps;
  encoder_class->need_reconfig = gst_msdkh264enc_need_reconfig;
  encoder_class->set_extra_params = gst_msdkh264enc_set_extra_params;
  encoder_class->is_format_supported = gst_msdkh264enc_is_format_supported;

  _msdkh264enc_install_properties (gobject_class, encoder_class);

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK H264 encoder", "Codec/Encoder/Video/Hardware",
      "H264 video encoder based on " MFX_API_SDK,
      "Josep Torra <jtorra@oblong.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkh264enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstMsdkH264Enc *thiz = GST_MSDKH264ENC (instance);
  thiz->cabac = PROP_CABAC_DEFAULT;
  thiz->lowpower = PROP_LOWPOWER_DEFAULT;
  thiz->frame_packing = PROP_FRAME_PACKING_DEFAULT;
  thiz->lookahead_ds = PROP_RC_LA_DOWNSAMPLING_DEFAULT;
  thiz->trellis = PROP_TRELLIS_DEFAULT;
  thiz->max_slice_size = PROP_MAX_SLICE_SIZE_DEFAULT;
  thiz->b_pyramid = PROP_B_PYRAMID_DEFAULT;
  thiz->tune_mode = PROP_TUNE_MODE_DEFAULT;
  thiz->p_pyramid = PROP_P_PYRAMID_DEFAULT;
  thiz->min_qp = PROP_MIN_QP_DEFAULT;
  thiz->min_qp_i = PROP_MIN_QP_DEFAULT;
  thiz->min_qp_p = PROP_MIN_QP_DEFAULT;
  thiz->min_qp_b = PROP_MIN_QP_DEFAULT;
  thiz->max_qp = PROP_MAX_QP_DEFAULT;
  thiz->max_qp_i = PROP_MAX_QP_DEFAULT;
  thiz->max_qp_p = PROP_MAX_QP_DEFAULT;
  thiz->max_qp_b = PROP_MAX_QP_DEFAULT;
  thiz->intra_refresh_type = PROP_INTRA_REFRESH_TYPE_DEFAULT;
  thiz->intra_refresh_cycle_size = PROP_INTRA_REFRESH_CYCLE_SIZE_DEFAULT;
  thiz->intra_refresh_qp_delta = PROP_INTRA_REFRESH_QP_DELTA_DEFAULT;
  thiz->intra_refresh_cycle_dist = PROP_INTRA_REFRESH_CYCLE_DIST_DEFAULT;
  thiz->dblk_idc = PROP_DBLK_IDC_DEFAULT;
  thiz->pic_timing_sei = PROP_PIC_TIMING_SEI_DEFAULT;
}

gboolean
gst_msdkh264enc_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkEncCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkH264EncClass),
    .class_init = gst_msdkh264enc_class_init,
    .instance_size = sizeof (GstMsdkH264Enc),
    .instance_init = gst_msdkh264enc_init
  };

  cdata = g_new (MsdkEncCData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_copy (src_caps);

  gst_caps_set_simple (cdata->src_caps,
      "alignment", G_TYPE_STRING, "au",
      "stream-format", G_TYPE_STRING, "byte-stream", NULL);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkH264Enc");
  feature_name = g_strdup ("msdkh264enc");

  type = g_type_register_static (GST_TYPE_MSDKENC, type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
