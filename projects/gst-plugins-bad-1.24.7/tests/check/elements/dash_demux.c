/* GStreamer unit test for MPEG-DASH
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

#define DEMUX_ELEMENT_NAME "dashdemux"

#define COPY_OUTPUT_TEST_DATA(outputTestData,testData) do { \
    guint otdPos, otdLen = sizeof((outputTestData)) / sizeof((outputTestData)[0]); \
    for(otdPos=0; otdPos<otdLen; ++otdPos){ \
  GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->output_streams = g_list_append (GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->output_streams, &(outputTestData)[otdPos]); \
    } \
  } while(0)

typedef struct _GstDashDemuxTestInputData
{
  const gchar *uri;
  const guint8 *payload;
  guint64 size;
} GstDashDemuxTestInputData;

typedef struct _GstTestHTTPSrcTestData
{
  const GstDashDemuxTestInputData *input;
  GstStructure *data;
} GstTestHTTPSrcTestData;

typedef struct _GstDashDemuxTestCase
{
  GstAdaptiveDemuxTestCase parent;

  /* the number of Protection Events sent to each pad */
  GstStructure *countContentProtectionEvents;
} GstDashDemuxTestCase;

GType gst_dash_demux_test_case_get_type (void);
static void gst_dash_demux_test_case_dispose (GObject * object);
static void gst_dash_demux_test_case_finalize (GObject * object);
static void gst_dash_demux_test_case_clear (GstDashDemuxTestCase * test_case);

static GstDashDemuxTestCase *
gst_dash_demux_test_case_new (void)
    G_GNUC_MALLOC;

#define GST_TYPE_DASH_DEMUX_TEST_CASE \
  (gst_dash_demux_test_case_get_type())
#define GST_DASH_DEMUX_TEST_CASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DASH_DEMUX_TEST_CASE, GstDashDemuxTestCase))
#define GST_DASH_DEMUX_TEST_CASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DASH_DEMUX_TEST_CASE, GstDashDemuxTestCaseClass))
#define GST_DASH_DEMUX_TEST_CASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DASH_DEMUX_TEST_CASE, GstDashDemuxTestCaseClass))
#define GST_IS_DASH_DEMUX_TEST_CASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DASH_DEMUX_TEST_CASE))
#define GST_IS_DASH_DEMUX_TEST_CASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DASH_DEMUX_TEST_CASE))

     static GstDashDemuxTestCase *gst_dash_demux_test_case_new (void)
{
  return g_object_new (GST_TYPE_DASH_DEMUX_TEST_CASE, NULL);
}

typedef struct _GstDashDemuxTestCaseClass
{
  GstAdaptiveDemuxTestCaseClass parent_class;
} GstDashDemuxTestCaseClass;

#define gst_dash_demux_test_case_parent_class parent_class

G_DEFINE_TYPE (GstDashDemuxTestCase, gst_dash_demux_test_case,
    GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE);

static void
gst_dash_demux_test_case_class_init (GstDashDemuxTestCaseClass * klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);

  object->dispose = gst_dash_demux_test_case_dispose;
  object->finalize = gst_dash_demux_test_case_finalize;
}

static void
gst_dash_demux_test_case_init (GstDashDemuxTestCase * test_case)
{
  test_case->countContentProtectionEvents = NULL;
  gst_dash_demux_test_case_clear (test_case);
}

static void
gst_dash_demux_test_case_clear (GstDashDemuxTestCase * test_case)
{
  if (test_case->countContentProtectionEvents) {
    gst_structure_free (test_case->countContentProtectionEvents);
    test_case->countContentProtectionEvents = NULL;
  }
}

