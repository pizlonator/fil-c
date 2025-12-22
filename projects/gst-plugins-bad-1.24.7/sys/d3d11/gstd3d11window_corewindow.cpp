/*
 * GStreamer
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

#include "gstd3d11window_corewindow.h"

/* workaround for GetCurrentTime collision */
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <windows.applicationmodel.core.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.graphics.display.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Graphics;

typedef ABI::Windows::Foundation::
    __FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowSizeChangedEventArgs_t
        IWindowSizeChangedEventHandler;

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_window_debug);
#define GST_CAT_DEFAULT gst_d3d11_window_debug

/* timeout to wait busy UI thread */
#define DEFAULT_ASYNC_TIMEOUT (10 * 1000)

typedef struct _CoreWindowWinRTStorage
{
  ComPtr<Core::ICoreWindow> core_window;
  ComPtr<Core::ICoreDispatcher> dispatcher;
  HANDLE cancellable;
  EventRegistrationToken event_token;
} CoreWindowWinRTStorage;
/* *INDENT-ON* */

struct _GstD3D11WindowCoreWindow
{
  GstD3D11Window parent;

  CoreWindowWinRTStorage *storage;
};

#define gst_d3d11_window_core_window_parent_class parent_class
G_DEFINE_TYPE (GstD3D11WindowCoreWindow, gst_d3d11_window_core_window,
    GST_TYPE_D3D11_WINDOW);

static void gst_d3d11_window_core_window_constructed (GObject * object);
static void gst_d3d11_window_core_window_dispose (GObject * object);
static void gst_d3d11_window_core_window_update_swap_chain (GstD3D11Window *
    window);
static void gst_d3d11_window_core_window_change_fullscreen_mode (GstD3D11Window
    * window);
static gboolean gst_d3d11_window_core_window_create_swap_chain (GstD3D11Window *
    window, DXGI_FORMAT format, guint width, guint height,
    guint swapchain_flags, IDXGISwapChain ** swap_chain);
static GstFlowReturn gst_d3d11_window_core_window_present (GstD3D11Window *
    window, guint present_flags);
static gboolean gst_d3d11_window_core_window_unlock (GstD3D11Window * window);
static gboolean
gst_d3d11_window_core_window_unlock_stop (GstD3D11Window * window);
static void
gst_d3d11_window_core_window_on_resize (GstD3D11Window * window,
    guint width, guint height);
static void
gst_d3d11_window_core_window_on_resize_sync (GstD3D11Window * window);
static void gst_d3d11_window_core_window_unprepare (GstD3D11Window * window);

static float
get_logical_dpi (void)
{
  /* *INDENT-OFF* */
  ComPtr<Display::IDisplayPropertiesStatics> properties;
  /* *INDENT-ON* */
  HRESULT hr;
  HStringReference str_ref =
      HStringReference
      (RuntimeClass_Windows_Graphics_Display_DisplayProperties);

  hr = GetActivationFactory (str_ref.Get (), properties.GetAddressOf ());

  if (gst_d3d11_result (hr, NULL)) {
    float dpi = 96.0f;

    hr = properties->get_LogicalDpi (&dpi);
    if (gst_d3d11_result (hr, NULL))
      return dpi;
  }

  return 96.0f;
}

static inline float
dip_to_pixel (float dip)
{
  /* https://docs.microsoft.com/en-us/windows/win32/learnwin32/dpi-and-device-independent-pixels */
  return dip * get_logical_dpi () / 96.0f;
}

