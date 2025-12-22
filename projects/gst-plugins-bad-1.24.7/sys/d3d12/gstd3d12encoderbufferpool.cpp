/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
 *
 * This library is free software; you cln redistribute it and/or
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

#include "gstd3d12encoderbufferpool.h"
#include <directx/d3dx12.h>
#include <wrl.h>
#include <queue>
#include <mutex>
#include <condition_variable>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_encoder_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_d3d12_encoder_buffer_pool_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct _GstD3D12EncoderBuffer : public GstMiniObject
{
  GstD3D12EncoderBufferPool *pool = nullptr;
  ComPtr<ID3D12Resource> metadata;
  ComPtr<ID3D12Resource> resolved_metadata;
  ComPtr<ID3D12Resource> bitstream;
};

struct GstD3D12EncoderBufferPoolPrivate
{
  ~GstD3D12EncoderBufferPoolPrivate ()
  {
    while (!buffer_pool.empty ()) {
      auto buf = buffer_pool.front ();
      buffer_pool.pop ();
      gst_mini_object_unref (buf);
    }
  }

  ComPtr<ID3D12Device> device;

  std::mutex lock;
  std::condition_variable cond;
  std::queue<GstD3D12EncoderBuffer *>buffer_pool;

  guint metadata_size;
  guint resolved_metadata_size;
  guint bitstream_size;
  guint pool_size;
};
/* *INDENT-ON* */

struct _GstD3D12EncoderBufferPool
{
  GstObject parent;

  GstD3D12EncoderBufferPoolPrivate *priv;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstD3D12EncoderBuffer, gst_d3d12_encoder_buffer);

static void gst_d3d12_encoder_buffer_pool_finalize (GObject * object);

#define gst_d3d12_encoder_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstD3D12EncoderBufferPool,
    gst_d3d12_encoder_buffer_pool, GST_TYPE_OBJECT);

static void
gst_d3d12_encoder_buffer_pool_class_init (GstD3D12EncoderBufferPoolClass *
    klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_encoder_buffer_pool_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_encoder_buffer_pool_debug,
      "d3d12encoderbufferpool", 0, "d3d12encoderbufferpool");
}

static void
gst_d3d12_encoder_buffer_pool_init (GstD3D12EncoderBufferPool * self)
{
  self->priv = new GstD3D12EncoderBufferPoolPrivate ();
}

static void
gst_d3d12_encoder_buffer_pool_finalize (GObject * object)
{
  auto self = GST_D3D12_ENCODER_BUFFER_POOL (object);

  GST_DEBUG_OBJECT (self, "Finalize");

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_encoder_buffer_free (GstD3D12EncoderBuffer * buffer)
{
  delete buffer;
}

static GstD3D12EncoderBuffer *
gst_d3d12_encoder_buffer_pool_alloc (GstD3D12EncoderBufferPool * self)
{
  auto priv = self->priv;
  D3D12_HEAP_PROPERTIES prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC desc =
      CD3DX12_RESOURCE_DESC::Buffer (priv->metadata_size);

  ComPtr < ID3D12Resource > metadata;
  auto hr = priv->device->CreateCommittedResource (&prop,
      D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
      nullptr, IID_PPV_ARGS (&metadata));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't metadata buffer, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  prop = CD3DX12_HEAP_PROPERTIES (D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
      D3D12_MEMORY_POOL_L0);
  desc = CD3DX12_RESOURCE_DESC::Buffer (priv->resolved_metadata_size);
  ComPtr < ID3D12Resource > resolved_metadata;
  hr = priv->device->CreateCommittedResource (&prop,
      D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
      nullptr, IID_PPV_ARGS (&resolved_metadata));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't metadata buffer, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  desc = CD3DX12_RESOURCE_DESC::Buffer (priv->bitstream_size);
  ComPtr < ID3D12Resource > bitstream;
  hr = priv->device->CreateCommittedResource (&prop,
      D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
      nullptr, IID_PPV_ARGS (&bitstream));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't metadata buffer, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  auto new_buf = new GstD3D12EncoderBuffer ();

  gst_mini_object_init (new_buf, 0, gst_d3d12_encoder_buffer_get_type (),
      nullptr, nullptr,
      (GstMiniObjectFreeFunction) gst_d3d12_encoder_buffer_free);
  new_buf->metadata = metadata;
  new_buf->resolved_metadata = resolved_metadata;
  new_buf->bitstream = bitstream;

  return new_buf;
}

