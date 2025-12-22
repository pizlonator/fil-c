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

#ifdef WINAPI_PARTITION_APP
#undef WINAPI_PARTITION_APP
#endif

#define WINAPI_PARTITION_APP 1

#include "gstd3d11winrtcapture.h"
#include "gstd3d11pluginutils.h"
#include <gmodule.h>
#include <winstring.h>
#include <roapi.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <string.h>
#include <dwmapi.h>

#include <wrl.h>

#ifdef HAVE_WINMM
#include <mmsystem.h>
#endif

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_screen_capture_debug);
#define GST_CAT_DEFAULT gst_d3d11_screen_capture_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Graphics;
using namespace ABI::Windows::Graphics::Capture;
using namespace ABI::Windows::Graphics::DirectX;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Graphics::DirectX::Direct3D11;

static SRWLOCK capture_list_lock = SRWLOCK_INIT;
static GList *capture_list = nullptr;

#define D3D11_WINRT_CAPTURE_PROP_NAME "gst-d3d11-winrt-capture"
#define WM_GST_D3D11_WINRT_CAPTURE_CLOSED (WM_USER + 1)

typedef struct
{
  gboolean loaded;

  /* d3d11.dll */
  HRESULT (WINAPI * CreateDirect3D11DeviceFromDXGIDevice) (IDXGIDevice *
      dxgi_device, IInspectable ** graphics_device);

  /* combase.dll */
  HRESULT (WINAPI * RoInitialize) (RO_INIT_TYPE init_type);
  HRESULT (WINAPI * RoUninitialize) (void);
  HRESULT (WINAPI * WindowsCreateString) (PCNZWCH source_string,
      UINT32 length, HSTRING * string);
  HRESULT (WINAPI * WindowsDeleteString) (HSTRING string);
  HRESULT (WINAPI * RoGetActivationFactory) (HSTRING activatable_class_id,
      REFIID iid, void ** factory);
  DPI_AWARENESS_CONTEXT (WINAPI * SetThreadDpiAwarenessContext) (DPI_AWARENESS_CONTEXT context);
} GstD3D11WinRTVTable;

static GstD3D11WinRTVTable winrt_vtable = { FALSE, };

template < typename InterfaceType, PCNZWCH runtime_class_id >
static HRESULT
GstGetActivationFactory (InterfaceType ** factory)
{
  if (!gst_d3d11_winrt_capture_load_library ())
    return E_NOINTERFACE;

  HSTRING class_id_hstring;
  HRESULT hr = winrt_vtable.WindowsCreateString (runtime_class_id,
      wcslen (runtime_class_id), &class_id_hstring);

  if (FAILED (hr))
    return hr;

  hr = winrt_vtable.RoGetActivationFactory (class_id_hstring,
      IID_PPV_ARGS (factory));

  if (FAILED (hr)) {
    winrt_vtable.WindowsDeleteString (class_id_hstring);
    return hr;
  }

  return winrt_vtable.WindowsDeleteString (class_id_hstring);
}

#define CLOSE_COM(obj) G_STMT_START { \
  if (obj) { \
    ComPtr<IClosable> closable; \
    obj.As (&closable); \
    if (closable) \
      closable->Close (); \
    obj = nullptr; \
  } \
} G_STMT_END

struct GstD3D11WinRTCaptureInner
{
  ~GstD3D11WinRTCaptureInner()
  {
    CLOSE_COM (session);
    CLOSE_COM (pool);
    CLOSE_COM (item);
    CLOSE_COM (d3d_device);
  }

  STDMETHODIMP
  OnClosed (IGraphicsCaptureItem * item, IInspectable * args)
  {
    GST_WARNING ("Item %p got closed", this);
    this->closed = true;

    return S_OK;
  }

  ComPtr < IDirect3DDevice > d3d_device;
  ComPtr < IGraphicsCaptureItem > item;
  ComPtr < IDirect3D11CaptureFramePool > pool;
  ComPtr < IGraphicsCaptureSession > session;

  bool closed = false;
};
/* *INDENT-ON* */

