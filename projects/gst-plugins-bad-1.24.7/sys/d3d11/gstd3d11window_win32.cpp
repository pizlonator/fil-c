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

#include "gstd3d11window_win32.h"
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_window_debug);
#define GST_CAT_DEFAULT gst_d3d11_window_debug

G_LOCK_DEFINE_STATIC (create_lock);
G_LOCK_DEFINE_STATIC (get_instance_lock);

#define EXTERNAL_PROC_PROP_NAME "d3d11_window_external_proc"
#define D3D11_WINDOW_PROP_NAME "gst_d3d11_window_win32_object"

#define WM_GST_D3D11_FULLSCREEN (WM_USER + 1)
#define WM_GST_D3D11_CONSTRUCT_INTERNAL_WINDOW (WM_USER + 2)
#define WM_GST_D3D11_DESTROY_INTERNAL_WINDOW (WM_USER + 3)
#define WM_GST_D3D11_MOVE_WINDOW (WM_USER + 4)
#define WM_GST_D3D11_SHOW_WINDOW (WM_USER + 5)
#define WS_GST_D3D11 (WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW)

static LRESULT CALLBACK window_proc (HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);
static LRESULT FAR PASCAL sub_class_proc (HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);

typedef enum
{
  GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_NONE = 0,
  GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_OPENED,
  GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_CLOSED,
} GstD3D11WindowWin32OverlayState;

struct _GstD3D11WindowWin32
{
  GstD3D11Window parent;

  SRWLOCK lock;
  CONDITION_VARIABLE cond;

  GMainContext *main_context;
  GMainLoop *loop;

  gboolean visible;

  GSource *msg_source;
  GIOChannel *msg_io_channel;

  GThread *thread;

  GThread *internal_hwnd_thread;

  GRecMutex hwnds_lock;
  HWND internal_hwnd;
  HWND external_hwnd;
  GstD3D11WindowWin32OverlayState overlay_state;

  gboolean have_swapchain1;

  /* atomic */
  gint pending_fullscreen_count;
  gint pending_move_window;

  /* fullscreen related */
  LONG restore_style;
  WINDOWPLACEMENT restore_placement;

  /* Handle set_render_rectangle */
  GstVideoRectangle render_rect;

  gboolean flushing;
  gboolean setup_external_hwnd;
};

#define gst_d3d11_window_win32_parent_class parent_class
G_DEFINE_TYPE (GstD3D11WindowWin32, gst_d3d11_window_win32,
    GST_TYPE_D3D11_WINDOW);

static void gst_d3d11_window_win32_constructed (GObject * object);
static void gst_d3d11_window_win32_finalize (GObject * object);

static void gst_d3d11_window_win32_show (GstD3D11Window * window);
static void gst_d3d11_window_win32_update_swap_chain (GstD3D11Window * window);
static void
gst_d3d11_window_win32_change_fullscreen_mode (GstD3D11Window * window);
static gboolean
gst_d3d11_window_win32_create_swap_chain (GstD3D11Window * window,
    DXGI_FORMAT format, guint width, guint height,
    guint swapchain_flags, IDXGISwapChain ** swap_chain);
static GstFlowReturn gst_d3d11_window_win32_present (GstD3D11Window * window,
    guint present_flags);

static gpointer gst_d3d11_window_win32_thread_func (gpointer data);
static gboolean
gst_d3d11_window_win32_create_internal_window (GstD3D11WindowWin32 * self);
static void gst_d3d11_window_win32_destroy_internal_window (HWND hwnd);
static void
gst_d3d11_window_win32_release_external_handle (GstD3D11WindowWin32 * self);
static void
gst_d3d11_window_win32_on_resize (GstD3D11Window * window,
    guint width, guint height);
static GstFlowReturn gst_d3d11_window_win32_prepare (GstD3D11Window * window,
    guint display_width, guint display_height, GstCaps * caps,
    GstStructure * config, DXGI_FORMAT display_format, GError ** error);
static void gst_d3d11_window_win32_unprepare (GstD3D11Window * window);
static void
gst_d3d11_window_win32_set_render_rectangle (GstD3D11Window * window,
    const GstVideoRectangle * rect);
static void gst_d3d11_window_win32_set_title (GstD3D11Window * window,
    const gchar * title);
static gboolean gst_d3d11_window_win32_unlock (GstD3D11Window * window);
static gboolean gst_d3d11_window_win32_unlock_stop (GstD3D11Window * window);
static GstFlowReturn
gst_d3d11_window_win32_set_external_handle (GstD3D11WindowWin32 * self,
    HWND hwnd);

static void
gst_d3d11_window_win32_class_init (GstD3D11WindowWin32Class * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11WindowClass *window_class = GST_D3D11_WINDOW_CLASS (klass);

  gobject_class->constructed = gst_d3d11_window_win32_constructed;
  gobject_class->finalize = gst_d3d11_window_win32_finalize;

  window_class->show = GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_show);
  window_class->update_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_update_swap_chain);
  window_class->change_fullscreen_mode =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_change_fullscreen_mode);
  window_class->create_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_create_swap_chain);
  window_class->present = GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_present);
  window_class->on_resize =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_on_resize);
  window_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_prepare);
  window_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_unprepare);
  window_class->set_render_rectangle =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_set_render_rectangle);
  window_class->set_title =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_set_title);
  window_class->unlock = GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_unlock);
  window_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_win32_unlock_stop);
}

