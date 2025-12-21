/*
 * GStreamer
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
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

/**
 * SECTION:gstglcontext
 * @short_description: OpenGL context abstraction
 * @title: GstGLContext
 * @see_also: #GstGLDisplay, #GstGLWindow
 *
 * #GstGLContext wraps an OpenGL context object in a uniform API.  As a result
 * of the limitation on OpenGL context, this object is not thread safe unless
 * specified and must only be activated in a single thread.
 *
 * Environment variables:
 * - `GST_GL_API`: select which OpenGL API to create and OpenGL context for.
 *                 Depending on the platform, the available values are
 *                 'opengl', 'opengl3' (core profile), and 'gles2'.  See the
 *                 the #GstGLAPI enumeration for more details.
 * - `GST_GL_PLATFORM`: select which OpenGL platform to create an OpenGL
 *                      context with.  Depending on the platform and the
 *                      dependencies available build-time, the available values
 *                      are, 'glx', 'egl', 'cgl', 'wgl', and 'eagl'
 * - `GST_GL_CONFIG`: select the configuration used for creating the OpenGL
 *                    context and OpenGL surface.  Written out as a GstStructure
 *                    that has been serialized to string.  e.g.
 *                    `GST_GL_CONFIG="gst-gl-context-config,red-size=8,green-size=8,blue-size=8,alpha-size=8,depth-size=16"`.
 *                    Not all platforms will support the same level of
 *                    functionality.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(ANDROID) || defined(__ANDROID__)
/* Avoid a linker error with _isoc99_sscanf() when building a shared library
 * for android
 */
#define _GNU_SOURCE
#endif

#include "gstglcontext.h"
#include <gst/gl/gl.h>

#include <gmodule.h>
#include <string.h>
#include <stdio.h>

#include "gstglcontext_private.h"
#include "gstglfeature.h"
#include "gstglfeature_private.h"
#include "gstglfuncs.h"

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x0000821d
#endif

#if GST_GL_HAVE_PLATFORM_GLX
#include "x11/gstglcontext_glx.h"
#endif
#if GST_GL_HAVE_PLATFORM_EGL
#include "egl/gstglcontext_egl.h"
#endif
#if GST_GL_HAVE_PLATFORM_CGL
#include "cocoa/gstglcontext_cocoa.h"
#endif
#if GST_GL_HAVE_PLATFORM_WGL
#include "wgl/gstglcontext_wgl.h"
#endif
#if GST_GL_HAVE_PLATFORM_EAGL
#include "eagl/gstglcontext_eagl.h"
#endif

extern void _gst_gl_debug_enable (GstGLContext * context);

static GPrivate current_context_key;

static GModule *module_self;
static GOnce module_self_gonce = G_ONCE_INIT;

#if GST_GL_HAVE_OPENGL
static GOnce module_opengl_gonce = G_ONCE_INIT;
static GModule *module_opengl;

static gpointer
load_opengl_module (gpointer user_data)
{
#ifdef GST_GL_LIBGL_MODULE_NAME
  module_opengl = g_module_open (GST_GL_LIBGL_MODULE_NAME, G_MODULE_BIND_LAZY);
#else
  /* On Linux the .so is only in -dev packages, try with a real soname
   * Proper compilers will optimize away the strcmp */
  if (g_strcmp0 (G_MODULE_SUFFIX, "so") == 0)
    module_opengl = g_module_open ("libGL.so.1", G_MODULE_BIND_LAZY);
  else if (g_strcmp0 (G_MODULE_SUFFIX, "dll") == 0)
    module_opengl = g_module_open ("opengl32.dll", G_MODULE_BIND_LAZY);

  /* This automatically handles the suffix and even .la files */
  if (!module_opengl)
    module_opengl = g_module_open ("libGL", G_MODULE_BIND_LAZY);
#endif

  return NULL;
}
#endif

#if GST_GL_HAVE_GLES2
static GOnce module_gles2_gonce = G_ONCE_INIT;
static GModule *module_gles2;

static gpointer
load_gles2_module (gpointer user_data)
{
#ifdef GST_GL_LIBGLESV2_MODULE_NAME
  module_gles2 =
      g_module_open (GST_GL_LIBGLESV2_MODULE_NAME, G_MODULE_BIND_LAZY);
#else
  /* On Linux the .so is only in -dev packages, try with a real soname
   * Proper compilers will optimize away the strcmp */
  if (g_strcmp0 (G_MODULE_SUFFIX, "so") == 0)
    module_gles2 = g_module_open ("libGLESv2.so.2", G_MODULE_BIND_LAZY);

  /* This automatically handles the suffix and even .la files */
  if (!module_gles2)
    module_gles2 = g_module_open ("libGLESv2", G_MODULE_BIND_LAZY);

#endif

  return NULL;
}
#endif

static gpointer
load_self_module (gpointer user_data)
{
  module_self = g_module_open (NULL, G_MODULE_BIND_LAZY);

  return NULL;
}

/* Context sharedness is tracked by a refcounted pointer stored in each context
 * object to track complex creation/deletion scenarios.  As a result,
 * sharedness can only be successfully validated between two GstGLContext's
 * where one is not a wrapped context.
 *
 * As there is no API at the winsys level to tell whether two OpenGL contexts
 * can share GL resources, this is the next best thing.
 *
 * XXX: we may need a way to associate two wrapped GstGLContext's as being
 * shared however I have not come across a use case that requires this yet.
 */
struct ContextShareGroup
{
  int refcount;
};

static struct ContextShareGroup *
_context_share_group_new (void)
{
  struct ContextShareGroup *ret = g_new0 (struct ContextShareGroup, 1);

  ret->refcount = 1;

  return ret;
}

static struct ContextShareGroup *
_context_share_group_ref (struct ContextShareGroup *share)
{
  g_atomic_int_inc (&share->refcount);
  return share;
}

static void
_context_share_group_unref (struct ContextShareGroup *share)
{
  if (g_atomic_int_dec_and_test (&share->refcount))
    g_free (share);
}

static gboolean
_context_share_group_is_shared (struct ContextShareGroup *share)
{
  return g_atomic_int_get (&share->refcount) > 1;
}

#define GST_CAT_DEFAULT gst_gl_context_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);
GST_DEBUG_CATEGORY_STATIC (gst_gl_debug);

static void _init_debug (void);

static gpointer gst_gl_context_create_thread (GstGLContext * context);
static void gst_gl_context_finalize (GObject * object);
static void gst_gl_context_default_get_gl_platform_version (GstGLContext *
    context, gint * major, gint * minor);

struct _GstGLContextPrivate
{
  GThread *gl_thread;
  GThread *active_thread;

  /* conditions */
  GMutex render_lock;
  GCond create_cond;
  GCond destroy_cond;

  gboolean created;
  gboolean alive;

  GWeakRef other_context_ref;
  struct ContextShareGroup *sharegroup;
  GError **error;

  gint gl_major;
  gint gl_minor;

  gchar *gl_exts;
  GstStructure *requested_config;
};

typedef struct
{
  GstGLContext parent;

  guintptr handle;
  GstGLPlatform platform;
  GstGLAPI available_apis;
} GstGLWrappedContext;

typedef struct
{
  GstGLContextClass parent;
} GstGLWrappedContextClass;

#define gst_gl_context_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstGLContext, gst_gl_context,
    GST_TYPE_OBJECT);

G_DEFINE_TYPE (GstGLWrappedContext, gst_gl_wrapped_context,
    GST_TYPE_GL_CONTEXT);

/**
 * gst_gl_context_error_quark:
 *
 * Returns: the quark used for #GstGLContext in #GError's
 */
GQuark
gst_gl_context_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-context-error-quark");
}