/* *INDENT-OFF* */
class CoreResizeHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
        IWindowSizeChangedEventHandler>
{
public:
  CoreResizeHandler () {}
  HRESULT RuntimeClassInitialize (GstD3D11Window * listener)
  {
    if (!listener)
      return E_INVALIDARG;

    window = listener;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (Core::ICoreWindow * sender, Core::IWindowSizeChangedEventArgs * args)
  {
    if (window) {
      Size new_size;
      HRESULT hr = args->get_Size(&new_size);
      if (gst_d3d11_result (hr, NULL)) {
        guint width, height;

        width = (guint) dip_to_pixel (new_size.Width);
        height = (guint) dip_to_pixel (new_size.Height);

        window->surface_width = width;
        window->surface_height = height;
        gst_d3d11_window_core_window_on_resize_sync (window);
      }
    }

    return S_OK;
  }

private:
  GstD3D11Window * window;
};

template <typename CB>
static HRESULT
run_async (const ComPtr<Core::ICoreDispatcher> &dispatcher, HANDLE cancellable,
    DWORD timeout, CB &&cb)
{
  ComPtr<IAsyncAction> async_action;
  HRESULT hr;
  HRESULT async_hr;
  boolean can_now;
  DWORD wait_ret;
  HANDLE event_handle[2];

  hr = dispatcher->get_HasThreadAccess (&can_now);

  if (!gst_d3d11_result (hr, NULL))
    return hr;

  if (can_now)
    return cb ();

  Event event (CreateEventEx (NULL, NULL, CREATE_EVENT_MANUAL_RESET,
      EVENT_ALL_ACCESS));

  if (!event.IsValid())
    return E_FAIL;

  auto handler =
      Callback<Implements<RuntimeClassFlags<ClassicCom>,
          Core::IDispatchedHandler, FtmBase>>([&async_hr, &cb, &event] {
        async_hr = cb ();
        SetEvent (event.Get());
        return S_OK;
      });

  hr = dispatcher->RunAsync (Core::CoreDispatcherPriority_Normal, handler.Get(),
      async_action.GetAddressOf());

  if (!gst_d3d11_result (hr, NULL))
    return hr;

  event_handle[0] = event.Get();
  event_handle[1] = cancellable;

  wait_ret = WaitForMultipleObjects (2, event_handle, FALSE, timeout);
  if (wait_ret != WAIT_OBJECT_0)
    return E_FAIL;

  return async_hr;
}

static HRESULT
get_window_size (const ComPtr<Core::ICoreDispatcher> &dispatcher,
    HANDLE cancellable,
    const ComPtr<Core::ICoreWindow> &window, Size *size)
{
  return run_async (dispatcher, cancellable, INFINITE,
      [window, size] {
        HRESULT hr;
        Rect bounds;

        hr = window->get_Bounds (&bounds);
        if (gst_d3d11_result (hr, NULL)) {
          size->Width = dip_to_pixel (bounds.Width);
          size->Height = dip_to_pixel (bounds.Height);
        }

        return hr;
      });
}
/* *INDENT-ON* */

static void
gst_d3d11_window_core_window_class_init (GstD3D11WindowCoreWindowClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11WindowClass *window_class = GST_D3D11_WINDOW_CLASS (klass);

  gobject_class->constructed = gst_d3d11_window_core_window_constructed;
  gobject_class->dispose = gst_d3d11_window_core_window_dispose;

  window_class->update_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_update_swap_chain);
  window_class->change_fullscreen_mode =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_change_fullscreen_mode);
  window_class->create_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_create_swap_chain);
  window_class->present =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_present);
  window_class->unlock =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_unlock);
  window_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_unlock_stop);
  window_class->on_resize =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_on_resize);
  window_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_core_window_unprepare);
}

static void
gst_d3d11_window_core_window_init (GstD3D11WindowCoreWindow * self)
{
  self->storage = new CoreWindowWinRTStorage;
  self->storage->cancellable = CreateEvent (NULL, TRUE, FALSE, NULL);
}

/* *INDENT-OFF* */
static void
gst_d3d11_window_core_window_constructed (GObject * object)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (object);
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (object);
  CoreWindowWinRTStorage *storage = self->storage;
  HRESULT hr;
  ComPtr<IInspectable> inspectable;
  ComPtr<IWindowSizeChangedEventHandler> resize_handler;
  ComPtr<Core::ICoreWindow> core_window;
  Size size;

  if (!window->external_handle) {
    GST_ERROR_OBJECT (self, "No external window handle");
    return;
  }

  inspectable = reinterpret_cast<IInspectable*> (window->external_handle);

  hr = inspectable.As (&storage->core_window);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = storage->core_window->get_Dispatcher (&storage->dispatcher);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = get_window_size (storage->dispatcher, storage->cancellable,
      storage->core_window, &size);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  GST_DEBUG_OBJECT (self,
      "client size %dx%d", window->surface_width, window->surface_height);
  window->surface_width = size.Width;
  window->surface_height = size.Height;

  hr = MakeAndInitialize<CoreResizeHandler>(&resize_handler, window);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = storage->core_window.As (&core_window);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = run_async (storage->dispatcher,
      storage->cancellable, DEFAULT_ASYNC_TIMEOUT,
      [self, core_window, resize_handler] {
        return core_window->add_SizeChanged (resize_handler.Get(),
            &self->storage->event_token);
      });
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  window->initialized = TRUE;
  return;

error:
  GST_ERROR_OBJECT (self, "Invalid window handle");
  return;
}
/* *INDENT-ON* */

static void
gst_d3d11_window_core_window_dispose (GObject * object)
{
  gst_d3d11_window_core_window_unprepare (GST_D3D11_WINDOW (object));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* *INDENT-OFF* */
static void
gst_d3d11_window_core_window_unprepare (GstD3D11Window * window)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  CoreWindowWinRTStorage *storage = self->storage;

  if (storage) {
    if (storage->core_window && storage->dispatcher) {
      ComPtr<Core::ICoreWindow> window;
      HRESULT hr;

      hr = storage->core_window.As (&window);
      if (SUCCEEDED (hr)) {
        run_async (storage->dispatcher,
            storage->cancellable, DEFAULT_ASYNC_TIMEOUT,
            [self, window] {
              return window->remove_SizeChanged (self->storage->event_token);
            });
      }
    }

    CloseHandle (storage->cancellable);
    delete storage;
  }

  self->storage = NULL;
}
/* *INDENT-ON* */

