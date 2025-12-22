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
 * Boston, MA 02120-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12overlaycompositor.h"
#include <directx/d3dx12.h>
#include <wrl.h>
#include <memory>
#include <vector>
#include <algorithm>
#include "PSMain_sample.h"
#include "PSMain_sample_premul.h"
#include "VSMain_coord.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_overlay_compositor_debug);
#define GST_CAT_DEFAULT gst_d3d12_overlay_compositor_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct VertexData
{
  struct {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct {
    FLOAT u;
    FLOAT v;
  } texture;
};

struct GstD3D12OverlayRect : public GstMiniObject
{
  ~GstD3D12OverlayRect ()
  {
    if (overlay_rect)
      gst_video_overlay_rectangle_unref (overlay_rect);

    gst_clear_d3d12_descriptor (&srv_heap);
  }

  GstVideoOverlayRectangle *overlay_rect = nullptr;
  ComPtr<ID3D12Resource> texture;
  ComPtr<ID3D12Resource> staging;
  ComPtr<ID3D12Resource> vertex_buf;
  GstD3D12Descriptor *srv_heap = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vbv;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  gboolean premul_alpha = FALSE;
  gboolean need_upload = TRUE;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstD3D12OverlayRect, gst_d3d12_overlay_rect);

struct GstD3D12OverlayCompositorPrivate
{
  GstD3D12OverlayCompositorPrivate ()
  {
    sample_desc.Count = 1;
    sample_desc.Quality = 0;
  }

  ~GstD3D12OverlayCompositorPrivate ()
  {
    if (overlays)
      g_list_free_full (overlays, (GDestroyNotify) gst_mini_object_unref);

    gst_clear_object (&ca_pool);
    gst_clear_object (&srv_heap_pool);
  }

  GstVideoInfo info;

  D3D12_VIEWPORT viewport;
  D3D12_RECT scissor_rect;

  D3D12_INPUT_ELEMENT_DESC input_desc[2];
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_premul_desc = { };
  DXGI_SAMPLE_DESC sample_desc;

  ComPtr<ID3D12RootSignature> rs;
  ComPtr<ID3D12PipelineState> pso;
  ComPtr<ID3D12PipelineState> pso_premul;
  D3D12_INDEX_BUFFER_VIEW idv;
  ComPtr<ID3D12Resource> index_buf;
  ComPtr<ID3D12GraphicsCommandList> cl;
  GstD3D12CommandAllocatorPool *ca_pool = nullptr;
  GstD3D12DescriptorPool *srv_heap_pool = nullptr;

  GList *overlays = nullptr;

  std::vector<GstVideoOverlayRectangle *> rects_to_upload;
};
/* *INDENT-ON* */

struct _GstD3D12OverlayCompositor
{
  GstObject parent;

  GstD3D12Device *device;

  GstD3D12OverlayCompositorPrivate *priv;
};

static void gst_d3d12_overlay_compositor_finalize (GObject * object);

#define gst_d3d12_overlay_compositor_parent_class parent_class
G_DEFINE_TYPE (GstD3D12OverlayCompositor,
    gst_d3d12_overlay_compositor, GST_TYPE_OBJECT);

static void
gst_d3d12_overlay_compositor_class_init (GstD3D12OverlayCompositorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_overlay_compositor_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_overlay_compositor_debug,
      "d3d12overlaycompositor", 0, "d3d12overlaycompositor");
}

static void
gst_d3d12_overlay_compositor_init (GstD3D12OverlayCompositor * self)
{
  self->priv = new GstD3D12OverlayCompositorPrivate ();
}

