/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystree00@gmail.com>
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

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
#include <gst/gl/egl/gsteglimage.h>
#include <gst/allocators/gstdmabuf.h>
#endif

#include "gstglelements.h"
#include "gstgldownloadelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_download_element_debug);
#define GST_CAT_DEFAULT gst_gl_download_element_debug

#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <gst/gl/egl/gstglmemoryegl.h>

#include "nvbuf_utils.h"

static const char *
nv_buffer_payload_type_to_string (NvBufferPayloadType ptype)
{
  switch (ptype) {
    case NvBufferPayload_SurfArray:
      return "SurfArray";
    case NvBufferPayload_MemHandle:
      return "MemHandle";
    default:
      return "<unknown>";
  }
}

static const char *
nv_buffer_pixel_format_to_string (NvBufferColorFormat fmt)
{
  switch (fmt) {
    case NvBufferColorFormat_YUV420:
      return "YUV420";
    case NvBufferColorFormat_YVU420:
      return "YVU420";
    case NvBufferColorFormat_YUV422:
      return "YUV422";
    case NvBufferColorFormat_YUV420_ER:
      return "YUV420_ER";
    case NvBufferColorFormat_YVU420_ER:
      return "YVU420_ER";
    case NvBufferColorFormat_NV12:
      return "NV12";
    case NvBufferColorFormat_NV12_ER:
      return "NV12_ER";
    case NvBufferColorFormat_NV21:
      return "NV21";
    case NvBufferColorFormat_NV21_ER:
      return "NV21_ER";
    case NvBufferColorFormat_UYVY:
      return "UYVY";
    case NvBufferColorFormat_UYVY_ER:
      return "UYVY_ER";
    case NvBufferColorFormat_VYUY:
      return "VYUY";
    case NvBufferColorFormat_VYUY_ER:
      return "VYUY_ER";
    case NvBufferColorFormat_YUYV:
      return "YUYV";
    case NvBufferColorFormat_YUYV_ER:
      return "YUYV_ER";
    case NvBufferColorFormat_YVYU:
      return "YVYU";
    case NvBufferColorFormat_YVYU_ER:
      return "YVYU_ER";
    case NvBufferColorFormat_ABGR32:
      return "ABGR32";
    case NvBufferColorFormat_XRGB32:
      return "XRGB32";
    case NvBufferColorFormat_ARGB32:
      return "ARGB32";
    case NvBufferColorFormat_NV12_10LE:
      return "NV12_10LE";
    case NvBufferColorFormat_NV12_10LE_709:
      return "NV12_10LE_709";
    case NvBufferColorFormat_NV12_10LE_709_ER:
      return "NV12_10LE_709_ER";
    case NvBufferColorFormat_NV12_10LE_2020:
      return "NV12_2020";
    case NvBufferColorFormat_NV21_10LE:
      return "NV21_10LE";
    case NvBufferColorFormat_NV12_12LE:
      return "NV12_12LE";
    case NvBufferColorFormat_NV12_12LE_2020:
      return "NV12_12LE_2020";
    case NvBufferColorFormat_NV21_12LE:
      return "NV21_12LE";
    case NvBufferColorFormat_YUV420_709:
      return "YUV420_709";
    case NvBufferColorFormat_YUV420_709_ER:
      return "YUV420_709_ER";
    case NvBufferColorFormat_NV12_709:
      return "NV12_709";
    case NvBufferColorFormat_NV12_709_ER:
      return "NV12_709_ER";
    case NvBufferColorFormat_YUV420_2020:
      return "YUV420_2020";
    case NvBufferColorFormat_NV12_2020:
      return "NV12_2020";
    case NvBufferColorFormat_SignedR16G16:
      return "SignedR16G16";
    case NvBufferColorFormat_A32:
      return "A32";
    case NvBufferColorFormat_YUV444:
      return "YUV444";
    case NvBufferColorFormat_GRAY8:
      return "GRAY8";
    case NvBufferColorFormat_NV16:
      return "NV16";
    case NvBufferColorFormat_NV16_10LE:
      return "NV16_10LE";
    case NvBufferColorFormat_NV24:
      return "NV24";
    case NvBufferColorFormat_NV16_ER:
      return "NV16_ER";
    case NvBufferColorFormat_NV24_ER:
      return "NV24_ER";
    case NvBufferColorFormat_NV16_709:
      return "NV16_709";
    case NvBufferColorFormat_NV24_709:
      return "NV24_709";
    case NvBufferColorFormat_NV16_709_ER:
      return "NV16_709_ER";
    case NvBufferColorFormat_NV24_709_ER:
      return "NV24_709_ER";
    case NvBufferColorFormat_NV24_10LE_709:
      return "NV24_10LE_709";
    case NvBufferColorFormat_NV24_10LE_709_ER:
      return "NV24_10LE_709_ER";
    case NvBufferColorFormat_NV24_10LE_2020:
      return "NV24_10LE_2020";
    case NvBufferColorFormat_NV24_12LE_2020:
      return "NV24_12LE_2020";
    case NvBufferColorFormat_RGBA_10_10_10_2_709:
      return "RGBA_10_10_10_2_709";
    case NvBufferColorFormat_RGBA_10_10_10_2_2020:
      return "RGBA_10_10_10_2_2020";
    case NvBufferColorFormat_BGRA_10_10_10_2_709:
      return "BGRA_10_10_10_2_709";
    case NvBufferColorFormat_BGRA_10_10_10_2_2020:
      return "BGRA_10_10_10_2_2020";
    case NvBufferColorFormat_Invalid:
      return "Invalid";
    default:
      return "<unknown>";
  }
}