#define LOAD_SYMBOL(module,name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &winrt_vtable.func)) { \
    GST_WARNING ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    if (d3d11_module) \
      g_module_close (d3d11_module); \
    if (combase_module) \
      g_module_close (combase_module); \
    if (user32_module) \
      g_module_close (user32_module); \
    return; \
  } \
} G_STMT_END

gboolean
gst_d3d11_winrt_capture_load_library (void)
{
  static GModule *d3d11_module = nullptr;
  static GModule *combase_module = nullptr;
  static GModule *user32_module = nullptr;

  GST_D3D11_CALL_ONCE_BEGIN {
    d3d11_module = g_module_open ("d3d11.dll", G_MODULE_BIND_LAZY);
    /* Shouldn't happen... */
    if (!d3d11_module)
      return;

    combase_module = g_module_open ("combase.dll", G_MODULE_BIND_LAZY);
    if (!combase_module) {
      g_module_close (d3d11_module);
      return;
    }

    user32_module = g_module_open ("user32.dll", G_MODULE_BIND_LAZY);
    if (!user32_module) {
      g_module_close (combase_module);
      g_module_close (d3d11_module);
      return;
    }

    LOAD_SYMBOL (d3d11_module, CreateDirect3D11DeviceFromDXGIDevice,
        CreateDirect3D11DeviceFromDXGIDevice);
    LOAD_SYMBOL (combase_module, RoInitialize, RoInitialize);
    LOAD_SYMBOL (combase_module, RoUninitialize, RoUninitialize);
    LOAD_SYMBOL (combase_module, WindowsCreateString, WindowsCreateString);
    LOAD_SYMBOL (combase_module, WindowsDeleteString, WindowsDeleteString);
    LOAD_SYMBOL (combase_module, RoGetActivationFactory,
        RoGetActivationFactory);
    LOAD_SYMBOL (user32_module, SetThreadDpiAwarenessContext,
        SetThreadDpiAwarenessContext);

    winrt_vtable.loaded = TRUE;
  }
  GST_D3D11_CALL_ONCE_END;

  return winrt_vtable.loaded;
}

enum
{
  PROP_0,
  PROP_D3D11_DEVICE,
  PROP_MONITOR_HANDLE,
  PROP_WINDOW_HANDLE,
  PROP_CLIENT_ONLY,
};

struct _GstD3D11WinRTCapture
{
  GstD3D11ScreenCapture parent;

  GstD3D11Device *device;
  GstD3D11WinRTCaptureInner *inner;
  /* Reported by WGC API */
  SizeInt32 pool_size;
  /* Actual texture resolution */
  UINT width;
  UINT height;
  UINT capture_width;
  UINT capture_height;

  gboolean flushing;
  boolean show_mouse;
  boolean show_border;

  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;

  CRITICAL_SECTION lock;
  CONDITION_VARIABLE cond;
  LARGE_INTEGER frequency;

  HMONITOR monitor_handle;
  HWND window_handle;
  gboolean client_only;

  HWND hidden_window;
};

static void gst_d3d11_winrt_capture_constructed (GObject * object);
static void gst_d3d11_winrt_capture_dispose (GObject * object);
static void gst_d3d11_winrt_capture_finalize (GObject * object);
static void gst_d3d11_winrt_capture_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static GstFlowReturn
gst_d3d11_winrt_capture_prepare (GstD3D11ScreenCapture * capture);
static gboolean
gst_d3d11_winrt_capture_get_size (GstD3D11ScreenCapture * capture,
    guint * width, guint * height);
static gboolean
gst_d3d11_winrt_capture_unlock (GstD3D11ScreenCapture * capture);
static gboolean
gst_d3d11_winrt_capture_unlock_stop (GstD3D11ScreenCapture * capture);
static void
gst_d3d11_winrt_capture_show_border (GstD3D11ScreenCapture * capture,
    gboolean show);
static GstFlowReturn
gst_d3d11_winrt_capture_do_capture (GstD3D11ScreenCapture * capture,
    GstD3D11Device * device, ID3D11Texture2D * texture,
    ID3D11RenderTargetView * rtv, ShaderResource * resource,
    D3D11_BOX * crop_box, gboolean draw_mouse);
static gpointer
gst_d3d11_winrt_capture_thread_func (GstD3D11WinRTCapture * self);

