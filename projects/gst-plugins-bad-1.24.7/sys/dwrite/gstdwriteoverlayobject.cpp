/* GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/d3d11/gstd3d11-private.h>
#include "gstdwriteoverlayobject.h"
#include "gstdwritebitmappool.h"
#include "gstdwrite-renderer.h"
#include "gstdwrite-effect.h"
#include <wrl.h>
#include <mutex>

GST_DEBUG_CATEGORY_STATIC (dwrite_overlay_object_debug);
#define GST_CAT_DEFAULT dwrite_overlay_object_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct GstDWriteOverlayObjectPrivate
{
  GstDWriteOverlayObjectPrivate ()
  {
    gst_video_info_init (&info);
    gst_video_info_init (&bgra_info);
    gst_video_info_init (&layout_info);
  }

  ~GstDWriteOverlayObjectPrivate ()
  {
    ClearResource (true);
    gst_clear_caps (&outcaps);
    gst_clear_object (&device);
  }

  void ClearResource (bool hard)
  {
    if (hard)
      blend_mode = GstDWriteBlendMode::NOT_SUPPORTED;

    g_clear_pointer (&overlay_rect, gst_video_overlay_rectangle_unref);
    gst_clear_buffer (&layout_buf);
    layout = nullptr;

    if (layout_pool) {
      gst_buffer_pool_set_active (layout_pool, FALSE);
      gst_clear_object (&layout_pool);
    }

    if (blend_pool) {
      gst_buffer_pool_set_active (blend_pool, FALSE);
      gst_clear_object (&blend_pool);
    }

    gst_clear_object (&pre_conv);
    gst_clear_object (&blend_conv);
    gst_clear_object (&post_conv);
  }

  GstVideoInfo info;
  GstVideoInfo bgra_info;
  GstVideoInfo layout_info;

  GstD3D11Device *device = nullptr;
  GstCaps *outcaps = nullptr;

  ComPtr<ID2D1Factory> d2d_factory;
  ComPtr<IDWriteFactory> dwrite_factory;
  ComPtr<IDWriteTextLayout> layout;
  ComPtr<IGstDWriteTextRenderer> renderer;

  GstDWriteBlendMode blend_mode = GstDWriteBlendMode::NOT_SUPPORTED;

  GstBufferPool *layout_pool = nullptr;
  GstBufferPool *blend_pool = nullptr;
  GstBuffer *layout_buf = nullptr;
  /* Convert input texture to BGRA */
  GstD3D11Converter *pre_conv = nullptr;
  /* Blend converted BGRA texture with rendered text texture */
  GstD3D11Converter *blend_conv = nullptr;
  /* Convert blended texture to original format */
  GstD3D11Converter *post_conv = nullptr;
  GstVideoOverlayRectangle *overlay_rect = nullptr;

  gboolean is_d3d11 = FALSE;
  gboolean attach_meta = FALSE;
  gboolean use_bitmap = FALSE;

  std::recursive_mutex ctx_lock;
};
/* *INDENT-ON* */

struct _GstDWriteOverlayObject
{
  GstObject parent;

  GstDWriteOverlayObjectPrivate *priv;
};

static void gst_dwrite_overlay_object_finalize (GObject * object);

#define gst_dwrite_overlay_object_parent_class parent_class
G_DEFINE_TYPE (GstDWriteOverlayObject, gst_dwrite_overlay_object,
    GST_TYPE_OBJECT);

static void
gst_dwrite_overlay_object_class_init (GstDWriteOverlayObjectClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_dwrite_overlay_object_finalize;

  GST_DEBUG_CATEGORY_INIT (dwrite_overlay_object_debug,
      "dwriteoverlayobject", 0, "dwriteoverlayobject");
}

static void
gst_dwrite_overlay_object_init (GstDWriteOverlayObject * self)
{
  self->priv = new GstDWriteOverlayObjectPrivate ();
}

