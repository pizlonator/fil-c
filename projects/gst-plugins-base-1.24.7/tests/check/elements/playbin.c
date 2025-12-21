/* GStreamer unit tests for playbin
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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

/* FIXME 0.11: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/base/gstpushsrc.h>

#ifndef GST_DISABLE_REGISTRY

static GType gst_red_video_src_get_type (void);
static GType gst_codec_src_get_type (void);

GST_START_TEST (test_uri)
{
  GstElement *playbin, *fakesink;
  gchar *uri;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");

  g_object_set (playbin, "video-sink", fakesink, NULL);

  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_get (playbin, "uri", &uri, NULL);

  fail_unless_equals_string (uri, "redvideo://");
  g_free (uri);

  g_object_get (playbin, "current-uri", &uri, NULL);
  fail_unless_equals_string (uri, NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  g_object_get (playbin, "uri", &uri, NULL);
  fail_unless_equals_string (uri, NULL);
  g_object_get (playbin, "current-uri", &uri, NULL);
  fail_unless_equals_string (uri, "redvideo://");
  g_free (uri);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

/* make sure the audio sink is not touched for video-only streams */
GST_START_TEST (test_sink_usage_video_only_stream)
{
  GstElement *playbin, *fakevideosink, *fakeaudiosink;
  GstState cur_state, pending_state;
  GstElement *source;
  GstSample *last_sample;
  gint nstreams;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakevideosink = gst_element_factory_make ("fakesink", "fakevideosink");
  fail_unless (fakevideosink != NULL, "Failed to create fakevideosink element");

  fakeaudiosink = gst_element_factory_make ("fakesink", "fakeaudiosink");
  fail_unless (fakeaudiosink != NULL, "Failed to create fakeaudiosink element");

  /* video-only stream, audiosink will error out in ready => paused if used */
  g_object_set (fakeaudiosink, "state-error", 2, NULL);

  g_object_set (playbin, "video-sink", fakevideosink, NULL);
  g_object_set (playbin, "audio-sink", fakeaudiosink, NULL);

  g_object_set (playbin, "uri", "redvideo://", NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (gst_element_get_state (fakeaudiosink, &cur_state,
          &pending_state, 0), GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (cur_state, GST_STATE_NULL);
  fail_unless_equals_int (pending_state, GST_STATE_VOID_PENDING);

  g_object_get (playbin, "n-video", &nstreams, NULL);
  fail_unless_equals_int (nstreams, 1);

  g_object_get (playbin, "n-audio", &nstreams, NULL);
  fail_unless_equals_int (nstreams, 0);

  g_object_get (playbin, "n-text", &nstreams, NULL);
  fail_unless_equals_int (nstreams, 0);

  g_object_get (playbin, "source", &source, NULL);
  fail_unless (G_TYPE_FROM_INSTANCE (source) == gst_red_video_src_get_type ());
  gst_object_unref (source);

  g_object_get (playbin, "sample", &last_sample, NULL);
  fail_unless (GST_IS_SAMPLE (last_sample));
  gst_sample_unref (last_sample);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

/* this tests async error handling when setting up the subbin */
GST_START_TEST (test_suburi_error_unknowntype)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* suburi file format unknown: playbin should just ignore the suburi and
   * preroll normally (if /dev/zero does not exist, this test should behave
   * the same as test_suburi_error_invalidfile() */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "file:///dev/zero", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

GST_START_TEST (test_suburi_error_invalidfile)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* suburi file does not exist: playbin should just ignore the suburi and
   * preroll normally */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "file:///foo/bar/803129999/32x9ax1", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

GST_START_TEST (test_suburi_error_wrongproto)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* wrong protocol for suburi: playbin should just ignore the suburi and
   * preroll normally */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "nosuchproto://foo.bar:80", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

static GstElement *
create_playbin (const gchar * uri)
{
  GstElement *playbin, *fakesink1, *fakesink2;

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink1 = gst_element_factory_make ("fakesink", NULL);
  fail_unless (fakesink1 != NULL, "Failed to create fakesink element #1");

  fakesink2 = gst_element_factory_make ("fakesink", NULL);
  fail_unless (fakesink2 != NULL, "Failed to create fakesink element #2");

  /* make them behave like normal sinks, even if not needed for the test */
  g_object_set (fakesink1, "sync", TRUE, NULL);
  g_object_set (fakesink2, "sync", TRUE, NULL);

  g_object_set (playbin, "video-sink", fakesink1, NULL);
  g_object_set (playbin, "audio-sink", fakesink2, NULL);

  g_object_set (playbin, "uri", uri, NULL);

  return playbin;
}

GST_START_TEST (test_missing_urisource_handler)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;

  playbin = create_playbin ("chocchipcookie://withahint.of/cinnamon");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  /* there should be at least a missing-plugin message on the bus now and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  s = (GstStructure *) gst_message_get_structure (msg);
  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"),
      "chocchipcookie");
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a CORE MISSING_PLUGIN one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
      "%s instead of core-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
      "code %u instead of GST_CORE_ERROR_MISSING_PLUGIN", err->code);
  g_error_free (err);
  gst_message_unref (msg);
  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

GST_START_TEST (test_missing_suburisource_handler)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;

  playbin = create_playbin ("file:///does/not/exis.t");

  g_object_set (playbin, "suburi", "cookie://withahint.of/cinnamon", NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  gst_element_set_state (playbin, GST_STATE_PAUSED);

  /* there should be at least a missing-plugin message on the bus now and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  s = (GstStructure *) gst_message_get_structure (msg);
  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "cookie");
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_WARNING, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_WARNING);

  /* make sure the *warning* is a CORE MISSING_PLUGIN one */
  gst_message_parse_warning (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
      "%s instead of core-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
      "code %u instead of GST_CORE_ERROR_MISSING_PLUGIN", err->code);
  g_error_free (err);
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a RESOURCE NOT_FOUND one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_RESOURCE_ERROR,
      "error has wrong error domain " "%s instead of resource-error-quark",
      g_quark_to_string (err->domain));
  fail_unless (err->code == GST_RESOURCE_ERROR_NOT_FOUND,
      "error has wrong " "code %u instead of GST_RESOURCE_ERROR_NOT_FOUND",
      err->code);
  g_error_free (err);
  gst_message_unref (msg);

  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

GST_START_TEST (test_missing_primary_decoder)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;

  fail_unless (gst_element_register (NULL, "codecsrc", GST_RANK_PRIMARY,
          gst_codec_src_get_type ()));

  playbin = create_playbin ("codec://");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* there should soon be at least a missing-plugin message on the bus and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  s = (GstStructure *) gst_message_get_structure (msg);
  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "decoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_WARNING, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_WARNING);

  /* make sure the *warning* is a STREAM CODEC_NOT_FOUND one */
  gst_message_parse_warning (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_STREAM_ERROR, "error has wrong error domain "
      "%s instead of stream-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND, "error has wrong "
      "code %u instead of GST_STREAM_ERROR_CODEC_NOT_FOUND", err->code);
  g_error_free (err);
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a CORE MISSING_PLUGIN one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
      "%s instead of core-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
      "code %u instead of GST_CORE_ERROR_MISSING_PLUGIN", err->code);
  g_error_free (err);
  gst_message_unref (msg);

  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

GST_END_TEST;

GST_START_TEST (test_refcount)
{
  GstElement *playbin, *audiosink, *videosink, *vis;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", NULL);
  audiosink = gst_element_factory_make ("fakesink", "myaudiosink");
  videosink = gst_element_factory_make ("fakesink", "myvideosink");
  vis = gst_element_factory_make ("identity", "myvis");

  /* ref because we need them after we unref playbin */
  gst_object_ref (audiosink);
  gst_object_ref (videosink);
  gst_object_ref (vis);

  /* Sinks have floating ref only, setting the properties takes ownership. */
  g_object_set (playbin,
      "audio-sink", audiosink,
      "video-sink", videosink,
      "vis-plugin", vis, "flags", 0x01 | 0x02 | 0x08, NULL);

  g_object_set (playbin, "uri", "redvideo://", NULL);
  //"uri", "file:///home/wim/data/cdda.ogg", NULL);

  ASSERT_OBJECT_REFCOUNT (playbin, "playbin", 1);

  /* we have 3 refs now, one from ourselves, one from playbin and one from playsink */
  ASSERT_OBJECT_REFCOUNT (audiosink, "myaudiosink", 3);
  ASSERT_OBJECT_REFCOUNT (videosink, "myvideosink", 3);
  ASSERT_OBJECT_REFCOUNT (vis, "myvis", 2);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  ASSERT_OBJECT_REFCOUNT (playbin, "playbin", 1);
  /* refcount of our elements is undefined, playbin might keep additional refs
   * because it cached the elements in bins */
  gst_object_unref (playbin);

  /* now we are back to our refs */
  ASSERT_OBJECT_REFCOUNT (audiosink, "myaudiosink", 1);
  ASSERT_OBJECT_REFCOUNT (videosink, "myvideosink", 1);
  ASSERT_OBJECT_REFCOUNT (vis, "myvis", 1);

  gst_object_unref (audiosink);
  gst_object_unref (videosink);
  gst_object_unref (vis);
}

GST_END_TEST;

static void
source_setup (GstElement * playbin, GstElement * source, GstElement ** p_src)
{
  GST_LOG ("source-setup called, source = %s", G_OBJECT_TYPE_NAME (source));
  *p_src = gst_object_ref (source);
  GST_LOG ("here");
}

GST_START_TEST (test_source_setup)
{
  GstElement *playbin, *videosink;
  GstElement *src = NULL;

  if (!gst_registry_check_feature_version (gst_registry_get (), "redvideosrc",
          GST_VERSION_MAJOR, GST_VERSION_MINOR, 0)) {
    fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
            gst_red_video_src_get_type ()));
  }

  playbin = gst_element_factory_make ("playbin", NULL);
  g_object_set (playbin, "uri", "redvideo://", NULL);

  videosink = gst_element_factory_make ("fakesink", "myvideosink");
  g_object_set (playbin, "video-sink", videosink, NULL);

  g_signal_connect (playbin, "source-setup", G_CALLBACK (source_setup), &src);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  fail_unless (src != NULL);
  fail_unless (G_OBJECT_TYPE (src) == gst_red_video_src_get_type ());

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (playbin);
  gst_object_unref (src);
}

