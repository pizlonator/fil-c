/* GStreamer unit test for gstprofile
 *
 * Copyright (C) <2009> Edward Hervey <edward.hervey@collabora.co.uk>
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

#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/check/gstcheck.h>

/* Helper functions to create profiles */

static GstEncodingProfile *
create_ogg_profile ()
{
  GstEncodingContainerProfile *prof;
  GstCaps *ogg;

  ogg = gst_caps_new_empty_simple ("application/ogg");
  prof = gst_encoding_container_profile_new ((gchar *) "myprofile", NULL, ogg,
      NULL);
  gst_caps_unref (ogg);
  return (GstEncodingProfile *) prof;
}

static GstEncodingProfile *
create_ogg_vorbis_profile (guint presence, gchar * preset)
{
  GstEncodingContainerProfile *cprof;
  GstCaps *ogg, *vorbis;

  ogg = gst_caps_new_empty_simple ("application/ogg");
  cprof =
      gst_encoding_container_profile_new ((gchar *) "myprofile", NULL, ogg,
      NULL);
  gst_caps_unref (ogg);

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  fail_unless (gst_encoding_container_profile_add_profile (cprof,
          (GstEncodingProfile *) gst_encoding_audio_profile_new (vorbis, preset,
              NULL, presence)));
  gst_caps_unref (vorbis);

  return (GstEncodingProfile *) cprof;
}

static GstEncodingProfile *
create_ogg_theora_vorbis_profile (guint theorapresence, guint vorbispresence)
{
  GstEncodingContainerProfile *prof;
  GstCaps *ogg, *vorbis, *theora;

  ogg = gst_caps_new_empty_simple ("application/ogg");
  prof =
      gst_encoding_container_profile_new ((gchar *) "myprofile", NULL, ogg,
      NULL);
  gst_caps_unref (ogg);

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  fail_unless (gst_encoding_container_profile_add_profile (prof,
          (GstEncodingProfile *) gst_encoding_audio_profile_new (vorbis, NULL,
              NULL, vorbispresence)));
  gst_caps_unref (vorbis);

  theora = gst_caps_new_empty_simple ("video/x-theora");
  fail_unless (gst_encoding_container_profile_add_profile (prof,
          (GstEncodingProfile *) gst_encoding_video_profile_new (theora, NULL,
              NULL, theorapresence)));
  gst_caps_unref (theora);

  return (GstEncodingProfile *) prof;
}

static GstEncodingProfile *
create_vorbis_only_profile (void)
{
  GstEncodingProfile *prof;
  GstCaps *vorbis;

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  prof =
      (GstEncodingProfile *) gst_encoding_audio_profile_new (vorbis, NULL, NULL,
      0);
  gst_caps_unref (vorbis);

  return prof;
}

static GstCaps *
create_unsupported_caps (void)
{
  return gst_caps_new_empty_simple ("audio/x-bogus");
}

static GstEncodingProfile *
create_unsupported_profile (void)
{
  GstEncodingProfile *prof;
  GstCaps *caps;

  caps = create_unsupported_caps ();
  prof =
      (GstEncodingProfile *) gst_encoding_audio_profile_new (caps, NULL, NULL,
      0);
  gst_caps_unref (caps);

  return prof;
}

static void
_caps_match (GstPad * sinkpad, const gchar * capsname)
{
  GstCaps *caps, *sinkcaps;
  gchar *name;

  caps = gst_caps_from_string (capsname);
  sinkcaps = gst_pad_query_caps (sinkpad, NULL);
  fail_unless (sinkcaps != NULL);
  name = gst_caps_to_string (sinkcaps);
  fail_unless (gst_caps_is_subset (sinkcaps, caps),
      "caps ('%s') are not a subset of ('%s')", name, capsname);
  g_free (name);
  gst_caps_unref (sinkcaps);
  gst_caps_unref (caps);
}

static void
set_profile (GstElement * ebin, GstEncodingProfile * prof)
{
  g_object_set (ebin, "profile", prof, NULL);
  gst_encoding_profile_unref (prof);
}

/* Tests */

