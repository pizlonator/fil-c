/* GStreamer unit tests for discoverer
 *
 * Copyright (C) 2011 Stefan Kost <ensonic@users.sf.net>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/pbutils/pbutils.h>

#include <stdio.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

static gboolean have_theora, have_ogg;

GST_START_TEST (test_disco_init)
{
  GError *err = NULL;
  GstDiscoverer *dc;

  dc = gst_discoverer_new (GST_SECOND, &err);
  fail_unless (dc != NULL);
  fail_unless (err == NULL);

  g_object_unref (dc);
}

GST_END_TEST;

GST_START_TEST (test_disco_serializing)
{
  GError *err = NULL;
  GstDiscoverer *dc;
  GstDiscovererInfo *info, *dinfo;
  gchar *uri;
  GVariant *serialized, *reserialized;
  GList *audio_streams;
  gchar *path =
      g_build_filename (GST_TEST_FILES_PATH, "theora-vorbis.ogg", NULL);

  /* high timeout, in case we're running under valgrind */
  dc = gst_discoverer_new (30 * GST_SECOND, &err);
  fail_unless (dc != NULL);
  fail_unless (err == NULL);

  uri = gst_filename_to_uri (path, &err);
  g_free (path);
  fail_unless (err == NULL);

  info = gst_discoverer_discover_uri (dc, uri, &err);
  fail_unless (info);
  if (have_theora && have_ogg) {
    fail_unless_equals_int (gst_discoverer_info_get_result (info),
        GST_DISCOVERER_OK);
  } else {
    fail_unless_equals_int (gst_discoverer_info_get_result (info),
        GST_DISCOVERER_MISSING_PLUGINS);
    g_clear_error (&err);
    goto missing_plugins;
  }
  serialized =
      gst_discoverer_info_to_variant (info, GST_DISCOVERER_SERIALIZE_ALL);


  fail_unless (serialized);
  dinfo = gst_discoverer_info_from_variant (serialized);

  fail_unless (dinfo);
  audio_streams = gst_discoverer_info_get_audio_streams (dinfo);
  fail_unless_equals_int (g_list_length (audio_streams), 1);
  gst_discoverer_stream_info_list_free (audio_streams);

  reserialized =
      gst_discoverer_info_to_variant (dinfo, GST_DISCOVERER_SERIALIZE_ALL);

  fail_unless (g_variant_equal (serialized, reserialized));

  gst_discoverer_info_unref (dinfo);
  g_variant_unref (serialized);
  g_variant_unref (reserialized);

missing_plugins:

  gst_discoverer_info_unref (info);
  g_object_unref (dc);
  g_free (uri);
}

GST_END_TEST;

GST_START_TEST (test_disco_sync)
{
  GError *err = NULL;
  GstDiscoverer *dc;
  GstDiscovererInfo *info;
  GstDiscovererResult result;
  gchar *uri;

  /* high timeout, in case we're running under valgrind */
  dc = gst_discoverer_new (30 * GST_SECOND, &err);
  fail_unless (dc != NULL);
  fail_unless (err == NULL);

  /* GST_TEST_FILE comes from makefile CFLAGS */
  GST_INFO ("discovering file '%s'", GST_TEST_FILE);
  uri = g_filename_to_uri (GST_TEST_FILE, NULL, &err);
  fail_unless (err == NULL);
  GST_INFO ("discovering uri '%s'", uri);

  info = gst_discoverer_discover_uri (dc, uri, &err);
  result = gst_discoverer_info_get_result (info);
  GST_INFO ("result: %d", result);
  gst_discoverer_info_unref (info);
  g_free (uri);

  if (err) {
    /* we won't have the codec for the jpeg */
    g_error_free (err);
  }

  g_object_unref (dc);
}

GST_END_TEST;
static void
test_disco_sync_reuse (const gchar * test_fn, guint num, GstClockTime timeout)
{
  GError *err = NULL;
  GstDiscoverer *dc;
  GstDiscovererInfo *info;
  GstDiscovererResult result;
  gchar *uri, *path;
  int i;

  dc = gst_discoverer_new (timeout, &err);
  fail_unless (dc != NULL);
  fail_unless (err == NULL);

  /* GST_TEST_FILE comes from makefile CFLAGS */
  path = g_build_filename (GST_TEST_FILES_PATH, test_fn, NULL);
  uri = gst_filename_to_uri (path, &err);
  g_free (path);
  fail_unless (err == NULL);

  for (i = 0; i < num; ++i) {
    GST_INFO ("[%02d] discovering uri '%s'", i, uri);
    info = gst_discoverer_discover_uri (dc, uri, &err);
    if (info) {
      result = gst_discoverer_info_get_result (info);
      GST_INFO ("result: %d", result);
      gst_discoverer_info_unref (info);
    }
    /* in case we don't have some of the elements needed */
    if (err) {
      g_error_free (err);
      err = NULL;
    }
  }
  g_free (uri);

  g_object_unref (dc);
}