GST_END_TEST;

static void
element_setup (GstElement * playbin, GstElement * element, GQueue * elts)
{
  GstElementFactory *f = gst_element_get_factory (element);

  g_queue_push_tail (elts, f ? GST_OBJECT_NAME (f) : GST_OBJECT_NAME (element));
}

GST_START_TEST (test_element_setup)
{
  GstElement *playbin, *videosink;
  GQueue elts = G_QUEUE_INIT;

  if (!gst_registry_check_feature_version (gst_registry_get (), "redvideosrc",
          GST_VERSION_MAJOR, GST_VERSION_MINOR, 0)) {
    fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
            gst_red_video_src_get_type ()));
  }

  playbin = gst_element_factory_make ("playbin", NULL);
  g_object_set (playbin, "uri", "redvideo://", NULL);

  videosink = gst_element_factory_make ("fakesink", "myvideosink");
  g_object_set (playbin, "video-sink", videosink, NULL);

  g_signal_connect (playbin, "element-setup", G_CALLBACK (element_setup),
      &elts);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

#define seen_element(e) g_queue_find_custom(&elts, e, (GCompareFunc) strcmp)

  fail_unless (seen_element ("redvideosrc"));
  fail_unless (seen_element ("uridecodebin"));
  fail_unless (seen_element ("videoconvert"));
  fail_unless (seen_element ("videoscale"));
  fail_unless (seen_element ("fakesink"));

