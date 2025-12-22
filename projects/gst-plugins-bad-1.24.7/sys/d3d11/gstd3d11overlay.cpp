/*
 * GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-d3d11overlay
 * @title: d3d11overlay
 *
 * Provides Direct3D11 render target view handles to an application so that
 * the application can overlay/blend an image on the render target
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d11testsrc ! queue ! d3d11overlay ! queue ! d3d11videosink
 * ```
 *
 * Since: 1.24
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstd3d11overlay.h"
#include <gst/d3d11/gstd3d11-private.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_overlay_debug);
#define GST_CAT_DEFAULT gst_d3d11_overlay_debug

static GstStaticCaps template_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, "{ BGRA, RGBA }"));

enum
{
  SIGNAL_DRAW,
  SIGNAL_CAPS_CHANGED,
  SIGNAL_LAST,
};

static guint gst_d3d11_overlay_signals[SIGNAL_LAST];

struct _GstD3D11Overlay
{
  GstD3D11BaseFilter parent;

  GstBufferPool *fallback_pool;
};

#define gst_d3d11_overlay_parent_class parent_class
G_DEFINE_TYPE (GstD3D11Overlay, gst_d3d11_overlay, GST_TYPE_D3D11_BASE_FILTER);

static gboolean gst_d3d11_overlay_stop (GstBaseTransform * trans);
static gboolean gst_d3d11_overlay_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static GstFlowReturn gst_d3d11_overlay_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_d3d11_overlay_set_info (GstD3D11BaseFilter * filter,
    GstCaps * in_caps, GstVideoInfo * in_info, GstCaps * out_caps,
    GstVideoInfo * out_info);

static void
gst_d3d11_overlay_class_init (GstD3D11OverlayClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstD3D11BaseFilterClass *filter_class = GST_D3D11_BASE_FILTER_CLASS (klass);
  GstCaps *caps;

  /**
   * GstD3D11Overlay::draw:
   * @overlay: Overlay element emitting the signal
   * @device: GstD3D11Device object
   * @render_target: ID3D11RenderTargetView handle
   * @timestamp: Timestamp (see #GstClockTime) of the current buffer
   * @duration: Duration (see #GstClockTime) of the current buffer
   *
   * This signal is emitted when an overlay can be drawn. This signal is
   * emitted with gst_d3d11_device_lock taken.
   *
   * Client should return %TRUE if an overlay has been rendered.
   * Otherwise the element might discard the updated scene.
   *
   * Returns: %TRUE if rendering operation happend
   *
   * Since: 1.24
   */
  gst_d3d11_overlay_signals[SIGNAL_DRAW] = g_signal_new ("draw",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
      nullptr, G_TYPE_BOOLEAN, 4, GST_TYPE_OBJECT, G_TYPE_POINTER,
      G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * GstD3D11Overlay::caps-changed:
   * @overlay: Overlay element emitting the signal
   * @caps: #GstCaps of the element

   * This signal is emitted when the caps or associated GstD3D11Device
   * of the element has changed
   *
   * Since: 1.24
   */
  gst_d3d11_overlay_signals[SIGNAL_CAPS_CHANGED] = g_signal_new ("caps-changed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
      nullptr, G_TYPE_NONE, 1, GST_TYPE_CAPS);

  caps = gst_d3d11_get_updated_template_caps (&template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Overlay", "Filter/Video",
      "Provides application renderable Direct3D11 render target view",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->passthrough_on_same_caps = FALSE;

  trans_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_overlay_stop);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_overlay_propose_allocation);
  trans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_d3d11_overlay_transform_ip);

  filter_class->set_info = GST_DEBUG_FUNCPTR (gst_d3d11_overlay_set_info);

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_overlay_debug,
      "d3d11overlay", 0, "d3d11overlay");
}

static void
gst_d3d11_overlay_init (GstD3D11Overlay * self)
{
}

static gboolean
gst_d3d11_overlay_stop (GstBaseTransform * trans)
{
  GstD3D11Overlay *self = GST_D3D11_OVERLAY (trans);

  if (self->fallback_pool) {
    gst_buffer_pool_set_active (self->fallback_pool, FALSE);
    gst_clear_object (&self->fallback_pool);
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static gboolean
gst_d3d11_overlay_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstVideoInfo info;
  GstCaps *caps;
  guint size;
  GstBufferPool *pool = nullptr;
  gboolean update_pool = FALSE;
  guint min = 0;
  guint max = 0;
  GstStructure *config;
  GstD3D11AllocationParams *params;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_WARNING_OBJECT (filter, "Query without caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (filter, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, nullptr, &min, &max);

    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != filter->device)
          gst_clear_object (&pool);
      }
    }

    update_pool = TRUE;
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (filter->device);

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

  params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (params) {
    gst_d3d11_allocation_params_set_bind_flags (params,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
  } else {
    params = gst_d3d11_allocation_params_new (filter->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (filter, "Couldn't set pool config");
    gst_object_unref (pool);
    return FALSE;
  }

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);

  return TRUE;
}

