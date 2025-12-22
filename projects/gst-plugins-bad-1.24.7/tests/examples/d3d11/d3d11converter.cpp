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
#include <gst/app/app.h>
#include <windows.h>
#include <wrl.h>
#include <string>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include "../key-handler.h"

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

#define APP_DATA_PROP_NAME "AppData"

typedef struct
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstD3D11Device *d3d11_device;

  ID3D11Device *device;
  ID3D11DeviceContext *context;
  IDXGISwapChain1 *swapchain;

  GstD3D11Converter *converter;
  /* Wrapping swap chain backbuffer */
  GstBuffer *backbuffer;

  GstVideoInfo source_info;

  guint direction;

  guint window_width;
  guint window_height;

  HWND hwnd;
} AppData;

static void
keyboard_cb (gchar input, gboolean is_ascii, AppData * data)
{
  if (!is_ascii)
    return;

  switch (input) {
    case 'q':
    case 'Q':
      gst_element_send_event (data->pipeline, gst_event_new_eos ());
      break;
    case ' ':
      gst_d3d11_device_lock (data->d3d11_device);
      data->direction++;
      data->direction %= (guint) GST_VIDEO_ORIENTATION_AUTO;

      gst_println ("Set orientation %d", data->direction);
      gst_d3d11_device_unlock (data->d3d11_device);
      break;
    default:
      break;
  }
}

static bool
create_device (AppData * data)
{
  ComPtr < IDXGIFactory1 > factory;
  ComPtr < ID3D11Device > device;
  ComPtr < ID3D11DeviceContext > context;
  ComPtr < IDXGIAdapter1 > adapter;
  HRESULT hr;
  DXGI_ADAPTER_DESC1 desc;
  static const D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
  };

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return false;

  /* Find hardware adapter */
  for (guint i = 0; SUCCEEDED (hr); i++) {

    hr = factory->EnumAdapters1 (i, &adapter);
    if (FAILED (hr))
      return false;

    hr = adapter->GetDesc1 (&desc);
    if (FAILED (hr))
      return false;

    break;
  }

  if (!adapter)
    return false;

  hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN,
      nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      feature_levels,
      G_N_ELEMENTS (feature_levels), D3D11_SDK_VERSION, &device, nullptr,
      &context);

  if (FAILED (hr))
    return false;

  data->device = device.Detach ();
  data->context = context.Detach ();

  return true;
}

