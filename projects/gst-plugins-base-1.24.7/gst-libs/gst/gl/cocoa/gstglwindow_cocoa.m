/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED >= 1014
# define GL_SILENCE_DEPRECATION
#endif

#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>

#include "gstgl_cocoa_private.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSWindowStyleMaskTitled              NSTitledWindowMask
#define NSWindowStyleMaskClosable            NSClosableWindowMask
#define NSWindowStyleMaskResizable           NSResizableWindowMask
#define NSWindowStyleMaskMiniaturizable      NSMiniaturizableWindowMask
#endif

/* =============================================================*/
/*                                                              */
/*               GstGLNSWindow declaration                      */
/*                                                              */
/* =============================================================*/

@interface GstGLNSWindow: NSWindow {
  BOOL m_isClosed;
  GstGLWindowCocoa *window_cocoa;
}
- (id)initWithContentRect:(NSRect)contentRect
    styleMask: (unsigned int) styleMask
    backing: (NSBackingStoreType) bufferingType
    defer: (BOOL) flag screen: (NSScreen *) aScreen
    gstWin: (GstGLWindowCocoa *) window;
- (void) resize: (int) width height: (int) height;
- (void) setClosed;
- (BOOL) isClosed;
- (BOOL) canBecomeMainWindow;
- (BOOL) canBecomeKeyWindow;
@end

/* =============================================================*/
/*                                                              */
/*                      GstGLWindow                             */
/*                                                              */
/* =============================================================*/

#define GST_CAT_DEFAULT gst_gl_window_cocoa_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void gst_gl_window_cocoa_finalize (GObject * object);

static gboolean gst_gl_window_cocoa_open (GstGLWindow *window, GError **err);
static void gst_gl_window_cocoa_close (GstGLWindow *window);
static void gst_gl_window_cocoa_quit (GstGLWindow *window);
static guintptr gst_gl_window_cocoa_get_window_handle (GstGLWindow * window);
static void gst_gl_window_cocoa_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_cocoa_draw (GstGLWindow * window);
static void gst_gl_window_cocoa_set_preferred_size (GstGLWindow * window,
    gint width, gint height);
static void gst_gl_window_cocoa_show (GstGLWindow * window);
static void gst_gl_window_cocoa_queue_resize (GstGLWindow * window);
static void gst_gl_window_cocoa_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy);
static gboolean gst_gl_window_cocoa_set_render_rectangle (GstGLWindow * window,
    gint x, gint y, gint width, gint height);
static gboolean gst_gl_window_cocoa_controls_viewport (GstGLWindow * window);


struct _GstGLWindowCocoaPrivate
{
  gpointer internal_win_id;
  gpointer internal_view;
  gpointer external_view;
  gboolean visible;
  gint preferred_width;
  gint preferred_height;

  /* atomic set when the internal NSView has been created */
  int view_ready;

  gpointer gl_queue;

  gboolean shutting_down;
  GstGLContext *last_context;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_GET (GST_CAT_DEFAULT, "glwindow");

#define gst_gl_window_cocoa_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLWindowCocoa, gst_gl_window_cocoa, GST_TYPE_GL_WINDOW,
    G_ADD_PRIVATE (GstGLWindowCocoa)
    DEBUG_INIT);

static void
gst_gl_window_cocoa_class_init (GstGLWindowCocoaClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  window_class->open = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_close);
  window_class->quit = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_quit);
  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_get_window_handle);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_set_window_handle);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_draw);
  window_class->set_preferred_size =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_set_preferred_size);
  window_class->show = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_show);
  window_class->queue_resize = GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_queue_resize);
  window_class->send_message_async =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_send_message_async);
  window_class->set_render_rectangle =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_set_render_rectangle);
  window_class->controls_viewport =
      GST_DEBUG_FUNCPTR (gst_gl_window_cocoa_controls_viewport);

  gobject_class->finalize = gst_gl_window_cocoa_finalize;
}

static void
gst_gl_window_cocoa_init (GstGLWindowCocoa * window)
{
  window->priv = gst_gl_window_cocoa_get_instance_private (window);

  window->priv->preferred_width = 320;
  window->priv->preferred_height = 240;
#if OS_OBJECT_USE_OBJC
  window->priv->gl_queue = (__bridge_retained gpointer)
      (dispatch_queue_create ("org.freedesktop.gstreamer.glwindow", NULL));
#else
  window->priv->gl_queue = (gpointer)
      (dispatch_queue_create ("org.freedesktop.gstreamer.glwindow", NULL));
#endif
}

