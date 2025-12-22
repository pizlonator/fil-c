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

/* TODO:
 *  - Add support for interlaced content
 *  - Add support for MVC AVC
 *  - Wrap more configuration options and maybe move properties to derived
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#ifdef _WIN32
#  include <malloc.h>
#endif

#include <stdlib.h>

#include "gstmsdkenc.h"
#include "gstmsdkcontextutil.h"
#include "gstmsdkallocator.h"
#include "mfxjpeg.h"
#include "gstmsdkcaps.h"

#ifndef _WIN32
#include <libdrm/drm_fourcc.h>
#include "gstmsdkallocator_libva.h"
#include <gst/va/gstvaallocator.h>
#else
#include <gst/d3d11/gstd3d11.h>
#endif

static inline void *
_aligned_alloc (size_t alignment, size_t size)
{
#ifdef _WIN32
  return _aligned_malloc (size, alignment);
#else
  void *out;
  if (posix_memalign (&out, alignment, size) != 0)
    out = NULL;
  return out;
#endif
}

#ifndef _WIN32
#define _aligned_free free
#endif

static void gst_msdkenc_close_encoder (GstMsdkEnc * thiz);

GST_DEBUG_CATEGORY_EXTERN (gst_msdkenc_debug);
#define GST_CAT_DEFAULT gst_msdkenc_debug

#define PROP_HARDWARE_DEFAULT            TRUE
#define PROP_ASYNC_DEPTH_DEFAULT         4
#define PROP_TARGET_USAGE_DEFAULT        (MFX_TARGETUSAGE_BALANCED)
#define PROP_RATE_CONTROL_DEFAULT        (MFX_RATECONTROL_VBR)
#define PROP_BITRATE_DEFAULT             (2 * 1024)
#define PROP_QPI_DEFAULT                 0
#define PROP_QPP_DEFAULT                 0
#define PROP_QPB_DEFAULT                 0
#define PROP_GOP_SIZE_DEFAULT            0
#define PROP_REF_FRAMES_DEFAULT          0
#define PROP_I_FRAMES_DEFAULT            0
#define PROP_B_FRAMES_DEFAULT            -1
#define PROP_NUM_SLICES_DEFAULT          0
#define PROP_AVBR_ACCURACY_DEFAULT       0
#define PROP_AVBR_CONVERGENCE_DEFAULT    0
#define PROP_RC_LOOKAHEAD_DEPTH_DEFAULT  10
#define PROP_MAX_VBV_BITRATE_DEFAULT     0
#define PROP_MAX_FRAME_SIZE_DEFAULT      0
#define PROP_MAX_FRAME_SIZE_I_DEFAULT    0
#define PROP_MAX_FRAME_SIZE_P_DEFAULT    0
#define PROP_MBBRC_DEFAULT               MFX_CODINGOPTION_OFF
#define PROP_LOWDELAY_BRC_DEFAULT        MFX_CODINGOPTION_OFF
#define PROP_ADAPTIVE_I_DEFAULT          MFX_CODINGOPTION_UNKNOWN
#define PROP_ADAPTIVE_B_DEFAULT          MFX_CODINGOPTION_UNKNOWN

/* External coding properties */
#define EC_PROPS_STRUCT_NAME             "props"
#define EC_PROPS_EXTBRC                  "extbrc"

#define gst_msdkenc_parent_class parent_class
G_DEFINE_TYPE (GstMsdkEnc, gst_msdkenc, GST_TYPE_VIDEO_ENCODER);

void
gst_msdkenc_add_extra_param (GstMsdkEnc * thiz, mfxExtBuffer * param)
{
  if (thiz->num_extra_params < MAX_EXTRA_PARAMS) {
    thiz->extra_params[thiz->num_extra_params] = param;
    thiz->num_extra_params++;
  }
}

