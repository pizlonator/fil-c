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

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* FIXME: Sharing contexts requires the Display to be the same.
 * May need to box it
 */

#include <gst/gst.h>

#include "../gstgl_fwd.h"
#include <gst/gl/gstglcontext.h>

#include <gst/gl/gl.h>
#include "gstglcontext_glx.h"
#include "../utils/opengl_versions.h"
#include "../gstglcontext_private.h"

#define GST_CAT_DEFAULT gst_gl_context_debug

static guintptr gst_gl_context_glx_get_gl_context (GstGLContext * context);
static void gst_gl_context_glx_swap_buffers (GstGLContext * context);
static gboolean gst_gl_context_glx_activate (GstGLContext * context,
    gboolean activate);
static gboolean gst_gl_context_glx_create_context (GstGLContext *
    context, GstGLAPI gl_api, GstGLContext * other_context, GError ** error);
static void gst_gl_context_glx_destroy_context (GstGLContext * context);
static gboolean gst_gl_context_glx_choose_format (GstGLContext *
    context, GError ** error);
static GstGLAPI gst_gl_context_glx_get_gl_api (GstGLContext * context);
static GstGLPlatform gst_gl_context_glx_get_gl_platform (GstGLContext *
    context);
static void gst_gl_context_glx_get_gl_platform_version (GstGLContext * context,
    gint * major, gint * minor);
static GstStructure *gst_gl_context_glx_get_config (GstGLContext * context);
static gboolean gst_gl_context_glx_request_config (GstGLContext * context,
    GstStructure * config);

struct _GstGLContextGLXPrivate
{
  int glx_major;
  int glx_minor;

  GstGLAPI context_api;

  GLXFBConfig *fbconfigs;
    GLXContext (*glXCreateContextAttribsARB) (Display *, GLXFBConfig,
      GLXContext, Bool, const int *);

  GstStructure *requested_config;
};

#define gst_gl_context_glx_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstGLContextGLX, gst_gl_context_glx,
    GST_TYPE_GL_CONTEXT);

static void
gst_gl_context_glx_class_init (GstGLContextGLXClass * klass)
{
  GstGLContextClass *context_class = (GstGLContextClass *) klass;

  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_gl_context);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_context_glx_activate);
  context_class->create_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_create_context);
  context_class->destroy_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_destroy_context);
  context_class->choose_format =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_choose_format);
  context_class->swap_buffers =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_swap_buffers);

  context_class->get_gl_api = GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_gl_platform);
  context_class->get_proc_address =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_proc_address);
  context_class->get_current_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_current_context);
  context_class->get_gl_platform_version =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_gl_platform_version);
  context_class->get_config = GST_DEBUG_FUNCPTR (gst_gl_context_glx_get_config);
  context_class->request_config =
      GST_DEBUG_FUNCPTR (gst_gl_context_glx_request_config);
}

static void
gst_gl_context_glx_init (GstGLContextGLX * context)
{
  context->priv = gst_gl_context_glx_get_instance_private (context);
}

GstGLContextGLX *
gst_gl_context_glx_new (GstGLDisplay * display)
{
  GstGLContextGLX *context;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_X11) == 0)
    /* we require an x11 display handle to create GLX contexts */
    return NULL;

  context = g_object_new (GST_TYPE_GL_CONTEXT_GLX, NULL);
  gst_object_ref_sink (context);

  return context;
}

static GstGLConfigSurfaceType
glx_drawable_type_to_gst (int drawable_type)
{
  GstGLConfigSurfaceType ret = GST_GL_CONFIG_SURFACE_TYPE_NONE;

  if (drawable_type & GLX_WINDOW_BIT)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_WINDOW;
  if (drawable_type & GLX_PIXMAP_BIT)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_PIXMAP;
  if (drawable_type & GLX_PBUFFER_BIT)
    ret |= GST_GL_CONFIG_SURFACE_TYPE_PBUFFER;

  return ret;
}

