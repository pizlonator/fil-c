/*
 * Copyright (C) 2017 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcuvidloader.h"
#include "gstnvdec.h"

#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_nvdec_debug);
#define GST_CAT_DEFAULT gst_nvdec_debug

#define DEFAULT_MAX_DISPLAY_DELAY -1

enum
{
  PROP_0,
  PROP_MAX_DISPLAY_DELAY,
  PROP_CUDA_DEVICE_ID,
};

#ifdef HAVE_CUDA_GST_GL
#define SUPPORTED_GL_APIS (GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2)

static gboolean
gst_nvdec_copy_device_to_gl (GstNvDec * nvdec,
    CUVIDPARSERDISPINFO * dispinfo, GstBuffer * output_buffer);
#endif

static gboolean
gst_nvdec_copy_device_to_memory (GstNvDec * nvdec,
    CUVIDPARSERDISPINFO * dispinfo, GstBuffer * output_buffer);

#ifdef HAVE_CUDA_GST_GL
typedef struct _GstNvDecRegisterResourceData
{
  GstMemory *mem;
  GstCudaGraphicsResource *resource;
  GstNvDec *nvdec;
  gboolean ret;
} GstNvDecRegisterResourceData;

static void
register_cuda_resource (GstGLContext * context,
    GstNvDecRegisterResourceData * data)
{
  GstMemory *mem = data->mem;
  GstCudaGraphicsResource *resource = data->resource;
  GstNvDec *nvdec = data->nvdec;
  GstMapInfo map_info = GST_MAP_INFO_INIT;
  GstGLBuffer *gl_buf_obj;

  data->ret = FALSE;

  if (!gst_cuda_context_push (nvdec->cuda_ctx)) {
    GST_WARNING_OBJECT (nvdec, "failed to push CUDA context");
    return;
  }

  if (gst_memory_map (mem, &map_info, GST_MAP_READ | GST_MAP_GL)) {
    GstGLMemoryPBO *gl_mem = (GstGLMemoryPBO *) data->mem;
    gl_buf_obj = gl_mem->pbo;

    GST_LOG_OBJECT (nvdec,
        "register glbuffer %d to CUDA resource", gl_buf_obj->id);

    /* register resource without read/write only flags, since
     * downstream CUDA elements (e.g., nvenc) might want to access
     * this resource later. Instead, use map flags during map/unmap */
    if (gst_cuda_graphics_resource_register_gl_buffer (resource,
            gl_buf_obj->id, CU_GRAPHICS_REGISTER_FLAGS_NONE)) {
      data->ret = TRUE;
    } else {
      GST_WARNING_OBJECT (nvdec, "failed to register memory");
    }

    gst_memory_unmap (mem, &map_info);
  } else {
    GST_WARNING_OBJECT (nvdec, "failed to map memory");
  }

  if (!gst_cuda_context_pop (NULL))
    GST_WARNING_OBJECT (nvdec, "failed to unlock CUDA context");
}

static GstCudaGraphicsResource *
ensure_cuda_graphics_resource (GstMemory * mem, GstNvDec * nvdec)
{
  GQuark quark;
  GstCudaGraphicsResource *cgr_info;
  GstNvDecRegisterResourceData data;

  if (!gst_is_gl_memory_pbo (mem)) {
    GST_WARNING_OBJECT (nvdec, "memory is not GL PBO memory, %s",
        mem->allocator->mem_type);
    return NULL;
  }

  quark = gst_cuda_quark_from_id (GST_CUDA_QUARK_GRAPHICS_RESOURCE);

  cgr_info = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem), quark);
  if (!cgr_info) {
    cgr_info = gst_cuda_graphics_resource_new (nvdec->cuda_ctx,
        GST_OBJECT (GST_GL_BASE_MEMORY_CAST (mem)->context),
        GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER);
    data.mem = mem;
    data.resource = cgr_info;
    data.nvdec = nvdec;
    gst_gl_context_thread_add ((GstGLContext *) cgr_info->graphics_context,
        (GstGLContextThreadFunc) register_cuda_resource, &data);
    if (!data.ret) {
      GST_WARNING_OBJECT (nvdec, "could not register resource");
      gst_cuda_graphics_resource_free (cgr_info);

      return NULL;
    }

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem), quark, cgr_info,
        (GDestroyNotify) gst_cuda_graphics_resource_free);
  }

  return cgr_info;
}
#endif /* HAVE_CUDA_GST_GL */

static gboolean gst_nvdec_open (GstVideoDecoder * decoder);
static gboolean gst_nvdec_start (GstVideoDecoder * decoder);
static gboolean gst_nvdec_stop (GstVideoDecoder * decoder);
static gboolean gst_nvdec_close (GstVideoDecoder * decoder);
static gboolean gst_nvdec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_nvdec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_nvdec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);
static void gst_nvdec_set_context (GstElement * element, GstContext * context);
static gboolean gst_nvdec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nvdec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_nvdec_drain (GstVideoDecoder * decoder);
static GstFlowReturn gst_nvdec_finish (GstVideoDecoder * decoder);
static gboolean gst_nvdec_negotiate (GstVideoDecoder * decoder);
#ifdef HAVE_CUDA_GST_GL
static gboolean gst_nvdec_ensure_gl_context (GstNvDec * nvdec);
#endif

#define gst_nvdec_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstNvDec, gst_nvdec, GST_TYPE_VIDEO_DECODER);

static void
gst_nv_dec_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstNvDec *nvdec = GST_NVDEC (object);

  switch (prop_id) {
    case PROP_MAX_DISPLAY_DELAY:
      nvdec->max_display_delay = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvDec *nvdec = GST_NVDEC (object);
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (nvdec);

  switch (prop_id) {
    case PROP_MAX_DISPLAY_DELAY:
      g_value_set_int (value, nvdec->max_display_delay);
      break;
    case PROP_CUDA_DEVICE_ID:
      g_value_set_uint (value, klass->cuda_device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nvdec_class_init (GstNvDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_nv_dec_set_property;
  gobject_class->get_property = gst_nv_dec_get_property;

  video_decoder_class->open = GST_DEBUG_FUNCPTR (gst_nvdec_open);
  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_nvdec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_nvdec_stop);
  video_decoder_class->close = GST_DEBUG_FUNCPTR (gst_nvdec_close);
  video_decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_nvdec_set_format);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_nvdec_handle_frame);
  video_decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nvdec_decide_allocation);
  video_decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nvdec_src_query);
  video_decoder_class->drain = GST_DEBUG_FUNCPTR (gst_nvdec_drain);
  video_decoder_class->flush = GST_DEBUG_FUNCPTR (gst_nvdec_flush);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_nvdec_finish);
  video_decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_nvdec_negotiate);

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nvdec_set_context);
  gst_type_mark_as_plugin_api (GST_TYPE_NVDEC, 0);

  /**
   * GstNvDec:max-display-delay:
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_MAX_DISPLAY_DELAY,
      g_param_spec_int ("max-display-delay", "Max Display Delay",
          "Improves pipelining of decode with display, 0 means no delay "
          "(auto = -1)",
          -1, G_MAXINT, DEFAULT_MAX_DISPLAY_DELAY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstNvDec:cuda-device-id:
   *
   * Assigned CUDA device id
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CUDA_DEVICE_ID,
      g_param_spec_uint ("cuda-device-id", "CUDA device id",
          "Assigned CUDA device id", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_nvdec_init (GstNvDec * nvdec)
{
  nvdec->max_display_delay = DEFAULT_MAX_DISPLAY_DELAY;
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (nvdec), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (nvdec), TRUE);
}

static cudaVideoSurfaceFormat
get_cuda_surface_format_from_gst (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      return cudaVideoSurfaceFormat_NV12;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
      return cudaVideoSurfaceFormat_P016;
    case GST_VIDEO_FORMAT_Y444:
      return cudaVideoSurfaceFormat_YUV444;
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_Y444_16BE:
      return cudaVideoSurfaceFormat_YUV444_16Bit;
    default:
      g_assert_not_reached ();
      break;
  }

  return cudaVideoSurfaceFormat_NV12;
}

static guint
calculate_num_decode_surface (cudaVideoCodec codec, guint width, guint height)
{
  switch (codec) {
    case cudaVideoCodec_VP9:
      return 12;
    case cudaVideoCodec_H264:
    case cudaVideoCodec_H264_SVC:
    case cudaVideoCodec_H264_MVC:
      return 20;
    case cudaVideoCodec_HEVC:{
      gint max_dpb_size;
      gint MaxLumaPS;
      const gint MaxDpbPicBuf = 6;
      gint PicSizeInSamplesY;

      /* A.4.1 */
      MaxLumaPS = 35651584;
      PicSizeInSamplesY = width * height;
      if (PicSizeInSamplesY <= (MaxLumaPS >> 2))
        max_dpb_size = MaxDpbPicBuf * 4;
      else if (PicSizeInSamplesY <= (MaxLumaPS >> 1))
        max_dpb_size = MaxDpbPicBuf * 2;
      else if (PicSizeInSamplesY <= ((3 * MaxLumaPS) >> 2))
        max_dpb_size = (MaxDpbPicBuf * 4) / 3;
      else
        max_dpb_size = MaxDpbPicBuf;

      max_dpb_size = MIN (max_dpb_size, 16);

      return max_dpb_size + 4;
    }
    default:
      break;
  }

  return 8;
}

