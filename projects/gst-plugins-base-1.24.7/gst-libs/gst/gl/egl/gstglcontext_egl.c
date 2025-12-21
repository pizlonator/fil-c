/*
 * GStreamer
 * Copyright (C) 2013 Sebastian Dröge <slomo@circular-chaos.org>
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

#include <gmodule.h>

/* FIXME: Sharing contexts requires the EGLDisplay to be the same
 * may need to box it.
 */

#include "gstglcontext_egl.h"

#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext_private.h>

#include "gstegl.h"
#include "../utils/opengl_versions.h"
#include "../utils/gles_versions.h"

#if GST_GL_HAVE_WINDOW_X11
#include "../x11/gstglwindow_x11.h"
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
#include "../wayland/gstglwindow_wayland_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_WIN32
#include "../win32/gstglwindow_win32.h"
#endif
#if GST_GL_HAVE_WINDOW_DISPMANX
#include "../dispmanx/gstglwindow_dispmanx_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_GBM
#include "../gbm/gstglwindow_gbm_egl.h"
#endif
#if GST_GL_HAVE_WINDOW_VIV_FB
#include "../viv-fb/gstglwindow_viv_fb_egl.h"
#endif

#define GST_CAT_DEFAULT gst_gl_context_debug

typedef struct _GstGLDmaFormat GstGLDmaFormat;

/**
 * GstGLDmaFormat: (skip)
 *
 * Opaque struct
 */
struct _GstGLDmaFormat
{
  gint fourcc;
  GArray *modifiers;
};

static gboolean gst_gl_context_egl_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error);
static void gst_gl_context_egl_destroy_context (GstGLContext * context);
static gboolean gst_gl_context_egl_choose_format (GstGLContext * context,
    GError ** error);

static gboolean gst_gl_context_egl_activate (GstGLContext * context,
    gboolean activate);
static void gst_gl_context_egl_swap_buffers (GstGLContext * context);
static guintptr gst_gl_context_egl_get_gl_context (GstGLContext * context);
static GstGLAPI gst_gl_context_egl_get_gl_api (GstGLContext * context);
static GstGLPlatform gst_gl_context_egl_get_gl_platform (GstGLContext *
    context);
static gboolean gst_gl_context_egl_check_feature (GstGLContext * context,
    const gchar * feature);
static void gst_gl_context_egl_get_gl_platform_version (GstGLContext * context,
    gint * major, gint * minor);
static GstStructure *gst_gl_context_egl_get_config (GstGLContext * context);
static gboolean gst_gl_context_egl_request_config (GstGLContext * context,
    GstStructure * config);

G_DEFINE_TYPE (GstGLContextEGL, gst_gl_context_egl, GST_TYPE_GL_CONTEXT);

static void
gst_gl_context_egl_class_init (GstGLContextEGLClass * klass)
{
  GstGLContextClass *context_class = (GstGLContextClass *) klass;

  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_gl_context);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_context_egl_activate);
  context_class->create_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_create_context);
  context_class->destroy_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_destroy_context);
  context_class->choose_format =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_choose_format);
  context_class->swap_buffers =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_swap_buffers);

  context_class->get_gl_api = GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_gl_platform);
  context_class->get_proc_address =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_proc_address);
  context_class->check_feature =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_check_feature);
  context_class->get_current_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_current_context);
  context_class->get_gl_platform_version =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_gl_platform_version);
  context_class->get_config = GST_DEBUG_FUNCPTR (gst_gl_context_egl_get_config);
  context_class->request_config =
      GST_DEBUG_FUNCPTR (gst_gl_context_egl_request_config);
}

static void
gst_gl_context_egl_init (GstGLContextEGL * context)
{
}

/* Must be called in the gl thread */
GstGLContextEGL *
gst_gl_context_egl_new (GstGLDisplay * display)
{
  GstGLContextEGL *context;

  /* XXX: display type could theoretically be anything, as long as
   * eglGetDisplay supports it. */
  context = g_object_new (GST_TYPE_GL_CONTEXT_EGL, NULL);
  gst_object_ref_sink (context);

  return context;
}

static gboolean
gst_gl_context_egl_choose_format (GstGLContext * context, GError ** error)
{
#if GST_GL_HAVE_WINDOW_X11
  if (GST_IS_GL_WINDOW_X11 (context->window)) {
    GstGLWindow *window = gst_gl_context_get_window (context);
    GstGLWindowX11 *window_x11 = GST_GL_WINDOW_X11 (window);
    gint ret;

    window_x11->visual_info = g_new0 (XVisualInfo, 1);
    ret = XMatchVisualInfo (window_x11->device, window_x11->screen_num,
        window_x11->depth, TrueColor, window_x11->visual_info);

    gst_object_unref (window);

    if (ret == 0) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG, "Failed to match XVisualInfo");
      return FALSE;
    }
  }
#endif

  return TRUE;
}

static GstGLAPI
egl_conformant_to_gst (int conformant)
{
  GstGLAPI ret = GST_GL_API_NONE;

  if (conformant & EGL_OPENGL_BIT)
    ret |= GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
  if (conformant & EGL_OPENGL_ES_BIT)
    ret |= GST_GL_API_GLES1;
  if (conformant & EGL_OPENGL_ES2_BIT)
    ret |= GST_GL_API_GLES2;
#if defined(EGL_KHR_create_context)
  if (conformant & EGL_OPENGL_ES3_BIT_KHR)
    /* FIXME: need another gles3 value? */
    ret |= GST_GL_API_GLES2;
#endif
#if 0
  if (conformant & EGL_OPENVG_BIT)
    conformant_values[i++] = "OpenVG";
#endif

  return ret;
}

static GstGLConfigSurfaceType
egl_surface_type_to_gst (int surface)
{
  GstGLConfigSurfaceType ret = GST_GL_CONFIG_SURFACE_TYPE_NONE;

  if (surface & EGL_WINDOW_BIT)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_WINDOW;
  if (surface & EGL_PBUFFER_BIT)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_PBUFFER;
#if 0
  if (surface & EGL_MULTISAMPLE_RESOLVE_BOX_BIT)
    surface_values[i++] = "multisample-resolve-box";
  if (surface & EGL_SWAP_BEHAVIOR_PRESERVED_BIT)
    surface_values[i++] = "swap-behaviour-preserved";
  if (surface & EGL_VG_ALPHA_FORMAT_PRE_BIT)
    surface_values[i++] = "vg-alpha-format-pre";
  if (surface & EGL_VG_COLORSPACE_LINEAR_BIT)
    surface_values[i++] = "vg-colorspace-linear";
#endif
  return ret;
}

static GstGLConfigCaveat
egl_caveat_to_gst (int caveat)
{
  switch (caveat) {
    case EGL_NONE:
      return GST_GL_CONFIG_CAVEAT_NONE;
    case EGL_SLOW_CONFIG:
      return GST_GL_CONFIG_CAVEAT_SLOW;
    case EGL_NON_CONFORMANT_CONFIG:
      return GST_GL_CONFIG_CAVEAT_NON_CONFORMANT;
    default:
      GST_WARNING ("unknown EGL caveat value %u (0x%x)", caveat, caveat);
      return GST_GL_CONFIG_CAVEAT_NON_CONFORMANT;
  }
}

