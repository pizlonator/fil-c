/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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

#include "gstnvjpegenc.h"
#include <gst/cuda/gstcuda-private.h>
#include <gst/video/video.h>
#include <gmodule.h>
#include <string>
#include <mutex>

/**
 * SECTION:element-nvjpegenc
 * @title: nvjpegenc
 *
 * A nvJPEG library based JPEG encoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=1 ! nvjpegenc ! filesink location=myjpeg.jpg
 * ```
 *
 * Since: 1.24
 *
 */

GST_DEBUG_CATEGORY_STATIC (gst_nv_jpeg_enc_debug);
#define GST_CAT_DEFAULT gst_nv_jpeg_enc_debug

typedef gpointer nvjpegHandle_t;
typedef gpointer nvjpegEncoderState_t;
typedef gpointer nvjpegEncoderParams_t;

enum nvjpegStatus_t
{
  NVJPEG_STATUS_SUCCESS = 0,
};

enum nvjpegChromaSubsampling_t
{
  NVJPEG_CSS_444 = 0,
  NVJPEG_CSS_422 = 1,
  NVJPEG_CSS_420 = 2,
  NVJPEG_CSS_440 = 3,
  NVJPEG_CSS_411 = 4,
  NVJPEG_CSS_410 = 5,
  NVJPEG_CSS_GRAY = 6,
  NVJPEG_CSS_410V = 7,
  NVJPEG_CSS_UNKNOWN = -1
};

struct nvjpegImage_t
{
  unsigned char *channel[4];
  size_t pitch[4];
};

/* *INDENT-OFF* */
struct GstNvJpegVTable
{
  gboolean loaded = FALSE;

  nvjpegStatus_t (*NvjpegCreateSimple) (nvjpegHandle_t * handle);
  nvjpegStatus_t (*NvjpegDestroy) (nvjpegHandle_t handle);

  nvjpegStatus_t (*NvjpegEncoderStateCreate) (nvjpegHandle_t handle,
      nvjpegEncoderState_t * state, CUstream stream);
  nvjpegStatus_t (*NvjpegEncoderStateDestroy) (nvjpegEncoderState_t state);

  nvjpegStatus_t (*NvjpegEncoderParamsCreate) (nvjpegHandle_t handle,
      nvjpegEncoderParams_t * params, CUstream stream);
  nvjpegStatus_t (*NvjpegEncoderParamsDestroy) (nvjpegEncoderParams_t params);

  nvjpegStatus_t (*NvjpegEncoderParamsSetQuality) (nvjpegEncoderParams_t params,
      const int quality, CUstream stream);
  nvjpegStatus_t (*NvjpegEncoderParamsSetSamplingFactors) (
      nvjpegEncoderParams_t params, const nvjpegChromaSubsampling_t subsampling,
      CUstream stream);
  nvjpegStatus_t (*NvjpegEncodeYUV) (nvjpegHandle_t handle,
      nvjpegEncoderState_t state, const nvjpegEncoderParams_t params,
      const nvjpegImage_t * source, nvjpegChromaSubsampling_t subsampling,
      int width, int height, CUstream stream);
  nvjpegStatus_t (*NvjpegEncodeRetrieveBitstream) (nvjpegHandle_t handle,
      nvjpegEncoderState_t state, unsigned char *data, size_t *length,
      CUstream stream);
};
/* *INDENT-ON* */

static GstNvJpegVTable g_vtable = { };

#define LOAD_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_ERROR ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    return; \
  } \
} G_STMT_END;

