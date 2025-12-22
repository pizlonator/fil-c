/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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

/**
 * element-cudaupload:
 *
 * Uploads data to NVIDIA GPU via CUDA APIs
 *
 * Since: 1.20
 */

/**
 * element-cudadownload:
 *
 * Downloads data from NVIDIA GPU via CUDA APIs
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcudabasetransform.h"
#include "gstcudamemorycopy.h"
#include "gstcudaformat.h"
#include <gst/cuda/gstcuda-private.h>
#ifdef HAVE_NVCODEC_NVMM
#include "gstcudanvmm.h"
#endif

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#endif
#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#endif

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_cuda_memory_copy_debug);
#define GST_CAT_DEFAULT gst_cuda_memory_copy_debug

typedef struct _GstCudaMemoryCopyClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
} GstCudaMemoryCopyClassData;

struct _GstCudaMemoryCopy
{
  GstCudaBaseTransform parent;
  GstCudaBufferCopyType in_type;
  GstCudaBufferCopyType out_type;

  gboolean downstream_supports_video_meta;

#ifdef HAVE_CUDA_GST_GL
  GstGLDisplay *gl_display;
  GstGLContext *gl_context;
  GstGLContext *other_gl_context;
#endif
#ifdef G_OS_WIN32
  GstD3D11Device *d3d11_device;
#endif
};

typedef struct _GstCudaUpload
{
  GstCudaMemoryCopy parent;
} GstCudaUpload;

typedef struct _GstCudaUploadClass
{
  GstCudaMemoryCopyClass parent_class;
} GstCudaUploadClass;

typedef struct _GstCudaDownload
{
  GstCudaMemoryCopy parent;
} GstCudaDownload;

typedef struct _GstCudaDownloadClass
{
  GstCudaMemoryCopyClass parent_class;
} GstCudaDownloadClass;

/**
 * GstCudaMemoryCopy:
 *
 * Since: 1.22
 */
#define gst_cuda_memory_copy_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstCudaMemoryCopy, gst_cuda_memory_copy,
    GST_TYPE_CUDA_BASE_TRANSFORM);

static void gst_cuda_memory_copy_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_cuda_memory_copy_transform_stop (GstBaseTransform * trans);
static GstCaps *gst_cuda_memory_copy_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_cuda_memory_copy_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static gboolean gst_cuda_memory_copy_propose_allocation (GstBaseTransform *
    trans, GstQuery * decide_query, GstQuery * query);
static gboolean gst_cuda_memory_copy_decide_allocation (GstBaseTransform *
    trans, GstQuery * query);
static gboolean gst_cuda_memory_copy_set_info (GstCudaBaseTransform * btrans,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_cuda_memory_copy_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
#ifdef G_OS_WIN32
static gboolean
gst_cuda_memory_copy_ensure_d3d11_interop (GstCudaContext * context,
    GstD3D11Device * device);
#endif

static void
gst_cuda_memory_copy_class_init (GstCudaMemoryCopyClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCudaBaseTransformClass *btrans_class =
      GST_CUDA_BASE_TRANSFORM_CLASS (klass);

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_set_context);

  trans_class->stop = GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_transform_stop);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_transform_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_decide_allocation);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_query);

  btrans_class->set_info = GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_set_info);

  gst_type_mark_as_plugin_api (GST_TYPE_CUDA_MEMORY_COPY,
      (GstPluginAPIFlags) 0);
}

static void
gst_cuda_memory_copy_init (GstCudaMemoryCopy * self)
{
}

