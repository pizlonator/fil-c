/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32.c: Win32 specific OpenGL wrappers
 *
 * Copyright © 2014 Emmanuele Bassi
 * Copyright © 2014 Alexander Larsson
 * Copyright © 2014 Chun-wei Fan
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"

#include "gdkwin32display.h"
#include "gdkwin32glcontext.h"
#include "gdkwin32misc.h"
#include "gdkwin32screen.h"
#include "gdkwin32surface.h"

#include "gdkglcontext.h"
#include "gdkprofilerprivate.h"
#include <glib/gi18n-lib.h>
#include "gdksurface.h"

#include <cairo.h>
#include <epoxy/wgl.h>

struct _GdkWin32GLContextWGL
{
  GdkWin32GLContext parent_instance;

  HGLRC wgl_context;
  guint do_frame_sync : 1;
};

typedef struct _GdkWin32GLContextClass    GdkWin32GLContextWGLClass;

G_DEFINE_TYPE (GdkWin32GLContextWGL, gdk_win32_gl_context_wgl, GDK_TYPE_WIN32_GL_CONTEXT)

static void
gdk_win32_gl_context_wgl_dispose (GObject *gobject)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (gobject);

  if (context_wgl->wgl_context != NULL)
    {
      if (gdk_win32_private_wglGetCurrentContext () == context_wgl->wgl_context)
        gdk_win32_private_wglMakeCurrent (NULL, NULL);

      GDK_NOTE (OPENGL, g_print ("Destroying WGL context\n"));

      gdk_win32_private_wglDeleteContext (context_wgl->wgl_context);
      context_wgl->wgl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_wgl_parent_class)->dispose (gobject);
}

static void
gdk_win32_gl_context_wgl_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkWin32Display *display_win32 = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));
  gboolean can_wait = display_win32->hasWglOMLSyncControl;
  HDC hdc;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_wgl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "win32", "swap buffers");

  if (surface != NULL)
    hdc = GDK_WIN32_SURFACE (surface)->hdc;
  else
    hdc = display_win32->dummy_context_wgl.hdc;

  if (context_wgl->do_frame_sync)
    {

      glFinish ();

      if (can_wait)
        {
          gint64 ust, msc, sbc;

          wglGetSyncValuesOML (hdc, &ust, &msc, &sbc);
          wglWaitForMscOML (hdc,
                            0,
                            2,
                            (msc + 1) % 2,
                           &ust, &msc, &sbc);
        }

    }

  SwapBuffers (hdc);
}

static void
gdk_win32_gl_context_wgl_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_win32_gl_context_wgl_begin_frame (GdkDrawContext *draw_context,
                                      GdkMemoryDepth  depth,
                                      cairo_region_t *update_area)
{
  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_wgl_parent_class)->begin_frame (draw_context, depth, update_area);
}

#define PIXEL_ATTRIBUTES 21

