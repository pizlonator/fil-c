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
#include <wrl.h>
#include <math.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

typedef struct
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstD3D11Device *d3d11_device;
  GstBufferPool *pool;

  ID3D11Device *device;
  ID3D11DeviceContext *context;
  gsize mem_size;

  D3D11_TEXTURE2D_DESC desc;
  GstVideoInfo video_info;

  GQueue unused_textures;
  GstClockTime next_pts;
  GstClockTime duration;

  GRecMutex lock;
  gint remaining;
  guint64 num_frames;
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

typedef struct
{
  AppData *app_data;
  ID3D11Texture2D *texture;
} MemoryUserData;

static void
on_memory_freed (MemoryUserData * data)
{
  g_rec_mutex_lock (&data->app_data->lock);
  g_queue_push_tail (&data->app_data->unused_textures, data->texture);
  g_rec_mutex_unlock (&data->app_data->lock);

  g_free (data);
}

static gdouble
get_clear_value (guint64 num_frames, guint scale)
{
  gdouble val = (gdouble) num_frames / scale;

  val = sin (val);
  val = ABS (val);

  return val;
}

static void
on_need_data (GstAppSrc * appsrc, guint length, gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  ID3D11Texture2D *texture = nullptr;
  HRESULT hr;
  ComPtr < ID3D11RenderTargetView > rtv;
  FLOAT clear_color[4] = { 1.0, 1.0, 1.0, 1.0 };
  GstMemory *mem;
  GstD3D11Memory *dmem;
  MemoryUserData *memory_data;
  GstBuffer *buffer;
  gsize offset[GST_VIDEO_MAX_PLANES];
  gint stride[GST_VIDEO_MAX_PLANES];
  guint pitch;
  gsize dummy;

  if (app_data->remaining == 0) {
    gst_app_src_end_of_stream (appsrc);
    return;
  }

  clear_color[0] = get_clear_value (app_data->num_frames, 50);
  clear_color[1] = get_clear_value (app_data->num_frames, 100);
  clear_color[2] = get_clear_value (app_data->num_frames, 200);
  app_data->num_frames++;

  g_rec_mutex_lock (&app_data->lock);
  texture = (ID3D11Texture2D *) g_queue_pop_head (&app_data->unused_textures);
  g_rec_mutex_unlock (&app_data->lock);

  if (!texture) {
    hr = app_data->device->CreateTexture2D (&app_data->desc, nullptr, &texture);
    if (FAILED (hr)) {
      gst_printerrln ("Failed to create texture");
      exit (1);
    }
  }

  hr = app_data->device->CreateRenderTargetView (texture, nullptr, &rtv);
  if (FAILED (hr)) {
    gst_printerrln ("Failed to create RTV");
    exit (1);
  }

  /* ID3D11DeviceContext API is not thread safe, application should takes lock
   * when it's shared with GStreamer */
  gst_d3d11_device_lock (app_data->d3d11_device);
  /* Clear with white */
  app_data->context->ClearRenderTargetView (rtv.Get (), clear_color);
  gst_d3d11_device_unlock (app_data->d3d11_device);

  /* Demonstrating application-side texture pool.
   * GstD3D11BufferPool can be used instead */
  memory_data = g_new0 (MemoryUserData, 1);
  memory_data->app_data = app_data;
  /* Transfer ownership of this texture to this user data */
  memory_data->texture = texture;

  /* gst_d3d11_allocator_alloc_wrapped() method does not take ownership of
   * ID3D11Texture2D object, but in this example, we pass ownership via
   * user data */
  mem = gst_d3d11_allocator_alloc_wrapped (nullptr, app_data->d3d11_device,
      texture, app_data->mem_size, memory_data,
      (GDestroyNotify) on_memory_freed);

  if (!mem) {
    gst_printerrln ("Couldn't allocate memory");
    exit (1);
  }

  /* update memory size with calculated value by allocator, and reuse it
   * for later alloc_wrapped() call to avoid allocating staging texture */
  app_data->mem_size = mem->size;

  /* Calculates CPU accessible (via staging texture) memory layout.
   * GstD3D11Memory allows CPU access but application must calculate the layout
   * pitch would be likely different from width */
  dmem = GST_D3D11_MEMORY_CAST (mem);
  if (!gst_d3d11_memory_get_resource_stride (dmem, &pitch)) {
    gst_printerrln ("Couldn't get resource stride");
    exit (1);
  }

  if (!gst_d3d11_dxgi_format_get_size (app_data->desc.Format,
          app_data->desc.Width, app_data->desc.Height, pitch, offset, stride,
          &dummy)) {
    gst_printerrln ("Couldn't get memory layout");
    exit (1);
  }

  buffer = gst_buffer_new ();
  gst_buffer_append_memory (buffer, mem);

  /* Then attach video-meta to signal CPU accessible memory layout information */
  gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&app_data->video_info),
      GST_VIDEO_INFO_WIDTH (&app_data->video_info),
      GST_VIDEO_INFO_HEIGHT (&app_data->video_info),
      GST_VIDEO_INFO_N_PLANES (&app_data->video_info), offset, stride);

  GST_BUFFER_PTS (buffer) = app_data->next_pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = app_data->duration;

  app_data->next_pts += app_data->duration;

  if (gst_app_src_push_buffer (appsrc, buffer) != GST_FLOW_OK) {
    gst_printerrln ("Couldn't push buffer to appsrc");
    exit (1);
  }

  if (app_data->remaining > 0)
    app_data->remaining--;
}

