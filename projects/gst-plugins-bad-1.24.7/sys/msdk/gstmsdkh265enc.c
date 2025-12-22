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
 * SECTION:element-msdkh265enc
 * @title: msdkh265enc
 * @short_description: Intel MSDK H265 encoder
 *
 * H265 video encoder based on Intel MFX
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=90 ! msdkh265enc ! h265parse ! filesink location=output.h265
 * ```
 *
 * Since: 1.12
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/allocators/gstdmabuf.h>

#include "gstmsdkh265enc.h"

GST_DEBUG_CATEGORY_EXTERN (gst_msdkh265enc_debug);
#define GST_CAT_DEFAULT gst_msdkh265enc_debug

#define GST_MSDKH265ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkH265Enc))
#define GST_MSDKH265ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkH265EncClass))
#define GST_IS_MSDKH265ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKH265ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

enum
{
#ifndef GST_REMOVE_DEPRECATED
  PROP_LOW_POWER = GST_MSDKENC_PROP_MAX,
  PROP_TILE_ROW,
#else
  PROP_TILE_ROW = GST_MSDKENC_PROP_MAX,
#endif
  PROP_TILE_COL,
  PROP_MAX_SLICE_SIZE,
  PROP_TUNE_MODE,
  PROP_TRANSFORM_SKIP,
  PROP_B_PYRAMID,
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

#define PROP_LOWPOWER_DEFAULT                 FALSE
#define PROP_TILE_ROW_DEFAULT                 1
#define PROP_TILE_COL_DEFAULT                 1
#define PROP_MAX_SLICE_SIZE_DEFAULT           0
#define PROP_TUNE_MODE_DEFAULT                MFX_CODINGOPTION_UNKNOWN
#define PROP_TRANSFORM_SKIP_DEFAULT           MFX_CODINGOPTION_UNKNOWN
#define PROP_B_PYRAMID_DEFAULT                FALSE
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
    GST_VIDEO_CAPS_MAKE (
        "{ NV12, P010_10LE, YUY2, BGRA, VUYA, BGR10A2_LE, Y210, Y410, "
        "P012_LE, Y212_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, P010_10LE, YUY2, BGRA, VUYA, BGR10A2_LE, Y210, Y410, "
        "P012_LE, Y212_LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory",
        "{ NV12, P010_10LE }");
/* *INDENT-ON* */

static const gchar *doc_src_caps_str = "video/x-h265";

static GstElementClass *parent_class = NULL;

static void
gst_msdkh265enc_insert_sei (GstMsdkH265Enc * thiz, GstVideoCodecFrame * frame,
    GstMemory * sei_mem)
{
  GstBuffer *new_buffer;

  if (!thiz->parser)
    thiz->parser = gst_h265_parser_new ();

  new_buffer = gst_h265_parser_insert_sei (thiz->parser,
      frame->output_buffer, sei_mem);

  if (!new_buffer) {
    GST_WARNING_OBJECT (thiz, "Cannot insert SEI nal into AU buffer");
    return;
  }

  gst_buffer_unref (frame->output_buffer);
  frame->output_buffer = new_buffer;
}

static void
gst_msdkh265enc_add_cc (GstMsdkH265Enc * thiz, GstVideoCodecFrame * frame)
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
    GstH265SEIMessage sei;
    GstH265RegisteredUserData *rud;
    guint8 *data;

    if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
      continue;

    memset (&sei, 0, sizeof (GstH265SEIMessage));
    sei.payloadType = GST_H265_SEI_REGISTERED_USER_DATA;
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
          g_array_new (FALSE, FALSE, sizeof (GstH265SEIMessage));
      g_array_set_clear_func (thiz->cc_sei_array,
          (GDestroyNotify) gst_h265_sei_free);
    }

    g_array_append_val (thiz->cc_sei_array, sei);
  }

  if (!thiz->cc_sei_array || !thiz->cc_sei_array->len)
    return;

  /* layer_id and temporal_id will be updated by parser later */
  mem = gst_h265_create_sei_memory (0, 1, 4, thiz->cc_sei_array);

  if (!mem) {
    GST_WARNING_OBJECT (thiz, "Cannot create SEI nal unit");
    return;
  }

  GST_DEBUG_OBJECT (thiz,
      "Inserting %d closed caption SEI message(s)", thiz->cc_sei_array->len);

  gst_msdkh265enc_insert_sei (thiz, frame, mem);
  gst_memory_unref (mem);
}

static GstFlowReturn
gst_msdkh265enc_pre_push (GstVideoEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (encoder);

  gst_msdkh265enc_add_cc (thiz, frame);

  return GST_FLOW_OK;
}

static gboolean
gst_msdkh265enc_set_format (GstMsdkEnc * encoder)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (encoder);
  GstPad *srcpad;
  GstCaps *template_caps, *allowed_caps;

  g_free (thiz->profile_name);
  thiz->profile_name = NULL;

  srcpad = GST_VIDEO_ENCODER_SRC_PAD (encoder);

  allowed_caps = gst_pad_get_allowed_caps (srcpad);
  if (!allowed_caps || gst_caps_is_empty (allowed_caps)) {
    if (allowed_caps)
      gst_caps_unref (allowed_caps);
    return FALSE;
  }

  template_caps = gst_pad_get_pad_template_caps (srcpad);

  if (gst_caps_is_equal (allowed_caps, template_caps)) {
    GST_INFO_OBJECT (thiz,
        "downstream have the same caps, profile set to auto");
  } else {
    GstStructure *s;
    const gchar *profile;

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_fixate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);
    profile = gst_structure_get_string (s, "profile");

    if (profile) {
      thiz->profile_name = g_strdup (profile);
    }
  }

  gst_caps_unref (allowed_caps);
  gst_caps_unref (template_caps);

  return TRUE;
}

static void
_get_hdr_sei (GstMsdkEnc * encoder)
{
  GstVideoCodecState *state;
  GstVideoMasteringDisplayInfo mdcv_info;
  GstVideoContentLightLevel cll_info;
  GstMsdkH265Enc *h265enc = GST_MSDKH265ENC (encoder);
  const guint chroma_den = 50000;
  const guint luma_den = 10000;

  h265enc->have_mdcv = h265enc->have_cll = FALSE;
  state = encoder->input_state;
  if (gst_video_mastering_display_info_from_caps (&mdcv_info, state->caps)) {
    memset (&h265enc->mdcv, 0, sizeof (mfxExtMasteringDisplayColourVolume));
    h265enc->mdcv.Header.BufferId = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
    h265enc->mdcv.Header.BufferSz = sizeof (mfxExtMasteringDisplayColourVolume);
    h265enc->mdcv.InsertPayloadToggle = MFX_PAYLOAD_IDR;

    /* According to HEVC spec, display primaries order is G,B,R */
    h265enc->mdcv.DisplayPrimariesX[0] =
        MIN ((mdcv_info.display_primaries[1].x * chroma_den), chroma_den);
    h265enc->mdcv.DisplayPrimariesY[0] =
        MIN ((mdcv_info.display_primaries[1].y * chroma_den), chroma_den);
    h265enc->mdcv.DisplayPrimariesX[1] =
        MIN ((mdcv_info.display_primaries[2].x * chroma_den), chroma_den);
    h265enc->mdcv.DisplayPrimariesY[1] =
        MIN ((mdcv_info.display_primaries[2].y * chroma_den), chroma_den);
    h265enc->mdcv.DisplayPrimariesX[2] =
        MIN ((mdcv_info.display_primaries[0].x * chroma_den), chroma_den);
    h265enc->mdcv.DisplayPrimariesY[2] =
        MIN ((mdcv_info.display_primaries[0].y * chroma_den), chroma_den);

    h265enc->mdcv.WhitePointX =
        MIN ((mdcv_info.white_point.x * chroma_den), chroma_den);
    h265enc->mdcv.WhitePointY =
        MIN ((mdcv_info.white_point.y * chroma_den), chroma_den);

    h265enc->mdcv.MaxDisplayMasteringLuminance =
        mdcv_info.max_display_mastering_luminance * luma_den;
    h265enc->mdcv.MinDisplayMasteringLuminance =
        MIN ((mdcv_info.min_display_mastering_luminance * luma_den),
        h265enc->mdcv.MaxDisplayMasteringLuminance);
    h265enc->have_mdcv = TRUE;
  }

  if (gst_video_content_light_level_from_caps (&cll_info, state->caps)) {
    memset (&h265enc->cll, 0, sizeof (mfxExtContentLightLevelInfo));
    h265enc->cll.Header.BufferId = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
    h265enc->cll.Header.BufferSz = sizeof (mfxExtContentLightLevelInfo);
    h265enc->cll.InsertPayloadToggle = MFX_PAYLOAD_IDR;

    h265enc->cll.MaxContentLightLevel =
        MIN (cll_info.max_content_light_level, 65535);
    h265enc->cll.MaxPicAverageLightLevel =
        MIN (cll_info.max_frame_average_light_level, 65535);
    h265enc->have_cll = TRUE;
  }

  return;
}

static gboolean
gst_msdkh265enc_configure (GstMsdkEnc * encoder)
{
  GstMsdkH265Enc *h265enc = GST_MSDKH265ENC (encoder);
  mfxSession session;
  const mfxPluginUID *uid;

  session = gst_msdk_context_get_session (encoder->context);

  if (encoder->hardware)
    uid = &MFX_PLUGINID_HEVCE_HW;
  else
    uid = &MFX_PLUGINID_HEVCE_SW;

  if (!gst_msdk_load_plugin (session, uid, 1, "msdkh265enc"))
    return FALSE;

  encoder->param.mfx.CodecId = MFX_CODEC_HEVC;

  if (h265enc->profile_name) {
    encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

    if (!g_strcmp0 (h265enc->profile_name, "main-10"))
      encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
    else if (!g_strcmp0 (h265enc->profile_name, "main-still-picture"))
      encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAINSP;
    else if (!g_strcmp0 (h265enc->profile_name, "main-10-still-picture")) {
      encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
      h265enc->ext_param.Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
      h265enc->ext_param.Header.BufferSz = sizeof (h265enc->ext_param);
      h265enc->ext_param.GeneralConstraintFlags =
          MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY;
      gst_msdkenc_add_extra_param (encoder,
          (mfxExtBuffer *) & h265enc->ext_param);
    } else if (!g_strcmp0 (h265enc->profile_name, "main-444") ||
        !g_strcmp0 (h265enc->profile_name, "main-422-10") ||
        !g_strcmp0 (h265enc->profile_name, "main-444-10") ||
        !g_strcmp0 (h265enc->profile_name, "main-12"))
      encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
#if (MFX_VERSION >= 1032)
    else if (!g_strcmp0 (h265enc->profile_name, "screen-extended-main") ||
        !g_strcmp0 (h265enc->profile_name, "screen-extended-main-10") ||
        !g_strcmp0 (h265enc->profile_name, "screen-extended-main-444") ||
        !g_strcmp0 (h265enc->profile_name, "screen-extended-main-444-10"))
      encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_SCC;
#endif
  } else {
    switch (encoder->param.mfx.FrameInfo.FourCC) {
      case MFX_FOURCC_P010:
        encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
        break;
      case MFX_FOURCC_AYUV:
      case MFX_FOURCC_YUY2:
      case MFX_FOURCC_A2RGB10:
#if (MFX_VERSION >= 1027)
      case MFX_FOURCC_Y410:
      case MFX_FOURCC_Y210:
#endif
#if (MFX_VERSION >= 1031)
      case MFX_FOURCC_P016:
#endif
        encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
        break;
      default:
        encoder->param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
    }
  }

  /* IdrInterval field of MediaSDK HEVC encoder behaves differently
   * than other encoders. IdrInteval == 1 indicate every
   * I-frame should be an IDR, IdrInteval == 2 means every other
   * I-frame is an IDR etc. So we generalize the behaviour of property
   * "i-frames" by incrementing the value by one in each case*/
  encoder->param.mfx.IdrInterval += 1;

  encoder->option2.MaxSliceSize = h265enc->max_slice_size;
  encoder->option2.MinQPI = h265enc->min_qp_i;
  encoder->option2.MinQPP = h265enc->min_qp_p;
  encoder->option2.MinQPB = h265enc->min_qp_b;
  encoder->option2.MaxQPI = h265enc->max_qp_i;
  encoder->option2.MaxQPP = h265enc->max_qp_p;
  encoder->option2.MaxQPB = h265enc->max_qp_b;
  encoder->option2.DisableDeblockingIdc = h265enc->dblk_idc;

  if (h265enc->tune_mode == 16 || h265enc->lowpower) {
    encoder->option2.IntRefType = h265enc->intra_refresh_type;
    encoder->option2.IntRefCycleSize = h265enc->intra_refresh_cycle_size;
    encoder->option2.IntRefQPDelta = h265enc->intra_refresh_qp_delta;
    encoder->option3.IntRefCycleDist = h265enc->intra_refresh_cycle_dist;
    encoder->enable_extopt3 = TRUE;
  } else if (h265enc->intra_refresh_type || h265enc->intra_refresh_cycle_size
      || h265enc->intra_refresh_qp_delta || h265enc->intra_refresh_cycle_dist) {
    GST_WARNING_OBJECT (h265enc,
        "Intra refresh is only supported under lowpower mode, ingoring...");
  }
#if (MFX_VERSION >= 1026)
  if (h265enc->transform_skip != MFX_CODINGOPTION_UNKNOWN) {
    encoder->option3.TransformSkip = h265enc->transform_skip;
    encoder->enable_extopt3 = TRUE;
  }
#endif

  if (h265enc->b_pyramid) {
    encoder->option2.BRefType = MFX_B_REF_PYRAMID;
    /* Don't define Gop structure for B-pyramid, otherwise EncodeInit
     * will throw Invalid param error */
    encoder->param.mfx.GopRefDist = 0;
  }

  if (h265enc->p_pyramid) {
    encoder->option3.PRefType = MFX_P_REF_PYRAMID;
    /* MFX_P_REF_PYRAMID is available for GopRefDist = 1 */
    encoder->param.mfx.GopRefDist = 1;
    /* SDK decides the DPB size for P pyramid */
    encoder->param.mfx.NumRefFrame = 0;
    encoder->enable_extopt3 = TRUE;
  }

  /* Fill Extended coding options */
  h265enc->option.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
  h265enc->option.Header.BufferSz = sizeof (h265enc->option);
  h265enc->option.PicTimingSEI =
      (h265enc->pic_timing_sei ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);

  if (encoder->option3.LowDelayBRC == MFX_CODINGOPTION_ON)
    h265enc->option.NalHrdConformance = MFX_CODINGOPTION_OFF;

  gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & h265enc->option);

  gst_msdkenc_ensure_extended_coding_options (encoder);

  if (h265enc->num_tile_rows > 1 || h265enc->num_tile_cols > 1) {
    h265enc->ext_tiles.Header.BufferId = MFX_EXTBUFF_HEVC_TILES;
    h265enc->ext_tiles.Header.BufferSz = sizeof (h265enc->ext_tiles);
    h265enc->ext_tiles.NumTileRows = h265enc->num_tile_rows;
    h265enc->ext_tiles.NumTileColumns = h265enc->num_tile_cols;

    gst_msdkenc_add_extra_param (encoder,
        (mfxExtBuffer *) & h265enc->ext_tiles);

    /* Set a valid value to NumSlice */
    if (encoder->param.mfx.NumSlice == 0)
      encoder->param.mfx.NumSlice =
          h265enc->num_tile_rows * h265enc->num_tile_cols;
  }

  encoder->param.mfx.LowPower = h265enc->tune_mode;

  /* Set HDR SEI */
  _get_hdr_sei (encoder);
  if (h265enc->have_mdcv)
    gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & h265enc->mdcv);
  if (h265enc->have_cll)
    gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & h265enc->cll);

  return TRUE;
}

static inline const gchar *
level_to_string (gint level)
{
  switch (level) {
    case MFX_LEVEL_HEVC_1:
      return "1";
    case MFX_LEVEL_HEVC_2:
      return "2";
    case MFX_LEVEL_HEVC_21:
      return "2.1";
    case MFX_LEVEL_HEVC_3:
      return "3";
    case MFX_LEVEL_HEVC_31:
      return "3.1";
    case MFX_LEVEL_HEVC_4:
      return "4";
    case MFX_LEVEL_HEVC_41:
      return "4.1";
    case MFX_LEVEL_HEVC_5:
      return "5";
    case MFX_LEVEL_HEVC_51:
      return "5.1";
    case MFX_LEVEL_HEVC_52:
      return "5.2";
    case MFX_LEVEL_HEVC_6:
      return "6";
    case MFX_LEVEL_HEVC_61:
      return "6.1";
    case MFX_LEVEL_HEVC_62:
      return "6.2";
    default:
      break;
  }

  return NULL;
}

static GstCaps *
gst_msdkh265enc_set_src_caps (GstMsdkEnc * encoder)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (encoder);
  GstCaps *caps;
  GstStructure *structure;
  const gchar *level;

  caps = gst_caps_new_empty_simple ("video/x-h265");
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_set (structure, "stream-format", G_TYPE_STRING, "byte-stream",
      NULL);

  gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);

  if (thiz->profile_name)
    gst_structure_set (structure, "profile", G_TYPE_STRING, thiz->profile_name,
        NULL);
  else {
    switch (encoder->param.mfx.FrameInfo.FourCC) {
      case MFX_FOURCC_P010:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-10",
            NULL);
        break;
      case MFX_FOURCC_AYUV:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-444",
            NULL);
        break;
      case MFX_FOURCC_YUY2:
        /* The profile is main-422-10 for 8-bit 422 */
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-422-10",
            NULL);
        break;
      case MFX_FOURCC_A2RGB10:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-444-10",
            NULL);
        break;
#if (MFX_VERSION >= 1027)
      case MFX_FOURCC_Y410:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-444-10",
            NULL);
        break;
      case MFX_FOURCC_Y210:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-422-10",
            NULL);
        break;
#endif
#if (MFX_VERSION >= 1031)
      case MFX_FOURCC_P016:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main-12",
            NULL);
        break;
#endif
      default:
        gst_structure_set (structure, "profile", G_TYPE_STRING, "main", NULL);
        break;
    }
  }

  level = level_to_string (encoder->param.mfx.CodecLevel);
  if (level)
    gst_structure_set (structure, "level", G_TYPE_STRING, level, NULL);

  return caps;
}

static void
gst_msdkh265enc_finalize (GObject * object)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (object);

  if (thiz->parser)
    gst_h265_parser_free (thiz->parser);
  if (thiz->cc_sei_array)
    g_array_unref (thiz->cc_sei_array);

  g_free (thiz->profile_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_msdkh265enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkEnc *enc = GST_MSDKENC (object);
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (object);

  if (gst_msdkenc_set_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);

  switch (prop_id) {
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

    case PROP_TILE_ROW:
      thiz->num_tile_rows = g_value_get_uint (value);
      break;

    case PROP_TILE_COL:
      thiz->num_tile_cols = g_value_get_uint (value);
      break;

    case PROP_MAX_SLICE_SIZE:
      thiz->max_slice_size = g_value_get_uint (value);
      break;

    case PROP_TUNE_MODE:
      thiz->tune_mode = g_value_get_enum (value);
      thiz->prop_flag |= GST_MSDK_FLAG_TUNE_MODE;
      break;

    case PROP_TRANSFORM_SKIP:
      thiz->transform_skip = g_value_get_enum (value);
      break;

    case PROP_B_PYRAMID:
      thiz->b_pyramid = g_value_get_boolean (value);
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
}

static void
gst_msdkh265enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (object);

  if (gst_msdkenc_get_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);
  switch (prop_id) {
#ifndef GST_REMOVE_DEPRECATED
    case PROP_LOW_POWER:
      g_value_set_boolean (value, thiz->lowpower);
      break;
#endif

    case PROP_TILE_ROW:
      g_value_set_uint (value, thiz->num_tile_rows);
      break;

    case PROP_TILE_COL:
      g_value_set_uint (value, thiz->num_tile_cols);
      break;

    case PROP_MAX_SLICE_SIZE:
      g_value_set_uint (value, thiz->max_slice_size);
      break;

    case PROP_TUNE_MODE:
      g_value_set_enum (value, thiz->tune_mode);
      break;

    case PROP_TRANSFORM_SKIP:
      g_value_set_enum (value, thiz->transform_skip);
      break;

    case PROP_B_PYRAMID:
      g_value_set_boolean (value, thiz->b_pyramid);
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
gst_msdkh265enc_need_reconfig (GstMsdkEnc * encoder, GstVideoCodecFrame * frame)
{
  GstMsdkH265Enc *h265enc = GST_MSDKH265ENC (encoder);

  return gst_msdkenc_get_roi_params (encoder, frame, h265enc->roi);
}

static void
gst_msdkh265enc_set_extra_params (GstMsdkEnc * encoder,
    GstVideoCodecFrame * frame)
{
  GstMsdkH265Enc *h265enc = GST_MSDKH265ENC (encoder);

  if (h265enc->roi[0].NumROI)
    gst_msdkenc_add_extra_param (encoder, (mfxExtBuffer *) & h265enc->roi[0]);
}

static gboolean
gst_msdkh265enc_is_format_supported (GstMsdkEnc * encoder,
    GstVideoFormat format)
{
  GstMsdkH265Enc *h265enc = GST_MSDKH265ENC (encoder);

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_Y212_LE:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_P010_10LE:
#if (MFX_VERSION >= 1027)
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_Y210:
#endif
#if (MFX_VERSION >= 1031)
    case GST_VIDEO_FORMAT_P012_LE:
#endif
      return TRUE;
    case GST_VIDEO_FORMAT_YUY2:
#if (MFX_VERSION >= 1027)
      if (encoder->codename >= MFX_PLATFORM_ICELAKE &&
          h265enc->tune_mode == MFX_CODINGOPTION_OFF)
        return TRUE;
#endif
    default:
      return FALSE;
  }
}

static void
_msdkh265enc_install_properties (GObjectClass * gobject_class,
    GstMsdkEncClass * encoder_class)
{
  gst_msdkenc_install_common_properties (encoder_class);

#ifndef GST_REMOVE_DEPRECATED
  g_object_class_install_property (gobject_class, PROP_LOW_POWER,
      g_param_spec_boolean ("low-power", "Low power",
          "Enable low power mode (DEPRECATED, use tune instead)",
          PROP_LOWPOWER_DEFAULT,
          G_PARAM_DEPRECATED | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_TILE_ROW,
      g_param_spec_uint ("num-tile-rows", "number of rows for tiled encoding",
          "number of rows for tiled encoding",
          1, 8192, PROP_TILE_ROW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_COL,
      g_param_spec_uint ("num-tile-cols",
          "number of columns for tiled encoding",
          "number of columns for tiled encoding", 1, 8192,
          PROP_TILE_COL_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_SLICE_SIZE,
      g_param_spec_uint ("max-slice-size", "Max Slice Size",
          "Maximum slice size in bytes (if enabled MSDK will ignore the control over num-slices)",
          0, G_MAXUINT32, PROP_MAX_SLICE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TUNE_MODE,
      g_param_spec_enum ("tune", "Encoder tuning",
          "Encoder tuning option",
          gst_msdkenc_tune_mode_get_type (), PROP_TUNE_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TRANSFORM_SKIP,
      g_param_spec_enum ("transform-skip", "Transform Skip",
          "Transform Skip option",
          gst_msdkenc_transform_skip_get_type (), PROP_TRANSFORM_SKIP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_B_PYRAMID,
      g_param_spec_boolean ("b-pyramid", "B-pyramid",
          "Enable B-Pyramid Reference structure", FALSE,
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
   * GstMsdkH265Enc:min-qp-i:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MIN_QP_I,
      g_param_spec_uint ("min-qp-i", "Min QP I",
          "Minimal quantizer scale for I frame",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH265Enc:min-qp-p:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MIN_QP_P,
      g_param_spec_uint ("min-qp-p", "Min QP P",
          "Minimal quantizer scale for P frame",
          0, 51, PROP_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH265Enc:min-qp-b:
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
   * GstMsdkH265Enc:max-qp-i:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_QP_I,
      g_param_spec_uint ("max-qp-i", "Max QP I",
          "Maximum quantizer scale for I frame",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH265Enc:max-qp-p:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_QP_P,
      g_param_spec_uint ("max-qp-p", "Max QP P",
          "Maximum quantizer scale for P frame",
          0, 51, PROP_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH265Enc:max-qp-b:
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
          "Set intra refresh cycle size, valid value starts from 2, only available when tune=low-power",
          0, G_MAXUINT16, PROP_INTRA_REFRESH_CYCLE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_QP_DELTA,
      g_param_spec_int ("intra-refresh-qp-delta", "Intra refresh qp delta",
          "Set intra refresh qp delta, only available when tune=low-power",
          -51, 51, PROP_INTRA_REFRESH_QP_DELTA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_CYCLE_DIST,
      g_param_spec_uint ("intra-refresh-cycle-dist", "Intra refresh cycle dist",
          "Set intra refresh cycle dist, only available when tune=low-power",
          0, G_MAXUINT16, PROP_INTRA_REFRESH_CYCLE_DIST_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DBLK_IDC,
      g_param_spec_uint ("dblk-idc", "Disable Deblocking Idc",
          "Option of disable deblocking idc",
          0, 2, PROP_DBLK_IDC_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMsdkH265Enc:pic-timing-sei:
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
gst_msdkh265enc_class_init (gpointer klass, gpointer data)
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

  gobject_class->finalize = gst_msdkh265enc_finalize;
  gobject_class->set_property = gst_msdkh265enc_set_property;
  gobject_class->get_property = gst_msdkh265enc_get_property;

  videoencoder_class->pre_push = gst_msdkh265enc_pre_push;

  encoder_class->set_format = gst_msdkh265enc_set_format;
  encoder_class->configure = gst_msdkh265enc_configure;
  encoder_class->set_src_caps = gst_msdkh265enc_set_src_caps;
  encoder_class->need_reconfig = gst_msdkh265enc_need_reconfig;
  encoder_class->set_extra_params = gst_msdkh265enc_set_extra_params;
  encoder_class->is_format_supported = gst_msdkh265enc_is_format_supported;

  _msdkh265enc_install_properties (gobject_class, encoder_class);

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK H265 encoder",
      "Codec/Encoder/Video/Hardware",
      "H265 video encoder based on " MFX_API_SDK,
      "Josep Torra <jtorra@oblong.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkh265enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstMsdkH265Enc *thiz = GST_MSDKH265ENC (instance);
  GstMsdkEnc *msdk_enc = (GstMsdkEnc *) instance;
  thiz->lowpower = PROP_LOWPOWER_DEFAULT;
  thiz->num_tile_rows = PROP_TILE_ROW_DEFAULT;
  thiz->num_tile_cols = PROP_TILE_COL_DEFAULT;
  thiz->max_slice_size = PROP_MAX_SLICE_SIZE_DEFAULT;
  thiz->tune_mode = PROP_TUNE_MODE_DEFAULT;
  thiz->transform_skip = PROP_TRANSFORM_SKIP_DEFAULT;
  thiz->b_pyramid = PROP_B_PYRAMID_DEFAULT;
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
  msdk_enc->num_extra_frames = 1;
}

gboolean
gst_msdkh265enc_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkEncCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkH265EncClass),
    .class_init = gst_msdkh265enc_class_init,
    .instance_size = sizeof (GstMsdkH265Enc),
    .instance_init = gst_msdkh265enc_init
  };

  cdata = g_new (MsdkEncCData, 1);
  cdata->sink_caps = gst_caps_copy (sink_caps);
  cdata->src_caps = gst_caps_copy (src_caps);

#ifdef _WIN32
  gst_msdkcaps_set_strings (cdata->sink_caps,
      "memory:D3D11Memory", "format", "NV12, P010_10LE");
#endif

  gst_caps_set_simple (cdata->src_caps,
      "alignment", G_TYPE_STRING, "au",
      "stream-format", G_TYPE_STRING, "byte-stream", NULL);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkH265Enc");
  feature_name = g_strdup ("msdkh265enc");

  type = g_type_register_static (GST_TYPE_MSDKENC, type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
