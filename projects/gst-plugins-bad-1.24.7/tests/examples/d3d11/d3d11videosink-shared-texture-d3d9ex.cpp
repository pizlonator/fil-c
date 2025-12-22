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
#include <d3d9.h>

using namespace Microsoft::WRL;

static gchar *uri = nullptr;

static GMainLoop *loop = nullptr;
static gboolean visible = FALSE;
static HWND hwnd = nullptr;
std::mutex lock;

/* Device handles */
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGIFactory2> factory;

ComPtr<IDirect3D9Ex> d3d9_handle;
ComPtr<IDirect3DDevice9Ex> d3d9_device;

/* SwapChain resources */
ComPtr<IDirect3DSwapChain9> swapchain;

/* Shard texture resources */
ComPtr<ID3D11Texture2D> shared_texture;
ComPtr<IDirect3DTexture9> shared_d3d9_texture;
ComPtr<IDirect3DSurface9> d3d9_surface;
HANDLE shared_handle = nullptr;

static void
on_begin_draw (GstElement * videosink, gpointer user_data)
{
  std::lock_guard<std::mutex> lk (lock);
  GstElement *sink = GST_ELEMENT (user_data);
  gboolean ret = TRUE;
  HRESULT hr;
  ComPtr<IDirect3DSurface9> backbuffer;

  /* Windows was destroyed, nothing to draw */
  if (!hwnd)
    return;

  if (!shared_handle) {
    gst_printerrln ("Shared handle wasn't configured");
    exit (-1);
  }

  if (!swapchain) {
    gst_printerrln ("SwapChain wasn't configured");
    exit (-1);
  }

  g_signal_emit_by_name (sink,
      "draw", shared_handle, D3D11_RESOURCE_MISC_SHARED, 0, 0, &ret);

  if (!ret) {
    gst_printerrln ("Failed to draw on shared handle");
    if (loop)
      g_main_loop_quit (loop);

    return;
  }

  /* Get swapchain's backbuffer */
  hr = swapchain->GetBackBuffer (0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get backbuffer");
    exit (-1);
  }

  /* Copy shared texture to backbuffer */
  hr = d3d9_device->StretchRect (d3d9_surface.Get(), nullptr,
      backbuffer.Get(), nullptr, D3DTEXF_LINEAR);
  if (FAILED (hr)) {
    gst_printerrln ("StretchRect failed");
    exit (-1);
  }

  hr = d3d9_device->BeginScene ();
  if (FAILED (hr)) {
    gst_printerrln ("BeginScene failed");
    exit (-1);
  }

  hr = swapchain->Present (nullptr, nullptr, nullptr, nullptr, 0);
  if (FAILED (hr)) {
    gst_printerrln ("Present failed");
    exit (-1);
  }

  hr = d3d9_device->EndScene ();
  if (FAILED (hr)) {
    gst_printerrln ("BeginScene failed");
    exit (-1);
  }
}

static void
on_resize (void)
{
  std::lock_guard<std::mutex> lk (lock);
  RECT client_rect;
  guint width, height;
  HRESULT hr;

  GetClientRect (hwnd, &client_rect);

  width = MAX (1, (client_rect.right - client_rect.left));
  height = MAX (1, (client_rect.bottom - client_rect.top));

  D3DPRESENT_PARAMETERS params = { 0, };

  if (!swapchain) {
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = hwnd;
    /* GST_VIDEO_FORMAT_BGRA */
    params.BackBufferFormat = D3DFMT_A8R8G8B8;

    hr = d3d9_device->CreateAdditionalSwapChain (&params, &swapchain);
    if (FAILED (hr)) {
      gst_printerrln ("Couldn't create swapchain");
      exit (-1);
    }
  } else {
    /* Check whether we need to re-create swapchain */
    hr = swapchain->GetPresentParameters (&params);
    if (FAILED (hr)) {
      gst_printerrln ("Couldn't get swapchain parameters");
      exit (-1);
    }

    if (params.BackBufferWidth != width || params.BackBufferHeight != height) {
      /* Backbuffer will have client area size */
      params.BackBufferWidth = 0;
      params.BackBufferHeight = 0;

      swapchain = nullptr;
      hr = d3d9_device->CreateAdditionalSwapChain (&params, &swapchain);
      if (FAILED (hr)) {
        gst_printerrln ("Couldn't re-create swapchain");
        exit (-1);
      }
    }
  }
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
  HRESULT hr;
  GOptionEntry options[] = {
    {"uri", 0, 0, G_OPTION_ARG_STRING, &uri,
        "URI to test (if unspecified, videotestsrc will be used)",
        NULL},
    {NULL}
  };
  GOptionContext *option_ctx;
  gboolean ret;
  GError *error = nullptr;

  option_ctx = g_option_context_new ("d3d11videosink shard-texture with d3d9 interop example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  /* 1) Prepare window */
  wc.cbSize = sizeof (WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = (WNDPROC) window_proc;
  wc.hInstance = hinstance;
  wc.hCursor = LoadCursor (NULL, IDC_ARROW);
  wc.lpszClassName = TEXT ("GstD3D11VideoSinkSharedTextureD3D9ExExample");
  RegisterClassEx (&wc);

  AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);

  hwnd = CreateWindowEx (0, wc.lpszClassName,
      TEXT ("GstD3D11VideoSinkSharedTextureD3D9ExExample"),
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

  /* 3) Prepare D3D9EX device */
  Direct3DCreate9Ex (D3D_SDK_VERSION, &d3d9_handle);
  if (!d3d9_handle) {
    gst_printerrln ("D3D9 handle is unavailable");
    return -1;
  }

  D3DPRESENT_PARAMETERS params = { 0,};
  params.Windowed = TRUE;
  params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  params.hDeviceWindow = hwnd;
  params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

  hr = d3d9_handle->CreateDeviceEx (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
      &params, nullptr, &d3d9_device);
  if (FAILED (hr)) {
    gst_printerrln ("D3d9 deice is unavailable");
    return -1;
  }

  /* 4) Create shared texture */
  /* Texture size doesn't need to be identical to that of backbuffer */
  hr = prepare_shared_texture (device.Get(), 1280, 720,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      /* NOTE: D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is incompatible with
       * d3d9. User should use D3D11_RESOURCE_MISC_SHARED in case of d3d9 */
      D3D11_RESOURCE_MISC_SHARED,
      &shared_texture, nullptr, nullptr, &shared_handle);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create texture to share with d3d11videosink");
    return -1;
  }

  hr = d3d9_device->CreateTexture (1280, 720, 1, D3DUSAGE_RENDERTARGET,
      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &shared_d3d9_texture, &shared_handle);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create shared d3d9 texture");
    return -1;
  }

  hr = shared_d3d9_texture->GetSurfaceLevel (0, &d3d9_surface);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't get surface from shared d3d9 texture");
    return -1;
  }

  /* Call initial resize to prepare swapchain */
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
  g_free (uri);

  return 0;
}