static guint
gst_nvdec_get_max_display_delay (GstNvDec * nvdec)
{
  return nvdec->max_display_delay >= 0 ? nvdec->max_display_delay :
      (nvdec->is_live ? 0 : 4);
}

static gint64
gst_nvdec_get_latency (GstNvDec * nvdec)
{
  gint fps_n, fps_d;

  if (!nvdec->input_state)
    return 0;
  fps_n = GST_VIDEO_INFO_FPS_N (&nvdec->input_state->info);
  fps_d = GST_VIDEO_INFO_FPS_D (&nvdec->input_state->info);

  /* We assume 25 fps if the input framerate is invalid */
  if (fps_n < 1 || fps_d < 1) {
    fps_n = 25;
    fps_d = 1;
  }

  return gst_util_uint64_scale_int ((nvdec->num_decode_surface +
          gst_nvdec_get_max_display_delay (nvdec)) * GST_SECOND, fps_d, fps_n);
}

/* 0: fail, 1: succeeded, > 1: override dpb size of parser
 * (set by CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces while creating parser) */
static gint CUDAAPI
parser_sequence_callback (GstNvDec * nvdec, CUVIDEOFORMAT * format)
{
  guint width, height;
  CUVIDDECODECREATEINFO create_info = { 0, };
  GstVideoFormat out_format;
  GstVideoInfo *in_info = &nvdec->input_state->info;
  GstVideoInfo *out_info = &nvdec->out_info;
  GstVideoInfo prev_out_info = *out_info;
  GstCudaContext *ctx = nvdec->cuda_ctx;
  GstStructure *in_s = NULL;
  gboolean updata = FALSE;
  guint major_api_ver = 0;
  guint64 curr_latency, old_latency;

  old_latency = gst_nvdec_get_latency (nvdec);
  width = format->display_area.right - format->display_area.left;
  height = format->display_area.bottom - format->display_area.top;

  switch (format->chroma_format) {
    case cudaVideoChromaFormat_444:
      if (format->bit_depth_luma_minus8 == 0) {
        out_format = GST_VIDEO_FORMAT_Y444;
      } else if (format->bit_depth_luma_minus8 == 2 ||
          format->bit_depth_luma_minus8 == 4) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        out_format = GST_VIDEO_FORMAT_Y444_16LE;
#else
        out_format = GST_VIDEO_FORMAT_Y444_16BE;
#endif
      } else {
        GST_ERROR_OBJECT (nvdec, "Unknown 4:4:4 format bitdepth %d",
            format->bit_depth_luma_minus8 + 8);

        nvdec->last_ret = GST_FLOW_NOT_NEGOTIATED;
        return 0;
      }
      break;
    case cudaVideoChromaFormat_420:
      if (format->bit_depth_luma_minus8 == 0) {
        out_format = GST_VIDEO_FORMAT_NV12;
      } else if (format->bit_depth_luma_minus8 == 2) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        out_format = GST_VIDEO_FORMAT_P010_10LE;
#else
        out_format = GST_VIDEO_FORMAT_P010_10BE;
#endif
      } else if (format->bit_depth_luma_minus8 == 4) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        out_format = GST_VIDEO_FORMAT_P016_LE;
#else
        out_format = GST_VIDEO_FORMAT_P016_BE;
#endif
      } else {
        GST_ERROR_OBJECT (nvdec, "Unknown 4:2:0 format bitdepth %d",
            format->bit_depth_luma_minus8 + 8);

        nvdec->last_ret = GST_FLOW_NOT_NEGOTIATED;
        return 0;
      }
      break;
    default:
      GST_ERROR_OBJECT (nvdec, "unhandled chroma format %d, bitdepth %d",
          format->chroma_format, format->bit_depth_luma_minus8 + 8);

      nvdec->last_ret = GST_FLOW_NOT_NEGOTIATED;
      return 0;
  }

  GST_DEBUG_OBJECT (nvdec,
      "out format: %s", gst_video_format_to_string (out_format));

  GST_DEBUG_OBJECT (nvdec, "width: %u, height: %u", width, height);

  gst_video_info_set_format (out_info, out_format, width, height);
  GST_VIDEO_INFO_FPS_N (out_info) = GST_VIDEO_INFO_FPS_N (in_info);
  GST_VIDEO_INFO_FPS_D (out_info) = GST_VIDEO_INFO_FPS_D (in_info);

  if (GST_VIDEO_INFO_FPS_N (out_info) < 1 ||
      GST_VIDEO_INFO_FPS_D (out_info) < 1) {
    GST_VIDEO_INFO_FPS_N (out_info) = format->frame_rate.numerator;
    GST_VIDEO_INFO_FPS_D (out_info) = MAX (1, format->frame_rate.denominator);
  }

  GST_LOG_OBJECT (nvdec,
      "Reading colorimetry information full-range %d matrix %d transfer %d primaries %d",
      format->video_signal_description.video_full_range_flag,
      format->video_signal_description.matrix_coefficients,
      format->video_signal_description.transfer_characteristics,
      format->video_signal_description.color_primaries);

  if (nvdec->input_state->caps)
    in_s = gst_caps_get_structure (nvdec->input_state->caps, 0);

  /* Set colorimetry when upstream did not provide it */
  if (in_s && !gst_structure_has_field (in_s, "colorimetry")) {
    GstVideoColorimetry colorimetry = { 0, };

    if (format->video_signal_description.video_full_range_flag)
      colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
    else
      colorimetry.range = GST_VIDEO_COLOR_RANGE_16_235;

    colorimetry.primaries =
        gst_video_color_primaries_from_iso
        (format->video_signal_description.color_primaries);

    colorimetry.transfer =
        gst_video_transfer_function_from_iso
        (format->video_signal_description.transfer_characteristics);

    colorimetry.matrix =
        gst_video_color_matrix_from_iso
        (format->video_signal_description.matrix_coefficients);

    /* Use a colorimetry having at least one valid colorimetry entry,
     * because we don't know whether the returned
     * colorimetry (by nvdec) was actually parsed information or not.
     * Otherwise let GstVideoInfo handle it with default colorimetry */
    if (colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN ||
        colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN ||
        colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_UNKNOWN) {
      GST_DEBUG_OBJECT (nvdec,
          "Found valid colorimetry, update output colorimetry");
      out_info->colorimetry = colorimetry;
    }
  } else {
    out_info->colorimetry = in_info->colorimetry;
  }

  if (format->progressive_sequence) {
    out_info->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

    /* nvdec doesn't seem to deal with interlacing with hevc so rely
     * on upstream's value */
    if (format->codec == cudaVideoCodec_HEVC) {
      out_info->interlace_mode = in_info->interlace_mode;
    }
  } else {
    out_info->interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
  }

  if (gst_cuvid_get_api_version (&major_api_ver, NULL) && major_api_ver >= 9) {
    /* min_num_decode_surfaces was introduced in nvcodec sdk 9.0 header */
    nvdec->num_decode_surface = format->min_num_decode_surfaces;

    GST_DEBUG_OBJECT (nvdec,
        "Num decode surface: %d", nvdec->num_decode_surface);
  } else {
    nvdec->num_decode_surface =
        calculate_num_decode_surface (format->codec, width, height);

    GST_DEBUG_OBJECT (nvdec,
        "Calculated num decode surface: %d", nvdec->num_decode_surface);
  }

  /* Update the latency if it has changed */
  curr_latency = gst_nvdec_get_latency (nvdec);
  if (old_latency != curr_latency)
    gst_video_decoder_set_latency (GST_VIDEO_DECODER (nvdec), curr_latency,
        curr_latency);

  if (!nvdec->decoder || !gst_video_info_is_equal (out_info, &prev_out_info)) {
    updata = TRUE;

    if (!gst_cuda_context_push (ctx)) {
      GST_ERROR_OBJECT (nvdec, "failed to lock CUDA context");
      goto error;
    }

    if (nvdec->decoder) {
      GST_DEBUG_OBJECT (nvdec, "destroying decoder");
      if (!gst_cuda_result (CuvidDestroyDecoder (nvdec->decoder))) {
        GST_ERROR_OBJECT (nvdec, "failed to destroy decoder");
        goto error;
      } else
        nvdec->decoder = NULL;
    }

    GST_DEBUG_OBJECT (nvdec, "creating decoder");
    create_info.ulWidth = format->coded_width;
    create_info.ulHeight = format->coded_height;
    create_info.ulNumDecodeSurfaces = nvdec->num_decode_surface;
    create_info.CodecType = format->codec;
    create_info.ChromaFormat = format->chroma_format;
    create_info.ulCreationFlags = cudaVideoCreate_Default;
    create_info.display_area.left = format->display_area.left;
    create_info.display_area.top = format->display_area.top;
    create_info.display_area.right = format->display_area.right;
    create_info.display_area.bottom = format->display_area.bottom;
    create_info.OutputFormat = get_cuda_surface_format_from_gst (out_format);
    create_info.bitDepthMinus8 = format->bit_depth_luma_minus8;
    create_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    create_info.ulTargetWidth = width;
    create_info.ulTargetHeight = height;
    create_info.ulNumOutputSurfaces = 1;
    create_info.target_rect.left = 0;
    create_info.target_rect.top = 0;
    create_info.target_rect.right = width;
    create_info.target_rect.bottom = height;

    if (nvdec->decoder
        || !gst_cuda_result (CuvidCreateDecoder (&nvdec->decoder,
                &create_info))) {
      GST_ERROR_OBJECT (nvdec, "failed to create decoder");
      goto error;
    }

    if (!gst_cuda_context_pop (NULL)) {
      GST_ERROR_OBJECT (nvdec, "failed to unlock CUDA context");
      goto error;
    }
  }

  if (!gst_pad_has_current_caps (GST_VIDEO_DECODER_SRC_PAD (nvdec)) || updata) {
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (nvdec))) {
      nvdec->last_ret = GST_FLOW_NOT_NEGOTIATED;
      return 0;
    }
  }

  return nvdec->num_decode_surface;

