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

/**
 * SECTION:element-d3d12testsrc
 * @title: d3d12testsrc
 *
 * The d3d12testsrc element is used to produce test video data
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d12testsrc ! queue ! d3d12videosink
 * ```
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12testsrc.h"
#include "gstd3d11on12.h"
#include "gstd3d12pluginutils.h"
#include <directx/d3dx12.h>
#include <wrl.h>
#include <string.h>
#include <d3d11.h>
#include <d2d1.h>
#include <math.h>
#include <memory>
#include <vector>
#include <queue>
#include "PSMain_checker.h"
#include "PSMain_color.h"
#include "PSMain_snow.h"
#include "VSMain_color.h"
#include "VSMain_coord.h"

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_test_src_debug);
#define GST_CAT_DEFAULT gst_d3d12_test_src_debug

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (GST_D3D12_ALL_FORMATS)));

enum GstD3D12TestSrcPattern
{
  GST_D3D12_TEST_SRC_SMPTE,
  GST_D3D12_TEST_SRC_SNOW,
  GST_D3D12_TEST_SRC_BLACK,
  GST_D3D12_TEST_SRC_WHITE,
  GST_D3D12_TEST_SRC_RED,
  GST_D3D12_TEST_SRC_GREEN,
  GST_D3D12_TEST_SRC_BLUE,
  GST_D3D12_TEST_SRC_CHECKERS1,
  GST_D3D12_TEST_SRC_CHECKERS2,
  GST_D3D12_TEST_SRC_CHECKERS4,
  GST_D3D12_TEST_SRC_CHECKERS8,
  GST_D3D12_TEST_SRC_CIRCULAR,
  GST_D3D12_TEST_SRC_BLINK,
  /* sync with videotestsrc */
  GST_D3D12_TEST_SRC_BALL = 18,
};

#define GST_TYPE_D3D12_TEST_SRC_PATTERN (gst_d3d12_test_src_pattern_get_type ())
static GType
gst_d3d12_test_src_pattern_get_type (void)
{
  static GType pattern_type = 0;

  GST_D3D12_CALL_ONCE_BEGIN {
    static const GEnumValue pattern_types[] = {
      {GST_D3D12_TEST_SRC_SMPTE, "SMPTE 100% color bars", "smpte"},
      {GST_D3D12_TEST_SRC_SNOW, "Random (television snow)", "snow"},
      {GST_D3D12_TEST_SRC_BLACK, "100% Black", "black"},
      {GST_D3D12_TEST_SRC_WHITE, "100% White", "white"},
      {GST_D3D12_TEST_SRC_RED, "Red", "red"},
      {GST_D3D12_TEST_SRC_GREEN, "Green", "green"},
      {GST_D3D12_TEST_SRC_BLUE, "Blue", "blue"},
      {GST_D3D12_TEST_SRC_CHECKERS1, "Checkers 1px", "checkers-1"},
      {GST_D3D12_TEST_SRC_CHECKERS2, "Checkers 2px", "checkers-2"},
      {GST_D3D12_TEST_SRC_CHECKERS4, "Checkers 4px", "checkers-4"},
      {GST_D3D12_TEST_SRC_CHECKERS8, "Checkers 8px", "checkers-8"},
      {GST_D3D12_TEST_SRC_CIRCULAR, "Circular", "circular"},
      {GST_D3D12_TEST_SRC_BLINK, "Blink", "blink"},
      {GST_D3D12_TEST_SRC_BALL, "Moving ball", "ball"},
      {0, nullptr, nullptr},
    };

    pattern_type = g_enum_register_static ("GstD3D12TestSrcPattern",
        pattern_types);
  } GST_D3D12_CALL_ONCE_END;

  return pattern_type;
}

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_IS_LIVE,
  PROP_PATTERN,
  PROP_ALPHA,
  PROP_ALPHA_MODE,
};

#define DEFAULT_ADAPTER -1
#define DEFAULT_PATTERN GST_D3D12_TEST_SRC_SMPTE
#define DEFAULT_ALPHA 1.0f

#define ASYNC_DEPTH 2

struct ColorValue
{
  union
  {
    struct
    {
      FLOAT r;
      FLOAT g;
      FLOAT b;
      FLOAT a;
    };
    FLOAT color[4];
  };
};

static const ColorValue color_table[] = {
  /* white */
  {1.0f, 1.0f, 1.0f, 1.0f},
  /* yellow */
  {1.0f, 1.0f, 0.0f, 1.0f},
  /* cyan */
  {0.0f, 1.0f, 1.0f, 1.0f},
  /* green */
  {0.0f, 1.0f, 0.0f, 1.0f},
  /* magenta */
  {1.0f, 0.0f, 1.0f, 1.0f},
  /* red */
  {1.0f, 0.0f, 0.0f, 1.0f},
  /* blue */
  {0.0f, 0.0f, 1.0f, 1.0f},
  /* black */
  {0.0f, 0.0f, 0.0f, 1.0f},
  /* -I */
  {0.0, 0.0f, 0.5f, 1.0f},
  /* +Q */
  {0.0f, 0.5, 1.0f, 1.0f},
  /* superblack */
  {0.0f, 0.0f, 0.0f, 1.0f},
  /* 7.421875% grey */
  {19. / 256.0f, 19. / 256.0f, 19. / 256.0, 1.0f},
};

enum
{
  COLOR_WHITE = 0,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_GREEN,
  COLOR_MAGENTA,
  COLOR_RED,
  COLOR_BLUE,
  COLOR_BLACK,
  COLOR_NEG_I,
  COLOR_POS_Q,
  COLOR_SUPER_BLACK,
  COLOR_DARK_GREY,
};

struct SnowConstBuffer
{
  FLOAT time = 0.0f;
  FLOAT alpha = 1.0f;
};

struct CheckerConstBuffer
{
  FLOAT width;
  FLOAT height;
  FLOAT checker_size;
  FLOAT alpha = 1.0f;
};

struct GstD3D12TestSrcQuad
{
  ComPtr < ID3D12RootSignature > rs;
  ComPtr < ID3D12PipelineState > pso;
  ComPtr < ID3D12Resource > vertex_index_buf;
  ComPtr < ID3D12Resource > vertex_index_upload;
  D3D12_VERTEX_BUFFER_VIEW vbv;
  D3D12_INDEX_BUFFER_VIEW ibv;
  guint index_count = 0;
  gboolean is_checker = FALSE;
  gboolean is_snow = FALSE;
  CheckerConstBuffer checker_const_buffer;
  SnowConstBuffer snow_const_buffer;
};

struct StaticColor
{
  ColorValue value;
  gboolean is_valid = FALSE;
};

