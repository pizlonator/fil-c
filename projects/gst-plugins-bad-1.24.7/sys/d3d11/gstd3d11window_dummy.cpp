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

#include "gstd3d11window_dummy.h"
#include "gstd3d11pluginutils.h"
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_window_debug);
#define GST_CAT_DEFAULT gst_d3d11_window_debug

struct _GstD3D11WindowDummy
{
  GstD3D11Window parent;

  ID3D11Texture2D *fallback_texture;
  ID3D11VideoProcessorOutputView *fallback_pov;
  ID3D11RenderTargetView *fallback_rtv;

  GstD3D11Fence *fence;
};

#define gst_d3d11_window_dummy_parent_class parent_class
G_DEFINE_TYPE (GstD3D11WindowDummy, gst_d3d11_window_dummy,
    GST_TYPE_D3D11_WINDOW);

static void gst_d3d11_window_dummy_on_resize (GstD3D11Window * window,
    guint width, guint height);
static GstFlowReturn gst_d3d11_window_dummy_prepare (GstD3D11Window * window,
    guint display_width, guint display_height, GstCaps * caps,
    GstStructure * config, DXGI_FORMAT display_format, GError ** error);
static void gst_d3d11_window_dummy_unprepare (GstD3D11Window * window);
static gboolean
gst_d3d11_window_dummy_open_shared_handle (GstD3D11Window * window,
    GstD3D11WindowSharedHandleData * data);
static gboolean
gst_d3d11_window_dummy_release_shared_handle (GstD3D11Window * window,
    GstD3D11WindowSharedHandleData * data);

static void
gst_d3d11_window_dummy_class_init (GstD3D11WindowDummyClass * klass)
{
  GstD3D11WindowClass *window_class = GST_D3D11_WINDOW_CLASS (klass);

  window_class->on_resize =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_dummy_on_resize);
  window_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d11_window_dummy_prepare);
  window_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_dummy_unprepare);
  window_class->open_shared_handle =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_dummy_open_shared_handle);
  window_class->release_shared_handle =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_dummy_release_shared_handle);
}

static void
gst_d3d11_window_dummy_init (GstD3D11WindowDummy * self)
{
}