#undef seen_element

  g_queue_clear (&elts);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (playbin);
}

GST_END_TEST;

/*** redvideo:// source ***/

static GstURIType
gst_red_video_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_red_video_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "redvideo", NULL };

  return protocols;
}

static gchar *
gst_red_video_src_uri_get_uri (GstURIHandler * handler)
{
  return g_strdup ("redvideo://");
}

static gboolean
gst_red_video_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  return (uri != NULL && g_str_has_prefix (uri, "redvideo:"));
}

static void
gst_red_video_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_red_video_src_uri_get_type;
  iface->get_protocols = gst_red_video_src_uri_get_protocols;
  iface->get_uri = gst_red_video_src_uri_get_uri;
  iface->set_uri = gst_red_video_src_uri_set_uri;
}

static void
gst_red_video_src_init_type (GType type)
{
  static const GInterfaceInfo uri_hdlr_info = {
    gst_red_video_src_uri_handler_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_hdlr_info);
}

typedef GstPushSrc GstRedVideoSrc;
typedef GstPushSrcClass GstRedVideoSrcClass;

G_DEFINE_TYPE_WITH_CODE (GstRedVideoSrc, gst_red_video_src,
    GST_TYPE_PUSH_SRC, gst_red_video_src_init_type (g_define_type_id));

