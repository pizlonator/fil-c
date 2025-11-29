/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2011 Hans Breuer
 * Copyright (C) 2007-2009 Cody Russell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <stdlib.h>

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-win32.h"
#include "gdkenumtypes.h"
#include "gdkwin32.h"
#include "gdkdisplayprivate.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkmonitorprivate.h"
#include "gdkpopupprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkwin32surface.h"
#include "gdkwin32cursor.h"
#include "gdkinput-dmanipulation.h"
#include "gdkinput-winpointer.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"
#include "gdkdevice-win32.h"
#include "gdkcairocontext-win32.h"
#include "gdkmonitor-win32.h"

#include <cairo-win32.h>
#include <dwmapi.h>
#include <math.h>

static void gdk_surface_win32_finalize (GObject *object);
static void compute_toplevel_size      (GdkSurface *surface,
                                        gboolean    update_geometry,
                                        int        *width,
                                        int        *height);
static void gdk_win32_toplevel_state_callback (GdkSurface *surface);

static gpointer parent_class = NULL;
static GSList *modal_window_stack = NULL;

typedef struct _FullscreenInfo FullscreenInfo;

struct _FullscreenInfo
{
  RECT  r;
  guint hint_flags;
  LONG  style;
};

struct _AeroSnapEdgeRegion
{
  /* The rectangle along the edge of the desktop
   * that allows application of the snap transformation.
   */
  GdkRectangle edge;

  /* A subregion of the "edge". When the pointer hits
   * this region, the transformation is revealed.
   * Usually it is 1-pixel thick and is located at the
   * very edge of the screen. When there's a toolbar
   * at that edge, the "trigger" and the "edge" regions
   * are extended to cover that toolbar.
   */
  GdkRectangle trigger;
};

typedef struct _AeroSnapEdgeRegion AeroSnapEdgeRegion;

/* Size of the regions at the edges of the desktop where
 * snapping can take place (in pixels)
 */
#define AEROSNAP_REGION_THICKNESS (20)
/* Size of the subregions that actually trigger the snapping prompt
 * (in pixels).
 */
#define AEROSNAP_REGION_TRIGGER_THICKNESS (1)

/* The gap between the snap indicator and the edge of the work area
 * (in pixels).
 */
#define AEROSNAP_INDICATOR_EDGE_GAP (10)

/* Width of the outline of the snap indicator
 * (in pixels).
 */
#define AEROSNAP_INDICATOR_LINE_WIDTH (3.0)

/* Corner radius of the snap indicator.
 */
#define AEROSNAP_INDICATOR_CORNER_RADIUS (3.0)

/* The time it takes for snap indicator to expand/shrink
 * from current window size to future position of the
 * snapped window (in microseconds).
 */
#define AEROSNAP_INDICATOR_ANIMATION_DURATION (200 * 1000)

/* Opacity if the snap indicator. */
#define AEROSNAP_INDICATOR_OPACITY (0.5)

/* The interval between snap indicator redraws (in milliseconds).
 * 16 is ~ 1/60 of a second, for ~60 FPS.
 */
#define AEROSNAP_INDICATOR_ANIMATION_TICK (16)

static void     gdk_win32_impl_frame_clock_after_paint (GdkFrameClock *clock,
                                                        GdkSurface    *surface);

G_DEFINE_TYPE (GdkWin32Surface, gdk_win32_surface, GDK_TYPE_SURFACE)

static void
gdk_win32_surface_init (GdkWin32Surface *impl)
{
  impl->hicon_big = NULL;
  impl->hicon_small = NULL;
  impl->hint_flags = 0;
  impl->transient_owner = NULL;
  impl->transient_children = NULL;
  impl->num_transients = 0;
  impl->changing_state = FALSE;
  impl->surface_scale = 1;
}


static void
gdk_surface_win32_dispose (GObject *object)
{
  GdkWin32Surface *surface;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (object));

  surface = GDK_WIN32_SURFACE (object);

  g_clear_object (&surface->cursor);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gdk_surface_win32_finalize (GObject *object)
{
  GdkWin32Surface *surface;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (object));

  surface = GDK_WIN32_SURFACE (object);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_win32_handle_table_remove (surface->handle);
    }

  g_clear_pointer (&surface->snap_stash, g_free);
  g_clear_pointer (&surface->snap_stash_int, g_free);

  if (surface->hicon_big != NULL)
    {
      GDI_CALL (DestroyIcon, (surface->hicon_big));
      surface->hicon_big = NULL;
    }

  if (surface->hicon_small != NULL)
    {
      GDI_CALL (DestroyIcon, (surface->hicon_small));
      surface->hicon_small = NULL;
    }

  _gdk_win32_surface_unregister_dnd (GDK_SURFACE (surface));

  g_assert (surface->transient_owner == NULL);
  g_assert (surface->transient_children == NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_win32_get_window_client_area_rect (GdkSurface *window,
                                        int         scale,
                                        RECT       *rect)
{
  int x, y, width, height;

  gdk_surface_get_geometry (window, &x, &y, NULL, NULL);
  width = gdk_surface_get_width (window);
  height = gdk_surface_get_height (window);
  rect->left = x * scale;
  rect->top = y * scale;
  rect->right = rect->left + width * scale;
  rect->bottom = rect->top + height * scale;
}

static void
gdk_win32_impl_frame_clock_after_paint (GdkFrameClock *clock,
                                        GdkSurface    *surface)
{
  DWM_TIMING_INFO timing_info;
  LARGE_INTEGER tick_frequency;
  GdkFrameTimings *timings;

  timings = gdk_frame_clock_get_timings (clock, gdk_frame_clock_get_frame_counter (clock));

  if (timings)
    {
      timings->refresh_interval = 16667; /* default to 1/60th of a second */
      timings->presentation_time = 0;

      if (QueryPerformanceFrequency (&tick_frequency))
        {
          HRESULT hr;

          timing_info.cbSize = sizeof (timing_info);
          hr = DwmGetCompositionTimingInfo (NULL, &timing_info);

          if (SUCCEEDED (hr))
            {
              timings->refresh_interval = timing_info.qpcRefreshPeriod * (double)G_USEC_PER_SEC / tick_frequency.QuadPart;
              timings->presentation_time = timing_info.qpcCompose * (double)G_USEC_PER_SEC / tick_frequency.QuadPart;
            }
        }

      timings->complete = TRUE;
    }
}

void
_gdk_win32_adjust_client_rect (GdkSurface *window,
			       RECT      *rect)
{
  LONG style, exstyle;

  style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);
  API_CALL (AdjustWindowRectEx, (rect, style, FALSE, exstyle));
}

gboolean
_gdk_win32_surface_enable_transparency (GdkSurface *window)
{
  DWM_BLURBEHIND blur_behind;
  HRGN empty_region;
  HRESULT call_result;
  HWND thiswindow;

  if (window == NULL || GDK_SURFACE_HWND (window) == NULL)
    return FALSE;

  if (!gdk_display_is_composited (gdk_surface_get_display (window)))
    return FALSE;

  thiswindow = GDK_SURFACE_HWND (window);

  empty_region = CreateRectRgn (0, 0, -1, -1);

  if (empty_region == NULL)
    return FALSE;

  memset (&blur_behind, 0, sizeof (blur_behind));
  blur_behind.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  blur_behind.hRgnBlur = empty_region;
  blur_behind.fEnable = TRUE;
  call_result = DwmEnableBlurBehindWindow (thiswindow, &blur_behind);

  if (!SUCCEEDED (call_result))
    g_warning ("%s: %s (%p) failed: %" G_GINT32_MODIFIER "x",
        G_STRLOC, "DwmEnableBlurBehindWindow", thiswindow, (guint32) call_result);

  DeleteObject (empty_region);

  return SUCCEEDED (call_result);
}

static const char *
get_default_title (void)
{
  const char *title;
  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

/* RegisterGdkClass
 *   is a wrapper function for RegisterWindowClassEx.
 *   It creates at least one unique class for every
 *   GdkSurfaceType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
static ATOM
RegisterGdkClass (GType wtype)
{
  static ATOM klassTOPLEVEL = 0;
  static ATOM klassTEMP     = 0;
  static HICON hAppIcon = NULL;
  static HICON hAppIconSm = NULL;
  static WNDCLASSEXW wcl;
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_surface_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = this_module ();
  wcl.hIcon = 0;
  wcl.hIconSm = 0;

  /* initialize once! */
  if (0 == hAppIcon && 0 == hAppIconSm)
    {
      char sLoc [MAX_PATH+1];

      // try to load first icon of executable program
      if (0 != GetModuleFileName (NULL, sLoc, MAX_PATH))
        {
          ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);

          if (0 == hAppIcon && 0 == hAppIconSm)
            {
              // fallback : load icon from GTK DLL
              if (0 != GetModuleFileName (this_module (), sLoc, MAX_PATH))
		{
		  ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);
		}
            }
        }

      if (0 == hAppIcon && 0 == hAppIconSm)
        {
          hAppIcon = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                GetSystemMetrics (SM_CXICON),
                                GetSystemMetrics (SM_CYICON), 0);
          hAppIconSm = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                  GetSystemMetrics (SM_CXSMICON),
                                  GetSystemMetrics (SM_CYSMICON), 0);
        }
    }

  if (0 == hAppIcon)
    hAppIcon = hAppIconSm;
  else if (0 == hAppIconSm)
    hAppIconSm = hAppIcon;

  wcl.lpszMenuName = NULL;

  /* initialize once per class */
  /*
   * HB: Setting the background brush leads to flicker, because we
   * don't get asked how to clear the background. This is not what
   * we want, at least not for input_only windows ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIconSm); \
  wcl.hbrBackground = NULL; \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);

  /* MSDN: CS_OWNDC is needed for OpenGL contexts */
  wcl.style |= CS_OWNDC;

  if (wtype != GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      if (0 == klassTOPLEVEL)
        {
          wcl.lpszClassName = L"gdkSurfaceToplevel";

          ONCE_PER_CLASS ();
          klassTOPLEVEL = RegisterClassExW (&wcl);
        }
      klass = klassTOPLEVEL;
    }
  else
    {
      if (klassTEMP == 0)
        {
          wcl.lpszClassName = L"gdkSurfaceTemp";
          wcl.style |= CS_SAVEBITS;
          ONCE_PER_CLASS ();
          klassTEMP = RegisterClassExW (&wcl);
        }

      klass = klassTEMP;
    }

  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }
  return klass;
}

static void
gdk_win32_surface_constructed (GObject *object)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (object);
  GdkSurface *surface = GDK_SURFACE (impl);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkFrameClock *frame_clock;
  HANDLE owner;
  ATOM klass = 0;
  DWORD dwStyle = 0, dwExStyle;
  RECT rect;
  const char *title;
  wchar_t *wtitle;

  impl->surface_scale = gdk_win32_display_get_monitor_scale_factor (display_win32, NULL, NULL);

  dwExStyle = 0;
  owner = NULL;

  /* MSDN: We need WS_CLIPCHILDREN and WS_CLIPSIBLINGS for GL Context Creation */
  dwStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_TOPLEVEL)
    {
      dwStyle |= WS_OVERLAPPEDWINDOW;
      frame_clock = _gdk_frame_clock_idle_new ();
    }
  else if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
      dwStyle |= WS_POPUP;
      frame_clock = _gdk_frame_clock_idle_new ();
    }
  else if (G_OBJECT_TYPE (impl) == GDK_TYPE_WIN32_POPUP)
    {
      GdkSurface *parent = gdk_popup_get_parent (GDK_POPUP (impl));
      dwStyle |= WS_POPUP;
      owner = GDK_SURFACE_HWND (parent);
      frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
    }
  else
    {
      g_assert_not_reached ();
    }

  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  title = get_default_title ();
  if (!title || !*title)
    title = "";

  klass = RegisterGdkClass (G_OBJECT_TYPE (impl));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);

  impl->handle = CreateWindowExW (dwExStyle,
                                  MAKEINTRESOURCEW (klass),
                                  wtitle,
                                  dwStyle,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  owner,
                                  NULL,
                                  this_module (),
                                  surface);
  if (impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowExW");
      g_error ("Fatal error: CreateWindowExW failed.");
    }

  GetWindowRect (impl->handle, &rect);
  impl->initial_x = rect.left;
  impl->initial_y = rect.top;

  g_object_ref (impl);
  /* Take note: we're inserting a pointer into a heap-allocated
   * object (impl). Inserting a pointer to a stack variable
   * will break the logic, since stack variables are short-lived.
   * We insert a pointer to the handle instead of the handle itself
   * probably because we need to hash them differently depending
   * on the bitness of the OS. That pointer is still unique,
   * so this works out in the end.
   */
  gdk_win32_handle_table_insert (&GDK_SURFACE_HWND (impl), impl);

  g_free (wtitle);

  gdk_surface_set_egl_native_window (surface, (void *) impl->handle);

  if (G_OBJECT_TYPE (impl) != GDK_TYPE_WIN32_DRAG_SURFACE)
    {
      if (display_win32->tablet_input_api == GDK_WIN32_TABLET_INPUT_API_WINPOINTER)
        gdk_winpointer_initialize_surface (surface);

      gdk_dmanipulation_initialize_surface (surface);
    }

  _gdk_win32_surface_enable_transparency (surface);
  _gdk_win32_surface_register_dnd (surface);
  _gdk_win32_surface_update_style_bits (surface);

  g_signal_connect (frame_clock,
                    "after-paint",
                    G_CALLBACK (gdk_win32_impl_frame_clock_after_paint),
                    impl);

  impl->hdc = GetDC (impl->handle);
  impl->inhibit_configure = TRUE;

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gdk_win32_surface_set_transient_for (GdkSurface *window,
			      GdkSurface *parent);

static void
gdk_win32_surface_destroy (GdkSurface *window,
			   gboolean   foreign_destroy)
{
  GdkWin32Surface *surface = GDK_WIN32_SURFACE (window);

  g_return_if_fail (GDK_IS_SURFACE (window));

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_destroy: %p\n",
			   GDK_SURFACE_HWND (window)));

  /* Remove ourself from the modal stack */
  _gdk_remove_modal_window (window);

  g_signal_handlers_disconnect_by_func (gdk_surface_get_frame_clock (window),
                                        gdk_win32_impl_frame_clock_after_paint,
                                        window);

  /* Remove all our transient children */
  while (surface->transient_children != NULL)
    {
      GdkSurface *child = surface->transient_children->data;
      gdk_win32_surface_set_transient_for (child, NULL);
    }

  /* Remove ourself from our transient owner */
  if (surface->transient_owner != NULL)
    {
      gdk_win32_surface_set_transient_for (window, NULL);
    }

  if (!foreign_destroy)
    {
      gdk_surface_set_egl_native_window (window, NULL);
      window->destroyed = TRUE;
      DestroyWindow (GDK_SURFACE_HWND (window));
    }
}

/* This function is called when the window really gone.
 */
static void
gdk_win32_surface_destroy_notify (GdkSurface *window)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_surface_destroy_notify: %p%s\n",
		     GDK_SURFACE_HWND (window),
		     (GDK_SURFACE_DESTROYED (window) ? " (destroyed)" : "")));

  if (!GDK_SURFACE_DESTROYED (window))
    {
      g_warning ("window %p unexpectedly destroyed",
                 GDK_SURFACE_HWND (window));

      _gdk_surface_destroy (window, TRUE);
    }

  gdk_win32_handle_table_remove (GDK_SURFACE_HWND (window));
  g_object_unref (window);
}

static void
get_outer_rect (GdkSurface *window,
		int        width,
		int        height,
		RECT      *rect)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  rect->left = rect->top = 0;
  rect->right = width * impl->surface_scale;
  rect->bottom = height * impl->surface_scale;

  _gdk_win32_adjust_client_rect (window, rect);
}

static void
gdk_win32_surface_fullscreen (GdkSurface *window,
                              GdkMonitor *monitor);

