/*
 * GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#include <string>
#include <queue>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct DisplayContext
{
  HWND window_handle = nullptr;
  GstElement *pipeline = nullptr;
  GstElement *sink = nullptr;
  GIOChannel *io_ch = nullptr;

  bool enable_overlay = false;

  ID2D1Factory *d2d_factory = nullptr;
  IDWriteFactory *dwrite_factory = nullptr;

  IDWriteTextFormat *format = nullptr;
  IDWriteTextLayout *layout = nullptr;

  /* D3D objects for background redraw with alpha blending */
  ID3D11BlendState *blend = nullptr;
  ID3D11PixelShader *ps = nullptr;
  ID3D11VertexShader *vs = nullptr;
  ID3D11InputLayout *input_layout = nullptr;
  ID3D11Buffer *index_buf = nullptr;
  ID3D11Buffer *vertex_buf = nullptr;

  UINT width = 0;
  UINT height = 0;

  SRWLOCK lock = RTL_SRWLOCK_INIT;
  double avg_framerate = 0;
  double last_framerate = 0;

  LARGE_INTEGER frequency;
  std::queue < LARGE_INTEGER > render_timestamp;

  GMainLoop *loop = nullptr;
};

static const gchar templ_vs_color[] =
    "struct VS_INPUT {\n"
    "  float4 Position: POSITION;\n"
    "  float4 Color: COLOR;\n"
    "};\n"
    "struct VS_OUTPUT {\n"
    "  float4 Position: SV_POSITION;\n"
    "  float4 Color: COLOR;\n"
    "};\n"
    "VS_OUTPUT main (VS_INPUT input)\n"
    "{\n"
    "  return input;\n"
    "}";

static const gchar templ_ps_color[] =
    "struct PS_INPUT {\n"
    "  float4 Position: SV_POSITION;\n"
    "  float4 Color: COLOR;\n"
    "};\n"
    "float4 main(PS_INPUT input) : SV_TARGET\n"
    "{\n"
    "  return input.Color;\n"
    "}";
/* *INDENT-ON* */

#define DISPLAY_CONTEXT_PROP "d3d11videosink.example.context"

#define CLEAR_COM(obj) do { \
  if (obj) { \
    obj->Release (); \
    obj = nullptr; \
  } \
} while (0)

static LRESULT CALLBACK
window_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  DisplayContext *context = (DisplayContext *) GetPropA (hwnd,
      DISPLAY_CONTEXT_PROP);

  switch (message) {
    case WM_DESTROY:
      gst_println ("Window is destroying");
      if (context) {
        context->window_handle = nullptr;
        RemovePropA (hwnd, DISPLAY_CONTEXT_PROP);
        g_main_loop_quit (context->loop);
      }
      break;
    case WM_LBUTTONUP:
      if (!context) {
        gst_printerrln ("Display context is not attached on HWND");
      } else {
        context->enable_overlay = !context->enable_overlay;
        gst_println ("Enable overlay %d", context->enable_overlay);
        /* Call expose method so that videosink can immediately
         * redraw client area */
        if (context->sink)
          gst_video_overlay_expose (GST_VIDEO_OVERLAY (context->sink));
      }
      break;
    default:
      break;
  }

  return DefWindowProc (hwnd, message, wParam, lParam);
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, DisplayContext * context)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s", err->message);
      if (dbg)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (context->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_println ("Got EOS");
      g_main_loop_quit (context->loop);
      break;
    default:
      break;
  }

  return TRUE;
}

static gboolean
msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

typedef struct
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
} VertexData;