static void
gst_dwrite_overlay_object_finalize (GObject * object)
{
  GstDWriteOverlayObject *self = GST_DWRITE_OVERLAY_OBJECT (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dwrite_overlay_object_decide_blend_mode (GstDWriteOverlayObject * self)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;

  if (priv->attach_meta) {
    if (priv->is_d3d11)
      priv->blend_mode = GstDWriteBlendMode::ATTACH_TEXTURE;
    else
      priv->blend_mode = GstDWriteBlendMode::ATTACH_BITMAP;
    return;
  }

  if (!priv->is_d3d11) {
    priv->blend_mode = GstDWriteBlendMode::SW_BLEND;
    return;
  }

  /* Decide best blend mode to use based on format */
  switch (GST_VIDEO_INFO_FORMAT (&priv->info)) {
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
      /* Alpha aware formats */
      priv->blend_mode = GstDWriteBlendMode::BLEND;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBRA:
      /* 8bits formats */
      priv->blend_mode = GstDWriteBlendMode::CONVERT;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_AYUV64:
    case GST_VIDEO_FORMAT_GBR_10LE:
    case GST_VIDEO_FORMAT_GBR_12LE:
    case GST_VIDEO_FORMAT_GBRA_10LE:
    case GST_VIDEO_FORMAT_GBRA_12LE:
      /* high bitdept formats */
      priv->blend_mode = GstDWriteBlendMode::CONVERT_64;
      break;
    default:
      /* d3d11 blending is not supported, fallback to software blending */
      priv->blend_mode = GstDWriteBlendMode::SW_BLEND;
      break;
  }
}

static gboolean
is_subsampled_yuv (const GstVideoInfo * info)
{
  if (!GST_VIDEO_INFO_IS_YUV (info))
    return FALSE;

  for (guint i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    if (info->finfo->w_sub[i] != 0 || info->finfo->h_sub[i] != 0)
      return TRUE;
  }

  return FALSE;
}

static GstD3D11Converter *
gst_dwrite_overlay_object_create_converter (GstDWriteOverlayObject * self,
    const GstVideoInfo * in_info, const GstVideoInfo * out_info,
    gboolean is_blend)
{
  GstD3D11Converter *ret;
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstStructure *config;
  D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

  if (is_subsampled_yuv (in_info) || is_subsampled_yuv (out_info))
    filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

  config = gst_structure_new ("convert-config",
      GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
      GST_D3D11_CONVERTER_BACKEND_SHADER,
      GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER,
      GST_TYPE_D3D11_CONVERTER_SAMPLER_FILTER, filter, nullptr);
  if (is_blend) {
    gst_structure_set (config, GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE,
        GST_TYPE_D3D11_CONVERTER_ALPHA_MODE,
        GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED, nullptr);
  }

  ret = gst_d3d11_converter_new (priv->device, in_info, out_info, config);
  if (!ret)
    GST_ERROR_OBJECT (self, "Couldn't create converter");

  return ret;
}

static GstBufferPool *
gst_dwrite_overlay_object_create_d3d11_pool (GstDWriteOverlayObject * self,
    const GstVideoInfo * info)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstCaps *caps = nullptr;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  GstBufferPool *pool = nullptr;

  caps = gst_video_info_to_caps (info);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Couldn't create caps");
    return nullptr;
  }

  pool = gst_d3d11_buffer_pool_new (priv->device);
  config = gst_buffer_pool_get_config (pool);

  params = gst_d3d11_allocation_params_new (priv->device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, caps, info->size, 0, 0);
  gst_caps_unref (caps);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set pool config");
    goto error;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't activate pool");
    goto error;
  }

  return pool;

error:
  gst_clear_object (&pool);

  return nullptr;
}

static GstBufferPool *
gst_dwrite_overlay_object_create_bitmap_pool (GstDWriteOverlayObject * self,
    const GstVideoInfo * info)
{
  GstCaps *caps = nullptr;
  GstStructure *config;
  GstBufferPool *pool = nullptr;

  caps = gst_video_info_to_caps (info);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Couldn't create caps");
    return nullptr;
  }

  pool = gst_dwrite_bitmap_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, info->size, 0, 0);
  gst_caps_unref (caps);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set pool config");
    goto error;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't activate pool");
    goto error;
  }

  return pool;

error:
  gst_clear_object (&pool);

  return nullptr;
}