static GstFlowReturn
gst_d3d11_window_dummy_prepare (GstD3D11Window * window,
    guint display_width, guint display_height, GstCaps * caps,
    GstStructure * config, DXGI_FORMAT display_format, GError ** error)
{
  gst_clear_object (&window->compositor);
  gst_clear_object (&window->converter);

  /* We are supporting only RGBA, BGRA or RGB10A2_LE formats but we don't know
   * which format texture will be used at this moment */

  gst_video_info_from_caps (&window->info, caps);
  window->render_rect.left = 0;
  window->render_rect.top = 0;
  window->render_rect.right = display_width;
  window->render_rect.bottom = display_height;

  window->input_rect.left = 0;
  window->input_rect.top = 0;
  window->input_rect.right = GST_VIDEO_INFO_WIDTH (&window->info);
  window->input_rect.bottom = GST_VIDEO_INFO_HEIGHT (&window->info);

  gst_video_info_set_format (&window->render_info,
      GST_VIDEO_FORMAT_BGRA, display_width, display_height);

  /* TODO: not sure which colorspace should be used, let's use BT709 since
   * it's default and most common one */
  window->render_info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
  window->render_info.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;
  window->render_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

  if (config) {
    gst_structure_set (config, GST_D3D11_CONVERTER_OPT_BACKEND,
        GST_TYPE_D3D11_CONVERTER_BACKEND, GST_D3D11_CONVERTER_BACKEND_SHADER,
        nullptr);
  } else {
    config = gst_structure_new ("converter-config",
        GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
        GST_D3D11_CONVERTER_BACKEND_SHADER, nullptr);
  }

  GstD3D11DeviceLockGuard lk (window->device);
  window->converter = gst_d3d11_converter_new (window->device, &window->info,
      &window->render_info, config);

  if (!window->converter) {
    GST_ERROR_OBJECT (window, "Cannot create converter");
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Cannot create converter");
    return GST_FLOW_ERROR;
  }

  window->compositor =
      gst_d3d11_overlay_compositor_new (window->device, &window->render_info);
  if (!window->compositor) {
    GST_ERROR_OBJECT (window, "Cannot create overlay compositor");
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Cannot create overlay compositor");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
gst_d3d11_window_dummy_unprepare (GstD3D11Window * window)
{
  GstD3D11WindowDummy *self = GST_D3D11_WINDOW_DUMMY (window);

  gst_clear_d3d11_fence (&self->fence);
}

static void
gst_d3d11_window_dummy_on_resize (GstD3D11Window * window,
    guint width, guint height)
{
  GstVideoRectangle src_rect, dst_rect, rst_rect;

  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.w = width;
  dst_rect.h = height;

  if (window->force_aspect_ratio) {
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = GST_VIDEO_INFO_WIDTH (&window->render_info);
    src_rect.h = GST_VIDEO_INFO_HEIGHT (&window->render_info);

    switch (window->method) {
      case GST_VIDEO_ORIENTATION_90R:
      case GST_VIDEO_ORIENTATION_90L:
      case GST_VIDEO_ORIENTATION_UL_LR:
      case GST_VIDEO_ORIENTATION_UR_LL:
        src_rect.w = GST_VIDEO_INFO_HEIGHT (&window->render_info);
        src_rect.h = GST_VIDEO_INFO_WIDTH (&window->render_info);
        break;
      default:
        src_rect.w = GST_VIDEO_INFO_WIDTH (&window->render_info);
        src_rect.h = GST_VIDEO_INFO_HEIGHT (&window->render_info);
        break;
    }

    gst_video_sink_center_rect (src_rect, dst_rect, &rst_rect, TRUE);
  } else {
    rst_rect = dst_rect;
  }

  window->render_rect.left = rst_rect.x;
  window->render_rect.top = rst_rect.y;
  window->render_rect.right = rst_rect.x + rst_rect.w;
  window->render_rect.bottom = rst_rect.y + rst_rect.h;

  window->first_present = TRUE;
}

static gboolean
gst_d3d11_window_dummy_open_shared_handle (GstD3D11Window * window,
    GstD3D11WindowSharedHandleData * data)
{
  GstD3D11Device *device = window->device;
  ID3D11Device *device_handle;
  HRESULT hr;
  ComPtr < ID3D11Texture2D > texture;
  ComPtr < IDXGIKeyedMutex > keyed_mutex;
  ID3D11RenderTargetView *rtv;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  D3D11_TEXTURE2D_DESC desc;

  device_handle = gst_d3d11_device_get_device_handle (device);

  if ((data->texture_misc_flags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) ==
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE) {
    ComPtr < ID3D11Device1 > device1_handle;

    hr = device_handle->QueryInterface (IID_PPV_ARGS (&device1_handle));
    if (!gst_d3d11_result (hr, device))
      return FALSE;

    hr = device1_handle->OpenSharedResource1 (data->shared_handle,
        IID_PPV_ARGS (&texture));
  } else {
    hr = device_handle->OpenSharedResource (data->shared_handle,
        IID_PPV_ARGS (&texture));
  }

  if (!gst_d3d11_result (hr, device))
    return FALSE;

  texture->GetDesc (&desc);
  data->use_keyed_mutex =
      (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) ==
      D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  mem = gst_d3d11_allocator_alloc_wrapped (nullptr,
      device, texture.Get (), desc.Width * desc.Height * 4, nullptr, nullptr);
  if (!mem) {
    GST_ERROR_OBJECT (window, "Couldn't allocate memory");
    return FALSE;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  rtv = gst_d3d11_memory_get_render_target_view (dmem, 0);
  if (!rtv) {
    GST_ERROR_OBJECT (window, "Render target view is unavailable");
    gst_memory_unref (mem);
    return FALSE;
  }

  /* Everything is prepared now */
  gst_d3d11_window_dummy_on_resize (window, desc.Width, desc.Height);

  /* Move owned resources */
  data->render_target = gst_buffer_new ();
  gst_buffer_append_memory (data->render_target, mem);

  return TRUE;
}

static gboolean
gst_d3d11_window_dummy_release_shared_handle (GstD3D11Window * window,
    GstD3D11WindowSharedHandleData * data)
{
  GstD3D11WindowDummy *self = GST_D3D11_WINDOW_DUMMY (window);
  GstD3D11Device *device = window->device;

  /* TODO: cache owned resource for the later reuse? */
  if (!data->use_keyed_mutex) {
    /* If keyed mutex is not used, let's handle sync manually by using
     * fence. Issued GPU commands might not be finished yet */

    if (!self->fence)
      self->fence = gst_d3d11_device_create_fence (device);

    if (!self->fence) {
      GST_ERROR_OBJECT (self, "Couldn't Create event query");
      return FALSE;
    }

    if (!gst_d3d11_fence_signal (self->fence) ||
        !gst_d3d11_fence_wait (self->fence)) {
      GST_ERROR_OBJECT (self, "Couldn't sync GPU operation");
      return FALSE;
    }
  }

  gst_clear_buffer (&data->render_target);

  return TRUE;
}

GstD3D11Window *
gst_d3d11_window_dummy_new (GstD3D11Device * device)
{
  GstD3D11Window *window;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  window = (GstD3D11Window *)
      g_object_new (GST_TYPE_D3D11_WINDOW_DUMMY, "d3d11device", device, NULL);

  window->initialized = TRUE;
  gst_object_ref_sink (window);

  return window;
}
