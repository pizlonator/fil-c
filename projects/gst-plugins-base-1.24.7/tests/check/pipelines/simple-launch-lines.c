/* GStreamer
 * Copyright (C) 2005 Andy Wingo <wingo@pobox.com>
 *
 * simple_launch_lines.c: Unit test for simple pipelines
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
#include <gst/audio/audio-format.h>

#ifndef GST_DISABLE_PARSE

static GstElement *
setup_pipeline (const gchar * pipe_descr)
{
  GstElement *pipeline;

  GST_LOG ("pipeline: %s", pipe_descr);
  pipeline = gst_parse_launch (pipe_descr, NULL);
  g_return_val_if_fail (GST_IS_PIPELINE (pipeline), NULL);
  return pipeline;
}

/* 
 * run_pipeline:
 * @pipe: the pipeline to run
 * @desc: the description for use in messages
 * @events: is a mask of expected events
 * @tevent: is the expected terminal event.
 *
 * the poll call will time out after half a second.
 */
static void
run_pipeline (GstElement * pipe, const gchar * descr,
    GstMessageType events, GstMessageType tevent)
{
  GstBus *bus;
  GstMessage *message;
  GstMessageType revent;
  GstStateChangeReturn ret;

  g_assert (pipe);
  bus = gst_element_get_bus (pipe);
  g_assert (bus);

  fail_if (gst_element_set_state (pipe, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE, "Could not set pipeline %s to playing", descr);
  ret = gst_element_get_state (pipe, NULL, NULL, 10 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_ASYNC) {
    g_critical ("Pipeline '%s' failed to go to PLAYING fast enough", descr);
    goto done;
  } else if (ret != GST_STATE_CHANGE_SUCCESS) {
    g_critical ("Pipeline '%s' failed to go into PLAYING state", descr);
    goto done;
  }

  while (1) {
    message = gst_bus_poll (bus, GST_MESSAGE_ANY, GST_SECOND / 2);


    /* always have to pop the message before getting back into poll */
    if (message) {
      revent = GST_MESSAGE_TYPE (message);
      gst_message_unref (message);
    } else {
      revent = GST_MESSAGE_UNKNOWN;
    }

    if (revent == tevent) {
      break;
    } else if (revent == GST_MESSAGE_UNKNOWN) {
      g_critical ("Unexpected timeout in gst_bus_poll, looking for %d: %s",
          tevent, descr);
      break;
    } else if (revent & events) {
      continue;
    }
    g_critical
        ("Unexpected message received of type %d, '%s', looking for %d: %s",
        revent, gst_message_type_get_name (revent), tevent, descr);
  }

done:
  fail_if (gst_element_set_state (pipe, GST_STATE_NULL) ==
      GST_STATE_CHANGE_FAILURE, "Could not set pipeline %s to NULL", descr);
  gst_element_get_state (pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
  gst_object_unref (pipe);

  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);
}

GST_START_TEST (test_element_negotiation)
{
  const gchar *s;

  /* Ensures that filtering buffers with unknown caps down to fixed-caps 
   * will apply those caps to the buffers.
   * see http://bugzilla.gnome.org/show_bug.cgi?id=315126 */
  s = "fakesrc num-buffers=2 ! " "audio/x-raw,format=" GST_AUDIO_NE (S16)
      ",rate=22050,channels=1,layout=interleaved " "! audioconvert "
      "! audio/x-raw,format=" GST_AUDIO_NE (S16)
      ",rate=22050,channels=1,layout=interleaved " "! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);

  /* FIXME: don't use real audio/video outputs */
#if 0
  {
    gboolean have_libvisual_lv_scope;

    have_libvisual_lv_scope =
        gst_registry_check_feature_version (gst_registry_get (),
        "libvisual_lv_scope", GST_VERSION_MAJOR, GST_VERSION_MINOR, 0);

    if (have_libvisual_lv_scope) {
      s = "audiotestsrc num-buffers=30 ! tee name=t ! alsasink t. ! "
          "audioconvert ! libvisual_lv_scope ! videoconvert ! xvimagesink";
      run_pipeline (setup_pipeline (s), s,
          GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
          GST_MESSAGE_UNKNOWN);
    }
  }
#endif
}

GST_END_TEST;

GST_START_TEST (test_basetransform_based)
{
  /* Each of these tests is to check whether various basetransform based 
   * elements can select output caps when not allowed to do passthrough 
   * and going to a generic sink such as fakesink or filesink */
  const gchar *s;

  /* Check that videoscale can pick a height given only a width */
  s = "videotestsrc num-buffers=2 ! "
      "video/x-raw,format=(string)I420,width=320,height=240 ! "
      "videoscale ! video/x-raw,width=640 ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);

  /* Test that videoconvert can pick an output format that isn't
   * passthrough without completely specified output caps */
  s = "videotestsrc num-buffers=2 ! "
      "video/x-raw,format=(string)I420,width=320,height=240 ! "
      "videoconvert ! video/x-raw,format=(string)RGB ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);

  /* Check that audioresample can pick a samplerate to use from a
   * range that doesn't include the input */
  s = "audiotestsrc num-buffers=2 ! "
      "audio/x-raw,format=" GST_AUDIO_NE (S16) ",rate=8000 ! "
      "audioresample ! audio/x-raw,rate=[16000,48000] ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);

  /* Check that audioconvert can pick a depth to use, given a width */
  s = "audiotestsrc num-buffers=30 ! audio/x-raw,format=" GST_AUDIO_NE (S16)
      " ! audioconvert ! " "audio/x-raw,format=" GST_AUDIO_NE (S32)
      " ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);

  /* Check that videoscale doesn't claim to be able to transform input in
   * formats it can't handle for a given scaling method; videoconvert
   * should then make sure a format that can be handled is chosen (4-tap
   * scaling is not implemented for RGB and packed yuv currently) */
  s = "videotestsrc num-buffers=2 ! video/x-raw,format=(string)ARGB64 ! "
      "videoconvert ! videoscale method=4-tap ! videoconvert ! "
      "video/x-raw,format=(string)RGB, width=32,height=32,framerate=(fraction)30/1,"
      "pixel-aspect-ratio=(fraction)1/1 ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);
  s = "videotestsrc num-buffers=2 ! video/x-raw,format=(string)AYUV,"
      "width=64,height=64 ! videoconvert ! videoscale method=4-tap ! "
      "videoconvert ! video/x-raw,format=(string)AYUV,width=32,"
      "height=32 ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);
  /* make sure nothing funny happens in passthrough mode (we don't check that
   * passthrough mode is chosen though) */
  s = "videotestsrc num-buffers=2 ! video/x-raw,format=(string)I420,"
      "width=64,height=64 ! videoconvert ! videoscale method=4-tap ! "
      "videoconvert ! video/x-raw,format=(string)I420,width=32,"
      "height=32 ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN);
}

GST_END_TEST;

#endif /* #ifndef GST_DISABLE_PARSE */
static Suite *
simple_launch_lines_suite (void)
{
  Suite *s = suite_create ("Pipelines");
  TCase *tc_chain = tcase_create ("linear");

  /* time out after 60s, not the default 3 */
  tcase_set_timeout (tc_chain, 60);

  suite_add_tcase (s, tc_chain);
#ifndef GST_DISABLE_PARSE
  tcase_add_test (tc_chain, test_element_negotiation);
  tcase_add_test (tc_chain, test_basetransform_based);
#endif
  return s;
}

GST_CHECK_MAIN (simple_launch_lines);
