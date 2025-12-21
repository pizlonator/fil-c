/* GStreamer
 * Copyright (C) 2012-2016 Matthew Waters <ystreet00@gmail.com>
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

#include <gst/check/gstcheck.h>
#include <gst/gl/gstglconfig.h>

#ifndef GST_DISABLE_PARSE

static GstElement *
setup_pipeline (const gchar * pipe_descr)
{
  GstElement *pipeline;

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
    GstMessageType events, GstMessageType tevent, GstState target_state)
{
  GstBus *bus;
  GstMessage *message;
  GstMessageType revent;
  GstStateChangeReturn ret;

  g_assert (pipe);
  bus = gst_element_get_bus (pipe);
  g_assert (bus);

  fail_if (gst_element_set_state (pipe, target_state) ==
      GST_STATE_CHANGE_FAILURE, "Could not set pipeline %s to playing", descr);
  ret = gst_element_get_state (pipe, NULL, NULL, 10 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_ASYNC) {
    g_critical ("Pipeline '%s' failed to go to PAUSED fast enough", descr);
    goto done;
  } else if ((ret != GST_STATE_CHANGE_SUCCESS)
      && (ret != GST_STATE_CHANGE_NO_PREROLL)) {
    g_critical ("Pipeline '%s' failed to go into PAUSED state (%s)", descr,
        gst_element_state_change_return_get_name (ret));
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

GST_START_TEST (test_glimagesink)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glimagesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
GST_START_TEST (test_glfiltercube)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glfiltercube ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
#define N_EFFECTS 18
GST_START_TEST (test_gleffects)
{
  gchar *s;
  GstState target_state = GST_STATE_PLAYING;
  guint i;

  for (i = 0; i < N_EFFECTS; i++) {
    s = g_strdup_printf ("videotestsrc num-buffers=10 ! glupload ! "
        "gleffects effect=%i ! fakesink", i);
    run_pipeline (setup_pipeline (s), s,
        GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
        GST_MESSAGE_UNKNOWN, target_state);
    g_free (s);
  }
}

GST_END_TEST
#undef N_EFFECTS
GST_START_TEST (test_glshader)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glshader ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! glshader ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
GST_START_TEST (test_glfilterapp)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glfilterapp ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! glfilterapp ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
GST_START_TEST (test_glmosaic)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glmosaic ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! glmosaic ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
GST_START_TEST (test_gloverlay)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! gloverlay ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! gloverlay ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
#define N_SRCS 13
GST_START_TEST (test_gltestsrc)
{
  gchar *s;
  GstState target_state = GST_STATE_PLAYING;
  guint i;

  for (i = 0; i < N_SRCS; i++) {
    s = g_strdup_printf ("gltestsrc pattern=%i num-buffers=10 ! fakesink", i);
    run_pipeline (setup_pipeline (s), s,
        GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
        GST_MESSAGE_UNKNOWN, target_state);
    g_free (s);
  }
}

GST_END_TEST
#undef N_SRCS
#if GST_GL_HAVE_OPENGL
GST_START_TEST (test_glfilterglass)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glfilterglass ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! glfilterglass ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
#if 0
GST_START_TEST (test_glfilterreflectedscreen)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! glfilterreflectedscreen ! "
      "fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! glfilterreflectedscreen ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
#endif
GST_START_TEST (test_gldeinterlace)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! gldeinterlace ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! gldeinterlace ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
GST_START_TEST (test_gldifferencematte)
{
  const gchar *s;
  GstState target_state = GST_STATE_PLAYING;

  s = "videotestsrc num-buffers=10 ! glupload ! gldifferencematte ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);

  s = "gltestsrc num-buffers=10 ! gldifferencematte ! fakesink";
  run_pipeline (setup_pipeline (s), s,
      GST_MESSAGE_ANY & ~(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING),
      GST_MESSAGE_UNKNOWN, target_state);
}

GST_END_TEST
#endif /* GST_GL_HAVE_OPENGL */
#endif /* !GST_DISABLE_PARSE */
static Suite *
gl_launch_lines_suite (void)
{
#if GST_GL_HAVE_OPENGL
  gboolean have_gldifferencematte;
#endif
  gboolean have_gloverlay;

  Suite *s = suite_create ("OpenGL pipelines");
  TCase *tc_chain = tcase_create ("linear");

  /* time out after 60s, not the default 3 */
  tcase_set_timeout (tc_chain, 60);

#if GST_GL_HAVE_OPENGL
  have_gldifferencematte =
      gst_registry_check_feature_version (gst_registry_get (),
      "gldifferencematte", GST_VERSION_MAJOR, GST_VERSION_MINOR, 0);
#endif
  have_gloverlay =
      gst_registry_check_feature_version (gst_registry_get (),
      "gloverlay", GST_VERSION_MAJOR, GST_VERSION_MINOR, 0);

  suite_add_tcase (s, tc_chain);
#ifndef GST_DISABLE_PARSE
  tcase_add_test (tc_chain, test_glimagesink);
  tcase_add_test (tc_chain, test_glfiltercube);
  tcase_add_test (tc_chain, test_gleffects);
  tcase_add_test (tc_chain, test_glshader);
  tcase_add_test (tc_chain, test_glfilterapp);
  tcase_add_test (tc_chain, test_glmosaic);
  if (have_gloverlay) {
    tcase_add_test (tc_chain, test_gloverlay);
  }
  tcase_add_test (tc_chain, test_gltestsrc);

#if GST_GL_HAVE_OPENGL
  tcase_add_test (tc_chain, test_glfilterglass);
/*  tcase_add_test (tc_chain, test_glfilterreflectedscreen);*/
  tcase_add_test (tc_chain, test_gldeinterlace);

  if (have_gldifferencematte) {
    tcase_add_test (tc_chain, test_gldifferencematte);
  }
/*  tcase_add_test (tc_chain, test_glbumper);*/
#endif /* GST_GL_HAVE_OPENGL */
#endif /* !GST_DISABLE_PARSE */
  return s;
}

GST_CHECK_MAIN (gl_launch_lines);
