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
 * SECTION:element-d3d12compositor
 * @title: d3d12compositor
 *
 * A Direct3D12 based video compositing element.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d12compositor name=c ! d3d12videosink \
 *     videotestsrc ! video/x-raw,width=320,height=240 ! c. \
 *     videotestsrc pattern=ball ! video/x-raw,width=100,height=100 ! c.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12compositor.h"
#include "gstd3d12pluginutils.h"
#include <directx/d3dx12.h>
#include <mutex>
#include <future>
#include <vector>
#include <queue>
#include <string.h>
#include <wrl.h>
#include "PSMain_checker_luma.h"
#include "PSMain_checker_rgb.h"
#include "PSMain_checker_vuya.h"
#include "VSMain_pos.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_compositor_debug);
#define GST_CAT_DEFAULT gst_d3d12_compositor_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

enum GstD3D12CompositorBackground
{
  GST_D3D12_COMPOSITOR_BACKGROUND_CHECKER,
  GST_D3D12_COMPOSITOR_BACKGROUND_BLACK,
  GST_D3D12_COMPOSITOR_BACKGROUND_WHITE,
  GST_D3D12_COMPOSITOR_BACKGROUND_TRANSPARENT,
};

#define GST_TYPE_D3D12_COMPOSITOR_BACKGROUND (gst_d3d12_compositor_background_get_type())
static GType
gst_d3d12_compositor_background_get_type (void)
{
  static GType compositor_background_type = 0;
  static const GEnumValue compositor_background[] = {
    {GST_D3D12_COMPOSITOR_BACKGROUND_CHECKER, "Checker pattern", "checker"},
    {GST_D3D12_COMPOSITOR_BACKGROUND_BLACK, "Black", "black"},
    {GST_D3D12_COMPOSITOR_BACKGROUND_WHITE, "White", "white"},
    {GST_D3D12_COMPOSITOR_BACKGROUND_TRANSPARENT,
        "Transparent Background to enable further compositing", "transparent"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    compositor_background_type =
        g_enum_register_static ("GstD3D12CompositorBackground",
        compositor_background);
  } GST_D3D12_CALL_ONCE_END;

  return compositor_background_type;
}

enum GstD3D12CompositorOperator
{
  GST_D3D12_COMPOSITOR_OPERATOR_SOURCE,
  GST_D3D12_COMPOSITOR_OPERATOR_OVER,
};

#define GST_TYPE_D3D12_COMPOSITOR_OPERATOR (gst_d3d12_compositor_operator_get_type())
static GType
gst_d3d12_compositor_operator_get_type (void)
{
  static GType compositor_operator_type = 0;
  static const GEnumValue compositor_operator[] = {
    {GST_D3D12_COMPOSITOR_OPERATOR_SOURCE, "Source", "source"},
    {GST_D3D12_COMPOSITOR_OPERATOR_OVER, "Over", "over"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    compositor_operator_type =
        g_enum_register_static ("GstD3D12CompositorOperator",
        compositor_operator);
  } GST_D3D12_CALL_ONCE_END;

  return compositor_operator_type;
}

enum GstD3D12CompositorSizingPolicy
{
  GST_D3D12_COMPOSITOR_SIZING_POLICY_NONE,
  GST_D3D12_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO,
};

#define GST_TYPE_D3D12_COMPOSITOR_SIZING_POLICY (gst_d3d12_compositor_sizing_policy_get_type())
static GType
gst_d3d12_compositor_sizing_policy_get_type (void)
{
  static GType sizing_policy_type = 0;

  static const GEnumValue sizing_polices[] = {
    {GST_D3D12_COMPOSITOR_SIZING_POLICY_NONE,
        "None: Image is scaled to fill configured destination rectangle without "
          "padding or keeping the aspect ratio", "none"},
    {GST_D3D12_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO,
          "Keep Aspect Ratio: Image is scaled to fit destination rectangle "
          "specified by GstD3D12CompositorPad:{xpos, ypos, width, height} "
          "with preserved aspect ratio. Resulting image will be centered in "
          "the destination rectangle with padding if necessary",
        "keep-aspect-ratio"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    sizing_policy_type =
        g_enum_register_static ("GstD3D12CompositorSizingPolicy",
        sizing_polices);
  } GST_D3D12_CALL_ONCE_END;

  return sizing_policy_type;
}

enum
{
  PROP_PAD_0,
  PROP_PAD_XPOS,
  PROP_PAD_YPOS,
  PROP_PAD_WIDTH,
  PROP_PAD_HEIGHT,
  PROP_PAD_ALPHA,
  PROP_PAD_OPERATOR,
  PROP_PAD_SIZING_POLICY,
  PROP_PAD_GAMMA_MODE,
  PROP_PAD_PRIMARIES_MODE,
};

#define DEFAULT_PAD_XPOS   0
#define DEFAULT_PAD_YPOS   0
#define DEFAULT_PAD_WIDTH  0
#define DEFAULT_PAD_HEIGHT 0
#define DEFAULT_PAD_ALPHA  1.0
#define DEFAULT_PAD_OPERATOR GST_D3D12_COMPOSITOR_OPERATOR_OVER
#define DEFAULT_PAD_SIZING_POLICY GST_D3D12_COMPOSITOR_SIZING_POLICY_NONE

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_BACKGROUND,
  PROP_IGNORE_INACTIVE_PADS,
};

#define DEFAULT_ADAPTER -1
#define DEFAULT_BACKGROUND GST_D3D12_COMPOSITOR_BACKGROUND_CHECKER

static const D3D12_RENDER_TARGET_BLEND_DESC g_blend_source = {
  TRUE,
  FALSE,
  D3D12_BLEND_ONE,
  D3D12_BLEND_ZERO,
  D3D12_BLEND_OP_ADD,
  D3D12_BLEND_ONE,
  D3D12_BLEND_ZERO,
  D3D12_BLEND_OP_ADD,
  D3D12_LOGIC_OP_NOOP,
  D3D12_COLOR_WRITE_ENABLE_ALL,
};

static const D3D12_RENDER_TARGET_BLEND_DESC g_blend_over = {
  TRUE,
  FALSE,
  D3D12_BLEND_SRC_ALPHA,
  D3D12_BLEND_INV_SRC_ALPHA,
  D3D12_BLEND_OP_ADD,
  D3D12_BLEND_ONE,
  D3D12_BLEND_INV_SRC_ALPHA,
  D3D12_BLEND_OP_ADD,
  D3D12_LOGIC_OP_NOOP,
  D3D12_COLOR_WRITE_ENABLE_ALL,
};

static const D3D12_RENDER_TARGET_BLEND_DESC g_blend_over_factor = {
  TRUE,
  FALSE,
  D3D12_BLEND_BLEND_FACTOR,
  D3D12_BLEND_INV_BLEND_FACTOR,
  D3D12_BLEND_OP_ADD,
  D3D12_BLEND_BLEND_FACTOR,
  D3D12_BLEND_INV_BLEND_FACTOR,
  D3D12_BLEND_OP_ADD,
  D3D12_LOGIC_OP_NOOP,
  D3D12_COLOR_WRITE_ENABLE_ALL,
};

static const D3D12_ROOT_SIGNATURE_FLAGS g_rs_flags =
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

struct PadContext
{
  PadContext (GstD3D12Device * dev)
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    device = (GstD3D12Device *) gst_object_ref (dev);
    ca_pool = gst_d3d12_command_allocator_pool_new (device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);
    gst_video_info_init (&info);
  }

  PadContext () = delete;

  ~PadContext () {
    gst_d3d12_device_fence_wait (device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        fence_val, event_handle);

    CloseHandle (event_handle);

    gst_clear_d3d12_fence_data (&fence_data);
    gst_clear_object (&conv);
    gst_clear_object (&ca_pool);
    gst_clear_object (&device);
  }

  GstVideoInfo info;
  GstD3D12CommandAllocatorPool *ca_pool;
  ComPtr < ID3D12GraphicsCommandList > cl;
  GstD3D12FenceData *fence_data = nullptr;
  GstD3D12Device *device;
  GstD3D12Converter *conv = nullptr;
  HANDLE event_handle;
  guint64 fence_val = 0;
};

struct GstD3D12CompositorPadPrivate
{
  GstD3D12CompositorPadPrivate ()
  {
    blend_desc = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
    blend_desc.RenderTarget[0] = g_blend_over;
    for (guint i = 0; i < 4; i++)
      blend_factor[i] = 1.0f;
  }

  std::unique_ptr < PadContext > ctx;
  std::future < gboolean > prepare_rst;

  gboolean position_updated = FALSE;
  gboolean alpha_updated = FALSE;
  gboolean blend_desc_updated = FALSE;
  D3D12_BLEND_DESC blend_desc;
  gfloat blend_factor[4];

  std::recursive_mutex lock;

  /* properties */
  gint xpos = DEFAULT_PAD_XPOS;
  gint ypos = DEFAULT_PAD_YPOS;
  gint width = DEFAULT_PAD_WIDTH;
  gint height = DEFAULT_PAD_HEIGHT;
  gdouble alpha = DEFAULT_PAD_ALPHA;
  GstD3D12CompositorOperator op = DEFAULT_PAD_OPERATOR;
  GstD3D12CompositorSizingPolicy sizing_policy = DEFAULT_PAD_SIZING_POLICY;
};

struct _GstD3D12CompositorPad
{
  GstVideoAggregatorConvertPad parent;

  GstD3D12CompositorPadPrivate *priv;
};

struct VertexData
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

struct BackgroundRender
{
  BackgroundRender (GstD3D12Device * dev, const GstVideoInfo & info)
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    device = (GstD3D12Device *) gst_object_ref (dev);
    ca_pool = gst_d3d12_command_allocator_pool_new (device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
        0, nullptr, 0, nullptr, g_rs_flags);

    ComPtr < ID3DBlob > rs_blob;
    ComPtr < ID3DBlob > error_blob;
    auto hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
    if (!gst_d3d12_result (hr, device)) {
      const gchar *error_msg = nullptr;
      if (error_blob)
        error_msg = (const gchar *) error_blob->GetBufferPointer ();

      GST_ERROR_OBJECT (device, "Couldn't serialize root signature, error: %s",
          GST_STR_NULL (error_msg));
      return;
    }

    auto device_handle = gst_d3d12_device_get_device_handle (device);
    hr = device_handle->CreateRootSignature (0, rs_blob->GetBufferPointer (),
        rs_blob->GetBufferSize (), IID_PPV_ARGS (&rs));
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create root signature");
      return;
    }

    GstD3D12Format format;
    gst_d3d12_device_get_format (device, GST_VIDEO_INFO_FORMAT (&info),
        &format);

    D3D12_INPUT_ELEMENT_DESC input_desc;
    input_desc.SemanticName = "POSITION";
    input_desc.SemanticIndex = 0;
    input_desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    input_desc.InputSlot = 0;
    input_desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    input_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    input_desc.InstanceDataStepRate = 0;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
    pso_desc.pRootSignature = rs.Get ();
    pso_desc.VS.BytecodeLength = sizeof (g_VSMain_pos);
    pso_desc.VS.pShaderBytecode = g_VSMain_pos;
    if (GST_VIDEO_INFO_IS_RGB (&info)) {
      pso_desc.PS.BytecodeLength = sizeof (g_PSMain_checker_rgb);
      pso_desc.PS.pShaderBytecode = g_PSMain_checker_rgb;
    } else if (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_VUYA) {
      pso_desc.PS.BytecodeLength = sizeof (g_PSMain_checker_vuya);
      pso_desc.PS.pShaderBytecode = g_PSMain_checker_vuya;
    } else {
      pso_desc.PS.BytecodeLength = sizeof (g_PSMain_checker_luma);
      pso_desc.PS.pShaderBytecode = g_PSMain_checker_luma;
    }
    pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.InputLayout.pInputElementDescs = &input_desc;
    pso_desc.InputLayout.NumElements = 1;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = format.resource_format[0];
    pso_desc.SampleDesc.Count = 1;

    hr = device_handle->CreateGraphicsPipelineState (&pso_desc,
        IID_PPV_ARGS (&pso));
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create pso");
      return;
    }

    VertexData vertex_data[4];
    const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };

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
        CD3DX12_RESOURCE_DESC::Buffer (sizeof (VertexData) * 4 +
        sizeof (indices));

    hr = device_handle->CreateCommittedResource (&heap_prop,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS (&vertex_index_upload));
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create vertex upload buf");
      return;
    }

    guint8 *data;
    CD3DX12_RANGE range (0, 0);
    hr = vertex_index_upload->Map (0, &range, (void **) &data);
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't map index buffer");
      return;
    }

    memcpy (data, vertex_data, sizeof (VertexData) * 4);
    memcpy (data + sizeof (VertexData) * 4, indices, sizeof (indices));
    vertex_index_upload->Unmap (0, nullptr);

    heap_prop = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
    hr = device_handle->CreateCommittedResource (&heap_prop,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &buffer_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS (&vertex_index_buf));
    if (!gst_d3d12_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create index buffer");
      return;
    }

    vbv.BufferLocation = vertex_index_buf->GetGPUVirtualAddress ();
    vbv.SizeInBytes = sizeof (VertexData) * 4;
    vbv.StrideInBytes = sizeof (VertexData);
    ibv.BufferLocation = vbv.BufferLocation + vbv.SizeInBytes;
    ibv.SizeInBytes = sizeof (indices);
    ibv.Format = DXGI_FORMAT_R16_UINT;

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = info.width;
    viewport.Height = info.height;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;

    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = info.width;
    scissor_rect.bottom = info.height;

    rtv_inc_size =
        device_handle->GetDescriptorHandleIncrementSize
        (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    is_valid = true;
  }
  BackgroundRender () = delete;

  ~BackgroundRender () {
    gst_d3d12_device_fence_wait (device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        fence_val, event_handle);

    CloseHandle (event_handle);

    gst_clear_object (&ca_pool);
    gst_clear_object (&device);
  }

  GstD3D12Device *device;
  ComPtr < ID3D12RootSignature > rs;
  ComPtr < ID3D12PipelineState > pso;
  ComPtr < ID3D12Resource > vertex_index_buf;
  ComPtr < ID3D12Resource > vertex_index_upload;
  D3D12_VERTEX_BUFFER_VIEW vbv;
  D3D12_INDEX_BUFFER_VIEW ibv;
  ComPtr < ID3D12GraphicsCommandList > cl;
  GstD3D12CommandAllocatorPool *ca_pool;
  D3D12_VIEWPORT viewport;
  D3D12_RECT scissor_rect;
  guint rtv_inc_size;
  bool need_upload = true;
  bool is_valid = false;
  HANDLE event_handle;
  guint64 fence_val = 0;
};

