/* GStreamer
 * Copyright (C) <2018-2019> Seungha Yang <seungha.yang@navercorp.com>
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

#include "gstcudautils.h"
#include "gstcudacontext.h"
#include "gstcuda-private.h"
#include <atomic>
#include <set>
#include <string>
#include <mutex>

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#endif

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#include <sddl.h>
#endif

#ifdef HAVE_NVCODEC_NVMM
#include "gstcudanvmm.h"
#endif

#include "gstcudamemory.h"

GST_DEBUG_CATEGORY_STATIC (gst_cuda_utils_debug);
#define GST_CAT_DEFAULT gst_cuda_utils_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

static void
_init_debug (void)
{
  GST_CUDA_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_INIT (gst_cuda_utils_debug, "cudautils", 0,
        "CUDA utils");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
  } GST_CUDA_CALL_ONCE_END;
}

static gboolean
pad_query (const GValue * item, GValue * value, gpointer user_data)
{
  GstPad *pad = (GstPad *) g_value_get_object (item);
  GstQuery *query = (GstQuery *) user_data;
  gboolean res;

  res = gst_pad_peer_query (pad, query);

  if (res) {
    g_value_set_boolean (value, TRUE);
    return FALSE;
  }

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, pad, "pad peer query failed");
  return TRUE;
}

static gboolean
run_query (GstElement * element, GstQuery * query, GstPadDirection direction)
{
  GstIterator *it;
  GstIteratorFoldFunction func = pad_query;
  GValue res = { 0 };

  g_value_init (&res, G_TYPE_BOOLEAN);
  g_value_set_boolean (&res, FALSE);

  /* Ask neighbor */
  if (direction == GST_PAD_SRC)
    it = gst_element_iterate_src_pads (element);
  else
    it = gst_element_iterate_sink_pads (element);

  while (gst_iterator_fold (it, func, &res, query) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);

  gst_iterator_free (it);

  return g_value_get_boolean (&res);
}

static void
find_cuda_context (GstElement * element, GstCudaContext ** cuda_ctx)
{
  GstQuery *query;
  GstContext *ctxt;

  /*  1) Query downstream with GST_QUERY_CONTEXT for the context and
   *      check if upstream already has a context of the specific type
   *  2) Query upstream as above.
   */
  query = gst_query_new_context (GST_CUDA_CONTEXT_TYPE);
  if (run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%p) in downstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  /* although we found cuda context above, the element does not want
   * to use the context. Then try to find from the other direction */
  if (*cuda_ctx == nullptr && run_query (element, query, GST_PAD_SINK)) {
    gst_query_parse_context (query, &ctxt);
    if (ctxt) {
      GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
          "found context (%p) in upstream query", ctxt);
      gst_element_set_context (element, ctxt);
    }
  }

  if (*cuda_ctx == nullptr) {
    /* 3) Post a GST_MESSAGE_NEED_CONTEXT message on the bus with
     *    the required context type and afterwards check if a
     *    usable context was set now. The message could
     *    be handled by the parent bins of the element and the
     *    application.
     */
    GstMessage *msg;

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting need context message");
    msg = gst_message_new_need_context (GST_OBJECT_CAST (element),
        GST_CUDA_CONTEXT_TYPE);
    gst_element_post_message (element, msg);
  }

  /*
   * Whomever responds to the need-context message performs a
   * GstElement::set_context() with the required context in which the element
   * is required to update the cuda_ctx or call gst_cuda_handle_set_context().
   */

  gst_query_unref (query);
}

static void
context_set_cuda_context (GstContext * context, GstCudaContext * cuda_ctx)
{
  GstStructure *s;
  guint device_id;

  g_return_if_fail (context != nullptr);

  g_object_get (G_OBJECT (cuda_ctx), "cuda-device-id", &device_id, nullptr);

  GST_CAT_LOG (GST_CAT_CONTEXT,
      "setting GstCudaContext(%" GST_PTR_FORMAT
      ") with cuda-device-id %d on context(%" GST_PTR_FORMAT ")",
      cuda_ctx, device_id, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_CUDA_CONTEXT_TYPE, GST_TYPE_CUDA_CONTEXT,
      cuda_ctx, "cuda-device-id", G_TYPE_UINT, device_id, nullptr);
}

/**
 * gst_cuda_ensure_element_context:
 * @element: the #GstElement running the query
 * @device_id: preferred device-id, pass device_id >=0 when
 *             the device_id explicitly required. Otherwise, set -1.
 * @cuda_ctx: (inout): the resulting #GstCudaContext
 *
 * Perform the steps necessary for retrieving a #GstCudaContext from the
 * surrounding elements or from the application using the #GstContext mechanism.
 *
 * If the content of @cuda_ctx is not %NULL, then no #GstContext query is
 * necessary for #GstCudaContext.
 *
 * Returns: whether a #GstCudaContext exists in @cuda_ctx
 *
 * Since: 1.22
 */
gboolean
gst_cuda_ensure_element_context (GstElement * element, gint device_id,
    GstCudaContext ** cuda_ctx)
{
  guint target_device_id = 0;
  gboolean ret = TRUE;
  static std::recursive_mutex lock;

  g_return_val_if_fail (element != nullptr, FALSE);
  g_return_val_if_fail (cuda_ctx != nullptr, FALSE);

  _init_debug ();

  std::lock_guard < std::recursive_mutex > lk (lock);

  if (*cuda_ctx)
    return TRUE;

  find_cuda_context (element, cuda_ctx);
  if (*cuda_ctx)
    return TRUE;

  if (device_id > 0)
    target_device_id = device_id;

  /* No available CUDA context in pipeline, create new one here */
  *cuda_ctx = gst_cuda_context_new (target_device_id);

  if (*cuda_ctx == nullptr) {
    GST_CAT_ERROR_OBJECT (GST_CAT_CONTEXT, element,
        "Failed to create CUDA context with device-id %d", device_id);
    ret = FALSE;
  } else {
    GstContext *context;
    GstMessage *msg;

    /* Propagate new CUDA context */

    context = gst_context_new (GST_CUDA_CONTEXT_TYPE, TRUE);
    context_set_cuda_context (context, *cuda_ctx);

    gst_element_set_context (element, context);

    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "posting have context (%p) message with CUDA context (%p)",
        context, *cuda_ctx);
    msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
    gst_element_post_message (GST_ELEMENT_CAST (element), msg);
  }

  return ret;
}

/**
 * gst_cuda_handle_set_context:
 * @element: a #GstElement
 * @context: a #GstContext
 * @device_id: preferred device-id, pass device_id >=0 when
 *             the device_id explicitly required. Otherwise, set -1.
 * @cuda_ctx: (inout) (transfer full): location of a #GstCudaContext
 *
 * Helper function for implementing #GstElementClass.set_context() in
 * CUDA capable elements.
 *
 * Retrieves the #GstCudaContext in @context and places the result in @cuda_ctx.
 *
 * Returns: whether the @cuda_ctx could be set successfully
 *
 * Since: 1.22
 */
