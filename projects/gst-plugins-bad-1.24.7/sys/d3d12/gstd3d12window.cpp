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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12window.h"
#include "gstd3d12overlaycompositor.h"
#include <directx/d3dx12.h>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <wrl.h>
#include <string>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
static std::mutex global_hwnd_lock;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_window_debug);
#define GST_CAT_DEFAULT gst_d3d12_window_debug

#define WS_GST_D3D12 (WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW)
#define EXTERNAL_PROC_PROP_NAME L"gst-d3d12-hwnd-external-proc"
#define D3D12_WINDOW_PROP_NAME L"gst-d3d12-hwnd-obj"
#define WM_GST_D3D12_FULLSCREEN (WM_USER + 1)
#define WM_GST_D3D12_CONSTRUCT_INTERNAL_WINDOW (WM_USER + 2)
#define WM_GST_D3D12_DESTROY_INTERNAL_WINDOW (WM_USER + 3)
#define WM_GST_D3D12_UPDATE_RENDER_RECT (WM_USER + 4)

#define BACK_BUFFER_COUNT 3

enum
{
  SIGNAL_KEY_EVENT,
  SIGNAL_MOUSE_EVENT,
  SIGNAL_FULLSCREEN,
  SIGNAL_LAST
};

static guint d3d12_window_signals[SIGNAL_LAST] = { 0, };

/* *INDENT-OFF* */
struct SwapBuffer
{
  SwapBuffer (GstBuffer * buf)
  {
    backbuf = buf;
  }

  ~SwapBuffer ()
  {
    gst_clear_buffer (&backbuf);
  }

  GstBuffer *backbuf = nullptr;
  gboolean first = TRUE;
};

struct DeviceContext
{
  DeviceContext () = delete;
  DeviceContext (GstD3D12Device * dev)
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    device = (GstD3D12Device *) gst_object_ref (dev);

    D3D12_COMMAND_QUEUE_DESC queue_desc = { };
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    queue = gst_d3d12_command_queue_new (device,
        &queue_desc, BACK_BUFFER_COUNT * 2);
    if (!queue) {
      GST_ERROR_OBJECT (device, "Couldn't create command queue");
      return;
    }

    ca_pool = gst_d3d12_command_allocator_pool_new (device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!ca_pool) {
      GST_ERROR_OBJECT (device, "Couldn't create command allocator pool");
      return;
    }

    initialized = true;
  }

  ~DeviceContext ()
  {
    WaitGpu ();

    CloseHandle (event_handle);

    gst_clear_object (&queue);
    gst_clear_object (&ca_pool);
    gst_clear_buffer (&cached_buf);
    gst_clear_object (&conv);
    gst_clear_object (&comp);
    gst_clear_buffer (&msaa_buf);
    gst_clear_object (&device);
  }

  void WaitGpu ()
  {
    if (!queue)
      return;

    gst_d3d12_command_queue_fence_wait (queue, G_MAXUINT64, event_handle);
  }

  ComPtr<ID3D12GraphicsCommandList> cl;
  ComPtr<IDXGISwapChain4> swapchain;
  GstBuffer *msaa_buf = nullptr;
  std::vector<std::shared_ptr<SwapBuffer>> swap_buffers;
  D3D12_RESOURCE_DESC buffer_desc;
  GstD3D12Converter *conv = nullptr;
  GstD3D12OverlayCompositor *comp = nullptr;
  GstBuffer *cached_buf = nullptr;
  GstD3D12Device *device = nullptr;
  GstD3D12CommandQueue *queue = nullptr;
  GstD3D12CommandAllocatorPool *ca_pool = nullptr;
  UINT64 fence_val = 0;
  HANDLE event_handle;
  bool initialized = false;
};

struct GstD3D12WindowPrivate
{
  GstD3D12WindowPrivate ()
  {
    main_context = g_main_context_new ();
    loop = g_main_loop_new (main_context, FALSE);

    render_rect.w = -1;
    render_rect.h = -1;

    gst_video_info_init (&input_info);
    gst_video_info_init (&display_info);

    fence_data_pool = gst_d3d12_fence_data_pool_new ();
  }

  ~GstD3D12WindowPrivate ()
  {
    g_main_loop_unref (loop);
    g_main_context_unref (main_context);
    gst_clear_object (&fence_data_pool);
  }

  std::recursive_mutex lock;
  DXGI_FORMAT display_format = DXGI_FORMAT_R8G8B8A8_UNORM;

  GstVideoOrientationMethod orientation = GST_VIDEO_ORIENTATION_IDENTITY;
  gfloat fov = 90.0f;
  gboolean ortho = FALSE;
  gfloat rotation_x = 0;
  gfloat rotation_y = 0;
  gfloat rotation_z = 0;
  gfloat scale_x = 1.0f;
  gfloat scale_y = 1.0f;

  /* fullscreen related variables */
  gboolean fullscreen_on_alt_enter = TRUE;
  gboolean requested_fullscreen = FALSE;
  gboolean applied_fullscreen = FALSE;
  LONG restore_style;
  WINDOWPLACEMENT restore_placement;

  GstD3D12FenceDataPool *fence_data_pool;

  /* User specified rect */
  GstVideoRectangle render_rect = { };

  GstVideoRectangle output_rect = { };
  RECT dirty_rect = { };
  GstVideoInfo input_info;
  GstVideoInfo display_info;

  D3D12_BOX crop_rect = { };
  D3D12_BOX prev_crop_rect = { };

  gboolean force_aspect_ratio = TRUE;
  std::atomic<gboolean> enable_navigation = { TRUE };
  gboolean first_present = TRUE;
  gboolean backbuf_rendered = FALSE;

  std::unique_ptr<DeviceContext> ctx;

  std::wstring title;
  gboolean update_title = FALSE;

  GstD3D12MSAAMode msaa = GST_D3D12_MSAA_DISABLED;

  /* Win32 window handles */
  std::mutex hwnd_lock;
  std::condition_variable hwnd_cond;
  HWND hwnd = nullptr;
  HWND external_hwnd = nullptr;
  std::atomic<GstD3D12WindowState> state = { GST_D3D12_WINDOW_STATE_INIT };
  GThread *main_loop_thread = nullptr;

  GThread *internal_hwnd_thread = nullptr;

  GMainLoop *loop = nullptr;
  GMainContext *main_context = nullptr;
  gboolean flushing = FALSE;
};
/* *INDENT-ON* */

struct _GstD3D12Window
{
  GstObject parent;

  GstD3D12Device *device;

  GstD3D12WindowPrivate *priv;
};

static GstFlowReturn gst_d3d12_window_on_resize (GstD3D12Window * self);

#define gst_d3d12_window_parent_class parent_class
G_DEFINE_TYPE (GstD3D12Window, gst_d3d12_window, GST_TYPE_OBJECT);

static void gst_d3d12_window_finalize (GObject * object);

