/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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
#include <gst/video/videooverlay.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <math.h>
#include <sys/time.h>

static GMainLoop *loop;

static Display *disp;
static Window root, win = 0;
static GC gc;
static gint width = 320, height = 240, x = 0, y = 0;
static gint disp_width, disp_height;

static inline long
myclock (void)
{
  struct timeval tv;

  gettimeofday (&tv, NULL);
  return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void
open_display (void)
{
  gint screen_num;

  disp = XOpenDisplay (NULL);
  root = DefaultRootWindow (disp);
  screen_num = DefaultScreen (disp);
  disp_width = DisplayWidth (disp, screen_num);
  disp_height = DisplayHeight (disp, screen_num);
}

static void
close_display (void)
{
  XCloseDisplay (disp);
}

static gboolean
resize_window (GstPipeline * pipeline)
{
  width = (sin (myclock () / 300.0) * 200) + 640;
  height = (sin (myclock () / 300.0) * 200) + 480;

  XResizeWindow (disp, win, width, height);

  XSync (disp, FALSE);

  return TRUE;
}

static gboolean
move_window (GstPipeline * pipeline)
{
  x += 5;
  y = disp_height - height + (sin (myclock () / 300.0) * height);
  if (x > disp_width)
    x = 0;

  XMoveWindow (disp, win, x, y);

  XSync (disp, FALSE);

  return TRUE;
}

static gboolean
toggle_events (GstVideoOverlay * ov)
{
  static gboolean events_toggled;

  gst_video_overlay_handle_events (ov, events_toggled);

  if (events_toggled) {
    g_print ("Events are handled\n");
    events_toggled = FALSE;
  } else {
    g_print ("Events are NOT handled\n");
    events_toggled = TRUE;
  }

  return TRUE;
}

static gboolean
cycle_window (GstVideoOverlay * ov)
{
  XGCValues values;
  Window old_win = win;
  GC old_gc = gc;

  win = XCreateSimpleWindow (disp, root, 0, 0, width, height, 0, 0, 0);

  XSetWindowBackgroundPixmap (disp, win, None);

  gc = XCreateGC (disp, win, 0, &values);

  XMapRaised (disp, win);

  XSync (disp, FALSE);

  gst_video_overlay_set_window_handle (ov, win);

  if (old_win) {
    XDestroyWindow (disp, old_win);
    XFreeGC (disp, old_gc);
    XSync (disp, FALSE);
  }

  return TRUE;
}

static GstBusSyncReply
create_window (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  GstVideoOverlay *ov = NULL;

  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  ov = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));

  g_print ("Creating our own window\n");

  cycle_window (ov);

  g_timeout_add (50, (GSourceFunc) resize_window, pipeline);
  g_timeout_add (50, (GSourceFunc) move_window, pipeline);
  g_timeout_add (100, (GSourceFunc) cycle_window, ov);
  g_timeout_add_seconds (2, (GSourceFunc) toggle_events, ov);

  gst_message_unref (message);
  return GST_BUS_DROP;
}

#if 0
static gboolean
terminate_playback (GstElement * pipeline)
{
  g_print ("Terminating playback\n");
  g_main_loop_quit (loop);
  return FALSE;
}
#endif

static gboolean
pause_playback (GstElement * pipeline)
{
  g_print ("Pausing playback\n");
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  return FALSE;
}

static gboolean
start_playback (GstElement * pipeline)
{
  g_print ("Starting playback\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  return FALSE;
}

int
main (int argc, char **argv)
{
  GstElement *pipeline;
  GstBus *bus;

#ifndef GST_DISABLE_PARSE
  GError *error = NULL;
#endif

  gst_init (&argc, &argv);

  if (argc != 2) {
    g_print ("Usage: %s \"pipeline description with launch format\"\n",
        argv[0]);
    g_print
        ("The pipeline should contain an element implementing GstVideoOverlay.\n");
    g_print ("Example: %s \"videotestsrc ! ximagesink\"\n", argv[0]);
    return -1;
  }
#ifdef GST_DISABLE_PARSE
  g_print ("GStreamer was built without pipeline parsing capabilities.\n");
  g_print
      ("Please rebuild GStreamer with pipeline parsing capabilities activated to use this example.\n");
  return 1;
#else
  pipeline = gst_parse_launch (argv[1], &error);
  if (error) {
    g_print ("Error while parsing pipeline description: %s\n", error->message);
    return -1;
  }
#endif

  loop = g_main_loop_new (NULL, FALSE);

  open_display ();

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, pipeline,
      NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* We want to get out after */
  //g_timeout_add (500000, (GSourceFunc) terminate_playback, pipeline);
  g_timeout_add_seconds (10, (GSourceFunc) pause_playback, pipeline);
  g_timeout_add_seconds (20, (GSourceFunc) start_playback, pipeline);

  g_main_loop_run (loop);

  close_display ();

  g_main_loop_unref (loop);

  gst_object_unref (bus);

  return 0;
}
