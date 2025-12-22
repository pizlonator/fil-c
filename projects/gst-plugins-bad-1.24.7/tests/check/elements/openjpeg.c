/* GStreamer
 *
 * Copyright (c) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (c) 2010 David Schleef <ds@schleef.org>
 * Copyright (c) 2014 Thijs Vermeir <thijs.vermeir@barco.com>
 * Copyright (c) 2021 Stéphane Cerveau <scerveau@collabora.com>
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

#include <gst/check/gstcheck.h>

static GstStaticPadTemplate enc_sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-j2c, "
        "width = (int) [16, MAX], "
        "height = (int) [16, MAX], " "framerate = (fraction) [0, MAX]"));

static GstStaticPadTemplate enc_srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) I420, "
        "width = (int) [16, MAX], "
        "height = (int) [16, MAX], " "framerate = (fraction) [0, MAX]"));

#define MAX_THREADS 8
#define NUM_BUFFERS 4
#define FRAME_RATE 1000

static GstPad *sinkpad, *srcpad;

typedef struct _OpenJPEGData
{
  GMainLoop *loop;
  gboolean failing_pipeline;
} OpenJPEGData;

static GstElement *
setup_openjpegenc (const gchar * src_caps_str, gint num_stripes)
{
  GstElement *openjpegenc;
  GstCaps *srccaps = NULL;
  GstBus *bus;

  if (src_caps_str) {
    srccaps = gst_caps_from_string (src_caps_str);
    fail_unless (srccaps != NULL);
  }

  openjpegenc = gst_check_setup_element ("openjpegenc");
  fail_unless (openjpegenc != NULL);
  g_object_set (openjpegenc, "num-stripes", num_stripes, NULL);
  srcpad = gst_check_setup_src_pad (openjpegenc, &enc_srctemplate);
  sinkpad = gst_check_setup_sink_pad (openjpegenc, &enc_sinktemplate);
  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  gst_check_setup_events (srcpad, openjpegenc, srccaps, GST_FORMAT_TIME);

  bus = gst_bus_new ();
  gst_element_set_bus (openjpegenc, bus);

  fail_unless (gst_element_set_state (openjpegenc,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
      "could not set to playing");

  if (srccaps)
    gst_caps_unref (srccaps);

  buffers = NULL;
  return openjpegenc;
}

static void
cleanup_openjpegenc (GstElement * openjpegenc)
{
  GstBus *bus;

  /* Free parsed buffers */
  gst_check_drop_buffers ();

  bus = GST_ELEMENT_BUS (openjpegenc);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_check_teardown_src_pad (openjpegenc);
  gst_check_teardown_sink_pad (openjpegenc);
  gst_check_teardown_element (openjpegenc);
}

GST_START_TEST (test_openjpeg_encode_simple)
{
  GstElement *openjpegenc;
  GstBuffer *buffer;
  gint i;
  GList *l;
  GstCaps *outcaps, *sinkcaps;
  GstSegment seg;

  openjpegenc =
      setup_openjpegenc
      ("video/x-raw,format=(string)I420,width=(int)320,height=(int)240,framerate=(fraction)25/1",
      1);

  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.stop = gst_util_uint64_scale (10, GST_SECOND, 25);

  fail_unless (gst_pad_push_event (srcpad, gst_event_new_segment (&seg)));

  buffer = gst_buffer_new_allocate (NULL, 320 * 240 + 2 * 160 * 120, NULL);
  gst_buffer_memset (buffer, 0, 0, -1);

  for (i = 0; i < 10; i++) {
    GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (i, GST_SECOND, 25);
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
    fail_unless (gst_pad_push (srcpad, gst_buffer_ref (buffer)) == GST_FLOW_OK);
  }

  gst_buffer_unref (buffer);

  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));

  /* All buffers must be there now */
  fail_unless_equals_int (g_list_length (buffers), 10);

  outcaps =
      gst_caps_from_string
      ("image/x-j2c,width=(int)320,height=(int)240,framerate=(fraction)25/1");

  for (l = buffers, i = 0; l; l = l->next, i++) {
    buffer = l->data;

    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
        gst_util_uint64_scale (1, GST_SECOND, 25));

    sinkcaps = gst_pad_get_current_caps (sinkpad);
    fail_unless (gst_caps_can_intersect (sinkcaps, outcaps));
    gst_caps_unref (sinkcaps);
  }

  gst_caps_unref (outcaps);

  cleanup_openjpegenc (openjpegenc);
}