static void
nv_buffer_dump_params (GstObject * debug_object, NvBufferParamsEx * params)
{
  GST_DEBUG_OBJECT (debug_object, "nvbuffer fd: %u size %i nv_buffer: %p of "
      "size %u, payload: (0x%x) %s, pixel format: (0x%x) %s, n_planes: %u, "
      "plane 0 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u } "
      "plane 1 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u } "
      "plane 2 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u }",
      params->params.dmabuf_fd, params->params.memsize,
      params->params.nv_buffer, params->params.nv_buffer_size,
      params->params.payloadType,
      nv_buffer_payload_type_to_string (params->params.payloadType),
      params->params.pixel_format,
      nv_buffer_pixel_format_to_string (params->params.pixel_format),
      params->params.num_planes, params->params.width[0],
      params->params.height[0], params->params.pitch[0],
      params->params.offset[0], params->params.psize[0],
      params->params.offset[0], params->params.width[1],
      params->params.height[1], params->params.pitch[1],
      params->params.offset[1], params->params.psize[1],
      params->params.offset[1], params->params.width[2],
      params->params.height[2], params->params.pitch[2],
      params->params.offset[2], params->params.psize[2],
      params->params.offset[2]);
}

struct _GstMemoryNVMM
{
  GstMemory parent;

  int dmabuf_fd;
  NvBufferParamsEx params;
};

typedef struct _GstMemoryNVMM GstMemoryNVMM;

struct _GstAllocatorNVMM
{
  GstAllocator parent;
};

typedef struct _GstAllocatorNVMM GstAllocatorNVMM;

struct _GstAllocatorNVMMClass
{
  GstAllocatorClass parent_class;
};

typedef struct _GstAllocatorNVMMClass GstAllocatorNVMMClass;

GType gst_allocator_nvmm_get_type (void);
G_DEFINE_TYPE (GstAllocatorNVMM, gst_allocator_nvmm, GST_TYPE_ALLOCATOR);

static gboolean
gst_memory_nvmm_init (GstMemoryNVMM * nvmm, GstMemoryFlags flags,
    GstAllocator * allocator, GstMemory * parent, const GstVideoInfo * vinfo)
{
  gsize size = NvBufferGetSize ();
  NvBufferCreateParams create_params = {
    .width = GST_VIDEO_INFO_WIDTH (vinfo),
    .height = GST_VIDEO_INFO_HEIGHT (vinfo),
    .payloadType = NvBufferPayload_SurfArray,
    .memsize = GST_VIDEO_INFO_SIZE (vinfo),
    .layout = NvBufferLayout_BlockLinear,
    .colorFormat = NvBufferColorFormat_ABGR32,
    .nvbuf_tag = NvBufferTag_NONE,
  };

  nvmm->dmabuf_fd = -1;

  if (NvBufferCreateEx (&nvmm->dmabuf_fd, &create_params)) {
    GST_WARNING_OBJECT (allocator, "Failed to create NvBuffer");
    return FALSE;
  }

  if (NvBufferGetParamsEx (nvmm->dmabuf_fd, &nvmm->params)) {
    GST_WARNING_OBJECT (allocator, "Failed to get NvBuffer params");
    NvReleaseFd (nvmm->dmabuf_fd);
    nvmm->dmabuf_fd = -1;
    return FALSE;
  }
  nv_buffer_dump_params ((GstObject *) allocator, &nvmm->params);

  gst_memory_init (&nvmm->parent, flags, allocator, parent, size, 0, 0, size);

  return TRUE;
}

static gpointer
gst_memory_nvmm_map_full (GstMemory * mem, GstMapInfo * info, gsize size)
{
  GstMemoryNVMM *nvmm = (GstMemoryNVMM *) mem;

  GST_TRACE ("%p fd:%i map", mem, nvmm->dmabuf_fd);

  // This is what the Nvidia elements do so...
  return nvmm->params.params.nv_buffer;
}

static void
gst_memory_nvmm_unmap_full (GstMemory * mem, GstMapInfo * info)
{
  GstMemoryNVMM *nvmm = (GstMemoryNVMM *) mem;

  GST_TRACE ("%p fd:%i unmap", mem, nvmm->dmabuf_fd);
}

static GstMemory *
gst_memory_nvmm_copy (GstMemory * mem, gssize offset, gssize size)
{
  return NULL;
}

static GstMemory *
gst_memory_nvmm_share (GstMemory * mem, gssize offset, gssize size)
{
  return NULL;
}

static gboolean
gst_memory_nvmm_is_span (GstMemory * mem, GstMemory * mem2, gsize * offset)
{
  return FALSE;
}

static gboolean
gst_is_memory_nvmm (GstMemory * mem)
{
  return mem && mem->allocator
      && g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      gst_allocator_nvmm_get_type ());
}

static GstAllocator *_nvmm_allocator;

static void
init_nvmm_allocator (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    _nvmm_allocator = g_object_new (gst_allocator_nvmm_get_type (), NULL);
/*    gst_allocator_register ("NvBuffer", _nvmm_allocator); */
    GST_OBJECT_FLAG_SET (_nvmm_allocator, GST_OBJECT_FLAG_MAY_BE_LEAKED);
    g_once_init_leave (&_init, 1);
  }
}

static GstMemory *
gst_allocator_nvmm_alloc (const GstVideoInfo * info)
{
  GstMemoryNVMM *nvmm = g_new0 (GstMemoryNVMM, 1);

  init_nvmm_allocator ();

  if (!gst_memory_nvmm_init (nvmm, 0, _nvmm_allocator, NULL, info)) {
    g_free (nvmm);
    return NULL;
  }

  return (GstMemory *) nvmm;
}

static GstMemory *
_gst_allocator_nvmm_alloc (GstAllocator * alloc, gsize size,
    GstAllocationParams * params)
{
  g_warning
      ("Can't allocate using gst_allocator_alloc().  Use gst_allocator_nvmm_alloc() instead");

  return NULL;
}

