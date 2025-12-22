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
#include "gstd3d12-private.h"
#include <wrl.h>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_command_queue_debug);
#define GST_CAT_DEFAULT gst_d3d12_command_queue_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct GCData
{
  GCData (gpointer user_data, GDestroyNotify destroy_notify,
      guint64 fence_value) : data (user_data),
          notify(destroy_notify), fence_val (fence_value) {}
  ~GCData ()
  {
    if (notify)
      notify (data);
  }

  gpointer data = nullptr;
  GDestroyNotify notify = nullptr;
  guint64 fence_val = 0;
};

typedef std::shared_ptr<GCData> GCDataPtr;

struct GstD3D12CommandQueuePrivate
{
  GstD3D12CommandQueuePrivate ()
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
  }

  ~GstD3D12CommandQueuePrivate ()
  {
    {
      std::lock_guard <std::mutex> lk (lock);
      shutdown = true;
      cond.notify_one ();
    }

    g_clear_pointer (&gc_thread, g_thread_join);

    auto completed = fence->GetCompletedValue ();
    if (fence_val > completed) {
      auto hr = fence->SetEventOnCompletion (completed, event_handle);
      if (SUCCEEDED (hr))
        WaitForSingleObjectEx (event_handle, INFINITE, FALSE);
    }

    CloseHandle (event_handle);
  }

  struct gc_cmp {
    bool operator()(const GCDataPtr &a, const GCDataPtr &b)
    {
      return a->fence_val > b->fence_val;
    }
  };

  D3D12_COMMAND_QUEUE_DESC desc;

  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> cq;
  ComPtr<ID3D12Fence> fence;
  HANDLE event_handle;
  guint64 fence_val = 0;

  GThread *gc_thread = nullptr;
  std::priority_queue<GCDataPtr, std::vector<GCDataPtr>, gc_cmp> gc_list;

  std::mutex execute_lock;
  std::mutex lock;
  std::condition_variable cond;
  bool shutdown = false;
  size_t queue_size = 0;
};
/* *INDENT-ON* */

struct _GstD3D12CommandQueue
{
  GstObject parent;

  GstD3D12CommandQueuePrivate *priv;
};

static void gst_d3d12_command_queue_finalize (GObject * object);

#define gst_d3d12_command_queue_parent_class parent_class
G_DEFINE_TYPE (GstD3D12CommandQueue, gst_d3d12_command_queue, GST_TYPE_OBJECT);

static void
gst_d3d12_command_queue_class_init (GstD3D12CommandQueueClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_command_queue_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_command_queue_debug,
      "d3d12commandqueue", 0, "d3d12commandqueue");
}

static void
gst_d3d12_command_queue_init (GstD3D12CommandQueue * self)
{
  self->priv = new GstD3D12CommandQueuePrivate ();
}

static void
gst_d3d12_command_queue_finalize (GObject * object)
{
  auto self = GST_D3D12_COMMAND_QUEUE (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstD3D12CommandQueue *
gst_d3d12_command_queue_new (GstD3D12Device * device,
    const D3D12_COMMAND_QUEUE_DESC * desc, guint queue_size)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);
  g_return_val_if_fail (desc, nullptr);

  auto device_handle = gst_d3d12_device_get_device_handle (device);

  ComPtr < ID3D12CommandQueue > cq;
  auto hr = device_handle->CreateCommandQueue (desc, IID_PPV_ARGS (&cq));
  if (!gst_d3d12_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Couldn't create command queue");
    return nullptr;
  }

  ComPtr < ID3D12Fence > fence;
  hr = device_handle->CreateFence (0,
      D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&fence));
  if (!gst_d3d12_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Couldn't create fence");
    return nullptr;
  }

  auto self = (GstD3D12CommandQueue *)
      g_object_new (GST_TYPE_D3D12_COMMAND_QUEUE, nullptr);
  gst_object_ref_sink (self);

  auto priv = self->priv;
  priv->device = gst_d3d12_device_get_device_handle (device);
  priv->cq = cq;
  priv->fence = fence;
  priv->queue_size = queue_size;

  return self;
}

gboolean
gst_d3d12_command_queue_get_handle (GstD3D12CommandQueue * queue,
    ID3D12CommandQueue ** handle)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), FALSE);
  g_return_val_if_fail (handle, FALSE);

  auto priv = queue->priv;

  *handle = priv->cq.Get ();
  (*handle)->AddRef ();

  return TRUE;
}

gboolean
gst_d3d12_command_queue_get_fence (GstD3D12CommandQueue * queue,
    ID3D12Fence ** handle)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), FALSE);
  g_return_val_if_fail (handle, FALSE);

  auto priv = queue->priv;

  *handle = priv->fence.Get ();
  (*handle)->AddRef ();

  return TRUE;
}