static gboolean
gst_nv_jpeg_enc_load_library (void)
{
  static GModule *module = nullptr;

  GST_CUDA_CALL_ONCE_BEGIN {
    gint cuda_version;
    auto ret = CuDriverGetVersion (&cuda_version);
    if (ret != CUDA_SUCCESS) {
      GST_WARNING ("Couldn't get driver version, 0x%x", (guint) ret);
      return;
    }

    auto cuda_major_ver = cuda_version / 1000;
    std::string nvjpeg_lib_name;
#ifdef G_OS_WIN32
    nvjpeg_lib_name = "nvjpeg64_" + std::to_string (cuda_major_ver) + ".dll";
    module = g_module_open (nvjpeg_lib_name.c_str (), G_MODULE_BIND_LAZY);
#else
    nvjpeg_lib_name = "libnvjpeg.so";
    module = g_module_open (nvjpeg_lib_name.c_str (), G_MODULE_BIND_LAZY);
    if (!module) {
      nvjpeg_lib_name += "." + std::to_string (cuda_major_ver);
      module = g_module_open (nvjpeg_lib_name.c_str (), G_MODULE_BIND_LAZY);
    }
#endif

    if (!module)
      return;

    GstNvJpegVTable *vtable = &g_vtable;

    LOAD_SYMBOL (nvjpegCreateSimple, NvjpegCreateSimple);
    LOAD_SYMBOL (nvjpegDestroy, NvjpegDestroy);

    LOAD_SYMBOL (nvjpegEncoderStateCreate, NvjpegEncoderStateCreate);
    LOAD_SYMBOL (nvjpegEncoderStateDestroy, NvjpegEncoderStateDestroy);

    LOAD_SYMBOL (nvjpegEncoderParamsCreate, NvjpegEncoderParamsCreate);
    LOAD_SYMBOL (nvjpegEncoderParamsDestroy, NvjpegEncoderParamsDestroy);

    LOAD_SYMBOL (nvjpegEncoderParamsSetQuality, NvjpegEncoderParamsSetQuality);
    LOAD_SYMBOL (nvjpegEncoderParamsSetSamplingFactors,
        NvjpegEncoderParamsSetSamplingFactors);
    LOAD_SYMBOL (nvjpegEncodeYUV, NvjpegEncodeYUV);
    LOAD_SYMBOL (nvjpegEncodeRetrieveBitstream, NvjpegEncodeRetrieveBitstream);

    vtable->loaded = TRUE;
    GST_INFO ("nvjpeg library loaded");
  }
  GST_CUDA_CALL_ONCE_END;

  return g_vtable.loaded;
}

enum
{
  PROP_0,
  PROP_CUDA_DEVICE_ID,
  PROP_QUALITY,
};

#define DEFAULT_JPEG_QUALITY 85

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg, width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE)
    );

struct GstNvJpegEncCData
{
  guint cuda_device_id;
  GstCaps *sink_caps;
  gboolean have_nvrtc;
};

/* *INDENT-OFF* */
struct GstNvJpegEncPrivate
{
  GstCudaContext *context = nullptr;
  GstCudaStream *stream = nullptr;

  nvjpegHandle_t handle = nullptr;
  nvjpegEncoderState_t state = nullptr;
  nvjpegEncoderParams_t params = nullptr;
  nvjpegChromaSubsampling_t subsampling;

  CUmodule module = nullptr;
  CUfunction kernel_func = nullptr;
  bool launch_kernel = false;

  CUdeviceptr uv[2] = { 0, };
  gint pitch;

  GstVideoInfo info;
  GstBufferPool *pool = nullptr;
  GstBuffer *fallback_buf = nullptr;

  std::mutex lock;
  guint quality = DEFAULT_JPEG_QUALITY;
  bool quality_updated = false;
};
/* *INDENT-ON* */

struct GstNvJpegEnc
{
  GstVideoEncoder parent;

  GstNvJpegEncPrivate *priv;
};

struct GstNvJpegEncClass
{
  GstVideoEncoderClass parent_class;

  guint cuda_device_id;
  gboolean have_nvrtc;
};

static void gst_nv_jpeg_enc_finalize (GObject * object);
static void gst_nv_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_jpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_nv_jpeg_enc_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_jpeg_enc_open (GstVideoEncoder * encoder);
static gboolean gst_nv_jpeg_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_nv_jpeg_enc_close (GstVideoEncoder * encoder);
static gboolean gst_nv_jpeg_enc_sink_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_nv_jpeg_enc_src_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_nv_jpeg_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_nv_jpeg_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_nv_jpeg_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);

static GstElementClass *parent_class = nullptr;

#define GST_NV_JPEG_ENC(object) ((GstNvJpegEnc *) (object))
#define GST_NV_JPEG_ENC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvJpegEncClass))