static void
_gst_allocator_nvmm_free (GstAllocator * alloc, GstMemory * mem)
{
  GstMemoryNVMM *nvmm = (GstMemoryNVMM *) mem;

  if (nvmm->dmabuf_fd > 0)
    NvReleaseFd (nvmm->dmabuf_fd);
  nvmm->dmabuf_fd = -1;

  g_free (nvmm);
}

static void
gst_allocator_nvmm_class_init (GstAllocatorNVMMClass * klass)
{
  GstAllocatorClass *alloc_class = (GstAllocatorClass *) klass;

  alloc_class->alloc = _gst_allocator_nvmm_alloc;
  alloc_class->free = _gst_allocator_nvmm_free;
}

static void
gst_allocator_nvmm_init (GstAllocatorNVMM * nvmm)
{
  GstAllocator *alloc = (GstAllocator *) nvmm;

  alloc->mem_map_full = gst_memory_nvmm_map_full;
  alloc->mem_unmap_full = gst_memory_nvmm_unmap_full;
  alloc->mem_copy = gst_memory_nvmm_copy;
  alloc->mem_share = gst_memory_nvmm_share;
  alloc->mem_is_span = gst_memory_nvmm_is_span;
}

GType gst_nvmm_parent_meta_api_get_type (void);
#define GST_NVMM_PARENT_META_API_TYPE (gst_nvmm_parent_meta_api_get_type())

#define gst_buffer_get_nvmm_parent_meta(b) \
  ((GstNVMMParentMeta*)gst_buffer_get_meta((b),GST_NVMM_PARENT_META_API_TYPE))

const GstMetaInfo *gst_nvmm_parent_meta_get_info (void);
#define GST_NVMM_PARENT_META_INFO (gst_nvmm_parent_meta_get_info())

/* GstParentBufferMeta but supporting NULL and no copying to avoid accidentally
 * introducing a circular reference when copying GstMeta's */
struct _GstNVMMParentMeta
{
  GstMeta parent;

  GstBuffer *buffer;
};
typedef struct _GstNVMMParentMeta GstNVMMParentMeta;

static GstNVMMParentMeta *
gst_buffer_add_nvmm_parent_meta (GstBuffer * buffer, GstBuffer * ref)
{
  GstNVMMParentMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (ref), NULL);

  meta =
      (GstNVMMParentMeta *) gst_buffer_add_meta (buffer,
      GST_NVMM_PARENT_META_INFO, NULL);

  if (!meta)
    return NULL;

  if (ref)
    meta->buffer = gst_buffer_ref (ref);

  return meta;
}

static gboolean
_gst_nvmm_parent_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  return FALSE;
}

static void
_gst_nvmm_parent_meta_free (GstNVMMParentMeta * parent_meta, GstBuffer * buffer)
{
  GST_DEBUG ("Dropping reference on buffer %p", parent_meta->buffer);
  gst_clear_buffer (&parent_meta->buffer);
}

static gboolean
_gst_nvmm_parent_meta_init (GstNVMMParentMeta * parent_meta,
    gpointer params, GstBuffer * buffer)
{
  parent_meta->buffer = NULL;

  return TRUE;
}

GType
gst_nvmm_parent_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstNVMMParentMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }

  return type;
}

const GstMetaInfo *
gst_nvmm_parent_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (gst_nvmm_parent_meta_api_get_type (),
        "GstNVMMParentMeta",
        sizeof (GstNVMMParentMeta),
        (GstMetaInitFunction) _gst_nvmm_parent_meta_init,
        (GstMetaFreeFunction) _gst_nvmm_parent_meta_free,
        _gst_nvmm_parent_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) meta);
  }

  return meta_info;
}

static GstMiniObjectDisposeFunction parent_gst_buffer_dispose = NULL;

static gboolean
gst_buffer_nvmm_dispose (GstMiniObject * obj)
{
  GstBuffer *buf = (GstBuffer *) obj;
  GstNVMMParentMeta *nv_buf_meta = gst_buffer_get_nvmm_parent_meta (buf);

  GST_TRACE ("nvmm buffer dispose %p, parent_buf_meta %p", obj, nv_buf_meta);
  if (nv_buf_meta && nv_buf_meta->buffer) {
    GstNVMMParentMeta *gl_buf_meta;

    gl_buf_meta = gst_buffer_get_nvmm_parent_meta (nv_buf_meta->buffer);
    if (gl_buf_meta && !gl_buf_meta->buffer) {
      // reattache the NVMM buffer to the parent buffer
      GST_LOG ("readding nvmm buffer %p %i, to glmemory buffer %p %i", buf,
          GST_MINI_OBJECT_REFCOUNT_VALUE (buf), nv_buf_meta->buffer,
          GST_MINI_OBJECT_REFCOUNT_VALUE (nv_buf_meta->buffer));
      gl_buf_meta->buffer = gst_buffer_ref (buf);
      gst_clear_buffer (&nv_buf_meta->buffer);
      return FALSE;
    }
  }

  return parent_gst_buffer_dispose (obj);
}

struct _GstGLBufferPoolNVMMPrivate
{
  GstGLVideoAllocationParams *gl_params;
};
typedef struct _GstGLBufferPoolNVMMPrivate GstGLBufferPoolNVMMPrivate;

struct _GstGLBufferPoolNVMM
{
  GstGLBufferPool parent;
};

#define NVMM_POOL_GET_PRIV(obj) gst_gl_buffer_pool_nvmm_get_instance_private((GstGLBufferPoolNVMM *)(obj));

G_DECLARE_FINAL_TYPE (GstGLBufferPoolNVMM, gst_gl_buffer_pool_nvmm, GST,
    GL_BUFFER_POOL_NVMM, GstGLBufferPool);
G_DEFINE_TYPE_WITH_CODE (GstGLBufferPoolNVMM, gst_gl_buffer_pool_nvmm,
    GST_TYPE_GL_BUFFER_POOL, G_ADD_PRIVATE (GstGLBufferPoolNVMM));