static int
get_wgl_pfd (HDC                    hdc,
             PIXELFORMATDESCRIPTOR *pfd,
             GdkWin32Display       *display_win32)
{
  int best_pf = 0;

  pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);

  if (display_win32 != NULL &&
      display_win32->hasWglARBPixelFormat)
    {
      UINT num_formats;
      int colorbits = GetDeviceCaps (hdc, BITSPIXEL);
      int i = 0;
      int pixelAttribs[PIXEL_ATTRIBUTES];

      /* Save up the HDC and HGLRC that we are currently using, to restore back to it when we are done here */
      HDC hdc_current = wglGetCurrentDC ();
      HGLRC hglrc_current = wglGetCurrentContext ();

      /* Update PIXEL_ATTRIBUTES above if any groups are added here! */
      pixelAttribs[i++] = WGL_DRAW_TO_WINDOW_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_SUPPORT_OPENGL_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_DOUBLE_BUFFER_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_ACCELERATION_ARB;
      pixelAttribs[i++] = WGL_FULL_ACCELERATION_ARB;

      pixelAttribs[i++] = WGL_PIXEL_TYPE_ARB;
      pixelAttribs[i++] = WGL_TYPE_RGBA_ARB;

      pixelAttribs[i++] = WGL_COLOR_BITS_ARB;
      pixelAttribs[i++] = colorbits;

      pixelAttribs[i++] = WGL_ALPHA_BITS_ARB;
      pixelAttribs[i++] = 8;

      pixelAttribs[i++] = WGL_DEPTH_BITS_ARB;
      pixelAttribs[i++] = 0;

      pixelAttribs[i++] = WGL_STENCIL_BITS_ARB;
      pixelAttribs[i++] = 0;

      pixelAttribs[i++] = WGL_ACCUM_BITS_ARB;
      pixelAttribs[i++] = 0;

      /* end of "Update PIXEL_ATTRIBUTES above if any groups are added here!" */

      pixelAttribs[i++] = 0; /* end of pixelAttribs */
      g_assert (i == PIXEL_ATTRIBUTES);

      if (!wglMakeCurrent (display_win32->dummy_context_wgl.hdc,
                           display_win32->dummy_context_wgl.hglrc))
        {
          wglMakeCurrent (hdc_current, hglrc_current);
          return 0;
        }

      wglChoosePixelFormatARB (hdc,
                               pixelAttribs,
                               NULL,
                               1,
                               &best_pf,
                               &num_formats);

      /* Go back to the HDC that we were using, since we are done with the dummy HDC and GL Context */
      wglMakeCurrent (hdc_current, hglrc_current);
    }
  else
    {
      pfd->nVersion = 1;
      pfd->dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
      pfd->iPixelType = PFD_TYPE_RGBA;
      pfd->cColorBits = GetDeviceCaps (hdc, BITSPIXEL);
      pfd->cAlphaBits = 8;
      pfd->iLayerType = PFD_MAIN_PLANE;

      best_pf = ChoosePixelFormat (hdc, pfd);
    }

  return best_pf;
}

/* in WGL, for many OpenGL items, we need a dummy WGL context, so create
 * one and cache it for later use
 */
static int
gdk_init_dummy_wgl_context (GdkWin32Display *display_win32)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;
  int best_idx = 0;

  memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

  best_idx = get_wgl_pfd (display_win32->dummy_context_wgl.hdc, &pfd, NULL);

  if (best_idx != 0)
    set_pixel_format_result = SetPixelFormat (display_win32->dummy_context_wgl.hdc,
                                              best_idx,
                                             &pfd);

  if (best_idx == 0 || !set_pixel_format_result)
    return 0;

  display_win32->dummy_context_wgl.hglrc =
    wglCreateContext (display_win32->dummy_context_wgl.hdc);

  if (display_win32->dummy_context_wgl.hglrc == NULL)
    return 0;

  return best_idx;
}

/*
 * Use a dummy HWND to init GL, sadly we can't just use the
 * HWND that we use for notifications as we may only call
 * SetPixelFormat() on an HDC once, and that notification HWND
 * uses the CS_OWNDC style meaning that even if we were to call
 * DeleteDC() on it, we would get the exact same HDC when we call
 * GetDC() on it later, meaning SetPixelFormat() cannot be used
 * again on the HDC that we acquire from the notification HWND.
 */
static HWND
create_dummy_gl_window (void)
{
  WNDCLASS wclass = { 0, };
  ATOM klass;
  HWND hwnd = NULL;

  wclass.lpszClassName = "GdkGLDummyWindow";
  wclass.lpfnWndProc = DefWindowProc;
  wclass.hInstance = this_module ();
  wclass.style = CS_OWNDC;

  klass = RegisterClass (&wclass);
  if (klass)
    {
      hwnd = CreateWindow (MAKEINTRESOURCE (klass),
                           NULL, WS_POPUP,
                           0, 0, 0, 0, NULL, NULL,
                           this_module (), NULL);
      if (!hwnd)
        {
          UnregisterClass (MAKEINTRESOURCE (klass), this_module ());
        }
    }

  return hwnd;
}