static void
show_window_internal (GdkSurface *window,
                      gboolean    already_mapped,
		      gboolean    unminimize)
{
  GdkWin32Surface *surface;
  DWORD exstyle;

  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_window_internal: %p: %s%s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state),
			   (unminimize ? " unminimize" : "")));

  /* If asked to show (not unminimize) a withdrawn and iconified
   * window, do that.
   */
  if (!unminimize &&
      !already_mapped &&
      (window->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    {
      GtkShowWindow (window, SW_SHOWMINNOACTIVE);
      return;
    }

  /* If asked to just show an iconified window, do nothing. */
  if (!unminimize && (window->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    return;

  /* If asked to unminimize an already noniconified window, do
   * nothing. (Especially, don't cause the window to rise and
   * activate. There are different calls for that.)
   */
  if (unminimize && !(window->state & GDK_TOPLEVEL_STATE_MINIMIZED))
    return;

  /* If asked to show (but not raise) a window that is already
   * visible, do nothing.
   */
  if (!unminimize && !already_mapped && IsWindowVisible (GDK_SURFACE_HWND (window)))
    return;

  /* For initial map of "normal" windows we want to emulate WM window
   * positioning behaviour, which means:
   * + default to the initial CW_USEDEFAULT placement,
   *   no matter if the user moved the window before showing it.
   * + Certain window types and hints have more elaborate positioning
   *   schemes.
   */
  surface = GDK_WIN32_SURFACE (window);
  if (!already_mapped &&
      GDK_IS_TOPLEVEL (window))
    {
      gboolean center = FALSE;
      RECT window_rect, center_on_rect;
      int x, y;

      x = surface->initial_x;
      y = surface->initial_y;

      if (FALSE)
	{
	  HMONITOR monitor;
	  MONITORINFO mi;

	  monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
	  mi.cbSize = sizeof (mi);
	  if (monitor && GetMonitorInfo (monitor, &mi))
	    center_on_rect = mi.rcMonitor;
	  else
	    {
	      center_on_rect.left = 0;
	      center_on_rect.top = 0;
	      center_on_rect.right = GetSystemMetrics (SM_CXSCREEN);
	      center_on_rect.bottom = GetSystemMetrics (SM_CYSCREEN);
	    }
	  center = TRUE;
	}
      else if (surface->transient_owner != NULL &&
	       GDK_SURFACE_IS_MAPPED (surface->transient_owner))
	{
	  GdkSurface *owner = surface->transient_owner;
	  /* Center on transient parent */
          center_on_rect.left = owner->x * surface->surface_scale;
          center_on_rect.top = owner->y * surface->surface_scale;
	  center_on_rect.right = center_on_rect.left + owner->width * surface->surface_scale;
	  center_on_rect.bottom = center_on_rect.top + owner->height * surface->surface_scale;

	  _gdk_win32_adjust_client_rect (GDK_SURFACE (owner), &center_on_rect);
	  center = TRUE;
	}

      if (center)
	{
	  GetWindowRect (GDK_SURFACE_HWND (window), &window_rect);

	  x = center_on_rect.left + ((center_on_rect.right - center_on_rect.left) - (window_rect.right - window_rect.left)) / 2;
	  y = center_on_rect.top + ((center_on_rect.bottom - center_on_rect.top) - (window_rect.bottom - window_rect.top)) / 2;
	}

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       SWP_NOZORDER_SPECIFIED,
			       x, y, 0, 0,
			       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }

  if (!already_mapped && GDK_IS_TOPLEVEL (window))
    {
      /* Ensure new windows are fully onscreen */
      RECT window_rect;
      HMONITOR monitor;
      MONITORINFO mi;
      int x, y;

      GetWindowRect (GDK_SURFACE_HWND (window), &window_rect);

      monitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
	{
	  x = window_rect.left;
	  y = window_rect.top;

	  if (window_rect.right > mi.rcWork.right)
	    {
	      window_rect.left -= (window_rect.right - mi.rcWork.right);
	      window_rect.right -= (window_rect.right - mi.rcWork.right);
	    }

	  if (window_rect.bottom > mi.rcWork.bottom)
	    {
	      window_rect.top -= (window_rect.bottom - mi.rcWork.bottom);
	      window_rect.bottom -= (window_rect.bottom - mi.rcWork.bottom);
	    }

	  if (window_rect.left < mi.rcWork.left)
	    {
	      window_rect.right += (mi.rcWork.left - window_rect.left);
	      window_rect.left += (mi.rcWork.left - window_rect.left);
	    }

	  if (window_rect.top < mi.rcWork.top)
	    {
	      window_rect.bottom += (mi.rcWork.top - window_rect.top);
	      window_rect.top += (mi.rcWork.top - window_rect.top);
	    }

	  if (x != window_rect.left || y != window_rect.top)
	    API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
				     SWP_NOZORDER_SPECIFIED,
				     window_rect.left, window_rect.top, 0, 0,
				     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
	}
    }


  if (window->state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    {
      GtkShowWindow (window, SW_MAXIMIZE);
    }
  else if (window->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    {
      GtkShowWindow (window, SW_RESTORE);
    }
  else if (GDK_IS_DRAG_SURFACE (window))
    {
      if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
        GtkShowWindow (window, SW_SHOWNOACTIVATE);
      else
        GtkShowWindow (window, SW_SHOWNA);
    }
  else if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
    {
      GtkShowWindow (window, SW_SHOWNORMAL);
    }
  else
    {
      GtkShowWindow (window, SW_SHOW);
    }

  exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  /* Sync STATE_ABOVE to TOPMOST */
  if (!GDK_IS_DRAG_SURFACE (window) &&
      (((window->state & GDK_TOPLEVEL_STATE_ABOVE) &&
	!(exstyle & WS_EX_TOPMOST)) ||
       (!(window->state & GDK_TOPLEVEL_STATE_ABOVE) &&
	(exstyle & WS_EX_TOPMOST))))
    {
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
			       (window->state & GDK_TOPLEVEL_STATE_ABOVE)?HWND_TOPMOST:HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }
}

void
gdk_win32_surface_show (GdkSurface *window,
		       gboolean already_mapped)
{
  show_window_internal (window, FALSE, FALSE);
}

static void
gdk_win32_surface_hide (GdkSurface *window)
{
  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_hide: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    gdk_surface_set_is_mapped (window, FALSE);

  _gdk_surface_clear_update_area (window);

  GtkShowWindow (window, SW_HIDE);
}

static void
gdk_win32_surface_do_move (GdkSurface *window,
                           int x, int y)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move: %p: %+d%+d\n",
                           GDK_SURFACE_HWND (window), x, y));

  if (window->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  impl = GDK_WIN32_SURFACE (window);
  get_outer_rect (window, window->width, window->height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,0,0,"
                           "NOACTIVATE|NOSIZE|NOZORDER)\n",
                           GDK_SURFACE_HWND (window),
                           x * impl->surface_scale,
                           y * impl->surface_scale));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                           SWP_NOZORDER_SPECIFIED,
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           0, 0,
                           SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
}

void
gdk_win32_surface_resize (GdkSurface *surface,
                          int width,
                          int height)
{
  RECT outer_rect;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_resize: %p: %dx%d\n",
                           GDK_SURFACE_HWND (surface), width, height));

  if (surface->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  get_outer_rect (surface, width, height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,0,0,%ld,%ld,"
                           "NOACTIVATE|NOMOVE|NOZORDER)\n",
                           GDK_SURFACE_HWND (surface),
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                           SWP_NOZORDER_SPECIFIED,
                           0, 0,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));
  surface->resize_count += 1;

  if (!GDK_WIN32_SURFACE (surface)->force_recompute_size)
    gdk_surface_request_layout (surface);
}

static void
gdk_win32_surface_do_move_resize (GdkSurface *window,
                                  int         x,
                                  int         y,
                                  int         width,
                                  int         height)
{
  RECT outer_rect;
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (window->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_move_resize: %p: %dx%d@%+d%+d\n",
                           GDK_SURFACE_HWND (window),
                           width, height, x, y));

  impl = GDK_WIN32_SURFACE (window);

  get_outer_rect (window, width, height, &outer_rect);

  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%ld,%ld,"
                           "NOACTIVATE|NOZORDER)\n",
                           GDK_SURFACE_HWND (window),
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top));

  API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                           SWP_NOZORDER_SPECIFIED,
                           x * impl->surface_scale,
                           y * impl->surface_scale,
                           outer_rect.right - outer_rect.left,
                           outer_rect.bottom - outer_rect.top,
                           SWP_NOACTIVATE | SWP_NOZORDER));
}

static void
gdk_win32_surface_move_resize_internal (GdkSurface *window,
                                        gboolean    with_move,
                                        int         x,
                                        int         y,
                                        int         width,
                                        int         height)
{
  /* We ignore changes to the window being moved or resized by the
     user, as we don't want to fight the user */
  if (GDK_SURFACE_HWND (window) == _modal_move_resize_window)
    goto out;

  if (with_move && (width < 0 && height < 0))
    {
      gdk_win32_surface_do_move (window, x, y);
    }
  else
    {
      _gdk_win32_surface_invalidate_egl_framebuffer (window);

      if (with_move)
        {
          gdk_win32_surface_do_move_resize (window, x, y, width, height);
        }
      else
        {
          gdk_win32_surface_resize (window, width, height);
        }
    }

 out:
  gdk_surface_request_layout (window);
}

void
gdk_win32_surface_move_resize (GdkSurface *window,
                               int         x,
                               int         y,
                               int         width,
                               int         height)
{
  gdk_win32_surface_move_resize_internal (window, TRUE, x, y, width, height);
}

void
gdk_win32_surface_move (GdkSurface *surface,
                        int         x,
                        int         y)
{
  gdk_win32_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
}

static void gdk_win32_surface_set_shadow_width (GdkSurface *window,
                                                int        left,
                                                int        right,
                                                int        top,
                                                int        bottom);

static void
gdk_win32_surface_layout_popup (GdkSurface     *surface,
                                int             width,
                                int             height,
                                GdkPopupLayout *layout)
{
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle final_rect;
  int x, y;
  int shadow_left, shadow_right, shadow_top, shadow_bottom;

  monitor = gdk_surface_get_layout_monitor (surface, layout,
                                            gdk_win32_monitor_get_workarea);
  if (!monitor)
    {
      GdkDisplay *display = gdk_surface_get_display (surface);
      monitor = gdk_win32_display_get_primary_monitor (display);
    }
  gdk_win32_monitor_get_workarea (monitor, &bounds);

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  gdk_win32_surface_set_shadow_width (surface,
                                      shadow_left,
                                      shadow_right,
                                      shadow_top,
                                      shadow_bottom);

  gdk_surface_layout_popup_helper (surface,
                                   width,
                                   height,
                                   shadow_left,
                                   shadow_right,
                                   shadow_top,
                                   shadow_bottom,
                                   monitor,
                                   &bounds,
                                   layout,
                                   &final_rect);

  gdk_surface_get_origin (surface->parent, &x, &y);
  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != surface->width ||
      final_rect.height != surface->height)
    {
      gdk_win32_surface_move_resize (surface,
                                     x,
                                     y,
                                     final_rect.width,
                                     final_rect.height);
    }
  else
    gdk_win32_surface_move (surface, x, y);
}

static void
maybe_notify_mapped (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    {
      gdk_surface_set_is_mapped (surface, TRUE);
      gdk_surface_invalidate_rect (surface, NULL);
    }
}

static void
show_popup (GdkSurface *surface)
{
  gdk_win32_surface_raise (surface);
  maybe_notify_mapped (surface);
  show_window_internal (surface, FALSE, FALSE);
  gdk_surface_invalidate_rect (surface, NULL);
}

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  show_popup (surface);
}

static gboolean
gdk_win32_surface_present_popup (GdkSurface     *surface,
                                 int             width,
                                 int             height,
                                 GdkPopupLayout *layout)
{
  gdk_win32_surface_layout_popup (surface, width, height, layout);

  if (GDK_SURFACE_IS_MAPPED (surface))
    return TRUE;

  if (surface->autohide)
    {
      gdk_seat_grab (gdk_display_get_default_seat (surface->display),
                     surface,
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL,
                     show_grabbing_popup, NULL);
    }
  else
    {
      show_popup (surface);
    }

  return GDK_SURFACE_IS_MAPPED (surface);
}

void
gdk_win32_surface_raise (GdkSurface *window)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_surface_raise: %p\n",
			       GDK_SURFACE_HWND (window)));

      if (GDK_IS_DRAG_SURFACE (window))
        API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_TOPMOST,
	                         0, 0, 0, 0,
				 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER));
         
      else if (GDK_IS_POPUP (window))
        ShowWindow (GDK_SURFACE_HWND (window), SW_SHOWNOACTIVATE);
      else
        /* Do not wrap this in an API_CALL macro as SetForegroundWindow might
         * fail when for example dragging a window belonging to a different
         * application at the time of a gtk_window_present() call due to focus
         * stealing prevention. */
        SetForegroundWindow (GDK_SURFACE_HWND (window));
    }
}

/**
 * gdk_win32_surface_set_urgency_hint:
 * @surface: (type GdkWin32Surface): a native `GdkSurface`.
 * @urgent: if %TRUE, flashes both the window and the taskbar button
 *          continuously.
 *
 * Flashes the specified @surface.
 */
void
gdk_win32_surface_set_urgency_hint (GdkSurface *surface,
                                    gboolean    urgent)
{
  FLASHWINFO flashwinfo;

  g_return_if_fail (GDK_IS_WIN32_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  flashwinfo.cbSize = sizeof (flashwinfo);
  flashwinfo.hwnd = GDK_SURFACE_HWND (surface);
  if (urgent)
    flashwinfo.dwFlags = FLASHW_ALL | FLASHW_TIMER;
  else
    flashwinfo.dwFlags = FLASHW_STOP;
  flashwinfo.uCount = 0;
  flashwinfo.dwTimeout = 0;

  FlashWindowEx (&flashwinfo);
}

static gboolean
get_effective_window_decorations (GdkSurface       *window,
                                  GdkWMDecoration *decoration)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  *decoration = 0;

  if (!GDK_IS_TOPLEVEL (window))
    return FALSE;

  /* we want to apply the "no decorations", if decorations are disabled */
  if (!GDK_WIN32_SURFACE (window)->decorate_all)
    return TRUE;

  if ((impl->hint_flags & GDK_HINT_MIN_SIZE) &&
      (impl->hint_flags & GDK_HINT_MAX_SIZE) &&
      impl->hints.min_width == impl->hints.max_width &&
      impl->hints.min_height == impl->hints.max_height)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MAXIMIZE;

      *decoration |= GDK_DECOR_MINIMIZE;

      return TRUE;
    }
  else
    {
      *decoration = GDK_DECOR_ALL;
      return TRUE;
    }

  return FALSE;
}

static void
gdk_win32_surface_set_geometry_hints (GdkSurface         *window,
			       const GdkGeometry *geometry,
			       GdkSurfaceHints     geom_mask)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_geometry_hints: %p\n",
			   GDK_SURFACE_HWND (window)));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    fi->hint_flags = geom_mask;
  else
    impl->hint_flags = geom_mask;
  impl->hints = *geometry;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MIN_SIZE: %dx%d\n",
			       geometry->min_width, geometry->min_height));
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MAX_SIZE: %dx%d\n",
			       geometry->max_width, geometry->max_height));
    }

  _gdk_win32_surface_update_style_bits (window);
}

static void
gdk_win32_surface_set_title (GdkSurface   *window,
		      const char *title)
{
  wchar_t *wtitle;

  g_return_if_fail (GDK_IS_SURFACE (window));
  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (window))
    return;

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";

  GDK_NOTE (MISC, g_print ("gdk_surface_set_title: %p: %s\n",
			   GDK_SURFACE_HWND (window), title));

  GDK_NOTE (MISC_OR_EVENTS, title = g_strdup_printf ("%p %s", GDK_SURFACE_HWND (window), title));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  API_CALL (SetWindowTextW, (GDK_SURFACE_HWND (window), wtitle));
  g_free (wtitle);

  GDK_NOTE (MISC_OR_EVENTS, g_free ((char *) title));
}

static void
gdk_win32_surface_set_transient_for (GdkSurface *window,
			      GdkSurface *parent)
{
  HWND window_id, parent_id;
  LONG_PTR old_ptr;
  DWORD w32_error;
  GdkWin32Surface *surface = GDK_WIN32_SURFACE (window);
  GdkWin32Surface *parent_impl = NULL;
  GSList *item;

  g_return_if_fail (GDK_IS_SURFACE (window));

  window_id = GDK_SURFACE_HWND (window);
  parent_id = parent != NULL ? GDK_SURFACE_HWND (parent) : NULL;

  GDK_NOTE (MISC, g_print ("gdk_surface_set_transient_for: %p: %p\n", window_id, parent_id));

  if (GDK_SURFACE_DESTROYED (window) || (parent && GDK_SURFACE_DESTROYED (parent)))
    {
      if (GDK_SURFACE_DESTROYED (window))
	GDK_NOTE (MISC, g_print ("... destroyed!\n"));
      else
	GDK_NOTE (MISC, g_print ("... owner destroyed!\n"));

      return;
    }

  if (surface->transient_owner == parent)
    return;

  if (GDK_IS_SURFACE (surface->transient_owner))
    {
      GdkWin32Surface *trans_impl = GDK_WIN32_SURFACE (surface->transient_owner);
      item = g_slist_find (trans_impl->transient_children, window);
      item->data = NULL;
      trans_impl->transient_children = g_slist_delete_link (trans_impl->transient_children, item);
      trans_impl->num_transients--;

      if (!trans_impl->num_transients)
        {
          trans_impl->transient_children = NULL;
        }

      g_object_unref (G_OBJECT (surface->transient_owner));
      g_object_unref (G_OBJECT (window));

      surface->transient_owner = NULL;
    }

  if (parent)
    {
      parent_impl = GDK_WIN32_SURFACE (parent);

      parent_impl->transient_children = g_slist_append (parent_impl->transient_children, window);
      g_object_ref (G_OBJECT (window));
      parent_impl->num_transients++;
      surface->transient_owner = parent;
      g_object_ref (G_OBJECT (parent));
    }

  SetLastError (0);
  old_ptr = GetWindowLongPtr (window_id, GWLP_HWNDPARENT);
  w32_error = GetLastError ();

  /* Don't re-set GWLP_HWNDPARENT to the same value */
  if ((HWND) old_ptr == parent_id && w32_error == NO_ERROR)
    return;

  /* Don't return if it failed, try SetWindowLongPtr() anyway */
  if (old_ptr == 0 && w32_error != NO_ERROR)
    WIN32_API_FAILED ("GetWindowLongPtr");

  /* This changes the *owner* of the window, despite the misleading
   * name. (Owner and parent are unrelated concepts.) At least that's
   * what people who seem to know what they talk about say on
   * USENET. Search on Google.
   */
  SetLastError (0);
  old_ptr = SetWindowLongPtr (window_id, GWLP_HWNDPARENT, (LONG_PTR) parent_id);
  w32_error = GetLastError ();

  if (old_ptr == 0 && w32_error != NO_ERROR)
    WIN32_API_FAILED ("SetWindowLongPtr");
}