static void
gst_cuda_memory_copy_set_context (GstElement * element, GstContext * context)
{
  /* CUDA context is handled by parent class, handle only non-CUDA context */
#if defined (HAVE_CUDA_GST_GL) || defined (G_OS_WIN32)
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (element);

#ifdef HAVE_CUDA_GST_GL
  gst_gl_handle_set_context (element, context, &self->gl_display,
      &self->other_gl_context);
#endif /* HAVE_CUDA_GST_GL */

#ifdef G_OS_WIN32
  GstCudaBaseTransform *base = GST_CUDA_BASE_TRANSFORM (element);
  if (gst_d3d11_handle_set_context (element, context, -1, &self->d3d11_device)) {
    gboolean compatible = TRUE;

    if (base->context) {
      if (!gst_cuda_memory_copy_ensure_d3d11_interop (base->context,
              self->d3d11_device)) {
        GST_INFO_OBJECT (self, "%" GST_PTR_FORMAT
            " is not CUDA compatible with %" GST_PTR_FORMAT,
            self->d3d11_device, base->context);
        compatible = FALSE;
      }
    } else {
      guint device_count = 0;
      CUdevice device_list[1] = { 0, };
      CUresult cuda_ret;

      cuda_ret = CuD3D11GetDevices (&device_count, device_list, 1,
          gst_d3d11_device_get_device_handle (self->d3d11_device),
          CU_D3D11_DEVICE_LIST_ALL);
      if (cuda_ret != CUDA_SUCCESS || device_count == 0) {
        GST_INFO_OBJECT (self, "%" GST_PTR_FORMAT " is not CUDA compatible",
            self->d3d11_device);
        compatible = FALSE;
      }
    }

    if (!compatible) {
      gst_clear_object (&self->d3d11_device);
    } else {
      GST_INFO_OBJECT (self, "%" GST_PTR_FORMAT " is CUDA compatible",
          self->d3d11_device);
    }
  }
#endif /* G_OS_WIN32 */
#endif /* defined (HAVE_CUDA_GST_GL) || defined (G_OS_WIN32) */

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_cuda_memory_copy_transform_stop (GstBaseTransform * trans)
{
#if defined(HAVE_CUDA_GST_GL) || defined(G_OS_WIN32)
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (trans);

#ifdef HAVE_CUDA_GST_GL
  gst_clear_object (&self->gl_display);
  gst_clear_object (&self->gl_context);
  gst_clear_object (&self->other_gl_context);
#endif
#ifdef G_OS_WIN32
  gst_clear_object (&self->d3d11_device);
#endif
#endif

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static GstCaps *
_set_caps_features (const GstCaps * caps, const gchar * feature_name)
{
  GstCaps *tmp = gst_caps_copy (caps);
  guint n = gst_caps_get_size (tmp);
  guint i = 0;

  for (i = 0; i < n; i++)
    gst_caps_set_features (tmp, i,
        gst_caps_features_from_string (feature_name));

  return tmp;
}

static void
_remove_field (GstCaps * caps, const gchar * field)
{
  guint n = gst_caps_get_size (caps);
  guint i = 0;

  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);
    gst_structure_remove_field (s, field);
  }
}

static GstCaps *
create_transform_caps (GstCaps * caps, gboolean to_cuda)
{
  GstCaps *ret = NULL;
  GstCaps *new_caps = NULL;

  if (to_cuda) {
    /* SRC -> SINK of cudadownload or SINK -> SRC of cudaupload */
    ret = gst_caps_copy (caps);

#ifdef HAVE_NVCODEC_NVMM
    if (gst_cuda_nvmm_init_once ()) {
      new_caps = _set_caps_features (caps,
          GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY);
      ret = gst_caps_merge (ret, new_caps);
    }
#endif

    new_caps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY);
    ret = gst_caps_merge (ret, new_caps);

    ret = gst_caps_make_writable (ret);
    _remove_field (ret, "texture-target");
  } else {
    /* SINK -> SRC of cudadownload or SRC -> SINK of cudaupload */
    ret = gst_caps_ref (caps);

#ifdef HAVE_NVCODEC_NVMM
    if (gst_cuda_nvmm_init_once ()) {
      new_caps = _set_caps_features (caps,
          GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY);
      ret = gst_caps_merge (ret, new_caps);
    }
#endif

#ifdef HAVE_CUDA_GST_GL
    new_caps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    ret = gst_caps_merge (ret, new_caps);
#endif
#ifdef G_OS_WIN32
    new_caps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);
    ret = gst_caps_merge (ret, new_caps);
#endif

    new_caps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    ret = gst_caps_merge (ret, new_caps);

    ret = gst_caps_make_writable (ret);
    _remove_field (ret, "texture-target");
  }

  return ret;
}

