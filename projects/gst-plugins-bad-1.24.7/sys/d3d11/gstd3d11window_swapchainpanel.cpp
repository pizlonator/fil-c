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

#include "gstd3d11window_swapchainpanel.h"

/* workaround for GetCurrentTime collision */
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <windows.applicationmodel.core.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

/* *INDENT-OFF* */

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::Foundation;

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_window_debug);
#define GST_CAT_DEFAULT gst_d3d11_window_debug

/* timeout to wait busy UI thread */
#define DEFAULT_ASYNC_TIMEOUT (10 * 1000)

typedef struct _SwapChainPanelWinRTStorage
{
  ComPtr<Xaml::Controls::ISwapChainPanel> panel;
  ComPtr<Core::ICoreDispatcher> dispatcher;
  ComPtr<IDXGISwapChain1> swapchain;
  HANDLE cancellable;
  EventRegistrationToken event_token;
} SwapChainPanelWinRTStorage;
/* *INDENT-ON* */

struct _GstD3D11WindowSwapChainPanel
{
  GstD3D11Window parent;

  SwapChainPanelWinRTStorage *storage;
};

#define gst_d3d11_window_swap_chain_panel_parent_class parent_class
G_DEFINE_TYPE (GstD3D11WindowSwapChainPanel,
    gst_d3d11_window_swap_chain_panel, GST_TYPE_D3D11_WINDOW);

static void gst_d3d11_window_swap_chain_panel_constructed (GObject * object);
static void gst_d3d11_window_swap_chain_panel_dispose (GObject * object);

static void
gst_d3d11_window_swap_chain_panel_update_swap_chain (GstD3D11Window * window);
static void
gst_d3d11_window_swap_chain_panel_change_fullscreen_mode (GstD3D11Window *
    window);
static gboolean
gst_d3d11_window_swap_chain_panel_create_swap_chain (GstD3D11Window * window,
    DXGI_FORMAT format, guint width, guint height, guint swapchain_flags,
    IDXGISwapChain ** swap_chain);
static GstFlowReturn
gst_d3d11_window_swap_chain_panel_present (GstD3D11Window * window,
    guint present_flags);
static gboolean
gst_d3d11_window_swap_chain_panel_unlock (GstD3D11Window * window);
static gboolean
gst_d3d11_window_swap_chain_panel_unlock_stop (GstD3D11Window * window);
static void
gst_d3d11_window_swap_chain_panel_on_resize (GstD3D11Window * window,
    guint width, guint height);
static void
gst_d3d11_window_swap_chain_panel_on_resize_sync (GstD3D11Window * window);
static void
gst_d3d11_window_swap_chain_panel_unprepare (GstD3D11Window * window);

/* *INDENT-OFF* */
class PanelResizeHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
        Xaml::ISizeChangedEventHandler>
{
public:
  PanelResizeHandler () {}
  HRESULT RuntimeClassInitialize (GstD3D11Window * listener)
  {
    if (!listener)
      return E_INVALIDARG;

    window = listener;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IInspectable * sender, Xaml::ISizeChangedEventArgs * args)
  {
    if (window) {
      Size new_size;
      HRESULT hr = args->get_NewSize(&new_size);
      if (SUCCEEDED(hr)) {
        window->surface_width = new_size.Width;
        window->surface_height = new_size.Height;
        gst_d3d11_window_swap_chain_panel_on_resize_sync (window);
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
get_panel_size (const ComPtr<Core::ICoreDispatcher> &dispatcher,
    HANDLE cancellable,
    const ComPtr<Xaml::Controls::ISwapChainPanel> &panel, Size *size)
{
  ComPtr<Xaml::IUIElement> ui;
  HRESULT hr = panel.As (&ui);

  if (!gst_d3d11_result (hr, NULL))
    return hr;

  hr = run_async (dispatcher, cancellable, DEFAULT_ASYNC_TIMEOUT,
      [ui, size] {
        return ui->get_RenderSize (size);
      });

  return hr;
}
/* *INDENT-ON* */

static void
gst_d3d11_window_swap_chain_panel_class_init (GstD3D11WindowSwapChainPanelClass
    * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11WindowClass *window_class = GST_D3D11_WINDOW_CLASS (klass);

  gobject_class->constructed = gst_d3d11_window_swap_chain_panel_constructed;
  gobject_class->dispose = gst_d3d11_window_swap_chain_panel_dispose;

  window_class->update_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_update_swap_chain);
  window_class->change_fullscreen_mode =
      GST_DEBUG_FUNCPTR
      (gst_d3d11_window_swap_chain_panel_change_fullscreen_mode);
  window_class->create_swap_chain =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_create_swap_chain);
  window_class->present =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_present);
  window_class->unlock =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_unlock);
  window_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_unlock_stop);
  window_class->on_resize =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_on_resize);
  window_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_d3d11_window_swap_chain_panel_unprepare);
}

