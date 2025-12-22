/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel corporation
 * All rights reserved.
 *
 * Author:Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#include "msdk-enums.h"

/*========= MSDK Decoder Enums =========================*/
GType
gst_msdkdec_output_order_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {GST_MSDKDEC_OUTPUT_ORDER_DISPLAY, "Output frames in Display order",
        "display"},
    {GST_MSDKDEC_OUTPUT_ORDER_DECODE, "Output frames in Decoded order",
        "decoded"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkDecOutputOrder", values);
  }
  return type;
}

/*========= MSDK Encoder Enums =========================*/
GType
gst_msdkenc_rate_control_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_RATECONTROL_CBR, "Constant Bitrate", "cbr"},
    {MFX_RATECONTROL_VBR, "Variable Bitrate", "vbr"},
    {MFX_RATECONTROL_CQP, "Constant Quantizer", "cqp"},
    {MFX_RATECONTROL_AVBR, "Average Bitrate", "avbr"},
    {MFX_RATECONTROL_LA, "VBR with look ahead (Non HRD compliant)", "la_vbr"},
    {MFX_RATECONTROL_ICQ, "Intelligent CQP", "icq"},
    {MFX_RATECONTROL_VCM, "Video Conferencing Mode (Non HRD compliant)", "vcm"},
    {MFX_RATECONTROL_LA_ICQ, "Intelligent CQP with LA (Non HRD compliant)",
        "la_icq"},
#if 0
    /* intended for one to N transcode scenario */
    {MFX_RATECONTROL_LA_EXT, "Extended LA", "la_ext"},
#endif
    {MFX_RATECONTROL_LA_HRD, "HRD compliant LA", "la_hrd"},
    {MFX_RATECONTROL_QVBR, "VBR with CQP", "qvbr"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncRateControl", values);
  }
  return type;
}

GType
gst_msdkenc_trellis_quantization_get_type (void)
{
  static GType type = 0;

  static const GFlagsValue values[] = {
    {_MFX_TRELLIS_NONE, "Disable for all frames", "None"},
    {MFX_TRELLIS_I, "Enable for I frames", "i"},
    {MFX_TRELLIS_P, "Enable for P frames", "p"},
    {MFX_TRELLIS_B, "Enable for B frames", "b"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_flags_register_static ("GstMsdkEncTrellisQuantization", values);
  }
  return type;
}

GType
gst_msdkenc_rc_lookahead_ds_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_LOOKAHEAD_DS_UNKNOWN, "SDK desides what to do", "default"},
    {MFX_LOOKAHEAD_DS_OFF, "No downsampling", "off"},
    {MFX_LOOKAHEAD_DS_2x, "Down sample 2-times before estimation", "2x"},
    {MFX_LOOKAHEAD_DS_4x, "Down sample 4-times before estimation", "4x"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncRCLookAheadDownsampling", values);
  }
  return type;
}

GType
gst_msdkenc_mbbrc_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "SDK desides what to do", "auto"},
    {MFX_CODINGOPTION_OFF, "Disable Macroblock level bit rate control", "off"},
    {MFX_CODINGOPTION_ON, "Enable Macroblock level bit rate control ", "on"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncMbBitrateControl", values);
  }
  return type;
}

GType
gst_msdkenc_lowdelay_brc_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "SDK decides what to do", "auto"},
    {MFX_CODINGOPTION_OFF, "Disable LowDelay bit rate control", "off"},
    {MFX_CODINGOPTION_ON, "Enable LowDelay bit rate control ", "on"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncLowDelayBitrateControl", values);
  }
  return type;
}

GType
gst_msdkenc_adaptive_i_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "SDK desides what to do", "auto"},
    {MFX_CODINGOPTION_OFF, "Disable Adaptive I frame insertion ", "off"},
    {MFX_CODINGOPTION_ON, "Enable Aaptive I frame insertion ", "on"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncAdaptiveI", values);
  }
  return type;
}

GType
gst_msdkenc_adaptive_b_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "SDK desides what to do", "auto"},
    {MFX_CODINGOPTION_OFF, "Disable Adaptive B-Frame insertion ", "off"},
    {MFX_CODINGOPTION_ON, "Enable Aaptive B-Frame insertion ", "on"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncAdaptiveB", values);
  }
  return type;
}

GType
gst_msdkenc_tune_mode_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "Auto ", "auto"},
    {MFX_CODINGOPTION_OFF, "None ", "none"},
    {MFX_CODINGOPTION_ON, "Low power mode ", "low-power"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncTuneMode", values);
  }

  return type;
}

GType
gst_msdkenc_transform_skip_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_CODINGOPTION_UNKNOWN, "SDK desides what to do", "auto"},
    {MFX_CODINGOPTION_OFF,
        "transform_skip_enabled_flag will be set to 0 in PPS ", "off"},
    {MFX_CODINGOPTION_ON,
        "transform_skip_enabled_flag will be set to 1 in PPS ", "on"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncTransformSkip", values);
  }
  return type;
}

GType
gst_msdkenc_intra_refresh_type_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_REFRESH_NO, "No (default)", "no"},
    {MFX_REFRESH_VERTICAL, "Vertical", "vertical"},
    {MFX_REFRESH_HORIZONTAL, "Horizontal ", "horizontal"},
    {MFX_REFRESH_SLICE, "Slice ", "slice"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkEncIntraRefreshType", values);
  }

  return type;
}