static gboolean
gst_dwrite_overlay_object_prepare_resource (GstDWriteOverlayObject * self)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;

  switch (priv->blend_mode) {
    case GstDWriteBlendMode::ATTACH_TEXTURE:
    case GstDWriteBlendMode::ATTACH_BITMAP:
    case GstDWriteBlendMode::SW_BLEND:
      /* Updated later */
      break;
    case GstDWriteBlendMode::BLEND:
      priv->blend_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->bgra_info, &priv->info, TRUE);
      if (!priv->blend_conv)
        return FALSE;
      break;
    case GstDWriteBlendMode::CONVERT:
      priv->blend_pool = gst_dwrite_overlay_object_create_d3d11_pool (self,
          &priv->bgra_info);
      if (!priv->blend_pool)
        return FALSE;

      priv->pre_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->info, &priv->bgra_info, FALSE);
      if (!priv->pre_conv)
        return FALSE;

      priv->blend_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->bgra_info, &priv->bgra_info, TRUE);
      if (!priv->blend_conv)
        return FALSE;

      priv->post_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->bgra_info, &priv->info, FALSE);
      if (!priv->blend_conv)
        return FALSE;
      break;
    case GstDWriteBlendMode::CONVERT_64:
    {
      GstVideoInfo blend_info;
      gst_video_info_set_format (&blend_info, GST_VIDEO_FORMAT_RGBA64_LE,
          priv->info.width, priv->info.height);

      priv->blend_pool = gst_dwrite_overlay_object_create_d3d11_pool (self,
          &blend_info);
      if (!priv->blend_pool)
        return FALSE;

      priv->pre_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->info, &blend_info, FALSE);
      if (!priv->pre_conv)
        return FALSE;

      priv->blend_conv = gst_dwrite_overlay_object_create_converter (self,
          &priv->bgra_info, &blend_info, TRUE);
      if (!priv->pre_conv)
        return FALSE;

      priv->post_conv = gst_dwrite_overlay_object_create_converter (self,
          &blend_info, &priv->info, FALSE);
      if (!priv->post_conv)
        return FALSE;

      break;
    }
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  if (priv->blend_conv) {
    D3D11_BLEND_DESC desc = { 0, };
    ComPtr < ID3D11BlendState > blend;
    ID3D11Device *device = gst_d3d11_device_get_device_handle (priv->device);
    HRESULT hr;

    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState (&desc, &blend);
    if (!gst_d3d11_result (hr, priv->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create blend state");
      return FALSE;
    }

    g_object_set (priv->blend_conv, "blend-state", blend.Get (), nullptr);
  }

  return TRUE;
}

static const gchar *
gst_dwrite_overlay_mode_to_string (GstDWriteBlendMode mode)
{
  switch (mode) {
    case GstDWriteBlendMode::NOT_SUPPORTED:
      return "not-supported";
    case GstDWriteBlendMode::ATTACH_TEXTURE:
      return "attach-texture";
    case GstDWriteBlendMode::ATTACH_BITMAP:
      return "attach-bitmap";
    case GstDWriteBlendMode::SW_BLEND:
      return "sw-blend";
    case GstDWriteBlendMode::BLEND:
      return "blend";
    case GstDWriteBlendMode::CONVERT:
      return "convert";
    case GstDWriteBlendMode::CONVERT_64:
      return "convert-64";
    default:
      break;
  }

  return "unknown";
}

GstDWriteOverlayObject *
gst_dwrite_overlay_object_new (void)
{
  GstDWriteOverlayObject *self;

  self = (GstDWriteOverlayObject *)
      g_object_new (GST_TYPE_DWRITE_OVERLAY_OBJECT, nullptr);
  gst_object_ref_sink (self);

  return self;
}

gboolean
gst_dwrite_overlay_object_start (GstDWriteOverlayObject * object,
    IDWriteFactory * dwrite_factory)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  HRESULT hr;
  ComPtr < ID2D1Factory > d2d_factory;

  hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
      IID_PPV_ARGS (&d2d_factory));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (object, "Couldn't create d2d factory");
    return FALSE;
  }

  priv->d2d_factory = d2d_factory;
  priv->dwrite_factory = dwrite_factory;
  IGstDWriteTextRenderer::CreateInstance (dwrite_factory, &priv->renderer);

  return TRUE;
}

gboolean
gst_dwrite_overlay_object_stop (GstDWriteOverlayObject * object)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;

  priv->ClearResource (true);
  priv->dwrite_factory = nullptr;
  priv->d2d_factory = nullptr;
  priv->renderer = nullptr;
  gst_clear_object (&priv->device);
  gst_clear_caps (&priv->outcaps);

  return TRUE;
}

void
gst_dwrite_overlay_object_set_context (GstDWriteOverlayObject * object,
    GstElement * elem, GstContext * context)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  std::lock_guard < std::recursive_mutex > lk (priv->ctx_lock);

  gst_d3d11_handle_set_context (elem, context, -1, &priv->device);
}

gboolean
gst_dwrite_overlay_object_handle_query (GstDWriteOverlayObject * object,
    GstElement * elem, GstQuery * query)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;

  if (GST_QUERY_TYPE (query) != GST_QUERY_CONTEXT)
    return FALSE;

  std::lock_guard < std::recursive_mutex > lk (priv->ctx_lock);
  if (gst_d3d11_handle_context_query (elem, query, priv->device))
    return TRUE;

  return FALSE;
}

