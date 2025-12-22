/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#include "gstvkwindow_win32.h"

static LRESULT CALLBACK window_proc (HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);
static LRESULT FAR PASCAL sub_class_proc (HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);

enum
{
  PROP_0
};

struct _GstVulkanWindowWin32Private
{
  GIOChannel *msg_io_channel;

  gint preferred_width;
  gint preferred_height;
};

#define GST_CAT_DEFAULT gst_vulkan_window_win32_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_GET (GST_CAT_DEFAULT, "vulkanwindow");
#define gst_vulkan_window_win32_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanWindowWin32, gst_vulkan_window_win32,
    GST_TYPE_VULKAN_WINDOW, G_ADD_PRIVATE (GstVulkanWindowWin32) DEBUG_INIT);

#define GET_PRIV(window) gst_vulkan_window_win32_get_instance_private (window)

static void gst_vulkan_window_win32_set_window_handle (GstVulkanWindow * window,
    guintptr handle);
static VkSurfaceKHR
gst_vulkan_window_win32_get_surface (GstVulkanWindow * window, GError ** error);
static gboolean
gst_vulkan_window_win32_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx);
static gboolean gst_vulkan_window_win32_open (GstVulkanWindow * window,
    GError ** error);
static void gst_vulkan_window_win32_close (GstVulkanWindow * window);
static void release_parent_win_id (GstVulkanWindowWin32 * window_win32);
static void gst_vulkan_window_win32_show (GstVulkanWindowWin32 * window);
static gboolean
gst_vulkan_window_win32_create_window (GstVulkanWindowWin32 * window_win32,
    GError ** error);

static void
gst_vulkan_window_win32_class_init (GstVulkanWindowWin32Class * klass)
{
  GstVulkanWindowClass *window_class = (GstVulkanWindowClass *) klass;

  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_vulkan_window_win32_set_window_handle);
  window_class->get_surface =
      GST_DEBUG_FUNCPTR (gst_vulkan_window_win32_get_surface);
  window_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_win32_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_win32_close);
  window_class->get_presentation_support =
      GST_DEBUG_FUNCPTR (gst_vulkan_window_win32_get_presentation_support);
}

static void
gst_vulkan_window_win32_init (GstVulkanWindowWin32 * window)
{
  GstVulkanWindowWin32Private *priv = GET_PRIV (window);

  priv->preferred_width = 320;
  priv->preferred_height = 240;
}

GstVulkanWindowWin32 *
gst_vulkan_window_win32_new (GstVulkanDisplay * display)
{
  GstVulkanWindowWin32 *window;

  if ((gst_vulkan_display_get_handle_type (display) &
          GST_VULKAN_DISPLAY_TYPE_WIN32) == 0)
    /* we require an win32 display to create win32 windows */
    return NULL;

  window = g_object_new (GST_TYPE_VULKAN_WINDOW_WIN32, NULL);
  gst_object_ref_sink (window);

  return window;
}

static gboolean
msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  GST_TRACE ("handle message");
  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

typedef struct
{
  GstVulkanWindowWin32 *self;
  GError **error;
  gboolean fired;
  gboolean ret;
  GMutex lock;
  GCond cond;
} CreateWindowData;

static gboolean
_create_window (CreateWindowData * data)
{
  data->ret = gst_vulkan_window_win32_create_window (data->self, data->error);

  g_mutex_lock (&data->lock);
  data->fired = TRUE;
  g_cond_signal (&data->cond);
  g_mutex_unlock (&data->lock);

  return G_SOURCE_REMOVE;
}

