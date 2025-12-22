/* GStreamer unit test for HLS demux
 *
 * Copyright (c) <2015> YouView TV Ltd
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
#include "adaptive_demux_common.h"

#define DEMUX_ELEMENT_NAME "hlsdemux"

#define TS_PACKET_LEN 188

typedef struct _GstHlsDemuxTestInputData
{
  const gchar *uri;
  const guint8 *payload;
  guint64 size;
} GstHlsDemuxTestInputData;

typedef struct _GstHlsDemuxTestCase
{
  const GstHlsDemuxTestInputData *input;
  GstStructure *state;
} GstHlsDemuxTestCase;

typedef struct _GstHlsDemuxTestAppendUriContext
{
  GQuark field_id;
  const gchar *uri;
} GstHlsDemuxTestAppendUriContext;

typedef struct _GstHlsDemuxTestSelectBitrateContext
{
  GstAdaptiveDemuxTestEngine *engine;
  GstAdaptiveDemuxTestCase *testData;
  guint select_count;
  gulong signal_handle;
} GstHlsDemuxTestSelectBitrateContext;

static GByteArray *
generate_transport_stream (guint length)
{
  guint pos;
  guint cc = 0;
  GByteArray *mpeg_ts;

  fail_unless ((length % TS_PACKET_LEN) == 0);
  mpeg_ts = g_byte_array_sized_new (length);
  if (!mpeg_ts) {
    return NULL;
  }
  memset (mpeg_ts->data, 0xFF, length);
  for (pos = 0; pos < length; pos += TS_PACKET_LEN) {
    mpeg_ts->data[pos] = 0x47;
    mpeg_ts->data[pos + 1] = 0x1F;
    mpeg_ts->data[pos + 2] = 0xFF;
    mpeg_ts->data[pos + 3] = cc;
    cc = (cc + 1) & 0x0F;
  }
  return mpeg_ts;
}

static GByteArray *
setup_test_variables (const gchar * funcname,
    GstHlsDemuxTestInputData * inputTestData,
    GstAdaptiveDemuxTestExpectedOutput * outputTestData,
    GstHlsDemuxTestCase * hlsTestCase,
    GstAdaptiveDemuxTestCase * engineTestData, guint segment_size)
{
  GByteArray *mpeg_ts = NULL;

  if (segment_size) {
    guint itd, otd;

    mpeg_ts = generate_transport_stream ((segment_size));
    fail_unless (mpeg_ts != NULL);
    for (itd = 0; inputTestData[itd].uri; ++itd) {
      if (g_str_has_suffix (inputTestData[itd].uri, ".ts")) {
        inputTestData[itd].payload = mpeg_ts->data;
      }
    }
    for (otd = 0; outputTestData[otd].name; ++otd) {
      outputTestData[otd].expected_data = mpeg_ts->data;
      engineTestData->output_streams =
          g_list_append (engineTestData->output_streams, &outputTestData[otd]);
    }
  }
  hlsTestCase->input = inputTestData;
  hlsTestCase->state = gst_structure_new_empty (funcname);
  return mpeg_ts;
}

#define TESTCASE_INIT_BOILERPLATE(segment_size) \
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 }; \
  GstAdaptiveDemuxTestCallbacks engine_callbacks = { 0 }; \
  GstAdaptiveDemuxTestCase *engineTestData; \
  GstHlsDemuxTestCase hlsTestCase = { 0 }; \
  GByteArray *mpeg_ts=NULL; \
  engineTestData = gst_adaptive_demux_test_case_new(); \
  fail_unless (engineTestData!=NULL); \
  mpeg_ts = setup_test_variables(__FUNCTION__, inputTestData, outputTestData, \
                                 &hlsTestCase, engineTestData, segment_size); \

#define TESTCASE_UNREF_BOILERPLATE do{ \
  if(engineTestData->signal_context){ \
    g_slice_free (GstHlsDemuxTestSelectBitrateContext, engineTestData->signal_context);        \
  } \
  if(mpeg_ts) { g_byte_array_free (mpeg_ts, TRUE); }         \
  gst_structure_free (hlsTestCase.state); \
  g_object_unref (engineTestData); \
} while(0)

static gboolean
append_request_uri (GQuark field_id, GValue * value, gpointer user_data)
{
  GstHlsDemuxTestAppendUriContext *context =
      (GstHlsDemuxTestAppendUriContext *) user_data;
  GValue uri_val = G_VALUE_INIT;

  if (context->field_id == field_id) {
    g_value_init (&uri_val, G_TYPE_STRING);
    g_value_set_string (&uri_val, context->uri);
    gst_value_array_append_value (value, &uri_val);
    g_value_unset (&uri_val);
  }
  return TRUE;
}

static void
gst_hlsdemux_test_set_input_data (const GstHlsDemuxTestCase * test_case,
    const GstHlsDemuxTestInputData * input, GstTestHTTPSrcInput * output)
{
  output->size = input->size;
  output->context = (gpointer) input;
  if (output->size == 0) {
    output->size = strlen ((gchar *) input->payload);
  }
  fail_unless (input->uri != NULL);
  if (g_str_has_suffix (input->uri, ".m3u8")) {
    output->response_headers = gst_structure_new ("response-headers",
        "Content-Type", G_TYPE_STRING, "application/vnd.apple.mpegurl", NULL);
  } else if (g_str_has_suffix (input->uri, ".ts")) {
    output->response_headers = gst_structure_new ("response-headers",
        "Content-Type", G_TYPE_STRING, "video/mp2t", NULL);
  }
  if (gst_structure_has_field (test_case->state, "requests")) {
    GstHlsDemuxTestAppendUriContext context =
        { g_quark_from_string ("requests"), input->uri };
    gst_structure_map_in_place (test_case->state, append_request_uri, &context);
  } else {
    GValue requests = G_VALUE_INIT;
    GValue uri_val = G_VALUE_INIT;

    g_value_init (&requests, GST_TYPE_ARRAY);
    g_value_init (&uri_val, G_TYPE_STRING);
    g_value_set_string (&uri_val, input->uri);
    gst_value_array_append_value (&requests, &uri_val);
    gst_structure_set_value (test_case->state, "requests", &requests);
    g_value_unset (&uri_val);
    g_value_unset (&requests);
  }
}

static gboolean
gst_hlsdemux_test_src_start (GstTestHTTPSrc * src,
    const gchar * uri, GstTestHTTPSrcInput * input_data, gpointer user_data)
{
  const GstHlsDemuxTestCase *test_case =
      (const GstHlsDemuxTestCase *) user_data;
  guint fail_count = 0;
  guint i;

  GST_DEBUG ("src_start %s", uri);
  for (i = 0; test_case->input[i].uri; ++i) {
    if (strcmp (test_case->input[i].uri, uri) == 0) {
      gst_hlsdemux_test_set_input_data (test_case, &test_case->input[i],
          input_data);
      GST_DEBUG ("open URI %s", uri);
      return TRUE;
    }
  }
  gst_structure_get_uint (test_case->state, "failure-count", &fail_count);
  fail_count++;
  gst_structure_set (test_case->state, "failure-count", G_TYPE_UINT,
      fail_count, NULL);
  return FALSE;
}

static GstFlowReturn
gst_hlsdemux_test_src_create (GstTestHTTPSrc * src,
    guint64 offset,
    guint length, GstBuffer ** retbuf, gpointer context, gpointer user_data)
{
  GstBuffer *buf;
  /*  const GstHlsDemuxTestCase *test_case = (const GstHlsDemuxTestCase *) user_data; */
  GstHlsDemuxTestInputData *input = (GstHlsDemuxTestInputData *) context;

  buf = gst_buffer_new_allocate (NULL, length, NULL);
  fail_if (buf == NULL, "Not enough memory to allocate buffer");
  fail_if (input->payload == NULL);
  gst_buffer_fill (buf, 0, input->payload + offset, length);
  *retbuf = buf;
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_hlsdemux_test_network_error_src_create (GstTestHTTPSrc * src,
    guint64 offset,
    guint length, GstBuffer ** retbuf, gpointer context, gpointer user_data)
{
  const GstHlsDemuxTestCase *test_case =
      (const GstHlsDemuxTestCase *) user_data;
  GstHlsDemuxTestInputData *input = (GstHlsDemuxTestInputData *) context;
  const gchar *failure_suffix;
  guint64 failure_position = 0;

  fail_unless (test_case != NULL);
  fail_unless (input != NULL);
  fail_unless (input->uri != NULL);
  failure_suffix =
      gst_structure_get_string (test_case->state, "failure-suffix");
  if (!failure_suffix) {
    failure_suffix = ".ts";
  }
  if (!gst_structure_get_uint64 (test_case->state, "failure-position",
          &failure_position)) {
    failure_position = 10 * TS_PACKET_LEN;
  }
  GST_DEBUG ("network_error %s %s %" G_GUINT64_FORMAT " @ %" G_GUINT64_FORMAT,
      input->uri, failure_suffix, offset, failure_position);
  if (g_str_has_suffix (input->uri, failure_suffix)
      && offset >= failure_position) {
    GST_DEBUG ("return error");
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (("A network error occurred, or the server closed the connection unexpectedly.")), ("A network error occurred, or the server closed the connection unexpectedly."));
    *retbuf = NULL;
    return GST_FLOW_ERROR;
  }
  return gst_hlsdemux_test_src_create (src, offset, length, retbuf, context,
      user_data);
}