static GstGLConfigCaveat
glx_caveat_to_gst (int caveat)
{
  switch (caveat) {
    case GLX_NONE:
      return GST_GL_CONFIG_CAVEAT_NONE;
    case GLX_SLOW_CONFIG:
      return GST_GL_CONFIG_CAVEAT_SLOW;
    case GLX_NON_CONFORMANT_CONFIG:
      return GST_GL_CONFIG_CAVEAT_NON_CONFORMANT;
    default:
      GST_WARNING ("unknown GLX caveat value %u (0x%x)", caveat, caveat);
      return GST_GL_CONFIG_CAVEAT_NON_CONFORMANT;
  }
}

static GstStructure *
fb_config_to_structure (GstGLContext * context,
    Display * dpy, GLXFBConfig fbconfig)
{
  GstStructure *ret;
  int val, render_type;

  ret = gst_structure_new (GST_GL_CONFIG_STRUCTURE_NAME,
      GST_GL_CONFIG_STRUCTURE_SET_ARGS (PLATFORM, GstGLPlatform,
          GST_GL_PLATFORM_GLX), "platform-sub-type", G_TYPE_STRING, "fbconfig",
      NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_FBCONFIG_ID, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (CONFIG_ID, int,
          val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_VISUAL_ID, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (NATIVE_VISUAL_ID,
          guint, val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_CONFIG_CAVEAT, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (CAVEAT,
          GstGLConfigCaveat, glx_caveat_to_gst (val)), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_DRAWABLE_TYPE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (SURFACE_TYPE,
          GstGLConfigSurfaceType, glx_drawable_type_to_gst (val)), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_X_RENDERABLE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (NATIVE_RENDERABLE,
          gboolean, val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_LEVEL, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (LEVEL, int, val),
      NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_RENDER_TYPE,
          &render_type))
    goto failure;

  if (render_type & GLX_RGBA_BIT) {
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_RED_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (RED_SIZE, int,
            val), NULL);

    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_GREEN_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (GREEN_SIZE, int,
            val), NULL);

    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_BLUE_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (BLUE_SIZE, int,
            val), NULL);

    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ALPHA_SIZE, &val))
      goto failure;
    gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (ALPHA_SIZE, int,
            val), NULL);
  }

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_DEPTH_SIZE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (DEPTH_SIZE, int,
          val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_STENCIL_SIZE, &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (STENCIL_SIZE, int,
          val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_WIDTH,
          &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_WIDTH,
          int, val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_HEIGHT,
          &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_HEIGHT,
          int, val), NULL);

  if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_PIXELS,
          &val))
    goto failure;
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS (MAX_PBUFFER_PIXELS,
          int, val), NULL);

  return ret;

failure:
  gst_structure_free (ret);
  return NULL;
}

