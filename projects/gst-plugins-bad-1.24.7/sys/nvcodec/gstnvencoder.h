/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#endif

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#endif

#include <string.h>

#include <gst/cuda/gstcuda.h>
#include "nvEncodeAPI.h"
#include "gstnvenc.h"

G_BEGIN_DECLS

#define GST_TYPE_NV_ENCODER            (gst_nv_encoder_get_type())
#define GST_NV_ENCODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_NV_ENCODER, GstNvEncoder))
#define GST_NV_ENCODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_NV_ENCODER, GstNvEncoderClass))
#define GST_IS_NV_ENCODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_NV_ENCODER))
#define GST_IS_NV_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_NV_ENCODER))
#define GST_NV_ENCODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_NV_ENCODER, GstNvEncoderClass))
#define GST_NV_ENCODER_CAST(obj)       ((GstNvEncoder *)obj)

typedef struct _GstNvEncoder GstNvEncoder;
typedef struct _GstNvEncoderClass GstNvEncoderClass;
typedef struct _GstNvEncoderPrivate GstNvEncoderPrivate;

typedef enum
{
  GST_NV_ENCODER_RECONFIGURE_NONE,
  GST_NV_ENCODER_RECONFIGURE_BITRATE,
  GST_NV_ENCODER_RECONFIGURE_FULL,
} GstNvEncoderReconfigure;

#define GST_TYPE_NV_ENCODER_PRESET (gst_nv_encoder_preset_get_type())
GType gst_nv_encoder_preset_get_type (void);
typedef enum
{
  GST_NV_ENCODER_PRESET_DEFAULT,
  GST_NV_ENCODER_PRESET_HP,
  GST_NV_ENCODER_PRESET_HQ,
  GST_NV_ENCODER_PRESET_LOW_LATENCY_DEFAULT,
  GST_NV_ENCODER_PRESET_LOW_LATENCY_HQ,
  GST_NV_ENCODER_PRESET_LOW_LATENCY_HP,
  GST_NV_ENCODER_PRESET_LOSSLESS_DEFAULT,
  GST_NV_ENCODER_PRESET_LOSSLESS_HP,
  GST_NV_ENCODER_PRESET_P1,
  GST_NV_ENCODER_PRESET_P2,
  GST_NV_ENCODER_PRESET_P3,
  GST_NV_ENCODER_PRESET_P4,
  GST_NV_ENCODER_PRESET_P5,
  GST_NV_ENCODER_PRESET_P6,
  GST_NV_ENCODER_PRESET_P7,
} GstNvEncoderPreset;

#define GST_TYPE_NV_ENCODER_RC_MODE (gst_nv_encoder_rc_mode_get_type())
GType gst_nv_encoder_rc_mode_get_type (void);

typedef enum
{
  GST_NV_ENCODER_RC_MODE_CONSTQP,
  GST_NV_ENCODER_RC_MODE_VBR,
  GST_NV_ENCODER_RC_MODE_CBR,
  GST_NV_ENCODER_RC_MODE_CBR_LOWDELAY_HQ,
  GST_NV_ENCODER_RC_MODE_CBR_HQ,
  GST_NV_ENCODER_RC_MODE_VBR_HQ,
} GstNvEncoderRCMode;

#define GST_TYPE_NV_ENCODER_SEI_INSERT_MODE (gst_nv_encoder_sei_insert_mode_get_type ())
GType gst_nv_encoder_sei_insert_mode_get_type (void);

typedef enum
{
  GST_NV_ENCODER_SEI_INSERT,
  GST_NV_ENCODER_SEI_INSERT_AND_DROP,
  GST_NV_ENCODER_SEI_DISABLED,
} GstNvEncoderSeiInsertMode;

#define GST_TYPE_NV_ENCODER_MULTI_PASS (gst_nv_encoder_multi_pass_get_type ())
GType gst_nv_encoder_multi_pass_get_type (void);
typedef enum
{
  GST_NV_ENCODER_MULTI_PASS_DEFAULT = 0,
  GST_NV_ENCODER_MULTI_PASS_DISABLED = 1,
  GST_NV_ENCODER_TWO_PASS_QUARTER_RESOLUTION = 2,
  GST_NV_ENCODER_TWO_PASS_FULL_RESOLUTION = 3,
} GstNvEncoderMultiPass;

#define GST_TYPE_NV_ENCODER_TUNE (gst_nv_encoder_tune_get_type ())
GType gst_nv_encoder_tune_get_type (void);
typedef enum
{
  GST_NV_ENCODER_TUNE_DEFAULT = 0,
  GST_NV_ENCODER_TUNE_HIGH_QUALITY = 1,
  GST_NV_ENCODER_TUNE_LOW_LATENCY = 2,
  GST_NV_ENCODER_TUNE_ULTRA_LOW_LATENCY = 3,
  GST_NV_ENCODER_TUNE_LOSSLESS = 4,
} GstNvEncoderTune;