static void
gst_d3d11_window_win32_init (GstD3D11WindowWin32 * self)
{
  self->main_context = g_main_context_new ();
  self->restore_placement.length = sizeof (WINDOWPLACEMENT);
  g_rec_mutex_init (&self->hwnds_lock);
}

static void
gst_d3d11_window_win32_constructed (GObject * object)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (object);
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (object);

  if (window->external_handle) {
    /* Will setup internal child window on ::prepare() */
    self->setup_external_hwnd = TRUE;
    window->initialized = TRUE;
    goto done;
  }

  AcquireSRWLockExclusive (&self->lock);
  self->loop = g_main_loop_new (self->main_context, FALSE);
  self->thread = g_thread_new ("GstD3D11WindowWin32",
      (GThreadFunc) gst_d3d11_window_win32_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    SleepConditionVariableSRW (&self->cond, &self->lock, INFINITE, 0);
  ReleaseSRWLockExclusive (&self->lock);

done:
  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_d3d11_window_win32_finalize (GObject * object)
{
  GstD3D11WindowWin32 *self = (GstD3D11WindowWin32 *) object;

  GST_DEBUG_OBJECT (object, "finalize");
  gst_d3d11_window_win32_unprepare (GST_D3D11_WINDOW (object));
  g_rec_mutex_clear (&self->hwnds_lock);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_d3d11_window_win32_prepare (GstD3D11Window * window, guint display_width,
    guint display_height, GstCaps * caps, GstStructure * config,
    DXGI_FORMAT display_format, GError ** error)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  HWND hwnd;
  GstFlowReturn ret;

  if (!self->setup_external_hwnd)
    goto done;

  hwnd = (HWND) window->external_handle;
  if (!IsWindow (hwnd)) {
    gst_structure_free (config);
    GST_ERROR_OBJECT (self, "Invalid window handle");
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Invalid window handle");
    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (self, "Preparing external handle");
  ret = gst_d3d11_window_win32_set_external_handle (self, hwnd);
  if (ret != GST_FLOW_OK) {
    gst_structure_free (config);
    if (ret == GST_FLOW_FLUSHING) {
      GST_WARNING_OBJECT (self, "Flushing");
      return GST_FLOW_FLUSHING;
    }

    GST_ERROR_OBJECT (self, "Couldn't configure internal window");
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Window handle configuration failed");
    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (self, "External handle got prepared");
  self->setup_external_hwnd = FALSE;

done:
  return GST_D3D11_WINDOW_CLASS (parent_class)->prepare (window, display_width,
      display_height, caps, config, display_format, error);
}

static void
gst_d3d11_window_win32_unprepare (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);

  GST_DEBUG_OBJECT (self, "unprepare");

  if (self->external_hwnd) {
    G_LOCK (get_instance_lock);
    gst_d3d11_window_win32_release_external_handle (self);
    RemovePropA (self->internal_hwnd, D3D11_WINDOW_PROP_NAME);
    G_UNLOCK (get_instance_lock);

    if (self->internal_hwnd_thread == g_thread_self ()) {
      /* State changing thread is identical to internal window thread.
       * window can be closed here */

      GST_INFO_OBJECT (self, "Closing internal window immediately");
      gst_d3d11_window_win32_destroy_internal_window (self->internal_hwnd);
    } else if (self->internal_hwnd) {
      /* We cannot destroy internal window from non-window thread.
       * and we cannot use synchronously SendMessage() method at this point
       * since window thread might be waiting for current thread and SendMessage()
       * will be blocked until it's called from window thread.
       * Instead, posts message so that it can be closed from window thread
       * asynchronously */
      GST_INFO_OBJECT (self, "Posting custom destory message");
      PostMessageA (self->internal_hwnd, WM_GST_D3D11_DESTROY_INTERNAL_WINDOW,
          0, 0);
    }

    /* Hold hwnds lock to allow the window thread finish processing what it could
     * already have in the queue at this moment, before we reset the handlers to null */
    g_rec_mutex_lock (&self->hwnds_lock);
    self->external_hwnd = NULL;
    self->internal_hwnd = NULL;
    self->internal_hwnd_thread = NULL;
    g_rec_mutex_unlock (&self->hwnds_lock);
  }

  if (self->loop) {
    g_main_loop_quit (self->loop);
  }

  if (self->thread) {
    g_thread_join (self->thread);
    self->thread = NULL;
  }

  if (self->loop) {
    g_main_loop_unref (self->loop);
    self->loop = NULL;
  }

  if (self->main_context) {
    g_main_context_unref (self->main_context);
    self->main_context = NULL;
  }
}

static GstD3D11WindowWin32 *
gst_d3d11_window_win32_hwnd_get_instance (HWND hwnd)
{
  HANDLE handle;
  G_LOCK (get_instance_lock);
  handle = GetPropA (hwnd, D3D11_WINDOW_PROP_NAME);
  if (handle)
    handle = gst_object_ref (handle);
  G_UNLOCK (get_instance_lock);

  return (GstD3D11WindowWin32 *) handle;
}

static void
gst_d3d11_window_win32_set_render_rectangle (GstD3D11Window * window,
    const GstVideoRectangle * rect)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);

  self->render_rect = *rect;

  if (self->external_hwnd && self->internal_hwnd) {
    g_atomic_int_add (&self->pending_move_window, 1);

    if (self->internal_hwnd_thread == g_thread_self ()) {
      /* We are on message pumping thread already, handle this synchroniously */
      SendMessageA (self->internal_hwnd, WM_GST_D3D11_MOVE_WINDOW, 0, 0);
    } else {
      /* Post message to message pumping thread. Handling HWND specific message
       * on message pumping thread is not a worst idea in generall */
      PostMessageA (self->internal_hwnd, WM_GST_D3D11_MOVE_WINDOW, 0, 0);
    }
  } else if (!window->external_handle && self->internal_hwnd) {
    MoveWindow (self->internal_hwnd,
        self->render_rect.x, self->render_rect.y, self->render_rect.w,
        self->render_rect.h, TRUE);
  }
}