struct ClearColor
{
  /* [rtv][colors] */
  FLOAT color[4][4];
};

struct GStD3D12CompositorPrivate
{
  GStD3D12CompositorPrivate ()
  {
    fence_data_pool = gst_d3d12_fence_data_pool_new ();
    gst_video_info_init (&negotiated_info);
  }

   ~GStD3D12CompositorPrivate ()
  {
    gst_clear_buffer (&fallback_buf);
    gst_clear_object (&fence_data_pool);
  }

  GstBuffer *fallback_buf = nullptr;
  GstBuffer *generated_output_buf = nullptr;

  std::unique_ptr < BackgroundRender > bg_render;
  /* black/white/transparent */
  ClearColor clear_color[3];
  GstD3D12FenceDataPool *fence_data_pool;
  std::vector < D3D12_CPU_DESCRIPTOR_HANDLE > rtv_handles;
  std::queue < guint64 > scheduled;

  GstVideoInfo negotiated_info;

  gboolean downstream_supports_d3d12 = FALSE;

  std::recursive_mutex lock;

  /* properties */
  gint adapter = DEFAULT_ADAPTER;
  GstD3D12CompositorBackground background = DEFAULT_BACKGROUND;
};

struct _GstD3D12Compositor
{
  GstVideoAggregator parent;

  GstD3D12Device *device;

  GStD3D12CompositorPrivate *priv;
};

static void gst_d3d12_compositor_pad_finalize (GObject * object);
static void gst_d3d12_compositor_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d12_compositor_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void
gst_d3d12_compositor_pad_update_conversion_info (GstVideoAggregatorPad * pad);
static void
gst_d3d12_compositor_pad_prepare_frame_start (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame);
static void
gst_d3d12_compositor_pad_prepare_frame_finish (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame);
static void gst_d3d12_compositor_pad_clean_frame (GstVideoAggregatorPad * vpad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame);

#define gst_d3d12_compositor_pad_parent_class parent_pad_class
G_DEFINE_TYPE (GstD3D12CompositorPad, gst_d3d12_compositor_pad,
    GST_TYPE_VIDEO_AGGREGATOR_PAD);

static void
gst_d3d12_compositor_pad_class_init (GstD3D12CompositorPadClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto vagg_pad_class = GST_VIDEO_AGGREGATOR_PAD_CLASS (klass);
  GParamFlags param_flags = (GParamFlags)
      (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS);

  object_class->finalize = gst_d3d12_compositor_pad_finalize;
  object_class->set_property = gst_d3d12_compositor_pad_set_property;
  object_class->get_property = gst_d3d12_compositor_pad_get_property;

  g_object_class_install_property (object_class, PROP_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_WIDTH, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_HEIGHT, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_PAD_ALPHA, param_flags));
  g_object_class_install_property (object_class, PROP_PAD_OPERATOR,
      g_param_spec_enum ("operator", "Operator",
          "Blending operator to use for blending this pad over the previous ones",
          GST_TYPE_D3D12_COMPOSITOR_OPERATOR, DEFAULT_PAD_OPERATOR,
          param_flags));
  g_object_class_install_property (object_class, PROP_PAD_SIZING_POLICY,
      g_param_spec_enum ("sizing-policy", "Sizing policy",
          "Sizing policy to use for image scaling",
          GST_TYPE_D3D12_COMPOSITOR_SIZING_POLICY, DEFAULT_PAD_SIZING_POLICY,
          param_flags));

  vagg_pad_class->update_conversion_info =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_pad_update_conversion_info);
  vagg_pad_class->prepare_frame_start =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_pad_prepare_frame_start);
  vagg_pad_class->prepare_frame_finish =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_pad_prepare_frame_finish);
  vagg_pad_class->clean_frame =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_pad_clean_frame);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_COMPOSITOR_OPERATOR,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_COMPOSITOR_SIZING_POLICY,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d12_compositor_pad_init (GstD3D12CompositorPad * pad)
{
  pad->priv = new GstD3D12CompositorPadPrivate ();
}

