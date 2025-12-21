/*
 * GStreamer
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it un der the terms of the GNU Library General Public
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

#include "gstglwindow_eagl.h"
#include "gstglcontext_eagl.h"
#include "gstglios_utils.h"

#define GST_CAT_DEFAULT gst_gl_window_eagl_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_GL_WINDOW_EAGL_LAYER(obj) \
    ((__bridge CAEAGLLayer *)(obj->priv->layer))
#define GST_GL_WINDOW_EAGL_QUEUE(obj) \
    ((__bridge dispatch_queue_t)(obj->priv->gl_queue))

static void gst_gl_window_eagl_finalize (GObject * object);

static guintptr gst_gl_window_eagl_get_display (GstGLWindow * window);
static guintptr gst_gl_window_eagl_get_window_handle (GstGLWindow * window);
static void gst_gl_window_eagl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_eagl_set_preferred_size (GstGLWindow * window,
    gint width, gint height);
static void gst_gl_window_eagl_draw (GstGLWindow * window);
static void gst_gl_window_eagl_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy);
static void gst_gl_window_eagl_quit (GstGLWindow *window);

struct _GstGLWindowEaglPrivate
{
  gboolean pending_set_window_handle;
  gpointer external_view;
  gpointer internal_view;
  gpointer layer;
  CGFloat window_width, window_height;
  gint preferred_width, preferred_height;
  gpointer gl_queue;
  GMutex draw_lock;
  GCond cond;

  gboolean shutting_down;
  GstGLContext *last_context;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_GET (GST_CAT_DEFAULT, "glwindow");
#define gst_gl_window_eagl_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLWindowEagl, gst_gl_window_eagl,
    GST_TYPE_GL_WINDOW, G_ADD_PRIVATE (GstGLWindowEagl) DEBUG_INIT);

static void
gst_gl_window_eagl_class_init (GstGLWindowEaglClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  gobject_class->finalize = gst_gl_window_eagl_finalize;

  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_eagl_get_display);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_eagl_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_eagl_set_window_handle);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_eagl_draw);
  window_class->set_preferred_size =
      GST_DEBUG_FUNCPTR (gst_gl_window_eagl_set_preferred_size);
  window_class->send_message_async =
      GST_DEBUG_FUNCPTR (gst_gl_window_eagl_send_message_async);
  window_class->quit = GST_DEBUG_FUNCPTR (gst_gl_window_eagl_quit);
}

static void
gst_gl_window_eagl_init (GstGLWindowEagl * window)
{
  window->priv = gst_gl_window_eagl_get_instance_private (window);
  window->priv->gl_queue =
      (__bridge_retained gpointer)dispatch_queue_create ("org.freedesktop.gstreamer.glwindow", NULL);
  g_mutex_init (&window->priv->draw_lock);
  g_cond_init (&window->priv->cond);
}

static void
gst_gl_window_eagl_finalize (GObject * object)
{
  GstGLWindowEagl *window = GST_GL_WINDOW_EAGL (object);

  if (window->priv->layer)
    CFRelease (window->priv->layer);
  window->priv->layer = NULL;
  CFRelease(window->priv->gl_queue);
  g_mutex_clear (&window->priv->draw_lock);
  g_cond_clear (&window->priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Must be called in the gl thread */
GstGLWindowEagl *
gst_gl_window_eagl_new (GstGLDisplay * display)
{
  GstGLWindowEagl *window;

  /* there isn't an eagl display type */
  window = g_object_new (GST_TYPE_GL_WINDOW_EAGL, NULL);
  gst_object_ref_sink (window);

  return window;
}

static guintptr
gst_gl_window_eagl_get_display (GstGLWindow * window)
{
  return 0;
}

static guintptr
gst_gl_window_eagl_get_window_handle (GstGLWindow * window)
{
  return (guintptr) GST_GL_WINDOW_EAGL (window)->priv->internal_view;
}

static void
_create_gl_window (GstGLWindowEagl * window_eagl)
{
  GstGLWindowEaglPrivate *priv = window_eagl->priv;
  UIView *external_view;
  CGRect rect;
  GstGLUIView *view;

  g_mutex_lock (&priv->draw_lock);

  external_view = (__bridge UIView *) priv->external_view;
  rect = CGRectMake (0, 0, external_view.frame.size.width, external_view.frame.size.height);

  window_eagl->priv->window_width = rect.size.width;
  window_eagl->priv->window_height = rect.size.height;

  view = [[GstGLUIView alloc] initWithFrame:rect];
  [view setGstWindow:window_eagl];
  view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  view.contentMode = UIViewContentModeRedraw;

  priv->internal_view = (__bridge_retained gpointer) view;
  [external_view addSubview:view];
  priv->internal_view = (__bridge_retained gpointer) view;
  priv->layer = (__bridge_retained gpointer) [view layer];

  NSDictionary * dict = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
      kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

  [[view layer] setOpaque:YES];
  [(CAEAGLLayer *) [view layer] setDrawableProperties:dict];

  g_cond_broadcast (&priv->cond);
  g_mutex_unlock (&priv->draw_lock);
}

static void
ensure_window_handle_is_set_unlocked (GstGLWindowEagl * window_eagl)
{
  GstGLContext *context;

  if (!window_eagl->priv->pending_set_window_handle)
    return;

  context = gst_gl_window_get_context (GST_GL_WINDOW (window_eagl));
  if (!context) {
    g_critical ("Window does not have a GstGLContext attached!. "
        "Aborting set window handle.");
    g_mutex_unlock (&window_eagl->priv->draw_lock);
    return;
  }

  while (!window_eagl->priv->internal_view)
    g_cond_wait (&window_eagl->priv->cond, &window_eagl->priv->draw_lock);

  GST_INFO_OBJECT (context, "handle set, updating layer");
  gst_gl_context_eagl_update_layer (context, window_eagl->priv->layer);
  gst_object_unref (context);

  window_eagl->priv->pending_set_window_handle = FALSE;
}