static void
gst_gl_window_cocoa_finalize (GObject * object)
{
  GstGLWindowCocoa *window = GST_GL_WINDOW_COCOA (object);

#if OS_OBJECT_USE_OBJC
  /* Let ARC clean up our queue */
  dispatch_queue_t queue = (__bridge_transfer dispatch_queue_t) window->priv->gl_queue;
  (void) queue;
#else
  dispatch_release (window->priv->gl_queue);
#endif

  window->priv->gl_queue = NULL;
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstGLWindowCocoa *
gst_gl_window_cocoa_new (GstGLDisplay * display)
{
  GstGLWindowCocoa *window;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_COCOA) == 0)
    /* we require an cocoa display to create CGL windows */
    return NULL;

  window = g_object_new (GST_TYPE_GL_WINDOW_COCOA, NULL);
  gst_object_ref_sink (window);

  return window;
}

/* Must be called from the main thread */
gboolean
gst_gl_window_cocoa_create_window (GstGLWindowCocoa *window_cocoa)
{
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLWindow *window = GST_GL_WINDOW (window_cocoa);
  GstGLNSWindow *internal_win_id;
  NSRect mainRect = [[NSScreen mainScreen] visibleFrame];
  gint h = priv->preferred_height;
  gint y = mainRect.size.height > h ? (mainRect.size.height - h) * 0.5 : 0;
  NSRect rect = NSMakeRect (0, y, priv->preferred_width, priv->preferred_height);
  NSRect windowRect = NSMakeRect (0, y, priv->preferred_width, priv->preferred_height);
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLCAOpenGLLayer *layer;
  GstGLNSView *glView;

  if (!context)
    return FALSE;

  layer = [[GstGLCAOpenGLLayer alloc] initWithGstGLWindow:window];
  layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  layer.needsDisplayOnBoundsChange = YES;
  glView = [[GstGLNSView alloc] initWithFrameLayer:window_cocoa rect:windowRect layer:layer];

  gst_object_unref (context);

  internal_win_id = [[GstGLNSWindow alloc] initWithContentRect:rect styleMask:
      (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
      backing: NSBackingStoreBuffered defer: NO screen: nil gstWin: window_cocoa];

  priv->internal_win_id = (__bridge_retained gpointer)internal_win_id;
  priv->internal_view = (__bridge gpointer)glView;

  GST_DEBUG ("NSWindow id: %"G_GUINTPTR_FORMAT, (guintptr) priv->internal_win_id);

  [internal_win_id setContentView:glView];

  g_atomic_int_set (&window_cocoa->priv->view_ready, 1);

  /* Set the window handle for real now that the NSWindow has been created. */
  if (priv->external_view)
    gst_gl_window_cocoa_set_window_handle (window,
        (guintptr) priv->external_view);

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  return TRUE;
}

static gboolean
gst_gl_window_cocoa_open (GstGLWindow *window, GError **err)
{
  return TRUE;
}

static void
_close_window (gpointer * data)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (data);
  GstGLNSView *view =
      (__bridge GstGLNSView *) window_cocoa->priv->internal_view;

  [view removeFromSuperview];

  CFBridgingRelease (window_cocoa->priv->internal_win_id);
  CFBridgingRelease (window_cocoa->priv->internal_view);
  window_cocoa->priv->internal_win_id = NULL;
  window_cocoa->priv->internal_view = NULL;
}

static void
gst_gl_window_cocoa_close (GstGLWindow * window)
{
  _gst_gl_invoke_on_main ((GstGLWindowCB) _close_window,
      gst_object_ref (window), (GDestroyNotify) gst_object_unref);
}

static guintptr
gst_gl_window_cocoa_get_window_handle (GstGLWindow *window)
{
  return (guintptr) GST_GL_WINDOW_COCOA (window)->priv->internal_win_id;
}