static IDXGISwapChain1 *
create_swap_chain_for_core_window (GstD3D11WindowCoreWindow * self,
    GstD3D11Device * device, guintptr core_window, DXGI_SWAP_CHAIN_DESC1 * desc,
    IDXGIOutput * output)
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
  hr = factory2->CreateSwapChainForCoreWindow (device_handle,
      (IUnknown *) core_window, desc, output, &swap_chain);

  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Cannot create SwapChain Object: 0x%x",
        (guint) hr);
    swap_chain = NULL;
  }

  return swap_chain;
}

static gboolean
gst_d3d11_window_core_window_create_swap_chain (GstD3D11Window * window,
    DXGI_FORMAT format, guint width, guint height, guint swapchain_flags,
    IDXGISwapChain ** swap_chain)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  /* *INDENT-OFF* */
  ComPtr<IDXGISwapChain1> new_swapchain;
  /* *INDENT-ON* */
  GstD3D11Device *device = window->device;
  DXGI_SWAP_CHAIN_DESC1 desc1 = { 0, };

  desc1.Width = width;
  desc1.Height = height;
  desc1.Format = format;
  desc1.Stereo = FALSE;
  desc1.SampleDesc.Count = 1;
  desc1.SampleDesc.Quality = 0;
  desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc1.BufferCount = 2;
  desc1.Scaling = DXGI_SCALING_NONE;
  desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  desc1.Flags = swapchain_flags;

  new_swapchain =
      create_swap_chain_for_core_window (self, device,
      window->external_handle, &desc1, NULL);

  if (!new_swapchain) {
    GST_ERROR_OBJECT (self, "Cannot create swapchain");
    return FALSE;
  }

  new_swapchain.CopyTo (swap_chain);

  return TRUE;
}

static GstFlowReturn
gst_d3d11_window_core_window_present (GstD3D11Window * window,
    guint present_flags)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  HRESULT hr;
  DXGI_PRESENT_PARAMETERS present_params = { 0, };
  IDXGISwapChain1 *swap_chain = (IDXGISwapChain1 *) window->swap_chain;

  /* the first present should not specify dirty-rect */
  if (!window->first_present && !window->emit_present) {
    present_params.DirtyRectsCount = 1;
    present_params.pDirtyRects = &window->render_rect;
  }

  hr = swap_chain->Present1 (0, present_flags, &present_params);

  if (!gst_d3d11_result (hr, window->device)) {
    GST_WARNING_OBJECT (self, "Direct3D cannot present texture, hr: 0x%x",
        (guint) hr);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_window_core_window_unlock (GstD3D11Window * window)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  CoreWindowWinRTStorage *storage = self->storage;

  SetEvent (storage->cancellable);

  return TRUE;
}

static gboolean
gst_d3d11_window_core_window_unlock_stop (GstD3D11Window * window)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  CoreWindowWinRTStorage *storage = self->storage;

  ResetEvent (storage->cancellable);

  return TRUE;
}

static void
gst_d3d11_window_core_window_update_swap_chain (GstD3D11Window * window)
{
  gst_d3d11_window_core_window_on_resize (window,
      window->surface_width, window->surface_height);

  return;
}

static void
gst_d3d11_window_core_window_change_fullscreen_mode (GstD3D11Window * window)
{
  GST_FIXME_OBJECT (window, "Implement fullscreen mode change");

  return;
}

static void
gst_d3d11_window_core_window_on_resize (GstD3D11Window * window,
    guint width, guint height)
{
  GstD3D11WindowCoreWindow *self = GST_D3D11_WINDOW_CORE_WINDOW (window);
  CoreWindowWinRTStorage *storage = self->storage;

  /* *INDENT-OFF* */
  run_async (storage->dispatcher, storage->cancellable, INFINITE,
      [window] {
        gst_d3d11_window_core_window_on_resize_sync (window);
        return S_OK;
      });
  /* *INDENT-ON* */
}

static void
gst_d3d11_window_core_window_on_resize_sync (GstD3D11Window * window)
{
  GST_LOG_OBJECT (window,
      "New size %dx%d", window->surface_width, window->surface_height);

  GST_D3D11_WINDOW_CLASS (parent_class)->on_resize (window,
      window->surface_width, window->surface_height);
}

GstD3D11Window *
gst_d3d11_window_core_window_new (GstD3D11Device * device, guintptr handle)
{
  GstD3D11Window *window;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);
  g_return_val_if_fail (handle, NULL);

  window = (GstD3D11Window *)
      g_object_new (GST_TYPE_D3D11_WINDOW_CORE_WINDOW,
      "d3d11device", device, "window-handle", handle, NULL);

  if (!window->initialized) {
    gst_object_unref (window);
    return NULL;
  }

  gst_object_ref_sink (window);

  return window;
}