static void
_ensure_window (GstGLContext * context)
{
  GstGLWindow *window;

  if (context->window)
    return;

  window = gst_gl_display_create_window (context->display);

  gst_gl_context_set_window (context, window);

  gst_object_unref (window);
}

static void
gst_gl_context_init (GstGLContext * context)
{
  context->priv = gst_gl_context_get_instance_private (context);

  context->window = NULL;
  context->gl_vtable = g_new0 (GstGLFuncs, 1);

  g_mutex_init (&context->priv->render_lock);

  g_cond_init (&context->priv->create_cond);
  g_cond_init (&context->priv->destroy_cond);
  context->priv->created = FALSE;

  g_weak_ref_init (&context->priv->other_context_ref, NULL);
}

static void
gst_gl_context_class_init (GstGLContextClass * klass)
{
  klass->get_proc_address =
      GST_DEBUG_FUNCPTR (gst_gl_context_default_get_proc_address);
  klass->get_gl_platform_version =
      GST_DEBUG_FUNCPTR (gst_gl_context_default_get_gl_platform_version);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_context_finalize;

  _init_debug ();
}

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_context_debug, "glcontext", 0,
        "glcontext element");
    GST_DEBUG_CATEGORY_INIT (gst_gl_debug, "gldebug", 0, "OpenGL Debugging");
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_gl_context_new:
 * @display: a #GstGLDisplay
 *
 * Create a new #GstGLContext with the specified @display
 *
 * Returns: a new #GstGLContext
 *
 * Since: 1.4
 */
GstGLContext *
gst_gl_context_new (GstGLDisplay * display)
{
  GstGLContext *context = NULL;
  const gchar *user_choice;

  _init_debug ();

  user_choice = g_getenv ("GST_GL_PLATFORM");
  GST_INFO ("creating a context for display %" GST_PTR_FORMAT
      ", user choice:%s", display, user_choice);
#if GST_GL_HAVE_PLATFORM_CGL
  if (!context && (!user_choice || g_strstr_len (user_choice, 3, "cgl")))
    context = GST_GL_CONTEXT (gst_gl_context_cocoa_new (display));
#endif
#if GST_GL_HAVE_PLATFORM_GLX
  if (!context && (!user_choice || g_strstr_len (user_choice, 3, "glx")))
    context = GST_GL_CONTEXT (gst_gl_context_glx_new (display));
#endif
#if GST_GL_HAVE_PLATFORM_WGL
  if (!context && (!user_choice || g_strstr_len (user_choice, 3, "wgl")))
    context = GST_GL_CONTEXT (gst_gl_context_wgl_new (display));
#endif
#if GST_GL_HAVE_PLATFORM_EAGL
  if (!context && (!user_choice || g_strstr_len (user_choice, 4, "eagl")))
    context = GST_GL_CONTEXT (gst_gl_context_eagl_new (display));
#endif
#if GST_GL_HAVE_PLATFORM_EGL
  if (!context && (!user_choice || g_strstr_len (user_choice, 3, "egl")))
    context = GST_GL_CONTEXT (gst_gl_context_egl_new (display));
#endif

  if (!context) {
    /* subclass returned a NULL context */
    GST_WARNING ("Could not create context. user specified %s",
        user_choice ? user_choice : "(null)");

    return NULL;
  }

  context->display = gst_object_ref (display);

  GST_DEBUG_OBJECT (context,
      "Done creating context for display %" GST_PTR_FORMAT " (user_choice:%s)",
      display, user_choice);

  return context;
}

/**
 * gst_gl_context_new_wrapped:
 * @display: a #GstGLDisplay
 * @handle: the OpenGL context to wrap
 * @context_type: a #GstGLPlatform specifying the type of context in @handle
 * @available_apis: a #GstGLAPI containing the available OpenGL apis in @handle
 *
 * Wraps an existing OpenGL context into a #GstGLContext.
 *
 * Note: The caller is responsible for ensuring that the OpenGL context
 * represented by @handle stays alive while the returned #GstGLContext is
 * active.
 *
 * @context_type must not be %GST_GL_PLATFORM_NONE or %GST_GL_PLATFORM_ANY
 *
 * @available_apis must not be %GST_GL_API_NONE or %GST_GL_API_ANY
 *
 * Returns: (transfer full) (nullable): a #GstGLContext wrapping @handle
 *
 * Since: 1.4
 */
GstGLContext *
gst_gl_context_new_wrapped (GstGLDisplay * display, guintptr handle,
    GstGLPlatform context_type, GstGLAPI available_apis)
{
  GstGLContext *context;
  GstGLWrappedContext *context_wrap = NULL;
  GstGLContextClass *context_class;
  GstGLAPI display_api;

  _init_debug ();

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), NULL);
  g_return_val_if_fail (handle != 0, NULL);
  g_return_val_if_fail (available_apis != GST_GL_API_ANY, NULL);
  g_return_val_if_fail (available_apis != GST_GL_API_NONE, NULL);
  g_return_val_if_fail (context_type != GST_GL_PLATFORM_NONE, NULL);
  g_return_val_if_fail (context_type != GST_GL_PLATFORM_ANY, NULL);

  display_api = gst_gl_display_get_gl_api (display);
  g_return_val_if_fail ((display_api & available_apis) != GST_GL_API_NONE,
      NULL);

  context_wrap = g_object_new (GST_TYPE_GL_WRAPPED_CONTEXT, NULL);
  gst_object_ref_sink (context_wrap);

  context = (GstGLContext *) context_wrap;

  context->display = gst_object_ref (display);
  context->priv->sharegroup = _context_share_group_new ();
  context_wrap->handle = handle;
  context_wrap->platform = context_type;
  context_wrap->available_apis = available_apis;

  context_class = GST_GL_CONTEXT_GET_CLASS (context);

#if GST_GL_HAVE_PLATFORM_GLX
  if (context_type == GST_GL_PLATFORM_GLX) {
    context_class->get_current_context = gst_gl_context_glx_get_current_context;
    context_class->get_proc_address = gst_gl_context_glx_get_proc_address;
  }
#endif
#if GST_GL_HAVE_PLATFORM_EGL
  if (context_type == GST_GL_PLATFORM_EGL) {
    context_class->get_current_context = gst_gl_context_egl_get_current_context;
    context_class->get_proc_address = gst_gl_context_egl_get_proc_address;
  }
#endif
#if GST_GL_HAVE_PLATFORM_CGL
  if (context_type == GST_GL_PLATFORM_CGL) {
    context_class->get_current_context =
        gst_gl_context_cocoa_get_current_context;
    context_class->get_proc_address = gst_gl_context_default_get_proc_address;
  }
#endif
#if GST_GL_HAVE_PLATFORM_WGL
  if (context_type == GST_GL_PLATFORM_WGL) {
    context_class->get_current_context = gst_gl_context_wgl_get_current_context;
    context_class->get_proc_address = gst_gl_context_wgl_get_proc_address;
  }
#endif
#if GST_GL_HAVE_PLATFORM_EAGL
  if (context_type == GST_GL_PLATFORM_EAGL) {
    context_class->get_current_context =
        gst_gl_context_eagl_get_current_context;
    context_class->get_proc_address = gst_gl_context_default_get_proc_address;
  }
#endif

  if (!context_class->get_current_context) {
    /* we don't have API support */
    gst_object_unref (context);
    return NULL;
  }

  return context;
}

/**
 * gst_gl_context_get_current_gl_context:
 * @context_type: a #GstGLPlatform specifying the type of context to retrieve
 *
 * Returns: (nullable): The OpenGL context handle current in the calling thread or %NULL
 *
 * Since: 1.6
 */