static GstStructure *
egl_config_to_structure (EGLDisplay egl_display, EGLConfig config)
{
  GstStructure *ret;
  int val;
  int buffer_type;

  if (!egl_display)
    return NULL;

  ret = gst_structure_new (GST_GL_CONFIG_STRUCTURE_NAME,
      GST_GL_CONFIG_STRUCTURE_SET_ARGS (PLATFORM, GstGLPlatform,
          GST_GL_PLATFORM_EGL), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_CONFIG_ID, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (CONFIG_ID, int,
          val), NULL);

#if 0
  {
    /* Don't know how to translate this value, it's platform and implementation
     * dependant
     */
    int native_visual_type;
    if (!eglGetConfigAttrib (egl_display, config, EGL_NATIVE_VISUAL_TYPE,
            &native_visual_type))
      goto failure;
  }
#endif

  if (!eglGetConfigAttrib (egl_display, config, EGL_NATIVE_VISUAL_ID, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (NATIVE_VISUAL_ID,
          guint, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_NATIVE_RENDERABLE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (NATIVE_RENDERABLE,
          gboolean, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_CONFORMANT, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (CONFORMANT_API,
          GstGLAPI, egl_conformant_to_gst (val)), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_RENDERABLE_TYPE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (RENDERABLE_API,
          GstGLAPI, egl_conformant_to_gst (val)), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_SURFACE_TYPE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (SURFACE_TYPE,
          GstGLConfigSurfaceType, egl_surface_type_to_gst (val)), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_CONFIG_CAVEAT, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (CAVEAT,
          GstGLConfigCaveat, egl_caveat_to_gst (val)), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_LEVEL, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (LEVEL, int, val),
      NULL);


  if (!eglGetConfigAttrib (egl_display, config, EGL_COLOR_BUFFER_TYPE,
          &buffer_type))
    goto failure;

  if (buffer_type == EGL_RGB_BUFFER) {
    if (!eglGetConfigAttrib (egl_display, config, EGL_RED_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (RED_SIZE, int,
            val), NULL);

    if (!eglGetConfigAttrib (egl_display, config, EGL_GREEN_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (GREEN_SIZE, int,
            val), NULL);

    if (!eglGetConfigAttrib (egl_display, config, EGL_BLUE_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (BLUE_SIZE, int,
            val), NULL);

    if (!eglGetConfigAttrib (egl_display, config, EGL_ALPHA_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (ALPHA_SIZE, int,
            val), NULL);
  } else if (buffer_type == EGL_LUMINANCE_BUFFER) {
    if (!eglGetConfigAttrib (egl_display, config, EGL_LUMINANCE_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (LUMINANCE_SIZE,
            int, val), NULL);

    if (!eglGetConfigAttrib (egl_display, config, EGL_ALPHA_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (ALPHA_SIZE, int,
            val), NULL);
  } else {
    GST_WARNING ("unknown EGL_COLOR_BUFFER_TYPE value %x", buffer_type);
    goto failure;
  }

  if (!eglGetConfigAttrib (egl_display, config, EGL_DEPTH_SIZE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (DEPTH_SIZE, int,
          val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_STENCIL_SIZE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (STENCIL_SIZE, int,
          val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_MIN_SWAP_INTERVAL, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MIN_SWAP_INTERVAL,
          int, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_MAX_SWAP_INTERVAL, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_SWAP_INTERVAL,
          int, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_MAX_PBUFFER_WIDTH, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_WIDTH,
          int, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_MAX_PBUFFER_HEIGHT, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_HEIGHT,
          int, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_MAX_PBUFFER_PIXELS, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_PIXELS,
          int, val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_SAMPLE_BUFFERS, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (SAMPLE_BUFFERS, int,
          val), NULL);

  if (!eglGetConfigAttrib (egl_display, config, EGL_SAMPLES, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (SAMPLES, int, val),
      NULL);

  return ret;

failure:
  gst_structure_free (ret);
  return NULL;
}

static void
gst_gl_context_egl_dump_config (GstGLContextEGL * egl, EGLConfig config)
{
  int id;
  int buffer_type;

  if (!egl->egl_display)
    return;

  {
    int native_visual_id, native_visual_type;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_CONFIG_ID, &id))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_NATIVE_VISUAL_ID,
            &native_visual_id))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_NATIVE_VISUAL_TYPE,
            &native_visual_type))
      return;
    GST_DEBUG_OBJECT (egl, "dumping EGLConfig %p with id 0x%x and "
        "native visual id 0x%x of type 0x%x", config, id, native_visual_id,
        native_visual_type);
  }

  {
#define MAX_CONFORMANT 8
    int conformant, i = 0;
    const char *conformant_values[MAX_CONFORMANT] = { NULL, };
    char *conformant_str = NULL;;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_CONFORMANT,
            &conformant))
      return;

    if (conformant & EGL_OPENGL_BIT)
      conformant_values[i++] = "OpenGL";
    if (conformant & EGL_OPENGL_ES_BIT)
      conformant_values[i++] = "OpenGL ES";
    if (conformant & EGL_OPENGL_ES2_BIT)
      conformant_values[i++] = "OpenGL ES 2.x";
#if defined(EGL_KHR_create_context)
    if (conformant & EGL_OPENGL_ES3_BIT_KHR)
      conformant_values[i++] = "OpenGL ES 3.x";
#endif
    if (conformant & EGL_OPENVG_BIT)
      conformant_values[i++] = "OpenVG";

    /* bad things have happened if this fails: we haven't allocated enough
     * space to hold all the values */
    g_assert (i < MAX_CONFORMANT);

    conformant_str = g_strjoinv ("|", (char **) conformant_values);
    GST_DEBUG_OBJECT (egl, "Conformant for %s", conformant_str);
    g_free (conformant_str);
#undef MAX_CONFORMANT
  }

  {
#define MAX_RENDERABLE 8
    int renderable, i = 0;
    const char *renderable_values[MAX_RENDERABLE] = { NULL, };
    char *renderable_str = NULL;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_RENDERABLE_TYPE,
            &renderable))
      return;

    if (renderable & EGL_OPENGL_BIT)
      renderable_values[i++] = "OpenGL";
    if (renderable & EGL_OPENGL_ES_BIT)
      renderable_values[i++] = "OpenGL ES";
    if (renderable & EGL_OPENGL_ES2_BIT)
      renderable_values[i++] = "OpenGL ES 2.x";
#if defined(EGL_KHR_create_context)
    if (renderable & EGL_OPENGL_ES3_BIT_KHR)
      renderable_values[i++] = "OpenGL ES 3.x";
#endif
    if (renderable & EGL_OPENVG_BIT)
      renderable_values[i++] = "OpenVG";

    /* bad things have happened if this fails: we haven't allocated enough
     * space to hold all the values */
    g_assert (i < MAX_RENDERABLE);

    renderable_str = g_strjoinv ("|", (char **) renderable_values);
    GST_DEBUG_OBJECT (egl, "Renderable for %s", renderable_str);
    g_free (renderable_str);
#undef MAX_RENDERABLE
  }

  {
#define MAX_SURFACE 8
    int surface, i = 0;
    const char *surface_values[MAX_SURFACE] = { NULL, };
    char *surface_str = NULL;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_SURFACE_TYPE,
            &surface))
      return;

    if (surface & EGL_WINDOW_BIT)
      surface_values[i++] = "window";
    if (surface & EGL_PBUFFER_BIT)
      surface_values[i++] = "pbuffer";
    if (surface & EGL_MULTISAMPLE_RESOLVE_BOX_BIT)
      surface_values[i++] = "multisample-resolve-box";
    if (surface & EGL_SWAP_BEHAVIOR_PRESERVED_BIT)
      surface_values[i++] = "swap-behaviour-preserved";
    if (surface & EGL_VG_ALPHA_FORMAT_PRE_BIT)
      surface_values[i++] = "vg-alpha-format-pre";
    if (surface & EGL_VG_COLORSPACE_LINEAR_BIT)
      surface_values[i++] = "vg-colorspace-linear";

    /* bad things have happened if this fails: we haven't allocated enough
     * space to hold all the values */
    g_assert (i < MAX_SURFACE);

    surface_str = g_strjoinv ("|", (char **) surface_values);
    GST_DEBUG_OBJECT (egl, "Surface for (0x%x) %s", surface, surface_str);
    g_free (surface_str);
#undef MAX_RENDERABLE
  }

  {
#define MAX_CAVEAT 8
    int caveat, i = 0;
    const char *caveat_values[MAX_CAVEAT] = { NULL, };
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_CONFIG_CAVEAT,
            &caveat))
      return;
    if (caveat == EGL_SLOW_CONFIG) {
      caveat_values[i++] = "slow";
    } else if (caveat == EGL_NON_CONFORMANT_CONFIG) {
      caveat_values[i++] = "non-conformant";
    }
    if (i > 0) {
      char *caveat_str = g_strjoinv ("|", (char **) caveat_values);
      GST_DEBUG_OBJECT (egl, "Advertised as %s", caveat_str);
      g_free (caveat_str);
    }
#undef MAX_CAVEAT
  }

  if (!eglGetConfigAttrib (egl->egl_display, config, EGL_COLOR_BUFFER_TYPE,
          &buffer_type))
    return;
  if (buffer_type == EGL_RGB_BUFFER) {
    int red, blue, green, alpha;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_RED_SIZE, &red))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_GREEN_SIZE, &green))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_BLUE_SIZE, &blue))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_ALPHA_SIZE, &alpha))
      return;

    GST_DEBUG_OBJECT (egl, "[R, G, B, A] = [%i, %i, %i, %i]", red, green, blue,
        alpha);
  } else if (buffer_type == EGL_LUMINANCE_BUFFER) {
    int luminance, alpha;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_LUMINANCE_SIZE,
            &luminance))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_ALPHA_SIZE, &alpha))
      return;
    GST_DEBUG_OBJECT (egl, "[L, A] = [%i, %i]", luminance, alpha);
  } else {
    GST_WARNING_OBJECT (egl, "unknown EGL_COLOR_BUFFER_TYPE value %x",
        buffer_type);
    return;
  }
  {
    int depth, stencil;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_DEPTH_SIZE, &depth))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_STENCIL_SIZE,
            &stencil))
      return;
    GST_DEBUG_OBJECT (egl, "[D, S] = [%i, %i]", depth, stencil);
  }
  {
    int min, max;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_MIN_SWAP_INTERVAL,
            &min))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_MAX_SWAP_INTERVAL,
            &max))
      return;
    GST_DEBUG_OBJECT (egl, "Swap interval range is [%i, %i]", min, max);
  }
  {
    int width, height, pixels;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_MAX_PBUFFER_WIDTH,
            &width))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_MAX_PBUFFER_HEIGHT,
            &height))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_MAX_PBUFFER_PIXELS,
            &pixels))
      return;
    GST_DEBUG_OBJECT (egl,
        "PBuffer maximum dimensions are [%i, %i]. Max pixels are %i", width,
        height, pixels);
  }
  {
    int sample_buffers, samples_per_pixel;

    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_SAMPLE_BUFFERS,
            &sample_buffers))
      return;
    if (!eglGetConfigAttrib (egl->egl_display, config, EGL_SAMPLES,
            &samples_per_pixel))
      return;
    GST_DEBUG_OBJECT (egl, "Multisample buffers: %i and Samples per pixel: %i",
        sample_buffers, samples_per_pixel);
  }
}