static gboolean
gst_gl_buffer_pool_nvmm_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstGLBufferPoolNVMMPrivate *priv;
  GstGLBufferPool *glpool = GST_GL_BUFFER_POOL (pool);
  GstGLVideoAllocationParams *parent_gl_params;
  GstCaps *caps = NULL;
  GstVideoInfo vinfo;
  GstAllocationParams alloc_params;

  priv = NVMM_POOL_GET_PRIV (pool);

  if (!gst_buffer_pool_config_get_allocator (config, NULL, &alloc_params))
    goto wrong_config;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&vinfo, caps))
    goto wrong_caps;

  // TODO: fallback to regular GLMemory PBO/GetTexImage downloads?
  if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_RGBA)
    goto wrong_vformat;

  if (!GST_BUFFER_POOL_CLASS (gst_gl_buffer_pool_nvmm_parent_class)->set_config
      (pool, config))
    return FALSE;

  parent_gl_params = (GstGLVideoAllocationParams *)
      gst_gl_buffer_pool_get_gl_allocation_params (glpool);

  if (priv->gl_params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) priv->gl_params);
  priv->gl_params =
      gst_gl_video_allocation_params_new_wrapped_gl_handle
      (parent_gl_params->parent.context, parent_gl_params->parent.alloc_params,
      parent_gl_params->v_info, 0, parent_gl_params->valign,
      parent_gl_params->target, parent_gl_params->tex_format, NULL, NULL, NULL);

  gst_buffer_pool_config_set_gl_allocation_params (config,
      (GstGLAllocationParams *) priv->gl_params);
  gst_gl_allocation_params_free ((GstGLAllocationParams *) parent_gl_params);

  if (!GST_BUFFER_POOL_CLASS (gst_gl_buffer_pool_nvmm_parent_class)->set_config
      (pool, config))
    return FALSE;

  return TRUE;

wrong_config:
  {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }
no_caps:
  {
    GST_WARNING_OBJECT (pool, "no caps in config");
    return FALSE;
  }
wrong_caps:
  {
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
wrong_vformat:
  {
    GST_WARNING_OBJECT (pool, "This pool only deals with RGBA textures");
    return FALSE;
  }
}

static void
nv_buffer_egl_image_mem_unref (GstEGLImage * image, GstMemory * mem)
{
  GstGLDisplayEGL *egl_display = NULL;
  EGLDisplay display;

  egl_display = gst_gl_display_egl_from_gl_display (image->context->display);
  if (!egl_display) {
    GST_ERROR ("Could not retrieve GstGLDisplayEGL from GstGLDisplay");
    return;
  }
  display =
      (EGLDisplay) gst_gl_display_get_handle (GST_GL_DISPLAY (egl_display));

  if (NvDestroyEGLImage (display, image->image)) {
    GST_ERROR ("Failed to destroy EGLImage %p from NvBuffer", image->image);
  } else {
    GST_DEBUG ("destroyed EGLImage %p from NvBuffer", image->image);
  }

  gst_memory_unref (mem);
  gst_object_unref (egl_display);
}

static GstFlowReturn
gst_gl_buffer_pool_nvmm_alloc (GstBufferPool * pool, GstBuffer ** outbuf,
    GstBufferPoolAcquireParams * acquire_params)
{
  GstGLBufferPool *gl_pool = GST_GL_BUFFER_POOL (pool);
  GstGLBufferPoolNVMMPrivate *priv;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *downstream_buf = NULL;
  GstMapInfo in_map_info = GST_MAP_INFO_INIT;
  GstGLDisplayEGL *egl_display = NULL;
  GstEGLImage *eglimage = NULL;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLImageKHR image = EGL_NO_IMAGE;
  GstGLMemoryAllocator *allocator = NULL;
  GstMemory *nvmm_mem = NULL;
  int in_dmabuf_fd;

  priv = NVMM_POOL_GET_PRIV (pool);

  *outbuf = NULL;
  downstream_buf = gst_buffer_new ();
  if (!parent_gst_buffer_dispose)
    parent_gst_buffer_dispose = ((GstMiniObject *) downstream_buf)->dispose;
  ((GstMiniObject *) downstream_buf)->dispose = gst_buffer_nvmm_dispose;

  nvmm_mem = gst_allocator_nvmm_alloc (priv->gl_params->v_info);
  if (!nvmm_mem) {
    GST_WARNING_OBJECT (pool, "Failed to create NVMM GstMemory");
    return GST_FLOW_ERROR;
  }
  gst_buffer_append_memory (downstream_buf, nvmm_mem);
  in_dmabuf_fd = ((GstMemoryNVMM *) nvmm_mem)->dmabuf_fd;

  egl_display = gst_gl_display_egl_from_gl_display (gl_pool->context->display);
  if (!egl_display) {
    GST_WARNING ("Failed to retrieve GstGLDisplayEGL from GstGLDisplay");
    goto done;
  }
  display =
      (EGLDisplay) gst_gl_display_get_handle (GST_GL_DISPLAY (egl_display));

  image = NvEGLImageFromFd (display, in_dmabuf_fd);
  if (!image) {
    GST_DEBUG_OBJECT (pool, "Failed construct EGLImage "
        "from NvBuffer fd %i", in_dmabuf_fd);
    goto done;
  }
  GST_DEBUG_OBJECT (pool, "constructed EGLImage %p "
      "from NvBuffer fd %i", image, in_dmabuf_fd);

  eglimage = gst_egl_image_new_wrapped (gl_pool->context, image,
      GST_GL_RGBA, gst_memory_ref (nvmm_mem),
      (GstEGLImageDestroyNotify) nv_buffer_egl_image_mem_unref);
  if (!eglimage) {
    GST_WARNING_OBJECT (pool, "Failed to wrap constructed "
        "EGLImage from NvBuffer");
    goto done;
  }

  gst_buffer_unmap (downstream_buf, &in_map_info);
  in_map_info = (GstMapInfo) GST_MAP_INFO_INIT;

  allocator =
      GST_GL_MEMORY_ALLOCATOR (gst_allocator_find
      (GST_GL_MEMORY_EGL_ALLOCATOR_NAME));

  /* TODO: buffer pool */
  *outbuf = gst_buffer_new ();
  if (!gst_gl_memory_setup_buffer (allocator, *outbuf, priv->gl_params,
          NULL, (gpointer *) & eglimage, 1)) {
    GST_WARNING_OBJECT (pool, "Failed to setup NVMM -> EGLImage buffer");
    goto done;
  }

  gst_egl_image_unref (eglimage);

  /* TODO: NvBuffer has some sync functions that may be more useful here */
  {
    GstGLSyncMeta *sync_meta;

    sync_meta = gst_buffer_add_gl_sync_meta (gl_pool->context, *outbuf);
    if (sync_meta) {
      gst_gl_sync_meta_set_sync_point (sync_meta, gl_pool->context);
    }
  }

  // possible circular reference here
  gst_buffer_add_nvmm_parent_meta (*outbuf, downstream_buf);
  gst_buffer_unref (downstream_buf);

  ret = GST_FLOW_OK;

done:
  if (in_map_info.memory)
    gst_buffer_unmap (downstream_buf, &in_map_info);

  gst_clear_object (&egl_display);
  gst_clear_object (&allocator);

  return ret;
}

