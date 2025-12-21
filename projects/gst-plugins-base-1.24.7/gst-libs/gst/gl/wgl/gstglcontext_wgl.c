/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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

#include <gst/gst.h>
#include <gmodule.h>

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include "gstglcontext_wgl.h"
#include <GL/wglext.h>

#include "../utils/opengl_versions.h"
#include "../gstglcontext_private.h"

struct _GstGLContextWGLPrivate
{
  PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
  PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;

  GstGLAPI context_api;
  const gchar *wgl_exts;
};

#define GST_CAT_DEFAULT gst_gl_context_debug

#define gst_gl_context_wgl_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstGLContextWGL, gst_gl_context_wgl,
    GST_TYPE_GL_CONTEXT);

static guintptr gst_gl_context_wgl_get_gl_context (GstGLContext * context);
static void gst_gl_context_wgl_swap_buffers (GstGLContext * context);
static gboolean gst_gl_context_wgl_choose_format (GstGLContext * context,
    GError ** error);
static gboolean gst_gl_context_wgl_activate (GstGLContext * context,
    gboolean activate);
static gboolean gst_gl_context_wgl_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error);
static void gst_gl_context_wgl_destroy_context (GstGLContext * context);
GstGLAPI gst_gl_context_wgl_get_gl_api (GstGLContext * context);
static GstGLPlatform gst_gl_context_wgl_get_gl_platform (GstGLContext *
    context);
static gboolean gst_gl_context_wgl_check_feature (GstGLContext * context,
    const gchar * feature);
GstStructure *gst_gl_context_wgl_get_config (GstGLContext * context);

static void
gst_gl_context_wgl_class_init (GstGLContextWGLClass * klass)
{
  GstGLContextClass *context_class = (GstGLContextClass *) klass;

  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_get_gl_context);
  context_class->choose_format =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_choose_format);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_context_wgl_activate);
  context_class->create_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_create_context);
  context_class->destroy_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_destroy_context);
  context_class->swap_buffers =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_swap_buffers);

  context_class->get_proc_address =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_get_proc_address);
  context_class->get_gl_api = GST_DEBUG_FUNCPTR (gst_gl_context_wgl_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_get_gl_platform);
  context_class->check_feature =
      GST_DEBUG_FUNCPTR (gst_gl_context_wgl_check_feature);
  context_class->get_config = GST_DEBUG_FUNCPTR (gst_gl_context_wgl_get_config);
}

static void
gst_gl_context_wgl_init (GstGLContextWGL * context_wgl)
{
  context_wgl->priv = gst_gl_context_wgl_get_instance_private (context_wgl);

  context_wgl->priv->context_api = GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
}

/* Must be called in the gl thread */
GstGLContextWGL *
gst_gl_context_wgl_new (GstGLDisplay * display)
{
  GstGLContextWGL *context;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_WIN32) ==
      0)
    /* we require an win32 display handle to create WGL contexts */
    return NULL;

  context = g_object_new (GST_TYPE_GL_CONTEXT_WGL, NULL);
  gst_object_ref_sink (context);

  return context;
}

