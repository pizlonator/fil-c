/* GStreamer
 *
 * unit test for audiotestsrc
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
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
#include <gst/check/gstharness.h>
#include <gst/audio/audio.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysinkpad;


#define CAPS_TEMPLATE_STRING            \
    "audio/x-raw, "                     \
    "format = (string) "GST_AUDIO_NE(S16)", "   \
    "channels = (int) 1, "              \
    "rate = (int) [ 1,  MAX ]"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_TEMPLATE_STRING)
    );

static GstElement *
setup_audiotestsrc (void)
{
  GstElement *audiotestsrc;

  GST_DEBUG ("setup_audiotestsrc");
  audiotestsrc = gst_check_setup_element ("audiotestsrc");
  mysinkpad = gst_check_setup_sink_pad (audiotestsrc, &sinktemplate);
  gst_pad_set_active (mysinkpad, TRUE);

  return audiotestsrc;
}

static void
cleanup_audiotestsrc (GstElement * audiotestsrc)
{
  GST_DEBUG ("cleanup_audiotestsrc");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_sink_pad (audiotestsrc);
  gst_check_teardown_element (audiotestsrc);
}

GST_START_TEST (test_all_waves)
{
  GstElement *audiotestsrc;
  GObjectClass *oclass;
  GParamSpec *property;
  GEnumValue *values;
  guint j = 0;

  audiotestsrc = setup_audiotestsrc ();
  oclass = G_OBJECT_GET_CLASS (audiotestsrc);
  property = g_object_class_find_property (oclass, "wave");
  fail_unless (G_IS_PARAM_SPEC_ENUM (property));
  values = G_ENUM_CLASS (g_type_class_ref (property->value_type))->values;


  while (values[j].value_name) {
    GST_DEBUG_OBJECT (audiotestsrc, "testing wave %s", values[j].value_name);
    g_object_set (audiotestsrc, "wave", values[j].value, NULL);

    fail_unless (gst_element_set_state (audiotestsrc,
            GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
        "could not set to playing");

    g_mutex_lock (&check_mutex);
    while (g_list_length (buffers) < 10)
      g_cond_wait (&check_cond, &check_mutex);
    g_mutex_unlock (&check_mutex);

    gst_element_set_state (audiotestsrc, GST_STATE_READY);

    g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
    g_list_free (buffers);
    buffers = NULL;
    ++j;
  }

  /* cleanup */
  cleanup_audiotestsrc (audiotestsrc);
}

GST_END_TEST;


#define TEST_LAYOUT_CHANNELS 6

static GstStaticPadTemplate sinktemplate_interleaved =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "channels = (int) " G_STRINGIFY (TEST_LAYOUT_CHANNELS) ", "
        "rate = (int) [ 1,  MAX ], layout = (string) interleaved")
    );

static GstStaticPadTemplate sinktemplate_planar =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "channels = (int) " G_STRINGIFY (TEST_LAYOUT_CHANNELS) ", "
        "rate = (int) [ 1,  MAX ], layout = (string) non-interleaved")
    );

typedef enum
{
  GST_AUDIO_TEST_SRC_WAVE_SINE,
  GST_AUDIO_TEST_SRC_WAVE_SQUARE,
  GST_AUDIO_TEST_SRC_WAVE_SAW,
  GST_AUDIO_TEST_SRC_WAVE_TRIANGLE,
  GST_AUDIO_TEST_SRC_WAVE_SILENCE,
  GST_AUDIO_TEST_SRC_WAVE_WHITE_NOISE,
  GST_AUDIO_TEST_SRC_WAVE_PINK_NOISE,
  GST_AUDIO_TEST_SRC_WAVE_SINE_TAB,
  GST_AUDIO_TEST_SRC_WAVE_TICKS,
  GST_AUDIO_TEST_SRC_WAVE_GAUSSIAN_WHITE_NOISE,
  GST_AUDIO_TEST_SRC_WAVE_RED_NOISE,
  GST_AUDIO_TEST_SRC_WAVE_BLUE_NOISE,
  GST_AUDIO_TEST_SRC_WAVE_VIOLET_NOISE,

  _GST_AUDIO_TEST_SRC_WAVE_LAST
} GstAudioTestSrcWave;