gboolean
gst_cuda_handle_set_context (GstElement * element,
    GstContext * context, gint device_id, GstCudaContext ** cuda_ctx)
{
  const gchar *context_type;

  g_return_val_if_fail (element != nullptr, FALSE);
  g_return_val_if_fail (cuda_ctx != nullptr, FALSE);

  _init_debug ();

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);
  if (g_strcmp0 (context_type, GST_CUDA_CONTEXT_TYPE) == 0) {
    const GstStructure *str;
    GstCudaContext *other_ctx = nullptr;
    guint other_device_id = 0;

    /* If we had context already, will not replace it */
    if (*cuda_ctx)
      return TRUE;

    str = gst_context_get_structure (context);
    if (gst_structure_get (str, GST_CUDA_CONTEXT_TYPE, GST_TYPE_CUDA_CONTEXT,
            &other_ctx, nullptr)) {
      g_object_get (other_ctx, "cuda-device-id", &other_device_id, nullptr);

      if (device_id == -1 || other_device_id == (guint) device_id) {
        GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT, element, "Found CUDA context");
        *cuda_ctx = other_ctx;

        return TRUE;
      }

      gst_object_unref (other_ctx);
    }
  }

  return FALSE;
}

/**
 * gst_cuda_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type %GST_QUERY_CONTEXT
 * @cuda_ctx: (transfer none) (nullable): a #GstCudaContext
 *
 * Returns: Whether the @query was successfully responded to from the passed
 *          @context.
 *
 * Since: 1.22
 */
gboolean
gst_cuda_handle_context_query (GstElement * element,
    GstQuery * query, GstCudaContext * cuda_ctx)
{
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (GST_IS_QUERY (query), FALSE);
  g_return_val_if_fail (cuda_ctx == nullptr
      || GST_IS_CUDA_CONTEXT (cuda_ctx), FALSE);

  _init_debug ();

  GST_CAT_LOG_OBJECT (GST_CAT_CONTEXT, element,
      "handle context query %" GST_PTR_FORMAT, query);
  gst_query_parse_context_type (query, &context_type);

  if (cuda_ctx && g_strcmp0 (context_type, GST_CUDA_CONTEXT_TYPE) == 0) {
    gst_query_parse_context (query, &old_context);

    if (old_context)
      context = gst_context_copy (old_context);
    else
      context = gst_context_new (GST_CUDA_CONTEXT_TYPE, TRUE);

    context_set_cuda_context (context, cuda_ctx);
    gst_query_set_context (query, context);
    gst_context_unref (context);
    GST_CAT_DEBUG_OBJECT (GST_CAT_CONTEXT, element,
        "successfully set %" GST_PTR_FORMAT " on %" GST_PTR_FORMAT, cuda_ctx,
        query);

    return TRUE;
  }

  return FALSE;
}

/**
 * gst_context_new_cuda_context:
 * @cuda_ctx: (transfer none): a #GstCudaContext
 *
 * Returns: (transfer full): a new #GstContext embedding the @cuda_ctx
 *
 * Since: 1.22
 */
GstContext *
gst_context_new_cuda_context (GstCudaContext * cuda_ctx)
{
  GstContext *context;

  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (cuda_ctx), nullptr);

  _init_debug ();

  context = gst_context_new (GST_CUDA_CONTEXT_TYPE, TRUE);
  context_set_cuda_context (context, cuda_ctx);

  return context;
}

static const gchar *gst_cuda_quark_strings[] =
    { "GstCudaQuarkGraphicsResource" };

static GQuark gst_cuda_quark_table[GST_CUDA_QUARK_MAX];

static void
init_cuda_quark_once (void)
{
  GST_CUDA_CALL_ONCE_BEGIN {
    for (guint i = 0; i < GST_CUDA_QUARK_MAX; i++) {
      gst_cuda_quark_table[i] =
          g_quark_from_static_string (gst_cuda_quark_strings[i]);
    }
  }
  GST_CUDA_CALL_ONCE_END;
}

/**
 * gst_cuda_quark_from_id: (skip)
 * @id: a #GstCudaQuarkId
 *
 * Returns: the GQuark for given @id or 0 if @id is unknown value
 *
 * Since: 1.22
 */
GQuark
gst_cuda_quark_from_id (GstCudaQuarkId id)
{
  g_return_val_if_fail (id < GST_CUDA_QUARK_MAX, 0);

  init_cuda_quark_once ();
  _init_debug ();

  return gst_cuda_quark_table[id];
}

/**
 * gst_cuda_graphics_resource_new: (skip)
 * @context: (transfer none): a #GstCudaContext
 * @graphics_context: (transfer none) (nullable): a graphics API specific context object
 * @type: a #GstCudaGraphicsResourceType of resource registration
 *
 * Create new #GstCudaGraphicsResource with given @context and @type
 *
 * Returns: a new #GstCudaGraphicsResource.
 * Free with gst_cuda_graphics_resource_free
 *
 * Since: 1.22
 */
GstCudaGraphicsResource *
gst_cuda_graphics_resource_new (GstCudaContext *
    context, GstObject * graphics_context, GstCudaGraphicsResourceType type)
{
  GstCudaGraphicsResource *resource;

  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  _init_debug ();

  resource = g_new0 (GstCudaGraphicsResource, 1);
  resource->cuda_context = (GstCudaContext *) gst_object_ref (context);
  if (graphics_context) {
    resource->graphics_context =
        (GstObject *) gst_object_ref (graphics_context);
  }

  return resource;
}

/**
 * gst_cuda_graphics_resource_register_gl_buffer: (skip)
 * @resource a #GstCudaGraphicsResource
 * @buffer: a GL buffer object
 * @flags: a `CUgraphicsRegisterFlags`
 *
 * Register the @buffer for access by CUDA.
 * Must be called from the gl context thread with current cuda context was
 * pushed on the current thread
 *
 * Returns: whether @buffer was registered or not
 *
 * Since: 1.22
 */
gboolean
gst_cuda_graphics_resource_register_gl_buffer (GstCudaGraphicsResource *
    resource, guint buffer, CUgraphicsRegisterFlags flags)
{
  CUresult cuda_ret;

  g_return_val_if_fail (resource != nullptr, FALSE);
  g_return_val_if_fail (resource->registered == FALSE, FALSE);

  _init_debug ();

  cuda_ret = CuGraphicsGLRegisterBuffer (&resource->resource, buffer, flags);

  if (!gst_cuda_result (cuda_ret))
    return FALSE;

  resource->registered = TRUE;
  resource->type = GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER;
  resource->flags = flags;

  return TRUE;
}

#ifdef G_OS_WIN32
/**
 * gst_cuda_graphics_resource_register_d3d11_resource: (skip)
 * @resource a #GstCudaGraphicsResource
 * @d3d11_resource: a ID3D11Resource
 * @flags: a CUgraphicsRegisterFlags
 *
 * Register the @d3d11_resource for accessing by CUDA.
 * Must be called with d3d11 device lock with current cuda context was
 * pushed on the current thread
 *
 * Returns: whether @d3d11_resource was registered or not
 *
 * Since: 1.22
 */
gboolean
gst_cuda_graphics_resource_register_d3d11_resource (GstCudaGraphicsResource *
    resource, ID3D11Resource * d3d11_resource, CUgraphicsRegisterFlags flags)
{
  CUresult cuda_ret;

  g_return_val_if_fail (resource != nullptr, FALSE);
  g_return_val_if_fail (resource->registered == FALSE, FALSE);

  _init_debug ();

  cuda_ret = CuGraphicsD3D11RegisterResource (&resource->resource,
      d3d11_resource, flags);

  if (!gst_cuda_result (cuda_ret))
    return FALSE;

  resource->registered = TRUE;
  resource->type = GST_CUDA_GRAPHICS_RESOURCE_D3D11_RESOURCE;
  resource->flags = flags;

  return TRUE;
}
#endif

/**
 * gst_cuda_graphics_resource_unregister: (skip)
 * @resource: a #GstCudaGraphicsResource
 *
 * Unregister previously registered resource.
 * For GL resource, this method must be called from gl context thread.
 * Also, current cuda context should be pushed on the current thread
 * before calling this method.
 *
 * Since: 1.22
 */