static void
gst_msdkenc_set_context (GstElement * element, GstContext * context)
{
  GstMsdkContext *msdk_context = NULL;
  GstMsdkEnc *thiz = GST_MSDKENC (element);

  if (gst_msdk_context_get_context (context, &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  } else
#ifndef _WIN32
    if (gst_msdk_context_from_external_va_display (context,
          thiz->hardware, 0 /* GST_MSDK_JOB_ENCODER will be set later */ ,
          &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  }
#else
    if (gst_msdk_context_from_external_d3d11_device (context,
          thiz->hardware, 0 /* GST_MSDK_JOB_ENCODER will be set later */ ,
          &msdk_context)) {
    gst_object_replace ((GstObject **) & thiz->context,
        (GstObject *) msdk_context);
    gst_object_unref (msdk_context);
  }
#endif

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static void
ensure_bitrate_control (GstMsdkEnc * thiz)
{
  mfxInfoMFX *mfx = &thiz->param.mfx;
  mfxExtCodingOption2 *option2 = &thiz->option2;
  mfxExtCodingOption3 *option3 = &thiz->option3;

  GST_DEBUG_OBJECT (thiz, "set target bitrate: %u kbit/sec", thiz->bitrate);

  mfx->RateControlMethod = thiz->rate_control;
  /* No effect in CQP variant algorithms */
  if ((mfx->RateControlMethod != MFX_RATECONTROL_CQP) &&
      (thiz->bitrate > G_MAXUINT16 || thiz->max_vbv_bitrate > G_MAXUINT16)) {
    mfxU32 max_val = MAX (thiz->max_vbv_bitrate, thiz->bitrate);

    mfx->BRCParamMultiplier = (mfxU16) ((max_val + 0x10000) / 0x10000);
    mfx->TargetKbps = (mfxU16) (thiz->bitrate / mfx->BRCParamMultiplier);
    mfx->MaxKbps = (mfxU16) (thiz->max_vbv_bitrate / mfx->BRCParamMultiplier);
    mfx->BufferSizeInKB =
        (mfxU16) (mfx->BufferSizeInKB / mfx->BRCParamMultiplier);
    /* Currently InitialDelayInKB is not used in this plugin */
    mfx->InitialDelayInKB =
        (mfxU16) (mfx->InitialDelayInKB / mfx->BRCParamMultiplier);
  } else {
    mfx->TargetKbps = thiz->bitrate;
    mfx->MaxKbps = thiz->max_vbv_bitrate;
    mfx->BRCParamMultiplier = 1;
  }

  switch (mfx->RateControlMethod) {
    case MFX_RATECONTROL_CQP:
      mfx->QPI = thiz->qpi;
      mfx->QPP = thiz->qpp;
      mfx->QPB = thiz->qpb;
      break;

    case MFX_RATECONTROL_LA_ICQ:
      option2->LookAheadDepth = thiz->lookahead_depth;
    case MFX_RATECONTROL_ICQ:
      mfx->ICQQuality = CLAMP (thiz->qpi, 1, 51);
      break;

    case MFX_RATECONTROL_LA:   /* VBR with LA. Only supported in H264?? */
    case MFX_RATECONTROL_LA_HRD:       /* VBR with LA, HRD compliant */
      option2->LookAheadDepth = thiz->lookahead_depth;
      break;

    case MFX_RATECONTROL_QVBR:
      option3->QVBRQuality = CLAMP (thiz->qpi, 1, 51);
      thiz->enable_extopt3 = TRUE;
      break;

    case MFX_RATECONTROL_AVBR:
      mfx->Accuracy = thiz->accuracy;
      mfx->Convergence = thiz->convergence;
      break;

    case MFX_RATECONTROL_VBR:
      thiz->enable_extopt3 = TRUE;
      option2->MaxFrameSize = thiz->max_frame_size * 1000;
      if (thiz->max_frame_size_i > 0)
        option3->MaxFrameSizeI = thiz->max_frame_size_i * 1000;
      if (thiz->max_frame_size_p > 0)
        option3->MaxFrameSizeP = thiz->max_frame_size_p * 1000;
      if (thiz->lowdelay_brc != MFX_CODINGOPTION_UNKNOWN) {
        option3->LowDelayBRC = thiz->lowdelay_brc;
      }
      break;

    case MFX_RATECONTROL_VCM:
      /*Non HRD compliant mode with no B-frame and interlaced support */
      thiz->param.mfx.GopRefDist = 0;
      break;

    case MFX_RATECONTROL_CBR:
      break;

    default:
      GST_ERROR ("Unsupported RateControl!");
      break;
  }
}

static gint16
coding_option_get_value (const gchar * key, const gchar * nickname)
{
  if (!g_strcmp0 (nickname, "on")) {
    return MFX_CODINGOPTION_ON;
  } else if (!g_strcmp0 (nickname, "off")) {
    return MFX_CODINGOPTION_OFF;
  } else if (!g_strcmp0 (nickname, "auto")) {
    return MFX_CODINGOPTION_UNKNOWN;
  }

  GST_ERROR ("\"%s\" illegal option \"%s\", set to \"off\"", key, nickname);

  return MFX_CODINGOPTION_OFF;
}

static gboolean
structure_transform (const GstStructure * src, GstStructure * dst)
{
  guint len;
  GValue dst_value = G_VALUE_INIT;
  gboolean ret = TRUE;

  g_return_val_if_fail (src != NULL, FALSE);
  g_return_val_if_fail (dst != NULL, FALSE);

  len = gst_structure_n_fields (src);

  for (guint i = 0; i < len; i++) {
    const gchar *key = gst_structure_nth_field_name (src, i);
    const GValue *src_value = gst_structure_get_value (src, key);

    if (!gst_structure_has_field (dst, key)) {
      GST_ERROR ("structure \"%s\" does not support \"%s\"",
          gst_structure_get_name (dst), key);
      ret = FALSE;
      continue;
    }

    g_value_init (&dst_value, gst_structure_get_field_type (dst, key));

    if (g_value_transform (src_value, &dst_value)) {
      gst_structure_set_value (dst, key, &dst_value);
    } else {
      GST_ERROR ("\"%s\" transform %s to %s failed", key,
          G_VALUE_TYPE_NAME (src_value), G_VALUE_TYPE_NAME (&dst_value));
      ret = FALSE;
    }

    g_value_unset (&dst_value);
  }

  return ret;
}

/* Supported types: gchar*, gboolean, gint, guint, gfloat, gdouble */
static gboolean
structure_get_value (const GstStructure * s, const gchar * key, gpointer value)
{
  const GValue *gvalue = gst_structure_get_value (s, key);
  if (!gvalue) {
    GST_ERROR ("structure \"%s\" does not support \"%s\"",
        gst_structure_get_name (s), key);
    return FALSE;
  }

  switch (G_VALUE_TYPE (gvalue)) {
    case G_TYPE_STRING:{
      const gchar **val = (const gchar **) value;
      *val = g_value_get_string (gvalue);
      break;
    }
    case G_TYPE_BOOLEAN:{
      gboolean *val = (gboolean *) value;
      *val = g_value_get_boolean (gvalue);
      break;
    }
    case G_TYPE_INT:{
      gint *val = (gint *) value;
      *val = g_value_get_int (gvalue);
      break;
    }
    case G_TYPE_UINT:{
      guint *val = (guint *) value;
      *val = g_value_get_uint (gvalue);
      break;
    }
    case G_TYPE_FLOAT:{
      gfloat *val = (gfloat *) value;
      *val = g_value_get_float (gvalue);
      break;
    }
    case G_TYPE_DOUBLE:{
      gdouble *val = (gdouble *) value;
      *val = g_value_get_double (gvalue);
      break;
    }
    default:
      GST_ERROR ("\"%s\" unsupported type %s", key, G_VALUE_TYPE_NAME (gvalue));
      return FALSE;
  }

  return TRUE;
}

static gboolean
ext_coding_props_get_value (GstMsdkEnc * thiz,
    const gchar * key, gpointer value)
{
  gboolean ret;
  if (!(ret = structure_get_value (thiz->ext_coding_props, key, value))) {
    GST_ERROR_OBJECT (thiz, "structure \"%s\" failed to get value for \"%s\"",
        gst_structure_get_name (thiz->ext_coding_props), key);
  }

  return ret;
}

void
gst_msdkenc_ensure_extended_coding_options (GstMsdkEnc * thiz)
{
  mfxExtCodingOption2 *option2 = &thiz->option2;
  mfxExtCodingOption3 *option3 = &thiz->option3;

  gchar *extbrc;
  ext_coding_props_get_value (thiz, EC_PROPS_EXTBRC, &extbrc);

  /* Fill ExtendedCodingOption2, set non-zero defaults too */
  option2->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
  option2->Header.BufferSz = sizeof (thiz->option2);
  option2->MBBRC = thiz->mbbrc;
  option2->ExtBRC = coding_option_get_value (EC_PROPS_EXTBRC, extbrc);
  option2->AdaptiveI = thiz->adaptive_i;
  option2->AdaptiveB = thiz->adaptive_b;
  option2->BitrateLimit = MFX_CODINGOPTION_OFF;
  option2->EnableMAD = MFX_CODINGOPTION_OFF;
  option2->UseRawRef = MFX_CODINGOPTION_OFF;
  gst_msdkenc_add_extra_param (thiz, (mfxExtBuffer *) option2);

  if (thiz->enable_extopt3) {
    option3->Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    option3->Header.BufferSz = sizeof (thiz->option3);
    gst_msdkenc_add_extra_param (thiz, (mfxExtBuffer *) option3);
  }
}

/* Return TRUE if ROI is changed and update ROI parameters in encoder_roi */
gboolean
gst_msdkenc_get_roi_params (GstMsdkEnc * thiz,
    GstVideoCodecFrame * frame, mfxExtEncoderROI * encoder_roi)
{
  GstBuffer *input;
  guint num_roi, i, num_valid_roi = 0;
  gushort roi_mode = G_MAXUINT16;
  gpointer state = NULL;
  mfxExtEncoderROI *curr_roi = encoder_roi;
  mfxExtEncoderROI *prev_roi = encoder_roi + 1;

  if (!frame || !frame->input_buffer)
    return FALSE;

  memset (curr_roi, 0, sizeof (mfxExtEncoderROI));
  input = frame->input_buffer;

  num_roi =
      gst_buffer_get_n_meta (input, GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE);

  if (num_roi == 0)
    goto end;

  curr_roi->Header.BufferId = MFX_EXTBUFF_ENCODER_ROI;
  curr_roi->Header.BufferSz = sizeof (mfxExtEncoderROI);

  for (i = 0; i < num_roi && num_valid_roi < 256; i++) {
    GstVideoRegionOfInterestMeta *roi;
    GstStructure *s;

    roi = (GstVideoRegionOfInterestMeta *)
        gst_buffer_iterate_meta_filtered (input, &state,
        GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE);

    if (!roi)
      continue;

    /* ignore roi if overflow */
    if ((roi->x > G_MAXINT16) || (roi->y > G_MAXINT16)
        || (roi->w > G_MAXUINT16) || (roi->h > G_MAXUINT16)) {
      GST_DEBUG_OBJECT (thiz, "Ignoring ROI... ROI overflow");
      continue;
    }

    GST_LOG ("Input buffer ROI: type=%s id=%d (%d, %d) %dx%d",
        g_quark_to_string (roi->roi_type), roi->id, roi->x, roi->y, roi->w,
        roi->h);

    curr_roi->ROI[num_valid_roi].Left = roi->x;
    curr_roi->ROI[num_valid_roi].Top = roi->y;
    curr_roi->ROI[num_valid_roi].Right = roi->x + roi->w;
    curr_roi->ROI[num_valid_roi].Bottom = roi->y + roi->h;

    s = gst_video_region_of_interest_meta_get_param (roi, "roi/msdk");

    if (s) {
      int value = 0;

      if (roi_mode == G_MAXUINT16) {
        if (gst_structure_get_int (s, "delta-qp", &value)) {
#if (MFX_VERSION >= 1022)
          roi_mode = MFX_ROI_MODE_QP_DELTA;
          curr_roi->ROI[num_valid_roi].DeltaQP = CLAMP (value, -51, 51);
          GST_LOG ("Use delta-qp %d", value);
#else
          GST_WARNING
              ("Ignore delta QP because the MFX doesn't support delta QP mode");
#endif
        } else if (gst_structure_get_int (s, "priority", &value)) {
          roi_mode = MFX_ROI_MODE_PRIORITY;
          curr_roi->ROI[num_valid_roi].Priority = CLAMP (value, -3, 3);
          GST_LOG ("Use priority %d", value);
        } else
          continue;
#if (MFX_VERSION >= 1022)
      } else if (roi_mode == MFX_ROI_MODE_QP_DELTA &&
          gst_structure_get_int (s, "delta-qp", &value)) {
        curr_roi->ROI[num_valid_roi].DeltaQP = CLAMP (value, -51, 51);
#endif
      } else if (roi_mode == MFX_ROI_MODE_PRIORITY &&
          gst_structure_get_int (s, "priority", &value)) {
        curr_roi->ROI[num_valid_roi].Priority = CLAMP (value, -3, 3);
      } else
        continue;

      num_valid_roi++;
    }
  }

#if (MFX_VERSION >= 1022)
  curr_roi->ROIMode = roi_mode;
#endif

  curr_roi->NumROI = num_valid_roi;

end:
  if (curr_roi->NumROI == 0 && prev_roi->NumROI == 0)
    return FALSE;

  if (curr_roi->NumROI != prev_roi->NumROI ||
      memcmp (curr_roi, prev_roi, sizeof (mfxExtEncoderROI)) != 0) {
    *prev_roi = *curr_roi;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_msdkenc_init_encoder (GstMsdkEnc * thiz)
{
  GstMsdkEncClass *klass = GST_MSDKENC_GET_CLASS (thiz);
  GstVideoInfo info;
  mfxSession session;
  mfxStatus status;
  mfxFrameAllocRequest request;
  guint i;
  mfxExtVideoSignalInfo ext_vsi;

  if (thiz->initialized) {
    GST_DEBUG_OBJECT (thiz, "Already initialized");
    return TRUE;
  }

  if (!thiz->context) {
    GST_WARNING_OBJECT (thiz, "No MSDK Context");
    return FALSE;
  }

  if (!thiz->input_state) {
    GST_DEBUG_OBJECT (thiz, "Have no input state yet");
    return FALSE;
  }

  if (!gst_msdkcaps_video_info_from_caps (thiz->input_state->caps, &info,
          &thiz->modifier)) {
    GST_WARNING_OBJECT (thiz, "Failed to get video info from input caps");
    return FALSE;
  }

  GST_OBJECT_LOCK (thiz);
  session = gst_msdk_context_get_session (thiz->context);
  thiz->codename = msdk_get_platform_codename (session);

  if (thiz->use_video_memory)
    gst_msdk_set_frame_allocator (thiz->context);

#if (MFX_VERSION < 2000)
  /* check the format for MSDK path */
  if (!klass->is_format_supported (thiz, GST_VIDEO_INFO_FORMAT (&info))) {
    GST_ERROR_OBJECT (thiz,
        "internal vpp is no longer supported, "
        "please use msdkvpp plugin to do conversion first");
    goto failed;
  }
#endif

  thiz->param.AsyncDepth = thiz->async_depth;
  if (thiz->use_video_memory)
    thiz->param.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
  else
    thiz->param.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

  thiz->param.mfx.TargetUsage = thiz->target_usage;
  thiz->param.mfx.GopPicSize = thiz->gop_size;
  thiz->param.mfx.GopRefDist = thiz->b_frames + 1;
  thiz->param.mfx.IdrInterval = thiz->i_frames;
  thiz->param.mfx.NumSlice = thiz->num_slices;
  thiz->param.mfx.NumRefFrame = thiz->ref_frames;
  thiz->param.mfx.EncodedOrder = 0;     /* Take input frames in display order */

  thiz->param.mfx.FrameInfo.Width = GST_ROUND_UP_16 (info.width);
  thiz->param.mfx.FrameInfo.Height = GST_ROUND_UP_32 (info.height);
  thiz->param.mfx.FrameInfo.CropW = info.width;
  thiz->param.mfx.FrameInfo.CropH = info.height;
  thiz->param.mfx.FrameInfo.FrameRateExtN = info.fps_n;
  thiz->param.mfx.FrameInfo.FrameRateExtD = info.fps_d;
  thiz->param.mfx.FrameInfo.AspectRatioW = info.par_n;
  thiz->param.mfx.FrameInfo.AspectRatioH = info.par_d;
  thiz->param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

  /* work-around to avoid zero fps in msdk structure */
  if (0 == thiz->param.mfx.FrameInfo.FrameRateExtN)
    thiz->param.mfx.FrameInfo.FrameRateExtN = 30;

  thiz->frame_duration =
      gst_util_uint64_scale (GST_SECOND,
      thiz->param.mfx.FrameInfo.FrameRateExtD,
      thiz->param.mfx.FrameInfo.FrameRateExtN);

  switch (GST_VIDEO_INFO_FORMAT (&info)) {
    case GST_VIDEO_FORMAT_P010_10LE:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 10;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 10;
      thiz->param.mfx.FrameInfo.Shift = 1;
      break;
    case GST_VIDEO_FORMAT_VUYA:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 8;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 8;
      break;
#if (MFX_VERSION >= 1027)
    case GST_VIDEO_FORMAT_Y410:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 10;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 10;
      break;
    case GST_VIDEO_FORMAT_Y210:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 10;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 10;
      thiz->param.mfx.FrameInfo.Shift = 1;
      break;
#endif
    case GST_VIDEO_FORMAT_BGRA:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 8;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 8;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_A2RGB10;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 10;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 10;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 8;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 8;
      break;
#if (MFX_VERSION >= 1031)
    case GST_VIDEO_FORMAT_P012_LE:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
      thiz->param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 12;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 12;
      thiz->param.mfx.FrameInfo.Shift = 1;
      break;
#endif
    default:
      thiz->param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
      thiz->param.mfx.FrameInfo.BitDepthLuma = 8;
      thiz->param.mfx.FrameInfo.BitDepthChroma = 8;
  }

  /* work-around to avoid zero fps in msdk structure */
  if (0 == thiz->param.mfx.FrameInfo.FrameRateExtN)
    thiz->param.mfx.FrameInfo.FrameRateExtN = 30;

  /* ensure bitrate control parameters */
  ensure_bitrate_control (thiz);

  /* allow subclass configure further */
  if (klass->configure) {
    if (!klass->configure (thiz))
      goto failed;
  }

  /* If color properties are available from upstream, set it and pass to MediaSDK here.
   * MJPEG and VP9 are excluded as MediaSDK does not support to handle video param
   * extbuff with buffer id equals to MFX_EXTBUFF_VIDEO_SIGNAL_INFO.
   */
  if (thiz->param.mfx.CodecId != MFX_CODEC_JPEG &&
      thiz->param.mfx.CodecId != MFX_CODEC_VP9 &&
      (info.colorimetry.primaries || info.colorimetry.transfer
          || info.colorimetry.matrix)) {
    memset (&ext_vsi, 0, sizeof (ext_vsi));
    ext_vsi.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    ext_vsi.Header.BufferSz = sizeof (ext_vsi);
    ext_vsi.ColourDescriptionPresent = 1;
    ext_vsi.ColourPrimaries =
        gst_video_color_primaries_to_iso (info.colorimetry.primaries);
    ext_vsi.TransferCharacteristics =
        gst_video_transfer_function_to_iso (info.colorimetry.transfer);
    ext_vsi.MatrixCoefficients =
        gst_video_color_matrix_to_iso (info.colorimetry.matrix);
    gst_msdkenc_add_extra_param (thiz, (mfxExtBuffer *) & ext_vsi);
  }

  if (thiz->num_extra_params) {
    thiz->param.NumExtParam = thiz->num_extra_params;
    thiz->param.ExtParam = thiz->extra_params;
  }

  /* validate parameters and allow MFX to make adjustments */
  status = MFXVideoENCODE_Query (session, &thiz->param, &thiz->param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Video Encode Query failed (%s)",
        msdk_status_to_string (status));
    goto failed;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Video Encode Query returned: %s",
        msdk_status_to_string (status));
  }

  status = MFXVideoENCODE_QueryIOSurf (session, &thiz->param, &request);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Encode Query IO surfaces failed (%s)",
        msdk_status_to_string (status));
    goto failed;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Encode Query IO surfaces returned: %s",
        msdk_status_to_string (status));
  }

  request.NumFrameSuggested += thiz->num_extra_frames;

  if (request.NumFrameSuggested < thiz->param.AsyncDepth) {
    GST_ERROR_OBJECT (thiz, "Required %d surfaces (%d suggested), async %d",
        request.NumFrameMin, request.NumFrameSuggested, thiz->param.AsyncDepth);
    goto failed;
  }

  GST_DEBUG_OBJECT (thiz, "Required %d surfaces (%d suggested), allocated %d",
      request.NumFrameMin, request.NumFrameSuggested,
      request.NumFrameSuggested);

  status = MFXVideoENCODE_Init (session, &thiz->param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Init failed (%s)", msdk_status_to_string (status));
    goto failed;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Init returned: %s",
        msdk_status_to_string (status));
  }

  status = MFXVideoENCODE_GetVideoParam (session, &thiz->param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (thiz, "Get Video Parameters failed (%s)",
        msdk_status_to_string (status));
    goto failed;
  } else if (status > MFX_ERR_NONE) {
    GST_WARNING_OBJECT (thiz, "Get Video Parameters returned: %s",
        msdk_status_to_string (status));
  }

  thiz->num_tasks = thiz->param.AsyncDepth;
  thiz->tasks = g_new0 (MsdkEncTask, thiz->num_tasks);
  for (i = 0; i < thiz->num_tasks; i++) {
    thiz->tasks[i].output_bitstream.Data = _aligned_alloc (32,
        thiz->param.mfx.BufferSizeInKB * thiz->param.mfx.BRCParamMultiplier *
        1024);
    if (!thiz->tasks[i].output_bitstream.Data) {
      GST_ERROR_OBJECT (thiz, "Memory allocation failed");
      goto failed;
    }
    thiz->tasks[i].output_bitstream.MaxLength =
        thiz->param.mfx.BufferSizeInKB * thiz->param.mfx.BRCParamMultiplier *
        1024;
  }
  thiz->next_task = 0;

  thiz->reconfig = FALSE;
  thiz->initialized = TRUE;

  GST_OBJECT_UNLOCK (thiz);

  return TRUE;