void
_gdk_push_modal_window (GdkSurface *window)
{
  modal_window_stack = g_slist_prepend (modal_window_stack, window);
}

void
_gdk_remove_modal_window (GdkSurface *window)
{
  GSList *tmp;

  g_return_if_fail (window != NULL);

  /* It's possible to be NULL here if someone sets the modal hint of the window
   * to FALSE before a modal window stack has ever been created. */
  if (modal_window_stack == NULL)
    return;

  /* Find the requested window in the stack and remove it.  Yeah, I realize this
   * means we're not a 'real stack', strictly speaking.  Sue me. :) */
  tmp = g_slist_find (modal_window_stack, window);
  if (tmp != NULL)
    {
      modal_window_stack = g_slist_delete_link (modal_window_stack, tmp);
    }
}

gboolean
_gdk_modal_blocked (GdkSurface *window)
{
  GSList *l;
  gboolean found_any = FALSE;

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkSurface *modal = l->data;

      if (modal == window)
	return FALSE;

      if (GDK_SURFACE_IS_MAPPED (modal))
	found_any = TRUE;
    }

  return found_any;
}

GdkSurface *
_gdk_modal_current (void)
{
  GSList *l;

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkSurface *modal = l->data;

      if (GDK_SURFACE_IS_MAPPED (modal))
	return modal;
    }

  return NULL;
}

static void
gdk_win32_surface_get_geometry (GdkSurface *window,
			       int       *x,
			       int       *y,
			       int       *width,
			       int       *height)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
      RECT rect;
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      if (GDK_IS_TOPLEVEL (window) && impl->drag_move_resize_context.native_move_resize_pending)
         rect = impl->next_layout.configured_rect;
      else
        {
          POINT pt;
          GdkSurface *parent;

          if (GDK_IS_TOPLEVEL (window))
            parent = NULL;
          else if (GDK_IS_POPUP (window))
            parent = gdk_popup_get_parent (GDK_POPUP (window));
          else
            parent = NULL;

          API_CALL (GetClientRect, (GDK_SURFACE_HWND (window), &rect));

          pt.x = rect.left;
          pt.y = rect.top;
	      ClientToScreen (GDK_SURFACE_HWND (window), &pt);
          if (parent)
            ScreenToClient (GDK_SURFACE_HWND (parent), &pt);

          rect.left = pt.x;
          rect.top = pt.y;

          pt.x = rect.right;
          pt.y = rect.bottom;
          ClientToScreen (GDK_SURFACE_HWND (window), &pt);
          if (parent)
            ScreenToClient (GDK_SURFACE_HWND (parent), &pt);

          rect.right = pt.x;
          rect.bottom = pt.y;
        }

      if (x)
	*x = rect.left / impl->surface_scale;
      if (y)
	*y = rect.top / impl->surface_scale;
      if (width)
	*width = (rect.right - rect.left) / impl->surface_scale;
      if (height)
	*height = (rect.bottom - rect.top) / impl->surface_scale;

      GDK_NOTE (MISC, g_print ("gdk_win32_surface_get_geometry: %p: %ldx%ld@%+ld%+ld\n",
			       GDK_SURFACE_HWND (window),
			       (rect.right - rect.left) / impl->surface_scale,
			       (rect.bottom - rect.top) / impl->surface_scale,
			       rect.left, rect.top));
    }
}

static void
gdk_win32_surface_get_root_coords (GdkSurface *window,
				  int        x,
				  int        y,
				  int       *root_x,
				  int       *root_y)
{
  int tx;
  int ty;
  POINT pt;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  pt.x = x * impl->surface_scale;
  pt.y = y * impl->surface_scale;
  ClientToScreen (GDK_SURFACE_HWND (window), &pt);
  tx = pt.x;
  ty = pt.y;

  if (root_x)
    *root_x = tx / impl->surface_scale;
  if (root_y)
    *root_y = ty / impl->surface_scale;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_get_root_coords: %p: %+d%+d %+d%+d\n",
			   GDK_SURFACE_HWND (window),
			   x * impl->surface_scale,
			   y * impl->surface_scale,
			   tx / impl->surface_scale,
			   ty / impl->surface_scale));
}

static gboolean
gdk_surface_win32_get_device_state (GdkSurface     *window,
                                    GdkDevice       *device,
                                    double          *x,
                                    double          *y,
                                    GdkModifierType *mask)
{
  _gdk_device_win32_query_state (device, window, NULL, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < window->width && *y < window->height;

}

static void
update_single_bit (LONG    *style,
                   gboolean all,
		   int      gdk_bit,
		   int      style_bit)
{
  /* all controls the interpretation of gdk_bit -- if all is TRUE,
   * gdk_bit indicates whether style_bit is off; if all is FALSE, gdk
   * bit indicate whether style_bit is on
   */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    *style |= style_bit;
  else
    *style &= ~style_bit;
}

/*
 * Returns TRUE if window has no decorations.
 * Usually it means CSD windows, because GTK
 * calls gdk_surface_set_decorations (window, 0);
 */
gboolean
_gdk_win32_surface_lacks_wm_decorations (GdkSurface *window)
{
  GdkWin32Surface *impl;
  LONG style;
  gboolean has_any_decorations;

  if (GDK_SURFACE_DESTROYED (window))
    return FALSE;

  impl = GDK_WIN32_SURFACE (window);

  /* This is because GTK calls gdk_surface_set_decorations (window, 0),
   * even though GdkWMDecoration docs indicate that 0 does NOT mean
   * "no decorations".
   */
  if (!impl->decorate_all)
    return TRUE;

  if (GDK_SURFACE_HWND (window) == 0)
    return FALSE;

  style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);

  if (style == 0)
    {
      DWORD w32_error = GetLastError ();

      GDK_NOTE (MISC, g_print ("Failed to get style of window %p (handle %p): %lu\n",
                               window, GDK_SURFACE_HWND (window), w32_error));
      return FALSE;
    }

  /* Keep this in sync with _gdk_win32_surface_update_style_bits() */
  /* We don't check what get_effective_window_decorations()
   * has to say, because it gives suggestions based on
   * various hints, while we want *actual* decorations,
   * or their absence.
   */
  has_any_decorations = FALSE;

  if (style & (WS_BORDER | WS_THICKFRAME | WS_CAPTION |
               WS_SYSMENU | WS_MAXIMIZEBOX))
    has_any_decorations = TRUE;
  else
    GDK_NOTE (MISC, g_print ("Window %p (handle %p): has no decorations (style %lx)\n",
                             window, GDK_SURFACE_HWND (window), style));

  return !has_any_decorations;
}

void
_gdk_win32_surface_update_style_bits (GdkSurface *window)
{
  GdkWMDecoration decorations;
  LONG old_style, new_style, old_exstyle, new_exstyle;
  gboolean all;
  RECT rect, before, after;
  gboolean was_topmost;
  gboolean will_be_topmost;
  gboolean was_layered;
  gboolean will_be_layered;
  HWND insert_after;
  UINT flags;

  if (window->state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    return;

  old_style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);
  old_exstyle = GetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE);

  GetClientRect (GDK_SURFACE_HWND (window), &before);
  after = before;
  AdjustWindowRectEx (&before, old_style, FALSE, old_exstyle);

  was_topmost = (old_exstyle & WS_EX_TOPMOST) ? TRUE : FALSE;
  was_layered = (old_exstyle & WS_EX_LAYERED) ? TRUE : FALSE;
  will_be_topmost = was_topmost;
  will_be_layered = was_layered;

  old_exstyle &= ~WS_EX_TOPMOST;

  new_style = old_style;
  new_exstyle = old_exstyle;

  if (GDK_IS_DRAG_SURFACE (window))
    {
      new_exstyle |= WS_EX_TOOLWINDOW;

      /* WS_EX_LAYERED | WS_EX_TRANSPARENT makes the drag surface behave
       * in pointer input passthrough mode, so it doesn't interfere with
       * the drag and drop operation.
       */
      new_exstyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
      will_be_topmost = TRUE;
      will_be_layered = TRUE;
    }
  else
    {
      new_exstyle &= ~WS_EX_TOOLWINDOW;
    }

  if (get_effective_window_decorations (window, &decorations))
    {
      all = (decorations & GDK_DECOR_ALL);

      /* Keep this in sync with the test in _gdk_win32_surface_lacks_wm_decorations() */
      update_single_bit (&new_style, all, decorations & GDK_DECOR_BORDER, WS_BORDER);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_RESIZEH, WS_THICKFRAME);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_TITLE, WS_CAPTION);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MENU, WS_SYSMENU);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MAXIMIZE, WS_MAXIMIZEBOX);
    }

  if (old_style == new_style && old_exstyle == new_exstyle )
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: no change\n",
			       GDK_SURFACE_HWND (window)));
      return;
    }

  if (old_style != new_style)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: STYLE: %s => %s\n",
			       GDK_SURFACE_HWND (window),
			       _gdk_win32_surface_style_to_string (old_style),
			       _gdk_win32_surface_style_to_string (new_style)));

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE, new_style);
    }

  if (old_exstyle != new_exstyle)
    {
      GDK_NOTE (MISC, g_print ("_gdk_win32_surface_update_style_bits: %p: EXSTYLE: %s => %s\n",
			       GDK_SURFACE_HWND (window),
			       _gdk_win32_surface_exstyle_to_string (old_exstyle),
			       _gdk_win32_surface_exstyle_to_string (new_exstyle)));

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_EXSTYLE, new_exstyle);

      if (!was_layered && will_be_layered)
        {
          /* We have to call SetLayeredWindowAttributes when setting the
           * WS_EX_LAYERED style anew, otherwise the window won't show up
           */
          API_CALL (SetLayeredWindowAttributes, (GDK_SURFACE_HWND (window), 0, 255, LWA_ALPHA));
        }
    }

  AdjustWindowRectEx (&after, new_style, FALSE, new_exstyle);

  GetWindowRect (GDK_SURFACE_HWND (window), &rect);
  rect.left += after.left - before.left;
  rect.top += after.top - before.top;
  rect.right += after.right - before.right;
  rect.bottom += after.bottom - before.bottom;

  flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOREPOSITION;

  if (will_be_topmost && !was_topmost)
    {
      insert_after = HWND_TOPMOST;
    }
  else if (was_topmost && !will_be_topmost)
    {
      insert_after = HWND_NOTOPMOST;
    }
  else
    {
      flags |= SWP_NOZORDER;
      insert_after = SWP_NOZORDER_SPECIFIED;
    }

  SetWindowPos (GDK_SURFACE_HWND (window), insert_after,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		flags);
}

#if defined(MORE_AEROSNAP_DEBUGGING)
static void
log_region (char *prefix, AeroSnapEdgeRegion *region)
{
  GDK_NOTE (MISC, g_print ("Region %s:\n"
                           "edge %d x %d @ %d x %d\n"
                           "trig %d x %d @ %d x %d\n",
                           prefix,
                           region->edge.width,
                           region->edge.height,
                           region->edge.x,
                           region->edge.y,
                           region->trigger.width,
                           region->trigger.height,
                           region->trigger.x,
                           region->trigger.y));
}
#endif