guintptr
gst_gl_context_get_current_gl_context (GstGLPlatform context_type)
{
  guintptr handle = 0;

  _init_debug ();

#if GST_GL_HAVE_PLATFORM_GLX
  if (!handle && (context_type & GST_GL_PLATFORM_GLX) != 0)
    handle = gst_gl_context_glx_get_current_context ();
#endif
#if GST_GL_HAVE_PLATFORM_EGL
  if (!handle && (context_type & GST_GL_PLATFORM_EGL) != 0)
    handle = gst_gl_context_egl_get_current_context ();
#endif
#if GST_GL_HAVE_PLATFORM_CGL
  if (!handle && (context_type & GST_GL_PLATFORM_CGL) != 0)
    handle = gst_gl_context_cocoa_get_current_context ();
#endif
#if GST_GL_HAVE_PLATFORM_WGL
  if (!handle && (context_type & GST_GL_PLATFORM_WGL) != 0)
    handle = gst_gl_context_wgl_get_current_context ();
#endif
#if GST_GL_HAVE_PLATFORM_EAGL
  if (!handle && (context_type & GST_GL_PLATFORM_EAGL) != 0)
    handle = gst_gl_context_eagl_get_current_context ();
#endif

  if (!handle)
    GST_WARNING ("Could not retrieve current context");

  return handle;
}

/**
 * gst_gl_context_get_proc_address_with_platform:
 * @context_type: a #GstGLPlatform
 * @gl_api: a #GstGLAPI
 * @name: the name of the function to retrieve
 *
 * Attempts to use the @context_type specific GetProcAddress implementations
 * to retrieve @name.
 *
 * See also gst_gl_context_get_proc_address().
 *
 * Returns: (nullable): a function pointer for @name, or %NULL
 *
 * Since: 1.6
 */
gpointer
gst_gl_context_get_proc_address_with_platform (GstGLPlatform context_type,
    GstGLAPI gl_api, const gchar * name)
{
  gpointer ret = NULL;

#if GST_GL_HAVE_PLATFORM_GLX
  if (!ret && (context_type & GST_GL_PLATFORM_GLX) != 0)
    ret = gst_gl_context_glx_get_proc_address (gl_api, name);
#endif
#if GST_GL_HAVE_PLATFORM_EGL
  if (!ret && (context_type & GST_GL_PLATFORM_EGL) != 0)
    ret = gst_gl_context_egl_get_proc_address (gl_api, name);
#endif
#if GST_GL_HAVE_PLATFORM_WGL
  if (!ret && (context_type & GST_GL_PLATFORM_WGL) != 0)
    ret = gst_gl_context_wgl_get_proc_address (gl_api, name);
#endif
  /* CGL and EAGL rely on the default impl */

  if (!ret)
    ret = gst_gl_context_default_get_proc_address (gl_api, name);

  return ret;
}

/**
 * gst_gl_context_get_current_gl_api:
 * @platform: the #GstGLPlatform to retrieve the API for
 * @major: (out) (allow-none): the major version
 * @minor: (out) (allow-none): the minor version
 *
 * If an error occurs, @major and @minor are not modified and %GST_GL_API_NONE is
 * returned.
 *
 * Returns: The version supported by the OpenGL context current in the calling
 *          thread or %GST_GL_API_NONE
 *
 * Since: 1.6
 */
GstGLAPI
gst_gl_context_get_current_gl_api (GstGLPlatform platform, guint * major,
    guint * minor)
{
  const GLubyte *(GSTGLAPI * GetString) (GLenum name);
#if GST_GL_HAVE_OPENGL
  void (GSTGLAPI * GetIntegerv) (GLenum name, GLuint * n);
#endif
  const gchar *version;
  gint maj, min, n;
  GstGLAPI ret = (1U << 31);

  _init_debug ();

  while (ret != GST_GL_API_NONE) {
    /* FIXME: attempt to delve into the platform specific GetProcAddress */
    GetString =
        gst_gl_context_get_proc_address_with_platform (platform, ret,
        "glGetString");
#if GST_GL_HAVE_OPENGL
    GetIntegerv =
        gst_gl_context_get_proc_address_with_platform (platform, ret,
        "glGetIntegerv");
#endif
    if (!GetString) {
      goto next;
    }

    version = (const gchar *) GetString (GL_VERSION);
    if (!version)
      goto next;

    /* strlen (x.x) == 3 */
    n = strlen (version);
    if (n < 3)
      goto next;

    if (g_strstr_len (version, 9, "OpenGL ES")) {
      /* strlen (OpenGL ES x.x) == 13 */
      if (n < 13)
        goto next;

      sscanf (&version[10], "%d.%d", &maj, &min);

      if (maj <= 0 || min < 0)
        goto next;

      if (maj == 1) {
        ret = GST_GL_API_GLES1;
        break;
      } else if (maj == 2 || maj == 3) {
        ret = GST_GL_API_GLES2;
        break;
      }

      goto next;
    } else {
      sscanf (version, "%d.%d", &maj, &min);

      if (maj <= 0 || min < 0)
        goto next;

#if GST_GL_HAVE_OPENGL
      if (GetIntegerv && (maj > 3 || (maj == 3 && min > 1))) {
        GLuint context_flags = 0;

        ret = GST_GL_API_NONE;
        GetIntegerv (GL_CONTEXT_PROFILE_MASK, &context_flags);
        if (context_flags & GL_CONTEXT_CORE_PROFILE_BIT)
          ret |= GST_GL_API_OPENGL3;
        if (context_flags & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
          ret |= GST_GL_API_OPENGL;
        break;
      }
#endif
      ret = GST_GL_API_OPENGL;
      break;
    }

  next:
    /* iterate through the apis */
    ret >>= 1;
  }

  if (ret == GST_GL_API_NONE)
    return GST_GL_API_NONE;

  if (major)
    *major = maj;
  if (minor)
    *minor = min;

  return ret;
}

static void
gst_gl_context_finalize (GObject * object)
{
  GstGLContext *context = GST_GL_CONTEXT (object);

  if (context->window) {
    gst_gl_window_set_resize_callback (context->window, NULL, NULL, NULL);
    gst_gl_window_set_draw_callback (context->window, NULL, NULL, NULL);

    g_mutex_lock (&context->priv->render_lock);
    if (context->priv->alive) {
      GST_INFO_OBJECT (context, "send quit gl window loop");
      gst_gl_window_quit (context->window);

      GST_INFO_OBJECT (context, "joining gl thread");
      while (context->priv->alive)
        g_cond_wait (&context->priv->destroy_cond, &context->priv->render_lock);
      GST_INFO_OBJECT (context, "gl thread joined");

      if (context->priv->gl_thread) {
        g_thread_unref (context->priv->gl_thread);
        context->priv->gl_thread = NULL;
      }
    }
    g_mutex_unlock (&context->priv->render_lock);

    gst_gl_window_set_close_callback (context->window, NULL, NULL, NULL);
    gst_object_unref (context->window);
    context->window = NULL;
  }

  if (context->priv->active_thread) {
    g_thread_unref (context->priv->active_thread);
    context->priv->active_thread = NULL;
  }

  if (context->priv->gl_thread) {
    g_thread_unref (context->priv->gl_thread);
    context->priv->gl_thread = NULL;
  }

  if (context->priv->sharegroup) {
    _context_share_group_unref (context->priv->sharegroup);
    context->priv->sharegroup = NULL;
  }

  if (context->display) {
    gst_object_unref (context->display);
    context->display = NULL;
  }

  if (context->gl_vtable) {
    g_free (context->gl_vtable);
    context->gl_vtable = NULL;
  }

  if (context->priv->requested_config)
    gst_structure_free (context->priv->requested_config);
  context->priv->requested_config = NULL;

  g_mutex_clear (&context->priv->render_lock);

  g_cond_clear (&context->priv->create_cond);
  g_cond_clear (&context->priv->destroy_cond);

  g_free (context->priv->gl_exts);
  g_weak_ref_clear (&context->priv->other_context_ref);

  GST_DEBUG_OBJECT (context, "End of finalize");
  G_OBJECT_CLASS (gst_gl_context_parent_class)->finalize (object);
}

/**
 * gst_gl_context_activate:
 * @context: a #GstGLContext
 * @activate: %TRUE to activate, %FALSE to deactivate
 *
 * (De)activate the OpenGL context represented by this @context.
 *
 * In OpenGL terms, calls eglMakeCurrent or similar with this context and the
 * currently set window.  See gst_gl_context_set_window() for details.
 *
 * Returns: Whether the activation succeeded
 *
 * Since: 1.4
 */
gboolean
gst_gl_context_activate (GstGLContext * context, gboolean activate)
{
  GstGLContextClass *context_class;
  gboolean result;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_val_if_fail (context_class->activate != NULL, FALSE);

  GST_DEBUG_OBJECT (context, "activate:%d", activate);

  GST_OBJECT_LOCK (context);
  result = context_class->activate (context, activate);

  if (result && activate) {
    GThread *old_thread = context->priv->active_thread;
    context->priv->active_thread = g_thread_ref (g_thread_self ());
    if (old_thread) {
      g_thread_unref (old_thread);
    }

    g_private_set (&current_context_key, context);
  } else {
    if (context->priv->active_thread) {
      g_thread_unref (context->priv->active_thread);
      context->priv->active_thread = NULL;
    }
    g_private_set (&current_context_key, NULL);
  }
  GST_OBJECT_UNLOCK (context);

  return result;
}

/**
 * gst_gl_context_get_thread:
 * @context: a #GstGLContext
 *
 * Returns: (transfer full) (nullable): The #GThread, @context is current in or NULL
 *
 * Since: 1.6
 */
GThread *
gst_gl_context_get_thread (GstGLContext * context)
{
  GThread *ret;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  GST_OBJECT_LOCK (context);
  ret = context->priv->active_thread;
  if (ret)
    g_thread_ref (ret);
  GST_OBJECT_UNLOCK (context);

  return ret;
}

/**
 * gst_gl_context_get_gl_api:
 * @context: a #GstGLContext
 *
 * Get the currently enabled OpenGL api.
 *
 * The currently available API may be limited by the #GstGLDisplay in use and/or
 * the #GstGLWindow chosen.
 *
 * Returns: the available OpenGL api
 *
 * Since: 1.4
 */
GstGLAPI
gst_gl_context_get_gl_api (GstGLContext * context)
{
  GstGLContextClass *context_class;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), GST_GL_API_NONE);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_val_if_fail (context_class->get_gl_api != NULL, GST_GL_API_NONE);

  return context_class->get_gl_api (context);
}