static void
gst_dash_demux_test_case_dispose (GObject * object)
{
  GstDashDemuxTestCase *testData = GST_DASH_DEMUX_TEST_CASE (object);

  gst_dash_demux_test_case_clear (testData);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_dash_demux_test_case_finalize (GObject * object)
{
  /*GstDashDemuxTestCase *testData = GST_DASH_DEMUX_TEST_CASE (object); */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_dashdemux_http_src_start (GstTestHTTPSrc * src,
    const gchar * uri, GstTestHTTPSrcInput * input_data, gpointer user_data)
{
  const GstTestHTTPSrcTestData *test_case =
      (const GstTestHTTPSrcTestData *) user_data;
  guint i;

  for (i = 0; test_case->input[i].uri; ++i) {
    if (g_strcmp0 (test_case->input[i].uri, uri) == 0) {
      input_data->context = (gpointer) & test_case->input[i];
      input_data->size = test_case->input[i].size;
      if (test_case->input[i].size == 0)
        input_data->size = strlen ((gchar *) test_case->input[i].payload);
      return TRUE;
    }
  }
  return FALSE;
}

static GstFlowReturn
gst_dashdemux_http_src_create (GstTestHTTPSrc * src,
    guint64 offset,
    guint length, GstBuffer ** retbuf, gpointer context, gpointer user_data)
{
  const GstDashDemuxTestInputData *input =
      (const GstDashDemuxTestInputData *) context;
  GstBuffer *buf;

  buf = gst_buffer_new_allocate (NULL, length, NULL);
  fail_if (buf == NULL, "Not enough memory to allocate buffer");

  if (input->payload) {
    gst_buffer_fill (buf, 0, input->payload + offset, length);
  } else {
    GstMapInfo info;
    guint pattern;
    guint64 i;

    pattern = offset - offset % sizeof (pattern);

    gst_buffer_map (buf, &info, GST_MAP_WRITE);
    for (i = 0; i < length; ++i) {
      gchar pattern_byte_to_write = (offset + i) % sizeof (pattern);
      if (pattern_byte_to_write == 0) {
        pattern = offset + i;
      }
      info.data[i] = (pattern >> (pattern_byte_to_write * 8)) & 0xFF;
    }
    gst_buffer_unmap (buf, &info);
  }
  *retbuf = buf;
  return GST_FLOW_OK;
}

/******************** Test specific code starts here **************************/

/*
 * Test an mpd with an audio and a video stream
 *
 */
GST_START_TEST (simpleTest)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation>"
      "    </AdaptationSet>"
      "    <AdaptationSet mimeType=\"video/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"242\""
      "                      codecs=\"vp9\""
      "                      width=\"426\""
      "                      height=\"240\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"490208\">"
      "        <BaseURL>video.webm</BaseURL>"
      "        <SegmentBase indexRange=\"234-682\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-233\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {"http://unit.test/video.webm", NULL, 9000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 5000, NULL},
    {"video_00", 9000, NULL}
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME, "http://unit.test/test.mpd",
      &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/*
 * Test an mpd with 2 periods
 *
 */
GST_START_TEST (testTwoPeriods)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT300S\">"
      "  <Period id=\"Period0\" duration=\"PT0.1S\">"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio1.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation>"
      "    </AdaptationSet>"
      "    <AdaptationSet mimeType=\"video/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"242\""
      "                      codecs=\"vp9\""
      "                      width=\"426\""
      "                      height=\"240\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"490208\">"
      "        <BaseURL>video1.webm</BaseURL>"
      "        <SegmentBase indexRange=\"234-682\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-233\" />"
      "        </SegmentBase>"
      "      </Representation>"
      "    </AdaptationSet>"
      "  </Period>"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio2.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation>"
      "    </AdaptationSet>"
      "    <AdaptationSet mimeType=\"video/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"242\""
      "                      codecs=\"vp9\""
      "                      width=\"426\""
      "                      height=\"240\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"490208\">"
      "        <BaseURL>video2.webm</BaseURL>"
      "        <SegmentBase indexRange=\"234-682\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-233\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio1.webm", NULL, 5001},
    {"http://unit.test/video1.webm", NULL, 9001},
    {"http://unit.test/audio2.webm", NULL, 5002},
    {"http://unit.test/video2.webm", NULL, 9002},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 5001, NULL},
    {"video_00", 9001, NULL},
    {"audio_01", 5002, NULL},
    {"video_01", 9002, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/* test setting a property on an object */
#define test_int_prop(object, name, value) \
do \
{ \
  int val = value; \
  int val_after; \
  g_object_set (object, name, val, NULL); \
  g_object_get (object, name, &val_after, NULL); \
  fail_unless (val_after == val, "property check failed for %s: set to %d, but got %d", \
      name, val, val_after); \
} while (0)

#define test_float_prop(object, name, value) \
do \
{ \
  float val = value; \
  float val_after; \
  g_object_set (object, name, val, NULL); \
  g_object_get (object, name, &val_after, NULL); \
  fail_unless (val_after == val, "property check failed for %s: set to %f, but got %f", \
      name, val, val_after); \
} while (0)

/* test setting an invalid value for a property on an object.
 * Expect an assert and the property to remain unchanged
 */
#define test_invalid_int_prop(object, name, value) \
do \
{ \
  int val_before; \
  int val_after; \
  int val = value; \
  g_object_get (object, name, &val_before, NULL); \
  ASSERT_WARNING (g_object_set (object, name, val, NULL)); \
  g_object_get (object, name, &val_after, NULL); \
  fail_unless (val_after == val_before, "property check failed for %s: before %d, after %d", \
      name, val_before, val_after); \
} while (0)

#define test_invalid_float_prop(object, name, value) \
do \
{ \
  float val_before; \
  float val_after; \
  float val = value; \
  g_object_get (object, name, &val_before, NULL); \
  ASSERT_WARNING (g_object_set (object, name, val, NULL)); \
  g_object_get (object, name, &val_after, NULL); \
  fail_unless (val_after == val_before, "property check failed for %s: before %f, after %f", \
      name, val_before, val_after); \
} while (0)

static void
setAndTestDashParams (GstAdaptiveDemuxTestEngine * engine, gpointer user_data)
{
  /*  GstDashDemuxTestCase * testData = (GstDashDemuxTestCase*)user_data; */
  GObject *dashdemux = G_OBJECT (engine->demux);

  test_int_prop (dashdemux, "connection-speed", 1000);
  test_invalid_int_prop (dashdemux, "connection-speed", 4294967 + 1);

  test_float_prop (dashdemux, "bitrate-limit", 1);
  test_invalid_float_prop (dashdemux, "bitrate-limit", 2.1);

  test_int_prop (dashdemux, "max-buffering-time", 15);
  test_invalid_int_prop (dashdemux, "max-buffering-time", 1);

  test_float_prop (dashdemux, "bandwidth-usage", 0.5);
  test_invalid_float_prop (dashdemux, "bandwidth-usage", 2);

  test_int_prop (dashdemux, "max-bitrate", 1000);
  test_invalid_int_prop (dashdemux, "max-bitrate", 10);
}

/*
 * Test setting parameters
 *
 */
GST_START_TEST (testParameters)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 5000, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.pre_test = setAndTestDashParams;
  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME, "http://unit.test/test.mpd",
      &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/*
 * Test seeking
 *
 */
GST_START_TEST (testSeek)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 10000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 10000, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  /* media segment starts at 4687
   * Issue a seek request after media segment has started to be downloaded
   * on the first pad listed in GstAdaptiveDemuxTestOutputStreamData and the
   * first chunk of at least one byte has already arrived in AppSink
   */
  GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->threshold_for_seek = 4687 + 1;

  /* seek to 5ms.
   * Because there is only one fragment, we expect the whole file to be
   * downloaded again
   */
  GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->seek_event =
      gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, GST_SEEK_TYPE_SET,
      5 * GST_MSECOND, GST_SEEK_TYPE_NONE, 0);

  gst_adaptive_demux_test_seek (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", GST_ADAPTIVE_DEMUX_TEST_CASE (testData));

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;


#define SEGMENT_SIZE 10000
static void
run_seek_position_test (gdouble rate, GstSeekType start_type,
    guint64 seek_start, GstSeekType stop_type, guint64 seek_stop,
    GstSeekFlags flags, guint64 segment_start, guint64 segment_stop,
    gint segments, gint seek_threshold_bytes)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet "
      "        mimeType=\"audio/mp4\" minBandwidth=\"128000\" "
      "        maxBandwidth=\"128000\" segmentAlignment=\"true\">"
      "      <SegmentTemplate timescale=\"48000\" "
      "          initialization=\"init-$RepresentationID$.mp4\" "
      "          media=\"$RepresentationID$-$Number$.mp4\" "
      "          startNumber=\"1\">"
      "        <SegmentTimeline>"
      "          <S t=\"0\" d=\"48000\" /> "
      "          <S d=\"48000\" /> "
      "          <S d=\"48000\" /> "
      "          <S d=\"48000\" /> "
      "        </SegmentTimeline>"
      "      </SegmentTemplate>"
      "      <Representation id=\"audio\" bandwidth=\"128000\" "
      "          codecs=\"mp4a.40.2\" audioSamplingRate=\"48000\"> "
      "        <AudioChannelConfiguration "
      "            schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "            value=\"2\"> "
      "        </AudioChannelConfiguration> "
      "    </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/init-audio.mp4", NULL, 10000},
    {"http://unit.test/audio-1.mp4", NULL, 10000},
    {"http://unit.test/audio-2.mp4", NULL, 10000},
    {"http://unit.test/audio-3.mp4", NULL, 10000},
    {"http://unit.test/audio-4.mp4", NULL, 10000},
    {NULL, NULL, 0},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    /* 1 from the init segment */
    {"audio_00", (segments ? 1 + segments : 0) * 10000, NULL},
  };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  /* media segment starts at 4687
   * Issue a seek request after media segment has started to be downloaded
   * on the first pad listed in GstAdaptiveDemuxTestOutputStreamData and the
   * first chunk of at least one byte has already arrived in AppSink
   */
  if (seek_threshold_bytes)
    GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->threshold_for_seek =
        seek_threshold_bytes;
  else
    GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->threshold_for_seek = 4687 + 1;

  /* FIXME hack to avoid having a 0 seqnum */
  gst_util_seqnum_next ();

  /* seek to 5ms.
   * Because there is only one fragment, we expect the whole file to be
   * downloaded again
   */
  GST_ADAPTIVE_DEMUX_TEST_CASE (testData)->seek_event =
      gst_event_new_seek (rate, GST_FORMAT_TIME, flags, start_type,
      seek_start, stop_type, seek_stop);

  gst_adaptive_demux_test_seek (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", GST_ADAPTIVE_DEMUX_TEST_CASE (testData));

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_START_TEST (testSeekKeyUnitPosition)
{
  /* Seek to 1.5s with key unit, it should go back to 1.0s. 3 segments will be
   * pushed */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
      1000 * GST_MSECOND, -1, 3, 0);
}