void
gst_cuda_graphics_resource_unregister (GstCudaGraphicsResource * resource)
{
  g_return_if_fail (resource != nullptr);

  _init_debug ();

  if (!resource->registered)
    return;

  gst_cuda_result (CuGraphicsUnregisterResource (resource->resource));
  resource->resource = nullptr;
  resource->registered = FALSE;

  return;
}

/**
 * gst_cuda_graphics_resource_map: (skip)
 * @resource: a #GstCudaGraphicsResource
 * @stream: a CUstream
 * @flags: a CUgraphicsMapResourceFlags
 *
 * Map previously registered resource with map flags
 *
 * Returns: (nullable): the `CUgraphicsResource` if successful or %NULL when failed
 *
 * Since: 1.22
 */
CUgraphicsResource
gst_cuda_graphics_resource_map (GstCudaGraphicsResource * resource,
    CUstream stream, CUgraphicsMapResourceFlags flags)
{
  CUresult cuda_ret;

  g_return_val_if_fail (resource != nullptr, nullptr);
  g_return_val_if_fail (resource->registered != FALSE, nullptr);

  _init_debug ();

  cuda_ret = CuGraphicsResourceSetMapFlags (resource->resource, flags);
  if (!gst_cuda_result (cuda_ret))
    return nullptr;

  cuda_ret = CuGraphicsMapResources (1, &resource->resource, stream);
  if (!gst_cuda_result (cuda_ret))
    return nullptr;

  resource->mapped = TRUE;

  return resource->resource;
}

/**
 * gst_cuda_graphics_resource_unmap: (skip)
 * @resource: a #GstCudaGraphicsResource
 * @stream: a `CUstream`
 *
 * Unmap previously mapped resource
 *
 * Since: 1.22
 */
void
gst_cuda_graphics_resource_unmap (GstCudaGraphicsResource * resource,
    CUstream stream)
{
  g_return_if_fail (resource != nullptr);
  g_return_if_fail (resource->registered != FALSE);

  _init_debug ();

  if (!resource->mapped)
    return;

  gst_cuda_result (CuGraphicsUnmapResources (1, &resource->resource, stream));

  resource->mapped = FALSE;
}

#ifdef HAVE_CUDA_GST_GL
static void
unregister_resource_from_gl_thread (GstGLContext * gl_context,
    GstCudaGraphicsResource * resource)
{
  GstCudaContext *cuda_context = resource->cuda_context;

  if (!gst_cuda_context_push (cuda_context)) {
    GST_WARNING_OBJECT (cuda_context, "failed to push CUDA context");
    return;
  }

  gst_cuda_graphics_resource_unregister (resource);

  if (!gst_cuda_context_pop (nullptr)) {
    GST_WARNING_OBJECT (cuda_context, "failed to pop CUDA context");
  }
}
#endif

#ifdef G_OS_WIN32
static void
unregister_d3d11_resource (GstCudaGraphicsResource * resource)
{
  GstCudaContext *cuda_context = resource->cuda_context;
  GstD3D11Device *device = GST_D3D11_DEVICE (resource->graphics_context);

  if (!gst_cuda_context_push (cuda_context)) {
    GST_WARNING_OBJECT (cuda_context, "failed to push CUDA context");
    return;
  }

  gst_d3d11_device_lock (device);
  gst_cuda_graphics_resource_unregister (resource);
  gst_d3d11_device_unlock (device);

  if (!gst_cuda_context_pop (nullptr)) {
    GST_WARNING_OBJECT (cuda_context, "failed to pop CUDA context");
  }
}
#endif

/**
 * gst_cuda_graphics_resource_free: (skip)
 * @resource: a #GstCudaGraphicsResource
 *
 * Free @resource
 *
 * Since: 1.22
 */
void
gst_cuda_graphics_resource_free (GstCudaGraphicsResource * resource)
{
  g_return_if_fail (resource != nullptr);

  if (resource->registered) {
#ifdef HAVE_CUDA_GST_GL
    if (resource->type == GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER) {
      gst_gl_context_thread_add ((GstGLContext *) resource->graphics_context,
          (GstGLContextThreadFunc) unregister_resource_from_gl_thread,
          resource);
    } else
#endif
#ifdef G_OS_WIN32
    if (resource->type == GST_CUDA_GRAPHICS_RESOURCE_D3D11_RESOURCE) {
      unregister_d3d11_resource (resource);
    } else
#endif
    {
      /* FIXME: currently only opengl & d3d11 */
      g_assert_not_reached ();
    }
  }

  gst_object_unref (resource->cuda_context);
  if (resource->graphics_context)
    gst_object_unref (resource->graphics_context);
  g_free (resource);
}

const gchar *
gst_cuda_buffer_copy_type_to_string (GstCudaBufferCopyType type)
{
  switch (type) {
    case GST_CUDA_BUFFER_COPY_SYSTEM:
      return "SYSTEM";
    case GST_CUDA_BUFFER_COPY_CUDA:
      return "CUDA";
    case GST_CUDA_BUFFER_COPY_GL:
      return "GL";
    case GST_CUDA_BUFFER_COPY_D3D11:
      return "D3D11";
    case GST_CUDA_BUFFER_COPY_NVMM:
      return "NVMM";
    default:
      g_assert_not_reached ();
      break;
  }

  return "UNKNOWN";
}

static gboolean
gst_cuda_buffer_fallback_copy (GstBuffer * dst, const GstVideoInfo * dst_info,
    GstBuffer * src, const GstVideoInfo * src_info)
{
  GstVideoFrame dst_frame, src_frame;
  guint i, j;

  if (!gst_video_frame_map (&dst_frame, dst_info, dst, GST_MAP_WRITE)) {
    GST_ERROR ("Failed to map dst buffer");
    return FALSE;
  }

  if (!gst_video_frame_map (&src_frame, src_info, src, GST_MAP_READ)) {
    gst_video_frame_unmap (&dst_frame);
    GST_ERROR ("Failed to map src buffer");
    return FALSE;
  }

  /* src and dst resolutions can be different, pick min value */
  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (&dst_frame); i++) {
    guint dst_width_in_bytes, src_width_in_bytes;
    guint dst_height, src_height;
    guint width_in_bytes, height;
    guint dst_stride, src_stride;
    guint8 *dst_data, *src_data;

    dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&dst_frame, i);
    src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);

    width_in_bytes = MIN (dst_width_in_bytes, src_width_in_bytes);

    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&dst_frame, i);
    src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);

    height = MIN (dst_height, src_height);

    dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&dst_frame, i);
    src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&src_frame, i);

    dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);
    src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);

    for (j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += dst_stride;
      src_data += src_stride;
    }
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dst_frame);

  return TRUE;
}

