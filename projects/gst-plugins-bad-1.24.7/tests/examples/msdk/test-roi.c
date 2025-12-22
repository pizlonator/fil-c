/*
 *  test-roi.c - Testsuite for Region of Interest
 *
 *  Copyright (C) 2019 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <gst/gst.h>
#include <gst/video/navigation.h>
#include <gst/video/gstvideometa.h>

typedef struct _CustomData
{
  GstElement *pipeline;
  GMainLoop *loop;
  gboolean roi_enabled;
  gboolean is_delta_qp;
} AppData;

static void
send_eos_event (AppData * data)
{
  gst_element_send_event (data->pipeline, gst_event_new_eos ());
}

static void
dispatch_keystroke (AppData * app, const gchar * str)
{
  switch (g_ascii_tolower (str[0])) {
    case 'r':
      app->roi_enabled = !app->roi_enabled;
      gst_println ("ROI %s", app->roi_enabled ? "enabled" : "disabled");
      break;
    case 'd':
      app->is_delta_qp = !app->is_delta_qp;
      gst_println ("Use %s", app->is_delta_qp ? "delta QP" : "Priority");
      break;
    case 'q':
      send_eos_event (app);
      break;
    default:
      break;
  }

  return;
}

static void
cb_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  AppData *app = data;
  GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);
  GstEvent *ev = NULL;
  GstNavigationEventType type;
  const gchar *key;

  if (mtype != GST_NAVIGATION_MESSAGE_EVENT)
    return;
  if (!gst_navigation_message_parse_event (msg, &ev))
    goto bail;

  type = gst_navigation_event_get_type (ev);
  if (type != GST_NAVIGATION_EVENT_KEY_PRESS)
    goto bail;
  if (!gst_navigation_event_parse_key_event (ev, &key))
    goto bail;

  dispatch_keystroke (app, key);

bail:
  if (ev)
    gst_event_unref (ev);
}

static void
cb_msg_eos (GstBus * bus, GstMessage * msg, gpointer data)
{
  AppData *app = data;
  g_main_loop_quit (app->loop);
}


static void
cb_msg_error (GstBus * bus, GstMessage * msg, gpointer data)
{
  AppData *app = data;
  gchar *debug = NULL;
  GError *err = NULL;

  gst_message_parse_error (msg, &err, &debug);

  g_print ("Error: %s\n", err->message);
  g_error_free (err);

  if (debug) {
    g_print ("Debug details: %s\n", debug);
    g_free (debug);
  }

  g_main_loop_quit (app->loop);
}

static GstPadProbeReturn
cb_add_roi (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  AppData *app = data;
  GstVideoRegionOfInterestMeta *rmeta;
  GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER (info);
  GstStructure *s;

  if (!app->roi_enabled)
    return GST_PAD_PROBE_OK;

  buf = gst_buffer_make_writable (buf);

  rmeta =
      gst_buffer_add_video_region_of_interest_meta (buf, "test", 0, 0, 320,
      240);

  if (app->is_delta_qp)
    s = gst_structure_new ("roi/msdk", "delta-qp", G_TYPE_INT, 20, NULL);
  else
    s = gst_structure_new ("roi/msdk", "priority", G_TYPE_INT, -3, NULL);
  gst_video_region_of_interest_meta_add_param (rmeta, s);

  GST_PAD_PROBE_INFO_DATA (info) = buf;
  return GST_PAD_PROBE_OK;
}

/* Process keyboard input */
static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, gpointer data)
{
  AppData *app = data;
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL,
          NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  dispatch_keystroke (app, str);

  g_free (str);
  return TRUE;
}

/*
 * This is an example pipeline to recognize difference between ROI and non-ROI.
 * 1. Produce snow pattern with 320p
 * 2. Encode and decode the raw data with 2 pipelines at same time.
 *    2.1. Insert GstVideoRegionOfInterestMeta to the 2nd pipeline buffers to enable ROI.
 * 3. Mix both streams in videomixer.
 * 5. Output the result in one window.
 *
 * Note that the higher definition of original raw data, the easier we
 * recognize.  So you can replace videotestsrc with your
 * high-definition camera or other src elements.
 */

/*
.----------.  .---.     .--------.  .---.  .---.  .---.  .--------.  .----------.  .-----.
| videosrc |->|tee|->Q->|txtovrly|->|enc|->|dec|->|vpp|->|videobox|->|videomixer|->|vsink|
'----------'  '---'     '--------'  '---'  '---'  '---'  '--------'  '----------'  '-----'
                ^                                                    ^
                |                                                    |
                |       .--------.  .---.  .---.  .---.  .--------.  |
                '--->Q->|txtovrly|->|enc|->|dec|->|vpp|->|videobox|->'
                     ^  '--------'  '---'  '---'  '---'  '--------'
                     |
                     '-- Insert GstVideoRegionOfInterestMeta width roi/msdk params on buffers
*/

int
main (int argc, char *argv[])
{
  AppData data = { 0, };
  GstStateChangeReturn ret;
  GstElement *el;
  GstPad *pad;
  GError *err = NULL;
  GIOChannel *io_stdin;
  GstBus *bus;

  data.roi_enabled = TRUE;
  data.is_delta_qp = TRUE;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Print usage map */
  g_print ("USAGE: 'r' to enable/disable ROI, "
      "'d' to change ROI mode && 'q' to quit\n");

#define SRC "videotestsrc pattern=snow ! " \
            "video/x-raw, format=NV12, width=320, framerate=5/1"
#define ENCDEC "msdkh265enc rate-control=cqp ! msdkh265dec ! " \
               "msdkvpp ! video/x-raw, width=640"
#define TEXT "textoverlay font-desc=\"Arial Bold 48\" "

  data.pipeline =
      gst_parse_launch
      ("videomixer name=mix ! msdkvpp ! glimagesink sync=false "
      SRC " ! tee name=t ! queue ! " TEXT " text=\"non-ROI\" ! " ENCDEC
      " ! videobox left=-640 ! mix. "
      " t. ! queue name=roi ! " TEXT " text=\"ROI\" ! " ENCDEC
      " ! videobox ! mix.", &err);

  if (err) {
    g_printerr ("failed to parse pipeline: %s\n", err->message);
    g_error_free (err);
    return -1;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  gst_bus_enable_sync_message_emission (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (cb_msg_error), &data);
  g_signal_connect (bus, "message::eos", G_CALLBACK (cb_msg_eos), &data);
  g_signal_connect (bus, "message::element", G_CALLBACK (cb_msg), &data);
  gst_object_unref (bus);

  el = gst_bin_get_by_name (GST_BIN (data.pipeline), "roi");
  pad = gst_element_get_static_pad (el, "src");
  gst_object_unref (el);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, cb_add_roi, &data, NULL);
  gst_object_unref (pad);

  /* Add a keyboard watch so we get notified of keystrokes */
  io_stdin = g_io_channel_unix_new (fileno (stdin));
  g_io_add_watch (io_stdin, G_IO_IN, handle_keyboard, &data);

  /* Start playing */
  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Create a GLib Main Loop and set it to run */
  data.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.loop);

  /* Free resources */
  g_main_loop_unref (data.loop);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  g_io_channel_unref (io_stdin);

  return 0;
}