/******************** Test specific code starts here **************************/

/*
 * Test a media manifest with a single segment
 *
 */
GST_START_TEST (simpleTest)
{
  /* segment_size needs to larger than 2K, otherwise gsthlsdemux will
     not perform a typefind on the buffer */
  const guint segment_size = 30 * TS_PACKET_LEN;
  const gchar *manifest =
      "#EXTM3U \n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/media.m3u8", (guint8 *) manifest, 0},
    {"http://unit.test/001.ts", NULL, segment_size},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", segment_size, NULL},
    {NULL, 0, NULL}
  };
  TESTCASE_INIT_BOILERPLATE (segment_size);

  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;
  engine_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  engine_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      inputTestData[0].uri, &engine_callbacks, engineTestData);
  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

GST_START_TEST (testMasterPlaylist)
{
  const guint segment_size = 30 * TS_PACKET_LEN;
  const gchar *master_playlist =
      "#EXTM3U\n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f mp4a.40.2\", RESOLUTION=640x352\n"
      "1200.m3u8\n";
  const gchar *media_playlist =
      "#EXTM3U \n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/master.m3u8", (guint8 *) master_playlist, 0},
    {"http://unit.test/1200.m3u8", (guint8 *) media_playlist, 0},
    {"http://unit.test/001.ts", NULL, segment_size},
    {NULL, NULL, 0}
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", segment_size, NULL},
    {NULL, 0, NULL}
  };
  const GValue *requests;
  guint i;
  TESTCASE_INIT_BOILERPLATE (segment_size);

  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;
  engine_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  engine_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/master.m3u8", &engine_callbacks, engineTestData);

  requests = gst_structure_get_value (hlsTestCase.state, "requests");
  fail_unless (requests != NULL);
  assert_equals_uint64 (gst_value_array_get_size (requests),
      sizeof (inputTestData) / sizeof (inputTestData[0]) - 1);
  for (i = 0; inputTestData[i].uri; ++i) {
    const GValue *uri;
    uri = gst_value_array_get_value (requests, i);
    fail_unless (uri != NULL);
    assert_equals_string (inputTestData[i].uri, g_value_get_string (uri));
  }
  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