#define gst_d3d11_winrt_capture_parent_class parent_class
G_DEFINE_TYPE (GstD3D11WinRTCapture, gst_d3d11_winrt_capture,
    GST_TYPE_D3D11_SCREEN_CAPTURE);

static void
gst_d3d11_winrt_capture_class_init (GstD3D11WinRTCaptureClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11ScreenCaptureClass *capture_class =
      GST_D3D11_SCREEN_CAPTURE_CLASS (klass);

  gobject_class->constructed = gst_d3d11_winrt_capture_constructed;
  gobject_class->dispose = gst_d3d11_winrt_capture_dispose;
  gobject_class->finalize = gst_d3d11_winrt_capture_finalize;
  gobject_class->set_property = gst_d3d11_winrt_capture_set_property;

  g_object_class_install_property (gobject_class, PROP_D3D11_DEVICE,
      g_param_spec_object ("d3d11device", "D3D11 Device",
          "GstD3D11Device object for operating",
          GST_TYPE_D3D11_DEVICE, (GParamFlags)
          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MONITOR_HANDLE,
      g_param_spec_pointer ("monitor-handle", "Monitor Handle",
          "A HMONITOR handle of monitor to capture", (GParamFlags)
          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_WINDOW_HANDLE,
      g_param_spec_pointer ("window-handle", "Window Handle",
          "A HWND handle of window to capture", (GParamFlags)
          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CLIENT_ONLY,
      g_param_spec_boolean ("client-only",
          "Client Only", "Captures only client area", FALSE,
          (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));

  capture_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_prepare);
  capture_class->get_size =
      GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_get_size);
  capture_class->unlock = GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_unlock);
  capture_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_unlock_stop);
  capture_class->show_border =
      GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_show_border);
  capture_class->do_capture =
      GST_DEBUG_FUNCPTR (gst_d3d11_winrt_capture_do_capture);
}

static void
gst_d3d11_winrt_capture_init (GstD3D11WinRTCapture * self)
{
  InitializeCriticalSection (&self->lock);
}