static void
gst_gl_context_egl_dump_all_configs (GstGLContextEGL * egl)
{
  int i, n;
  EGLConfig *configs;

  if (!eglGetConfigs (egl->egl_display, NULL, 0, &n)) {
    GST_WARNING_OBJECT (egl, "Failed to get number of EGLConfig's");
    return;
  }

  configs = g_new0 (EGLConfig, n);
  if (!eglGetConfigs (egl->egl_display, configs, n, &n)) {
    GST_WARNING_OBJECT (egl, "Failed to get the list of EGLConfig's");
    goto out;
  }

  for (i = 0; i < n; i++)
    gst_gl_context_egl_dump_config (egl, configs[i]);

out:
  g_free (configs);
}

static gboolean
gst_gl_context_egl_choose_config (GstGLContextEGL * egl, GstGLAPI gl_api,
    gint major, GError ** error)
{
  gboolean create_context;
  EGLint numConfigs;
  gint i, n;
  EGLint config_attrib[20];
  EGLint egl_api = 0;
  EGLBoolean ret = EGL_FALSE;
  EGLint surface_type = EGL_WINDOW_BIT;
  EGLint alpha_size = 1;
  GstGLWindow *window;

  window = gst_gl_context_get_window (GST_GL_CONTEXT (egl));

  if (!window || !gst_gl_window_has_output_surface (window)) {
    GST_INFO_OBJECT (egl,
        "gl window has no output surface, use pixel buffer surfaces");
    surface_type = EGL_PBUFFER_BIT;
  }

  if (window)
    gst_object_unref (window);

  create_context =
      gst_gl_check_extension ("EGL_KHR_create_context", egl->egl_exts);
  /* silence unused warnings */
  (void) create_context;

  if (gl_api & GST_GL_API_GLES2) {
    if (major == 3) {
#if defined(EGL_KHR_create_context)
      if (create_context) {
        egl_api = EGL_OPENGL_ES3_BIT_KHR;
      } else
#endif
      {
        return FALSE;
      }
    } else {
      egl_api = EGL_OPENGL_ES2_BIT;
    }
  } else
    egl_api = EGL_OPENGL_BIT;

try_again:
  i = 0;
  n = G_N_ELEMENTS (config_attrib);
  config_attrib[i++] = EGL_SURFACE_TYPE;
  config_attrib[i++] = surface_type;
  config_attrib[i++] = EGL_RENDERABLE_TYPE;
  config_attrib[i++] = egl_api;

  if (egl->requested_config) {
#define TRANSFORM_VALUE(GL_CONF_NAME,EGL_ATTR_NAME) \
  G_STMT_START { \
    if (gst_structure_has_field_typed (egl->requested_config, \
          GST_GL_CONFIG_ATTRIB_NAME(GL_CONF_NAME), \
          GST_GL_CONFIG_ATTRIB_GTYPE(GL_CONF_NAME))) { \
      int val; \
      if (gst_structure_get (egl->requested_config, \
          GST_GL_CONFIG_ATTRIB_NAME(GL_CONF_NAME), \
          GST_GL_CONFIG_ATTRIB_GTYPE(GL_CONF_NAME), &val, NULL)) { \
        config_attrib[i++] = EGL_ATTR_NAME; \
        config_attrib[i++] = (int) val; \
        g_assert (i <= n); \
      } \
    } \
  } G_STMT_END

    TRANSFORM_VALUE (CONFIG_ID, EGL_CONFIG_ID);
    TRANSFORM_VALUE (RED_SIZE, EGL_RED_SIZE);
    TRANSFORM_VALUE (GREEN_SIZE, EGL_GREEN_SIZE);
    TRANSFORM_VALUE (BLUE_SIZE, EGL_BLUE_SIZE);
    TRANSFORM_VALUE (ALPHA_SIZE, EGL_ALPHA_SIZE);
    TRANSFORM_VALUE (DEPTH_SIZE, EGL_DEPTH_SIZE);
    TRANSFORM_VALUE (STENCIL_SIZE, EGL_STENCIL_SIZE);
    /* TODO: more values */
#undef TRANSFORM_VALUE
  } else {
    config_attrib[i++] = EGL_DEPTH_SIZE;
    config_attrib[i++] = 16;
    config_attrib[i++] = EGL_RED_SIZE;
    config_attrib[i++] = 1;
    config_attrib[i++] = EGL_GREEN_SIZE;
    config_attrib[i++] = 1;
    config_attrib[i++] = EGL_BLUE_SIZE;
    config_attrib[i++] = 1;
    config_attrib[i++] = EGL_ALPHA_SIZE;
    config_attrib[i++] = alpha_size;
  }

  config_attrib[i++] = EGL_NONE;
  g_assert (i <= n);

  ret = eglChooseConfig (egl->egl_display, config_attrib,
      &egl->egl_config, 1, &numConfigs);

  if (ret && numConfigs == 0) {
    if (surface_type == EGL_PBUFFER_BIT) {
      surface_type = EGL_WINDOW_BIT;
      GST_TRACE_OBJECT (egl, "Retrying config with window bit");
      goto try_again;
    }

    if (alpha_size == 1) {
      alpha_size = 0;
      GST_TRACE_OBJECT (egl, "Retrying config not forcing an alpha channel");
      goto try_again;
    }
  }

  if (ret && numConfigs == 1) {
    GST_INFO ("config set: %" G_GUINTPTR_FORMAT ", %u",
        (guintptr) egl->egl_config, (unsigned int) numConfigs);
  } else {
    if (!ret) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG, "Failed to choose EGLConfig: %s",
          gst_egl_get_error_string (eglGetError ()));
    } else if (numConfigs <= 1) {
      g_set_error_literal (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Could not find a compatible EGLConfig:");
    } else {
      g_warn_if_reached ();
    }
    goto failure;
  }

  GST_DEBUG_OBJECT (egl, "chosen EGLConfig:");
  gst_gl_context_egl_dump_config (egl, egl->egl_config);

  return TRUE;