gboolean
gst_dwrite_overlay_object_decide_allocation (GstDWriteOverlayObject * object,
    GstElement * elem, GstQuery * query)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  guint min, max, size;
  gboolean update_pool;
  GstCaps *caps = nullptr;
  GstVideoInfo info;
  GstBufferPool *pool = nullptr;
  GstCapsFeatures *features;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  guint bind_flags = 0;
  GstD3D11Format d3d11_format;

  GST_DEBUG_OBJECT (elem, "Decide allocation");

  gst_query_parse_allocation (query, &caps, nullptr);
  if (!caps) {
    GST_WARNING_OBJECT (elem, "Query without caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (elem, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
  if (!features || !gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    GST_DEBUG_OBJECT (elem, "Not a d3d11 memory");
    return TRUE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    min = max = 0;
    size = info.size;
  }

  if (pool) {
    std::lock_guard < std::recursive_mutex > lk (priv->ctx_lock);
    if (!gst_d3d11_ensure_element_data (elem, -1, &priv->device)) {
      GST_ERROR_OBJECT (elem, "Couldn't create deice");
      return FALSE;
    }

    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != priv->device)
        gst_clear_object (&pool);
    }
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (priv->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);

  gst_d3d11_device_get_format (priv->device, GST_VIDEO_INFO_FORMAT (&info),
      &d3d11_format);
  if ((d3d11_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0) {
    bind_flags |= D3D11_BIND_SHADER_RESOURCE;
  }

  if ((d3d11_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  }

  params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!params) {
    params = gst_d3d11_allocation_params_new (priv->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (params, bind_flags);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (elem, "Couldn't set config");
    gst_object_unref (pool);
    return FALSE;
  }

  /* Get updated size in case of d3d11 */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

gboolean
gst_dwrite_overlay_object_propose_allocation (GstDWriteOverlayObject * object,
    GstElement * elem, GstQuery * query)
{
  GstCaps *caps = nullptr;
  GstVideoInfo info;
  GstCapsFeatures *features;
  guint min, max, size;
  GstBufferPool *pool;
  GstD3D11BufferPool *dpool;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  guint bind_flags = 0;
  GstD3D11Format d3d11_format;

  GST_DEBUG_OBJECT (elem, "Propose allocation");

  gst_query_parse_allocation (query, &caps, nullptr);
  if (!caps) {
    GST_WARNING_OBJECT (elem, "Query without caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (elem, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
  if (!features || !gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    GST_DEBUG_OBJECT (elem, "Not a d3d11 memory");
    return TRUE;
  }

  if (gst_query_get_n_allocation_pools (query) == 0)
    return TRUE;

  gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
  if (!pool)
    return TRUE;

  if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
    gst_object_unref (pool);
    return TRUE;
  }

  dpool = GST_D3D11_BUFFER_POOL (pool);
  gst_d3d11_device_get_format (dpool->device, GST_VIDEO_INFO_FORMAT (&info),
      &d3d11_format);
  if ((d3d11_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0) {
    bind_flags |= D3D11_BIND_SHADER_RESOURCE;
  }

  if ((d3d11_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  }

  config = gst_buffer_pool_get_config (pool);
  params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!params) {
    params = gst_d3d11_allocation_params_new (dpool->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (params, bind_flags);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (elem, "Couldn't set config");
    gst_object_unref (pool);
    return FALSE;
  }

  gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}

gboolean
gst_dwrite_overlay_object_set_caps (GstDWriteOverlayObject * object,
    GstElement * elem, GstCaps * in_caps, GstCaps * out_caps,
    GstVideoInfo * info, GstDWriteBlendMode * selected_mode)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  gboolean is_system;
  GstCapsFeatures *features;

  *selected_mode = GstDWriteBlendMode::NOT_SUPPORTED;

  priv->ClearResource (true);
  gst_caps_replace (&priv->outcaps, out_caps);

  if (!gst_video_info_from_caps (info, in_caps)) {
    GST_WARNING_OBJECT (elem, "Invalid caps %" GST_PTR_FORMAT, in_caps);
    return FALSE;
  }

  if (!gst_video_info_from_caps (&priv->info, out_caps)) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, out_caps);
    return FALSE;
  }

  gst_video_info_set_format (&priv->bgra_info, GST_VIDEO_FORMAT_BGRA,
      priv->info.width, priv->info.height);

  features = gst_caps_get_features (out_caps, 0);
  priv->is_d3d11 = gst_caps_features_contains (features,
      GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);
  is_system = gst_caps_features_contains (features,
      GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
  priv->attach_meta = gst_caps_features_contains (features,
      GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

  if (priv->is_d3d11) {
    std::lock_guard < std::recursive_mutex > lk (priv->ctx_lock);
    if (!gst_d3d11_ensure_element_data (elem, -1, &priv->device)) {
      GST_ERROR_OBJECT (elem, "Couldn't create deice");
      return FALSE;
    }
  }

  if (!priv->is_d3d11 && !is_system && !priv->attach_meta) {
    GST_WARNING_OBJECT (elem,
        "Not d3d11/system memory without composition meta support");
    return TRUE;
  }

  gst_dwrite_overlay_object_decide_blend_mode (object);
  GST_INFO_OBJECT (elem, "Selected blend mode: %s",
      gst_dwrite_overlay_mode_to_string (priv->blend_mode));

  if (priv->blend_mode == GstDWriteBlendMode::SW_BLEND ||
      priv->blend_mode == GstDWriteBlendMode::ATTACH_BITMAP) {
    priv->use_bitmap = TRUE;
  } else {
    priv->use_bitmap = FALSE;
  }

  if (!gst_dwrite_overlay_object_prepare_resource (object)) {
    GST_ERROR_OBJECT (elem, "Couldn't prepare resource");
    priv->ClearResource (true);
    return FALSE;
  }

  *selected_mode = priv->blend_mode;
  return TRUE;
}

gboolean
gst_dwrite_overlay_object_update_device (GstDWriteOverlayObject * object,
    GstBuffer * buffer)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  GstMemory *mem;
  GstD3D11Memory *dmem;

  if (priv->blend_mode == GstDWriteBlendMode::NOT_SUPPORTED || priv->use_bitmap)
    return FALSE;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem))
    return FALSE;

  dmem = GST_D3D11_MEMORY_CAST (mem);
  std::lock_guard < std::recursive_mutex > lk (priv->ctx_lock);
  if (dmem->device == priv->device)
    return FALSE;

  GST_DEBUG_OBJECT (object, "Updating device");
  gst_clear_object (&priv->device);
  priv->device = (GstD3D11Device *) gst_object_ref (dmem->device);
  priv->ClearResource (false);
  gst_dwrite_overlay_object_prepare_resource (object);

  return TRUE;
}

static gboolean
gst_dwrite_overlay_object_upload_system (GstDWriteOverlayObject * self,
    GstBuffer * dst, GstBuffer * src)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstVideoFrame in_frame, out_frame;
  gboolean ret;

  GST_TRACE_OBJECT (self, "system copy");

  if (!gst_video_frame_map (&in_frame, &priv->info, src,
          (GstMapFlags) (GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF))) {
    GST_ERROR_OBJECT (self, "Couldn't map input frame");
    return FALSE;
  }

  if (!gst_video_frame_map (&out_frame, &priv->info, dst,
          (GstMapFlags) (GST_MAP_WRITE | GST_VIDEO_FRAME_MAP_FLAG_NO_REF))) {
    gst_video_frame_unmap (&in_frame);
    GST_ERROR_OBJECT (self, "Couldn't map output frame");
    return FALSE;
  }

  ret = gst_video_frame_copy (&out_frame, &in_frame);

  gst_video_frame_unmap (&in_frame);
  gst_video_frame_unmap (&out_frame);

  return ret;
}

static gboolean
gst_dwrite_overlay_object_upload_d3d11 (GstDWriteOverlayObject * self,
    GstBuffer * dst, GstBuffer * src)
{
  GST_TRACE_OBJECT (self, "d3d11 copy");

  for (guint i = 0; i < gst_buffer_n_memory (dst); i++) {
    GstMemory *dst_mem, *src_mem;
    GstD3D11Memory *dst_dmem, *src_dmem;
    GstMapInfo dst_info;
    GstMapInfo src_info;
    ID3D11Resource *dst_texture, *src_texture;
    ID3D11DeviceContext *device_context;
    GstD3D11Device *device;
    D3D11_BOX src_box = { 0, };
    D3D11_TEXTURE2D_DESC dst_desc, src_desc;
    guint dst_subidx, src_subidx;

    dst_mem = gst_buffer_peek_memory (dst, i);
    src_mem = gst_buffer_peek_memory (src, i);

    dst_dmem = (GstD3D11Memory *) dst_mem;
    src_dmem = (GstD3D11Memory *) src_mem;

    device = dst_dmem->device;

    gst_d3d11_memory_get_texture_desc (dst_dmem, &dst_desc);
    gst_d3d11_memory_get_texture_desc (src_dmem, &src_desc);

    device_context = gst_d3d11_device_get_device_context_handle (device);

    if (!gst_memory_map (dst_mem, &dst_info,
            (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (self, "Cannot map dst d3d11 memory");
      return FALSE;
    }

    if (!gst_memory_map (src_mem, &src_info,
            (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (self, "Cannot map src d3d11 memory");
      gst_memory_unmap (dst_mem, &dst_info);
      return FALSE;
    }

    dst_texture = (ID3D11Resource *) dst_info.data;
    src_texture = (ID3D11Resource *) src_info.data;

    /* src/dst texture size might be different if padding was used.
     * select smaller size */
    src_box.left = 0;
    src_box.top = 0;
    src_box.front = 0;
    src_box.back = 1;
    src_box.right = MIN (src_desc.Width, dst_desc.Width);
    src_box.bottom = MIN (src_desc.Height, dst_desc.Height);

    dst_subidx = gst_d3d11_memory_get_subresource_index (dst_dmem);
    src_subidx = gst_d3d11_memory_get_subresource_index (src_dmem);

    GstD3D11DeviceLockGuard lk (device);
    device_context->CopySubresourceRegion (dst_texture, dst_subidx, 0, 0, 0,
        src_texture, src_subidx, &src_box);

    gst_memory_unmap (src_mem, &src_info);
    gst_memory_unmap (dst_mem, &dst_info);
  }

  return TRUE;
}

GstFlowReturn
gst_dwrite_overlay_object_prepare_output (GstDWriteOverlayObject * object,
    GstBaseTransform * trans, gpointer trans_class, GstBuffer * inbuf,
    GstBuffer ** outbuf)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  GstMemory *mem = gst_buffer_peek_memory (inbuf, 0);
  GstFlowReturn ret;
  gboolean is_d3d11 = FALSE;
  gboolean upload_ret;

  /* attaching meta or software blending can be in-place processing */
  switch (priv->blend_mode) {
    case GstDWriteBlendMode::ATTACH_TEXTURE:
    case GstDWriteBlendMode::ATTACH_BITMAP:
    case GstDWriteBlendMode::SW_BLEND:
      goto inplace;
    default:
      break;
  }

  if (gst_is_d3d11_memory (mem)) {
    D3D11_TEXTURE2D_DESC desc;
    GstD3D11Memory *dmem;
    const guint bind_flags = (D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE);

    is_d3d11 = TRUE;

    dmem = GST_D3D11_MEMORY_CAST (mem);
    gst_d3d11_memory_get_texture_desc (dmem, &desc);

    /* Cannot write on decoder resource */
    if ((desc.BindFlags & D3D11_BIND_DECODER) == 0 &&
        (desc.BindFlags & bind_flags) == bind_flags) {
      goto inplace;
    }
  }

  /* Needs to allocate new buffer */
  ret = GST_BASE_TRANSFORM_CLASS (trans_class)->prepare_output_buffer (trans,
      inbuf, outbuf);
  if (ret != GST_FLOW_OK)
    return ret;

  if (is_d3d11) {
    upload_ret = gst_dwrite_overlay_object_upload_d3d11 (object,
        *outbuf, inbuf);
  } else {
    upload_ret = gst_dwrite_overlay_object_upload_system (object,
        *outbuf, inbuf);
  }

  if (!upload_ret) {
    gst_clear_buffer (outbuf);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;

inplace:
  if (gst_buffer_is_writable (inbuf))
    *outbuf = inbuf;
  else
    *outbuf = gst_buffer_copy (inbuf);

  return GST_FLOW_OK;
}

static gboolean
gst_dwrite_overlay_object_get_target_from_d3d11 (GstDWriteOverlayObject * self,
    GstMemory * mem, ID2D1RenderTarget ** target)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  ComPtr < IDXGISurface > surface;
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  ID3D11Resource *texture;
  HRESULT hr;
  static const D2D1_RENDER_TARGET_PROPERTIES props = {
    D2D1_RENDER_TARGET_TYPE_DEFAULT, DXGI_FORMAT_B8G8R8A8_UNORM,
    D2D1_ALPHA_MODE_PREMULTIPLIED, 0, 0, D2D1_RENDER_TARGET_USAGE_NONE,
    D2D1_FEATURE_LEVEL_DEFAULT
  };

  texture = gst_d3d11_memory_get_resource_handle (dmem);
  hr = texture->QueryInterface (IID_PPV_ARGS (&surface));
  if (!gst_d3d11_result (hr, priv->device))
    return FALSE;

  hr = priv->d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (), props,
      target);

  return gst_d3d11_result (hr, priv->device);
}

static gboolean
gst_dwrite_overlay_object_get_target_from_bitmap (GstDWriteOverlayObject * self,
    GstMemory * mem, ID2D1RenderTarget ** target)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstDWriteBitmapMemory *dmem = (GstDWriteBitmapMemory *) mem;
  HRESULT hr;
  static const D2D1_RENDER_TARGET_PROPERTIES props = {
    D2D1_RENDER_TARGET_TYPE_DEFAULT, DXGI_FORMAT_B8G8R8A8_UNORM,
    D2D1_ALPHA_MODE_PREMULTIPLIED, 0, 0, D2D1_RENDER_TARGET_USAGE_NONE,
    D2D1_FEATURE_LEVEL_DEFAULT
  };

  hr = priv->d2d_factory->CreateWicBitmapRenderTarget (dmem->bitmap, &props,
      target);

  return SUCCEEDED (hr);
}

static gboolean
gst_dwrite_overlay_object_draw_layout (GstDWriteOverlayObject * self,
    IDWriteTextLayout * layout, gint x, gint y)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  gint width, height;
  GstMemory *mem;
  ComPtr < ID2D1RenderTarget > target;
  GstMapInfo info;

  if (priv->layout_buf) {
    if (priv->layout && priv->layout.Get () == layout)
      return TRUE;

    gst_clear_buffer (&priv->layout_buf);
    g_clear_pointer (&priv->overlay_rect, gst_video_overlay_rectangle_unref);
  }

  priv->layout = nullptr;
  priv->layout = layout;

  if (priv->layout_buf)
    return TRUE;

  width = (gint) layout->GetMaxWidth ();
  height = (gint) layout->GetMaxHeight ();

  if (priv->layout_pool &&
      (priv->layout_info.width != width
          || priv->layout_info.height != height)) {
    gst_buffer_pool_set_active (priv->layout_pool, FALSE);
    gst_clear_object (&priv->layout_pool);
  }

  if (!priv->layout_pool) {
    gst_video_info_set_format (&priv->layout_info, GST_VIDEO_FORMAT_BGRA,
        width, height);
    if (priv->use_bitmap) {
      priv->layout_pool = gst_dwrite_overlay_object_create_bitmap_pool (self,
          &priv->layout_info);
    } else {
      priv->layout_pool = gst_dwrite_overlay_object_create_d3d11_pool (self,
          &priv->layout_info);
    }
  }

  if (!priv->layout_pool)
    return FALSE;

  gst_buffer_pool_acquire_buffer (priv->layout_pool,
      &priv->layout_buf, nullptr);
  if (!priv->layout_buf) {
    GST_ERROR_OBJECT (self, "Couldn't acquire layout buffer");
    return FALSE;
  }

  mem = gst_buffer_peek_memory (priv->layout_buf, 0);
  if (priv->use_bitmap) {
    if (!gst_dwrite_overlay_object_get_target_from_bitmap (self, mem, &target)) {
      GST_ERROR_OBJECT (self, "Couldn't get target from bitmap");
      gst_clear_buffer (&priv->layout_buf);
      return FALSE;
    }
  } else {
    if (!gst_memory_map (mem, &info,
            (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (self, "Could not map buffer");
      gst_clear_buffer (&priv->layout_buf);
      return FALSE;
    }

    if (!gst_dwrite_overlay_object_get_target_from_d3d11 (self, mem, &target)) {
      GST_ERROR_OBJECT (self, "Couldn't get target from texture");
      gst_memory_unmap (mem, &info);
      gst_clear_buffer (&priv->layout_buf);
      return FALSE;
    }
  }

  target->BeginDraw ();
  target->Clear (D2D1::ColorF (D2D1::ColorF::Black, 0.0));
  priv->renderer->Draw (D2D1::Point2F (),
      D2D1::Rect (0, 0, width, height), layout, target.Get ());
  target->EndDraw ();

  /* Release render target before unmapping. Otherwise pending GPU operations
   * can be executed after releasing keyed-mutex, if texture was allocated with
   * keyed-mutex enabled */
  target = nullptr;

  if (!priv->use_bitmap)
    gst_memory_unmap (mem, &info);

  priv->overlay_rect = gst_video_overlay_rectangle_new_raw (priv->layout_buf,
      x, y, width, height, GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

  return TRUE;
}

static gboolean
gst_dwrite_overlay_object_mode_attach (GstDWriteOverlayObject * self,
    GstBuffer * buffer)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstVideoOverlayCompositionMeta *meta;

  meta = gst_buffer_get_video_overlay_composition_meta (buffer);
  if (meta) {
    if (meta->overlay) {
      meta->overlay =
          gst_video_overlay_composition_make_writable (meta->overlay);
      gst_video_overlay_composition_add_rectangle (meta->overlay,
          priv->overlay_rect);
    } else {
      meta->overlay = gst_video_overlay_composition_new (priv->overlay_rect);
    }
  } else {
    GstVideoOverlayComposition *comp =
        gst_video_overlay_composition_new (priv->overlay_rect);
    meta = gst_buffer_add_video_overlay_composition_meta (buffer, comp);
    gst_video_overlay_composition_unref (comp);
  }

  return TRUE;
}

static gboolean
gst_dwrite_overlay_mode_sw_blend (GstDWriteOverlayObject * self,
    GstBuffer * buffer, gint x, gint y)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstVideoFrame dst_frame, src_frame;
  gboolean ret;

  if (!gst_video_frame_map (&dst_frame, &priv->info, buffer,
          (GstMapFlags) (GST_MAP_WRITE | GST_VIDEO_FRAME_MAP_FLAG_NO_REF))) {
    GST_ERROR_OBJECT (self, "Couldn't map input buffer");
    return FALSE;
  }

  if (!gst_video_frame_map (&src_frame, &priv->layout_info, priv->layout_buf,
          (GstMapFlags) (GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF))) {
    gst_video_frame_unmap (&dst_frame);
    GST_ERROR_OBJECT (self, "Couldn't map text buffer");
    return FALSE;
  }

  src_frame.info.flags = (GstVideoFlags)
      (src_frame.info.flags | GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA);
  ret = gst_video_blend (&dst_frame, &src_frame, x, y, 1.0);
  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dst_frame);

  return ret;
}

static gboolean
gst_dwrite_overlay_mode_blend (GstDWriteOverlayObject * self,
    GstBuffer * buffer, gint x, gint y)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;

  g_object_set (priv->blend_conv, "src-width", priv->layout_info.width,
      "src-height", priv->layout_info.height,
      "dest-x", x, "dest-y", y, "dest-width", priv->layout_info.width,
      "dest-height", priv->layout_info.height, nullptr);

  return gst_d3d11_converter_convert_buffer (priv->blend_conv,
      priv->layout_buf, buffer);
}