static void
gst_d3d12_window_class_init (GstD3D12WindowClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_d3d12_window_finalize;

  d3d12_window_signals[SIGNAL_KEY_EVENT] =
      g_signal_new ("key-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  d3d12_window_signals[SIGNAL_MOUSE_EVENT] =
      g_signal_new ("mouse-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
      G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  d3d12_window_signals[SIGNAL_FULLSCREEN] =
      g_signal_new ("fullscreen", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr,
      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_window_debug,
      "d3d12window", 0, "d3d12window");
}

static void
gst_d3d12_window_init (GstD3D12Window * self)
{
  self->priv = new GstD3D12WindowPrivate ();
}

static void
gst_d3d12_window_finalize (GObject * object)
{
  auto self = GST_D3D12_WINDOW (object);

  delete self->priv;

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_window_on_key_event (GstD3D12Window * self,
    UINT msg, WPARAM wparam, LPARAM lparam)
{
  gunichar2 keyname[128];
  const gchar *event;

  if (!GetKeyNameTextW (lparam, (LPWSTR) keyname, 128))
    return;

  gchar *name = g_utf16_to_utf8 (keyname, 128, nullptr, nullptr, nullptr);
  if (!name)
    return;

  if (msg == WM_KEYDOWN)
    event = "key-press";
  else
    event = "key-release";

  g_signal_emit (self, d3d12_window_signals[SIGNAL_KEY_EVENT], 0, event, name);
  g_free (name);
}

static void
gst_d3d12_window_on_mouse_event (GstD3D12Window * self, UINT msg, WPARAM wparam,
    LPARAM lparam)
{
  auto priv = self->priv;
  gint button = 0;
  const gchar *event = nullptr;

  switch (msg) {
    case WM_MOUSEMOVE:
      button = 0;
      event = "mouse-move";
      break;
    case WM_LBUTTONDOWN:
      button = 1;
      event = "mouse-button-press";
      break;
    case WM_LBUTTONUP:
      button = 1;
      event = "mouse-button-release";
      break;
    case WM_RBUTTONDOWN:
      button = 2;
      event = "mouse-button-press";
      break;
    case WM_RBUTTONUP:
      button = 2;
      event = "mouse-button-release";
      break;
    case WM_MBUTTONDOWN:
      button = 3;
      event = "mouse-button-press";
      break;
    case WM_MBUTTONUP:
      button = 3;
      event = "mouse-button-release";
      break;
    default:
      return;
  }

  GstVideoRectangle output_rect = { };
  GstVideoOrientationMethod orientation;
  gint in_w, in_h;
  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    orientation = priv->orientation;
    output_rect = priv->output_rect;
    in_w = priv->input_info.width;
    in_h = priv->input_info.height;
  }
  auto xpos = LOWORD (lparam);
  auto ypos = HIWORD (lparam);

  if (in_w <= 0 || in_h <= 0 || xpos < output_rect.x ||
      xpos >= output_rect.x + output_rect.w || ypos < output_rect.y ||
      ypos >= output_rect.y + output_rect.h) {
    return;
  }

  gint src_w, src_h;
  switch (orientation) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      src_w = in_h;
      src_h = in_w;
      break;
    default:
      src_w = in_w;
      src_h = in_h;
      break;
  }

  xpos = ((xpos - output_rect.x) / (double) output_rect.w) * src_w;
  ypos = ((ypos - output_rect.y) / (double) output_rect.h) * src_h;

  xpos = CLAMP (xpos, 0, (LONG) (src_w - 1));
  ypos = CLAMP (ypos, 0, (LONG) (src_h - 1));

  double final_x = 0;
  double final_y = 0;

  switch (orientation) {
    case GST_VIDEO_ORIENTATION_90R:
      final_x = ypos;
      final_y = src_w - xpos;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      final_x = src_h - ypos;
      final_y = xpos;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      final_x = src_h - ypos;
      final_y = src_w - xpos;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      final_x = ypos;
      final_y = xpos;
      break;
    case GST_VIDEO_ORIENTATION_180:
      final_x = src_w - xpos;
      final_y = src_h - ypos;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      final_x = src_w - xpos;
      final_y = ypos;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      final_x = xpos;
      final_y = src_h - ypos;
      break;
    default:
      final_x = xpos;
      final_y = ypos;
      break;
  }

  g_signal_emit (self, d3d12_window_signals[SIGNAL_MOUSE_EVENT], 0,
      event, button, final_x, final_y);
}

static void
gst_d3d12_window_toggle_fullscreen_mode (GstD3D12Window * self,
    gboolean emit_signal)
{
  auto priv = self->priv;
  HWND hwnd = nullptr;
  gboolean is_fullscreen;
  ComPtr < IDXGISwapChain > swapchain;

  {
    std::lock_guard < std::mutex > hlk (priv->hwnd_lock);
    hwnd = priv->external_hwnd ? priv->external_hwnd : priv->hwnd;

    if (!hwnd)
      return;

    if (priv->requested_fullscreen == priv->applied_fullscreen)
      return;

    {
      std::lock_guard < std::recursive_mutex > lk (priv->lock);
      if (priv->ctx)
        swapchain = priv->ctx->swapchain;
    }

    if (!swapchain)
      return;

    GST_DEBUG_OBJECT (self, "Change mode to %s",
        priv->requested_fullscreen ? "fullscreen" : "windowed");

    priv->applied_fullscreen = priv->requested_fullscreen;
    is_fullscreen = priv->applied_fullscreen;
  }

  if (!is_fullscreen) {
    SetWindowLongW (hwnd, GWL_STYLE, priv->restore_style);
    SetWindowPlacement (hwnd, &priv->restore_placement);
  } else {
    ComPtr < IDXGIOutput > output;
    DXGI_OUTPUT_DESC output_desc;

    /* remember current placement to restore window later */
    GetWindowPlacement (hwnd, &priv->restore_placement);

    /* show window before change style */
    ShowWindow (hwnd, SW_SHOW);

    priv->restore_style = GetWindowLong (hwnd, GWL_STYLE);

    /* Make the window borderless so that the client area can fill the screen */
    SetWindowLongA (hwnd, GWL_STYLE,
        priv->restore_style &
        ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU |
            WS_THICKFRAME | WS_MAXIMIZE));

    swapchain->GetContainingOutput (&output);
    output->GetDesc (&output_desc);

    SetWindowPos (hwnd, HWND_TOP,
        output_desc.DesktopCoordinates.left,
        output_desc.DesktopCoordinates.top,
        output_desc.DesktopCoordinates.right,
        output_desc.DesktopCoordinates.bottom,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

    ShowWindow (hwnd, SW_MAXIMIZE);
  }

  GST_DEBUG_OBJECT (self, "Fullscreen mode change done");

  if (emit_signal) {
    g_signal_emit (self, d3d12_window_signals[SIGNAL_FULLSCREEN],
        0, is_fullscreen);
  }
}

static GstD3D12Window *
gst_d3d12_window_from_hwnd (HWND hwnd)
{
  std::lock_guard < std::mutex > lk (global_hwnd_lock);
  auto self = GetPropW (hwnd, D3D12_WINDOW_PROP_NAME);
  if (self)
    gst_object_ref (self);

  return (GstD3D12Window *) self;
}

static LRESULT CALLBACK
gst_d3d12_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg) {
    case WM_CREATE:
    {
      LPCREATESTRUCTW create_param = (LPCREATESTRUCTW) lparam;
      SetPropW (hwnd, D3D12_WINDOW_PROP_NAME, create_param->lpCreateParams);
      break;
    }
    case WM_GST_D3D12_DESTROY_INTERNAL_WINDOW:
    {
      GST_INFO ("Handle destroy window message");
      DestroyWindow (hwnd);
      return 0;
    }
    case WM_GST_D3D12_UPDATE_RENDER_RECT:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        GstVideoRectangle render_rect = { };
        {
          auto priv = self->priv;
          std::lock_guard < std::mutex > lk (priv->hwnd_lock);
          render_rect = priv->render_rect;
        }

        if (render_rect.w > 0 && render_rect.h > 0) {
          MoveWindow (hwnd,
              render_rect.x, render_rect.y, render_rect.w, render_rect.h, TRUE);
        }

        gst_object_unref (self);
      }
      return 0;
    }
    case WM_GST_D3D12_FULLSCREEN:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        gst_d3d12_window_toggle_fullscreen_mode (self, FALSE);
        gst_object_unref (self);
      }

      return 0;
    }
    case WM_SYSKEYDOWN:
    {
      WORD state = GetKeyState (VK_RETURN);
      BYTE high = HIBYTE (state);

      if (high & 0x1) {
        auto self = gst_d3d12_window_from_hwnd (hwnd);
        if (self) {
          auto priv = self->priv;
          bool do_toggle = false;
          {
            std::lock_guard < std::mutex > lk (priv->hwnd_lock);
            if (priv->fullscreen_on_alt_enter) {
              priv->requested_fullscreen = !priv->applied_fullscreen;
              do_toggle = true;
            }
          }

          if (do_toggle)
            gst_d3d12_window_toggle_fullscreen_mode (self, TRUE);

          gst_object_unref (self);
        }
      }
      break;
    }
    case WM_SIZE:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        gst_d3d12_window_on_resize (self);
        gst_object_unref (self);
      }
      break;
    }
    case WM_CLOSE:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        auto priv = self->priv;
        GST_DEBUG_OBJECT (self, "Internal window is being closed");
        {
          std::lock_guard < std::mutex > lk (priv->hwnd_lock);
          RemovePropW (priv->hwnd, D3D12_WINDOW_PROP_NAME);
          priv->hwnd = nullptr;
          priv->state = GST_D3D12_WINDOW_STATE_CLOSED;

        }
        gst_object_unref (self);
      }
      break;
    }
    case WM_NCHITTEST:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self && self->priv->external_hwnd) {
        gst_object_unref (self);

        /* To passthrough mouse event if external window is used.
         * Only hit-test succeeded window can receive/handle some mouse events
         * and we want such events to be handled by parent (application) window
         */
        return (LRESULT) HTTRANSPARENT;
      }

      gst_clear_object (&self);
      break;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        auto priv = self->priv;
        if (priv->enable_navigation)
          gst_d3d12_window_on_key_event (self, msg, wparam, lparam);
        gst_object_unref (self);
      }
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    {
      auto self = gst_d3d12_window_from_hwnd (hwnd);
      if (self) {
        auto priv = self->priv;
        if (priv->enable_navigation)
          gst_d3d12_window_on_mouse_event (self, msg, wparam, lparam);
        gst_object_unref (self);
      }
      break;
    }
    default:
      break;
  }

  return DefWindowProcW (hwnd, msg, wparam, lparam);
}