static void
gst_d3d11_window_swap_chain_panel_init (GstD3D11WindowSwapChainPanel * self)
{
  self->storage = new SwapChainPanelWinRTStorage;
  self->storage->cancellable = CreateEvent (NULL, TRUE, FALSE, NULL);
}

/* *INDENT-OFF* */
static void
gst_d3d11_window_swap_chain_panel_constructed (GObject * object)
{
  GstD3D11Window *window = GST_D3D11_WINDOW (object);
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (object);
  SwapChainPanelWinRTStorage *storage = self->storage;
  HRESULT hr;
  ComPtr<IInspectable> inspectable;
  ComPtr<Xaml::IDependencyObject> dependency_obj;
  Size size;
  ComPtr<Xaml::ISizeChangedEventHandler> resize_handler;
  ComPtr<Xaml::IFrameworkElement> framework;

  if (!window->external_handle) {
    GST_ERROR_OBJECT (self, "No external window handle");
    return;
  }

  inspectable = reinterpret_cast<IInspectable*> (window->external_handle);

  hr = inspectable.As (&storage->panel);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = storage->panel.As (&dependency_obj);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = dependency_obj->get_Dispatcher(storage->dispatcher.GetAddressOf());
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = get_panel_size (storage->dispatcher,
      storage->cancellable, storage->panel, &size);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  GST_DEBUG_OBJECT (self, "client size %dx%d", size.Width, size.Height);
  window->surface_width = size.Width;
  window->surface_height = size.Height;

  hr = MakeAndInitialize<PanelResizeHandler>(&resize_handler, window);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = storage->panel.As (&framework);
  if (!gst_d3d11_result (hr, NULL))
    goto error;

  hr = run_async (storage->dispatcher,
      storage->cancellable, DEFAULT_ASYNC_TIMEOUT,
      [self, framework, resize_handler] {
        return framework->add_SizeChanged (resize_handler.Get(),
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
gst_d3d11_window_swap_chain_panel_dispose (GObject * object)
{
  gst_d3d11_window_swap_chain_panel_unprepare (GST_D3D11_WINDOW (object));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* *INDENT-OFF* */
static void
gst_d3d11_window_swap_chain_panel_unprepare (GstD3D11Window * window)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
  SwapChainPanelWinRTStorage *storage = self->storage;

  if (storage) {
    if (storage->panel && storage->dispatcher) {
      ComPtr<Xaml::IFrameworkElement> framework;
      HRESULT hr;

      hr = storage->panel.As (&framework);
      if (SUCCEEDED (hr)) {
        run_async (storage->dispatcher,
            storage->cancellable, DEFAULT_ASYNC_TIMEOUT,
            [self, framework] {
              return framework->remove_SizeChanged (self->storage->event_token);
            });
      }

      CloseHandle (storage->cancellable);
    }

    delete storage;
  }

  self->storage = NULL;
}
/* *INDENT-ON* */

static IDXGISwapChain1 *
create_swap_chain_for_composition (GstD3D11WindowSwapChainPanel * self,
    GstD3D11Device * device, DXGI_SWAP_CHAIN_DESC1 * desc, IDXGIOutput * output)
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
  hr = factory2->CreateSwapChainForComposition (device_handle,
      desc, output, &swap_chain);

  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Cannot create SwapChain Object: 0x%x",
        (guint) hr);
    swap_chain = NULL;
  }

  return swap_chain;
}

/* *INDENT-OFF* */
static gboolean
gst_d3d11_window_swap_chain_panel_create_swap_chain (GstD3D11Window * window,
    DXGI_FORMAT format, guint width, guint height, guint swapchain_flags,
    IDXGISwapChain ** swap_chain)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
  SwapChainPanelWinRTStorage *storage = self->storage;
  ComPtr<IDXGISwapChain1> new_swapchain;
  ComPtr<ISwapChainPanelNative> panel_native;
  GstD3D11Device *device = window->device;
  DXGI_SWAP_CHAIN_DESC1 desc1 = { 0, };
  HRESULT hr;

  desc1.Width = width;
  desc1.Height = height;
  desc1.Format = format;
  desc1.Stereo = FALSE;
  desc1.SampleDesc.Count = 1;
  desc1.SampleDesc.Quality = 0;
  desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc1.BufferCount = 2;
  desc1.Scaling = DXGI_SCALING_STRETCH;
  desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  desc1.Flags = swapchain_flags;

  new_swapchain =
    create_swap_chain_for_composition (self, device, &desc1, NULL);

  if (!new_swapchain) {
    GST_ERROR_OBJECT (self, "Cannot create swapchain");
    return FALSE;
  }

  hr = storage->panel.As (&panel_native);
  if (!gst_d3d11_result (hr, device))
    return FALSE;

  hr = run_async (storage->dispatcher,
      storage->cancellable, INFINITE,
      [panel_native, new_swapchain] {
        return panel_native->SetSwapChain(new_swapchain.Get());
      });

  if (!gst_d3d11_result (hr, device))
    return FALSE;

  new_swapchain.CopyTo (swap_chain);

  return TRUE;
}
/* *INDENT-ON* */