static gboolean
bus_handler (GstBus * bus, GstMessage * msg, AppData * app_data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s", err->message);
      if (dbg != nullptr)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (app_data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_println ("Got EOS");
      g_main_loop_quit (app_data->loop);
      break;
    default:
      break;
  }

  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * msg, AppData * data)
{
  const gchar *context_type;
  GstContext *context;
  gchar *context_str;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_HAVE_CONTEXT:{
      gchar *context_str;

      gst_message_parse_have_context (msg, &context);

      context_type = gst_context_get_context_type (context);
      context_str =
          gst_structure_to_string (gst_context_get_structure (context));
      gst_println ("Got context from element '%s': %s=%s",
          GST_ELEMENT_NAME (GST_MESSAGE_SRC (msg)), context_type, context_str);
      g_free (context_str);
      gst_context_unref (context);
      break;
    }
    case GST_MESSAGE_NEED_CONTEXT:{
      gst_message_parse_context_type (msg, &context_type);
      if (g_strcmp0 (context_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
        return GST_BUS_PASS;

      context = gst_d3d11_context_new (data->d3d11_device);
      context_str =
          gst_structure_to_string (gst_context_get_structure (context));
      gst_println ("Setting context '%s': %s=%s",
          GST_ELEMENT_NAME (GST_MESSAGE_SRC (msg)), context_type, context_str);
      g_free (context_str);
      gst_element_set_context (GST_ELEMENT (msg->src), context);
      gst_context_unref (context);
      break;
    }
    default:
      break;
  }

  return GST_BUS_PASS;
}

static GstFlowReturn
on_new_sample (GstAppSink * appsink, gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  GstSample *sample = gst_app_sink_pull_sample (appsink);
  GstBuffer *buffer;
  GstVideoRectangle s, d, r;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstVideoOrientationMethod direction;

  if (!sample)
    return GST_FLOW_ERROR;

  buffer = gst_sample_get_buffer (sample);
  if (!buffer) {
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }

  /* DXGI, ID3D11DeviceContext, and ID3D11VideoContext APIs are not thread-safe.
   * Application should take d3d11 device lock */
  gst_d3d11_device_lock (app_data->d3d11_device);

  /* calculates destination render rectangle to keep aspect ratio */
  if (app_data->window_width == 0 || app_data->window_height == 0) {
    /* No client area to draw */
    goto out;
  }


  direction = (GstVideoOrientationMethod) app_data->direction;
  switch (direction) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      s.h = GST_VIDEO_INFO_WIDTH (&app_data->source_info);
      s.w = GST_VIDEO_INFO_HEIGHT (&app_data->source_info);
      break;
    default:
      s.w = GST_VIDEO_INFO_WIDTH (&app_data->source_info);
      s.h = GST_VIDEO_INFO_HEIGHT (&app_data->source_info);
      break;
  }

  s.x = 0;
  s.y = 0;

  d.x = 0;
  d.y = 0;
  d.w = app_data->window_width;
  d.h = app_data->window_height;

  gst_video_center_rect (&s, &d, &r, TRUE);

  /* Update converter output size and direction */
  g_object_set (app_data->converter, "dest-x", r.x, "dest-y", r.y,
      "dest-width", r.w, "dest-height", r.h, "video-direction", direction,
      nullptr);

  if (!gst_d3d11_converter_convert_buffer_unlocked (app_data->converter,
          buffer, app_data->backbuffer)) {
    gst_printerrln ("Couldn't convert");
    goto out;
  }

  app_data->swapchain->Present (0, 0);
  ret = GST_FLOW_OK;

out:
  gst_d3d11_device_unlock (app_data->d3d11_device);
  gst_sample_unref (sample);

  return ret;
}

static bool
create_pipelne (AppData * app_data)
{
  GstElement *pipeline;
  GstElement *sink;
  GError *error = nullptr;
  GstCaps *caps;
  GstBus *bus;
  GstAppSinkCallbacks callbacks = { nullptr };
  GstVideoInfo in_info, out_info;
  GstStructure *config;

  /* testsrc will output NV12 texture and this example will convert
   * each texture into RGBA swapchain backbuffer.
   * Note that GstD3D11Converter supports dynamic input/output resolution
   * and we will update output resolution later per swapchain update */
  gst_video_info_set_format (&in_info, GST_VIDEO_FORMAT_NV12, 640, 480);
  gst_video_info_set_format (&out_info, GST_VIDEO_FORMAT_RGBA, 640, 480);

  /* Video processor is not required in this example, specifies only shader
   * to prevent additional resource allocation for video processor */
  config = gst_structure_new ("converter-config",
      GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
      GST_D3D11_CONVERTER_BACKEND_SHADER, nullptr);

  app_data->converter = gst_d3d11_converter_new (app_data->d3d11_device,
      &in_info, &out_info, config);
  if (!app_data->converter) {
    gst_printerrln ("Couldn't create converter");
    return false;
  }

  /* Enable border filling with black color (ARGB64 representation)
   * in order to clear background when video direction is updated.
   * Altanative approach is resizing swapchain per video direction update */
  g_object_set (app_data->converter, "fill-border", TRUE,
      "border-color", (guint64) 0xffff000000000000, nullptr);

  app_data->source_info = in_info;

  pipeline = gst_parse_launch ("d3d11testsrc ! appsink name=sink", &error);
  if (error) {
    gst_printerrln ("Couldn't create pipeline: %s", error->message);
    g_clear_error (&error);
    return false;
  }

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

  callbacks.new_sample = on_new_sample;
  gst_app_sink_set_callbacks (GST_APP_SINK (sink),
      &callbacks, app_data, nullptr);

  in_info.fps_n = 30;
  in_info.fps_d = 1;

  caps = gst_video_info_to_caps (&in_info);
  /* Set d3d11 caps feature so that d3d11testsrc can output GPU memory
   * instead of system memory */
  gst_caps_set_features (caps, 0,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr));

  g_object_set (sink, "caps", caps, nullptr);
  gst_caps_unref (caps);
  gst_object_unref (sink);

  app_data->pipeline = pipeline;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  /* Listen need-context message from sync handler in case that application
   * wants to share application's d3d11 device with pipeline */
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, app_data,
      nullptr);
  gst_bus_add_watch (bus, (GstBusFunc) bus_handler, app_data);
  gst_object_unref (bus);

  return true;
}