/* *INDENT-OFF* */
struct RenderContext
{
  RenderContext (GstD3D12Device * dev)
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    device = (GstD3D12Device *) gst_object_ref (dev);
    ca_pool = gst_d3d12_command_allocator_pool_new (device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);
  }

  ~RenderContext ()
  {
    gst_d3d12_device_fence_wait (device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        fence_val, event_handle);

    CloseHandle (event_handle);

    /* releasing d3d12/d3d11/d2d shared resource might not thread safe? */
    gst_d3d12_device_lock (device);
    brush = nullptr;
    d2d_target = nullptr;
    wrapped_texture = nullptr;
    gst_d3d12_device_unlock (device);

    gst_clear_buffer (&render_buffer);

    if (convert_pool) {
      gst_buffer_pool_set_active (convert_pool, FALSE);
      gst_clear_object (&convert_pool);
    }

    gst_clear_object (&ca_pool);
    gst_clear_object (&conv);
    gst_clear_object (&device);
  }

  GstD3D12Device *device = nullptr;
  GstD3D12Converter *conv = nullptr;
  GstBuffer *render_buffer = nullptr;
  GstBufferPool *convert_pool = nullptr;

  ComPtr<IUnknown> d3d11on12;
  ComPtr<ID3D11DeviceContext> d3d11_context;
  ComPtr<ID2D1RenderTarget> d2d_target;
  ComPtr<ID2D1RadialGradientBrush> brush;
  gdouble rad;
  FLOAT x;
  FLOAT y;

  ComPtr<ID3D12Resource> texture;
  ComPtr<ID3D12DescriptorHeap> rtv_heap;
  ComPtr<ID3D11Resource> wrapped_texture;

  ComPtr<ID3D12GraphicsCommandList> cl;
  std::queue<guint64> scheduled;
  GstD3D12CommandAllocatorPool *ca_pool;

  D3D12_VIEWPORT viewport;
  D3D12_RECT scissor_rect;

  StaticColor static_color[2];
  std::vector < std::shared_ptr < GstD3D12TestSrcQuad >> quad;
  GstD3D12TestSrcPattern pattern;
  HANDLE event_handle;
  guint64 fence_val = 0;
};

struct GstD3D12TestSrcPrivate
{
  GstD3D12TestSrcPrivate ()
  {
    fence_data_pool = gst_d3d12_fence_data_pool_new ();
    gst_video_info_init (&info);
  }

  ~GstD3D12TestSrcPrivate ()
  {
    gst_clear_object (&fence_data_pool);
  }

  gboolean downstream_supports_d3d12 = FALSE;

  GstVideoInfo info;

  GstD3D12FenceDataPool *fence_data_pool;

  gint adapter_index = DEFAULT_ADAPTER;
  GstD3D12TestSrcPattern pattern = DEFAULT_PATTERN;
  std::unique_ptr<RenderContext> ctx;
  D3D12_VIEWPORT viewport;
  ComPtr<ID2D1Factory> d2d_factory;
  gfloat alpha = DEFAULT_ALPHA;

  gboolean reverse = FALSE;
  gint64 n_frames = 0;
  gint64 accum_frames = 0;
  GstClockTime accum_rtime = 0;
  GstClockTime running_time = 0;
};
/* *INDENT-ON* */

struct _GstD3D12TestSrc
{
  GstBaseSrc src;

  GstD3D12Device *device;

  GstD3D12TestSrcPrivate *priv;
};

struct UvVertexData
{
  struct
  {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct
  {
    FLOAT u;
    FLOAT v;
  } texture;
};

struct ColorVertexData
{
  struct
  {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct
  {
    FLOAT r;
    FLOAT g;
    FLOAT b;
    FLOAT a;
  } color;
};

static const D3D12_ROOT_SIGNATURE_FLAGS g_rs_flags =
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;


static gboolean
setup_snow_render (GstD3D12TestSrc * self, RenderContext * ctx,
    gboolean on_smpte)
{
  auto priv = self->priv;
  HRESULT hr;
  UvVertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };

  CD3DX12_ROOT_PARAMETER param;
  param.InitAsConstants (2, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
      1, &param, 0, nullptr, g_rs_flags);

  ComPtr < ID3DBlob > rs_blob;
  ComPtr < ID3DBlob > error_blob;
  hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
  if (!gst_d3d12_result (hr, self->device)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();

    GST_ERROR_OBJECT (self, "Couldn't serialize root signature, error: %s",
        GST_STR_NULL (error_msg));
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  ComPtr < ID3D12RootSignature > rs;
  hr = device->CreateRootSignature (0, rs_blob->GetBufferPointer (),
      rs_blob->GetBufferSize (), IID_PPV_ARGS (&rs));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create root signature");
    return FALSE;
  }

  D3D12_INPUT_ELEMENT_DESC input_desc[2];
  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
  pso_desc.pRootSignature = rs.Get ();
  pso_desc.VS.BytecodeLength = sizeof (g_VSMain_coord);
  pso_desc.VS.pShaderBytecode = g_VSMain_coord;
  pso_desc.PS.BytecodeLength = sizeof (g_PSMain_snow);
  pso_desc.PS.pShaderBytecode = g_PSMain_snow;
  pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
  pso_desc.SampleMask = UINT_MAX;
  pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.DepthStencilState.DepthEnable = FALSE;
  pso_desc.DepthStencilState.StencilEnable = FALSE;
  pso_desc.InputLayout.pInputElementDescs = input_desc;
  pso_desc.InputLayout.NumElements = G_N_ELEMENTS (input_desc);
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
  pso_desc.SampleDesc.Count = 1;

  ComPtr < ID3D12PipelineState > pso;
  hr = device->CreateGraphicsPipelineState (&pso_desc, IID_PPV_ARGS (&pso));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  if (on_smpte) {
    FLOAT left, right, top, bottom;
    FLOAT left_u, right_u, top_v, bottom_v;

    left = 0.5f;
    right = 1.0f;
    top = -0.5f;
    bottom = -1.0f;

    left_u = 3.0f / 4.0f;
    right_u = 1.0f;
    top_v = 3.0f / 4.0f;
    bottom_v = 1.0f;

    /* bottom left */
    vertex_data[0].position.x = left;
    vertex_data[0].position.y = bottom;
    vertex_data[0].position.z = 0.0f;
    vertex_data[0].texture.u = left_u;
    vertex_data[0].texture.v = bottom_v;

    /* top left */
    vertex_data[1].position.x = left;
    vertex_data[1].position.y = top;
    vertex_data[1].position.z = 0.0f;
    vertex_data[1].texture.u = left_u;
    vertex_data[1].texture.v = top_v;

    /* top right */
    vertex_data[2].position.x = right;
    vertex_data[2].position.y = top;
    vertex_data[2].position.z = 0.0f;
    vertex_data[2].texture.u = right_u;
    vertex_data[2].texture.v = top_v;

    /* bottom right */
    vertex_data[3].position.x = right;
    vertex_data[3].position.y = bottom;
    vertex_data[3].position.z = 0.0f;
    vertex_data[3].texture.u = right_u;
    vertex_data[3].texture.v = bottom_v;
  } else {
    /* bottom left */
    vertex_data[0].position.x = -1.0f;
    vertex_data[0].position.y = -1.0f;
    vertex_data[0].position.z = 0.0f;
    vertex_data[0].texture.u = 0.0f;
    vertex_data[0].texture.v = 1.0f;
    /* top left */
    vertex_data[1].position.x = -1.0f;
    vertex_data[1].position.y = 1.0f;
    vertex_data[1].position.z = 0.0f;
    vertex_data[1].texture.u = 0.0f;
    vertex_data[1].texture.v = 0.0f;

    /* top right */
    vertex_data[2].position.x = 1.0f;
    vertex_data[2].position.y = 1.0f;
    vertex_data[2].position.z = 0.0f;
    vertex_data[2].texture.u = 1.0f;
    vertex_data[2].texture.v = 0.0f;

    /* bottom right */
    vertex_data[3].position.x = 1.0f;
    vertex_data[3].position.y = -1.0f;
    vertex_data[3].position.z = 0.0f;
    vertex_data[3].texture.u = 1.0f;
    vertex_data[3].texture.v = 1.0f;
  }

  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC buffer_desc =
      CD3DX12_RESOURCE_DESC::Buffer (sizeof (UvVertexData) * 4
      + sizeof (indices));
  ComPtr < ID3D12Resource > vertex_index_upload;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&vertex_index_upload));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  guint8 *data;
  CD3DX12_RANGE range (0, 0);
  hr = vertex_index_upload->Map (0, &range, (void **) &data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map index buffer");
    return FALSE;
  }

  memcpy (data, vertex_data, sizeof (UvVertexData) * 4);
  memcpy (data + sizeof (UvVertexData) * 4, indices, sizeof (indices));
  vertex_index_upload->Unmap (0, nullptr);

  heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  ComPtr < ID3D12Resource > vertex_index_buf;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS (&vertex_index_buf));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D12TestSrcQuad > ();

  quad->rs = rs;
  quad->pso = pso;
  quad->vertex_index_buf = vertex_index_buf;
  quad->vertex_index_upload = vertex_index_upload;
  quad->vbv.BufferLocation = vertex_index_buf->GetGPUVirtualAddress ();
  quad->vbv.SizeInBytes = sizeof (UvVertexData) * 4;
  quad->vbv.StrideInBytes = sizeof (UvVertexData);
  quad->ibv.BufferLocation = quad->vbv.BufferLocation + quad->vbv.SizeInBytes;
  quad->ibv.SizeInBytes = sizeof (indices);
  quad->ibv.Format = DXGI_FORMAT_R16_UINT;
  quad->index_count = 6;
  quad->is_snow = TRUE;
  quad->snow_const_buffer.time = 0;
  quad->snow_const_buffer.alpha = priv->alpha;

  ctx->quad.push_back (quad);

  return TRUE;
}