static void
gst_d3d11_window_win32_set_title (GstD3D11Window * window, const gchar * title)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);

  /* Do this only when we are rendring on our own HWND */
  if (!self->external_hwnd && self->internal_hwnd) {
    gunichar2 *str = g_utf8_to_utf16 (title, -1, nullptr, nullptr, nullptr);

    if (str) {
      SetWindowTextW (self->internal_hwnd, (LPCWSTR) str);
      g_free (str);
    }
  }
}

static gboolean
gst_d3d11_window_win32_unlock (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  GstD3D11SRWLockGuard lk (&self->lock);

  GST_DEBUG_OBJECT (self, "Unlock");

  self->flushing = TRUE;
  WakeAllConditionVariable (&self->cond);

  return TRUE;
}

static gboolean
gst_d3d11_window_win32_unlock_stop (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  GstD3D11SRWLockGuard lk (&self->lock);

  GST_DEBUG_OBJECT (self, "Unlock stop");

  self->flushing = FALSE;
  WakeAllConditionVariable (&self->cond);

  return TRUE;
}

static gboolean
running_cb (gpointer user_data)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (user_data);

  GST_TRACE_OBJECT (self, "Main loop running now");

  AcquireSRWLockExclusive (&self->lock);
  WakeConditionVariable (&self->cond);
  ReleaseSRWLockExclusive (&self->lock);

  return G_SOURCE_REMOVE;
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

static gpointer
gst_d3d11_window_win32_thread_func (gpointer data)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (data);
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (data);
  GSource *source;

  GST_DEBUG_OBJECT (self, "Enter loop");
  g_main_context_push_thread_default (self->main_context);

  window->initialized = gst_d3d11_window_win32_create_internal_window (self);

  /* Watching and dispatching all messages on this thread */
  self->msg_io_channel = g_io_channel_win32_new_messages (0);
  self->msg_source = g_io_create_watch (self->msg_io_channel, G_IO_IN);
  g_source_set_callback (self->msg_source, (GSourceFunc) msg_cb, self, NULL);
  g_source_attach (self->msg_source, self->main_context);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) running_cb, self, NULL);
  g_source_attach (source, self->main_context);
  g_source_unref (source);

  g_main_loop_run (self->loop);

  RemovePropA (self->internal_hwnd, D3D11_WINDOW_PROP_NAME);
  gst_d3d11_window_win32_destroy_internal_window (self->internal_hwnd);
  self->internal_hwnd = NULL;
  self->internal_hwnd_thread = NULL;

  if (self->msg_source) {
    g_source_destroy (self->msg_source);
    g_source_unref (self->msg_source);
    self->msg_source = NULL;
  }

  if (self->msg_io_channel) {
    g_io_channel_unref (self->msg_io_channel);
    self->msg_io_channel = NULL;
  }

  g_main_context_pop_thread_default (self->main_context);

  GST_DEBUG_OBJECT (self, "Exit loop");

  return NULL;
}

static void
gst_d3d11_window_win32_destroy_internal_window (HWND hwnd)
{
  if (!hwnd)
    return;

  ShowWindow (hwnd, SW_HIDE);
  SetParent (hwnd, NULL);

  GST_INFO ("Destroying internal window %" G_GUINTPTR_FORMAT, (guintptr) hwnd);

  if (!DestroyWindow (hwnd))
    g_critical ("failed to destroy window %" G_GUINTPTR_FORMAT
        ", 0x%x", (guintptr) hwnd, (guint) GetLastError ());
}

