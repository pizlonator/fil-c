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

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

#define APP_DATA_PROP_NAME "AppData"

typedef struct
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstD3D11Device *d3d11_device;

  LUID luid;

  IDXGIFactory1 *factory;
  ID3D11Device *device;
  ID3D11DeviceContext *context;

  ID3D11VideoDevice *video_device;
  ID3D11VideoContext1 *video_context;
  ID3D11VideoProcessorEnumerator *proc_enum;
  ID3D11VideoProcessor *processor;

  IDXGISwapChain1 *swapchain;
  ID3D11VideoProcessorOutputView *pov;

  guint window_width;
  guint window_height;

  HWND hwnd;
} AppData;

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

    /* DXGI_ADAPTER_FLAG_SOFTWARE, old mingw does not define this enum */
    if ((desc.Flags & 0x2) == 0)
      break;

    adapter = nullptr;
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

  data->factory = factory.Detach ();
  data->device = device.Detach ();
  data->context = context.Detach ();
  data->luid = desc.AdapterLuid;

  return true;
}

static bool
find_decoder (gint64 luid, std::string & feature_name)
{
  GList *features;
  GList *iter;

  /* Load features of d3d11 plugin */
  features = gst_registry_get_feature_list_by_plugin (gst_registry_get (),
      "d3d11");

  if (!features)
    return false;

  for (iter = features; iter; iter = g_list_next (iter)) {
    GstPluginFeature *f = GST_PLUGIN_FEATURE (iter->data);
    GstElementFactory *factory;
    const gchar *name;
    GstElement *element;
    gint64 adapter_luid;

    if (!GST_IS_ELEMENT_FACTORY (f))
      continue;

    factory = GST_ELEMENT_FACTORY (f);
    if (!gst_element_factory_list_is_type (factory,
            GST_ELEMENT_FACTORY_TYPE_DECODER))
      continue;

    name = gst_plugin_feature_get_name (f);
    if (!g_strrstr (name, "h264"))
      continue;

    element = gst_element_factory_create (factory, nullptr);
    /* unexpected */
    if (!element)
      continue;

    /* query adapter-luid associated with this decoder */
    g_object_get (element, "adapter-luid", &adapter_luid, nullptr);
    gst_object_unref (element);

    /* element object can be directly used in pipeline, but this example
     * demonstrates a way of plugin enumeration */
    if (adapter_luid == luid) {
      feature_name = name;
      break;
    }
  }

  gst_plugin_feature_list_free (features);

  if (feature_name.empty ())
    return false;

  return true;
}

