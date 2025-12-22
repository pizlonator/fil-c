/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
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

#include "gstnvenc.h"
#include "gstnvh264enc.h"
#include "gstnvh265enc.h"
#include <gst/cuda/gstcudautils.h>
#include <gst/cuda/gstcudabufferpool.h>
#include <string.h>

#include <gmodule.h>

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#endif

#ifdef _WIN32
#ifdef _WIN64
#define NVENC_LIBRARY_NAME "nvEncodeAPI64.dll"
#else
#define NVENC_LIBRARY_NAME "nvEncodeAPI.dll"
#endif
#else
#define NVENC_LIBRARY_NAME "libnvidia-encode.so.1"
#endif

/* For backward compatibility */
#define GST_NVENC_MIN_API_MAJOR_VERSION 8
#define GST_NVENC_MIN_API_MINOR_VERSION 1

#define GST_NVENCAPI_VERSION(major,minor) ((major) | ((minor) << 24))
#define GST_NVENCAPI_STRUCT_VERSION(ver,api_ver) ((uint32_t)(api_ver) | ((ver)<<16) | (0x7 << 28))

static guint32 gst_nvenc_api_version = NVENCAPI_VERSION;
static gboolean gst_nvenc_supports_cuda_stream = FALSE;

typedef NVENCSTATUS NVENCAPI
tNvEncodeAPICreateInstance (NV_ENCODE_API_FUNCTION_LIST * functionList);
tNvEncodeAPICreateInstance *nvEncodeAPICreateInstance;

typedef NVENCSTATUS NVENCAPI
tNvEncodeAPIGetMaxSupportedVersion (uint32_t * version);
tNvEncodeAPIGetMaxSupportedVersion *nvEncodeAPIGetMaxSupportedVersion;

GST_DEBUG_CATEGORY_EXTERN (gst_nvenc_debug);
#define GST_CAT_DEFAULT gst_nvenc_debug

static NV_ENCODE_API_FUNCTION_LIST nvenc_api;

NVENCSTATUS NVENCAPI
NvEncOpenEncodeSessionEx (NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS * params,
    void **encoder)
{
  g_assert (nvenc_api.nvEncOpenEncodeSessionEx != NULL);
  return nvenc_api.nvEncOpenEncodeSessionEx (params, encoder);
}

NVENCSTATUS NVENCAPI
NvEncDestroyEncoder (void *encoder)
{
  g_assert (nvenc_api.nvEncDestroyEncoder != NULL);
  return nvenc_api.nvEncDestroyEncoder (encoder);
}