static void
calculate_aerosnap_regions (GdkW32DragMoveResizeContext *context)
{
  GdkDisplay *display;
  GListModel *monitors;
  int monitor_idx, other_monitor_idx;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (context->window);
#if defined(MORE_AEROSNAP_DEBUGGING)
  int i;
#endif

  display = gdk_surface_get_display (context->window);
  monitors = gdk_display_get_monitors (display);

#define _M_UP 0
#define _M_DOWN 1
#define _M_LEFT 2
#define _M_RIGHT 3

  for (monitor_idx = 0; monitor_idx < g_list_model_get_n_items (monitors); monitor_idx++)
    {
      GdkRectangle wa;
      GdkRectangle geometry;
      AeroSnapEdgeRegion snap_region;
      gboolean move_edge[4] = { TRUE, FALSE, TRUE, TRUE };
      gboolean resize_edge[2] = { TRUE, TRUE };
      int diff;
      int thickness, trigger_thickness;
      GdkMonitor *monitor;

      monitor = g_list_model_get_item (monitors, monitor_idx);
      g_object_unref (monitor);
      gdk_win32_monitor_get_workarea (monitor, &wa);
      gdk_monitor_get_geometry (monitor, &geometry);

      for (other_monitor_idx = 0;
           other_monitor_idx < g_list_model_get_n_items (monitors) &&
           (move_edge[_M_UP] || move_edge[_M_LEFT] ||
           move_edge[_M_RIGHT] || resize_edge[_M_DOWN]);
           other_monitor_idx++)
        {
          GdkRectangle other_wa;
          GdkMonitor *other_monitor;

          if (other_monitor_idx == monitor_idx)
            continue;

          other_monitor = g_list_model_get_item (monitors, other_monitor_idx);
          g_object_unref (other_monitor);
          gdk_win32_monitor_get_workarea (other_monitor, &other_wa);

          /* An edge triggers AeroSnap only if there are no
           * monitors beyond that edge.
           * Even if there's another monitor, but it does not cover
           * the whole edge (it's smaller or is not aligned to
           * the corner of current monitor), that edge is still
           * removed from the trigger list.
           */
          if (other_wa.x >= wa.x + wa.width)
            move_edge[_M_RIGHT] = FALSE;

          if (other_wa.x + other_wa.width <= wa.x)
            move_edge[_M_LEFT] = FALSE;

          if (other_wa.y + other_wa.height <= wa.y)
            {
              move_edge[_M_UP] = FALSE;
              resize_edge[_M_UP] = FALSE;
            }

          if (other_wa.y >= wa.y + wa.height)
            {
              /* no move_edge for the bottom edge, just resize_edge */
              resize_edge[_M_DOWN] = FALSE;
            }
        }

      thickness = AEROSNAP_REGION_THICKNESS * impl->surface_scale;
      trigger_thickness = AEROSNAP_REGION_TRIGGER_THICKNESS * impl->surface_scale;

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.height = thickness;
      snap_region.trigger.height = trigger_thickness;

      /* Extend both regions into toolbar space.
       * When there's no toolbar, diff == 0.
       */
      diff = wa.y - geometry.y;
      snap_region.edge.height += diff;
      snap_region.edge.y -= diff;
      snap_region.trigger.height += diff;
      snap_region.trigger.y -= diff;

      if (move_edge[_M_UP])
        g_array_append_val (context->maximize_regions, snap_region);

      if (resize_edge[_M_UP])
        g_array_append_val (context->fullup_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.width = thickness;
      snap_region.trigger.width = trigger_thickness;

      diff = wa.x - geometry.x;
      snap_region.edge.width += diff;
      snap_region.edge.x -= diff;
      snap_region.trigger.width += diff;
      snap_region.trigger.x -= diff;

      if (move_edge[_M_LEFT])
        g_array_append_val (context->halfleft_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.x += wa.width - thickness;
      snap_region.edge.width = thickness;
      snap_region.trigger.x += wa.width - trigger_thickness;
      snap_region.trigger.width = trigger_thickness;

      diff = (geometry.x + geometry.width) - (wa.x + wa.width);
      snap_region.edge.width += diff;
      snap_region.trigger.width += diff;

      if (move_edge[_M_RIGHT])
        g_array_append_val (context->halfright_regions, snap_region);

      snap_region.edge = wa;
      snap_region.trigger = wa;
      snap_region.edge.y += wa.height - thickness;
      snap_region.edge.height = thickness;
      snap_region.trigger.y += wa.height - trigger_thickness;
      snap_region.trigger.height = trigger_thickness;

      diff = (geometry.y + geometry.height) - (wa.y + wa.height);
      snap_region.edge.height += diff;
      snap_region.trigger.height += diff;

      if (resize_edge[_M_DOWN])
        g_array_append_val (context->fullup_regions, snap_region);
    }

#undef _M_UP
#undef _M_DOWN
#undef _M_LEFT
#undef _M_RIGHT

#if defined(MORE_AEROSNAP_DEBUGGING)
  for (i = 0; i < context->maximize_regions->len; i++)
    log_region ("maximize", &g_array_index (context->maximize_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->halfleft_regions->len; i++)
    log_region ("halfleft", &g_array_index (context->halfleft_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->halfright_regions->len; i++)
    log_region ("halfright", &g_array_index (context->halfright_regions, AeroSnapEdgeRegion, i));

  for (i = 0; i < context->fullup_regions->len; i++)
    log_region ("fullup", &g_array_index (context->fullup_regions, AeroSnapEdgeRegion, i));
#endif
}

static void
discard_snapinfo (GdkSurface *window)
{
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;

  if (impl->snap_stash == NULL)
    return;

  g_clear_pointer (&impl->snap_stash, g_free);
  g_clear_pointer (&impl->snap_stash_int, g_free);
}

static void
unsnap (GdkSurface  *window,
        GdkMonitor *monitor)
{
  GdkWin32Surface *impl;
  GdkRectangle rect;

  impl = GDK_WIN32_SURFACE (window);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;

  if (impl->snap_stash == NULL)
    return;

  gdk_win32_monitor_get_workarea (monitor, &rect);

  GDK_NOTE (MISC, g_print ("Monitor work area %d x %d @ %d : %d\n", rect.width, rect.height, rect.x, rect.y));

  if (rect.width >= impl->snap_stash_int->width &&
      rect.height >= impl->snap_stash_int->height)
    {
      /* If the window fits into new work area without resizing it,
       * place it into new work area without resizing it.
       */
      double left, right, up, down, hratio, vratio;
      double hscale, vscale;
      double new_left, new_up;

      left = impl->snap_stash->x;
      right = 1.0 - (impl->snap_stash->x + impl->snap_stash->width);
      up = impl->snap_stash->y;
      down = 1.0 - (impl->snap_stash->y + impl->snap_stash->height);
      hscale = 1.0;

      if (right > 0.001)
        {
          hratio = left / right;
          hscale = hratio / (1.0 + hratio);
        }

      new_left = (double) (rect.width - impl->snap_stash_int->width) * hscale;

      vscale = 1.0;

      if (down > 0.001)
        {
          vratio = up / down;
          vscale = vratio / (1.0 + vratio);
        }

      new_up = (double) (rect.height - impl->snap_stash_int->height) * vscale;

      rect.x = round (rect.x + new_left);
      rect.y = round (rect.y + new_up);
      rect.width = impl->snap_stash_int->width;
      rect.height = impl->snap_stash_int->height;
    }
  else
    {
      /* Calculate actual unsnapped window size based on its
       * old relative size. Same for position.
       */
      rect.x += round (rect.width * impl->snap_stash->x);
      rect.y += round (rect.height * impl->snap_stash->y);
      rect.width = round (rect.width * impl->snap_stash->width);
      rect.height = round (rect.height * impl->snap_stash->height);
    }

  GDK_NOTE (MISC, g_print ("Unsnapped window size %d x %d @ %d : %d\n", rect.width, rect.height, rect.x, rect.y));

  gdk_win32_surface_move_resize (window, rect.x, rect.y,
                                 rect.width, rect.height);

  g_clear_pointer (&impl->snap_stash, g_free);
  g_clear_pointer (&impl->snap_stash_int, g_free);
}

static void
stash_window (GdkSurface          *window,
              GdkWin32Surface *impl)
{
  int x, y;
  int width, wwidth;
  int height, wheight;
  WINDOWPLACEMENT placement;
  HMONITOR hmonitor;
  MONITORINFO hmonitor_info;

  placement.length = sizeof(WINDOWPLACEMENT);

  /* Use W32 API to get unmaximized window size, which GDK doesn't remember */
  if (!GetWindowPlacement (GDK_SURFACE_HWND (window), &placement))
    return;

  /* MSDN is very vague, but in practice rcNormalPosition is the same as GetWindowRect(),
   * only with adjustments for toolbars (which creates rather weird coordinate space issues).
   * We need to get monitor info and apply workarea vs monitorarea diff to turn
   * these into screen coordinates proper.
   */
  hmonitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);
  hmonitor_info.cbSize = sizeof (hmonitor_info);

  if (!GetMonitorInfoA (hmonitor, &hmonitor_info))
    return;

  if (impl->snap_stash == NULL)
    impl->snap_stash = g_new0 (GdkRectangleDouble, 1);

  if (impl->snap_stash_int == NULL)
    impl->snap_stash_int = g_new0 (GdkRectangle, 1);

  GDK_NOTE (MISC, g_print ("monitor work area  %ld x %ld @ %ld : %ld\n",
                           (hmonitor_info.rcWork.right - hmonitor_info.rcWork.left) / impl->surface_scale,
                           (hmonitor_info.rcWork.bottom - hmonitor_info.rcWork.top) / impl->surface_scale,
                           hmonitor_info.rcWork.left,
                           hmonitor_info.rcWork.top));
  GDK_NOTE (MISC, g_print ("monitor      area  %ld x %ld @ %ld : %ld\n",
                           (hmonitor_info.rcMonitor.right - hmonitor_info.rcMonitor.left) / impl->surface_scale,
                           (hmonitor_info.rcMonitor.bottom - hmonitor_info.rcMonitor.top) / impl->surface_scale,
                           hmonitor_info.rcMonitor.left,
                           hmonitor_info.rcMonitor.top));
  GDK_NOTE (MISC, g_print ("window  work place %ld x %ld @ %ld : %ld\n",
                           (placement.rcNormalPosition.right - placement.rcNormalPosition.left) / impl->surface_scale,
                           (placement.rcNormalPosition.bottom - placement.rcNormalPosition.top) / impl->surface_scale,
                           placement.rcNormalPosition.left,
                           placement.rcNormalPosition.top));

  width = (placement.rcNormalPosition.right - placement.rcNormalPosition.left) / impl->surface_scale;
  height = (placement.rcNormalPosition.bottom - placement.rcNormalPosition.top) / impl->surface_scale;
  x = (placement.rcNormalPosition.left - hmonitor_info.rcMonitor.left) / impl->surface_scale;
  y = (placement.rcNormalPosition.top - hmonitor_info.rcMonitor.top) / impl->surface_scale;

  wwidth = (hmonitor_info.rcWork.right - hmonitor_info.rcWork.left) / impl->surface_scale;
  wheight = (hmonitor_info.rcWork.bottom - hmonitor_info.rcWork.top) / impl->surface_scale;

  impl->snap_stash->x = (double) (x) / (double) (wwidth);
  impl->snap_stash->y = (double) (y) / (double) (wheight);
  impl->snap_stash->width = (double) width / (double) (wwidth);
  impl->snap_stash->height = (double) height / (double) (wheight);

  impl->snap_stash_int->x = x;
  impl->snap_stash_int->y = y;
  impl->snap_stash_int->width = width;
  impl->snap_stash_int->height = height;

  GDK_NOTE (MISC, g_print ("Stashed window %d x %d @ %d : %d as %f x %f @ %f : %f\n",
                           width, height, x, y,
                           impl->snap_stash->width, impl->snap_stash->height, impl->snap_stash->x, impl->snap_stash->y));
}

static void
snap_up (GdkSurface *surface)
{
  SHORT maxysize;
  int x, y;
  int width, height;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (surface);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_FULLUP;

  stash_window (surface, impl);

  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN) / impl->surface_scale;
  x = y = 0;
  width = gdk_surface_get_width (surface);

  y = 0;
  height = maxysize;

  x = surface->x - impl->shadow.left / impl->surface_scale;
  y = y - impl->shadow.top / impl->surface_scale;
  width += impl->shadow_x;
  height += impl->shadow_y;

  gdk_win32_surface_move_resize (surface, x, y, width, height);
}

static void
snap_left (GdkSurface *surface,
           GdkMonitor *monitor,
           GdkMonitor *snap_monitor)
{
  GdkRectangle rect;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (surface);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;

  gdk_win32_monitor_get_workarea (snap_monitor, &rect);

  stash_window (surface, impl);

  rect.width = rect.width / 2;

  rect.x = rect.x - impl->shadow.left / impl->surface_scale;
  rect.y = rect.y - impl->shadow.top / impl->surface_scale;
  rect.width = rect.width + impl->shadow_x;
  rect.height = rect.height + impl->shadow_y;

  gdk_win32_surface_move_resize (surface,
                                 rect.x, rect.y,
                                 rect.width, rect.height);
}

static void
snap_right (GdkSurface *surface,
            GdkMonitor *monitor,
            GdkMonitor *snap_monitor)
{
  GdkRectangle rect;
  GdkWin32Surface *impl;

  impl = GDK_WIN32_SURFACE (surface);

  impl->snap_state = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;

  gdk_win32_monitor_get_workarea (snap_monitor, &rect);

  stash_window (surface, impl);

  rect.width = rect.width / 2;
  rect.x += rect.width;

  rect.x = rect.x - impl->shadow.left / impl->surface_scale;
  rect.y = rect.y - impl->shadow.top / impl->surface_scale;
  rect.width = rect.width + impl->shadow_x;
  rect.height = rect.height + impl->shadow_y;

  gdk_win32_surface_move_resize (surface,
                                 rect.x, rect.y,
                                 rect.width, rect.height);
}

static void
gdk_win32_surface_maximize (GdkSurface *window);
static void
gdk_win32_surface_unmaximize (GdkSurface *window);
static void
gdk_win32_surface_minimize (GdkSurface *window);

void
_gdk_win32_surface_handle_aerosnap (GdkSurface            *window,
                                   GdkWin32AeroSnapCombo combo)
{
  GdkWin32Surface *impl;
  GdkDisplay *display;
  GListModel *monitors;
  int n_monitors;
  GdkToplevelState surface_state = gdk_toplevel_get_state (GDK_TOPLEVEL (window));
  gboolean minimized = surface_state & GDK_TOPLEVEL_STATE_MINIMIZED;
  gboolean maximized = surface_state & GDK_TOPLEVEL_STATE_MAXIMIZED;
  gboolean halfsnapped;
  GdkMonitor *monitor;

  impl = GDK_WIN32_SURFACE (window);
  display = gdk_surface_get_display (window);
  monitors = gdk_display_get_monitors (display);
  n_monitors = g_list_model_get_n_items (monitors);
  monitor = gdk_display_get_monitor_at_surface (display, window);

  if (minimized && maximized)
    minimized = FALSE;

  halfsnapped = (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
                 impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
                 impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP);

  switch (combo)
    {
    case GDK_WIN32_AEROSNAP_COMBO_NOTHING:
      /* Do nothing */
      break;
    case GDK_WIN32_AEROSNAP_COMBO_UP:
      if (!maximized)
        {
	  unsnap (window, monitor);
          gdk_win32_surface_maximize (window);
        }
      break;
    case GDK_WIN32_AEROSNAP_COMBO_DOWN:
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTDOWN:
      if (maximized)
        {
	  gdk_win32_surface_unmaximize (window);
	  unsnap (window, monitor);
        }
      else if (halfsnapped)
	unsnap (window, monitor);
      else if (!minimized)
	gdk_win32_surface_minimize (window);
      break;
    case GDK_WIN32_AEROSNAP_COMBO_LEFT:
      if (maximized)
        gdk_win32_surface_unmaximize (window);

      if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED ||
	  impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP)
	{
	  unsnap (window, monitor);
	  snap_left (window, monitor, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT)
	{
          GdkMonitor *other;

	  unsnap (window, monitor);
          if (gdk_win32_display_get_primary_monitor (monitor->display) == monitor)
            other = g_object_ref (monitor);
          else
            other = g_list_model_get_item (monitors, n_monitors - 1);
	  snap_right (window, monitor, other);
          g_object_unref (other);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT)
	{
	  unsnap (window, monitor);
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_RIGHT:
      if (maximized)
        gdk_win32_surface_unmaximize (window);

      if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED ||
	  impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP)
	{
	  unsnap (window, monitor);
	  snap_right (window, monitor, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT)
	{
	  unsnap (window, monitor);
	}
      else if (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT)
	{
          GdkMonitor *other;
	  int i;

	  unsnap (window, monitor);
          for (i = 0; i < n_monitors; i++)
            {
              other = g_list_model_get_item (monitors, i);
              g_object_unref (other);
              if (monitor == other)
                break;
            }

          other = g_list_model_get_item (monitors, (i + 1) % n_monitors);
          snap_left (window, monitor, other);
          g_object_unref (other);
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTUP:
      if (!maximized &&
          impl->snap_state == GDK_WIN32_AEROSNAP_STATE_UNDETERMINED)
	{
	  snap_up (window);
	}
      break;
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTLEFT:
    case GDK_WIN32_AEROSNAP_COMBO_SHIFTRIGHT:
      /* No implementation needed at the moment */
      break;
    }
}

static void
apply_snap (GdkSurface             *surface,
            GdkWin32AeroSnapState   snap)
{
  GdkMonitor *monitor;
  GdkDisplay *display;

  display = gdk_surface_get_display (surface);
  monitor = gdk_display_get_monitor_at_surface (display, surface);

  switch (snap)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      break;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      unsnap (surface, monitor);
      gdk_win32_surface_maximize (surface);
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      unsnap (surface, monitor);
      snap_left (surface, monitor, monitor);
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      unsnap (surface, monitor);
      snap_right (surface, monitor, monitor);
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      snap_up (surface);
      break;
    }

  if (snap != GDK_WIN32_AEROSNAP_STATE_UNDETERMINED)
    {
      GDK_WIN32_SURFACE (surface)->inhibit_configure = TRUE;
      GDK_WIN32_SURFACE (surface)->force_recompute_size = FALSE;
    }
}

/* Registers a dumb window class. This window
 * has DefWindowProc() for a window procedure and
 * does not do anything that GdkSurface-bound HWNDs do.
 */
static ATOM
RegisterGdkDumbClass ()
{
  static ATOM klassDUMB = 0;
  static WNDCLASSEXW wcl;
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = DefWindowProcW;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = this_module ();
  wcl.hIcon = 0;
  wcl.hIconSm = 0;
  wcl.lpszMenuName = NULL;
  wcl.hbrBackground = NULL;
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);
  wcl.style |= CS_OWNDC;
  wcl.lpszClassName = L"gdkSurfaceDumb";

  if (klassDUMB == 0)
    klassDUMB = RegisterClassExW (&wcl);

  klass = klassDUMB;

  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }

  return klass;
}

static gboolean
ensure_snap_indicator_exists (GdkW32DragMoveResizeContext *context)
{
  if (context->shape_indicator == NULL)
    {
      HWND handle;
      ATOM klass;
      klass = RegisterGdkDumbClass ();

      handle = CreateWindowExW (WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
                                MAKEINTRESOURCEW (klass),
                                L"",
                                WS_POPUP,
                                0,
                                0,
                                0, 0,
                                NULL,
                                NULL,
                                this_module (),
                                NULL);

      context->shape_indicator = handle;
    }

  return context->shape_indicator != NULL;
}

static gboolean
ensure_snap_indicator_surface (GdkW32DragMoveResizeContext *context,
                          int                          width,
                          int                          height,
                          guint                        scale)
{
  if (context->indicator_surface != NULL &&
      (context->indicator_surface_width < width ||
       context->indicator_surface_height < height))
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;
    }

  if (context->indicator_surface == NULL)
    context->indicator_surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32,
                                                                      width * scale,
                                                                      height * scale);

  if (cairo_surface_status (context->indicator_surface) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;

      return FALSE;
    }

  return TRUE;
}

/* Indicator is drawn with some inward offset, so that it does
 * not hug screen edges.
 */
static void
adjust_indicator_rectangle (GdkRectangle *rect,
                            gboolean      inward)
{
  double inverter;
  const int gap = AEROSNAP_INDICATOR_EDGE_GAP;
#if defined(MORE_AEROSNAP_DEBUGGING)
  GdkRectangle cache = *rect;
#endif

  if (inward)
    inverter = 1.0;
  else
    inverter = -1.0;

  rect->x += (gap * inverter);
  rect->y += (gap * inverter);
  rect->width -= (gap * 2 * inverter);
  rect->height -= (gap * 2 * inverter);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Adjusted %d x %d @ %d : %d -> %d x %d @ %d : %d\n",
                           cache.width, cache.height, cache.x, cache.y,
                           rect->width, rect->height, rect->x, rect->y));
#endif
}