error:
  nvdec->last_ret = GST_FLOW_ERROR;
  return 0;
}

static gboolean
gst_nvdec_negotiate (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  GstVideoCodecState *state;
  GstVideoInfo *vinfo;
  GstVideoInfo *out_info = &nvdec->out_info;
  gboolean ret;

  GST_DEBUG_OBJECT (nvdec, "negotiate");

  state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (nvdec),
      GST_VIDEO_INFO_FORMAT (out_info), GST_VIDEO_INFO_WIDTH (out_info),
      GST_VIDEO_INFO_HEIGHT (out_info), nvdec->input_state);
  vinfo = &state->info;

  /* update output info with CUvidparser provided one */
  vinfo->interlace_mode = out_info->interlace_mode;
  vinfo->fps_n = out_info->fps_n;
  vinfo->fps_d = out_info->fps_d;

  state->caps = gst_video_info_to_caps (&state->info);
  nvdec->mem_type = GST_NVDEC_MEM_TYPE_SYSTEM;

  {
    GstCaps *caps;
    caps = gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD (nvdec));
    GST_DEBUG_OBJECT (nvdec, "Allowed caps %" GST_PTR_FORMAT, caps);

    if (!caps || gst_caps_is_any (caps)) {
      GST_DEBUG_OBJECT (nvdec,
          "cannot determine output format, use system memory");
    } else {
      GstCapsFeatures *features;
      guint size = gst_caps_get_size (caps);
      guint i;
      gboolean have_cuda = FALSE;
      gboolean have_gl = FALSE;

      for (i = 0; i < size; i++) {
        features = gst_caps_get_features (caps, i);
        if (features && gst_caps_features_contains (features,
                GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
          GST_DEBUG_OBJECT (nvdec, "found CUDA memory feature");
          have_cuda = TRUE;
          break;
        }
#ifdef HAVE_CUDA_GST_GL
        if (nvdec->gl_display &&
            features && gst_caps_features_contains (features,
                GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
          GST_DEBUG_OBJECT (nvdec, "found GL memory feature");
          have_gl = TRUE;
        }
#endif
      }

      if (have_cuda)
        nvdec->mem_type = GST_NVDEC_MEM_TYPE_CUDA;
      else if (have_gl)
        nvdec->mem_type = GST_NVDEC_MEM_TYPE_GL;
    }
    gst_clear_caps (&caps);
  }

#ifdef HAVE_CUDA_GST_GL
  if (nvdec->mem_type == GST_NVDEC_MEM_TYPE_GL &&
      !gst_nvdec_ensure_gl_context (nvdec)) {
    GST_WARNING_OBJECT (nvdec,
        "OpenGL context is not CUDA-compatible, fallback to system memory");
    nvdec->mem_type = GST_NVDEC_MEM_TYPE_SYSTEM;
  }
#endif

  switch (nvdec->mem_type) {
    case GST_NVDEC_MEM_TYPE_CUDA:
      GST_DEBUG_OBJECT (nvdec, "use cuda memory");
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, NULL));
      break;
#ifdef HAVE_CUDA_GST_GL
    case GST_NVDEC_MEM_TYPE_GL:
      GST_DEBUG_OBJECT (nvdec, "use gl memory");
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, NULL));
      gst_caps_set_simple (state->caps, "texture-target", G_TYPE_STRING,
          "2D", NULL);
      break;
#endif
    default:
      GST_DEBUG_OBJECT (nvdec, "use system memory");
      break;
  }

  if (nvdec->output_state)
    gst_video_codec_state_unref (nvdec->output_state);

  nvdec->output_state = state;

  ret = GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);

  if (!ret) {
    GST_ERROR_OBJECT (nvdec, "failed to negotiate with downstream");
    nvdec->last_ret = GST_FLOW_NOT_NEGOTIATED;
  }

  return ret;
}

static gboolean CUDAAPI
parser_decode_callback (GstNvDec * nvdec, CUVIDPICPARAMS * params)
{
  GList *iter, *pending_frames;
  GstCudaContext *ctx = nvdec->cuda_ctx;

  GST_LOG_OBJECT (nvdec, "picture index: %u", params->CurrPicIdx);

  if (!gst_cuda_context_push (ctx)) {
    GST_ERROR_OBJECT (nvdec, "failed to lock CUDA context");
    goto error;
  }

  if (!gst_cuda_result (CuvidDecodePicture (nvdec->decoder, params))) {
    GST_ERROR_OBJECT (nvdec, "failed to decode picture");
    goto error;
  }

  if (!gst_cuda_context_pop (NULL)) {
    GST_ERROR_OBJECT (nvdec, "failed to unlock CUDA context");
    goto error;
  }

  pending_frames = gst_video_decoder_get_frames (GST_VIDEO_DECODER (nvdec));

  /* NOTE: this decode callback could be invoked multiple times for
   * one cuvidParseVideoData() call. Most likely it can be related to "decode only"
   * frame of VPX codec but no document available.
   * In that case, the last decoded frame seems to be displayed */

  for (iter = pending_frames; iter; iter = g_list_next (iter)) {
    guint id;
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) iter->data;
    gboolean set_data = FALSE;

    id = GPOINTER_TO_UINT (gst_video_codec_frame_get_user_data (frame));
    if (G_UNLIKELY (nvdec->state == GST_NVDEC_STATE_DECODE)) {
      if (id) {
        GST_LOG_OBJECT (nvdec, "reset the last user data");
        set_data = TRUE;
      }
    } else if (!id) {
      set_data = TRUE;
    }

    if (set_data) {
      gst_video_codec_frame_set_user_data (frame,
          GUINT_TO_POINTER (params->CurrPicIdx + 1), NULL);
      break;
    }
  }

  nvdec->state = GST_NVDEC_STATE_DECODE;

  g_list_free_full (pending_frames,
      (GDestroyNotify) gst_video_codec_frame_unref);

  return TRUE;

error:
  nvdec->last_ret = GST_FLOW_ERROR;
  return FALSE;
}