static void
on_need_data_buffer_pool (GstAppSrc * appsrc, guint length, gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  ID3D11Texture2D *texture = nullptr;
  HRESULT hr;
  ComPtr < ID3D11RenderTargetView > rtv;
  FLOAT clear_color[4] = { 1.0, 1.0, 1.0, 1.0 };
  GstBuffer *buffer;
  GstMemory *mem;
  GstFlowReturn ret;
  GstMapInfo info;
  GstMapFlags map_flags;

  if (app_data->remaining == 0) {
    gst_app_src_end_of_stream (appsrc);
    return;
  }

  clear_color[0] = get_clear_value (app_data->num_frames, 50);
  clear_color[1] = get_clear_value (app_data->num_frames, 100);
  clear_color[2] = get_clear_value (app_data->num_frames, 200);
  app_data->num_frames++;

  ret = gst_buffer_pool_acquire_buffer (app_data->pool, &buffer, nullptr);
  if (ret != GST_FLOW_OK) {
    gst_printerrln ("Failed to acquire buffer");
    exit (1);
  }

  /* buffer acquired from d3d11 buffer pool will hold video meta already.
   * Application can just update already allocated texture */
  mem = gst_buffer_peek_memory (buffer, 0);

  /* Use GST_MAP_D3D11 flag to indicate that direct Direct3D11 resource
   * is required instead of system memory access */
  map_flags = (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11);
  if (!gst_memory_map (mem, &info, map_flags)) {
    gst_printerrln ("Failed to map memory");
    exit (1);
  }

  texture = (ID3D11Texture2D *) info.data;
  hr = app_data->device->CreateRenderTargetView (texture, nullptr, &rtv);
  if (FAILED (hr)) {
    gst_printerrln ("Failed to create RTV");
    exit (1);
  }

  /* ID3D11DeviceContext API is not thread safe, application should takes lock
   * when it's shared with GStreamer */
  gst_d3d11_device_lock (app_data->d3d11_device);
  /* Clear with white */
  app_data->context->ClearRenderTargetView (rtv.Get (), clear_color);
  gst_d3d11_device_unlock (app_data->d3d11_device);

  gst_memory_unmap (mem, &info);

  GST_BUFFER_PTS (buffer) = app_data->next_pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = app_data->duration;

  app_data->next_pts += app_data->duration;

  if (gst_app_src_push_buffer (appsrc, buffer) != GST_FLOW_OK) {
    gst_printerrln ("Couldn't push buffer to appsrc");
    exit (1);
  }

  if (app_data->remaining > 0)
    app_data->remaining--;
}