static GstFlowReturn
gst_d3d11_window_win32_set_external_handle (GstD3D11WindowWin32 * self,
    HWND hwnd)
{
  WNDPROC external_window_proc;
  GstFlowReturn ret = GST_FLOW_OK;

  GstD3D11SRWLockGuard lk (&self->lock);
  self->overlay_state = GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_NONE;
  self->external_hwnd = hwnd;

  G_LOCK (get_instance_lock);
  external_window_proc = (WNDPROC) GetWindowLongPtrA (hwnd, GWLP_WNDPROC);

  GST_DEBUG_OBJECT (self, "set external window %" G_GUINTPTR_FORMAT
      ", original window procedure %p", (guintptr) hwnd, external_window_proc);

  g_assert (external_window_proc != sub_class_proc);
  g_warn_if_fail (GetPropA (hwnd, EXTERNAL_PROC_PROP_NAME) == NULL);
  g_warn_if_fail (GetPropA (hwnd, D3D11_WINDOW_PROP_NAME) == NULL);

  SetPropA (hwnd, EXTERNAL_PROC_PROP_NAME, (HANDLE) external_window_proc);
  SetPropA (hwnd, D3D11_WINDOW_PROP_NAME, self);

  SetWindowLongPtrA (hwnd, GWLP_WNDPROC, (LONG_PTR) sub_class_proc);
  G_UNLOCK (get_instance_lock);

  /* SendMessage() may cause deadlock if parent window thread is busy
   * for changing pipeline's state. Post our message instead, and wait for
   * the parent window's thread or flushing */
  PostMessageA (hwnd, WM_GST_D3D11_CONSTRUCT_INTERNAL_WINDOW, 0, 0);
  while (self->external_hwnd &&
      self->overlay_state == GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_NONE &&
      !self->flushing) {
    SleepConditionVariableSRW (&self->cond, &self->lock, INFINITE, 0);
  }

  if (self->overlay_state != GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_OPENED) {
    if (self->flushing)
      ret = GST_FLOW_FLUSHING;
    else
      ret = GST_FLOW_ERROR;
  }

  return ret;
}

static void
gst_d3d11_window_win32_release_external_handle (GstD3D11WindowWin32 * self)
{
  WNDPROC external_proc;
  HWND hwnd = self->external_hwnd;

  if (!hwnd)
    return;

  self->external_hwnd = NULL;
  external_proc = (WNDPROC) GetPropA (hwnd, EXTERNAL_PROC_PROP_NAME);
  if (!external_proc) {
    GST_WARNING_OBJECT (self, "Failed to get original window procedure");
    return;
  }

  GST_DEBUG_OBJECT (self, "release external window %" G_GUINTPTR_FORMAT
      ", original window procedure %p", (guintptr) hwnd, external_proc);

  RemovePropA (hwnd, EXTERNAL_PROC_PROP_NAME);
  RemovePropA (hwnd, D3D11_WINDOW_PROP_NAME);

  if (!SetWindowLongPtrA (hwnd, GWLP_WNDPROC, (LONG_PTR) external_proc))
    GST_WARNING_OBJECT (self, "Couldn't restore original window procedure");
}

static gboolean
gst_d3d11_window_win32_create_internal_window (GstD3D11WindowWin32 * self)
{
  WNDCLASSEXA wc;
  ATOM atom = 0;
  HINSTANCE hinstance = GetModuleHandleA (NULL);

  GST_LOG_OBJECT (self, "Attempting to create a win32 window");

  G_LOCK (create_lock);
  atom = GetClassInfoExA (hinstance, "GSTD3D11", &wc);
  if (atom == 0) {
    GST_LOG_OBJECT (self, "Register internal window class");
    ZeroMemory (&wc, sizeof (WNDCLASSEXA));

    wc.cbSize = sizeof (WNDCLASSEXA);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon (NULL, IDI_WINLOGO);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
    wc.lpszClassName = "GSTD3D11";

    atom = RegisterClassExA (&wc);

    if (atom == 0) {
      G_UNLOCK (create_lock);
      GST_ERROR_OBJECT (self, "Failed to register window class 0x%x",
          (unsigned int) GetLastError ());
      return FALSE;
    }
  } else {
    GST_LOG_OBJECT (self, "window class was already registered");
  }

  self->internal_hwnd = 0;
  self->visible = FALSE;

  self->internal_hwnd = CreateWindowExA (0,
      "GSTD3D11",
      "Direct3D11 renderer", WS_GST_D3D11,
      CW_USEDEFAULT, CW_USEDEFAULT,
      0, 0, (HWND) NULL, (HMENU) NULL, hinstance, self);

  G_UNLOCK (create_lock);

  if (!self->internal_hwnd) {
    GST_ERROR_OBJECT (self, "Failed to create d3d11 window");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "d3d11 window created: %" G_GUINTPTR_FORMAT,
      (guintptr) self->internal_hwnd);

  GST_LOG_OBJECT (self,
      "Created a internal d3d11 window %p", self->internal_hwnd);

  self->internal_hwnd_thread = g_thread_self ();

  return TRUE;
}

/* always called from window thread */
static void
gst_d3d11_window_win32_change_fullscreen_mode_internal (GstD3D11WindowWin32 *
    self)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (self);
  HWND hwnd = self->external_hwnd ? self->external_hwnd : self->internal_hwnd;

  if (!window->swap_chain)
    return;

  if (window->requested_fullscreen == window->fullscreen)
    return;

  GST_DEBUG_OBJECT (self, "Change mode to %s",
      window->requested_fullscreen ? "fullscreen" : "windowed");

  window->fullscreen = !window->fullscreen;

  if (!window->fullscreen) {
    /* Restore the window's attributes and size */
    SetWindowLongA (hwnd, GWL_STYLE, self->restore_style);

    SetWindowPlacement (hwnd, &self->restore_placement);
  } else {
    ComPtr < IDXGIOutput > output;
    DXGI_OUTPUT_DESC output_desc;
    IDXGISwapChain *swap_chain = window->swap_chain;

    /* remember current placement to restore window later */
    GetWindowPlacement (hwnd, &self->restore_placement);

    /* show window before change style */
    ShowWindow (hwnd, SW_SHOW);

    self->restore_style = GetWindowLong (hwnd, GWL_STYLE);

    /* Make the window borderless so that the client area can fill the screen */
    SetWindowLongA (hwnd, GWL_STYLE,
        self->restore_style &
        ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU |
            WS_THICKFRAME | WS_MAXIMIZE));

    swap_chain->GetContainingOutput (&output);
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
}