static gboolean
map_buffer_and_fill_copy2d (GstBuffer * buf, const GstVideoInfo * info,
    GstCudaBufferCopyType copy_type, GstVideoFrame * frame,
    GstMapInfo * map_info, gboolean is_src,
    CUDA_MEMCPY2D copy_params[GST_VIDEO_MAX_PLANES])
{
  gboolean buffer_mapped = FALSE;
  guint i;

#ifdef HAVE_NVCODEC_NVMM
  if (copy_type == GST_CUDA_BUFFER_COPY_NVMM) {
    NvBufSurface *surface;
    NvBufSurfaceParams *surface_params;
    NvBufSurfacePlaneParams *plane_params;

    if (!gst_buffer_map (buf, map_info, GST_MAP_READ)) {
      GST_ERROR ("Failed to map input NVMM buffer");
      memset (map_info, 0, sizeof (GstMapInfo));
      return FALSE;
    }

    surface = (NvBufSurface *) map_info->data;

    GST_TRACE ("batch-size %d, num-filled %d, memType %d",
        surface->batchSize, surface->numFilled, surface->memType);

    surface_params = surface->surfaceList;
    buffer_mapped = TRUE;
    if (!surface_params) {
      GST_ERROR ("NVMM memory doesn't hold buffer");
      goto error;
    }

    plane_params = &surface_params->planeParams;
    if (plane_params->num_planes != GST_VIDEO_INFO_N_PLANES (info)) {
      GST_ERROR ("num_planes mismatch, %d / %d",
          plane_params->num_planes, GST_VIDEO_INFO_N_PLANES (info));
      goto error;
    }

    switch (surface->memType) {
        /* TODO: NVBUF_MEM_DEFAULT on jetson is SURFACE_ARRAY */
      case NVBUF_MEM_DEFAULT:
      case NVBUF_MEM_CUDA_DEVICE:
      {
        for (i = 0; i < plane_params->num_planes; i++) {
          if (is_src) {
            copy_params[i].srcMemoryType = CU_MEMORYTYPE_DEVICE;
            copy_params[i].srcDevice = (CUdeviceptr)
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].srcPitch = plane_params->pitch[i];
          } else {
            copy_params[i].dstMemoryType = CU_MEMORYTYPE_DEVICE;
            copy_params[i].dstDevice = (CUdeviceptr)
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].dstPitch = plane_params->pitch[i];
          }
        }
        break;
      }
      case NVBUF_MEM_CUDA_PINNED:
      {
        for (i = 0; i < plane_params->num_planes; i++) {
          if (is_src) {
            copy_params[i].srcMemoryType = CU_MEMORYTYPE_HOST;
            copy_params[i].srcHost =
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].srcPitch = plane_params->pitch[i];
          } else {
            copy_params[i].dstMemoryType = CU_MEMORYTYPE_HOST;
            copy_params[i].dstHost =
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].dstPitch = plane_params->pitch[i];
          }
        }
        break;
      }
      case NVBUF_MEM_CUDA_UNIFIED:
      {
        for (i = 0; i < plane_params->num_planes; i++) {
          if (is_src) {
            copy_params[i].srcMemoryType = CU_MEMORYTYPE_UNIFIED;
            copy_params[i].srcDevice = (CUdeviceptr)
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].srcPitch = plane_params->pitch[i];
          } else {
            copy_params[i].dstMemoryType = CU_MEMORYTYPE_UNIFIED;
            copy_params[i].dstDevice = (CUdeviceptr)
                ((guint8 *) surface_params->dataPtr + plane_params->offset[i]);
            copy_params[i].dstPitch = plane_params->pitch[i];
          }
        }
        break;
      }
      default:
        GST_ERROR ("Unexpected NVMM memory type %d", surface->memType);
        goto error;
    }

    for (i = 0; i < plane_params->num_planes; i++) {
      gsize width_in_bytes, height;

      width_in_bytes = plane_params->width[i] * plane_params->bytesPerPix[i];
      height = plane_params->height[i];

      if (copy_params[i].WidthInBytes == 0 ||
          width_in_bytes < copy_params[i].WidthInBytes) {
        copy_params[i].WidthInBytes = width_in_bytes;
      }

      if (copy_params[i].Height == 0 || height < copy_params[i].Height) {
        copy_params[i].Height = height;
      }
    }
  } else
#endif
  {
    GstMapFlags map_flags;

    if (is_src)
      map_flags = GST_MAP_READ;
    else
      map_flags = GST_MAP_WRITE;

    if (copy_type == GST_CUDA_BUFFER_COPY_CUDA)
      map_flags = (GstMapFlags) (map_flags | GST_MAP_CUDA);

    if (!gst_video_frame_map (frame, info, buf, map_flags)) {
      GST_ERROR ("Failed to map buffer");
      goto error;
    }

    for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (frame); i++) {
      gsize width_in_bytes, height;

      if (is_src) {
        if (copy_type == GST_CUDA_BUFFER_COPY_CUDA) {
          copy_params[i].srcMemoryType = CU_MEMORYTYPE_DEVICE;
          copy_params[i].srcDevice =
              (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (frame, i);
        } else {
          copy_params[i].srcMemoryType = CU_MEMORYTYPE_HOST;
          copy_params[i].srcHost = GST_VIDEO_FRAME_PLANE_DATA (frame, i);
        }
        copy_params[i].srcPitch = GST_VIDEO_FRAME_PLANE_STRIDE (frame, i);
      } else {
        if (copy_type == GST_CUDA_BUFFER_COPY_CUDA) {
          copy_params[i].dstMemoryType = CU_MEMORYTYPE_DEVICE;
          copy_params[i].dstDevice =
              (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (frame, i);
        } else {
          copy_params[i].dstMemoryType = CU_MEMORYTYPE_HOST;
          copy_params[i].dstHost = GST_VIDEO_FRAME_PLANE_DATA (frame, i);
        }
        copy_params[i].dstPitch = GST_VIDEO_FRAME_PLANE_STRIDE (frame, i);
      }

      width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (frame, i) *
          GST_VIDEO_FRAME_COMP_PSTRIDE (frame, i);
      height = GST_VIDEO_FRAME_COMP_HEIGHT (frame, i);

      if (copy_params[i].WidthInBytes == 0 ||
          width_in_bytes < copy_params[i].WidthInBytes) {
        copy_params[i].WidthInBytes = width_in_bytes;
      }

      if (copy_params[i].Height == 0 || height < copy_params[i].Height) {
        copy_params[i].Height = height;
      }
    }
  }

  return TRUE;

error:
  if (buffer_mapped) {
    gst_buffer_unmap (buf, map_info);
    memset (map_info, 0, sizeof (GstMapInfo));
  }

  return FALSE;
}

static void
unmap_buffer_or_frame (GstBuffer * buf, GstVideoFrame * frame,
    GstMapInfo * map_info)
{
  if (frame->buffer)
    gst_video_frame_unmap (frame);

  if (map_info->data)
    gst_buffer_unmap (buf, map_info);
}

static gboolean
gst_cuda_buffer_copy_internal (GstBuffer * dst_buf,
    GstCudaBufferCopyType dst_type, const GstVideoInfo * dst_info,
    GstBuffer * src_buf, GstCudaBufferCopyType src_type,
    const GstVideoInfo * src_info, GstCudaContext * context, CUstream stream)
{
  GstVideoFrame dst_frame, src_frame;
  gboolean ret = FALSE;
  GstMapInfo dst_map, src_map;
  guint i;
  CUDA_MEMCPY2D copy_params[GST_VIDEO_MAX_PLANES];

  memset (copy_params, 0, sizeof (copy_params));
  memset (&dst_frame, 0, sizeof (GstVideoFrame));
  memset (&src_frame, 0, sizeof (GstVideoFrame));
  memset (&dst_map, 0, sizeof (GstMapInfo));
  memset (&src_map, 0, sizeof (GstMapInfo));

  if (!map_buffer_and_fill_copy2d (dst_buf, dst_info,
          dst_type, &dst_frame, &dst_map, FALSE, copy_params)) {
    GST_ERROR_OBJECT (context, "Failed to map output buffer");
    return FALSE;
  }

  if (!map_buffer_and_fill_copy2d (src_buf, src_info,
          src_type, &src_frame, &src_map, TRUE, copy_params)) {
    GST_ERROR_OBJECT (context, "Failed to map input buffer");
    unmap_buffer_or_frame (dst_buf, &dst_frame, &dst_map);
    return FALSE;
  }

  if (!gst_cuda_context_push (context)) {
    GST_ERROR_OBJECT (context, "Failed to push our context");
    goto unmap_and_out;
  }

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (dst_info); i++) {
    ret = gst_cuda_result (CuMemcpy2DAsync (&copy_params[i], stream));
    if (!ret) {
      GST_ERROR_OBJECT (context, "Failed to copy plane %d", i);
      break;
    }
  }

  gst_cuda_result (CuStreamSynchronize (stream));
  gst_cuda_context_pop (nullptr);