GST_START_TEST (test_encodebin_set_profile)
{
  GstElement *ebin;
  GstEncodingProfile *prof, *prof2;

  /* Create an encodebin and check that it correctly changes states
   * according to whether a profile is set or not */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* Set a profile on encodebin... */
  prof = create_ogg_profile ();
  g_object_set (ebin, "profile", prof, NULL);

  /* ... and check the profile has been properly set */
  g_object_get (ebin, "profile", &prof2, NULL);

  fail_unless (gst_encoding_profile_is_equal (prof, prof2));

  gst_encoding_profile_unref (prof);
  gst_encoding_profile_unref (prof2);

  gst_object_unref (ebin);
};

GST_END_TEST;


GST_START_TEST (test_encodebin_can_go_to_ready_without_profile)
{
  GstElement *ebin;
  GstPad *srcpad;
  GstPad *target;

  /* Create an encodebin and check that it correctly changes states
   * according to whether a profile is set or not */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);

  /* At this point, the ghostpad has *NO* target */
  target = gst_ghost_pad_get_target (GST_GHOST_PAD (srcpad));
  fail_unless (target == NULL);
  gst_object_unref (srcpad);

  /* No profile,
   * switching to READY should succeed,
   * but switching to PAUSED should fail
   */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_can_go_to_paused_with_profile)
{
  GstElement *ebin;
  GstPad *srcpad;
  GstPad *target;

  /* Create an encodebin and check that it correctly changes states
   * according to whether a profile is set or not */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* Set a profile on encodebin... */
  set_profile (ebin, create_ogg_profile ());

  /* Make sure we can go to PAUSED */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* At this point, the source pad *HAS* a target */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  target = gst_ghost_pad_get_target (GST_GHOST_PAD (srcpad));
  fail_unless (target != NULL);
  gst_object_unref (target);
  gst_object_unref (srcpad);


  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;


GST_START_TEST (test_encodebin_sink_pads_static)
{
  GstElement *ebin;
  GstPad *srcpad, *sinkpad;

  /* Create an encodebin and check that it properly creates the sink pads
   * for a single-stream profile with fixed presence */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* streamprofile that has a forced presence of 1 */
  set_profile (ebin, create_ogg_vorbis_profile (1, NULL));

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Check if the audio sink pad was properly created */
  sinkpad = gst_element_get_static_pad (ebin, "audio_0");
  fail_unless (sinkpad != NULL);
  /* Check caps match */
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");
  gst_object_unref (sinkpad);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_preset)
{
  GstElement *ebin;
  GstEncodingProfile *prof;
  guint64 max_delay = 0;
  GstPreset *oggmuxpreset;

  /* Create an encodebin with a bogus preset and check it fails switching states */

  ebin = gst_element_factory_make ("encodebin", NULL);
  oggmuxpreset = GST_PRESET (gst_element_factory_make ("oggmux", NULL));

  /* We also set the name as the load_preset call will reset the element name to
   * what is described in the preset... which might not be very smart tbh */
  g_object_set (oggmuxpreset, "max-delay", (guint64) 12, "name",
      "testingoggmux", NULL);

  /* Give a name someone should never use outside of that test */
  gst_preset_save_preset (oggmuxpreset, "test_encodebin_preset");

  /* streamprofile that has a forced presence of 1 */
  prof = create_ogg_vorbis_profile (1, NULL);

  gst_encoding_profile_set_preset (prof, "test_encodebin_preset");
  gst_encoding_profile_set_preset_name (prof, "oggmux");

  set_profile (ebin, prof);

  /* It will go to READY... */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* ... and to PAUSED */
  fail_unless (gst_element_set_state (ebin, GST_STATE_PAUSED) !=
      GST_STATE_CHANGE_FAILURE);

  gst_child_proxy_get (GST_CHILD_PROXY (ebin), "testingoggmux::max-delay",
      &max_delay, NULL);
  fail_unless_equals_uint64 (max_delay, 12);

  gst_element_set_state (ebin, GST_STATE_NULL);
  gst_preset_delete_preset (oggmuxpreset, "test_encodebin_preset");

  gst_object_unref (oggmuxpreset);
  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_sink_pads_nopreset_static)
{
  GstElement *ebin;

  /* Create an encodebin with a bogus preset and check it fails switching states */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* streamprofile that has a forced presence of 1 */
  set_profile (ebin,
      create_ogg_vorbis_profile (1, (gchar *) "nowaythispresetexists"));

  /* It will go to READY... */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* ... but to not PAUSED */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  gst_element_set_state (ebin, GST_STATE_NULL);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_sink_pads_dynamic)
{
  GstElement *ebin;
  GstPad *srcpad, *sinkpad;
  GstCaps *sinkcaps;

  /* Create an encodebin and check that it properly creates the sink pads
   * for a single-stream profile with a unfixed presence */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* streamprofile that has non-forced presence */
  set_profile (ebin, create_ogg_vorbis_profile (0, NULL));

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Check if the audio sink pad can be requested */
  sinkpad = gst_element_request_pad_simple (ebin, "audio_0");
  fail_unless (sinkpad != NULL);
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");
  gst_element_release_request_pad (ebin, sinkpad);
  gst_object_unref (sinkpad);
  sinkpad = NULL;

  /* Check again with the 'request-pad' signal */
  sinkcaps = gst_caps_new_empty_simple ("audio/x-raw");
  g_signal_emit_by_name (ebin, "request-pad", sinkcaps, &sinkpad);
  gst_caps_unref (sinkcaps);
  fail_unless (sinkpad != NULL);
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");
  gst_element_release_request_pad (ebin, sinkpad);
  gst_object_unref (sinkpad);
  sinkpad = NULL;

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_sink_pads_multiple_static)
{
  GstElement *ebin;
  GstPad *srcpad, *sinkpadvorbis, *sinkpadtheora;

  /* Create an encodebin and check that it properly creates the sink pads */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* First try is with a streamprofile that has a forced presence of 1 */
  set_profile (ebin, create_ogg_theora_vorbis_profile (1, 1));

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Check if the audio sink pad was properly created */
  sinkpadvorbis = gst_element_get_static_pad (ebin, "audio_0");
  fail_unless (sinkpadvorbis != NULL);
  _caps_match (sinkpadvorbis, "audio/x-raw;audio/x-vorbis");
  gst_object_unref (sinkpadvorbis);

  /* Check if the video sink pad was properly created */
  sinkpadtheora = gst_element_get_static_pad (ebin, "video_1");
  fail_unless (sinkpadtheora != NULL);
  _caps_match (sinkpadtheora, "video/x-raw;video/x-theora");
  gst_object_unref (sinkpadtheora);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_sink_pads_multiple_dynamic)
{
  GstElement *ebin;
  GstPad *srcpad, *sinkpadvorbis, *sinkpadtheora;

  /* Create an encodebin and check that it properly creates the sink pads
   * for a multiple-stream with unfixed presence */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* multi-stream profile that has non-forced presence */
  set_profile (ebin, create_ogg_theora_vorbis_profile (0, 0));

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Check if the audio sink pad was properly created */
  sinkpadvorbis = gst_element_request_pad_simple (ebin, "audio_0");
  _caps_match (sinkpadvorbis, "audio/x-raw;audio/x-vorbis");
  fail_unless (sinkpadvorbis != NULL);

  /* Check if the video sink pad was properly created */
  sinkpadtheora = gst_element_request_pad_simple (ebin, "video_1");
  _caps_match (sinkpadtheora, "video/x-raw;video/x-theora");
  fail_unless (sinkpadtheora != NULL);

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_release_request_pad (GST_ELEMENT (ebin), sinkpadvorbis);
  gst_object_unref (sinkpadvorbis);
  gst_element_release_request_pad (GST_ELEMENT (ebin), sinkpadtheora);
  gst_object_unref (sinkpadtheora);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_sink_pads_dynamic_encoder)
{
  GstElement *ebin;
  GstPad *srcpad, *sinkpad = NULL;
  GstCaps *vorbiscaps;

  /* Create an encodebin and check that it properly creates the sink pads
   * for a single-stream profile with a unfixed presence */

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* streamprofile that has non-forced presence */
  set_profile (ebin, create_ogg_vorbis_profile (0, NULL));

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Check if the audio sink pad was properly created */
  vorbiscaps = gst_caps_from_string ("audio/x-vorbis,channels=2,rate=44100");
  g_signal_emit_by_name (ebin, "request-pad", vorbiscaps, &sinkpad);
  gst_caps_unref (vorbiscaps);
  fail_unless (sinkpad != NULL);
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");
  gst_element_release_request_pad (ebin, sinkpad);
  gst_object_unref (sinkpad);

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_render_audio_static)
{
  GstElement *ebin, *pipeline, *audiotestsrc, *fakesink;
  GstBus *bus;
  gboolean done = FALSE;

  /* Create an encodebin and render 5s of vorbis/ogg */

  pipeline = gst_pipeline_new ("encodebin-pipeline");
  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 10, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  set_profile (ebin, create_ogg_vorbis_profile (1, NULL));

  gst_bin_add_many ((GstBin *) pipeline, audiotestsrc, ebin, fakesink, NULL);

  fail_unless (gst_element_link_many (audiotestsrc, ebin, fakesink, NULL));

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  while (!done) {
    GstMessage *msg;

    /* poll the bus until we get EOS without any errors */
    msg = gst_bus_timed_pop (bus, GST_SECOND / 10);
    if (msg) {
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          fail ("GST_MESSAGE_ERROR");
          break;
        case GST_MESSAGE_EOS:
          done = TRUE;
          break;
        default:
          break;
      }
      gst_message_unref (msg);
    }
  }

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (bus);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_render_audio_only_static)
{
  GstElement *ebin, *pipeline, *audiotestsrc, *fakesink;
  GstBus *bus;
  gboolean done = FALSE;
  GstPad *sinkpad;
  GstCaps *sinkcaps;

  /* Create an encodebin and render 5s of vorbis only */
  pipeline = gst_pipeline_new ("encodebin-pipeline");
  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 10, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  set_profile (ebin, create_vorbis_only_profile ());

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, ebin, fakesink, NULL);

  GST_DEBUG ("linking encodebin");
  fail_unless (gst_element_link_many (audiotestsrc, ebin, fakesink, NULL));

  /* Requesting a new pad should fail */
  ASSERT_CRITICAL (gst_element_request_pad_simple (ebin, "audio_0"));

  sinkcaps = gst_caps_new_empty_simple ("audio/x-raw");
  g_signal_emit_by_name (ebin, "request-pad", sinkcaps, &sinkpad);
  gst_caps_unref (sinkcaps);
  fail_if (sinkpad != NULL);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  while (!done) {
    GstMessage *msg;

    /* poll the bus until we get EOS without any errors */
    msg = gst_bus_timed_pop (bus, GST_SECOND / 10);
    if (msg) {
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          fail ("GST_MESSAGE_ERROR");
          break;
        case GST_MESSAGE_EOS:
          done = TRUE;
          break;
        default:
          break;
      }
      gst_message_unref (msg);
    }
  }

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (bus);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_render_audio_dynamic)
{
  GstElement *ebin, *pipeline, *audiotestsrc, *fakesink;
  GstBus *bus;
  GstPad *sinkpad, *srcpad;
  gboolean done = FALSE;

  /* Create an encodebin and render 5s of vorbis/ogg */

  pipeline = gst_pipeline_new ("encodebin-pipeline");
  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 10, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  set_profile (ebin, create_ogg_vorbis_profile (0, NULL));

  gst_bin_add_many ((GstBin *) pipeline, audiotestsrc, ebin, fakesink, NULL);

  srcpad = gst_element_get_static_pad (audiotestsrc, "src");
  fail_unless (srcpad != NULL);

  sinkpad = gst_element_request_pad_simple (ebin, "audio_0");
  fail_unless (sinkpad != NULL);
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");

  fail_unless_equals_int (gst_pad_link (srcpad, sinkpad), GST_PAD_LINK_OK);

  gst_object_unref (srcpad);

  fail_unless (gst_element_link (ebin, fakesink));

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  while (!done) {
    GstMessage *msg;

    /* poll the bus until we get EOS without any errors */
    msg = gst_bus_timed_pop (bus, GST_SECOND / 10);
    if (msg) {
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          fail ("GST_MESSAGE_ERROR");
          break;
        case GST_MESSAGE_EOS:
          done = TRUE;
          break;
        default:
          break;
      }
      gst_message_unref (msg);
    }
  }

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_release_request_pad (GST_ELEMENT (ebin), sinkpad);
  gst_object_unref (sinkpad);

  gst_object_unref (bus);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_render_audio_video_static)
{
  GstElement *ebin, *pipeline, *audiotestsrc, *videotestsrc, *fakesink;
  GstBus *bus;
  gboolean done = FALSE;

  /* Create an encodebin and render 5s of vorbis/ogg */

  pipeline = gst_pipeline_new ("encodebin-pipeline");
  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 10, NULL);
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  g_object_set (videotestsrc, "num-buffers", 5, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  set_profile (ebin, create_ogg_theora_vorbis_profile (1, 1));

  gst_bin_add_many ((GstBin *) pipeline, audiotestsrc, videotestsrc, ebin,
      fakesink, NULL);

  fail_unless (gst_element_link (videotestsrc, ebin));
  fail_unless (gst_element_link_many (audiotestsrc, ebin, fakesink, NULL));

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  while (!done) {
    GstMessage *msg;

    /* poll the bus until we get EOS without any errors */
    msg = gst_bus_timed_pop (bus, GST_SECOND / 10);
    if (msg) {
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          fail ("GST_MESSAGE_ERROR");
          break;
        case GST_MESSAGE_EOS:
          done = TRUE;
          break;
        default:
          break;
      }
      gst_message_unref (msg);
    }
  }

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (bus);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_render_audio_video_dynamic)
{
  GstElement *ebin, *pipeline, *audiotestsrc, *videotestsrc, *fakesink;
  GstBus *bus;
  gboolean done = FALSE;
  GstPad *sinkpad1, *sinkpad2, *srcpad;

  /* Create an encodebin and render 5s of vorbis/ogg */

  pipeline = gst_pipeline_new ("encodebin-pipeline");
  bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 10, NULL);
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  g_object_set (videotestsrc, "num-buffers", 5, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  ebin = gst_element_factory_make ("encodebin", NULL);
  set_profile (ebin, create_ogg_theora_vorbis_profile (0, 0));

  gst_bin_add_many ((GstBin *) pipeline, audiotestsrc, videotestsrc, ebin,
      fakesink, NULL);

  fail_unless (gst_element_link (ebin, fakesink));

  srcpad = gst_element_get_static_pad (audiotestsrc, "src");
  sinkpad1 = gst_element_request_pad_simple (ebin, "audio_0");
  fail_unless (srcpad != NULL);
  fail_unless (sinkpad1 != NULL);
  _caps_match (sinkpad1, "audio/x-raw;audio/x-vorbis");
  fail_unless_equals_int (gst_pad_link (srcpad, sinkpad1), GST_PAD_LINK_OK);
  gst_object_unref (srcpad);

  srcpad = gst_element_get_static_pad (videotestsrc, "src");
  sinkpad2 = gst_element_request_pad_simple (ebin, "video_1");
  _caps_match (sinkpad2, "video/x-raw;video/x-theora");
  fail_unless_equals_int (gst_pad_link (srcpad, sinkpad2), GST_PAD_LINK_OK);
  gst_object_unref (srcpad);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  while (!done) {
    GstMessage *msg;

    /* poll the bus until we get EOS without any errors */
    msg = gst_bus_timed_pop (bus, GST_SECOND / 10);
    if (msg) {
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          fail ("GST_MESSAGE_ERROR");
          break;
        case GST_MESSAGE_EOS:
          done = TRUE;
          break;
        default:
          break;
      }
      gst_message_unref (msg);
    }
  }

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_release_request_pad (GST_ELEMENT (ebin), sinkpad1);
  gst_object_unref (sinkpad1);
  gst_element_release_request_pad (GST_ELEMENT (ebin), sinkpad2);
  gst_object_unref (sinkpad2);

  gst_object_unref (bus);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_impossible_element_combination)
{
  GstElement *ebin;
  GstEncodingProfile *prof;
  GstCaps *ogg, *x264;

  ebin = gst_element_factory_make ("x264enc", NULL);
  if (ebin == NULL) {
    GST_DEBUG ("No available h264 encoder, skipping test");
    return;
  }
  gst_object_unref (ebin);

  /* Make sure that impossible combinations of encoders and muxer
   * properly fail. In this case we try putting h264 in ogg.
   *
   * To properly test we abort early, we use a presence of zero for the
   * h264 stream profile. */

  ebin = gst_element_factory_make ("encodebin", NULL);

  ogg = gst_caps_new_empty_simple ("application/ogg");
  prof = (GstEncodingProfile *) gst_encoding_container_profile_new ((gchar *)
      "myprofile", NULL, ogg, NULL);
  gst_caps_unref (ogg);

  x264 = gst_caps_new_empty_simple ("video/x-h264");
  fail_unless (gst_encoding_container_profile_add_profile
      (GST_ENCODING_CONTAINER_PROFILE (prof),
          (GstEncodingProfile *) gst_encoding_video_profile_new (x264, NULL,
              NULL, 0)));
  gst_caps_unref (x264);

  g_object_set (ebin, "profile", prof, NULL);
  gst_encoding_profile_unref (prof);

  /* It will go to READY... */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* ... but to not PAUSED */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  gst_element_set_state (ebin, GST_STATE_NULL);

  gst_object_unref (ebin);
};