static void
gst_d3d11_winrt_capture_constructed (GObject * object)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (object);
  GstD3D11CSLockGuard lk (&self->lock);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
  self->thread = g_thread_new ("GstD3D11WinRTCapture",
      (GThreadFunc) gst_d3d11_winrt_capture_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    SleepConditionVariableCS (&self->cond, &self->lock, INFINITE);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_d3d11_winrt_capture_dispose (GObject * object)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (object);

  if (self->loop)
    g_main_loop_quit (self->loop);

  g_clear_pointer (&self->thread, g_thread_join);
  g_clear_pointer (&self->loop, g_main_loop_unref);
  g_clear_pointer (&self->context, g_main_context_unref);

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_winrt_capture_finalize (GObject * object)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (object);

  DeleteCriticalSection (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_winrt_capture_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (object);

  switch (prop_id) {
    case PROP_D3D11_DEVICE:
      self->device = (GstD3D11Device *) g_value_dup_object (value);
      break;
    case PROP_MONITOR_HANDLE:
      self->monitor_handle = (HMONITOR) g_value_get_pointer (value);
      break;
    case PROP_WINDOW_HANDLE:
      self->window_handle = (HWND) g_value_get_pointer (value);
      break;
    case PROP_CLIENT_ONLY:
      self->client_only = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_d3d11_winrt_capture_running_cb (GstD3D11WinRTCapture * self)
{
  GstD3D11CSLockGuard lk (&self->lock);
  WakeAllConditionVariable (&self->cond);

  return G_SOURCE_REMOVE;
}

static void
gst_d3d11_winrt_configure (GstD3D11WinRTCapture * self)
{
  HRESULT hr;
  GstD3D11Device *device = self->device;
  ComPtr < ID3D10Multithread > multi_thread;
  ComPtr < IGraphicsCaptureItemInterop > interop;
  ID3D11Device *device_handle;
  ComPtr < IDXGIDevice > dxgi_device;
  ComPtr < IInspectable > inspectable;
  ComPtr < IDirect3D11CaptureFramePoolStatics > pool_statics;
  ComPtr < IDirect3D11CaptureFramePoolStatics2 > pool_statics2;
  ComPtr < IGraphicsCaptureSession2 > session2;
  ComPtr < IGraphicsCaptureSession3 > session3;
  GstD3D11WinRTCaptureInner *inner = nullptr;

  device_handle = gst_d3d11_device_get_device_handle (device);
  hr = device_handle->QueryInterface (IID_PPV_ARGS (&multi_thread));
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "ID3D10Multithread interface is unavailable");
    return;
  }

  multi_thread->SetMultithreadProtected (TRUE);

  hr = GstGetActivationFactory < IGraphicsCaptureItemInterop,
      RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem > (&interop);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "IGraphicsCaptureItemInterop is not available");
    return;
  }

  inner = new GstD3D11WinRTCaptureInner ();

  if (self->monitor_handle) {
    hr = interop->CreateForMonitor (self->monitor_handle,
        IID_PPV_ARGS (&inner->item));
  } else if (self->window_handle) {
    hr = interop->CreateForWindow (self->window_handle,
        IID_PPV_ARGS (&inner->item));
  } else {
    g_assert_not_reached ();
    goto error;
  }

  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Could not create item");
    goto error;
  }

  hr = device_handle->QueryInterface (IID_PPV_ARGS (&dxgi_device));
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "IDXGIDevice is not available");
    goto error;
  }

  hr = winrt_vtable.CreateDirect3D11DeviceFromDXGIDevice (dxgi_device.Get (),
      &inspectable);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "CreateDirect3D11DeviceFromDXGIDevice failed");
    goto error;
  }

  hr = inspectable.As (&inner->d3d_device);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (device, "IDirect3DDevice is not available");
    goto error;
  }

  hr = GstGetActivationFactory < IDirect3D11CaptureFramePoolStatics,
      RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool >
      (&pool_statics);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "IDirect3D11CaptureFramePoolStatics is not available");
    goto error;
  }

  hr = pool_statics.As (&pool_statics2);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self,
        "IDirect3D11CaptureFramePoolStatics2 is not available");
    goto error;
  }

  hr = inner->item->get_Size (&self->pool_size);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not get item size");
    goto error;
  }

  self->width = self->capture_width = (UINT) self->pool_size.Width;
  self->height = self->capture_height = (UINT) self->pool_size.Height;

  if (self->window_handle && self->client_only) {
    RECT rect;
    if (!GetClientRect (self->window_handle, &rect)) {
      GST_ERROR_OBJECT (self, "Could not get client rect");
      goto error;
    }

    self->capture_width = rect.right - rect.left;
    self->capture_height = rect.bottom - rect.top;

    self->capture_width = MAX (self->capture_width, 1);
    self->capture_height = MAX (self->capture_height, 1);

    GST_DEBUG_OBJECT (self, "Client rect %d:%d:%d:%d, pool size %dx%d",
        rect.left, rect.top, rect.right, rect.bottom,
        self->width, self->height);
  }

  hr = pool_statics2->CreateFreeThreaded (inner->d3d_device.Get (),
      DirectXPixelFormat::DirectXPixelFormat_B8G8R8A8UIntNormalized,
      1, self->pool_size, &inner->pool);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not setup pool");
    goto error;
  }

  hr = inner->pool->CreateCaptureSession (inner->item.Get (), &inner->session);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not create session");
    goto error;
  }

  inner->session.As (&session2);
  if (session2)
    session2->put_IsCursorCaptureEnabled (FALSE);

  inner->session.As (&session3);
  if (session3)
    session3->put_IsBorderRequired (self->show_border);

  hr = inner->session->StartCapture ();
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not start capture");
    goto error;
  }

  self->inner = inner;

  return;

error:
  if (inner)
    delete inner;
}