static GstFlowReturn
gst_red_video_src_create (GstPushSrc * src, GstBuffer ** p_buf)
{
  GstBuffer *buf;
  GstMapInfo map;
  guint w = 64, h = 64;
  guint size;

  size = w * h * 3 / 2;
  buf = gst_buffer_new_and_alloc (size);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  memset (map.data, 76, w * h);
  memset (map.data + (w * h), 85, (w * h) / 4);
  memset (map.data + (w * h) + ((w * h) / 4), 255, (w * h) / 4);
  gst_buffer_unmap (buf, &map);

  *p_buf = buf;
  return GST_FLOW_OK;
}

static GstCaps *
gst_red_video_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  guint w = 64, h = 64;
  return gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING,
      "I420", "width", G_TYPE_INT, w, "height",
      G_TYPE_INT, h, "framerate", GST_TYPE_FRACTION, 1, 1, NULL);
}

static void
gst_red_video_src_class_init (GstRedVideoSrcClass * klass)
{
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw, format=(string)I420")
      );
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_set_metadata (element_class,
      "Red Video Src", "Source/Video", "yep", "me");

  pushsrc_class->create = gst_red_video_src_create;
  basesrc_class->get_caps = gst_red_video_src_get_caps;
}

static void
gst_red_video_src_init (GstRedVideoSrc * src)
{
}

/*** codec:// source ***/

static GstURIType
gst_codec_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_codec_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "codec", NULL };

  return protocols;
}

static gchar *
gst_codec_src_uri_get_uri (GstURIHandler * handler)
{
  return g_strdup ("codec://");
}

static gboolean
gst_codec_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  return (uri != NULL && g_str_has_prefix (uri, "codec:"));
}

static void
gst_codec_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_codec_src_uri_get_type;
  iface->get_protocols = gst_codec_src_uri_get_protocols;
  iface->get_uri = gst_codec_src_uri_get_uri;
  iface->set_uri = gst_codec_src_uri_set_uri;
}

static void
gst_codec_src_init_type (GType type)
{
  static const GInterfaceInfo uri_hdlr_info = {
    gst_codec_src_uri_handler_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_hdlr_info);
}

#undef parent_class
#define parent_class codec_src_parent_class

typedef GstPushSrc GstCodecSrc;
typedef GstPushSrcClass GstCodecSrcClass;

G_DEFINE_TYPE_WITH_CODE (GstCodecSrc, gst_codec_src,
    GST_TYPE_PUSH_SRC, gst_codec_src_init_type (g_define_type_id));