typedef struct
{
  gint max_bframes;
  gint ratecontrol_modes;
  gint field_encoding;
  gint monochrome;
  gint fmo;
  gint qpelmv;
  gint bdirect_mode;
  gint cabac;
  gint adaptive_transform;
  gint stereo_mvc;
  gint temoral_layers;
  gint hierarchical_pframes;
  gint hierarchical_bframes;
  gint level_max;
  gint level_min;
  gint separate_colour_plane;
  gint width_max;
  gint height_max;
  gint temporal_svc;
  gint dyn_res_change;
  gint dyn_bitrate_change;
  gint dyn_force_constqp;
  gint dyn_rcmode_change;
  gint subframe_readback;
  gint constrained_encoding;
  gint intra_refresh;
  gint custom_vbv_buf_size;
  gint dynamic_slice_mode;
  gint ref_pic_invalidation;
  gint preproc_support;
  gint async_encoding_support;
  gint mb_num_max;
  gint mb_per_sec_max;
  gint yuv444_encode;
  gint lossless_encode;
  gint sao;
  gint meonly_mode;
  gint lookahead;
  gint temporal_aq;
  gint supports_10bit_encode;
  gint num_max_ltr_frames;
  gint weighted_prediction;
  gint bframe_ref_mode;
  gint emphasis_level_map;
  gint width_min;
  gint height_min;
  gint multiple_ref_frames;
} GstNvEncoderDeviceCaps;

typedef enum
{
  GST_NV_ENCODER_DEVICE_D3D11,
  GST_NV_ENCODER_DEVICE_CUDA,
  GST_NV_ENCODER_DEVICE_AUTO_SELECT,
} GstNvEncoderDeviceMode;

typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;

  guint cuda_device_id;
  gint64 adapter_luid;

  GstNvEncoderDeviceMode device_mode;
  GstNvEncoderDeviceCaps device_caps;

  GList *formats;
  GList *profiles;

  /* auto gpu select mode */
  guint adapter_luid_size;
  gint64 adapter_luid_list[8];

  guint cuda_device_id_size;
  guint cuda_device_id_list[8];

  gint ref_count;
} GstNvEncoderClassData;

typedef struct
{
  GstNvEncoderDeviceMode device_mode;
  guint cuda_device_id;
  gint64 adapter_luid;
  GstObject *device;
} GstNvEncoderDeviceData;

struct _GstNvEncoder
{
  GstVideoEncoder parent;

  GstNvEncoderPrivate *priv;
};

struct _GstNvEncoderClass
{
  GstVideoEncoderClass parent_class;

  gboolean    (*set_format)           (GstNvEncoder * encoder,
                                       GstVideoCodecState * state,
                                       gpointer session,
                                       NV_ENC_INITIALIZE_PARAMS * init_params,
                                       NV_ENC_CONFIG * config);

  gboolean    (*set_output_state)     (GstNvEncoder * encoder,
                                       GstVideoCodecState * state,
                                       gpointer session);

  GstBuffer * (*create_output_buffer) (GstNvEncoder * encoder,
                                       NV_ENC_LOCK_BITSTREAM * bitstream);

  GstNvEncoderReconfigure (*check_reconfigure)  (GstNvEncoder * encoder,
                                                 NV_ENC_CONFIG * config);

  gboolean    (*select_device)        (GstNvEncoder * encoder,
                                       const GstVideoInfo * info,
                                       GstBuffer * buffer,
                                       GstNvEncoderDeviceData * data);

  guint       (*calculate_min_buffers) (GstNvEncoder * encoder);
};

GType gst_nv_encoder_get_type (void);

void gst_nv_encoder_preset_to_native (GstNvEncoderPreset preset,
                                      GstNvEncoderTune tune,
                                      GUID * preset_guid,
                                      NV_ENC_TUNING_INFO * tune_info);

void gst_nv_encoder_rc_mode_to_native (GstNvEncoderRCMode rc_mode,
                                       GstNvEncoderMultiPass multipass,
                                       NV_ENC_PARAMS_RC_MODE * rc_mode_native,
                                       NV_ENC_MULTI_PASS * multipass_native);


void gst_nv_encoder_set_device_mode (GstNvEncoder * encoder,
                                     GstNvEncoderDeviceMode mode,
                                     guint cuda_device_id,
                                     gint64 adapter_luid);

GstNvEncoderClassData * gst_nv_encoder_class_data_new (void);

GstNvEncoderClassData * gst_nv_encoder_class_data_ref (GstNvEncoderClassData * cdata);

void gst_nv_encoder_class_data_unref (GstNvEncoderClassData * cdata);

void gst_nv_encoder_get_encoder_caps (gpointer session,
                                      const GUID * encode_guid,
                                      GstNvEncoderDeviceCaps * device_caps);

void gst_nv_encoder_merge_device_caps (const GstNvEncoderDeviceCaps * a,
                                       const GstNvEncoderDeviceCaps * b,
                                       GstNvEncoderDeviceCaps * merged);

gboolean _gst_nv_enc_result (NVENCSTATUS status,
                             GObject * self,
                             const gchar * file,
                             const gchar * function,
                             gint line);

#define gst_nv_enc_result(status,self) \
    _gst_nv_enc_result (status, (GObject *) self, __FILE__, GST_FUNCTION, __LINE__)

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstNvEncoder, gst_object_unref)

G_END_DECLS

#ifdef __cplusplus
#ifndef G_OS_WIN32
inline bool is_equal_guid(const GUID & lhs, const GUID & rhs)
{
  return memcmp(&lhs, &rhs, sizeof (GUID)) == 0;
}

inline bool operator==(const GUID & lhs, const GUID & rhs)
{
  return is_equal_guid(lhs, rhs);
}

inline bool operator!=(const GUID & lhs, const GUID & rhs)
{
  return !is_equal_guid(lhs, rhs);
}
#endif /* G_OS_WIN32 */
#endif /* __cplusplus */