failed:
  GST_OBJECT_UNLOCK (thiz);
  return FALSE;
}

static void
gst_msdkenc_close_encoder (GstMsdkEnc * thiz)
{
  guint i;
  mfxStatus status;

  if (!thiz->context || !thiz->initialized)
    return;

  GST_DEBUG_OBJECT (thiz, "Closing encoder with context %" GST_PTR_FORMAT,
      thiz->context);

  gst_clear_object (&thiz->msdk_pool);
  gst_clear_object (&thiz->msdk_converted_pool);

  status = MFXVideoENCODE_Close (gst_msdk_context_get_session (thiz->context));
  if (status != MFX_ERR_NONE && status != MFX_ERR_NOT_INITIALIZED) {
    GST_WARNING_OBJECT (thiz, "Encoder close failed (%s)",
        msdk_status_to_string (status));
  }

  if (thiz->tasks) {
    for (i = 0; i < thiz->num_tasks; i++) {
      MsdkEncTask *task = &thiz->tasks[i];
      if (task->output_bitstream.Data) {
        _aligned_free (task->output_bitstream.Data);
      }
    }
  }
  g_free (thiz->tasks);
  thiz->tasks = NULL;

  memset (&thiz->param, 0, sizeof (thiz->param));
  thiz->num_extra_params = 0;
  thiz->initialized = FALSE;
}

typedef struct
{
  GstVideoCodecFrame *frame;
  GstMsdkSurface *frame_surface;
  GstMsdkSurface *converted_surface;
} FrameData;

static FrameData *
gst_msdkenc_queue_frame (GstMsdkEnc * thiz, GstVideoCodecFrame * frame,
    GstVideoInfo * info)
{
  FrameData *fdata;

  fdata = g_slice_new (FrameData);
  fdata->frame = gst_video_codec_frame_ref (frame);

  thiz->pending_frames = g_list_prepend (thiz->pending_frames, fdata);

  return fdata;
}

static void
gst_msdkenc_free_surface (GstMsdkSurface * surface)
{
  if (surface->buf)
    gst_buffer_unref (surface->buf);

  g_slice_free (GstMsdkSurface, surface);
}

static void
gst_msdkenc_free_frame_data (GstMsdkEnc * thiz, FrameData * fdata)
{
  if (fdata->frame_surface)
    gst_msdkenc_free_surface (fdata->frame_surface);

  gst_video_codec_frame_unref (fdata->frame);
  g_slice_free (FrameData, fdata);
}

static void
gst_msdkenc_dequeue_frame (GstMsdkEnc * thiz, GstVideoCodecFrame * frame)
{
  GList *l;

  for (l = thiz->pending_frames; l;) {
    FrameData *fdata = l->data;
    GList *l1 = l;

    l = l->next;

    if (fdata->frame != frame)
      continue;

    gst_msdkenc_free_frame_data (thiz, fdata);

    thiz->pending_frames = g_list_delete_link (thiz->pending_frames, l1);
    return;
  }
}

static void
gst_msdkenc_dequeue_all_frames (GstMsdkEnc * thiz)
{
  GList *l;

  for (l = thiz->pending_frames; l; l = l->next) {
    FrameData *fdata = l->data;

    gst_msdkenc_free_frame_data (thiz, fdata);
  }
  g_list_free (thiz->pending_frames);
  thiz->pending_frames = NULL;
}

static MsdkEncTask *
gst_msdkenc_get_free_task (GstMsdkEnc * thiz)
{
  MsdkEncTask *tasks = thiz->tasks;
  guint size = thiz->num_tasks;
  guint start = thiz->next_task;
  guint i;

  if (tasks) {
    for (i = 0; i < size; i++) {
      guint t = (start + i) % size;
      if (tasks[t].sync_point == NULL)
        return &tasks[t];
    }
  }
  return NULL;
}

static void
gst_msdkenc_reset_task (MsdkEncTask * task)
{
  task->output_bitstream.DataLength = 0;
  task->sync_point = NULL;
}