unmap_and_out:
  unmap_buffer_or_frame (dst_buf, &src_frame, &src_map);
  unmap_buffer_or_frame (src_buf, &dst_frame, &dst_map);

  return ret;
}

#ifdef HAVE_CUDA_GST_GL
static gboolean
ensure_gl_interop (void)
{
  guint device_count = 0;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  cuda_ret = CuGLGetDevices (&device_count,
      device_list, 1, CU_GL_DEVICE_LIST_ALL);

  if (cuda_ret != CUDA_SUCCESS || device_count == 0)
    return FALSE;

  return TRUE;
}

typedef struct _GLCopyData
{
  GstBuffer *src_buf;
  const GstVideoInfo *src_info;
  GstBuffer *dst_buf;
  const GstVideoInfo *dst_info;

  gboolean pbo_to_cuda;
  GstCudaBufferCopyType copy_type;
  GstCudaContext *context;
  CUstream stream;
  gboolean ret;
} GLCopyData;

static GstCudaGraphicsResource *
ensure_cuda_gl_graphics_resource (GstCudaContext * context, GstMemory * mem)
{
  GQuark quark;
  GstCudaGraphicsResource *ret = nullptr;

  if (!gst_is_gl_memory_pbo (mem)) {
    GST_WARNING_OBJECT (context, "memory is not GL PBO memory, %s",
        mem->allocator->mem_type);
    return nullptr;
  }

  quark = gst_cuda_quark_from_id (GST_CUDA_QUARK_GRAPHICS_RESOURCE);
  ret = (GstCudaGraphicsResource *)
      gst_mini_object_get_qdata (GST_MINI_OBJECT (mem), quark);

  if (!ret) {
    GstGLMemoryPBO *pbo;
    GstGLBuffer *buf;
    GstMapInfo info;

    ret = gst_cuda_graphics_resource_new (context,
        GST_OBJECT (GST_GL_BASE_MEMORY_CAST (mem)->context),
        GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER);

    if (!gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_READ | GST_MAP_GL))) {
      GST_ERROR_OBJECT (context, "Failed to map gl memory");
      gst_cuda_graphics_resource_free (ret);
      return nullptr;
    }

    pbo = (GstGLMemoryPBO *) mem;
    buf = pbo->pbo;

    if (!gst_cuda_graphics_resource_register_gl_buffer (ret,
            buf->id, CU_GRAPHICS_REGISTER_FLAGS_NONE)) {
      GST_ERROR_OBJECT (context, "Failed to register gl buffer");
      gst_memory_unmap (mem, &info);
      gst_cuda_graphics_resource_free (ret);

      return nullptr;
    }

    gst_memory_unmap (mem, &info);

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem), quark, ret,
        (GDestroyNotify) gst_cuda_graphics_resource_free);
  }

  return ret;
}

static void
gl_copy_thread_func (GstGLContext * gl_context, GLCopyData * data)
{
  GstCudaGraphicsResource *resources[GST_VIDEO_MAX_PLANES];
  guint num_resources;
  GstBuffer *gl_buf, *cuda_buf;
  GstVideoFrame cuda_frame;
  GstMapInfo cuda_map_info;
  CUDA_MEMCPY2D copy_params[GST_VIDEO_MAX_PLANES];
  guint i;
  GstCudaContext *context = data->context;
  CUstream stream = data->stream;

  memset (copy_params, 0, sizeof (copy_params));
  memset (&cuda_frame, 0, sizeof (GstVideoFrame));
  memset (&cuda_map_info, 0, sizeof (GstMapInfo));

  data->ret = FALSE;

  /* Incompatible gl context */
  if (!ensure_gl_interop ())
    return;

  if (data->pbo_to_cuda) {
    gl_buf = data->src_buf;
    cuda_buf = data->dst_buf;

    if (!map_buffer_and_fill_copy2d (cuda_buf,
            data->dst_info, data->copy_type, &cuda_frame, &cuda_map_info,
            FALSE, copy_params)) {
      GST_ERROR_OBJECT (context, "Failed to map output CUDA buffer");
      return;
    }
  } else {
    gl_buf = data->dst_buf;
    cuda_buf = data->src_buf;

    if (!map_buffer_and_fill_copy2d (cuda_buf,
            data->src_info, data->copy_type, &cuda_frame, &cuda_map_info,
            TRUE, copy_params)) {
      GST_ERROR_OBJECT (context, "Failed to map input CUDA buffer");
      return;
    }
  }

  num_resources = gst_buffer_n_memory (gl_buf);
  g_assert (num_resources >= GST_VIDEO_INFO_N_PLANES (data->src_info));

  if (!gst_cuda_context_push (context)) {
    GST_ERROR_OBJECT (context, "Failed to push context");
    unmap_buffer_or_frame (cuda_buf, &cuda_frame, &cuda_map_info);
    return;
  }

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (data->src_info); i++) {
    GstMemory *mem = gst_buffer_peek_memory (gl_buf, i);
    GstGLMemoryPBO *pbo;

    resources[i] = ensure_cuda_gl_graphics_resource (context, mem);
    if (!resources[i])
      goto out;

    pbo = (GstGLMemoryPBO *) mem;
    if (!data->pbo_to_cuda) {
      /* Need PBO -> texture */
      GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);

      /* PBO -> sysmem */
      GST_MINI_OBJECT_FLAG_SET (pbo->pbo,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);
    } else {
      /* get the texture into the PBO */
      gst_gl_memory_pbo_upload_transfer (pbo);
      gst_gl_memory_pbo_download_transfer (pbo);
    }
  }

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (data->src_info); i++) {
    CUgraphicsResource cuda_resource;
    CUdeviceptr dev_ptr;
    size_t size;
    gboolean copy_ret;
    gsize width_in_bytes, height;

    if (data->pbo_to_cuda) {
      cuda_resource =
          gst_cuda_graphics_resource_map (resources[i], stream,
          CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
    } else {
      cuda_resource =
          gst_cuda_graphics_resource_map (resources[i], stream,
          CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD);
    }

    if (!cuda_resource) {
      GST_ERROR_OBJECT (context, "Failed to map graphics resource %d", i);
      goto out;
    }

    if (!gst_cuda_result (CuGraphicsResourceGetMappedPointer (&dev_ptr, &size,
                cuda_resource))) {
      gst_cuda_graphics_resource_unmap (resources[i], stream);
      GST_ERROR_OBJECT (context, "Failed to get mapped pointer");
      goto out;
    }

    if (data->pbo_to_cuda) {
      copy_params[i].srcMemoryType = CU_MEMORYTYPE_DEVICE;
      copy_params[i].srcDevice = dev_ptr;
      copy_params[i].srcPitch = GST_VIDEO_INFO_PLANE_STRIDE (data->src_info, i);

      width_in_bytes = GST_VIDEO_INFO_COMP_WIDTH (data->src_info, i) *
          GST_VIDEO_INFO_COMP_PSTRIDE (data->src_info, i);
      height = GST_VIDEO_INFO_COMP_HEIGHT (data->src_info, i);
    } else {
      copy_params[i].dstMemoryType = CU_MEMORYTYPE_DEVICE;
      copy_params[i].dstDevice = dev_ptr;
      copy_params[i].dstPitch = GST_VIDEO_INFO_PLANE_STRIDE (data->dst_info, i);

      width_in_bytes = GST_VIDEO_INFO_COMP_WIDTH (data->dst_info, i) *
          GST_VIDEO_INFO_COMP_PSTRIDE (data->dst_info, i);
      height = GST_VIDEO_INFO_COMP_HEIGHT (data->dst_info, i);
    }

    if (width_in_bytes < copy_params[i].WidthInBytes)
      copy_params[i].WidthInBytes = width_in_bytes;

    if (height < copy_params[i].Height)
      copy_params[i].Height = height;

    copy_ret = gst_cuda_result (CuMemcpy2DAsync (&copy_params[i], stream));
    gst_cuda_graphics_resource_unmap (resources[i], stream);

    if (!copy_ret) {
      GST_ERROR_OBJECT (context, "Failed to copy plane %d", i);
      goto out;
    }
  }

  data->ret = TRUE;

out:
  gst_cuda_result (CuStreamSynchronize (stream));
  gst_cuda_context_pop (nullptr);
  unmap_buffer_or_frame (cuda_buf, &cuda_frame, &cuda_map_info);
}