static GstCaps *
gst_cuda_memory_copy_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCudaMemoryCopyClass *klass = GST_CUDA_MEMORY_COPY_GET_CLASS (trans);
  GstCaps *result, *tmp;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp = create_transform_caps (caps, klass->uploader);
  } else {
    tmp = create_transform_caps (caps, !klass->uploader);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

#ifdef HAVE_CUDA_GST_GL
static void
gst_cuda_memory_copy_ensure_gl_interop (GstGLContext * context, gboolean * ret)
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
gst_cuda_memory_copy_ensure_gl_context (GstCudaMemoryCopy * self)
{
  GstGLDisplay *display;
  GstGLContext *context;
  gboolean ret = FALSE;

  if (!gst_gl_ensure_element_data (GST_ELEMENT (self),
          &self->gl_display, &self->other_gl_context)) {
    GST_DEBUG_OBJECT (self, "No available OpenGL display");
    return FALSE;
  }

  display = self->gl_display;

  if (!gst_gl_query_local_gl_context (GST_ELEMENT (self), GST_PAD_SRC,
          &self->gl_context) &&
      !gst_gl_query_local_gl_context (GST_ELEMENT (self), GST_PAD_SINK,
          &self->gl_context)) {
    GST_INFO_OBJECT (self, "failed to query local OpenGL context");

    gst_clear_object (&self->gl_context);
    self->gl_context = gst_gl_display_get_gl_context_for_thread (display, NULL);
    if (!self->gl_context
        || !gst_gl_display_add_context (display,
            GST_GL_CONTEXT (self->gl_context))) {
      gst_clear_object (&self->gl_context);
      if (!gst_gl_display_create_context (display,
              self->other_gl_context, &self->gl_context, NULL)) {
        GST_WARNING_OBJECT (self, "failed to create OpenGL context");
        return FALSE;
      }

      if (!gst_gl_display_add_context (display, self->gl_context)) {
        GST_WARNING_OBJECT (self,
            "failed to add the OpenGL context to the display");
        return FALSE;
      }
    }
  }

  context = self->gl_context;

  if (!gst_gl_context_check_gl_version (context,
          (GstGLAPI) (GST_GL_API_OPENGL | GST_GL_API_OPENGL3), 3, 0)) {
    GST_WARNING_OBJECT (self, "OpenGL context could not support PBO download");
    return FALSE;
  }

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) gst_cuda_memory_copy_ensure_gl_interop, &ret);
  if (!ret) {
    GST_WARNING_OBJECT (self, "Current GL context is not CUDA compatible");
    return FALSE;
  }

  return TRUE;
}
#endif

#ifdef G_OS_WIN32
static gboolean
gst_cuda_memory_copy_ensure_d3d11_interop (GstCudaContext * context,
    GstD3D11Device * device)
{
  guint device_count = 0;
  CUdevice cuda_device_id;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  g_object_get (context, "cuda-device-id", &cuda_device_id, NULL);

  cuda_ret = CuD3D11GetDevices (&device_count,
      device_list, 1, gst_d3d11_device_get_device_handle (device),
      CU_D3D11_DEVICE_LIST_ALL);

  if (cuda_ret != CUDA_SUCCESS || device_count == 0)
    return FALSE;

  if (device_list[0] != cuda_device_id)
    return FALSE;

  return TRUE;
}

static gboolean
gst_cuda_memory_copy_ensure_d3d11_context (GstCudaMemoryCopy * self)
{
  gint64 dxgi_adapter_luid = 0;

  g_object_get (GST_CUDA_BASE_TRANSFORM (self)->context, "dxgi-adapter-luid",
      &dxgi_adapter_luid, NULL);

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT (self),
          dxgi_adapter_luid, &self->d3d11_device)) {
    GST_DEBUG_OBJECT (self, "No available D3D11 device");
    return FALSE;
  }

  if (!gst_cuda_memory_copy_ensure_d3d11_interop (GST_CUDA_BASE_TRANSFORM
          (self)->context, self->d3d11_device)) {
    GST_WARNING_OBJECT (self, "Current D3D11 device is not CUDA compatible");
    return FALSE;
  }

  return TRUE;
}
#endif

static gboolean
gst_cuda_memory_copy_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (trans);
  GstCudaBaseTransform *ctrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  guint size;
  gboolean is_cuda = FALSE;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (decide_query == NULL)
    return TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstCapsFeatures *features;
    GstStructure *config;

    features = gst_caps_get_features (caps, 0);

    if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
      GST_DEBUG_OBJECT (self, "upstream support CUDA memory");
      pool = gst_cuda_buffer_pool_new (ctrans->context);