/*
 * Test seeking
 *
 */
GST_START_TEST (testSeek)
{
  const guint segment_size = 60 * TS_PACKET_LEN;
  const gchar *manifest =
      "#EXTM3U \n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/media.m3u8", (guint8 *) manifest, 0},
    {"http://unit.test/001.ts", NULL, segment_size},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", segment_size, NULL},
    {NULL, 0, NULL}
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstAdaptiveDemuxTestCase *engineTestData;
  GstHlsDemuxTestCase hlsTestCase = { 0 };
  GByteArray *mpeg_ts = NULL;

  engineTestData = gst_adaptive_demux_test_case_new ();
  mpeg_ts = setup_test_variables (__FUNCTION__, inputTestData, outputTestData,
      &hlsTestCase, engineTestData, segment_size);

  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;
  /* seek to 5ms.
   * Because there is only one fragment, we expect the whole file to be
   * downloaded again
   */
  engineTestData->threshold_for_seek = 20 * TS_PACKET_LEN;
  engineTestData->seek_event =
      gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, GST_SEEK_TYPE_SET,
      5 * GST_MSECOND, GST_SEEK_TYPE_NONE, 0);

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_seek (DEMUX_ELEMENT_NAME,
      inputTestData[0].uri, engineTestData);

  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

static void
run_seek_position_test (gdouble rate, GstSeekType start_type,
    guint64 seek_start, GstSeekType stop_type,
    guint64 seek_stop, GstSeekFlags flags, guint64 segment_start,
    guint64 segment_stop, gint segments)
{
  const guint segment_size = 60 * TS_PACKET_LEN;
  const gchar *manifest =
      "#EXTM3U \n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n"
      "#EXTINF:1,Test\n" "002.ts\n"
      "#EXTINF:1,Test\n" "003.ts\n"
      "#EXTINF:1,Test\n" "004.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/media.m3u8", (guint8 *) manifest, 0},
    {"http://unit.test/001.ts", NULL, segment_size},
    {"http://unit.test/002.ts", NULL, segment_size},
    {"http://unit.test/003.ts", NULL, segment_size},
    {"http://unit.test/004.ts", NULL, segment_size},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", segment_size * segments, NULL},
    {NULL, 0, NULL}
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstAdaptiveDemuxTestCase *engineTestData;
  GstHlsDemuxTestCase hlsTestCase = { 0 };
  GByteArray *mpeg_ts = NULL;

  engineTestData = gst_adaptive_demux_test_case_new ();
  mpeg_ts = setup_test_variables (__FUNCTION__, inputTestData, outputTestData,
      &hlsTestCase, engineTestData, segment_size);

  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;

  /* FIXME hack to avoid having a 0 seqnum */
  gst_util_seqnum_next ();

  /* Seek to 1.5s, expect it to start from 1s */
  engineTestData->threshold_for_seek = 20 * TS_PACKET_LEN;
  engineTestData->seek_event =
      gst_event_new_seek (rate, GST_FORMAT_TIME, flags, start_type,
      seek_start, stop_type, seek_stop);
  gst_segment_init (&outputTestData[0].post_seek_segment, GST_FORMAT_TIME);
  outputTestData[0].post_seek_segment.rate = rate;
  outputTestData[0].post_seek_segment.start = segment_start;
  outputTestData[0].post_seek_segment.time = segment_start;
  outputTestData[0].post_seek_segment.stop = segment_stop;
  outputTestData[0].segment_verification_needed = TRUE;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_seek (DEMUX_ELEMENT_NAME,
      inputTestData[0].uri, engineTestData);

  TESTCASE_UNREF_BOILERPLATE;
}