static gboolean
gst_dwrite_overlay_mode_convert (GstDWriteOverlayObject * self,
    GstBuffer * buffer, gint x, gint y)
{
  GstDWriteOverlayObjectPrivate *priv = self->priv;
  GstBuffer *pre_buf = nullptr;

  g_object_set (priv->blend_conv, "src-width", priv->layout_info.width,
      "src-height", priv->layout_info.height,
      "dest-x", x, "dest-y", y, "dest-width", priv->layout_info.width,
      "dest-height", priv->layout_info.height, nullptr);

  gst_buffer_pool_acquire_buffer (priv->blend_pool, &pre_buf, nullptr);
  if (!pre_buf) {
    GST_ERROR_OBJECT (self, "Couldn't acquire preconv buffer");
    return FALSE;
  }

  if (!gst_d3d11_converter_convert_buffer_unlocked (priv->pre_conv,
          buffer, pre_buf)) {
    GST_ERROR_OBJECT (self, "pre-convert failed");
    goto error;
  }

  if (!gst_d3d11_converter_convert_buffer_unlocked (priv->blend_conv,
          priv->layout_buf, pre_buf)) {
    GST_ERROR_OBJECT (self, "blend-convert failed");
    goto error;
  }

  if (!gst_d3d11_converter_convert_buffer_unlocked (priv->post_conv,
          pre_buf, buffer)) {
    GST_ERROR_OBJECT (self, "post-convert failed");
    goto error;
  }

  gst_buffer_unref (pre_buf);

  return TRUE;

error:
  gst_clear_buffer (&pre_buf);
  return FALSE;
}