GST_END_TEST;

static void
_test_encodebin_reuse (GstEncodingProfile * prof1, GstEncodingProfile * prof2)
{
  GstElement *ebin;

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* Set a profile on encodebin... */
  if (prof1)
    g_object_set (ebin, "profile", prof1, NULL);

  /* Make sure we can go to PAUSED */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  if (prof2)
    g_object_set (ebin, "profile", prof2, NULL);

  /* Make sure we can go to PLAYING */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);
}

GST_START_TEST (test_encodebin_reuse)
{
  GstEncodingProfile *prof1;
  GstEncodingProfile *prof2;
  GstEncodingProfile *prof3;

  prof1 = create_ogg_profile ();
  prof2 = create_ogg_theora_vorbis_profile (1, 1);
  prof3 = create_vorbis_only_profile ();

  _test_encodebin_reuse (prof1, NULL);
  _test_encodebin_reuse (prof1, prof1);
  _test_encodebin_reuse (prof1, prof2);
  _test_encodebin_reuse (prof2, prof3);

  gst_encoding_profile_unref (prof1);
  gst_encoding_profile_unref (prof2);
  gst_encoding_profile_unref (prof3);
};

GST_END_TEST;

GST_START_TEST (test_encodebin_named_requests)
{
  GstElement *ebin;
  GstEncodingContainerProfile *cprof;
  GstCaps *ogg, *vorbis, *theora;
  GstEncodingProfile *vorbisprof, *theoraprof;
  GstPad *srcpad, *sinkpadvorbis, *sinkpadtheora;

  /* Create a profile with vorbis/theora named profile */
  ogg = gst_caps_new_empty_simple ("application/ogg");
  cprof =
      gst_encoding_container_profile_new ((gchar *) "myprofile", NULL, ogg,
      NULL);
  gst_caps_unref (ogg);

  vorbis = gst_caps_new_empty_simple ("audio/x-vorbis");
  vorbisprof =
      (GstEncodingProfile *) gst_encoding_audio_profile_new (vorbis, NULL, NULL,
      0);
  gst_encoding_profile_set_name (vorbisprof, "vorbisprofile");
  fail_unless (gst_encoding_container_profile_add_profile (cprof, vorbisprof));
  gst_caps_unref (vorbis);

  theora = gst_caps_new_empty_simple ("video/x-theora");
  theoraprof =
      (GstEncodingProfile *) gst_encoding_video_profile_new (theora, NULL, NULL,
      0);
  gst_encoding_profile_set_name (theoraprof, "theoraprofile");
  fail_unless (gst_encoding_container_profile_add_profile (cprof, theoraprof));
  gst_caps_unref (theora);

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* First try is with a streamprofile that has a forced presence of 1 */
  g_object_set (ebin, "profile", cprof, NULL);

  gst_encoding_profile_unref (cprof);

  /* Check if the source pad was properly created */
  srcpad = gst_element_get_static_pad (ebin, "src");
  fail_unless (srcpad != NULL);
  gst_object_unref (srcpad);

  /* Request a vorbis profile pad */
  g_signal_emit_by_name (ebin, "request-profile-pad", "vorbisprofile",
      &sinkpadvorbis);
  fail_unless (sinkpadvorbis != NULL);
  _caps_match (sinkpadvorbis, "audio/x-raw;audio/x-vorbis");
  gst_object_unref (sinkpadvorbis);

  /* Request a theora profile pad */
  g_signal_emit_by_name (ebin, "request-profile-pad", "theoraprofile",
      &sinkpadtheora);
  fail_unless (sinkpadtheora != NULL);
  _caps_match (sinkpadtheora, "video/x-raw;video/x-theora");
  gst_object_unref (sinkpadtheora);

  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_SUCCESS);

  /* Set back to NULL */
  fail_unless_equals_int (gst_element_set_state (ebin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (ebin);

}

GST_END_TEST;

GST_START_TEST (test_encodebin_missing_plugin_messages)
{
  GstElement *pipeline = gst_pipeline_new ("test");
  GstBus *bus = gst_pipeline_get_bus ((GstPipeline *) pipeline);
  GstElement *ebin = gst_element_factory_make ("encodebin", NULL);
  GstMessage *message;
  GstElement *audiotestsrc;
  GstPad *sinkpad, *srcpad;

  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "num-buffers", 1, NULL);
  gst_bin_add ((GstBin *) pipeline, audiotestsrc);

  /* first add to bin, then set profile */
  gst_bin_add ((GstBin *) pipeline, ebin);
  set_profile (ebin, create_unsupported_profile ());

  srcpad = gst_element_get_static_pad (audiotestsrc, "src");
  sinkpad = gst_element_get_static_pad (ebin, "audio_0");
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  message =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ELEMENT);
  fail_if (message == NULL);
  fail_if (!gst_is_missing_plugin_message (message));
  gst_message_unref (message);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_encodebin_fallback_profiles_on_failure)
{
  GstElement *ebin;
  GstPad *sinkpad = NULL;
  GstCaps *vorbiscaps;
  GstEncodingProfile *profile, *vorbis_profile;

  ebin = gst_element_factory_make ("encodebin", NULL);

  /* Create a ogg profile with a vorbis sub profile
   * that can't be 'instanciated' because its preset is set
   * to nowaythispresetexists. */
  profile = create_ogg_vorbis_profile (0, (gchar *) "nowaythispresetexists");
  vorbis_profile = create_vorbis_only_profile ();
  gst_encoding_container_profile_add_profile (GST_ENCODING_CONTAINER_PROFILE
      (profile), vorbis_profile);
  set_profile (ebin, profile);

  /* Check if the audio sink pad can be created, meaning
   * that the first profile with a 'nowaythispresetexists'
   * preset has been skipped. */
  vorbiscaps = gst_caps_from_string ("audio/x-vorbis");
  g_signal_emit_by_name (ebin, "request-pad", vorbiscaps, &sinkpad);
  _caps_match (sinkpad, "audio/x-raw;audio/x-vorbis");
  gst_element_release_request_pad (ebin, sinkpad);
  gst_object_unref (sinkpad);
  gst_caps_unref (vorbiscaps);

  gst_object_unref (ebin);
};