static GstVideoCodecFrame *
gst_msdkenc_find_best_frame (GstMsdkEnc * thiz, GList * frames,
    mfxBitstream * bitstream)
{
  GList *iter;
  GstVideoCodecFrame *ret = NULL;
  GstClockTime pts;
  GstClockTimeDiff best_diff = GST_CLOCK_STIME_NONE;

  if (!bitstream)
    return NULL;

  if (bitstream->TimeStamp == MFX_TIMESTAMP_UNKNOWN) {
    pts = GST_CLOCK_TIME_NONE;
  } else {
    pts = gst_util_uint64_scale (bitstream->TimeStamp, GST_SECOND, 90000);
  }

  for (iter = frames; iter; iter = g_list_next (iter)) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) iter->data;

    /* if we don't know the time stamp, find the first frame which
     * has unknown timestamp */
    if (!GST_CLOCK_TIME_IS_VALID (pts)) {
      if (!GST_CLOCK_TIME_IS_VALID (frame->pts)) {
        ret = frame;
        break;
      }
    } else {
      GstClockTimeDiff abs_diff = ABS (GST_CLOCK_DIFF (frame->pts, pts));
      if (abs_diff == 0) {
        ret = frame;
        break;
      }

      if (!GST_CLOCK_STIME_IS_VALID (best_diff) || abs_diff < best_diff) {
        ret = frame;
        best_diff = abs_diff;
      }
    }
  }

  if (ret)
    gst_video_codec_frame_ref (ret);

  return ret;
}

static GstFlowReturn
gst_msdkenc_finish_frame (GstMsdkEnc * thiz, MsdkEncTask * task,
    gboolean discard)
{
  GstVideoCodecFrame *frame;
  GList *list;

  if (!task->sync_point)
    return GST_FLOW_OK;

  list = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (thiz));

  if (!list) {
    GST_ERROR_OBJECT (thiz, "failed to get list of frame");
    return GST_FLOW_ERROR;
  }

  /* Wait for encoding operation to complete, the magic number 300000 below
   * is used in MSDK samples
   * #define MSDK_ENC_WAIT_INTERVAL 300000
   */
  if (MFXVideoCORE_SyncOperation (gst_msdk_context_get_session (thiz->context),
          task->sync_point, 300000) != MFX_ERR_NONE)
    GST_WARNING_OBJECT (thiz, "failed to do sync operation");

  if (!discard && task->output_bitstream.DataLength) {
    GstBuffer *out_buf = NULL;
    guint8 *data =
        task->output_bitstream.Data + task->output_bitstream.DataOffset;
    gsize size = task->output_bitstream.DataLength;

    frame = gst_msdkenc_find_best_frame (thiz, list, &task->output_bitstream);
    if (!frame) {
      /* just pick the oldest one */
      frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (thiz));
    }

    out_buf = gst_buffer_new_allocate (NULL, size, NULL);
    gst_buffer_fill (out_buf, 0, data, size);
    frame->output_buffer = out_buf;
    frame->pts =
        gst_util_uint64_scale (task->output_bitstream.TimeStamp, GST_SECOND,
        90000);
    frame->dts =
        gst_util_uint64_scale (task->output_bitstream.DecodeTimeStamp,
        GST_SECOND, 90000);
    if ((task->output_bitstream.FrameType & MFX_FRAMETYPE_IDR) != 0 ||
        (task->output_bitstream.FrameType & MFX_FRAMETYPE_xIDR) != 0) {
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    }

    /* Mark task as available */
    gst_msdkenc_reset_task (task);
  } else {
    frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (thiz));
  }

  g_list_free_full (list, (GDestroyNotify) gst_video_codec_frame_unref);

  gst_video_codec_frame_unref (frame);
  gst_msdkenc_dequeue_frame (thiz, frame);

  return gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (thiz), frame);
}

static GstFlowReturn
gst_msdkenc_encode_frame (GstMsdkEnc * thiz, mfxFrameSurface1 * surface,
    GstVideoCodecFrame * input_frame)
{
  mfxSession session;
  MsdkEncTask *task;
  mfxStatus status;

  if (G_UNLIKELY (thiz->context == NULL)) {
    gst_msdkenc_dequeue_frame (thiz, input_frame);
    gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (thiz), input_frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }
  session = gst_msdk_context_get_session (thiz->context);

  task = gst_msdkenc_get_free_task (thiz);

  for (;;) {
    /* Force key-frame if needed */
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (input_frame))
      thiz->enc_cntrl.FrameType =
          MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
    else
      thiz->enc_cntrl.FrameType = MFX_FRAMETYPE_UNKNOWN;

    status =
        MFXVideoENCODE_EncodeFrameAsync (session, &thiz->enc_cntrl, surface,
        &task->output_bitstream, &task->sync_point);

    if (status != MFX_WRN_DEVICE_BUSY)
      break;
    /* If device is busy, wait 1ms and retry, as per MSDK's recomendation */
    g_usleep (1000);
  };

  if (status != MFX_ERR_NONE && status != MFX_ERR_MORE_DATA) {
    GST_ELEMENT_ERROR (thiz, STREAM, ENCODE, ("Encode frame failed."),
        ("MSDK encode error (%s)", msdk_status_to_string (status)));
    gst_msdkenc_dequeue_frame (thiz, input_frame);
    gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (thiz), input_frame);
    return GST_FLOW_ERROR;
  }

  if (task->sync_point) {
    thiz->next_task = ((task - thiz->tasks) + 1) % thiz->num_tasks;
  } else if (status == MFX_ERR_MORE_DATA) {
    gst_msdkenc_dequeue_frame (thiz, input_frame);
  }

  /* Ensure that next task is available */
  task = thiz->tasks + thiz->next_task;
  return gst_msdkenc_finish_frame (thiz, task, FALSE);
}

static guint
gst_msdkenc_maximum_delayed_frames (GstMsdkEnc * thiz)
{
  return thiz->num_tasks;
}

static void
gst_msdkenc_set_latency (GstMsdkEnc * thiz)
{
  GstVideoInfo *info = &thiz->input_state->info;
  gint max_delayed_frames;
  GstClockTime latency;

  max_delayed_frames = gst_msdkenc_maximum_delayed_frames (thiz);

  if (info->fps_n) {
    latency = gst_util_uint64_scale_ceil (GST_SECOND * info->fps_d,
        max_delayed_frames, info->fps_n);
  } else {
    /* FIXME: Assume 25fps. This is better than reporting no latency at
     * all and then later failing in live pipelines
     */
    latency = gst_util_uint64_scale_ceil (GST_SECOND * 1,
        max_delayed_frames, 25);
  }

  GST_INFO_OBJECT (thiz,
      "Updating latency to %" GST_TIME_FORMAT " (%d frames)",
      GST_TIME_ARGS (latency), max_delayed_frames);

  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (thiz), latency, latency);
}

static void
gst_msdkenc_flush_frames (GstMsdkEnc * thiz, gboolean discard)
{
  mfxStatus status;
  mfxSession session;
  MsdkEncTask *task;
  guint i, t;

  if (!thiz->tasks)
    return;

  GST_DEBUG_OBJECT (thiz, "flush frames");

  session = gst_msdk_context_get_session (thiz->context);

  for (;;) {
    task = thiz->tasks + thiz->next_task;
    gst_msdkenc_finish_frame (thiz, task, FALSE);

    status = MFXVideoENCODE_EncodeFrameAsync (session, NULL, NULL,
        &task->output_bitstream, &task->sync_point);

    if (status != MFX_ERR_NONE && status != MFX_ERR_MORE_DATA) {
      GST_ELEMENT_ERROR (thiz, STREAM, ENCODE, ("Encode frame failed."),
          ("MSDK encode error (%s)", msdk_status_to_string (status)));
      break;
    }

    if (task->sync_point) {
      thiz->next_task = ((task - thiz->tasks) + 1) % thiz->num_tasks;
    } else if (status == MFX_ERR_MORE_DATA) {
      break;
    }
  };

  t = thiz->next_task;
  for (i = 0; i < thiz->num_tasks; i++) {
    gst_msdkenc_finish_frame (thiz, &thiz->tasks[t], discard);
    t = (t + 1) % thiz->num_tasks;
  }
}

static gboolean
gst_msdkenc_set_src_caps (GstMsdkEnc * thiz)
{
  GstMsdkEncClass *klass = GST_MSDKENC_GET_CLASS (thiz);
  GstCaps *outcaps = NULL;
  GstVideoCodecState *state;
  GstTagList *tags;

  if (klass->set_src_caps)
    outcaps = klass->set_src_caps (thiz);

  if (!outcaps)
    return FALSE;

  state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (thiz),
      outcaps, thiz->input_state);
  GST_DEBUG_OBJECT (thiz, "output caps: %" GST_PTR_FORMAT, state->caps);

  gst_video_codec_state_unref (state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "msdkenc",
      GST_TAG_MAXIMUM_BITRATE, thiz->bitrate * 1024,
      GST_TAG_NOMINAL_BITRATE, thiz->bitrate * 1024, NULL);
  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (thiz), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