HRESULT
gst_d3d12_command_queue_execute_command_lists (GstD3D12CommandQueue * queue,
    guint num_command_lists, ID3D12CommandList ** command_lists,
    guint64 * fence_value)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), E_INVALIDARG);

  auto priv = queue->priv;
  HRESULT hr;

  std::lock_guard < std::mutex > lk (priv->execute_lock);
  priv->fence_val++;
  priv->cq->ExecuteCommandLists (num_command_lists, command_lists);
  hr = priv->cq->Signal (priv->fence.Get (), priv->fence_val);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (queue, "Signal failed");
    priv->fence_val--;
  } else if (fence_value) {
    *fence_value = priv->fence_val;
  }

  if (priv->queue_size > 0) {
    auto completed = priv->fence->GetCompletedValue ();
    if (completed + priv->queue_size < priv->fence_val) {
      hr = priv->fence->SetEventOnCompletion (priv->fence_val -
          priv->queue_size, priv->event_handle);
      if (FAILED (hr)) {
        GST_ERROR_OBJECT (queue, "SetEventOnCompletion failed");
        return hr;
      }
      WaitForSingleObjectEx (priv->event_handle, INFINITE, FALSE);
    }
  }

  return hr;
}

HRESULT
gst_d3d12_command_queue_execute_wait (GstD3D12CommandQueue * queue,
    ID3D12Fence * fence, guint64 fence_value)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), E_INVALIDARG);
  g_return_val_if_fail (fence, E_INVALIDARG);

  auto priv = queue->priv;

  return priv->cq->Wait (fence, fence_value);
}

guint64
gst_d3d12_command_queue_get_completed_value (GstD3D12CommandQueue * queue)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), G_MAXUINT64);

  return queue->priv->fence->GetCompletedValue ();
}

HRESULT
gst_d3d12_command_queue_fence_wait (GstD3D12CommandQueue * queue,
    guint64 fence_value, HANDLE event_handle)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue), E_INVALIDARG);

  auto priv = queue->priv;
  guint64 fence_to_wait = fence_value;
  HRESULT hr;
  if (fence_value == G_MAXUINT64) {
    std::lock_guard < std::mutex > lk (priv->execute_lock);
    priv->fence_val++;
    hr = priv->cq->Signal (priv->fence.Get (), priv->fence_val);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (queue, "Signal failed");
      priv->fence_val--;
      return hr;
    }

    fence_to_wait = priv->fence_val;
  }

  auto completed = priv->fence->GetCompletedValue ();
  if (completed < fence_to_wait) {
    bool close_handle = false;
    if (!event_handle) {
      event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
      close_handle = true;
    }

    hr = priv->fence->SetEventOnCompletion (fence_to_wait, event_handle);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (queue, "SetEventOnCompletion failed");
      if (close_handle)
        CloseHandle (event_handle);

      return hr;
    }

    WaitForSingleObjectEx (event_handle, INFINITE, FALSE);
    if (close_handle)
      CloseHandle (event_handle);
  }

  return S_OK;
}

static gpointer
gst_d3d12_command_queue_gc_thread (GstD3D12CommandQueue * self)
{
  auto priv = self->priv;

  GST_INFO_OBJECT (self, "Entering GC thread");

  HANDLE event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);

  while (true) {
    GCDataPtr gc_data;

    {
      std::unique_lock < std::mutex > lk (priv->lock);
      while (!priv->shutdown && priv->gc_list.empty ())
        priv->cond.wait (lk);

      if (priv->shutdown)
        break;

      auto completed = priv->fence->GetCompletedValue ();
      while (!priv->gc_list.empty ()) {
        auto top = priv->gc_list.top ();
        if (top->fence_val > completed) {
          gc_data = top;
          priv->gc_list.pop ();
          break;
        }

        GST_LOG_OBJECT (self, "Releasing fence data, completed %"
            G_GUINT64_FORMAT ", fence value %" G_GUINT64_FORMAT,
            completed, top->fence_val);

        priv->gc_list.pop ();
      }
    }

    if (gc_data) {
      GST_LOG_OBJECT (self, "Waiting for fence data %" G_GUINT64_FORMAT,
          gc_data->fence_val);
      auto hr =
          priv->fence->SetEventOnCompletion (gc_data->fence_val, event_handle);
      if (FAILED (hr)) {
        GST_ERROR_OBJECT (self, "SetEventOnCompletion failed");
      } else {
        WaitForSingleObjectEx (event_handle, INFINITE, FALSE);
        GST_LOG_OBJECT (self, "Waiting done, %" G_GUINT64_FORMAT,
            gc_data->fence_val);
      }
    }
  }

  GST_INFO_OBJECT (self, "Leaving GC thread");

  CloseHandle (event_handle);

  return nullptr;
}

void
gst_d3d12_command_queue_set_notify (GstD3D12CommandQueue * queue,
    guint64 fence_value, gpointer fence_data, GDestroyNotify notify)
{
  g_return_if_fail (GST_IS_D3D12_COMMAND_QUEUE (queue));

  auto priv = queue->priv;

  auto completed = priv->fence->GetCompletedValue ();
  if (completed >= fence_value) {
    GST_DEBUG_OBJECT (queue, "Already completed fence value %"
        G_GUINT64_FORMAT, fence_value);

    if (notify)
      notify (fence_data);
    return;
  }

  auto gc_data = std::make_shared < GCData > (fence_data, notify, fence_value);

  std::lock_guard < std::mutex > lk (priv->lock);
  if (!priv->gc_thread) {
    gst_d3d12_init_background_thread ();
    priv->gc_thread = g_thread_new ("GstD3D12Gc",
        (GThreadFunc) gst_d3d12_command_queue_gc_thread, queue);
  }

  GST_LOG_OBJECT (queue, "Pushing GC data %" G_GUINT64_FORMAT, fence_value);

  priv->gc_list.push (std::move (gc_data));
  priv->cond.notify_one ();
}