static void
gst_d3d12_overlay_compositor_finalize (GObject * object)
{
  GstD3D12OverlayCompositor *self = GST_D3D12_OVERLAY_COMPOSITOR (object);

  delete self->priv;

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_overlay_rect_free (GstD3D12OverlayRect * rect)
{
  if (rect)
    delete rect;
}

static GstD3D12OverlayRect *
gst_d3d12_overlay_rect_new (GstD3D12OverlayCompositor * self,
    GstVideoOverlayRectangle * overlay_rect)
{
  auto priv = self->priv;
  gint x, y;
  guint width, height;
  VertexData vertex_data[4];
  FLOAT x1, y1, x2, y2;
  gdouble val;
  GstVideoOverlayFormatFlags flags;
  gboolean premul_alpha = FALSE;

  if (!gst_video_overlay_rectangle_get_render_rectangle (overlay_rect, &x, &y,
          &width, &height)) {
    GST_ERROR_OBJECT (self, "Failed to get render rectangle");
    return nullptr;
  }

  flags = gst_video_overlay_rectangle_get_flags (overlay_rect);
  if ((flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA) != 0) {
    premul_alpha = TRUE;
    flags = GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA;
  } else {
    flags = GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE;
  }

  auto buf = gst_video_overlay_rectangle_get_pixels_unscaled_argb (overlay_rect,
      flags);
  if (!buf) {
    GST_ERROR_OBJECT (self, "Failed to get overlay buffer");
    return nullptr;
  }

  auto vmeta = gst_buffer_get_video_meta (buf);
  if (!vmeta) {
    GST_ERROR_OBJECT (self, "Failed to get video meta");
    return nullptr;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC desc =
      CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_B8G8R8A8_UNORM, vmeta->width,
      vmeta->height, 1, 1);

  ComPtr < ID3D12Resource > texture;
  auto hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS (&texture));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create texture");
    return nullptr;
  }

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  UINT64 size;
  device->GetCopyableFootprints (&desc, 0, 1, 0, &layout, nullptr, nullptr,
      &size);

  ComPtr < ID3D12Resource > staging;
  heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  desc = CD3DX12_RESOURCE_DESC::Buffer (size);
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&staging));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create upload buffer");
    return nullptr;
  }

  guint8 *map_data;
  hr = staging->Map (0, nullptr, (void **) &map_data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map staging");
    return nullptr;
  }

  guint8 *data;
  gint stride;
  GstMapInfo info;
  if (!gst_video_meta_map (vmeta,
          0, &info, (gpointer *) & data, &stride, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map");
    return nullptr;
  }

  if (layout.Footprint.RowPitch == (UINT) stride) {
    memcpy (map_data, data, stride * layout.Footprint.Height);
  } else {
    guint width_in_bytes = 4 * layout.Footprint.Width;
    for (UINT i = 0; i < layout.Footprint.Height; i++) {
      memcpy (map_data, data, width_in_bytes);
      map_data += layout.Footprint.RowPitch;
      data += stride;
    }
  }

  staging->Unmap (0, nullptr);
  gst_video_meta_unmap (vmeta, 0, &info);

  /* bottom left */
  gst_util_fraction_to_double (x, GST_VIDEO_INFO_WIDTH (&priv->info), &val);
  x1 = (val * 2.0f) - 1.0f;

  gst_util_fraction_to_double (y + height,
      GST_VIDEO_INFO_HEIGHT (&priv->info), &val);
  y1 = (val * -2.0f) + 1.0f;

  /* top right */
  gst_util_fraction_to_double (x + width,
      GST_VIDEO_INFO_WIDTH (&priv->info), &val);
  x2 = (val * 2.0f) - 1.0f;

  gst_util_fraction_to_double (y, GST_VIDEO_INFO_HEIGHT (&priv->info), &val);
  y2 = (val * -2.0f) + 1.0f;

  /* bottom left */
  vertex_data[0].position.x = x1;
  vertex_data[0].position.y = y1;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.u = 0.0f;
  vertex_data[0].texture.v = 1.0f;

  /* top left */
  vertex_data[1].position.x = x1;
  vertex_data[1].position.y = y2;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.u = 0.0f;
  vertex_data[1].texture.v = 0.0f;

  /* top right */
  vertex_data[2].position.x = x2;
  vertex_data[2].position.y = y2;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.u = 1.0f;
  vertex_data[2].texture.v = 0.0f;

  /* bottom right */
  vertex_data[3].position.x = x2;
  vertex_data[3].position.y = y1;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.u = 1.0f;
  vertex_data[3].texture.v = 1.0f;

  ComPtr < ID3D12Resource > vertex_buf;
  heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  desc = CD3DX12_RESOURCE_DESC::Buffer (sizeof (VertexData) * 4);
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&vertex_buf));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create vertex buffer");
    return nullptr;
  }

  hr = vertex_buf->Map (0, nullptr, (void **) &map_data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map vertex buffer");
    return nullptr;
  }

  memcpy (map_data, vertex_data, sizeof (VertexData) * 4);
  vertex_buf->Unmap (0, nullptr);

  GstD3D12Descriptor *srv_heap;
  if (!gst_d3d12_descriptor_pool_acquire (priv->srv_heap_pool, &srv_heap)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    return nullptr;
  }

  ComPtr < ID3D12DescriptorHeap > srv_heap_handle;
  gst_d3d12_descriptor_get_handle (srv_heap, &srv_heap_handle);
  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Texture2D.MipLevels = 1;
  srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

  device->CreateShaderResourceView (texture.Get (), &srv_desc,
      srv_heap_handle->GetCPUDescriptorHandleForHeapStart ());

  auto rect = new GstD3D12OverlayRect ();
  gst_mini_object_init (rect, 0, gst_d3d12_overlay_rect_get_type (),
      nullptr, nullptr,
      (GstMiniObjectFreeFunction) gst_d3d12_overlay_rect_free);

  rect->overlay_rect = gst_video_overlay_rectangle_ref (overlay_rect);
  rect->texture = texture;
  rect->staging = staging;
  rect->vertex_buf = vertex_buf;
  rect->vbv.BufferLocation = vertex_buf->GetGPUVirtualAddress ();
  rect->vbv.SizeInBytes = sizeof (VertexData) * 4;
  rect->vbv.StrideInBytes = sizeof (VertexData);
  rect->layout = layout;
  rect->srv_heap = srv_heap;
  rect->premul_alpha = premul_alpha;

  return rect;
}