static HGLRC
_create_context_with_flags (GstGLContextWGL * context_wgl, HDC dpy,
    HGLRC share_context, gint major, gint minor, gint contextFlags,
    gint profileMask)
{
  HGLRC ret;
#define N_ATTRIBS 20
  gint attribs[N_ATTRIBS];
  gint n = 0;

  if (major) {
    attribs[n++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
    attribs[n++] = major;
  }
  if (minor) {
    attribs[n++] = WGL_CONTEXT_MINOR_VERSION_ARB;
    attribs[n++] = minor;
  }
  if (contextFlags) {
    attribs[n++] = WGL_CONTEXT_FLAGS_ARB;
    attribs[n++] = contextFlags;
  }
  if (profileMask) {
    attribs[n++] = WGL_CONTEXT_PROFILE_MASK_ARB;
    attribs[n++] = profileMask;
  }
  attribs[n++] = 0;

  g_assert (n < N_ATTRIBS);
#undef N_ATTRIBS

  ret =
      context_wgl->priv->wglCreateContextAttribsARB (dpy, share_context,
      attribs);

  return ret;
}

static gboolean
gst_gl_context_wgl_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error)
{
  GstGLWindow *window;
  GstGLContextWGL *context_wgl;
  HGLRC external_gl_context = NULL;
  HGLRC trampoline;
  HDC device;

  context_wgl = GST_GL_CONTEXT_WGL (context);
  window = gst_gl_context_get_window (context);
  device = (HDC) gst_gl_window_get_display (window);

  if (other_context) {
    if (gst_gl_context_get_gl_platform (other_context) != GST_GL_PLATFORM_WGL) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Cannot share context with a non-WGL context");
      goto failure;
    }
    external_gl_context = (HGLRC) gst_gl_context_get_gl_context (other_context);
  }

  trampoline = wglCreateContext (device);
  if (trampoline)
    GST_DEBUG ("gl context created: %" G_GUINTPTR_FORMAT,
        (guintptr) trampoline);
  else {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT, "failed to create glcontext:0x%x",
        (unsigned int) GetLastError ());
    goto failure;
  }
  g_assert (trampoline);

  /* get extension functions */
  wglMakeCurrent (device, trampoline);

  context_wgl->priv->wglCreateContextAttribsARB =
      (PFNWGLCREATECONTEXTATTRIBSARBPROC)
      wglGetProcAddress ("wglCreateContextAttribsARB");
  context_wgl->priv->wglGetExtensionsStringARB =
      (PFNWGLGETEXTENSIONSSTRINGARBPROC)
      wglGetProcAddress ("wglGetExtensionsStringARB");

  wglMakeCurrent (device, 0);
  wglDeleteContext (trampoline);
  trampoline = NULL;

  if (context_wgl->priv->wglGetExtensionsStringARB) {
    context_wgl->priv->wgl_exts =
        context_wgl->priv->wglGetExtensionsStringARB (device);

    GST_DEBUG_OBJECT (context, "Available WGL extensions %s",
        GST_STR_NULL (context_wgl->priv->wgl_exts));
  }

  if (context_wgl->priv->wglCreateContextAttribsARB != NULL
      && gl_api & GST_GL_API_OPENGL3) {
    gint i;

    for (i = 0; i < G_N_ELEMENTS (opengl_versions); i++) {
      gint profileMask = 0;
      gint contextFlags = 0;

      if ((opengl_versions[i].major > 3
              || (opengl_versions[i].major == 3
                  && opengl_versions[i].minor >= 2))) {
        profileMask |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        contextFlags |= WGL_CONTEXT_DEBUG_BIT_ARB;
      } else {
        break;
      }

      GST_DEBUG_OBJECT (context, "trying to create a GL %d.%d context",
          opengl_versions[i].major, opengl_versions[i].minor);

      context_wgl->wgl_context = _create_context_with_flags (context_wgl,
          device, external_gl_context, opengl_versions[i].major,
          opengl_versions[i].minor, contextFlags, profileMask);

      if (context_wgl->wgl_context) {
        context_wgl->priv->context_api = GST_GL_API_OPENGL3;
        break;
      }
    }
  }

  if (!context_wgl->wgl_context) {

    if (context_wgl->priv->wglCreateContextAttribsARB && external_gl_context) {
      context_wgl->wgl_context =
          context_wgl->priv->wglCreateContextAttribsARB (device,
          external_gl_context, 0);
    }


    if (!context_wgl->wgl_context) {

      context_wgl->wgl_context = wglCreateContext (device);

      if (!context_wgl->wgl_context) {
        g_set_error (error, GST_GL_CONTEXT_ERROR,
            GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
            "Failed to create WGL context 0x%x",
            (unsigned int) GetLastError ());
        goto failure;
      }

      if (external_gl_context) {
        if (!wglShareLists (external_gl_context, context_wgl->wgl_context)) {
          g_set_error (error, GST_GL_CONTEXT_ERROR,
              GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
              "failed to share contexts through wglShareLists 0x%x",
              (unsigned int) GetLastError ());
          goto failure;
        }
      }
    }

    context_wgl->priv->context_api = GST_GL_API_OPENGL;
  }

  GST_LOG ("gl context id: %" G_GUINTPTR_FORMAT,
      (guintptr) context_wgl->wgl_context);

  gst_object_unref (window);

  return TRUE;