static void
gst_gl_window_cocoa_set_window_handle (GstGLWindow * window, guintptr handle)
{
  GstGLWindowCocoa *window_cocoa;
  GstGLWindowCocoaPrivate *priv;

  window_cocoa = GST_GL_WINDOW_COCOA (window);
  priv = window_cocoa->priv;

  if (priv->internal_win_id) {
    GstGLWindowCocoa *window_cocoa2 = gst_object_ref (window_cocoa);
    if (handle) {
      priv->external_view = (gpointer)handle;
      priv->visible = TRUE;
    } else {
      /* bring back our internal window */
      priv->external_view = 0;
      priv->visible = FALSE;
    }

    dispatch_async (dispatch_get_main_queue (), ^{
      GstGLNSWindow *internal_win_id =
          (__bridge GstGLNSWindow *)window_cocoa->priv->internal_win_id;
      NSView *external_view =
          (__bridge NSView *)window_cocoa->priv->external_view;
      NSView *view = (__bridge NSView *)window_cocoa->priv->internal_view;

      [internal_win_id orderOut:internal_win_id];

      [external_view addSubview: view];

      [external_view setAutoresizesSubviews: YES];
      [view setFrame: [external_view bounds]];
      [view setAutoresizingMask: NSViewWidthSizable|NSViewHeightSizable];
      gst_object_unref (window_cocoa2);
    });
  } else {
    /* no internal window yet so delay it to the next drawing */
    priv->external_view = (gpointer)handle;
    priv->visible = FALSE;
  }
}

static void
_show_window (gpointer data)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (data);
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSWindow *internal_win_id = (__bridge GstGLNSWindow *)priv->internal_win_id;

  GST_DEBUG_OBJECT (window_cocoa, "make the window available\n");

  /* Preferred size might not yet be set when create_window is called,
   * so we need to resize the window here to be sure. */
  [internal_win_id resize: priv->preferred_width height: priv->preferred_height];
  [internal_win_id makeMainWindow];
  [internal_win_id orderFrontRegardless];
  [internal_win_id setViewsNeedDisplay:YES];

  priv->visible = TRUE;
}

static void
gst_gl_window_cocoa_show (GstGLWindow * window)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (window);
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;

  if (!priv->visible) {
    /* useful when set_window_handle is called before
     * the internal NSWindow */
    if (priv->external_view) {
      gst_gl_window_cocoa_set_window_handle (window, (guintptr) priv->external_view);
      priv->visible = TRUE;
      return;
    }

    if (!priv->external_view && !priv->visible)
      _gst_gl_invoke_on_main ((GstGLWindowCB) _show_window,
          gst_object_ref (window), (GDestroyNotify) gst_object_unref);
  }
}

static void
gst_gl_window_cocoa_queue_resize (GstGLWindow * window)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (window);
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSView *view = (__bridge GstGLNSView *)priv->internal_view;

  if (!g_atomic_int_get (&window_cocoa->priv->view_ready))
    return;

  [view->layer queueResize];
}

static void
gst_gl_window_cocoa_draw (GstGLWindow * window)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (window);
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSView *view = (__bridge GstGLNSView *)priv->internal_view;

  /* As the view is created asynchronously in the main thread we cannot know
   * exactly when it will be ready to draw to */
  if (!g_atomic_int_get (&window_cocoa->priv->view_ready))
    return;

  /* this redraws the GstGLCAOpenGLLayer which calls
   * gst_gl_window_cocoa_draw_thread(). Use an explicit CATransaction since we
   * don't know how often the main runloop is running.
   */
  [CATransaction begin];
  [view setNeedsDisplay:YES];
  [CATransaction commit];
}

static void
gst_gl_window_cocoa_set_preferred_size (GstGLWindow * window, gint width,
    gint height)
{
  GstGLWindowCocoa *window_cocoa = GST_GL_WINDOW_COCOA (window);

  window_cocoa->priv->preferred_width = width;
  window_cocoa->priv->preferred_height = height;
}

static void
gst_gl_cocoa_draw_cb (GstGLWindowCocoa *window_cocoa)
{
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSWindow *internal_win_id = (__bridge GstGLNSWindow *)priv->internal_win_id;

  if (internal_win_id && ![internal_win_id isClosed]) {
    GstGLWindow *window = GST_GL_WINDOW (window_cocoa);

    /* draw opengl scene in the back buffer */
    /* We do not need to change viewports like in other window implementations
     * as the caopengllayer will take care of that for us. */
    if (window->draw)
      window->draw (window->draw_data);
  }
}