static GstFlowReturn
gst_codec_src_create (GstPushSrc * src, GstBuffer ** p_buf)
{
  GstBuffer *buf;

  buf = gst_buffer_new_and_alloc (20);
  gst_buffer_memset (buf, 0, 0, 20);

  *p_buf = buf;
  return GST_FLOW_OK;
}

static GstCaps *
gst_codec_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  return gst_caps_new_empty_simple ("application/x-codec");
}

static void
gst_codec_src_class_init (GstCodecSrcClass * klass)
{
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("application/x-codec")
      );
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_set_metadata (element_class,
      "Codec Src", "Source/Video", "yep", "me");

  pushsrc_class->create = gst_codec_src_create;
  basesrc_class->get_caps = gst_codec_src_get_caps;
}

static void
gst_codec_src_init (GstCodecSrc * src)
{
}

#if 0
GST_START_TEST (test_appsink_twice)
{
  GstElement *playbin, *appsink;
  GstSample *sample;
  gchar *fn, *uri;
  int flags;

  fn = g_build_filename (GST_TEST_FILES_PATH, "theora-vorbis.ogg", NULL);
  uri = gst_filename_to_uri (fn, NULL);
  g_free (fn);

  playbin = gst_element_factory_make ("playbin", NULL);
  g_object_set (playbin, "uri", uri, NULL);
  g_free (uri);

  /* disable video decoding/rendering (doesn't actually work yet though) */
  g_object_get (playbin, "flags", &flags, NULL);
  g_object_set (playbin, "flags", flags & ~1, NULL);

  appsink = gst_element_factory_make ("appsink", "appsink");
  g_object_set (playbin, "audio-sink", appsink, NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  do {
    g_signal_emit_by_name (appsink, "pull-sample", &sample);
    GST_LOG ("got sample: %p", sample);
    if (sample)
      gst_sample_unref (sample);
  }
  while (sample != NULL);

  GST_INFO ("got first EOS");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  do {
    g_signal_emit_by_name (appsink, "pull-sample", &sample);
    GST_LOG ("got sample: %p", sample);
    if (sample)
      gst_sample_unref (sample);
  }
  while (sample != NULL);

  GST_INFO ("got second EOS");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (playbin);
}

GST_END_TEST;
#endif

#endif /* GST_DISABLE_REGISTRY */


static Suite *
playbin_suite (void)
{
  Suite *s = suite_create ("playbin");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

#ifndef GST_DISABLE_REGISTRY
  tcase_add_test (tc_chain, test_uri);
  tcase_add_test (tc_chain, test_sink_usage_video_only_stream);
  tcase_add_test (tc_chain, test_suburi_error_wrongproto);
  tcase_add_test (tc_chain, test_suburi_error_invalidfile);
  tcase_add_test (tc_chain, test_suburi_error_unknowntype);
  tcase_add_test (tc_chain, test_missing_urisource_handler);
  tcase_add_test (tc_chain, test_missing_suburisource_handler);
  tcase_add_test (tc_chain, test_missing_primary_decoder);
  tcase_add_test (tc_chain, test_refcount);
  tcase_add_test (tc_chain, test_source_setup);
  tcase_add_test (tc_chain, test_element_setup);

#if 0
  {
    GstRegistry *reg = gst_registry_get ();

    if (gst_registry_check_feature_version (reg, "oggdemux", 1, 0, 0) &&
        gst_registry_check_feature_version (reg, "theoradec", 1, 0, 0) &&
        gst_registry_check_feature_version (reg, "vorbisdec", 1, 0, 0)) {
      tcase_add_test (tc_chain, test_appsink_twice);
    }
  }
#endif

  /* one day we might also want to have the following checks:
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_one_fatal);
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_two_fatal);
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_two_with_preroll);
   */
#endif

  return s;
}

GST_CHECK_MAIN (playbin);