static LRESULT CALLBACK
gst_d3d11_winrt_capture_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  GstD3D11WinRTCapture *self;

  if (msg == WM_CREATE) {
    self = GST_D3D11_WINRT_CAPTURE (((LPCREATESTRUCTA) lparam)->lpCreateParams);

    SetPropA (hwnd, D3D11_WINRT_CAPTURE_PROP_NAME, self);
  } else if (GetPropA (hwnd, D3D11_WINRT_CAPTURE_PROP_NAME) &&
      msg == WM_GST_D3D11_WINRT_CAPTURE_CLOSED) {
    HANDLE handle = GetPropA (hwnd, D3D11_WINRT_CAPTURE_PROP_NAME);

    if (!GST_IS_D3D11_WINRT_CAPTURE (handle)) {
      GST_WARNING ("%p is not d3d11window object", handle);
      return DefWindowProcA (hwnd, msg, wparam, lparam);
    }

    self = GST_D3D11_WINRT_CAPTURE (handle);
    GST_INFO_OBJECT (self, "Target window got closed");
    GstD3D11CSLockGuard lk (&self->lock);
    if (self->inner)
      self->inner->closed = true;
    WakeAllConditionVariable (&self->cond);

    return 0;
  }

  return DefWindowProcA (hwnd, msg, wparam, lparam);
}

static HWND
gst_d3d11_winrt_create_hidden_window (GstD3D11WinRTCapture * self)
{
  static SRWLOCK lock = SRWLOCK_INIT;
  WNDCLASSEXA wc;
  ATOM atom;
  HINSTANCE inst = GetModuleHandle (nullptr);

  AcquireSRWLockExclusive (&lock);
  atom = GetClassInfoExA (inst, "GstD3D11WinRTCapture", &wc);
  if (atom == 0) {
    ZeroMemory (&wc, sizeof (WNDCLASSEXA));

    wc.cbSize = sizeof (WNDCLASSEXA);
    wc.lpfnWndProc = gst_d3d11_winrt_capture_proc;
    wc.hInstance = inst;
    wc.style = CS_OWNDC;
    wc.lpszClassName = "GstD3D11WinRTCapture";

    atom = RegisterClassExA (&wc);
    ReleaseSRWLockExclusive (&lock);

    if (atom == 0) {
      GST_ERROR_OBJECT (self, "Failed to register window class 0x%x",
          (guint) GetLastError ());
      return nullptr;
    }
  } else {
    ReleaseSRWLockExclusive (&lock);
  }

  return CreateWindowExA (0, "GstD3D11WinRTCapture", "GstD3D11WinRTCapture",
      WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, inst, self);
}

static void CALLBACK
event_hook_func (HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG id_obj,
    LONG id_child, DWORD id_event_thread, DWORD event_time)
{
  if (event != EVENT_OBJECT_DESTROY || id_obj != OBJID_WINDOW ||
      id_child != INDEXID_CONTAINER || !hwnd) {
    return;
  }

  GstD3D11SRWLockGuard lk (&capture_list_lock);
  GList *iter;

  for (iter = capture_list; iter; iter = g_list_next (iter)) {
    GstD3D11WinRTCapture *capture = GST_D3D11_WINRT_CAPTURE (iter->data);
    GstD3D11CSLockGuard capture_lk (&capture->lock);

    if (capture->hidden_window && capture->window_handle == hwnd) {
      PostMessageA (capture->hidden_window, WM_GST_D3D11_WINRT_CAPTURE_CLOSED,
          0, 0);
      return;
    }
  }
}