static void
gst_gl_window_eagl_set_window_handle (GstGLWindow * window, guintptr handle)
{
  GstGLWindowEagl *window_eagl;

  window_eagl = GST_GL_WINDOW_EAGL (window);

  g_mutex_lock (&window_eagl->priv->draw_lock);
  if (window_eagl->priv->external_view)
    CFRelease (window_eagl->priv->external_view);
  window_eagl->priv->external_view = (gpointer)handle;
  window_eagl->priv->pending_set_window_handle = TRUE;
  g_mutex_unlock (&window_eagl->priv->draw_lock);

  /* XXX: Maybe we need an async set_window_handle? */
  _gl_invoke_on_main ((GstGLWindowEaglFunc) _create_gl_window,
      gst_object_ref (window_eagl), gst_object_unref);
}

static void
gst_gl_window_eagl_quit (GstGLWindow * window)
{
  GstGLWindowEagl *window_eagl = (GstGLWindowEagl *) window;

  window_eagl->priv->shutting_down = TRUE;

  GST_GL_WINDOW_CLASS (parent_class)->quit (window);
}

static void
gst_gl_window_eagl_set_preferred_size (GstGLWindow * window, gint width, gint height)
{
  GstGLWindowEagl *window_eagl = GST_GL_WINDOW_EAGL (window);

  window_eagl->priv->preferred_width = width;
  window_eagl->priv->preferred_height = height;
}

static void
gst_gl_window_eagl_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy)
{
  GstGLWindowEagl *window_eagl = (GstGLWindowEagl *) window;
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContext *unref_context = NULL;
  GThread *thread;

  if (context)
    window_eagl->priv->last_context = unref_context = context;

  /* we may not have a context if we are shutting down */
  if (!context && window_eagl->priv->shutting_down) {
    context = window_eagl->priv->last_context;
    window_eagl->priv->shutting_down = FALSE;
  }

  g_return_if_fail (context != NULL);

  thread = gst_gl_context_get_thread (context);

  if (thread == g_thread_self()) {
    /* this case happens for nested calls happening from inside the GCD queue */
    callback (data);
    if (destroy)
      destroy (data);
    if (unref_context)
      gst_object_unref (unref_context);
  } else {
    dispatch_async ((__bridge dispatch_queue_t)(window_eagl->priv->gl_queue), ^{
      gst_gl_context_activate (context, TRUE);
      callback (data);
      if (unref_context)
        gst_object_unref (unref_context);
      if (destroy)
        destroy (data);
    });
  }
  if (thread)
    g_thread_unref (thread);
}

static void
draw_cb (gpointer data)
{
  GstGLWindowEagl *window_eagl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_eagl);
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContextEagl *eagl_context = GST_GL_CONTEXT_EAGL (context);

  g_mutex_lock (&window_eagl->priv->draw_lock);

  ensure_window_handle_is_set_unlocked (window_eagl);

  if (window_eagl->priv->internal_view) {
    CGSize size;
    CAEAGLLayer *eagl_layer;

    eagl_layer = GST_GL_WINDOW_EAGL_LAYER (window_eagl);
    size = eagl_layer.frame.size;

    size = CGSizeMake (size.width * eagl_layer.contentsScale, size.height * eagl_layer.contentsScale);

    if (window->queue_resize || window_eagl->priv->window_width != size.width ||
        window_eagl->priv->window_height != size.height) {

      window_eagl->priv->window_width = size.width;
      window_eagl->priv->window_height = size.height;

      gst_gl_context_eagl_resize (eagl_context);

      gst_gl_window_resize (window, window_eagl->priv->window_width,
            window_eagl->priv->window_height);
    }
  }

  gst_gl_context_eagl_prepare_draw (eagl_context);

  if (window->draw)
    window->draw (window->draw_data);

  gst_gl_context_swap_buffers (context);

  gst_gl_context_eagl_finish_draw (eagl_context);

  g_mutex_unlock (&window_eagl->priv->draw_lock);

  gst_object_unref (context);
}

static void
gst_gl_window_eagl_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}

gpointer
gst_gl_window_eagl_get_layer (GstGLWindowEagl * window_eagl)
{
  gpointer layer;

  g_mutex_lock (&window_eagl->priv->draw_lock);
  if (window_eagl->priv->layer)
    CFRetain (window_eagl->priv->layer);
  layer = window_eagl->priv->layer;
  g_mutex_unlock (&window_eagl->priv->draw_lock);

  return layer;
}

@implementation GstGLUIView {
    GstGLWindowEagl * window_eagl;
};

+(Class) layerClass
{
  return [CAEAGLLayer class];
}

-(void) setGstWindow:(GstGLWindowEagl *) window
{
  window_eagl = window;
}

@end

void
_gl_invoke_on_main (GstGLWindowEaglFunc func, gpointer data, GDestroyNotify notify)
{
  if ([NSThread isMainThread]) {
    func (data);
    if (notify)
      notify (data);
  } else {
    dispatch_async (dispatch_get_main_queue (), ^{
      func (data);
      if (notify)
        notify (data);
    });
  }
}

G_GNUC_END_IGNORE_DEPRECATIONS