static void
gst_d3d11_window_win32_on_key_event (GstD3D11WindowWin32 * self,
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (self);
  gunichar2 wcrep[128];
  const gchar *event;

  if (!window->enable_navigation_events)
    return;

  if (GetKeyNameTextW (lParam, (LPWSTR) wcrep, 128)) {
    gchar *utfrep = g_utf16_to_utf8 (wcrep, 128, NULL, NULL, NULL);
    if (utfrep) {
      if (uMsg == WM_KEYDOWN)
        event = "key-press";
      else
        event = "key-release";

      gst_d3d11_window_on_key_event (window, event, utfrep);
      g_free (utfrep);
    }
  }
}

static void
gst_d3d11_window_win32_on_mouse_event (GstD3D11WindowWin32 * self,
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (self);
  gint button;
  const gchar *event = NULL;

  if (!window->enable_navigation_events)
    return;

  switch (uMsg) {
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
      break;
  }

  if (event)
    gst_d3d11_window_on_mouse_event (window,
        event, button, (gdouble) LOWORD (lParam), (gdouble) HIWORD (lParam));
}

static void
gst_d3d11_window_win32_handle_window_proc (GstD3D11WindowWin32 * self,
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (self);

  switch (uMsg) {
    case WM_SIZE:
      gst_d3d11_window_win32_on_resize (window, 0, 0);
      break;
    case WM_CLOSE:
      if (self->internal_hwnd) {
        RemovePropA (self->internal_hwnd, D3D11_WINDOW_PROP_NAME);
        gst_d3d11_window_win32_destroy_internal_window (hWnd);
        self->overlay_state = GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_CLOSED;
        self->internal_hwnd = NULL;
        self->internal_hwnd_thread = NULL;
      }
      break;
    case WM_KEYDOWN:
    case WM_KEYUP:
      gst_d3d11_window_win32_on_key_event (self, hWnd, uMsg, wParam, lParam);
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
      gst_d3d11_window_win32_on_mouse_event (self, hWnd, uMsg, wParam, lParam);
      break;
    case WM_SYSKEYDOWN:
      if ((window->fullscreen_toggle_mode &
              GST_D3D11_WINDOW_FULLSCREEN_TOGGLE_MODE_ALT_ENTER)
          == GST_D3D11_WINDOW_FULLSCREEN_TOGGLE_MODE_ALT_ENTER) {
        WORD state = GetKeyState (VK_RETURN);
        BYTE high = HIBYTE (state);

        if (high & 0x1) {
          window->requested_fullscreen = !window->fullscreen;
          gst_d3d11_window_win32_change_fullscreen_mode_internal (self);
        }
      }
      break;
    case WM_GST_D3D11_FULLSCREEN:
      if (g_atomic_int_get (&self->pending_fullscreen_count)) {
        g_atomic_int_dec_and_test (&self->pending_fullscreen_count);
        if ((window->fullscreen_toggle_mode &
                GST_D3D11_WINDOW_FULLSCREEN_TOGGLE_MODE_PROPERTY)
            == GST_D3D11_WINDOW_FULLSCREEN_TOGGLE_MODE_PROPERTY)
          gst_d3d11_window_win32_change_fullscreen_mode_internal (self);
      }
      break;
    case WM_GST_D3D11_MOVE_WINDOW:
      if (g_atomic_int_get (&self->pending_move_window)) {
        g_atomic_int_set (&self->pending_move_window, 0);

        if (self->internal_hwnd && self->external_hwnd) {
          if (self->render_rect.w < 0 || self->render_rect.h < 0) {
            RECT rect;

            /* Reset render rect and back to full-size window */
            if (GetClientRect (self->external_hwnd, &rect)) {
              MoveWindow (self->internal_hwnd, 0, 0,
                  rect.right - rect.left, rect.bottom - rect.top, FALSE);
            }
          } else {
            MoveWindow (self->internal_hwnd, self->render_rect.x,
                self->render_rect.y, self->render_rect.w, self->render_rect.h,
                FALSE);
          }
        }
      }
      break;
    case WM_GST_D3D11_SHOW_WINDOW:
      ShowWindow (self->internal_hwnd, SW_SHOW);
      break;
    default:
      break;
  }

  return;
}