static gboolean
setup_smpte_render (GstD3D12TestSrc * self, RenderContext * ctx)
{
  auto priv = self->priv;
  HRESULT hr;
  ColorVertexData vertex_data[4 * 20];
  WORD indices[6 * 20];
  guint num_vertex = 0;
  guint num_index = 0;

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
      0, nullptr, 0, nullptr, g_rs_flags);

  ComPtr < ID3DBlob > rs_blob;
  ComPtr < ID3DBlob > error_blob;
  hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
  if (!gst_d3d12_result (hr, self->device)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();

    GST_ERROR_OBJECT (self, "Couldn't serialize root signature, error: %s",
        GST_STR_NULL (error_msg));
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  ComPtr < ID3D12RootSignature > rs;
  hr = device->CreateRootSignature (0, rs_blob->GetBufferPointer (),
      rs_blob->GetBufferSize (), IID_PPV_ARGS (&rs));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create root signature");
    return FALSE;
  }

  D3D12_INPUT_ELEMENT_DESC input_desc[2];
  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "COLOR";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
  pso_desc.pRootSignature = rs.Get ();
  pso_desc.VS.BytecodeLength = sizeof (g_VSMain_color);
  pso_desc.VS.pShaderBytecode = g_VSMain_color;
  pso_desc.PS.BytecodeLength = sizeof (g_PSMain_color);
  pso_desc.PS.pShaderBytecode = g_PSMain_color;
  pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
  pso_desc.SampleMask = UINT_MAX;
  pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.DepthStencilState.DepthEnable = FALSE;
  pso_desc.DepthStencilState.StencilEnable = FALSE;
  pso_desc.InputLayout.pInputElementDescs = input_desc;
  pso_desc.InputLayout.NumElements = G_N_ELEMENTS (input_desc);
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
  pso_desc.SampleDesc.Count = 1;

  ComPtr < ID3D12PipelineState > pso;
  hr = device->CreateGraphicsPipelineState (&pso_desc, IID_PPV_ARGS (&pso));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  /* top row */
  for (guint i = 0; i < 7; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 2.0f / 7.0f;
    guint base = i * 4;
    guint idx_base = i * 6;
    const ColorValue *color = &color_table[i];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = 1.0f;
    bottom = -1.0f / 3.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = priv->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = priv->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = priv->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = priv->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 7;
  num_index += 6 * 7;

  /* middle row */
  for (guint i = 0; i < 7; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 2.0f / 7.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if ((i % 2) != 0)
      color = &color_table[COLOR_BLACK];
    else
      color = &color_table[COLOR_BLUE - i];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = -1.0f / 3.0f;
    bottom = -0.5f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = priv->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = priv->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = priv->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = priv->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 7;
  num_index += 6 * 7;

  /* bottom row, left three */
  for (guint i = 0; i < 3; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 1.0f / 3.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if (i == 0)
      color = &color_table[COLOR_NEG_I];
    else if (i == 1)
      color = &color_table[COLOR_WHITE];
    else
      color = &color_table[COLOR_POS_Q];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = -0.5f;
    bottom = -1.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = priv->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = priv->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = priv->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = priv->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 3;
  num_index += 6 * 3;

  /* bottom row, middle three */
  for (guint i = 0; i < 3; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 1.0f / 6.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if (i == 0)
      color = &color_table[COLOR_SUPER_BLACK];
    else if (i == 1)
      color = &color_table[COLOR_BLACK];
    else
      color = &color_table[COLOR_DARK_GREY];

    left = i * scale;
    right = (i + 1) * scale;
    top = -0.5f;
    bottom = -1.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = priv->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = priv->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = priv->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = priv->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }

  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC buffer_desc =
      CD3DX12_RESOURCE_DESC::Buffer (sizeof (ColorVertexData) * 4 * 20
      + sizeof (WORD) * 6 * 20);
  ComPtr < ID3D12Resource > vertex_index_upload;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&vertex_index_upload));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  guint8 *data;
  CD3DX12_RANGE range (0, 0);
  hr = vertex_index_upload->Map (0, &range, (void **) &data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map index buffer");
    return FALSE;
  }

  memcpy (data, vertex_data, sizeof (ColorVertexData) * 4 * 20);
  memcpy (data + sizeof (ColorVertexData) * 4 * 20, indices,
      sizeof (WORD) * 6 * 20);
  vertex_index_upload->Unmap (0, nullptr);

  heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  ComPtr < ID3D12Resource > vertex_index_buf;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS (&vertex_index_buf));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D12TestSrcQuad > ();

  quad->rs = rs;
  quad->pso = pso;
  quad->vertex_index_buf = vertex_index_buf;
  quad->vertex_index_upload = vertex_index_upload;
  quad->vbv.BufferLocation = vertex_index_buf->GetGPUVirtualAddress ();
  quad->vbv.SizeInBytes = sizeof (ColorVertexData) * 4 * 20;
  quad->vbv.StrideInBytes = sizeof (ColorVertexData);
  quad->ibv.BufferLocation = quad->vbv.BufferLocation + quad->vbv.SizeInBytes;
  quad->ibv.SizeInBytes = sizeof (WORD) * 6 * 20;
  quad->ibv.Format = DXGI_FORMAT_R16_UINT;
  quad->index_count = 6 * 20;

  ctx->quad.push_back (quad);

  return setup_snow_render (self, ctx, TRUE);
}