/**
 * gst_gl_context_get_proc_address:
 * @context: a #GstGLContext
 * @name: an opengl function name
 *
 * Get a function pointer to a specified opengl function, @name.  If the the
 * specific function does not exist, NULL is returned instead.
 *
 * Platform specific functions (names starting 'egl', 'glX', 'wgl', etc) can also
 * be retrieved using this method.
 *
 * Note: This function may return valid function pointers that may not be valid
 * to call in @context.  The caller is responsible for ensuring that the
 * returned function is a valid function to call in @context by either checking
 * the OpenGL API and version or for an appropriate OpenGL extension.
 *
 * Note: On success, you need to cast the returned function pointer to the
 * correct type to be able to call it correctly.  On 32-bit Windows, this will
 * include the `GSTGLAPI` identifier to use the correct calling convention.
 * e.g.
 *
 * |[<!-- language="C" -->
 * void (GSTGLAPI *PFN_glGetIntegerv) (GLenum name, GLint * ret)
 * ]|
 *
 * Returns: (nullable): a function pointer or %NULL
 *
 * Since: 1.4
 */
gpointer
gst_gl_context_get_proc_address (GstGLContext * context, const gchar * name)
{
  gpointer ret;
  GstGLContextClass *context_class;
  GstGLAPI gl_api;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_val_if_fail (context_class->get_proc_address != NULL, NULL);

  gl_api = gst_gl_context_get_gl_api (context);
  ret = context_class->get_proc_address (gl_api, name);

  return ret;
}

/**
 * gst_gl_context_default_get_proc_address:
 * @gl_api: a #GstGLAPI
 * @name: then function to get the address of
 *
 * A default implementation of the various GetProcAddress functions that looks
 * for @name in the OpenGL shared libraries or in the current process.
 *
 * See also: gst_gl_context_get_proc_address()
 *
 * Returns: (nullable): an address pointing to @name or %NULL
 *
 * Since: 1.4
 */
gpointer
gst_gl_context_default_get_proc_address (GstGLAPI gl_api, const gchar * name)
{
  gpointer ret = NULL;

  /* First try to load symbol from the selected GL API for this context */
#if GST_GL_HAVE_GLES2
  if (!ret && (gl_api & GST_GL_API_GLES2)) {
    g_once (&module_gles2_gonce, load_gles2_module, NULL);
    if (module_gles2)
      g_module_symbol (module_gles2, name, &ret);
  }
#endif

#if GST_GL_HAVE_OPENGL
  if (!ret && (gl_api & (GST_GL_API_OPENGL | GST_GL_API_OPENGL3))) {
    g_once (&module_opengl_gonce, load_opengl_module, NULL);
    if (module_opengl)
      g_module_symbol (module_opengl, name, &ret);
  }
#endif

  /* Otherwise fall back to the current module */
  g_once (&module_self_gonce, load_self_module, NULL);
  if (!ret)
    g_module_symbol (module_self, name, &ret);

  return ret;
}

/**
 * gst_gl_context_set_window:
 * @context: a #GstGLContext
 * @window: (transfer full): a #GstGLWindow
 *
 * Set's the current window on @context to @window.  The window can only be
 * changed before gst_gl_context_create() has been called and the @window is not
 * already running.
 *
 * Returns: Whether the window was successfully updated
 *
 * Since: 1.4
 */
gboolean
gst_gl_context_set_window (GstGLContext * context, GstGLWindow * window)
{
  g_return_val_if_fail (!GST_IS_GL_WRAPPED_CONTEXT (context), FALSE);

  GST_DEBUG_OBJECT (context, "window:%" GST_PTR_FORMAT, window);

  /* we can't change the window while we are running */
  if (context->priv->alive)
    return FALSE;

  if (window)
    g_weak_ref_set (&window->context_ref, context);

  if (context->window)
    gst_object_unref (context->window);

  context->window = window ? gst_object_ref (window) : NULL;

  return TRUE;
}

/**
 * gst_gl_context_get_window:
 * @context: a #GstGLContext
 *
 * Returns: (transfer full) (nullable): the currently set window
 *
 * Since: 1.4
 */
GstGLWindow *
gst_gl_context_get_window (GstGLContext * context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  if (GST_IS_GL_WRAPPED_CONTEXT (context)) {
    GST_WARNING_OBJECT (context, "context is not toplevel, returning NULL");
    return NULL;
  }

  _ensure_window (context);

  return gst_object_ref (context->window);
}

/**
 * gst_gl_context_can_share:
 * @context: a #GstGLContext
 * @other_context: another #GstGLContext
 *
 * Note: This will always fail for two wrapped #GstGLContext's
 *
 * Returns: whether @context and @other_context are able to share OpenGL
 *      resources.
 *
 * Since: 1.6
 */
gboolean
gst_gl_context_can_share (GstGLContext * context, GstGLContext * other_context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (other_context), FALSE);

  /* check if the contexts are descendants or the root nodes are the same */
  return context->priv->sharegroup != NULL
      && context->priv->sharegroup == other_context->priv->sharegroup;
}