static void
gst_d3d12_compositor_pad_finalize (GObject * object)
{
  auto self = GST_D3D12_COMPOSITOR_PAD (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_pad_class)->finalize (object);
}

static void
gst_d3d12_compositor_pad_update_position (GstD3D12CompositorPad * pad,
    gint * old, const GValue * value)
{
  auto priv = pad->priv;
  gint tmp = g_value_get_int (value);

  if (*old != tmp) {
    *old = tmp;
    priv->position_updated = TRUE;
  }
}

static void
gst_d3d12_compositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  auto pad = GST_D3D12_COMPOSITOR_PAD (object);
  auto priv = pad->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_PAD_XPOS:
      gst_d3d12_compositor_pad_update_position (pad, &priv->xpos, value);
      break;
    case PROP_PAD_YPOS:
      gst_d3d12_compositor_pad_update_position (pad, &priv->ypos, value);
      break;
    case PROP_PAD_WIDTH:
      gst_d3d12_compositor_pad_update_position (pad, &priv->width, value);
      break;
    case PROP_PAD_HEIGHT:
      gst_d3d12_compositor_pad_update_position (pad, &priv->height, value);
      break;
    case PROP_PAD_ALPHA:{
      gdouble alpha = g_value_get_double (value);
      if (priv->alpha != alpha) {
        priv->alpha_updated = TRUE;
        priv->alpha = alpha;
      }
      break;
    }
    case PROP_PAD_OPERATOR:{
      GstD3D12CompositorOperator op =
          (GstD3D12CompositorOperator) g_value_get_enum (value);
      if (op != priv->op) {
        priv->op = op;
        priv->blend_desc_updated = TRUE;
      }
      break;
    }
    case PROP_PAD_SIZING_POLICY:{
      GstD3D12CompositorSizingPolicy policy =
          (GstD3D12CompositorSizingPolicy) g_value_get_enum (value);
      if (priv->sizing_policy != policy) {
        priv->sizing_policy = policy;
        priv->position_updated = TRUE;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_compositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  auto pad = GST_D3D12_COMPOSITOR_PAD (object);
  auto priv = pad->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_PAD_XPOS:
      g_value_set_int (value, priv->xpos);
      break;
    case PROP_PAD_YPOS:
      g_value_set_int (value, priv->ypos);
      break;
    case PROP_PAD_WIDTH:
      g_value_set_int (value, priv->width);
      break;
    case PROP_PAD_HEIGHT:
      g_value_set_int (value, priv->height);
      break;
    case PROP_PAD_ALPHA:
      g_value_set_double (value, priv->alpha);
      break;
    case PROP_PAD_OPERATOR:
      g_value_set_enum (value, priv->op);
      break;
    case PROP_PAD_SIZING_POLICY:
      g_value_set_enum (value, priv->sizing_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_compositor_pad_update_conversion_info (GstVideoAggregatorPad * pad)
{
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  priv->position_updated = TRUE;
}

static void
gst_d3d12_compositor_pad_get_output_size (GstD3D12CompositorPad * pad,
    gint out_par_n, gint out_par_d, gint * width, gint * height,
    gint * x_offset, gint * y_offset)
{
  auto priv = pad->priv;
  auto vagg_pad = GST_VIDEO_AGGREGATOR_PAD (pad);
  gint pad_width, pad_height;
  guint dar_n, dar_d;

  *x_offset = 0;
  *y_offset = 0;
  *width = 0;
  *height = 0;

  /* FIXME: Anything better we can do here? */
  if (!vagg_pad->info.finfo
      || vagg_pad->info.finfo->format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_DEBUG_OBJECT (pad, "Have no caps yet");
    return;
  }

  std::lock_guard < std::recursive_mutex > lk (priv->lock);

  pad_width = priv->width <= 0 ? GST_VIDEO_INFO_WIDTH (&vagg_pad->info) :
      priv->width;
  pad_height = priv->height <= 0 ? GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) :
      priv->height;

  if (pad_width == 0 || pad_height == 0)
    return;

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
          GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
          GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d)) {
    GST_WARNING_OBJECT (pad, "Cannot calculate display aspect ratio");
    return;
  }

  GST_TRACE_OBJECT (pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)",
      pad_width, pad_height, dar_n, dar_d,
      GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d);

  switch (priv->sizing_policy) {
    case GST_D3D12_COMPOSITOR_SIZING_POLICY_NONE:
      /* Pick either height or width, whichever is an integer multiple of the
       * display aspect ratio. However, prefer preserving the height to account
       * for interlaced video. */
      if (pad_height % dar_n == 0) {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      } else if (pad_width % dar_d == 0) {
        pad_height = gst_util_uint64_scale_int (pad_width, dar_d, dar_n);
      } else {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      }
      break;
    case GST_D3D12_COMPOSITOR_SIZING_POLICY_KEEP_ASPECT_RATIO:{
      gint from_dar_n, from_dar_d, to_dar_n, to_dar_d, num, den;

      /* Calculate DAR again with actual video size */
      if (!gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (&vagg_pad->info),
              GST_VIDEO_INFO_HEIGHT (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_D (&vagg_pad->info), &from_dar_n,
              &from_dar_d)) {
        from_dar_n = from_dar_d = -1;
      }

      if (!gst_util_fraction_multiply (pad_width, pad_height,
              out_par_n, out_par_d, &to_dar_n, &to_dar_d)) {
        to_dar_n = to_dar_d = -1;
      }

      if (from_dar_n != to_dar_n || from_dar_d != to_dar_d) {
        /* Calculate new output resolution */
        if (from_dar_n != -1 && from_dar_d != -1
            && gst_util_fraction_multiply (from_dar_n, from_dar_d,
                out_par_d, out_par_n, &num, &den)) {
          GstVideoRectangle src_rect, dst_rect, rst_rect;

          src_rect.h = gst_util_uint64_scale_int (pad_width, den, num);
          if (src_rect.h == 0) {
            pad_width = 0;
            pad_height = 0;
            break;
          }

          src_rect.x = src_rect.y = 0;
          src_rect.w = pad_width;

          dst_rect.x = dst_rect.y = 0;
          dst_rect.w = pad_width;
          dst_rect.h = pad_height;

          /* Scale rect to be centered in destination rect */
          gst_video_center_rect (&src_rect, &dst_rect, &rst_rect, TRUE);

          GST_LOG_OBJECT (pad,
              "Re-calculated size %dx%d -> %dx%d (x-offset %d, y-offset %d)",
              pad_width, pad_height, rst_rect.w, rst_rect.h, rst_rect.x,
              rst_rect.h);

          *x_offset = rst_rect.x;
          *y_offset = rst_rect.y;
          pad_width = rst_rect.w;
          pad_height = rst_rect.h;
        } else {
          GST_WARNING_OBJECT (pad, "Failed to calculate output size");

          *x_offset = 0;
          *y_offset = 0;
          pad_width = 0;
          pad_height = 0;
        }
      }
      break;
    }
  }

  *width = pad_width;
  *height = pad_height;
}

static GstVideoRectangle
clamp_rectangle (gint x, gint y, gint w, gint h, gint outer_width,
    gint outer_height)
{
  gint x2 = x + w;
  gint y2 = y + h;
  GstVideoRectangle clamped;

  /* Clamp the x/y coordinates of this frame to the output boundaries to cover
   * the case where (say, with negative xpos/ypos or w/h greater than the output
   * size) the non-obscured portion of the frame could be outside the bounds of
   * the video itself and hence not visible at all */
  clamped.x = CLAMP (x, 0, outer_width);
  clamped.y = CLAMP (y, 0, outer_height);
  clamped.w = CLAMP (x2, 0, outer_width) - clamped.x;
  clamped.h = CLAMP (y2, 0, outer_height) - clamped.y;

  return clamped;
}