const char *NVENCAPI
NvEncGetLastErrorString (void *encoder)
{
  g_assert (nvenc_api.nvEncGetLastErrorString != NULL);
  return nvenc_api.nvEncGetLastErrorString (encoder);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodeGUIDs (void *encoder, GUID * array, uint32_t array_size,
    uint32_t * count)
{
  g_assert (nvenc_api.nvEncGetEncodeGUIDs != NULL);
  return nvenc_api.nvEncGetEncodeGUIDs (encoder, array, array_size, count);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodeProfileGUIDCount (void *encoder, GUID encodeGUID,
    uint32_t * encodeProfileGUIDCount)
{
  g_assert (nvenc_api.nvEncGetEncodeProfileGUIDCount != NULL);
  return nvenc_api.nvEncGetEncodeProfileGUIDCount (encoder, encodeGUID,
      encodeProfileGUIDCount);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodeProfileGUIDs (void *encoder, GUID encodeGUID,
    GUID * profileGUIDs, uint32_t guidArraySize, uint32_t * GUIDCount)
{
  g_assert (nvenc_api.nvEncGetEncodeProfileGUIDs != NULL);
  return nvenc_api.nvEncGetEncodeProfileGUIDs (encoder, encodeGUID,
      profileGUIDs, guidArraySize, GUIDCount);
}

NVENCSTATUS NVENCAPI
NvEncGetInputFormats (void *encoder, GUID enc_guid,
    NV_ENC_BUFFER_FORMAT * array, uint32_t size, uint32_t * num)
{
  g_assert (nvenc_api.nvEncGetInputFormats != NULL);
  return nvenc_api.nvEncGetInputFormats (encoder, enc_guid, array, size, num);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodePresetCount (void *encoder, GUID encodeGUID,
    uint32_t * encodePresetGUIDCount)
{
  g_assert (nvenc_api.nvEncGetEncodeProfileGUIDCount != NULL);
  return nvenc_api.nvEncGetEncodePresetCount (encoder, encodeGUID,
      encodePresetGUIDCount);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodePresetGUIDs (void *encoder, GUID encodeGUID,
    GUID * presetGUIDs, uint32_t guidArraySize, uint32_t * GUIDCount)
{
  g_assert (nvenc_api.nvEncGetEncodeProfileGUIDs != NULL);
  return nvenc_api.nvEncGetEncodePresetGUIDs (encoder, encodeGUID,
      presetGUIDs, guidArraySize, GUIDCount);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodePresetConfig (void *encoder, GUID encodeGUID,
    GUID presetGUID, NV_ENC_PRESET_CONFIG * presetConfig)
{
  g_assert (nvenc_api.nvEncGetEncodePresetConfig != NULL);
  return nvenc_api.nvEncGetEncodePresetConfig (encoder, encodeGUID, presetGUID,
      presetConfig);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodePresetConfigEx (void *encoder, GUID encodeGUID,
    GUID presetGUID, NV_ENC_TUNING_INFO tuningInfo,
    NV_ENC_PRESET_CONFIG * presetConfig)
{
  if (!nvenc_api.nvEncGetEncodePresetConfigEx)
    return NV_ENC_ERR_UNIMPLEMENTED;

  return nvenc_api.nvEncGetEncodePresetConfigEx (encoder, encodeGUID,
      presetGUID, tuningInfo, presetConfig);
}

NVENCSTATUS NVENCAPI
NvEncGetEncodeCaps (void *encoder, GUID encodeGUID,
    NV_ENC_CAPS_PARAM * capsParam, int *capsVal)
{
  g_assert (nvenc_api.nvEncGetEncodeCaps != NULL);
  return nvenc_api.nvEncGetEncodeCaps (encoder, encodeGUID, capsParam, capsVal);
}

NVENCSTATUS NVENCAPI
NvEncGetSequenceParams (void *encoder,
    NV_ENC_SEQUENCE_PARAM_PAYLOAD * sequenceParamPayload)
{
  g_assert (nvenc_api.nvEncGetSequenceParams != NULL);
  return nvenc_api.nvEncGetSequenceParams (encoder, sequenceParamPayload);
}

NVENCSTATUS NVENCAPI
NvEncInitializeEncoder (void *encoder, NV_ENC_INITIALIZE_PARAMS * params)
{
  g_assert (nvenc_api.nvEncInitializeEncoder != NULL);
  return nvenc_api.nvEncInitializeEncoder (encoder, params);
}

NVENCSTATUS NVENCAPI
NvEncReconfigureEncoder (void *encoder, NV_ENC_RECONFIGURE_PARAMS * params)
{
  g_assert (nvenc_api.nvEncReconfigureEncoder != NULL);
  return nvenc_api.nvEncReconfigureEncoder (encoder, params);
}

NVENCSTATUS NVENCAPI
NvEncRegisterResource (void *encoder, NV_ENC_REGISTER_RESOURCE * params)
{
  g_assert (nvenc_api.nvEncRegisterResource != NULL);
  return nvenc_api.nvEncRegisterResource (encoder, params);
}

NVENCSTATUS NVENCAPI
NvEncUnregisterResource (void *encoder, NV_ENC_REGISTERED_PTR resource)
{
  g_assert (nvenc_api.nvEncUnregisterResource != NULL);
  return nvenc_api.nvEncUnregisterResource (encoder, resource);
}

NVENCSTATUS NVENCAPI
NvEncMapInputResource (void *encoder, NV_ENC_MAP_INPUT_RESOURCE * params)
{
  g_assert (nvenc_api.nvEncMapInputResource != NULL);
  return nvenc_api.nvEncMapInputResource (encoder, params);
}

NVENCSTATUS NVENCAPI
NvEncUnmapInputResource (void *encoder, NV_ENC_INPUT_PTR input_buffer)
{
  g_assert (nvenc_api.nvEncUnmapInputResource != NULL);
  return nvenc_api.nvEncUnmapInputResource (encoder, input_buffer);
}

NVENCSTATUS NVENCAPI
NvEncCreateInputBuffer (void *encoder, NV_ENC_CREATE_INPUT_BUFFER * input_buf)
{
  g_assert (nvenc_api.nvEncCreateInputBuffer != NULL);
  return nvenc_api.nvEncCreateInputBuffer (encoder, input_buf);
}

NVENCSTATUS NVENCAPI
NvEncLockInputBuffer (void *encoder, NV_ENC_LOCK_INPUT_BUFFER * input_buf)
{
  g_assert (nvenc_api.nvEncLockInputBuffer != NULL);
  return nvenc_api.nvEncLockInputBuffer (encoder, input_buf);
}

NVENCSTATUS NVENCAPI
NvEncUnlockInputBuffer (void *encoder, NV_ENC_INPUT_PTR input_buf)
{
  g_assert (nvenc_api.nvEncUnlockInputBuffer != NULL);
  return nvenc_api.nvEncUnlockInputBuffer (encoder, input_buf);
}

NVENCSTATUS NVENCAPI
NvEncDestroyInputBuffer (void *encoder, NV_ENC_INPUT_PTR input_buf)
{
  g_assert (nvenc_api.nvEncDestroyInputBuffer != NULL);
  return nvenc_api.nvEncDestroyInputBuffer (encoder, input_buf);
}

NVENCSTATUS NVENCAPI
NvEncCreateBitstreamBuffer (void *encoder, NV_ENC_CREATE_BITSTREAM_BUFFER * bb)
{
  g_assert (nvenc_api.nvEncCreateBitstreamBuffer != NULL);
  return nvenc_api.nvEncCreateBitstreamBuffer (encoder, bb);
}

NVENCSTATUS NVENCAPI
NvEncLockBitstream (void *encoder, NV_ENC_LOCK_BITSTREAM * lock_bs)
{
  g_assert (nvenc_api.nvEncLockBitstream != NULL);
  return nvenc_api.nvEncLockBitstream (encoder, lock_bs);
}

NVENCSTATUS NVENCAPI
NvEncUnlockBitstream (void *encoder, NV_ENC_OUTPUT_PTR bb)
{
  g_assert (nvenc_api.nvEncUnlockBitstream != NULL);
  return nvenc_api.nvEncUnlockBitstream (encoder, bb);
}

NVENCSTATUS NVENCAPI
NvEncDestroyBitstreamBuffer (void *encoder, NV_ENC_OUTPUT_PTR bit_buf)
{
  g_assert (nvenc_api.nvEncDestroyBitstreamBuffer != NULL);
  return nvenc_api.nvEncDestroyBitstreamBuffer (encoder, bit_buf);
}

NVENCSTATUS NVENCAPI
NvEncEncodePicture (void *encoder, NV_ENC_PIC_PARAMS * pic_params)
{
  g_assert (nvenc_api.nvEncEncodePicture != NULL);
  return nvenc_api.nvEncEncodePicture (encoder, pic_params);
}

NVENCSTATUS NVENCAPI
NvEncRegisterAsyncEvent (void *encoder, NV_ENC_EVENT_PARAMS * event_params)
{
  g_assert (nvenc_api.nvEncRegisterAsyncEvent != NULL);
  return nvenc_api.nvEncRegisterAsyncEvent (encoder, event_params);
}

NVENCSTATUS NVENCAPI
NvEncUnregisterAsyncEvent (void *encoder, NV_ENC_EVENT_PARAMS * event_params)
{
  g_assert (nvenc_api.nvEncUnregisterAsyncEvent != NULL);
  return nvenc_api.nvEncUnregisterAsyncEvent (encoder, event_params);
}

NVENCSTATUS NVENCAPI
NvEncSetIOCudaStreams (void *encoder, NV_ENC_CUSTREAM_PTR input_stream,
    NV_ENC_CUSTREAM_PTR output_stream)
{
  g_assert (nvenc_api.nvEncSetIOCudaStreams != NULL);
  return nvenc_api.nvEncSetIOCudaStreams (encoder, input_stream, output_stream);
}

gboolean
gst_nvenc_cmp_guid (GUID g1, GUID g2)
{
  return (g1.Data1 == g2.Data1 && g1.Data2 == g2.Data2 && g1.Data3 == g2.Data3
      && g1.Data4[0] == g2.Data4[0] && g1.Data4[1] == g2.Data4[1]
      && g1.Data4[2] == g2.Data4[2] && g1.Data4[3] == g2.Data4[3]
      && g1.Data4[4] == g2.Data4[4] && g1.Data4[5] == g2.Data4[5]
      && g1.Data4[6] == g2.Data4[6] && g1.Data4[7] == g2.Data4[7]);
}

NV_ENC_BUFFER_FORMAT
gst_nvenc_get_nv_buffer_format (GstVideoFormat fmt)
{
  switch (fmt) {
    case GST_VIDEO_FORMAT_NV12:
      return NV_ENC_BUFFER_FORMAT_NV12_PL;
    case GST_VIDEO_FORMAT_YV12:
      return NV_ENC_BUFFER_FORMAT_YV12_PL;
    case GST_VIDEO_FORMAT_I420:
      return NV_ENC_BUFFER_FORMAT_IYUV_PL;
    case GST_VIDEO_FORMAT_Y444:
      return NV_ENC_BUFFER_FORMAT_YUV444_PL;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
      return NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
    case GST_VIDEO_FORMAT_BGRA:
      return NV_ENC_BUFFER_FORMAT_ARGB;
    case GST_VIDEO_FORMAT_RGBA:
      return NV_ENC_BUFFER_FORMAT_ABGR;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      return NV_ENC_BUFFER_FORMAT_ARGB10;
    case GST_VIDEO_FORMAT_RGB10A2_LE:
      return NV_ENC_BUFFER_FORMAT_ABGR10;
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_Y444_16BE:
      return NV_ENC_BUFFER_FORMAT_YUV444_10BIT;
    case GST_VIDEO_FORMAT_VUYA:
      return NV_ENC_BUFFER_FORMAT_AYUV;
    default:
      break;
  }
  return NV_ENC_BUFFER_FORMAT_UNDEFINED;
}

typedef struct
{
  GstVideoFormat gst_format;
  NV_ENC_BUFFER_FORMAT nv_format;
  gboolean is_10bit;

  gboolean supported;
} GstNvEncFormat;

gboolean
gst_nvenc_get_supported_input_formats (gpointer encoder, GUID codec_id,
    GValue ** formats)
{
  guint32 i, count = 0;
  NV_ENC_BUFFER_FORMAT format_list[64];
  GValue val = G_VALUE_INIT;
  GValue *ret = NULL;
  NV_ENC_CAPS_PARAM param = { 0, };
  gint support_yuv444 = 0;
  gint support_10bit = 0;
  guint num_format = 0;
  GstNvEncFormat format_map[] = {
    {GST_VIDEO_FORMAT_NV12, NV_ENC_BUFFER_FORMAT_NV12, FALSE, FALSE},
    {GST_VIDEO_FORMAT_YV12, NV_ENC_BUFFER_FORMAT_YV12, FALSE, FALSE},
    {GST_VIDEO_FORMAT_I420, NV_ENC_BUFFER_FORMAT_IYUV, FALSE, FALSE},
    {GST_VIDEO_FORMAT_BGRA, NV_ENC_BUFFER_FORMAT_ARGB, FALSE, FALSE},
    {GST_VIDEO_FORMAT_RGBA, NV_ENC_BUFFER_FORMAT_ABGR, FALSE, FALSE},
    {GST_VIDEO_FORMAT_Y444, NV_ENC_BUFFER_FORMAT_YUV444, FALSE, FALSE},
    {GST_VIDEO_FORMAT_VUYA, NV_ENC_BUFFER_FORMAT_AYUV, FALSE, FALSE},
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    {GST_VIDEO_FORMAT_P010_10LE, NV_ENC_BUFFER_FORMAT_YUV420_10BIT, TRUE,
        FALSE},
    {GST_VIDEO_FORMAT_BGR10A2_LE, NV_ENC_BUFFER_FORMAT_ARGB10, TRUE,
        FALSE},
    {GST_VIDEO_FORMAT_RGB10A2_LE, NV_ENC_BUFFER_FORMAT_ABGR10, TRUE,
        FALSE},
    {GST_VIDEO_FORMAT_Y444_16LE, NV_ENC_BUFFER_FORMAT_YUV444_10BIT, TRUE,
        FALSE},
#else
    {GST_VIDEO_FORMAT_P010_10BE, NV_ENC_BUFFER_FORMAT_YUV420_10BIT, TRUE,
        FALSE},
    {GST_VIDEO_FORMAT_Y444_16BE, NV_ENC_BUFFER_FORMAT_YUV444_10BIT, TRUE,
        FALSE},
    /* FIXME: No 10bits big-endian ARGB10 format is defined */
#endif
  };

  param.version = gst_nvenc_get_caps_param_version ();
  param.capsToQuery = NV_ENC_CAPS_SUPPORT_YUV444_ENCODE;
  if (NvEncGetEncodeCaps (encoder,
          codec_id, &param, &support_yuv444) != NV_ENC_SUCCESS) {
    support_yuv444 = 0;
  }

  param.capsToQuery = NV_ENC_CAPS_SUPPORT_10BIT_ENCODE;
  if (NvEncGetEncodeCaps (encoder,
          codec_id, &param, &support_10bit) != NV_ENC_SUCCESS) {
    support_10bit = 0;
  }

  if (NvEncGetInputFormats (encoder,
          codec_id, format_list, G_N_ELEMENTS (format_list),
          &count) != NV_ENC_SUCCESS || count == 0) {
    return FALSE;
  }

  for (i = 0; i < count; i++) {
    GST_INFO ("input format: 0x%08x", format_list[i]);
    switch (format_list[i]) {
      case NV_ENC_BUFFER_FORMAT_NV12:
      case NV_ENC_BUFFER_FORMAT_YV12:
      case NV_ENC_BUFFER_FORMAT_IYUV:
      case NV_ENC_BUFFER_FORMAT_ARGB:
      case NV_ENC_BUFFER_FORMAT_ABGR:
        if (!format_map[i].supported) {
          format_map[i].supported = TRUE;
          num_format++;
        }
        break;
      case NV_ENC_BUFFER_FORMAT_YUV444:
      case NV_ENC_BUFFER_FORMAT_AYUV:
        if (support_yuv444 && !format_map[i].supported) {
          format_map[i].supported = TRUE;
          num_format++;
        }
        break;
      case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        if (support_10bit && !format_map[i].supported) {
          format_map[i].supported = TRUE;
          num_format++;
        }
        break;
      case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        if (support_yuv444 && support_10bit && !format_map[i].supported) {
          format_map[i].supported = TRUE;
          num_format++;
        }
        break;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      case NV_ENC_BUFFER_FORMAT_ARGB10:
      case NV_ENC_BUFFER_FORMAT_ABGR10:
        if (support_10bit && !format_map[i].supported) {
          format_map[i].supported = TRUE;
          num_format++;
        }
        break;
#endif
      default:
        GST_FIXME ("unmapped input format: 0x%08x", format_list[i]);
        break;
    }
  }

  if (num_format == 0)
    return FALSE;

  /* process a second time so we can add formats in the order we want */
  g_value_init (&val, G_TYPE_STRING);
  ret = g_new0 (GValue, 1);
  g_value_init (ret, GST_TYPE_LIST);

  for (i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (!format_map[i].supported)
      continue;

    g_value_set_static_string (&val,
        gst_video_format_to_string (format_map[i].gst_format));

    gst_value_list_append_value (ret, &val);
  }

  g_value_unset (&val);

  *formats = ret;

  return TRUE;
}

GValue *
gst_nvenc_get_interlace_modes (gpointer enc, GUID codec_id)
{
  NV_ENC_CAPS_PARAM caps_param = { 0, };
  GValue *list;
  GValue val = G_VALUE_INIT;
  gint interlace_modes = 0;

  caps_param.version = gst_nvenc_get_caps_param_version ();
  caps_param.capsToQuery = NV_ENC_CAPS_SUPPORT_FIELD_ENCODING;

  if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
          &interlace_modes) != NV_ENC_SUCCESS)
    interlace_modes = 0;

  list = g_new0 (GValue, 1);

  g_value_init (list, GST_TYPE_LIST);
  g_value_init (&val, G_TYPE_STRING);

  g_value_set_static_string (&val, "progressive");
  gst_value_list_append_value (list, &val);

  if (interlace_modes == 0)
    return list;

  if (interlace_modes >= 1) {
    g_value_set_static_string (&val, "interleaved");
    gst_value_list_append_value (list, &val);
    g_value_set_static_string (&val, "mixed");
    gst_value_list_append_value (list, &val);
    g_value_unset (&val);
  }
  /* TODO: figure out what nvenc frame based interlacing means in gst terms */

  return list;
}

typedef struct
{
  const gchar *gst_profile;
  const GUID nv_profile;
  const GUID codec_id;
  const gboolean need_yuv444;
  const gboolean need_10bit;

  gboolean supported;
} GstNvEncCodecProfile;

GValue *
gst_nvenc_get_supported_codec_profiles (gpointer enc, GUID codec_id)
{
  NVENCSTATUS nv_ret;
  GUID profile_guids[64];
  GValue *ret;
  GValue val = G_VALUE_INIT;
  guint i, j, n, n_profiles;
  NV_ENC_CAPS_PARAM param = { 0, };
  gint support_yuv444 = 0;
  gint support_10bit = 0;
  GstNvEncCodecProfile profiles[] = {
    /* avc profiles */
    {"main", NV_ENC_H264_PROFILE_MAIN_GUID, NV_ENC_CODEC_H264_GUID, FALSE,
        FALSE, FALSE},
    {"high", NV_ENC_H264_PROFILE_HIGH_GUID, NV_ENC_CODEC_H264_GUID, FALSE,
        FALSE, FALSE},
    {"high-4:4:4", NV_ENC_H264_PROFILE_HIGH_444_GUID, NV_ENC_CODEC_H264_GUID,
        TRUE, FALSE, FALSE},
    /* put baseline to last since it does not support bframe */
    {"baseline", NV_ENC_H264_PROFILE_BASELINE_GUID, NV_ENC_CODEC_H264_GUID,
        FALSE, FALSE, FALSE},
    {"constrained-baseline", NV_ENC_H264_PROFILE_BASELINE_GUID,
          NV_ENC_CODEC_H264_GUID,
        FALSE, FALSE, FALSE},
    /* hevc profiles */
    {"main", NV_ENC_HEVC_PROFILE_MAIN_GUID, NV_ENC_CODEC_HEVC_GUID, FALSE,
        FALSE, FALSE},
    {"main-10", NV_ENC_HEVC_PROFILE_MAIN10_GUID, NV_ENC_CODEC_HEVC_GUID, FALSE,
        TRUE, FALSE},
    {"main-444", NV_ENC_HEVC_PROFILE_FREXT_GUID, NV_ENC_CODEC_HEVC_GUID, TRUE,
        FALSE, FALSE},
#if 0
    /* FIXME: seems to unsupported format */
    {"main-444-10", NV_ENC_HEVC_PROFILE_FREXT_GUID, FALSE}
#endif
  };

  param.version = gst_nvenc_get_caps_param_version ();
  param.capsToQuery = NV_ENC_CAPS_SUPPORT_YUV444_ENCODE;
  if (NvEncGetEncodeCaps (enc,
          codec_id, &param, &support_yuv444) != NV_ENC_SUCCESS) {
    support_yuv444 = 0;
  }

  param.capsToQuery = NV_ENC_CAPS_SUPPORT_10BIT_ENCODE;
  if (NvEncGetEncodeCaps (enc,
          codec_id, &param, &support_10bit) != NV_ENC_SUCCESS) {
    support_10bit = 0;
  }

  nv_ret = NvEncGetEncodeProfileGUIDCount (enc, codec_id, &n);

  if (nv_ret != NV_ENC_SUCCESS)
    return NULL;

  nv_ret = NvEncGetEncodeProfileGUIDs (enc,
      codec_id, profile_guids, G_N_ELEMENTS (profile_guids), &n);

  if (nv_ret != NV_ENC_SUCCESS)
    return NULL;

  n_profiles = 0;

  for (i = 0; i < n; i++) {
    for (j = 0; j < G_N_ELEMENTS (profiles); j++) {
      if (profiles[j].supported == FALSE &&
          gst_nvenc_cmp_guid (profile_guids[i], profiles[j].nv_profile) &&
          gst_nvenc_cmp_guid (codec_id, profiles[j].codec_id)) {
        if (profiles[j].need_yuv444 && !support_yuv444)
          continue;

        if (profiles[j].need_10bit && !support_10bit)
          continue;

        profiles[j].supported = TRUE;
        n_profiles++;
      }
    }
  }

  if (n_profiles == 0)
    return NULL;

  ret = g_new0 (GValue, 1);

  g_value_init (ret, GST_TYPE_LIST);
  g_value_init (&val, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (profiles); i++) {
    if (!profiles[i].supported)
      continue;

    g_value_set_static_string (&val, profiles[i].gst_profile);
    gst_value_list_append_value (ret, &val);
  }

  g_value_unset (&val);

  return ret;
}

#define DEBUG_DEVICE_CAPS(d,c,caps,s) \
  GST_DEBUG ("[device-%d %s] %s: %s", \
      d, c, caps, s ? "supported" : "not supported");

#define ERROR_DETAILS "codec %s, device %i, error code %i"

static void
gst_nv_enc_register (GstPlugin * plugin, GUID codec_id, const gchar * codec,
    guint rank, gint device_index, CUcontext cuda_ctx)
{
  {
    GValue *formats = NULL;
    GValue *profiles;
    GValue *interlace_modes;
    gpointer enc;
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params = { 0, };
    NV_ENC_CAPS_PARAM caps_param = { 0, };
    GUID guids[16];
    guint32 count;
    gint max_width = 0;
    gint max_height = 0;
    gint min_width = 16;
    gint min_height = 16;
    GstCaps *sink_templ = NULL;
    GstCaps *src_templ = NULL;
    gchar *name;
    gint j;
    GstNvEncDeviceCaps device_caps = { 0, };
    NVENCSTATUS status;
    CUresult cu_res;

    params.version = gst_nvenc_get_open_encode_session_ex_params_version ();
    params.apiVersion = gst_nvenc_get_api_version ();
    params.device = cuda_ctx;
    params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

    if ((cu_res = CuCtxPushCurrent (cuda_ctx)) != CUDA_SUCCESS) {
      GST_ERROR ("CuCtxPushCurrent failed: " ERROR_DETAILS, codec,
          device_index, cu_res);
      goto done;
    }

    if ((status = NvEncOpenEncodeSessionEx (&params, &enc)) != NV_ENC_SUCCESS) {
      CuCtxPopCurrent (NULL);
      GST_ERROR ("NvEncOpenEncodeSessionEx failed: " ERROR_DETAILS, codec,
          device_index, status);
      goto done;
    }

    if ((status = NvEncGetEncodeGUIDs (enc, guids, G_N_ELEMENTS (guids),
                &count)) != NV_ENC_SUCCESS) {
      GST_ERROR ("NvEncGetEncodeGUIDs failed: " ERROR_DETAILS, codec,
          device_index, status);
      goto enc_free;
    }

    for (j = 0; j < count; j++) {
      if (gst_nvenc_cmp_guid (guids[j], codec_id))
        break;
    }

    if (j == count)
      goto enc_free;

    if (!gst_nvenc_get_supported_input_formats (enc, codec_id, &formats))
      goto enc_free;

    profiles = gst_nvenc_get_supported_codec_profiles (enc, codec_id);
    if (!profiles)
      goto free_format;

    caps_param.version = gst_nvenc_get_caps_param_version ();
    caps_param.capsToQuery = NV_ENC_CAPS_WIDTH_MAX;
    if ((status = NvEncGetEncodeCaps (enc,
                codec_id, &caps_param, &max_width)) != NV_ENC_SUCCESS) {
      max_width = 4096;
      GST_WARNING ("could not query max width, setting as %i: "
          ERROR_DETAILS, max_width, codec, device_index, status);
    } else if (max_width < 4096) {
      GST_WARNING ("max width %d is less than expected value", max_width);
      max_width = 4096;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_HEIGHT_MAX;
    if ((status = NvEncGetEncodeCaps (enc,
                codec_id, &caps_param, &max_height)) != NV_ENC_SUCCESS) {
      GST_WARNING ("could not query max height, setting as %i: "
          ERROR_DETAILS, max_height, codec, device_index, status);
      max_height = 4096;
    } else if (max_height < 4096) {
      GST_WARNING ("max height %d is less than expected value", max_height);
      max_height = 4096;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_WIDTH_MIN;
    if ((status = NvEncGetEncodeCaps (enc,
                codec_id, &caps_param, &min_width)) != NV_ENC_SUCCESS) {
      GST_WARNING ("could not query min width, setting as %i: "
          ERROR_DETAILS, min_width, codec, device_index, status);
      min_width = 16;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_HEIGHT_MIN;
    if ((status = NvEncGetEncodeCaps (enc,
                codec_id, &caps_param, &min_height)) != NV_ENC_SUCCESS) {
      GST_WARNING ("could not query min height, setting as %i: "
          ERROR_DETAILS, min_height, codec, device_index, status);
      min_height = 16;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES;
    if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
            &device_caps.rc_modes) != NV_ENC_SUCCESS) {
      device_caps.rc_modes = 0;
    } else {
      GST_DEBUG ("[device-%d %s] rate control modes: 0x%x",
          device_index, codec, device_caps.rc_modes);
#define IS_SUPPORTED_RC(rc_modes,mode) \
      ((((rc_modes) & (mode)) == mode) ? "supported" : "not supported")

      GST_DEBUG ("\tconst-qp:         %s",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_CONSTQP));
      GST_DEBUG ("\tvbr:              %s",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_VBR));
      GST_DEBUG ("\tcbr:              %s",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_CBR));
      GST_DEBUG ("\tcbr-lowdelay-hq:  %s",
          IS_SUPPORTED_RC (device_caps.rc_modes,
              NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ));
      GST_DEBUG ("\tcbr-hq:           %s",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_CBR_HQ));
      GST_DEBUG ("\tvbr-hq:           %s",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_VBR_HQ));
      GST_DEBUG ("\tvbr-minqp:        %s (deprecated)",
          IS_SUPPORTED_RC (device_caps.rc_modes, NV_ENC_PARAMS_RC_VBR_MINQP));
#undef IS_SUPPORTED_RC
    }

    caps_param.capsToQuery = NV_ENC_CAPS_SUPPORT_WEIGHTED_PREDICTION;
    if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
            &device_caps.weighted_prediction) != NV_ENC_SUCCESS) {
      device_caps.weighted_prediction = FALSE;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE;
    if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
            &device_caps.custom_vbv_bufsize) != NV_ENC_SUCCESS) {
      device_caps.custom_vbv_bufsize = FALSE;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_SUPPORT_LOOKAHEAD;
    if (NvEncGetEncodeCaps (enc,
            codec_id, &caps_param, &device_caps.lookahead) != NV_ENC_SUCCESS) {
      device_caps.lookahead = FALSE;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ;
    if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
            &device_caps.temporal_aq) != NV_ENC_SUCCESS) {
      device_caps.temporal_aq = FALSE;
    }

    caps_param.capsToQuery = NV_ENC_CAPS_NUM_MAX_BFRAMES;
    if (NvEncGetEncodeCaps (enc, codec_id, &caps_param,
            &device_caps.bframes) != NV_ENC_SUCCESS) {
      device_caps.bframes = 0;
    }

    DEBUG_DEVICE_CAPS (device_index,
        codec, "weighted prediction", device_caps.weighted_prediction);

    DEBUG_DEVICE_CAPS (device_index, codec, "custom vbv-buffer-size",
        device_caps.custom_vbv_bufsize);

    DEBUG_DEVICE_CAPS (device_index, codec, "rc-loockahead",
        device_caps.lookahead);

    DEBUG_DEVICE_CAPS (device_index, codec, "temporal adaptive quantization",
        device_caps.temporal_aq);

    GST_DEBUG ("[device-%d %s] max bframes: %d", device_index, codec,
        device_caps.bframes);

    interlace_modes = gst_nvenc_get_interlace_modes (enc, codec_id);

    sink_templ = gst_caps_new_empty_simple ("video/x-raw");
    gst_caps_set_value (sink_templ, "format", formats);

    gst_caps_set_simple (sink_templ,
        "width", GST_TYPE_INT_RANGE, min_width, max_width,
        "height", GST_TYPE_INT_RANGE, min_height, max_height,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

    if (interlace_modes) {
      gst_caps_set_value (sink_templ, "interlace-mode", interlace_modes);
      g_value_unset (interlace_modes);
      g_free (interlace_modes);
    }

    {
      GstCaps *cuda_caps = gst_caps_copy (sink_templ);
#ifdef HAVE_CUDA_GST_GL
      GstCaps *gl_caps = gst_caps_copy (sink_templ);
      gst_caps_set_features_simple (gl_caps,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
      gst_caps_append (sink_templ, gl_caps);
#endif

      gst_caps_set_features_simple (cuda_caps,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY));
      gst_caps_append (sink_templ, cuda_caps);
    }

    name = g_strdup_printf ("video/x-%s", codec);
    src_templ = gst_caps_new_simple (name,
        "width", GST_TYPE_INT_RANGE, min_width, max_width,
        "height", GST_TYPE_INT_RANGE, min_height, max_height,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "au", NULL);
    gst_caps_set_value (src_templ, "profile", profiles);
    g_free (name);

    GST_DEBUG ("sink template caps %" GST_PTR_FORMAT, sink_templ);
    GST_DEBUG ("src template caps %" GST_PTR_FORMAT, src_templ);

    g_value_unset (profiles);
    g_free (profiles);

  free_format:
    if (formats) {
      g_value_unset (formats);
      g_free (formats);
    }
    /* fall-through */

  enc_free:
    NvEncDestroyEncoder (enc);
    CuCtxPopCurrent (NULL);
    /* fall-through */

  done:
    if (sink_templ && src_templ) {
      if (gst_nvenc_cmp_guid (codec_id, NV_ENC_CODEC_H264_GUID)) {
        gst_nv_h264_enc_register (plugin, device_index, rank, sink_templ,
            src_templ, &device_caps);
      } else if (gst_nvenc_cmp_guid (codec_id, NV_ENC_CODEC_HEVC_GUID)) {
        gst_nv_h265_enc_register (plugin, device_index, rank, sink_templ,
            src_templ, &device_caps);
      } else {
        g_assert_not_reached ();
      }
    }

    gst_clear_caps (&sink_templ);
    gst_clear_caps (&src_templ);
  }
}