static gboolean
gst_d3d12_overlay_compositor_setup_shader (GstD3D12OverlayCompositor * self)
{
  auto priv = self->priv;
  GstVideoInfo *info = &priv->info;
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
  const D3D12_ROOT_SIGNATURE_FLAGS rs_flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
  const D3D12_STATIC_SAMPLER_DESC static_sampler_desc = {
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    0,
    1,
    D3D12_COMPARISON_FUNC_ALWAYS,
    D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
    0,
    D3D12_FLOAT32_MAX,
    0,
    0,
    D3D12_SHADER_VISIBILITY_PIXEL
  };

  CD3DX12_ROOT_PARAMETER param;
  D3D12_DESCRIPTOR_RANGE range;
  std::vector < D3D12_ROOT_PARAMETER > param_list;

  range = CD3DX12_DESCRIPTOR_RANGE (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  param.InitAsDescriptorTable (1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
  param_list.push_back (param);

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
      param_list.size (), param_list.data (),
      1, &static_sampler_desc, rs_flags);

  ComPtr < ID3DBlob > rs_blob;
  ComPtr < ID3DBlob > error_blob;
  auto hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
  if (!gst_d3d12_result (hr, self->device)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();

    GST_ERROR_OBJECT (self, "Couldn't serialize root signature, error: %s",
        GST_STR_NULL (error_msg));
    return FALSE;
  }

  GstD3D12Format device_format;
  gst_d3d12_device_get_format (self->device, GST_VIDEO_INFO_FORMAT (info),
      &device_format);

  auto device = gst_d3d12_device_get_device_handle (self->device);
  ComPtr < ID3D12RootSignature > rs;
  device->CreateRootSignature (0, rs_blob->GetBufferPointer (),
      rs_blob->GetBufferSize (), IID_PPV_ARGS (&rs));

  priv->input_desc[0].SemanticName = "POSITION";
  priv->input_desc[0].SemanticIndex = 0;
  priv->input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  priv->input_desc[0].InputSlot = 0;
  priv->input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  priv->input_desc[0].InputSlotClass =
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  priv->input_desc[0].InstanceDataStepRate = 0;

  priv->input_desc[1].SemanticName = "TEXCOORD";
  priv->input_desc[1].SemanticIndex = 0;
  priv->input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  priv->input_desc[1].InputSlot = 0;
  priv->input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  priv->input_desc[1].InputSlotClass =
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  priv->input_desc[1].InstanceDataStepRate = 0;

  auto & pso_desc = priv->pso_desc;
  pso_desc.pRootSignature = rs.Get ();
  pso_desc.VS.BytecodeLength = sizeof (g_VSMain_coord);
  pso_desc.VS.pShaderBytecode = g_VSMain_coord;
  pso_desc.PS.BytecodeLength = sizeof (g_PSMain_sample);
  pso_desc.PS.pShaderBytecode = g_PSMain_sample;
  pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
  pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pso_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
  pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pso_desc.BlendState.RenderTarget[0].DestBlendAlpha =
      D3D12_BLEND_INV_SRC_ALPHA;
  pso_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pso_desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;
  pso_desc.SampleMask = UINT_MAX;
  pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.DepthStencilState.DepthEnable = FALSE;
  pso_desc.DepthStencilState.StencilEnable = FALSE;
  pso_desc.InputLayout.pInputElementDescs = priv->input_desc;
  pso_desc.InputLayout.NumElements = 2;
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = device_format.resource_format[0];
  pso_desc.SampleDesc.Count = 1;

  ComPtr < ID3D12PipelineState > pso;
  hr = device->CreateGraphicsPipelineState (&pso_desc, IID_PPV_ARGS (&pso));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  ComPtr < ID3D12PipelineState > pso_premul;
  auto & pso_premul_desc = priv->pso_premul_desc;
  pso_premul_desc = priv->pso_desc;
  pso_premul_desc.PS.BytecodeLength = sizeof (g_PSMain_sample_premul);
  pso_premul_desc.PS.pShaderBytecode = g_PSMain_sample_premul;
  hr = device->CreateGraphicsPipelineState (&pso_premul_desc,
      IID_PPV_ARGS (&pso_premul));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC buffer_desc =
      CD3DX12_RESOURCE_DESC::Buffer (sizeof (indices));
  ComPtr < ID3D12Resource > index_buf;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&index_buf));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  void *data;
  hr = index_buf->Map (0, nullptr, &data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map index buffer");
    return FALSE;
  }

  memcpy (data, indices, sizeof (indices));
  index_buf->Unmap (0, nullptr);

  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = { };
  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap_desc.NumDescriptors = 1;
  heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  priv->rs = rs;
  priv->pso = pso;
  priv->pso_premul = pso_premul;
  priv->idv.BufferLocation = index_buf->GetGPUVirtualAddress ();
  priv->idv.SizeInBytes = sizeof (indices);
  priv->idv.Format = DXGI_FORMAT_R16_UINT;
  priv->index_buf = index_buf;
  priv->srv_heap_pool = gst_d3d12_descriptor_pool_new (self->device,
      &heap_desc);
  priv->ca_pool = gst_d3d12_command_allocator_pool_new (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);

  priv->viewport.TopLeftX = 0;
  priv->viewport.TopLeftY = 0;
  priv->viewport.Width = GST_VIDEO_INFO_WIDTH (info);
  priv->viewport.Height = GST_VIDEO_INFO_HEIGHT (info);
  priv->viewport.MinDepth = 0.0f;
  priv->viewport.MaxDepth = 1.0f;

  priv->scissor_rect.left = 0;
  priv->scissor_rect.top = 0;
  priv->scissor_rect.right = GST_VIDEO_INFO_WIDTH (info);
  priv->scissor_rect.bottom = GST_VIDEO_INFO_HEIGHT (info);

  return TRUE;
}