static gboolean
gst_d3d12_compositor_pad_check_frame_obscured (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;
  gint width, height;
  GstVideoInfo *info = &vagg->info;
  /* The rectangle representing this frame, clamped to the video's boundaries.
   * Due to the clamping, this is different from the frame width/height above. */
  GstVideoRectangle frame_rect;
  gint x_offset, y_offset;

  /* There's three types of width/height here:
   * 1. GST_VIDEO_FRAME_WIDTH/HEIGHT:
   *     The frame width/height (same as pad->info.height/width;
   *     see gst_video_frame_map())
   * 2. cpad->width/height:
   *     The optional pad property for scaling the frame (if zero, the video is
   *     left unscaled)
   */

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (priv->alpha == 0)
    return TRUE;

  gst_d3d12_compositor_pad_get_output_size (cpad, GST_VIDEO_INFO_PAR_N (info),
      GST_VIDEO_INFO_PAR_D (info), &width, &height, &x_offset, &y_offset);

  frame_rect = clamp_rectangle (priv->xpos + x_offset, priv->ypos + y_offset,
      width, height, GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

  if (frame_rect.w == 0 || frame_rect.h == 0) {
    GST_DEBUG_OBJECT (pad, "Resulting frame is zero-width or zero-height "
        "(w: %i, h: %i), skipping", frame_rect.w, frame_rect.h);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_d3d12_compositor_pad_setup_converter (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;
  auto self = GST_D3D12_COMPOSITOR (vagg);
  gint width, height;
  GstVideoInfo *info = &vagg->info;
  GstVideoRectangle frame_rect;
  gboolean output_has_alpha_comp = FALSE;
  gint x_offset, y_offset;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);

  if (GST_VIDEO_INFO_HAS_ALPHA (info) ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_BGRx ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_RGBx) {
    output_has_alpha_comp = TRUE;
  }

  if (priv->ctx) {
    if (GST_VIDEO_INFO_FORMAT (&priv->ctx->info) !=
        GST_VIDEO_INFO_FORMAT (&pad->info)) {
      priv->ctx = nullptr;
    }
  }

  if (!priv->ctx || priv->blend_desc_updated) {
    switch (priv->op) {
      case GST_D3D12_COMPOSITOR_OPERATOR_SOURCE:
        priv->blend_desc.RenderTarget[0] = g_blend_source;
        break;
      case GST_D3D12_COMPOSITOR_OPERATOR_OVER:
        if (output_has_alpha_comp)
          priv->blend_desc.RenderTarget[0] = g_blend_over;
        else
          priv->blend_desc.RenderTarget[0] = g_blend_over_factor;
        break;
      default:
        g_assert_not_reached ();
        return FALSE;
    }
  }

  if (!priv->ctx || priv->alpha_updated) {
    for (guint i = 0; i < 4; i++)
      priv->blend_factor[i] = priv->alpha;
  }

  if (!priv->ctx) {
    auto ctx = std::make_unique < PadContext > (self->device);
    ctx->info = pad->info;

    ctx->conv = gst_d3d12_converter_new (self->device, &pad->info, info,
        &priv->blend_desc, priv->blend_factor, nullptr);
    if (!ctx->conv) {
      GST_ERROR_OBJECT (pad, "Couldn't create converter");
      return FALSE;
    }

    priv->ctx = std::move (ctx);
  }

  if (priv->ctx->fence_val == 0 || priv->alpha_updated) {
    g_object_set (priv->ctx->conv, "alpha", priv->alpha, nullptr);
    gst_d3d12_converter_update_blend_state (priv->ctx->conv,
        &priv->blend_desc, priv->blend_factor);
  }

  priv->alpha_updated = FALSE;
  priv->blend_desc_updated = FALSE;

  if (priv->ctx->fence_val != 0 && !priv->position_updated)
    return TRUE;

  gst_d3d12_compositor_pad_get_output_size (cpad, GST_VIDEO_INFO_PAR_N (info),
      GST_VIDEO_INFO_PAR_D (info), &width, &height, &x_offset, &y_offset);

  frame_rect = clamp_rectangle (priv->xpos + x_offset, priv->ypos + y_offset,
      width, height, GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

#ifndef GST_DISABLE_GST_DEBUG
  guint zorder = 0;
  g_object_get (pad, "zorder", &zorder, nullptr);

  GST_LOG_OBJECT (pad, "Update position, pad-xpos %d, pad-ypos %d, "
      "pad-zorder %d, pad-width %d, pad-height %d, in-resolution %dx%d, "
      "out-resoution %dx%d, dst-{x,y,width,height} %d-%d-%d-%d",
      priv->xpos, priv->ypos, zorder, priv->width, priv->height,
      GST_VIDEO_INFO_WIDTH (&pad->info), GST_VIDEO_INFO_HEIGHT (&pad->info),
      GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info),
      frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h);
#endif

  priv->position_updated = FALSE;

  g_object_set (priv->ctx->conv, "dest-x", frame_rect.x,
      "dest-y", frame_rect.y, "dest-width", frame_rect.w,
      "dest-height", frame_rect.h, nullptr);

  return TRUE;
}

static gboolean
gst_d3d12_compositor_preprare_func (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  auto self = GST_D3D12_COMPOSITOR (vagg);
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;

  GST_LOG_OBJECT (pad, "Building command list");

  if (!self->priv->generated_output_buf) {
    GST_ERROR_OBJECT (cpad, "Have no generated output buf");
    return FALSE;
  }

  /* Skip this frame */
  if (gst_d3d12_compositor_pad_check_frame_obscured (pad, vagg))
    return TRUE;

  if (!gst_d3d12_compositor_pad_setup_converter (pad, vagg))
    return FALSE;

  gint x, y, w, h;
  auto crop_meta = gst_buffer_get_video_crop_meta (buffer);
  if (crop_meta) {
    x = crop_meta->x;
    y = crop_meta->y;
    w = crop_meta->width;
    h = crop_meta->height;
  } else {
    x = y = 0;
    w = pad->info.width;
    h = pad->info.height;
  }

  g_assert (priv->ctx);

  g_object_set (priv->ctx->conv, "src-x", x, "src-y", y, "src-width", w,
      "src-height", h, nullptr);

  GstD3D12CommandAllocator *gst_ca;
  if (!gst_d3d12_command_allocator_pool_acquire (priv->ctx->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (cpad, "Couldn't acquire command allocator");
    return FALSE;
  }

  GstD3D12FenceData *fence_data;
  gst_d3d12_fence_data_pool_acquire (self->priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  auto hr = ca->Reset ();
  if (!gst_d3d12_result (hr, priv->ctx->device)) {
    GST_ERROR_OBJECT (cpad, "Couldn't reset command allocator");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  if (!priv->ctx->cl) {
    auto device = gst_d3d12_device_get_device_handle (priv->ctx->device);
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), nullptr, IID_PPV_ARGS (&priv->ctx->cl));
    if (!gst_d3d12_result (hr, priv->ctx->device)) {
      GST_ERROR_OBJECT (cpad, "Couldn't create command list");
      gst_d3d12_fence_data_unref (fence_data);
      return FALSE;
    }
  } else {
    hr = priv->ctx->cl->Reset (ca.Get (), nullptr);
    if (!gst_d3d12_result (hr, priv->ctx->device)) {
      GST_ERROR_OBJECT (self, "Couldn't reset command list");
      gst_d3d12_fence_data_unref (fence_data);
      return FALSE;
    }
  }

  if (!gst_d3d12_converter_convert_buffer (priv->ctx->conv,
          buffer, self->priv->generated_output_buf, fence_data,
          priv->ctx->cl.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't build command list");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  hr = priv->ctx->cl->Close ();
  if (!gst_d3d12_result (hr, priv->ctx->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  prepared_frame->buffer = buffer;

  priv->ctx->fence_data = fence_data;

  GST_LOG_OBJECT (pad, "Command list prepared");

  return TRUE;
}

static void
gst_d3d12_compositor_pad_prepare_frame_start (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstBuffer * buffer,
    GstVideoFrame * prepared_frame)
{
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;

  GST_LOG_OBJECT (cpad, "Prepare start");

  priv->prepare_rst = std::async (std::launch::async,
      gst_d3d12_compositor_preprare_func, pad, vagg, buffer, prepared_frame);
}

static void
gst_d3d12_compositor_pad_prepare_frame_finish (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  /* Will wait on aggregate() function */
}

static void
gst_d3d12_compositor_pad_clean_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg, GstVideoFrame * prepared_frame)
{
  auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
  auto priv = cpad->priv;

  if (priv->prepare_rst.valid ()) {
    GST_WARNING_OBJECT (cpad, "Async task still pending");
    priv->prepare_rst.get ();
  }

  memset (prepared_frame, 0, sizeof (GstVideoFrame));

  if (priv->ctx && priv->ctx->fence_data) {
    gst_d3d12_device_set_fence_notify (priv->ctx->device,
        D3D12_COMMAND_LIST_TYPE_DIRECT, priv->ctx->fence_val,
        priv->ctx->fence_data);
    priv->ctx->fence_data = nullptr;
  }
}

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (GST_D3D12_ALL_FORMATS)));

/* formats we can output without conversion.
 * Excludes 10/12 bits planar YUV (needs bitshift) and
 * AYUV/AYUV64 (d3d12 runtime does not understand the ayuv order) */
#define COMPOSITOR_SRC_FORMATS \
    "{ RGBA64_LE, RGB10A2_LE, BGRA, RGBA, BGRx, RGBx, VUYA, NV12, NV21, " \
    "P010_10LE, P012_LE, P016_LE, I420, YV12, Y42B, Y444, Y444_16LE, " \
    "GRAY8, GRAY16_LE }"

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, COMPOSITOR_SRC_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (COMPOSITOR_SRC_FORMATS)));

static void gst_d3d12_compositor_child_proxy_init (gpointer g_iface,
    gpointer iface_data);
static void gst_d3d12_compositor_finalize (GObject * object);
static void gst_d3d12_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d12_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstPad *gst_d3d12_compositor_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_d3d12_compositor_release_pad (GstElement * element,
    GstPad * pad);
static void gst_d3d12_compositor_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_d3d12_compositor_start (GstAggregator * agg);
static gboolean gst_d3d12_compositor_stop (GstAggregator * agg);
static gboolean gst_d3d12_compositor_sink_query (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * query);
static gboolean gst_d3d12_compositor_src_query (GstAggregator * agg,
    GstQuery * query);
static GstCaps *gst_d3d12_compositor_fixate_src_caps (GstAggregator * agg,
    GstCaps * caps);
static gboolean gst_d3d12_compositor_negotiated_src_caps (GstAggregator * agg,
    GstCaps * caps);
static gboolean
gst_d3d12_compositor_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query);
static gboolean gst_d3d12_compositor_decide_allocation (GstAggregator * agg,
    GstQuery * query);
static GstFlowReturn
gst_d3d12_compositor_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuf);
static GstFlowReturn
gst_d3d12_compositor_create_output_buffer (GstVideoAggregator * vagg,
    GstBuffer ** outbuffer);