static void
rounded_rectangle (cairo_t  *cr,
                   int       x,
                   int       y,
                   int       width,
                   int       height,
                   double    radius,
                   double    line_width,
                   GdkRGBA  *fill,
                   GdkRGBA  *outline)
{
  double degrees = M_PI / 180.0;

  if (fill == NULL && outline == NULL)
    return;

  cairo_save (cr);
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, (x + radius), y + height - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, (x + radius), (y + radius), radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  if (fill)
    {
      cairo_set_source_rgba (cr, fill->red, fill->green, fill->blue, fill->alpha);

      if (outline)
        cairo_fill_preserve (cr);
      else
        cairo_fill (cr);
    }

  if (outline)
    {
      cairo_set_source_rgba (cr, outline->red, outline->green, outline->blue, outline->alpha);
      cairo_set_line_width (cr, line_width);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
}

/* Translates linear animation scale into some kind of curve */
static double
curve (double val)
{
  /* TODO: try different curves. For now it's just linear */
  return val;
}

static gboolean
draw_indicator (GdkW32DragMoveResizeContext *context,
                gint64                       timestamp)
{
  cairo_t *cr;
  GdkRGBA outline = {0, 0, 1.0, 1.0};
  GdkRGBA fill = {0, 0, 1.0, 0.8};
  GdkRectangle current_rect;
  gint64 current_time = g_get_monotonic_time ();
  double animation_progress;
  gboolean last_draw;
  double line_width;
  double corner_radius;
  gint64 animation_duration;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (context->window);

  line_width = AEROSNAP_INDICATOR_LINE_WIDTH * impl->surface_scale;
  corner_radius = AEROSNAP_INDICATOR_CORNER_RADIUS;
  animation_duration = AEROSNAP_INDICATOR_ANIMATION_DURATION;
  last_draw = FALSE;

  if (timestamp == 0 &&
      current_time - context->indicator_start_time > animation_duration)
    {
      timestamp = context->indicator_start_time + animation_duration;
      last_draw = TRUE;
    }

  if (timestamp != 0)
    current_time = timestamp;

  animation_progress = (double) (current_time - context->indicator_start_time) / animation_duration;

  if (animation_progress > 1.0)
    animation_progress = 1.0;

  if (animation_progress < 0)
    animation_progress = 0;

  animation_progress = curve (animation_progress);

  current_rect = context->indicator_start;
  current_rect.x += (context->indicator_target.x - context->indicator_start.x) * animation_progress;
  current_rect.y += (context->indicator_target.y - context->indicator_start.y) * animation_progress;
  current_rect.width += (context->indicator_target.width - context->indicator_start.width) * animation_progress;
  current_rect.height += (context->indicator_target.height - context->indicator_start.height) * animation_progress;

  if (context->op == GDK_WIN32_DRAGOP_RESIZE && last_draw)
    {
      switch (context->edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_NORTH:
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          break;
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          current_rect.x = context->indicator_target.x + (context->indicator_target.width - current_rect.width);
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_NORTH_EAST:
          current_rect.x = context->indicator_target.x;
          current_rect.y = context->indicator_target.y + (context->indicator_target.height - current_rect.height);
          break;
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          current_rect.x = context->indicator_target.x;
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_SOUTH:
          current_rect.y = context->indicator_target.y;
          break;
        case GDK_SURFACE_EDGE_EAST:
          current_rect.x = context->indicator_target.x;
          break;
        }
    }

  cr = cairo_create (context->indicator_surface);
  rounded_rectangle (cr,
                     (current_rect.x - context->indicator_window_rect.x) * impl->surface_scale,
                     (current_rect.y - context->indicator_window_rect.y) * impl->surface_scale,
                     current_rect.width * impl->surface_scale,
                     current_rect.height * impl->surface_scale,
                     corner_radius,
                     line_width,
                     &fill, &outline);
  cairo_destroy (cr);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Indicator is %d x %d @ %d : %d; current time is %" G_GINT64_FORMAT "\n",
                           current_rect.width, current_rect.height,
                           current_rect.x - context->indicator_window_rect.x,
                           current_rect.y - context->indicator_window_rect.y,
                           current_time));
#endif

  return last_draw;
}

static gboolean
redraw_indicator (gpointer user_data)
{
  GdkW32DragMoveResizeContext *context = user_data;
  POINT window_position;
  SIZE window_size;
  BLENDFUNCTION blender;
  HDC hdc;
  POINT source_point = { 0, 0 };
  gboolean last_draw;
  double indicator_opacity;
  GdkWin32Surface *impl;
  gboolean do_source_remove = FALSE;

  indicator_opacity = AEROSNAP_INDICATOR_OPACITY;

  if (GDK_SURFACE_DESTROYED (context->window) ||
      !ensure_snap_indicator_exists (context))
    {
      do_source_remove = TRUE;
    }

  impl = GDK_WIN32_SURFACE (context->window);

  if (!ensure_snap_indicator_surface (context,
                                      context->indicator_window_rect.width,
                                      context->indicator_window_rect.height,
                                      impl->surface_scale))
    {
      do_source_remove = TRUE;
    }

  if (do_source_remove)
    {
      context->timer = 0;
      return G_SOURCE_REMOVE;
    }

  last_draw = draw_indicator (context, context->draw_timestamp);

  window_position.x = context->indicator_window_rect.x * impl->surface_scale;
  window_position.y = context->indicator_window_rect.y * impl->surface_scale;
  window_size.cx = context->indicator_window_rect.width * impl->surface_scale;
  window_size.cy = context->indicator_window_rect.height * impl->surface_scale;

  blender.BlendOp = AC_SRC_OVER;
  blender.BlendFlags = 0;
  blender.AlphaFormat = AC_SRC_ALPHA;
  blender.SourceConstantAlpha = 255 * indicator_opacity;

  hdc = cairo_win32_surface_get_dc (context->indicator_surface);

  API_CALL (SetWindowPos, (context->shape_indicator,
                           GDK_SURFACE_HWND (context->window),
                           0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_SHOWWINDOW | SWP_NOACTIVATE));

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Indicator window position is %ld x %ld @ %ld : %ld\n",
                           window_size.cx, window_size.cy,
                           window_position.x, window_position.y));
#endif

  API_CALL (UpdateLayeredWindow, (context->shape_indicator, NULL,
                                  &window_position, &window_size,
                                  hdc, &source_point,
                                  0, &blender, ULW_ALPHA));

  if (last_draw)
    context->timer = 0;

  return last_draw ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

static GdkRectangle
unity_of_rects (GdkRectangle a,
                GdkRectangle b)
{
  GdkRectangle u = b;

  if (a.x < u.x)
    {
      u.width += u.x - a.x;
      u.x = a.x;
    }

  if (a.y < u.y)
    {
      u.height += (u.y - a.y);
      u.y = a.y;
    }

  if (a.x + a.width > u.x + u.width)
    u.width += (a.x + a.width) - (u.x + u.width);

  if (a.y + a.height > u.y + u.height)
    u.height += (a.y + a.height) - (u.y + u.height);

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("Unified 2 rects into %d x %d @ %d : %d\n",
                           u.width, u.height, u.x, u.y));
#endif

  return u;
}

static void
start_indicator_drawing (GdkW32DragMoveResizeContext *context,
                         GdkRectangle                 from,
                         GdkRectangle                 to,
                         guint                        scale)
{
  GdkRectangle to_adjusted, from_adjusted, from_or_to;
  gint64 indicator_animation_tick = AEROSNAP_INDICATOR_ANIMATION_TICK;

  GDK_NOTE (MISC, g_print ("Start drawing snap indicator %d x %d @ %d : %d -> %d x %d @ %d : %d\n",
                           from.width * scale, from.height * scale, from.x, from.y, to.width * scale, to.height * scale, to.x, to.y));

  if (GDK_SURFACE_DESTROYED (context->window))
    return;

  if (!ensure_snap_indicator_exists (context))
    return;

  from_or_to = unity_of_rects (from, to);

  if (!ensure_snap_indicator_surface (context, from_or_to.width, from_or_to.height, scale))
    return;

  to_adjusted = to;
  adjust_indicator_rectangle (&to_adjusted, TRUE);

  from_adjusted = from;
  adjust_indicator_rectangle (&from_adjusted, TRUE);

  context->draw_timestamp = 0;
  context->indicator_start = from_adjusted;
  context->indicator_target = to_adjusted;
  context->indicator_window_rect = from_or_to;
  context->indicator_start_time = g_get_monotonic_time ();

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  context->timer = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                       indicator_animation_tick,
                                       redraw_indicator,
                                       context,
                                       NULL);
}

static void
update_fullup_indicator (GdkSurface                   *window,
                         GdkW32DragMoveResizeContext *context)
{
  SHORT maxysize;
  GdkRectangle from, to;
  GdkRectangle to_adjusted, from_adjusted, from_or_to;
  GdkWin32Surface *impl;

  GDK_NOTE (MISC, g_print ("Update fullup indicator\n"));

  if (GDK_SURFACE_DESTROYED (context->window))
    return;

  if (context->shape_indicator == NULL)
    return;

  impl = GDK_WIN32_SURFACE (window);
  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN);
  to.x = to.y = 0;
  to.width = gdk_surface_get_width (window);
  to.height = gdk_surface_get_height (window);

  to.y = 0;
  to.x = window->x;
  to.height = maxysize;
  from = context->indicator_target;

  if (context->timer == 0)
    {
      from_adjusted = from;
      adjust_indicator_rectangle (&from_adjusted, FALSE);

      GDK_NOTE (MISC, g_print ("Restart fullup animation from %d x %d @ %d : %d -> %d x %d @ %d x %d\n",
                               context->indicator_target.width, context->indicator_target.height,
                               context->indicator_target.x, context->indicator_target.y,
                               to.width, to.height, to.x, to.y));
      start_indicator_drawing (context, from_adjusted, to, impl->surface_scale);

      return;
    }

  from_or_to = unity_of_rects (from, to);

  to_adjusted = to;
  adjust_indicator_rectangle (&to_adjusted, TRUE);

  GDK_NOTE (MISC, g_print ("Retarget fullup animation %d x %d @ %d : %d -> %d x %d @ %d x %d\n",
                           context->indicator_target.width, context->indicator_target.height,
                           context->indicator_target.x, context->indicator_target.y,
                           to_adjusted.width, to_adjusted.height, to_adjusted.x, to_adjusted.y));

  context->indicator_target = to_adjusted;
  context->indicator_window_rect = from_or_to;

  ensure_snap_indicator_surface (context, from_or_to.width, from_or_to.height, impl->surface_scale);
}

static GdkMonitor *
get_monitor_at_point (GdkDisplay *display,
                      int         x,
                      int         y)
{
  GListModel *monitors;
  GdkMonitor *nearest = NULL;
  int nearest_dist = G_MAXINT;
  guint i;

  monitors = gdk_display_get_monitors (display);
  for (i = 0; i < g_list_model_get_n_items (monitors); i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;
      int dist_x, dist_y, dist;

      monitor = g_list_model_get_item (monitors, i);
      gdk_monitor_get_geometry (monitor, &geometry);

      if (x < geometry.x)
        dist_x = geometry.x - x;
      else if (geometry.x + geometry.width <= x)
        dist_x = x - (geometry.x + geometry.width) + 1;
      else
        dist_x = 0;

      if (y < geometry.y)
        dist_y = geometry.y - y;
      else if (geometry.y + geometry.height <= y)
        dist_y = y - (geometry.y + geometry.height) + 1;
      else
        dist_y = 0;

      dist = dist_x + dist_y;
      if (dist < nearest_dist)
        {
          nearest_dist = dist;
          nearest = monitor;
        }

      if (x < geometry.x)
        dist_x = geometry.x - x;
      else if (geometry.x + geometry.width <= x)
        dist_x = x - (geometry.x + geometry.width) + 1;
      else
        dist_x = 0;

      if (y < geometry.y)
        dist_y = geometry.y - y;
      else if (geometry.y + geometry.height <= y)
        dist_y = y - (geometry.y + geometry.height) + 1;
      else
        dist_y = 0;

      dist = dist_x + dist_y;
      if (dist < nearest_dist)
        {
          nearest_dist = dist;
          nearest = monitor;
        }

      g_object_unref (monitor);

      if (nearest_dist == 0)
        break;
    }

  return nearest;
}

static void
start_indicator (GdkSurface                   *window,
                 GdkW32DragMoveResizeContext *context,
                 int                          x,
                 int                          y,
                 GdkWin32AeroSnapState        state)
{
  GdkMonitor *monitor;
  GdkRectangle workarea;
  SHORT maxysize;
  GdkRectangle start_size, end_size;
  GdkDisplay *display;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  display = gdk_surface_get_display (window);
  monitor = get_monitor_at_point (display, x, y);
  gdk_win32_monitor_get_workarea (monitor, &workarea);

  maxysize = GetSystemMetrics (SM_CYVIRTUALSCREEN) / impl->surface_scale;
  start_size.x = start_size.y = 0;
  start_size.width = gdk_surface_get_width (window);
  start_size.height = gdk_surface_get_height (window);

  end_size = start_size;

  switch (state)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      return;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      end_size.x = workarea.x;
      end_size.y = workarea.y;
      end_size.width = workarea.width;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      end_size.x = workarea.x;
      end_size.y = workarea.y;
      end_size.width = workarea.width / 2;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      end_size.x = (workarea.x + workarea.width / 2);
      end_size.y = workarea.y;
      end_size.width = workarea.width / 2;
      end_size.height = workarea.height;
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      start_size.x = end_size.x = window->x;
      end_size.y = 0;
      end_size.height = maxysize;
      break;
    }

  start_indicator_drawing (context, start_size, end_size, impl->surface_scale);
}

static void
stop_indicator (GdkSurface                   *window,
                GdkW32DragMoveResizeContext *context)
{
  GDK_NOTE (MISC, g_print ("Stop drawing snap indicator\n"));

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  API_CALL (SetWindowPos, (context->shape_indicator,
                           SWP_NOZORDER_SPECIFIED,
                           0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE |
                           SWP_NOSIZE | SWP_NOREDRAW | SWP_HIDEWINDOW | SWP_NOACTIVATE));
}

static int
point_in_aerosnap_region (int                 x,
                          int                 y,
                          AeroSnapEdgeRegion *region)
{
  int edge, trigger;

  edge = (x >= region->edge.x &&
          y >= region->edge.y &&
          x <= region->edge.x + region->edge.width &&
          y <= region->edge.y + region->edge.height) ? 1 : 0;
  trigger = (x >= region->trigger.x &&
             y >= region->trigger.y &&
             x <= region->trigger.x + region->trigger.width &&
             y <= region->trigger.y + region->trigger.height) ? 1 : 0;
  return edge + trigger;
}

static void
handle_aerosnap_move_resize (GdkSurface                   *window,
                             GdkW32DragMoveResizeContext *context,
                             int                          x,
                             int                          y)
{
  int i;
  AeroSnapEdgeRegion *reg;
  int maximize = 0;
  int halfleft = 0;
  int halfright = 0;
  int fullup = 0;
  gboolean fullup_edge = FALSE;

  if (context->op == GDK_WIN32_DRAGOP_RESIZE)
    switch (context->edge)
      {
      case GDK_SURFACE_EDGE_NORTH_WEST:
      case GDK_SURFACE_EDGE_NORTH_EAST:
      case GDK_SURFACE_EDGE_WEST:
      case GDK_SURFACE_EDGE_EAST:
      case GDK_SURFACE_EDGE_SOUTH_WEST:
      case GDK_SURFACE_EDGE_SOUTH_EAST:
        break;
      case GDK_SURFACE_EDGE_SOUTH:
      case GDK_SURFACE_EDGE_NORTH:
        fullup_edge = TRUE;
        break;
      }

  for (i = 0; i < context->maximize_regions->len && maximize == 0; i++)
    {
      reg = &g_array_index (context->maximize_regions, AeroSnapEdgeRegion, i);
      maximize = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->halfleft_regions->len && halfleft == 0; i++)
    {
      reg = &g_array_index (context->halfleft_regions, AeroSnapEdgeRegion, i);
      halfleft = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->halfright_regions->len && halfright == 0; i++)
    {
      reg = &g_array_index (context->halfright_regions, AeroSnapEdgeRegion, i);
      halfright = point_in_aerosnap_region (x, y, reg);
    }

  for (i = 0; i < context->fullup_regions->len && fullup == 0; i++)
    {
      reg = &g_array_index (context->fullup_regions, AeroSnapEdgeRegion, i);
      fullup = point_in_aerosnap_region (x, y, reg);
    }

#if defined(MORE_AEROSNAP_DEBUGGING)
  GDK_NOTE (MISC, g_print ("AeroSnap: point %d : %d - max: %d, left %d, right %d, up %d\n",
                           x, y, maximize, halfleft, halfright, fullup));
#endif

  if (!context->revealed)
    {
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright == 2)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup == 2 && fullup_edge)
        {
          context->revealed = TRUE;
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_FULLUP;
          start_indicator (window, context, x, y, context->current_snap);
        }

      return;
    }

  switch (context->current_snap)
    {
    case GDK_WIN32_AEROSNAP_STATE_UNDETERMINED:
      if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_FULLUP;
          start_indicator (window, context, x, y, context->current_snap);
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_MAXIMIZE:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFLEFT:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFRIGHT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_HALFRIGHT:
      if (context->op == GDK_WIN32_DRAGOP_MOVE && halfright > 0)
        break;
      if (context->op == GDK_WIN32_DRAGOP_MOVE && maximize > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_MAXIMIZE;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else if (context->op == GDK_WIN32_DRAGOP_MOVE && halfleft > 0)
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_HALFLEFT;
          start_indicator (window, context, x, y, context->current_snap);
        }
      else
        {
          context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
          stop_indicator (window, context);
          context->revealed = FALSE;
        }
      break;
    case GDK_WIN32_AEROSNAP_STATE_FULLUP:
      if (context->op == GDK_WIN32_DRAGOP_RESIZE && fullup > 0 && fullup_edge)
        {
          update_fullup_indicator (window, context);
          break;
        }

      context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
      stop_indicator (window, context);
      break;
    }
}