static void
gst_d3d12_window_create_hwnd (GstD3D12Window * self)
{
  auto priv = self->priv;
  auto inst = GetModuleHandle (nullptr);

  {
    static std::mutex class_lock;
    std::lock_guard < std::mutex > lk (class_lock);
    WNDCLASSEXW wc = { };

    if (!GetClassInfoExW (inst, L"GstD3D12Hwnd", &wc)) {
      wc.cbSize = sizeof (WNDCLASSEXW);
      wc.lpfnWndProc = gst_d3d12_window_proc;
      wc.hInstance = inst;
      wc.hIcon = LoadIcon (nullptr, IDI_WINLOGO);
      wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
      wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
      wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
      wc.lpszClassName = L"GstD3D12Hwnd";

      RegisterClassExW (&wc);
    }
  }

  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int w = 0;
  int h = 0;
  DWORD style = WS_GST_D3D12;

  std::wstring title = L"Direct3D12 Renderer";
  if (!priv->title.empty ())
    title = priv->title;

  if (!priv->external_hwnd) {
    if (priv->render_rect.w > 0 && priv->render_rect.h > 0) {
      x = priv->render_rect.x;
      y = priv->render_rect.y;
      w = priv->render_rect.w;
      h = priv->render_rect.h;
    } else {
      std::lock_guard < std::recursive_mutex > lk (priv->lock);
      RECT rect = { };
      switch (priv->orientation) {
        case GST_VIDEO_ORIENTATION_90R:
        case GST_VIDEO_ORIENTATION_90L:
        case GST_VIDEO_ORIENTATION_UL_LR:
        case GST_VIDEO_ORIENTATION_UR_LL:
          rect.right = GST_VIDEO_INFO_HEIGHT (&priv->display_info);
          rect.bottom = GST_VIDEO_INFO_WIDTH (&priv->display_info);
          break;
        default:
          rect.right = GST_VIDEO_INFO_WIDTH (&priv->display_info);
          rect.bottom = GST_VIDEO_INFO_HEIGHT (&priv->display_info);
          break;
      }

      AdjustWindowRect (&rect, WS_GST_D3D12, FALSE);

      w = rect.right - rect.left;
      h = rect.bottom - rect.top;
    }

    style |= WS_VISIBLE;
  }

  priv->hwnd = CreateWindowExW (0, L"GstD3D12Hwnd", title.c_str (),
      style, x, y, w, h, (HWND) nullptr, (HMENU) nullptr, inst, self);
  priv->applied_fullscreen = FALSE;
  priv->internal_hwnd_thread = g_thread_self ();
}

static void
gst_d3d12_window_release_external_hwnd (GstD3D12Window * self)
{
  auto priv = self->priv;

  if (!priv->external_hwnd)
    return;

  auto hwnd = priv->external_hwnd;
  priv->external_hwnd = nullptr;

  auto external_proc = (WNDPROC) GetPropW (hwnd, EXTERNAL_PROC_PROP_NAME);
  if (!external_proc) {
    GST_WARNING_OBJECT (self, "Failed to get original window procedure");
    return;
  }

  GST_DEBUG_OBJECT (self, "release external window %" G_GUINTPTR_FORMAT
      ", original window procedure %p", (guintptr) hwnd, external_proc);

  RemovePropW (hwnd, EXTERNAL_PROC_PROP_NAME);
  RemovePropW (hwnd, D3D12_WINDOW_PROP_NAME);

  if (!SetWindowLongPtrW (hwnd, GWLP_WNDPROC, (LONG_PTR) external_proc))
    GST_WARNING_OBJECT (self, "Couldn't restore original window procedure");
}