static void
gst_gl_context_glx_dump_fb_config (GstGLContextGLX * glx,
    Display * dpy, GLXFBConfig fbconfig)
{

#define SIMPLE_STRING_ASSIGN(res_str,value,to_check,str) \
    if (res_str == NULL && value == to_check) \
      res_str = str

  int fb_id, render_type;
  {
    int visual_id;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_FBCONFIG_ID,
            &fb_id))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_VISUAL_ID,
            &visual_id))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_RENDER_TYPE,
            &render_type))
      return;

    GST_DEBUG_OBJECT (glx, "dumping GLXFBConfig %p with id 0x%x and "
        "visual id 0x%x", fbconfig, fb_id, visual_id);
  }

  {
#define MAX_RENDER_TYPE 8
#define MAX_DRAWABLE_TYPE 8
    int x_renderable, visual_type, drawable_type, caveat, i = 0;
    const char *render_values[MAX_RENDER_TYPE] = { NULL, };
    const char *drawable_values[MAX_DRAWABLE_TYPE] = { NULL, };
    const char *caveat_str = NULL;
    const char *visual_type_str = NULL;
    char *render_type_str = NULL;
    char *drawable_type_str = NULL;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_X_RENDERABLE,
            &x_renderable))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_CONFIG_CAVEAT,
            &caveat))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_X_VISUAL_TYPE,
            &visual_type))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_DRAWABLE_TYPE,
            &drawable_type))
      return;

    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_TRUE_COLOR,
        "TrueColor");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_DIRECT_COLOR,
        "DirectColor");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_PSEUDO_COLOR,
        "PseudoColor");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_STATIC_COLOR,
        "StaticColor");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_GRAY_SCALE,
        "GrayScale");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_STATIC_GRAY,
        "StaticGray");
    SIMPLE_STRING_ASSIGN (visual_type_str, visual_type, GLX_NONE, "None");

    SIMPLE_STRING_ASSIGN (caveat_str, caveat, GLX_NONE, "None");
    SIMPLE_STRING_ASSIGN (caveat_str, caveat, GLX_SLOW_CONFIG, "SlowConfig");
    SIMPLE_STRING_ASSIGN (caveat_str, caveat, GLX_NON_CONFORMANT_CONFIG,
        "NonConformantConfig");

    i = 0;
    if (render_type & GLX_RGBA_BIT)
      render_values[i++] = "RGBA";
    if (render_type & GLX_COLOR_INDEX_BIT)
      render_values[i++] = "Color Index";

    /* bad things have happened if this fails: we haven't allocated enough
     * space to hold all the values */
    g_assert (i < MAX_RENDER_TYPE);

    i = 0;
    if (drawable_type & GLX_WINDOW_BIT)
      drawable_values[i++] = "Window";
    if (drawable_type & GLX_PIXMAP_BIT)
      drawable_values[i++] = "Pixmap";
    if (drawable_type & GLX_PBUFFER_BIT)
      drawable_values[i++] = "PBuffer";

    /* bad things have happened if this fails: we haven't allocated enough
     * space to hold all the values */
    g_assert (i < MAX_DRAWABLE_TYPE);

    render_type_str = g_strjoinv ("|", (char **) render_values);
    drawable_type_str = g_strjoinv ("|", (char **) drawable_values);
    GST_DEBUG_OBJECT (glx, "Is XRenderable?: %s, visual type: (0x%x) %s, "
        "render type: (0x%x) %s, drawable type: (0x%x) %s, caveat: (0x%x) %s",
        x_renderable ? "YES" : "NO", visual_type, visual_type_str, render_type,
        render_type_str, drawable_type, drawable_type_str, caveat, caveat_str);
    g_free (render_type_str);
    g_free (drawable_type_str);