GstD3D12OverlayCompositor *
gst_d3d12_overlay_compositor_new (GstD3D12Device * device,
    const GstVideoInfo * info)
{
  GstD3D12OverlayCompositor *self = nullptr;
  GstD3D12OverlayCompositorPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);
  g_return_val_if_fail (info != nullptr, nullptr);

  self = (GstD3D12OverlayCompositor *)
      g_object_new (GST_TYPE_D3D12_OVERLAY_COMPOSITOR, nullptr);
  gst_object_ref_sink (self);
  priv = self->priv;

  self->device = (GstD3D12Device *) gst_object_ref (device);
  priv->info = *info;

  if (!gst_d3d12_overlay_compositor_setup_shader (self)) {
    gst_object_unref (self);
    return nullptr;
  }

  return self;
}

static gboolean
gst_d3d12_overlay_compositor_foreach_meta (GstBuffer * buffer, GstMeta ** meta,
    GstD3D12OverlayCompositor * self)
{
  auto priv = self->priv;

  if ((*meta)->info->api != GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE)
    return TRUE;

  auto cmeta = (GstVideoOverlayCompositionMeta *) (*meta);
  if (!cmeta->overlay)
    return TRUE;

  auto num_rect = gst_video_overlay_composition_n_rectangles (cmeta->overlay);
  for (guint i = 0; i < num_rect; i++) {
    auto rect = gst_video_overlay_composition_get_rectangle (cmeta->overlay, i);
    priv->rects_to_upload.push_back (rect);
  }

  return TRUE;
}