static gboolean
setup_checker_render (GstD3D12TestSrc * self, RenderContext * ctx,
    guint checker_size)
{
  auto priv = self->priv;
  HRESULT hr;
  UvVertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };

  CD3DX12_ROOT_PARAMETER param;
  param.InitAsConstants (4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
      1, &param, 0, nullptr, g_rs_flags);

  ComPtr < ID3DBlob > rs_blob;
  ComPtr < ID3DBlob > error_blob;
  hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
  if (!gst_d3d12_result (hr, self->device)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();

    GST_ERROR_OBJECT (self, "Couldn't serialize root signature, error: %s",
        GST_STR_NULL (error_msg));
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  ComPtr < ID3D12RootSignature > rs;
  hr = device->CreateRootSignature (0, rs_blob->GetBufferPointer (),
      rs_blob->GetBufferSize (), IID_PPV_ARGS (&rs));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create root signature");
    return FALSE;
  }

  D3D12_INPUT_ELEMENT_DESC input_desc[2];
  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
  pso_desc.pRootSignature = rs.Get ();
  pso_desc.VS.BytecodeLength = sizeof (g_VSMain_coord);
  pso_desc.VS.pShaderBytecode = g_VSMain_coord;
  pso_desc.PS.BytecodeLength = sizeof (g_PSMain_checker);
  pso_desc.PS.pShaderBytecode = g_PSMain_checker;
  pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
  pso_desc.SampleMask = UINT_MAX;
  pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.DepthStencilState.DepthEnable = FALSE;
  pso_desc.DepthStencilState.StencilEnable = FALSE;
  pso_desc.InputLayout.pInputElementDescs = input_desc;
  pso_desc.InputLayout.NumElements = G_N_ELEMENTS (input_desc);
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
  pso_desc.SampleDesc.Count = 1;

  ComPtr < ID3D12PipelineState > pso;
  hr = device->CreateGraphicsPipelineState (&pso_desc, IID_PPV_ARGS (&pso));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  /* bottom left */
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.u = 0.0f;
  vertex_data[0].texture.v = 1.0f;
  /* top left */
  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.u = 0.0f;
  vertex_data[1].texture.v = 0.0f;

  /* top right */
  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.u = 1.0f;
  vertex_data[2].texture.v = 0.0f;

  /* bottom right */
  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.u = 1.0f;
  vertex_data[3].texture.v = 1.0f;

  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC buffer_desc =
      CD3DX12_RESOURCE_DESC::Buffer (sizeof (UvVertexData) * 4
      + sizeof (indices));
  ComPtr < ID3D12Resource > vertex_index_upload;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS (&vertex_index_upload));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  guint8 *data;
  CD3DX12_RANGE range (0, 0);
  hr = vertex_index_upload->Map (0, &range, (void **) &data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map index buffer");
    return FALSE;
  }

  memcpy (data, vertex_data, sizeof (UvVertexData) * 4);
  memcpy (data + sizeof (UvVertexData) * 4, indices, sizeof (indices));
  vertex_index_upload->Unmap (0, nullptr);

  heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  ComPtr < ID3D12Resource > vertex_index_buf;
  hr = device->CreateCommittedResource (&heap_prop,
      D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
      &buffer_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS (&vertex_index_buf));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create index buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D12TestSrcQuad > ();

  quad->rs = rs;
  quad->pso = pso;
  quad->vertex_index_buf = vertex_index_buf;
  quad->vertex_index_upload = vertex_index_upload;
  quad->vbv.BufferLocation = vertex_index_buf->GetGPUVirtualAddress ();
  quad->vbv.SizeInBytes = sizeof (UvVertexData) * 4;
  quad->vbv.StrideInBytes = sizeof (UvVertexData);
  quad->ibv.BufferLocation = quad->vbv.BufferLocation + quad->vbv.SizeInBytes;
  quad->ibv.SizeInBytes = sizeof (indices);
  quad->ibv.Format = DXGI_FORMAT_R16_UINT;
  quad->index_count = 6;
  quad->is_checker = TRUE;
  quad->checker_const_buffer.width = priv->info.width;
  quad->checker_const_buffer.height = priv->info.height;
  quad->checker_const_buffer.checker_size = checker_size;
  quad->checker_const_buffer.alpha = priv->alpha;

  ctx->quad.push_back (quad);

  return TRUE;
}

static gboolean
setup_d2d_render (GstD3D12TestSrc * self, RenderContext * ctx)
{
  auto priv = self->priv;
  HRESULT hr;

  if (!priv->d2d_factory) {
    ComPtr < ID2D1Factory > d2d_factory;
    hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
        IID_PPV_ARGS (&d2d_factory));

    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create D2D factory");
      return FALSE;
    }

    priv->d2d_factory = d2d_factory;
  }

  hr = gst_d3d12_device_get_d3d11on12_device (self->device, &ctx->d3d11on12);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get d3d11on12 device");
    return FALSE;
  }

  ComPtr < ID3D11Device > d3d11dev;
  hr = ctx->d3d11on12.As (&d3d11dev);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get d3d11 device");
    return FALSE;
  }

  d3d11dev->GetImmediateContext (&ctx->d3d11_context);

  hr = GstD3D11On12CreateWrappedResource (ctx->d3d11on12.Get (),
      ctx->texture.Get (),
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      D3D11_RESOURCE_MISC_SHARED, 0, 0,
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &ctx->wrapped_texture);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create wrapped resource");
    return FALSE;
  }

  ComPtr < IDXGISurface > surface;
  hr = ctx->wrapped_texture.As (&surface);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get DXGI surface");
    return FALSE;
  }

  D2D1_RENDER_TARGET_PROPERTIES props = { };
  props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
  props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  props.dpiX = 0;
  props.dpiY = 0;
  props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
  props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

  hr = priv->d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (),
      &props, &ctx->d2d_target);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create d2d render target");
    return FALSE;
  }

  switch (ctx->pattern) {
    case GST_D3D12_TEST_SRC_BALL:
    {
      D2D1_GRADIENT_STOP stops[3];
      stops[0].color = D2D1::ColorF (D2D1::ColorF::White, priv->alpha);
      stops[0].position = 0.0f;
      stops[1].color = D2D1::ColorF (D2D1::ColorF::Snow, priv->alpha);
      stops[1].position = 0.3f;
      stops[2].color = D2D1::ColorF (D2D1::ColorF::Black, priv->alpha);
      stops[2].position = 1.0f;

      ComPtr < ID2D1GradientStopCollection > collection;
      hr = ctx->d2d_target->CreateGradientStopCollection (stops,
          G_N_ELEMENTS (stops), D2D1_GAMMA_1_0, D2D1_EXTEND_MODE_CLAMP,
          &collection);
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create stop collection");
        return FALSE;
      }

      hr = ctx->
          d2d_target->CreateRadialGradientBrush (D2D1::
          RadialGradientBrushProperties (D2D1::Point2F (0, 0), D2D1::Point2F (0,
                  0), 20, 20), collection.Get (), &ctx->brush);
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create brush");
        return FALSE;
      }
      break;
    }
    case GST_D3D12_TEST_SRC_CIRCULAR:
    {
      D2D1_GRADIENT_STOP stops[129];
      FLOAT position = 1.0f;

      for (guint i = 0; i < G_N_ELEMENTS (stops); i++) {
        FLOAT diff;
        if ((i % 2) == 0)
          stops[i].color = D2D1::ColorF (D2D1::ColorF::Black, priv->alpha);
        else
          stops[i].color = D2D1::ColorF (D2D1::ColorF::White, priv->alpha);

        stops[i].position = position;
        diff = position / G_N_ELEMENTS (stops) * 2;
        position -= diff;
      }

      ComPtr < ID2D1GradientStopCollection > collection;
      hr = ctx->d2d_target->CreateGradientStopCollection (stops,
          G_N_ELEMENTS (stops), D2D1_GAMMA_1_0, D2D1_EXTEND_MODE_CLAMP,
          &collection);
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create stop collection");
        return FALSE;
      }

      ctx->x = priv->info.width / 2;
      ctx->y = priv->info.height / 2;
      ctx->rad = MAX (ctx->x, ctx->y);

      hr = ctx->
          d2d_target->CreateRadialGradientBrush (D2D1::
          RadialGradientBrushProperties (D2D1::Point2F (ctx->x, ctx->y),
              D2D1::Point2F (0, 0), ctx->rad, ctx->rad), collection.Get (),
          &ctx->brush);
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create brush");
        return FALSE;
      }
      break;
    }
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static void gst_d3d12_test_src_finalize (GObject * object);
static void gst_d3d12_test_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d12_test_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void gst_d3d12_test_src_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_d3d12_test_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_d3d12_test_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static GstCaps *gst_d3d12_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_d3d12_test_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_d3d12_test_src_decide_allocation (GstBaseSrc *
    bsrc, GstQuery * query);