static bool
create_pipelne (AppData * app_data, gboolean use_pool)
{
  GstElement *pipeline;
  GstAppSrc *src;
  GError *error = nullptr;
  GstCaps *caps;
  GstBus *bus;
  GstAppSrcCallbacks callbacks = { nullptr };
  D3D11_TEXTURE2D_DESC desc = { 0, };

  /* 640x480 RGBA format will be used in this example */
  desc.Width = 640;
  desc.Height = 480;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  /* Bind shader resource for this texture can be used in shader and also
   * RTV is used in this example */
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  app_data->desc = desc;

  app_data->next_pts = 0;
  app_data->duration = GST_SECOND / 30;

  pipeline = gst_parse_launch ("appsrc name=src ! queue ! d3d11videosink",
      &error);
  if (error) {
    gst_printerrln ("Couldn't create pipeline: %s", error->message);
    g_clear_error (&error);
    return false;
  }

  src = GST_APP_SRC (gst_bin_get_by_name (GST_BIN (pipeline), "src"));

  if (use_pool)
    callbacks.need_data = on_need_data_buffer_pool;
  else
    callbacks.need_data = on_need_data;
  gst_app_src_set_callbacks (src, &callbacks, app_data, nullptr);

  caps =
      gst_caps_from_string
      ("video/x-raw(memory:D3D11Memory),format=RGBA,width=640,height=480,framerate=30/1");
  gst_app_src_set_caps (src, caps);
  gst_video_info_from_caps (&app_data->video_info, caps);

  gst_app_src_set_stream_type (src, GST_APP_STREAM_TYPE_STREAM);
  g_object_set (src, "format", GST_FORMAT_TIME, nullptr);
  g_object_unref (src);

  app_data->pipeline = pipeline;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  /* Listen need-context message from sync handler in case that application
   * wants to share application's d3d11 device with pipeline */
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, app_data,
      nullptr);
  gst_bus_add_watch (bus, (GstBusFunc) bus_handler, app_data);
  gst_object_unref (bus);

  if (use_pool) {
    GstBufferPool *pool;
    GstStructure *config;
    GstD3D11AllocationParams *params;

    pool = gst_d3d11_buffer_pool_new (app_data->d3d11_device);
    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_set_params (config, caps,
        GST_VIDEO_INFO_SIZE (&app_data->video_info), 0, 0);

    /* default allocation param doesn't use any binding flag.
     * If binding flag is required, application should create
     * allocation param struct and specify options */
    params = gst_d3d11_allocation_params_new (app_data->d3d11_device,
        &app_data->video_info, GST_D3D11_ALLOCATION_FLAG_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0);

    gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
    gst_d3d11_allocation_params_free (params);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_printerrln ("Couldn't set config to pool");
      gst_object_unref (pool);
      gst_caps_unref (caps);
      return false;
    }

    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      gst_printerrln ("Couldn't set active");
      gst_object_unref (pool);
      gst_caps_unref (caps);
      return false;
    }

    app_data->pool = pool;
  }

  gst_caps_unref (caps);

  return true;
}

static void
clear_texture (ID3D11Texture2D * texture)
{
  texture->Release ();
}

gint
main (gint argc, gchar ** argv)
{
  GOptionContext *option_ctx;
  GError *error = nullptr;
  gboolean ret;
  gboolean use_pool = FALSE;
  gint num_buffers = -1;
  GOptionEntry options[] = {
    {"use-bufferpool", 0, 0, G_OPTION_ARG_NONE, &use_pool,
        "Use buffer pool", nullptr},
    {"num-buffers", 0, 0, G_OPTION_ARG_INT, &num_buffers,
        "The number of buffers to run", nullptr},
    {nullptr,}
  };
  AppData app_data = { nullptr, };

  option_ctx = g_option_context_new ("Direct3D11 appsrc example");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", error->message);
    g_clear_error (&error);
    exit (1);
  }

  app_data.remaining = num_buffers;
  g_rec_mutex_init (&app_data.lock);
  g_queue_init (&app_data.unused_textures);

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

  if (!create_pipelne (&app_data, use_pool))
    exit (1);

  /* All done! */
  gst_element_set_state (app_data.pipeline, GST_STATE_PLAYING);
  g_main_loop_run (app_data.loop);

  gst_element_set_state (app_data.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (app_data.pipeline));

#define CLEAR_COM(obj) G_STMT_START { \
    if (obj) { \
      (obj)->Release (); \
    } \
  } G_STMT_END

  g_queue_clear_full (&app_data.unused_textures,
      (GDestroyNotify) clear_texture);
  g_rec_mutex_clear (&app_data.lock);

  CLEAR_COM (app_data.context);
  CLEAR_COM (app_data.device);

  gst_clear_object (&app_data.d3d11_device);
  gst_clear_object (&app_data.pipeline);
  g_main_loop_unref (app_data.loop);

  return 0;
}