static void
handle_window_resize (AppData * app_data)
{
  HRESULT hr;
  ComPtr < ID3D11Texture2D > backbuffer;
  D3D11_TEXTURE2D_DESC desc;
  GstMemory *mem;

  if (!app_data->device || !app_data->swapchain)
    return;

  /* DXGI and ID3D11DeviceContext APIs are not thread-safe.
   * Application must take gst_d3d11_device_lock() in those case */
  gst_d3d11_device_lock (app_data->d3d11_device);

  /* Clear previous swapchain backbuffer */
  gst_clear_buffer (&app_data->backbuffer);

  hr = app_data->swapchain->ResizeBuffers (0,
      0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
  if (FAILED (hr)) {
    gst_printerrln ("Failed to resize swapchain buffers");
    gst_d3d11_device_unlock (app_data->d3d11_device);
    exit (1);
  }

  hr = app_data->swapchain->GetBuffer (0, IID_PPV_ARGS (&backbuffer));
  if (FAILED (hr)) {
    gst_printerrln ("Failed to get swapchain backbuffer");
    gst_d3d11_device_unlock (app_data->d3d11_device);
    exit (1);
  }

  backbuffer->GetDesc (&desc);

  mem = gst_d3d11_allocator_alloc_wrapped (nullptr,
      app_data->d3d11_device, backbuffer.Get (),
      /* This might not be correct CPU accessible (staging) texture size
       * but it's fine since we don't use this memory for CPU access */
      desc.Width * desc.Height * 4, nullptr, nullptr);
  if (!mem) {
    gst_printerrln ("Failed to wrap backbuffer");
    gst_d3d11_device_unlock (app_data->d3d11_device);
    exit (1);
  }

  app_data->backbuffer = gst_buffer_new ();
  gst_buffer_append_memory (app_data->backbuffer, mem);

  app_data->window_width = desc.Width;
  app_data->window_height = desc.Height;
  gst_d3d11_device_unlock (app_data->d3d11_device);
}

static LRESULT CALLBACK
window_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_DESTROY:{
      AppData *app_data = (AppData *) GetPropA (hwnd, APP_DATA_PROP_NAME);
      if (app_data) {
        app_data->hwnd = nullptr;
        if (app_data->loop)
          g_main_loop_quit (app_data->loop);
      }
      break;
    }
    case WM_SIZE:{
      AppData *app_data = (AppData *) GetPropA (hwnd, APP_DATA_PROP_NAME);
      if (!app_data)
        break;

      handle_window_resize (app_data);
      break;
    }
    default:
      break;
  }

  return DefWindowProc (hwnd, message, wParam, lParam);
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