#define gst_d3d12_compositor_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstD3D12Compositor, gst_d3d12_compositor,
    GST_TYPE_VIDEO_AGGREGATOR, G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_d3d12_compositor_child_proxy_init));

static void
gst_d3d12_compositor_class_init (GstD3D12CompositorClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto element_class = GST_ELEMENT_CLASS (klass);
  auto agg_class = GST_AGGREGATOR_CLASS (klass);
  auto vagg_class = GST_VIDEO_AGGREGATOR_CLASS (klass);

  object_class->finalize = gst_d3d12_compositor_finalize;
  object_class->set_property = gst_d3d12_compositor_set_property;
  object_class->get_property = gst_d3d12_compositor_get_property;

  g_object_class_install_property (object_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "Adapter index for creating device (-1 for default)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_BACKGROUND,
      g_param_spec_enum ("background", "Background", "Background type",
          GST_TYPE_D3D12_COMPOSITOR_BACKGROUND,
          DEFAULT_BACKGROUND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class,
      PROP_IGNORE_INACTIVE_PADS, g_param_spec_boolean ("ignore-inactive-pads",
          "Ignore inactive pads",
          "Avoid timing out waiting for inactive pads", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_request_new_pad);
  element_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_release_pad);
  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_set_context);

  agg_class->start = GST_DEBUG_FUNCPTR (gst_d3d12_compositor_start);
  agg_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_compositor_stop);
  agg_class->sink_query = GST_DEBUG_FUNCPTR (gst_d3d12_compositor_sink_query);
  agg_class->src_query = GST_DEBUG_FUNCPTR (gst_d3d12_compositor_src_query);
  agg_class->fixate_src_caps =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_fixate_src_caps);
  agg_class->negotiated_src_caps =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_negotiated_src_caps);
  agg_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_propose_allocation);
  agg_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_decide_allocation);

  vagg_class->aggregate_frames =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_aggregate_frames);
  vagg_class->create_output_buffer =
      GST_DEBUG_FUNCPTR (gst_d3d12_compositor_create_output_buffer);

  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &sink_template, GST_TYPE_D3D12_COMPOSITOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &src_template, GST_TYPE_AGGREGATOR_PAD);

  gst_element_class_set_static_metadata (element_class, "Direct3D12 Compositor",
      "Filter/Editor/Video/Compositor", "A Direct3D12 compositor",
      "Seungha Yang <seungha@centricular.com>");

  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_COMPOSITOR_BACKGROUND,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_COMPOSITOR_PAD,
      (GstPluginAPIFlags) 0);

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_compositor_debug,
      "d3d12compositor", 0, "d3d12compositor element");
}

static void
gst_d3d12_compositor_init (GstD3D12Compositor * self)
{
  self->priv = new GStD3D12CompositorPrivate ();
}

static void
gst_d3d12_compositor_finalize (GObject * object)
{
  auto self = GST_D3D12_COMPOSITOR (object);

  delete self->priv;

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_COMPOSITOR (object);
  auto priv = self->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_ADAPTER:
      priv->adapter = g_value_get_int (value);
      break;
    case PROP_BACKGROUND:
      priv->background =
          (GstD3D12CompositorBackground) g_value_get_enum (value);
      break;
    case PROP_IGNORE_INACTIVE_PADS:
      gst_aggregator_set_ignore_inactive_pads (GST_AGGREGATOR (object),
          g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_COMPOSITOR (object);
  auto priv = self->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, priv->adapter);
      break;
    case PROP_BACKGROUND:
      g_value_set_enum (value, priv->background);
      break;
    case PROP_IGNORE_INACTIVE_PADS:
      g_value_set_boolean (value,
          gst_aggregator_get_ignore_inactive_pads (GST_AGGREGATOR (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GObject *
gst_d3d12_compositor_child_proxy_get_child_by_index (GstChildProxy * proxy,
    guint index)
{
  auto self = GST_D3D12_COMPOSITOR (proxy);
  GObject *obj = nullptr;

  GST_OBJECT_LOCK (self);
  obj = (GObject *) g_list_nth_data (GST_ELEMENT_CAST (self)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (self);

  return obj;
}

static guint
gst_d3d12_compositor_child_proxy_get_children_count (GstChildProxy * proxy)
{
  auto self = GST_D3D12_COMPOSITOR (proxy);
  guint count = 0;

  GST_OBJECT_LOCK (self);
  count = GST_ELEMENT_CAST (self)->numsinkpads;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_d3d12_compositor_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = (GstChildProxyInterface *) g_iface;

  iface->get_child_by_index =
      gst_d3d12_compositor_child_proxy_get_child_by_index;
  iface->get_children_count =
      gst_d3d12_compositor_child_proxy_get_children_count;
}

static GstPad *
gst_d3d12_compositor_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad;

  pad = GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, name, caps);

  if (!pad) {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return nullptr;
  }

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_DEBUG_OBJECT (element, "Created new pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  return pad;
}

static void
gst_d3d12_compositor_release_pad (GstElement * element, GstPad * pad)
{
  auto self = GST_D3D12_COMPOSITOR (element);

  GST_DEBUG_OBJECT (self, "Releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}

static void
gst_d3d12_compositor_set_context (GstElement * element, GstContext * context)
{
  auto self = GST_D3D12_COMPOSITOR (element);
  auto priv = self->priv;

  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    gst_d3d12_handle_set_context (element, context, priv->adapter,
        &self->device);
  }

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d12_compositor_start (GstAggregator * agg)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  auto priv = self->priv;

  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    if (!gst_d3d12_ensure_element_data (GST_ELEMENT_CAST (self),
            priv->adapter, &self->device)) {
      GST_ERROR_OBJECT (self, "Failed to get D3D12 device");
      return FALSE;
    }
  }

  priv->scheduled = { };

  return GST_AGGREGATOR_CLASS (parent_class)->start (agg);
}

static gboolean
gst_d3d12_compositor_stop (GstAggregator * agg)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  auto priv = self->priv;

  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    priv->bg_render = nullptr;
    gst_clear_object (&self->device);
  }

  return GST_AGGREGATOR_CLASS (parent_class)->stop (agg);
}

static GstCaps *
gst_d3d12_compositor_sink_getcaps (GstPad * pad, GstCaps * filter)
{
  GstCaps *sinkcaps;
  GstCaps *template_caps;
  GstCaps *filtered_caps;
  GstCaps *returned_caps;

  template_caps = gst_pad_get_pad_template_caps (pad);

  sinkcaps = gst_pad_get_current_caps (pad);
  if (sinkcaps == nullptr) {
    sinkcaps = gst_caps_ref (template_caps);
  } else {
    sinkcaps = gst_caps_merge (sinkcaps, gst_caps_ref (template_caps));
  }

  if (filter) {
    filtered_caps = gst_caps_intersect (sinkcaps, filter);
    gst_caps_unref (sinkcaps);
  } else {
    filtered_caps = sinkcaps;   /* pass ownership */
  }

  returned_caps = gst_caps_intersect (filtered_caps, template_caps);

  gst_caps_unref (template_caps);
  gst_caps_unref (filtered_caps);

  GST_DEBUG_OBJECT (pad, "returning %" GST_PTR_FORMAT, returned_caps);

  return returned_caps;
}

static gboolean
gst_d3d12_compositor_sink_acceptcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret;
  GstCaps *template_caps;

  GST_DEBUG_OBJECT (pad, "try accept caps of %" GST_PTR_FORMAT, caps);

  template_caps = gst_pad_get_pad_template_caps (pad);
  template_caps = gst_caps_make_writable (template_caps);

  ret = gst_caps_can_intersect (caps, template_caps);
  GST_DEBUG_OBJECT (pad, "%saccepted caps %" GST_PTR_FORMAT,
      (ret ? "" : "not "), caps);
  gst_caps_unref (template_caps);

  return ret;
}

static gboolean
gst_d3d12_compositor_sink_query (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * query)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  auto priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      std::lock_guard < std::recursive_mutex > lk (priv->lock);
      if (gst_d3d12_handle_context_query (GST_ELEMENT (agg), query,
              self->device)) {
        return TRUE;
      }
      break;
    }
    case GST_QUERY_CAPS:{
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_d3d12_compositor_sink_getcaps (GST_PAD (pad), filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      return TRUE;
    }
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *caps;
      gboolean ret;

      gst_query_parse_accept_caps (query, &caps);
      ret = gst_d3d12_compositor_sink_acceptcaps (GST_PAD (pad), caps);
      gst_query_set_accept_caps_result (query, ret);
      return TRUE;
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, pad, query);
}