#ifdef HAVE_CUDA_GST_GL
    } else if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_GL_MEMORY) &&
        gst_cuda_memory_copy_ensure_gl_context (self)) {
      GST_DEBUG_OBJECT (self, "upstream support GL memory");

      pool = gst_gl_buffer_pool_new (self->gl_context);
#endif
#ifdef G_OS_WIN32
    } else if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY) &&
        gst_cuda_memory_copy_ensure_d3d11_context (self)) {
      GST_DEBUG_OBJECT (self, "upstream support D3D11 memory");

      pool = gst_d3d11_buffer_pool_new (self->d3d11_device);
#endif
#ifdef HAVE_NVCODEC_NVMM
    } else if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY) &&
        gst_cuda_nvmm_init_once ()) {
      guint gpu_id = 0;
      GST_DEBUG_OBJECT (self, "upstream support NVMM memory");

      g_object_get (ctrans->context, "cuda-device-id", &gpu_id, NULL);

      pool = gst_cuda_nvmm_buffer_pool_new ();
      if (!pool) {
        GST_ERROR_OBJECT (self, "Failed to create pool");
        return FALSE;
      }

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps,
          sizeof (NvBufSurface), 0, 0);

      gst_structure_set (config, "memtype", G_TYPE_UINT, NVBUF_MEM_DEFAULT,
          "gpu-id", G_TYPE_UINT, gpu_id, "batch-size", G_TYPE_UINT, 1, NULL);

      if (!gst_buffer_pool_set_config (pool, config)) {
        GST_ERROR_OBJECT (self, "Failed to set config");
        gst_object_unref (pool);
        return FALSE;
      }

      gst_query_add_allocation_pool (query, pool, sizeof (NvBufSurface), 0, 0);

      return TRUE;
#endif
    }

    if (!pool) {
      GST_DEBUG_OBJECT (self, "creating system buffer pool");
      pool = gst_video_buffer_pool_new ();
    }

    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    if (is_cuda && ctrans->stream)
      gst_buffer_pool_config_set_cuda_stream (config, ctrans->stream);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (ctrans, "failed to set config");
      gst_object_unref (pool);
      return FALSE;
    }

    /* Get updated size by cuda buffer pool */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);

    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static gboolean
gst_cuda_memory_copy_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (trans);
  GstCudaBaseTransform *ctrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstStructure *config;
  gboolean update_pool = FALSE;
  GstCapsFeatures *features;
  gboolean need_cuda = FALSE;
#ifdef HAVE_CUDA_GST_GL
  gboolean need_gl = FALSE;
#endif
#ifdef G_OS_WIN32
  gboolean need_d3d11 = FALSE;
#endif
#ifdef HAVE_NVCODEC_NVMM
  gboolean need_nvmm = FALSE;
#endif

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  self->downstream_supports_video_meta =
      gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  GST_DEBUG_OBJECT (self, "Downstream supports video meta: %d",
      self->downstream_supports_video_meta);

  features = gst_caps_get_features (outcaps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
    need_cuda = TRUE;
  }
#ifdef HAVE_CUDA_GST_GL
  else if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY) &&
      gst_cuda_memory_copy_ensure_gl_context (self)) {
    need_gl = TRUE;
  }
#endif
#ifdef G_OS_WIN32
  else if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY) &&
      gst_cuda_memory_copy_ensure_d3d11_context (self)) {
    need_d3d11 = TRUE;
  }
#endif
#ifdef HAVE_NVCODEC_NVMM
  else if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY) &&
      gst_cuda_nvmm_init_once ()) {
    need_nvmm = TRUE;
  }
#endif

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (need_cuda && pool) {
      if (!GST_IS_CUDA_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstCudaBufferPool *cpool = GST_CUDA_BUFFER_POOL (pool);

        if (cpool->context != ctrans->context) {
          gst_clear_object (&pool);
        }
      }
    }
#ifdef HAVE_NVCODEC_NVMM
    if (need_nvmm) {
      /* XXX: Always create new pool to set config option */
      gst_clear_object (&pool);
    }
#endif

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;
    gst_video_info_from_caps (&vinfo, outcaps);
    size = GST_VIDEO_INFO_SIZE (&vinfo);
    min = max = 0;
  }

  if (!pool) {
    if (need_cuda) {
      GST_DEBUG_OBJECT (self, "creating cuda pool");
      pool = gst_cuda_buffer_pool_new (ctrans->context);
    }
#ifdef HAVE_CUDA_GST_GL
    else if (need_gl) {
      GST_DEBUG_OBJECT (self, "creating gl pool");
      pool = gst_gl_buffer_pool_new (self->gl_context);
    }
#endif
#ifdef G_OS_WIN32
    else if (need_d3d11) {
      GST_DEBUG_OBJECT (self, "creating d3d11 pool");
      pool = gst_d3d11_buffer_pool_new (self->d3d11_device);
    }
#endif
#ifdef HAVE_NVCODEC_NVMM
    else if (need_nvmm) {
      guint gpu_id = 0;
      GST_DEBUG_OBJECT (self, "create nvmm pool");

      g_object_get (ctrans->context, "cuda-device-id", &gpu_id, NULL);

      pool = gst_cuda_nvmm_buffer_pool_new ();
      if (!pool) {
        GST_ERROR_OBJECT (self, "Failed to create pool");
        return FALSE;
      }

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, outcaps,
          sizeof (NvBufSurface), min, max);

      gst_structure_set (config, "memtype", G_TYPE_UINT, NVBUF_MEM_DEFAULT,
          "gpu-id", G_TYPE_UINT, gpu_id, "batch-size", G_TYPE_UINT, 1, NULL);

      if (!gst_buffer_pool_set_config (pool, config)) {
        GST_ERROR_OBJECT (self, "Failed to set config");
        gst_object_unref (pool);
        return FALSE;
      }

      if (update_pool) {
        gst_query_set_nth_allocation_pool (query,
            0, pool, sizeof (NvBufSurface), min, max);
      } else {
        gst_query_add_allocation_pool (query,
            pool, sizeof (NvBufSurface), min, max);
      }

      gst_object_unref (pool);

      /* Don't chain up to parent method, which will break NVMM specific
       * config */

      return TRUE;
    }