static void
gst_gl_cocoa_resize_cb (GstGLNSView * view, guint width, guint height)
{
  GstGLWindowCocoa *window_cocoa = view->window_cocoa;
  GstGLWindow *window = GST_GL_WINDOW (window_cocoa);
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSWindow *internal_win_id = (__bridge GstGLNSWindow *)priv->internal_win_id;

  if (internal_win_id && ![internal_win_id isClosed]) {
    const GstGLFuncs *gl;
    NSRect bounds = [view bounds];
    NSRect visibleRect = [view visibleRect];
    gint viewport_dim[4];
    GstVideoRectangle viewport;

    gl = context->gl_vtable;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    bounds = [view convertRectToBacking:bounds];
    visibleRect = [view convertRectToBacking:visibleRect];
#endif

    /* don't use the default gst_gl_window_resize() as that will marshal through
     * the GL thread.  We are being called from the main thread by the
     * caopengllayer */
    if (window->resize)
      window->resize (window->resize_data, width, height);

    gl->GetIntegerv (GL_VIEWPORT, viewport_dim);

    GST_DEBUG_OBJECT (window, "Window resized: bounds %lf %lf %lf %lf "
                      "visibleRect %lf %lf %lf %lf, "
                      "viewport dimensions %i %i %i %i",
                      bounds.origin.x, bounds.origin.y,
                      bounds.size.width, bounds.size.height,
                      visibleRect.origin.x, visibleRect.origin.y,
                      visibleRect.size.width, visibleRect.size.height,
                      viewport_dim[0], viewport_dim[1], viewport_dim[2],
                      viewport_dim[3]);

    viewport.x = viewport_dim[0] - visibleRect.origin.x;
    viewport.x = viewport_dim[1] - visibleRect.origin.y;
    viewport.w = viewport_dim[2];
    viewport.h = viewport_dim[3];

    gl->Viewport (viewport.x, viewport.y, viewport.w, viewport.h);
  }

  gst_object_unref (context);
}

static void
gst_gl_window_cocoa_send_message_async (GstGLWindow * window,
    GstGLWindowCB callback, gpointer data, GDestroyNotify destroy)
{
  GstGLWindowCocoa *window_cocoa = (GstGLWindowCocoa *) window;
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContext *unref_context = NULL;
  GThread *thread = NULL;
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
#if OS_OBJECT_USE_OBJC
  dispatch_queue_t gl_queue = (__bridge dispatch_queue_t)priv->gl_queue;
#else
  dispatch_queue_t gl_queue = (dispatch_queue_t)priv->gl_queue;
#endif

  if (context)
    window_cocoa->priv->last_context = unref_context = context;

  /* we may not have a context if we are shutting down */
  if (!context && window_cocoa->priv->shutting_down) {
    context = window_cocoa->priv->last_context;
    window_cocoa->priv->shutting_down = FALSE;
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
    dispatch_async (gl_queue, ^{
      gst_gl_context_activate (context, TRUE);
      if (unref_context)
        gst_object_unref (unref_context);
      callback (data);
      if (destroy)
        destroy (data);
    });
  }
  if (thread)
    g_thread_unref (thread);
}

static void
gst_gl_window_cocoa_quit (GstGLWindow * window)
{
  GstGLWindowCocoa *window_cocoa = (GstGLWindowCocoa *) window;

  window_cocoa->priv->shutting_down = TRUE;

  GST_GL_WINDOW_CLASS (parent_class)->quit (window);
}

struct SetRenderRectangle
{
 GstGLWindowCocoa *window_cocoa;
 GstVideoRectangle rect;
};

static void
_free_set_render_rectangle (struct SetRenderRectangle *render)
{
  if (render) {
    if (render->window_cocoa) {
      gst_object_unref (render->window_cocoa);
    }
    g_free (render);
  }
}

static void
_set_render_rectangle (gpointer data)
{
 struct SetRenderRectangle *render = data;
 GstGLWindowCocoaPrivate *priv = render->window_cocoa->priv;
 GstGLNSView *view = (__bridge GstGLNSView *)priv->internal_view;

 GST_LOG_OBJECT (render->window_cocoa, "setting render rectangle %i,%i+%ix%i",
                 render->rect.x, render->rect.y, render->rect.w, render->rect.h);
 if (!g_atomic_int_get (&render->window_cocoa->priv->view_ready)) {
   return;
 }

 NSRect newMainViewFrame = NSMakeRect(render->rect.x,
                                      render->rect.y,
                                      render->rect.w,
                                      render->rect.h);

 [view.superview setFrame:newMainViewFrame];
 [view setFrame: view.superview.bounds];

 [CATransaction begin];
 [view setNeedsDisplay:YES];
 [CATransaction commit];
}

static gboolean
gst_gl_window_cocoa_set_render_rectangle (GstGLWindow * window, gint x, gint y, gint width, gint height)
{
 GstGLWindowCocoa *window_cocoa = (GstGLWindowCocoa *) window;
 struct SetRenderRectangle *render;

 render = g_new0 (struct SetRenderRectangle, 1);
 render->window_cocoa = gst_object_ref (window_cocoa);
 render->rect.x = x;
 render->rect.y = y;
 render->rect.w = width;
 render->rect.h = height;

 _gst_gl_invoke_on_main ((GstGLWindowCB) _set_render_rectangle, render,
     (GDestroyNotify) _free_set_render_rectangle);

 return TRUE;
}