gboolean
gst_dwrite_overlay_object_draw (GstDWriteOverlayObject * object,
    GstBuffer * buffer, IDWriteTextLayout * layout, gint x, gint y)
{
  GstDWriteOverlayObjectPrivate *priv = object->priv;
  gboolean ret = FALSE;

  if (priv->device)
    gst_d3d11_device_lock (priv->device);

  if (!gst_dwrite_overlay_object_draw_layout (object, layout, x, y))
    goto out;

  switch (priv->blend_mode) {
    case GstDWriteBlendMode::ATTACH_TEXTURE:
    case GstDWriteBlendMode::ATTACH_BITMAP:
      ret = gst_dwrite_overlay_object_mode_attach (object, buffer);
      break;
    case GstDWriteBlendMode::SW_BLEND:
      ret = gst_dwrite_overlay_mode_sw_blend (object, buffer, x, y);
      break;
    case GstDWriteBlendMode::BLEND:
      ret = gst_dwrite_overlay_mode_blend (object, buffer, x, y);
      break;
    case GstDWriteBlendMode::CONVERT:
    case GstDWriteBlendMode::CONVERT_64:
      ret = gst_dwrite_overlay_mode_convert (object, buffer, x, y);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

out:
  if (priv->device)
    gst_d3d11_device_unlock (priv->device);
  return ret;
}
