/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d12.h"
#include <wrl.h>
#include <queue>
#include <mutex>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_descriptor_pool_debug);
#define GST_CAT_DEFAULT gst_d3d12_descriptor_pool_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct _GstD3D12Descriptor : public GstMiniObject
{
  GstD3D12DescriptorPool *pool = nullptr;
  ComPtr < ID3D12DescriptorHeap > heap;
};

struct GstD3D12DescriptorPoolPrivate
{
  ~GstD3D12DescriptorPoolPrivate ()
  {
    while (!heap_pool.empty ()) {
      auto desc = heap_pool.front ();
      heap_pool.pop ();
      gst_mini_object_unref (desc);
    }
  }

  ComPtr<ID3D12Device> device;

  std::mutex lock;
  std::queue<GstD3D12Descriptor *>heap_pool;
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc;
};
/* *INDENT-ON* */

struct _GstD3D12DescriptorPool
{
  GstObject parent;

  GstD3D12DescriptorPoolPrivate *priv;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstD3D12Descriptor, gst_d3d12_descriptor);

static void gst_d3d12_descriptor_pool_finalize (GObject * object);

#define gst_d3d12_descriptor_pool_parent_class parent_class
G_DEFINE_TYPE (GstD3D12DescriptorPool,
    gst_d3d12_descriptor_pool, GST_TYPE_OBJECT);

static void
gst_d3d12_descriptor_pool_class_init (GstD3D12DescriptorPoolClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_descriptor_pool_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_descriptor_pool_debug,
      "d3d12descriptorpool", 0, "d3d12descriptorpool");
}

static void
gst_d3d12_descriptor_pool_init (GstD3D12DescriptorPool * self)
{
  self->priv = new GstD3D12DescriptorPoolPrivate ();
}

static void
gst_d3d12_descriptor_pool_finalize (GObject * object)
{
  auto self = GST_D3D12_DESCRIPTOR_POOL (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstD3D12DescriptorPool *
gst_d3d12_descriptor_pool_new (GstD3D12Device * device,
    const D3D12_DESCRIPTOR_HEAP_DESC * desc)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);
  g_return_val_if_fail (desc, nullptr);

  auto self = (GstD3D12DescriptorPool *)
      g_object_new (GST_TYPE_D3D12_DESCRIPTOR_POOL, nullptr);
  gst_object_ref_sink (self);

  auto priv = self->priv;
  priv->device = gst_d3d12_device_get_device_handle (device);
  priv->heap_desc = *desc;

  return self;
}

static void
gst_d3d12_descriptor_pool_release (GstD3D12DescriptorPool * pool,
    GstD3D12Descriptor * desc)
{
  auto priv = pool->priv;
  {
    std::lock_guard < std::mutex > lk (priv->lock);
    desc->dispose = nullptr;
    desc->pool = nullptr;
    priv->heap_pool.push (desc);
  }

  gst_object_unref (pool);
}

static gboolean
gst_d3d12_descriptor_dispose (GstD3D12Descriptor * desc)
{
  if (!desc->pool)
    return TRUE;

  gst_mini_object_ref (desc);
  gst_d3d12_descriptor_pool_release (desc->pool, desc);

  return FALSE;
}

static void
gst_d3d12_descriptor_free (GstD3D12Descriptor * desc)
{
  delete desc;
}

static GstD3D12Descriptor *
gst_d3d12_descriptor_new (ID3D12DescriptorHeap * heap)
{
  auto desc = new GstD3D12Descriptor ();
  desc->heap = heap;

  gst_mini_object_init (desc, 0, gst_d3d12_descriptor_get_type (),
      nullptr, nullptr, (GstMiniObjectFreeFunction) gst_d3d12_descriptor_free);

  return desc;
}

gboolean
gst_d3d12_descriptor_pool_acquire (GstD3D12DescriptorPool * pool,
    GstD3D12Descriptor ** desc)
{
  g_return_val_if_fail (GST_IS_D3D12_DESCRIPTOR_POOL (pool), FALSE);
  g_return_val_if_fail (desc, FALSE);

  *desc = nullptr;

  auto priv = pool->priv;
  GstD3D12Descriptor *new_desc = nullptr;
  HRESULT hr;

  {
    std::lock_guard < std::mutex > lk (priv->lock);
    if (!priv->heap_pool.empty ()) {
      new_desc = priv->heap_pool.front ();
      priv->heap_pool.pop ();
    }
  }

  if (!new_desc) {
    ComPtr < ID3D12DescriptorHeap > heap;
    hr = priv->device->CreateDescriptorHeap (&priv->heap_desc,
        IID_PPV_ARGS (&heap));
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (pool, "Couldn't create descriptor heap, hr: 0x%x",
          (guint) hr);
      return FALSE;
    }

    new_desc = gst_d3d12_descriptor_new (heap.Get ());
  }

  new_desc->pool = (GstD3D12DescriptorPool *) gst_object_ref (pool);
  new_desc->dispose =
      (GstMiniObjectDisposeFunction) gst_d3d12_descriptor_dispose;

  *desc = new_desc;

  return TRUE;
}

GstD3D12Descriptor *
gst_d3d12_descriptor_ref (GstD3D12Descriptor * desc)
{
  return (GstD3D12Descriptor *) gst_mini_object_ref (desc);
}

void
gst_d3d12_descriptor_unref (GstD3D12Descriptor * desc)
{
  gst_mini_object_unref (desc);
}

void
gst_clear_d3d12_descriptor (GstD3D12Descriptor ** desc)
{
  gst_clear_mini_object (desc);
}

gboolean
gst_d3d12_descriptor_get_handle (GstD3D12Descriptor * desc,
    ID3D12DescriptorHeap ** heap)
{
  g_return_val_if_fail (desc, FALSE);
  g_return_val_if_fail (heap, FALSE);

  *heap = desc->heap.Get ();
  (*heap)->AddRef ();

  return TRUE;
}