static gboolean gst_d3d12_test_src_start (GstBaseSrc * bsrc);
static gboolean gst_d3d12_test_src_stop (GstBaseSrc * bsrc);
static gboolean gst_d3d12_test_src_src_query (GstBaseSrc * bsrc,
    GstQuery * query);
static void gst_d3d12_test_src_get_times (GstBaseSrc * bsrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static GstFlowReturn gst_d3d12_test_src_create (GstBaseSrc * bsrc,
    guint64 offset, guint size, GstBuffer ** buf);

#define gst_d3d12_test_src_parent_class parent_class
G_DEFINE_TYPE (GstD3D12TestSrc, gst_d3d12_test_src, GST_TYPE_BASE_SRC);

static void
gst_d3d12_test_src_class_init (GstD3D12TestSrcClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto element_class = GST_ELEMENT_CLASS (klass);
  auto basesrc_class = GST_BASE_SRC_CLASS (klass);

  object_class->finalize = gst_d3d12_test_src_finalize;
  object_class->set_property = gst_d3d12_test_src_set_property;
  object_class->get_property = gst_d3d12_test_src_get_property;

  g_object_class_install_property (object_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "DXGI Adapter index (-1 for any device)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_PATTERN,
      g_param_spec_enum ("pattern", "Pattern",
          "Type of test pattern to generate", GST_TYPE_D3D12_TEST_SRC_PATTERN,
          DEFAULT_PATTERN,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_ALPHA,
      g_param_spec_float ("alpha", "Alpha", "Global alpha value to use",
          0, 1, DEFAULT_ALPHA,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d12_test_src_set_context);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D12 Test Source", "Source/Video",
      "Creates a test video stream", "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &src_template);

  basesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_d3d12_test_src_is_seekable);
  basesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_do_seek);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_fixate);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_set_caps);
  basesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_test_src_decide_allocation);
  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_stop);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_src_query);
  basesrc_class->get_times = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_get_times);
  basesrc_class->create = GST_DEBUG_FUNCPTR (gst_d3d12_test_src_create);

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_test_src_debug, "d3d12testsrc", 0,
      "d3d12testsrc");

  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_TEST_SRC_PATTERN,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d12_test_src_init (GstD3D12TestSrc * self)
{
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);

  self->priv = new GstD3D12TestSrcPrivate ();
}

static void
gst_d3d12_test_src_finalize (GObject * object)
{
  auto self = GST_D3D12_TEST_SRC (object);

  delete self->priv;
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_TEST_SRC (object);
  auto priv = self->priv;

  switch (prop_id) {
    case PROP_ADAPTER:
      priv->adapter_index = g_value_get_int (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (self), g_value_get_boolean (value));
      break;
    case PROP_PATTERN:
      priv->pattern = (GstD3D12TestSrcPattern) g_value_get_enum (value);
      break;
    case PROP_ALPHA:
      priv->alpha = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_TEST_SRC (object);
  auto priv = self->priv;

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, priv->adapter_index);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (self)));
      break;
    case PROP_PATTERN:
      g_value_set_enum (value, priv->pattern);
      break;
    case PROP_ALPHA:
      g_value_set_float (value, priv->alpha);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_test_src_set_context (GstElement * element, GstContext * context)
{
  auto self = GST_D3D12_TEST_SRC (element);
  auto priv = self->priv;

  gst_d3d12_handle_set_context (element,
      context, priv->adapter_index, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d12_test_src_is_seekable (GstBaseSrc * bsrc)
{
  return TRUE;
}

static gboolean
gst_d3d12_test_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;
  GstClockTime position;

  segment->time = segment->start;
  position = segment->position;
  priv->reverse = segment->rate < 0;

  /* now move to the position indicated */
  if (priv->info.fps_n) {
    priv->n_frames = gst_util_uint64_scale (position,
        priv->info.fps_n, priv->info.fps_d * GST_SECOND);
  } else {
    priv->n_frames = 0;
  }
  priv->accum_frames = 0;
  priv->accum_rtime = 0;
  if (priv->info.fps_n) {
    priv->running_time = gst_util_uint64_scale (priv->n_frames,
        priv->info.fps_d * GST_SECOND, priv->info.fps_n);
  } else {
    /* FIXME : Not sure what to set here */
    priv->running_time = 0;
  }

  return TRUE;
}

static GstCaps *
gst_d3d12_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *s;

  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (s, "width", 320);
  gst_structure_fixate_field_nearest_int (s, "height", 240);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", 30, 1);

  return GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);
}

