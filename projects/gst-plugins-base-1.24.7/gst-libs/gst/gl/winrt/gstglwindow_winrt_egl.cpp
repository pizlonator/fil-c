/*
 * GStreamer
 * Copyright (C) 2019 Nirbheek Chauhan <nirbheek@centricular.com>
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

#include <gst/gl/gstglcontext.h>
#include <gst/gl/egl/gstglcontext_egl.h>

#include "gstglwindow_winrt_egl.h"
#include "../gstglwindow_private.h"

/* workaround for GetCurrentTime collision */
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <windows.applicationmodel.core.h>
#include <windows.graphics.display.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Graphics;

#define GST_CAT_DEFAULT gst_gl_window_debug

/* timeout to wait busy UI thread, 15 seconds */
/* XXX: If UI is not responsible in this amount of time, that means
 * there were something wrong situation at the application side.
 * Note that ANGLE uses 10 seconds timeout value, so even if a timeout happens
 * on our side, it would be a timeout condition of ANGLE as well.
 */
#define DEFAULT_ASYNC_TIMEOUT (15 * 1000)

static void gst_gl_window_winrt_egl_on_resize (GstGLWindow * window,
    guint width, guint height);

typedef enum
{
  GST_GL_WINDOW_WINRT_NATIVE_TYPE_NONE = 0,
  GST_GL_WINDOW_WINRT_NATIVE_TYPE_CORE_WINDOW,
  GST_GL_WINDOW_WINRT_NATIVE_TYPE_SWAP_CHAIN_PANEL,
} GstGLWindowWinRTNativeType;

template <typename CB>
static HRESULT
run_async (const ComPtr<Core::ICoreDispatcher> &dispatcher, DWORD timeout,
    CB &&cb)
{
  ComPtr<IAsyncAction> async_action;
  HRESULT hr;
  HRESULT async_hr;
  boolean can_now;
  DWORD wait_ret;

  hr = dispatcher->get_HasThreadAccess (&can_now);

  if (FAILED (hr))
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
        SetEvent (event.Get ());
        return S_OK;
      });

  hr = dispatcher->RunAsync (Core::CoreDispatcherPriority_Normal,
      handler.Get (), &async_action);

  if (FAILED (hr))
    return hr;

  wait_ret = WaitForSingleObjectEx (event.Get (), timeout, true);
  if (wait_ret != WAIT_OBJECT_0)
    return E_FAIL;

  return async_hr;
}

/* ICoreWindow resize event handler */
typedef ABI::Windows::Foundation::
    __FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowSizeChangedEventArgs_t
        IWindowSizeChangedEventHandler;

static float
get_logical_dpi (void)
{
  ComPtr<Display::IDisplayPropertiesStatics> properties;
  HRESULT hr;
  HStringReference str_ref =
      HStringReference (RuntimeClass_Windows_Graphics_Display_DisplayProperties);

  hr = GetActivationFactory (str_ref.Get(), properties.GetAddressOf());

  if (SUCCEEDED (hr)) {
    float dpi = 96.0f;

    hr = properties->get_LogicalDpi (&dpi);
    if (SUCCEEDED (hr))
      return dpi;
  }

  return 96.0f;
}

static inline float dip_to_pixel (float dip, float logical_dpi)
{
  /* https://docs.microsoft.com/en-us/windows/win32/learnwin32/dpi-and-device-independent-pixels */
  return dip * logical_dpi / 96.0f;
}

class CoreResizeHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          IWindowSizeChangedEventHandler>
{
public:
  CoreResizeHandler () {}
  HRESULT RuntimeClassInitialize (GstGLWindow * window)
  {
    if (!window)
      return E_INVALIDARG;

    listener_ = window;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (Core::ICoreWindow * sender, Core::IWindowSizeChangedEventArgs * args)
  {
    Size new_size;
    HRESULT hr;

    if (!listener_)
      return S_OK;

    hr = args->get_Size(&new_size);
    if (SUCCEEDED (hr)) {
      gint width, height;
      float dpi;

      dpi = get_logical_dpi ();

      width = (guint) dip_to_pixel (new_size.Width, dpi);
      height = (guint) dip_to_pixel (new_size.Height, dpi);

      gst_gl_window_winrt_egl_on_resize (listener_, width, height);
    }

    return S_OK;
  }

private:
  GstGLWindow * listener_;
};

class PanelResizeHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
        Xaml::ISizeChangedEventHandler>
{
public:
  PanelResizeHandler () {}
  HRESULT RuntimeClassInitialize (GstGLWindow * window)
  {
    if (!window)
      return E_INVALIDARG;

    listener_ = window;
    return S_OK;
  }

  IFACEMETHOD(Invoke)
  (IInspectable * sender, Xaml::ISizeChangedEventArgs * args)
  {
    Size new_size;
    HRESULT hr;

    if (!listener_)
      return S_OK;

    hr = args->get_NewSize(&new_size);
    if (SUCCEEDED(hr)) {
      gst_gl_window_winrt_egl_on_resize (listener_,
          (guint) new_size.Width, (guint) new_size.Height);
    }

    return S_OK;
  }

private:
  GstGLWindow * listener_;
};

class GstGLWindowWinRTEGLResizeHandler
{
public:
  GstGLWindowWinRTEGLResizeHandler(IInspectable * native_handle,
      GstGLWindow * listener)
    : native_type_ (GST_GL_WINDOW_WINRT_NATIVE_TYPE_NONE)
    , isValid_ (false)
  {
    ComPtr<IInspectable> window;
    HRESULT hr = E_FAIL;

    if (!native_handle) {
      GST_WARNING ("Null handler");
      return;
    }

    window = native_handle;
    if (SUCCEEDED (window.As (&core_window_))) {
      GST_INFO ("Valid ICoreWindow");
      native_type_ = GST_GL_WINDOW_WINRT_NATIVE_TYPE_CORE_WINDOW;
      core_window_->get_Dispatcher (&dispatcher_);
    } else if (SUCCEEDED (window.As (&panel_))) {
      ComPtr<Xaml::IDependencyObject> dependency_obj;

      GST_INFO ("Valid ISwapChainPanel");
      native_type_ = GST_GL_WINDOW_WINRT_NATIVE_TYPE_SWAP_CHAIN_PANEL;
      hr = panel_.As (&dependency_obj);
      if (FAILED (hr)) {
        GST_WARNING ("Couldn't get IDependencyObject interface");
        return;
      }

      dependency_obj->get_Dispatcher (&dispatcher_);
    } else {
      GST_ERROR ("Invalid window handle");
      return;
    }

    if (!dispatcher_) {
      GST_WARNING ("ICoreDispatcher is unavailable");
      return;
    }

    switch (native_type_) {
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_CORE_WINDOW:
        if (!registerSizeChangedHandlerForCoreWindow (listener)) {
          GST_WARNING
              ("Couldn't install size changed event handler for corewindow");
          return;
        }
        break;
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_SWAP_CHAIN_PANEL:
        if (!registerSizeChangedHandlerForSwapChainPanel (listener)) {
          GST_WARNING
              ("Couldn't install size changed event handler for swapchainpanel");
          return;
        }
        break;
      default:
        g_assert_not_reached ();
        return;
    }

    isValid_ = true;
  }

  ~GstGLWindowWinRTEGLResizeHandler()
  {
    if (!isValid_ || !dispatcher_)
      return;

    switch (native_type_) {
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_CORE_WINDOW:
        unregisterSizeChangedHandlerForCoreWindow ();
        break;
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_SWAP_CHAIN_PANEL:
        unregisterSizeChangedHandlerForSwapChainPanel ();
        break;
      default:
        g_assert_not_reached ();
        return;
    }
  }

  bool
  IsValid (void)
  {
    return isValid_;
  }

  bool
  GetWindowSize (guint * width, guint * height)
  {
    if (!isValid_)
      return false;

    switch (native_type_) {
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_CORE_WINDOW:
        return getWindowSizeForCoreWindow (width, height);
      case GST_GL_WINDOW_WINRT_NATIVE_TYPE_SWAP_CHAIN_PANEL:
        return getWindowSizeForSwapChainPanel (width, height);
      default:
        g_assert_not_reached ();
        break;
    }

    return false;
  }

  HRESULT
  GetHasThreadAccess (bool &has_access)
  {
    HRESULT hr;
    boolean val;

    if (!isValid_ || !dispatcher_)
      return E_FAIL;

    hr = dispatcher_->get_HasThreadAccess (&val);
    if (FAILED (hr))
      return hr;

    if (val)
      has_access = true;
    else
      has_access = false;

    return hr;
  }

private:
  bool
  registerSizeChangedHandlerForCoreWindow (GstGLWindow * window)
  {
    ComPtr<IWindowSizeChangedEventHandler> resize_handler;
    HRESULT hr;

    hr = MakeAndInitialize<CoreResizeHandler>(&resize_handler, window);
    if (FAILED (hr)) {
      GST_WARNING ("Couldn't creat resize handler object");
      return false;
    }

    hr = run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
        [this, resize_handler] {
          return core_window_->add_SizeChanged (resize_handler.Get(),
              &event_token_);
        });

    if (FAILED (hr)) {
      GST_WARNING ("Couldn't install resize handler");
      return false;
    }

    return true;
  }