static void
ensure_d3d11_resource (DisplayContext * context, GstD3D11Device * device)
{
  D3D11_BLEND_DESC blend_desc;
  D3D11_INPUT_ELEMENT_DESC input_desc[2];
  D3D11_BUFFER_DESC buffer_desc;
  D3D11_MAPPED_SUBRESOURCE map;
  VertexData *vertex_data;
  WORD *indices;
  HRESULT hr;
  ID3D11Device *device_handle;
  ID3D11DeviceContext *context_handle;

  if (context->blend)
    return;

  device_handle = gst_d3d11_device_get_device_handle (device);
  context_handle = gst_d3d11_device_get_device_context_handle (device);

  ZeroMemory (&blend_desc, sizeof (blend_desc));
  ZeroMemory (input_desc, sizeof (input_desc));
  ZeroMemory (&buffer_desc, sizeof (buffer_desc));

  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "COLOR";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  hr = gst_d3d11_create_vertex_shader_simple (device, templ_vs_color,
      "main", input_desc, G_N_ELEMENTS (input_desc), &context->vs,
      &context->input_layout);
  g_assert (SUCCEEDED (hr));

  hr = gst_d3d11_create_pixel_shader_simple (device,
      templ_ps_color, "main", &context->ps);

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (VertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  hr = device_handle->CreateBuffer (&buffer_desc, nullptr,
      &context->vertex_buf);
  g_assert (SUCCEEDED (hr));

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  hr = device_handle->CreateBuffer (&buffer_desc, nullptr, &context->index_buf);
  g_assert (SUCCEEDED (hr));

  blend_desc.AlphaToCoverageEnable = FALSE;
  blend_desc.IndependentBlendEnable = FALSE;
  blend_desc.RenderTarget[0].BlendEnable = TRUE;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  hr = device_handle->CreateBlendState (&blend_desc, &context->blend);
  g_assert (SUCCEEDED (hr));

  hr = context_handle->Map (context->vertex_buf, 0, D3D11_MAP_WRITE_DISCARD, 0,
      &map);
  g_assert (SUCCEEDED (hr));
  vertex_data = (VertexData *) map.pData;

  hr = context_handle->Map (context->index_buf, 0, D3D11_MAP_WRITE_DISCARD, 0,
      &map);
  g_assert (SUCCEEDED (hr));
  indices = (WORD *) map.pData;
  for (guint i = 0; i < 4; i++) {
    vertex_data[i].color.r = 0.0f;
    vertex_data[i].color.g = 0.5f;
    vertex_data[i].color.b = 0.5f;
    vertex_data[i].color.a = 0.5f;
  }

  /* bottom left */
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;

  /* top left */
  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;

  /* top right */
  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;

  /* bottom right */
  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;

  /* clockwise indexing */
  indices[0] = 0;               /* bottom left */
  indices[1] = 1;               /* top left */
  indices[2] = 2;               /* top right */

  indices[3] = 3;               /* bottom right */
  indices[4] = 0;               /* bottom left  */
  indices[5] = 2;               /* top right */

  context_handle->Unmap (context->vertex_buf, 0);
  context_handle->Unmap (context->index_buf, 0);
}

/* This callback will be called with gst_d3d11_device_lock() taken by
 * d3d11videosink. We can perform GPU operation here safely */
static void
on_present (GstElement * sink, GstD3D11Device * device,
    ID3D11RenderTargetView * rtv, DisplayContext * context)
{
  ComPtr < ID3D11Resource > resource;
  ComPtr < ID3D11Texture2D > texture;
  ComPtr < IDXGISurface > surface;
  ComPtr < ID2D1RenderTarget > d2d_target;
  ComPtr < ID2D1SolidColorBrush > text_brush;
  ID3D11DeviceContext *device_context;
  HRESULT hr;
  D3D11_TEXTURE2D_DESC desc;
  ID2D1Factory *d2d_factory;
  double framerate;
  D3D11_VIEWPORT viewport;
  UINT vertex_stride = sizeof (VertexData);
  UINT offsets = 0;

  if (!context->enable_overlay)
    return;

  rtv->GetResource (&resource);
  hr = resource.As (&texture);
  g_assert (SUCCEEDED (hr));

  hr = texture.As (&surface);
  g_assert (SUCCEEDED (hr));

  texture->GetDesc (&desc);

  ensure_d3d11_resource (context, device);
  device_context = gst_d3d11_device_get_device_context_handle (device);

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = desc.Width;
  viewport.Height = desc.Height / 5.0f;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  /* Draw background using D3D11 */
  device_context->IASetPrimitiveTopology
      (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  device_context->IASetInputLayout (context->input_layout);
  device_context->IASetVertexBuffers (0, 1, &context->vertex_buf,
      &vertex_stride, &offsets);
  device_context->IASetIndexBuffer (context->index_buf, DXGI_FORMAT_R16_UINT,
      0);
  device_context->VSSetShader (context->vs, nullptr, 0);
  device_context->PSSetShader (context->ps, nullptr, 0);
  device_context->RSSetViewports (1, &viewport);
  device_context->OMSetRenderTargets (1, &rtv, nullptr);
  device_context->OMSetBlendState (context->blend, nullptr, 0xffffffff);
  device_context->DrawIndexed (6, 0, 0);

  /* Creates new layout on window size or framerate change */
  AcquireSRWLockExclusive (&context->lock);
  framerate = context->avg_framerate;
  if (context->layout && (context->width != desc.Width ||
          context->height != desc.Height
          || context->last_framerate != framerate)) {
    CLEAR_COM (context->layout);
  }
  context->last_framerate = framerate;
  ReleaseSRWLockExclusive (&context->lock);

  context->width = desc.Width;
  context->height = desc.Height;

  if (!context->layout) {
    IDWriteFactory *factory = context->dwrite_factory;
    std::wstring overlay_string;
    wchar_t fps_buf[128];
    DWRITE_TEXT_METRICS metrics;
    FLOAT font_size;
    bool was_decreased = false;
    DWRITE_TEXT_RANGE range;

    overlay_string = L"Text Overlay, FPS: ";
    std::swprintf (fps_buf, L"%.1f", framerate);

    overlay_string += fps_buf;

    hr = factory->CreateTextLayout (overlay_string.c_str (),
        overlay_string.length (),
        context->format, desc.Width, desc.Height / 5.0f, &context->layout);
    g_assert (SUCCEEDED (hr));

    context->layout->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
    context->layout->SetParagraphAlignment (DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    range.startPosition = 0;
    range.length = overlay_string.length ();

    /* Calculate best font size */
    do {
      hr = context->layout->GetMetrics (&metrics);
      g_assert (SUCCEEDED (hr));

      context->layout->GetFontSize (0, &font_size);
      if (metrics.widthIncludingTrailingWhitespace >= (FLOAT) desc.Width) {
        if (font_size > 1.0f) {
          font_size -= 0.5f;
          was_decreased = true;
          hr = context->layout->SetFontSize (font_size, range);
          g_assert (SUCCEEDED (hr));
          continue;
        }

        break;
      }

      if (was_decreased)
        break;

      if (metrics.widthIncludingTrailingWhitespace < (FLOAT) desc.Width) {
        if (metrics.widthIncludingTrailingWhitespace >= desc.Width * 0.7)
          break;

        font_size += 0.5f;
        hr = context->layout->SetFontSize (font_size, range);
        g_assert (SUCCEEDED (hr));
        continue;
      }
    } while (true);
  }

  d2d_factory = context->d2d_factory;
  D2D1_RENDER_TARGET_PROPERTIES props;
  props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
  props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  /* default DPI */
  props.dpiX = 0;
  props.dpiY = 0;
  props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
  props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

  /* Creates D2D render target using swapchin's backbuffer */
  hr = d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (), props,
      &d2d_target);
  g_assert (SUCCEEDED (hr));

  /* text brush */
  hr = d2d_target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black),
      &text_brush);
  g_assert (SUCCEEDED (hr));

  d2d_target->BeginDraw ();
  /* Draw text */
  d2d_target->DrawTextLayout (D2D1::Point2F (0, 0),
      context->layout, text_brush.Get (), D2D1_DRAW_TEXT_OPTIONS_NONE);
  d2d_target->EndDraw ();
}