#ifndef _WIN32
static GstBufferPool *
gst_msdk_create_va_pool (GstMsdkEnc * thiz, GstVideoInfo * info,
    guint num_buffers)
{
  GstBufferPool *pool = NULL;
  GstAllocator *allocator;
  GArray *formats = NULL;
  GstAllocationParams alloc_params = { 0, 31, 0, 0 };
  GstVaDisplay *display = NULL;
  GstCaps *aligned_caps = NULL;

  display = (GstVaDisplay *) gst_msdk_context_get_va_display (thiz->context);

  if (thiz->use_dmabuf) {
    allocator = gst_va_dmabuf_allocator_new (display);
  } else {
    formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));
    g_array_append_val (formats, GST_VIDEO_INFO_FORMAT (info));
    allocator = gst_va_allocator_new (display, formats);
  }

  gst_object_unref (display);

  if (!allocator) {
    GST_ERROR_OBJECT (thiz, "failed to create allocator");
    if (formats)
      g_array_unref (formats);
    return NULL;
  }

  if (thiz->use_dmabuf && thiz->modifier != DRM_FORMAT_MOD_INVALID) {
    aligned_caps = gst_msdkcaps_video_info_to_drm_caps (info, thiz->modifier);
    gst_caps_set_features (aligned_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL));
  } else
    aligned_caps = gst_video_info_to_caps (info);

  pool = gst_va_pool_new_with_config (aligned_caps, num_buffers, 0,
      VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC, GST_VA_FEATURE_AUTO,
      allocator, &alloc_params);

  gst_object_unref (allocator);
  gst_caps_unref (aligned_caps);

  GST_LOG_OBJECT (thiz, "Creating va pool");
  return pool;
}
#else
static GstBufferPool *
gst_msdk_create_d3d11_pool (GstMsdkEnc * thiz, guint num_buffers)
{
  GstBufferPool *pool = NULL;
  GstD3D11Device *device;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  GstD3D11Format device_format;
  guint bind_flags = 0;
  GstCaps *aligned_caps = NULL;
  GstVideoInfo *info = &thiz->input_state->info;
  GstVideoInfo aligned_info;
  gint aligned_width;
  gint aligned_height;

  device = gst_msdk_context_get_d3d11_device (thiz->context);

  aligned_width = GST_ROUND_UP_16 (info->width);
  if (GST_VIDEO_INFO_IS_INTERLACED (info)) {
    aligned_height = GST_ROUND_UP_32 (info->height);
  } else {
    aligned_height = GST_ROUND_UP_16 (info->height);
  }

  gst_video_info_set_interlaced_format (&aligned_info,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_INTERLACE_MODE (info),
      aligned_width, aligned_height);

  gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (&aligned_info),
      &device_format);
  if ((device_format.format_support[0] & D3D11_FORMAT_SUPPORT_RENDER_TARGET) ==
      D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
    bind_flags = D3D11_BIND_RENDER_TARGET;
  }

  aligned_caps = gst_video_info_to_caps (&aligned_info);

  pool = gst_d3d11_buffer_pool_new (device);
  config = gst_buffer_pool_get_config (pool);
  params = gst_d3d11_allocation_params_new (device, &aligned_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags,
      D3D11_RESOURCE_MISC_SHARED);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, aligned_caps,
      GST_VIDEO_INFO_SIZE (&aligned_info), num_buffers, 0);
  gst_buffer_pool_set_config (pool, config);

  gst_caps_unref (aligned_caps);
  GST_LOG_OBJECT (thiz, "Creating d3d11 pool");

  return pool;
}
#endif

static GstBufferPool *
gst_msdkenc_create_buffer_pool (GstMsdkEnc * thiz, GstCaps * caps,
    guint num_buffers, gboolean set_align)
{
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstVideoInfo info;
  GstVideoAlignment align;

  if (!gst_msdkcaps_video_info_from_caps (caps, &info, NULL)) {
    GST_INFO_OBJECT (thiz, "failed to get video info");
    return FALSE;
  }

  gst_msdk_set_video_alignment (&info, 0, 0, &align);
  gst_video_info_align (&info, &align);
#ifndef _WIN32
  pool = gst_msdk_create_va_pool (thiz, &info, num_buffers);
#else
  pool = gst_msdk_create_d3d11_pool (thiz, num_buffers);
#endif
  if (!thiz->use_video_memory)
    pool = gst_video_buffer_pool_new ();
  if (!pool)
    goto error_no_pool;

  config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));
  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (&info), num_buffers, 0);
  gst_buffer_pool_config_set_video_alignment (config, &align);

  if (!gst_buffer_pool_set_config (pool, config))
    goto error_pool_config;

  if (set_align)
    thiz->aligned_info = info;

  return pool;

error_no_pool:
  {
    GST_INFO_OBJECT (thiz, "failed to create bufferpool");
    return NULL;
  }
error_pool_config:
  {
    GST_INFO_OBJECT (thiz, "failed to set config");
    gst_object_unref (pool);
    return NULL;
  }
}

static gboolean
sinkpad_can_dmabuf (GstMsdkEnc * thiz)
{
  gboolean ret = FALSE;
  GstCaps *caps, *allowed_caps;
  GstPad *sinkpad;

  sinkpad = GST_VIDEO_ENCODER_SINK_PAD (thiz);
  caps = gst_pad_get_pad_template_caps (sinkpad);

  allowed_caps = gst_pad_peer_query_caps (sinkpad, caps);
  if (!allowed_caps)
    goto done;
  if (gst_caps_is_any (allowed_caps) || gst_caps_is_empty (allowed_caps)
      || allowed_caps == caps)
    goto done;

  if (gst_msdkcaps_has_feature (allowed_caps, GST_CAPS_FEATURE_MEMORY_DMABUF))
    ret = TRUE;

done:
  if (caps)
    gst_caps_unref (caps);
  if (allowed_caps)
    gst_caps_unref (allowed_caps);
  return ret;
}

#ifndef _WIN32
static gboolean
sinkpad_is_va (GstMsdkEnc * thiz)
{
  GstCapsFeatures *features =
      gst_caps_get_features (thiz->input_state->caps, 0);
  if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_VA))
    return TRUE;

  return FALSE;
}
#else
static gboolean
sinkpad_is_d3d11 (GstMsdkEnc * thiz)
{
  GstCapsFeatures *features =
      gst_caps_get_features (thiz->input_state->caps, 0);
  if (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY))
    return TRUE;

  return FALSE;
}
#endif

static gboolean
gst_msdkenc_set_format (GstVideoEncoder * encoder, GstVideoCodecState * state)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);
  GstMsdkEncClass *klass = GST_MSDKENC_GET_CLASS (thiz);

  if (state) {
    if (thiz->input_state) {
      if (!gst_video_info_is_equal (&thiz->input_state->info, &state->info)) {
        GST_INFO_OBJECT (thiz, "Re-init the encoder as info changed");
        gst_msdkenc_flush_frames (thiz, FALSE);
        gst_msdkenc_close_encoder (thiz);
      }
      gst_video_codec_state_unref (thiz->input_state);
    }
    thiz->input_state = gst_video_codec_state_ref (state);
  }
#ifndef _WIN32
  thiz->use_video_memory = TRUE;
  if (sinkpad_is_va (thiz))
    thiz->use_va = TRUE;
#else
  thiz->use_video_memory = TRUE;
  if (sinkpad_is_d3d11 (thiz))
    thiz->use_d3d11 = TRUE;
#endif

  GST_INFO_OBJECT (encoder, "This MSDK encoder uses %s memory",
      thiz->use_video_memory ? "video" : "system");

  if (klass->set_format) {
    if (!klass->set_format (thiz))
      return FALSE;
  }

  /* If upstream supports DMABufCapsfeatures, then we request for the dmabuf
   * based pipeline usage. Ideally we should have dmabuf support even with
   * raw-caps negotiation, but we don't have dmabuf-import support in msdk
   * plugin yet */
#ifndef _WIN32
  /* If VA is set, we do not fallback to DMA. */
  if (!thiz->use_va && sinkpad_can_dmabuf (thiz)) {
    thiz->input_state->caps = gst_caps_make_writable (thiz->input_state->caps);
    gst_caps_set_features (thiz->input_state->caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL));
    thiz->use_dmabuf = TRUE;
    thiz->modifier = get_msdkcaps_get_modifier (state->caps);
  }
#endif

  if (!gst_msdkenc_init_encoder (thiz))
    return FALSE;

  if (!gst_msdkenc_set_src_caps (thiz)) {
    gst_msdkenc_close_encoder (thiz);
    return FALSE;
  }

  if (!thiz->msdk_pool) {
    guint num_buffers = gst_msdkenc_maximum_delayed_frames (thiz) + 1;
    thiz->msdk_pool =
        gst_msdkenc_create_buffer_pool (thiz, thiz->input_state->caps,
        num_buffers, TRUE);
  }

  gst_msdkenc_set_latency (thiz);

  return TRUE;
}

static GstMsdkSurface *
gst_msdkenc_get_surface_from_pool (GstMsdkEnc * thiz,
    GstVideoCodecFrame * frame, GstBuffer * buf)
{
  GstBuffer *upload_buf;
  GstMsdkSurface *msdk_surface = NULL;
  GstVideoFrame src_frame, dst_frame;

  if (!gst_buffer_pool_is_active (thiz->msdk_pool) &&
      !gst_buffer_pool_set_active (thiz->msdk_pool, TRUE)) {
    GST_ERROR_OBJECT (thiz->msdk_pool, "failed to activate buffer pool");
    return NULL;
  }

  if (gst_buffer_pool_acquire_buffer (thiz->msdk_pool, &upload_buf,
          NULL) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (thiz->msdk_pool, "failed to acquire a buffer from pool");
    return NULL;
  }

  if (!gst_video_frame_map (&src_frame, &thiz->input_state->info, buf,
          GST_MAP_READ)) {
    GST_WARNING ("Failed to map src frame");
    gst_buffer_unref (upload_buf);
    return NULL;
  }

  if (!gst_video_frame_map (&dst_frame, &thiz->aligned_info, upload_buf,
          GST_MAP_WRITE)) {
    GST_WARNING ("Failed to map dst frame");
    gst_video_frame_unmap (&src_frame);
    gst_buffer_unref (upload_buf);
    return NULL;
  }

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&src_frame); i++) {
    guint src_width_in_bytes, src_height;
    guint dst_width_in_bytes, dst_height;
    guint width_in_bytes, height;
    guint src_stride, dst_stride;
    guint8 *src_data, *dst_data;

    src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&src_frame, i);

    dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    dst_stride = GST_VIDEO_FRAME_COMP_STRIDE (&dst_frame, i);

    width_in_bytes = MIN (src_width_in_bytes, dst_width_in_bytes);
    height = MIN (src_height, dst_height);

    src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);
    dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += dst_stride;
      src_data += src_stride;
    }
  }

  gst_video_frame_unmap (&dst_frame);
  gst_video_frame_unmap (&src_frame);

  if (thiz->use_video_memory) {
    msdk_surface = gst_msdk_import_to_msdk_surface (upload_buf, thiz->context,
        &thiz->aligned_info, GST_MAP_READ);
  } else {
    msdk_surface =
        gst_msdk_import_sys_mem_to_msdk_surface (upload_buf,
        &thiz->aligned_info);
  }

  gst_buffer_replace (&frame->input_buffer, upload_buf);
  gst_buffer_unref (upload_buf);

  return msdk_surface;
}