GST_END_TEST;

static Suite *
encodebin_suite (void)
{
  Suite *s = suite_create ("encodebin element");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_encodebin_set_profile);
  tcase_add_test (tc_chain, test_encodebin_can_go_to_ready_without_profile);
  tcase_add_test (tc_chain, test_encodebin_can_go_to_paused_with_profile);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_static);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_nopreset_static);
  tcase_add_test (tc_chain, test_encodebin_preset);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_dynamic);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_multiple_static);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_multiple_dynamic);
  tcase_add_test (tc_chain, test_encodebin_sink_pads_dynamic_encoder);
  tcase_add_test (tc_chain, test_encodebin_render_audio_static);
  tcase_add_test (tc_chain, test_encodebin_render_audio_only_static);
  tcase_add_test (tc_chain, test_encodebin_render_audio_dynamic);
  tcase_add_test (tc_chain, test_encodebin_render_audio_video_static);
  tcase_add_test (tc_chain, test_encodebin_render_audio_video_dynamic);
  tcase_add_test (tc_chain, test_encodebin_impossible_element_combination);
  tcase_add_test (tc_chain, test_encodebin_reuse);
  tcase_add_test (tc_chain, test_encodebin_named_requests);
  tcase_add_test (tc_chain, test_encodebin_missing_plugin_messages);
  tcase_add_test (tc_chain, test_encodebin_fallback_profiles_on_failure);

  return s;
}

GST_CHECK_MAIN (encodebin);