#undef MAX_RENDER_TYPE
#undef MAX_DRAWABLE_TYPE
  }

  {
    int buffer_size, level, double_buffered, stereo, aux_buffers;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_BUFFER_SIZE,
            &buffer_size))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_LEVEL, &level))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_DOUBLEBUFFER,
            &double_buffered))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_STEREO, &stereo))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_AUX_BUFFERS,
            &aux_buffers))
      return;
    GST_DEBUG_OBJECT (glx, "Level: %i, buffer size: %i, double buffered: %i, "
        "stereo: %i, aux buffers: %i", level, buffer_size, double_buffered,
        stereo, aux_buffers);
  }

  if (render_type & GLX_RGBA_BIT) {
    int r, g, b, a;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_RED_SIZE, &r))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_GREEN_SIZE, &g))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_BLUE_SIZE, &b))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ALPHA_SIZE, &a))
      return;
    GST_DEBUG_OBJECT (glx, "[R, G, B, A] = [%i, %i, %i, %i]", r, g, b, a);
  }

  {
    int d, s;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_DEPTH_SIZE, &d))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_STENCIL_SIZE, &s))
      return;

    GST_DEBUG_OBJECT (glx, "[D, S] = [%i, %i]", d, s);
  }

  {
    int r, g, b, a;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ACCUM_RED_SIZE, &r))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ACCUM_GREEN_SIZE,
            &g))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ACCUM_BLUE_SIZE,
            &b))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_ACCUM_ALPHA_SIZE,
            &a))
      return;
    GST_DEBUG_OBJECT (glx, "Accumulation [R, G, B, A] = [%i, %i, %i, %i]", r, g,
        b, a);
  }

  {
    int transparent_type;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_TRANSPARENT_TYPE,
            &transparent_type))
      return;

    if (transparent_type == GLX_NONE) {
      GST_DEBUG_OBJECT (glx, "Is opaque");
    } else if (transparent_type == GLX_TRANSPARENT_INDEX) {
      int transparent_index;
      if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_TRANSPARENT_INDEX,
              &transparent_index))
        return;
      GST_DEBUG_OBJECT (glx, "Is transparent for index value 0x%x",
          transparent_index);
    } else if (transparent_type == GLX_TRANSPARENT_RGB) {
      int r, g, b, a;
      if (Success != glXGetFBConfigAttrib (dpy, fbconfig,
              GLX_TRANSPARENT_RED_VALUE, &r))
        return;
      if (Success != glXGetFBConfigAttrib (dpy, fbconfig,
              GLX_TRANSPARENT_GREEN_VALUE, &g))
        return;
      if (Success != glXGetFBConfigAttrib (dpy, fbconfig,
              GLX_TRANSPARENT_BLUE_VALUE, &b))
        return;
      if (Success != glXGetFBConfigAttrib (dpy, fbconfig,
              GLX_TRANSPARENT_ALPHA_VALUE, &a))
        return;
      GST_DEBUG_OBJECT (glx, "Is transparent for value [R, G, B, A] = "
          "[0x%x, 0x%x, 0x%x, 0x%x]", r, g, b, a);
    } else {
      GST_DEBUG_OBJECT (glx, "Unknown transparent type 0x%x", transparent_type);
    }
  }

  {
    int w, h, pixels;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_WIDTH,
            &w))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_HEIGHT,
            &h))
      return;
    if (Success != glXGetFBConfigAttrib (dpy, fbconfig, GLX_MAX_PBUFFER_PIXELS,
            &pixels))
      return;
    GST_DEBUG_OBJECT (glx,
        "PBuffer maximum dimensions are [%i, %i]. Max pixels are %i", w,
        h, pixels);
  }
#undef SIMPLE_STRING_ASSIGN
}

static void
gst_gl_context_glx_dump_all_fb_configs (GstGLContextGLX * glx,
    Display * dpy, int screen)
{
  int i, n;
  GLXFBConfig *configs;

  configs = glXGetFBConfigs (dpy, screen, &n);

  for (i = 0; i < n; i++) {
    gst_gl_context_glx_dump_fb_config (glx, dpy, configs[i]);
  }

  XFree (configs);
}

static int *
fb_config_attributes_from_structure (GstStructure * config)
{
  guint i = 0, n;
  int *ret;

  if (!config) {
    gint attribs[] = {
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      GLX_DEPTH_SIZE, 16,
      GLX_DOUBLEBUFFER, True,
      None
    };

    return g_memdup2 (attribs, sizeof (attribs));
  }

  n = gst_structure_n_fields (config) * 2 + 1;
  ret = g_new0 (gint, n);

#define TRANSFORM_VALUE(GL_CONF_NAME,GLX_ATTR_NAME) \
  G_STMT_START { \
    if (gst_structure_has_field_typed (config, \
          GST_GL_CONFIG_ATTRIB_NAME(GL_CONF_NAME), \
          GST_GL_CONFIG_ATTRIB_GTYPE(GL_CONF_NAME))) { \
      int val; \
      if (gst_structure_get (config, \
          GST_GL_CONFIG_ATTRIB_NAME(GL_CONF_NAME), \
          GST_GL_CONFIG_ATTRIB_GTYPE(GL_CONF_NAME), &val, NULL)) { \
        ret[i++] = GLX_ATTR_NAME; \
        ret[i++] = (int) val; \
      } \
    } \
  } G_STMT_END

  TRANSFORM_VALUE (CONFIG_ID, GLX_FBCONFIG_ID);
  TRANSFORM_VALUE (RED_SIZE, GLX_RED_SIZE);
  TRANSFORM_VALUE (GREEN_SIZE, GLX_GREEN_SIZE);
  TRANSFORM_VALUE (BLUE_SIZE, GLX_BLUE_SIZE);
  TRANSFORM_VALUE (ALPHA_SIZE, GLX_ALPHA_SIZE);
  TRANSFORM_VALUE (DEPTH_SIZE, GLX_DEPTH_SIZE);
  TRANSFORM_VALUE (STENCIL_SIZE, GLX_STENCIL_SIZE);
  /* TODO: more values */

#undef TRANSFORM_VALUE

  ret[i++] = None;
  g_assert (i <= n);
  return ret;
}

