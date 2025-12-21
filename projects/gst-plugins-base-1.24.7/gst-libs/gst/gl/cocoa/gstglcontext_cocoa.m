/*
 * GStreamer
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

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED >= 1014
# define GL_SILENCE_DEPRECATION
#endif

#include <Cocoa/Cocoa.h>

#include "gstglcontext_cocoa.h"
#include "gstgl_cocoa_private.h"

static gboolean gst_gl_context_cocoa_create_context (GstGLContext *context, GstGLAPI gl_api,
    GstGLContext * other_context, GError **error);
static void gst_gl_context_cocoa_destroy_context (GstGLContext *context);
static guintptr gst_gl_context_cocoa_get_gl_context (GstGLContext * window);
static gboolean gst_gl_context_cocoa_activate (GstGLContext * context, gboolean activate);
static GstGLAPI gst_gl_context_cocoa_get_gl_api (GstGLContext * context);
static GstGLPlatform gst_gl_context_cocoa_get_gl_platform (GstGLContext * context);
static void gst_gl_context_cocoa_swap_buffers (GstGLContext * context);
static GstStructure * gst_gl_context_cocoa_get_config (GstGLContext * context);

GST_DEBUG_CATEGORY_STATIC (gst_gl_context_cocoa_debug);
#define GST_CAT_DEFAULT gst_gl_context_cocoa_debug

G_DEFINE_TYPE_WITH_CODE (GstGLContextCocoa, gst_gl_context_cocoa,
    GST_TYPE_GL_CONTEXT,
    G_ADD_PRIVATE (GstGLContextCocoa)
    GST_DEBUG_CATEGORY_INIT (gst_gl_context_cocoa_debug, "glcontext_cocoa", 0, "Cocoa GL Context"); );

static void
gst_gl_context_cocoa_class_init (GstGLContextCocoaClass * klass)
{
  GstGLContextClass *context_class = (GstGLContextClass *) klass;

  context_class->swap_buffers =
    GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_swap_buffers);
  context_class->destroy_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_destroy_context);
  context_class->create_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_create_context);
  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_get_gl_context);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_activate);
  context_class->get_gl_api =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_get_gl_platform);
  context_class->get_config =
      GST_DEBUG_FUNCPTR (gst_gl_context_cocoa_get_config);
}

static void
gst_gl_context_cocoa_init (GstGLContextCocoa * context)
{
  context->priv = gst_gl_context_cocoa_get_instance_private (context);
}

/* Must be called in the gl thread */
GstGLContextCocoa *
gst_gl_context_cocoa_new (GstGLDisplay * display)
{
  GstGLContextCocoa *context;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_COCOA) == 0)
    /* we require an cocoa display to create CGL contexts */
    return NULL;

  context = g_object_new (GST_TYPE_GL_CONTEXT_COCOA, NULL);
  gst_object_ref_sink (context);

  return context;
}

struct pixel_attr
{
  CGLPixelFormatAttribute attr;
  const gchar *attr_name;
};

static struct pixel_attr pixel_attrs[] = {
  {kCGLPFAAllRenderers, "All Renderers"},
  {kCGLPFADoubleBuffer, "Double Buffered"},
  {kCGLPFAAuxBuffers, "Aux Buffers"},
  {kCGLPFAColorSize, "Color Size"},
  {kCGLPFAAlphaSize, "Alpha Size"},
  {kCGLPFADepthSize, "Depth Size"},
  {kCGLPFAStencilSize, "Stencil Size"},
  {kCGLPFAAccumSize, "Accum Size"},
  {kCGLPFAMinimumPolicy, "Minimum Policy"},
  {kCGLPFAMaximumPolicy, "Maximum Policy"},
  {kCGLPFASampleBuffers, "Sample Buffers"},
  {kCGLPFASamples, "Samples"},
  {kCGLPFAAuxDepthStencil, "Aux Depth Stencil"},
  {kCGLPFAColorFloat, "Color Float"},
  {kCGLPFAMultisample, "Multisample"},
  {kCGLPFASupersample, "Supersample"},
  {kCGLPFARendererID, "Renderer ID"},
  {kCGLPFANoRecovery, "No Recovery"},
  {kCGLPFAAccelerated, "Accelerated"},
  {kCGLPFAClosestPolicy, "Closest Policy"},
  {kCGLPFABackingStore, "Backing Store"},
  {kCGLPFADisplayMask, "Display Mask"},
  {kCGLPFAAllowOfflineRenderers, "Allow Offline Renderers"},
  {kCGLPFAAcceleratedCompute, "Accelerated Compute"},
  {kCGLPFAOpenGLProfile, "OpenGL Profile"},
  {kCGLPFAVirtualScreenCount, "Virtual Screen Count"},
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
  {kCGLPFAStereo, "Stereo"},
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9
  {kCGLPFACompliant, "Compliant"},
  {kCGLPFARemotePBuffer, "Remote PBuffer"},
  {kCGLPFASingleRenderer, "Single Renderer"},
  {kCGLPFAWindow, "Window"},
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
//  {kCGLPFAOffScreen, "Off Screen"},
//  {kCGLPFAPBuffer, "PBuffer"},
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
//  {kCGLPFAFullScreen, "Full Screen"},
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
//  {kCGLPFAMPSafe, "MP Safe"},
//  {kCGLPFAMultiScreen, "Multi Screen"},
//  {kCGLPFARobust, "Robust"},
#endif
};