static void
gst_gl_buffer_pool_nvmm_finalize (GObject * object)
{
  GstGLBufferPoolNVMMPrivate *priv = NVMM_POOL_GET_PRIV (object);

  if (priv->gl_params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) priv->gl_params);
  priv->gl_params = NULL;

  G_OBJECT_CLASS (gst_gl_buffer_pool_nvmm_parent_class)->finalize (object);
}

static void
gst_gl_buffer_pool_nvmm_init (GstGLBufferPoolNVMM * pool)
{
}

static void
gst_gl_buffer_pool_nvmm_class_init (GstGLBufferPoolNVMMClass * klass)
{
  GstBufferPoolClass *pool_class = (GstBufferPoolClass *) klass;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  pool_class->set_config = gst_gl_buffer_pool_nvmm_set_config;
  pool_class->alloc_buffer = gst_gl_buffer_pool_nvmm_alloc;

  gobject_class->finalize = gst_gl_buffer_pool_nvmm_finalize;
}

static GstBufferPool *
gst_gl_buffer_pool_nvmm_new (GstGLContext * context)
{
  GstGLBufferPoolNVMM *pool;
  GstGLBufferPool *gl_pool;

  pool = g_object_new (gst_gl_buffer_pool_nvmm_get_type (), NULL);
  gst_object_ref_sink (pool);
  gl_pool = GST_GL_BUFFER_POOL (pool);
  gl_pool->context = gst_object_ref (context);

  GST_LOG_OBJECT (pool, "new NVMM GL buffer pool for context %" GST_PTR_FORMAT,
      context);

  return GST_BUFFER_POOL_CAST (pool);
}
#endif /* GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM) */

#define gst_gl_download_element_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLDownloadElement, gst_gl_download_element,
    GST_TYPE_GL_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_gl_download_element_debug, "gldownloadelement",
        0, "download element"););
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gldownload, "gldownload",
    GST_RANK_NONE, GST_TYPE_GL_DOWNLOAD_ELEMENT, gl_element_init (plugin));

static gboolean gst_gl_download_element_start (GstBaseTransform * bt);
static gboolean gst_gl_download_element_stop (GstBaseTransform * bt);
static gboolean gst_gl_download_element_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static GstCaps *gst_gl_download_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_gl_download_element_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_gl_download_element_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);
static GstFlowReturn
gst_gl_download_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf);
static GstFlowReturn gst_gl_download_element_transform (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer * outbuf);
static gboolean gst_gl_download_element_transform_meta (GstBaseTransform * bt,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static gboolean gst_gl_download_element_decide_allocation (GstBaseTransform *
    trans, GstQuery * query);
static gboolean gst_gl_download_element_sink_event (GstBaseTransform * bt,
    GstEvent * event);
static gboolean gst_gl_download_element_src_event (GstBaseTransform * bt,
    GstEvent * event);
static gboolean gst_gl_download_element_propose_allocation (GstBaseTransform *
    bt, GstQuery * decide_query, GstQuery * query);
static void gst_gl_download_element_finalize (GObject * object);

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
#define EXTRA_CAPS_TEMPLATE1 "video/x-raw(" GST_CAPS_FEATURE_MEMORY_NVMM "), format=(string)RGBA; "
#else
#define EXTRA_CAPS_TEMPLATE1
#endif

#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
#define EXTRA_CAPS_TEMPLATE2 "video/x-raw(" GST_CAPS_FEATURE_MEMORY_DMABUF "); "
#else
#define EXTRA_CAPS_TEMPLATE2
#endif

static GstStaticPadTemplate gst_gl_download_element_src_pad_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (EXTRA_CAPS_TEMPLATE1 EXTRA_CAPS_TEMPLATE2
        "video/x-raw; video/x-raw(memory:GLMemory)"));

static GstStaticPadTemplate gst_gl_download_element_sink_pad_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(memory:GLMemory); video/x-raw"));