failure:
  return FALSE;
}

static EGLContext
_create_context_with_flags (GstGLContextEGL * egl, EGLContext share_context,
    GstGLAPI gl_api, gint major, gint minor, gint contextFlags,
    gint profileMask)
{
  gboolean create_context;
#define N_ATTRIBS 20
  gint attribs[N_ATTRIBS];
  gint n = 0;

  /* fail creation of apis/versions/flags that require EGL_KHR_create_context
   * if the extension doesn't exist, namely:0
   *
   * - profile mask
   * - context flags
   * - GL3 > 3.1
   * - GLES2 && minor > 0
   */
  create_context =
      gst_gl_check_extension ("EGL_KHR_create_context", egl->egl_exts);
  (void) create_context;
  if (!create_context && (profileMask || contextFlags
          || ((gl_api & GST_GL_API_OPENGL3)
              && GST_GL_CHECK_GL_VERSION (major, minor, 3, 2))
          || ((gl_api & GST_GL_API_GLES2) && minor > 0))) {
    return 0;
  }

  GST_DEBUG_OBJECT (egl, "attempting to create OpenGL%s context version %d.%d "
      "flags %x profile %x", gl_api & GST_GL_API_GLES2 ? " ES" : "", major,
      minor, contextFlags, profileMask);

#if defined(EGL_KHR_create_context)
  if (create_context) {
    if (major) {
      attribs[n++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
      attribs[n++] = major;
    }
    if (minor) {
      attribs[n++] = EGL_CONTEXT_MINOR_VERSION_KHR;
      attribs[n++] = minor;
    }
    if (contextFlags) {
      attribs[n++] = EGL_CONTEXT_FLAGS_KHR;
      attribs[n++] = contextFlags;
    }
    if (profileMask) {
      attribs[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      attribs[n++] = profileMask;
    }
  } else
#endif
  {
    attribs[n++] = EGL_CONTEXT_CLIENT_VERSION;
    attribs[n++] = major;
  }
  attribs[n++] = EGL_NONE;

  g_assert (n < N_ATTRIBS);
#undef N_ATTRIBS

  return eglCreateContext (egl->egl_display, egl->egl_config, share_context,
      attribs);
}

static void
gst_gl_context_egl_window_handle_changed_cb (GstGLContextEGL * egl,
    GstGLWindow * window)
{
  if (egl->egl_surface != EGL_NO_SURFACE) {
    if (!eglDestroySurface (egl->egl_display, egl->egl_surface))
      GST_WARNING_OBJECT (egl, "Failed to destroy old window surface: %s",
          gst_egl_get_error_string (eglGetError ()));
    egl->egl_surface = EGL_NO_SURFACE;
  }
}

static gboolean
gst_gl_context_egl_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error)
{
  GstGLContextEGL *egl;
  GstGLWindow *window = NULL;
  guintptr window_handle = 0;
  EGLint egl_major;
  EGLint egl_minor;
  gboolean need_surface = TRUE;
  guintptr external_gl_context = 0;
  guintptr egl_display;

  egl = GST_GL_CONTEXT_EGL (context);
  window = gst_gl_context_get_window (context);

  GST_DEBUG_OBJECT (context, "Creating EGL context");

  if (other_context) {
    if (gst_gl_context_get_gl_platform (other_context) != GST_GL_PLATFORM_EGL) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Cannot share context with non-EGL context");
      goto failure;
    }
    external_gl_context = gst_gl_context_get_gl_context (other_context);
  }

  if ((gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2)) ==
      GST_GL_API_NONE) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_WRONG_API,
        "EGL supports opengl or gles2");
    goto failure;
  }

  if (!egl->display_egl) {
    GstGLDisplay *display = gst_gl_context_get_display (context);

    egl->display_egl = gst_gl_display_egl_from_gl_display (display);
    if (!egl->display_egl) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE,
          "Failed to create EGLDisplay from native display");
      gst_object_unref (display);
      goto failure;
    }

    gst_object_unref (display);
  }

  egl_display = gst_gl_display_get_handle (GST_GL_DISPLAY (egl->display_egl));
  egl->egl_display = (EGLDisplay) egl_display;

  if (eglInitialize (egl->egl_display, &egl_major, &egl_minor)) {
    GST_INFO ("egl initialized, version: %d.%d", egl_major, egl_minor);
  } else {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to initialize egl: %s",
        gst_egl_get_error_string (eglGetError ()));
    goto failure;
  }

  egl->egl_exts = eglQueryString (egl->egl_display, EGL_EXTENSIONS);
  GST_DEBUG_OBJECT (egl, "Have EGL extensions: %s", egl->egl_exts);

  gst_gl_context_egl_dump_all_configs (egl);

  if (gl_api & GST_GL_API_GLES2) {
    gint i;

  try_gles2:
    if (!eglBindAPI (EGL_OPENGL_ES_API)) {
      g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
          "Failed to bind OpenGL|ES API: %s",
          gst_egl_get_error_string (eglGetError ()));
      goto failure;
    }

    GST_INFO ("Bound OpenGL|ES");

    for (i = 0; i < G_N_ELEMENTS (gles2_versions); i++) {
      gint profileMask = 0;
      gint contextFlags = 0;
      guint maj = gles2_versions[i].major;
      guint min = gles2_versions[i].minor;

      if (!gst_gl_context_egl_choose_config (egl, GST_GL_API_GLES2, maj, error)) {
        GST_DEBUG_OBJECT (context, "Failed to choose a GLES%d config: %s",
            maj, error && *error ? (*error)->message : "Unknown");
        g_clear_error (error);
        continue;
      }
#if defined(EGL_KHR_create_context)
      /* try a debug context */
      contextFlags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

      egl->egl_context =
          _create_context_with_flags (egl, (EGLContext) external_gl_context,
          GST_GL_API_GLES2, maj, min, contextFlags, profileMask);

      if (egl->egl_context)
        break;

      /* try without a debug context */
      contextFlags &= ~EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
#endif

      egl->egl_context =
          _create_context_with_flags (egl, (EGLContext) external_gl_context,
          GST_GL_API_GLES2, maj, min, contextFlags, profileMask);

      if (egl->egl_context)
        break;
    }
    egl->gl_api = GST_GL_API_GLES2;
  } else if (gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3)) {
    GstGLAPI chosen_gl_api = 0;
    gint i;

    /* egl + opengl only available with EGL 1.4+ */
    if (egl_major == 1 && egl_minor <= 3) {
      if ((gl_api & ~GST_GL_API_OPENGL) == GST_GL_API_NONE) {
        g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_OLD_LIBS,
            "EGL version (%i.%i) too old for OpenGL support, (needed at least 1.4)",
            egl_major, egl_minor);
        goto failure;
      } else {
        GST_WARNING
            ("EGL version (%i.%i) too old for OpenGL support, (needed at least 1.4)",
            egl_major, egl_minor);
        if (gl_api & GST_GL_API_GLES2) {
          goto try_gles2;
        } else {
          g_set_error (error, GST_GL_CONTEXT_ERROR,
              GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
              "Failed to choose a suitable OpenGL API");
          goto failure;
        }
      }
    }

    if (!eglBindAPI (EGL_OPENGL_API)) {
      g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
          "Failed to bind OpenGL API: %s",
          gst_egl_get_error_string (eglGetError ()));
      goto failure;
    }

    GST_INFO ("Bound OpenGL");

    /* api, version only matters for gles */
    if (!gst_gl_context_egl_choose_config (egl, GST_GL_API_OPENGL, 0, error)) {
      g_assert (error == NULL || *error != NULL);
      goto failure;
    }

    for (i = 0; i < G_N_ELEMENTS (opengl_versions); i++) {
      gint profileMask = 0;
      gint contextFlags = 0;

      if (GST_GL_CHECK_GL_VERSION (opengl_versions[i].major,
              opengl_versions[i].minor, 3, 2)) {
        /* skip gl3 contexts if requested */
        if ((gl_api & GST_GL_API_OPENGL3) == 0)
          continue;

        chosen_gl_api = GST_GL_API_OPENGL3;
#if defined(EGL_KHR_create_context)
        profileMask |= EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
        contextFlags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
#endif
      } else if (opengl_versions[i].major == 3 && opengl_versions[i].minor == 1) {
        /* skip 3.1, the implementation is free to give us either a core or a
         * compatibility context (we have no say) */
        continue;
      } else {
        /* skip legacy contexts if requested */
        if ((gl_api & GST_GL_API_OPENGL) == 0)
          continue;

        chosen_gl_api = GST_GL_API_OPENGL;
      }

      egl->egl_context =
          _create_context_with_flags (egl, (EGLContext) external_gl_context,
          chosen_gl_api, opengl_versions[i].major,
          opengl_versions[i].minor, contextFlags, profileMask);

      if (egl->egl_context)
        break;

#if defined(EGL_KHR_create_context)
      profileMask &= ~EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

      egl->egl_context =
          _create_context_with_flags (egl, (EGLContext) external_gl_context,
          chosen_gl_api, opengl_versions[i].major,
          opengl_versions[i].minor, contextFlags, profileMask);

      if (egl->egl_context)
        break;
#endif
    }

    egl->gl_api = chosen_gl_api;
  }

  if (egl->egl_context != EGL_NO_CONTEXT) {
    GST_INFO ("gl context created: %" G_GUINTPTR_FORMAT,
        (guintptr) egl->egl_context);
  } else {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "Failed to create a OpenGL context: %s",
        gst_egl_get_error_string (eglGetError ()));
    goto failure;
  }
  /* FIXME do we want a window vfunc ? */