static GLXContext
_create_context_with_flags (GstGLContextGLX * context_glx, Display * dpy,
    GLXFBConfig fbconfig, GLXContext share_context, gint major, gint minor,
    gint contextFlags, gint profileMask)
{
  GLXContext ret;
#define N_ATTRIBS 20
  gint attribs[N_ATTRIBS];
  int x_error = 0;
  gint n = 0;

  if (major) {
    attribs[n++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
    attribs[n++] = major;
  }
  if (minor) {
    attribs[n++] = GLX_CONTEXT_MINOR_VERSION_ARB;
    attribs[n++] = minor;
  }
  if (contextFlags) {
    attribs[n++] = GLX_CONTEXT_FLAGS_ARB;
    attribs[n++] = contextFlags;
  }
#ifdef GLX_ARB_create_context_profile
  if (profileMask) {
    attribs[n++] = GLX_CONTEXT_PROFILE_MASK_ARB;
    attribs[n++] = profileMask;
  }
#endif
  attribs[n++] = None;

  g_assert (n < N_ATTRIBS);
#undef N_ATTRIBS

  gst_gl_window_x11_trap_x_errors ();
  ret = context_glx->priv->glXCreateContextAttribsARB (dpy, fbconfig,
      share_context, True, attribs);
  x_error = gst_gl_window_x11_untrap_x_errors ();

  if (x_error)
    ret = 0;

  return ret;
}

static gboolean
gst_gl_context_glx_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error)
{
  GstGLContextGLX *context_glx;
  GstGLWindow *window;
  GstGLWindowX11 *window_x11;
  GstGLDisplay *display = NULL;
  gboolean create_context;
  const char *glx_exts;
  Display *device;
  guintptr external_gl_context = 0;

  context_glx = GST_GL_CONTEXT_GLX (context);
  window = gst_gl_context_get_window (context);

  if (!GST_IS_GL_WINDOW_X11 (window)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "Cannot create an GLX context from a non-X11 window");
    goto failure;
  }

  window_x11 = GST_GL_WINDOW_X11 (window);
  display = gst_gl_context_get_display (context);

  if (other_context) {
    if (gst_gl_context_get_gl_platform (other_context) != GST_GL_PLATFORM_GLX) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Cannot share context with non-GLX context");
      goto failure;
    }

    external_gl_context = gst_gl_context_get_gl_context (other_context);
  }

  device = (Display *) gst_gl_display_get_handle (display);
  if (!device) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE, "Invalid Display handle");
    goto failure;
  }

  glx_exts = glXQueryExtensionsString (device, DefaultScreen (device));

  create_context = gst_gl_check_extension ("GLX_ARB_create_context", glx_exts);
  context_glx->priv->glXCreateContextAttribsARB =
      (gpointer) glXGetProcAddressARB ((const GLubyte *)
      "glXCreateContextAttribsARB");

  if (!context_glx->glx_context && gl_api & GST_GL_API_OPENGL3 && create_context
      && context_glx->priv->glXCreateContextAttribsARB) {
    gint i;

    for (i = 0; i < G_N_ELEMENTS (opengl_versions); i++) {
      gint profileMask = 0;
      gint contextFlags = 0;

      if ((opengl_versions[i].major > 3
              || (opengl_versions[i].major == 3
                  && opengl_versions[i].minor >= 2))) {
        profileMask |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
        contextFlags |= GLX_CONTEXT_DEBUG_BIT_ARB;
      } else {
        break;
      }

      GST_DEBUG_OBJECT (context, "trying to create a GL %d.%d context",
          opengl_versions[i].major, opengl_versions[i].minor);

      context_glx->glx_context = _create_context_with_flags (context_glx,
          device, context_glx->priv->fbconfigs[0],
          (GLXContext) external_gl_context, opengl_versions[i].major,
          opengl_versions[i].minor, contextFlags, profileMask);

      if (context_glx->glx_context) {
        context_glx->priv->context_api = GST_GL_API_OPENGL3;
        break;
      }
    }
  }
  if (!context_glx->glx_context && gl_api & GST_GL_API_OPENGL) {
    context_glx->glx_context =
        glXCreateContext (device, window_x11->visual_info,
        (GLXContext) external_gl_context, TRUE);
    context_glx->priv->context_api = GST_GL_API_OPENGL;
  }

  if (!context_glx->glx_context) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT, "Failed to create opengl context");
    goto failure;
  }

  GST_LOG ("gl context id: %ld", (gulong) context_glx->glx_context);

  gst_object_unref (window);
  gst_object_unref (display);

  return TRUE;

