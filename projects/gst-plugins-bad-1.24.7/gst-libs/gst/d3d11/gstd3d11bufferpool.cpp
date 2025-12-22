/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d11bufferpool.h"
#include "gstd3d11memory.h"
#include "gstd3d11memory-private.h"
#include "gstd3d11device.h"
#include "gstd3d11utils.h"
#include "gstd3d11-private.h"

#include <string.h>

/**
 * SECTION:gstd3d11bufferpool
 * @title: GstD3D11BufferPool
 * @short_description: buffer pool for GstD3D11Memory object
 * @see_also: #GstBufferPool, #GstD3D11Memory
 *
 * This GstD3D11BufferPool is an object that allocates buffers
 * with #GstD3D11Memory
 *
 * Since: 1.22
 */

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_d3d11_buffer_pool_debug

struct _GstD3D11BufferPoolPrivate
{
  GstD3D11Allocator *alloc[GST_VIDEO_MAX_PLANES];

  GstD3D11AllocationParams *d3d11_params;

  gint stride[GST_VIDEO_MAX_PLANES];
  gsize offset[GST_VIDEO_MAX_PLANES];
};

#define gst_d3d11_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstD3D11BufferPool,
    gst_d3d11_buffer_pool, GST_TYPE_BUFFER_POOL);

static void gst_d3d11_buffer_pool_dispose (GObject * object);
static const gchar **gst_d3d11_buffer_pool_get_options (GstBufferPool * pool);
static gboolean gst_d3d11_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_d3d11_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static gboolean gst_d3d11_buffer_pool_start (GstBufferPool * pool);
static gboolean gst_d3d11_buffer_pool_stop (GstBufferPool * pool);

static void
gst_d3d11_buffer_pool_class_init (GstD3D11BufferPoolClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bufferpool_class = GST_BUFFER_POOL_CLASS (klass);

  gobject_class->dispose = gst_d3d11_buffer_pool_dispose;

  bufferpool_class->get_options = gst_d3d11_buffer_pool_get_options;
  bufferpool_class->set_config = gst_d3d11_buffer_pool_set_config;
  bufferpool_class->alloc_buffer = gst_d3d11_buffer_pool_alloc_buffer;
  bufferpool_class->start = gst_d3d11_buffer_pool_start;
  bufferpool_class->stop = gst_d3d11_buffer_pool_stop;

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_buffer_pool_debug, "d3d11bufferpool", 0,
      "d3d11bufferpool object");
}

static void
gst_d3d11_buffer_pool_init (GstD3D11BufferPool * self)
{
  self->priv = (GstD3D11BufferPoolPrivate *)
      gst_d3d11_buffer_pool_get_instance_private (self);
}

static void
gst_d3d11_buffer_pool_clear_allocator (GstD3D11BufferPool * self)
{
  GstD3D11BufferPoolPrivate *priv = self->priv;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    if (priv->alloc[i]) {
      gst_d3d11_allocator_set_active (priv->alloc[i], FALSE);
      gst_clear_object (&priv->alloc[i]);
    }
  }
}