static gboolean
gst_vulkan_window_win32_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowWin32 *window_win32 = GST_VULKAN_WINDOW_WIN32 (window);
  GstVulkanWindowWin32Private *priv = GET_PRIV (window_win32);
  GstVulkanDisplay *display;
  GMainContext *context;
  CreateWindowData data = { 0, };

  if (!GST_VULKAN_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  display = gst_vulkan_window_get_display (window);
  context = g_main_context_ref (display->main_context);
  gst_object_unref (display);

  priv->msg_io_channel = g_io_channel_win32_new_messages (0);
  window_win32->msg_source = g_io_create_watch (priv->msg_io_channel, G_IO_IN);
  g_source_set_callback (window_win32->msg_source, (GSourceFunc) msg_cb, NULL,
      NULL);
  g_source_attach (window_win32->msg_source, context);

  data.self = window_win32;
  data.error = error;
  g_mutex_init (&data.lock);
  g_cond_init (&data.cond);

  g_main_context_invoke (context, (GSourceFunc) _create_window, &data);
  g_mutex_lock (&data.lock);
  while (!data.fired)
    g_cond_wait (&data.cond, &data.lock);
  g_mutex_unlock (&data.lock);

  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);
  g_main_context_unref (context);

  if (!data.ret)
    return FALSE;

  gst_vulkan_window_win32_show (window_win32);

  return TRUE;
}

static void
gst_vulkan_window_win32_close (GstVulkanWindow * window)
{
  GstVulkanWindowWin32 *window_win32 = GST_VULKAN_WINDOW_WIN32 (window);
  GstVulkanWindowWin32Private *priv = GET_PRIV (window_win32);

  release_parent_win_id (window_win32);

  if (window_win32->internal_win_id) {
    RemoveProp (window_win32->internal_win_id, "vulkan_window");
    ShowWindow (window_win32->internal_win_id, SW_HIDE);
    SetParent (window_win32->internal_win_id, NULL);
    if (!DestroyWindow (window_win32->internal_win_id))
      GST_WARNING ("failed to destroy window %" G_GUINTPTR_FORMAT
          ", 0x%x", (guintptr) window_win32->internal_win_id,
          (unsigned int) GetLastError ());
  }

  g_source_destroy (window_win32->msg_source);
  g_source_unref (window_win32->msg_source);
  window_win32->msg_source = NULL;
  g_io_channel_unref (priv->msg_io_channel);
  priv->msg_io_channel = NULL;

  GST_VULKAN_WINDOW_CLASS (parent_class)->close (window);
}

static void
set_parent_win_id (GstVulkanWindowWin32 * window_win32)
{
  WNDPROC window_parent_proc;
  RECT rect;

  if (!window_win32->parent_win_id) {
    /* no parent so the internal window needs borders and system menu */
    SetWindowLongPtr (window_win32->internal_win_id, GWL_STYLE,
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW);
    SetParent (window_win32->internal_win_id, NULL);
    return;
  }

  window_parent_proc =
      (WNDPROC) GetWindowLongPtr (window_win32->parent_win_id, GWLP_WNDPROC);

  GST_DEBUG ("set parent %" G_GUINTPTR_FORMAT,
      (guintptr) window_win32->parent_win_id);

  SetProp (window_win32->parent_win_id, "vulkan_window_id",
      window_win32->internal_win_id);
  SetProp (window_win32->parent_win_id, "vulkan_window_parent_proc",
      (WNDPROC) window_parent_proc);
  SetWindowLongPtr (window_win32->parent_win_id, GWLP_WNDPROC,
      (LONG_PTR) sub_class_proc);

  SetWindowLongPtr (window_win32->internal_win_id, GWL_STYLE,
      WS_CHILD | WS_MAXIMIZE);
  SetParent (window_win32->internal_win_id, window_win32->parent_win_id);

  /* take changes into account: SWP_FRAMECHANGED */
  GetClientRect (window_win32->parent_win_id, &rect);
  SetWindowPos (window_win32->internal_win_id, HWND_TOP, rect.left, rect.top,
      rect.right, rect.bottom,
      SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
      SWP_FRAMECHANGED | SWP_NOACTIVATE);
  MoveWindow (window_win32->internal_win_id, rect.left, rect.top, rect.right,
      rect.bottom, FALSE);
}

static void
release_parent_win_id (GstVulkanWindowWin32 * window_win32)
{
  WNDPROC parent_proc;

  if (!window_win32->parent_win_id)
    return;

  parent_proc =
      GetProp (window_win32->parent_win_id, "vulkan_window_parent_proc");
  if (!parent_proc)
    return;

  GST_DEBUG ("release parent %" G_GUINTPTR_FORMAT,
      (guintptr) window_win32->parent_win_id);

  SetWindowLongPtr (window_win32->parent_win_id, GWLP_WNDPROC,
      (LONG_PTR) parent_proc);

  RemoveProp (window_win32->parent_win_id, "vulkan_window_parent_proc");
}