failure:
  if (window)
    gst_object_unref (window);
  if (display)
    gst_object_unref (display);

  return FALSE;
}

static void
gst_gl_context_glx_destroy_context (GstGLContext * context)
{
  GstGLWindow *window;
  GstGLContextGLX *context_glx;
  Display *device;

  context_glx = GST_GL_CONTEXT_GLX (context);
  window = gst_gl_context_get_window (context);
  device = (Display *) gst_gl_display_get_handle (window->display);

  if (context_glx->priv->fbconfigs)
    XFree (context_glx->priv->fbconfigs);
  context_glx->priv->fbconfigs = NULL;

  glXDestroyContext (device, context_glx->glx_context);

  context_glx->glx_context = 0;

  if (context_glx->priv->requested_config)
    gst_structure_free (context_glx->priv->requested_config);
  context_glx->priv->requested_config = NULL;

  gst_object_unref (window);
}

static gboolean
gst_gl_context_glx_choose_format (GstGLContext * context, GError ** error)
{
  GstGLContextGLX *context_glx;
  GstGLWindow *window;
  GstGLWindowX11 *window_x11;
  gint error_base;
  gint event_base;
  Display *device;

  context_glx = GST_GL_CONTEXT_GLX (context);
  window = gst_gl_context_get_window (context);

  if (!GST_IS_GL_WINDOW_X11 (window)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "Cannot create an GLX context from a non-X11 window");
    goto failure;
  }
  window_x11 = GST_GL_WINDOW_X11 (window);

  device = (Display *) gst_gl_display_get_handle (window->display);
  if (!device) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE, "Invalid Display handle");
    goto failure;
  }

  if (!glXQueryExtension (device, &error_base, &event_base)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE, "No GLX extension");
    goto failure;
  }

  if (!glXQueryVersion (device, &context_glx->priv->glx_major,
          &context_glx->priv->glx_minor)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "Failed to query GLX version (glXQueryVersion failed)");
    goto failure;
  }

  GST_INFO ("GLX Version: %d.%d", context_glx->priv->glx_major,
      context_glx->priv->glx_minor);

  /* legacy case */
  if (context_glx->priv->glx_major < 1 || (context_glx->priv->glx_major == 1
          && context_glx->priv->glx_minor < 3)) {
    gint attribs[] = {
      GLX_RGBA,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      GLX_DEPTH_SIZE, 16,
      GLX_DOUBLEBUFFER,
      None
    };

    window_x11->visual_info = glXChooseVisual (device,
        window_x11->screen_num, attribs);

    if (!window_x11->visual_info) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Bad attributes in glXChooseVisual");
      goto failure;
    }
  } else {
    int fbcount;
    int *attribs;

    attribs =
        fb_config_attributes_from_structure (context_glx->
        priv->requested_config);

    gst_gl_context_glx_dump_all_fb_configs (context_glx, device,
        DefaultScreen (device));

    context_glx->priv->fbconfigs = glXChooseFBConfig (device,
        DefaultScreen (device), attribs, &fbcount);

    g_free (attribs);

    if (!context_glx->priv->fbconfigs) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
          "Could not find any FBConfig's to use (check attributes?)");
      goto failure;
    }

    GST_DEBUG_OBJECT (context_glx, "Chosen GLXFBConfig:");
    gst_gl_context_glx_dump_fb_config (context_glx, device,
        context_glx->priv->fbconfigs[0]);

    window_x11->visual_info = glXGetVisualFromFBConfig (device,
        context_glx->priv->fbconfigs[0]);

    if (!window_x11->visual_info) {
      g_set_error (error, GST_GL_CONTEXT_ERROR,
          GST_GL_CONTEXT_ERROR_WRONG_CONFIG, "Bad attributes in FBConfig");
      goto failure;
    }
  }

  gst_gl_window_x11_create_window ((GstGLWindowX11 *) window);

  gst_object_unref (window);

  return TRUE;