GstD3D12EncoderBufferPool *
gst_d3d12_encoder_buffer_pool_new (GstD3D12Device * device,
    guint metadata_size, guint resolved_metadata_size, guint bitstream_size,
    guint pool_size)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  auto self = (GstD3D12EncoderBufferPool *)
      g_object_new (GST_TYPE_D3D12_ENCODER_BUFFER_POOL, nullptr);
  gst_object_ref_sink (self);

  auto priv = self->priv;
  priv->device = gst_d3d12_device_get_device_handle (device);;
  priv->metadata_size = metadata_size;
  priv->resolved_metadata_size = resolved_metadata_size;
  priv->bitstream_size = bitstream_size;
  priv->pool_size = pool_size;

  for (guint i = 0; i < pool_size; i++) {
    auto new_buf = gst_d3d12_encoder_buffer_pool_alloc (self);
    if (!new_buf) {
      gst_object_unref (self);
      return nullptr;
    }

    priv->buffer_pool.push (new_buf);
  }

  return self;
}

static void
gst_d3d12_encoder_buffer_pool_release (GstD3D12EncoderBufferPool * pool,
    GstD3D12EncoderBuffer * buffer)
{
  auto priv = pool->priv;
  {
    std::lock_guard < std::mutex > lk (priv->lock);
    buffer->dispose = nullptr;
    buffer->pool = nullptr;
    priv->buffer_pool.push (buffer);
    priv->cond.notify_one ();
  }

  gst_object_unref (pool);
}

static gboolean
gst_d3d12_encoder_buffer_dispose (GstD3D12EncoderBuffer * buffer)
{
  if (!buffer->pool)
    return TRUE;

  gst_mini_object_ref (buffer);
  gst_d3d12_encoder_buffer_pool_release (buffer->pool, buffer);

  return FALSE;
}

gboolean
gst_d3d12_encoder_buffer_pool_acquire (GstD3D12EncoderBufferPool * pool,
    GstD3D12EncoderBuffer ** buffer)
{
  g_return_val_if_fail (GST_IS_D3D12_ENCODER_BUFFER_POOL (pool), FALSE);
  g_return_val_if_fail (buffer, FALSE);

  *buffer = nullptr;

  auto priv = pool->priv;
  GstD3D12EncoderBuffer *new_buf = nullptr;

  {
    std::unique_lock < std::mutex > lk (priv->lock);
    if (priv->pool_size > 0) {
      while (priv->buffer_pool.empty ())
        priv->cond.wait (lk);
    }

    if (!priv->buffer_pool.empty ()) {
      new_buf = priv->buffer_pool.front ();
      priv->buffer_pool.pop ();
    }
  }

  if (!new_buf)
    new_buf = gst_d3d12_encoder_buffer_pool_alloc (pool);

  if (!new_buf)
    return FALSE;

  new_buf->pool = (GstD3D12EncoderBufferPool *) gst_object_ref (pool);
  new_buf->dispose =
      (GstMiniObjectDisposeFunction) gst_d3d12_encoder_buffer_dispose;

  *buffer = new_buf;

  return TRUE;
}

GstD3D12EncoderBuffer *
gst_d3d12_encoder_buffer_ref (GstD3D12EncoderBuffer * buffer)
{
  return (GstD3D12EncoderBuffer *) gst_mini_object_ref (buffer);
}

void
gst_d3d12_encoder_buffer_unref (GstD3D12EncoderBuffer * buffer)
{
  gst_mini_object_unref (buffer);
}

void
gst_clear_d3d12_encoder_buffer (GstD3D12EncoderBuffer ** buffer)
{
  gst_clear_mini_object (buffer);
}

gboolean
gst_d3d12_encoder_buffer_get_metadata (GstD3D12EncoderBuffer * buffer,
    ID3D12Resource ** metadata)
{
  g_return_val_if_fail (buffer, FALSE);
  g_return_val_if_fail (metadata, FALSE);

  *metadata = buffer->metadata.Get ();
  (*metadata)->AddRef ();

  return TRUE;
}

gboolean
gst_d3d12_encoder_buffer_get_resolved_metadata (GstD3D12EncoderBuffer * buffer,
    ID3D12Resource ** resolved_metadata)
{
  g_return_val_if_fail (buffer, FALSE);
  g_return_val_if_fail (resolved_metadata, FALSE);

  *resolved_metadata = buffer->resolved_metadata.Get ();
  (*resolved_metadata)->AddRef ();

  return TRUE;
}

gboolean
gst_d3d12_encoder_buffer_get_bitstream (GstD3D12EncoderBuffer * buffer,
    ID3D12Resource ** bitstream)
{
  g_return_val_if_fail (buffer, FALSE);
  g_return_val_if_fail (bitstream, FALSE);

  *bitstream = buffer->bitstream.Get ();
  (*bitstream)->AddRef ();

  return TRUE;
}