/**
 * gst_gl_context_create:
 * @context: a #GstGLContext:
 * @other_context: (nullable): a #GstGLContext to share OpenGL objects with
 * @error: a #GError
 *
 * Creates an OpenGL context with the specified @other_context as a context
 * to share shareable OpenGL objects with.  See the OpenGL specification for
 * what is shared between OpenGL contexts.
 *
 * Since 1.20, the configuration can be overriden with the environment variable
 * `GST_GL_CONFIG` which is a stringified #GstStructure as would be returned
 * from gst_gl_context_get_config().  If `GST_GL_CONFIG` is not set, then the
 * config will be chosen from @other_context by calling
 * gst_gl_context_get_config() on @other_context.  Otherwise, a default
 * configuration is used.
 *
 * Calling gst_gl_context_request_config()) before calling
 * gst_gl_context_create() will override the config from @other_context but
 * will not override the `GST_GL_CONFIG` environment variable.
 *
 * If an error occurs, and @error is not %NULL, then @error will contain
 * details of the error and %FALSE will be returned.
 *
 * Should only be called once.
 *
 * Returns: whether the context could successfully be created
 *
 * Since: 1.4
 */
gboolean
gst_gl_context_create (GstGLContext * context,
    GstGLContext * other_context, GError ** error)
{
  gboolean alive = FALSE;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (!GST_IS_GL_WRAPPED_CONTEXT (context), FALSE);

  GST_DEBUG_OBJECT (context, " other_context:%" GST_PTR_FORMAT, other_context);

  _ensure_window (context);

  g_mutex_lock (&context->priv->render_lock);

  if (!context->priv->created) {
    g_weak_ref_set (&context->priv->other_context_ref, other_context);
    context->priv->error = error;
    if (other_context == NULL)
      context->priv->sharegroup = _context_share_group_new ();
    else
      context->priv->sharegroup =
          _context_share_group_ref (other_context->priv->sharegroup);

    context->priv->gl_thread = g_thread_new ("gstglcontext",
        (GThreadFunc) gst_gl_context_create_thread, context);

    while (!context->priv->created)
      g_cond_wait (&context->priv->create_cond, &context->priv->render_lock);

    GST_INFO_OBJECT (context, "gl thread created");
  }

  alive = context->priv->alive;

  g_mutex_unlock (&context->priv->render_lock);

  return alive;
}

static gboolean
_create_context_info (GstGLContext * context, GstGLAPI gl_api, gint * gl_major,
    gint * gl_minor, GError ** error)
{
  const GstGLFuncs *gl;
  guint maj = 0, min = 0;
  GLenum gl_err = GL_NO_ERROR;
  const gchar *opengl_version = NULL;

  gl = context->gl_vtable;

  if (!gl->GetString || !gl->GetString (GL_VERSION)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "glGetString not defined or returned invalid value");
    return FALSE;
  }

  if (!gl->GetString (GL_SHADING_LANGUAGE_VERSION)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "No GL shader support available");
    return FALSE;
  }

  GST_INFO_OBJECT (context, "GL_VERSION: %s",
      GST_STR_NULL ((const gchar *) gl->GetString (GL_VERSION)));
  GST_INFO_OBJECT (context, "GL_SHADING_LANGUAGE_VERSION: %s",
      GST_STR_NULL ((const gchar *)
          gl->GetString (GL_SHADING_LANGUAGE_VERSION)));
  GST_INFO_OBJECT (context, "GL_VENDOR: %s",
      GST_STR_NULL ((const gchar *) gl->GetString (GL_VENDOR)));
  GST_INFO_OBJECT (context, "GL_RENDERER: %s",
      GST_STR_NULL ((const gchar *) gl->GetString (GL_RENDERER)));

  gl_err = gl->GetError ();
  if (gl_err != GL_NO_ERROR) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "glGetString error: 0x%x", gl_err);
    return FALSE;
  }

  opengl_version = (const gchar *) gl->GetString (GL_VERSION);
  if (opengl_version && g_str_has_prefix (opengl_version, "OpenGL ES "))
    opengl_version = &opengl_version[10];

  if (opengl_version)
    sscanf (opengl_version, "%d.%d", &maj, &min);

  /* OpenGL > 1.2.0 */
  if (gl_api & GST_GL_API_OPENGL || gl_api & GST_GL_API_OPENGL3) {
    if ((maj < 1) || (maj < 2 && maj >= 1 && min < 2)) {
      g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_OLD_LIBS,
          "OpenGL >= 1.2.0 required, found %u.%u", maj, min);
      return FALSE;
    }
  }

  if (gl_major)
    *gl_major = maj;
  if (gl_minor)
    *gl_minor = min;

  return TRUE;
}

static GstGLAPI
_compiled_api (void)
{
  GstGLAPI ret = GST_GL_API_NONE;

#if GST_GL_HAVE_OPENGL
  ret |= GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
#endif
#if GST_GL_HAVE_GLES2
  ret |= GST_GL_API_GLES2;
#endif

  return ret;
}

static void
_unlock_create_thread (GstGLContext * context)
{
  context->priv->created = TRUE;
  GST_INFO_OBJECT (context, "gl thread running");
  g_cond_signal (&context->priv->create_cond);
  g_mutex_unlock (&context->priv->render_lock);
}

static GString *
_build_extension_string (GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;
  GString *ext_g_str = g_string_sized_new (1024);
  const gchar *ext_const_c_str = NULL;
  GLint i = 0;
  GLint n = 0;

  gl->GetIntegerv (GL_NUM_EXTENSIONS, &n);

  for (i = 0; i < n; i++) {
    ext_const_c_str = (const gchar *) gl->GetStringi (GL_EXTENSIONS, i);
    if (ext_const_c_str)
      g_string_append_printf (ext_g_str, "%s ", ext_const_c_str);
  }

  return ext_g_str;
}

typedef struct
{
  GError **error;
  gboolean ret;
} FillInfoCtx;

static void
fill_info (GstGLContext * context, gpointer data)
{
  FillInfoCtx *ctx = data;
  ctx->ret = gst_gl_context_fill_info (context, ctx->error);
}