static void
gst_gl_download_element_class_init (GstGLDownloadElementClass * klass)
{
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  bt_class->start = gst_gl_download_element_start;
  bt_class->stop = gst_gl_download_element_stop;
  bt_class->transform_caps = gst_gl_download_element_transform_caps;
  bt_class->fixate_caps = gst_gl_download_element_fixate_caps;
  bt_class->set_caps = gst_gl_download_element_set_caps;
  bt_class->get_unit_size = gst_gl_download_element_get_unit_size;
  bt_class->prepare_output_buffer =
      gst_gl_download_element_prepare_output_buffer;
  bt_class->transform = gst_gl_download_element_transform;
  bt_class->decide_allocation = gst_gl_download_element_decide_allocation;
  bt_class->sink_event = gst_gl_download_element_sink_event;
  bt_class->src_event = gst_gl_download_element_src_event;
  bt_class->propose_allocation = gst_gl_download_element_propose_allocation;
  bt_class->transform_meta = gst_gl_download_element_transform_meta;

  bt_class->passthrough_on_same_caps = TRUE;

  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_download_element_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_download_element_sink_pad_template);

  gst_element_class_set_metadata (element_class,
      "OpenGL downloader", "Filter/Video",
      "Downloads data from OpenGL", "Matthew Waters <matthew@centricular.com>");

  object_class->finalize = gst_gl_download_element_finalize;
}

static void
gst_gl_download_element_init (GstGLDownloadElement * download)
{
  gst_base_transform_set_prefer_passthrough (GST_BASE_TRANSFORM (download),
      TRUE);
}

static gboolean
gst_gl_download_element_start (GstBaseTransform * bt)
{
#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);

  dl->dmabuf_allocator = gst_dmabuf_allocator_new ();
  g_atomic_int_set (&dl->try_dmabuf_exports, TRUE);
#endif

  return TRUE;
}

static gboolean
gst_gl_download_element_stop (GstBaseTransform * bt)
{
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);

  if (dl->dmabuf_allocator) {
    gst_object_unref (GST_OBJECT (dl->dmabuf_allocator));
    dl->dmabuf_allocator = NULL;
  }

  return TRUE;
}

static gboolean
gst_gl_download_element_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);
  GstVideoInfo out_info;
  GstCapsFeatures *features = NULL;

  if (!gst_video_info_from_caps (&out_info, out_caps))
    return FALSE;

  features = gst_caps_get_features (out_caps, 0);

  if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
    dl->mode = GST_GL_DOWNLOAD_MODE_PASSTHROUGH;
    GST_INFO_OBJECT (dl, "caps signal passthrough");
#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
  } else if (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_NVMM)) {
    dl->mode = GST_GL_DOWNLOAD_MODE_NVMM;
    GST_INFO_OBJECT (dl, "caps signal NVMM");
#endif
#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
  } else if (g_atomic_int_get (&dl->try_dmabuf_exports) &&
      gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    dl->mode = GST_GL_DOWNLOAD_MODE_DMABUF_EXPORTS;
    GST_INFO_OBJECT (dl, "caps signal dma-buf export");
#endif
  } else {
    /* System Memory */
    dl->mode = GST_GL_DOWNLOAD_MODE_PBO_TRANSFERS;
    GST_INFO_OBJECT (dl, "caps signal sysmem download");
  }

  return TRUE;
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
gst_gl_download_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  if (direction == GST_PAD_SRC) {
    GstCaps *sys_caps = gst_caps_simplify (_set_caps_features (caps,
            GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY));

    tmp = _set_caps_features (sys_caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    tmp = gst_caps_merge (tmp, sys_caps);
  } else {
    GstCaps *newcaps;
    tmp = gst_caps_ref (caps);

#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_NVMM);
    _remove_field (newcaps, "texture-target");
    // FIXME: RGBA-only?
    tmp = gst_caps_merge (tmp, newcaps);
#endif

#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_DMABUF);
    _remove_field (newcaps, "texture-target");
    tmp = gst_caps_merge (tmp, newcaps);
#endif

    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    _remove_field (newcaps, "texture-target");
    tmp = gst_caps_merge (tmp, newcaps);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (bt, "returning caps %" GST_PTR_FORMAT, result);

  return result;
}

static GstCaps *
gst_gl_download_element_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);

  /* Remove DMABuf features if try_dmabuf_exports is not set */
  if (direction == GST_PAD_SINK && !g_atomic_int_get (&dl->try_dmabuf_exports)) {
    gint i;

    for (i = 0; i < gst_caps_get_size (othercaps); i++) {
      GstCapsFeatures *features = gst_caps_get_features (othercaps, i);

      if (features && gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_DMABUF)) {
        caps = gst_caps_make_writable (othercaps);
        gst_caps_remove_structure (othercaps, i--);
      }
    }
  }
#endif

  return GST_BASE_TRANSFORM_CLASS (parent_class)->fixate_caps (bt, direction,
      caps, othercaps);
}

static gboolean
gst_gl_download_element_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF

struct DmabufInfo
{
  GstMemory *dmabuf;
  gint stride;
  gsize offset;
};

static void
_free_dmabuf_info (struct DmabufInfo *info)
{
  gst_memory_unref (info->dmabuf);
  g_free (info);
}

static GQuark
_dmabuf_info_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("GstGLDownloadDmabufInfo");
  return quark;
}

static struct DmabufInfo *
_get_cached_dmabuf_info (GstGLMemory * mem)
{
  return gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      _dmabuf_info_quark ());
}

static void
_set_cached_dmabuf_info (GstGLMemory * mem, struct DmabufInfo *info)
{
  return gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
      _dmabuf_info_quark (), info, (GDestroyNotify) _free_dmabuf_info);
}

struct DmabufTransfer
{
  GstGLDownloadElement *download;
  GstGLMemory *glmem;
  struct DmabufInfo *info;
};

static void
_create_cached_dmabuf_info (GstGLContext * context, gpointer data)
{
  struct DmabufTransfer *transfer = (struct DmabufTransfer *) data;
  GstEGLImage *image;

  image = gst_egl_image_from_texture (context, transfer->glmem, NULL);
  if (image) {
    int fd;
    gint stride;
    gsize offset;

    if (gst_egl_image_export_dmabuf (image, &fd, &stride, &offset)) {
      GstGLDownloadElement *download = transfer->download;
      struct DmabufInfo *info;
      gsize size;

      size =
          gst_gl_memory_get_texture_height (transfer->glmem) * stride + offset;

      info = g_new0 (struct DmabufInfo, 1);
      info->dmabuf =
          gst_dmabuf_allocator_alloc (download->dmabuf_allocator, fd, size);
      info->stride = stride;
      info->offset = offset;

      transfer->info = info;
    }

    gst_egl_image_unref (image);
  }
}