static LRESULT FAR PASCAL
sub_class_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  auto external_window_proc =
      (WNDPROC) GetPropW (hwnd, EXTERNAL_PROC_PROP_NAME);
  auto self = gst_d3d12_window_from_hwnd (hwnd);

  if (!self) {
    GST_DEBUG ("No object attached to the window, chain up to default");
    return CallWindowProcW (external_window_proc, hwnd, msg, wparam, lparam);
  }

  auto priv = self->priv;
  switch (msg) {
    case WM_GST_D3D12_CONSTRUCT_INTERNAL_WINDOW:
    {
      GST_DEBUG_OBJECT (self, "Create internal window");
      std::lock_guard < std::mutex > lk (priv->hwnd_lock);
      if (priv->hwnd) {
        GST_WARNING_OBJECT (self,
            "Window already created, probably we have received 2 creation messages");
        gst_object_unref (self);
        return 0;
      }

      gst_d3d12_window_create_hwnd (self);
      SetWindowLongPtrW (priv->hwnd, GWL_STYLE, WS_CHILD | WS_MAXIMIZE);
      SetParent (priv->hwnd, priv->external_hwnd);

      RECT rect;
      GetClientRect (priv->external_hwnd, &rect);

      if (priv->render_rect.w > 0 && priv->render_rect.h > 0) {
        rect.left = priv->render_rect.x;
        rect.top = priv->render_rect.y;
        rect.right = priv->render_rect.x + priv->render_rect.w;
        rect.bottom = priv->render_rect.y + priv->render_rect.h;
      }

      SetWindowPos (priv->hwnd, HWND_TOP, rect.left, rect.top,
          rect.right - rect.left, rect.bottom - rect.top,
          SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
          SWP_FRAMECHANGED | SWP_NOACTIVATE);
      MoveWindow (priv->hwnd, rect.left, rect.top,
          rect.right - rect.left, rect.bottom - rect.top, FALSE);
      ShowWindow (priv->hwnd, SW_SHOW);

      priv->state = GST_D3D12_WINDOW_STATE_OPENED;
      priv->hwnd_cond.notify_all ();

      /* don't need to be chained up to parent window procedure,
       * as this is our custom message */
      gst_object_unref (self);
      return 0;
    }
    case WM_SYSKEYDOWN:
    {
      WORD state = GetKeyState (VK_RETURN);
      BYTE high = HIBYTE (state);

      if (high & 0x1) {
        bool do_toggle = false;
        {
          std::lock_guard < std::mutex > lk (priv->hwnd_lock);
          if (priv->fullscreen_on_alt_enter) {
            priv->requested_fullscreen = !priv->applied_fullscreen;
            do_toggle = true;
          }
        }

        if (do_toggle)
          gst_d3d12_window_toggle_fullscreen_mode (self, TRUE);
      }
      break;
    }
    case WM_SIZE:
      if (priv->render_rect.w > 0 || priv->render_rect.h > 0) {
        MoveWindow (priv->hwnd,
            priv->render_rect.x, priv->render_rect.y,
            priv->render_rect.w, priv->render_rect.h, FALSE);
      } else {
        MoveWindow (priv->hwnd, 0, 0, LOWORD (lparam), HIWORD (lparam), FALSE);
      }
      break;
    case WM_DESTROY:
    {
      std::lock_guard < std::mutex > lk (priv->hwnd_lock);
      GST_WARNING_OBJECT (self, "external window is closing");

      gst_d3d12_window_release_external_hwnd (self);

      if (priv->hwnd)
        DestroyWindow (priv->hwnd);
      priv->hwnd = nullptr;

      priv->state = GST_D3D12_WINDOW_STATE_CLOSED;
      priv->hwnd_cond.notify_all ();
      break;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
      if (priv->enable_navigation)
        gst_d3d12_window_on_key_event (self, msg, wparam, lparam);
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
      if (priv->enable_navigation)
        gst_d3d12_window_on_mouse_event (self, msg, wparam, lparam);
      break;
    default:
      break;
  }

  gst_object_unref (self);

  return CallWindowProcW (external_window_proc, hwnd, msg, wparam, lparam);
}

static gboolean
msg_io_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

static gpointer
gst_d3d12_window_hwnd_thread_func (GstD3D12Window * self)
{
  auto priv = self->priv;

  g_main_context_push_thread_default (priv->main_context);

  gst_d3d12_window_create_hwnd (self);
  priv->state = GST_D3D12_WINDOW_STATE_OPENED;

  auto msg_io_ch = g_io_channel_win32_new_messages (0);
  auto msg_source = g_io_create_watch (msg_io_ch, G_IO_IN);
  g_source_set_callback (msg_source, (GSourceFunc) msg_io_cb, nullptr, nullptr);
  g_source_attach (msg_source, priv->main_context);

  auto idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,[](gpointer data)->gboolean {
        auto self = GST_D3D12_WINDOW (data);
        auto priv = self->priv;
        std::lock_guard < std::mutex > lk (priv->hwnd_lock);
        priv->hwnd_cond.notify_all ();
        return G_SOURCE_REMOVE;
      }
      , self, nullptr);

  g_source_attach (idle_source, priv->main_context);
  g_source_unref (idle_source);

  g_main_loop_run (priv->loop);

  g_clear_pointer (&priv->hwnd, DestroyWindow);

  g_source_destroy (msg_source);
  g_source_unref (msg_source);
  g_io_channel_unref (msg_io_ch);

  g_main_context_pop_thread_default (priv->main_context);

  return nullptr;
}