/*========= MSDK VPP Enums =========================*/

#ifndef GST_REMOVE_DEPRECATED
GType
gst_msdkvpp_rotation_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_ANGLE_0, "Unrotated mode", "0"},
    {MFX_ANGLE_90, "Rotated by 90°", "90"},
    {MFX_ANGLE_180, "Rotated by 180°", "180"},
    {MFX_ANGLE_270, "Rotated by 270°", "270"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPRotation", values);
  }
  return type;
}
#endif

GType
gst_msdkvpp_deinterlace_mode_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {GST_MSDKVPP_DEINTERLACE_MODE_AUTO,
        "Auto detection", "auto"},
    {GST_MSDKVPP_DEINTERLACE_MODE_INTERLACED,
        "Force deinterlacing", "interlaced"},
    {GST_MSDKVPP_DEINTERLACE_MODE_DISABLED,
        "Never deinterlace", "disabled"},
    {0, NULL, NULL},
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPDeinterlaceMode", values);
  }
  return type;
}

GType
gst_msdkvpp_deinterlace_method_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {_MFX_DEINTERLACE_METHOD_NONE,
        "Disable deinterlacing", "none"},
    {MFX_DEINTERLACING_BOB, "Bob deinterlacing", "bob"},
    {MFX_DEINTERLACING_ADVANCED, "Advanced deinterlacing (Motion adaptive)",
        "advanced"},
#if 0
    {MFX_DEINTERLACING_AUTO_DOUBLE,
          "Auto mode with deinterlacing double framerate output",
        "auto-double"},
    {MFX_DEINTERLACING_AUTO_SINGLE,
          "Auto mode with deinterlacing single framerate output",
        "auto-single"},
    {MFX_DEINTERLACING_FULL_FR_OUT,
        "Deinterlace only mode with full framerate output", "full-fr"},
    {MFX_DEINTERLACING_HALF_FR_OUT,
        "Deinterlace only Mode with half framerate output", "half-fr"},
    {MFX_DEINTERLACING_24FPS_OUT, "24 fps fixed output mode", "24-fps"},
    {MFX_DEINTERLACING_FIXED_TELECINE_PATTERN,
        "Fixed telecine pattern removal mode", "fixed-telecine-removal"},
    {MFX_DEINTERLACING_30FPS_OUT, "30 fps fixed output mode", "30-fps"},
    {MFX_DEINTERLACING_DETECT_INTERLACE, "Only interlace detection",
        "only-detect"},
#endif
    {MFX_DEINTERLACING_ADVANCED_NOREF,
          "Advanced deinterlacing mode without using of reference frames",
        "advanced-no-ref"},
    {MFX_DEINTERLACING_ADVANCED_SCD,
          "Advanced deinterlacing mode with scene change detection",
        "advanced-scd"},
    {MFX_DEINTERLACING_FIELD_WEAVING, "Field weaving", "field-weave"},
    {0, NULL, NULL},
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPDeinterlaceMethod", values);
  }
  return type;
}

#ifndef GST_REMOVE_DEPRECATED
GType
gst_msdkvpp_mirroring_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_MIRRORING_DISABLED, "Disable mirroring", "disable"},
    {MFX_MIRRORING_HORIZONTAL, "Horizontal Mirroring", "horizontal"},
    {MFX_MIRRORING_VERTICAL, "Vertical Mirroring", "vertical"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPMirroring", values);
  }
  return type;
}
#endif

GType
gst_msdkvpp_scaling_mode_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {MFX_SCALING_MODE_DEFAULT, "Default Scaling", "disable"},
    {MFX_SCALING_MODE_LOWPOWER, "Lowpower Scaling", "lowpower"},
    {MFX_SCALING_MODE_QUALITY, "High Quality Scaling", "quality"},
#if (MFX_VERSION >= 2007)
    {MFX_SCALING_MODE_INTEL_GEN_COMPUTE,
        "Compute Mode Scaling (running on EUs)", "compute"},
#endif
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPScalingMode", values);
  }
  return type;
}

GType
gst_msdkvpp_frc_algorithm_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {_MFX_FRC_ALGORITHM_NONE, "No FrameRate Control algorithm", "none"},
    {MFX_FRCALGM_PRESERVE_TIMESTAMP,
        "Frame dropping/repetition, Preserve timestamp", "preserve-ts"},
    {MFX_FRCALGM_DISTRIBUTED_TIMESTAMP,
        "Frame dropping/repetition, Distribute timestamp", "distribute-ts"},
    {MFX_FRCALGM_FRAME_INTERPOLATION, "Frame interpolation", "interpolate"},
    {MFX_FRCALGM_FRAME_INTERPOLATION | MFX_FRCALGM_PRESERVE_TIMESTAMP,
        "Frame interpolation, Preserve timestamp", "interpolate-preserve-ts"},
    {MFX_FRCALGM_FRAME_INTERPOLATION | MFX_FRCALGM_DISTRIBUTED_TIMESTAMP,
          "Frame interpolation, Distribute timestamp",
        "interpolate-distribute-ts"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstMsdkVPPFrcAlgorithm", values);
  }
  return type;
}