GST_START_TEST (testSeekKeyUnitPosition)
{
  /* Seek to 1.5s with key unit, it should go back to 1.0s. 3 segments will be
   * pushed */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
      1000 * GST_MSECOND, -1, 3);
}

GST_END_TEST;

GST_START_TEST (testSeekPosition)
{
  /* Seek to 1.5s without key unit, it should keep the 1.5s, but still push
   * from the 1st segment, so 3 segments will be
   * pushed */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0, GST_SEEK_FLAG_FLUSH, 1500 * GST_MSECOND, -1, 3);
}

GST_END_TEST;

GST_START_TEST (testSeekUpdateStopPosition)
{
  run_seek_position_test (1.0, GST_SEEK_TYPE_NONE, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 3000 * GST_MSECOND, 0, 0, 3000 * GST_MSECOND, 3);
}

GST_END_TEST;

GST_START_TEST (testSeekSnapBeforePosition)
{
  /* Seek to 1.5s, snap before, it go to 1s */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE,
      1000 * GST_MSECOND, -1, 3);
}

GST_END_TEST;


GST_START_TEST (testSeekSnapAfterPosition)
{
  /* Seek to 1.5s with snap after, it should move to 2s */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_AFTER,
      2000 * GST_MSECOND, -1, 2);
}

GST_END_TEST;