#if GST_GL_HAVE_WINDOW_X11
  if (GST_IS_GL_WINDOW_X11 (context->window)) {
    gst_gl_window_x11_create_window ((GstGLWindowX11 *) context->window);
  }
#endif

  if (other_context == NULL) {
    /* FIXME: fails to show two outputs at all.  We need a property/option for
     * glimagesink to say its a visible context */
#if GST_GL_HAVE_WINDOW_WAYLAND
    if (GST_IS_GL_WINDOW_WAYLAND_EGL (context->window)) {
      gst_gl_window_wayland_egl_create_window ((GstGLWindowWaylandEGL *)
          context->window);
    }
#endif
#if GST_GL_HAVE_WINDOW_WIN32
    if (GST_IS_GL_WINDOW_WIN32 (context->window)) {
      gst_gl_window_win32_create_window ((GstGLWindowWin32 *) context->window,
          NULL);
    }
#endif
#if GST_GL_HAVE_WINDOW_DISPMANX
    if (GST_IS_GL_WINDOW_DISPMANX_EGL (context->window)) {
      gst_gl_window_dispmanx_egl_create_window ((GstGLWindowDispmanxEGL *)
          context->window);
    }
#endif
#if GST_GL_HAVE_WINDOW_GBM
    if (GST_IS_GL_WINDOW_GBM_EGL (context->window)) {
      gst_gl_window_gbm_egl_create_window ((GstGLWindowGBMEGL *)
          context->window);
    }
#endif
#if GST_GL_HAVE_WINDOW_VIV_FB
    if (GST_IS_GL_WINDOW_VIV_FB_EGL (context->window)) {
      gst_gl_window_viv_fb_egl_create_window ((GstGLWindowVivFBEGL *)
          context->window);
    }
#endif
  }

  if (window)
    window_handle = gst_gl_window_get_window_handle (window);

  if (window_handle) {
#if GST_GL_HAVE_WINDOW_WINRT && defined (EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER)
    const EGLint attrs[] = {
      /* EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is an optimization that can
       * have large performance benefits on mobile devices. */
      EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
      EGL_NONE
    };
#else
    const EGLint *attrs = NULL;
#endif

    GST_DEBUG ("Creating EGLSurface from window_handle %p",
        (void *) window_handle);
    egl->egl_surface =
        eglCreateWindowSurface (egl->egl_display, egl->egl_config,
        (EGLNativeWindowType) window_handle, attrs);
  } else if (!gst_gl_check_extension ("EGL_KHR_surfaceless_context",
          egl->egl_exts)) {
    EGLint surface_attrib[7];
    gint j = 0;

    GST_DEBUG ("Surfaceless context, creating PBufferSurface");
    /* FIXME: Width/height doesn't seem to matter but we can't leave them
     * at 0, otherwise X11 complains about BadValue */
    surface_attrib[j++] = EGL_WIDTH;
    surface_attrib[j++] = 1;
    surface_attrib[j++] = EGL_HEIGHT;
    surface_attrib[j++] = 1;
    surface_attrib[j++] = EGL_LARGEST_PBUFFER;
    surface_attrib[j++] = EGL_TRUE;
    surface_attrib[j++] = EGL_NONE;

    egl->egl_surface =
        eglCreatePbufferSurface (egl->egl_display, egl->egl_config,
        surface_attrib);
  } else {
    GST_DEBUG ("No surface/handle !");
    egl->egl_surface = EGL_NO_SURFACE;
    need_surface = FALSE;
  }

  if (need_surface) {
    if (egl->egl_surface != EGL_NO_SURFACE) {
      GST_INFO ("surface created");
    } else {
      g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
          "Failed to create window surface: %s",
          gst_egl_get_error_string (eglGetError ()));
      goto failure;
    }
  }
  egl->egl_major = egl_major;
  egl->egl_minor = egl_minor;

  if (window) {
    egl->window_handle_signal = g_signal_connect_swapped (window,
        "window-handle-changed",
        G_CALLBACK (gst_gl_context_egl_window_handle_changed_cb), egl);
    gst_object_unref (window);
  }

  return TRUE;