static gboolean
gst_d3d11_overlay_copy_memory (GstD3D11Overlay * self, GstMemory * src,
    GstMemory * dst)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (self);
  GstMapInfo src_map;
  GstMapInfo dst_map;
  ID3D11Resource *dst_texture, *src_texture;
  ID3D11DeviceContext *context;
  D3D11_TEXTURE2D_DESC src_desc, dst_desc;
  D3D11_BOX src_box;

  context = gst_d3d11_device_get_device_context_handle (filter->device);

  if (!gst_memory_map (src,
          &src_map, (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map src memory");
    return FALSE;
  }

  if (!gst_memory_map (dst,
          &dst_map, (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map dst memory");
    gst_memory_unmap (src, &src_map);
    return FALSE;
  }

  gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (src), &src_desc);
  gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (dst), &dst_desc);

  src_texture = (ID3D11Texture2D *) src_map.data;
  dst_texture = (ID3D11Texture2D *) dst_map.data;

  src_box.front = 0;
  src_box.back = 1;
  src_box.left = 0;
  src_box.top = 0;
  src_box.right = MIN (src_desc.Width, dst_desc.Width);
  src_box.bottom = MIN (src_desc.Height, dst_desc.Height);

  context->CopySubresourceRegion (dst_texture,
      0, 0, 0, 0, src_texture, 0, &src_box);

  gst_memory_unmap (src, &src_map);
  gst_memory_unmap (dst, &dst_map);

  return TRUE;
}

static GstFlowReturn
gst_d3d11_overlay_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstD3D11Overlay *self = GST_D3D11_OVERLAY (trans);
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ID3D11RenderTargetView *rtv;
  GstBuffer *fallback_buf = nullptr;
  GstMemory *fallback_mem = nullptr;
  GstD3D11Memory *fallback_dmem = nullptr;
  GstMemory *target_mem;
  GstMapInfo map;
  gboolean rendered = FALSE;
  GstFlowReturn ret = GST_FLOW_OK;

  mem = gst_buffer_peek_memory (buf, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_ERROR_OBJECT (self, "Not a d3d11 memory");
    return GST_FLOW_ERROR;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  rtv = gst_d3d11_memory_get_render_target_view (dmem, 0);
  if (!rtv) {
    GST_DEBUG_OBJECT (self, "RTV is unavailable");

    gst_buffer_pool_acquire_buffer (self->fallback_pool, &fallback_buf,
        nullptr);
    if (!fallback_buf) {
      GST_ERROR_OBJECT (self, "Couldn't allocate fallback buffer");
      return GST_FLOW_ERROR;
    }
  }

  GstD3D11DeviceLockGuard lk (filter->device);
  if (fallback_buf) {
    fallback_mem = gst_buffer_peek_memory (fallback_buf, 0);

    if (!gst_d3d11_overlay_copy_memory (self, mem, fallback_mem)) {
      GST_ERROR_OBJECT (self, "Couldn't copy input memory to fallback");
      gst_buffer_unref (fallback_buf);
      return GST_FLOW_ERROR;
    }

    fallback_dmem = GST_D3D11_MEMORY_CAST (fallback_mem);
    rtv = gst_d3d11_memory_get_render_target_view (fallback_dmem, 0);
    if (!rtv) {
      GST_ERROR_OBJECT (self, "RTV is unavailable");
      gst_buffer_unref (fallback_buf);
      return GST_FLOW_ERROR;
    }

    target_mem = fallback_mem;
  } else {
    target_mem = mem;
  }

  if (!gst_memory_map (target_mem, &map,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map render target memory");
    gst_clear_buffer (&fallback_buf);
    return GST_FLOW_ERROR;
  }

  g_signal_emit (self, gst_d3d11_overlay_signals[SIGNAL_DRAW], 0,
      filter->device, rtv, GST_BUFFER_PTS (buf), GST_BUFFER_DURATION (buf),
      &rendered);

  GST_LOG_OBJECT (self, "Draw signal return: %d", rendered);

  gst_memory_unmap (target_mem, &map);

  if (fallback_buf && rendered) {
    if (!gst_d3d11_overlay_copy_memory (self, fallback_mem, mem)) {
      GST_ERROR_OBJECT (self, "Couldn't copy back to input memory");
      ret = GST_FLOW_ERROR;
    }
  }

  gst_clear_buffer (&fallback_buf);

  return ret;
}

static gboolean
gst_d3d11_overlay_set_info (GstD3D11BaseFilter * filter, GstCaps * in_caps,
    GstVideoInfo * in_info, GstCaps * out_caps, GstVideoInfo * out_info)
{
  GstD3D11Overlay *self = GST_D3D11_OVERLAY (filter);
  GstStructure *config;
  GstD3D11AllocationParams *params;

  if (self->fallback_pool) {
    gst_buffer_pool_set_active (self->fallback_pool, FALSE);
    gst_object_unref (self->fallback_pool);
  }

  self->fallback_pool = gst_d3d11_buffer_pool_new (filter->device);
  config = gst_buffer_pool_get_config (self->fallback_pool);

  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  gst_buffer_pool_config_set_params (config, in_caps,
      GST_VIDEO_INFO_SIZE (in_info), 0, 0);

  params = gst_d3d11_allocation_params_new (filter->device, in_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (self->fallback_pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set pool config");
    gst_clear_object (&self->fallback_pool);
    return FALSE;
  }

  if (!gst_buffer_pool_set_active (self->fallback_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't activate pool");
    gst_clear_object (&self->fallback_pool);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "New caps %" GST_PTR_FORMAT, in_caps);

  g_signal_emit (self, gst_d3d11_overlay_signals[SIGNAL_CAPS_CHANGED], 0,
      in_caps);

  return TRUE;
}