static gboolean
gst_d3d12_compositor_src_query (GstAggregator * agg, GstQuery * query)
{
  auto self = GST_D3D12_COMPOSITOR (agg);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d12_handle_context_query (GST_ELEMENT (agg), query,
              self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
}

static GstCaps *
gst_d3d12_compositor_fixate_src_caps (GstAggregator * agg, GstCaps * caps)
{
  auto vagg = GST_VIDEO_AGGREGATOR (agg);
  GList *l;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  gint par_n, par_d;
  gdouble best_fps = 0.;
  GstCaps *ret = nullptr;
  GstStructure *s;

  ret = gst_caps_make_writable (caps);

  /* we need this to calculate how large to make the output frame */
  s = gst_caps_get_structure (ret, 0);
  if (gst_structure_has_field (s, "pixel-aspect-ratio")) {
    gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio", 1, 1);
    gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);
  } else {
    par_n = par_d = 1;
  }

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    auto vaggpad = GST_VIDEO_AGGREGATOR_PAD (l->data);
    auto cpad = GST_D3D12_COMPOSITOR_PAD (vaggpad);
    auto priv = cpad->priv;
    gint this_width, this_height;
    gint width, height;
    gint fps_n, fps_d;
    gdouble cur_fps;
    gint x_offset;
    gint y_offset;

    fps_n = GST_VIDEO_INFO_FPS_N (&vaggpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&vaggpad->info);
    gst_d3d12_compositor_pad_get_output_size (cpad,
        par_n, par_d, &width, &height, &x_offset, &y_offset);

    if (width == 0 || height == 0)
      continue;

    /* {x,y}_offset represent padding size of each top and left area.
     * To calculate total resolution, count bottom and right padding area
     * as well here */
    this_width = width + MAX (priv->xpos + 2 * x_offset, 0);
    this_height = height + MAX (priv->ypos + 2 * y_offset, 0);

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }

  if (best_width <= 0 || best_height <= 0) {
    best_width = 320;
    best_height = 240;
  }

  gst_structure_fixate_field_nearest_int (s, "width", best_width);
  gst_structure_fixate_field_nearest_int (s, "height", best_height);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", best_fps_n,
      best_fps_d);
  ret = gst_caps_fixate (ret);

  GST_LOG_OBJECT (agg, "Fixated caps %" GST_PTR_FORMAT, ret);

  return ret;
}

static void
convert_info_gray_to_yuv (const GstVideoInfo * gray, GstVideoInfo * yuv)
{
  GstVideoInfo tmp;

  if (GST_VIDEO_INFO_IS_YUV (gray)) {
    *yuv = *gray;
    return;
  }

  if (gray->finfo->depth[0] == 8) {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444, gray->width, gray->height);
  } else {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444_16LE, gray->width, gray->height);
  }

  tmp.colorimetry.range = gray->colorimetry.range;
  if (tmp.colorimetry.range == GST_VIDEO_COLOR_RANGE_UNKNOWN)
    tmp.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

  tmp.colorimetry.primaries = gray->colorimetry.primaries;
  if (tmp.colorimetry.primaries == GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
    tmp.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;

  tmp.colorimetry.transfer = gray->colorimetry.transfer;
  if (tmp.colorimetry.transfer == GST_VIDEO_TRANSFER_UNKNOWN)
    tmp.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;

  tmp.colorimetry.matrix = gray->colorimetry.matrix;
  if (tmp.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN)
    tmp.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;

  *yuv = tmp;
}

static void
gst_d3d12_compositor_calculate_background_color (GstD3D12Compositor * self,
    const GstVideoInfo * info)
{
  auto priv = self->priv;
  GstD3D12ColorMatrix clear_color_matrix;
  gdouble rgb[3];
  gdouble converted[3];
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);

  if (GST_VIDEO_INFO_IS_RGB (info)) {
    GstVideoInfo rgb_info = *info;
    rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

    gst_d3d12_color_range_adjust_matrix_unorm (&rgb_info, info,
        &clear_color_matrix);
  } else {
    GstVideoInfo rgb_info;
    GstVideoInfo yuv_info;

    gst_video_info_set_format (&rgb_info, GST_VIDEO_FORMAT_RGBA64_LE,
        info->width, info->height);
    convert_info_gray_to_yuv (info, &yuv_info);

    if (yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
      GST_WARNING_OBJECT (self, "Invalid matrix is detected");
      yuv_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }

    gst_d3d12_rgb_to_yuv_matrix_unorm (&rgb_info,
        &yuv_info, &clear_color_matrix);
  }

  /* Calculate black and white color values */
  for (guint i = 0; i < 2; i++) {
    ClearColor *clear_color = &priv->clear_color[i];
    rgb[0] = rgb[1] = rgb[2] = (gdouble) i;

    for (guint j = 0; j < 3; j++) {
      converted[j] = 0;
      for (guint k = 0; k < 3; k++) {
        converted[j] += clear_color_matrix.matrix[j][k] * rgb[k];
      }
      converted[j] += clear_color_matrix.offset[j];
      converted[j] = CLAMP (converted[j],
          clear_color_matrix.min[j], clear_color_matrix.max[j]);
    }

    GST_DEBUG_OBJECT (self, "Calculated background color RGB: %f, %f, %f",
        converted[0], converted[1], converted[2]);

    if (GST_VIDEO_INFO_IS_RGB (info) || GST_VIDEO_INFO_IS_GRAY (info)) {
      for (guint j = 0; j < 3; j++)
        clear_color->color[0][j] = converted[j];
      clear_color->color[0][3] = 1.0;
    } else {
      switch (format) {
        case GST_VIDEO_FORMAT_VUYA:
          clear_color->color[0][0] = converted[2];
          clear_color->color[0][1] = converted[1];
          clear_color->color[0][2] = converted[0];
          clear_color->color[0][3] = 1.0;
          break;
        case GST_VIDEO_FORMAT_NV12:
        case GST_VIDEO_FORMAT_NV21:
        case GST_VIDEO_FORMAT_P010_10LE:
        case GST_VIDEO_FORMAT_P012_LE:
        case GST_VIDEO_FORMAT_P016_LE:
          clear_color->color[0][0] = converted[0];
          clear_color->color[0][1] = 0;
          clear_color->color[0][2] = 0;
          clear_color->color[0][3] = 1.0;
          if (format == GST_VIDEO_FORMAT_NV21) {
            clear_color->color[1][0] = converted[2];
            clear_color->color[1][1] = converted[1];
          } else {
            clear_color->color[1][0] = converted[1];
            clear_color->color[1][1] = converted[2];
          }
          clear_color->color[1][2] = 0;
          clear_color->color[1][3] = 1.0;
          break;
        case GST_VIDEO_FORMAT_I420:
        case GST_VIDEO_FORMAT_YV12:
        case GST_VIDEO_FORMAT_I420_10LE:
        case GST_VIDEO_FORMAT_I420_12LE:
        case GST_VIDEO_FORMAT_Y42B:
        case GST_VIDEO_FORMAT_I422_10LE:
        case GST_VIDEO_FORMAT_I422_12LE:
        case GST_VIDEO_FORMAT_Y444:
        case GST_VIDEO_FORMAT_Y444_10LE:
        case GST_VIDEO_FORMAT_Y444_12LE:
        case GST_VIDEO_FORMAT_Y444_16LE:
          clear_color->color[0][0] = converted[0];
          clear_color->color[0][1] = 0;
          clear_color->color[0][2] = 0;
          clear_color->color[0][3] = 1.0;
          if (format == GST_VIDEO_FORMAT_YV12) {
            clear_color->color[1][0] = converted[2];
            clear_color->color[2][0] = converted[1];
          } else {
            clear_color->color[1][0] = converted[1];
            clear_color->color[2][0] = converted[2];
          }
          clear_color->color[1][1] = 0;
          clear_color->color[1][2] = 0;
          clear_color->color[1][3] = 1.0;
          clear_color->color[2][1] = 0;
          clear_color->color[2][2] = 0;
          clear_color->color[2][3] = 1.0;
          break;
        default:
          g_assert_not_reached ();
          break;
      }
    }
  }

  priv->clear_color[2] = priv->clear_color[0];
  priv->clear_color[2].color[0][3] = 0.0;
}