typedef struct
{
  gint major;
  gint minor;
} GstNvEncVersion;

gboolean
gst_nvenc_load_library (guint * api_major_ver, guint * api_minor_ver)
{
  GModule *module;
  NVENCSTATUS ret = NV_ENC_SUCCESS;
  uint32_t max_supported_version;
  gint major_ver, minor_ver;
  gint i;
  static const GstNvEncVersion version_list[] = {
    {NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION},
    {9, 1},
    {9, 0},
    {GST_NVENC_MIN_API_MAJOR_VERSION, GST_NVENC_MIN_API_MINOR_VERSION}
  };

  module = g_module_open (NVENC_LIBRARY_NAME, G_MODULE_BIND_LAZY);
  if (module == NULL) {
    GST_WARNING ("Could not open library %s, %s",
        NVENC_LIBRARY_NAME, g_module_error ());
    return FALSE;
  }

  if (!g_module_symbol (module, "NvEncodeAPICreateInstance",
          (gpointer *) & nvEncodeAPICreateInstance)) {
    GST_ERROR ("%s", g_module_error ());
    return FALSE;
  }

  if (!g_module_symbol (module, "NvEncodeAPIGetMaxSupportedVersion",
          (gpointer *) & nvEncodeAPIGetMaxSupportedVersion)) {
    GST_ERROR ("NvEncodeAPIGetMaxSupportedVersion unavailable");
    return FALSE;
  }

  /* WARNING: Any developers who want to bump SDK version must ensure that
   * following macro values were not changed and also need to check ABI compatibility.
   * Otherwise, gst_nvenc_get_ helpers also should be updated.
   * Currently SDK 8.1 and 9.0 compatible
   *
   * NVENCAPI_VERSION (NVENCAPI_MAJOR_VERSION | (NVENCAPI_MINOR_VERSION << 24))
   *
   * NVENCAPI_STRUCT_VERSION(ver) ((uint32_t)NVENCAPI_VERSION | ((ver)<<16) | (0x7 << 28))
   *
   * NV_ENC_CAPS_PARAM_VER                NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_ENCODE_OUT_PARAMS_VER         NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_CREATE_INPUT_BUFFER_VER       NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_CREATE_BITSTREAM_BUFFER_VER   NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_CREATE_MV_BUFFER_VER          NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_RC_PARAMS_VER                 NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_CONFIG_VER                   (NVENCAPI_STRUCT_VERSION(7) | ( 1<<31 ))
   * NV_ENC_INITIALIZE_PARAMS_VER        (NVENCAPI_STRUCT_VERSION(5) | ( 1<<31 ))
   * NV_ENC_RECONFIGURE_PARAMS_VER       (NVENCAPI_STRUCT_VERSION(1) | ( 1<<31 ))
   * NV_ENC_PRESET_CONFIG_VER            (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 ))
   * NV_ENC_PIC_PARAMS_VER               (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 ))
   * NV_ENC_MEONLY_PARAMS_VER             NVENCAPI_STRUCT_VERSION(3)
   * NV_ENC_LOCK_BITSTREAM_VER            NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_LOCK_INPUT_BUFFER_VER         NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_MAP_INPUT_RESOURCE_VER        NVENCAPI_STRUCT_VERSION(4)
   * NV_ENC_REGISTER_RESOURCE_VER         NVENCAPI_STRUCT_VERSION(3)
   * NV_ENC_STAT_VER                      NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER    NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_EVENT_PARAMS_VER              NVENCAPI_STRUCT_VERSION(1)
   * NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)
   * NV_ENCODE_API_FUNCTION_LIST_VER      NVENCAPI_STRUCT_VERSION(2)
   */

  ret = nvEncodeAPIGetMaxSupportedVersion (&max_supported_version);

  if (ret != NV_ENC_SUCCESS) {
    GST_ERROR ("Could not query max supported api version, ret %d", ret);
    return FALSE;
  }

  /* 4 LSB: minor version
   * the rest: major version */
  major_ver = max_supported_version >> 4;
  minor_ver = max_supported_version & 0xf;

  GST_INFO ("Maximum supported API version by driver: %d.%d",
      major_ver, minor_ver);

  ret = NV_ENC_ERR_INVALID_VERSION;
  for (i = 0; i < G_N_ELEMENTS (version_list); i++) {
    if (version_list[i].major > major_ver ||
        (version_list[i].major == major_ver
            && version_list[i].minor > minor_ver)) {
      continue;
    }

    GST_INFO ("Checking version %d.%d", version_list[i].major,
        version_list[i].minor);

    gst_nvenc_api_version =
        GST_NVENCAPI_VERSION (version_list[i].major, version_list[i].minor);

    memset (&nvenc_api, 0, sizeof (NV_ENCODE_API_FUNCTION_LIST));
    nvenc_api.version = GST_NVENCAPI_STRUCT_VERSION (2, gst_nvenc_api_version);
    ret = nvEncodeAPICreateInstance (&nvenc_api);

    if (ret == NV_ENC_SUCCESS) {
      GST_INFO ("API version %d.%d load done",
          version_list[i].major, version_list[i].minor);

      *api_major_ver = version_list[i].major;
      *api_minor_ver = version_list[i].minor;

      if ((version_list[i].major > 9 ||
              (version_list[i].major == 9 && version_list[i].minor > 0)) &&
          nvenc_api.nvEncSetIOCudaStreams) {
        GST_INFO ("nvEncSetIOCudaStreams is supported");
        gst_nvenc_supports_cuda_stream = TRUE;
      }

      break;
    } else {
      GST_INFO ("Version %d.%d is not supported", version_list[i].major,
          version_list[i].minor);
    }
  }

  return ret == NV_ENC_SUCCESS;
}