static LRESULT CALLBACK
window_proc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  GstD3D11WindowWin32 *self;

  if (uMsg == WM_GST_D3D11_DESTROY_INTERNAL_WINDOW) {
    GST_INFO ("Handle destroy window message");
    gst_d3d11_window_win32_destroy_internal_window (hWnd);
    return 0;
  }

  if (uMsg == WM_CREATE) {
    self = GST_D3D11_WINDOW_WIN32 (((LPCREATESTRUCT) lParam)->lpCreateParams);

    GST_LOG_OBJECT (self, "WM_CREATE");

    SetPropA (hWnd, D3D11_WINDOW_PROP_NAME, self);
  } else if ((self = gst_d3d11_window_win32_hwnd_get_instance (hWnd))) {
    g_rec_mutex_lock (&self->hwnds_lock);
    if (self->internal_hwnd != hWnd) {
      g_warn_if_fail (self->internal_hwnd == NULL);
      /* Most likely the instance is already in the destruction proccess,
       * just let it process WM_GST_D3D11_DESTROY_INTERNAL_WINDOW */
      g_rec_mutex_unlock (&self->hwnds_lock);
      gst_object_unref (self);
      return DefWindowProcA (hWnd, uMsg, wParam, lParam);
    }

    gst_d3d11_window_win32_handle_window_proc (self, hWnd, uMsg, wParam,
        lParam);
    g_rec_mutex_unlock (&self->hwnds_lock);

    switch (uMsg) {
      case WM_SIZE:
        /* We handled this event already */
        gst_object_unref (self);
        return 0;
      case WM_NCHITTEST:
        /* To passthrough mouse event if external window is used.
         * Only hit-test succeeded window can receive/handle some mouse events
         * and we want such events to be handled by parent (application) window
         */
        if (self->external_hwnd) {
          gst_object_unref (self);
          return (LRESULT) HTTRANSPARENT;
        }
        break;
      default:
        break;
    }
    gst_object_unref (self);
  }

  return DefWindowProcA (hWnd, uMsg, wParam, lParam);
}

static LRESULT FAR PASCAL
sub_class_proc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC external_window_proc =
      (WNDPROC) GetPropA (hWnd, EXTERNAL_PROC_PROP_NAME);
  GstD3D11WindowWin32 *self = gst_d3d11_window_win32_hwnd_get_instance (hWnd);

  if (self == NULL || self->flushing) {
    GST_DEBUG ("No object attached to the window, chain up to default");
    gst_clear_object (&self);
    return CallWindowProcA (external_window_proc, hWnd, uMsg, wParam, lParam);
  }

  switch (uMsg) {
    case WM_GST_D3D11_CONSTRUCT_INTERNAL_WINDOW:{
      GstD3D11Window *window = GST_D3D11_WINDOW (self);
      RECT rect;

      GST_DEBUG_OBJECT (self, "Create internal window");

      GstD3D11SRWLockGuard lk (&self->lock);
      if (self->internal_hwnd) {
        GST_WARNING_OBJECT (self,
            "Window already created, probably we have received 2 creation messages");
        g_warn_if_fail (self->overlay_state ==
            GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_OPENED);
        gst_object_unref (self);
        return 0;
      }

      if (self->flushing) {
        GST_DEBUG_OBJECT (self, "Flushing");
        gst_object_unref (self);
        return 0;
      }

      window->initialized =
          gst_d3d11_window_win32_create_internal_window (self);

      SetWindowLongPtrA (self->internal_hwnd, GWL_STYLE,
          WS_CHILD | WS_MAXIMIZE);
      SetParent (self->internal_hwnd, self->external_hwnd);

      /* take changes into account: SWP_FRAMECHANGED */
      GetClientRect (self->external_hwnd, &rect);

      if (self->render_rect.x != 0 || self->render_rect.y != 0 ||
          self->render_rect.w != 0 || self->render_rect.h != 0) {
        rect.left = self->render_rect.x;
        rect.top = self->render_rect.y;
        rect.right = self->render_rect.x + self->render_rect.w;
        rect.bottom = self->render_rect.y + self->render_rect.h;
      }

      SetWindowPos (self->internal_hwnd, HWND_TOP, rect.left, rect.top,
          rect.right - rect.left, rect.bottom - rect.top,
          SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
          SWP_FRAMECHANGED | SWP_NOACTIVATE);
      MoveWindow (self->internal_hwnd, rect.left, rect.top,
          rect.right - rect.left, rect.bottom - rect.top, FALSE);

      self->overlay_state = GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_OPENED;
      WakeAllConditionVariable (&self->cond);

      /* don't need to be chained up to parent window procedure,
       * as this is our custom message */
      gst_object_unref (self);
      return 0;
    }
    case WM_SIZE:
      if (self->render_rect.x != 0 || self->render_rect.y != 0 ||
          self->render_rect.w != 0 || self->render_rect.h != 0) {
        MoveWindow (self->internal_hwnd,
            self->render_rect.x, self->render_rect.y,
            self->render_rect.w, self->render_rect.h, FALSE);
      } else {
        MoveWindow (self->internal_hwnd, 0, 0, LOWORD (lParam), HIWORD (lParam),
            FALSE);
      }
      break;
    case WM_CLOSE:
    case WM_DESTROY:{
      GstD3D11SRWLockGuard lk (&self->lock);
      GST_WARNING_OBJECT (self, "external window is closing");
      gst_d3d11_window_win32_release_external_handle (self);

      if (self->internal_hwnd) {
        RemovePropA (self->internal_hwnd, D3D11_WINDOW_PROP_NAME);
        gst_d3d11_window_win32_destroy_internal_window (self->internal_hwnd);
      }
      self->internal_hwnd = NULL;
      self->internal_hwnd_thread = NULL;

      self->overlay_state = GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_CLOSED;
      WakeAllConditionVariable (&self->cond);
      break;
    }
    default:
      gst_d3d11_window_win32_handle_window_proc (self, hWnd, uMsg, wParam,
          lParam);
      break;
  }

  gst_object_unref (self);
  return CallWindowProcA (external_window_proc, hWnd, uMsg, wParam, lParam);
}