GdkGLContext *
gdk_win32_display_init_wgl (GdkDisplay  *display,
                            GError     **error)
{
  int best_idx = 0;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkGLContext *context;
  HDC hdc;

  if (!gdk_gl_backend_can_be_used (GDK_GL_WGL, error))
    return NULL;

  /* acquire and cache dummy Window (HWND & HDC) and
   * dummy GL Context, it is used to query functions
   * and used for other stuff as well
   */

  if (display_win32->dummy_context_wgl.hdc == NULL)
    {
      display_win32->dummy_context_wgl.hwnd = create_dummy_gl_window ();

      if (display_win32->dummy_context_wgl.hwnd != NULL)
        display_win32->dummy_context_wgl.hdc = GetDC (display_win32->dummy_context_wgl.hwnd);
    }

  best_idx = gdk_init_dummy_wgl_context (display_win32);
  hdc = display_win32->dummy_context_wgl.hdc;

  if (best_idx == 0 ||
     !wglMakeCurrent (hdc, display_win32->dummy_context_wgl.hglrc))
    {
      if (display_win32->dummy_context_wgl.hglrc != NULL)
        wglDeleteContext (display_win32->dummy_context_wgl.hglrc);

      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));

      return NULL;
    }

  display_win32->hasWglARBCreateContext =
    epoxy_has_wgl_extension (hdc, "WGL_ARB_create_context");
  display_win32->hasWglEXTSwapControl =
    epoxy_has_wgl_extension (hdc, "WGL_EXT_swap_control");
  display_win32->hasWglOMLSyncControl =
    epoxy_has_wgl_extension (hdc, "WGL_OML_sync_control");
  display_win32->hasWglARBPixelFormat =
    epoxy_has_wgl_extension (hdc, "WGL_ARB_pixel_format");

  context = g_object_new (GDK_TYPE_WIN32_GL_CONTEXT_WGL,
                          "display", display,
                          NULL);
  if (!gdk_gl_context_realize (context, error))
    {
      g_object_unref (context);
      return NULL;
    }

  {
    int major, minor;
    gdk_gl_context_get_version (context, &major, &minor);
    GDK_NOTE (OPENGL, g_print ("WGL API version %d.%d found\n"
                         " - Vendor: %s\n"
                         " - Checked extensions:\n"
                         "\t* WGL_ARB_pixel_format: %s\n"
                         "\t* WGL_ARB_create_context: %s\n"
                         "\t* WGL_EXT_swap_control: %s\n"
                         "\t* WGL_OML_sync_control: %s\n",
                         major, minor,
                         glGetString (GL_VENDOR),
                         display_win32->hasWglARBPixelFormat ? "yes" : "no",
                         display_win32->hasWglARBCreateContext ? "yes" : "no",
                         display_win32->hasWglEXTSwapControl ? "yes" : "no",
                         display_win32->hasWglOMLSyncControl ? "yes" : "no"));
  }

  wglMakeCurrent (NULL, NULL);

  return context;
}

/* Setup the legacy context after creating it */
static gboolean
ensure_legacy_wgl_context (HDC            hdc,
                           HGLRC          hglrc_legacy,
                           GdkGLContext  *share,
                           GdkGLVersion  *version,
                           GError       **error)
{
  GdkWin32GLContextWGL *context_wgl;
  GdkGLVersion legacy_version;

  GDK_NOTE (OPENGL,
            g_print ("Creating legacy WGL context (version:%d.%d)\n",
                      gdk_gl_version_get_major (version),
                      gdk_gl_version_get_minor (version)));

  if (!wglMakeCurrent (hdc, hglrc_legacy))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  gdk_gl_version_init_epoxy (&legacy_version);
  if (!gdk_gl_version_greater_equal (&legacy_version, version))
    {
      g_set_error (error, GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   _("WGL version %d.%d is too low, need at least %d.%d"),
                   gdk_gl_version_get_major (&legacy_version),
                   gdk_gl_version_get_minor (&legacy_version),
                   gdk_gl_version_get_major (version),
                   gdk_gl_version_get_minor (version));
      return FALSE;
    }

  *version = legacy_version;

  if (share != NULL)
    {
      context_wgl = GDK_WIN32_GL_CONTEXT_WGL (share);

      if (!wglShareLists (hglrc_legacy, context_wgl->wgl_context))
        {
          g_set_error (error, GDK_GL_ERROR,
                       GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                       _("GL implementation cannot share GL contexts"));
          return FALSE;
        }
    }

  return TRUE;
}