static gboolean CUDAAPI
parser_display_callback (GstNvDec * nvdec, CUVIDPARSERDISPINFO * dispinfo)
{
  GList *iter, *pending_frames;
  GstVideoCodecFrame *frame = NULL;
  GstBuffer *output_buffer = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean copy_ret = FALSE;

  GST_LOG_OBJECT (nvdec, "picture index: %u", dispinfo->picture_index);

  pending_frames = gst_video_decoder_get_frames (GST_VIDEO_DECODER (nvdec));
  for (iter = pending_frames; iter; iter = g_list_next (iter)) {
    guint id;
    GstVideoCodecFrame *tmp = (GstVideoCodecFrame *) iter->data;

    id = GPOINTER_TO_UINT (gst_video_codec_frame_get_user_data (tmp));
    if (id == dispinfo->picture_index + 1) {
      frame = gst_video_codec_frame_ref (tmp);
      break;
    }
  }
  g_list_free_full (pending_frames,
      (GDestroyNotify) gst_video_codec_frame_unref);

  if (G_UNLIKELY (frame == NULL)) {
    GST_WARNING_OBJECT (nvdec, "no frame for picture index %u",
        dispinfo->picture_index);

    output_buffer =
        gst_video_decoder_allocate_output_buffer (GST_VIDEO_DECODER (nvdec));

    if (!output_buffer) {
      GST_ERROR_OBJECT (nvdec, "Couldn't allocate output buffer");
      nvdec->last_ret = GST_FLOW_ERROR;
      return FALSE;
    }

    GST_BUFFER_PTS (output_buffer) = dispinfo->timestamp;
    GST_BUFFER_DTS (output_buffer) = GST_CLOCK_TIME_NONE;
    /* assume buffer duration from framerate */
    if (nvdec->out_info.fps_n > 0 && nvdec->out_info.fps_d > 0) {
      GST_BUFFER_DURATION (output_buffer) =
          gst_util_uint64_scale (GST_SECOND,
          GST_VIDEO_INFO_FPS_D (&nvdec->out_info),
          GST_VIDEO_INFO_FPS_N (&nvdec->out_info));
    } else {
      GST_BUFFER_DURATION (output_buffer) = GST_CLOCK_TIME_NONE;
    }
  } else {
    ret = gst_video_decoder_allocate_output_frame (GST_VIDEO_DECODER (nvdec),
        frame);

    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (nvdec, "failed to allocate output frame");
      nvdec->last_ret = ret;
      return FALSE;
    }

    output_buffer = frame->output_buffer;

    if (dispinfo->timestamp != frame->pts) {
      GST_INFO_OBJECT (nvdec,
          "timestamp mismatch, diff: %" GST_STIME_FORMAT,
          GST_STIME_ARGS (GST_CLOCK_DIFF (dispinfo->timestamp, frame->pts)));
    }
  }

#ifdef HAVE_CUDA_GST_GL
  if (nvdec->mem_type == GST_NVDEC_MEM_TYPE_GL) {
    copy_ret = gst_nvdec_copy_device_to_gl (nvdec, dispinfo, output_buffer);

    /* FIXME: This is the case where OpenGL context of downstream glbufferpool
     * belongs to non-nvidia (or different device).
     * There should be enhancement to ensure nvdec has compatible OpenGL context
     */
    if (!copy_ret) {
      GST_WARNING_OBJECT (nvdec,
          "Couldn't copy frame to GL memory, fallback to system memory");
      nvdec->mem_type = GST_NVDEC_MEM_TYPE_SYSTEM;
    }
  }

  if (!copy_ret)
#endif
  {
    copy_ret = gst_nvdec_copy_device_to_memory (nvdec, dispinfo, output_buffer);
  }

  if (!copy_ret) {
    GST_ERROR_OBJECT (nvdec, "failed to copy decoded picture to output buffer");
    nvdec->last_ret = GST_FLOW_ERROR;

    if (frame)
      gst_video_decoder_drop_frame (GST_VIDEO_DECODER (nvdec), frame);
    else
      gst_buffer_unref (output_buffer);

    return FALSE;
  }

  if (!dispinfo->progressive_frame) {
    GST_BUFFER_FLAG_SET (output_buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);

    if (dispinfo->top_field_first) {
      GST_BUFFER_FLAG_SET (output_buffer, GST_VIDEO_BUFFER_FLAG_TFF);
    }

    if (dispinfo->repeat_first_field == -1) {
      GST_BUFFER_FLAG_SET (output_buffer, GST_VIDEO_BUFFER_FLAG_ONEFIELD);
    } else {
      GST_BUFFER_FLAG_SET (output_buffer, GST_VIDEO_BUFFER_FLAG_RFF);
    }
  }

  if (frame) {
    ret = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (nvdec), frame);
  } else {
    ret = gst_pad_push (GST_VIDEO_DECODER_SRC_PAD (nvdec), output_buffer);
  }

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (nvdec, "failed to finish frame %s",
        gst_flow_get_name (ret));
    nvdec->last_ret = ret;
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_nvdec_open (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (nvdec);

  GST_DEBUG_OBJECT (nvdec, "creating CUDA context");

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (decoder),
          klass->cuda_device_id, &nvdec->cuda_ctx)) {
    GST_ERROR_OBJECT (nvdec, "failed to create CUDA context");
    return FALSE;
  }

  nvdec->stream = gst_cuda_stream_new (nvdec->cuda_ctx);
  if (!nvdec->stream) {
    GST_WARNING_OBJECT (nvdec,
        "Could not create CUDA stream, will use default stream");
  }
#ifdef HAVE_CUDA_GST_GL
  gst_gl_ensure_element_data (GST_ELEMENT (nvdec),
      &nvdec->gl_display, &nvdec->other_gl_context);
  if (nvdec->gl_display)
    gst_gl_display_filter_gl_api (GST_GL_DISPLAY (nvdec->gl_display),
        SUPPORTED_GL_APIS);
#endif

  return TRUE;
}

static gboolean
gst_nvdec_start (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (nvdec);

  nvdec->state = GST_NVDEC_STATE_INIT;
  nvdec->last_ret = GST_FLOW_OK;
  gst_video_info_init (&nvdec->out_info);

  if (klass->codec_type == cudaVideoCodec_H264)
    nvdec->h264_parser = gst_h264_nal_parser_new ();
  else if (klass->codec_type == cudaVideoCodec_HEVC)
    nvdec->h265_parser = gst_h265_parser_new ();

  return TRUE;
}

static gboolean
maybe_destroy_decoder_and_parser (GstNvDec * nvdec)
{
  gboolean ret = TRUE;

  if (!gst_cuda_context_push (nvdec->cuda_ctx)) {
    GST_ERROR_OBJECT (nvdec, "failed to lock CUDA context");
    return FALSE;
  }

  if (nvdec->decoder) {
    GST_DEBUG_OBJECT (nvdec, "destroying decoder");
    ret = gst_cuda_result (CuvidDestroyDecoder (nvdec->decoder));
    nvdec->decoder = NULL;

    if (!ret)
      GST_ERROR_OBJECT (nvdec, "failed to destroy decoder");
  }

  if (nvdec->parser) {
    GST_DEBUG_OBJECT (nvdec, "destroying parser");
    if (!gst_cuda_result (CuvidDestroyVideoParser (nvdec->parser))) {
      GST_ERROR_OBJECT (nvdec, "failed to destroy parser");
      ret = FALSE;
    }
    nvdec->parser = NULL;
  }

  if (!gst_cuda_context_pop (NULL)) {
    GST_WARNING_OBJECT (nvdec, "failed to pop CUDA context");
  }

  return ret;
}

static void
gst_nvdec_clear_codec_data (GstNvDec * self)
{
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (self);
  guint i;

  if (klass->codec_type == cudaVideoCodec_HEVC) {
    for (i = 0; i < G_N_ELEMENTS (self->vps_nals); i++) {
      gst_clear_buffer (&self->vps_nals[i]);
    }
  }

  if (klass->codec_type == cudaVideoCodec_HEVC ||
      klass->codec_type == cudaVideoCodec_H264) {
    for (i = 0; i < G_N_ELEMENTS (self->sps_nals); i++) {
      gst_clear_buffer (&self->sps_nals[i]);
    }

    for (i = 0; i < G_N_ELEMENTS (self->pps_nals); i++) {
      gst_clear_buffer (&self->pps_nals[i]);
    }
  }

  gst_clear_buffer (&self->codec_data);

  self->need_codec_data = TRUE;
}

static gboolean
gst_nvdec_stop (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);

  GST_DEBUG_OBJECT (nvdec, "stop");

  if (!maybe_destroy_decoder_and_parser (nvdec))
    return FALSE;

#ifdef HAVE_CUDA_GST_GL
  gst_clear_object (&nvdec->gl_context);
  gst_clear_object (&nvdec->other_gl_context);
  gst_clear_object (&nvdec->gl_display);