static void
gst_d3d11_window_win32_disable_alt_enter (GstD3D11WindowWin32 * self,
    GstD3D11Device * device, IDXGISwapChain * swap_chain, HWND hwnd)
{
  ComPtr < IDXGIFactory1 > factory;
  HRESULT hr;

  hr = swap_chain->GetParent (IID_PPV_ARGS (&factory));
  if (!gst_d3d11_result (hr, device) || !factory) {
    GST_WARNING_OBJECT (self,
        "Cannot get parent dxgi factory for swapchain %p, hr: 0x%x",
        swap_chain, (guint) hr);
    return;
  }

  hr = factory->MakeWindowAssociation (hwnd, DXGI_MWA_NO_ALT_ENTER);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self,
        "MakeWindowAssociation failure, hr: 0x%x", (guint) hr);
  }
}

static IDXGISwapChain *
create_swap_chain (GstD3D11WindowWin32 * self, GstD3D11Device * device,
    DXGI_SWAP_CHAIN_DESC * desc)
{
  HRESULT hr;
  IDXGISwapChain *swap_chain = NULL;
  ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
  IDXGIFactory1 *factory = gst_d3d11_device_get_dxgi_factory_handle (device);

  GstD3D11DeviceLockGuard lk (device);
  hr = factory->CreateSwapChain (device_handle, desc, &swap_chain);

  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Cannot create SwapChain Object: 0x%x",
        (guint) hr);
    swap_chain = NULL;
  }

  return swap_chain;
}

static IDXGISwapChain1 *
create_swap_chain_for_hwnd (GstD3D11WindowWin32 * self, GstD3D11Device * device,
    HWND hwnd, DXGI_SWAP_CHAIN_DESC1 * desc,
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC * fullscreen_desc, IDXGIOutput * output)
{
  HRESULT hr;
  IDXGISwapChain1 *swap_chain = NULL;
  ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
  IDXGIFactory1 *factory = gst_d3d11_device_get_dxgi_factory_handle (device);
  ComPtr < IDXGIFactory2 > factory2;

  hr = factory->QueryInterface (IID_PPV_ARGS (&factory2));
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "IDXGIFactory2 interface is unavailable");
    return NULL;
  }

  GstD3D11DeviceLockGuard lk (device);
  hr = factory2->CreateSwapChainForHwnd (device_handle, hwnd, desc,
      fullscreen_desc, output, &swap_chain);

  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Cannot create SwapChain Object: 0x%x",
        (guint) hr);
    swap_chain = NULL;
  }

  return swap_chain;
}

static gboolean
gst_d3d11_window_win32_create_swap_chain (GstD3D11Window * window,
    DXGI_FORMAT format, guint width, guint height,
    guint swapchain_flags, IDXGISwapChain ** swap_chain)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  DXGI_SWAP_CHAIN_DESC desc = { 0, };
  IDXGISwapChain *new_swapchain = NULL;
  GstD3D11Device *device = window->device;

  self->have_swapchain1 = FALSE;

  {
    DXGI_SWAP_CHAIN_DESC1 desc1 = { 0, };
    desc1.Width = 0;
    desc1.Height = 0;
    desc1.Format = format;
    /* FIXME: add support stereo */
    desc1.Stereo = FALSE;
    desc1.SampleDesc.Count = 1;
    desc1.SampleDesc.Quality = 0;
    desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc1.BufferCount = 2;
    desc1.Scaling = DXGI_SCALING_STRETCH;

    /* scaling-stretch would break aspect-ratio so we prefer to use scaling-none,
     * but Windows7 does not support this method */
    if (gst_d3d11_is_windows_8_or_greater ())
      desc1.Scaling = DXGI_SCALING_NONE;
    desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc1.Flags = swapchain_flags;

    new_swapchain = create_swap_chain_for_hwnd (self, device,
        self->internal_hwnd, &desc1, NULL, NULL);

    if (!new_swapchain) {
      GST_WARNING_OBJECT (self, "Failed to create swapchain1");
    } else {
      self->have_swapchain1 = TRUE;
    }
  }

  if (!new_swapchain) {
    DXGI_SWAP_EFFECT swap_effect = DXGI_SWAP_EFFECT_DISCARD;

    if (gst_d3d11_is_windows_8_or_greater ())
      swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    /* we will get client area at on_resize */
    desc.BufferDesc.Width = 0;
    desc.BufferDesc.Height = 0;
    /* don't care refresh rate */
    desc.BufferDesc.RefreshRate.Numerator = 0;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferDesc.Format = format;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = swap_effect;
    desc.OutputWindow = self->internal_hwnd;
    desc.Windowed = TRUE;
    desc.Flags = swapchain_flags;

    new_swapchain = create_swap_chain (self, device, &desc);
  }

  if (!new_swapchain) {
    GST_ERROR_OBJECT (self, "Cannot create swapchain");
    return FALSE;
  }

  /* disable alt+enter here. It should be manually handled */
  GstD3D11DeviceLockGuard lk (device);
  gst_d3d11_window_win32_disable_alt_enter (self,
      device, new_swapchain, desc.OutputWindow);

  *swap_chain = new_swapchain;

  return TRUE;
}