static HWND
create_window (void)
{
  RECT wr = { 0, 0, 320, 240 };
  WNDCLASSEXA wc = { 0, };
  HINSTANCE hinstance = GetModuleHandle (nullptr);

  wc.cbSize = sizeof (WNDCLASSEXA);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = (WNDPROC) window_proc;
  wc.hInstance = hinstance;
  wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
  wc.lpszClassName = "GstD3D11VideoSinkExample";
  RegisterClassExA (&wc);

  AdjustWindowRect (&wr, WS_OVERLAPPEDWINDOW, FALSE);
  return CreateWindowExA (0, wc.lpszClassName, "GstD3D11VideoDecodeExample",
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, (HWND) nullptr, (HMENU) nullptr,
      hinstance, nullptr);
}

static bool
create_swapchain (AppData * data)
{
  DXGI_SWAP_CHAIN_DESC1 desc = { 0, };
  IDXGIFactory1 *factory;
  ComPtr < IDXGIFactory2 > factory2;
  ComPtr < IDXGISwapChain1 > swapchain;
  HRESULT hr;

  factory = gst_d3d11_device_get_dxgi_factory_handle (data->d3d11_device);
  hr = factory->QueryInterface (IID_PPV_ARGS (&factory2));
  if (FAILED (hr))
    return false;

  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_NONE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

  hr = factory2->CreateSwapChainForHwnd (data->device, data->hwnd, &desc,
      nullptr, nullptr, &swapchain);
  if (FAILED (hr))
    return false;

  data->swapchain = swapchain.Detach ();

  return true;
}

gint
main (gint argc, gchar ** argv)
{
  GIOChannel *msg_io_channel = nullptr;
  AppData app_data = { nullptr, };

  gst_init (nullptr, nullptr);

  app_data.loop = g_main_loop_new (nullptr, FALSE);

  /* Create D3D11 device */
  if (!create_device (&app_data)) {
    gst_printerrln ("No available hardware device");
    exit (1);
  }

  /* Creates GstD3D11Device using our device handle.
   * Note that gst_d3d11_device_new_wrapped() method does not take ownership of
   * ID3D11Device handle, instead refcount of ID3D11Device handle will be
   * increased by one */
  app_data.d3d11_device = gst_d3d11_device_new_wrapped (app_data.device);
  if (!app_data.d3d11_device) {
    gst_printerrln ("Couldn't create GstD3D11Device object");
    exit (1);
  }

  /* Creates window and swapchain */
  app_data.hwnd = create_window ();
  if (!app_data.hwnd) {
    gst_printerrln ("Couldn't create window handle");
    exit (1);
  }
  SetPropA (app_data.hwnd, APP_DATA_PROP_NAME, &app_data);

  msg_io_channel = g_io_channel_win32_new_messages (0);
  g_io_add_watch (msg_io_channel, G_IO_IN, msg_cb, NULL);

  if (!create_swapchain (&app_data)) {
    gst_printerrln ("Couldn't create swapchain");
    exit (1);
  }

  /* Calls this manually to hold swap chain backbuffer */
  handle_window_resize (&app_data);

  if (!create_pipelne (&app_data))
    exit (1);

  /* All done! */
  set_key_handler ((KeyInputCallback) keyboard_cb, &app_data);

  gst_element_set_state (app_data.pipeline, GST_STATE_PLAYING);
  ShowWindow (app_data.hwnd, SW_SHOW);
  g_main_loop_run (app_data.loop);

  unset_key_handler ();

  gst_element_set_state (app_data.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (app_data.pipeline));

#define CLEAR_COM(obj) G_STMT_START { \
    if (obj) { \
      (obj)->Release (); \
    } \
  } G_STMT_END

  gst_clear_buffer (&app_data.backbuffer);
  CLEAR_COM (app_data.swapchain);
  CLEAR_COM (app_data.context);
  CLEAR_COM (app_data.device);

  if (app_data.hwnd)
    DestroyWindow (app_data.hwnd);

  gst_clear_object (&app_data.d3d11_device);
  gst_clear_object (&app_data.pipeline);

  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);
  g_main_loop_unref (app_data.loop);

  return 0;
}
