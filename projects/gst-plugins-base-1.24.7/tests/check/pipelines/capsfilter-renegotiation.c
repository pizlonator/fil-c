/* GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 *
 * capsfilter-renegotiation.c: Unit test for capsfilter caps renegotiation
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

/* Ideally this would be in core, but using videotestsrc makes it easier */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>

#define FIRST_CAPS  "video/x-raw,width=(int)480,height=(int)320"
#define SECOND_CAPS "video/x-raw,width=(int)120,height=(int)100"
#define THIRD_CAPS  "video/x-raw,width=(int)[10,50],height=(int)[100,200]"
#define FOURTH_CAPS "video/x-raw,width=(int)300,height=(int)[25,75];" \
                    "video/x-raw,width=(int)[30,40]," \
                    "height=(int)[100,200],format=(string)YUY2"

int buffer_count = 0;
GstCaps *current_caps = NULL;
int caps_change = 0;

static GstPadProbeReturn
buffer_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstCaps *pad_caps;
  GstElement *capsfilter = GST_ELEMENT (data);
  GstCaps *caps = NULL;

  /* increment the buffer count and check if it is time to change the caps */
  buffer_count++;
  if (buffer_count == 50) {
    /* change the caps to another one */
    caps = gst_caps_from_string (SECOND_CAPS);
  } else if (buffer_count == 100) {
    /* change the caps to another one, this time unfixed */
    caps = gst_caps_from_string (THIRD_CAPS);
  } else if (buffer_count == 150) {
    /* change the caps to another one,
     * this time unfixed with multiple entries */
    caps = gst_caps_from_string (FOURTH_CAPS);
  }
  /* set the caps */
  if (caps) {
    g_object_set (capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
  }
  /* now check if the pad caps has changed since last check */
  pad_caps = gst_pad_get_current_caps (pad);
  if (current_caps == NULL && pad_caps != NULL) {
    /* probably the first caps, this is a change */
    current_caps = gst_caps_copy (pad_caps);
    caps_change++;
  } else if (current_caps != NULL) {
    if (pad_caps == NULL) {
      /* caps was set to NULL, we consider this a change */
      gst_caps_unref (current_caps);
      current_caps = NULL;
      caps_change++;
    } else {
      if (!gst_caps_is_equal (current_caps, pad_caps)) {
        /* a caps change */
        gst_caps_unref (current_caps);
        current_caps = gst_caps_copy (pad_caps);
        caps_change++;
      }
    }
  }
  gst_caps_unref (pad_caps);

  return TRUE;
}

/* launch line is a pipeline that must have a capsfilter named 'cf' that
 * will be used to trigger the renegotiation */
static void
run_capsfilter_renegotiation (const gchar * launch_line)
{
  GstElement *capsfilter;
  GstElement *sink;
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;
  GstPad *pad;

  caps_change = 0;
  buffer_count = 0;
  if (current_caps)
    gst_caps_unref (current_caps);
  current_caps = NULL;

  pipeline = gst_parse_launch (launch_line, NULL);
  fail_unless (pipeline != NULL);

  capsfilter = gst_bin_get_by_name (GST_BIN (pipeline), "cf");
  fail_unless (capsfilter != NULL);

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  fail_unless (sink != NULL);

  pad = gst_element_get_static_pad (sink, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, buffer_probe, capsfilter,
      NULL);
  gst_object_unref (pad);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  fail_unless (gst_element_set_state (pipeline, GST_STATE_PLAYING) !=
      GST_STATE_CHANGE_FAILURE);

  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_EOS | GST_MESSAGE_ERROR);

  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_EOS);
  fail_unless_equals_int (caps_change, 4);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  if (current_caps)
    gst_caps_unref (current_caps);
  current_caps = NULL;
  gst_message_unref (msg);
  g_object_unref (bus);
  gst_object_unref (sink);
  gst_object_unref (capsfilter);
  g_object_unref (G_OBJECT (pipeline));
}

GST_START_TEST (test_capsfilter_renegotiation)
{
  run_capsfilter_renegotiation ("videotestsrc num-buffers=200 "
      " ! capsfilter caps=\"" FIRST_CAPS "\" name=cf ! fakesink name=sink");
  run_capsfilter_renegotiation ("videotestsrc num-buffers=200 "
      " ! capsfilter caps=\"" FIRST_CAPS "\" name=cf ! fakesink name=sink");
  run_capsfilter_renegotiation ("videotestsrc num-buffers=200 "
      " ! capsfilter caps=\"video/x-raw, format=(string)I420, width=(int)100, height=(int)100\" "
      " ! videoconvert ! videoscale ! capsfilter caps=\"" FIRST_CAPS
      "\" name=cf " " ! fakesink name=sink");
}

GST_END_TEST;

static Suite *
capsfilter_renegotiation_suite (void)
{
  Suite *s = suite_create ("CapsfilterRenegotiation");
  TCase *tc_chain = tcase_create ("linear");

  /* time out after 60s, not the default 3 */
  tcase_set_timeout (tc_chain, 60);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_capsfilter_renegotiation);
  return s;
}

GST_CHECK_MAIN (capsfilter_renegotiation);