GST_START_TEST (test_disco_sync_reuse_ogg)
{
  test_disco_sync_reuse ("theora-vorbis.ogg", 2, 10 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_disco_sync_reuse_mp3)
{
  /* this will cause errors because -base doesn't do mp3 parsing or decoding */
  test_disco_sync_reuse ("test.mp3", 3, 10 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_disco_sync_reuse_timeout)
{
  /* set minimum timeout to test that, esp. leakage under valgrind */
  /* FIXME: should really be even shorter */
  test_disco_sync_reuse ("theora-vorbis.ogg", 2, GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_disco_missing_plugins)
{
  const gchar *files[] = { "test.mkv", "test.mp3", "partialframe.mjpeg" };
  GError *err = NULL;
  GstDiscoverer *dc;
  GstDiscovererInfo *info;
  GstDiscovererResult result;
  gchar *uri, *path;
  int i;

  for (i = 0; i < G_N_ELEMENTS (files); ++i) {
    dc = gst_discoverer_new (5 * GST_SECOND, &err);
    fail_unless (dc != NULL);
    fail_unless (err == NULL);

    /* GST_TEST_FILE comes from makefile CFLAGS */
    path = g_build_filename (GST_TEST_FILES_PATH, files[i], NULL);
    uri = gst_filename_to_uri (path, &err);
    g_free (path);
    fail_unless (err == NULL);

    GST_INFO ("discovering uri '%s'", uri);
    info = gst_discoverer_discover_uri (dc, uri, &err);
    fail_unless (info != NULL);
    fail_unless (err != NULL);
    result = gst_discoverer_info_get_result (info);
    GST_INFO ("result: %d, error message: %s", result, err->message);
    fail_unless_equals_int (result, GST_DISCOVERER_MISSING_PLUGINS);
#ifndef GST_DISABLE_DEPRECATED
    GST_INFO ("misc: %" GST_PTR_FORMAT, gst_discoverer_info_get_misc (info));
#endif

    gst_discoverer_info_unref (info);
    g_error_free (err);
    err = NULL;
    g_free (uri);
    g_object_unref (dc);
  }
}

GST_END_TEST;

typedef struct _AsyncTestData
{
  gchar *uri;
  GMainLoop *loop;
  GstDiscovererResult result;
} AsyncTestData;

static void
discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err, AsyncTestData * data)
{
  const gchar *uri = gst_discoverer_info_get_uri (info);

  fail_unless_equals_string (data->uri, uri);

  /* cannot ensure GST_DISCOVERER_OK since there might be missing plugins */
  data->result = gst_discoverer_info_get_result (info);

  g_main_loop_quit (data->loop);
}

static void
test_disco_async_with_context (GMainContext * context)
{
  GstDiscoverer *dc;
  GError *err = NULL;
  AsyncTestData data = { 0, };
  gchar *path =
      g_build_filename (GST_TEST_FILES_PATH, "theora-vorbis.ogg", NULL);

  if (context)
    g_main_context_push_thread_default (context);

  data.uri = gst_filename_to_uri (path, &err);
  /* something wrong if we have error here */
  fail_unless (err == NULL);
  g_free (path);

  data.loop = g_main_loop_new (context, FALSE);

  /* high timeout, in case we're running under valgrind */
  dc = gst_discoverer_new (30 * GST_SECOND, &err);
  fail_unless (dc != NULL);
  fail_unless (err == NULL);

  g_signal_connect (dc, "discovered", G_CALLBACK (discovered_cb), &data);

  gst_discoverer_start (dc);
  fail_unless (gst_discoverer_discover_uri_async (dc, data.uri) == TRUE);

  g_main_loop_run (data.loop);

  if (have_theora && have_ogg) {
    fail_unless_equals_int (data.result, GST_DISCOVERER_OK);
  } else {
    fail_unless_equals_int (data.result, GST_DISCOVERER_MISSING_PLUGINS);
  }

  gst_discoverer_stop (dc);
  g_object_unref (dc);
  g_free (data.uri);

  g_main_loop_unref (data.loop);

  if (context)
    g_main_context_pop_thread_default (context);
}

GST_START_TEST (test_disco_async)
{
  /* use default GMainContext */
  test_disco_async_with_context (NULL);
}

GST_END_TEST;

typedef struct _CustomContextData
{
  GMutex lock;
  GCond cond;
  gboolean finish;
} CustomContextData;


static gpointer
custom_context_thread_func (CustomContextData * data)
{
  GMainContext *context;

  /* test async APIs with custom GMainContext */
  context = g_main_context_new ();
  test_disco_async_with_context (context);
  g_main_context_unref (context);

  data->finish = TRUE;
  g_cond_signal (&data->cond);

  return NULL;
}

GST_START_TEST (test_disco_async_custom_context)
{
  GThread *thread;
  CustomContextData data;

  g_mutex_init (&data.lock);
  g_cond_init (&data.cond);
  data.finish = FALSE;

  /* ensure default context here, but we will use other thread default context
   * instead of this */
  g_main_context_default ();

  thread = g_thread_new ("test-custom-context-thread",
      (GThreadFunc) custom_context_thread_func, &data);

  g_mutex_lock (&data.lock);
  while (!data.finish)
    g_cond_wait (&data.cond, &data.lock);
  g_mutex_unlock (&data.lock);

  g_thread_join (thread);
  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);
}

GST_END_TEST;

static Suite *
discoverer_suite (void)
{
  Suite *s = suite_create ("discoverer");
  TCase *tc_chain = tcase_create ("general");

  have_theora = gst_registry_check_feature_version (gst_registry_get (),
      "theoradec", GST_VERSION_MAJOR, GST_VERSION_MINOR, 0);
  have_ogg = gst_registry_check_feature_version (gst_registry_get (),
      "oggdemux", GST_VERSION_MAJOR, GST_VERSION_MINOR, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_disco_init);
  tcase_add_test (tc_chain, test_disco_sync);
  tcase_add_test (tc_chain, test_disco_sync_reuse_ogg);
  tcase_add_test (tc_chain, test_disco_sync_reuse_mp3);
  tcase_add_test (tc_chain, test_disco_sync_reuse_timeout);
  tcase_add_test (tc_chain, test_disco_missing_plugins);
  tcase_add_test (tc_chain, test_disco_serializing);
  tcase_add_test (tc_chain, test_disco_async);
  tcase_add_test (tc_chain, test_disco_async_custom_context);
  return s;
}

GST_CHECK_MAIN (discoverer);