static void
gst_nv_jpeg_enc_class_init (GstNvJpegEncClass * klass, gpointer data)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto element_class = GST_ELEMENT_CLASS (klass);
  auto encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  auto cdata = (GstNvJpegEncCData *) data;

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  object_class->finalize = gst_nv_jpeg_enc_finalize;
  object_class->set_property = gst_nv_jpeg_enc_set_property;
  object_class->get_property = gst_nv_jpeg_enc_get_property;

  g_object_class_install_property (object_class, PROP_CUDA_DEVICE_ID,
      g_param_spec_uint ("cuda-device-id", "CUDA Device ID",
          "CUDA device ID of associated GPU", 0, G_MAXINT, 0,
          (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_QUALITY,
      g_param_spec_uint ("quality", "Quality",
          "Quality of encoding", 1, 100, DEFAULT_JPEG_QUALITY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "NVIDIA JPEG Encoder", "Codec/Encoder/Video/Hardware",
      "Encode JPEG image using nvJPEG library",
      "Seungha Yang <seungha@centricular.com>");

  auto sink_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      cdata->sink_caps);

  gst_element_class_add_pad_template (element_class, sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_set_context);

  encoder_class->open = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_open);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_stop);
  encoder_class->close = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_close);
  encoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_sink_query);
  encoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_src_query);
  encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_propose_allocation);
  encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_set_format);
  encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_nv_jpeg_enc_handle_frame);

  klass->cuda_device_id = cdata->cuda_device_id;
  klass->have_nvrtc = cdata->have_nvrtc;
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static void
gst_nv_jpeg_enc_init (GstNvJpegEnc * self)
{
  self->priv = new GstNvJpegEncPrivate ();
}