static GstMsdkSurface *
gst_msdkenc_get_surface_from_frame (GstMsdkEnc * thiz,
    GstVideoCodecFrame * frame)
{
  GstMsdkSurface *msdk_surface;
  GstBuffer *inbuf;

  inbuf = frame->input_buffer;

  msdk_surface = gst_msdk_import_to_msdk_surface (inbuf, thiz->context,
      &thiz->aligned_info, GST_MAP_READ);
  if (msdk_surface) {
    msdk_surface->buf = gst_buffer_ref (inbuf);
    return msdk_surface;
  }

  /* If upstream hasn't accpeted the proposed msdk bufferpool,
   * just copy frame to msdk buffer and take a surface from it.
   */

  return gst_msdkenc_get_surface_from_pool (thiz, frame, inbuf);
}

static GstFlowReturn
gst_msdkenc_handle_frame (GstVideoEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);
  GstMsdkEncClass *klass = GST_MSDKENC_GET_CLASS (thiz);
  GstVideoInfo *info = &thiz->input_state->info;
  FrameData *fdata;
  GstMsdkSurface *surface;

  if (thiz->reconfig || klass->need_reconfig (thiz, frame)) {
    gst_msdkenc_flush_frames (thiz, FALSE);
    gst_msdkenc_close_encoder (thiz);

    klass->set_extra_params (thiz, frame);

    // This will reinitialized the encoder but keep same input format.
    gst_msdkenc_set_format (encoder, NULL);
  }

  if (G_UNLIKELY (thiz->context == NULL))
    goto not_inited;

  surface = gst_msdkenc_get_surface_from_frame (thiz, frame);
  if (!surface)
    goto invalid_surface;

  fdata = gst_msdkenc_queue_frame (thiz, frame, info);
  if (!fdata)
    goto invalid_frame;

  fdata->frame_surface = surface;

  /* It is possible to have input frame without any framerate/pts info,
   * we need to set the correct pts here. */
  if (frame->presentation_frame_number == 0)
    thiz->start_pts = frame->pts;

  if (frame->pts != GST_CLOCK_TIME_NONE) {
    frame->pts = thiz->start_pts +
        frame->presentation_frame_number * thiz->frame_duration;
    frame->duration = thiz->frame_duration;
    surface->surface->Data.TimeStamp =
        gst_util_uint64_scale (frame->pts, 90000, GST_SECOND);
  } else {
    surface->surface->Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
  }

  return gst_msdkenc_encode_frame (thiz, surface->surface, frame);

/* ERRORS */
not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    return GST_FLOW_NOT_NEGOTIATED;
  }
invalid_surface:
  {
    GST_ERROR_OBJECT (encoder, "Surface pool is full");
    return GST_FLOW_ERROR;
  }
invalid_frame:
  {
    GST_WARNING_OBJECT (encoder, "Failed to map frame");
    return GST_FLOW_OK;
  }
}

static gboolean
gst_msdkenc_context_prepare (GstMsdkEnc * thiz)
{
  /* Try to find an existing context from the pipeline. This may (indirectly)
   * invoke gst_msdkenc_set_context, which will set thiz->context. */
  if (!gst_msdk_context_find (GST_ELEMENT_CAST (thiz), &thiz->context))
    return FALSE;

  if (thiz->context == thiz->old_context) {
    GST_INFO_OBJECT (thiz, "Found old context %" GST_PTR_FORMAT
        ", reusing as-is", thiz->context);
    return TRUE;
  }

  GST_INFO_OBJECT (thiz, "Found context %" GST_PTR_FORMAT " from neighbour",
      thiz->context);

  /* Check GST_MSDK_JOB_VPP and GST_MSDK_JOB_ENCODER together to avoid sharing context
   * between VPP and ENCODER
   * Example:
   * gst-launch-1.0 videotestsrc ! video/x-raw,format=I420 ! msdkh264enc ! \
   * msdkh264dec ! msdkvpp ! video/x-raw,format=YUY2 ! fakesink
   */
  if (!(gst_msdk_context_get_job_type (thiz->context) & (GST_MSDK_JOB_VPP |
              GST_MSDK_JOB_ENCODER))) {
    gst_msdk_context_add_job_type (thiz->context, GST_MSDK_JOB_ENCODER);
    return TRUE;
  }

  /* Found an existing context that's already being used as an encoder, clone
   * the MFX session inside it to create a new one */
  {
    GstMsdkContext *parent_context, *msdk_context;

    GST_INFO_OBJECT (thiz, "Creating new context %" GST_PTR_FORMAT " with "
        "joined session", thiz->context);
    parent_context = thiz->context;
    msdk_context = gst_msdk_context_new_with_parent (parent_context);

    if (!msdk_context) {
      GST_ERROR_OBJECT (thiz, "Failed to create a context with parent context "
          "as %" GST_PTR_FORMAT, parent_context);
      return FALSE;
    }

    thiz->context = msdk_context;
    gst_object_unref (parent_context);
  }

  return TRUE;
}

static gboolean
gst_msdkenc_start (GstVideoEncoder * encoder)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);

  if (!gst_msdkenc_context_prepare (thiz)) {
    if (!gst_msdk_ensure_new_context (GST_ELEMENT_CAST (thiz),
            thiz->hardware, GST_MSDK_JOB_ENCODER, &thiz->context))
      return FALSE;
    GST_INFO_OBJECT (thiz, "Creating new context %" GST_PTR_FORMAT,
        thiz->context);
  }

  /* Save the current context in a separate field so that we know whether it
   * has changed between calls to _start() */
  gst_object_replace ((GstObject **) & thiz->old_context,
      (GstObject *) thiz->context);

  gst_msdk_context_add_shared_async_depth (thiz->context, thiz->async_depth);

  /* Set the minimum pts to some huge value (1000 hours). This keeps
     the dts at the start of the stream from needing to be
     negative. */
  gst_video_encoder_set_min_pts (encoder, GST_SECOND * 60 * 60 * 1000);

  return TRUE;
}

static gboolean
gst_msdkenc_stop (GstVideoEncoder * encoder)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);

  gst_msdkenc_flush_frames (thiz, TRUE);
  gst_msdkenc_close_encoder (thiz);
  gst_msdkenc_dequeue_all_frames (thiz);

  if (thiz->input_state)
    gst_video_codec_state_unref (thiz->input_state);
  thiz->input_state = NULL;

  gst_clear_object (&thiz->context);

  return TRUE;
}

static gboolean
gst_msdkenc_flush (GstVideoEncoder * encoder)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);

  GST_DEBUG_OBJECT (encoder, "flush and close encoder");

  gst_msdkenc_flush_frames (thiz, TRUE);
  gst_msdkenc_close_encoder (thiz);
  gst_msdkenc_dequeue_all_frames (thiz);

  gst_msdkenc_init_encoder (thiz);

  return TRUE;
}

static GstFlowReturn
gst_msdkenc_finish (GstVideoEncoder * encoder)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);

  gst_msdkenc_flush_frames (thiz, FALSE);

  return GST_FLOW_OK;
}

#ifndef _WIN32
static gboolean
gst_msdkenc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstAllocator *allocator = NULL;
  GstCaps *caps;
  guint num_buffers;

  if (!thiz->input_state)
    return FALSE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_INFO_OBJECT (encoder, "failed to get caps");
    return FALSE;
  }

  if (!gst_msdkcaps_video_info_from_caps (caps, &info, NULL)) {
    GST_INFO_OBJECT (encoder, "failed to get video info");
    return FALSE;
  }

  /* if upstream allocation query supports dmabuf-capsfeatures,
   *  we do allocate dmabuf backed memory */
  if (gst_msdkcaps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    GST_INFO_OBJECT (thiz, "MSDK VPP srcpad uses DMABuf memory");
    thiz->use_dmabuf = TRUE;
  }

  num_buffers = gst_msdkenc_maximum_delayed_frames (thiz) + 1;
  pool = gst_msdkenc_create_buffer_pool (thiz, caps, num_buffers, FALSE);

  gst_query_add_allocation_pool (query, pool, GST_VIDEO_INFO_SIZE (&info),
      num_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  if (pool) {
    GstStructure *config;
    GstAllocationParams params = { 0, 31, 0, 0, };

    config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));

    if (gst_buffer_pool_config_get_allocator (config, &allocator, NULL))
      gst_query_add_allocation_param (query, allocator, &params);
    gst_structure_free (config);
    gst_object_unref (pool);
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}
#else
static gboolean
gst_msdkenc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstD3D11Device *device;
  GstCaps *caps;
  guint size;
  GstCapsFeatures *features;
  guint num_buffers;
  GstStructure *config;
  gboolean is_d3d11 = FALSE;

  if (!thiz->input_state)
    return FALSE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_INFO_OBJECT (encoder, "failed to get caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_INFO_OBJECT (encoder, "failed to get video info");
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    GST_DEBUG_OBJECT (thiz, "upstream support d3d11 memory");
    device = gst_msdk_context_get_d3d11_device (thiz->context);
    pool = gst_d3d11_buffer_pool_new (device);
    is_d3d11 = TRUE;
  } else {
    pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (is_d3d11) {
    GstD3D11AllocationParams *d3d11_params;
    GstVideoAlignment align;

    /* d3d11 buffer pool doesn't support generic video alignment
     * because memory layout of CPU accessible staging texture is uncontrollable.
     * Do D3D11 specific handling */
    gst_msdk_set_video_alignment (&info, 0, 0, &align);

    d3d11_params = gst_d3d11_allocation_params_new (device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);

    gst_d3d11_allocation_params_alignment (d3d11_params, &align);
    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  } else {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  num_buffers = gst_msdkenc_maximum_delayed_frames (thiz) + 1;
  gst_buffer_pool_config_set_params (config,
      caps, GST_VIDEO_INFO_SIZE (&info), num_buffers, 0);
  gst_buffer_pool_set_config (pool, config);

  /* d3d11 buffer pool will update actual CPU accessible buffer size based on
   * allocated staging texture per gst_buffer_pool_set_config() call,
   * need query again to get the size */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, num_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_object_unref (pool);

  return TRUE;
}
#endif

static gboolean
gst_msdkenc_query (GstVideoEncoder * encoder, GstQuery * query,
    GstPadDirection dir)
{
  GstMsdkEnc *thiz = GST_MSDKENC (encoder);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      GstMsdkContext *msdk_context = NULL;

      gst_object_replace ((GstObject **) & msdk_context,
          (GstObject *) thiz->context);
      ret = gst_msdk_handle_context_query (GST_ELEMENT_CAST (encoder),
          query, msdk_context);
      gst_clear_object (&msdk_context);
      break;
    }
    default:
      if (dir == GST_PAD_SRC) {
        ret =
            GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
      } else {
        ret =
            GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
      }
      break;
  }

  return ret;
}