void
gst_gl_context_cocoa_dump_pixel_format (CGLPixelFormatObj fmt)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (pixel_attrs); i++) {
    gint val;
    CGLError ret = CGLDescribePixelFormat (fmt, 0, pixel_attrs[i].attr, &val);

    if (ret != kCGLNoError) {
      GST_WARNING ("failed to get pixel format %p attribute %s", fmt, pixel_attrs[i].attr_name);
    } else {
      GST_DEBUG ("Pixel format %p attr %s = %i", fmt, pixel_attrs[i].attr_name,
          val);
    }
  }
}

CGLPixelFormatObj
gst_gl_context_cocoa_get_pixel_format (GstGLContextCocoa *context)
{
  if (context->priv->pixel_format)
    return CGLRetainPixelFormat (context->priv->pixel_format);
  return NULL;
}

static GstStructure *
cgl_pixel_format_to_structure (CGLPixelFormatObj fmt)
{
  GstStructure *ret;
  int val, alpha;

  ret = gst_structure_new (GST_GL_CONFIG_STRUCTURE_NAME,
      GST_GL_CONFIG_STRUCTURE_SET_ARGS(PLATFORM, GstGLPlatform, GST_GL_PLATFORM_CGL),
      NULL);

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFAAlphaSize, &alpha) != kCGLNoError)
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(ALPHA_SIZE, int, alpha), NULL);

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFADepthSize, &val) != kCGLNoError)
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(DEPTH_SIZE, int, val), NULL);

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFAStencilSize, &val) != kCGLNoError)
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(STENCIL_SIZE, int, val), NULL);

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFAColorSize, &val) != kCGLNoError)
    goto failure;
  val -= alpha;
  if (val % 3 == 0) {
    /* XXX: assumes that bits are evenly distributed */
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(RED_SIZE, int, val / 3), NULL);
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(GREEN_SIZE, int, val / 3), NULL);
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(BLUE_SIZE, int, val / 3), NULL);
  } else {
    GST_WARNING ("Don't know how to split a color size of %u into R,G,B values",
        val);
    goto failure;
  }

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFASamples, &val) != kCGLNoError)
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(SAMPLES, int, val), NULL);

  if (CGLDescribePixelFormat (fmt, 0, kCGLPFASampleBuffers, &val) != kCGLNoError)
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(SAMPLE_BUFFERS, int, val), NULL);

  return ret;

failure:
  gst_structure_free (ret);
  return NULL;
}

