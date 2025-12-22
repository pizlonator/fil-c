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

#include "gstd3d12dpbstorage.h"
#include <wrl.h>
#include <vector>
#include <directx/d3dx12.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_dpb_storage_debug);
#define GST_CAT_DEFAULT gst_d3d12_dpb_storage_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct OwnedTexture
{
  ComPtr<ID3D12Resource> texture;
  guint subresource = 0;
  gboolean is_free = TRUE;
};

struct GstD3D12DpbStoragePrivate
{
  gboolean array_of_textures;
  DXGI_FORMAT format;
  guint width;
  guint height;
  D3D12_RESOURCE_FLAGS resource_flags;
  std::vector<ID3D12Resource *> dpb;
  std::vector<guint> dpb_subresource;
  std::vector<OwnedTexture> pool;
  ComPtr<ID3D12Resource> base_texture;
};
/* *INDENT-ON* */

struct _GstD3D12DpbStorage
{
  GstObject parent;

  GstD3D12Device *device;
  GstD3D12DpbStoragePrivate *priv;
};

static void gst_d3d12_dpb_storage_finalize (GObject * object);

#define gst_d3d12_dpb_storage_parent_class parent_class
G_DEFINE_TYPE (GstD3D12DpbStorage, gst_d3d12_dpb_storage, GST_TYPE_OBJECT);

static void
gst_d3d12_dpb_storage_class_init (GstD3D12DpbStorageClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_dpb_storage_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_dpb_storage_debug, "d3d12dpbstorage", 0,
      "d3d12dpbstorage");
}

static void
gst_d3d12_dpb_storage_init (GstD3D12DpbStorage * self)
{
  self->priv = new GstD3D12DpbStoragePrivate ();
}

static void
gst_d3d12_dpb_storage_finalize (GObject * object)
{
  auto self = GST_D3D12_DPB_STORAGE (object);

  delete self->priv;
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static HRESULT
allocate_texture (ID3D12Device * device, DXGI_FORMAT format, guint width,
    guint height, D3D12_RESOURCE_FLAGS resource_flags, guint array_size,
    ID3D12Resource ** texture)
{
  D3D12_HEAP_PROPERTIES prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D (format,
      width, height, array_size, 1, 1, 0, resource_flags);

  return device->CreateCommittedResource (&prop, D3D12_HEAP_FLAG_NONE,
      &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS (texture));
}

gboolean
gst_d3d12_dpb_storage_acquire_frame (GstD3D12DpbStorage * storage,
    D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * frame)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), FALSE);
  g_return_val_if_fail (frame, FALSE);

  auto priv = storage->priv;

  for (size_t i = 0; i < priv->pool.size (); i++) {
    auto & it = priv->pool[i];
    if (it.is_free) {
      frame->pReconstructedPicture = it.texture.Get ();
      frame->ReconstructedPictureSubresource = it.subresource;
      it.is_free = FALSE;
      return TRUE;
    }
  }

  if (!priv->array_of_textures) {
    GST_ERROR_OBJECT (storage, "No available free texture");
    frame->pReconstructedPicture = nullptr;
    frame->ReconstructedPictureSubresource = 0;
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (storage->device);

  OwnedTexture new_texture;
  new_texture.is_free = FALSE;
  HRESULT hr = allocate_texture (device, priv->format, priv->width,
      priv->height, priv->resource_flags, 1, &new_texture.texture);

  if (!gst_d3d12_result (hr, storage->device)) {
    GST_ERROR_OBJECT (storage, "Couldn't allocate texture");
    frame->pReconstructedPicture = nullptr;
    frame->ReconstructedPictureSubresource = 0;
    return FALSE;
  }

  frame->pReconstructedPicture = new_texture.texture.Get ();
  frame->ReconstructedPictureSubresource = 0;

  priv->pool.push_back (new_texture);

  return TRUE;
}

gboolean
gst_d3d12_dpb_storage_add_frame (GstD3D12DpbStorage * storage,
    D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * frame)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), FALSE);
  g_return_val_if_fail (frame, FALSE);
  g_return_val_if_fail (frame->pReconstructedPicture, FALSE);

  auto priv = storage->priv;

  priv->dpb.insert (priv->dpb.begin (), frame->pReconstructedPicture);
  priv->dpb_subresource.insert (priv->dpb_subresource.begin (),
      frame->ReconstructedPictureSubresource);

  return TRUE;
}