failure:
  if (window)
    gst_object_unref (window);

  return FALSE;
}

static void
gst_gl_context_egl_destroy_context (GstGLContext * context)
{
  GstGLContextEGL *egl;
  GstGLWindow *window;

  egl = GST_GL_CONTEXT_EGL (context);
  window = gst_gl_context_get_window (context);

  if (window && egl->window_handle_signal) {
    g_signal_handler_disconnect (window, egl->window_handle_signal);
    egl->window_handle_signal = 0;
  }
  if (window) {
    gst_object_unref (window);
  }

  g_clear_pointer (&egl->dma_formats, g_array_unref);

  gst_gl_context_egl_activate (context, FALSE);

  if (egl->egl_surface) {
    eglDestroySurface (egl->egl_display, egl->egl_surface);
    egl->egl_surface = EGL_NO_SURFACE;
  }

  if (egl->egl_context) {
    eglDestroyContext (egl->egl_display, egl->egl_context);
    egl->egl_context = NULL;
  }

  eglReleaseThread ();

  if (egl->display_egl) {
    gst_object_unref (egl->display_egl);
    egl->display_egl = NULL;
  }

  if (egl->requested_config)
    gst_structure_free (egl->requested_config);
  egl->requested_config = NULL;
}

static gboolean
gst_gl_context_egl_activate (GstGLContext * context, gboolean activate)
{
  GstGLContextEGL *egl;
  gboolean result;

  egl = GST_GL_CONTEXT_EGL (context);

  if (activate) {
    GstGLWindow *window = gst_gl_context_get_window (context);
    guintptr handle = 0;
    /* Check if the backing handle changed */
    if (window) {
      handle = gst_gl_window_get_window_handle (window);
      gst_object_unref (window);
    }
    if (handle && (egl->egl_surface == EGL_NO_SURFACE)) {
#if GST_GL_HAVE_WINDOW_WINRT && defined (EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER)
      const EGLint attrs[] = {
        /* EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is an optimization that can
         * have large performance benefits on mobile devices. */
        EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_NONE
      };
#else
      const EGLint *attrs = NULL;
#endif
      GST_DEBUG_OBJECT (context,
          "Handle changed (have:%p, now:%p), switching surface",
          (void *) egl->window_handle, (void *) handle);
      egl->egl_surface =
          eglCreateWindowSurface (egl->egl_display, egl->egl_config,
          (EGLNativeWindowType) handle, attrs);
      egl->window_handle = handle;

      if (egl->egl_surface == EGL_NO_SURFACE) {
        GST_ERROR_OBJECT (context, "Failed to create window surface: %s",
            gst_egl_get_error_string (eglGetError ()));
        result = FALSE;
        goto done;
      }
    }
    result = eglMakeCurrent (egl->egl_display, egl->egl_surface,
        egl->egl_surface, egl->egl_context);
  } else {
    result = eglMakeCurrent (egl->egl_display, EGL_NO_SURFACE,
        EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }

  if (!result) {
    GST_ERROR_OBJECT (context,
        "Failed to bind context to the current rendering thread: %s",
        gst_egl_get_error_string (eglGetError ()));
  }

done:
  return result;
}

static guintptr
gst_gl_context_egl_get_gl_context (GstGLContext * context)
{
  return (guintptr) GST_GL_CONTEXT_EGL (context)->egl_context;
}

static void
gst_gl_context_egl_swap_buffers (GstGLContext * context)
{
  GstGLContextEGL *egl;

  egl = GST_GL_CONTEXT_EGL (context);

  eglSwapBuffers (egl->egl_display, egl->egl_surface);
}

static GstGLAPI
gst_gl_context_egl_get_gl_api (GstGLContext * context)
{
  return GST_GL_CONTEXT_EGL (context)->gl_api;
}

static GstGLPlatform
gst_gl_context_egl_get_gl_platform (GstGLContext * context)
{
  return GST_GL_PLATFORM_EGL;
}

static GModule *module_egl;

static gpointer
load_egl_module (gpointer user_data)
{
#ifdef GST_GL_LIBEGL_MODULE_NAME
  module_egl = g_module_open (GST_GL_LIBEGL_MODULE_NAME, G_MODULE_BIND_LAZY);
#else
  /* On Linux the .so is only in -dev packages, try with a real soname
   * Proper compilers will optimize away the strcmp */
  if (g_strcmp0 (G_MODULE_SUFFIX, "so") == 0)
    module_egl = g_module_open ("libEGL.so.1", G_MODULE_BIND_LAZY);

  /* This automatically handles the suffix and even .la files */
  if (!module_egl)
    module_egl = g_module_open ("libEGL", G_MODULE_BIND_LAZY);
#endif

  return NULL;
}

gpointer
gst_gl_context_egl_get_proc_address (GstGLAPI gl_api, const gchar * name)
{
  gpointer result = NULL;
  static GOnce g_once = G_ONCE_INIT;

#ifdef __APPLE__
#if GST_GL_HAVE_OPENGL && !defined(GST_GL_LIBGL_MODULE_NAME)
  if (!result && (gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3))) {
    static GModule *module_opengl = NULL;
    if (g_once_init_enter (&module_opengl)) {
      GModule *setup_module_opengl =
          g_module_open ("libGL.dylib", G_MODULE_BIND_LAZY);
      g_once_init_leave (&module_opengl, setup_module_opengl);
    }
    if (module_opengl)
      g_module_symbol (module_opengl, name, &result);
  }
#endif
#if GST_GL_HAVE_GLES2 && !defined(GST_GL_LIBGLESV2_MODULE_NAME)
  if (!result && (gl_api & (GST_GL_API_GLES2))) {
    static GModule *module_gles2 = NULL;
    if (g_once_init_enter (&module_gles2)) {
      GModule *setup_module_gles2 =
          g_module_open ("libGLESv2.dylib", G_MODULE_BIND_LAZY);
      g_once_init_leave (&module_gles2, setup_module_gles2);
    }
    if (module_gles2)
      g_module_symbol (module_gles2, name, &result);
  }
#endif
#endif // __APPLE__

  if (!result)
    result = gst_gl_context_default_get_proc_address (gl_api, name);

  g_once (&g_once, load_egl_module, NULL);

  if (!result && module_egl) {
    g_module_symbol (module_egl, name, &result);
  }

  /* FIXME: On Android this returns wrong addresses for non-EGL functions */
#if GST_GL_HAVE_WINDOW_ANDROID
  if (!result && g_str_has_prefix (name, "egl")) {
#else
  if (!result) {
    result = eglGetProcAddress (name);
#endif
  }

  return result;
}

static gboolean
gst_gl_context_egl_check_feature (GstGLContext * context, const gchar * feature)
{
  GstGLContextEGL *context_egl = GST_GL_CONTEXT_EGL (context);

  return gst_gl_check_extension (feature, context_egl->egl_exts);
}

guintptr
gst_gl_context_egl_get_current_context (void)
{
  return (guintptr) eglGetCurrentContext ();
}

static void
gst_gl_context_egl_get_gl_platform_version (GstGLContext * context,
    gint * major, gint * minor)
{
  GstGLContextEGL *context_egl = GST_GL_CONTEXT_EGL (context);

  *major = context_egl->egl_major;
  *minor = context_egl->egl_minor;
}

static GstStructure *
gst_gl_context_egl_get_config (GstGLContext * context)
{
  GstGLContextEGL *egl = GST_GL_CONTEXT_EGL (context);

  g_return_val_if_fail (egl->egl_config, NULL);

  return egl_config_to_structure (egl->egl_display, egl->egl_config);
}

static gboolean
gst_gl_context_egl_request_config (GstGLContext * context,
    GstStructure * config)
{
  GstGLContextEGL *egl = GST_GL_CONTEXT_EGL (context);

  if (egl->requested_config)
    gst_structure_free (egl->requested_config);
  egl->requested_config = config;

  return TRUE;
}

gboolean
gst_gl_context_egl_fill_info (GstGLContext * context, GError ** error)
{
  EGLContext egl_context = (EGLContext) gst_gl_context_get_gl_context (context);
  GstGLDisplay *display_egl;
  GstStructure *config;
  EGLDisplay *egl_display;
  EGLConfig egl_config;
  int config_id, n_configs;
  int attrs[3];

  if (!egl_context) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE, "no EGL context");
    return FALSE;
  }

  display_egl =
      GST_GL_DISPLAY (gst_gl_display_egl_from_gl_display (context->display));
  egl_display = (EGLDisplay) gst_gl_display_get_handle (display_egl);

  if (EGL_TRUE != eglQueryContext (egl_display, egl_context, EGL_CONFIG_ID,
          &config_id)) {
    GST_WARNING_OBJECT (context,
        "could not retrieve egl config id from egl context: %s",
        gst_egl_get_error_string (eglGetError ()));
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not retrieve egl config id from egl context: %s",
        gst_egl_get_error_string (eglGetError ()));
    goto failure;
  }

  if (config_id == 0) {
    GST_INFO_OBJECT (context, "egl config not available. ID is 0");
    gst_object_unref (display_egl);
    return TRUE;
  }

  attrs[0] = EGL_CONFIG_ID;
  attrs[1] = config_id;
  attrs[2] = EGL_NONE;

  if (EGL_TRUE != eglChooseConfig (egl_display, attrs, &egl_config, 1,
          &n_configs) || n_configs <= 0) {
    GST_WARNING_OBJECT (context,
        "could not retrieve egl config from its ID 0x%x. "
        "Wrong EGLDisplay or context?", config_id);
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not retrieve egl config from its ID 0x%x. "
        "Wrong EGLDisplay or context?", config_id);
    goto failure;
  }

  config = egl_config_to_structure (egl_display, egl_config);
  if (!config) {
    GST_WARNING_OBJECT (context, "could not transform config id 0x%x into "
        "GstStructure", config_id);
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not transform config id 0x%x into GstStructure", config_id);
    goto failure;
  }

  GST_INFO_OBJECT (context, "found config %" GST_PTR_FORMAT, config);

  g_object_set_data_full (G_OBJECT (context),
      GST_GL_CONTEXT_WRAPPED_GL_CONFIG_NAME, config,
      (GDestroyNotify) gst_structure_free);

  gst_object_unref (display_egl);
  return TRUE;