  bool
  registerSizeChangedHandlerForSwapChainPanel (GstGLWindow * window)
  {
    ComPtr<Xaml::ISizeChangedEventHandler> resize_handler;
    ComPtr<Xaml::IFrameworkElement> framework;
    HRESULT hr;

    hr = MakeAndInitialize<PanelResizeHandler>(&resize_handler, window);
    if (FAILED (hr)) {
      GST_WARNING ("Couldn't creat resize handler object");
      return false;
    }

    hr = panel_.As (&framework);
    if (FAILED (hr)) {
      GST_WARNING ("Couldn't get IFrameworkElement interface");
      return false;
    }

    hr = run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
      [this, framework, resize_handler] {
        return framework->add_SizeChanged (resize_handler.Get(),
            &event_token_);
      });

    if (FAILED (hr)) {
      GST_WARNING ("Couldn't install resize handler");
      return false;
    }

    return true;
  }

  void
  unregisterSizeChangedHandlerForCoreWindow (void)
  {
    run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
        [this] {
          core_window_->remove_SizeChanged(event_token_);
          return S_OK;
        });
  }

  void
  unregisterSizeChangedHandlerForSwapChainPanel (void)
  {
    ComPtr<Xaml::IFrameworkElement> framework;
    HRESULT hr;

    hr = panel_.As (&framework);
    if (SUCCEEDED (hr)) {
      run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
          [this, framework] {
            return framework->remove_SizeChanged (event_token_);
          });
    }
  }

  bool
  getWindowSizeForCoreWindow (guint * width, guint * height)
  {
    HRESULT hr;

    hr = run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
      [this, width, height] {
        HRESULT async_hr;
        Rect bounds;

        async_hr = core_window_->get_Bounds (&bounds);
        if (SUCCEEDED (async_hr)) {
          float dpi;

          dpi = get_logical_dpi ();

          *width = (guint) dip_to_pixel (bounds.Width, dpi);
          *height = (guint) dip_to_pixel (bounds.Height, dpi);
        }

        return async_hr;
      });

    return SUCCEEDED (hr);
  }

  bool
  getWindowSizeForSwapChainPanel (guint * width, guint * height)
  {
    HRESULT hr;
    ComPtr<Xaml::IUIElement> ui;

    hr = panel_.As (&ui);
    if (FAILED (hr))
      return false;

    hr = run_async (dispatcher_, DEFAULT_ASYNC_TIMEOUT,
        [ui, width, height] {
          Size size;
          HRESULT async_hr;

          async_hr = ui->get_RenderSize (&size);
          if (SUCCEEDED (async_hr)) {
            *width = (guint) size.Width;
            *height = (guint) size.Height;
          }

          return async_hr;
        });

    return SUCCEEDED (hr);
  }

private:
  GstGLWindowWinRTNativeType native_type_;
  ComPtr<Core::ICoreDispatcher> dispatcher_;

  ComPtr<Core::ICoreWindow> core_window_;
  ComPtr<Xaml::Controls::ISwapChainPanel> panel_;

  EventRegistrationToken event_token_;
  bool isValid_;
};

struct _GstGLWindowWinRTEGLPrivate
{
  GstGLWindowWinRTEGLResizeHandler *resize_handler;
  guint surface_width;
  guint surface_height;
  GMutex event_lock;
};

#define gst_gl_window_winrt_egl_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstGLWindowWinRTEGL, gst_gl_window_winrt_egl,
    GST_TYPE_GL_WINDOW);

static void gst_gl_window_winrt_egl_dispose (GObject * object);
static void gst_gl_window_winrt_egl_finalize (GObject * object);
static guintptr gst_gl_window_winrt_egl_get_display (GstGLWindow * window);
static guintptr gst_gl_window_winrt_egl_get_window_handle (GstGLWindow *
    window);
static void gst_gl_window_winrt_egl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_winrt_egl_show (GstGLWindow * window);
static void gst_gl_window_winrt_egl_quit (GstGLWindow * window);

static void
gst_gl_window_winrt_egl_class_init (GstGLWindowWinRTEGLClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  gobject_class->dispose = gst_gl_window_winrt_egl_dispose;
  gobject_class->finalize = gst_gl_window_winrt_egl_finalize;

  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_winrt_egl_get_display);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_winrt_egl_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_winrt_egl_set_window_handle);
  window_class->show =
      GST_DEBUG_FUNCPTR (gst_gl_window_winrt_egl_show);
  window_class->quit =
      GST_DEBUG_FUNCPTR (gst_gl_window_winrt_egl_quit);
}

static void
gst_gl_window_winrt_egl_init (GstGLWindowWinRTEGL * window_winrt)
{
  window_winrt->priv = (GstGLWindowWinRTEGLPrivate *)
      gst_gl_window_winrt_egl_get_instance_private (window_winrt);

  g_mutex_init (&window_winrt->priv->event_lock);
}