static gboolean
gst_d3d11_winrt_capture_msg_cb (GIOChannel * source, GIOCondition condition,
    gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

static void
gst_d3d11_winrt_capture_weak_ref_notify (gpointer data,
    GstD3D11WinRTCapture * self)
{
  GstD3D11SRWLockGuard lk (&capture_list_lock);
  capture_list = g_list_remove (capture_list, self);
}

static gpointer
gst_d3d11_winrt_capture_thread_func (GstD3D11WinRTCapture * self)
{
  GSource *source;
  GSource *msg_source = nullptr;
  GIOChannel *msg_io_channel = nullptr;
  HWINEVENTHOOK hook = nullptr;
#if HAVE_WINMM
  TIMECAPS time_caps;
  guint timer_res = 0;

  if (timeGetDevCaps (&time_caps, sizeof (TIMECAPS)) == TIMERR_NOERROR) {
    guint resolution;

    resolution = MIN (MAX (time_caps.wPeriodMin, 1), time_caps.wPeriodMax);

    if (timeBeginPeriod (resolution) != TIMERR_NOERROR)
      timer_res = resolution;
  }
#endif

  winrt_vtable.SetThreadDpiAwarenessContext
      (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  QueryPerformanceFrequency (&self->frequency);

  winrt_vtable.RoInitialize (RO_INIT_MULTITHREADED);
  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gst_d3d11_winrt_capture_running_cb, self, nullptr);
  g_source_attach (source, self->context);
  g_source_unref (source);

  gst_d3d11_winrt_configure (self);
  if (self->inner && self->window_handle) {
    /* hold list of capture objects to send target window closed event */
    AcquireSRWLockExclusive (&capture_list_lock);
    g_object_weak_ref (G_OBJECT (self),
        (GWeakNotify) gst_d3d11_winrt_capture_weak_ref_notify, nullptr);
    capture_list = g_list_append (capture_list, self);
    ReleaseSRWLockExclusive (&capture_list_lock);

    self->hidden_window = gst_d3d11_winrt_create_hidden_window (self);
    if (self->hidden_window) {
      DWORD process_id, thread_id;

      thread_id = GetWindowThreadProcessId (self->window_handle, &process_id);
      if (thread_id) {
        hook = SetWinEventHook (EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
            nullptr, event_hook_func, process_id, thread_id,
            WINEVENT_OUTOFCONTEXT);
      }

      msg_io_channel =
          g_io_channel_win32_new_messages ((guintptr) self->hidden_window);
      msg_source = g_io_create_watch (msg_io_channel, G_IO_IN);
      g_source_set_callback (msg_source,
          (GSourceFunc) gst_d3d11_winrt_capture_msg_cb, self, nullptr);
      g_source_attach (msg_source, self->context);
    }
  }

  g_main_loop_run (self->loop);

  if (hook)
    UnhookWinEvent (hook);

  EnterCriticalSection (&self->lock);
  if (self->hidden_window) {
    RemovePropA (self->hidden_window, D3D11_WINRT_CAPTURE_PROP_NAME);
    DestroyWindow (self->hidden_window);
    self->hidden_window = nullptr;
  }
  LeaveCriticalSection (&self->lock);

  if (msg_source) {
    g_source_destroy (msg_source);
    g_source_unref (msg_source);
  }

  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);

  if (self->inner)
    delete self->inner;
  self->inner = nullptr;

  g_main_context_pop_thread_default (self->context);
  winrt_vtable.RoUninitialize ();

#if HAVE_WINMM
  if (timer_res != 0)
    timeEndPeriod (timer_res);
#endif

  return nullptr;
}

static GstFlowReturn
gst_d3d11_winrt_capture_prepare (GstD3D11ScreenCapture * capture)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);

  g_assert (self->inner != nullptr);

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_winrt_capture_get_size (GstD3D11ScreenCapture * capture,
    guint * width, guint * height)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);

  *width = self->capture_width;
  *height = self->capture_height;

  return TRUE;
}

static gboolean
gst_d3d11_winrt_capture_unlock (GstD3D11ScreenCapture * capture)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);
  GstD3D11CSLockGuard lk (&self->lock);

  self->flushing = TRUE;
  WakeAllConditionVariable (&self->cond);

  return TRUE;
}

static gboolean
gst_d3d11_winrt_capture_unlock_stop (GstD3D11ScreenCapture * capture)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);
  GstD3D11CSLockGuard lk (&self->lock);

  self->flushing = FALSE;
  WakeAllConditionVariable (&self->cond);

  return TRUE;
}

static void
gst_d3d11_winrt_capture_show_border (GstD3D11ScreenCapture * capture,
    gboolean show)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);
  GstD3D11CSLockGuard lk (&self->lock);

  self->show_border = show;
  if (self->inner->session) {
    ComPtr < IGraphicsCaptureSession3 > session3;
    self->inner->session.As (&session3);

    if (session3)
      session3->put_IsBorderRequired (self->show_border);
  }
}