static const char *
get_cursor_name_from_op (GdkW32WindowDragOp op,
                         GdkSurfaceEdge      edge)
{
  switch (op)
    {
    case GDK_WIN32_DRAGOP_MOVE:
      return "move";
    case GDK_WIN32_DRAGOP_RESIZE:
      switch (edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          return "nw-resize";
        case GDK_SURFACE_EDGE_NORTH:
          return "n-resize";
        case GDK_SURFACE_EDGE_NORTH_EAST:
          return "ne-resize";
        case GDK_SURFACE_EDGE_WEST:
          return "w-resize";
        case GDK_SURFACE_EDGE_EAST:
          return "e-resize";
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          return "sw-resize";
        case GDK_SURFACE_EDGE_SOUTH:
          return "s-resize";
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          return "se-resize";
        }
      /* default: warn about unhandled enum values,
       * fallthrough to GDK_WIN32_DRAGOP_NONE case
       */
    case GDK_WIN32_DRAGOP_COUNT:
      g_assert_not_reached ();
    case GDK_WIN32_DRAGOP_NONE:
      return "default";
    /* default: warn about unhandled enum values */
    }

  g_assert_not_reached ();

  return NULL;
}

static void
setup_drag_move_resize_context (GdkSurface                  *surface,
                                GdkW32DragMoveResizeContext *context,
                                GdkW32WindowDragOp           op,
                                GdkSurfaceEdge               edge,
                                GdkDevice                   *device,
                                int                          button,
                                double                       x,
                                double                       y,
                                guint32                      timestamp)
{
  RECT rect;
  const char *cursor_name;
  GdkSurface *pointer_surface;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  gboolean maximized = gdk_toplevel_get_state (GDK_TOPLEVEL (surface)) & GDK_TOPLEVEL_STATE_MAXIMIZED;
  int root_x, root_y;
  gboolean restore_configure = FALSE;

  gdk_win32_surface_get_root_coords (surface, x, y, &root_x, &root_y);

  /* Before we drag, we need to undo any maximization or snapping.
   * AeroSnap behaviour:
   *   If snapped halfleft/halfright:
   *     horizontal resize:
   *       resize
   *       don't unsnap
   *       keep stashed unsnapped size intact
   *     vertical resize:
   *       resize
   *       unsnap to new size (merge cached unsnapped state with current
   *                           snapped state in such a way that the gripped edge
   *                           does not move)
   *     diagonal resize:
   *       difficult to test (first move is usually either purely
   *                          horizontal or purely vertical, in which
   *                          case the above behaviour applies)
   *   If snapped up:
   *     horizontal resize:
   *       resize
   *       don't unsnap
   *       apply new width and x position to unsnapped cache,
   *         so that unsnapped surface only regains its height
   *         and y position, but inherits x and width from
   *         the fullup snapped state
   *     vertical resize:
   *       unsnap to new size (merge cached unsnapped state with current
   *                           snapped state in such a way that the gripped edge
   *                           does not move)
   *
   * This implementation behaviour:
   *   If snapped halfleft/halfright/fullup:
   *     any resize:
   *       unsnap to current size, discard cached pre-snap state
   *
   * TODO: make this implementation behave as AeroSnap on resizes?
   * There's also the case where
   * a halfleft/halfright surface isn't unsnapped when it's
   * being moved horizontally, but it's more difficult to implement.
   */
  if (op == GDK_WIN32_DRAGOP_RESIZE &&
      (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
       impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
       impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP))
    {
      discard_snapinfo (surface);
      restore_configure = TRUE;
    }
  else if (maximized ||
           (impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFRIGHT ||
            impl->snap_state == GDK_WIN32_AEROSNAP_STATE_HALFLEFT ||
            impl->snap_state == GDK_WIN32_AEROSNAP_STATE_FULLUP))
    {
      GdkMonitor *monitor;
      int wx, wy, wwidth, wheight;
      int swx, swy, swwidth, swheight;
      gboolean pointer_outside_of_surface;
      int offsetx, offsety;
      gboolean left_half;
      GdkDisplay *display;

      restore_configure = TRUE;
      display = gdk_surface_get_display (surface);
      monitor = gdk_display_get_monitor_at_surface (display, surface);
      gdk_surface_get_geometry (surface, &wx, &wy, &wwidth, &wheight);

      swx = wx;
      swy = wy;
      swwidth = wwidth;
      swheight = wheight;

      /* Subtract surface shadow. We don't want pointer to go outside of
       * the visible surface during drag-move. For drag-resize it's OK.
       * Don't take shadow into account if the surface is maximized -
       * maximized surfaces don't have shadows.
       */
      if (op == GDK_WIN32_DRAGOP_MOVE && !maximized)
        {
          swx += impl->shadow.left / impl->surface_scale;
          swy += impl->shadow.top / impl->surface_scale;
          swwidth -= impl->shadow_x;
          swheight -= impl->shadow_y;
        }

      pointer_outside_of_surface = root_x < swx || root_x > swx + swwidth ||
                                  root_y < swy || root_y > swy + swheight;
      /* Calculate the offset of the pointer relative to the surface */
      offsetx = root_x - swx;
      offsety = root_y - swy;

      /* Figure out in which half of the surface the pointer is.
       * The code currently only concerns itself with horizontal
       * dimension (left/right halves).
       * There's no upper/lower half, because usually surface
       * is dragged by its upper half anyway. If that changes, adjust
       * accordingly.
       */
      left_half = (offsetx < swwidth / 2);

      /* Inverse the offset for it to be from the right edge */
      if (!left_half)
        offsetx = swwidth - offsetx;

      GDK_NOTE (MISC, g_print ("Pointer at %d : %d, this is %d : %d relative to the surface's %s\n",
                               root_x, root_y, offsetx, offsety,
                               left_half ? "left half" : "right half"));

      /* Move surface in such a way that on unmaximization/unsnapping the pointer
       * is still pointing at the appropriate half of the surface,
       * with the same offset from the left or right edge. If the new
       * surface size is too small, and adding that offset puts the pointer
       * into the other half or even beyond, move the pointer to the middle.
       */
      if (!pointer_outside_of_surface && maximized)
        {
          WINDOWPLACEMENT placement;
          int unmax_width, unmax_height;
          int shadow_unmax_width, shadow_unmax_height;

          placement.length = sizeof (placement);
          API_CALL (GetWindowPlacement, (GDK_SURFACE_HWND (surface), &placement));

          GDK_NOTE (MISC, g_print ("W32 WM unmaximized surface placement is %ld x %ld @ %ld : %ld\n",
                                   placement.rcNormalPosition.right - placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.bottom - placement.rcNormalPosition.top,
                                   placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.top));

          unmax_width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
          unmax_height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;

          shadow_unmax_width = unmax_width - impl->shadow_x * impl->surface_scale;
          shadow_unmax_height = unmax_height - impl->shadow_y * impl->surface_scale;

          if (offsetx * impl->surface_scale < (shadow_unmax_width / 2) &&
              offsety * impl->surface_scale < (shadow_unmax_height / 2))
            {
              placement.rcNormalPosition.top = (root_y - offsety + impl->shadow.top) * impl->surface_scale;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;

              if (left_half)
                {
                  placement.rcNormalPosition.left = (root_x - offsetx + impl->shadow.left) * impl->surface_scale;
                  placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
                }
              else
                {
                  placement.rcNormalPosition.right = (root_x + offsetx + impl->shadow.right) * impl->surface_scale;
                  placement.rcNormalPosition.left = placement.rcNormalPosition.right - unmax_width;
                }
            }
          else
            {
              placement.rcNormalPosition.left = root_x * impl->surface_scale - unmax_width / 2;

              if (offsety * impl->surface_scale < shadow_unmax_height / 2)
                placement.rcNormalPosition.top = (root_y - offsety + impl->shadow.top) * impl->surface_scale;
              else
                placement.rcNormalPosition.top = root_y * impl->surface_scale - unmax_height / 2;

              placement.rcNormalPosition.right = placement.rcNormalPosition.left + unmax_width;
              placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + unmax_height;
            }

          GDK_NOTE (MISC, g_print ("Unmaximized window will be at %ld : %ld\n",
                                   placement.rcNormalPosition.left,
                                   placement.rcNormalPosition.top));

          API_CALL (SetWindowPlacement, (GDK_SURFACE_HWND (surface), &placement));
        }
      else if (!pointer_outside_of_surface && impl->snap_stash_int)
        {
          GdkRectangle new_pos;
          GdkRectangle snew_pos;

          new_pos.width = impl->snap_stash_int->width;
          new_pos.height = impl->snap_stash_int->height;
          snew_pos = new_pos;

          if (op == GDK_WIN32_DRAGOP_MOVE)
            {
              snew_pos.width -= impl->shadow_x;
              snew_pos.height -= impl->shadow_y;
            }

          if (offsetx < snew_pos.width / 2 && offsety < snew_pos.height / 2)
            {
              new_pos.y = root_y - offsety + impl->shadow.top / impl->surface_scale;

              if (left_half)
                new_pos.x = root_x - offsetx + impl->shadow.left / impl->surface_scale;
              else
                new_pos.x = root_x + offsetx + impl->shadow.left / impl->surface_scale - new_pos.width;
            }
          else
            {
              new_pos.x = root_x - new_pos.width / 2;
              new_pos.y = root_y - new_pos.height / 2;
            }

          GDK_NOTE (MISC, g_print ("Unsnapped surface to %d : %d\n",
                                   new_pos.x, new_pos.y));
          discard_snapinfo (surface);
          gdk_win32_surface_move_resize (surface, new_pos.x, new_pos.y,
                                         new_pos.width, new_pos.height);
        }


      if (maximized)
        gdk_win32_surface_unmaximize (surface);
      else
        unsnap (surface, monitor);

      if (pointer_outside_of_surface)
        {
          /* Pointer outside of the surface, move pointer into surface */
          GDK_NOTE (MISC, g_print ("Pointer at %d : %d is outside of %d x %d @ %d : %d, move it to %d : %d\n",
                                   root_x, root_y, wwidth, wheight, wx, wy, wx + wwidth / 2, wy + wheight / 2));
          root_x = wx + wwidth / 2;
          /* This is Gnome behaviour. Windows WM would put the pointer
           * in the middle of the titlebar, but GDK doesn't know where
           * the titlebar is, if any.
           */
          root_y = wy + wheight / 2;
          SetCursorPos (root_x, root_y);
        }
    }

  if (restore_configure)
    impl->inhibit_configure = FALSE;

  _gdk_win32_get_window_rect (surface, &rect);

  cursor_name = get_cursor_name_from_op (op, edge);

  context->cursor = gdk_cursor_new_from_name (cursor_name, NULL);

  pointer_surface = surface;

  /* Note: This triggers a WM_CAPTURECHANGED, which will trigger
   * gdk_win32_surface_end_move_resize_drag(), which will end
   * our op before it even begins, but only if context->op is not NONE.
   * This is why we first do the grab, *then* set the op.
   */
  gdk_device_grab (device, pointer_surface,
                   FALSE,
                   GDK_ALL_EVENTS_MASK,
                   context->cursor,
                   timestamp);

  context->window = g_object_ref (surface);
  context->op = op;
  context->edge = edge;
  context->device = device;
  context->button = button;
  context->start_root_x = root_x;
  context->start_root_y = root_y;
  context->current_root_x = root_x;
  context->current_root_y = root_y;
  context->timestamp = timestamp;
  context->start_rect = rect;

  context->shape_indicator = NULL;
  context->revealed = FALSE;
  context->halfleft_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->halfright_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->maximize_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));
  context->fullup_regions = g_array_new (FALSE, FALSE, sizeof (AeroSnapEdgeRegion));

  calculate_aerosnap_regions (context);

  GDK_NOTE (EVENTS,
            g_print ("begin drag moveresize: surface %p, toplevel %p, "
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     pointer_surface, surface,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));
}

void
gdk_win32_surface_end_move_resize_drag (GdkSurface *window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);
  GdkW32DragMoveResizeContext *context = &impl->drag_move_resize_context;
  
  if (context->op == GDK_WIN32_DRAGOP_RESIZE)
    _gdk_win32_surface_invalidate_egl_framebuffer (window);

  context->op = GDK_WIN32_DRAGOP_NONE;

  gdk_device_ungrab (context->device, GDK_CURRENT_TIME);

  g_clear_object (&context->cursor);

  context->revealed = FALSE;

  if (context->timer)
    {
      g_source_remove (context->timer);
      context->timer = 0;
    }

  g_clear_object (&context->window);

  if (context->indicator_surface)
    {
      cairo_surface_destroy (context->indicator_surface);
      context->indicator_surface = NULL;
    }

  if (context->shape_indicator)
    {
      stop_indicator (window, context);
      DestroyWindow (context->shape_indicator);
      context->shape_indicator = NULL;
    }

  g_clear_pointer (&context->halfleft_regions, g_array_unref);
  g_clear_pointer (&context->halfright_regions, g_array_unref);
  g_clear_pointer (&context->maximize_regions, g_array_unref);
  g_clear_pointer (&context->fullup_regions, g_array_unref);

  GDK_NOTE (EVENTS,
            g_print ("end drag moveresize: window %p, toplevel %p,"
                     "op %u, edge %d, device %p, "
                     "button %d, coord %d:%d, time %u\n",
                     window, window,
                     context->op, context->edge, context->device,
                     context->button, context->start_root_x,
                     context->start_root_y, context->timestamp));

  if (context->current_snap != GDK_WIN32_AEROSNAP_STATE_UNDETERMINED)
    apply_snap (window, context->current_snap);

  context->current_snap = GDK_WIN32_AEROSNAP_STATE_UNDETERMINED;
}

static void
gdk_win32_get_window_size_and_position_from_client_rect (GdkSurface *window,
                                                         RECT      *window_rect,
                                                         SIZE      *window_size,
                                                         POINT     *window_position)
{
  /* Turn client area into window area */
  _gdk_win32_adjust_client_rect (window, window_rect);

  window_position->x = window_rect->left;
  window_position->y = window_rect->top;
  window_size->cx = window_rect->right - window_rect->left;
  window_size->cy = window_rect->bottom - window_rect->top;
}