static gboolean
gst_d3d12_test_src_setup_context (GstD3D12TestSrc * self, GstCaps * caps)
{
  auto priv = self->priv;
  GstVideoInfo draw_info;
  HRESULT hr;
  GstStructure *config = nullptr;

  auto ctx = std::make_unique < RenderContext > (self->device);

  /* D2D uses premultiplied alpha */
  if (priv->pattern == GST_D3D12_TEST_SRC_CIRCULAR ||
      priv->pattern == GST_D3D12_TEST_SRC_BALL) {
    config = gst_structure_new ("converter-config",
        GST_D3D12_CONVERTER_OPT_SRC_ALPHA_MODE,
        GST_TYPE_D3D12_CONVERTER_ALPHA_MODE,
        GST_D3D12_CONVERTER_ALPHA_MODE_PREMULTIPLIED, nullptr);
  }

  gst_video_info_set_format (&draw_info, GST_VIDEO_FORMAT_BGRA,
      priv->info.width, priv->info.height);
  ctx->conv = gst_d3d12_converter_new (self->device,
      &draw_info, &priv->info, nullptr, nullptr, config);

  if (!ctx->conv) {
    GST_ERROR_OBJECT (self, "Failed to create converter");
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC resource_desc =
      CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_B8G8R8A8_UNORM,
      priv->info.width, priv->info.height, 1, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
      D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
  D3D12_CLEAR_VALUE clear_value = { };
  D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
  clear_value.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  clear_value.Color[0] = 0.0f;
  clear_value.Color[1] = 0.0f;
  clear_value.Color[2] = 0.0f;
  clear_value.Color[3] = 1.0f;

  switch (priv->pattern) {
    case GST_D3D12_TEST_SRC_WHITE:
      memcpy (clear_value.Color, color_table[COLOR_WHITE].color,
          sizeof (FLOAT) * 4);
      break;
    case GST_D3D12_TEST_SRC_RED:
      memcpy (clear_value.Color, color_table[COLOR_RED].color,
          sizeof (FLOAT) * 4);
      break;
    case GST_D3D12_TEST_SRC_GREEN:
      memcpy (clear_value.Color, color_table[COLOR_GREEN].color,
          sizeof (FLOAT) * 4);
      break;
    case GST_D3D12_TEST_SRC_BLUE:
      memcpy (clear_value.Color, color_table[COLOR_BLUE].color,
          sizeof (FLOAT) * 4);
      break;
    case GST_D3D12_TEST_SRC_CIRCULAR:
    case GST_D3D12_TEST_SRC_BALL:
      heap_flags |= D3D12_HEAP_FLAG_SHARED;
      break;
    default:
      break;
  }

  hr = device->CreateCommittedResource (&heap_prop, heap_flags,
      &resource_desc, D3D12_RESOURCE_STATE_COMMON, &clear_value,
      IID_PPV_ARGS (&ctx->texture));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create texture");
    return FALSE;
  }

  auto mem = gst_d3d12_allocator_alloc_wrapped (nullptr, self->device,
      ctx->texture.Get (), 0);
  if (!mem) {
    GST_ERROR_OBJECT (self, "Couldn't wrap texture");
    return FALSE;
  }

  if (!gst_d3d12_memory_get_render_target_view_heap ((GstD3D12Memory *) mem,
          &ctx->rtv_heap)) {
    GST_ERROR_OBJECT (self, "Couldn't get rtv heap");
    gst_memory_unref (mem);
    return FALSE;
  }

  ctx->render_buffer = gst_buffer_new ();
  gst_buffer_append_memory (ctx->render_buffer, mem);

  if (!priv->downstream_supports_d3d12) {
    ctx->convert_pool = gst_d3d12_buffer_pool_new (self->device);
    config = gst_buffer_pool_get_config (ctx->convert_pool);
    gst_buffer_pool_config_set_params (config, caps, priv->info.size, 0, 0);
    auto params = gst_d3d12_allocation_params_new (self->device, &priv->info,
        GST_D3D12_ALLOCATION_FLAG_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
    gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
    gst_d3d12_allocation_params_free (params);

    if (!gst_buffer_pool_set_config (ctx->convert_pool, config)) {
      GST_ERROR_OBJECT (self, "Couldn't set pool config");
      return FALSE;
    }

    if (!gst_buffer_pool_set_active (ctx->convert_pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Couldn't configure buffer pool");
      return FALSE;
    }
  }

  ctx->viewport.TopLeftX = 0;
  ctx->viewport.TopLeftY = 0;
  ctx->viewport.Width = priv->info.width;
  ctx->viewport.Height = priv->info.height;
  ctx->viewport.MinDepth = 0.0f;
  ctx->viewport.MaxDepth = 1.0f;

  ctx->scissor_rect.left = 0;
  ctx->scissor_rect.top = 0;
  ctx->scissor_rect.right = priv->info.width;
  ctx->scissor_rect.bottom = priv->info.height;

  ctx->pattern = priv->pattern;

  switch (priv->pattern) {
    case GST_D3D12_TEST_SRC_SMPTE:
      if (!setup_smpte_render (self, ctx.get ()))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_SNOW:
      if (!setup_snow_render (self, ctx.get (), FALSE))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_BLACK:
      ctx->static_color[0].value = color_table[COLOR_BLACK];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_WHITE:
      ctx->static_color[0].value = color_table[COLOR_WHITE];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_RED:
      ctx->static_color[0].value = color_table[COLOR_RED];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_GREEN:
      ctx->static_color[0].value = color_table[COLOR_GREEN];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_BLUE:
      ctx->static_color[0].value = color_table[COLOR_BLUE];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_CHECKERS1:
      if (!setup_checker_render (self, ctx.get (), 1))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_CHECKERS2:
      if (!setup_checker_render (self, ctx.get (), 2))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_CHECKERS4:
      if (!setup_checker_render (self, ctx.get (), 4))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_CHECKERS8:
      if (!setup_checker_render (self, ctx.get (), 8))
        return FALSE;
      break;
    case GST_D3D12_TEST_SRC_BLINK:
      ctx->static_color[0].value = color_table[COLOR_BLACK];
      ctx->static_color[0].value.a = priv->alpha;
      ctx->static_color[0].is_valid = TRUE;
      ctx->static_color[1].value = color_table[COLOR_WHITE];
      ctx->static_color[1].value.a = priv->alpha;
      ctx->static_color[1].is_valid = TRUE;
      break;
    case GST_D3D12_TEST_SRC_CIRCULAR:
    case GST_D3D12_TEST_SRC_BALL:
      if (!setup_d2d_render (self, ctx.get ()))
        return FALSE;
      break;
  }

  priv->ctx = std::move (ctx);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Set caps %" GST_PTR_FORMAT, caps);

  priv->ctx = nullptr;

  auto features = gst_caps_get_features (caps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY)) {
    priv->downstream_supports_d3d12 = TRUE;
  } else {
    priv->downstream_supports_d3d12 = FALSE;
  }

  GST_OBJECT_LOCK (self);
  gst_video_info_from_caps (&priv->info, caps);
  GST_OBJECT_UNLOCK (self);
  if (priv->info.fps_d <= 0 || priv->info.fps_n <= 0) {
    GST_ERROR_OBJECT (self, "Invalid framerate %d/%d", priv->info.fps_n,
        priv->info.fps_d);
    return FALSE;
  }

  gst_base_src_set_blocksize (bsrc, GST_VIDEO_INFO_SIZE (&priv->info));

  return gst_d3d12_test_src_setup_context (self, caps);
}

static gboolean
gst_d3d12_test_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;
  GstBufferPool *pool = nullptr;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;
  GstVideoInfo vinfo;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_ERROR_OBJECT (self, "No output caps");
    return FALSE;
  }

  gst_video_info_from_caps (&vinfo, caps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    size = GST_VIDEO_INFO_SIZE (&vinfo);

    min = max = 0;
    update_pool = FALSE;
  }

  if (pool && priv->downstream_supports_d3d12) {
    if (!GST_IS_D3D12_BUFFER_POOL (pool)) {
      gst_clear_object (&pool);
    } else {
      GstD3D12BufferPool *dpool = GST_D3D12_BUFFER_POOL (pool);
      if (dpool->device != self->device)
        gst_clear_object (&pool);
    }
  }

  if (!pool) {
    if (priv->downstream_supports_d3d12)
      pool = gst_d3d12_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();
  }

  auto config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (priv->downstream_supports_d3d12) {
    D3D12_RESOURCE_FLAGS resource_flags =
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS |
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    auto params = gst_buffer_pool_config_get_d3d12_allocation_params (config);
    if (!params) {
      params = gst_d3d12_allocation_params_new (self->device, &vinfo,
          GST_D3D12_ALLOCATION_FLAG_DEFAULT, resource_flags);
    } else {
      gst_d3d12_allocation_params_set_resource_flags (params, resource_flags);
      gst_d3d12_allocation_params_unset_resource_flags (params,
          D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
    }

    gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
    gst_d3d12_allocation_params_free (params);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set config");
    gst_clear_object (&pool);
    return FALSE;
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_start (GstBaseSrc * bsrc)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;

  if (!gst_d3d12_ensure_element_data (GST_ELEMENT (bsrc), priv->adapter_index,
          &self->device)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Failed to prepare device"), (nullptr));
    return FALSE;
  }

  priv->running_time = 0;
  priv->reverse = FALSE;
  priv->n_frames = 0;
  priv->accum_frames = 0;
  priv->accum_rtime = 0;

  gst_video_info_init (&priv->info);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_stop (GstBaseSrc * bsrc)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;

  priv->ctx = nullptr;
  priv->d2d_factory = nullptr;
  gst_clear_object (&self->device);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d12_handle_context_query (GST_ELEMENT_CAST (self), query,
              self->device)) {
        return TRUE;
      }
      break;
    case GST_QUERY_LATENCY:
      GST_OBJECT_LOCK (self);
      if (priv->info.fps_n > 0 && priv->info.fps_d > 0) {
        GstClockTime latency;

        latency =
            gst_util_uint64_scale (GST_SECOND, priv->info.fps_d,
            priv->info.fps_n);
        GST_OBJECT_UNLOCK (self);
        gst_query_set_latency (query,
            gst_base_src_is_live (bsrc), latency, GST_CLOCK_TIME_NONE);
        GST_DEBUG_OBJECT (self, "Reporting latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency));
        return TRUE;
      }
      GST_OBJECT_UNLOCK (self);
      break;
    case GST_QUERY_DURATION:
      if (bsrc->num_buffers > 0) {
        GstFormat format;

        gst_query_parse_duration (query, &format, nullptr);
        if (format != GST_FORMAT_TIME)
          return FALSE;

        GST_OBJECT_LOCK (self);
        if (format == GST_FORMAT_TIME && priv->info.fps_n > 0 &&
            priv->info.fps_d > 0) {
          gint64 dur;
          dur = gst_util_uint64_scale_int_round (bsrc->num_buffers
              * GST_SECOND, priv->info.fps_d, priv->info.fps_n);
          gst_query_set_duration (query, GST_FORMAT_TIME, dur);
          GST_OBJECT_UNLOCK (self);
          return TRUE;
        }

        GST_OBJECT_UNLOCK (self);
      }
      break;
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
}

static void
gst_d3d12_test_src_get_times (GstBaseSrc * bsrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (bsrc)) {
    GstClockTime timestamp = GST_BUFFER_PTS (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_d3d12_test_src_draw_ball (GstD3D12TestSrc * self)
{
  auto priv = self->priv;
  gdouble rad;
  FLOAT x, y;

  rad = (gdouble) priv->n_frames / 200;
  rad = 2 * G_PI * rad;
  x = 20 + (0.5 + 0.5 * sin (rad)) * (priv->info.width - 40);
  y = 20 + (0.5 + 0.5 * sin (rad * sqrt (2))) * (priv->info.height - 40);

  gst_d3d12_device_lock (self->device);
  ID3D11Resource *resources[] = { priv->ctx->wrapped_texture.Get () };

  GstD3D11On12AcquireWrappedResource (priv->ctx->d3d11on12.Get (), resources,
      1);

  priv->ctx->brush->SetCenter (D2D1::Point2F (x, y));
  priv->ctx->d2d_target->BeginDraw ();
  priv->ctx->d2d_target->Clear (D2D1::ColorF (D2D1::ColorF::Black));
  priv->ctx->d2d_target->FillEllipse (D2D1::Ellipse (D2D1::Point2F (x, y),
          20, 20), priv->ctx->brush.Get ());
  priv->ctx->d2d_target->EndDraw ();

  GstD3D11On12ReleaseWrappedResource (priv->ctx->d3d11on12.Get (), resources,
      1);

  priv->ctx->d3d11_context->Flush ();
  gst_d3d12_device_unlock (self->device);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_draw_circular (GstD3D12TestSrc * self)
{
  auto priv = self->priv;

  gst_d3d12_device_lock (self->device);
  ID3D11Resource *resources[] = { priv->ctx->wrapped_texture.Get () };

  GstD3D11On12AcquireWrappedResource (priv->ctx->d3d11on12.Get (), resources,
      1);
  priv->ctx->d2d_target->BeginDraw ();
  priv->ctx->d2d_target->Clear (D2D1::ColorF (D2D1::ColorF::Black));
  priv->ctx->d2d_target->FillEllipse (D2D1::Ellipse (D2D1::Point2F (priv->
              ctx->x, priv->ctx->y), priv->ctx->rad, priv->ctx->rad),
      priv->ctx->brush.Get ());
  priv->ctx->d2d_target->EndDraw ();

  GstD3D11On12ReleaseWrappedResource (priv->ctx->d3d11on12.Get (), resources,
      1);

  priv->ctx->d3d11_context->Flush ();

  gst_d3d12_device_unlock (self->device);

  return TRUE;
}

static gboolean
gst_d3d12_test_src_draw_pattern (GstD3D12TestSrc * self, GstClockTime pts,
    ID3D12GraphicsCommandList * cl)
{
  auto priv = self->priv;
  auto ctx = priv->ctx.get ();
  D3D12_RESOURCE_BARRIER barrier;

  if (ctx->pattern == GST_D3D12_TEST_SRC_BALL)
    return gst_d3d12_test_src_draw_ball (self);
  else if (ctx->pattern == GST_D3D12_TEST_SRC_CIRCULAR)
    return gst_d3d12_test_src_draw_circular (self);

  if (ctx->static_color[0].is_valid) {
    if (ctx->static_color[1].is_valid && (priv->n_frames % 2) == 1) {
      cl->ClearRenderTargetView (ctx->
          rtv_heap->GetCPUDescriptorHandleForHeapStart (),
          ctx->static_color[1].value.color, 0, nullptr);
    } else {
      cl->ClearRenderTargetView (ctx->
          rtv_heap->GetCPUDescriptorHandleForHeapStart (),
          ctx->static_color[0].value.color, 0, nullptr);
    }
  } else {
    cl->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->RSSetViewports (1, &ctx->viewport);
    cl->RSSetScissorRects (1, &ctx->scissor_rect);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_heaps[] = {
      priv->ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart ()
    };
    cl->OMSetRenderTargets (1, rtv_heaps, FALSE, nullptr);

    for (size_t i = 0; i < ctx->quad.size (); i++) {
      auto quad = ctx->quad[i];
      if (priv->ctx->fence_val == 0) {
        cl->CopyResource (quad->vertex_index_buf.Get (),
            quad->vertex_index_upload.Get ());
        barrier =
            CD3DX12_RESOURCE_BARRIER::Transition (quad->vertex_index_buf.Get (),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        cl->ResourceBarrier (1, &barrier);
      }

      cl->SetGraphicsRootSignature (quad->rs.Get ());
      if (quad->is_snow) {
        quad->snow_const_buffer.time = (FLOAT) pts / GST_SECOND;
        quad->snow_const_buffer.alpha = priv->alpha;
        cl->SetGraphicsRoot32BitConstants (0, 2, &quad->snow_const_buffer, 0);
      } else if (quad->is_checker) {
        quad->checker_const_buffer.alpha = priv->alpha;
        cl->SetGraphicsRoot32BitConstants (0, 4, &quad->checker_const_buffer,
            0);
      }

      cl->SetPipelineState (quad->pso.Get ());
      cl->IASetIndexBuffer (&quad->ibv);
      cl->IASetVertexBuffers (0, 1, &quad->vbv);
      cl->DrawIndexedInstanced (quad->index_count, 1, 0, 0, 0);
    }
  }

  barrier = CD3DX12_RESOURCE_BARRIER::Transition (ctx->texture.Get (),
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cl->ResourceBarrier (1, &barrier);

  return TRUE;
}

static GstFlowReturn
gst_d3d12_test_src_create (GstBaseSrc * bsrc, guint64 offset,
    guint size, GstBuffer ** buf)
{
  auto self = GST_D3D12_TEST_SRC (bsrc);
  auto priv = self->priv;
  GstBuffer *buffer = nullptr;
  GstBuffer *convert_buffer = nullptr;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstClockTime pts;
  GstClockTime next_time;

  if (priv->downstream_supports_d3d12) {
    ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc,
        offset, size, &convert_buffer);
  } else {
    ret = gst_buffer_pool_acquire_buffer (priv->ctx->convert_pool,
        &convert_buffer, nullptr);
  }

  if (ret != GST_FLOW_OK)
    return ret;

  auto completed = gst_d3d12_device_get_completed_value (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);
  while (!priv->ctx->scheduled.empty ()) {
    if (priv->ctx->scheduled.front () > completed)
      break;

    priv->ctx->scheduled.pop ();
  }

  if (priv->ctx->scheduled.size () >= ASYNC_DEPTH) {
    auto fence_to_wait = priv->ctx->scheduled.front ();
    priv->ctx->scheduled.pop ();
    gst_d3d12_device_fence_wait (self->device,
        D3D12_COMMAND_LIST_TYPE_DIRECT, fence_to_wait, priv->ctx->event_handle);
  }

  GstD3D12CommandAllocator *gst_ca;
  if (!gst_d3d12_command_allocator_pool_acquire (priv->ctx->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    gst_clear_buffer (&convert_buffer);
    return GST_FLOW_ERROR;
  }

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  auto hr = ca->Reset ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command allocator");
    gst_d3d12_command_allocator_unref (gst_ca);
    gst_clear_buffer (&convert_buffer);
    return GST_FLOW_ERROR;
  }

  if (!priv->ctx->cl) {
    auto device = gst_d3d12_device_get_device_handle (self->device);
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), nullptr, IID_PPV_ARGS (&priv->ctx->cl));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't reset command list");
      gst_d3d12_command_allocator_unref (gst_ca);
      gst_clear_buffer (&convert_buffer);
      return GST_FLOW_ERROR;
    }
  } else {
    hr = priv->ctx->cl->Reset (ca.Get (), nullptr);
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't reset command list");
      gst_d3d12_command_allocator_unref (gst_ca);
      gst_clear_buffer (&convert_buffer);
      return GST_FLOW_ERROR;
    }
  }

  auto cl = priv->ctx->cl;
  GstD3D12FenceData *fence_data;
  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  pts = priv->accum_rtime + priv->running_time;
  gst_d3d12_test_src_draw_pattern (self, pts, cl.Get ());

  if (!gst_d3d12_converter_convert_buffer (priv->ctx->conv,
          priv->ctx->render_buffer, convert_buffer, fence_data, cl.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't build convert command");
    gst_clear_buffer (&convert_buffer);
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  hr = cl->Close ();

  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    gst_clear_buffer (&convert_buffer);
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  ID3D12CommandList *cmd_list[] = { priv->ctx->cl.Get () };

  if (!gst_d3d12_device_execute_command_lists (self->device,
          D3D12_COMMAND_LIST_TYPE_DIRECT, 1, cmd_list, &priv->ctx->fence_val)) {
    GST_ERROR_OBJECT (self, "Couldn't execute command list");
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  gst_d3d12_buffer_after_write (convert_buffer, priv->ctx->fence_val);

  gst_d3d12_device_set_fence_notify (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT, priv->ctx->fence_val, fence_data);

  priv->ctx->scheduled.push (priv->ctx->fence_val);

  if (priv->downstream_supports_d3d12) {
    buffer = convert_buffer;
    convert_buffer = nullptr;
  } else {
    ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc,
        offset, size, &buffer);
    if (ret != GST_FLOW_OK) {
      gst_buffer_unref (convert_buffer);
      return ret;
    }

    GstVideoFrame src_frame, dst_frame;
    if (!gst_video_frame_map (&src_frame, &priv->info, convert_buffer,
            GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "Couldn't map convert buffer");
      gst_buffer_unref (convert_buffer);
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }

    if (!gst_video_frame_map (&dst_frame, &priv->info, buffer, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (self, "Couldn't map output buffer");
      gst_video_frame_unmap (&src_frame);
      gst_buffer_unref (convert_buffer);
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }

    auto copy_ret = gst_video_frame_copy (&dst_frame, &src_frame);
    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&dst_frame);
    gst_buffer_unref (convert_buffer);

    if (!copy_ret) {
      GST_ERROR_OBJECT (self, "Couldn't copy frame");
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }
  }

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buffer) = priv->accum_frames + priv->n_frames;
  if (priv->reverse) {
    priv->n_frames--;
  } else {
    priv->n_frames++;
  }
  GST_BUFFER_OFFSET_END (buffer) = GST_BUFFER_OFFSET (buffer) + 1;

  next_time = gst_util_uint64_scale (priv->n_frames,
      priv->info.fps_d * GST_SECOND, priv->info.fps_n);
  if (priv->reverse) {
    /* We already decremented to next frame */
    GstClockTime prev_pts = gst_util_uint64_scale (priv->n_frames + 2,
        priv->info.fps_d * GST_SECOND, priv->info.fps_n);

    GST_BUFFER_DURATION (buffer) = prev_pts - GST_BUFFER_PTS (buffer);
  } else {
    GST_BUFFER_DURATION (buffer) = next_time - priv->running_time;
  }

  priv->running_time = next_time;
  *buf = buffer;

  return GST_FLOW_OK;
}