GST_END_TEST;

static gboolean
bus_cb (GstBus * bus, GstMessage * message, gpointer data)
{
  OpenJPEGData *opj_data = (OpenJPEGData *) data;
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);

      GST_ERROR ("Error: %s : %s", err->message, debug);
      g_error_free (err);
      g_free (debug);
      fail_if (!opj_data->failing_pipeline, "failed");
      g_main_loop_quit (opj_data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      fail_if (opj_data->failing_pipeline, "failed");
      g_main_loop_quit (opj_data->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

static void
run_openjpeg_pipeline (const gchar * in_format, gint width, gint height,
    gint num_stripes, gint enc_threads, gint dec_threads,
    gboolean failing_pipeline)
{
  GstBus *bus;
  OpenJPEGData opj_data;
  GstElement *pipeline;
  gchar *pipeline_str =
      g_strdup_printf ("videotestsrc num-buffers=%d ! "
      "video/x-raw,format=%s, width=%d, height=%d, framerate=%d/1 ! openjpegenc num-stripes=%d num-threads=%d ! jpeg2000parse"
      " ! openjpegdec max-threads=%d ! fakevideosink",
      NUM_BUFFERS, in_format, width, height, FRAME_RATE, num_stripes,
      enc_threads, dec_threads);
  GST_LOG ("Running pipeline: %s", pipeline_str);
  pipeline = gst_parse_launch (pipeline_str, NULL);
  fail_unless (pipeline != NULL);
  g_free (pipeline_str);

  opj_data.loop = g_main_loop_new (NULL, FALSE);
  opj_data.failing_pipeline = failing_pipeline;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, (GstBusFunc) bus_cb, &opj_data);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (opj_data.loop);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  g_main_loop_unref (opj_data.loop);
}


GST_START_TEST (test_openjpeg_simple)
{
  int i;
  const gchar *in_format_list[] = {
    "ARGB64", "ARGB", "xRGB", "AYUV64", "Y444_10LE", "I422_10LE", "I420_10LE",
    "AYUV", "Y444", "Y42B", "Y41B", "YUV9", "I420", "GRAY8", "GRAY16_LE"
  };

  for (i = 0; i < G_N_ELEMENTS (in_format_list); i++) {
    run_openjpeg_pipeline (in_format_list[i], 320, 200, 1, 1, 1, FALSE);
  }

  /* Check that the pipeline is failing properly */
  run_openjpeg_pipeline (in_format_list[0], 16, 16, 1, 0, 0, TRUE);
  run_openjpeg_pipeline (in_format_list[0], 16, 16, 1, 1, 1, TRUE);

  for (i = 1; i < 8; i++) {
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 0, 0, FALSE);
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 1, 0, FALSE);
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 0, 1, FALSE);
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 0, 4, FALSE);
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 5, 3, FALSE);
    run_openjpeg_pipeline (in_format_list[0], 320, 200, i, 8, 8, FALSE);
  }
}

GST_END_TEST;


static Suite *
openjpeg_suite (void)
{
  Suite *s = suite_create ("openjpeg");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_openjpeg_encode_simple);
  tcase_add_test (tc_chain, test_openjpeg_simple);
  tcase_set_timeout (tc_chain, 5 * 60);
  return s;
}

GST_CHECK_MAIN (openjpeg);