static void
gst_d3d11_window_win32_show (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  gint width, height;

  switch (window->method) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      width = GST_VIDEO_INFO_HEIGHT (&window->render_info);
      height = GST_VIDEO_INFO_WIDTH (&window->render_info);
      break;
    default:
      width = GST_VIDEO_INFO_WIDTH (&window->render_info);
      height = GST_VIDEO_INFO_HEIGHT (&window->render_info);
      break;
  }

  if (!self->visible) {
    /* if no parent the real size has to be set now because this has not been done
     * when at window creation */
    if (!self->external_hwnd) {
      if (self->render_rect.x != 0 || self->render_rect.y != 0 ||
          self->render_rect.w != 0 || self->render_rect.h != 0) {
        MoveWindow (self->internal_hwnd,
            self->render_rect.x, self->render_rect.y, self->render_rect.w,
            self->render_rect.h, FALSE);
      } else {
        RECT rect = { 0, };

        rect.right = width;
        rect.bottom = height;

        if (AdjustWindowRect (&rect, WS_GST_D3D11, FALSE)) {
          width = rect.right - rect.left;
          height = rect.bottom - rect.top;
        } else {
          width += 2 * GetSystemMetrics (SM_CXSIZEFRAME);
          height +=
              2 * GetSystemMetrics (SM_CYSIZEFRAME) +
              GetSystemMetrics (SM_CYCAPTION);
        }

        MoveWindow (self->internal_hwnd, 0, 0, width, height, FALSE);
      }

      ShowWindow (self->internal_hwnd, SW_SHOW);
    } else if (self->internal_hwnd) {
      /* ShowWindow will throw message to message pumping thread (app thread)
       * synchroniously, which can be blocked at the moment.
       * Post message to internal hwnd and do that from message pumping thread
       */
      PostMessageA (self->internal_hwnd, WM_GST_D3D11_SHOW_WINDOW, 0, 0);
    }

    if (g_atomic_int_get (&self->pending_fullscreen_count) > 0)
      PostMessageA (self->internal_hwnd, WM_GST_D3D11_FULLSCREEN, 0, 0);

    self->visible = TRUE;
  }
}

static GstFlowReturn
gst_d3d11_window_win32_present (GstD3D11Window * window, guint present_flags)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  HRESULT hr;

  if ((!self->external_hwnd &&
          self->overlay_state == GST_D3D11_WINDOW_WIN32_OVERLAY_STATE_CLOSED)
      || !self->internal_hwnd) {
    GST_ERROR_OBJECT (self, "Output window was closed");

    return GST_D3D11_WINDOW_FLOW_CLOSED;
  }

  if (self->have_swapchain1) {
    IDXGISwapChain1 *swap_chain1 = (IDXGISwapChain1 *) window->swap_chain;
    DXGI_PRESENT_PARAMETERS present_params = { 0, };

    /* the first present should not specify dirty-rect */
    if (!window->first_present && !window->emit_present) {
      present_params.DirtyRectsCount = 1;
      present_params.pDirtyRects = &window->render_rect;
    }

    hr = swap_chain1->Present1 (0, present_flags, &present_params);
  } else {
    hr = window->swap_chain->Present (0, present_flags);
  }

  if (!gst_d3d11_result (hr, window->device)) {
    GST_WARNING_OBJECT (self, "Direct3D cannot present texture, hr: 0x%x",
        (guint) hr);
  }

  return GST_FLOW_OK;
}

static void
gst_d3d11_window_win32_on_resize (GstD3D11Window * window,
    guint width, guint height)
{
  /* Set zero width and height here. dxgi will decide client area by itself */
  GST_D3D11_WINDOW_CLASS (parent_class)->on_resize (window, 0, 0);
}

static void
gst_d3d11_window_win32_update_swap_chain (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);

  if (self->internal_hwnd)
    PostMessageA (self->internal_hwnd, WM_SIZE, 0, 0);

  return;
}

static void
gst_d3d11_window_win32_change_fullscreen_mode (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);

  if (self->internal_hwnd) {
    g_atomic_int_add (&self->pending_fullscreen_count, 1);
    if (self->visible)
      PostMessageA (self->internal_hwnd, WM_GST_D3D11_FULLSCREEN, 0, 0);
  }
}

HWND
gst_d3d11_window_win32_get_internal_hwnd (GstD3D11Window * window)
{
  GstD3D11WindowWin32 *self = GST_D3D11_WINDOW_WIN32 (window);
  return self->internal_hwnd;
}

GstD3D11Window *
gst_d3d11_window_win32_new (GstD3D11Device * device, guintptr handle)
{
  GstD3D11Window *window;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);

  window = (GstD3D11Window *) g_object_new (GST_TYPE_D3D11_WINDOW_WIN32,
      "d3d11device", device, "window-handle", handle, NULL);
  if (!window->initialized) {
    gst_object_unref (window);
    return NULL;
  }

  gst_object_ref_sink (window);

  return window;
}