static GstFlowReturn
gst_d3d12_window_prepare_hwnd (GstD3D12Window * self, guintptr window_handle)
{
  auto priv = self->priv;

  switch (priv->state) {
    case GST_D3D12_WINDOW_STATE_OPENED:
      return GST_FLOW_OK;
    case GST_D3D12_WINDOW_STATE_CLOSED:
      return GST_FLOW_ERROR;
    default:
      break;
  }

  std::unique_lock < std::mutex > lk (priv->hwnd_lock);
  priv->external_hwnd = (HWND) window_handle;
  if (!priv->external_hwnd) {
    priv->main_loop_thread = g_thread_new ("GstD3D12Window",
        (GThreadFunc) gst_d3d12_window_hwnd_thread_func, self);
    while (!g_main_loop_is_running (priv->loop))
      priv->hwnd_cond.wait (lk);

    return GST_FLOW_OK;
  }

  if (!IsWindow (priv->external_hwnd)) {
    GST_ERROR_OBJECT (self, "Invalid window handle");
    return GST_FLOW_ERROR;
  }

  {
    std::lock_guard < std::mutex > glk (global_hwnd_lock);
    auto external_proc =
        (WNDPROC) GetWindowLongPtrA (priv->external_hwnd, GWLP_WNDPROC);

    SetPropW (priv->external_hwnd, EXTERNAL_PROC_PROP_NAME,
        (HANDLE) external_proc);
    SetPropW (priv->external_hwnd, D3D12_WINDOW_PROP_NAME, self);
    SetWindowLongPtrW (priv->external_hwnd, GWLP_WNDPROC,
        (LONG_PTR) sub_class_proc);
  }

  PostMessageW (priv->external_hwnd,
      WM_GST_D3D12_CONSTRUCT_INTERNAL_WINDOW, 0, 0);
  while (priv->external_hwnd &&
      priv->state == GST_D3D12_WINDOW_STATE_INIT && !priv->flushing) {
    priv->hwnd_cond.wait (lk);
  }

  if (priv->state != GST_D3D12_WINDOW_STATE_OPENED) {
    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      return GST_FLOW_FLUSHING;
    }

    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

void
gst_d3d12_window_unprepare (GstD3D12Window * window)
{
  GST_DEBUG_OBJECT (window, "Unprepare");

  auto priv = window->priv;

  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    priv->ctx = nullptr;
    gst_clear_object (&window->device);
  }

  if (priv->external_hwnd) {
    {
      std::lock_guard < std::mutex > lk (global_hwnd_lock);
      gst_d3d12_window_release_external_hwnd (window);
    }

    std::lock_guard < std::mutex > lk (priv->hwnd_lock);
    if (priv->hwnd) {
      if (priv->internal_hwnd_thread == g_thread_self ()) {
        /* State changing thread is identical to internal window thread.
         * window can be closed here */

        GST_INFO_OBJECT (window, "Closing internal window immediately");
        DestroyWindow (priv->hwnd);
      } else {
        /* We cannot destroy internal window from non-window thread.
         * and we cannot use synchronously SendMessage() method at this point
         * since window thread might be waiting for current thread and SendMessage()
         * will be blocked until it's called from window thread.
         * Instead, posts message so that it can be closed from window thread
         * asynchronously */
        GST_INFO_OBJECT (window, "Posting custom destory message");
        PostMessageW (priv->hwnd, WM_GST_D3D12_DESTROY_INTERNAL_WINDOW, 0, 0);
      }
    }
  }

  g_main_loop_quit (priv->loop);
  g_clear_pointer (&priv->main_loop_thread, g_thread_join);

  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  priv->hwnd = nullptr;
  priv->external_hwnd = nullptr;
  priv->internal_hwnd_thread = nullptr;
  priv->state = GST_D3D12_WINDOW_STATE_INIT;
  priv->applied_fullscreen = FALSE;
}

void
gst_d3d12_window_unlock (GstD3D12Window * window)
{
  GST_DEBUG_OBJECT (window, "Unlock");

  auto priv = window->priv;
  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  priv->flushing = TRUE;
  priv->hwnd_cond.notify_all ();
}

void
gst_d3d12_window_unlock_stop (GstD3D12Window * window)
{
  GST_DEBUG_OBJECT (window, "Unlock stop");

  auto priv = window->priv;
  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  priv->flushing = FALSE;
  priv->hwnd_cond.notify_all ();
}

static GstFlowReturn
gst_d3d12_window_on_resize (GstD3D12Window * self)
{
  auto priv = self->priv;
  std::lock_guard < std::recursive_mutex > lk (priv->lock);

  /* resize might be called before swapchain configuration */
  if (!priv->ctx)
    return GST_FLOW_OK;

  if (priv->ctx->fence_val != 0)
    priv->ctx->WaitGpu ();
  priv->ctx->swap_buffers.clear ();
  gst_clear_buffer (&priv->ctx->msaa_buf);

  DXGI_SWAP_CHAIN_DESC desc = { };
  priv->ctx->swapchain->GetDesc (&desc);
  auto hr = priv->ctx->swapchain->ResizeBuffers (BACK_BUFFER_COUNT,
      0, 0, priv->display_format, desc.Flags);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't resize buffers");
    return GST_FLOW_ERROR;
  }

  for (guint i = 0; i < BACK_BUFFER_COUNT; i++) {
    ComPtr < ID3D12Resource > backbuf;
    hr = priv->ctx->swapchain->GetBuffer (i, IID_PPV_ARGS (&backbuf));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't get backbuffer");
      return GST_FLOW_ERROR;
    }

    if (i == 0)
      priv->ctx->buffer_desc = backbuf->GetDesc ();

    auto mem = gst_d3d12_allocator_alloc_wrapped (nullptr, self->device,
        backbuf.Get (), 0);
    auto buf = gst_buffer_new ();
    gst_buffer_append_memory (buf, mem);
    priv->ctx->swap_buffers.push_back (std::make_shared < SwapBuffer > (buf));
  }

  guint sample_count = 1;
  switch (priv->msaa) {
    case GST_D3D12_MSAA_2X:
      sample_count = 2;
      break;
    case GST_D3D12_MSAA_4X:
      sample_count = 4;
      break;
    case GST_D3D12_MSAA_8X:
      sample_count = 8;
      break;
    default:
      break;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS feature_data = { };
  feature_data.Format = priv->ctx->buffer_desc.Format;
  feature_data.SampleCount = sample_count;

  while (feature_data.SampleCount > 1) {
    hr = device->CheckFeatureSupport (D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &feature_data, sizeof (feature_data));
    if (SUCCEEDED (hr) && feature_data.NumQualityLevels > 0)
      break;

    feature_data.SampleCount /= 2;
  }

  if (feature_data.SampleCount > 1 && feature_data.NumQualityLevels > 0) {
    GST_DEBUG_OBJECT (self, "Enable MSAA x%d with quality level %d",
        feature_data.SampleCount, feature_data.NumQualityLevels - 1);
    D3D12_HEAP_PROPERTIES heap_prop =
        CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resource_desc =
        CD3DX12_RESOURCE_DESC::Tex2D (priv->ctx->buffer_desc.Format,
        priv->ctx->buffer_desc.Width, priv->ctx->buffer_desc.Height,
        1, 1, feature_data.SampleCount, feature_data.NumQualityLevels - 1,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE clear_value = { };
    clear_value.Format = priv->ctx->buffer_desc.Format;
    clear_value.Color[0] = 0.0f;
    clear_value.Color[1] = 0.0f;
    clear_value.Color[2] = 0.0f;
    clear_value.Color[3] = 1.0f;

    ComPtr < ID3D12Resource > msaa_texture;
    hr = device->CreateCommittedResource (&heap_prop, D3D12_HEAP_FLAG_NONE,
        &resource_desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear_value,
        IID_PPV_ARGS (&msaa_texture));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create MSAA texture");
      return GST_FLOW_ERROR;
    }

    auto mem = gst_d3d12_allocator_alloc_wrapped (nullptr, self->device,
        msaa_texture.Get (), 0);
    priv->ctx->msaa_buf = gst_buffer_new ();
    gst_buffer_append_memory (priv->ctx->msaa_buf, mem);
  }

  priv->first_present = TRUE;
  priv->backbuf_rendered = FALSE;

  /* redraw the last scene if cached buffer exits */
  GstFlowReturn ret = GST_FLOW_OK;
  if (priv->ctx->cached_buf) {
    ret = gst_d3d12_window_set_buffer (self, priv->ctx->cached_buf);
    if (ret == GST_FLOW_OK)
      ret = gst_d3d12_window_present (self);
  }

  return ret;
}