#endif

  g_clear_pointer (&nvdec->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&nvdec->output_state, gst_video_codec_state_unref);

  g_clear_pointer (&nvdec->h264_parser, gst_h264_nal_parser_free);
  g_clear_pointer (&nvdec->h265_parser, gst_h265_parser_free);

  gst_nvdec_clear_codec_data (nvdec);

  return TRUE;
}

static gboolean
gst_nvdec_close (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);

  gst_clear_cuda_stream (&nvdec->stream);
  gst_clear_object (&nvdec->cuda_ctx);

  return TRUE;
}

static gboolean
gst_nvdec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (decoder);
  CUVIDPARSERPARAMS parser_params = { 0, };
  GstQuery *query;
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (nvdec, "set format");

  if (nvdec->input_state)
    gst_video_codec_state_unref (nvdec->input_state);

  nvdec->input_state = gst_video_codec_state_ref (state);

  if (!maybe_destroy_decoder_and_parser (nvdec))
    return FALSE;

  /* Check if pipeline is live */
  nvdec->is_live = FALSE;
  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (decoder), query))
    gst_query_parse_latency (query, &nvdec->is_live, NULL, NULL);
  gst_query_unref (query);

  parser_params.CodecType = klass->codec_type;
  /* ulMaxNumDecodeSurfaces will be updated by the return value of
   * SequenceCallback */
  parser_params.ulMaxNumDecodeSurfaces = 1;
  parser_params.ulErrorThreshold = 100;
  parser_params.ulMaxDisplayDelay = gst_nvdec_get_max_display_delay (nvdec);
  parser_params.ulClockRate = GST_SECOND;
  parser_params.pUserData = nvdec;
  parser_params.pfnSequenceCallback =
      (PFNVIDSEQUENCECALLBACK) parser_sequence_callback;
  parser_params.pfnDecodePicture =
      (PFNVIDDECODECALLBACK) parser_decode_callback;
  parser_params.pfnDisplayPicture =
      (PFNVIDDISPLAYCALLBACK) parser_display_callback;

  gst_cuda_context_push (nvdec->cuda_ctx);
  GST_DEBUG_OBJECT (nvdec, "creating parser");
  if (!gst_cuda_result (CuvidCreateVideoParser (&nvdec->parser,
              &parser_params))) {
    GST_ERROR_OBJECT (nvdec, "failed to create parser");
    ret = FALSE;
  }

  gst_cuda_context_pop (NULL);

  /* store codec data */
  gst_nvdec_clear_codec_data (nvdec);

  if (ret && nvdec->input_state->caps) {
    GstStructure *str;

    str = gst_caps_get_structure (nvdec->input_state->caps, 0);

    if (klass->codec_type == cudaVideoCodec_MPEG4) {
      const GValue *codec_data_value;
      codec_data_value = gst_structure_get_value (str, "codec_data");
      if (codec_data_value && GST_VALUE_HOLDS_BUFFER (codec_data_value)) {
        GstBuffer *codec_data = gst_value_get_buffer (codec_data_value);
        gst_buffer_replace (&nvdec->codec_data, codec_data);
      }
    }

    /* For all CODEC we get complete picture ... */
    nvdec->recv_complete_picture = TRUE;

    /* Except for JPEG, for which it depends on the caps */
    if (klass->codec_type == cudaVideoCodec_JPEG) {
      gboolean parsed;
      if (gst_structure_get_boolean (str, "parsed", &parsed))
        nvdec->recv_complete_picture = parsed;
      else
        nvdec->recv_complete_picture = FALSE;
    }
  }

  return ret;
}

#ifdef HAVE_CUDA_GST_GL
typedef struct
{
  GstNvDec *nvdec;
  CUVIDPARSERDISPINFO *dispinfo;
  gboolean ret;
  GstBuffer *output_buffer;
} GstNvDecCopyToGLData;

static void
copy_video_frame_to_gl_textures (GstGLContext * context,
    GstNvDecCopyToGLData * data)
{
  GstNvDec *nvdec = data->nvdec;
  CUVIDPARSERDISPINFO *dispinfo = data->dispinfo;
  GstCudaGraphicsResource **resources;
  guint num_resources;
  CUVIDPROCPARAMS proc_params = { 0, };
  guintptr dptr;
  guint pitch, i;
  CUDA_MEMCPY2D mcpy2d = { 0, };
  GstVideoInfo *info = &nvdec->output_state->info;
  CUstream stream = gst_cuda_stream_get_handle (nvdec->stream);

  GST_LOG_OBJECT (nvdec, "picture index: %u", dispinfo->picture_index);

  proc_params.progressive_frame = dispinfo->progressive_frame;
  proc_params.top_field_first = dispinfo->top_field_first;
  proc_params.unpaired_field = dispinfo->repeat_first_field == -1;
  proc_params.output_stream = stream;

  data->ret = TRUE;

  num_resources = gst_buffer_n_memory (data->output_buffer);
  resources = g_newa (GstCudaGraphicsResource *, num_resources);

  for (i = 0; i < num_resources; i++) {
    GstMemory *mem;

    mem = gst_buffer_peek_memory (data->output_buffer, i);
    resources[i] = ensure_cuda_graphics_resource (mem, nvdec);
    if (!resources[i]) {
      GST_WARNING_OBJECT (nvdec, "could not register %dth memory", i);
      data->ret = FALSE;

      return;
    }

    /* Need PBO -> texture */
    GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
  }

  if (!gst_cuda_context_push (nvdec->cuda_ctx)) {
    GST_WARNING_OBJECT (nvdec, "failed to lock CUDA context");
    data->ret = FALSE;
    return;
  }

  if (!gst_cuda_result (CuvidMapVideoFrame (nvdec->decoder,
              dispinfo->picture_index, &dptr, &pitch, &proc_params))) {
    GST_WARNING_OBJECT (nvdec, "failed to map CUDA video frame");
    data->ret = FALSE;
    goto unlock_cuda_context;
  }

  mcpy2d.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mcpy2d.srcPitch = pitch;
  mcpy2d.dstMemoryType = CU_MEMORYTYPE_DEVICE;

  for (i = 0; i < num_resources; i++) {
    CUdeviceptr cuda_ptr;
    gsize size;
    CUgraphicsResource cuda_resource =
        gst_cuda_graphics_resource_map (resources[i], stream,
        CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD);

    if (!cuda_resource) {
      GST_WARNING_OBJECT (nvdec, "failed to map CUDA resources");
      data->ret = FALSE;
      goto unmap_video_frame;
    }

    if (!gst_cuda_result (CuGraphicsResourceGetMappedPointer (&cuda_ptr, &size,
                cuda_resource))) {
      GST_WARNING_OBJECT (nvdec, "failed to map CUDA resource");
      data->ret = FALSE;
      break;
    }

    mcpy2d.dstPitch = GST_VIDEO_INFO_PLANE_STRIDE (info, i);
    mcpy2d.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (info, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (info, i);

    mcpy2d.srcDevice = dptr + (i * pitch * GST_VIDEO_INFO_HEIGHT (info));
    mcpy2d.dstDevice = cuda_ptr;
    mcpy2d.Height = GST_VIDEO_INFO_COMP_HEIGHT (info, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&mcpy2d, stream))) {
      GST_WARNING_OBJECT (nvdec, "memcpy to mapped array failed");
      data->ret = FALSE;
    }
  }

  gst_cuda_result (CuStreamSynchronize (stream));

unmap_video_frame:
  for (i = 0; i < num_resources; i++) {
    gst_cuda_graphics_resource_unmap (resources[i], stream);
  }

  if (!gst_cuda_result (CuvidUnmapVideoFrame (nvdec->decoder, dptr)))
    GST_WARNING_OBJECT (nvdec, "failed to unmap CUDA video frame");

unlock_cuda_context:
  if (!gst_cuda_context_pop (NULL))
    GST_WARNING_OBJECT (nvdec, "failed to unlock CUDA context");
}

static gboolean
gst_nvdec_copy_device_to_gl (GstNvDec * nvdec,
    CUVIDPARSERDISPINFO * dispinfo, GstBuffer * output_buffer)
{
  GstNvDecCopyToGLData data = { 0, };

  data.nvdec = nvdec;
  data.dispinfo = dispinfo;
  data.output_buffer = output_buffer;

  gst_gl_context_thread_add (nvdec->gl_context,
      (GstGLContextThreadFunc) copy_video_frame_to_gl_textures, &data);

  return data.ret;
}
#endif

