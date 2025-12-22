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

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_command_list_pool_debug);
#define GST_CAT_DEFAULT gst_d3d12_command_list_pool_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct _GstD3D12CommandList : public GstMiniObject
{
  GstD3D12CommandListPool *pool = nullptr;
  D3D12_COMMAND_LIST_TYPE type;
  ComPtr < ID3D12CommandList > cl;
};

struct GstD3D12CommandListPoolPrivate
{
  ~GstD3D12CommandListPoolPrivate ()
  {
    while (!cmd_pool.empty ()) {
      auto cmd = cmd_pool.front ();
      cmd_pool.pop ();
      gst_mini_object_unref (cmd);
    }
  }

  ComPtr<ID3D12Device> device;

  std::mutex lock;
  std::queue<GstD3D12CommandList *>cmd_pool;
  D3D12_COMMAND_LIST_TYPE cmd_type;
};
/* *INDENT-ON* */

struct _GstD3D12CommandListPool
{
  GstObject parent;

  GstD3D12CommandListPoolPrivate *priv;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstD3D12CommandList, gst_d3d12_command_list);

static void gst_d3d12_command_list_pool_finalize (GObject * object);

#define gst_d3d12_command_list_pool_parent_class parent_class
G_DEFINE_TYPE (GstD3D12CommandListPool,
    gst_d3d12_command_list_pool, GST_TYPE_OBJECT);

static void
gst_d3d12_command_list_pool_class_init (GstD3D12CommandListPoolClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_command_list_pool_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_command_list_pool_debug,
      "d3d12commandlistpool", 0, "d3d12commandlistpool");
}

static void
gst_d3d12_command_list_pool_init (GstD3D12CommandListPool * self)
{
  self->priv = new GstD3D12CommandListPoolPrivate ();
}

static void
gst_d3d12_command_list_pool_finalize (GObject * object)
{
  auto self = GST_D3D12_COMMAND_LIST_POOL (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstD3D12CommandListPool *
gst_d3d12_command_list_pool_new (GstD3D12Device * device,
    D3D12_COMMAND_LIST_TYPE type)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  if (type != D3D12_COMMAND_LIST_TYPE_DIRECT &&
      type != D3D12_COMMAND_LIST_TYPE_COPY) {
    GST_ERROR_OBJECT (device, "Not supported command list type");
    return nullptr;
  }

  auto self = (GstD3D12CommandListPool *)
      g_object_new (GST_TYPE_D3D12_COMMAND_LIST_POOL, nullptr);
  gst_object_ref_sink (self);

  auto priv = self->priv;
  priv->device = gst_d3d12_device_get_device_handle (device);;
  priv->cmd_type = type;

  return self;
}

static void
gst_d3d12_command_list_pool_release (GstD3D12CommandListPool * pool,
    GstD3D12CommandList * cmd)
{
  auto priv = pool->priv;
  {
    std::lock_guard < std::mutex > lk (priv->lock);
    cmd->dispose = nullptr;
    cmd->pool = nullptr;
    priv->cmd_pool.push (cmd);
  }

  gst_object_unref (pool);
}

static gboolean
gst_d3d12_command_list_dispose (GstD3D12CommandList * cmd)
{
  if (!cmd->pool)
    return TRUE;

  gst_mini_object_ref (cmd);
  gst_d3d12_command_list_pool_release (cmd->pool, cmd);

  return FALSE;
}

static void
gst_d3d12_command_list_free (GstD3D12CommandList * cmd)
{
  delete cmd;
}

static GstD3D12CommandList *
gst_d3d12_command_list_new (ID3D12CommandList * cl,
    D3D12_COMMAND_LIST_TYPE type)
{
  auto cmd = new GstD3D12CommandList ();
  cmd->cl = cl;
  cmd->type = type;

  gst_mini_object_init (cmd, 0, gst_d3d12_command_list_get_type (),
      nullptr, nullptr,
      (GstMiniObjectFreeFunction) gst_d3d12_command_list_free);

  return cmd;
}

gboolean
gst_d3d12_command_list_pool_acquire (GstD3D12CommandListPool * pool,
    ID3D12CommandAllocator * ca, GstD3D12CommandList ** cmd)
{
  g_return_val_if_fail (GST_IS_D3D12_COMMAND_LIST_POOL (pool), FALSE);
  g_return_val_if_fail (ca, FALSE);
  g_return_val_if_fail (cmd, FALSE);

  *cmd = nullptr;

  auto priv = pool->priv;
  GstD3D12CommandList *new_cmd = nullptr;
  HRESULT hr;

  hr = ca->Reset ();
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (pool, "Couldn't reset command allocator");
    return FALSE;
  }

  {
    std::lock_guard < std::mutex > lk (priv->lock);
    if (!priv->cmd_pool.empty ()) {
      new_cmd = priv->cmd_pool.front ();
      priv->cmd_pool.pop ();
    }
  }

  ComPtr < ID3D12GraphicsCommandList > cl;
  if (!new_cmd) {
    hr = priv->device->CreateCommandList (0, priv->cmd_type,
        ca, nullptr, IID_PPV_ARGS (&cl));
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (pool, "Couldn't create command list, hr: 0x%x",
          (guint) hr);
      return FALSE;
    }

    new_cmd = gst_d3d12_command_list_new (cl.Get (), priv->cmd_type);
  } else {
    new_cmd->cl->QueryInterface (IID_PPV_ARGS (&cl));
    hr = cl->Reset (ca, nullptr);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (pool, "Couldn't reset command list");
      gst_mini_object_unref (new_cmd);
      return false;
    }
  }

  new_cmd->pool = (GstD3D12CommandListPool *) gst_object_ref (pool);
  new_cmd->dispose =
      (GstMiniObjectDisposeFunction) gst_d3d12_command_list_dispose;

  *cmd = new_cmd;

  return TRUE;
}

GstD3D12CommandList *
gst_d3d12_command_list_ref (GstD3D12CommandList * cmd)
{
  return (GstD3D12CommandList *) gst_mini_object_ref (cmd);
}

void
gst_d3d12_command_list_unref (GstD3D12CommandList * cmd)
{
  gst_mini_object_unref (cmd);
}

void
gst_clear_d3d12_command_list (GstD3D12CommandList ** cmd)
{
  gst_clear_mini_object (cmd);
}

D3D12_COMMAND_LIST_TYPE
gst_d3d12_command_list_get_command_type (GstD3D12CommandList * cmd)
{
  g_return_val_if_fail (cmd, D3D12_COMMAND_LIST_TYPE_NONE);

  return cmd->type;
}

gboolean
gst_d3d12_command_list_get_handle (GstD3D12CommandList * cmd,
    ID3D12CommandList ** cl)
{
  g_return_val_if_fail (cmd, FALSE);
  g_return_val_if_fail (cl, FALSE);

  *cl = cmd->cl.Get ();
  (*cl)->AddRef ();

  return TRUE;
}