failure:
  if (window)
    gst_object_unref (window);

  return FALSE;
}

static void
gst_gl_context_glx_swap_buffers (GstGLContext * context)
{
  GstGLWindow *window = gst_gl_context_get_window (context);
  Display *device = (Display *) gst_gl_display_get_handle (window->display);
  Window window_handle = (Window) gst_gl_window_get_window_handle (window);

  glXSwapBuffers (device, window_handle);

  gst_object_unref (window);
}

static guintptr
gst_gl_context_glx_get_gl_context (GstGLContext * context)
{
  return (guintptr) GST_GL_CONTEXT_GLX (context)->glx_context;
}

static gboolean
gst_gl_context_glx_activate (GstGLContext * context, gboolean activate)
{
  GstGLWindow *window = gst_gl_context_get_window (context);
  Display *device = (Display *) gst_gl_display_get_handle (window->display);
  Window window_handle = (Window) gst_gl_window_get_window_handle (window);
  gboolean result;

  if (activate) {
    result = glXMakeCurrent (device, window_handle,
        GST_GL_CONTEXT_GLX (context)->glx_context);
  } else {
    result = glXMakeCurrent (device, None, NULL);
  }

  gst_object_unref (window);

  return result;
}

GstGLAPI
gst_gl_context_glx_get_gl_api (GstGLContext * context)
{
  GstGLContextGLX *context_glx;

  context_glx = GST_GL_CONTEXT_GLX (context);

  return context_glx->priv->context_api;
}

static GstGLPlatform
gst_gl_context_glx_get_gl_platform (GstGLContext * context)
{
  return GST_GL_PLATFORM_GLX;
}

gpointer
gst_gl_context_glx_get_proc_address (GstGLAPI gl_api, const gchar * name)
{
  gpointer result;

  if (!(result = gst_gl_context_default_get_proc_address (gl_api, name))) {
    result = glXGetProcAddressARB ((const GLubyte *) name);
  }

  return result;
}

guintptr
gst_gl_context_glx_get_current_context (void)
{
  return (guintptr) glXGetCurrentContext ();
}

static void
gst_gl_context_glx_get_gl_platform_version (GstGLContext * context,
    gint * major, gint * minor)
{
  GstGLContextGLX *context_glx = GST_GL_CONTEXT_GLX (context);

  *major = context_glx->priv->glx_major;
  *minor = context_glx->priv->glx_minor;
}