void
gdk_win32_surface_do_move_resize_drag (GdkSurface *window,
                                      int        x,
                                      int        y)
{
  RECT rect;
  RECT new_rect;
  int diffy, diffx;
  MINMAXINFO mmi;
  GdkWin32Surface *impl;
  GdkW32DragMoveResizeContext *context;
  int width;
  int height;

  impl = GDK_WIN32_SURFACE (window);
  context = &impl->drag_move_resize_context;

  if (!_gdk_win32_get_window_rect (window, &rect))
    return;

  x /= impl->surface_scale;
  y /= impl->surface_scale;

  if (context->current_root_x == x &&
      context->current_root_y == y)
    return;

  context->current_root_x = x;
  context->current_root_y = y;

  new_rect = context->start_rect;
  diffx = (x - context->start_root_x) * impl->surface_scale;
  diffy = (y - context->start_root_y) * impl->surface_scale;

  switch (context->op)
    {
    case GDK_WIN32_DRAGOP_RESIZE:

      switch (context->edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          new_rect.left += diffx;
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_NORTH:
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_NORTH_EAST:
          new_rect.right += diffx;
          new_rect.top += diffy;
          break;

        case GDK_SURFACE_EDGE_WEST:
          new_rect.left += diffx;
          break;

        case GDK_SURFACE_EDGE_EAST:
          new_rect.right += diffx;
          break;

        case GDK_SURFACE_EDGE_SOUTH_WEST:
          new_rect.left += diffx;
          new_rect.bottom += diffy;
          break;

        case GDK_SURFACE_EDGE_SOUTH:
          new_rect.bottom += diffy;
          break;

        case GDK_SURFACE_EDGE_SOUTH_EAST:
        default:
          new_rect.right += diffx;
          new_rect.bottom += diffy;
          break;
        }

      /* When handling WM_GETMINMAXINFO, mmi is already populated
       * by W32 WM and we apply our stuff on top of that.
       * Here it isn't, so we should at least clear it.
       */
      memset (&mmi, 0, sizeof (mmi));

      if (!_gdk_win32_surface_fill_min_max_info (window, &mmi))
        break;

      width = new_rect.right - new_rect.left;
      height = new_rect.bottom - new_rect.top;

      if (width > mmi.ptMaxTrackSize.x)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_WEST:
            case GDK_SURFACE_EDGE_SOUTH_WEST:
              new_rect.left = new_rect.right - mmi.ptMaxTrackSize.x;
              break;

            case GDK_SURFACE_EDGE_NORTH_EAST:
            case GDK_SURFACE_EDGE_EAST:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.right = new_rect.left + mmi.ptMaxTrackSize.x;
              break;
            }
        }
      else if (width < mmi.ptMinTrackSize.x)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_WEST:
            case GDK_SURFACE_EDGE_SOUTH_WEST:
              new_rect.left = new_rect.right - mmi.ptMinTrackSize.x;
              break;

            case GDK_SURFACE_EDGE_NORTH_EAST:
            case GDK_SURFACE_EDGE_EAST:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.right = new_rect.left + mmi.ptMinTrackSize.x;
              break;
            }
        }

      if (height > mmi.ptMaxTrackSize.y)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_NORTH:
            case GDK_SURFACE_EDGE_NORTH_EAST:
              new_rect.top = new_rect.bottom - mmi.ptMaxTrackSize.y;

            case GDK_SURFACE_EDGE_SOUTH_WEST:
            case GDK_SURFACE_EDGE_SOUTH:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.bottom = new_rect.top + mmi.ptMaxTrackSize.y;
              break;
            }
        }
      else if (height < mmi.ptMinTrackSize.y)
        {
          switch (context->edge)
            {
            case GDK_SURFACE_EDGE_NORTH_WEST:
            case GDK_SURFACE_EDGE_NORTH:
            case GDK_SURFACE_EDGE_NORTH_EAST:
              new_rect.top = new_rect.bottom - mmi.ptMinTrackSize.y;

            case GDK_SURFACE_EDGE_SOUTH_WEST:
            case GDK_SURFACE_EDGE_SOUTH:
            case GDK_SURFACE_EDGE_SOUTH_EAST:
            default:
              new_rect.bottom = new_rect.top + mmi.ptMinTrackSize.y;
              break;
            }
        }

      break;
    case GDK_WIN32_DRAGOP_MOVE:
      new_rect.left += diffx;
      new_rect.top += diffy;
      new_rect.right += diffx;
      new_rect.bottom += diffy;
      break;
    default:
      break;
    }

  if (context->op == GDK_WIN32_DRAGOP_RESIZE &&
      (rect.left != new_rect.left ||
       rect.right != new_rect.right ||
       rect.top != new_rect.top ||
       rect.bottom != new_rect.bottom))
    {
      if (GDK_IS_TOPLEVEL (window))
        {
          int scale = impl->surface_scale;

          impl->next_layout.configured_rect = new_rect;
          impl->next_layout.configured_width = (new_rect.right - new_rect.left + scale - 1) / scale;
          impl->next_layout.configured_height = (new_rect.bottom - new_rect.top + scale - 1) / scale;
        }

      context->native_move_resize_pending = TRUE;
    }
  else if (context->op == GDK_WIN32_DRAGOP_MOVE &&
           (rect.left != new_rect.left ||
            rect.top != new_rect.top))
    {
      SIZE window_size;
      POINT window_position;

      context->native_move_resize_pending = FALSE;

      gdk_surface_request_layout (window);

      gdk_win32_get_window_size_and_position_from_client_rect (window,
                                                              &new_rect,
                                                              &window_size,
                                                              &window_position);

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window),
                               SWP_NOZORDER_SPECIFIED,
                               window_position.x, window_position.y,
                               0, 0,
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE));
    }

  if (context->op == GDK_WIN32_DRAGOP_RESIZE ||
      context->op == GDK_WIN32_DRAGOP_MOVE)
    handle_aerosnap_move_resize (window, context, x, y);

  gdk_surface_request_layout (window);
}

static void
gdk_win32_toplevel_begin_resize (GdkToplevel    *toplevel,
                                 GdkSurfaceEdge  edge,
                                 GdkDevice      *device,
                                 int             button,
                                 double          x,
                                 double          y,
                                 guint32         timestamp)
{
  GdkSurface *window = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      IsIconic (GDK_SURFACE_HWND (window)))
    return;

  /* Tell Windows to start interactively resizing the window by pretending that
   * the left pointer button was clicked in the suitable edge or corner. This
   * will only work if the button is down when this function is called, and
   * will only work with button 1 (left), since Windows only allows window
   * dragging using the left mouse button.
   */

  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (window);

  setup_drag_move_resize_context (window, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_RESIZE, edge, device,
                                  button, x, y, timestamp);
}

static void
gdk_win32_toplevel_begin_move (GdkToplevel *toplevel,
                               GdkDevice   *device,
                               int          button,
                               double       x,
                               double       y,
                               guint32      timestamp)
{
  GdkSurface *window = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      IsIconic (GDK_SURFACE_HWND (window)))
    return;

  /* Tell Windows to start interactively moving the window by pretending that
   * the left pointer button was clicked in the titlebar. This will only work
   * if the button is down when this function is called, and will only work
   * with button 1 (left), since Windows only allows window dragging using the
   * left mouse button.
   */
  if (button != 1)
    return;

  impl = GDK_WIN32_SURFACE (window);

  if (impl->drag_move_resize_context.op != GDK_WIN32_DRAGOP_NONE)
    gdk_win32_surface_end_move_resize_drag (window);

  setup_drag_move_resize_context (window, &impl->drag_move_resize_context,
                                  GDK_WIN32_DRAGOP_MOVE, GDK_SURFACE_EDGE_NORTH_WEST,
                                  device, button, x, y, timestamp);
}


/*
 * Setting window states
 */
static void
gdk_win32_surface_minimize (GdkSurface *window)
{
  HWND old_active_window;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_minimize: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      old_active_window = GetActiveWindow ();
      GtkShowWindow (window, SW_MINIMIZE);
      if (old_active_window != GDK_SURFACE_HWND (window))
	SetActiveWindow (old_active_window);
    }
  else
    {
      gdk_synthesize_surface_state (window,
                                    0,
                                    GDK_TOPLEVEL_STATE_MINIMIZED);
    }
}

static void
gdk_win32_surface_maximize (GdkSurface *surface)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_maximize: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  impl = GDK_WIN32_SURFACE (surface);
  impl->inhibit_configure = TRUE;
  impl->force_recompute_size = FALSE;

  if (GDK_SURFACE_IS_MAPPED (surface))
    GtkShowWindow (surface, SW_MAXIMIZE);
  else
    gdk_synthesize_surface_state (surface,
				 0,
				 GDK_TOPLEVEL_STATE_MAXIMIZED);
}

static void
gdk_win32_surface_unmaximize (GdkSurface *surface)
{
  GdkWin32Surface *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_unmaximize: %p: %s\n",
			   GDK_SURFACE_HWND (surface),
			   _gdk_win32_surface_state_to_string (surface->state)));

  _gdk_win32_surface_invalidate_egl_framebuffer (surface);

  if (GDK_SURFACE_IS_MAPPED (surface))
    GtkShowWindow (surface, SW_RESTORE);
  else
    gdk_synthesize_surface_state (surface,
				 GDK_TOPLEVEL_STATE_MAXIMIZED,
				 0);

  impl = GDK_WIN32_SURFACE (surface);

  if (impl->inhibit_configure)
    {
      impl->inhibit_configure = FALSE;
      impl->force_recompute_size = TRUE;
    }
}

static void
gdk_win32_surface_fullscreen (GdkSurface *window,
                              GdkMonitor *monitor)
{
  int x, y, width, height;
  FullscreenInfo *fi;
  HMONITOR hmonitor = NULL;
  MONITORINFO mi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  fi = g_new (FullscreenInfo, 1);

  if (!GetWindowRect (GDK_SURFACE_HWND (window), &(fi->r)))
    g_free (fi);
  else
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      if (monitor && GDK_IS_WIN32_MONITOR (monitor))
        hmonitor = GDK_WIN32_MONITOR (monitor)->hmonitor;

      if (!hmonitor)
        hmonitor = MonitorFromWindow (GDK_SURFACE_HWND (window), MONITOR_DEFAULTTONEAREST);

      mi.cbSize = sizeof (mi);
      if (hmonitor && GetMonitorInfo (hmonitor, &mi))
	{
	  x = mi.rcMonitor.left;
	  y = mi.rcMonitor.top;
	  width = mi.rcMonitor.right - x;
	  height = mi.rcMonitor.bottom - y;
	}
      else
	{
	  x = y = 0;
	  width = GetSystemMetrics (SM_CXSCREEN);
	  height = GetSystemMetrics (SM_CYSCREEN);
	}

      /* remember for restoring */
      fi->hint_flags = impl->hint_flags;
      impl->hint_flags &= ~GDK_HINT_MAX_SIZE;
      g_object_set_data (G_OBJECT (window), "fullscreen-info", fi);
      fi->style = GetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE);

      impl->inhibit_configure = TRUE;
      impl->force_recompute_size = FALSE;

      /* Send state change before configure event */
      gdk_synthesize_surface_state (window, 0, GDK_TOPLEVEL_STATE_FULLSCREEN);

      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE,
                     (fi->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_TOP,
                x, y, width, height,
                SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_FRAMECHANGED));
    }
}

static void
gdk_win32_surface_unfullscreen (GdkSurface *window)
{
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_SURFACE (window));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      gdk_synthesize_surface_state (window, GDK_TOPLEVEL_STATE_FULLSCREEN, 0);

      impl->hint_flags = fi->hint_flags;
      SetWindowLong (GDK_SURFACE_HWND (window), GWL_STYLE, fi->style);
      _gdk_win32_surface_invalidate_egl_framebuffer (window);
      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (window), HWND_NOTOPMOST,
			       fi->r.left, fi->r.top,
			       fi->r.right - fi->r.left, fi->r.bottom - fi->r.top,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW | SWP_FRAMECHANGED));

      g_object_set_data (G_OBJECT (window), "fullscreen-info", NULL);
      g_free (fi);
      _gdk_win32_surface_update_style_bits (window);

      if (impl->inhibit_configure)
        {
          impl->inhibit_configure = FALSE;
          impl->force_recompute_size = TRUE;
        }
    }
}

static void
gdk_win32_surface_focus (GdkSurface *window,
			guint32    timestamp)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_surface_focus: %p: %s\n",
			   GDK_SURFACE_HWND (window),
			   _gdk_win32_surface_state_to_string (window->state)));

  if (window->state & GDK_TOPLEVEL_STATE_MAXIMIZED)
    GtkShowWindow (window, SW_SHOWMAXIMIZED);
  else if (window->state & GDK_TOPLEVEL_STATE_MINIMIZED)
    GtkShowWindow (window, SW_RESTORE);
  else if (!IsWindowVisible (GDK_SURFACE_HWND (window)))
    GtkShowWindow (window, SW_SHOWNORMAL);
  else
    GtkShowWindow (window, SW_SHOW);

  SetFocus (GDK_SURFACE_HWND (window));
}

/**
 * gdk_win32_surface_lookup_for_display:
 * @display: a %GdkDisplay
 * @anid: a HWND window handle
 *
 * Returns: (nullable): the %GdkSurface associated with the given @anid, or %NULL.
 */
GdkSurface *
gdk_win32_surface_lookup_for_display (GdkDisplay *display,
                                     HWND        anid)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  return (GdkSurface*) gdk_win32_handle_table_lookup_ (anid);
}

/**
 * gdk_win32_surface_is_win32:
 * @surface: a `GdkSurface`
 *
 * Returns: %TRUE if the @surface is a win32 implemented surface.
 *
 * Deprecated: 4.8: Use `GDK_IS_WIN32_SURFACE` instead.
 */
gboolean
gdk_win32_surface_is_win32 (GdkSurface *surface)
{
  return GDK_IS_WIN32_SURFACE (surface);
}

static gboolean
gdk_win32_surface_show_window_menu (GdkSurface *surface,
                                    GdkEvent  *event)
{
  double event_x, event_y;
  int x, y;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkEventType event_type = gdk_event_get_event_type (event);

  switch ((int) event_type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  gdk_event_get_position (event, &event_x, &event_y);
  gdk_win32_surface_get_root_coords (surface, event_x, event_y, &x, &y);

  SendMessage (GDK_SURFACE_HWND (surface),
               WM_SYSMENU,
               0,
               MAKELPARAM (x * impl->surface_scale, y * impl->surface_scale));

  return TRUE;
}

/**
 * gdk_win32_surface_get_impl_hwnd:
 * @surface: a `GdkSurface`
 *
 * Returns: the associated @surface HWND handle.
 *
 * Deprecated: 4.8: Use gdk_win32_surface_get_handle() instead.
 */
HWND
gdk_win32_surface_get_impl_hwnd (GdkSurface *surface)
{
  if (GDK_IS_WIN32_SURFACE (surface))
    return GDK_SURFACE_HWND (surface);
  return NULL;
}

BOOL WINAPI
GtkShowWindow (GdkSurface *window,
               int        cmd_show)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  /* Ensure that maximized window size is corrected later on */
  if (cmd_show == SW_MAXIMIZE)
    impl->maximizing = TRUE;

  return ShowWindow (GDK_SURFACE_HWND (window), cmd_show);
}

static void
gdk_win32_surface_set_shadow_width (GdkSurface *window,
                                   int        left,
                                   int        right,
                                   int        top,
                                   int        bottom)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (GDK_SURFACE_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_surface_set_shadow_width: window %p, "
                           "left %d, top %d, right %d, bottom %d\n",
                           window, left, top, right, bottom));

  impl->zero_shadow = left == 0 && right == 0 && top == 0 && bottom == 0;

  if (impl->zero_shadow)
    return;

  impl->shadow.left = left * impl->surface_scale;;
  impl->shadow.right = right * impl->surface_scale;
  impl->shadow.top = top * impl->surface_scale;;
  impl->shadow.bottom = bottom * impl->surface_scale;
  impl->shadow_x = left + right;
  impl->shadow_y = top + bottom;
}

static void
gdk_win32_surface_set_icon_list (GdkSurface *surface,
                                 GList      *textures)
{
  GdkTexture *texture, *big_texture, *small_texture;
  gint big_diff, small_diff;
  gint big_w, big_h, small_w, small_h;
  gint w, h;
  gint dw, dh, diff;
  HICON small_hicon, big_hicon;
  GdkWin32Surface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_WIN32_SURFACE (surface);

  /* ideal sizes for small and large icons */
  big_w = GetSystemMetrics (SM_CXICON);
  big_h = GetSystemMetrics (SM_CYICON);
  small_w = GetSystemMetrics (SM_CXSMICON);
  small_h = GetSystemMetrics (SM_CYSMICON);

  /* find closest sized icons in the list */
  big_texture = NULL;
  small_texture = NULL;
  big_diff = 0;
  small_diff = 0;
  while (textures)
    {
      texture = (GdkTexture*) textures->data;
      w = gdk_texture_get_width (texture);
      h = gdk_texture_get_height (texture);

      dw = ABS (w - big_w);
      dh = ABS (h - big_h);
      diff = dw*dw + dh*dh;
      if (big_texture == NULL || diff < big_diff)
        {
          big_texture = texture;
          big_diff = diff;
        }

      dw = ABS (w - small_w);
      dh = ABS (h - small_h);
      diff = dw*dw + dh*dh;
      if (small_texture == NULL || diff < small_diff)
        {
          small_texture = texture;
          small_diff = diff;
        }

      textures = textures->next;
    }

  if (big_texture == NULL || small_texture == NULL)
    return;

  /* Create the icons */
  big_hicon = big_texture ? _gdk_win32_create_hicon_for_texture (big_texture, TRUE, 0, 0) : NULL;
  small_hicon = small_texture ? _gdk_win32_create_hicon_for_texture (small_texture, TRUE, 0, 0) : NULL;

  /* Set the icons */
  SendMessage (GDK_SURFACE_HWND (surface), WM_SETICON, ICON_BIG,
               (LPARAM)big_hicon);
  SendMessage (GDK_SURFACE_HWND (surface), WM_SETICON, ICON_SMALL,
               (LPARAM)small_hicon);

  /* Store the icons, destroying any previous icons */
  if (impl->hicon_big)
    GDI_CALL (DestroyIcon, (impl->hicon_big));
  impl->hicon_big = big_hicon;
  if (impl->hicon_small)
    GDI_CALL (DestroyIcon, (impl->hicon_small));
  impl->hicon_small = small_hicon;
}