failure:
  gst_object_unref (display_egl);
  return FALSE;
}

#if GST_GL_HAVE_DMABUF
static void
_print_all_dma_formats (GstGLContext * context, GArray * dma_formats)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstGLDmaFormat *dma_fmt;
  GstGLDmaModifier *dma_modifier;
  const gchar *fmt_str, *gst_fmt_str;
  GString *str;
  guint i, j;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_INFO)
    return;

  str = g_string_new (NULL);
  g_string_append_printf (str, "\n============= All DMA Formats With"
      " Modifiers =============");
  g_string_append_printf (str, "\n| Gst Format   | DRM Format      "
      "        | External Flag |");
  g_string_append_printf (str, "\n|================================"
      "========================|");

  for (i = 0; i < dma_formats->len; i++) {
    dma_fmt = &g_array_index (dma_formats, GstGLDmaFormat, i);

    gst_fmt_str = gst_video_format_to_string
        (gst_video_dma_drm_fourcc_to_format (dma_fmt->fourcc));

    g_string_append_printf (str, "\n| %-12s |", gst_fmt_str);

    if (!dma_fmt->modifiers) {
      fmt_str = gst_video_dma_drm_fourcc_to_string (dma_fmt->fourcc, 0);
      g_string_append_printf (str, " %-23s |", fmt_str);
      g_string_append_printf (str, " %-13s |\n", "external only");
    } else {
      for (j = 0; j < dma_fmt->modifiers->len; j++) {
        dma_modifier = &g_array_index (dma_fmt->modifiers, GstGLDmaModifier, j);

        fmt_str = gst_video_dma_drm_fourcc_to_string (dma_fmt->fourcc,
            dma_modifier->modifier);

        if (j > 0)
          g_string_append_printf (str, "|              |");

        g_string_append_printf (str, " %-23s |", fmt_str);
        g_string_append_printf (str, " %-13s |\n", dma_modifier->external_only ?
            "external only" : "");
      }
    }

    if (i < dma_formats->len - 1)
      g_string_append_printf (str, "|--------------------------------"
          "------------------------|");
  }

  g_string_append_printf (str, "================================="
      "=========================");

  GST_INFO_OBJECT (context, "%s", str->str);
  g_string_free (str, TRUE);
#endif
}

static int
_compare_dma_formats (gconstpointer a, gconstpointer b)
{
  return ((((GstGLDmaFormat *) a)->fourcc) - (((GstGLDmaFormat *) b)->fourcc));
}

static void
_free_dma_formats (gpointer data)
{
  GstGLDmaFormat *format = data;
  if (format->modifiers)
    g_array_unref (format->modifiers);
}

/**
 * gst_gl_context_egl_get_dma_formats: (skip)
 * @context: A #GstGLContextEGL object
 *
 * Returns: %TRUE if the array of DMABufs modifiers were fetched. Otherwise,
 *     %FALSE
 */