static gboolean
gst_nvdec_copy_device_to_memory (GstNvDec * nvdec,
    CUVIDPARSERDISPINFO * dispinfo, GstBuffer * output_buffer)
{
  CUVIDPROCPARAMS params = { 0, };
  CUDA_MEMCPY2D copy_params = { 0, };
  guintptr dptr;
  guint pitch;
  GstVideoFrame video_frame;
  GstVideoInfo *info = &nvdec->output_state->info;
  gint i;
  GstMemory *mem;
  gboolean use_device_copy = FALSE;
  gboolean need_sync = TRUE;
  GstMapFlags map_flags = GST_MAP_WRITE;
  CUstream stream = gst_cuda_stream_get_handle (nvdec->stream);

  if (nvdec->mem_type == GST_NVDEC_MEM_TYPE_CUDA &&
      (mem = gst_buffer_peek_memory (output_buffer, 0)) &&
      gst_is_cuda_memory (mem)) {
    GstCudaStream *mem_stream;

    map_flags |= GST_MAP_CUDA;
    use_device_copy = TRUE;

    mem_stream = gst_cuda_memory_get_stream (GST_CUDA_MEMORY_CAST (mem));
    if (mem_stream) {
      stream = gst_cuda_stream_get_handle (mem_stream);
      need_sync = FALSE;
    }
  }

  if (!gst_video_frame_map (&video_frame, info, output_buffer, map_flags)) {
    GST_ERROR_OBJECT (nvdec, "frame map failure");
    return FALSE;
  }

  if (!gst_cuda_context_push (nvdec->cuda_ctx)) {
    gst_video_frame_unmap (&video_frame);
    GST_WARNING_OBJECT (nvdec, "failed to lock CUDA context");
    return FALSE;
  }

  params.progressive_frame = dispinfo->progressive_frame;
  params.second_field = dispinfo->repeat_first_field + 1;
  params.top_field_first = dispinfo->top_field_first;
  params.unpaired_field = dispinfo->repeat_first_field < 0;
  params.output_stream = stream;

  if (!gst_cuda_result (CuvidMapVideoFrame (nvdec->decoder,
              dispinfo->picture_index, &dptr, &pitch, &params))) {
    GST_ERROR_OBJECT (nvdec, "failed to map video frame");
    gst_cuda_context_pop (NULL);
    return FALSE;
  }

  copy_params.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_params.srcPitch = pitch;
  copy_params.dstMemoryType =
      use_device_copy ? CU_MEMORYTYPE_DEVICE : CU_MEMORYTYPE_HOST;

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    copy_params.srcDevice = dptr + (i * pitch * GST_VIDEO_INFO_HEIGHT (info));
    if (use_device_copy) {
      copy_params.dstDevice =
          (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (&video_frame, i);
    } else {
      copy_params.dstHost = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, i);
    }
    copy_params.dstPitch = GST_VIDEO_FRAME_PLANE_STRIDE (&video_frame, i);
    copy_params.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (info, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (info, i);
    copy_params.Height = GST_VIDEO_INFO_COMP_HEIGHT (info, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&copy_params, stream))) {
      GST_ERROR_OBJECT (nvdec, "failed to copy %dth plane", i);
      CuvidUnmapVideoFrame (nvdec->decoder, dptr);
      gst_video_frame_unmap (&video_frame);
      gst_cuda_context_pop (NULL);
      return FALSE;
    }
  }

  if (need_sync)
    gst_cuda_result (CuStreamSynchronize (stream));

  gst_video_frame_unmap (&video_frame);

  if (!gst_cuda_result (CuvidUnmapVideoFrame (nvdec->decoder, dptr)))
    GST_WARNING_OBJECT (nvdec, "failed to unmap video frame");

  if (!gst_cuda_context_pop (NULL))
    GST_WARNING_OBJECT (nvdec, "failed to unlock CUDA context");

  return TRUE;
}

static void
gst_nvdec_store_h264_nal (GstNvDec * self, guint id,
    GstH264NalUnitType nal_type, GstH264NalUnit * nalu)
{
  GstBuffer *buf, **store;
  guint size = nalu->size, store_size;
  static const guint8 start_code[] = { 0, 0, 1 };

  if (nal_type == GST_H264_NAL_SPS || nal_type == GST_H264_NAL_SUBSET_SPS) {
    store_size = GST_H264_MAX_SPS_COUNT;
    store = self->sps_nals;
    GST_DEBUG_OBJECT (self, "storing sps %u", id);
  } else if (nal_type == GST_H264_NAL_PPS) {
    store_size = GST_H264_MAX_PPS_COUNT;
    store = self->pps_nals;
    GST_DEBUG_OBJECT (self, "storing pps %u", id);
  } else {
    return;
  }

  if (id >= store_size) {
    GST_DEBUG_OBJECT (self, "unable to store nal, id out-of-range %d", id);
    return;
  }

  buf = gst_buffer_new_allocate (NULL, size + sizeof (start_code), NULL);
  gst_buffer_fill (buf, 0, start_code, sizeof (start_code));
  gst_buffer_fill (buf, sizeof (start_code), nalu->data + nalu->offset, size);

  if (store[id])
    gst_buffer_unref (store[id]);

  store[id] = buf;
}

static GstBuffer *
gst_nvdec_handle_h264_buffer (GstNvDec * self, GstBuffer * buffer)
{
  GstH264NalParser *parser = self->h264_parser;
  GstH264NalUnit nalu;
  GstH264ParserResult pres;
  GstMapInfo map;
  gboolean have_sps = FALSE;
  gboolean have_pps = FALSE;
  guint i;
  GstBuffer *new_buf;

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_WARNING_OBJECT (self, "Failed to map input buffer");
    return gst_buffer_ref (buffer);
  }

  memset (&nalu, 0, sizeof (GstH264NalUnit));

  do {
    pres = gst_h264_parser_identify_nalu (parser,
        map.data, nalu.offset + nalu.size, map.size, &nalu);

    if (pres == GST_H264_PARSER_NO_NAL_END)
      pres = GST_H264_PARSER_OK;

    switch (nalu.type) {
      case GST_H264_NAL_SPS:
      case GST_H264_NAL_SUBSET_SPS:{
        GstH264SPS sps;

        if (nalu.type == GST_H264_NAL_SPS) {
          pres = gst_h264_parser_parse_sps (parser, &nalu, &sps);
        } else {
          pres = gst_h264_parser_parse_subset_sps (parser, &nalu, &sps);
        }

        if (pres != GST_H264_PARSER_OK)
          break;

        have_sps = TRUE;
        gst_nvdec_store_h264_nal (self, sps.id, nalu.type, &nalu);
        gst_h264_sps_clear (&sps);
        break;
      }
      case GST_H264_NAL_PPS:{
        GstH264PPS pps;

        pres = gst_h264_parser_parse_pps (parser, &nalu, &pps);
        if (pres != GST_H264_PARSER_OK)
          break;

        have_pps = TRUE;
        gst_nvdec_store_h264_nal (self, pps.id, nalu.type, &nalu);
        gst_h264_pps_clear (&pps);
        break;
      }
      default:
        break;
    }
  } while (pres == GST_H264_PARSER_OK);

  gst_buffer_unmap (buffer, &map);

  if (!self->need_codec_data || (have_sps && have_pps)) {
    self->need_codec_data = FALSE;
    return gst_buffer_ref (buffer);
  }

  new_buf = gst_buffer_new ();
  if (!have_sps) {
    for (i = 0; i < GST_H264_MAX_SPS_COUNT; i++) {
      if (!self->sps_nals[i])
        continue;

      have_sps = TRUE;
      new_buf = gst_buffer_append (new_buf, gst_buffer_ref (self->sps_nals[i]));
    }
  }

  if (!have_pps) {
    for (i = 0; i < GST_H264_MAX_PPS_COUNT; i++) {
      if (!self->pps_nals[i])
        continue;

      have_pps = TRUE;
      new_buf = gst_buffer_append (new_buf, gst_buffer_ref (self->pps_nals[i]));
    }
  }

  new_buf = gst_buffer_append (new_buf, gst_buffer_ref (buffer));

  if (have_sps && have_pps)
    self->need_codec_data = FALSE;

  return new_buf;
}