static gboolean
gst_msdkenc_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  return gst_msdkenc_query (encoder, query, GST_PAD_SRC);
}

static gboolean
gst_msdkenc_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  return gst_msdkenc_query (encoder, query, GST_PAD_SINK);
}

static void
gst_msdkenc_dispose (GObject * object)
{
  GstMsdkEnc *thiz = GST_MSDKENC (object);

  if (thiz->input_state)
    gst_video_codec_state_unref (thiz->input_state);
  thiz->input_state = NULL;

  gst_clear_object (&thiz->msdk_pool);
  gst_clear_object (&thiz->msdk_converted_pool);
  gst_clear_object (&thiz->old_context);

  gst_clear_structure (&thiz->ext_coding_props);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
gst_msdkenc_is_format_supported (GstMsdkEnc * encoder, GstVideoFormat format)
{
  return format == GST_VIDEO_FORMAT_NV12;
}

static gboolean
gst_msdkenc_need_reconfig (GstMsdkEnc * encoder, GstVideoCodecFrame * frame)
{
  return FALSE;
}

static void
gst_msdkenc_set_extra_params (GstMsdkEnc * encoder, GstVideoCodecFrame * frame)
{
  /* Do nothing */
}

static void
gst_msdkenc_class_init (GstMsdkEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *gstencoder_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  gstencoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  klass->is_format_supported = gst_msdkenc_is_format_supported;
  klass->need_reconfig = gst_msdkenc_need_reconfig;
  klass->set_extra_params = gst_msdkenc_set_extra_params;
  klass->qp_max = 51;
  klass->qp_min = 0;

  gobject_class->dispose = gst_msdkenc_dispose;

  element_class->set_context = gst_msdkenc_set_context;

  gstencoder_class->set_format = GST_DEBUG_FUNCPTR (gst_msdkenc_set_format);
  gstencoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_msdkenc_handle_frame);
  gstencoder_class->start = GST_DEBUG_FUNCPTR (gst_msdkenc_start);
  gstencoder_class->stop = GST_DEBUG_FUNCPTR (gst_msdkenc_stop);
  gstencoder_class->flush = GST_DEBUG_FUNCPTR (gst_msdkenc_flush);
  gstencoder_class->finish = GST_DEBUG_FUNCPTR (gst_msdkenc_finish);
  gstencoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_msdkenc_propose_allocation);
  gstencoder_class->src_query = GST_DEBUG_FUNCPTR (gst_msdkenc_src_query);
  gstencoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_msdkenc_sink_query);
}

static void
gst_msdkenc_init (GstMsdkEnc * thiz)
{
  thiz->hardware = PROP_HARDWARE_DEFAULT;
  thiz->async_depth = PROP_ASYNC_DEPTH_DEFAULT;
  thiz->target_usage = PROP_TARGET_USAGE_DEFAULT;
  thiz->rate_control = PROP_RATE_CONTROL_DEFAULT;
  thiz->bitrate = PROP_BITRATE_DEFAULT;
  thiz->max_frame_size = PROP_MAX_FRAME_SIZE_DEFAULT;
  thiz->max_frame_size_i = PROP_MAX_FRAME_SIZE_I_DEFAULT;
  thiz->max_frame_size_p = PROP_MAX_FRAME_SIZE_P_DEFAULT;
  thiz->max_vbv_bitrate = PROP_MAX_VBV_BITRATE_DEFAULT;
  thiz->accuracy = PROP_AVBR_ACCURACY_DEFAULT;
  thiz->convergence = PROP_AVBR_ACCURACY_DEFAULT;
  thiz->lookahead_depth = PROP_RC_LOOKAHEAD_DEPTH_DEFAULT;
  thiz->qpi = PROP_QPI_DEFAULT;
  thiz->qpp = PROP_QPP_DEFAULT;
  thiz->qpb = PROP_QPB_DEFAULT;
  thiz->gop_size = PROP_GOP_SIZE_DEFAULT;
  thiz->ref_frames = PROP_REF_FRAMES_DEFAULT;
  thiz->i_frames = PROP_I_FRAMES_DEFAULT;
  thiz->b_frames = PROP_B_FRAMES_DEFAULT;
  thiz->num_slices = PROP_NUM_SLICES_DEFAULT;
  thiz->mbbrc = PROP_MBBRC_DEFAULT;
  thiz->lowdelay_brc = PROP_LOWDELAY_BRC_DEFAULT;
  thiz->adaptive_i = PROP_ADAPTIVE_I_DEFAULT;
  thiz->adaptive_b = PROP_ADAPTIVE_B_DEFAULT;
#ifndef _WIN32
  thiz->modifier = DRM_FORMAT_MOD_INVALID;
#endif

  thiz->ext_coding_props = gst_structure_new (EC_PROPS_STRUCT_NAME,
      EC_PROPS_EXTBRC, G_TYPE_STRING, "off", NULL);
}

/* *INDENT-OFF* */
#define UPDATE_PROPERTY                         \
  if (*old_val == new_val) {                    \
    return FALSE;                               \
  }                                             \
  *old_val = new_val;                                                   \
  thiz->reconfig = TRUE;                                                \
  return TRUE;                                                          \

gboolean
gst_msdkenc_check_update_property_uint (GstMsdkEnc * thiz, guint * old_val,
    guint new_val)
{
  UPDATE_PROPERTY
}

gboolean
gst_msdkenc_check_update_property_int (GstMsdkEnc * thiz, gint * old_val,
    gint new_val)
{
  UPDATE_PROPERTY
}

gboolean
gst_msdkenc_check_update_property_bool (GstMsdkEnc * thiz, gboolean * old_val,
    gboolean new_val)
{
  UPDATE_PROPERTY
}

#undef UPDATE_PROPERTY
/* *INDENT-ON* */

/* gst_msdkenc_set_common_property:
 *
 * This is a helper function to set the common property
 * of base encoder from subclass implementation.
 */
gboolean
gst_msdkenc_set_common_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkEnc *thiz = GST_MSDKENC (object);
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (thiz);

  switch (prop_id) {
    case GST_MSDKENC_PROP_HARDWARE:
      thiz->hardware = g_value_get_boolean (value);
      break;
    case GST_MSDKENC_PROP_ASYNC_DEPTH:
      thiz->async_depth = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_TARGET_USAGE:
      thiz->target_usage = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_RATE_CONTROL:
      thiz->rate_control = g_value_get_enum (value);
      break;
    case GST_MSDKENC_PROP_BITRATE:
    {
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->bitrate,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed bitrate to %u", thiz->bitrate);
      }
      break;
    }
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->max_frame_size,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-frame-size to %u",
            thiz->max_frame_size);
      }
      break;
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE_I:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->max_frame_size_i,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-frame-size-i to %u",
            thiz->max_frame_size_i);
      }
      break;
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE_P:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->max_frame_size_p,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed max-frame-size-p to %u",
            thiz->max_frame_size_p);
      }
      break;
    case GST_MSDKENC_PROP_MAX_VBV_BITRATE:
      thiz->max_vbv_bitrate = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_AVBR_ACCURACY:
      thiz->accuracy = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_AVBR_CONVERGENCE:
      thiz->convergence = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_RC_LOOKAHEAD_DEPTH:
      thiz->lookahead_depth = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_QPI:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->qpi,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed qpi to %u", thiz->qpi);
      }
      break;
    case GST_MSDKENC_PROP_QPP:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->qpp,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed qpp to %u", thiz->qpp);
      }
      break;
    case GST_MSDKENC_PROP_QPB:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->qpb,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed qpb to %u", thiz->qpb);
      }
      break;
    case GST_MSDKENC_PROP_GOP_SIZE:
      if (gst_msdkenc_check_update_property_uint (thiz, &thiz->gop_size,
              g_value_get_uint (value))) {
        GST_DEBUG_OBJECT (thiz, "changed gop-size to %u", thiz->gop_size);
      }
      break;
    case GST_MSDKENC_PROP_REF_FRAMES:
      thiz->ref_frames = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_I_FRAMES:
      thiz->i_frames = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_B_FRAMES:
      thiz->b_frames = g_value_get_int (value);
      break;
    case GST_MSDKENC_PROP_NUM_SLICES:
      thiz->num_slices = g_value_get_uint (value);
      break;
    case GST_MSDKENC_PROP_MBBRC:
      thiz->mbbrc = g_value_get_enum (value);
      break;
    case GST_MSDKENC_PROP_LOWDELAY_BRC:
      thiz->lowdelay_brc = g_value_get_enum (value);
      break;
    case GST_MSDKENC_PROP_ADAPTIVE_I:
      thiz->adaptive_i = g_value_get_enum (value);
      break;
    case GST_MSDKENC_PROP_ADAPTIVE_B:
      thiz->adaptive_b = g_value_get_enum (value);
      break;
    case GST_MSDKENC_PROP_EXT_CODING_PROPS:
    {
      const GstStructure *s = gst_value_get_structure (value);
      const gchar *name = gst_structure_get_name (s);
      gst_structure_set_name (thiz->ext_coding_props, name);
      if (!structure_transform (s, thiz->ext_coding_props)) {
        GST_ERROR_OBJECT (thiz, "failed to transform structure");
      }
      break;
    }
    default:
      ret = FALSE;
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
  return ret;
}

/* gst_msdkenc_get_common_property:
 *
 * This is a helper function to get the common property
 * of base encoder from subclass implementation.
 */