static gpointer
gst_gl_context_create_thread (GstGLContext * context)
{
  GstGLContextClass *context_class;
  GstGLWindowClass *window_class;
  GstGLAPI compiled_api, user_api, gl_api, display_api;
  gchar *api_string;
  gchar *compiled_api_s;
  gchar *user_api_s;
  gchar *display_api_s;
  const gchar *user_choice;
  GError **error;
  GstGLContext *other_context;
  GstStructure *config;

  g_mutex_lock (&context->priv->render_lock);

  GST_DEBUG_OBJECT (context, "Creating thread");

  error = context->priv->error;
  other_context = g_weak_ref_get (&context->priv->other_context_ref);

  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  window_class = GST_GL_WINDOW_GET_CLASS (context->window);

  display_api = gst_gl_display_get_gl_api_unlocked (context->display);
  if (display_api == GST_GL_API_NONE) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_WRONG_API,
        "Cannot create context with satisfying requested apis "
        "(display has no GL api!)");
    goto failure;
  }

  if (window_class->open) {
    if (!window_class->open (context->window, error)) {
      GST_WARNING_OBJECT (context, "Failed to open window");
      g_assert (error == NULL || *error != NULL);
      goto failure;
    }
  }

  compiled_api = _compiled_api ();
  compiled_api_s = gst_gl_api_to_string (compiled_api);

  user_choice = g_getenv ("GST_GL_API");
  user_api = gst_gl_api_from_string (user_choice);
  user_api_s = gst_gl_api_to_string (user_api);

  display_api_s = gst_gl_api_to_string (display_api);

  if ((user_api & compiled_api & display_api) == GST_GL_API_NONE) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_WRONG_API,
        "Cannot create context with the user requested api (%s).  "
        "We have support for (%s), display api (%s)", user_api_s,
        compiled_api_s, display_api_s);
    g_free (user_api_s);
    g_free (compiled_api_s);
    g_free (display_api_s);
    goto failure;
  }

  {
    const gchar *config_str = g_getenv ("GST_GL_CONFIG");
    if (config_str) {
      GstStructure *config = gst_structure_from_string (config_str, NULL);
      if (!config) {
        g_set_error (error, GST_GL_CONTEXT_ERROR,
            GST_GL_CONTEXT_ERROR_WRONG_CONFIG,
            "could not construct OpenGL config from the \'GST_GL_CONFIG\' "
            "environment variable");
        g_free (compiled_api_s);
        g_free (user_api_s);
        g_free (display_api_s);
        goto failure;
      }
      if (!gst_gl_context_request_config (context, gst_structure_copy (config))) {
        GST_WARNING_OBJECT (context,
            "failed to request config %" GST_PTR_FORMAT, config);
      } else {
        GST_INFO_OBJECT (context, "requesting config from environment %"
            GST_PTR_FORMAT, config);
      }
      gst_structure_free (config);
    } else if (other_context && !context->priv->requested_config) {
      GstStructure *config = gst_gl_context_get_config (other_context);
      if (config) {
        if (!gst_gl_context_request_config (context,
                gst_structure_copy (config))) {
          GST_WARNING_OBJECT (context,
              "failed to request config %" GST_PTR_FORMAT, config);
        } else {
          GST_INFO_OBJECT (context, "requesting config from other context %"
              GST_PTR_FORMAT " %" GST_PTR_FORMAT, other_context, config);
        }
        gst_structure_free (config);
      }
    }
  }

  if (context_class->choose_format &&
      !context_class->choose_format (context, error)) {
    GST_WARNING_OBJECT (context, "Failed to choose format");
    g_assert (error == NULL || *error != NULL);
    g_free (compiled_api_s);
    g_free (user_api_s);
    g_free (display_api_s);
    goto failure;
  }

  GST_INFO_OBJECT (context,
      "Attempting to create opengl context. user chosen api(s) (%s), "
      "compiled api support (%s) display api (%s)", user_api_s, compiled_api_s,
      display_api_s);

  if (!context_class->create_context (context,
          compiled_api & user_api & display_api, other_context, error)) {
    GST_WARNING_OBJECT (context, "Failed to create context");
    g_assert (error == NULL || *error != NULL);
    g_free (compiled_api_s);
    g_free (user_api_s);
    g_free (display_api_s);
    goto failure;
  }
  GST_INFO_OBJECT (context, "created context");

  if (!gst_gl_context_activate (context, TRUE)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_RESOURCE_UNAVAILABLE,
        "Failed to activate the GL Context");
    g_free (compiled_api_s);
    g_free (user_api_s);
    g_free (display_api_s);
    goto failure;
  }

  gl_api = gst_gl_context_get_gl_api (context);
  g_assert (gl_api != GST_GL_API_NONE && gl_api != GST_GL_API_ANY);

  api_string = gst_gl_api_to_string (gl_api);
  GST_INFO_OBJECT (context, "available GL APIs: %s", api_string);

  if ((config = gst_gl_context_get_config (context))) {
    GST_DEBUG_OBJECT (context, "Chosen config %" GST_PTR_FORMAT, config);
    gst_structure_free (config);
  }

  if (((compiled_api & gl_api & display_api) & user_api) == GST_GL_API_NONE) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_WRONG_API,
        "failed to create context, context "
        "could not provide correct api. user (%s), compiled (%s), context (%s)",
        user_api_s, compiled_api_s, api_string);
    g_free (api_string);
    g_free (compiled_api_s);
    g_free (user_api_s);
    g_free (display_api_s);
    goto failure;
  }

  g_free (api_string);
  g_free (compiled_api_s);
  g_free (user_api_s);
  g_free (display_api_s);

  GST_DEBUG_OBJECT (context, "Filling info");
  {
    FillInfoCtx ctx = { 0 };
    ctx.error = error;
    gst_gl_context_thread_add (context, fill_info, &ctx);
    if (!ctx.ret) {
      g_assert (error == NULL || *error != NULL);
      goto failure;
    }
  }

  context->priv->alive = TRUE;

#if !defined(GST_DISABLE_GST_DEBUG)
  _gst_gl_debug_enable (context);
#endif

  if (other_context) {
    GST_DEBUG_OBJECT (context, "Unreffing other_context %" GST_PTR_FORMAT,
        other_context);
    gst_object_unref (other_context);
  }

  /* unlocking of the render_lock happens when the
   * context's loop is running from inside that loop */
  gst_gl_window_send_message_async (context->window,
      (GstGLWindowCB) _unlock_create_thread, context, NULL);

  gst_gl_window_run (context->window);

  GST_INFO_OBJECT (context, "loop exited");

  g_mutex_lock (&context->priv->render_lock);
  context->priv->alive = FALSE;

  gst_gl_context_activate (context, FALSE);

  context_class->destroy_context (context);

  /* User supplied callback */
  if (context->window->close)
    context->window->close (context->window->close_data);

  /* window specific shutdown */
  if (window_class->close) {
    window_class->close (context->window);
  }

  context->priv->created = FALSE;
  g_cond_signal (&context->priv->destroy_cond);
  g_mutex_unlock (&context->priv->render_lock);

  return NULL;

failure:
  {
    if (other_context)
      gst_object_unref (other_context);

    /* A context that fails to be created is considered created but not alive
     * and will never be able to be alive as creation can't happen */
    context->priv->created = TRUE;
    g_cond_signal (&context->priv->create_cond);
    g_mutex_unlock (&context->priv->render_lock);
    return NULL;
  }
}

/**
 * gst_gl_context_destroy:
 * @context: a #GstGLContext:
 *
 * Destroys an OpenGL context.
 *
 * Should only be called after gst_gl_context_create() has been successfully
 * called for this context.
 *
 * Since: 1.6
 */
void
gst_gl_context_destroy (GstGLContext * context)
{
  GstGLContextClass *context_class;

  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_if_fail (context_class->destroy_context != NULL);

  context_class->destroy_context (context);
}

/**
 * gst_gl_context_fill_info:
 * @context: a #GstGLContext:
 * @error: (allow-none): a #GError to fill on failure
 *
 * Fills @context's info (version, extensions, vtable, etc) from the GL
 * context in the current thread.  Typically used with wrapped contexts to
 * allow wrapped contexts to be used as regular #GstGLContext's.
 *
 * Since: 1.6
 */