static void
gst_d3d11_buffer_pool_dispose (GObject * object)
{
  GstD3D11BufferPool *self = GST_D3D11_BUFFER_POOL (object);
  GstD3D11BufferPoolPrivate *priv = self->priv;

  g_clear_pointer (&priv->d3d11_params, gst_d3d11_allocation_params_free);
  gst_clear_object (&self->device);
  gst_d3d11_buffer_pool_clear_allocator (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static const gchar **
gst_d3d11_buffer_pool_get_options (GstBufferPool * pool)
{
  /* NOTE: d3d11 memory does not support alignment */
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META, NULL };

  return options;
}

static gboolean
gst_d3d11_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstD3D11BufferPool *self = GST_D3D11_BUFFER_POOL (pool);
  GstD3D11BufferPoolPrivate *priv = self->priv;
  GstVideoInfo info;
  GstCaps *caps = NULL;
  guint min_buffers, max_buffers;
  gboolean ret = TRUE;
  D3D11_TEXTURE2D_DESC desc[GST_VIDEO_MAX_PLANES];
  const GstD3D11Format *format;
  GstD3D11AllocationParams *params;
  gsize offset = 0;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, &min_buffers,
          &max_buffers))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps))
    goto wrong_caps;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  gst_d3d11_buffer_pool_clear_allocator (self);

  memset (priv->stride, 0, sizeof (priv->stride));
  memset (priv->offset, 0, sizeof (priv->offset));

  if (priv->d3d11_params)
    gst_d3d11_allocation_params_free (priv->d3d11_params);
  priv->d3d11_params =
      gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!priv->d3d11_params) {
    /* allocate memory with resource format by default */
    priv->d3d11_params =
        gst_d3d11_allocation_params_new (self->device,
        &info, GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);
  }

  params = priv->d3d11_params;
  memset (desc, 0, sizeof (desc));
  if (params->d3d11_format.dxgi_format != DXGI_FORMAT_UNKNOWN) {
    desc[0].Width = GST_VIDEO_INFO_WIDTH (&params->aligned_info);
    desc[0].Height = GST_VIDEO_INFO_HEIGHT (&params->aligned_info);
    desc[0].Format = params->d3d11_format.dxgi_format;
    desc[0].MipLevels = 1;
    desc[0].ArraySize = params->array_size;
    desc[0].SampleDesc.Count = params->sample_count;
    desc[0].SampleDesc.Quality = params->sample_quality;
    desc[0].BindFlags = params->bind_flags;
    desc[0].MiscFlags = params->misc_flags;

    switch (params->d3d11_format.dxgi_format) {
      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_P010:
      case DXGI_FORMAT_P016:
        desc[0].Width = GST_ROUND_UP_2 (desc[0].Width);
        desc[0].Height = GST_ROUND_UP_2 (desc[0].Height);
        break;
      case DXGI_FORMAT_Y210:
      case DXGI_FORMAT_Y216:
        desc[0].Width = GST_ROUND_UP_2 (desc[0].Width);
        break;
      default:
        break;
    }
  } else {
    for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
      if (params->d3d11_format.resource_format[i] == DXGI_FORMAT_UNKNOWN)
        break;

      desc[i].Width = GST_VIDEO_INFO_COMP_WIDTH (&params->aligned_info, i);
      desc[i].Height = GST_VIDEO_INFO_COMP_HEIGHT (&params->aligned_info, i);
      desc[i].Format = params->d3d11_format.resource_format[i];
      desc[i].MipLevels = 1;
      desc[i].ArraySize = params->array_size;
      desc[i].SampleDesc.Count = params->sample_count;
      desc[i].SampleDesc.Quality = params->sample_quality;
      desc[i].BindFlags = params->bind_flags;
      desc[i].MiscFlags = params->misc_flags;
    }
  }

  if (params->array_size > 1)
    max_buffers = params->array_size;

  offset = 0;
  for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    GstD3D11Allocator *alloc;
    GstD3D11PoolAllocator *pool_alloc;
    GstFlowReturn flow_ret;
    GstMemory *mem = NULL;
    guint stride = 0;

    if (desc[i].Format == DXGI_FORMAT_UNKNOWN)
      break;

    alloc =
        (GstD3D11Allocator *) gst_d3d11_pool_allocator_new (self->device,
        &desc[i]);
    if (!gst_d3d11_allocator_set_active (alloc, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to activate allocator");
      gst_object_unref (alloc);
      return FALSE;
    }

    pool_alloc = GST_D3D11_POOL_ALLOCATOR (alloc);
    flow_ret = gst_d3d11_pool_allocator_acquire_memory (pool_alloc, &mem);
    gst_d3d11_allocator_set_active (alloc, FALSE);
    if (flow_ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Failed to allocate initial memory");
      gst_object_unref (alloc);
      return FALSE;
    }

    if (!gst_d3d11_memory_get_resource_stride (GST_D3D11_MEMORY_CAST (mem),
            &stride) || stride < desc[i].Width) {
      GST_ERROR_OBJECT (self, "Failed to calculate stride");

      gst_object_unref (alloc);
      gst_memory_unref (mem);

      return FALSE;
    }

    priv->stride[i] = stride;
    priv->offset[i] = offset;
    offset += mem->size;

    priv->alloc[i] = alloc;

    gst_memory_unref (mem);
  }

  format = &params->d3d11_format;
  /* single texture semi-planar formats */
  if (format->dxgi_format != DXGI_FORMAT_UNKNOWN &&
      GST_VIDEO_INFO_N_PLANES (&info) == 2) {
    priv->stride[1] = priv->stride[0];
    priv->offset[1] = priv->stride[0] * desc[0].Height;
  }

  gst_buffer_pool_config_set_params (config,
      caps, offset, min_buffers, max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config) && ret;

  /* ERRORS */
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
}

