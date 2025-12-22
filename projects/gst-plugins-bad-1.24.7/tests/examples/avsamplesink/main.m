/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <gst/gst.h>

#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSWindowStyleMaskBorderless          NSBorderlessWindowMask
#endif

static NSRunLoop *loop;
static int quit = 0;

static void
end_stream_cb(GstBus* bus, GstMessage* message, GstElement* pipeline)
{
  switch (GST_MESSAGE_TYPE (message))
  {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");

      g_atomic_int_set (&quit, 1);;
      CFRunLoopStop ([loop getCFRunLoop]);
      break;
    case GST_MESSAGE_ERROR:
    {
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (message, &err, &debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      if (debug)
      {
        g_print ("Debug details: %s\n", debug);
        g_free (debug);
      }

      g_atomic_int_set (&quit, 1);;
      CFRunLoopStop ([loop getCFRunLoop]);
      break;
    }
    default:
      break;
  }
}

gint main (gint argc, gchar *argv[])
{
  CALayer *layer;
  loop = [NSRunLoop currentRunLoop];

  gst_init (&argc, &argv);

  [NSApplication sharedApplication];

  GstElement* pipeline = gst_pipeline_new ("pipeline");
  GstElement* videosrc  = gst_element_factory_make ("videotestsrc", NULL);
  GstElement* videosink = gst_element_factory_make ("avsamplebufferlayersink", NULL);
  GstCaps *caps = gst_caps_from_string ("video/x-raw,format=UYVY");

  gst_bin_add_many (GST_BIN (pipeline), videosrc, videosink, NULL);

  gboolean link_ok = gst_element_link_filtered (videosrc, videosink, caps);
  gst_caps_unref (caps);
  if (!link_ok) {
     g_critical ("Failed to link an element!\n") ;
    return -1;
  }

  g_object_set (videosrc, "num-buffers", 500, NULL);

  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_element_set_state (pipeline, GST_STATE_READY);
  gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

  g_object_get (videosink, "layer", &layer, NULL);

  NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect (0, 0, 320, 240)
      styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
  [window setOpaque:NO];
  [window.contentView setWantsLayer:YES];

  NSView *view = [window.contentView superview];
  [view setLayer:layer];

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  [window orderFront:window];

  while (!g_atomic_int_get (&quit) && [loop runMode:NSDefaultRunLoopMode
      beforeDate:[NSDate distantPast]]) {
    GstMessage *msg;
    while ((msg = gst_bus_timed_pop (bus, 1 * GST_MSECOND)))
      end_stream_cb (bus, msg, pipeline);
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (bus);
  gst_object_unref (pipeline);

  return 0;
}

