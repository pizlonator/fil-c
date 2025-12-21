/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED >= 1014
# define GL_SILENCE_DEPRECATION
#endif

#include <Cocoa/Cocoa.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSEventMaskAny                       NSAnyEventMask
#define NSWindowStyleMaskTitled              NSTitledWindowMask
#define NSWindowStyleMaskClosable            NSClosableWindowMask
#define NSWindowStyleMaskResizable           NSResizableWindowMask
#define NSWindowStyleMaskMiniaturizable      NSMiniaturizableWindowMask
#endif

/* ============================================================= */
/*                                                               */
/*                          MainWindow                           */
/*                                                               */
/* ============================================================= */

@interface MainWindow: NSWindow <NSApplicationDelegate> {
  GMainLoop *m_loop;
  GstElement *m_pipeline;
  gboolean m_isClosed;
}
- (id) initWithContentRect:(NSRect) contentRect Loop:(GMainLoop*)loop Pipeline:(GstElement*)pipeline;
- (GMainLoop*) loop;
- (GstElement*) pipeline;
- (gboolean) isClosed;
@end

@implementation MainWindow

- (id) initWithContentRect:(NSRect)contentRect Loop:(GMainLoop*)loop Pipeline:(GstElement*)pipeline
{
  m_loop = loop;
  m_pipeline = pipeline;
  m_isClosed = FALSE;

  self = [super initWithContentRect: contentRect
    styleMask: (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
    backing: NSBackingStoreBuffered defer: NO screen: nil];

  [self setReleasedWhenClosed:NO];
  [[NSApplication sharedApplication] setDelegate:self];

  [self setTitle:@"gst-plugins-gl implements videooverlay interface"];

  return self;
}

- (GMainLoop*) loop {
  return m_loop;
}

- (GstElement*) pipeline {
  return m_pipeline;
}

- (gboolean) isClosed {
  return m_isClosed;
}

- (void) customClose {
  m_isClosed = TRUE;
}

- (BOOL) windowShouldClose:(id)sender {
  gst_element_send_event (m_pipeline, gst_event_new_eos ());
  return YES;
}

- (void) applicationDidFinishLaunching: (NSNotification *) not {
  [self makeMainWindow];
  [self center];
  [self orderFront:self];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
  return NO;
}

@end


/* ============================================================= */
/*                                                               */
/*                   gstreamer callbacks                         */
/*                                                               */
/* ============================================================= */


static GstBusSyncReply create_window (GstBus* bus, GstMessage* message, MainWindow* window)
{
  // ignore anything but 'prepare-window-handle' element messages
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  g_print ("setting window handle %lud\n", (gulong) window);

  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message)), (guintptr) [window contentView]);

  gst_message_unref (message);

  return GST_BUS_DROP;
}


static void end_stream_cb(GstBus* bus, GstMessage* message, MainWindow* window)
{
  g_print ("end of stream\n");

  gst_element_set_state ([window pipeline], GST_STATE_NULL);
  gst_object_unref ([window pipeline]);
  g_main_loop_quit ([window loop]);

  [window performSelectorOnMainThread:@selector(customClose) withObject:nil waitUntilDone:YES];
}

static gpointer thread_func (MainWindow* window)
{
  g_main_loop_run ([window loop]);

  return NULL;
}


/* ============================================================= */
/*                                                               */
/*                         application                           */
/*                                                               */
/* ============================================================= */

int main(int argc, char **argv)
{
	int width = 640;
  int height = 480;

  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL;

  GstElement *videosrc  = NULL;
  GstElement *videosink = NULL;
  GstCaps *caps=NULL;
  gboolean ok=FALSE;
  GstBus *bus=NULL;
  GThread *loop_thread=NULL;
  NSRect rect;
  MainWindow *window=nil;

  [NSApplication sharedApplication];

  g_print("app created\n");

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline");

  videosrc  = gst_element_factory_make ("videotestsrc", "videotestsrc");
  videosink = gst_element_factory_make ("glimagesink", "glimagesink");

  g_object_set(G_OBJECT(videosrc), "num-buffers", 500, NULL);

  gst_bin_add_many (GST_BIN (pipeline), videosrc, videosink, NULL);

  caps = gst_caps_new_simple("video/x-raw",
    "width", G_TYPE_INT, width,
    "height", G_TYPE_INT, height,
    "framerate", GST_TYPE_FRACTION, 25, 1,
    "format", G_TYPE_STRING, "I420",
    NULL);

  ok = gst_element_link_filtered(videosrc, videosink, caps);
  gst_caps_unref(caps);
  if (!ok)
    g_warning("could not link videosrc to videosink\n");

  rect.origin.x = 0; rect.origin.y = 0;
  rect.size.width = width; rect.size.height = height;

  window = [[MainWindow alloc] initWithContentRect:rect Loop:loop Pipeline:pipeline];

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  /* NOTE: window is not bridge_retained because its lifetime is just this function */
  g_signal_connect(bus, "message::error", G_CALLBACK(end_stream_cb), (__bridge gpointer)window);
  g_signal_connect(bus, "message::warning", G_CALLBACK(end_stream_cb), (__bridge gpointer)window);
  g_signal_connect(bus, "message::eos", G_CALLBACK(end_stream_cb), (__bridge gpointer)window);
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, (__bridge gpointer)window, NULL);
  gst_object_unref (bus);

  loop_thread = g_thread_new (NULL,
      (GThreadFunc) thread_func, (__bridge gpointer)window);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  [window orderFront:window];

  while (![window isClosed]) {
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
      untilDate:[NSDate dateWithTimeIntervalSinceNow:1]
      inMode:NSDefaultRunLoopMode dequeue:YES];
    if (event)
      [NSApp sendEvent:event];
  }

  g_thread_join (loop_thread);

  return 0;
}