gboolean
gst_gl_context_fill_info (GstGLContext * context, GError ** error)
{
  GstGLFuncs *gl;
  GString *ext_g_str = NULL;
  const gchar *ext_const_c_str = NULL;
  GstGLAPI gl_api;
  gboolean ret;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (context->priv->active_thread == g_thread_self (),
      FALSE);

  gl = context->gl_vtable;
  gl_api = gst_gl_context_get_gl_api (context);

  gl->GetError = gst_gl_context_get_proc_address (context, "glGetError");
  gl->GetString = gst_gl_context_get_proc_address (context, "glGetString");
  gl->GetStringi = gst_gl_context_get_proc_address (context, "glGetStringi");
  gl->GetIntegerv = gst_gl_context_get_proc_address (context, "glGetIntegerv");

  if (!gl->GetError || !gl->GetString) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "could not GetProcAddress core opengl functions");
    goto failure;
  }

  /* gl api specific code */
  ret = _create_context_info (context, gl_api, &context->priv->gl_major,
      &context->priv->gl_minor, error);

  if (!ret) {
    g_assert (error == NULL || *error != NULL);
    goto failure;
  }

  /* GL core contexts and GLES3 */
  if (gl->GetIntegerv && gl->GetStringi && context->priv->gl_major >= 3)
    ext_g_str = _build_extension_string (context);

  if (ext_g_str && ext_g_str->len) {
    GST_DEBUG_OBJECT (context, "GL_EXTENSIONS: %s", ext_g_str->str);
    _gst_gl_feature_check_ext_functions (context, context->priv->gl_major,
        context->priv->gl_minor, ext_g_str->str);

    context->priv->gl_exts = g_string_free (ext_g_str, FALSE);
  } else {
    ext_const_c_str = (const gchar *) gl->GetString (GL_EXTENSIONS);
    if (!ext_const_c_str)
      ext_const_c_str = "";

    GST_DEBUG_OBJECT (context, "GL_EXTENSIONS: %s", ext_const_c_str);
    _gst_gl_feature_check_ext_functions (context, context->priv->gl_major,
        context->priv->gl_minor, ext_const_c_str);

    context->priv->gl_exts = g_strdup (ext_const_c_str);
  }

  if (gl_api & GST_GL_API_OPENGL3
      && !gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 4, 1)
      && !gst_gl_check_extension ("GL_ARB_ES2_compatibility",
          context->priv->gl_exts)) {
    g_set_error (error, GST_GL_CONTEXT_ERROR, GST_GL_CONTEXT_ERROR_FAILED,
        "An opengl3 context created but the required ES2 compatibility was not found");
    goto failure;
  }

  gst_gl_context_apply_quirks (context);

  if (GST_IS_GL_WRAPPED_CONTEXT (context)) {
    /* XXX: vfunc? */
#if GST_GL_HAVE_PLATFORM_GLX
    if (gst_gl_context_get_gl_platform (context) == GST_GL_PLATFORM_GLX
        && !gst_gl_context_glx_fill_info (context, error))
      goto failure;
#endif
#if GST_GL_HAVE_PLATFORM_EGL
    if (gst_gl_context_get_gl_platform (context) == GST_GL_PLATFORM_EGL
        && !gst_gl_context_egl_fill_info (context, error))
      goto failure;
#endif
  }

  return TRUE;

failure:
  return FALSE;
}

/**
 * gst_gl_context_get_gl_context:
 * @context: a #GstGLContext:
 *
 * Gets the backing OpenGL context used by @context.
 *
 * Returns: The platform specific backing OpenGL context
 *
 * Since: 1.4
 */
guintptr
gst_gl_context_get_gl_context (GstGLContext * context)
{
  GstGLContextClass *context_class;
  guintptr result;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), 0);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_val_if_fail (context_class->get_gl_context != NULL, 0);

  result = context_class->get_gl_context (context);

  return result;
}

/**
 * gst_gl_context_get_gl_platform:
 * @context: a #GstGLContext:
 *
 * Gets the OpenGL platform that used by @context.
 *
 * Returns: The platform specific backing OpenGL context
 *
 * Since: 1.4
 */
GstGLPlatform
gst_gl_context_get_gl_platform (GstGLContext * context)
{
  GstGLContextClass *context_class;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), 0);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_val_if_fail (context_class->get_gl_platform != NULL, 0);

  return context_class->get_gl_platform (context);
}

/**
 * gst_gl_context_get_display:
 * @context: a #GstGLContext:
 *
 * Returns: (transfer full): the #GstGLDisplay associated with this @context
 *
 * Since: 1.4
 */
GstGLDisplay *
gst_gl_context_get_display (GstGLContext * context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  return gst_object_ref (context->display);
}

typedef struct
{
  GstGLContext *context;
  GstGLContextThreadFunc func;
  gpointer data;
} RunGenericData;

static void
_gst_gl_context_thread_run_generic (RunGenericData * data)
{
  GST_TRACE_OBJECT (data->context, "running function:%p data:%p", data->func,
      data->data);

  data->func (data->context, data->data);
}

/**
 * gst_gl_context_thread_add:
 * @context: a #GstGLContext
 * @func: (scope call): a #GstGLContextThreadFunc
 * @data: (closure): user data to call @func with
 *
 * Execute @func in the OpenGL thread of @context with @data
 *
 * MT-safe
 *
 * Since: 1.4
 */
void
gst_gl_context_thread_add (GstGLContext * context,
    GstGLContextThreadFunc func, gpointer data)
{
  GstGLWindow *window;
  RunGenericData rdata;

  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  g_return_if_fail (func != NULL);

  if (GST_IS_GL_WRAPPED_CONTEXT (context))
    g_return_if_fail (context->priv->active_thread == g_thread_self ());

  if (context->priv->active_thread == g_thread_self ()) {
    func (context, data);
    return;
  }

  rdata.context = context;
  rdata.data = data;
  rdata.func = func;

  window = gst_gl_context_get_window (context);

  gst_gl_window_send_message (window,
      GST_GL_WINDOW_CB (_gst_gl_context_thread_run_generic), &rdata);

  gst_object_unref (window);
}

/**
 * gst_gl_context_get_gl_version:
 * @context: a #GstGLContext
 * @maj: (out): resulting major version
 * @min: (out): resulting minor version
 *
 * Returns the OpenGL version implemented by @context.  See
 * gst_gl_context_get_gl_api() for retrieving the OpenGL api implemented by
 * @context.
 *
 * Since: 1.4
 */
void
gst_gl_context_get_gl_version (GstGLContext * context, gint * maj, gint * min)
{
  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  g_return_if_fail (!(maj == NULL && min == NULL));

  if (maj)
    *maj = context->priv->gl_major;

  if (min)
    *min = context->priv->gl_minor;
}

/**
 * gst_gl_context_check_gl_version:
 * @context: a #GstGLContext
 * @api: api type required
 * @maj: major version required
 * @min: minor version required
 *
 * Returns: whether OpenGL context implements the required api and specified
 * version.
 *
 * Since: 1.4
 */
gboolean
gst_gl_context_check_gl_version (GstGLContext * context, GstGLAPI api,
    gint maj, gint min)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  if (maj > context->priv->gl_major)
    return FALSE;

  if ((gst_gl_context_get_gl_api (context) & api) == GST_GL_API_NONE)
    return FALSE;

  if (maj < context->priv->gl_major)
    return TRUE;

  if (min > context->priv->gl_minor)
    return FALSE;

  return TRUE;
}

/**
 * gst_gl_context_check_feature:
 * @context: a #GstGLContext
 * @feature: a platform specific feature
 *
 * Check for an OpenGL @feature being supported.
 *
 * Note: Most features require that the context be created before it is
 * possible to determine their existence and so will fail if that is not the
 * case.
 *
 * Returns: Whether @feature is supported by @context
 *
 * Since: 1.4
 */
gboolean
gst_gl_context_check_feature (GstGLContext * context, const gchar * feature)
{
  GstGLContextClass *context_class;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (feature != NULL, FALSE);

  context_class = GST_GL_CONTEXT_GET_CLASS (context);

  if (g_strstr_len (feature, 3, "GL_"))
    return gst_gl_check_extension (feature, context->priv->gl_exts);

  if (!context_class->check_feature)
    return FALSE;

  return context_class->check_feature (context, feature);
}

/**
 * gst_gl_context_get_current:
 *
 * See also gst_gl_context_activate().
 *
 * Returns: (transfer none) (nullable): the #GstGLContext active in the current thread or %NULL
 *
 * Since: 1.6
 */
GstGLContext *
gst_gl_context_get_current (void)
{
  return g_private_get (&current_context_key);
}

/**
 * gst_gl_context_is_shared:
 * @context: a #GstGLContext
 *
 * Returns: Whether the #GstGLContext has been shared with another #GstGLContext
 *
 * Since: 1.8
 */
gboolean
gst_gl_context_is_shared (GstGLContext * context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  if (!context->priv->sharegroup)
    return FALSE;

  if (GST_IS_GL_WRAPPED_CONTEXT (context))
    g_return_val_if_fail (context->priv->active_thread, FALSE);
  else
    g_return_val_if_fail (context->priv->alive, FALSE);

  return _context_share_group_is_shared (context->priv->sharegroup);
}