static gboolean
gst_vulkan_window_win32_create_window (GstVulkanWindowWin32 * window_win32,
    GError ** error)
{
  WNDCLASSEX wc;
  ATOM atom = 0;
  HINSTANCE hinstance = GetModuleHandle (NULL);

  static gint x = 0;
  static gint y = 0;

  GST_LOG ("Attempting to create a win32 window");

  x += 20;
  y += 20;

  atom = GetClassInfoEx (hinstance, "GSTVULKAN", &wc);

  if (atom == 0) {
    ZeroMemory (&wc, sizeof (WNDCLASSEX));

    wc.cbSize = sizeof (WNDCLASSEX);
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon (NULL, IDI_WINLOGO);
    wc.hIconSm = NULL;
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "GSTVULKAN";

    atom = RegisterClassEx (&wc);

    if (atom == 0) {
      g_set_error (error, GST_VULKAN_WINDOW_ERROR,
          GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
          "Failed to register window class 0x%x\n",
          (unsigned int) GetLastError ());
      goto failure;
    }
  }

  window_win32->internal_win_id = 0;
  window_win32->device = 0;
  window_win32->visible = FALSE;

  window_win32->internal_win_id = CreateWindowEx (0,
      "GSTVULKAN",
      "Vulkan renderer",
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
      x, y, 0, 0, (HWND) NULL, (HMENU) NULL, hinstance, window_win32);

  if (!window_win32->internal_win_id) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "failed to create vulkan window");
    goto failure;
  }

  GST_DEBUG ("vulkan window created: %" G_GUINTPTR_FORMAT,
      (guintptr) window_win32->internal_win_id);

  /* device is set in the window_proc */
  if (!window_win32->device) {
    g_set_error (error, GST_VULKAN_WINDOW_ERROR,
        GST_VULKAN_WINDOW_ERROR_RESOURCE_UNAVAILABLE,
        "failed to create device");
    goto failure;
  }

  ShowCursor (TRUE);

  GST_LOG ("Created a win32 window");

  /* The window has been created as if it had no parent, so there is nothing
   * else to do in that case. Even if user has already set a window,
   * parent_win_id could still be 0 at this point, and in that case calling
   * set_parent_win_id() here would steal focus from the parent window. */
  if (window_win32->parent_win_id)
    set_parent_win_id (window_win32);

  return TRUE;

failure:
  return FALSE;
}

static VkSurfaceKHR
gst_vulkan_window_win32_get_surface (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowWin32 *window_win32 = GST_VULKAN_WINDOW_WIN32 (window);
  VkWin32SurfaceCreateInfoKHR info = { 0, };
  VkSurfaceKHR ret;
  VkResult err;

  info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.hinstance = GetModuleHandle (NULL);
  info.hwnd = window_win32->parent_win_id ? window_win32->parent_win_id :
      window_win32->internal_win_id;

  if (!window_win32->CreateWin32Surface)
    window_win32->CreateWin32Surface =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkCreateWin32SurfaceKHR");
  if (!window_win32->CreateWin32Surface) {
    g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_FEATURE_NOT_PRESENT,
        "Could not retrieve \"vkCreateWin32SurfaceKHR\" function pointer");
    return VK_NULL_HANDLE;
  }

  err =
      window_win32->CreateWin32Surface (window->display->instance->instance,
      &info, NULL, &ret);

  if (gst_vulkan_error_to_g_error (err, error, "vkCreateWin32SurfaceKHR") < 0)
    return VK_NULL_HANDLE;

  return ret;
}

static gboolean
gst_vulkan_window_win32_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  GstVulkanWindowWin32 *window_win32 = GST_VULKAN_WINDOW_WIN32 (window);
  VkPhysicalDevice gpu;

  if (!window_win32->GetPhysicalDeviceWin32PresentationSupport)
    window_win32->GetPhysicalDeviceWin32PresentationSupport =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkGetPhysicalDeviceWin32PresentationSupportKHR");
  if (!window_win32->GetPhysicalDeviceWin32PresentationSupport) {
    GST_WARNING_OBJECT (window, "Could not retrieve "
        "\"GetPhysicalDeviceWin32PresentationSupport\" " "function pointer");
    return FALSE;
  }

  gpu = gst_vulkan_device_get_physical_device (device);
  if (window_win32->GetPhysicalDeviceWin32PresentationSupport (gpu,
          queue_family_idx))
    return TRUE;

  return FALSE;
}


