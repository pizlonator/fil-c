/*
 * GStreamer
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

/* The entirety of OpenGL is deprecated starting from ios 12.0 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <OpenGLES/ES2/gl.h>

#include "gstglcontext_eagl.h"
#include "../gstglcontext_private.h"
#include "gstglios_utils.h"

#define GST_GL_CONTEXT_EAGL_CONTEXT(obj) \
    ((__bridge EAGLContext *)(obj->priv->eagl_context))
#define GST_GL_CONTEXT_EAGL_LAYER(obj) \
    ((__bridge CAEAGLLayer *)(obj->priv->eagl_layer))

#define GST_CAT_DEFAULT gst_gl_context_debug

static gboolean gst_gl_context_eagl_create_context (GstGLContext * context,
    GstGLAPI gl_api, GstGLContext * other_context, GError ** error);
static void gst_gl_context_eagl_destroy_context (GstGLContext * context);
static gboolean gst_gl_context_eagl_choose_format (GstGLContext * context,
    GError ** error);
static guintptr gst_gl_context_eagl_get_gl_context (GstGLContext * window);
static gboolean gst_gl_context_eagl_activate (GstGLContext * context,
    gboolean activate);
static void gst_gl_context_eagl_swap_buffers (GstGLContext * context);
static GstGLAPI gst_gl_context_eagl_get_gl_api (GstGLContext * context);
static GstGLPlatform gst_gl_context_eagl_get_gl_platform (GstGLContext *
    context);
GstStructure *gst_gl_context_eagl_get_config (GstGLContext * context);

struct _GstGLContextEaglPrivate
{
  gpointer eagl_context;

  /* Used if we render to a window */
  gpointer eagl_layer;
  GLuint framebuffer;
  GLuint color_renderbuffer;
  GLuint depth_renderbuffer;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstGLContextEagl, gst_gl_context_eagl,
    GST_TYPE_GL_CONTEXT);

static void
gst_gl_context_eagl_class_init (GstGLContextEaglClass * klass)
{
  GstGLContextClass *context_class;

  context_class = (GstGLContextClass *) klass;

  context_class->destroy_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_destroy_context);
  context_class->create_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_create_context);
  context_class->choose_format =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_choose_format);
  context_class->get_gl_context =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_get_gl_context);
  context_class->activate = GST_DEBUG_FUNCPTR (gst_gl_context_eagl_activate);
  context_class->swap_buffers =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_swap_buffers);
  context_class->get_gl_api =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_get_gl_api);
  context_class->get_gl_platform =
      GST_DEBUG_FUNCPTR (gst_gl_context_eagl_get_gl_platform);
  context_class->get_config = GST_DEBUG_FUNCPTR (gst_gl_context_eagl_get_config);
}

static void
gst_gl_context_eagl_init (GstGLContextEagl * context)
{
  context->priv = gst_gl_context_eagl_get_instance_private (context);
}

/* Must be called in the gl thread */
GstGLContextEagl *
gst_gl_context_eagl_new (GstGLDisplay * display)
{
  GstGLContextEagl *context;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_EAGL)
      == GST_GL_DISPLAY_TYPE_NONE) {
    GST_INFO ("Wrong display type %u for this context type %u", display->type,
        GST_GL_DISPLAY_TYPE_EAGL);
    return NULL;
  }

  context = g_object_new (GST_TYPE_GL_CONTEXT_EAGL, NULL);
  gst_object_ref_sink (context);

  return context;
}

enum EAGLFormat
{
  FORMAT_RGBA8 = 1,
  FORMAT_RGB565,
};