double
_gdk_win32_surface_get_scale (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkWin32Surface *impl;
  GdkWin32Display *win32_display;

  g_return_val_if_fail (surface != NULL, 1);

  display = gdk_surface_get_display (surface);
  impl = GDK_WIN32_SURFACE (surface);

  win32_display = GDK_WIN32_DISPLAY (display);

  if (win32_display->dpi_aware_type != PROCESS_DPI_UNAWARE)
    {
      if (win32_display->has_fixed_scale)
        impl->surface_scale = win32_display->surface_scale;
      else
        impl->surface_scale = gdk_win32_display_get_monitor_scale_factor (win32_display,
                                                                          surface,
                                                                          NULL);

      return impl->surface_scale;
    }
  else
    {
      if (win32_display->has_fixed_scale)
        {
          static gsize hidpi_msg_displayed = 0;

          if (g_once_init_enter (&hidpi_msg_displayed))
            {
              g_message ("Note: GDK_SCALE is ignored as HiDPI awareness is disabled.");
              g_once_init_leave (&hidpi_msg_displayed, 1);
            }
        }

      /* Application is not DPI aware, don't bother */
      return 1;
    }
}

static void
gdk_win32_surface_set_input_region (GdkSurface     *window,
                                    cairo_region_t *input_region)
{
  /* Input region support is implemented by handling the
   * WM_NCHITTEST message. */
}

static void
compute_toplevel_size (GdkSurface *surface,
                       gboolean    update_geometry,
                       int        *width,
                       int        *height)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;
  GdkToplevelSize size;
  int bounds_width, bounds_height;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

  monitor = gdk_display_get_monitor_at_surface (display, surface);
  if (monitor)
    {
      GdkRectangle workarea;

      gdk_win32_monitor_get_workarea (monitor, &workarea);
      bounds_width = workarea.width;
      bounds_height = workarea.height;
    }
  else
    {
      bounds_width = G_MAXINT;
      bounds_height = G_MAXINT;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);
  g_warn_if_fail (size.width > 0);
  g_warn_if_fail (size.height > 0);
  *width = size.width;
  *height = size.height;

  if (size.shadow.is_valid)
    {
      gdk_win32_surface_set_shadow_width (surface,
                                          size.shadow.left,
                                          size.shadow.right,
                                          size.shadow.top,
                                          size.shadow.bottom);
    }

  if (update_geometry)
    {
      GdkGeometry geometry;
      GdkSurfaceHints mask;
      GdkToplevelLayout *layout = impl->toplevel_layout;

      if (gdk_toplevel_layout_get_resizable (layout))
        {
          geometry.min_width = size.min_width;
          geometry.min_height = size.min_height;
          mask = GDK_HINT_MIN_SIZE;
        }
      else
        {
          geometry.max_width = geometry.min_width = *width;
          geometry.max_height = geometry.min_height = *height;
          mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
        }
      gdk_win32_surface_set_geometry_hints (surface, &geometry, mask);
      gdk_surface_constrain_size (&geometry, mask, *width, *height, width, height);
    }
}

static void
_gdk_win32_surface_request_layout (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int scale = impl->surface_scale;
  RECT rect;

  if (impl->drag_move_resize_context.native_move_resize_pending)
    {
      surface->width = impl->next_layout.configured_width;
      surface->height = impl->next_layout.configured_height;
    }
  else
    {
      _gdk_win32_get_window_rect (surface, &rect);

      /* Keep current position if rect is invalid (i.e. queried in bad context) */
      if (rect.right == rect.left || rect.bottom == rect.top)
        return;

      impl->next_layout.configured_width = (rect.right - rect.left + scale - 1) / scale;
      impl->next_layout.configured_height = (rect.bottom - rect.top + scale - 1) / scale;

      if (GDK_IS_TOPLEVEL (surface))
        {
          surface->x = rect.left / scale;
          surface->y = rect.top / scale;
        }
      else if (GDK_IS_POPUP (surface))
        {
          gdk_win32_surface_get_geometry (surface,
                                         &surface->x, &surface->y,
                                          NULL, NULL);
        }

      if (!impl->inhibit_configure)
        impl->force_recompute_size = TRUE;
    }
}

static gboolean
_gdk_win32_surface_compute_size (GdkSurface *surface)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int width, height;

  if (GDK_IS_TOPLEVEL (surface))
    compute_toplevel_size (surface, TRUE, &width, &height);

  if (!impl->drag_move_resize_context.native_move_resize_pending)
    {
      if (GDK_IS_TOPLEVEL (surface) && impl->force_recompute_size)
        {
          surface->width = width;
          surface->height = height;
          gdk_win32_surface_resize (surface, width, height);
          impl->force_recompute_size = FALSE;
        }
      else
        {
          surface->width = impl->next_layout.configured_width;
          surface->height = impl->next_layout.configured_height;
        }

      _gdk_surface_update_size (surface);
    }

  return FALSE;
}

static void
gdk_win32_surface_class_init (GdkWin32SurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructed = gdk_win32_surface_constructed;
  object_class->dispose = gdk_surface_win32_dispose;
  object_class->finalize = gdk_surface_win32_finalize;

  impl_class->hide = gdk_win32_surface_hide;
  impl_class->get_geometry = gdk_win32_surface_get_geometry;
  impl_class->get_device_state = gdk_surface_win32_get_device_state;
  impl_class->get_root_coords = gdk_win32_surface_get_root_coords;

  impl_class->set_input_region = gdk_win32_surface_set_input_region;
  impl_class->destroy = gdk_win32_surface_destroy;

  //impl_class->beep = gdk_x11_surface_beep;

  impl_class->destroy_notify = gdk_win32_surface_destroy_notify;
  impl_class->drag_begin = _gdk_win32_surface_drag_begin;
  impl_class->get_scale = _gdk_win32_surface_get_scale;
  impl_class->request_layout = _gdk_win32_surface_request_layout;
  impl_class->compute_size = _gdk_win32_surface_compute_size;
}

/**
 * gdk_win32_surface_get_handle:
 * @surface: (type GdkWin32Surface): a native `GdkSurface`.
 *
 * Returns the HWND handle belonging to @surface.
 *
 * Returns: the associated HWND handle.
 */
HWND
gdk_win32_surface_get_handle (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WIN32_SURFACE (surface), NULL);

  return GDK_SURFACE_HWND (surface);
}

#define LAST_PROP 1

typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32Popup;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32PopupClass;

static void gdk_win32_popup_iface_init (GdkPopupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32Popup, gdk_win32_popup, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP,
                                                gdk_win32_popup_iface_init))

static void
gdk_win32_popup_init (GdkWin32Popup *popup)
{
}

static void
gdk_win32_popup_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_popup_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_popup_class_init (GdkWin32PopupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_win32_popup_get_property;
  object_class->set_property = gdk_win32_popup_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static gboolean
gdk_win32_popup_present (GdkPopup       *popup,
                         int             width,
                         int             height,
                         GdkPopupLayout *layout)
{
  return gdk_win32_surface_present_popup (GDK_SURFACE (popup), width, height, layout);
}

static GdkGravity
gdk_win32_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_win32_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_win32_popup_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_win32_popup_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
gdk_win32_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_win32_popup_present;
  iface->get_surface_anchor = gdk_win32_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_win32_popup_get_rect_anchor;
  iface->get_position_x = gdk_win32_popup_get_position_x;
  iface->get_position_y = gdk_win32_popup_get_position_y;
}


typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32Toplevel;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32ToplevelClass;

static void gdk_win32_toplevel_iface_init (GdkToplevelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32Toplevel, gdk_win32_toplevel, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL,
                                                gdk_win32_toplevel_iface_init))

static void
gdk_win32_toplevel_constructed (GObject *object)
{
  g_signal_connect (object, "notify::state",
                    G_CALLBACK (gdk_win32_toplevel_state_callback),
                    NULL);

  G_OBJECT_CLASS (gdk_win32_toplevel_parent_class)->constructed (object);
}

static void
gdk_win32_toplevel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      gdk_win32_surface_set_title (surface, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      gdk_win32_surface_set_transient_for (surface, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      GDK_SURFACE (surface)->modal_hint = g_value_get_boolean (value);

      if (GDK_SURFACE (surface)->modal_hint)
        _gdk_push_modal_window (surface);

      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      gdk_win32_surface_set_icon_list (surface, g_value_get_pointer (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      GDK_WIN32_SURFACE (surface)->decorate_all = g_value_get_boolean (value);
      _gdk_win32_surface_update_style_bits (surface);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_toplevel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, surface->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, GDK_SURFACE (surface)->modal_hint);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      g_value_set_boolean (value, GDK_WIN32_SURFACE (surface)->decorate_all);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_toplevel_finalize (GObject *object)
{
  GdkWin32Surface *self = GDK_WIN32_SURFACE (object);

  g_signal_handlers_disconnect_by_func (self,
                                        gdk_win32_toplevel_state_callback,
                                        NULL);

  G_OBJECT_CLASS (gdk_win32_toplevel_parent_class)->finalize (object);
}

static void
gdk_win32_toplevel_class_init (GdkWin32ToplevelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = gdk_win32_toplevel_constructed;
  object_class->finalize = gdk_win32_toplevel_finalize;
  object_class->get_property = gdk_win32_toplevel_get_property;
  object_class->set_property = gdk_win32_toplevel_set_property;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
gdk_win32_toplevel_init (GdkWin32Toplevel *toplevel)
{
}

static void
gdk_win32_toplevel_present (GdkToplevel       *toplevel,
                            GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  int width, height;
  gboolean maximize;
  gboolean fullscreen;

  g_clear_pointer (&impl->toplevel_layout, gdk_toplevel_layout_unref);
  impl->toplevel_layout = gdk_toplevel_layout_copy (layout);
  compute_toplevel_size (surface, FALSE, &width, &height);
  gdk_win32_surface_resize (surface, width, height);

  if (gdk_toplevel_layout_get_maximized (layout, &maximize))
    {
      if (maximize)
        gdk_win32_surface_maximize (surface);
      else
        gdk_win32_surface_unmaximize (surface);
    }

  if (gdk_toplevel_layout_get_fullscreen (layout, &fullscreen))
    {
      if (fullscreen)
        {
          GdkMonitor *monitor;

          monitor = gdk_toplevel_layout_get_fullscreen_monitor (layout);
          gdk_win32_surface_fullscreen (surface, monitor);
        }
      else
        {
          gdk_win32_surface_unfullscreen (surface);
        }
    }

  gdk_win32_surface_show (surface, FALSE);
  maybe_notify_mapped (surface);
}

static gboolean
gdk_win32_toplevel_minimize (GdkToplevel *toplevel)
{
  gdk_win32_surface_minimize (GDK_SURFACE (toplevel));

  return TRUE;
}

static gboolean
gdk_win32_toplevel_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_win32_toplevel_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
  gdk_win32_surface_focus (GDK_SURFACE (toplevel), timestamp);
}

static gboolean
gdk_win32_toplevel_show_window_menu (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  return gdk_win32_surface_show_window_menu (GDK_SURFACE (toplevel), event);
}

static gboolean
gdk_win32_toplevel_supports_edge_constraints (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_win32_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                             GdkEvent    *gdk_event)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;
  GdkGrabStatus status;

  if (surface->shortcuts_inhibited)
    return; /* Already inhibited */

  if (!(surface->state & GDK_TOPLEVEL_STATE_FOCUSED))
    return;

  gdk_seat = gdk_surface_get_seat_from_event (surface, gdk_event);

  if (!(gdk_seat_get_capabilities (gdk_seat) & GDK_SEAT_CAPABILITY_KEYBOARD))
    return;

  status = gdk_seat_grab (gdk_seat, surface, GDK_SEAT_CAPABILITY_KEYBOARD,
                          TRUE, NULL, gdk_event, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    return;

  // TODO: install a WH_KEYBOARD_LL hook to take alt-tab/win etc.

  surface->shortcuts_inhibited = TRUE;
  surface->current_shortcuts_inhibited_seat = gdk_seat;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_win32_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);
  GdkSeat *gdk_seat;

  if (!surface->shortcuts_inhibited)
    return; /* Not inhibited */

  gdk_seat = surface->current_shortcuts_inhibited_seat;
  gdk_seat_ungrab (gdk_seat);
  surface->current_shortcuts_inhibited_seat = NULL;

  surface->shortcuts_inhibited = FALSE;
  g_object_notify (G_OBJECT (toplevel), "shortcuts-inhibited");
}

static void
gdk_win32_toplevel_state_callback (GdkSurface *surface)
{
  if (surface->state & GDK_TOPLEVEL_STATE_FOCUSED)
    return;

  if (surface->shortcuts_inhibited)
    gdk_win32_toplevel_restore_system_shortcuts (GDK_TOPLEVEL (surface));
}

static void
gdk_win32_toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_win32_toplevel_present;
  iface->minimize = gdk_win32_toplevel_minimize;
  iface->lower = gdk_win32_toplevel_lower;
  iface->focus = gdk_win32_toplevel_focus;
  iface->show_window_menu = gdk_win32_toplevel_show_window_menu;
  iface->supports_edge_constraints = gdk_win32_toplevel_supports_edge_constraints;
  iface->inhibit_system_shortcuts = gdk_win32_toplevel_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_win32_toplevel_restore_system_shortcuts;
  iface->begin_resize = gdk_win32_toplevel_begin_resize;
  iface->begin_move = gdk_win32_toplevel_begin_move;
}

typedef struct
{
  GdkWin32Surface parent_instance;
} GdkWin32DragSurface;

typedef struct
{
  GdkWin32SurfaceClass parent_class;
} GdkWin32DragSurfaceClass;

static void gdk_win32_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWin32DragSurface, gdk_win32_drag_surface, GDK_TYPE_WIN32_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_win32_drag_surface_iface_init))

static void
gdk_win32_drag_surface_init (GdkWin32DragSurface *surface)
{
}

static void
gdk_win32_drag_surface_class_init (GdkWin32DragSurfaceClass *class)
{
}

static gboolean
gdk_win32_drag_surface_present (GdkDragSurface *drag_surface,
                                  int             width,
                                  int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);

  gdk_win32_surface_resize (surface, width, height);  
  gdk_win32_surface_show (surface, FALSE);
  maybe_notify_mapped (surface);

  return TRUE;
}

static void
gdk_win32_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_win32_drag_surface_present;
}


static void
gdk_win32_surface_get_queued_window_rect (GdkSurface *surface,
                                          int         scale,
                                          RECT       *return_window_rect)
{
  RECT window_rect;

  _gdk_win32_get_window_client_area_rect (surface, scale, &window_rect);

  /* Turn client area into window area */
  _gdk_win32_adjust_client_rect (surface, &window_rect);

  *return_window_rect = window_rect;
}

static void
gdk_win32_surface_apply_queued_move_resize (GdkSurface *surface,
                                            RECT        window_rect)
{
  if (!IsIconic (GDK_SURFACE_HWND (surface)))
    {
      GDK_NOTE (EVENTS, g_print ("Setting window position ... "));

      API_CALL (SetWindowPos, (GDK_SURFACE_HWND (surface),
                               SWP_NOZORDER_SPECIFIED,
                               window_rect.left, window_rect.top,
                               window_rect.right - window_rect.left,
                               window_rect.bottom - window_rect.top,
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW));

      GDK_NOTE (EVENTS, g_print (" ... set window position\n"));

      return;
    }

  /* Don't move iconic windows */
  /* TODO: use SetWindowPlacement() to change non-minimized window position */
}

RECT
gdk_win32_surface_handle_queued_move_resize (GdkDrawContext *draw_context)
{
  GdkSurface *surface;
  GdkWin32Surface *impl;
  int scale;
  RECT queued_window_rect;

  surface = gdk_draw_context_get_surface (draw_context);
  impl = GDK_WIN32_SURFACE (surface);
  scale = gdk_surface_get_scale_factor (surface);

  gdk_win32_surface_get_queued_window_rect (surface, scale, &queued_window_rect);

  /* Apply queued resizes for non-double-buffered windows
   * before painting them (we paint on the window DC directly,
   * it must have the right size).
   * Due to some poorly-undetstood issue delayed
   * resizing of double-buffered windows can produce weird
   * artefacts, so these are also resized before we paint.
   */
  if (impl->drag_move_resize_context.native_move_resize_pending)
    {
      impl->drag_move_resize_context.native_move_resize_pending = FALSE;
      gdk_win32_surface_apply_queued_move_resize (surface, queued_window_rect);
    }

  return queued_window_rect;
}

void
_gdk_win32_surface_invalidate_egl_framebuffer (GdkSurface *surface)
{
/* If we are using ANGLE, we need to force redraw of the whole Window and its child windows
 *  as we need to re-acquire the EGL surfaces that we rendered to upload to Cairo explicitly,
 *  using gdk_window_invalidate_rect (), when we maximize or restore or use aerosnap
 */
#ifdef HAVE_EGL
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      impl->egl_force_redraw_all = TRUE;
    }
#endif
}

GdkSurface *
gdk_win32_drag_surface_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_WIN32_DRAG_SURFACE,
                       "display", display,
                       NULL);
}