static GstStructure *
gst_gl_context_glx_get_config (GstGLContext * context)
{
  GstGLContextGLX *glx = GST_GL_CONTEXT_GLX (context);
  GstGLWindow *window;
  GstGLWindowX11 *window_x11;
  Display *device;
  GstStructure *ret;

  window = gst_gl_context_get_window (context);
  device = (Display *) gst_gl_display_get_handle (window->display);
  window_x11 = GST_GL_WINDOW_X11 (window);

  g_return_val_if_fail (glx->priv->fbconfigs || window_x11->visual_info, NULL);

  if (glx->priv->fbconfigs) {
    ret = fb_config_to_structure (context, device, glx->priv->fbconfigs[0]);
  } else {
    /*TODO: XVisualInfo for really old GLX/X11 versions, */
    ret = NULL;
  }
  gst_object_unref (window);
  return ret;
}

static gboolean
gst_gl_context_glx_request_config (GstGLContext * context,
    GstStructure * config)
{
  GstGLContextGLX *glx = GST_GL_CONTEXT_GLX (context);

  if (glx->priv->requested_config)
    gst_structure_free (glx->priv->requested_config);
  glx->priv->requested_config = config;

  return TRUE;
}

gboolean
gst_gl_context_glx_fill_info (GstGLContext * context, GError ** error)
{
  GLXContext glx_context = (GLXContext) gst_gl_context_get_gl_context (context);
  GstStructure *config;
  Display *device;
  GLXFBConfig *fbconfigs;
  int fbconfig_id, n_fbconfigs;
  int glx_major, glx_minor;
  int attrs[3];

  if (!glx_context) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE, "No GLX context");
    return FALSE;
  }

  device = (Display *) gst_gl_display_get_handle (context->display);

  if (!glXQueryVersion (device, &glx_major, &glx_minor)) {
    GST_WARNING_OBJECT (context, "could not retrieve GLX version");
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE,
        "could not retrieve GLX version");
    return FALSE;
  }

  if (!GST_GL_CHECK_GL_VERSION (glx_major, glx_minor, 1, 4)) {
    GST_FIXME_OBJECT (context, "No support for retrieving the "
        "GstGLContextConfig from GLX < 1.4, have %u.%u", glx_major, glx_minor);
    return TRUE;
  }

  if (Success != glXQueryContext (device, glx_context, GLX_FBCONFIG_ID,
          &fbconfig_id)) {
    GST_WARNING_OBJECT (context,
        "could not retrieve fbconfig id from glx context");
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not retrieve fbconfig id from glx context");
    goto failure;
  }

  attrs[0] = GLX_FBCONFIG_ID;
  attrs[1] = fbconfig_id;
  attrs[2] = None;

  fbconfigs = glXChooseFBConfig (device, DefaultScreen (device), attrs,
      &n_fbconfigs);
  if (!fbconfigs || n_fbconfigs <= 0) {
    GST_WARNING_OBJECT (context,
        "could not retrieve fbconfig from its ID 0x%x. "
        "Wrong Display or Screen?", fbconfig_id);
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not retrieve fbconfig from its ID 0x%x. "
        "Wrong Display or Screen?", fbconfig_id);
    goto failure;
  }

  config = fb_config_to_structure (context, device, fbconfigs[0]);
  if (!config) {
    GST_WARNING_OBJECT (context, "could not transform fbconfig id 0x%x into "
        "GstStructure.", fbconfig_id);
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
        "could not transform fbconfig id 0x%x into GstStructure.", fbconfig_id);
    goto failure;
  }

  GST_INFO_OBJECT (context, "found config %" GST_PTR_FORMAT, config);

  g_object_set_data_full (G_OBJECT (context),
      GST_GL_CONTEXT_WRAPPED_GL_CONFIG_NAME, config,
      (GDestroyNotify) gst_structure_free);

  XFree (fbconfigs);

  return TRUE;

failure:
  return FALSE;
}
