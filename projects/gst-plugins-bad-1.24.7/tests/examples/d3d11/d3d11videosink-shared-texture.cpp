/*
 * GStreamer
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

#include <gst/gst.h>
#include <gst/video/video.h>

#include "d3d11device.h"
#include <wrl.h>
#include <mutex>

using namespace Microsoft::WRL;

static gboolean use_keyed_mutex = FALSE;
static gboolean use_nt_handle = FALSE;
static gchar *texture_foramt = nullptr;
static gchar *uri = nullptr;

static GMainLoop *loop = nullptr;
static gboolean visible = FALSE;
static HWND hwnd = nullptr;
std::mutex lock;

/* Device handles */
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGIFactory2> factory;

/* SwapChain resources */
ComPtr<IDXGISwapChain1> swapchain;
ComPtr<ID3D11RenderTargetView> rtv;

/* Shard texture resources */
ComPtr<ID3D11Texture2D> shared_texture;
ComPtr<ID3D11ShaderResourceView> srv;
ComPtr<IDXGIKeyedMutex> keyed_mutex;
HANDLE shared_handle = nullptr;
UINT misc_flags = 0;

static void
on_begin_draw (GstElement * videosink, gpointer user_data)
{
  std::lock_guard<std::mutex> lk (lock);
  GstElement *sink = GST_ELEMENT (user_data);
  gboolean ret = TRUE;
  HRESULT hr;

  /* Windows was destroyed, nothing to draw */
  if (!hwnd)
    return;

  if (!shared_handle) {
    gst_printerrln ("Shared handle wasn't configured");
    exit (-1);
  }

  g_signal_emit_by_name (sink,
      "draw", shared_handle, misc_flags, 0, 0, &ret);

  if (!ret) {
    gst_printerrln ("Failed to draw on shared handle");
    if (loop)
      g_main_loop_quit (loop);

    return;
  }

  /* Acquire sync */
  if (keyed_mutex) {
    hr = keyed_mutex->AcquireSync (0, INFINITE);
    if (FAILED (hr)) {
      gst_printerrln ("Failed to acquire sync");
      exit (-1);
    }
  }

  ID3D11RenderTargetView *render_target_view = rtv.Get();
  context->OMSetRenderTargets (1, &render_target_view, nullptr);

  ID3D11ShaderResourceView *shader_resource = srv.Get();
  context->PSSetShaderResources (0, 1, &shader_resource);

  context->DrawIndexed (6, 0, 0);
  if (keyed_mutex)
    keyed_mutex->ReleaseSync (0);

  swapchain->Present (0, 0);
}

static void
on_resize (void)
{
  std::lock_guard<std::mutex> lk (lock);
  ComPtr<ID3D11Texture2D> backbuffer;

  rtv = nullptr;

  HRESULT hr = swapchain->ResizeBuffers (0,
      /* Specify zero width/height to use the size of current client area */
      0, 0,
      /* Reuse configured format */
      DXGI_FORMAT_UNKNOWN,
      0);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't resize swapchain");
    exit(-1);
  }

  hr = swapchain->GetBuffer (0, IID_PPV_ARGS (&backbuffer));
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get backbuffer from swapchain");
    exit(-1);
  }

  hr = device->CreateRenderTargetView (backbuffer.Get(), nullptr, &rtv);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get ID3D11RenderTargetView from backbuffer");
    exit(-1);
  }

  D3D11_TEXTURE2D_DESC desc;
  backbuffer->GetDesc(&desc);

  D3D11_VIEWPORT viewport;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = desc.Width;
  viewport.Height = desc.Height;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  context->RSSetViewports (1, &viewport);
}