static gboolean
gst_gl_context_egl_fetch_dma_formats (GstGLContext * context)
{
  GstGLContextEGL *egl;
  EGLDisplay egl_dpy = EGL_DEFAULT_DISPLAY;
  GstGLDisplayEGL *gl_dpy_egl;
  EGLint *formats = NULL, num_formats, mods_len = 0;
  guint i, j;
  gboolean ret;
  EGLuint64KHR *modifiers = NULL;
  EGLBoolean *ext_only = NULL;
  GArray *dma_formats;

  EGLBoolean (*gst_eglQueryDmaBufFormatsEXT) (EGLDisplay dpy,
      EGLint max_formats, EGLint * formats, EGLint * num_formats);
  EGLBoolean (*gst_eglQueryDmaBufModifiersEXT) (EGLDisplay dpy,
      EGLint format, EGLint max_modifiers, EGLuint64KHR * modifiers,
      EGLBoolean * external_only, EGLint * num_modifiers);

  egl = GST_GL_CONTEXT_EGL (context);

  GST_OBJECT_LOCK (context);
  if (egl->dma_formats) {
    GST_OBJECT_UNLOCK (context);
    return TRUE;
  }
  GST_OBJECT_UNLOCK (context);

  if (!gst_gl_context_check_feature (context,
          "EGL_EXT_image_dma_buf_import_modifiers")) {
    GST_WARNING_OBJECT (context, "\"EGL_EXT_image_dma_buf_import_modifiers\" "
        "feature is not available");
    goto failed;
  }

  gst_eglQueryDmaBufFormatsEXT =
      gst_gl_context_get_proc_address (context, "eglQueryDmaBufFormatsEXT");
  if (!gst_eglQueryDmaBufFormatsEXT) {
    GST_ERROR_OBJECT (context, "\"eglQueryDmaBufFormatsEXT\" not exposed by the"
        " implementation as required by EGL >= 1.2");
    goto failed;
  }

  gst_eglQueryDmaBufModifiersEXT =
      gst_gl_context_get_proc_address (context, "eglQueryDmaBufModifiersEXT");
  if (!gst_eglQueryDmaBufModifiersEXT) {
    GST_ERROR_OBJECT (context, "\"eglQueryDmaBufModifiersEXT\" not exposed by "
        "the implementation as required by EGL >= 1.2");
    goto failed;
  }

  gl_dpy_egl = gst_gl_display_egl_from_gl_display (context->display);
  if (!gl_dpy_egl) {
    GST_WARNING_OBJECT (context,
        "Failed to retrieve GstGLDisplayEGL from %" GST_PTR_FORMAT,
        context->display);
    goto failed;
  }
  egl_dpy =
      (EGLDisplay) gst_gl_display_get_handle (GST_GL_DISPLAY (gl_dpy_egl));
  gst_object_unref (gl_dpy_egl);

  ret = gst_eglQueryDmaBufFormatsEXT (egl_dpy, 0, NULL, &num_formats);
  if (!ret) {
    GST_WARNING_OBJECT (context, "Failed to get number of DMABuf formats: %s",
        gst_egl_get_error_string (eglGetError ()));
    goto failed;
  }
  if (num_formats == 0) {
    GST_INFO_OBJECT (context, "No DMABuf formats available");
    goto failed;
  }

  formats = g_new (EGLint, num_formats);

  ret = gst_eglQueryDmaBufFormatsEXT (egl_dpy, num_formats, formats,
      &num_formats);
  if (!ret) {
    GST_ERROR_OBJECT (context, "Failed to get number of DMABuf formats: %s",
        gst_egl_get_error_string (eglGetError ()));
    goto failed;
  }
  if (num_formats == 0) {
    GST_ERROR_OBJECT (context, "No DMABuf formats available");
    goto failed;
  }

  dma_formats = g_array_sized_new (FALSE, FALSE, sizeof (GstGLDmaFormat),
      num_formats);
  g_array_set_clear_func (dma_formats, _free_dma_formats);

  for (i = 0; i < num_formats; i++) {
    EGLint num_mods = 0;
    GstVideoFormat gst_format;
    GstGLDmaFormat dma_frmt;

    gst_format = gst_video_dma_drm_fourcc_to_format (formats[i]);
    if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
      continue;

    dma_frmt.fourcc = formats[i];
    dma_frmt.modifiers = NULL;

    ret = gst_eglQueryDmaBufModifiersEXT (egl_dpy, formats[i], 0,
        NULL, NULL, &num_mods);
    if (!ret) {
      GST_WARNING_OBJECT (context, "Failed to get number of DMABuf modifiers: "
          "%s", gst_egl_get_error_string (eglGetError ()));
      continue;
    }

    if (num_mods > 0) {

      if (mods_len == 0) {
        modifiers = g_new (EGLuint64KHR, num_mods);
        ext_only = g_new (EGLBoolean, num_mods);
        mods_len = num_mods;
      } else if (mods_len < num_mods) {
        modifiers = g_renew (EGLuint64KHR, modifiers, num_mods);
        ext_only = g_renew (EGLBoolean, ext_only, num_mods);
        mods_len = num_mods;
      }

      ret = gst_eglQueryDmaBufModifiersEXT (egl_dpy, formats[i], num_mods,
          modifiers, ext_only, &num_mods);
      if (!ret) {
        GST_ERROR_OBJECT (context, "Failed to get number of DMABuf modifiers: "
            "%s", gst_egl_get_error_string (eglGetError ()));
        continue;
      }

      dma_frmt.modifiers = g_array_sized_new (FALSE, FALSE,
          sizeof (GstGLDmaModifier), num_mods);
      dma_frmt.modifiers = g_array_set_size (dma_frmt.modifiers, num_mods);

      for (j = 0; j < num_mods; j++) {
        GstGLDmaModifier *modifier =
            &g_array_index (dma_frmt.modifiers, GstGLDmaModifier, j);
        modifier->modifier = modifiers[j];
        modifier->external_only = ext_only[j];
      }
    }

    g_array_append_val (dma_formats, dma_frmt);
  }

  g_array_sort (dma_formats, _compare_dma_formats);

  _print_all_dma_formats (context, dma_formats);

  GST_OBJECT_LOCK (context);
  egl->dma_formats = dma_formats;
  GST_OBJECT_UNLOCK (context);

  g_free (formats);
  g_free (modifiers);
  g_free (ext_only);

  return TRUE;

failed:
  {
    g_free (formats);
    return FALSE;
  }
}
#endif /* GST_GL_HAVE_DMABUF */

/**
 * gst_gl_context_egl_get_format_modifiers: (skip)
 * @context: an EGL #GStGLContext
 * @fourcc: the FourCC format to look up
 * @modifiers: (out) (nullable) (element-type GstGLDmaModifier) (transfer none):
 *     #GArray of modifiers for @fourcc
 *
 * Don't modify the content of @modifiers.
 *
 * Returns: %TRUE if the @modifiers for @fourcc were fetched correctly.
 *
 * Since: 1.24
 */
gboolean
gst_gl_context_egl_get_format_modifiers (GstGLContext * context, gint fourcc,
    const GArray ** modifiers)
{
#if GST_GL_HAVE_DMABUF
  GstGLContextEGL *egl;
  GstGLDmaFormat *format;
  guint index;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_IS_GL_CONTEXT_EGL (context), FALSE);

  if (!gst_gl_context_egl_fetch_dma_formats (context))
    return FALSE;

  egl = GST_GL_CONTEXT_EGL (context);

  GST_OBJECT_LOCK (context);
  if (!egl->dma_formats)
    goto beach;

  if (!g_array_binary_search (egl->dma_formats, &fourcc, _compare_dma_formats,
          &index))
    goto beach;

  format = &g_array_index (egl->dma_formats, GstGLDmaFormat, index);
  if (!format)
    goto beach;

  *modifiers = format->modifiers;
  ret = TRUE;

beach:
  GST_OBJECT_UNLOCK (context);
  return ret;
#endif
  return FALSE;
}

/**
 * gst_gl_context_egl_supports_modifier: (skip)
 * @context: an EGL #GStGLContext
 *
 * Returns: %TRUE if the @context supports the modifiers.
 *
 * Since: 1.24
 */
gboolean
gst_gl_context_egl_supports_modifier (GstGLContext * context)
{
#if GST_GL_HAVE_DMABUF
  g_return_val_if_fail (GST_IS_GL_CONTEXT_EGL (context), FALSE);

  return gst_gl_context_egl_fetch_dma_formats (context);
#else
  return FALSE;
#endif
}