gboolean
gst_d3d12_overlay_compositor_upload (GstD3D12OverlayCompositor * compositor,
    GstBuffer * buf)
{
  g_return_val_if_fail (compositor != nullptr, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (buf), FALSE);

  auto priv = compositor->priv;
  priv->rects_to_upload.clear ();

  gst_buffer_foreach_meta (buf,
      (GstBufferForeachMetaFunc) gst_d3d12_overlay_compositor_foreach_meta,
      compositor);

  if (priv->rects_to_upload.empty ()) {
    if (priv->overlays)
      g_list_free_full (priv->overlays, (GDestroyNotify) gst_mini_object_unref);
    priv->overlays = nullptr;
    return TRUE;
  }

  GST_LOG_OBJECT (compositor, "Found %" G_GSIZE_FORMAT
      " overlay rectangles", priv->rects_to_upload.size ());

  for (size_t i = 0; i < priv->rects_to_upload.size (); i++) {
    GList *iter;
    bool found = false;
    for (iter = priv->overlays; iter; iter = g_list_next (iter)) {
      auto rect = (GstD3D12OverlayRect *) iter->data;
      if (rect->overlay_rect == priv->rects_to_upload[i]) {
        found = true;
        break;
      }
    }

    if (!found) {
      auto new_rect = gst_d3d12_overlay_rect_new (compositor,
          priv->rects_to_upload[i]);
      if (new_rect)
        priv->overlays = g_list_append (priv->overlays, new_rect);
    }
  }

  /* Remove old overlay */
  GList *iter;
  GList *next;
  for (iter = priv->overlays; iter; iter = next) {
    auto rect = (GstD3D12OverlayRect *) iter->data;
    next = g_list_next (iter);

    if (std::find_if (priv->rects_to_upload.begin (),
            priv->rects_to_upload.end (),[&](const auto & overlay)->bool
            {
            return overlay == rect->overlay_rect;}
        ) == priv->rects_to_upload.end ()) {
      gst_mini_object_unref (rect);
      priv->overlays = g_list_delete_link (priv->overlays, iter);
    }
  }

  return TRUE;
}

gboolean
gst_d3d12_overlay_compositor_update_viewport (GstD3D12OverlayCompositor *
    compositor, GstVideoRectangle * viewport)
{
  g_return_val_if_fail (GST_IS_D3D12_OVERLAY_COMPOSITOR (compositor), FALSE);
  g_return_val_if_fail (viewport != nullptr, FALSE);

  auto priv = compositor->priv;

  priv->viewport.TopLeftX = viewport->x;
  priv->viewport.TopLeftY = viewport->y;
  priv->viewport.Width = viewport->w;
  priv->viewport.Height = viewport->h;

  priv->scissor_rect.left = viewport->x;
  priv->scissor_rect.top = viewport->y;
  priv->scissor_rect.right = viewport->x + viewport->w;
  priv->scissor_rect.bottom = viewport->y + viewport->h;

  return TRUE;
}