static GstStructure *
layer_config_to_structure (GstGLContextEagl *eagl, CAEAGLLayer * layer)
{
  GstStructure *ret;
  NSDictionary *drawableProps = [layer drawableProperties];
  NSString *color_format;
  enum EAGLFormat eagl_format = FORMAT_RGBA8;

  ret = gst_structure_new (GST_GL_CONFIG_STRUCTURE_NAME,
      GST_GL_CONFIG_STRUCTURE_SET_ARGS(PLATFORM, GstGLPlatform, GST_GL_PLATFORM_EAGL),
      NULL);

  color_format = [drawableProps objectForKey:kEAGLDrawablePropertyColorFormat];
  if (!color_format)
    color_format = [layer contentsFormat];

  if (!color_format) {
    GST_WARNING_OBJECT (eagl, "Could not retrieve color format from layer %p", layer);
    goto failure;
  }

  if (color_format == kEAGLColorFormatRGBA8 || color_format == kCAContentsFormatRGBA8Uint) {
    eagl_format = FORMAT_RGBA8;
  } else if (color_format == kEAGLColorFormatRGB565) {
    eagl_format = FORMAT_RGB565;
  } else {
    GST_WARNING_OBJECT (eagl, "unknown drawable format: %s", [color_format UTF8String]);
    goto failure;
  }

  /* XXX: defaults chosen by _update_layer() */
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(DEPTH_SIZE, int, 16), NULL);
  gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(STENCIL_SIZE, int, 0), NULL);

  switch (eagl_format) {
    case FORMAT_RGBA8:
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(RED_SIZE, int, 8), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(GREEN_SIZE, int, 8), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(BLUE_SIZE, int, 8), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(ALPHA_SIZE, int, 8), NULL);
      break;
    case FORMAT_RGB565:
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(RED_SIZE, int, 5), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(GREEN_SIZE, int, 6), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(BLUE_SIZE, int, 5), NULL);
      gst_structure_set (ret, GST_GL_CONFIG_STRUCTURE_SET_ARGS(ALPHA_SIZE, int, 0), NULL);
      break;
    default:
      GST_WARNING_OBJECT (eagl, "Unhandled format!");
      goto failure;
  }

  return ret;

failure:
  gst_structure_free (ret);
  return NULL;
}

void
gst_gl_context_eagl_resize (GstGLContextEagl * eagl_context)
{
  int width, height;

  glBindRenderbuffer (GL_RENDERBUFFER, eagl_context->priv->color_renderbuffer);
  [GST_GL_CONTEXT_EAGL_CONTEXT(eagl_context)
      renderbufferStorage:GL_RENDERBUFFER
      fromDrawable:GST_GL_CONTEXT_EAGL_LAYER(eagl_context)];
  glGetRenderbufferParameteriv (GL_RENDERBUFFER,
      GL_RENDERBUFFER_WIDTH, &width);
  glGetRenderbufferParameteriv (GL_RENDERBUFFER,
      GL_RENDERBUFFER_HEIGHT, &height);
  glBindRenderbuffer (GL_RENDERBUFFER, eagl_context->priv->depth_renderbuffer);
  glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width,
      height);
}

static void
gst_gl_context_eagl_release_layer (GstGLContext * context)
{
  GstGLContextEagl *context_eagl;

  context_eagl = GST_GL_CONTEXT_EAGL (context);

  if (context_eagl->priv->eagl_layer) {
    gst_gl_context_eagl_activate (context, TRUE);

    [GST_GL_CONTEXT_EAGL_CONTEXT(context_eagl)
        renderbufferStorage:GL_RENDERBUFFER
        fromDrawable:nil];

    glDeleteFramebuffers (1, &context_eagl->priv->framebuffer);
    context_eagl->priv->framebuffer = 0;

    glDeleteRenderbuffers (1, &context_eagl->priv->depth_renderbuffer);
    context_eagl->priv->depth_renderbuffer = 0;
    glDeleteRenderbuffers (1, &context_eagl->priv->color_renderbuffer);
    context_eagl->priv->color_renderbuffer = 0;

    CFRelease (context_eagl->priv->eagl_layer);
    context_eagl->priv->eagl_layer = NULL;
    gst_gl_context_eagl_activate (context, FALSE);
  }
}