void
gst_nvenc_plugin_init (GstPlugin * plugin, guint device_index,
    CUcontext cuda_ctx)
{
  gst_nv_enc_register (plugin, NV_ENC_CODEC_H264_GUID,
      "h264", GST_RANK_PRIMARY * 2, device_index, cuda_ctx);
  gst_nv_enc_register (plugin, NV_ENC_CODEC_HEVC_GUID,
      "h265", GST_RANK_PRIMARY * 2, device_index, cuda_ctx);
}

guint32
gst_nvenc_get_api_version (void)
{
  /* NVENCAPI_VERSION == (NVENCAPI_MAJOR_VERSION | (NVENCAPI_MINOR_VERSION << 24)) */
  return gst_nvenc_api_version;
}

guint32
gst_nvenc_get_caps_param_version (void)
{
  /* NV_ENC_CAPS_PARAM_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_encode_out_params_version (void)
{
  /* NV_ENC_ENCODE_OUT_PARAMS_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_create_input_buffer_version (void)
{
  /* NV_ENC_CREATE_INPUT_BUFFER_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_create_bitstream_buffer_version (void)
{
  /* NV_ENC_CREATE_BITSTREAM_BUFFER_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_create_mv_buffer_version (void)
{
  /* NV_ENC_CREATE_MV_BUFFER_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_rc_params_version (void)
{
  /* NV_ENC_RC_PARAMS_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_config_version (void)
{
  /* NV_ENC_CONFIG_VER ==
   *   (NVENCAPI_STRUCT_VERSION(7) | ( 1<<31 )) */
  return GST_NVENCAPI_STRUCT_VERSION (7, gst_nvenc_api_version) | (1 << 31);
}