static void
gst_nvdec_store_h265_nal (GstNvDec * self, guint id,
    GstH265NalUnitType nal_type, GstH265NalUnit * nalu)
{
  GstBuffer *buf, **store;
  guint size = nalu->size, store_size;
  static const guint8 start_code[] = { 0, 0, 1 };

  if (nal_type == GST_H265_NAL_VPS) {
    store_size = GST_H265_MAX_VPS_COUNT;
    store = self->vps_nals;
    GST_DEBUG_OBJECT (self, "storing vps %u", id);
  } else if (nal_type == GST_H265_NAL_SPS) {
    store_size = GST_H265_MAX_SPS_COUNT;
    store = self->sps_nals;
    GST_DEBUG_OBJECT (self, "storing sps %u", id);
  } else if (nal_type == GST_H265_NAL_PPS) {
    store_size = GST_H265_MAX_PPS_COUNT;
    store = self->pps_nals;
    GST_DEBUG_OBJECT (self, "storing pps %u", id);
  } else {
    return;
  }

  if (id >= store_size) {
    GST_DEBUG_OBJECT (self, "unable to store nal, id out-of-range %d", id);
    return;
  }

  buf = gst_buffer_new_allocate (NULL, size + sizeof (start_code), NULL);
  gst_buffer_fill (buf, 0, start_code, sizeof (start_code));
  gst_buffer_fill (buf, sizeof (start_code), nalu->data + nalu->offset, size);

  if (store[id])
    gst_buffer_unref (store[id]);

  store[id] = buf;
}

static GstBuffer *
gst_nvdec_handle_h265_buffer (GstNvDec * self, GstBuffer * buffer)
{
  GstH265Parser *parser = self->h265_parser;
  GstH265NalUnit nalu;
  GstH265ParserResult pres;
  GstMapInfo map;
  gboolean have_vps = FALSE;
  gboolean have_sps = FALSE;
  gboolean have_pps = FALSE;
  GstBuffer *new_buf;
  guint i;

  if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
    GST_WARNING_OBJECT (self, "Failed to map input buffer");
    return gst_buffer_ref (buffer);
  }

  memset (&nalu, 0, sizeof (GstH265NalUnit));

  do {
    pres = gst_h265_parser_identify_nalu (parser,
        map.data, nalu.offset + nalu.size, map.size, &nalu);

    if (pres == GST_H265_PARSER_NO_NAL_END)
      pres = GST_H265_PARSER_OK;

    switch (nalu.type) {
      case GST_H265_NAL_VPS:{
        GstH265VPS vps;

        pres = gst_h265_parser_parse_vps (parser, &nalu, &vps);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_vps = TRUE;
        gst_nvdec_store_h265_nal (self, vps.id, nalu.type, &nalu);
        break;
      }
      case GST_H265_NAL_SPS:{
        GstH265SPS sps;

        pres = gst_h265_parser_parse_sps (parser, &nalu, &sps, FALSE);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_sps = TRUE;
        gst_nvdec_store_h265_nal (self, sps.id, nalu.type, &nalu);
        break;
      }
      case GST_H265_NAL_PPS:{
        GstH265PPS pps;

        pres = gst_h265_parser_parse_pps (parser, &nalu, &pps);
        if (pres != GST_H265_PARSER_OK)
          break;

        have_pps = TRUE;
        gst_nvdec_store_h265_nal (self, pps.id, nalu.type, &nalu);
        break;
      }
      default:
        break;
    }
  } while (pres == GST_H265_PARSER_OK);

  gst_buffer_unmap (buffer, &map);

  if (!self->need_codec_data || (have_sps && have_pps)) {
    self->need_codec_data = FALSE;
    return gst_buffer_ref (buffer);
  }

  new_buf = gst_buffer_new ();
  if (!have_vps) {
    for (i = 0; i < GST_H265_MAX_VPS_COUNT; i++) {
      if (!self->vps_nals[i])
        continue;

      new_buf = gst_buffer_append (new_buf, gst_buffer_ref (self->vps_nals[i]));
    }
  }

  if (!have_sps) {
    for (i = 0; i < GST_H265_MAX_SPS_COUNT; i++) {
      if (!self->sps_nals[i])
        continue;

      have_sps = TRUE;
      new_buf = gst_buffer_append (new_buf, gst_buffer_ref (self->sps_nals[i]));
    }
  }

  if (!have_pps) {
    for (i = 0; i < GST_H265_MAX_PPS_COUNT; i++) {
      if (!self->pps_nals[i])
        continue;

      have_pps = TRUE;
      new_buf = gst_buffer_append (new_buf, gst_buffer_ref (self->pps_nals[i]));
    }
  }

  if (have_sps && have_pps)
    self->need_codec_data = FALSE;

  return gst_buffer_append (new_buf, gst_buffer_ref (buffer));
}

static GstBuffer *
gst_nvdec_process_input (GstNvDec * self, GstBuffer * inbuf)
{
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (self);
  gboolean parse_nal = FALSE;

  if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DELTA_UNIT) ||
      self->need_codec_data) {
    parse_nal = TRUE;
  }

  if (klass->codec_type == cudaVideoCodec_MPEG4 &&
      self->codec_data && GST_BUFFER_IS_DISCONT (inbuf)) {
    return gst_buffer_append (gst_buffer_ref (self->codec_data),
        gst_buffer_ref (inbuf));
  } else if (klass->codec_type == cudaVideoCodec_H264 && parse_nal) {
    return gst_nvdec_handle_h264_buffer (self, inbuf);
  } else if (klass->codec_type == cudaVideoCodec_HEVC && parse_nal) {
    return gst_nvdec_handle_h265_buffer (self, inbuf);
  }

  return gst_buffer_ref (inbuf);
}

static GstFlowReturn
gst_nvdec_handle_frame (GstVideoDecoder * decoder, GstVideoCodecFrame * frame)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  GstMapInfo map_info = GST_MAP_INFO_INIT;
  CUVIDSOURCEDATAPACKET packet = { 0, };
  GstBuffer *in_buffer;

  GST_LOG_OBJECT (nvdec, "handle frame");

  /* initialize with zero to keep track of frames */
  gst_video_codec_frame_set_user_data (frame, GUINT_TO_POINTER (0), NULL);

  in_buffer = gst_nvdec_process_input (nvdec, frame->input_buffer);

  if (!gst_buffer_map (in_buffer, &map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (nvdec, "failed to map input buffer");
    gst_buffer_unref (in_buffer);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  packet.payload_size = (gulong) map_info.size;
  packet.payload = map_info.data;
  packet.timestamp = frame->pts;
  packet.flags |= CUVID_PKT_TIMESTAMP;

  if (nvdec->recv_complete_picture)
    packet.flags |= CUVID_PKT_ENDOFPICTURE;

  nvdec->state = GST_NVDEC_STATE_PARSE;
  nvdec->last_ret = GST_FLOW_OK;

  if (!gst_cuda_result (CuvidParseVideoData (nvdec->parser, &packet)))
    GST_WARNING_OBJECT (nvdec, "parser failed");

  gst_buffer_unmap (in_buffer, &map_info);
  gst_buffer_unref (in_buffer);
  gst_video_codec_frame_unref (frame);

  return nvdec->last_ret;
}

static gboolean
gst_nvdec_flush (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  CUVIDSOURCEDATAPACKET packet = { 0, };

  GST_DEBUG_OBJECT (nvdec, "flush");

  packet.payload_size = 0;
  packet.payload = NULL;
  packet.flags = CUVID_PKT_ENDOFSTREAM;

  nvdec->state = GST_NVDEC_STATE_PARSE;
  nvdec->last_ret = GST_FLOW_OK;

  if (nvdec->parser
      && !gst_cuda_result (CuvidParseVideoData (nvdec->parser, &packet)))
    GST_WARNING_OBJECT (nvdec, "parser failed");

  nvdec->need_codec_data = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_nvdec_drain (GstVideoDecoder * decoder)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);
  CUVIDSOURCEDATAPACKET packet = { 0, };

  GST_DEBUG_OBJECT (nvdec, "draining decoder");

  packet.payload_size = 0;
  packet.payload = NULL;
  packet.flags = CUVID_PKT_ENDOFSTREAM;

  nvdec->state = GST_NVDEC_STATE_PARSE;
  nvdec->last_ret = GST_FLOW_OK;

  if (nvdec->parser
      && !gst_cuda_result (CuvidParseVideoData (nvdec->parser, &packet)))
    GST_WARNING_OBJECT (nvdec, "parser failed");

  nvdec->need_codec_data = TRUE;

  return nvdec->last_ret;
}

static GstFlowReturn
gst_nvdec_finish (GstVideoDecoder * decoder)
{
  GST_DEBUG_OBJECT (decoder, "finish");

  return gst_nvdec_drain (decoder);
}

#ifdef HAVE_CUDA_GST_GL
static void
gst_nvdec_check_cuda_device_from_context (GstGLContext * context,
    gboolean * ret)
{
  guint device_count = 0;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  *ret = FALSE;

  cuda_ret = CuGLGetDevices (&device_count,
      device_list, 1, CU_GL_DEVICE_LIST_ALL);

  if (!gst_cuda_result (cuda_ret) || device_count == 0)
    return;

  *ret = TRUE;

  return;
}