void
gst_gl_context_eagl_update_layer (GstGLContext * context, gpointer layer)
{
  GLuint framebuffer;
  GLuint color_renderbuffer;
  GLuint depth_renderbuffer;
  GLint width;
  GLint height;
  CAEAGLLayer *eagl_layer;
  GLenum status;
  GstGLContextEagl *context_eagl = GST_GL_CONTEXT_EAGL (context);
  GstGLContextEaglPrivate *priv = context_eagl->priv;
  GstGLWindow *window = gst_gl_context_get_window (context);
  GstStructure *fmt;

  if (!layer || !gst_gl_window_get_window_handle (window)) {
    GST_INFO_OBJECT (context, "window handle not set yet, not updating layer");
    goto out;
  }

  if (priv->eagl_layer)
    gst_gl_context_eagl_release_layer (context);

  eagl_layer = (__bridge CAEAGLLayer *) layer;
  [EAGLContext setCurrentContext:GST_GL_CONTEXT_EAGL_CONTEXT(context_eagl)];

  /* Allocate framebuffer */
  glGenFramebuffers (1, &framebuffer);
  glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
  /* Allocate color render buffer */
  glGenRenderbuffers (1, &color_renderbuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, color_renderbuffer);
  [GST_GL_CONTEXT_EAGL_CONTEXT(context_eagl) renderbufferStorage: GL_RENDERBUFFER fromDrawable:eagl_layer];
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_RENDERBUFFER, color_renderbuffer);
  /* Get renderbuffer width/height */
  glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
      &width);
  glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT,
      &height);
  /* allocate depth render buffer */
  glGenRenderbuffers (1, &depth_renderbuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, depth_renderbuffer);
  glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width,
      height);
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
      GL_RENDERBUFFER, depth_renderbuffer);

  /* check creation status */
  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    GST_ERROR ("Failed to make complete framebuffer object %x", status);
    goto out;
  }
  glBindRenderbuffer (GL_RENDERBUFFER, 0);
  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  priv->eagl_layer = (__bridge_retained gpointer) eagl_layer;
  priv->framebuffer = framebuffer;
  priv->color_renderbuffer = color_renderbuffer;
  priv->depth_renderbuffer = depth_renderbuffer;

  fmt = layer_config_to_structure (context_eagl, eagl_layer);
  if (fmt) {
    GST_DEBUG_OBJECT (context_eagl, "chosen config %" GST_PTR_FORMAT, fmt);
    gst_structure_free (fmt);
  }

out:
  if (window)
    gst_object_unref (window);
  if (layer)
    CFRelease (layer);
}

static gboolean
gst_gl_context_eagl_create_context (GstGLContext * context, GstGLAPI gl_api,
    GstGLContext * other_context, GError ** error)
{
  GstGLContextEagl *context_eagl = GST_GL_CONTEXT_EAGL (context);
  GstGLContextEaglPrivate *priv = context_eagl->priv;
  GstGLWindow *window;
  GstGLWindowEagl *window_eagl;
  gpointer layer;
  EAGLSharegroup *share_group;

  if (other_context) {
    EAGLContext *external_gl_context = (__bridge EAGLContext *)(void *)
        gst_gl_context_get_gl_context (other_context);
    share_group = [external_gl_context sharegroup];
  } else {
    share_group = nil;
  }

  priv->eagl_context = (__bridge_retained gpointer)[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:share_group];
  if (!priv->eagl_context) {
    priv->eagl_context = (__bridge_retained gpointer)[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:share_group];
  }
  if (!priv->eagl_context) {
    g_set_error_literal (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "Failed to create OpenGL ES context");
    return FALSE;
  }

  priv->eagl_layer = NULL;
  priv->framebuffer = 0;
  priv->color_renderbuffer = 0;
  priv->depth_renderbuffer = 0;

  GST_INFO_OBJECT (context, "context created, updating layer");
  window = gst_gl_context_get_window (context);
  if (!window) {
    g_set_error_literal (error, GST_GL_CONTEXT_ERROR,
        GST_GL_CONTEXT_ERROR_CREATE_CONTEXT,
        "No window to render into");
    return FALSE;
  }

  window_eagl = GST_GL_WINDOW_EAGL (window);
  layer = gst_gl_window_eagl_get_layer (window_eagl);
  gst_gl_context_eagl_update_layer (context, layer);

  gst_object_unref (window);

  return TRUE;
}