guint32
gst_nvenc_get_initialize_params_version (void)
{
  /* NV_ENC_INITIALIZE_PARAMS_VER ==
   *   (NVENCAPI_STRUCT_VERSION(5) | ( 1<<31 )) */
  return GST_NVENCAPI_STRUCT_VERSION (5, gst_nvenc_api_version) | (1 << 31);
}

guint32
gst_nvenc_get_reconfigure_params_version (void)
{
  /* NV_ENC_RECONFIGURE_PARAMS_VER ==
   *   (NVENCAPI_STRUCT_VERSION(1) | ( 1<<31 )) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version) | (1 << 31);
}

guint32
gst_nvenc_get_preset_config_version (void)
{
  /* NV_ENC_PRESET_CONFIG_VER ==
   *   (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 )) */
  return GST_NVENCAPI_STRUCT_VERSION (4, gst_nvenc_api_version) | (1 << 31);
}

guint32
gst_nvenc_get_pic_params_version (void)
{
  /* NV_ENC_PIC_PARAMS_VER ==
   *  (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 )) */
  return GST_NVENCAPI_STRUCT_VERSION (4, gst_nvenc_api_version) | (1 << 31);
}

guint32
gst_nvenc_get_meonly_params_version (void)
{
  /* NV_ENC_MEONLY_PARAMS_VER == NVENCAPI_STRUCT_VERSION(3) */
  return GST_NVENCAPI_STRUCT_VERSION (3, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_lock_bitstream_version (void)
{
  /* NV_ENC_LOCK_BITSTREAM_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_lock_input_buffer_version (void)
{
  /* NV_ENC_LOCK_INPUT_BUFFER_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_map_input_resource_version (void)
{
  /* NV_ENC_MAP_INPUT_RESOURCE_VER == NVENCAPI_STRUCT_VERSION(4) */
  return GST_NVENCAPI_STRUCT_VERSION (4, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_register_resource_version (void)
{
  /* NV_ENC_REGISTER_RESOURCE_VER == NVENCAPI_STRUCT_VERSION(3) */
  return GST_NVENCAPI_STRUCT_VERSION (3, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_stat_version (void)
{
  /* NV_ENC_STAT_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_sequence_param_payload_version (void)
{
  /* NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_event_params_version (void)
{
  /* NV_ENC_EVENT_PARAMS_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

guint32
gst_nvenc_get_open_encode_session_ex_params_version (void)
{
  /* NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER == NVENCAPI_STRUCT_VERSION(1) */
  return GST_NVENCAPI_STRUCT_VERSION (1, gst_nvenc_api_version);
}

gboolean
gst_nvenc_have_set_io_cuda_streams (void)
{
  return gst_nvenc_supports_cuda_stream;
}