static gboolean
cuda_copy_gl_interop (GstBuffer * dst_buf, const GstVideoInfo * dst_info,
    GstBuffer * src_buf, const GstVideoInfo * src_info,
    GstGLContext * gl_context, GstCudaContext * context, CUstream stream,
    gboolean pbo_to_cuda, GstCudaBufferCopyType copy_type)
{
  GLCopyData data;

  g_assert (copy_type == GST_CUDA_BUFFER_COPY_CUDA ||
      copy_type == GST_CUDA_BUFFER_COPY_NVMM);

  data.src_buf = src_buf;
  data.src_info = src_info;
  data.dst_buf = dst_buf;
  data.dst_info = dst_info;
  data.pbo_to_cuda = pbo_to_cuda;
  data.copy_type = copy_type;
  data.context = context;
  data.stream = stream;
  data.ret = FALSE;

  gst_gl_context_thread_add (gl_context,
      (GstGLContextThreadFunc) gl_copy_thread_func, &data);

  return data.ret;
}
#endif

#ifdef G_OS_WIN32
static gboolean
ensure_d3d11_interop (GstCudaContext * context, GstD3D11Device * device)
{
  guint device_count = 0;
  guint cuda_device_id;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  g_object_get (context, "cuda-device-id", &cuda_device_id, nullptr);

  cuda_ret = CuD3D11GetDevices (&device_count,
      device_list, 1, gst_d3d11_device_get_device_handle (device),
      CU_D3D11_DEVICE_LIST_ALL);

  if (cuda_ret != CUDA_SUCCESS || device_count == 0)
    return FALSE;

  if (device_list[0] != (CUdevice) cuda_device_id)
    return FALSE;

  return TRUE;
}