static GstBuffer *
_try_export_dmabuf (GstGLDownloadElement * download, GstBuffer * inbuf)
{
  GstGLMemory *glmem;
  GstBuffer *buffer = NULL;
  int i;
  gsize offset[GST_VIDEO_MAX_PLANES];
  gint stride[GST_VIDEO_MAX_PLANES];
  GstCaps *src_caps;
  GstVideoInfo out_info;
  gsize total_offset;
  GstVideoAlignment *alig = NULL;

  glmem = GST_GL_MEMORY_CAST (gst_buffer_peek_memory (inbuf, 0));
  if (glmem) {
    GstGLContext *context = GST_GL_BASE_MEMORY_CAST (glmem)->context;
    if (gst_gl_context_get_gl_platform (context) != GST_GL_PLATFORM_EGL)
      return NULL;
    alig = &glmem->valign;
  }

  buffer = gst_buffer_new ();
  total_offset = 0;

  for (i = 0; i < gst_buffer_n_memory (inbuf); i++) {
    struct DmabufInfo *info;

    glmem = GST_GL_MEMORY_CAST (gst_buffer_peek_memory (inbuf, i));
    info = _get_cached_dmabuf_info (glmem);
    if (!info) {
      GstGLContext *context = GST_GL_BASE_MEMORY_CAST (glmem)->context;
      struct DmabufTransfer transfer;

      transfer.download = download;
      transfer.glmem = glmem;
      transfer.info = NULL;
      gst_gl_context_thread_add (context, _create_cached_dmabuf_info,
          &transfer);
      info = transfer.info;

      if (info)
        _set_cached_dmabuf_info (glmem, info);
    }

    if (info) {
      offset[i] = total_offset + info->offset;
      stride[i] = info->stride;
      total_offset += gst_memory_get_sizes (info->dmabuf, NULL, NULL);
      gst_buffer_insert_memory (buffer, -1, gst_memory_ref (info->dmabuf));
    } else {
      gst_buffer_unref (buffer);
      buffer = NULL;
      goto export_complete;
    }
  }

  src_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM (download)->srcpad);
  gst_video_info_from_caps (&out_info, src_caps);
  gst_caps_unref (src_caps);

  if (download->add_videometa) {
    GstVideoMeta *meta;

    meta = gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
        out_info.finfo->format, out_info.width, out_info.height,
        out_info.finfo->n_planes, offset, stride);

    if (alig)
      gst_video_meta_set_alignment (meta, *alig);
  } else {
    int i;
    gboolean match = TRUE;
    for (i = 0; i < gst_buffer_n_memory (inbuf); i++) {
      if (offset[i] != out_info.offset[i] || stride[i] != out_info.stride[i]) {
        match = FALSE;
        break;
      }
    }

    if (!match) {
      gst_buffer_unref (buffer);
      buffer = NULL;
    }
  }

export_complete:

  return buffer;
}
#endif /* GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF */

static GstFlowReturn
gst_gl_download_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);
  GstBaseTransformClass *bclass = GST_BASE_TRANSFORM_GET_CLASS (bt);
  GstGLContext *context = GST_GL_BASE_FILTER (bt)->context;
  GstGLSyncMeta *in_sync_meta;
  gint i, n;

  *outbuf = inbuf;

  (void) bclass;

  in_sync_meta = gst_buffer_get_gl_sync_meta (inbuf);
  if (in_sync_meta) {
    if (context) {
      gst_gl_sync_meta_wait (in_sync_meta, context);
    } else if (dl->mode != GST_GL_DOWNLOAD_MODE_PASSTHROUGH) {
      GST_WARNING_OBJECT (dl, "No configured GL context in non-passthrough "
          "mode. Cannot wait on incoming `GstGLSyncMeta`");
    }
  }

#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
  if (dl->mode == GST_GL_DOWNLOAD_MODE_NVMM) {
    GstNVMMParentMeta *buf_meta = gst_buffer_get_nvmm_parent_meta (inbuf);
    GstMemory *mem;
    GstMemoryNVMM *nvmm_mem;

    if (!buf_meta || !buf_meta->buffer) {
      // TODO: remove this restriction with an e.g. copy...
      GST_ERROR_OBJECT (dl,
          "Cannot push upstream created buffer when outputting NVMM");
      return GST_FLOW_ERROR;
    }

    if (!(mem = gst_buffer_peek_memory (buf_meta->buffer, 0))) {
      GST_ERROR_OBJECT (dl, "No memory in buffer?");
      return GST_FLOW_ERROR;
    }

    if (!gst_is_memory_nvmm (mem)) {
      GST_ERROR_OBJECT (dl,
          "Upstream buffer does not contain an attached NVMM GstMemory");
      return GST_FLOW_ERROR;
    }
    nvmm_mem = (GstMemoryNVMM *) mem;

    /* switch up the parent buffer references so that when the NVMM buffer is
     * released, the associated EGLImage/OpenGL texture is as well
     */
    GST_DEBUG_OBJECT (dl, "NVMM buffer fd:%i passed through %" GST_PTR_FORMAT,
        nvmm_mem->dmabuf_fd, buf_meta->buffer);
    *outbuf = buf_meta->buffer;
    bclass->copy_metadata (bt, inbuf, *outbuf);
    buf_meta->buffer = NULL;
    buf_meta = gst_buffer_get_nvmm_parent_meta (*outbuf);
    if (!buf_meta) {
      buf_meta = gst_buffer_add_nvmm_parent_meta (*outbuf, inbuf);
    } else {
      gst_clear_buffer (&buf_meta->buffer);
      buf_meta->buffer = gst_buffer_ref (inbuf);
    }

    return GST_FLOW_OK;
  }