static gboolean
gst_d3d12_compositor_clear_pad_context (GstD3D12Compositor * self,
    GstD3D12CompositorPad * cpad, gpointer user_data)
{
  auto priv = cpad->priv;
  priv->ctx = nullptr;

  return TRUE;
}

static gboolean
gst_d3d12_compositor_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  auto priv = self->priv;
  GstVideoInfo info;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Failed to convert caps to info");
    return FALSE;
  }

  auto features = gst_caps_get_features (caps, 0);
  if (features
      && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY)) {
    GST_DEBUG_OBJECT (self, "Negotiated with D3D12 memory caps");
    priv->downstream_supports_d3d12 = TRUE;
  } else {
    GST_DEBUG_OBJECT (self, "Negotiated with system memory caps");
    priv->downstream_supports_d3d12 = FALSE;
  }

  if (GST_VIDEO_INFO_FORMAT (&info) !=
      GST_VIDEO_INFO_FORMAT (&priv->negotiated_info)) {
    gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
        (GstElementForeachPadFunc) gst_d3d12_compositor_clear_pad_context,
        nullptr);
    priv->bg_render = nullptr;
  }
  gst_clear_buffer (&priv->fallback_buf);

  priv->negotiated_info = info;
  gst_d3d12_compositor_calculate_background_color (self, &info);

  if (!priv->bg_render) {
    auto bg_render = std::make_unique < BackgroundRender > (self->device, info);
    if (!bg_render->is_valid) {
      GST_ERROR_OBJECT (self, "Couldn't configure background render object");
      return FALSE;
    }

    priv->bg_render = std::move (bg_render);
  } else {
    priv->bg_render->viewport.Width = info.width;
    priv->bg_render->viewport.Height = info.height;
    priv->bg_render->scissor_rect.right = info.width;
    priv->bg_render->scissor_rect.bottom = info.height;
  }

  if (!priv->downstream_supports_d3d12) {
    auto pool = gst_d3d12_buffer_pool_new (self->device);
    auto config = gst_buffer_pool_get_config (pool);
    auto params = gst_d3d12_allocation_params_new (self->device, &info,
        GST_D3D12_ALLOCATION_FLAG_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
    gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
    gst_d3d12_allocation_params_free (params);
    gst_buffer_pool_config_set_params (config, caps, info.size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (self, "Couldn't set pool config");
      gst_object_unref (pool);
      return FALSE;
    }

    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to set active");
      gst_object_unref (pool);
      return FALSE;
    }

    gst_buffer_pool_acquire_buffer (pool, &priv->fallback_buf, nullptr);
    gst_buffer_pool_set_active (pool, FALSE);
    gst_object_unref (pool);

    if (!priv->fallback_buf) {
      GST_ERROR_OBJECT (self, "Couldn't acquire fallback buf");
      return FALSE;
    }
  }

  return GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps);
}

static gboolean
gst_d3d12_compositor_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  gboolean is_d3d12 = FALSE;
  guint size;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  auto features = gst_caps_get_features (caps, 0);
  if (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY)) {
    GST_DEBUG_OBJECT (pad, "Upstream support d3d12 memory");
    is_d3d12 = TRUE;
  }

  if (gst_query_get_n_allocation_pools (query) == 0) {
    if (is_d3d12)
      pool = gst_d3d12_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();

    if (!pool) {
      GST_ERROR_OBJECT (self, "Failed to create buffer pool");
      return FALSE;
    }

    auto config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    size = GST_VIDEO_INFO_SIZE (&info);
    if (is_d3d12) {
      auto params = gst_d3d12_allocation_params_new (self->device,
          &info, GST_D3D12_ALLOCATION_FLAG_DEFAULT,
          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
          D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);

      gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
      gst_d3d12_allocation_params_free (params);
    } else {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    gst_buffer_pool_config_set_params (config, caps, (guint) size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (pool, "Couldn't set config");
      gst_object_unref (pool);

      return FALSE;
    }

    /* d3d12 buffer pool will update buffer size based on allocated texture,
     * get size from config again */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config,
        nullptr, &size, nullptr, nullptr);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  if (is_d3d12) {
    gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE,
        nullptr);
  }

  return TRUE;
}

static gboolean
gst_d3d12_compositor_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  auto priv = self->priv;
  GstCaps *caps;
  GstBufferPool *pool = nullptr;
  guint n, size, min, max;
  GstVideoInfo info;
  gboolean use_d3d12_pool;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_DEBUG_OBJECT (self, "No output caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Invalid caps");
    return FALSE;
  }

  use_d3d12_pool = priv->downstream_supports_d3d12;

  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  /* create our own pool */
  if (pool && use_d3d12_pool) {
    if (!GST_IS_D3D12_BUFFER_POOL (pool)) {
      GST_DEBUG_OBJECT (self,
          "Downstream pool is not d3d12, will create new one");
      gst_clear_object (&pool);
    } else {
      GstD3D12BufferPool *dpool = GST_D3D12_BUFFER_POOL (pool);
      if (dpool->device != self->device) {
        GST_DEBUG_OBJECT (self, "Different device, will create new one");
        gst_clear_object (&pool);
      }
    }
  }

  size = (guint) info.size;

  if (!pool) {
    if (use_d3d12_pool)
      pool = gst_d3d12_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();

    min = 0;
    max = 0;
  }

  auto config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (use_d3d12_pool) {
    auto params = gst_buffer_pool_config_get_d3d12_allocation_params (config);
    if (!params) {
      params = gst_d3d12_allocation_params_new (self->device, &info,
          GST_D3D12_ALLOCATION_FLAG_DEFAULT,
          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
          D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
    } else {
      gst_d3d12_allocation_params_set_resource_flags (params,
          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
          D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
    }

    gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
    gst_d3d12_allocation_params_free (params);
  }

  gst_buffer_pool_set_config (pool, config);

  /* d3d12 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_d3d12_compositor_draw_background (GstD3D12Compositor * self)
{
  auto priv = self->priv;
  ClearColor *color = &priv->clear_color[0];
  auto bg_render = priv->bg_render.get ();
  auto & rtv_handles = priv->rtv_handles;
  std::vector < D3D12_RECT > rtv_rects;

  rtv_handles.clear ();
  for (guint i = 0; i < gst_buffer_n_memory (priv->generated_output_buf); i++) {
    auto mem = (GstD3D12Memory *)
        gst_buffer_peek_memory (priv->generated_output_buf, i);
    auto num_planes = gst_d3d12_memory_get_plane_count (mem);
    ComPtr < ID3D12DescriptorHeap > rtv_heap;

    if (!gst_d3d12_memory_get_render_target_view_heap (mem, &rtv_heap)) {
      GST_ERROR_OBJECT (self, "Couldn't get rtv heap");
      return FALSE;
    }

    auto cpu_handle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE
        (rtv_heap->GetCPUDescriptorHandleForHeapStart ());

    for (guint plane = 0; plane < num_planes; plane++) {
      D3D12_RECT rect = { };
      gst_d3d12_memory_get_plane_rectangle (mem, plane, &rect);
      rtv_rects.push_back (rect);
      rtv_handles.push_back (cpu_handle);
      cpu_handle.Offset (bg_render->rtv_inc_size);
    }
  }

  GstD3D12CommandAllocator *gst_ca;
  if (!gst_d3d12_command_allocator_pool_acquire (bg_render->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    return FALSE;
  }

  GstD3D12FenceData *fence_data;
  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  auto hr = ca->Reset ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command allocator");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  if (!bg_render->cl) {
    auto device = gst_d3d12_device_get_device_handle (self->device);
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), bg_render->pso.Get (), IID_PPV_ARGS (&bg_render->cl));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create command list");
      gst_d3d12_fence_data_unref (fence_data);
      return FALSE;
    }
  } else {
    hr = bg_render->cl->Reset (ca.Get (), bg_render->pso.Get ());
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't reset command list");
      gst_d3d12_fence_data_unref (fence_data);
      return FALSE;
    }
  }

  auto cl = bg_render->cl;
  if (bg_render->vertex_index_upload) {
    cl->CopyResource (bg_render->vertex_index_buf.Get (),
        bg_render->vertex_index_upload.Get ());
    D3D12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition (bg_render->
        vertex_index_buf.Get (),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
        D3D12_RESOURCE_STATE_INDEX_BUFFER);
    cl->ResourceBarrier (1, &barrier);
  }

  if (priv->background == GST_D3D12_COMPOSITOR_BACKGROUND_CHECKER) {
    cl->SetGraphicsRootSignature (bg_render->rs.Get ());
    cl->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->IASetIndexBuffer (&bg_render->ibv);
    cl->IASetVertexBuffers (0, 1, &bg_render->vbv);
    cl->RSSetViewports (1, &bg_render->viewport);
    cl->RSSetScissorRects (1, &bg_render->scissor_rect);
    cl->OMSetRenderTargets (1, rtv_handles.data (), FALSE, nullptr);
    cl->DrawIndexedInstanced (6, 1, 0, 0, 0);

    /* clear U and V components if needed */
    for (size_t i = 1; i < rtv_handles.size (); i++) {
      cl->ClearRenderTargetView (rtv_handles[i], color->color[i], 1,
          &rtv_rects[i]);
    }
  } else {
    switch (priv->background) {
      case GST_D3D12_COMPOSITOR_BACKGROUND_BLACK:
        color = &priv->clear_color[0];
        break;
      case GST_D3D12_COMPOSITOR_BACKGROUND_WHITE:
        color = &priv->clear_color[1];
        break;
      case GST_D3D12_COMPOSITOR_BACKGROUND_TRANSPARENT:
        color = &priv->clear_color[2];
        break;
      default:
        g_assert_not_reached ();
        return FALSE;
    }

    for (size_t i = 0; i < priv->rtv_handles.size (); i++) {
      cl->ClearRenderTargetView (rtv_handles[i], color->color[i], 1,
          &rtv_rects[i]);
    }
  }

  hr = cl->Close ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  ID3D12CommandList *cmd_list[] = { cl.Get () };

  if (!gst_d3d12_device_execute_command_lists (self->device,
          D3D12_COMMAND_LIST_TYPE_DIRECT, 1, cmd_list,
          &priv->bg_render->fence_val)) {
    GST_ERROR_OBJECT (self, "Couldn't execute command list");
    gst_d3d12_fence_data_unref (fence_data);
    return FALSE;
  }

  gst_d3d12_buffer_after_write (priv->generated_output_buf,
      bg_render->fence_val);

  if (bg_render->vertex_index_upload) {
    gst_d3d12_fence_data_add_notify_com (fence_data,
        bg_render->vertex_index_upload.Detach ());
  }

  gst_d3d12_device_set_fence_notify (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT, priv->bg_render->fence_val, fence_data);

  return TRUE;
}