GST_END_TEST;


GST_START_TEST (testSeekUpdateStopPosition)
{
  run_seek_position_test (1.0, GST_SEEK_TYPE_NONE, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 3000 * GST_MSECOND, 0, 0, 3000 * GST_MSECOND, 3, 0);
}

GST_END_TEST;

GST_START_TEST (testSeekPosition)
{
  /* Seek to 1.5s without key unit, it should keep the 1.5s, but still push
   * from the 1st segment, so 3 segments will be
   * pushed */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0, GST_SEEK_FLAG_FLUSH, 1500 * GST_MSECOND, -1, 3, 0);
}

GST_END_TEST;

GST_START_TEST (testSeekSnapBeforePosition)
{
  /* Seek to 1.5s, snap before, it go to 1s */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE,
      1000 * GST_MSECOND, -1, 3, 0);
}

GST_END_TEST;


GST_START_TEST (testSeekSnapAfterPosition)
{
  /* Seek to 1.5s with snap after, it should move to 2s */
  run_seek_position_test (1.0, GST_SEEK_TYPE_SET, 1500 * GST_MSECOND,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_AFTER,
      2000 * GST_MSECOND, -1, 2, 0);
}

GST_END_TEST;


GST_START_TEST (testSeekSnapBeforeSamePosition)
{
  /* Snap seek without position */
  run_seek_position_test (1.0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE,
      2 * GST_MSECOND, -1, 2, SEGMENT_SIZE * 3 + 1);
}