static LRESULT CALLBACK
window_proc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_DESTROY:
    {
      std::lock_guard<std::mutex> lk (lock);
      hwnd = NULL;
      if (loop)
        g_main_loop_quit (loop);
      break;
    }
    case WM_SIZE:
      on_resize ();
      break;
    default:
      break;
  }

  return DefWindowProc (hWnd, message, wParam, lParam);
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, GstElement * pipeline)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:
      /* make window visible when we have something to show */
      if (!visible && hwnd) {
        ShowWindow (hwnd, SW_SHOW);
        visible = TRUE;
      }
      gst_element_set_state (pipeline, GST_STATE_PLAYING);
      break;
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("ERROR %s \n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline, *sink;
  GstStateChangeReturn sret;
  WNDCLASSEX wc = { 0, };
  HINSTANCE hinstance = GetModuleHandle (NULL);
  GIOChannel *msg_io_channel = NULL;
  RECT wr = { 0, 0, 320, 240 };
  ComPtr<ID3D11SamplerState> sampler;
  ComPtr<ID3D11PixelShader> ps;
  ComPtr<ID3D11VertexShader> vs;
  ComPtr<ID3D11InputLayout> layout;
  ComPtr<ID3D11Buffer> vertex;
  ComPtr<ID3D11Buffer> index;
  HRESULT hr;
  GOptionEntry options[] = {
    {"use-keyed-mutex", 0, 0, G_OPTION_ARG_NONE, &use_keyed_mutex,
        "Allocate shared texture with D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX flag",
        NULL},
    {"use-nt-handle", 0, 0, G_OPTION_ARG_NONE, &use_nt_handle,
        "Allocate shared texture with D3D11_RESOURCE_MISC_SHARED_NTHANDLE flag",
        NULL},
    {"texture-format", 0, 0, G_OPTION_ARG_STRING, &texture_foramt,
        "texture format to test, supported arguments are { BGRA, RGBA, RGB10A2_LE }",
        NULL},
    {"uri", 0, 0, G_OPTION_ARG_STRING, &uri,
        "URI to test (if unspecified, videotestsrc will be used)",
        NULL},
    {NULL}
  };
  GOptionContext *option_ctx;
  gboolean ret;
  GError *error = nullptr;
  DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;

  option_ctx = g_option_context_new ("d3d11videosink shard-texture example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  if (g_strcmp0 (texture_foramt, "RGBA") == 0) {
    gst_println ("Use DXGI_FORMAT_R8G8B8A8_UNORM (RGBA) format");
    format = DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (g_strcmp0 (texture_foramt, "RGB10A2_LE") == 0) {
    gst_println ("Use DXGI_FORMAT_R10G10B10A2_UNORM (RGB10A2_LE) format");
    format = DXGI_FORMAT_R10G10B10A2_UNORM;
  } else {
    gst_println ("Use DXGI_FORMAT_B8G8R8A8_UNORM format");
    format = DXGI_FORMAT_B8G8R8A8_UNORM;
  }

  /* NT handle needs to be used with keyed mutex */
  if (use_nt_handle) {
    misc_flags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
  } else if (use_keyed_mutex) {
    misc_flags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  } else {
    misc_flags = D3D11_RESOURCE_MISC_SHARED;
  }

  gst_println ("Use keyed-mutex: %d, use_nt_handle: %d", use_keyed_mutex,
      use_nt_handle);

  /* 1) Prepare window */
  wc.cbSize = sizeof (WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = (WNDPROC) window_proc;
  wc.hInstance = hinstance;
  wc.hCursor = LoadCursor (NULL, IDC_ARROW);
  wc.lpszClassName = TEXT ("GstD3D11VideoSinkSharedTextureExExample");
  RegisterClassEx (&wc);

  AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);

  hwnd = CreateWindowEx (0, wc.lpszClassName,
      TEXT ("GstD3D11VideoSinkSharedTextureExExample"),
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, (HWND) NULL, (HMENU) NULL,
      hinstance, NULL);

  /* 2) Prepare D3D11 device */
  hr = prepare_d3d11_device (&device, &context, &factory);
  if (FAILED (hr)) {
    gst_printerrln ("D3D11 device is unavailable");
    return -1;
  }

  hr = prepare_shader (device.Get(), context.Get(), &sampler,
      &ps, &vs, &layout, &vertex, &index);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't setup shader");
    return -1;
  }

  /* 3) Prepare SwapChain */
  DXGI_SWAP_CHAIN_DESC1 desc = { 0, };
  desc.Width = 0;
  desc.Height = 0;
  desc.Format = format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_NONE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

  hr = factory->CreateSwapChainForHwnd (device.Get(), hwnd, &desc,
      nullptr, nullptr, &swapchain);
  if (FAILED (hr)) {
    gst_printerrln ("IDXGISwapChain1 is unavailable");
    return -1;
  }

  /* 4) Create shared texture */
  /* Texture size doesn't need to be identical to that of backbuffer */
  hr = prepare_shared_texture (device.Get(), 1280, 720,
      format, misc_flags, &shared_texture, &srv, &keyed_mutex, &shared_handle);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create texture to share with d3d11videosink");
    return -1;
  }

  context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->IASetInputLayout (layout.Get());
  ID3D11Buffer *buf = vertex.Get();
  UINT offsets = 0;
  UINT stride = sizeof(VertexData);
  context->IASetVertexBuffers (0, 1, &buf, &stride, &offsets);
  context->IASetIndexBuffer (index.Get(), DXGI_FORMAT_R16_UINT, 0);

  ID3D11SamplerState *sampler_state = sampler.Get();
  context->PSSetSamplers (0, 1, &sampler_state);
  context->VSSetShader (vs.Get(), nullptr, 0);
  context->PSSetShader (ps.Get(), nullptr, 0);

  /* Call initial resize to prepare resources */
  on_resize();

  loop = g_main_loop_new (NULL, FALSE);
  msg_io_channel = g_io_channel_win32_new_messages ((gsize) hwnd);
  g_io_add_watch (msg_io_channel, G_IO_IN, msg_cb, NULL);

  /* Enable drawing on our texture and add signal handler */
  sink = gst_element_factory_make ("d3d11videosink", NULL);
  g_object_set (sink, "draw-on-shared-texture", TRUE, nullptr);
  g_signal_connect (sink, "begin-draw", G_CALLBACK (on_begin_draw), sink);

  if (uri) {
    pipeline = gst_element_factory_make ("playbin", nullptr);
    g_object_set (pipeline, "uri", uri, "video-sink", sink, nullptr);
  } else {
    GstElement *src = gst_element_factory_make ("videotestsrc", nullptr);

    pipeline = gst_pipeline_new ("d3d11videosink-pipeline");
    gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
    gst_element_link (src, sink);
  }

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), (GstBusFunc) bus_msg,
      pipeline);

  sret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Pipeline doesn't want to pause\n");
  } else {
    g_main_loop_run (loop);
    gst_element_set_state (pipeline, GST_STATE_NULL);
  }

  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

  if (hwnd)
    DestroyWindow (hwnd);

  gst_object_unref (pipeline);
  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);
  g_main_loop_unref (loop);

  gst_deinit ();

  g_free (texture_foramt);
  g_free (uri);

  /* NT handle should be explicitly closed to avoid leak */
  if (use_nt_handle)
    CloseHandle (shared_handle);

  return 0;
}