GST_START_TEST (testReverseSeekSnapBeforePosition)
{
  run_seek_position_test (-1.0, GST_SEEK_TYPE_SET, 1000 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 2500 * GST_MSECOND,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE,
      1000 * GST_MSECOND, 3000 * GST_MSECOND, 2);
}

GST_END_TEST;


GST_START_TEST (testReverseSeekSnapAfterPosition)
{
  run_seek_position_test (-1.0, GST_SEEK_TYPE_SET, 1000 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 2500 * GST_MSECOND,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_AFTER,
      1000 * GST_MSECOND, 2000 * GST_MSECOND, 1);
}

GST_END_TEST;

static void
testDownloadErrorMessageCallback (GstAdaptiveDemuxTestEngine * engine,
    GstMessage * msg, gpointer user_data)
{
  GError *err = NULL;
  gchar *dbg_info = NULL;

  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
  gst_message_parse_error (msg, &err, &dbg_info);
  GST_DEBUG ("Error from element %s : %s",
      GST_OBJECT_NAME (msg->src), err->message);
  fail_unless_equals_string (GST_OBJECT_NAME (msg->src), DEMUX_ELEMENT_NAME);
  g_error_free (err);
  g_free (dbg_info);
  g_main_loop_quit (engine->loop);
}

/* test failing to download the media playlist */
GST_START_TEST (testMediaPlaylistNotFound)
{
  const gchar *master_playlist =
      "#EXTM3U\n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f mp4a.40.2\", RESOLUTION=640x352\n"
      "1200.m3u8\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/master.m3u8", (guint8 *) master_playlist, 0},
    {NULL, NULL, 0}
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", 0, NULL},
    {NULL, 0, NULL}
  };
  TESTCASE_INIT_BOILERPLATE (0);

  gst_structure_set (hlsTestCase.state,
      "failure-count", G_TYPE_UINT, 0,
      "failure-suffix", G_TYPE_STRING, "1200.m3u8", NULL);
  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;
  engine_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  engine_callbacks.bus_error_message = testDownloadErrorMessageCallback;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/master.m3u8", &engine_callbacks, engineTestData);

  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

static void
hlsdemux_test_check_no_data_received (GstAdaptiveDemuxTestEngine
    * engine, GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data)
{
  assert_equals_uint64 (stream->total_received_size, 0);
  g_main_loop_quit (engine->loop);
}

/* test failing to download a media segment (a 404 error) */
GST_START_TEST (testFragmentNotFound)
{
  const gchar *master_playlist =
      "#EXTM3U\n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f mp4a.40.2\", RESOLUTION=640x352\n"
      "1200.m3u8\n";
  const gchar *media_playlist =
      "#EXTM3U \n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/master.m3u8", (guint8 *) master_playlist, 0},
    {"http://unit.test/1200.m3u8", (guint8 *) media_playlist, 0},
    {NULL, NULL, 0}
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"src_0", 0, NULL},
    {NULL, 0, NULL}
  };
  TESTCASE_INIT_BOILERPLATE (0);

  gst_structure_set (hlsTestCase.state,
      "failure-count", G_TYPE_UINT, 0,
      "failure-suffix", G_TYPE_STRING, "001.ts", NULL);
  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_src_create;
  engine_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  engine_callbacks.appsink_eos = hlsdemux_test_check_no_data_received;
  engine_callbacks.bus_error_message = testDownloadErrorMessageCallback;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/master.m3u8", &engine_callbacks, engineTestData);

  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

/* work-around that adaptivedemux is not posting an error message
   about failure to download a fragment */
static void
missing_message_eos_callback (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data)
{
  GstAdaptiveDemuxTestCase *testData = GST_ADAPTIVE_DEMUX_TEST_CASE (user_data);
  GstAdaptiveDemuxTestExpectedOutput *testOutputStreamData;

  fail_unless (stream != NULL);
  testOutputStreamData =
      gst_adaptive_demux_test_find_test_data_by_stream (testData, stream, NULL);
  fail_unless (testOutputStreamData != NULL);
  /* expect to receive less than file size */
  fail_unless (stream->total_received_size <
      testOutputStreamData->expected_size,
      "size validation failed for %s, expected < %d received %d",
      testOutputStreamData->name, testOutputStreamData->expected_size,
      stream->total_received_size);
  testData->count_of_finished_streams++;
  GST_DEBUG ("EOS callback %d %d",
      testData->count_of_finished_streams,
      g_list_length (testData->output_streams));
  if (testData->count_of_finished_streams ==
      g_list_length (testData->output_streams)) {
    g_main_loop_quit (engine->loop);
  }
}