static gboolean
gst_gl_window_cocoa_controls_viewport (GstGLWindow * window)
{
  return TRUE;
}

/* =============================================================*/
/*                                                              */
/*                    GstGLNSWindow implementation              */
/*                                                              */
/* =============================================================*/

/* Must be called from the main thread */
@implementation GstGLNSWindow

- (id) initWithContentRect: (NSRect) contentRect
        styleMask: (unsigned int) styleMask
    backing: (NSBackingStoreType) bufferingType
    defer: (BOOL) flag screen: (NSScreen *) aScreen
    gstWin: (GstGLWindowCocoa *) cocoa {

  m_isClosed = NO;
  window_cocoa = cocoa;
  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSWindow *internal_win_id = (__bridge GstGLNSWindow *)priv->internal_win_id;

  self = [super initWithContentRect: contentRect
        styleMask: styleMask backing: bufferingType
        defer: flag screen:aScreen];

  [self setReleasedWhenClosed:NO];

  GST_DEBUG ("initializing GstGLNSWindow\n");

  [self setTitle:@"OpenGL renderer"];

  [self setBackgroundColor:[NSColor blackColor]];

  [self orderOut:internal_win_id];

  return self;
}

- (void) resize: (gint) width height: (gint) height {
  NSRect frame = [super frame];
  if (frame.size.width == width && frame.size.height == height)
    return;

  frame.size = NSMakeSize (width, height);
  [super setFrame:frame display:YES];
}

- (void) setClosed {
  m_isClosed = YES;
}

- (BOOL) isClosed {
  return m_isClosed;
}

- (BOOL) canBecomeMainWindow {
  return YES;
}

- (BOOL) canBecomeKeyWindow {
  return YES;
}

static void
close_window_cb (gpointer data)
{
  GstGLWindowCocoa *window_cocoa = data;
  GstGLWindow *window;

  window = GST_GL_WINDOW (window_cocoa);

  if (window->close) {
    window->close (window->close_data);
  }
}

/* Called in the main thread which is never the gl thread */
- (BOOL) windowShouldClose:(id)sender {

  GstGLWindowCocoaPrivate *priv = window_cocoa->priv;
  GstGLNSWindow *internal_win_id = (__bridge GstGLNSWindow *)priv->internal_win_id;
  GST_DEBUG ("user clicked the close button\n");
  [internal_win_id setClosed];
  gst_gl_window_send_message_async (GST_GL_WINDOW (window_cocoa),
      (GstGLWindowCB) close_window_cb, gst_object_ref (window_cocoa),
      (GDestroyNotify) gst_object_unref);
  return YES;
}

@end

/* =============================================================*/
/*                                                              */
/*                GstGLNSView implementation              */
/*                                                              */
/* =============================================================*/

@implementation GstGLNSView

/* Must be called from the application main thread */
- (id)initWithFrameLayer:(GstGLWindowCocoa *)window rect:(NSRect)contentRect layer:(CALayer *)layerContent {

  self = [super initWithFrame: contentRect];

  window_cocoa = window;

  /* The order of the next two calls matters.  This creates a layer-hosted
   * NSView.  Calling setWantsLayer before setLayer will create a
   * layer-backed NSView.  See the apple developer documentation on the
   * difference.
   */
  [self setLayer:layerContent];
  [self setWantsLayer:YES];
  self->layer = (GstGLCAOpenGLLayer *)layerContent;
  [self->layer setDrawCallback:(GstGLWindowCB)gst_gl_cocoa_draw_cb
      data:window notify:NULL];
  [self->layer setResizeCallback:(GstGLWindowResizeCB)gst_gl_cocoa_resize_cb
      data:(__bridge_retained gpointer)self notify:(GDestroyNotify)CFRelease];

  [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawOnSetNeedsDisplay];

  [self setWantsBestResolutionOpenGLSurface:YES];

  return self;
}

- (void) dealloc {
  self->layer = nil;
}

- (void)renewGState {
  /* Don't update the screen until we redraw, this
   * prevents flickering during scrolling, clipping,
   * resizing, etc
   */
  [[self window] disableScreenUpdatesUntilFlush];

  [super renewGState];
}

- (BOOL) isOpaque {
    return YES;
}

- (BOOL) isFlipped {
    return NO;
}

@end

void
_gst_gl_invoke_on_main (GstGLWindowCB func, gpointer data, GDestroyNotify notify)
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