static GstFlowReturn
gst_d3d11_window_swap_chain_panel_present (GstD3D11Window * window,
    guint present_flags)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
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
gst_d3d11_window_swap_chain_panel_unlock (GstD3D11Window * window)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
  SwapChainPanelWinRTStorage *storage = self->storage;

  SetEvent (storage->cancellable);

  return TRUE;
}

static gboolean
gst_d3d11_window_swap_chain_panel_unlock_stop (GstD3D11Window * window)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
  SwapChainPanelWinRTStorage *storage = self->storage;

  ResetEvent (storage->cancellable);

  return TRUE;
}

static void
gst_d3d11_window_swap_chain_panel_update_swap_chain (GstD3D11Window * window)
{
  gst_d3d11_window_swap_chain_panel_on_resize (window, window->surface_width,
      window->surface_height);

  return;
}

static void
gst_d3d11_window_swap_chain_panel_change_fullscreen_mode (GstD3D11Window *
    window)
{
  GST_FIXME_OBJECT (window, "Implement fullscreen mode change");

  return;
}

static void
gst_d3d11_window_swap_chain_panel_on_resize (GstD3D11Window * window,
    guint width, guint height)
{
  GstD3D11WindowSwapChainPanel *self =
      GST_D3D11_WINDOW_SWAP_CHAIN_PANEL (window);
  SwapChainPanelWinRTStorage *storage = self->storage;

  run_async (storage->dispatcher, storage->cancellable, INFINITE,[window] {
        gst_d3d11_window_swap_chain_panel_on_resize_sync (window);
        return S_OK;
      }
  );
}

static void
gst_d3d11_window_swap_chain_panel_on_resize_sync (GstD3D11Window * window)
{
  GST_LOG_OBJECT (window,
      "New size %dx%d", window->surface_width, window->surface_height);

  GST_D3D11_WINDOW_CLASS (parent_class)->on_resize (window,
      window->surface_width, window->surface_height);
}

GstD3D11Window *
gst_d3d11_window_swap_chain_panel_new (GstD3D11Device * device, guintptr handle)
{
  GstD3D11Window *window;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);
  g_return_val_if_fail (handle, NULL);

  window = (GstD3D11Window *)
      g_object_new (GST_TYPE_D3D11_WINDOW_SWAP_CHAIN_PANEL,
      "d3d11device", device, "window-handle", handle, NULL);
  if (!window->initialized) {
    gst_object_unref (window);
    return NULL;
  }

  gst_object_ref_sink (window);

  return window;
}