static void
gst_nv_jpeg_enc_finalize (GObject * object)
{
  auto self = GST_NV_JPEG_ENC (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  auto self = GST_NV_JPEG_ENC (object);
  auto priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_QUALITY:
    {
      auto quality = g_value_get_uint (value);
      if (quality != priv->quality) {
        priv->quality_updated = true;
        priv->quality = quality;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_jpeg_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  auto self = GST_NV_JPEG_ENC (object);
  auto priv = self->priv;
  auto klass = GST_NV_JPEG_ENC_GET_CLASS (self);

  std::lock_guard < std::mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_CUDA_DEVICE_ID:
      g_value_set_uint (value, klass->cuda_device_id);
      break;
    case PROP_QUALITY:
      g_value_set_uint (value, priv->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_jpeg_enc_set_context (GstElement * element, GstContext * context)
{
  auto self = GST_NV_JPEG_ENC (element);
  auto priv = self->priv;
  auto klass = GST_NV_JPEG_ENC_GET_CLASS (self);

  gst_cuda_handle_set_context (element, context, klass->cuda_device_id,
      &priv->context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

#define KERNEL_MAIN_FUNC "gst_nv_jpec_enc_kernel"
/* *INDENT-OFF* */
const static gchar kernel_source[] =
"extern \"C\" {\n"
"__device__ inline unsigned char\n"
"scale_to_uchar (float val)\n"
"{\n"
"  return (unsigned char) __float2int_rz (val * 255.0);\n"
"}\n"
"\n"
"__global__ void\n"
KERNEL_MAIN_FUNC "(cudaTextureObject_t uv_tex, unsigned char * out_u,\n"
"    unsigned char * out_v, int width, int height, int stride)\n"
"{\n"
"  int x_pos = blockIdx.x * blockDim.x + threadIdx.x;\n"
"  int y_pos = blockIdx.y * blockDim.y + threadIdx.y;\n"
"  if (x_pos >= width || y_pos >= height)\n"
"    return;\n"
"  float x = 0;\n"
"  float y = 0;\n"
"  if (width > 1)\n"
"    x = (float) x_pos / (width - 1);\n"
"  if (height > 1)\n"
"    y = (float) y_pos / (height - 1);\n"
"  float2 uv = tex2D<float2> (uv_tex, x, y);\n"
"  unsigned int pos = x_pos + (y_pos * stride);\n"
"  out_u[pos] = scale_to_uchar (uv.x);\n"
"  out_v[pos] = scale_to_uchar (uv.y);\n"
"}\n"
"}";
/* *INDENT-ON* */

static gboolean
gst_nv_jpeg_enc_open (GstVideoEncoder * encoder)
{
  auto self = GST_NV_JPEG_ENC (encoder);
  auto priv = self->priv;
  auto klass = GST_NV_JPEG_ENC_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Open");

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (encoder),
          klass->cuda_device_id, &priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't create CUDA context");
    return FALSE;
  }

  if (!gst_cuda_context_push (priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't push context");
    return FALSE;
  }

  if (!priv->module && klass->have_nvrtc) {
    auto program = gst_cuda_nvrtc_compile_cubin (kernel_source,
        klass->cuda_device_id);
    if (!program)
      program = gst_cuda_nvrtc_compile (kernel_source);

    if (!program) {
      GST_ERROR_OBJECT (self, "Couldn't compile kernel source");
      gst_cuda_context_pop (nullptr);
      return FALSE;
    }

    auto ret = CuModuleLoadData (&priv->module, program);
    g_free (program);

    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't load module");
      gst_cuda_context_pop (nullptr);
      return FALSE;
    }

    ret = CuModuleGetFunction (&priv->kernel_func, priv->module,
        KERNEL_MAIN_FUNC);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't get kernel function");
      gst_cuda_context_pop (nullptr);
      return FALSE;
    }
  }

  auto ret = g_vtable.NvjpegCreateSimple (&priv->handle);
  gst_cuda_context_pop (nullptr);

  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Couldn't create encoder handle");
    return FALSE;
  }

  priv->stream = gst_cuda_stream_new (priv->context);

  return TRUE;
}

static void
gst_nv_jpeg_enc_reset (GstNvJpegEnc * self)
{
  auto priv = self->priv;
  if (priv->context && gst_cuda_context_push (priv->context)) {
    if (priv->state)
      g_vtable.NvjpegEncoderStateDestroy (priv->state);

    if (priv->params)
      g_vtable.NvjpegEncoderParamsDestroy (priv->params);

    for (guint i = 0; i < G_N_ELEMENTS (priv->uv); i++) {
      if (priv->uv[i]) {
        CuMemFree (priv->uv[i]);
        priv->uv[i] = 0;
      }
    }

    gst_cuda_context_pop (nullptr);
  }

  priv->state = nullptr;
  priv->params = nullptr;
  priv->launch_kernel = false;

  gst_clear_buffer (&priv->fallback_buf);

  if (priv->pool) {
    gst_buffer_pool_set_active (priv->pool, FALSE);
    gst_clear_object (&priv->pool);
  }
}

static gboolean
gst_nv_jpeg_enc_stop (GstVideoEncoder * encoder)
{
  auto self = GST_NV_JPEG_ENC (encoder);

  gst_nv_jpeg_enc_reset (self);

  return TRUE;
}

static gboolean
gst_nv_jpeg_enc_close (GstVideoEncoder * encoder)
{
  auto self = GST_NV_JPEG_ENC (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Close");

  if (priv->context && gst_cuda_context_push (priv->context)) {
    if (priv->handle)
      g_vtable.NvjpegDestroy (priv->handle);

    if (priv->module) {
      CuModuleUnload (priv->module);
      priv->module = nullptr;
    }

    gst_cuda_context_pop (nullptr);
  }

  priv->handle = nullptr;
  gst_clear_cuda_stream (&priv->stream);
  gst_clear_object (&priv->context);

  return TRUE;
}

static gboolean
gst_nv_jpeg_enc_handle_query (GstNvJpegEnc * self, GstQuery * query)
{
  auto priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      return gst_cuda_handle_context_query (GST_ELEMENT (self), query,
          priv->context);
    default:
      break;
  }

  return FALSE;
}

static gboolean
gst_nv_jpeg_enc_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  auto self = GST_NV_JPEG_ENC (encoder);

  if (gst_nv_jpeg_enc_handle_query (self, query))
    return TRUE;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
}

static gboolean
gst_nv_jpeg_enc_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  auto self = GST_NV_JPEG_ENC (encoder);

  if (gst_nv_jpeg_enc_handle_query (self, query))
    return TRUE;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
}

static gboolean
gst_nv_jpeg_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  auto self = GST_NV_JPEG_ENC (encoder);
  auto priv = self->priv;
  GstVideoInfo info;
  GstBufferPool *pool = nullptr;
  GstCaps *caps;
  gboolean use_cuda_pool = FALSE;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_WARNING_OBJECT (self, "null caps in query");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps into info");
    return FALSE;
  }

  auto features = gst_caps_get_features (caps, 0);
  if (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
    GST_DEBUG_OBJECT (self, "Upstream support CUDA memory");
    use_cuda_pool = TRUE;
  }

  if (use_cuda_pool)
    pool = gst_cuda_buffer_pool_new (priv->context);
  else
    pool = gst_video_buffer_pool_new ();

  auto config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (!use_cuda_pool) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  guint size = GST_VIDEO_INFO_SIZE (&info);
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
  if (use_cuda_pool && priv->stream) {
    /* Set our stream on buffer pool config so that CUstream can be shared */
    gst_buffer_pool_config_set_cuda_stream (config, priv->stream);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);
    return FALSE;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, 0, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_nv_jpeg_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  auto self = GST_NV_JPEG_ENC (encoder);
  auto priv = self->priv;

  priv->info = state->info;

  auto caps = gst_caps_new_empty_simple ("image/jpeg");
  auto output_state = gst_video_encoder_set_output_state (encoder, caps,
      state);
  gst_video_codec_state_unref (output_state);

  gst_nv_jpeg_enc_reset (self);

  switch (GST_VIDEO_INFO_FORMAT (&priv->info)) {
    case GST_VIDEO_FORMAT_I420:
      priv->subsampling = NVJPEG_CSS_420;
      break;
    case GST_VIDEO_FORMAT_NV12:
      priv->subsampling = NVJPEG_CSS_420;
      priv->launch_kernel = true;
      break;
    case GST_VIDEO_FORMAT_Y42B:
      priv->subsampling = NVJPEG_CSS_422;
      break;
    case GST_VIDEO_FORMAT_Y444:
      priv->subsampling = NVJPEG_CSS_444;
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->quality_updated = false;

  if (!gst_cuda_context_push (priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't push context");
    return FALSE;
  }

  /* Allocate memory  */
  if (priv->launch_kernel) {
    auto width = (priv->info.width + 1) / 2;
    auto height = (priv->info.height + 1) / 2;
    size_t pitch;
    auto ret = CuMemAllocPitch (&priv->uv[0], &pitch, width, height, 16);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't allocate U plane memory");
      gst_cuda_context_pop (nullptr);
      return FALSE;
    }

    ret = CuMemAllocPitch (&priv->uv[1], &pitch, width, height, 16);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't allocate V plane memory");
      gst_cuda_context_pop (nullptr);
      gst_nv_jpeg_enc_reset (self);
      return FALSE;
    }

    priv->pitch = pitch;
  }

  auto stream = gst_cuda_stream_get_handle (priv->stream);
  auto ret = g_vtable.NvjpegEncoderParamsCreate (priv->handle, &priv->params,
      stream);
  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Couldn't create param handle, ret %d", ret);
    gst_cuda_context_pop (nullptr);
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  ret = g_vtable.NvjpegEncoderParamsSetQuality (priv->params,
      priv->quality, stream);
  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Couldn't set quality, ret %d", ret);
    gst_cuda_context_pop (nullptr);
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  ret = g_vtable.NvjpegEncoderParamsSetSamplingFactors (priv->params,
      priv->subsampling, stream);
  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Couldn't set subsampling factor, ret %d", ret);
    gst_cuda_context_pop (nullptr);
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  ret = g_vtable.NvjpegEncoderStateCreate (priv->handle, &priv->state, stream);
  gst_cuda_context_pop (nullptr);

  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "Couldn't create state handle, ret %d", ret);
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  priv->pool = gst_cuda_buffer_pool_new (priv->context);
  auto config = gst_buffer_pool_get_config (priv->pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config,
      state->caps, priv->info.size, 0, 0);
  if (priv->stream)
    gst_buffer_pool_config_set_cuda_stream (config, priv->stream);

  if (!gst_buffer_pool_set_config (priv->pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  if (!gst_buffer_pool_set_active (priv->pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Pool set active failed");
    gst_nv_jpeg_enc_reset (self);
    return FALSE;
  }

  return TRUE;
}

static GstBuffer *
gst_nv_jpeg_enc_upload_system (GstNvJpegEnc * self, GstBuffer * buffer)
{
  auto priv = self->priv;
  auto info = &priv->info;

  if (!priv->fallback_buf) {
    gst_buffer_pool_acquire_buffer (priv->pool, &priv->fallback_buf, nullptr);
    if (!priv->fallback_buf) {
      GST_ERROR_OBJECT (self, "Couldn't acquire upload buffer");
      return nullptr;
    }
  }

  if (!gst_cuda_buffer_copy (priv->fallback_buf, GST_CUDA_BUFFER_COPY_CUDA,
          info, buffer, GST_CUDA_BUFFER_COPY_SYSTEM, info, priv->context,
          priv->stream)) {
    GST_ERROR_OBJECT (self, "Couldn't upload frame");
    return nullptr;
  }

  return priv->fallback_buf;
}

static GstBuffer *
gst_nv_jpeg_enc_upload (GstNvJpegEnc * self, GstBuffer * buffer)
{
  auto priv = self->priv;
  auto mem = gst_buffer_peek_memory (buffer, 0);

  if (!gst_is_cuda_memory (mem))
    return gst_nv_jpeg_enc_upload_system (self, buffer);

  auto cmem = GST_CUDA_MEMORY_CAST (mem);
  if (cmem->context != priv->context)
    return gst_nv_jpeg_enc_upload_system (self, buffer);

  auto stream = gst_cuda_memory_get_stream (cmem);
  if (stream != priv->stream)
    gst_cuda_memory_sync (cmem);

  return buffer;
}

#define CUDA_BLOCK_X 16
#define CUDA_BLOCK_Y 16
#define DIV_UP(size,block) (((size) + ((block) - 1)) / (block))

static gboolean
gst_nv_jpeg_enc_fill_source (GstNvJpegEnc * self, GstBuffer * buffer,
    nvjpegImage_t * source)
{
  auto priv = self->priv;
  auto upload = gst_nv_jpeg_enc_upload (self, buffer);
  if (!upload)
    return FALSE;

  GstVideoFrame frame;
  if (!gst_video_frame_map (&frame,
          &priv->info, upload, (GstMapFlags) (GST_MAP_READ | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (self, "Couldn't map input buffer");
    return FALSE;
  }

  if (priv->launch_kernel) {
    CUtexObject texture;
    auto cmem = (GstCudaMemory *) gst_buffer_peek_memory (upload, 0);
    if (!gst_cuda_memory_get_texture (cmem,
            1, CU_TR_FILTER_MODE_POINT, &texture)) {
      GST_ERROR_OBJECT (self, "Couldn't get texture");
      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    gint width = priv->info.width / 2;
    gint height = priv->info.height / 2;
    gpointer args[] = { &texture, &priv->uv[0], &priv->uv[1], &width, &height,
      &priv->pitch
    };

    if (!gst_cuda_context_push (priv->context)) {
      GST_ERROR_OBJECT (self, "Couldn't push context");
      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    auto stream = gst_cuda_stream_get_handle (priv->stream);
    auto ret = CuLaunchKernel (priv->kernel_func, DIV_UP (width, CUDA_BLOCK_X),
        DIV_UP (height, CUDA_BLOCK_Y), 1, CUDA_BLOCK_X, CUDA_BLOCK_Y, 1, 0,
        stream, args, nullptr);
    gst_cuda_context_pop (nullptr);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't launch kernel");
      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    source->channel[0] = (unsigned char *)
        GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
    source->pitch[0] = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);

    source->channel[1] = (unsigned char *) priv->uv[0];
    source->channel[2] = (unsigned char *) priv->uv[1];
    source->pitch[1] = source->pitch[2] = priv->pitch;
  } else {
    for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&frame); i++) {
      source->channel[i] = (unsigned char *)
          GST_VIDEO_FRAME_PLANE_DATA (&frame, i);
      source->pitch[i] = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, i);
    }
  }

  gst_video_frame_unmap (&frame);

  return TRUE;
}

static GstFlowReturn
gst_nv_jpeg_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  auto self = GST_NV_JPEG_ENC (encoder);
  auto priv = self->priv;

  if (!gst_cuda_context_push (priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't push context");
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  auto stream = gst_cuda_stream_get_handle (priv->stream);

  {
    std::lock_guard < std::mutex > lk (priv->lock);
    if (priv->quality_updated) {
      priv->quality_updated = false;
      auto ret = g_vtable.NvjpegEncoderParamsSetQuality (priv->params,
          priv->quality, stream);
      if (ret != NVJPEG_STATUS_SUCCESS) {
        GST_ERROR_OBJECT (self, "Couldn't set quality, ret %d", ret);
        gst_cuda_context_pop (nullptr);
        gst_video_encoder_finish_frame (encoder, frame);
        return GST_FLOW_ERROR;
      }
    }
  }

  nvjpegImage_t source = { };
  if (!gst_nv_jpeg_enc_fill_source (self, frame->input_buffer, &source)) {
    GST_ERROR_OBJECT (self, "Couldn't fill source struct");
    gst_cuda_context_pop (nullptr);
    gst_video_encoder_finish_frame (encoder, frame);

    return GST_FLOW_ERROR;
  }

  auto ret = g_vtable.NvjpegEncodeYUV (priv->handle, priv->state, priv->params,
      &source, priv->subsampling, priv->info.width, priv->info.height, stream);
  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self, "nvjpegEncodeYUV failed, ret: %d", ret);
    gst_cuda_context_pop (nullptr);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  size_t length = 0;
  ret = g_vtable.NvjpegEncodeRetrieveBitstream (priv->handle,
      priv->state, nullptr, &length, stream);
  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self,
        "nvjpegEncodeRetrieveBitstream failed, ret: %d", ret);
    gst_cuda_context_pop (nullptr);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  CuStreamSynchronize (stream);

  auto outbuf = gst_buffer_new_and_alloc (length);
  GstMapInfo map;
  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
  ret = g_vtable.NvjpegEncodeRetrieveBitstream (priv->handle,
      priv->state, (unsigned char *) map.data, &length, stream);
  gst_buffer_unmap (outbuf, &map);
  gst_cuda_context_pop (nullptr);

  if (ret != NVJPEG_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (self,
        "nvjpegEncodeRetrieveBitstream failed, ret: %d", ret);
    gst_buffer_unref (outbuf);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  frame->output_buffer = outbuf;
  frame->dts = frame->pts;
  GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

  return gst_video_encoder_finish_frame (encoder, frame);
}

void
gst_nv_jpeg_enc_register (GstPlugin * plugin, GstCudaContext * context,
    guint rank, gboolean have_nvrtc)
{
  GST_DEBUG_CATEGORY_INIT (gst_nv_jpeg_enc_debug, "nvjpegenc", 0, "nvjpegenc");

  if (!gst_nv_jpeg_enc_load_library ())
    return;

  GType type;
  guint index = 0;
  GTypeInfo type_info = {
    sizeof (GstNvJpegEncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_jpeg_enc_class_init,
    nullptr,
    nullptr,
    sizeof (GstNvJpegEnc),
    0,
    (GInstanceInitFunc) gst_nv_jpeg_enc_init,
  };

  guint cuda_device_id;
  g_object_get (context, "cuda-device-id", &cuda_device_id, nullptr);

  std::string format_string;
  if (have_nvrtc)
    format_string = "NV12, I420, Y42B, Y444";
  else
    format_string = "I420, Y42B, Y444";

  std::string cuda_caps_str = "video/x-raw(memory:CUDAMemory), "
      "format = (string) { " + format_string + " }, width = "
      GST_VIDEO_SIZE_RANGE ", height = " GST_VIDEO_SIZE_RANGE;
  GstCaps *sink_caps = gst_caps_from_string (cuda_caps_str.c_str ());
  GstCaps *sysmem_caps = gst_caps_from_string ("video/x-raw, format = (string)"
      "{ I420, Y42B, Y444 }, width = " GST_VIDEO_SIZE_RANGE
      ", height = " GST_VIDEO_SIZE_RANGE);

  gst_caps_append (sink_caps, sysmem_caps);
  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  auto cdata = g_new0 (GstNvJpegEncCData, 1);
  cdata->cuda_device_id = cuda_device_id;
  cdata->sink_caps = sink_caps;
  cdata->have_nvrtc = have_nvrtc;
  type_info.class_data = cdata;

  auto type_name = g_strdup ("GstNvJpegEnc");
  auto feature_name = g_strdup ("nvjpegenc");
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvJpegDevice%dEnc", index);
    feature_name = g_strdup_printf ("nvjpegdevice%denc", index);
  }

  type = g_type_register_static (GST_TYPE_VIDEO_ENCODER,
      type_name, &type_info, (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