#endif
    else {
      GST_DEBUG_OBJECT (self, "creating system pool");
      pool = gst_video_buffer_pool_new ();
    }
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_set_config (pool, config);

  /* Get updated size by cuda buffer pool */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_cuda_memory_copy_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
#if defined(HAVE_CUDA_GST_GL) || defined(G_OS_WIN32)
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      gboolean ret;
#ifdef HAVE_CUDA_GST_GL
      ret = gst_gl_handle_context_query (GST_ELEMENT (self), query,
          self->gl_display, self->gl_context, self->other_gl_context);
      if (ret)
        return TRUE;
#endif
#ifdef G_OS_WIN32
      ret = gst_d3d11_handle_context_query (GST_ELEMENT (self), query,
          self->d3d11_device);
      if (ret)
        return TRUE;
#endif
      break;
    }
    default:
      break;
  }
#endif

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static gboolean
gst_cuda_memory_copy_set_info (GstCudaBaseTransform * btrans,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (btrans);
  GstCapsFeatures *in_features;
  GstCapsFeatures *out_features;

  self->in_type = GST_CUDA_BUFFER_COPY_SYSTEM;
  self->out_type = GST_CUDA_BUFFER_COPY_SYSTEM;

  in_features = gst_caps_get_features (incaps, 0);
  out_features = gst_caps_get_features (outcaps, 0);
  if (in_features && gst_caps_features_contains (in_features,
          GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
    self->in_type = GST_CUDA_BUFFER_COPY_CUDA;
  }

  if (out_features && gst_caps_features_contains (out_features,
          GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
    self->out_type = GST_CUDA_BUFFER_COPY_CUDA;
  }
#ifdef HAVE_CUDA_GST_GL
  if (in_features && gst_caps_features_contains (in_features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
    self->in_type = GST_CUDA_BUFFER_COPY_GL;
  }

  if (out_features && gst_caps_features_contains (out_features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
    self->out_type = GST_CUDA_BUFFER_COPY_GL;
  }
#endif

#ifdef G_OS_WIN32
  if (in_features && gst_caps_features_contains (in_features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    self->in_type = GST_CUDA_BUFFER_COPY_D3D11;
  }

  if (out_features && gst_caps_features_contains (out_features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    self->out_type = GST_CUDA_BUFFER_COPY_D3D11;
  }

  /* Clear d3d11 device, this set_info() might be called to update
   * cuda context and therefore d3d11 device object should be updated as well */
  gst_clear_object (&self->d3d11_device);
#endif

#ifdef HAVE_NVCODEC_NVMM
  if (gst_cuda_nvmm_init_once ()) {
    if (in_features && gst_caps_features_contains (in_features,
            GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY)) {
      GST_DEBUG_OBJECT (self, "Input memory type is NVMM");
      self->in_type = GST_CUDA_BUFFER_COPY_NVMM;
    }

    if (out_features && gst_caps_features_contains (out_features,
            GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY)) {
      GST_DEBUG_OBJECT (self, "Output memory type is NVMM");
      self->out_type = GST_CUDA_BUFFER_COPY_NVMM;
    }
  }
#endif

  return TRUE;
}

static GstFlowReturn
gst_cuda_memory_copy_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstCudaMemoryCopy *self = GST_CUDA_MEMORY_COPY (trans);
  GstCudaBaseTransform *ctrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstMemory *in_mem;
  GstMemory *out_mem;
  GstVideoInfo *in_info, *out_info;
  gboolean ret = FALSE;
  GstCudaBufferCopyType in_type = GST_CUDA_BUFFER_COPY_SYSTEM;
  GstCudaBufferCopyType out_type = GST_CUDA_BUFFER_COPY_SYSTEM;
  gboolean use_device_copy = FALSE;
#ifdef G_OS_WIN32
  D3D11_TEXTURE2D_DESC desc;
#endif

  in_info = &ctrans->in_info;
  out_info = &ctrans->out_info;

  in_mem = gst_buffer_peek_memory (inbuf, 0);
  if (!in_mem) {
    GST_ERROR_OBJECT (self, "Empty input buffer");
    return GST_FLOW_ERROR;
  }

  out_mem = gst_buffer_peek_memory (outbuf, 0);
  if (!out_mem) {
    GST_ERROR_OBJECT (self, "Empty output buffer");
    return GST_FLOW_ERROR;
  }

  if (self->in_type == GST_CUDA_BUFFER_COPY_NVMM) {
    in_type = GST_CUDA_BUFFER_COPY_NVMM;
    use_device_copy = TRUE;
  } else if (gst_is_cuda_memory (in_mem)) {
    in_type = GST_CUDA_BUFFER_COPY_CUDA;
    use_device_copy = TRUE;
#ifdef HAVE_CUDA_GST_GL
  } else if (self->gl_context && gst_is_gl_memory_pbo (in_mem)) {
    in_type = GST_CUDA_BUFFER_COPY_GL;
#endif
#ifdef G_OS_WIN32
  } else if (self->d3d11_device && gst_is_d3d11_memory (in_mem)
      && gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (in_mem),
          &desc) && desc.Usage == D3D11_USAGE_DEFAULT) {
    in_type = GST_CUDA_BUFFER_COPY_D3D11;
#endif
  } else {
    in_type = GST_CUDA_BUFFER_COPY_SYSTEM;
  }

  if (self->out_type == GST_CUDA_BUFFER_COPY_NVMM) {
    out_type = GST_CUDA_BUFFER_COPY_NVMM;
    use_device_copy = TRUE;
  } else if (gst_is_cuda_memory (out_mem)) {
    out_type = GST_CUDA_BUFFER_COPY_CUDA;
    use_device_copy = TRUE;
#ifdef HAVE_CUDA_GST_GL
  } else if (self->gl_context && gst_is_gl_memory_pbo (out_mem)) {
    out_type = GST_CUDA_BUFFER_COPY_GL;
#endif
#ifdef G_OS_WIN32
  } else if (self->d3d11_device && gst_is_d3d11_memory (out_mem)
      && gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (out_mem),
          &desc) && desc.Usage == D3D11_USAGE_DEFAULT) {
    out_type = GST_CUDA_BUFFER_COPY_D3D11;
#endif
  } else {
    out_type = GST_CUDA_BUFFER_COPY_SYSTEM;
  }

  if (!use_device_copy) {
    GST_TRACE_OBJECT (self, "Both in/out buffers are not CUDA");
    if (!gst_cuda_buffer_copy (outbuf, GST_CUDA_BUFFER_COPY_SYSTEM, out_info,
            inbuf, GST_CUDA_BUFFER_COPY_SYSTEM, in_info, ctrans->context,
            ctrans->stream)) {
      return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
  }

  ret = gst_cuda_buffer_copy (outbuf, out_type, out_info, inbuf, in_type,
      in_info, ctrans->context, ctrans->stream);

  /* system memory <-> CUDA copy fallback if possible */
  if (!ret) {
    GstCudaBufferCopyType fallback_in_type = in_type;
    GstCudaBufferCopyType fallback_out_type = out_type;

    GST_LOG_OBJECT (self,
        "Copy %s -> %s failed, checking whether fallback is possible",
        gst_cuda_buffer_copy_type_to_string (in_type),
        gst_cuda_buffer_copy_type_to_string (out_type));

    switch (in_type) {
      case GST_CUDA_BUFFER_COPY_GL:
      case GST_CUDA_BUFFER_COPY_D3D11:
        fallback_in_type = GST_CUDA_BUFFER_COPY_SYSTEM;
        break;
      default:
        break;
    }

    switch (out_type) {
      case GST_CUDA_BUFFER_COPY_GL:
      case GST_CUDA_BUFFER_COPY_D3D11:
        fallback_out_type = GST_CUDA_BUFFER_COPY_SYSTEM;
        break;
      default:
        break;
    }

    if (in_type == fallback_in_type && out_type == fallback_out_type) {
      GST_ERROR_OBJECT (self, "Failed to copy %s -> %s",
          gst_cuda_buffer_copy_type_to_string (in_type),
          gst_cuda_buffer_copy_type_to_string (out_type));

      return GST_FLOW_ERROR;
    }

    GST_LOG_OBJECT (self, "Trying %s -> %s fallback",
        gst_cuda_buffer_copy_type_to_string (fallback_in_type),
        gst_cuda_buffer_copy_type_to_string (fallback_out_type));

    ret = gst_cuda_buffer_copy (outbuf, fallback_out_type, out_info, inbuf,
        fallback_in_type, in_info, ctrans->context, ctrans->stream);
  }

  if (ret)
    return GST_FLOW_OK;

  if (in_type == GST_CUDA_BUFFER_COPY_NVMM ||
      out_type == GST_CUDA_BUFFER_COPY_NVMM) {
    GST_ERROR_OBJECT (self, "Failed to copy NVMM memory");
    return GST_FLOW_ERROR;
  }

  /* final fallback using system memory */
  ret = gst_cuda_buffer_copy (outbuf, GST_CUDA_BUFFER_COPY_SYSTEM, out_info,
      inbuf, GST_CUDA_BUFFER_COPY_SYSTEM, in_info, ctrans->context,
      ctrans->stream);

  if (ret)
    return GST_FLOW_OK;

  GST_ERROR_OBJECT (self, "Failed to copy %s -> %s",
      gst_cuda_buffer_copy_type_to_string (in_type),
      gst_cuda_buffer_copy_type_to_string (out_type));

  return GST_FLOW_ERROR;
}

static void
gst_cuda_upload_class_init (GstCudaUploadClass * klass, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCudaMemoryCopyClass *copy_class = GST_CUDA_MEMORY_COPY_CLASS (klass);
  GstCudaMemoryCopyClassData *cdata = (GstCudaMemoryCopyClassData *) data;

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  gst_element_class_set_static_metadata (element_class,
      "CUDA uploader", "Filter/Video",
      "Uploads data into NVIDA GPU via CUDA APIs",
      "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->transform = GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_transform);

  copy_class->uploader = TRUE;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_cuda_upload_init (GstCudaUpload * self)
{
}

static void
gst_cuda_download_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstCudaMemoryCopy *copy = GST_CUDA_MEMORY_COPY (trans);
  gboolean old;
  gboolean new = FALSE;

  GST_BASE_TRANSFORM_CLASS (parent_class)->before_transform (trans, buffer);

  old = gst_base_transform_is_passthrough (trans);
  if (copy->in_type == copy->out_type) {
    new = TRUE;
  } else if (copy->in_type == GST_CUDA_BUFFER_COPY_CUDA &&
      copy->out_type == GST_CUDA_BUFFER_COPY_SYSTEM &&
      copy->downstream_supports_video_meta) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, 0);
    /* zero-copy decoded memory belongs to NVDEC's fixed size pool.
     * We should return the memory to the decoder as soon as possible */
    if (!gst_cuda_memory_is_from_fixed_pool (mem))
      new = TRUE;
  }

  if (new != old) {
    GST_INFO_OBJECT (trans, "Updated passthrough: %d", new);
    gst_base_transform_reconfigure_src (trans);
    gst_base_transform_set_passthrough (trans, new);
  }
}

static void
gst_cuda_download_class_init (GstCudaDownloadClass * klass, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCudaMemoryCopyClass *copy_class = GST_CUDA_MEMORY_COPY_CLASS (klass);
  GstCudaMemoryCopyClassData *cdata = (GstCudaMemoryCopyClassData *) data;

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  gst_element_class_set_static_metadata (element_class,
      "CUDA downloader", "Filter/Video",
      "Downloads data from NVIDA GPU via CUDA APIs",
      "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_cuda_download_before_transform);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_cuda_memory_copy_transform);

  copy_class->uploader = FALSE;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_cuda_download_init (GstCudaDownload * self)
{
}

void
gst_cuda_memory_copy_register (GstPlugin * plugin, guint rank)
{
  GType upload_type, download_type;
  GTypeInfo upload_type_info = {
    sizeof (GstCudaUploadClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_cuda_upload_class_init,
    NULL,
    NULL,
    sizeof (GstCudaUpload),
    0,
    (GInstanceInitFunc) gst_cuda_upload_init,
  };
  GTypeInfo download_type_info = {
    sizeof (GstCudaDownloadClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_cuda_download_class_init,
    NULL,
    NULL,
    sizeof (GstCudaDownload),
    0,
    (GInstanceInitFunc) gst_cuda_download_init,
  };
  GstCaps *sys_caps;
  GstCaps *cuda_caps;
#ifdef HAVE_NVCODEC_NVMM
  GstCaps *nvmm_caps = NULL;
#endif
#ifdef HAVE_CUDA_GST_GL
  GstCaps *gl_caps;
#endif
#ifdef G_OS_WIN32
  GstCaps *d3d11_caps;
#endif
  GstCaps *upload_sink_caps;
  GstCaps *upload_src_caps;
  GstCaps *download_sink_caps;
  GstCaps *download_src_caps;
  GstCudaMemoryCopyClassData *upload_cdata;
  GstCudaMemoryCopyClassData *download_cdata;

  GST_DEBUG_CATEGORY_INIT (gst_cuda_memory_copy_debug,
      "cudamemorycopy", 0, "cudamemorycopy");

  sys_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (GST_CUDA_FORMATS));
  cuda_caps =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
      (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, GST_CUDA_FORMATS));
#ifdef HAVE_NVCODEC_NVMM
  if (gst_cuda_nvmm_init_once ()) {
    nvmm_caps =
        gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_CUDA_NVMM_MEMORY, GST_CUDA_NVMM_FORMATS));
  }
#endif
#ifdef HAVE_CUDA_GST_GL
  gl_caps =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
      (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, GST_CUDA_GL_FORMATS));
#endif
#ifdef G_OS_WIN32
  d3d11_caps =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
      (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_CUDA_D3D11_FORMATS));
#endif

  upload_sink_caps = gst_caps_copy (sys_caps);
#ifdef HAVE_CUDA_GST_GL
  upload_sink_caps = gst_caps_merge (upload_sink_caps, gst_caps_copy (gl_caps));
#endif
#ifdef G_OS_WIN32
  upload_sink_caps =
      gst_caps_merge (upload_sink_caps, gst_caps_copy (d3d11_caps));
#endif
#ifdef HAVE_NVCODEC_NVMM
  if (nvmm_caps) {
    upload_sink_caps = gst_caps_merge (upload_sink_caps,
        gst_caps_copy (nvmm_caps));
  }
#endif
  upload_sink_caps =
      gst_caps_merge (upload_sink_caps, gst_caps_copy (cuda_caps));

  upload_src_caps = gst_caps_copy (cuda_caps);
#ifdef HAVE_NVCODEC_NVMM
  if (nvmm_caps) {
    upload_src_caps = gst_caps_merge (upload_src_caps,
        gst_caps_copy (nvmm_caps));
  }
#endif
  upload_src_caps = gst_caps_merge (upload_src_caps, gst_caps_copy (sys_caps));

  download_sink_caps = gst_caps_copy (cuda_caps);
#ifdef HAVE_NVCODEC_NVMM
  if (nvmm_caps) {
    download_sink_caps = gst_caps_merge (download_sink_caps,
        gst_caps_copy (nvmm_caps));
  }
#endif
  download_sink_caps =
      gst_caps_merge (download_sink_caps, gst_caps_copy (sys_caps));

  download_src_caps = sys_caps;
#ifdef HAVE_CUDA_GST_GL
  download_src_caps = gst_caps_merge (download_src_caps, gl_caps);
#endif
#ifdef G_OS_WIN32
  download_src_caps = gst_caps_merge (download_src_caps, d3d11_caps);
#endif
#ifdef HAVE_NVCODEC_NVMM
  if (nvmm_caps) {
    download_src_caps = gst_caps_merge (download_src_caps, nvmm_caps);
  }
#endif
  download_src_caps = gst_caps_merge (download_src_caps, cuda_caps);

  GST_MINI_OBJECT_FLAG_SET (upload_sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (upload_src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GST_MINI_OBJECT_FLAG_SET (download_sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (download_src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  upload_cdata = g_new0 (GstCudaMemoryCopyClassData, 1);
  upload_cdata->sink_caps = upload_sink_caps;
  upload_cdata->src_caps = upload_src_caps;
  upload_type_info.class_data = upload_cdata;

  download_cdata = g_new0 (GstCudaMemoryCopyClassData, 1);
  download_cdata->sink_caps = download_sink_caps;
  download_cdata->src_caps = download_src_caps;
  download_type_info.class_data = download_cdata;

  upload_type = g_type_register_static (GST_TYPE_CUDA_MEMORY_COPY,
      "GstCudaUpload", &upload_type_info, 0);
  download_type = g_type_register_static (GST_TYPE_CUDA_MEMORY_COPY,
      "GstCudaDownload", &download_type_info, 0);

  if (!gst_element_register (plugin, "cudaupload", rank, upload_type))
    GST_WARNING ("Failed to register cudaupload element");

  if (!gst_element_register (plugin, "cudadownload", rank, download_type))
    GST_WARNING ("Failed to register cudadownload element");
}