failure:
  gst_object_unref (window);

  return FALSE;
}

static void
gst_gl_context_wgl_destroy_context (GstGLContext * context)
{
  GstGLContextWGL *context_wgl;

  context_wgl = GST_GL_CONTEXT_WGL (context);

  if (context_wgl->wgl_context)
    wglDeleteContext (context_wgl->wgl_context);
  context_wgl->wgl_context = NULL;
}

static GstGLConfigSurfaceType
pfd_flags_to_surface_type (int flags)
{
  GstGLConfigSurfaceType ret = GST_GL_CONFIG_SURFACE_TYPE_NONE;

  if (flags & PFD_DRAW_TO_WINDOW)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_WINDOW;
  if (flags & PFD_DRAW_TO_BITMAP)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_PIXMAP;

  return ret;
}

static GstStructure *
pixel_format_to_structure (HDC hdc, int pixfmt)
{
  GstStructure *ret;
  PIXELFORMATDESCRIPTOR pfd;

  if (pixfmt == 0)
    return NULL;

  if (DescribePixelFormat (hdc, pixfmt, sizeof (pfd), &pfd) == 0)
    return NULL;

  ret = gst_structure_new (GST_GL_CONFIG_STRUCTURE_NAME,
      GST_GL_CONFIG_STRUCTURE_SET_ARGS (PLATFORM, GstGLPlatform,
          GST_GL_PLATFORM_WGL), GST_GL_CONFIG_STRUCTURE_SET_ARGS (RED_SIZE, int,
          pfd.cRedBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (BLUE_SIZE, int,
          pfd.cBlueBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (GREEN_SIZE, int,
          pfd.cGreenBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (ALPHA_SIZE, int,
          pfd.cAlphaBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (DEPTH_SIZE, int,
          pfd.cDepthBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (STENCIL_SIZE, int,
          pfd.cStencilBits), GST_GL_CONFIG_STRUCTURE_SET_ARGS (NATIVE_VISUAL_ID,
          guint, pixfmt), GST_GL_CONFIG_STRUCTURE_SET_ARGS (SURFACE_TYPE,
          GstGLConfigSurfaceType, pfd_flags_to_surface_type (pfd.dwFlags)),
      NULL);

  return ret;
}

static gboolean
gst_gl_context_wgl_choose_format (GstGLContext * context, GError ** error)
{
  GstGLWindow *window;
  PIXELFORMATDESCRIPTOR pfd;
  gint pixelformat = 0;
  gboolean res = FALSE;
  HDC device;
  GstStructure *config;

  window = gst_gl_context_get_window (context);
  gst_gl_window_win32_create_window (GST_GL_WINDOW_WIN32 (window), error);
  device = (HDC) gst_gl_window_get_display (window);
  gst_object_unref (window);

  pfd.nSize = sizeof (PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cRedBits = 8;
  pfd.cRedShift = 0;
  pfd.cGreenBits = 8;
  pfd.cGreenShift = 0;
  pfd.cBlueBits = 8;
  pfd.cBlueShift = 0;
  pfd.cAlphaBits = 0;
  pfd.cAlphaShift = 0;
  pfd.cAccumBits = 0;
  pfd.cAccumRedBits = 0;
  pfd.cAccumGreenBits = 0;
  pfd.cAccumBlueBits = 0;
  pfd.cAccumAlphaBits = 0;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.cAuxBuffers = 0;
  pfd.iLayerType = PFD_MAIN_PLANE;
  pfd.bReserved = 0;
  pfd.dwLayerMask = 0;
  pfd.dwVisibleMask = 0;
  pfd.dwDamageMask = 0;

  pfd.cColorBits = (BYTE) GetDeviceCaps (device, BITSPIXEL);

  pixelformat = ChoosePixelFormat (device, &pfd);

  if (!pixelformat) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "Failed to choose a pixel format");
    return FALSE;
  }

  config = pixel_format_to_structure (device, pixelformat);
  GST_INFO_OBJECT (context, "chosen config %" GST_PTR_FORMAT, config);
  gst_structure_free (config);

  res = SetPixelFormat (device, pixelformat, &pfd);

  return res;
}

static void
gst_gl_context_wgl_swap_buffers (GstGLContext * context)
{
  GstGLWindow *window = gst_gl_context_get_window (context);
  HDC device = (HDC) gst_gl_window_get_display (window);

  SwapBuffers (device);

  gst_object_unref (window);
}

static guintptr
gst_gl_context_wgl_get_gl_context (GstGLContext * context)
{
  return (guintptr) GST_GL_CONTEXT_WGL (context)->wgl_context;
}

static gboolean
gst_gl_context_wgl_activate (GstGLContext * context, gboolean activate)
{
  GstGLWindow *window;
  GstGLContextWGL *context_wgl;
  HDC device;
  gboolean result;

  window = gst_gl_context_get_window (context);
  context_wgl = GST_GL_CONTEXT_WGL (context);
  device = (HDC) gst_gl_window_get_display (window);

  if (activate) {
    result = wglMakeCurrent (device, context_wgl->wgl_context);
  } else {
    result = wglMakeCurrent (NULL, NULL);
  }

  gst_object_unref (window);

  return result;
}

GstGLAPI
gst_gl_context_wgl_get_gl_api (GstGLContext * context)
{
  GstGLContextWGL *context_wgl = GST_GL_CONTEXT_WGL (context);

  return context_wgl->priv->context_api;
}

static GstGLPlatform
gst_gl_context_wgl_get_gl_platform (GstGLContext * context)
{
  return GST_GL_PLATFORM_WGL;
}

static gboolean
gst_gl_context_wgl_check_feature (GstGLContext * context, const gchar * feature)
{
  GstGLContextWGL *context_wgl = GST_GL_CONTEXT_WGL (context);

  return gst_gl_check_extension (feature, context_wgl->priv->wgl_exts);
}

static GOnce module_opengl_dll_gonce = G_ONCE_INIT;
static GModule *module_opengl_dll;

static gpointer
load_opengl_dll_module (gpointer user_data)
{
#ifdef GST_GL_LIBGL_MODULE_NAME
  module_opengl_dll =
      g_module_open (GST_GL_LIBGL_MODULE_NAME, G_MODULE_BIND_LAZY);
#else
  if (g_strcmp0 (G_MODULE_SUFFIX, "dll") == 0)
    module_opengl_dll = g_module_open ("opengl32.dll", G_MODULE_BIND_LAZY);

  /* This automatically handles the suffix and even .la files */
  if (!module_opengl_dll)
    module_opengl_dll = g_module_open ("opengl32", G_MODULE_BIND_LAZY);
#endif

  return NULL;
}

gpointer
gst_gl_context_wgl_get_proc_address (GstGLAPI gl_api, const gchar * name)
{
  gpointer result = NULL;

  if (gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3)) {
    g_once (&module_opengl_dll_gonce, load_opengl_dll_module, NULL);
    if (module_opengl_dll)
      g_module_symbol (module_opengl_dll, name, &result);

    if (!result) {
      result = wglGetProcAddress ((LPCSTR) name);
    }
  }
  if (!result)
    result = gst_gl_context_default_get_proc_address (gl_api, name);

  return result;
}

guintptr
gst_gl_context_wgl_get_current_context (void)
{
  return (guintptr) wglGetCurrentContext ();
}

GstStructure *
gst_gl_context_wgl_get_config (GstGLContext * context)
{
  GstGLWindow *window;
  int pixfmt;
  HDC hdc;

  window = gst_gl_context_get_window (context);
  hdc = (HDC) gst_gl_window_get_display (window);

  pixfmt = GetPixelFormat (hdc);

  gst_object_unref (window);

  return pixel_format_to_structure (hdc, pixfmt);
}