#endif
#if GST_GL_HAVE_PLATFORM_EGL && GST_GL_HAVE_DMABUF
  if (dl->mode == GST_GL_DOWNLOAD_MODE_DMABUF_EXPORTS) {
    GstBuffer *buffer = _try_export_dmabuf (dl, inbuf);

    if (buffer) {
      if (GST_BASE_TRANSFORM_GET_CLASS (bt)->copy_metadata) {
        if (!GST_BASE_TRANSFORM_GET_CLASS (bt)->copy_metadata (bt, inbuf,
                buffer)) {
          GST_ELEMENT_WARNING (GST_ELEMENT (bt), STREAM, NOT_IMPLEMENTED,
              ("could not copy metadata"), (NULL));
        }
      }

      *outbuf = buffer;
    } else {
      GstCaps *src_caps;
      GstCapsFeatures *features;
      gboolean ret;

      src_caps = gst_pad_get_current_caps (bt->srcpad);
      src_caps = gst_caps_make_writable (src_caps);
      features = gst_caps_get_features (src_caps, 0);
      gst_caps_features_remove (features, GST_CAPS_FEATURE_MEMORY_DMABUF);
      g_atomic_int_set (&dl->try_dmabuf_exports, FALSE);
      dl->mode = GST_GL_DOWNLOAD_MODE_PBO_TRANSFERS;

      ret = gst_base_transform_update_src_caps (bt, src_caps);
      gst_caps_unref (src_caps);

      if (!ret) {
        GST_ERROR_OBJECT (bt, "DMABuf exportation didn't work and system "
            "memory is not supported.");
        return GST_FLOW_NOT_NEGOTIATED;
      }
    }
  }
#endif

  if (dl->mode == GST_GL_DOWNLOAD_MODE_PBO_TRANSFERS) {
    n = gst_buffer_n_memory (*outbuf);
    for (i = 0; i < n; i++) {
      GstMemory *mem = gst_buffer_peek_memory (*outbuf, i);

      if (gst_is_gl_memory_pbo (mem))
        gst_gl_memory_pbo_download_transfer ((GstGLMemoryPBO *) mem);
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_gl_download_element_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}

static gboolean
gst_gl_download_element_transform_meta (GstBaseTransform * bt,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  if (g_type_is_a (meta->info->api, GST_GL_SYNC_META_API_TYPE)) {
    GST_LOG_OBJECT (bt, "not copying GstGLSyncMeta onto output buffer");
    return FALSE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (bt, outbuf,
      meta, inbuf);
}

static gboolean
gst_gl_download_element_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstGLDownloadElement *download = GST_GL_DOWNLOAD_ELEMENT_CAST (trans);

  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    download->add_videometa = TRUE;
  } else {
    download->add_videometa = FALSE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_gl_download_element_sink_event (GstBaseTransform * bt, GstEvent * event)
{
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);

  /* Retry exporting whenever we have new caps from upstream */
  if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS)
    g_atomic_int_set (&dl->try_dmabuf_exports, TRUE);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (bt, event);
}

static gboolean
gst_gl_download_element_src_event (GstBaseTransform * bt, GstEvent * event)
{
  GstGLDownloadElement *dl = GST_GL_DOWNLOAD_ELEMENT (bt);

  /* Retry exporting whenever downstream have changed */
  if (GST_EVENT_TYPE (event) == GST_EVENT_RECONFIGURE)
    g_atomic_int_set (&dl->try_dmabuf_exports, TRUE);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->src_event (bt, event);
}

static gboolean
gst_gl_download_element_propose_allocation (GstBaseTransform * bt,
    GstQuery * decide_query, GstQuery * query)
{
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  GstGLContext *context;
  GstStructure *config;
  GstVideoInfo info;
  gsize size;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (bt,
          decide_query, query))
    return FALSE;

  gst_query_parse_allocation (query, &caps, NULL);
  if (caps == NULL)
    goto invalid_caps;

  context = GST_GL_BASE_FILTER (bt)->context;
  if (!context) {
    GST_ERROR_OBJECT (context, "got no GLContext");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

#if GST_GL_HAVE_PLATFORM_EGL && defined(HAVE_NVMM)
  if (!pool && decide_query) {
    GstCaps *decide_caps;

    gst_query_parse_allocation (decide_query, &decide_caps, NULL);
    if (decide_caps && gst_caps_get_size (decide_caps) > 0) {
      GstCapsFeatures *features = gst_caps_get_features (decide_caps, 0);

      if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_NVMM)) {
        pool = gst_gl_buffer_pool_nvmm_new (context);
        GST_INFO_OBJECT (bt, "have NVMM downstream, proposing NVMM "
            "pool %" GST_PTR_FORMAT, pool);
      }
    }
  }
#endif
  if (!pool) {
    pool = gst_gl_buffer_pool_new (context);
  }
  config = gst_buffer_pool_get_config (pool);

  /* the normal size of a frame */
  size = info.size;
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
  gst_buffer_pool_config_set_gl_min_free_queue_size (config, 1);

  if (!gst_buffer_pool_set_config (pool, config)) {
    gst_object_unref (pool);
    goto config_failed;
  }

  if (context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL);

  gst_query_add_allocation_pool (query, pool, size, 1, 0);

  gst_object_unref (pool);
  return TRUE;

invalid_caps:
  {
    GST_ERROR_OBJECT (bt, "Invalid Caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_ERROR_OBJECT (bt, "failed setting config");
    return FALSE;
  }
}


static void
gst_gl_download_element_finalize (GObject * object)
{
  GstGLDownloadElement *download = GST_GL_DOWNLOAD_ELEMENT_CAST (object);

  if (download->dmabuf_allocator) {
    gst_object_unref (GST_OBJECT (download->dmabuf_allocator));
    download->dmabuf_allocator = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}