static void
gst_gl_window_winrt_egl_dispose (GObject * object)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (object);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;

  if (priv->resize_handler) {
    delete priv->resize_handler;
    priv->resize_handler = nullptr;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_gl_window_winrt_egl_finalize (GObject * object)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (object);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;

  g_mutex_clear (&priv->event_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_window_winrt_egl_set_window_handle (GstGLWindow * window,
    guintptr handle)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (window);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;

  GST_INFO_OBJECT (window, "Setting WinRT EGL window handle: %p", handle);

  window_egl->window = (EGLNativeWindowType) handle;

  if (priv->resize_handler)
    delete priv->resize_handler;
  priv->resize_handler = nullptr;

  if (!handle) {
    GST_WARNING_OBJECT (window, "NULL window handle");
    return;
  }

  priv->resize_handler =
      new GstGLWindowWinRTEGLResizeHandler
      (reinterpret_cast<IInspectable*> (handle), window);

  if (!priv->resize_handler->IsValid ()) {
    GST_WARNING_OBJECT (window,
        "Invalid window handle %" G_GUINTPTR_FORMAT, handle);
    delete priv->resize_handler;
    priv->resize_handler = nullptr;
  }
}

static guintptr
gst_gl_window_winrt_egl_get_window_handle (GstGLWindow * window)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (window);

  GST_INFO_OBJECT (window, "Getting WinRT EGL window handle");

  return (guintptr) window_egl->window;
}

/* Must be called in the gl thread */
GstGLWindowWinRTEGL *
gst_gl_window_winrt_egl_new (GstGLDisplay * display)
{
  GstGLWindowWinRTEGL *window_egl;

  GST_INFO_OBJECT (display, "Trying to create WinRT EGL window");

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_EGL) == 0)
    /* we require an EGL display to create windows */
    return NULL;

  GST_INFO_OBJECT (display, "Creating WinRT EGL window");

  window_egl = (GstGLWindowWinRTEGL *)
      g_object_new (GST_TYPE_GL_WINDOW_WINRT_EGL, NULL);

  return window_egl;
}

static guintptr
gst_gl_window_winrt_egl_get_display (GstGLWindow * window)
{
  /* EGL_DEFAULT_DISPLAY */
  return 0;
}

static void
gst_gl_window_winrt_egl_show (GstGLWindow * window)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (window);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;
  guint width, height;
  gboolean resize = FALSE;

  if (!priv->resize_handler)
    return;

  g_mutex_lock (&priv->event_lock);
  if (!priv->surface_width || !priv->surface_height) {
    if (priv->resize_handler->GetWindowSize (&width, &height)) {
      GST_INFO_OBJECT (window, "Client window size %dx%d", width, height);
      priv->surface_width = width;
      priv->surface_height = height;
      resize = TRUE;
    }
  }
  g_mutex_unlock (&priv->event_lock);

  if (resize)
    gst_gl_window_resize (window, width, height);
}

static void
gst_gl_window_winrt_egl_on_resize (GstGLWindow * window,
    guint width, guint height)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (window);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;

  GST_DEBUG_OBJECT (window, "New client window size %dx%d", width, height);

  g_mutex_lock (&priv->event_lock);
  priv->surface_width = width;
  priv->surface_height = height;
  g_mutex_unlock (&priv->event_lock);

  gst_gl_window_resize (window, width, height);
}

static void
gst_gl_window_winrt_egl_quit (GstGLWindow * window)
{
  GstGLWindowWinRTEGL *window_egl = GST_GL_WINDOW_WINRT_EGL (window);
  GstGLWindowWinRTEGLPrivate *priv = window_egl->priv;

  if (priv->resize_handler) {
    HRESULT hr;
    bool is_dispatcher_thread = false;

    hr = priv->resize_handler->GetHasThreadAccess (is_dispatcher_thread);
    if (SUCCEEDED (hr) && is_dispatcher_thread) {
      /* In GstGLContext::destroy_context() -> eglDestroySurface(),
       * ANGLE will wait a UI thread for its own operations to be called
       * from the thread. Note that gst_gl_context_egl_destroy_context() will be
       * called from GstGLContext's internal GL thread.
       *
       * A problem is that if GstGLWindow is being closed from the UI thread,
       * ANGLE cannot access the UI thread as current thread is the thread.
       */
      GST_ERROR_OBJECT (window,
          "Closing from a UI thread might cause a deadlock or crash");

      g_warning ("GstGLWindowWinRTEGL should be closed from non-UI thread");
    }
  }

  GST_GL_WINDOW_CLASS (parent_class)->quit (window);
}