static HGLRC
create_wgl_context_with_attribs (HDC           hdc,
                                 GdkGLContext *share,
                                 int           flags,
                                 gboolean      is_legacy,
                                 GdkGLVersion *version)
{
  HGLRC hglrc;
  GdkWin32GLContextWGL *context_wgl;
  const GdkGLVersion *supported_versions = gdk_gl_versions_get_for_api (GDK_GL_API_GL);
  guint i;

  GDK_NOTE (OPENGL,
            g_print ("Creating %s WGL context (version:%d.%d, debug:%s, forward:%s)\n",
                      is_legacy ? "core" : "compat",
                      gdk_gl_version_get_major (version),
                      gdk_gl_version_get_minor (version),
                      (flags & WGL_CONTEXT_DEBUG_BIT_ARB) ? "yes" : "no",
                      (flags & WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB) ? "yes" : "no"));

  for (i = 0; gdk_gl_version_greater_equal (&supported_versions[i], version); i++)
    {
      /* if we have wglCreateContextAttribsARB(), create a
      * context with the compatibility profile if a legacy
      * context is requested, or when we go into fallback mode
      */
      int profile = is_legacy ? WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB :
                                WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

      int attribs[] = {
        WGL_CONTEXT_PROFILE_MASK_ARB,   profile,
        WGL_CONTEXT_MAJOR_VERSION_ARB,  gdk_gl_version_get_major (&supported_versions[i]),
        WGL_CONTEXT_MINOR_VERSION_ARB,  gdk_gl_version_get_minor (&supported_versions[i]),
        WGL_CONTEXT_FLAGS_ARB,          flags,
        0
      };

      if (share != NULL)
        context_wgl = GDK_WIN32_GL_CONTEXT_WGL (share);

      hglrc = wglCreateContextAttribsARB (hdc,
                                          share != NULL ? context_wgl->wgl_context : NULL,
                                          attribs);

      if (hglrc)
        {
          *version = supported_versions[i];
          return hglrc;
        }
    }

  return NULL;
}

static HGLRC
create_base_wgl_context (GdkWin32Display *display_win32,
                         HDC              hdc,
                         gboolean         force_create_base_context,
                         gboolean        *remove_base_context)
{
  HGLRC hglrc_base = NULL;

  if (force_create_base_context || display_win32->dummy_context_wgl.hglrc == NULL)
    {
      hglrc_base = wglCreateContext (hdc);

      if (hglrc_base == NULL)
        return NULL;

      *remove_base_context = !force_create_base_context;
    }
  else
    hglrc_base = display_win32->dummy_context_wgl.hglrc;

  return hglrc_base;
}

static HGLRC
create_wgl_context (GdkGLContext    *context,
                    GdkWin32Display *display_win32,
                    HDC              hdc,
                    GdkGLContext    *share,
                    int              flags,
                    gboolean         legacy,
                    GError         **error)
{
  /* We need a legacy context for *all* cases, if no WGL contexts are created */
  HGLRC hglrc_base, hglrc;
  GdkGLVersion version;
  gboolean remove_base_context = FALSE;
  /* Save up the HDC and HGLRC that we are currently using, to restore back to it when we are done here  */
  HDC hdc_current = wglGetCurrentDC ();
  HGLRC hglrc_current = wglGetCurrentContext ();

  hglrc = NULL;

  if (display_win32->hasWglARBCreateContext)
    {
      hglrc_base = create_base_wgl_context (display_win32,
                                            hdc,
                                            FALSE,
                                           &remove_base_context);

      if (hglrc_base == NULL || !wglMakeCurrent (hdc, hglrc_base))
        {
          g_clear_pointer (&hglrc_base, gdk_win32_private_wglDeleteContext);
          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               _("Unable to create a GL context"));
          return 0;
        }

      if (!legacy)
        {
          gdk_gl_context_get_matching_version (context,
                                               GDK_GL_API_GL,
                                               FALSE,
                                               &version);
          hglrc = create_wgl_context_with_attribs (hdc,
                                                   share,
                                                   flags,
                                                   FALSE,
                                                   &version);
        }
      if (hglrc == NULL)
        {
          legacy = TRUE;
          gdk_gl_context_get_matching_version (context,
                                               GDK_GL_API_GL,
                                               TRUE,
                                               &version);
          hglrc = create_wgl_context_with_attribs (hdc,
                                                   share,
                                                   flags,
                                                   TRUE,
                                                   &version);
        }
    }

  if (hglrc == NULL)
    {
      legacy = TRUE;
      hglrc_base = create_base_wgl_context (display_win32,
                                            hdc,
                                            TRUE,
                                           &remove_base_context);

      if (hglrc_base == NULL || !wglMakeCurrent (hdc, hglrc_base))
        {
          g_clear_pointer (&hglrc_base, gdk_win32_private_wglDeleteContext);
          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               _("Unable to create a GL context"));
          return 0;
        }

      gdk_gl_context_get_matching_version (context,
                                           GDK_GL_API_GL,
                                           TRUE,
                                           &version);
      if (ensure_legacy_wgl_context (hdc, hglrc_base, share, &version, error))
        hglrc = g_steal_pointer (&hglrc_base);
    }

  if (hglrc)
    {
      gdk_gl_context_set_version (context, &version);
      gdk_gl_context_set_is_legacy (context, legacy);
    }

  if (remove_base_context)
    g_clear_pointer (&hglrc_base, gdk_win32_private_wglDeleteContext);

  wglMakeCurrent (hdc_current, hglrc_current);

  return hglrc;
}