/**
 * gst_gl_context_set_shared_with:
 * @context: a wrapped #GstGLContext
 * @share: another #GstGLContext
 *
 * Will internally set @context as shared with @share
 *
 * Since: 1.8
 */
void
gst_gl_context_set_shared_with (GstGLContext * context, GstGLContext * share)
{
  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  g_return_if_fail (GST_IS_GL_CONTEXT (share));
  g_return_if_fail (!gst_gl_context_is_shared (context));
  /* XXX: may be a little too strict */
  g_return_if_fail (GST_IS_GL_WRAPPED_CONTEXT (context));

  if (context->priv->sharegroup)
    _context_share_group_unref (context->priv->sharegroup);
  context->priv->sharegroup =
      _context_share_group_ref (share->priv->sharegroup);
}

static void
gst_gl_context_default_get_gl_platform_version (GstGLContext * context,
    gint * major, gint * minor)
{
  if (major)
    *major = 0;
  if (minor)
    *minor = 0;
}

/**
 * gst_gl_context_get_gl_platform_version:
 * @context: a #GstGLContext
 * @major: (out): return for the major version
 * @minor: (out): return for the minor version
 *
 * Get the version of the OpenGL platform (GLX, EGL, etc) used.  Only valid
 * after a call to gst_gl_context_create().
 */
void
gst_gl_context_get_gl_platform_version (GstGLContext * context, gint * major,
    gint * minor)
{
  GstGLContextClass *context_class;

  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  g_return_if_fail (major != NULL);
  g_return_if_fail (minor != NULL);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_if_fail (context_class->get_gl_platform_version != NULL);

  context_class->get_gl_platform_version (context, major, minor);
}

/**
 * gst_gl_context_swap_buffers:
 * @context: a #GstGLContext
 *
 * Swap the front and back buffers on the window attached to @context.
 * This will display the frame on the next refresh cycle.
 */
void
gst_gl_context_swap_buffers (GstGLContext * context)
{
  GstGLContextClass *context_class;

  g_return_if_fail (GST_IS_GL_CONTEXT (context));
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  g_return_if_fail (context_class->swap_buffers != NULL);

  context_class->swap_buffers (context);
}

/**
 * gst_gl_context_get_config:
 * @context: the #GstGLContext
 *
 * Retrieve the OpenGL configuration for this context.  The context must
 * have been successfully created for this function to return a valid value.
 *
 * Not all implementations currently support retrieving the config and will
 * return %NULL when not supported.
 *
 * Returns: (transfer full) (nullable): the configuration chosen for this OpenGL context.
 *
 * Since: 1.20
 */
GstStructure *
gst_gl_context_get_config (GstGLContext * context)
{
  GstGLContextClass *context_class;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  if (!context_class->get_config) {
    GST_FIXME_OBJECT (context, "does not support retrieving a config");
    return NULL;
  }

  return context_class->get_config (context);
}

/**
 * gst_gl_context_request_config:
 * @context: the #GstGLContext
 * @gl_config: (nullable) (transfer full): a configuration structure for
 *             configuring the OpenGL context
 *
 * Set the OpenGL configuration for this context.  The context must not
 * have been created for this function to succeed.  Setting a %NULL
 * @config has the affect of removing any specific configuration request.
 *
 * Not all implementations currently support retrieving the config and this
 * function will return FALSE when not supported.
 *
 * Note that calling this function may cause a subsequent
 * gst_gl_context_create() to fail if @config could not be matched with
 * the platform-specific configuration.
 *
 * Note that the actual config used may be differ from the requested values.
 *
 * Returns: whether @gl_config could be successfully set on @context
 *
 * Since: 1.20
 */
gboolean
gst_gl_context_request_config (GstGLContext * context, GstStructure * gl_config)
{
  GstGLContextClass *context_class;
  gboolean ret;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (context->priv->created == FALSE, FALSE);
  context_class = GST_GL_CONTEXT_GET_CLASS (context);
  if (!context_class->request_config) {
    gst_structure_free (gl_config);
    GST_FIXME_OBJECT (context, "does not support requesting a config");
    return FALSE;
  }

  ret = context_class->request_config (context, gst_structure_copy (gl_config));
  if (ret) {
    if (context->priv->requested_config)
      gst_structure_free (context->priv->requested_config);
    context->priv->requested_config = gl_config;
  } else {
    gst_structure_free (gl_config);
  }

  return ret;
}

static GstGLAPI
gst_gl_wrapped_context_get_gl_api (GstGLContext * context)
{
  GstGLWrappedContext *context_wrap = GST_GL_WRAPPED_CONTEXT (context);

  return context_wrap->available_apis;
}

static guintptr
gst_gl_wrapped_context_get_gl_context (GstGLContext * context)
{
  GstGLWrappedContext *context_wrap = GST_GL_WRAPPED_CONTEXT (context);

  return context_wrap->handle;
}

static GstGLPlatform
gst_gl_wrapped_context_get_gl_platform (GstGLContext * context)
{
  GstGLWrappedContext *context_wrap = GST_GL_WRAPPED_CONTEXT (context);

  return context_wrap->platform;
}

static gboolean
gst_gl_wrapped_context_activate (GstGLContext * context, gboolean activate)
{
  if (activate) {
    GThread *old_thread = context->priv->gl_thread;
    context->priv->gl_thread = g_thread_ref (g_thread_self ());
    if (old_thread) {
      g_thread_unref (old_thread);
    }
  } else {
    if (context->priv->gl_thread) {
      g_thread_unref (context->priv->gl_thread);
      context->priv->gl_thread = NULL;
    }
  }

  return TRUE;
}

static gpointer
_structure_copy_if_set (gpointer data, gpointer user_data)
{
  GstStructure *ret = NULL;

  if (data)
    ret = gst_structure_copy (data);
  return ret;
}

static GstStructure *
gst_gl_wrapped_context_get_config (GstGLContext * context)
{
  GstStructure *ret;

  ret = g_object_dup_data (G_OBJECT (context),
      GST_GL_CONTEXT_WRAPPED_GL_CONFIG_NAME,
      (GDuplicateFunc) _structure_copy_if_set, NULL);
  if (ret) {
    GST_DEBUG_OBJECT (context, "wrapped context found config %" GST_PTR_FORMAT,
        ret);
    return ret;
  } else {
    GST_FIXME_OBJECT (context, "wrapped context could not retrieve config. "
        "The application may be missing a call to gst_gl_context_fill_info() "
        "or the specific platform implemention is not implemented for "
        "retrieving the config from a wrapped OpenGL context.");
    return NULL;
  }
}

static void
gst_gl_wrapped_context_class_init (GstGLWrappedContextClass * klass)
{
  GstGLContextClass *context_class = (GstGLContextClass *) klass;

  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_wrapped_context_get_gl_context);
  context_class->get_gl_api =
      GST_DEBUG_FUNCPTR (gst_gl_wrapped_context_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_wrapped_context_get_gl_platform);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_wrapped_context_activate);
  context_class->get_config =
      GST_DEBUG_FUNCPTR (gst_gl_wrapped_context_get_config);
}

static void
gst_gl_wrapped_context_init (GstGLWrappedContext * context)
{
}

G_GNUC_INTERNAL gboolean
_gst_gl_context_debug_is_enabled (GstGLContext * context)
{
#if !defined(GST_DISABLE_GST_DEBUG)
  GstDebugLevel level;

  level = gst_debug_category_get_threshold (gst_gl_debug);

  if (level < GST_LEVEL_WARNING) {
    GST_CAT_INFO_OBJECT (gst_gl_context_debug, context, "Disabling GL context "
        "debugging (gldebug category debug level < warning)");
    return FALSE;
  }
  return TRUE;
#else
  return FALSE;
#endif
}