static GstFlowReturn
gst_d3d11_buffer_pool_fill_buffer (GstD3D11BufferPool * self, GstBuffer * buf)
{
  GstD3D11BufferPoolPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstMemory *mem = NULL;
    GstD3D11PoolAllocator *alloc = GST_D3D11_POOL_ALLOCATOR (priv->alloc[i]);

    if (!alloc)
      break;

    ret = gst_d3d11_pool_allocator_acquire_memory (alloc, &mem);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to acquire memory, ret %s",
          gst_flow_get_name (ret));
      return ret;
    }

    gst_buffer_append_memory (buf, mem);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d11_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstD3D11BufferPool *self = GST_D3D11_BUFFER_POOL (pool);
  GstD3D11BufferPoolPrivate *priv = self->priv;
  GstD3D11AllocationParams *d3d11_params = priv->d3d11_params;
  GstVideoInfo *info = &d3d11_params->info;
  GstBuffer *buf;
  GstFlowReturn ret = GST_FLOW_OK;

  buf = gst_buffer_new ();
  ret = gst_d3d11_buffer_pool_fill_buffer (self, buf);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (buf);
    return ret;
  }

  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
      GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
      priv->offset, priv->stride);

  *buffer = buf;

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_buffer_pool_start (GstBufferPool * pool)
{
  GstD3D11BufferPool *self = GST_D3D11_BUFFER_POOL (pool);
  GstD3D11BufferPoolPrivate *priv = self->priv;
  guint i;

  GST_DEBUG_OBJECT (self, "Start");

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstD3D11Allocator *alloc = priv->alloc[i];

    if (!alloc)
      break;

    if (!gst_d3d11_allocator_set_active (alloc, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to activate allocator");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_d3d11_buffer_pool_stop (GstBufferPool * pool)
{
  GstD3D11BufferPool *self = GST_D3D11_BUFFER_POOL (pool);
  GstD3D11BufferPoolPrivate *priv = self->priv;
  guint i;

  GST_DEBUG_OBJECT (self, "Stop");

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstD3D11Allocator *alloc = priv->alloc[i];

    if (!alloc)
      break;

    if (!gst_d3d11_allocator_set_active (alloc, FALSE)) {
      GST_ERROR_OBJECT (self, "Failed to deactivate allocator");
      return FALSE;
    }
  }

  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

/**
 * gst_d3d11_buffer_pool_new:
 * @device: a #GstD3D11Device to use
 *
 * Returns: a #GstBufferPool that allocates buffers with #GstD3D11Memory
 *
 * Since: 1.22
 */
GstBufferPool *
gst_d3d11_buffer_pool_new (GstD3D11Device * device)
{
  GstD3D11BufferPool *pool;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  pool = (GstD3D11BufferPool *) g_object_new (GST_TYPE_D3D11_BUFFER_POOL, NULL);
  gst_object_ref_sink (pool);

  pool->device = (GstD3D11Device *) gst_object_ref (device);

  return GST_BUFFER_POOL_CAST (pool);
}

/**
 * gst_buffer_pool_config_get_d3d11_allocation_params:
 * @config: a buffer pool config
 *
 * Returns: (transfer full) (nullable): the currently configured
 * #GstD3D11AllocationParams on @config or %NULL if @config doesn't contain
 * #GstD3D11AllocationParams
 *
 * Since: 1.22
 */
GstD3D11AllocationParams *
gst_buffer_pool_config_get_d3d11_allocation_params (GstStructure * config)
{
  GstD3D11AllocationParams *ret;

  if (!gst_structure_get (config, "d3d11-allocation-params",
          GST_TYPE_D3D11_ALLOCATION_PARAMS, &ret, NULL))
    ret = NULL;

  return ret;
}

/**
 * gst_buffer_pool_config_set_d3d11_allocation_params:
 * @config: a buffer pool config
 * @params: (transfer none): a #GstD3D11AllocationParams
 *
 * Sets @params on @config
 *
 * Since: 1.22
 */
void
gst_buffer_pool_config_set_d3d11_allocation_params (GstStructure * config,
    GstD3D11AllocationParams * params)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (params != NULL);

  gst_structure_set (config, "d3d11-allocation-params",
      GST_TYPE_D3D11_ALLOCATION_PARAMS, params, NULL);
}