static gboolean
set_wgl_pixformat_for_hdc (GdkWin32Display *display_win32,
                           HDC             *hdc,
                           int             *best_idx,
                           gboolean        *recreate_dummy_context)
{
  gboolean skip_acquire = FALSE;
  gboolean set_pixel_format_result = FALSE;
  PIXELFORMATDESCRIPTOR pfd = {0};

  /* one is only allowed to call SetPixelFormat(), and so ChoosePixelFormat()
   * one single time per window HDC
   */
  GDK_NOTE (OPENGL, g_print ("requesting pixel format...\n"));
  *best_idx = get_wgl_pfd (*hdc, &pfd, display_win32);

  if (display_win32->dummy_context_wgl.hwnd != NULL)
    {
      /*
       * Ditch the initial dummy HDC, HGLRC and HWND used to initialize WGL,
       * we want to ensure that the HDC of the notification HWND that we will
       * also use for our new dummy HDC will have the correct pixel format set
       */
      wglDeleteContext (display_win32->dummy_context_wgl.hglrc);
      display_win32->dummy_context_wgl.hglrc = NULL;
      display_win32->dummy_context_wgl.hdc = GetDC (display_win32->hwnd);
      *hdc = display_win32->dummy_context_wgl.hdc;
      *recreate_dummy_context = TRUE;

      DestroyWindow (display_win32->dummy_context_wgl.hwnd);
      display_win32->dummy_context_wgl.hwnd = NULL;
    }

  if (GetPixelFormat (*hdc) != 0)
    {
      skip_acquire = TRUE;
      set_pixel_format_result = TRUE;
    }
  else if (*best_idx != 0)
    set_pixel_format_result = SetPixelFormat (*hdc, *best_idx, &pfd);

  /* ChoosePixelFormat() or SetPixelFormat() failed, bail out */
  if (*best_idx == 0 || !set_pixel_format_result)
    return FALSE;

  GDK_NOTE (OPENGL, g_print ("%s""requested and set pixel format: %d\n", skip_acquire ? "already " : "", *best_idx));

  return TRUE;
}

static GdkGLAPI
gdk_win32_gl_context_wgl_realize (GdkGLContext *context,
                                  GError **error)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);

  gboolean debug_bit, compat_bit, legacy_bit;

  /* request flags and specific versions for core (3.2+) WGL context */
  int flags = 0;
  HGLRC hglrc;
  int pixel_format = 0;
  HDC hdc;
  gboolean recreate_dummy_context = FALSE;

  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkGLContext *share = gdk_display_get_gl_context (display);

  if (!gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, error))
    return 0;

  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);

  /*
   * A legacy context cannot be shared with core profile ones, so this means we
   * must stick to a legacy context if the shared context is a legacy context
   */
  legacy_bit = share != NULL && gdk_gl_context_is_legacy (share);

  if (surface != NULL)
    hdc = GDK_WIN32_SURFACE (surface)->hdc;
  else
    hdc = display_win32->dummy_context_wgl.hdc;

  if (!set_wgl_pixformat_for_hdc (display_win32,
                                 &hdc,
                                 &pixel_format,
                                 &recreate_dummy_context))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));

      return 0;
    }

  /* if there isn't wglCreateContextAttribsARB() on WGL, use a legacy context */
  if (!legacy_bit)
    legacy_bit = !display_win32->hasWglARBCreateContext;
  if (debug_bit)
    flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
  if (compat_bit)
    flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

  hglrc = create_wgl_context (context,
                              display_win32,
                              hdc,
                              share,
                              flags,
                              legacy_bit,
                              error);

  if (recreate_dummy_context)
    {
      display_win32->dummy_context_wgl.hglrc =
        create_wgl_context (context,
                            display_win32,
                            display_win32->dummy_context_wgl.hdc,
                            NULL,
                            flags,
                            legacy_bit,
                            error);

      if (display_win32->dummy_context_wgl.hglrc == NULL)
        {
          if (hglrc != NULL)
            {
              wglDeleteContext (hglrc);
              hglrc = NULL;
            }
        }
    }

  if (hglrc == NULL)
    return 0;

  GDK_NOTE (OPENGL,
            g_print ("Created WGL context[%p], pixel_format=%d\n",
                     hglrc,
                     pixel_format));

  context_wgl->wgl_context = hglrc;

  return GDK_GL_API_GL;
}