gboolean
gst_d3d12_dpb_storage_get_reference_frames (GstD3D12DpbStorage * storage,
    D3D12_VIDEO_ENCODE_REFERENCE_FRAMES * ref_frames)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), FALSE);
  g_return_val_if_fail (ref_frames, FALSE);

  auto priv = storage->priv;

  ref_frames->NumTexture2Ds = priv->dpb.size ();
  ref_frames->ppTexture2Ds = priv->dpb.data ();
  if (priv->array_of_textures)
    ref_frames->pSubresources = nullptr;
  else
    ref_frames->pSubresources = priv->dpb_subresource.data ();

  return TRUE;
}

static void
gst_d3d12_dpb_storage_release_frame (GstD3D12DpbStorage * self,
    ID3D12Resource * texture, guint subresource)
{
  auto priv = self->priv;

  if (priv->array_of_textures) {
    for (size_t i = 0; i < priv->pool.size (); i++) {
      auto & it = priv->pool[i];
      if (texture == it.texture.Get () && it.subresource == subresource) {
        it.is_free = TRUE;
        return;
      }
    }

    g_assert_not_reached ();
  } else {
    g_return_if_fail (subresource < priv->pool.size ());

    priv->pool[subresource].is_free = TRUE;
  }
}

gboolean
gst_d3d12_dpb_storage_remove_oldest_frame (GstD3D12DpbStorage * storage)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), FALSE);

  auto priv = storage->priv;

  if (priv->dpb.empty ()) {
    GST_WARNING_OBJECT (storage, "DPB is empty now");
    return FALSE;
  }

  gst_d3d12_dpb_storage_release_frame (storage,
      priv->dpb.back (), priv->dpb_subresource.back ());

  priv->dpb.pop_back ();
  priv->dpb_subresource.pop_back ();

  return TRUE;
}

void
gst_d3d12_dpb_storage_clear_dpb (GstD3D12DpbStorage * storage)
{
  g_return_if_fail (GST_IS_D3D12_DPB_STORAGE (storage));

  auto priv = storage->priv;

  g_assert (priv->dpb.size () == priv->dpb_subresource.size ());

  for (size_t i = 0; i < priv->dpb.size (); i++) {
    gst_d3d12_dpb_storage_release_frame (storage,
        priv->dpb[i], priv->dpb_subresource[i]);
  }

  priv->dpb.clear ();
  priv->dpb_subresource.clear ();
}

guint
gst_d3d12_dpb_storage_get_dpb_size (GstD3D12DpbStorage * storage)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), 0);

  auto priv = storage->priv;

  return priv->dpb.size ();
}

guint
gst_d3d12_dpb_storage_get_pool_size (GstD3D12DpbStorage * storage)
{
  g_return_val_if_fail (GST_IS_D3D12_DPB_STORAGE (storage), 0);

  auto priv = storage->priv;

  return priv->pool.size ();
}

GstD3D12DpbStorage *
gst_d3d12_dpb_storage_new (GstD3D12Device * device, guint dpb_size,
    gboolean use_array_of_textures, DXGI_FORMAT format, guint width,
    guint height, D3D12_RESOURCE_FLAGS resource_flags)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  auto self = (GstD3D12DpbStorage *)
      g_object_new (GST_TYPE_D3D12_DPB_STORAGE, nullptr);
  gst_object_ref_sink (self);

  self->device = (GstD3D12Device *) gst_object_ref (device);
  auto priv = self->priv;
  auto device_handle = gst_d3d12_device_get_device_handle (device);
  HRESULT hr;

  if (use_array_of_textures) {
    for (guint i = 0; i < dpb_size; i++) {
      OwnedTexture texture;

      hr = allocate_texture (device_handle,
          format, width, height, resource_flags, 1, &texture.texture);
      if (!gst_d3d12_result (hr, device)) {
        GST_ERROR_OBJECT (self, "Couldn't allocate initial texture");
        gst_object_unref (self);
        return nullptr;
      }

      priv->pool.push_back (texture);
    }
  } else {
    hr = allocate_texture (device_handle,
        format, width, height, resource_flags, dpb_size, &priv->base_texture);

    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Couldn't allocate initial texture");
      gst_object_unref (self);
      return nullptr;
    }

    priv->pool.resize (dpb_size);

    for (guint i = 0; i < dpb_size; i++) {
      priv->pool[i].texture = priv->base_texture;
      priv->pool[i].subresource = i;
    }
  }

  priv->width = width;
  priv->height = height;
  priv->resource_flags = resource_flags;
  priv->array_of_textures = use_array_of_textures;
  priv->dpb.reserve (dpb_size);
  priv->dpb_subresource.reserve (dpb_size);

  return self;
}