gboolean
gst_msdkenc_get_common_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMsdkEnc *thiz = GST_MSDKENC (object);
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (thiz);
  switch (prop_id) {
    case GST_MSDKENC_PROP_HARDWARE:
      g_value_set_boolean (value, thiz->hardware);
      break;
    case GST_MSDKENC_PROP_ASYNC_DEPTH:
      g_value_set_uint (value, thiz->async_depth);
      break;
    case GST_MSDKENC_PROP_TARGET_USAGE:
      g_value_set_uint (value, thiz->target_usage);
      break;
    case GST_MSDKENC_PROP_RATE_CONTROL:
      g_value_set_enum (value, thiz->rate_control);
      break;
    case GST_MSDKENC_PROP_BITRATE:
      g_value_set_uint (value, thiz->bitrate);
      break;
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE:
      g_value_set_uint (value, thiz->max_frame_size);
      break;
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE_I:
      g_value_set_uint (value, thiz->max_frame_size_i);
      break;
    case GST_MSDKENC_PROP_MAX_FRAME_SIZE_P:
      g_value_set_uint (value, thiz->max_frame_size_p);
      break;
    case GST_MSDKENC_PROP_MAX_VBV_BITRATE:
      g_value_set_uint (value, thiz->max_vbv_bitrate);
      break;
    case GST_MSDKENC_PROP_AVBR_ACCURACY:
      g_value_set_uint (value, thiz->accuracy);
      break;
    case GST_MSDKENC_PROP_AVBR_CONVERGENCE:
      g_value_set_uint (value, thiz->convergence);
      break;
    case GST_MSDKENC_PROP_RC_LOOKAHEAD_DEPTH:
      g_value_set_uint (value, thiz->lookahead_depth);
      break;
    case GST_MSDKENC_PROP_QPI:
      g_value_set_uint (value, thiz->qpi);
      break;
    case GST_MSDKENC_PROP_QPP:
      g_value_set_uint (value, thiz->qpp);
      break;
    case GST_MSDKENC_PROP_QPB:
      g_value_set_uint (value, thiz->qpb);
      break;
    case GST_MSDKENC_PROP_GOP_SIZE:
      g_value_set_uint (value, thiz->gop_size);
      break;
    case GST_MSDKENC_PROP_REF_FRAMES:
      g_value_set_uint (value, thiz->ref_frames);
      break;
    case GST_MSDKENC_PROP_I_FRAMES:
      g_value_set_uint (value, thiz->i_frames);
      break;
    case GST_MSDKENC_PROP_B_FRAMES:
      g_value_set_int (value, thiz->b_frames);
      break;
    case GST_MSDKENC_PROP_NUM_SLICES:
      g_value_set_uint (value, thiz->num_slices);
      break;
    case GST_MSDKENC_PROP_MBBRC:
      g_value_set_enum (value, thiz->mbbrc);
      break;
    case GST_MSDKENC_PROP_LOWDELAY_BRC:
      g_value_set_enum (value, thiz->lowdelay_brc);
      break;
    case GST_MSDKENC_PROP_ADAPTIVE_I:
      g_value_set_enum (value, thiz->adaptive_i);
      break;
    case GST_MSDKENC_PROP_ADAPTIVE_B:
      g_value_set_enum (value, thiz->adaptive_b);
      break;
    case GST_MSDKENC_PROP_EXT_CODING_PROPS:
      gst_value_set_structure (value, thiz->ext_coding_props);
      break;
    default:
      ret = FALSE;
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
  return ret;
}

/* gst_msdkenc_install_common_properties:
 * @thiz: a #GstMsdkEnc
 *
 * This is a helper function to install common properties
 * of base encoder from subclass implementation.
 * Encoders like jpeg do't require all the common properties
 * and they can avoid installing it into base gobject.
 */
void
gst_msdkenc_install_common_properties (GstMsdkEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *obj_properties[GST_MSDKENC_PROP_MAX] = { NULL, };
  guint qp_range_max = klass->qp_max;
  guint qp_range_min = klass->qp_min;

  obj_properties[GST_MSDKENC_PROP_HARDWARE] =
      g_param_spec_boolean ("hardware", "Hardware", "Enable hardware encoders",
      PROP_HARDWARE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_ASYNC_DEPTH] =
      g_param_spec_uint ("async-depth", "Async Depth",
      "Depth of asynchronous pipeline",
      1, 20, PROP_ASYNC_DEPTH_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_TARGET_USAGE] =
      g_param_spec_uint ("target-usage", "Target Usage",
      "1: Best quality, 4: Balanced, 7: Best speed",
      1, 7, PROP_TARGET_USAGE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_RATE_CONTROL] =
      g_param_spec_enum ("rate-control", "Rate Control",
      "Rate control method", gst_msdkenc_rate_control_get_type (),
      PROP_RATE_CONTROL_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_BITRATE] =
      g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in kbit/sec", 1,
      2000 * 1024, PROP_BITRATE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING);

  obj_properties[GST_MSDKENC_PROP_MAX_FRAME_SIZE] =
      g_param_spec_uint ("max-frame-size", "Max Frame Size",
      "Maximum possible size (in kbyte) of any compressed frames (0: auto-calculate)",
      0, G_MAXUINT16, PROP_MAX_FRAME_SIZE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_MAX_FRAME_SIZE_I] =
      g_param_spec_uint ("max-frame-size-i", "Max Frame Size for I frame",
      "Maximum possible size (in kbyte) of I frames (0: auto-calculate)",
      0, G_MAXUINT16, PROP_MAX_FRAME_SIZE_I_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_MAX_FRAME_SIZE_P] =
      g_param_spec_uint ("max-frame-size-p", "Max Frame Size for P frame",
      "Maximum possible size (in kbyte) of P frames (0: auto-calculate)",
      0, G_MAXUINT16, PROP_MAX_FRAME_SIZE_P_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /* Set the same upper bound with bitrate */
  obj_properties[GST_MSDKENC_PROP_MAX_VBV_BITRATE] =
      g_param_spec_uint ("max-vbv-bitrate", "Max VBV Bitrate",
      "Maximum bitrate(kbit/sec) at which data enters Video Buffering Verifier (0: auto-calculate)",
      0, 2000 * 1024, PROP_MAX_VBV_BITRATE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_AVBR_ACCURACY] =
      g_param_spec_uint ("accuracy", "Accuracy", "The AVBR Accuracy in "
      "the unit of tenth of percent", 0, G_MAXUINT16,
      PROP_AVBR_ACCURACY_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_AVBR_CONVERGENCE] =
      g_param_spec_uint ("convergence", "Convergence",
      "The AVBR Convergence in the unit of 100 frames", 0, G_MAXUINT16,
      PROP_AVBR_CONVERGENCE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_RC_LOOKAHEAD_DEPTH] =
      g_param_spec_uint ("rc-lookahead", "Look-ahead depth",
      "Number of frames to look ahead for Rate control", 10, 100,
      PROP_RC_LOOKAHEAD_DEPTH_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_QPI] =
      g_param_spec_uint ("qpi", "QPI",
      "Constant quantizer for I frames (0 unlimited). Also used as "
      "ICQQuality or QVBRQuality for different RateControl methods",
      qp_range_min, qp_range_max, PROP_QPI_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_QPP] =
      g_param_spec_uint ("qpp", "QPP",
      "Constant quantizer for P frames (0 unlimited)",
      qp_range_min, qp_range_max, PROP_QPP_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_QPB] =
      g_param_spec_uint ("qpb", "QPB",
      "Constant quantizer for B frames (0 unlimited)",
      qp_range_min, qp_range_max, PROP_QPB_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_GOP_SIZE] =
      g_param_spec_uint ("gop-size", "GOP Size", "GOP Size", 0,
      G_MAXINT, PROP_GOP_SIZE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_REF_FRAMES] =
      g_param_spec_uint ("ref-frames", "Reference Frames",
      "Number of reference frames",
      0, G_MAXINT, PROP_REF_FRAMES_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_I_FRAMES] =
      g_param_spec_uint ("i-frames", "I Frames",
      "Number of I frames between IDR frames",
      0, G_MAXINT, PROP_I_FRAMES_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_B_FRAMES] =
      g_param_spec_int ("b-frames", "B Frames",
      "Number of B frames between I and P frames",
      -1, G_MAXINT, PROP_B_FRAMES_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_NUM_SLICES] =
      g_param_spec_uint ("num-slices", "Number of Slices",
      "Number of slices per frame, Zero tells the encoder to "
      "choose any slice partitioning allowed by the codec standard",
      0, G_MAXINT, PROP_NUM_SLICES_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_MBBRC] =
      g_param_spec_enum ("mbbrc", "MB level bitrate control",
      "Macroblock level bitrate control",
      gst_msdkenc_mbbrc_get_type (),
      PROP_MBBRC_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_LOWDELAY_BRC] =
      g_param_spec_enum ("lowdelay-brc", "Low delay bitrate control",
      "Bitrate control for low-delay user scenarios",
      gst_msdkenc_lowdelay_brc_get_type (),
      PROP_LOWDELAY_BRC_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_ADAPTIVE_I] =
      g_param_spec_enum ("i-adapt", "Adaptive I-Frame Insertion",
      "Adaptive I-Frame Insertion control",
      gst_msdkenc_adaptive_i_get_type (),
      PROP_ADAPTIVE_I_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[GST_MSDKENC_PROP_ADAPTIVE_B] =
      g_param_spec_enum ("b-adapt", "Adaptive B-Frame Insertion",
      "Adaptive B-Frame Insertion control",
      gst_msdkenc_adaptive_b_get_type (),
      PROP_ADAPTIVE_B_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMsdkEnc:ext-coding-props
   *
   * The properties for the external coding.
   *
   * Supported properties:
   * ```
   * extbrc         : External bitrate control
   *                  String. Range: { auto, on, off } Default: off
   * ```
   *
   * Example:
   * ```
   * ext-coding-props="props,extbrc=on"
   * ```
   *
   * Since: 1.20
   *
   */
  obj_properties[GST_MSDKENC_PROP_EXT_CODING_PROPS] =
      g_param_spec_boxed ("ext-coding-props", "External coding properties",
      "The properties for the external coding, refer to the hotdoc for the "
      "supported properties",
      GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
      GST_MSDKENC_PROP_MAX, obj_properties);
}