static GstCudaGraphicsResource *
ensure_cuda_d3d11_graphics_resource (GstCudaContext * context, GstMemory * mem)
{
  static gint64 d3d11_interop_token = 0;
  static std::once_flag once_flag;
  GstCudaGraphicsResource *ret = nullptr;
  GstD3D11Memory *dmem;

  GST_CUDA_CALL_ONCE_BEGIN {
    d3d11_interop_token = gst_d3d11_create_user_token ();
  } GST_CUDA_CALL_ONCE_END;

  if (!gst_is_d3d11_memory (mem)) {
    GST_WARNING_OBJECT (context, "memory is not D3D11 memory, %s",
        mem->allocator->mem_type);
    return nullptr;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  ret = (GstCudaGraphicsResource *)
      gst_d3d11_memory_get_token_data (dmem, d3d11_interop_token);

  if (!ret) {
    ret = gst_cuda_graphics_resource_new (context,
        GST_OBJECT (GST_D3D11_MEMORY_CAST (mem)->device),
        GST_CUDA_GRAPHICS_RESOURCE_D3D11_RESOURCE);

    if (!gst_cuda_graphics_resource_register_d3d11_resource (ret,
            gst_d3d11_memory_get_resource_handle (GST_D3D11_MEMORY_CAST (mem)),
            CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LOAD_STORE)) {
      GST_ERROR_OBJECT (context, "failed to register d3d11 resource");
      gst_cuda_graphics_resource_free (ret);

      ret = nullptr;
    } else {
      gst_d3d11_memory_set_token_data (dmem, d3d11_interop_token, ret,
          (GDestroyNotify) gst_cuda_graphics_resource_free);
    }
  }

  return ret;
}

static gboolean
cuda_copy_d3d11_interop (GstBuffer * dst_buf, const GstVideoInfo * dst_info,
    GstBuffer * src_buf, const GstVideoInfo * src_info, GstD3D11Device * device,
    GstCudaContext * context, CUstream stream, gboolean d3d11_to_cuda)
{
  GstCudaGraphicsResource *resources[GST_VIDEO_MAX_PLANES];
  D3D11_TEXTURE2D_DESC desc[GST_VIDEO_MAX_PLANES];
  guint num_resources;
  GstBuffer *d3d11_buf, *cuda_buf;
  GstVideoFrame d3d11_frame, cuda_frame;
  GstMapInfo cuda_map_info;
  CUDA_MEMCPY2D copy_params[GST_VIDEO_MAX_PLANES];
  guint i;
  gboolean ret = FALSE;

  memset (copy_params, 0, sizeof (copy_params));
  memset (&cuda_frame, 0, sizeof (GstVideoFrame));
  memset (&cuda_map_info, 0, sizeof (GstMapInfo));

  /* Incompatible d3d11 device */
  if (!ensure_d3d11_interop (context, device))
    return FALSE;

  if (d3d11_to_cuda) {
    d3d11_buf = src_buf;
    cuda_buf = dst_buf;
    if (!gst_video_frame_map (&d3d11_frame, src_info, d3d11_buf,
            (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (context, "Failed to map input D3D11 buffer");
      return FALSE;
    }
    if (!map_buffer_and_fill_copy2d (cuda_buf,
            dst_info, GST_CUDA_BUFFER_COPY_CUDA, &cuda_frame, &cuda_map_info,
            FALSE, copy_params)) {
      GST_ERROR_OBJECT (context, "Failed to map output CUDA buffer");
      gst_video_frame_unmap (&d3d11_frame);
      return FALSE;
    }
  } else {
    d3d11_buf = dst_buf;
    cuda_buf = src_buf;
    if (!gst_video_frame_map (&d3d11_frame, dst_info, d3d11_buf,
            (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (context, "Failed to map output D3D11 buffer");
      return FALSE;
    }
    if (!map_buffer_and_fill_copy2d (cuda_buf,
            src_info, GST_CUDA_BUFFER_COPY_CUDA, &cuda_frame, &cuda_map_info,
            TRUE, copy_params)) {
      GST_ERROR_OBJECT (context, "Failed to map input CUDA buffer");
      gst_video_frame_unmap (&d3d11_frame);
      return FALSE;
    }
  }

  num_resources = gst_buffer_n_memory (d3d11_buf);
  g_assert (num_resources >= GST_VIDEO_FRAME_N_PLANES (&d3d11_frame));

  if (!gst_cuda_context_push (context)) {
    GST_ERROR_OBJECT (context, "Failed to push context");
    gst_video_frame_unmap (&d3d11_frame);
    unmap_buffer_or_frame (cuda_buf, &cuda_frame, &cuda_map_info);
    return FALSE;
  }

  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (&d3d11_frame); i++) {
    GstMemory *mem = gst_buffer_peek_memory (d3d11_buf, i);

    resources[i] = ensure_cuda_d3d11_graphics_resource (context, mem);
    if (!resources[i]
        || !gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (mem),
            &desc[i]))
      goto out;
  }

  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (&d3d11_frame); i++) {
    CUgraphicsResource cuda_resource;
    CUarray d3d11_array;
    gboolean copy_ret;

    if (d3d11_to_cuda) {
      cuda_resource =
          gst_cuda_graphics_resource_map (resources[i], stream,
          CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
    } else {
      cuda_resource =
          gst_cuda_graphics_resource_map (resources[i], stream,
          CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD);
    }

    if (!cuda_resource) {
      GST_ERROR_OBJECT (context, "Failed to map graphics resource %d", i);
      goto out;
    }

    if (!gst_cuda_result (CuGraphicsSubResourceGetMappedArray (&d3d11_array,
                cuda_resource, 0, 0))) {
      gst_cuda_graphics_resource_unmap (resources[i], stream);
      GST_ERROR_OBJECT (context, "Failed to get mapped array");
      goto out;
    }

    if (d3d11_to_cuda) {
      copy_params[i].srcMemoryType = CU_MEMORYTYPE_ARRAY;
      copy_params[i].srcArray = d3d11_array;
      copy_params[i].srcPitch =
          desc[i].Width * GST_VIDEO_FRAME_COMP_PSTRIDE (&d3d11_frame, i);
    } else {
      copy_params[i].dstMemoryType = CU_MEMORYTYPE_ARRAY;
      copy_params[i].dstArray = d3d11_array;
      copy_params[i].dstPitch =
          desc[i].Width * GST_VIDEO_FRAME_COMP_PSTRIDE (&d3d11_frame, i);
    }

    copy_ret = gst_cuda_result (CuMemcpy2DAsync (&copy_params[i], stream));
    gst_cuda_graphics_resource_unmap (resources[i], stream);

    if (!copy_ret) {
      GST_ERROR_OBJECT (context, "Failed to copy plane %d", i);
      goto out;
    }
  }

  ret = TRUE;

out:
  gst_cuda_result (CuStreamSynchronize (stream));
  gst_cuda_context_pop (nullptr);
  gst_video_frame_unmap (&d3d11_frame);
  unmap_buffer_or_frame (cuda_buf, &cuda_frame, &cuda_map_info);

  return ret;
}
#endif

gboolean
gst_cuda_buffer_copy (GstBuffer * dst, GstCudaBufferCopyType dst_type,
    const GstVideoInfo * dst_info, GstBuffer * src,
    GstCudaBufferCopyType src_type, const GstVideoInfo * src_info,
    GstCudaContext * context, GstCudaStream * stream)
{
  gboolean use_copy_2d = FALSE;
  GstMemory *dst_mem, *src_mem;
#ifdef G_OS_WIN32
  D3D11_TEXTURE2D_DESC desc;
#endif
  GstCudaContext *cuda_context = context;
  GstCudaMemory *cmem = nullptr;
  GstCudaStream *mem_stream = nullptr;
  gboolean ret;

  g_return_val_if_fail (GST_IS_BUFFER (dst), FALSE);
  g_return_val_if_fail (dst_info != nullptr, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (src), FALSE);
  g_return_val_if_fail (src_info != nullptr, FALSE);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), FALSE);

  _init_debug ();

  if (dst_type == GST_CUDA_BUFFER_COPY_NVMM &&
      src_type == GST_CUDA_BUFFER_COPY_NVMM) {
    GST_ERROR_OBJECT (context, "Not supported copy NVMM -> NVMM");
    return FALSE;
  }

  if (GST_VIDEO_INFO_FORMAT (dst_info) != GST_VIDEO_INFO_FORMAT (src_info)) {
    GST_ERROR_OBJECT (context,
        "Copy between different format is not supported");
    return FALSE;
  }

  if (dst_type == GST_CUDA_BUFFER_COPY_CUDA ||
      dst_type == GST_CUDA_BUFFER_COPY_NVMM ||
      src_type == GST_CUDA_BUFFER_COPY_CUDA ||
      src_type == GST_CUDA_BUFFER_COPY_NVMM) {
    use_copy_2d = TRUE;
  }

  if (!use_copy_2d) {
    GST_TRACE_OBJECT (context, "Not a device memory, use system memory copy");
    return gst_cuda_buffer_fallback_copy (dst, dst_info, src, src_info);
  }

  dst_mem = gst_buffer_peek_memory (dst, 0);
  src_mem = gst_buffer_peek_memory (src, 0);

#ifdef HAVE_CUDA_GST_GL
  if (src_type == GST_CUDA_BUFFER_COPY_GL && gst_is_gl_memory_pbo (src_mem)) {
    GstGLMemory *gl_mem = (GstGLMemory *) src_mem;
    GstGLContext *gl_context = gl_mem->mem.context;

    if (dst_type == GST_CUDA_BUFFER_COPY_CUDA && gst_is_cuda_memory (dst_mem)) {
      cmem = GST_CUDA_MEMORY_CAST (dst_mem);
      cuda_context = cmem->context;
      mem_stream = gst_cuda_memory_get_stream (cmem);
      if (mem_stream)
        stream = mem_stream;
    }

    GST_TRACE_OBJECT (context, "GL -> %s",
        gst_cuda_buffer_copy_type_to_string (dst_type));

    ret = cuda_copy_gl_interop (dst, dst_info, src, src_info, gl_context,
        cuda_context, gst_cuda_stream_get_handle (stream), TRUE, dst_type);

    if (cmem)
      GST_MEMORY_FLAG_UNSET (cmem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

    return ret;
  }

  if (dst_type == GST_CUDA_BUFFER_COPY_GL && gst_is_gl_memory_pbo (dst_mem)) {
    GstGLMemory *gl_mem = (GstGLMemory *) dst_mem;
    GstGLContext *gl_context = gl_mem->mem.context;

    if (src_type == GST_CUDA_BUFFER_COPY_CUDA && gst_is_cuda_memory (src_mem)) {
      cmem = GST_CUDA_MEMORY_CAST (src_mem);
      cuda_context = cmem->context;

      /* Use memory's stream object if available */
      mem_stream = gst_cuda_memory_get_stream (cmem);
      if (mem_stream)
        stream = mem_stream;
    }

    GST_TRACE_OBJECT (context, "%s -> GL",
        gst_cuda_buffer_copy_type_to_string (src_type));

    return cuda_copy_gl_interop (dst, dst_info, src, src_info, gl_context,
        cuda_context, gst_cuda_stream_get_handle (stream), FALSE, src_type);
  }
#endif

#ifdef G_OS_WIN32
  if (src_type == GST_CUDA_BUFFER_COPY_D3D11 && gst_is_d3d11_memory (src_mem) &&
      gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (src_mem), &desc)
      && desc.Usage == D3D11_USAGE_DEFAULT && gst_is_cuda_memory (dst_mem)) {
    GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (src_mem);
    GstD3D11Device *device = dmem->device;

    cmem = GST_CUDA_MEMORY_CAST (dst_mem);
    cuda_context = cmem->context;

    /* Use memory's stream object if available */
    mem_stream = gst_cuda_memory_get_stream (cmem);
    if (mem_stream)
      stream = mem_stream;

    GST_TRACE_OBJECT (context, "D3D11 -> CUDA");

    gst_d3d11_device_lock (device);
    ret = cuda_copy_d3d11_interop (dst, dst_info, src, src_info, device,
        cuda_context, gst_cuda_stream_get_handle (stream), TRUE);
    gst_d3d11_device_unlock (device);

    GST_MEMORY_FLAG_UNSET (cmem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

    return ret;
  }

  if (dst_type == GST_CUDA_BUFFER_COPY_D3D11 && gst_is_d3d11_memory (dst_mem) &&
      gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (dst_mem), &desc)
      && desc.Usage == D3D11_USAGE_DEFAULT && gst_is_cuda_memory (src_mem)) {
    GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (dst_mem);
    GstD3D11Device *device = dmem->device;

    cmem = GST_CUDA_MEMORY_CAST (src_mem);
    cuda_context = cmem->context;

    /* Use memory's stream object if available */
    mem_stream = gst_cuda_memory_get_stream (cmem);
    if (mem_stream)
      stream = mem_stream;

    GST_TRACE_OBJECT (context, "CUDA -> D3D11");

    gst_d3d11_device_lock (device);
    ret = cuda_copy_d3d11_interop (dst, dst_info, src, src_info, device,
        cuda_context, gst_cuda_stream_get_handle (stream), FALSE);
    gst_d3d11_device_unlock (device);

    return ret;
  }
#endif

  if (gst_is_cuda_memory (dst_mem)) {
    cmem = GST_CUDA_MEMORY_CAST (dst_mem);
  } else if (gst_is_cuda_memory (src_mem)) {
    cmem = GST_CUDA_MEMORY_CAST (src_mem);
  } else {
    cmem = nullptr;
  }

  if (cmem) {
    context = cmem->context;
    mem_stream = gst_cuda_memory_get_stream (cmem);
    if (mem_stream)
      stream = mem_stream;
  }

  GST_TRACE_OBJECT (context, "%s -> %s",
      gst_cuda_buffer_copy_type_to_string (src_type),
      gst_cuda_buffer_copy_type_to_string (dst_type));

  ret = gst_cuda_buffer_copy_internal (dst, dst_type, dst_info,
      src, src_type, src_info, cuda_context,
      gst_cuda_stream_get_handle (stream));

  /* Already synchronized */
  if (gst_is_cuda_memory (src_mem))
    GST_MEMORY_FLAG_UNSET (src_mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

  return ret;
}

/**
 * gst_cuda_create_user_token:
 *
 * Creates new user token value
 *
 * Returns: user token value
 *
 * Since: 1.24
 */
gint64
gst_cuda_create_user_token (void)
{
  /* *INDENT-OFF* */
  static std::atomic < gint64 > user_token { 0 };
  /* *INDENT-ON* */

  return user_token.fetch_add (1);
}

static gboolean
_abort_on_error (CUresult result)
{
  static std::set < CUresult > abort_list;
  GST_CUDA_CALL_ONCE_BEGIN {
    const gchar *env = g_getenv ("GST_CUDA_CRITICAL_ERRORS");
    if (!env)
      return;

    gchar **split = g_strsplit (env, ",", 0);
    gchar **iter;
    for (iter = split; *iter; iter++) {
      int error_code = 0;
      try {
        error_code = std::stoi (*iter);
      } catch ( ...) {
        GST_WARNING ("Invalid argument \"%s\"", *iter);
        continue;
      };

      if (error_code > 0)
        abort_list.insert ((CUresult) error_code);
    }

    g_strfreev (split);
  }
  GST_CUDA_CALL_ONCE_END;

  if (abort_list.empty ())
    return FALSE;

  if (abort_list.find (result) != abort_list.end ())
    return TRUE;

  return FALSE;
}

/**
 * _gst_cuda_debug:
 * @result: CUDA result code
 * @cat: a #GstDebugCategory
 * @file: the file that checking the result code
 * @function: the function that checking the result code
 * @line: the line that checking the result code
 *
 * Returns: %TRUE if CUDA device API call result is CUDA_SUCCESS
 *
 * Since: 1.24
 */
gboolean
_gst_cuda_debug (CUresult result, GstDebugCategory * cat,
    const gchar * file, const gchar * function, gint line)
{
  if (result != CUDA_SUCCESS) {
#ifndef GST_DISABLE_GST_DEBUG
    const gchar *_error_name, *_error_text;
    CuGetErrorName (result, &_error_name);
    CuGetErrorString (result, &_error_text);
    gst_debug_log (cat, GST_LEVEL_WARNING, file, function, line,
        NULL, "CUDA call failed: %s, %s", _error_name, _error_text);
#endif
    if (_abort_on_error (result)) {
      GST_ERROR ("Critical error %d, abort", (gint) result);
      g_abort ();
    }

    return FALSE;
  }

  return TRUE;
}

#ifdef G_OS_WIN32
/* Defined in ntdef.h */
struct OBJECT_ATTRIBUTES
{
  ULONG Length;
  HANDLE RootDirectory;
  PVOID ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
};
#endif

gpointer
gst_cuda_get_win32_handle_metadata (void)
{
#ifdef G_OS_WIN32
  static const gchar sddl[] = "D:P(OA;;GARCSDWDWORPWPCCDCLCSWLODTCRFA;;;WD)";
  static OBJECT_ATTRIBUTES attr;
  static BOOL initialized = FALSE;

  /*
   * D:P(string_ace) -> DACL protected with "string_ace"
   *
   * ace_string format
   * ace_type;ace_flags;rights;object_guid;inherit_object_guid;account_sid
   *
   * ace_type: OA (ACCESS_ALLOWED_OBJECT_ACE_TYPE)
   *
   * ace_flags: (empty)
   *
   * rights: GARCSDWDWOCCDCLCSWLODTWPRPCRFA (allow all)
   * - Generic access rights
   *   GA (GENERIC_ALL)
   * - Standard access right
   *   RC (READ_CONTROL) SD (DELETE) WD (WRITE_DAC) WO (WRITE_OWNER)
   * - Directory service object access right
   *   RP (ADS_RIGHT_DS_READ_PROP)
   *   WP (ADS_RIGHT_DS_WRITE_PROP)
   *   CC (ADS_RIGHT_DS_CREATE_CHILD)
   *   DC (ADS_RIGHT_DS_DELETE_CHILD)
   *   LC (ADS_RIGHT_ACTRL_DS_LIST)
   *   SW (ADS_RIGHT_DS_SELF)
   *   LO (ADS_RIGHT_DS_LIST_OBJECT)
   *   DT (ADS_RIGHT_DS_DELETE_TREE)
   *   CR (ADS_RIGHT_DS_CONTROL_ACCESS)
   * - File access rights
   *   FA (FILE_GENERIC_ALL)
   *
   * object_guid: (empty)
   *
   * inherit_object_guid (empty)
   *
   * account_sid: WD (SDDL_EVERYONE)
   *
   * See also
   * https://learn.microsoft.com/en-us/windows/win32/secauthz/security-descriptor-string-format
   */
  GST_CUDA_CALL_ONCE_BEGIN {
    PSECURITY_DESCRIPTOR desc;

    memset (&attr, 0, sizeof (OBJECT_ATTRIBUTES));

    initialized = ConvertStringSecurityDescriptorToSecurityDescriptorA (sddl,
        SDDL_REVISION_1, &desc, nullptr);
    if (!initialized)
      return;

    attr.Length = sizeof (OBJECT_ATTRIBUTES);
    attr.SecurityDescriptor = desc;
  }
  GST_CUDA_CALL_ONCE_END;

  if (!initialized)
    return nullptr;

  return &attr;
#else
  return nullptr;
#endif
}