static GstFlowReturn
gst_d3d12_compositor_aggregate_frames (GstVideoAggregator * vagg,
    GstBuffer * outbuf)
{
  auto self = GST_D3D12_COMPOSITOR (vagg);
  auto priv = self->priv;
  GList *iter;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "aggregate");

  if (!priv->generated_output_buf) {
    GST_ERROR_OBJECT (self, "No generated output buffer");
    return GST_FLOW_ERROR;
  }

  auto completed = gst_d3d12_device_get_completed_value (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);
  while (!priv->scheduled.empty ()) {
    if (priv->scheduled.front () > completed)
      break;

    priv->scheduled.pop ();
  }

  /* avoid too large buffering */
  if (priv->scheduled.size () > 2) {
    auto fence_to_wait = priv->scheduled.front ();
    priv->scheduled.pop ();
    GST_LOG_OBJECT (self, "Waiting for previous command, %" G_GUINT64_FORMAT,
        fence_to_wait);
    gst_d3d12_device_fence_wait (self->device,
        D3D12_COMMAND_LIST_TYPE_DIRECT, fence_to_wait,
        priv->bg_render->event_handle);
  }

  if (!gst_d3d12_compositor_draw_background (self)) {
    GST_ERROR_OBJECT (self, "Couldn't draw background");
    return GST_FLOW_ERROR;
  }

  guint64 fence_val = priv->bg_render->fence_val;
  GST_OBJECT_LOCK (self);
  for (iter = GST_ELEMENT (vagg)->sinkpads; iter; iter = g_list_next (iter)) {
    auto pad = GST_VIDEO_AGGREGATOR_PAD (iter->data);
    auto cpad = GST_D3D12_COMPOSITOR_PAD (pad);
    auto pad_priv = cpad->priv;

    /* Might be a case where pad was added between prepare_frame() and
     * aggregate_frames() */
    if (!pad_priv->prepare_rst.valid ()) {
      GST_DEBUG_OBJECT (pad, "Ignoring non-prepared pad");
      continue;
    }

    GST_LOG_OBJECT (cpad, "Waiting for command list building thread");
    auto prepare_ret = pad_priv->prepare_rst.get ();
    if (!prepare_ret) {
      GST_ERROR_OBJECT (pad, "Couldn't build command list");
      ret = GST_FLOW_ERROR;
      break;
    }

    if (!gst_video_aggregator_pad_get_prepared_frame (pad))
      continue;

    GST_LOG_OBJECT (cpad, "Command list prepared");

    ID3D12CommandList *cmd_list[] = { pad_priv->ctx->cl.Get () };
    if (!gst_d3d12_device_execute_command_lists (self->device,
            D3D12_COMMAND_LIST_TYPE_DIRECT, 1, cmd_list,
            &pad_priv->ctx->fence_val)) {
      GST_ERROR_OBJECT (self, "Couldn't execute command list");
      ret = GST_FLOW_ERROR;
      break;
    }

    fence_val = pad_priv->ctx->fence_val;
    gst_d3d12_buffer_after_write (priv->generated_output_buf, fence_val);
  }
  GST_OBJECT_UNLOCK (self);

  if (ret != GST_FLOW_OK)
    return ret;

  priv->scheduled.push (fence_val);
  if (priv->generated_output_buf != outbuf) {
    GstVideoFrame out_frame, in_frame;
    if (!gst_video_frame_map (&in_frame, &vagg->info,
            priv->generated_output_buf, GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "Couldn't map generated buffer");
      return GST_FLOW_ERROR;
    }

    if (!gst_video_frame_map (&out_frame, &vagg->info, outbuf, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (self, "Couldn't map output buffer");
      gst_video_frame_unmap (&in_frame);
      return GST_FLOW_ERROR;
    }

    auto copy_ret = gst_video_frame_copy (&out_frame, &in_frame);
    gst_video_frame_unmap (&out_frame);
    gst_video_frame_unmap (&in_frame);
    if (!copy_ret) {
      GST_ERROR_OBJECT (self, "Couldn't copy frame");
      return GST_FLOW_ERROR;
    }
  }

  return GST_FLOW_OK;
}

struct DeviceCheckData
{
  /* without holding ref */
  GstD3D12Device *other_device = nullptr;
  gboolean have_same_device = FALSE;
};

static gboolean
gst_d3d12_compositor_check_device_update (GstElement * agg,
    GstVideoAggregatorPad * vpad, DeviceCheckData * data)
{
  auto self = GST_D3D12_COMPOSITOR (agg);
  GstBuffer *buf;
  GstMemory *mem;
  GstD3D12Memory *dmem;

  buf = gst_video_aggregator_pad_get_current_buffer (vpad);
  if (!buf)
    return TRUE;

  /* Ignore gap buffer */
  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP) ||
      gst_buffer_get_size (buf) == 0) {
    return TRUE;
  }

  mem = gst_buffer_peek_memory (buf, 0);
  if (!gst_is_d3d12_memory (mem))
    return TRUE;

  dmem = GST_D3D12_MEMORY_CAST (mem);

  /* We can use existing device */
  if (dmem->device == self->device) {
    data->have_same_device = TRUE;
    return FALSE;
  }

  data->other_device = dmem->device;

  /* Keep iterate since there might be one buffer which holds the same device
   * as ours */
  return TRUE;
}

static GstFlowReturn
gst_d3d12_compositor_create_output_buffer (GstVideoAggregator * vagg,
    GstBuffer ** outbuffer)
{
  auto self = GST_D3D12_COMPOSITOR (vagg);
  auto priv = self->priv;
  DeviceCheckData data;

  /* Check whether there is at least one sinkpad which holds d3d12 buffer
   * with compatible device, and if not, update our device */
  data.other_device = nullptr;
  data.have_same_device = FALSE;

  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg),
      (GstElementForeachPadFunc) gst_d3d12_compositor_check_device_update,
      &data);

  priv->generated_output_buf = nullptr;
  if (data.have_same_device || !data.other_device) {
    GstBuffer *buf = nullptr;
    auto ret =
        GST_VIDEO_AGGREGATOR_CLASS (parent_class)->create_output_buffer (vagg,
        &buf);
    if (ret != GST_FLOW_OK)
      return ret;

    if (priv->downstream_supports_d3d12)
      priv->generated_output_buf = buf;
    else
      priv->generated_output_buf = priv->fallback_buf;

    *outbuffer = buf;
    return GST_FLOW_OK;
  }

  /* Clear all device dependent resources */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (vagg),
      (GstElementForeachPadFunc) gst_d3d12_compositor_clear_pad_context,
      nullptr);

  gst_clear_buffer (&priv->fallback_buf);
  priv->bg_render = nullptr;
  priv->scheduled = { };

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, data.other_device);
  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    gst_object_unref (self->device);
    self->device = (GstD3D12Device *) gst_object_ref (data.other_device);
  }

  /* We cannot call gst_aggregator_negotiate() here, since GstVideoAggregator
   * is holding GST_VIDEO_AGGREGATOR_LOCK() already.
   * Mark reconfigure and do reconfigure later */
  gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (vagg));

  return GST_AGGREGATOR_FLOW_NEED_DATA;
}