static void
gst_vulkan_window_win32_set_window_handle (GstVulkanWindow * window,
    guintptr id)
{
  GstVulkanWindowWin32 *window_win32;

  window_win32 = GST_VULKAN_WINDOW_WIN32 (window);

  if (!window_win32->internal_win_id) {
    window_win32->parent_win_id = (HWND) id;
    return;
  }

  if (window_win32->visible) {
    ShowWindow (window_win32->internal_win_id, SW_HIDE);
    window_win32->visible = FALSE;
  }

  release_parent_win_id (window_win32);
  window_win32->parent_win_id = (HWND) id;
  set_parent_win_id (window_win32);
}

static void
gst_vulkan_window_win32_show (GstVulkanWindowWin32 * window)
{
  GstVulkanWindowWin32Private *priv = GET_PRIV (window);
  gint width = priv->preferred_width;
  gint height = priv->preferred_height;

  if (!window->visible) {
    HWND parent_id = window->parent_win_id;

    /* if no parent the real size has to be set now because this has not been done
     * when at window creation */
    if (!parent_id) {
      RECT rect;
      GetClientRect (window->internal_win_id, &rect);
      width += 2 * GetSystemMetrics (SM_CXSIZEFRAME);
      height +=
          2 * GetSystemMetrics (SM_CYSIZEFRAME) +
          GetSystemMetrics (SM_CYCAPTION);
      MoveWindow (window->internal_win_id, rect.left, rect.top, width,
          height, FALSE);
    }

    ShowWindowAsync (window->internal_win_id, SW_SHOW);
    window->visible = TRUE;
  }
}

/* PRIVATE */

static LRESULT CALLBACK
window_proc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  GstVulkanWindowWin32 *window_win32;
  LRESULT ret = 0;

  if (uMsg == WM_CREATE) {
    window_win32 =
        GST_VULKAN_WINDOW_WIN32 (((LPCREATESTRUCT) lParam)->lpCreateParams);

    GST_TRACE ("WM_CREATE");

    window_win32->device = GetDC (hWnd);
    /* Do this, otherwise we hang on exit. We can still use it (due to the
     * CS_OWNDC flag in the WindowClass) after we have Released.
     */
    ReleaseDC (hWnd, window_win32->device);

    SetProp (hWnd, "vulkan_window", window_win32);
  } else if (GetProp (hWnd, "vulkan_window")) {
    GstVulkanWindow *window;

    window_win32 = GST_VULKAN_WINDOW_WIN32 (GetProp (hWnd, "vulkan_window"));
    window = GST_VULKAN_WINDOW (window_win32);

    g_assert (window_win32->internal_win_id == hWnd);

    switch (uMsg) {
      case WM_SIZE:
        gst_vulkan_window_resize (window, LOWORD (lParam), HIWORD (lParam));
        break;
      case WM_PAINT:
      {
        PAINTSTRUCT ps;

        BeginPaint (hWnd, &ps);
        gst_vulkan_window_redraw (window);
        EndPaint (hWnd, &ps);
        break;
      }
      case WM_CLOSE:
      {
        ShowWindowAsync (window_win32->internal_win_id, SW_HIDE);

        gst_vulkan_window_win32_close (window);
        break;
      }
      case WM_ERASEBKGND:
      {
        ret = TRUE;
        break;
      }
      default:
      {
        ret = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
      }
    }
  } else {
    ret = DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  return ret;
}

static LRESULT FAR PASCAL
sub_class_proc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC window_parent_proc = GetProp (hWnd, "vulkan_window_parent_proc");

  if (uMsg == WM_SIZE) {
    HWND vulkan_window_id = GetProp (hWnd, "vulkan_window_id");
    MoveWindow (vulkan_window_id, 0, 0, LOWORD (lParam), HIWORD (lParam),
        FALSE);
  }

  return CallWindowProc (window_parent_proc, hWnd, uMsg, wParam, lParam);
}