GST_END_TEST;


GST_START_TEST (testSeekSnapAfterSamePosition)
{
  /* Snap seek without position */
  run_seek_position_test (1.0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
      GST_SEEK_TYPE_NONE, 0,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_AFTER,
      3 * GST_MSECOND, -1, 1, SEGMENT_SIZE * 3 + 1);
}

GST_END_TEST;



GST_START_TEST (testReverseSeekSnapBeforePosition)
{
  run_seek_position_test (-1.0, GST_SEEK_TYPE_SET, 1000 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 2500 * GST_MSECOND,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE,
      1000 * GST_MSECOND, 3000 * GST_MSECOND, 2, 0);
}

GST_END_TEST;


GST_START_TEST (testReverseSeekSnapAfterPosition)
{
  run_seek_position_test (-1.0, GST_SEEK_TYPE_SET, 1000 * GST_MSECOND,
      GST_SEEK_TYPE_SET, 2500 * GST_MSECOND,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_AFTER,
      1000 * GST_MSECOND, 2000 * GST_MSECOND, 1, 0);
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

/*
 * Test error case of failing to download a segment
 */
GST_START_TEST (testDownloadError)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT0.5S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio_file_not_available.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 0, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.bus_error_message = testDownloadErrorMessageCallback;
  test_callbacks.appsink_eos = gst_adaptive_demux_test_unexpected_eos;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME, "http://unit.test/test.mpd",
      &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

static GstFlowReturn
test_fragment_download_error_src_create (GstTestHTTPSrc * src,
    guint64 offset,
    guint length, GstBuffer ** retbuf, gpointer context, gpointer user_data)
{
  const GstDashDemuxTestInputData *input =
      (const GstDashDemuxTestInputData *) context;
  const GstTestHTTPSrcTestData *http_src_test_data =
      (const GstTestHTTPSrcTestData *) user_data;
  guint64 threshold_for_trigger;

  fail_unless (input != NULL);
  gst_structure_get_uint64 (http_src_test_data->data, "threshold_for_trigger",
      &threshold_for_trigger);

  if (!g_str_has_suffix (input->uri, ".mpd") && offset >= threshold_for_trigger) {
    GST_DEBUG ("network_error %s %" G_GUINT64_FORMAT " @ %" G_GUINT64_FORMAT,
        input->uri, offset, threshold_for_trigger);
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (("A network error occurred, or the server closed the connection unexpectedly.")), ("A network error occurred, or the server closed the connection unexpectedly."));
    return GST_FLOW_ERROR;
  }
  return gst_dashdemux_http_src_create (src, offset, length, retbuf, context,
      user_data);
}