static void
gst_gl_context_eagl_destroy_context (GstGLContext * context)
{
  GstGLContextEagl *context_eagl;

  context_eagl = GST_GL_CONTEXT_EAGL (context);

  if (!context_eagl->priv->eagl_context)
    return;

  gst_gl_context_eagl_release_layer (context);

  CFRelease(context_eagl->priv->eagl_context);
  context_eagl->priv->eagl_context = NULL;
}

static gboolean
gst_gl_context_eagl_choose_format (GstGLContext * context, GError ** error)
{
  return TRUE;
}

static guintptr
gst_gl_context_eagl_get_gl_context (GstGLContext * context)
{
  return (guintptr) GST_GL_CONTEXT_EAGL (context)->priv->eagl_context;
}

void
gst_gl_context_eagl_prepare_draw (GstGLContextEagl * context)
{
  if (!context->priv->eagl_layer)
    return;

  glBindFramebuffer (GL_FRAMEBUFFER, context->priv->framebuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, context->priv->color_renderbuffer);
}

void
gst_gl_context_eagl_finish_draw (GstGLContextEagl * context)
{
  if (!context->priv->eagl_layer)
    return;

  glBindRenderbuffer (GL_RENDERBUFFER, 0);
  glBindFramebuffer (GL_FRAMEBUFFER, 0);
}

static void
gst_gl_context_eagl_swap_buffers (GstGLContext * context)
{
  GstGLContextEagl *context_eagl;

  context_eagl = GST_GL_CONTEXT_EAGL (context);

  if (!context_eagl->priv->eagl_layer)
    return;

  [GST_GL_CONTEXT_EAGL_CONTEXT(context_eagl) presentRenderbuffer:GL_RENDERBUFFER];
}

static gboolean
gst_gl_context_eagl_activate (GstGLContext * context, gboolean activate)
{
  GstGLContextEagl *context_eagl;

  context_eagl = GST_GL_CONTEXT_EAGL (context);

  if (activate) {
    EAGLContext *cur_ctx =[EAGLContext currentContext];

    if (cur_ctx == context_eagl->priv->eagl_context) {
      GST_DEBUG ("Already attached the context to thread %p", g_thread_self ());
      return TRUE;
    }

    GST_DEBUG ("Attaching context to thread %p", g_thread_self ());
    if ([EAGLContext setCurrentContext:GST_GL_CONTEXT_EAGL_CONTEXT(context_eagl)] == NO) {
      GST_ERROR ("Couldn't make context current");
      return FALSE;
    }
  } else {
    GST_DEBUG ("Detaching context from thread %p", g_thread_self ());
    if ([EAGLContext setCurrentContext:nil] == NO) {
      GST_ERROR ("Couldn't unbind context");
      return FALSE;
    }
  }

  return TRUE;
}

static GstGLAPI
gst_gl_context_eagl_get_gl_api (GstGLContext * context)
{
  return GST_GL_API_GLES2;
}

static GstGLPlatform
gst_gl_context_eagl_get_gl_platform (GstGLContext * context)
{
  return GST_GL_PLATFORM_EAGL;
}

guintptr
gst_gl_context_eagl_get_current_context (void)
{
  return (guintptr) [EAGLContext currentContext];
}

GstStructure *
gst_gl_context_eagl_get_config (GstGLContext * context)
{
  GstGLContextEagl *eagl = GST_GL_CONTEXT_EAGL (context);

  if (!eagl->priv->eagl_layer)
    return NULL;

  return layer_config_to_structure (eagl, (__bridge CAEAGLLayer *) eagl->priv->eagl_layer);
}

G_GNUC_END_IGNORE_DEPRECATIONS