static GstFlowReturn
gst_d3d11_winrt_capture_do_capture (GstD3D11ScreenCapture * capture,
    GstD3D11Device * device, ID3D11Texture2D * texture,
    ID3D11RenderTargetView * rtv, ShaderResource * resource,
    D3D11_BOX * crop_box, gboolean draw_mouse)
{
  GstD3D11WinRTCapture *self = GST_D3D11_WINRT_CAPTURE (capture);
  GstD3D11WinRTCaptureInner *inner = self->inner;
  ComPtr < IDirect3D11CaptureFrame > frame;
  ComPtr < IDirect3DSurface > surface;
  ComPtr < IDirect3DDxgiInterfaceAccess > access;
  ComPtr < ID3D11Texture2D > captured_texture;
  ID3D11DeviceContext *context_handle;
  SizeInt32 size;
  HRESULT hr;
  LARGE_INTEGER now;
  LONGLONG timeout;
  D3D11_TEXTURE2D_DESC desc;
  gboolean size_changed = FALSE;
  D3D11_BOX box = *crop_box;

  GstD3D11CSLockGuard lk (&self->lock);
again:
  frame = nullptr;
  surface = nullptr;
  access = nullptr;
  captured_texture = nullptr;

  if (inner->closed) {
    GST_ERROR_OBJECT (self, "Item was closed");
    return GST_FLOW_ERROR;
  }

  if (self->flushing) {
    GST_INFO_OBJECT (self, "We are flushing");
    return GST_FLOW_FLUSHING;
  }

  if ((draw_mouse && !self->show_mouse) || (!draw_mouse && self->show_mouse)) {
    ComPtr < IGraphicsCaptureSession2 > session2;
    self->show_mouse = draw_mouse;

    inner->session.As (&session2);
    if (session2) {
      hr = session2->put_IsCursorCaptureEnabled (draw_mouse);
      if (!gst_d3d11_result (hr, self->device))
        GST_DEBUG_OBJECT (self, "Could not set IsCursorCaptureEnabled");
    } else {
      GST_LOG_OBJECT (self, "IGraphicsCaptureSession2 is not available");
    }
  }

  /* Magic number 5 sec timeout */
  QueryPerformanceCounter (&now);
  timeout = now.QuadPart + 5 * self->frequency.QuadPart;

  do {
    hr = inner->pool->TryGetNextFrame (&frame);
    if (frame)
      break;

    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Could not capture frame");
      return GST_FLOW_ERROR;
    }

    SleepConditionVariableCS (&self->cond, &self->lock, 1);
    QueryPerformanceCounter (&now);
  } while (!inner->closed && !self->flushing && now.QuadPart < timeout);

  if (self->flushing) {
    GST_INFO_OBJECT (self, "We are flushing");
    return GST_FLOW_FLUSHING;
  }

  if (inner->closed) {
    GST_WARNING_OBJECT (self, "Capture item was closed");
    return GST_FLOW_ERROR;
  }

  if (!frame) {
    GST_WARNING_OBJECT (self, "No frame available");
    return GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR;
  }

  hr = frame->get_ContentSize (&size);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not get content size");
    return GST_FLOW_ERROR;
  }

  if (size.Width != self->pool_size.Width ||
      size.Height != self->pool_size.Height) {
    GST_DEBUG_OBJECT (self, "Size changed %dx%d -> %dx%d",
        self->pool_size.Width, self->pool_size.Height, size.Width, size.Height);
    self->pool_size = size;
    frame = nullptr;
    hr = inner->pool->Recreate (inner->d3d_device.Get (),
        DirectXPixelFormat::DirectXPixelFormat_B8G8R8A8UIntNormalized,
        1, self->pool_size);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Could not recreate");
      return GST_FLOW_ERROR;
    }

    size_changed = TRUE;
    goto again;
  }

  hr = frame->get_Surface (&surface);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not get IDirect3DSurface");
    return GST_FLOW_ERROR;
  }

  hr = surface.As (&access);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not get IDirect3DDxgiInterfaceAccess");
    return GST_FLOW_ERROR;
  }

  hr = access->GetInterface (IID_PPV_ARGS (&captured_texture));
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Could not get texture from frame");
    return GST_FLOW_ERROR;
  }

  /* XXX: actual texture size can be different from reported pool size */
  captured_texture->GetDesc (&desc);
  if (desc.Width != self->width || desc.Height != self->height) {
    GST_DEBUG_OBJECT (self, "Texture size changed %dx%d -> %dx%d",
        self->width, self->height, desc.Width, desc.Height);
    self->width = desc.Width;
    self->height = desc.Height;
    if (!self->window_handle || !self->client_only) {
      self->capture_width = self->width;
      self->capture_height = self->capture_height;
    }

    size_changed = TRUE;
  }

  if (self->window_handle && self->client_only) {
    RECT client_rect, bound_rect;
    POINT client_pos = { 0, };
    UINT width, height;
    DPI_AWARENESS_CONTEXT prev;
    BOOL ret;

    prev = winrt_vtable.SetThreadDpiAwarenessContext
        (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    ret = GetClientRect (self->window_handle, &client_rect) &&
        DwmGetWindowAttribute (self->window_handle,
        DWMWA_EXTENDED_FRAME_BOUNDS, &bound_rect, sizeof (RECT)) == S_OK &&
        ClientToScreen (self->window_handle, &client_pos);

    if (prev)
      winrt_vtable.SetThreadDpiAwarenessContext (prev);

    if (!ret) {
      GST_ERROR_OBJECT (self, "Could not get client rect");
      return GST_FLOW_ERROR;
    }

    width = client_rect.right - client_rect.left;
    height = client_rect.bottom - client_rect.top;

    width = MAX (width, 1);
    height = MAX (height, 1);

    if (self->capture_width != width || self->capture_height != height) {
      GST_DEBUG_OBJECT (self, "Client rect size changed %dx%d -> %dx%d",
          self->capture_width, self->capture_height, width, height);
      self->capture_width = width;
      self->capture_height = height;
      size_changed = TRUE;
    } else {
      UINT x_offset = 0;
      UINT y_offset = 0;

      GST_LOG_OBJECT (self, "bound-rect: %d:%d:%d:%d, "
          "client-rect: %d:%d:%d:%d, client-upper-left: %d:%d",
          bound_rect.left, bound_rect.top, bound_rect.right, bound_rect.bottom,
          client_rect.left, client_rect.top, client_rect.right,
          client_rect.bottom, client_pos.x, client_pos.y);

      if (client_pos.x > bound_rect.left)
        x_offset = client_pos.x - bound_rect.left;

      if (client_pos.y > bound_rect.top)
        y_offset = client_pos.y - bound_rect.top;

      box.left += x_offset;
      box.top += y_offset;

      box.right += x_offset;
      box.bottom += y_offset;

      /* left and top is inclusive */
      box.left = MIN (desc.Width - 1, box.left);
      box.top = MIN (desc.Height - 1, box.top);

      box.right = MIN (desc.Width, box.right);
      box.bottom = MIN (desc.Height, box.bottom);
    }
  }

  if (size_changed)
    return GST_D3D11_SCREEN_CAPTURE_FLOW_SIZE_CHANGED;

  context_handle = gst_d3d11_device_get_device_context_handle (self->device);
  GstD3D11DeviceLockGuard device_lk (self->device);
  context_handle->CopySubresourceRegion (texture, 0, 0, 0, 0,
      captured_texture.Get (), 0, &box);

  return GST_FLOW_OK;
}

GstD3D11ScreenCapture *
gst_d3d11_winrt_capture_new (GstD3D11Device * device, HMONITOR monitor_handle,
    HWND window_handle, gboolean client_only)
{
  GstD3D11WinRTCapture *self;

  if (window_handle && !IsWindow (window_handle)) {
    GST_WARNING_OBJECT (device, "Not a valid window handle");
    return nullptr;
  }

  if (!gst_d3d11_winrt_capture_load_library ())
    return nullptr;

  self = (GstD3D11WinRTCapture *) g_object_new (GST_TYPE_D3D11_WINRT_CAPTURE,
      "d3d11device", device, "monitor-handle", (gpointer) monitor_handle,
      "window-handle", (gpointer) window_handle, "client-only", client_only,
      nullptr);

  if (!self->inner) {
    gst_clear_object (&self);
    return nullptr;
  }

  gst_object_ref_sink (self);

  return GST_D3D11_SCREEN_CAPTURE_CAST (self);
}