static GstPadProbeReturn
framerate_calculate_probe (GstPad * pad, GstPadProbeInfo * info,
    DisplayContext * context)
{
  LARGE_INTEGER now;

  AcquireSRWLockExclusive (&context->lock);

  QueryPerformanceCounter (&now);
  context->render_timestamp.push (now);

  if (context->render_timestamp.size () > 10) {
    LARGE_INTEGER last = context->render_timestamp.back ();
    LARGE_INTEGER first = context->render_timestamp.front ();
    double diff = last.QuadPart - first.QuadPart;
    context->avg_framerate =
        (double) context->frequency.QuadPart *
        (context->render_timestamp.size () - 1) / diff;

    std::queue < LARGE_INTEGER > empty_queue;
    std::swap (context->render_timestamp, empty_queue);
  }

  ReleaseSRWLockExclusive (&context->lock);

  return GST_PAD_PROBE_OK;
}

gint
main (gint argc, gchar ** argv)
{
  WNDCLASSEXA wc = { 0, };
  HINSTANCE hinstance = GetModuleHandle (nullptr);
  GOptionContext *option_ctx;
  GError *error = nullptr;
  RECT wr = { 0, 0, 320, 240 };
  gboolean ret;
  gchar *uri = nullptr;
  GOptionEntry options[] = {
    {"uri", 0, 0, G_OPTION_ARG_STRING, &uri, "URI to play", nullptr},
    {nullptr}
  };
  HRESULT hr;
  DisplayContext context;
  GstPad *pad;

  option_ctx =
      g_option_context_new ("d3d11videosink \"present\" signal example");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", error->message);
    g_clear_error (&error);
    return 1;
  }

  if (!uri) {
    gst_printerrln ("File name or URI must be provided");
    return 1;
  }

  if (!gst_uri_is_valid (uri)) {
    gchar *file = gst_filename_to_uri (uri, nullptr);
    g_free (uri);
    uri = file;
  }

  if (!uri) {
    gst_printerrln ("No valid URI");
    return 1;
  }

  /* Prepare device independent D2D objects */
  hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
      IID_PPV_ARGS (&context.d2d_factory));
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create D2D factory");
    return 1;
  }

  hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
      __uuidof (context.dwrite_factory),
      reinterpret_cast < IUnknown ** >(&context.dwrite_factory));
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create DirectWrite factory");
    return 1;
  }

  /* Font size will be re-calculated on present */
  hr = context.dwrite_factory->CreateTextFormat (L"Consolas", nullptr,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &context.format);

  /* For rendered framerate calculation */
  QueryPerformanceFrequency (&context.frequency);

  context.loop = g_main_loop_new (nullptr, FALSE);

  wc.cbSize = sizeof (WNDCLASSEXA);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = (WNDPROC) window_proc;
  wc.hInstance = hinstance;
  wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
  wc.lpszClassName = "GstD3D11VideoSinkExample";
  RegisterClassExA (&wc);

  AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);
  context.window_handle =
      CreateWindowExA (0, wc.lpszClassName, "GstD3D11VideoSinkExample",
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr,
      hinstance, nullptr);

  context.io_ch = g_io_channel_win32_new_messages (0);
  g_io_add_watch (context.io_ch, G_IO_IN, msg_cb, context.window_handle);

  context.pipeline = gst_element_factory_make ("playbin", nullptr);
  g_assert (context.pipeline);

  context.sink = gst_element_factory_make ("d3d11videosink", nullptr);
  g_assert (context.sink);

  /* Enables present signal */
  g_object_set (context.sink, "emit-present", TRUE, nullptr);

  /* D2D <-> DXGI interop requires BGRA format */
  g_object_set (context.sink, "display-format", DXGI_FORMAT_B8G8R8A8_UNORM,
      nullptr);

  g_signal_connect (context.sink, "present", G_CALLBACK (on_present), &context);
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (context.sink),
      (guintptr) context.window_handle);

  /* Attach our display context on HWND */
  SetPropA (context.window_handle, DISPLAY_CONTEXT_PROP, &context);

  g_object_set (context.pipeline,
      "uri", uri, "video-sink", context.sink, nullptr);
  gst_bus_add_watch (GST_ELEMENT_BUS (context.pipeline),
      (GstBusFunc) bus_msg, &context);

  pad = gst_element_get_static_pad (context.sink, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) framerate_calculate_probe, &context, nullptr);

  if (gst_element_set_state (context.pipeline, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    gst_printerrln ("Could not set state to playing for uri %s", uri);
    return 1;
  }

  ShowWindow (context.window_handle, SW_SHOW);
  gst_println ("Click window client area to toggle overlay");

  g_main_loop_run (context.loop);

  gst_element_set_state (context.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (context.pipeline));
  gst_object_unref (context.pipeline);
  g_io_channel_unref (context.io_ch);

  if (context.window_handle)
    DestroyWindow (context.window_handle);

  CLEAR_COM (context.blend);
  CLEAR_COM (context.ps);
  CLEAR_COM (context.vs);
  CLEAR_COM (context.input_layout);
  CLEAR_COM (context.index_buf);
  CLEAR_COM (context.vertex_buf);

  CLEAR_COM (context.layout);

  context.format->Release ();
  context.d2d_factory->Release ();
  context.dwrite_factory->Release ();

  g_main_loop_unref (context.loop);

  return 0;
}