static gboolean
gst_nvdec_ensure_gl_context (GstNvDec * nvdec)
{
  gboolean ret;

  if (!nvdec->gl_display) {
    GST_DEBUG_OBJECT (nvdec, "No available OpenGL display");
    return FALSE;
  }

  if (!gst_gl_query_local_gl_context (GST_ELEMENT (nvdec), GST_PAD_SRC,
          &nvdec->gl_context)) {
    GST_INFO_OBJECT (nvdec, "failed to query local OpenGL context");
    if (nvdec->gl_context)
      gst_object_unref (nvdec->gl_context);
    nvdec->gl_context =
        gst_gl_display_get_gl_context_for_thread (nvdec->gl_display, NULL);
    if (!nvdec->gl_context
        || !gst_gl_display_add_context (nvdec->gl_display, nvdec->gl_context)) {
      if (nvdec->gl_context)
        gst_object_unref (nvdec->gl_context);
      if (!gst_gl_display_create_context (nvdec->gl_display,
              nvdec->other_gl_context, &nvdec->gl_context, NULL)) {
        GST_ERROR_OBJECT (nvdec, "failed to create OpenGL context");
        return FALSE;
      }
      if (!gst_gl_display_add_context (nvdec->gl_display, nvdec->gl_context)) {
        GST_ERROR_OBJECT (nvdec,
            "failed to add the OpenGL context to the display");
        return FALSE;
      }
    }
  }

  if (!gst_gl_context_check_gl_version (nvdec->gl_context,
          SUPPORTED_GL_APIS, 3, 0)) {
    GST_WARNING_OBJECT (nvdec, "OpenGL context could not support PBO download");
    return FALSE;
  }

  gst_gl_context_thread_add (nvdec->gl_context,
      (GstGLContextThreadFunc) gst_nvdec_check_cuda_device_from_context, &ret);

  if (!ret) {
    GST_WARNING_OBJECT (nvdec, "Current OpenGL context is not CUDA-compatible");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_nvdec_ensure_gl_pool (GstNvDec * nvdec, GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint n, size, min, max;
  GstVideoInfo vinfo = { 0, };
  GstStructure *config;

  GST_DEBUG_OBJECT (nvdec, "decide allocation");

  gst_query_parse_allocation (query, &outcaps, NULL);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  if (pool && !GST_IS_GL_BUFFER_POOL (pool)) {
    gst_object_unref (pool);
    pool = NULL;
  }

  if (!pool) {
    GST_DEBUG_OBJECT (nvdec, "no downstream pool, create our pool");
    pool = gst_gl_buffer_pool_new (nvdec->gl_context);

    if (outcaps)
      gst_video_info_from_caps (&vinfo, outcaps);
    size = (guint) vinfo.size;
    min = max = 0;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config (pool, config);
  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}
#endif

static gboolean
gst_nvdec_ensure_cuda_pool (GstNvDec * nvdec, GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint n, size, min, max;
  GstVideoInfo vinfo = { 0, };
  GstStructure *config;

  gst_query_parse_allocation (query, &outcaps, NULL);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_CUDA_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstCudaBufferPool *cpool = GST_CUDA_BUFFER_POOL (pool);

        if (cpool->context != nvdec->cuda_ctx)
          gst_clear_object (&pool);
      }
    }
  }

  if (!pool) {
    GST_DEBUG_OBJECT (nvdec, "no downstream pool, create our pool");
    pool = gst_cuda_buffer_pool_new (nvdec->cuda_ctx);

    if (outcaps)
      gst_video_info_from_caps (&vinfo, outcaps);
    size = (guint) vinfo.size;
    min = max = 0;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config (pool, config);

  /* Get updated size by cuda buffer pool */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_nvdec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);

  GST_DEBUG_OBJECT (nvdec, "decide allocation");

  if (nvdec->mem_type == GST_NVDEC_MEM_TYPE_SYSTEM)
    goto done;

#ifdef HAVE_CUDA_GST_GL
  if (nvdec->mem_type == GST_NVDEC_MEM_TYPE_GL) {
    if (!gst_nvdec_ensure_gl_pool (nvdec, query))
      return FALSE;
  } else
#endif
  if (!gst_nvdec_ensure_cuda_pool (nvdec, query)) {
    return FALSE;
  }

done:
  return GST_VIDEO_DECODER_CLASS (gst_nvdec_parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_nvdec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvDec *nvdec = GST_NVDEC (decoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_cuda_handle_context_query (GST_ELEMENT (decoder),
              query, nvdec->cuda_ctx)) {
        return TRUE;
      }
#ifdef HAVE_CUDA_GST_GL
      if (gst_gl_handle_context_query (GST_ELEMENT (decoder), query,
              nvdec->gl_display, nvdec->gl_context, nvdec->other_gl_context)) {
        if (nvdec->gl_display)
          gst_gl_display_filter_gl_api (GST_GL_DISPLAY (nvdec->gl_display),
              SUPPORTED_GL_APIS);
        return TRUE;
      }
#endif
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (gst_nvdec_parent_class)->src_query (decoder,
      query);
}

static void
gst_nvdec_set_context (GstElement * element, GstContext * context)
{
  GstNvDec *nvdec = GST_NVDEC (element);
  GstNvDecClass *klass = GST_NVDEC_GET_CLASS (nvdec);

  GST_DEBUG_OBJECT (nvdec, "set context %s",
      gst_context_get_context_type (context));

  if (gst_cuda_handle_set_context (element,
          context, klass->cuda_device_id, &nvdec->cuda_ctx)) {
    goto done;
  }
#ifdef HAVE_CUDA_GST_GL
  gst_gl_handle_set_context (element, context, &nvdec->gl_display,
      &nvdec->other_gl_context);
#endif

done:
  GST_ELEMENT_CLASS (gst_nvdec_parent_class)->set_context (element, context);
}

typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  cudaVideoCodec codec_type;
  gchar *codec;
  guint cuda_device_id;
  gboolean is_default;
} GstNvDecClassData;

static void
gst_nvdec_subclass_init (gpointer g_class, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstNvDecClass *nvdec_class = GST_NVDEC_CLASS (g_class);
  GstNvDecClassData *cdata = data;
  gchar *long_name;

  if (cdata->is_default) {
    long_name = g_strdup_printf ("NVDEC %s Video Decoder", cdata->codec);
  } else {
    long_name = g_strdup_printf ("NVDEC %s Video Decoder with device %d",
        cdata->codec, cdata->cuda_device_id);
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Video/Hardware", "NVDEC video decoder",
      "Ericsson AB, http://www.ericsson.com, "
      "Seungha Yang <seungha.yang@navercorp.com>");
  g_free (long_name);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  nvdec_class->codec_type = cdata->codec_type;
  nvdec_class->cuda_device_id = cdata->cuda_device_id;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata->codec);
  g_free (cdata);
}

static void
gst_nvdec_subclass_register (GstPlugin * plugin, GType type,
    cudaVideoCodec codec_type, const gchar * codec, guint device_id, guint rank,
    GstCaps * sink_caps, GstCaps * src_caps)
{
  GTypeQuery type_query;
  GTypeInfo type_info = { 0, };
  GType subtype;
  gchar *type_name;
  GstNvDecClassData *cdata;
  gboolean is_default = TRUE;
  gint index = 0;

  cdata = g_new0 (GstNvDecClassData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);
  cdata->codec_type = codec_type;
  cdata->codec = g_strdup (codec);
  cdata->cuda_device_id = device_id;

  g_type_query (type, &type_query);
  memset (&type_info, 0, sizeof (type_info));
  type_info.class_size = type_query.class_size;
  type_info.instance_size = type_query.instance_size;
  type_info.class_init = gst_nvdec_subclass_init;
  type_info.class_data = cdata;

  type_name = g_strdup_printf ("nv%sdec", codec);
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    type_name = g_strdup_printf ("nv%sdevice%ddec", codec, index);
    is_default = FALSE;
  }

  cdata->is_default = is_default;
  subtype = g_type_register_static (type, type_name, &type_info, 0);

  /* make lower rank than default device */
  if (rank > 0 && !is_default)
    rank--;

  if (!gst_element_register (plugin, type_name, rank, subtype))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
}

void
gst_nvdec_plugin_init (GstPlugin * plugin, guint device_index,
    cudaVideoCodec codec, const gchar * codec_name, GstCaps * sink_template,
    GstCaps * src_template)
{
  gst_nvdec_subclass_register (plugin, GST_TYPE_NVDEC, codec,
      codec_name, device_index, GST_RANK_PRIMARY, sink_template, src_template);
}