static gboolean
gst_gl_context_cocoa_create_context (GstGLContext *context, GstGLAPI gl_api,
    GstGLContext *other_context, GError **error)
{
  GstGLContextCocoa *context_cocoa = GST_GL_CONTEXT_COCOA (context);
  GstGLContextCocoaPrivate *priv = context_cocoa->priv;
  GstGLWindow *window = gst_gl_context_get_window (context);
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (window);
  GstGLAPI context_api = GST_GL_API_NONE;
  const GLint swapInterval = 1;
  CGLPixelFormatObj fmt = NULL;
  CGLContextObj glContext;
  CGLPixelFormatAttribute attribs[] = {
    kCGLPFADoubleBuffer,
    kCGLPFAAccumSize, 32,
    0
  };
  CGLError ret;
  gint pix_fmt_i = 0;
  gint npix;

  if ((gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3)) == GST_GL_API_NONE) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "The CGL backend only supports GL and GL3");
    goto error;
  }

  priv->gl_context = nil;
  if (other_context)
    priv->external_gl_context = (CGLContextObj) gst_gl_context_get_gl_context (other_context);
  else
    priv->external_gl_context = NULL;

  if (priv->external_gl_context) {
    gint profile;

    fmt = CGLGetPixelFormat (priv->external_gl_context);
    CGLRetainPixelFormat (fmt);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    /* core profile is only available in >= 10.7 */
    if (kCGLNoError == CGLDescribePixelFormat (fmt, 0, kCGLPFAOpenGLProfile,
          &profile)) {
      if (profile == kCGLOGLPVersion_3_2_Core) {
        context_api = GST_GL_API_OPENGL3;
      } else {
        context_api =GST_GL_API_OPENGL;
      }
    }
#else
    context_api = GST_GL_API_OPENGL;
#endif
  }

  if (!fmt) {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    if (gl_api & GST_GL_API_OPENGL3) {
      attribs[pix_fmt_i++] = kCGLPFAOpenGLProfile;
      attribs[pix_fmt_i++] = (int) kCGLOGLPVersion_3_2_Core;
      context_api = GST_GL_API_OPENGL3;
    } else {
      context_api = GST_GL_API_OPENGL;
    }
#else
    context_api = GST_GL_API_OPENGL;
#endif

    attribs[pix_fmt_i++] = 0;

    ret = CGLChoosePixelFormat (attribs, &fmt, &npix);
    if (ret != kCGLNoError) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG, "cannot choose a pixel format: %s", CGLErrorString (ret));
      goto error;
    }
  }

  gst_gl_context_cocoa_dump_pixel_format (fmt);

  ret = CGLCreateContext (fmt, priv->external_gl_context, &glContext);
  if (ret != kCGLNoError) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "failed to create context: %s", CGLErrorString (ret));
    CGLReleasePixelFormat (fmt);
    goto error;
  }

  context_cocoa->priv->pixel_format = fmt;
  context_cocoa->priv->gl_context = glContext;

  _gst_gl_invoke_on_main ((GstGLWindowCB) gst_gl_window_cocoa_create_window,
      gst_object_ref (window_cocoa), (GDestroyNotify) gst_object_unref);

  if (!context_cocoa->priv->gl_context) {
    goto error;
  }

  GST_INFO_OBJECT (context, "GL context created: %p", context_cocoa->priv->gl_context);

  CGLSetCurrentContext (context_cocoa->priv->gl_context);

  /* Back and front buffers are swapped only during the vertical retrace of the monitor.
   * Discarded if you configured your driver to Never-use-V-Sync.
   */
  CGLSetParameter (context_cocoa->priv->gl_context, kCGLCPSwapInterval, &swapInterval);

  context_cocoa->priv->context_api = context_api;

  if (window)
    gst_object_unref (window);

  return TRUE;

error:
  {
    if (window)
      gst_object_unref (window);
    return FALSE;
  }
}

static void
gst_gl_context_cocoa_swap_buffers (GstGLContext * context)
{
}

static void
gst_gl_context_cocoa_destroy_context (GstGLContext *context)
{
  GstGLContextCocoa *context_cocoa = GST_GL_CONTEXT_COCOA (context);

  if (context_cocoa->priv->gl_context != NULL) {
    CGLDestroyContext(context_cocoa->priv->gl_context);
    context_cocoa->priv->gl_context = NULL;
  }

  if (context_cocoa->priv->pixel_format) {
    CGLReleasePixelFormat(context_cocoa->priv->pixel_format);
    context_cocoa->priv->pixel_format = NULL;
  }
}

static guintptr
gst_gl_context_cocoa_get_gl_context (GstGLContext * context)
{
  return (guintptr) GST_GL_CONTEXT_COCOA (context)->priv->gl_context;
}

static gboolean
gst_gl_context_cocoa_activate (GstGLContext * context, gboolean activate)
{
  GstGLContextCocoa *context_cocoa = GST_GL_CONTEXT_COCOA (context);
  gpointer context_handle = activate ? context_cocoa->priv->gl_context : NULL;

  return kCGLNoError == CGLSetCurrentContext (context_handle);
}

static GstGLAPI
gst_gl_context_cocoa_get_gl_api (GstGLContext * context)
{
  GstGLContextCocoa *context_cocoa = GST_GL_CONTEXT_COCOA (context);

  if (context_cocoa->priv->gl_context)
    return context_cocoa->priv->context_api;

  return GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
}

static GstGLPlatform
gst_gl_context_cocoa_get_gl_platform (GstGLContext * context)
{
  return GST_GL_PLATFORM_CGL;
}

guintptr
gst_gl_context_cocoa_get_current_context (void)
{
  return (guintptr) CGLGetCurrentContext ();
}

static GstStructure *
gst_gl_context_cocoa_get_config (GstGLContext * context)
{
  GstGLContextCocoa *cocoa = GST_GL_CONTEXT_COCOA (context);

  g_return_val_if_fail (cocoa->priv->pixel_format, NULL);

  return cgl_pixel_format_to_structure (cocoa->priv->pixel_format);
}