static gboolean
gst_d3d12_overlay_compositor_execute (GstD3D12OverlayCompositor * self,
    GstBuffer * buf, GstD3D12FenceData * fence_data,
    ID3D12GraphicsCommandList * cl)
{
  auto priv = self->priv;

  auto mem = (GstD3D12Memory *) gst_buffer_peek_memory (buf, 0);
  ComPtr < ID3D12DescriptorHeap > rtv_heap;
  if (!gst_d3d12_memory_get_render_target_view_heap (mem, &rtv_heap)) {
    GST_ERROR_OBJECT (self, "Couldn't get rtv heap");
    return FALSE;
  }

  GList *iter;
  ComPtr < ID3D12PipelineState > prev_pso;
  for (iter = priv->overlays; iter; iter = g_list_next (iter)) {
    auto rect = (GstD3D12OverlayRect *) iter->data;
    if (rect->need_upload) {
      D3D12_TEXTURE_COPY_LOCATION src =
          CD3DX12_TEXTURE_COPY_LOCATION (rect->staging.Get (), rect->layout);
      D3D12_TEXTURE_COPY_LOCATION dst =
          CD3DX12_TEXTURE_COPY_LOCATION (rect->texture.Get ());
      GST_LOG_OBJECT (self, "First render, uploading texture");
      cl->CopyTextureRegion (&dst, 0, 0, 0, &src, nullptr);
      D3D12_RESOURCE_BARRIER barrier =
          CD3DX12_RESOURCE_BARRIER::Transition (rect->texture.Get (),
          D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cl->ResourceBarrier (1, &barrier);
      rect->need_upload = FALSE;
    }

    cl->SetGraphicsRootSignature (priv->rs.Get ());

    ComPtr < ID3D12PipelineState > pso;
    if (rect->premul_alpha)
      pso = priv->pso;
    else
      pso = priv->pso_premul;

    if (!prev_pso) {
      cl->SetPipelineState (pso.Get ());
      cl->IASetIndexBuffer (&priv->idv);
      cl->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cl->RSSetViewports (1, &priv->viewport);
      cl->RSSetScissorRects (1, &priv->scissor_rect);
      D3D12_CPU_DESCRIPTOR_HANDLE rtv_heaps[] = {
        rtv_heap->GetCPUDescriptorHandleForHeapStart ()
      };
      cl->OMSetRenderTargets (1, rtv_heaps, FALSE, nullptr);
    } else if (pso != prev_pso) {
      cl->SetPipelineState (pso.Get ());
    }

    ComPtr < ID3D12DescriptorHeap > srv_heap;
    gst_d3d12_descriptor_get_handle (rect->srv_heap, &srv_heap);
    ID3D12DescriptorHeap *heaps[] = { srv_heap.Get () };
    cl->SetDescriptorHeaps (1, heaps);
    cl->SetGraphicsRootDescriptorTable (0,
        srv_heap->GetGPUDescriptorHandleForHeapStart ());
    cl->IASetVertexBuffers (0, 1, &rect->vbv);

    cl->DrawIndexedInstanced (6, 1, 0, 0, 0);

    gst_d3d12_fence_data_add_notify_mini_object (fence_data,
        gst_mini_object_ref (rect));

    prev_pso = nullptr;
    prev_pso = pso;
  }

  priv->pso->AddRef ();
  gst_d3d12_fence_data_add_notify_com (fence_data, priv->pso.Get ());

  priv->pso_premul->AddRef ();
  gst_d3d12_fence_data_add_notify_com (fence_data, priv->pso_premul.Get ());

  return TRUE;
}

gboolean
gst_d3d12_overlay_compositor_draw (GstD3D12OverlayCompositor * compositor,
    GstBuffer * buf, GstD3D12FenceData * fence_data,
    ID3D12GraphicsCommandList * command_list)
{
  g_return_val_if_fail (compositor != nullptr, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (buf), FALSE);
  g_return_val_if_fail (fence_data, FALSE);
  g_return_val_if_fail (command_list, FALSE);

  auto priv = compositor->priv;

  if (!priv->overlays)
    return TRUE;

  auto mem = (GstD3D12Memory *) gst_buffer_peek_memory (buf, 0);
  auto resource = gst_d3d12_memory_get_resource_handle (mem);
  auto desc = resource->GetDesc ();
  if (desc.SampleDesc.Count != priv->sample_desc.Count ||
      desc.SampleDesc.Quality != priv->sample_desc.Quality) {
    auto device = gst_d3d12_device_get_device_handle (compositor->device);

    auto pso_desc = priv->pso_desc;
    pso_desc.SampleDesc = desc.SampleDesc;
    ComPtr < ID3D12PipelineState > pso;
    auto hr = device->CreateGraphicsPipelineState (&pso_desc,
        IID_PPV_ARGS (&pso));
    if (!gst_d3d12_result (hr, compositor->device)) {
      GST_ERROR_OBJECT (compositor, "Couldn't create pso");
      return FALSE;
    }

    ComPtr < ID3D12PipelineState > pso_premul;
    auto pso_premul_desc = priv->pso_premul_desc;
    pso_premul_desc.SampleDesc = desc.SampleDesc;
    hr = device->CreateGraphicsPipelineState (&pso_premul_desc,
        IID_PPV_ARGS (&pso_premul));
    if (!gst_d3d12_result (hr, compositor->device)) {
      GST_ERROR_OBJECT (compositor, "Couldn't create pso");
      return FALSE;
    }

    priv->pso = nullptr;
    priv->pso_premul = nullptr;

    priv->pso = pso;
    priv->pso_premul = pso_premul;
    priv->sample_desc = desc.SampleDesc;
  }

  return gst_d3d12_overlay_compositor_execute (compositor,
      buf, fence_data, command_list);
}