GST_START_TEST (test_layout)
{
  GstHarness *interleavedsrc, *plannarsrc;
  GObjectClass *oclass;
  GParamSpec *property;
  GEnumValue *values;
  guint i, j;

  interleavedsrc = gst_harness_new_with_templates ("audiotestsrc", NULL,
      &sinktemplate_interleaved);
  plannarsrc = gst_harness_new_with_templates ("audiotestsrc", NULL,
      &sinktemplate_planar);

  gst_harness_use_testclock (interleavedsrc);
  gst_harness_use_testclock (plannarsrc);
  g_object_set (interleavedsrc->element, "is-live", TRUE, NULL);
  g_object_set (plannarsrc->element, "is-live", TRUE, NULL);

  oclass = G_OBJECT_GET_CLASS (interleavedsrc->element);
  property = g_object_class_find_property (oclass, "wave");
  fail_unless (G_IS_PARAM_SPEC_ENUM (property));
  values = G_ENUM_CLASS (g_type_class_ref (property->value_type))->values;

  for (j = 0; values[j].value_name; j++) {
    /* these produce random values by definition,
     * so we can't compare channels */
    switch (j) {
      case GST_AUDIO_TEST_SRC_WAVE_WHITE_NOISE:
      case GST_AUDIO_TEST_SRC_WAVE_PINK_NOISE:
      case GST_AUDIO_TEST_SRC_WAVE_GAUSSIAN_WHITE_NOISE:
      case GST_AUDIO_TEST_SRC_WAVE_RED_NOISE:
      case GST_AUDIO_TEST_SRC_WAVE_BLUE_NOISE:
      case GST_AUDIO_TEST_SRC_WAVE_VIOLET_NOISE:
        continue;
      default:
        break;
    }

    GST_DEBUG ("layout test with wave %s", values[j].value_name);
    g_object_set (interleavedsrc->element, "wave", values[j].value, NULL);
    g_object_set (plannarsrc->element, "wave", values[j].value, NULL);

    if (j == 0) {
      GST_DEBUG ("gst_harness_play");
      gst_harness_play (interleavedsrc);
      gst_harness_play (plannarsrc);
    } else {
      GST_DEBUG ("discarding buffers with old wave");
      fail_unless (gst_harness_crank_single_clock_wait (interleavedsrc));
      fail_unless (gst_harness_crank_single_clock_wait (plannarsrc));
      gst_buffer_unref (gst_harness_pull (interleavedsrc));
      gst_buffer_unref (gst_harness_pull (plannarsrc));
    }

    for (i = 0; i < 10; i++) {
      GstBuffer *ibuf, *pbuf;
      GstMapInfo imap, pmap;
      GstAudioMeta *meta;
      GstAudioBuffer pabuf;
      gint16 *iptr, *pptr;
      guint isamples, psamples, s, c;

      GST_DEBUG ("waiting on clock");
      fail_unless (gst_harness_crank_single_clock_wait (interleavedsrc));
      fail_unless (gst_harness_crank_single_clock_wait (plannarsrc));

      ibuf = gst_harness_pull (interleavedsrc);
      pbuf = gst_harness_pull (plannarsrc);

      gst_buffer_map (ibuf, &imap, GST_MAP_READ);
      gst_buffer_map (pbuf, &pmap, GST_MAP_READ);

      /* buffers should have the same size in bytes and in samples */
      fail_unless_equals_int (imap.size, pmap.size);
      isamples = imap.size / TEST_LAYOUT_CHANNELS;
      isamples /= 2;            /* S16 -> 2 bytes per sample */
      fail_unless_equals_int (imap.size % TEST_LAYOUT_CHANNELS, 0);
      psamples = pmap.size / TEST_LAYOUT_CHANNELS;
      psamples /= 2;            /* S16 -> 2 bytes per sample */
      fail_unless_equals_int (pmap.size % TEST_LAYOUT_CHANNELS, 0);
      fail_unless_equals_int (isamples, psamples);

      iptr = (gint16 *) imap.data;
      pptr = (gint16 *) pmap.data;

      GST_DEBUG ("verifying contents of buffers; samples=%d, channels=%d",
          isamples, TEST_LAYOUT_CHANNELS);

      for (s = 0; s < isamples; s++) {
        for (c = 0; c < TEST_LAYOUT_CHANNELS; c++) {
          guint iidx = s * TEST_LAYOUT_CHANNELS + c;
          guint pidx = c * isamples + s;

          GST_TRACE ("s = %u | c = %u | iidx (s * channels + c) = %u | "
              "pidx (c * samples + s) = %u", s, c, iidx, pidx);

          fail_unless (iidx < imap.size / 2);
          fail_unless (pidx < pmap.size / 2);
          fail_unless_equals_int (iptr[iidx], pptr[pidx]);
        }
      }

      gst_buffer_unmap (pbuf, &pmap);

      GST_DEBUG ("verify that mapping through GstAudioBuffer works the same");

      meta = gst_buffer_get_audio_meta (pbuf);
      fail_unless (meta);

      gst_audio_buffer_map (&pabuf, &meta->info, pbuf, GST_MAP_READ);

      for (s = 0; s < isamples; s++) {
        for (c = 0; c < TEST_LAYOUT_CHANNELS; c++) {
          guint iidx = s * TEST_LAYOUT_CHANNELS + c;

          fail_unless_equals_int (iptr[iidx], ((gint16 *) pabuf.planes[c])[s]);
        }
      }

      gst_audio_buffer_unmap (&pabuf);
      gst_buffer_unmap (ibuf, &imap);

      gst_buffer_unref (ibuf);
      gst_buffer_unref (pbuf);
    }

    /* ensure the audiotestsrcs are not in fill() while we change the wave */
    fail_unless (gst_harness_wait_for_clock_id_waits (interleavedsrc, 1, 1));
    fail_unless (gst_harness_wait_for_clock_id_waits (plannarsrc, 1, 1));
  }

  /* make sure we ran the test */
  fail_unless_equals_int (j, _GST_AUDIO_TEST_SRC_WAVE_LAST);

  gst_harness_teardown (interleavedsrc);
  gst_harness_teardown (plannarsrc);
}

GST_END_TEST;

static Suite *
audiotestsrc_suite (void)
{
  Suite *s = suite_create ("audiotestsrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_all_waves);
  tcase_add_test (tc_chain, test_layout);

  return s;
}

GST_CHECK_MAIN (audiotestsrc);