/*
 * Test header download error
 * Let the adaptive demux download a few bytes, then instruct the
 * GstTestHTTPSrc element to generate an error while the fragment header
 * is still being downloaded.
 */
GST_START_TEST (testHeaderDownloadError)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT0.5S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  /* generate error while the headers are still being downloaded
   * threshold_for_trigger must be less than the size of headers
   * (initialization + index) which is 4687.
   */
  guint64 threshold_for_trigger = 2000;

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    /* adaptive demux tries for 4 times (MAX_DOWNLOAD_ERROR_COUNT + 1) before giving up */
    {"audio_00", threshold_for_trigger * 4, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = test_fragment_download_error_src_create;
  http_src_test_data.data = gst_structure_new_empty (__FUNCTION__);
  gst_structure_set (http_src_test_data.data, "threshold_for_trigger",
      G_TYPE_UINT64, threshold_for_trigger, NULL);
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos = gst_adaptive_demux_test_unexpected_eos;
  test_callbacks.bus_error_message = testDownloadErrorMessageCallback;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  /* download in chunks of threshold_for_trigger size.
   * This means the first chunk will succeed, the second will generate
   * error because we already exceeded threshold_for_trigger bytes.
   */
  gst_test_http_src_set_default_blocksize (threshold_for_trigger);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/*
 * Test media download error on the last media fragment.
 * Let the adaptive demux download a few bytes, then instruct the
 * GstTestHTTPSrc element to generate an error while the last media fragment
 * is being downloaded.
 * Adaptive demux will not retry downloading the last media fragment. It will
 * be considered eos.
 */
GST_START_TEST (testMediaDownloadErrorLastFragment)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT0.5S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  /* generate error on the first media fragment */
  guint64 threshold_for_trigger = 4687;

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    /* adaptive demux will not retry because this is the last fragment */
    {"audio_00", threshold_for_trigger, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = test_fragment_download_error_src_create;
  http_src_test_data.data = gst_structure_new_empty (__FUNCTION__);
  gst_structure_set (http_src_test_data.data, "threshold_for_trigger",
      G_TYPE_UINT64, threshold_for_trigger, NULL);
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/*
 * Test media download error on a media fragment which is not the last one.
 * Let the adaptive demux download a few bytes, then instruct the
 * GstTestHTTPSrc element to generate an error while a media fragment
 * is being downloaded.
 */
GST_START_TEST (testMediaDownloadErrorMiddleFragment)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT10S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentList duration=\"1\">"
      "          <SegmentURL indexRange=\"1-10\""
      "                      mediaRange=\"11-30\">"
      "          </SegmentURL>"
      "          <SegmentURL indexRange=\"31-60\""
      "                      mediaRange=\"61-100\">"
      "          </SegmentURL>"
      "          <SegmentURL indexRange=\"101-150\""
      "                      mediaRange=\"151-210\">"
      "          </SegmentURL>"
      "        </SegmentList>"
      "      </Representation></AdaptationSet></Period></MPD>";

  /* generate error on the second media fragment */
  guint64 threshold_for_trigger = 31;

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    /* adaptive demux will download only the first media fragment */
    {"audio_00", 20, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = test_fragment_download_error_src_create;
  http_src_test_data.data = gst_structure_new_empty (__FUNCTION__);
  gst_structure_set (http_src_test_data.data, "threshold_for_trigger",
      G_TYPE_UINT64, threshold_for_trigger, NULL);
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos = gst_adaptive_demux_test_unexpected_eos;
  test_callbacks.bus_error_message = testDownloadErrorMessageCallback;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

/* generate queries to adaptive demux */
static gboolean
testQueryCheckDataReceived (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream,
    GstBuffer * buffer, gpointer user_data)
{
  GList *pads;
  GstPad *pad;
  GstQuery *query;
  gboolean ret;
  gint64 duration;
  gboolean seekable;
  gint64 segment_start;
  gint64 segment_end;
  gboolean live;
  GstClockTime min_latency;
  GstClockTime max_latency;
  gchar *uri;
  gchar *redirect_uri;
  gboolean redirect_permanent;

  pads = GST_ELEMENT_PADS (stream->appsink);

  /* AppSink should have only 1 pad */
  fail_unless (pads != NULL);
  fail_unless (g_list_length (pads) == 1);
  pad = GST_PAD (pads->data);

  /* duration query */
  query = gst_query_new_duration (GST_FORMAT_TIME);
  ret = gst_pad_peer_query (pad, query);
  fail_unless (ret == TRUE);
  gst_query_parse_duration (query, NULL, &duration);
  /* mediaPresentationDuration=\"PT135.743S\" */
  fail_unless (duration == 135743 * GST_MSECOND);
  gst_query_unref (query);

  /* seek query */
  query = gst_query_new_seeking (GST_FORMAT_TIME);
  ret = gst_pad_peer_query (pad, query);
  fail_unless (ret == TRUE);
  gst_query_parse_seeking (query, NULL, &seekable, &segment_start,
      &segment_end);
  fail_unless (seekable == TRUE);
  fail_unless (segment_start == 0);
  fail_unless (segment_end == duration);
  gst_query_unref (query);

  /* latency query */
  query = gst_query_new_latency ();
  ret = gst_pad_peer_query (pad, query);
  fail_unless (ret == TRUE);
  gst_query_parse_latency (query, &live, &min_latency, &max_latency);
  fail_unless (live == FALSE);
  fail_unless (min_latency == 0);
  fail_unless (max_latency == -1);
  gst_query_unref (query);

  /* uri query */
  query = gst_query_new_uri ();
  ret = gst_pad_peer_query (pad, query);
  fail_unless (ret == TRUE);
  gst_query_parse_uri (query, &uri);
  gst_query_parse_uri_redirection (query, &redirect_uri);
  gst_query_parse_uri_redirection_permanent (query, &redirect_permanent);
  fail_unless (g_strcmp0 (uri, "http://unit.test/test.mpd") == 0);
  /* adaptive demux does not reply with redirect information */
  fail_unless (redirect_uri == NULL);
  fail_unless (redirect_permanent == FALSE);
  g_free (uri);
  g_free (redirect_uri);
  gst_query_unref (query);

  return gst_adaptive_demux_test_check_received_data (engine,
      stream, buffer, user_data);
}

/*
 * Test queries
 *
 */
GST_START_TEST (testQuery)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 5000, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data = testQueryCheckDataReceived;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);

  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME,
      "http://unit.test/test.mpd", &test_callbacks, testData);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

static gboolean
testContentProtectionDashdemuxSendsEvent (GstAdaptiveDemuxTestEngine * engine,
    GstAdaptiveDemuxTestOutputStream * stream,
    GstEvent * event, gpointer user_data)
{
  GstDashDemuxTestCase *test_case = GST_DASH_DEMUX_TEST_CASE (user_data);
  const gchar *system_id;
  GstBuffer *data;
  const gchar *origin;
  GstMapInfo info;
  gchar *value;
  gchar *name;
  guint event_count = 0;

  GST_DEBUG ("received event %s", GST_EVENT_TYPE_NAME (event));

  if (GST_EVENT_TYPE (event) != GST_EVENT_PROTECTION) {
    return TRUE;
  }

  /* we expect content protection events only on video pad */
  name = gst_pad_get_name (stream->pad);
  fail_unless (g_strcmp0 (name, "video_00") == 0);
  gst_event_parse_protection (event, &system_id, &data, &origin);

  gst_buffer_map (data, &info, GST_MAP_READ);

  value = g_malloc (info.size + 1);
  strncpy (value, (gchar *) info.data, info.size);
  value[info.size] = 0;
  gst_buffer_unmap (data, &info);

  if (g_strcmp0 (system_id, "11111111-AAAA-BBBB-CCCC-123456789ABC") == 0) {
    fail_unless (g_strcmp0 (origin, "dash/mpd") == 0);
    fail_unless (g_strcmp0 (value, "test value") == 0);
  } else if (g_strcmp0 (system_id, "5e629af5-38da-4063-8977-97ffbd9902d4") == 0) {
    const gchar *str;

    fail_unless (g_strcmp0 (origin, "dash/mpd") == 0);

    /* We can't do a simple compare of value (which should be an XML dump
       of the ContentProtection element), because the whitespace
       formatting from xmlDump might differ between versions of libxml */
    str = strstr (value, "<ContentProtection");
    fail_if (str == NULL);
    str = strstr (value, "<mas:MarlinContentIds>");
    fail_if (str == NULL);
    str = strstr (value, "<mas:MarlinContentId>");
    fail_if (str == NULL);
    str = strstr (value, "urn:marlin:kid:02020202020202020202020202020202");
    fail_if (str == NULL);
    str = strstr (value, "</ContentProtection>");
    fail_if (str == NULL);
  } else if (g_strcmp0 (system_id, "9a04f079-9840-4286-ab92-e65be0885f95") == 0) {
    fail_unless (g_strcmp0 (origin, "dash/mpd") == 0);
    fail_unless (g_strcmp0 (value, "dGVzdA==") == 0);
  } else {
    fail ("unexpected content protection event '%s'", system_id);
  }

  g_free (value);

  fail_if (test_case->countContentProtectionEvents == NULL);
  gst_structure_get_uint (test_case->countContentProtectionEvents, name,
      &event_count);
  event_count++;
  gst_structure_set (test_case->countContentProtectionEvents, name, G_TYPE_UINT,
      event_count, NULL);

  g_free (name);
  return TRUE;
}

/*
 * Test content protection
 * Configure 3 content protection sources:
 * - a uuid scheme/value pair
 * - a non uuid scheme/value pair (dash recognises only uuid schemes)
 * - a complex uuid scheme, with trailing spaces and capital letters in scheme uri
 * Only the uuid scheme will be recognised. We expect to receive 2 content
 * protection events
 */
GST_START_TEST (testContentProtection)
{
  const gchar *mpd =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      "     xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""
      "     xmlns:mspr=\"urn:microsoft:playready\""
      "     xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\""
      "     profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\""
      "     type=\"static\""
      "     minBufferTime=\"PT1.500S\""
      "     mediaPresentationDuration=\"PT135.743S\">"
      "  <Period>"
      "    <AdaptationSet mimeType=\"audio/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <Representation id=\"171\""
      "                      codecs=\"vorbis\""
      "                      audioSamplingRate=\"44100\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"129553\">"
      "        <AudioChannelConfiguration"
      "           schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "           value=\"2\" />"
      "        <BaseURL>audio.webm</BaseURL>"
      "        <SegmentBase indexRange=\"4452-4686\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-4451\" />"
      "        </SegmentBase>"
      "      </Representation>"
      "    </AdaptationSet>"
      "    <AdaptationSet mimeType=\"video/webm\""
      "                   subsegmentAlignment=\"true\">"
      "      <ContentProtection schemeIdUri=\"urn:uuid:11111111-AAAA-BBBB-CCCC-123456789ABC\" value=\"test value\"/>"
      "      <ContentProtection schemeIdUri=\"urn:mpeg:dash:mp4protection:2011\" value=\"cenc\"/>"
      "      <ContentProtection schemeIdUri=\" URN:UUID:5e629af5-38da-4063-8977-97ffbd9902d4\" xmlns:mas=\"urn:marlin:mas:1-0:services:schemas:mpd\">"
      "        <mas:MarlinContentIds>"
      "          <mas:MarlinContentId>urn:marlin:kid:02020202020202020202020202020202</mas:MarlinContentId>"
      "        </mas:MarlinContentIds>"
      "      </ContentProtection>"
      "      <ContentProtection schemeIdUri=\"urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95\" value=\"MSPR 2.0\">"
      "        <mspr:pro>dGVzdA==</mspr:pro>"
      "      </ContentProtection>"
      "      <Representation id=\"242\""
      "                      codecs=\"vp9\""
      "                      width=\"426\""
      "                      height=\"240\""
      "                      startWithSAP=\"1\""
      "                      bandwidth=\"490208\">"
      "        <BaseURL>video.webm</BaseURL>"
      "        <SegmentBase indexRange=\"234-682\""
      "                     indexRangeExact=\"true\">"
      "          <Initialization range=\"0-233\" />"
      "        </SegmentBase>"
      "      </Representation></AdaptationSet></Period></MPD>";

  GstDashDemuxTestInputData inputTestData[] = {
    {"http://unit.test/test.mpd", (guint8 *) mpd, 0},
    {"http://unit.test/audio.webm", NULL, 5000},
    {"http://unit.test/video.webm", NULL, 9000},
    {NULL, NULL, 0},
  };
  GstAdaptiveDemuxTestExpectedOutput outputTestData[] = {
    {"audio_00", 5000, NULL},
    {"video_00", 9000, NULL},
  };
  GstTestHTTPSrcCallbacks http_src_callbacks = { 0 };
  GstTestHTTPSrcTestData http_src_test_data = { 0 };
  GstAdaptiveDemuxTestCallbacks test_callbacks = { 0 };
  GstDashDemuxTestCase *testData;
  guint event_count = 0;

  http_src_callbacks.src_start = gst_dashdemux_http_src_start;
  http_src_callbacks.src_create = gst_dashdemux_http_src_create;
  http_src_test_data.input = inputTestData;
  gst_test_http_src_install_callbacks (&http_src_callbacks,
      &http_src_test_data);

  test_callbacks.appsink_received_data =
      gst_adaptive_demux_test_check_received_data;
  test_callbacks.appsink_eos =
      gst_adaptive_demux_test_check_size_of_received_data;
  test_callbacks.demux_sent_event = testContentProtectionDashdemuxSendsEvent;

  testData = gst_dash_demux_test_case_new ();
  COPY_OUTPUT_TEST_DATA (outputTestData, testData);
  testData->countContentProtectionEvents =
      gst_structure_new_empty ("countContentProtectionEvents");
  gst_adaptive_demux_test_run (DEMUX_ELEMENT_NAME, "http://unit.test/test.mpd",
      &test_callbacks, testData);

  fail_unless (gst_structure_has_field_typed
      (testData->countContentProtectionEvents, "video_00", G_TYPE_UINT));

  gst_structure_get_uint (testData->countContentProtectionEvents, "video_00",
      &event_count);
  fail_unless (event_count == 3);

  g_object_unref (testData);
  if (http_src_test_data.data)
    gst_structure_free (http_src_test_data.data);
}

GST_END_TEST;

static Suite *
dash_demux_suite (void)
{
  Suite *s = suite_create ("dash_demux");
  TCase *tc_basicTest = tcase_create ("basicTest");

  tcase_add_test (tc_basicTest, simpleTest);
  tcase_add_test (tc_basicTest, testTwoPeriods);
  tcase_add_test (tc_basicTest, testParameters);
  tcase_add_test (tc_basicTest, testSeek);
  tcase_add_test (tc_basicTest, testSeekKeyUnitPosition);
  tcase_add_test (tc_basicTest, testSeekPosition);
  tcase_add_test (tc_basicTest, testSeekUpdateStopPosition);
  tcase_add_test (tc_basicTest, testSeekSnapBeforePosition);
  tcase_add_test (tc_basicTest, testSeekSnapAfterPosition);
  tcase_add_test (tc_basicTest, testSeekSnapBeforeSamePosition);
  tcase_add_test (tc_basicTest, testSeekSnapAfterSamePosition);
  tcase_add_test (tc_basicTest, testReverseSeekSnapBeforePosition);
  tcase_add_test (tc_basicTest, testReverseSeekSnapAfterPosition);
  tcase_add_test (tc_basicTest, testDownloadError);
  tcase_add_test (tc_basicTest, testHeaderDownloadError);
  tcase_add_test (tc_basicTest, testMediaDownloadErrorLastFragment);
  tcase_add_test (tc_basicTest, testMediaDownloadErrorMiddleFragment);
  tcase_add_test (tc_basicTest, testQuery);
  tcase_add_test (tc_basicTest, testContentProtection);

  tcase_add_unchecked_fixture (tc_basicTest, gst_adaptive_demux_test_setup,
      gst_adaptive_demux_test_teardown);

  suite_add_tcase (s, tc_basicTest);

  return s;
}

GST_CHECK_MAIN (dash_demux);