static bool
create_video_processor (AppData * data)
{
  ComPtr < ID3D11VideoDevice > video_device;
  ComPtr < ID3D11VideoContext1 > video_context;
  ComPtr < ID3D11VideoProcessorEnumerator > proc_enum;
  ComPtr < ID3D11VideoProcessor > processor;
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  HRESULT hr;

  hr = data->device->QueryInterface (IID_PPV_ARGS (&video_device));
  if (FAILED (hr))
    return false;

  hr = data->context->QueryInterface (IID_PPV_ARGS (&video_context));
  if (FAILED (hr))
    return false;

  memset (&desc, 0, sizeof (desc));

  /* resolution here is not that important */
  desc.InputWidth = desc.OutputWidth = 640;
  desc.InputHeight = desc.OutputHeight = 480;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = video_device->CreateVideoProcessorEnumerator (&desc, &proc_enum);
  if (FAILED (hr))
    return false;

  hr = video_device->CreateVideoProcessor (proc_enum.Get (), 0, &processor);
  if (FAILED (hr))
    return false;

  video_context->VideoProcessorSetStreamColorSpace1 (processor.Get (),
      0, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
  video_context->VideoProcessorSetOutputColorSpace1 (processor.Get (),
      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);

  data->video_device = video_device.Detach ();
  data->video_context = video_context.Detach ();
  data->proc_enum = proc_enum.Detach ();
  data->processor = processor.Detach ();

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
  ID3D11VideoContext1 *video_context = app_data->video_context;
  ID3D11VideoProcessor *processor = app_data->processor;
  ID3D11VideoProcessorOutputView *pov = app_data->pov;
  GstSample *sample = gst_app_sink_pull_sample (appsink);
  GstCaps *caps;
  GstBuffer *buffer;
  GstVideoInfo video_info;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ComPtr < ID3D11VideoProcessorInputView > piv;
  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC desc;
  guint subresource_index;
  HRESULT hr;
  GstMapInfo info;
  GstMapFlags map_flags;
  ID3D11Texture2D *texture;
  RECT src_rect, dest_rect;
  D3D11_VIDEO_PROCESSOR_STREAM stream = { 0, };
  GstVideoRectangle s, d, r;

  if (!sample)
    return GST_FLOW_ERROR;

  buffer = gst_sample_get_buffer (sample);
  if (!buffer) {
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }

  caps = gst_sample_get_caps (sample);
  if (!caps) {
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }

  if (!gst_video_info_from_caps (&video_info, caps)) {
    gst_printerrln ("Invalid caps");
    return GST_FLOW_ERROR;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!mem) {
    gst_printerrln ("Empty buffer");
    gst_sample_unref (sample);
    return GST_FLOW_ERROR;
  }

  /* memory must be d3d11 memory */
  if (!gst_is_d3d11_memory (mem)) {
    gst_printerrln ("Not a d3d11 memory");
    gst_sample_unref (sample);
    return GST_FLOW_ERROR;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  /* decoder output texture may be texture array. Application should check
   * subresource index */
  subresource_index = gst_d3d11_memory_get_subresource_index (dmem);

  /* Use GST_MAP_D3D11 flag to indicate that direct Direct3D11 resource
   * is required instead of system memory access.
   *
   * CAUTION: Application must not try to write/modify texture rendered by
   * video decoder since it's likely a reference frame. If it's modified by
   * application, then the other decoded frames would be broken.
   * Only read access is allowed in this case */
  map_flags = (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11);

  if (!gst_memory_map (mem, &info, map_flags)) {
    gst_printerrln ("Couldn't map d3d11 memory");
    gst_sample_unref (sample);
    return GST_FLOW_ERROR;
  }

  texture = (ID3D11Texture2D *) info.data;

  desc.FourCC = 0;
  desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  desc.Texture2D.MipSlice = 0;
  desc.Texture2D.ArraySlice = subresource_index;

  hr = app_data->video_device->CreateVideoProcessorInputView (texture,
      app_data->proc_enum, &desc, &piv);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create PIV");
    gst_memory_unmap (mem, &info);
    gst_sample_unref (sample);
    return GST_FLOW_ERROR;
  }

  /* DXGI, ID3D11DeviceContext, and ID3D11VideoContext APIs are not thread-safe.
   * Application should take d3d11 device lock */
  gst_d3d11_device_lock (app_data->d3d11_device);

  /* calculates destination render rectangle to keep aspect ratio */
  if (app_data->window_width == 0 || app_data->window_height == 0) {
    /* No client area to draw */
    goto out;
  }

  s.x = 0;
  s.y = 0;
  s.w = GST_VIDEO_INFO_WIDTH (&video_info);
  s.h = GST_VIDEO_INFO_HEIGHT (&video_info);

  d.x = 0;
  d.y = 0;
  d.w = app_data->window_width;
  d.h = app_data->window_height;

  gst_video_center_rect (&s, &d, &r, TRUE);

  src_rect.left = 0;
  src_rect.top = 0;
  src_rect.right = GST_VIDEO_INFO_WIDTH (&video_info);
  src_rect.bottom = GST_VIDEO_INFO_HEIGHT (&video_info);

  dest_rect.left = r.x;
  dest_rect.top = r.y;
  dest_rect.right = r.x + r.w;
  dest_rect.bottom = r.y + r.h;

  /* Converts YUV -> RGBA using processor */
  stream.Enable = TRUE;
  stream.pInputSurface = piv.Get ();

  video_context->VideoProcessorSetStreamSourceRect (processor, 0, TRUE,
      &src_rect);
  video_context->VideoProcessorSetStreamDestRect (processor,
      0, TRUE, &dest_rect);
  video_context->VideoProcessorSetOutputTargetRect (processor,
      TRUE, &dest_rect);
  video_context->VideoProcessorBlt (processor, pov, 0, 1, &stream);
  app_data->swapchain->Present (0, 0);

out:
  gst_d3d11_device_unlock (app_data->d3d11_device);
  gst_memory_unmap (mem, &info);
  gst_sample_unref (sample);

  return GST_FLOW_OK;
}

static bool
create_pipelne (const std::string & decoder_name,
    const gchar * location, AppData * app_data)
{
  GstElement *pipeline;
  GstElement *sink;
  std::string pipeline_str;
  GError *error = nullptr;
  GstCaps *caps;
  GstBus *bus;
  GstAppSinkCallbacks callbacks = { nullptr };

  pipeline_str = "filesrc location=" + std::string (location) +
      " ! parsebin ! " + decoder_name + " ! queue ! appsink name=sink";
  gst_println ("Creating pipeline %s", pipeline_str.c_str ());

  pipeline = gst_parse_launch (pipeline_str.c_str (), &error);
  if (error) {
    gst_printerrln ("Couldn't create pipeline: %s", error->message);
    g_clear_error (&error);
    return false;
  }

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

  callbacks.new_sample = on_new_sample;
  gst_app_sink_set_callbacks (GST_APP_SINK (sink),
      &callbacks, app_data, nullptr);

  /* Set d3d11 caps to appsink so that d3d11 decoder can decide output
   * memory type as d3d11, not system memory.
   * In case that downstream does not support d3d11 memory feature,
   * d3d11 decoder elements will output system memory
   */
  caps = gst_caps_from_string ("video/x-raw(memory:D3D11Memory)");
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
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC pov_desc;
  D3D11_TEXTURE2D_DESC desc;

  if (!app_data->device || !app_data->swapchain ||
      !app_data->video_device || !app_data->video_context
      || !app_data->proc_enum || !app_data->processor)
    return;

  /* DXGI and ID3D11DeviceContext APIs are not thread-safe.
   * Application must take gst_d3d11_device_lock() in those case */
  gst_d3d11_device_lock (app_data->d3d11_device);

  /* Clear previously configured POV if any */
  if (app_data->pov)
    app_data->pov->Release ();
  app_data->pov = nullptr;

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

  pov_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  pov_desc.Texture2D.MipSlice = 0;

  hr = app_data->video_device->
      CreateVideoProcessorOutputView (backbuffer.Get (), app_data->proc_enum,
      &pov_desc, &app_data->pov);
  if (FAILED (hr)) {
    gst_printerrln ("Failed to create POV");
    gst_d3d11_device_unlock (app_data->d3d11_device);
    exit (1);
  }

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
  ComPtr < IDXGIFactory2 > factory2;
  ComPtr < IDXGISwapChain1 > swapchain;
  HRESULT hr;

  hr = data->factory->QueryInterface (IID_PPV_ARGS (&factory2));
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
  GOptionContext *option_ctx;
  GError *error = nullptr;
  gboolean ret;
  gchar *location = nullptr;
  GOptionEntry options[] = {
    {"location", 0, 0, G_OPTION_ARG_STRING, &location,
        "H.264 encoded test file location", nullptr},
    {nullptr,}
  };
  gint64 luid;
  std::string decoder_name;
  AppData app_data = { nullptr, };

  option_ctx = g_option_context_new ("Direct3D11 decoding example");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", error->message);
    g_clear_error (&error);
    exit (1);
  }

  if (!location) {
    gst_printerrln ("File location is unspecified");
    exit (1);
  }

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

  /* Setup video processor for YUV -> RGBA conversion, since swapchain
   * used in this example supports only RGBA rendering */
  if (!create_video_processor (&app_data)) {
    gst_printerrln ("Couldn't setup video processor for colorspace conversion");
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

  /* Calls this manually for POV to be configured */
  handle_window_resize (&app_data);

  /* All the required preperation for rendering is done.
   * Setup GStreamer pipeline now */
  /* converts LUID to int64 and enumerates decoders */
  luid = gst_d3d11_luid_to_int64 (&app_data.luid);
  if (!find_decoder (luid, decoder_name)) {
    gst_printerrln ("Unable to find h264 decoder element to use");
    exit (1);
  }

  gst_println ("Target decoder name: %s", decoder_name.c_str ());
  if (!create_pipelne (decoder_name, location, &app_data))
    exit (1);

  /* All done! */
  gst_element_set_state (app_data.pipeline, GST_STATE_PLAYING);
  ShowWindow (app_data.hwnd, SW_SHOW);
  g_main_loop_run (app_data.loop);

  gst_element_set_state (app_data.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (app_data.pipeline));

#define CLEAR_COM(obj) G_STMT_START { \
    if (obj) { \
      (obj)->Release (); \
    } \
  } G_STMT_END

  CLEAR_COM (app_data.pov);
  CLEAR_COM (app_data.swapchain);
  CLEAR_COM (app_data.processor);
  CLEAR_COM (app_data.proc_enum);
  CLEAR_COM (app_data.video_context);
  CLEAR_COM (app_data.video_device);
  CLEAR_COM (app_data.context);
  CLEAR_COM (app_data.device);
  CLEAR_COM (app_data.factory);

  if (app_data.hwnd)
    DestroyWindow (app_data.hwnd);

  gst_clear_object (&app_data.d3d11_device);
  gst_clear_object (&app_data.pipeline);

  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);
  g_main_loop_unref (app_data.loop);

  g_free (location);

  return 0;
}