GstFlowReturn
gst_d3d12_window_prepare (GstD3D12Window * window, GstD3D12Device * device,
    guintptr window_handle, guint display_width, guint display_height,
    GstCaps * caps, GstStructure * config)
{
  auto priv = window->priv;
  GstVideoInfo in_info;
  GstVideoFormat format = GST_VIDEO_FORMAT_RGBA;
  priv->display_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  gst_video_info_from_caps (&in_info, caps);

  if (GST_VIDEO_INFO_COMP_DEPTH (&in_info, 0) > 8) {
    auto device_handle = gst_d3d12_device_get_device_handle (device);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = { };
    const D3D12_FORMAT_SUPPORT1 support_flags =
        D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_DISPLAY;
    format_support.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    format_support.Support1 = support_flags;
    auto hr = device_handle->CheckFeatureSupport (D3D12_FEATURE_FORMAT_SUPPORT,
        &format_support, sizeof (format_support));
    if (SUCCEEDED (hr) && (format_support.Support1 & support_flags)
        == support_flags) {
      priv->display_format = DXGI_FORMAT_R10G10B10A2_UNORM;
      format = GST_VIDEO_FORMAT_RGB10A2_LE;
    }
  }

  {
    std::lock_guard < std::recursive_mutex > lk (priv->lock);
    gst_video_info_set_format (&priv->display_info, format,
        display_width, display_height);
  }

  auto ret = gst_d3d12_window_prepare_hwnd (window, window_handle);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (window, "Couldn't setup window handle");
    if (config)
      gst_structure_free (config);

    return ret;
  }

  std::unique_lock < std::recursive_mutex > lk (priv->lock);
  HRESULT hr;

  if (window->device != device) {
    priv->ctx = nullptr;
    gst_clear_object (&window->device);
    window->device = (GstD3D12Device *) gst_object_ref (device);
  }

  if (!priv->ctx) {
    auto ctx = std::make_unique < DeviceContext > (device);
    if (!ctx->initialized) {
      GST_ERROR_OBJECT (window, "Couldn't initialize device context");
      if (config)
        gst_structure_free (config);

      return GST_FLOW_ERROR;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = { };
    desc.Format = priv->display_format;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = BACK_BUFFER_COUNT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    auto factory = gst_d3d12_device_get_factory_handle (device);

    ComPtr < ID3D12CommandQueue > cq;
    gst_d3d12_command_queue_get_handle (ctx->queue, &cq);

    ComPtr < IDXGISwapChain1 > swapchain;
    hr = factory->CreateSwapChainForHwnd (cq.Get (), priv->hwnd,
        &desc, nullptr, nullptr, &swapchain);
    if (!gst_d3d12_result (hr, window->device)) {
      GST_ERROR_OBJECT (window, "Couldn't create swapchain");
      if (config)
        gst_structure_free (config);

      return GST_FLOW_ERROR;
    }

    ComPtr < IDXGIFactory1 > parent_factory;
    hr = swapchain->GetParent (IID_PPV_ARGS (&parent_factory));
    if (!gst_d3d12_result (hr, window->device)) {
      GST_WARNING_OBJECT (window, "Couldn't get parent factory");
    } else {
      hr = parent_factory->MakeWindowAssociation (priv->hwnd,
          DXGI_MWA_NO_ALT_ENTER);
      if (!gst_d3d12_result (hr, window->device)) {
        GST_WARNING_OBJECT (window, "MakeWindowAssociation failed, hr: 0x%x",
            (guint) hr);
      }
    }

    hr = swapchain.As (&ctx->swapchain);
    if (!gst_d3d12_result (hr, window->device)) {
      GST_ERROR_OBJECT (window, "IDXGISwapChain4 interface is unavailable");
      if (config)
        gst_structure_free (config);
      return GST_FLOW_ERROR;
    }

    priv->ctx = std::move (ctx);
  } else {
    priv->ctx->WaitGpu ();
  }

  priv->input_info = in_info;
  priv->crop_rect = CD3DX12_BOX (0, 0, in_info.width, in_info.height);
  priv->prev_crop_rect = priv->crop_rect;

  gst_clear_object (&priv->ctx->conv);
  gst_clear_object (&priv->ctx->comp);
  gst_clear_buffer (&priv->ctx->cached_buf);

  if (GST_VIDEO_INFO_HAS_ALPHA (&in_info)) {
    gst_structure_set (config, GST_D3D12_CONVERTER_OPT_DEST_ALPHA_MODE,
        GST_TYPE_D3D12_CONVERTER_ALPHA_MODE,
        GST_D3D12_CONVERTER_ALPHA_MODE_PREMULTIPLIED, nullptr);
  }

  priv->ctx->conv = gst_d3d12_converter_new (window->device,
      &priv->input_info, &priv->display_info, nullptr, nullptr, config);
  if (!priv->ctx->conv) {
    GST_ERROR_OBJECT (window, "Couldn't create converter");
    priv->ctx = nullptr;
    return GST_FLOW_ERROR;
  }

  priv->ctx->comp = gst_d3d12_overlay_compositor_new (window->device,
      &priv->display_info);
  if (!priv->ctx->comp) {
    GST_ERROR_OBJECT (window, "Couldn't create overlay compositor");
    priv->ctx = nullptr;
    return GST_FLOW_ERROR;
  }

  ret = gst_d3d12_window_on_resize (window);
  if (ret != GST_FLOW_OK)
    return ret;

  lk.unlock ();

  std::lock_guard < std::mutex > hlk (priv->hwnd_lock);
  if (priv->requested_fullscreen != priv->applied_fullscreen)
    PostMessageW (priv->hwnd, WM_GST_D3D12_FULLSCREEN, 0, 0);

  return GST_FLOW_OK;
}