static gboolean
gdk_win32_gl_context_wgl_clear_current (GdkGLContext *context)
{
  return gdk_win32_private_wglMakeCurrent (NULL, NULL);
}

static gboolean
gdk_win32_gl_context_wgl_is_current (GdkGLContext *context)
{
  GdkWin32GLContextWGL *self = GDK_WIN32_GL_CONTEXT_WGL (context);

  return self->wgl_context == gdk_win32_private_wglGetCurrentContext ();
}

static gboolean
gdk_win32_gl_context_wgl_make_current (GdkGLContext *context,
                                       gboolean      surfaceless)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  HDC hdc;

  if (surfaceless || surface == NULL)
    hdc = display_win32->dummy_context_wgl.hdc;
  else
    hdc = GDK_WIN32_SURFACE (surface)->hdc;

  if (!gdk_win32_private_wglMakeCurrent (hdc, context_wgl->wgl_context))
    return FALSE;

  if (!surfaceless && display_win32->hasWglEXTSwapControl)
    {
      gboolean do_frame_sync = FALSE;

      /* If there is compositing there is no particular need to delay
       * the swap when drawing on the offscreen, rendering to the screen
       * happens later anyway, and its up to the compositor to sync that
       * to the vblank. */
      do_frame_sync = ! gdk_display_is_composited (display);

      if (do_frame_sync != context_wgl->do_frame_sync)
        {
          context_wgl->do_frame_sync = do_frame_sync;

          wglSwapIntervalEXT (do_frame_sync ? 1 : 0);
        }
    }

  return TRUE;
}

static void
gdk_win32_gl_context_wgl_class_init (GdkWin32GLContextWGLClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->backend_type = GDK_GL_WGL;

  context_class->realize = gdk_win32_gl_context_wgl_realize;
  context_class->make_current = gdk_win32_gl_context_wgl_make_current;
  context_class->clear_current = gdk_win32_gl_context_wgl_clear_current;
  context_class->is_current = gdk_win32_gl_context_wgl_is_current;

  draw_context_class->begin_frame = gdk_win32_gl_context_wgl_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_wgl_end_frame;
  draw_context_class->empty_frame = gdk_win32_gl_context_wgl_empty_frame;

  gobject_class->dispose = gdk_win32_gl_context_wgl_dispose;
}

static void
gdk_win32_gl_context_wgl_init (GdkWin32GLContextWGL *wgl_context)
{
}


/**
 * gdk_win32_display_get_wgl_version:
 * @display: a `GdkDisplay`
 * @major: (out): return location for the WGL major version
 * @minor: (out): return location for the WGL minor version
 *
 * Retrieves the version of the WGL implementation.
 *
 * Returns: %TRUE if WGL is available
 */
gboolean
gdk_win32_display_get_wgl_version (GdkDisplay *display,
                                   int         *major,
                                   int         *minor)
{
  GdkGLContext *context;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_WIN32_DISPLAY (display))
    return FALSE;

  if (!gdk_gl_backend_can_be_used (GDK_GL_WGL, NULL))
    return FALSE;

  context = gdk_display_get_gl_context (display);
  if (context == NULL)
    return FALSE;

  gdk_gl_context_get_version (context, major, minor);

  return TRUE;
}