/*
 * Test fragment download error
 * Let the adaptive demux download a few bytes, then instruct the
 * test soup http src element to generate an error.
 */
GST_START_TEST (testFragmentDownloadError)
{
  const guint segment_size = 30 * TS_PACKET_LEN;
  const gchar *master_playlist =
      "#EXTM3U\n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f mp4a.40.2\", RESOLUTION=640x352\n"
      "1200.m3u8\n";
  const gchar *media_playlist =
      "#EXTM3U \n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-TARGETDURATION:1\n"
      "#EXTINF:1,Test\n" "001.ts\n"
      "#EXTINF:1,Test\n" "002.ts\n" "#EXT-X-ENDLIST\n";
  GstHlsDemuxTestInputData inputTestData[] = {
    {"http://unit.test/master.m3u8", (guint8 *) master_playlist, 0},
    {"http://unit.test/1200.m3u8", (guint8 *) media_playlist, 0},
    {"http://unit.test/001.ts", NULL, segment_size},
    {"http://unit.test/002.ts", NULL, segment_size},
    {NULL, NULL, 0}
  };
  const guint64 failure_position = 2048;
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    /* adaptive demux tries for 4 times (MAX_DOWNLOAD_ERROR_COUNT + 1) before giving up */
    {"src_0", failure_position * 4, NULL},
    {NULL, 0, NULL}
  };
  TESTCASE_INIT_BOILERPLATE (segment_size);

  /* download in chunks of failure_position size.
   * This means the first chunk will succeed, the second will generate
   * error because we already exceeded failure_position bytes.
   */
  gst_test_http_src_set_default_blocksize (failure_position);

  http_src_callbacks.src_start = gst_hlsdemux_test_src_start;
  http_src_callbacks.src_create = gst_hlsdemux_test_network_error_src_create;
  gst_structure_set (hlsTestCase.state,
      "failure-suffix", G_TYPE_STRING, "001.ts",
      "failure-position", G_TYPE_UINT64, failure_position, NULL);
  engine_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  engine_callbacks.appsink_eos = missing_message_eos_callback;
  engine_callbacks.bus_error_message = testDownloadErrorMessageCallback;

  gst_test_http_src_install_callbacks (&http_src_callbacks, &hlsTestCase);
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      inputTestData[0].uri, &engine_callbacks, engineTestData);

  TESTCASE_UNREF_BOILERPLATE;
}

GST_END_TEST;

static Suite *
hls_demux_suite (void)
{
  Suite *s = suite_create ("hls_demux");
  TCase *tc_basicTest = tcase_create ("basicTest");

  tcase_add_test (tc_basicTest, simpleTest);
  tcase_add_test (tc_basicTest, testMasterPlaylist);
  tcase_add_test (tc_basicTest, testMediaPlaylistNotFound);
  tcase_add_test (tc_basicTest, testFragmentNotFound);
  tcase_add_test (tc_basicTest, testFragmentDownloadError);
  tcase_add_test (tc_basicTest, testSeek);
  tcase_add_test (tc_basicTest, testSeekKeyUnitPosition);
  tcase_add_test (tc_basicTest, testSeekPosition);
  tcase_add_test (tc_basicTest, testSeekUpdateStopPosition);
  tcase_add_test (tc_basicTest, testSeekSnapBeforePosition);
  tcase_add_test (tc_basicTest, testSeekSnapAfterPosition);
  tcase_add_test (tc_basicTest, testReverseSeekSnapBeforePosition);
  tcase_add_test (tc_basicTest, testReverseSeekSnapAfterPosition);

  tcase_add_unchecked_fixture (tc_basicTest, gst_adaptive_demux_test_setup,
      gst_adaptive_demux_test_teardown);

  suite_add_tcase (s, tc_basicTest);

  return s;
}

GST_CHECK_MAIN (hls_demux);