GstFlowReturn
gst_d3d12_window_set_buffer (GstD3D12Window * window, GstBuffer * buffer)
{
  auto priv = window->priv;

  if (buffer && priv->state != GST_D3D12_WINDOW_STATE_OPENED) {
    GST_ERROR_OBJECT (window, "Window was closed");
    return GST_D3D12_WINDOW_FLOW_CLOSED;
  }

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (!priv->ctx) {
    if (!buffer) {
      GST_DEBUG_OBJECT (window, "Swapchain was not configured");
      return GST_FLOW_OK;
    }

    GST_ERROR_OBJECT (window, "Swapchain was not configured");
    return GST_FLOW_ERROR;
  }

  if (buffer)
    gst_buffer_replace (&priv->ctx->cached_buf, buffer);

  if (!priv->ctx->cached_buf)
    return GST_FLOW_OK;

  auto cur_idx = priv->ctx->swapchain->GetCurrentBackBufferIndex ();
  auto swapbuf = priv->ctx->swap_buffers[cur_idx];

  auto crop_rect = priv->crop_rect;
  auto crop_meta = gst_buffer_get_video_crop_meta (priv->ctx->cached_buf);
  if (crop_meta) {
    crop_rect = CD3DX12_BOX (crop_meta->x, crop_meta->y,
        crop_meta->x + crop_meta->width, crop_meta->y + crop_meta->height);
  }

  if (crop_rect != priv->prev_crop_rect) {
    g_object_set (priv->ctx->conv, "src-x", (gint) crop_rect.left,
        "src-y", (gint) crop_rect.top,
        "src-width", (gint) (crop_rect.right - crop_rect.left),
        "src-height", (gint) (crop_rect.bottom - crop_rect.top), nullptr);
    priv->prev_crop_rect = crop_rect;
  }

  if (priv->first_present) {
    GstVideoRectangle dst_rect = { };
    GstVideoRectangle rst_rect = { };
    dst_rect.w = (gint) priv->ctx->buffer_desc.Width;
    dst_rect.h = (gint) priv->ctx->buffer_desc.Height;

    for (size_t i = 0; i < priv->ctx->swap_buffers.size (); i++)
      priv->ctx->swap_buffers[i]->first = true;

    if (priv->force_aspect_ratio) {
      GstVideoRectangle src_rect = { };

      switch (priv->orientation) {
        case GST_VIDEO_ORIENTATION_90R:
        case GST_VIDEO_ORIENTATION_90L:
        case GST_VIDEO_ORIENTATION_UL_LR:
        case GST_VIDEO_ORIENTATION_UR_LL:
          src_rect.w = GST_VIDEO_INFO_HEIGHT (&priv->display_info);
          src_rect.h = GST_VIDEO_INFO_WIDTH (&priv->display_info);
          break;
        default:
          src_rect.w = GST_VIDEO_INFO_WIDTH (&priv->display_info);
          src_rect.h = GST_VIDEO_INFO_HEIGHT (&priv->display_info);
          break;
      }

      gst_video_sink_center_rect (src_rect, dst_rect, &rst_rect, TRUE);
    } else {
      rst_rect = dst_rect;
    }

    priv->output_rect = rst_rect;
    priv->dirty_rect.left = rst_rect.x;
    priv->dirty_rect.top = rst_rect.y;
    priv->dirty_rect.right = rst_rect.x + rst_rect.w;
    priv->dirty_rect.bottom = rst_rect.y + rst_rect.h;

    g_object_set (priv->ctx->conv, "dest-x", priv->output_rect.x,
        "dest-y", priv->output_rect.y, "dest-width", priv->output_rect.w,
        "dest-height", priv->output_rect.h, nullptr);

    if (gst_d3d12_need_transform (priv->rotation_x, priv->rotation_y,
            priv->rotation_z, priv->scale_x, priv->scale_y)) {
      gst_d3d12_converter_apply_transform (priv->ctx->conv, priv->orientation,
          priv->output_rect.w, priv->output_rect.h, priv->fov, priv->ortho,
          priv->rotation_x, priv->rotation_y, priv->rotation_z,
          priv->scale_x, priv->scale_y);
    } else {
      g_object_set (priv->ctx->conv,
          "video-direction", priv->orientation, nullptr);
    }

    gst_d3d12_overlay_compositor_update_viewport (priv->ctx->comp,
        &priv->output_rect);
  }

  g_object_set (priv->ctx->conv, "fill-border", swapbuf->first, nullptr);
  swapbuf->first = FALSE;

  gst_d3d12_overlay_compositor_upload (priv->ctx->comp, priv->ctx->cached_buf);

  GstD3D12CommandAllocator *gst_ca;
  if (!gst_d3d12_command_allocator_pool_acquire (priv->ctx->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (window, "Couldn't acquire command allocator");
    return GST_FLOW_ERROR;
  }

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);
  auto hr = ca->Reset ();
  if (!gst_d3d12_result (hr, window->device)) {
    GST_ERROR_OBJECT (window, "Couldn't reset command list");
    gst_d3d12_command_allocator_unref (gst_ca);
    return GST_FLOW_ERROR;
  }

  ComPtr < ID3D12GraphicsCommandList > cl;
  if (!priv->ctx->cl) {
    auto device = gst_d3d12_device_get_device_handle (priv->ctx->device);
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), nullptr, IID_PPV_ARGS (&cl));
    if (!gst_d3d12_result (hr, priv->ctx->device)) {
      GST_ERROR_OBJECT (window, "Couldn't create command list");
      gst_d3d12_command_allocator_unref (gst_ca);
      return GST_FLOW_ERROR;
    }

    priv->ctx->cl = cl;
  } else {
    cl = priv->ctx->cl;
    hr = cl->Reset (ca.Get (), nullptr);
    if (!gst_d3d12_result (hr, priv->ctx->device)) {
      GST_ERROR_OBJECT (window, "Couldn't reset command list");
      gst_d3d12_command_allocator_unref (gst_ca);
      return GST_FLOW_ERROR;
    }
  }

  GstD3D12FenceData *fence_data;
  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  auto mem = (GstD3D12Memory *) gst_buffer_peek_memory (swapbuf->backbuf, 0);
  auto backbuf_texture = gst_d3d12_memory_get_resource_handle (mem);
  ID3D12Resource *msaa_resource = nullptr;
  GstBuffer *conv_outbuf = swapbuf->backbuf;
  if (priv->ctx->msaa_buf) {
    conv_outbuf = priv->ctx->msaa_buf;
    mem = (GstD3D12Memory *) gst_buffer_peek_memory (priv->ctx->msaa_buf, 0);
    msaa_resource = gst_d3d12_memory_get_resource_handle (mem);
    /* MSAA resource must be render target state here already */
  } else {
    D3D12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition (backbuf_texture,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cl->ResourceBarrier (1, &barrier);
  }

  if (!gst_d3d12_converter_convert_buffer (priv->ctx->conv,
          priv->ctx->cached_buf, conv_outbuf, fence_data, cl.Get ())) {
    GST_ERROR_OBJECT (window, "Couldn't build convert command");
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  if (!gst_d3d12_overlay_compositor_draw (priv->ctx->comp,
          conv_outbuf, fence_data, cl.Get ())) {
    GST_ERROR_OBJECT (window, "Couldn't build overlay command");
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  if (msaa_resource) {
    std::vector < D3D12_RESOURCE_BARRIER > barriers;
    barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (msaa_resource,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE));
    barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (backbuf_texture,
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST));
    cl->ResourceBarrier (barriers.size (), barriers.data ());

    cl->ResolveSubresource (backbuf_texture, 0, msaa_resource, 0,
        priv->display_format);

    barriers.clear ();
    barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (msaa_resource,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET));
    barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (backbuf_texture,
            D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON));
    cl->ResourceBarrier (barriers.size (), barriers.data ());
  } else {
    D3D12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition (backbuf_texture,
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

    cl->ResourceBarrier (1, &barrier);
  }

  hr = cl->Close ();
  if (!gst_d3d12_result (hr, priv->ctx->device)) {
    GST_ERROR_OBJECT (window, "Couldn't close command list");
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  guint64 max_fence_val = 0;
  auto num_mem = gst_buffer_n_memory (priv->ctx->cached_buf);
  for (guint i = 0; i < num_mem; i++) {
    mem = (GstD3D12Memory *) gst_buffer_peek_memory (priv->ctx->cached_buf, 0);
    if (mem->fence_value > max_fence_val)
      max_fence_val = mem->fence_value;
  }

  auto completed = gst_d3d12_device_get_completed_value (priv->ctx->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);

  if (completed < max_fence_val) {
    auto device_queue = gst_d3d12_device_get_command_queue (priv->ctx->device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);
    ComPtr < ID3D12Fence > fence;
    gst_d3d12_command_queue_get_fence (device_queue, &fence);
    gst_d3d12_command_queue_execute_wait (priv->ctx->queue, fence.Get (),
        max_fence_val);
  }

  ID3D12CommandList *cmd_list[] = { cl.Get () };
  hr = gst_d3d12_command_queue_execute_command_lists (priv->ctx->queue,
      1, cmd_list, &priv->ctx->fence_val);
  if (!gst_d3d12_result (hr, priv->ctx->device)) {
    GST_ERROR_OBJECT (window, "Signal failed");
    gst_d3d12_fence_data_unref (fence_data);
    return GST_FLOW_ERROR;
  }

  gst_d3d12_command_queue_set_notify (priv->ctx->queue, priv->ctx->fence_val,
      fence_data, (GDestroyNotify) gst_d3d12_fence_data_unref);

  priv->backbuf_rendered = TRUE;

  return GST_FLOW_OK;
}

GstFlowReturn
gst_d3d12_window_present (GstD3D12Window * window)
{
  auto priv = window->priv;

  if (priv->state != GST_D3D12_WINDOW_STATE_OPENED) {
    GST_ERROR_OBJECT (window, "Window was closed");
    return GST_D3D12_WINDOW_FLOW_CLOSED;
  }

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (!priv->ctx) {
    GST_ERROR_OBJECT (window, "Swapchain was not configured");
    return GST_FLOW_ERROR;
  }

  if (priv->backbuf_rendered) {
    DXGI_PRESENT_PARAMETERS params = { };

    if (!priv->first_present) {
      params.DirtyRectsCount = 1;
      params.pDirtyRects = &priv->dirty_rect;
    }

    auto hr = priv->ctx->swapchain->Present1 (0,
        DXGI_PRESENT_DO_NOT_WAIT, &params);

    priv->first_present = FALSE;
    priv->backbuf_rendered = FALSE;
    if (hr != DXGI_ERROR_WAS_STILL_DRAWING &&
        !gst_d3d12_result (hr, window->device))
      GST_WARNING_OBJECT (window, "Present return 0x%x", (guint) hr);
  }

  return GST_FLOW_OK;
}

guintptr
gst_d3d12_window_get_window_handle (GstD3D12Window * window)
{
  auto priv = window->priv;

  return (guintptr) priv->hwnd;
}

void
gst_d3d12_window_set_render_rect (GstD3D12Window * window,
    const GstVideoRectangle * rect)
{
  auto priv = window->priv;

  {
    std::lock_guard < std::mutex > lk (priv->hwnd_lock);
    priv->render_rect = *rect;
  }

  if (priv->hwnd) {
    PostMessageW (priv->hwnd, WM_GST_D3D12_UPDATE_RENDER_RECT, 0, 0);
    return;
  }
}

void
gst_d3d12_window_set_force_aspect_ratio (GstD3D12Window * window,
    gboolean force_aspect_ratio)
{
  auto priv = window->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (priv->force_aspect_ratio != force_aspect_ratio) {
    priv->force_aspect_ratio = force_aspect_ratio;
    gst_d3d12_window_on_resize (window);
  }
}

void
gst_d3d12_window_set_enable_navigation_events (GstD3D12Window * window,
    gboolean enable)
{
  auto priv = window->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  priv->enable_navigation = enable;
}

void
gst_d3d12_window_set_orientation (GstD3D12Window * window, gboolean immediate,
    GstVideoOrientationMethod orientation, gfloat fov, gboolean ortho,
    gfloat rotation_x, gfloat rotation_y, gfloat rotation_z,
    gfloat scale_x, gfloat scale_y)
{
  auto priv = window->priv;

  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (priv->orientation != orientation || priv->fov != fov
      || priv->ortho != ortho
      || priv->rotation_x != rotation_x || priv->rotation_y != rotation_y
      || priv->rotation_z != rotation_z || priv->scale_x != scale_x
      || priv->scale_y != scale_y) {
    priv->orientation = orientation;
    priv->fov = fov;
    priv->ortho = ortho;
    priv->rotation_x = rotation_x;
    priv->rotation_y = rotation_y;
    priv->rotation_z = rotation_z;
    priv->scale_x = scale_x;
    priv->scale_y = scale_y;
    priv->first_present = TRUE;
    if (immediate)
      gst_d3d12_window_set_buffer (window, nullptr);
  }
}

void
gst_d3d12_window_set_title (GstD3D12Window * window, const gchar * title)
{
  auto priv = window->priv;
  wchar_t *wtitle = nullptr;

  if (title)
    wtitle = (wchar_t *) g_utf8_to_utf16 (title, -1, nullptr, nullptr, nullptr);

  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  if (!wtitle) {
    priv->title.clear ();
  } else {
    priv->title = wtitle;
  }
  g_free (wtitle);
}

GstD3D12Window *
gst_d3d12_window_new (void)
{
  auto self = (GstD3D12Window *) g_object_new (GST_TYPE_D3D12_WINDOW, nullptr);
  gst_object_ref_sink (self);

  return self;
}

GstD3D12WindowState
gst_d3d12_window_get_state (GstD3D12Window * window)
{
  auto priv = window->priv;

  return priv->state;
}

void
gst_d3d12_window_enable_fullscreen_on_alt_enter (GstD3D12Window * window,
    gboolean enable)
{
  auto priv = window->priv;
  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  priv->fullscreen_on_alt_enter = enable;
}

void
gst_d3d12_window_set_fullscreen (GstD3D12Window * window, gboolean enable)
{
  auto priv = window->priv;
  std::lock_guard < std::mutex > lk (priv->hwnd_lock);
  priv->requested_fullscreen = enable;
  if (priv->hwnd && priv->applied_fullscreen != priv->requested_fullscreen)
    PostMessageW (priv->hwnd, WM_GST_D3D12_FULLSCREEN, 0, 0);
}

void
gst_d3d12_window_set_msaa (GstD3D12Window * window, GstD3D12MSAAMode msaa)
{
  auto priv = window->priv;
  std::lock_guard < std::recursive_mutex > lk (priv->lock);
  if (priv->msaa != msaa) {
    priv->msaa = msaa;
    gst_d3d12_window_on_resize (window);
  }
}
