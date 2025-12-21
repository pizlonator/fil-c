/* GStreamer
 *
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <ts.santos@sisa.samsung.com>
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
#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/video/video.h>
#include <gst/app/app.h>

static GstPad *mysrcpad, *mysinkpad;
static GstElement *enc;
static GList *events = NULL;

#define TEST_VIDEO_WIDTH 640
#define TEST_VIDEO_HEIGHT 480
#define TEST_VIDEO_FPS_N 30
#define TEST_VIDEO_FPS_D 1

#define GST_VIDEO_ENCODER_TESTER_TYPE gst_video_encoder_tester_get_type()
#define GST_VIDEO_ENCODER_TESTER(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_VIDEO_ENCODER_TESTER_TYPE, GstVideoEncoderTester))
static GType gst_video_encoder_tester_get_type (void);

typedef struct _GstVideoEncoderTester GstVideoEncoderTester;
typedef struct _GstVideoEncoderTesterClass GstVideoEncoderTesterClass;

struct _GstVideoEncoderTester
{
  GstVideoEncoder parent;

  GstFlowReturn pre_push_result;
  gint num_subframes;
  gint current_subframe;
  gboolean send_headers;
  gboolean key_frame_sent;
  gboolean enable_step_by_step;
  gboolean negotiate_in_set_format;
  GstVideoCodecFrame *last_frame;
};

struct _GstVideoEncoderTesterClass
{
  GstFlowReturn (*step_by_step) (GstVideoEncoder * encoder,
      GstVideoCodecFrame * frame, int steps);
  GstVideoEncoderClass parent_class;
};

G_DEFINE_TYPE (GstVideoEncoderTester, gst_video_encoder_tester,
    GST_TYPE_VIDEO_ENCODER);

static gboolean
gst_video_encoder_tester_start (GstVideoEncoder * enc)
{
  return TRUE;
}

static gboolean
gst_video_encoder_tester_stop (GstVideoEncoder * enc)
{
  return TRUE;
}

static gboolean
gst_video_encoder_tester_set_format (GstVideoEncoder * enc,
    GstVideoCodecState * state)
{
  GstVideoEncoderTester *enc_tester = GST_VIDEO_ENCODER_TESTER (enc);

  GstVideoCodecState *res = gst_video_encoder_set_output_state (enc,
      gst_caps_new_simple ("video/x-test-custom", "width", G_TYPE_INT,
          480, "height", G_TYPE_INT, 360, NULL),
      state);

  gst_video_codec_state_unref (res);

  if (enc_tester->negotiate_in_set_format) {
    gst_video_encoder_negotiate (enc);
  }

  return TRUE;
}

static GstFlowReturn
gst_video_encoder_push_subframe (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame, int current_subframe)
{
  guint8 *data;
  GstMapInfo map;
  guint64 input_num;
  GstVideoEncoderTester *enc_tester = GST_VIDEO_ENCODER_TESTER (enc);

  if (enc_tester->send_headers) {
    GstBuffer *hdr;
    GList *headers = NULL;
    hdr = gst_buffer_new_and_alloc (0);
    GST_BUFFER_FLAG_SET (hdr, GST_BUFFER_FLAG_HEADER);
    headers = g_list_append (headers, hdr);
    gst_video_encoder_set_headers (enc, headers);
    enc_tester->send_headers = FALSE;
  }

  gst_buffer_map (frame->input_buffer, &map, GST_MAP_READ);
  input_num = *((guint64 *) map.data);
  gst_buffer_unmap (frame->input_buffer, &map);

  if (!enc_tester->key_frame_sent
      || GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    enc_tester->key_frame_sent = TRUE;
  }

  data = g_malloc (sizeof (guint64));
  *(guint64 *) data = input_num;
  frame->output_buffer = gst_buffer_new_wrapped (data, sizeof (guint64));
  frame->pts = GST_BUFFER_PTS (frame->input_buffer);
  frame->duration = GST_BUFFER_DURATION (frame->input_buffer);

  if (current_subframe < enc_tester->num_subframes - 1)
    return gst_video_encoder_finish_subframe (enc, frame);
  else
    return gst_video_encoder_finish_frame (enc, frame);
}

static GstFlowReturn
gst_video_encoder_tester_output_step_by_step (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame, gint steps)
{
  GstVideoEncoderTester *enc_tester = GST_VIDEO_ENCODER_TESTER (enc);
  GstFlowReturn ret = GST_FLOW_OK;
  int i;
  for (i = enc_tester->current_subframe;
      i < MIN (steps + enc_tester->current_subframe, enc_tester->num_subframes);
      i++) {
    ret = gst_video_encoder_push_subframe (enc, frame, i);
  }
  enc_tester->current_subframe = i;
  if (enc_tester->current_subframe >= enc_tester->num_subframes) {
    enc_tester->current_subframe = 0;
    gst_video_codec_frame_unref (enc_tester->last_frame);
  }

  return ret;
}

static GstFlowReturn
gst_video_encoder_tester_handle_frame (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame)
{
  GstClockTimeDiff deadline;
  GstVideoEncoderTester *enc_tester = GST_VIDEO_ENCODER_TESTER (enc);

  deadline = gst_video_encoder_get_max_encode_time (enc, frame);
  if (deadline < 0) {
    /* Calling finish_frame() with frame->output_buffer == NULL means to drop it */
    return gst_video_encoder_finish_frame (enc, frame);
  }

  enc_tester->last_frame = gst_video_codec_frame_ref (frame);
  if (enc_tester->enable_step_by_step)
    return GST_FLOW_OK;

  return gst_video_encoder_tester_output_step_by_step (enc, frame,
      enc_tester->num_subframes);
}

static GstFlowReturn
gst_video_encoder_tester_pre_push (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame)
{
  GstVideoEncoderTester *tester = (GstVideoEncoderTester *) enc;
  return tester->pre_push_result;
}

static void
gst_video_encoder_tester_class_init (GstVideoEncoderTesterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoencoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw"));

  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-test-custom"));

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  gst_element_class_set_metadata (element_class,
      "VideoEncoderTester", "Encoder/Video", "yep", "me");

  videoencoder_class->start = gst_video_encoder_tester_start;
  videoencoder_class->stop = gst_video_encoder_tester_stop;
  videoencoder_class->handle_frame = gst_video_encoder_tester_handle_frame;
  videoencoder_class->pre_push = gst_video_encoder_tester_pre_push;
  videoencoder_class->set_format = gst_video_encoder_tester_set_format;

}

static void
gst_video_encoder_tester_init (GstVideoEncoderTester * tester)
{
  tester->pre_push_result = GST_FLOW_OK;
  /* One subframe is considered as a whole single frame. */
  tester->num_subframes = 1;
}

static gboolean
_mysinkpad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  events = g_list_append (events, event);
  return TRUE;
}

static void
setup_videoencodertester (void)
{
  static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-test-custom")
      );
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw")
      );

  enc = g_object_new (GST_VIDEO_ENCODER_TESTER_TYPE, NULL);
  mysrcpad = gst_check_setup_src_pad (enc, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (enc, &sinktemplate);

  gst_pad_set_event_function (mysinkpad, _mysinkpad_event);
}

static void
setup_videoencodertester_with_subframes (int num_subframes)
{
  GstVideoEncoderTester *enc_tester;
  setup_videoencodertester ();
  enc_tester = GST_VIDEO_ENCODER_TESTER (enc);
  enc_tester->num_subframes = num_subframes;
  enc_tester->send_headers = TRUE;
}

static void
cleanup_videoencodertest (void)
{
  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);

  gst_element_set_state (enc, GST_STATE_NULL);

  gst_check_teardown_src_pad (enc);
  gst_check_teardown_sink_pad (enc);
  gst_check_teardown_element (enc);

  g_list_free_full (events, (GDestroyNotify) gst_event_unref);
  events = NULL;
}

static GstBuffer *
create_test_buffer (guint64 num)
{
  GstBuffer *buffer;
  guint64 *data = g_malloc (sizeof (guint64));

  *data = num;

  buffer = gst_buffer_new_wrapped (data, sizeof (guint64));

  GST_BUFFER_PTS (buffer) =
      gst_util_uint64_scale_round (num, GST_SECOND * TEST_VIDEO_FPS_D,
      TEST_VIDEO_FPS_N);
  GST_BUFFER_DURATION (buffer) =
      gst_util_uint64_scale_round (GST_SECOND, TEST_VIDEO_FPS_D,
      TEST_VIDEO_FPS_N);

  return buffer;
}

static GstCaps *
create_test_caps (void)
{
  return gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT,
      TEST_VIDEO_WIDTH, "height", G_TYPE_INT, TEST_VIDEO_HEIGHT, "framerate",
      GST_TYPE_FRACTION, TEST_VIDEO_FPS_N, TEST_VIDEO_FPS_D,
      "format", G_TYPE_STRING, "GRAY8", NULL);
}

static void
send_startup_events (void)
{
  GstCaps *caps;

  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_stream_start ("randomvalue")));

  /* push caps */
  caps = create_test_caps ();
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);
}

#define NUM_BUFFERS 100
GST_START_TEST (videoencoder_playback)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GList *iter;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < NUM_BUFFERS; i++) {
    buffer = create_test_buffer (i);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that all buffers were received by our source pad */
  fail_unless (g_list_length (buffers) == NUM_BUFFERS);
  i = 0;
  for (iter = buffers; iter; iter = g_list_next (iter)) {
    GstMapInfo map;
    guint64 num;

    buffer = iter->data;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    num = *(guint64 *) map.data;
    fail_unless (i == num);
    fail_unless (GST_BUFFER_PTS (buffer) == gst_util_uint64_scale_round (i,
            GST_SECOND * TEST_VIDEO_FPS_D, TEST_VIDEO_FPS_N));
    fail_unless (GST_BUFFER_DURATION (buffer) ==
        gst_util_uint64_scale_round (GST_SECOND, TEST_VIDEO_FPS_D,
            TEST_VIDEO_FPS_N));

    gst_buffer_unmap (buffer, &map);
    i++;
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

/* make sure tags sent right before eos are pushed */
GST_START_TEST (videoencoder_tags_before_eos)
{
  GstSegment segment;
  GstBuffer *buffer;
  GstTagList *tags;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffer */
  buffer = create_test_buffer (0);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* clean received events list */
  g_list_free_full (events, (GDestroyNotify) gst_event_unref);
  events = NULL;

  /* push a tag event */
  tags = gst_tag_list_new (GST_TAG_COMMENT, "test-comment", NULL);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_tag (tags)));

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that the tag was received */
  {
    GstEvent *tag_event = events->data;
    gchar *str;

    fail_unless (GST_EVENT_TYPE (tag_event) == GST_EVENT_TAG);
    gst_event_parse_tag (tag_event, &tags);
    fail_unless (gst_tag_list_get_string (tags, GST_TAG_COMMENT, &str));
    fail_unless (strcmp (str, "test-comment") == 0);
    g_free (str);
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;
  g_list_free_full (events, (GDestroyNotify) gst_event_unref);
  events = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

/* make sure events sent right before eos are pushed */
GST_START_TEST (videoencoder_events_before_eos)
{
  GstSegment segment;
  GstBuffer *buffer;
  GstMessage *msg;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffer */
  buffer = create_test_buffer (0);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* clean received events list */
  g_list_free_full (events, (GDestroyNotify) gst_event_unref);
  events = NULL;

  /* push a serialized event */
  msg =
      gst_message_new_element (GST_OBJECT (mysrcpad),
      gst_structure_new_empty ("test"));
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_sink_message ("sink-test", msg)));
  gst_message_unref (msg);

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that the tag was received */
  {
    GstEvent *msg_event = events->data;
    const GstStructure *structure;

    fail_unless (GST_EVENT_TYPE (msg_event) == GST_EVENT_SINK_MESSAGE);
    fail_unless (gst_event_has_name (msg_event, "sink-test"));
    gst_event_parse_sink_message (msg_event, &msg);
    structure = gst_message_get_structure (msg);
    fail_unless (gst_structure_has_name (structure, "test"));
    gst_message_unref (msg);
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;
  g_list_free_full (events, (GDestroyNotify) gst_event_unref);
  events = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

GST_START_TEST (videoencoder_flush_events)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i;
  GList *events_iter;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < NUM_BUFFERS; i++) {
    if (i % 10 == 0) {
      GstTagList *tags;

      tags = gst_tag_list_new (GST_TAG_TRACK_NUMBER, i, NULL);
      fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_tag (tags)));
    } else {
      buffer = create_test_buffer (i);

      fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
    }
  }

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  events_iter = events;
  /* make sure the usual events have been received */
  {
    GstEvent *sstart = events_iter->data;
    fail_unless (GST_EVENT_TYPE (sstart) == GST_EVENT_STREAM_START);
    events_iter = g_list_next (events_iter);
  }
  {
    GstEvent *caps_event = events_iter->data;
    fail_unless (GST_EVENT_TYPE (caps_event) == GST_EVENT_CAPS);
    events_iter = g_list_next (events_iter);
  }
  {
    GstEvent *segment_event = events_iter->data;
    fail_unless (GST_EVENT_TYPE (segment_event) == GST_EVENT_SEGMENT);
    events_iter = g_list_next (events_iter);
  }

  /* check that EOS was received */
  fail_unless (GST_PAD_IS_EOS (mysrcpad));
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_start ()));
  fail_unless (GST_PAD_IS_EOS (mysrcpad));

  /* Check that we have tags */
  {
    GstEvent *tags = gst_pad_get_sticky_event (mysrcpad, GST_EVENT_TAG, 0);

    fail_unless (tags != NULL);
    gst_event_unref (tags);
  }

  /* Check that we still have a segment set */
  {
    GstEvent *segment =
        gst_pad_get_sticky_event (mysrcpad, GST_EVENT_SEGMENT, 0);

    fail_unless (segment != NULL);
    gst_event_unref (segment);
  }

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_flush_stop (TRUE)));
  fail_if (GST_PAD_IS_EOS (mysrcpad));

  /* Check that the segment was flushed on FLUSH_STOP */
  {
    GstEvent *segment =
        gst_pad_get_sticky_event (mysrcpad, GST_EVENT_SEGMENT, 0);

    fail_unless (segment == NULL);
  }

  /* Check the tags were not lost on FLUSH_STOP */
  {
    GstEvent *tags = gst_pad_get_sticky_event (mysrcpad, GST_EVENT_TAG, 0);

    fail_unless (tags != NULL);
    gst_event_unref (tags);

  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

/* When pre_push fails the correct GstFlowReturn should be returned and there
 * should be no leaks */
GST_START_TEST (videoencoder_pre_push_fails)
{
  GstVideoEncoderTester *tester;
  GstHarness *h;
  GstFlowReturn ret;

  tester = g_object_new (GST_VIDEO_ENCODER_TESTER_TYPE, NULL);
  tester->pre_push_result = GST_FLOW_ERROR;

  h = gst_harness_new_with_element (GST_ELEMENT (tester), "sink", "src");
  gst_harness_set_src_caps (h, create_test_caps ());

  ret = gst_harness_push (h, create_test_buffer (0));
  fail_unless_equals_int (ret, GST_FLOW_ERROR);

  gst_harness_teardown (h);
  gst_object_unref (tester);
}

GST_END_TEST;

GST_START_TEST (videoencoder_qos)
{
  GstSegment segment;
  GstBuffer *buffer;
  GstClockTime ts, rt;
  GstBus *bus;
  GstMessage *msg;

  setup_videoencodertester ();

  gst_video_encoder_set_qos_enabled (GST_VIDEO_ENCODER (enc), TRUE);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  bus = gst_bus_new ();
  gst_element_set_bus (enc, bus);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push the first buffer */
  buffer = create_test_buffer (0);
  ts = GST_BUFFER_PTS (buffer);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  /* pretend this buffer was late in the sink */
  rt = gst_segment_to_running_time (&segment, GST_FORMAT_TIME, ts);
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_event_new_qos (GST_QOS_TYPE_UNDERFLOW, 1.5, 500 * GST_MSECOND,
              rt)));

  /* push a second buffer which will be dropped as it's already late */
  buffer = create_test_buffer (1);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  /* A QoS message was sent by the encoder */
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_QOS);
  g_assert (msg != NULL);
  gst_message_unref (msg);

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

#define NUM_BUFFERS 100
GST_START_TEST (videoencoder_playback_subframes)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GList *iter;
  int subframes = 4;

  setup_videoencodertester_with_subframes (subframes);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < NUM_BUFFERS; i++) {
    buffer = create_test_buffer (i);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that all buffers (plus one header buffer) were received by our source pad */
  fail_unless (g_list_length (buffers) == NUM_BUFFERS * subframes + 1);
  /* check that first buffer is an header */
  buffer = buffers->data;
  fail_unless (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_HEADER));
  /* check the other buffers */
  i = 0;
  for (iter = g_list_next (buffers); iter; iter = g_list_next (iter)) {
    /* first buffer should be the header */
    GstMapInfo map;
    guint64 num;
    buffer = iter->data;
    fail_unless (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_HEADER));
    gst_buffer_map (buffer, &map, GST_MAP_READ);

    num = *(guint64 *) map.data;
    fail_unless (i / subframes == num);

    if (i % subframes)
      fail_unless (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT));

    fail_unless (GST_BUFFER_PTS (buffer) ==
        gst_util_uint64_scale_round (i / subframes,
            GST_SECOND * TEST_VIDEO_FPS_D, TEST_VIDEO_FPS_N));
    fail_unless (GST_BUFFER_DURATION (buffer) ==
        gst_util_uint64_scale_round (GST_SECOND, TEST_VIDEO_FPS_D,
            TEST_VIDEO_FPS_N));
    gst_buffer_unmap (buffer, &map);


    i++;
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

GST_START_TEST (videoencoder_playback_events_subframes)
{
  GstSegment segment;
  GstBuffer *buffer;
  GList *iter;
  gint subframes = 4;
  gint i, header_found;
  GstVideoEncoderTester *enc_tester;

  setup_videoencodertester_with_subframes (subframes);

  enc_tester = GST_VIDEO_ENCODER_TESTER (enc);
  enc_tester->send_headers = TRUE;
  enc_tester->enable_step_by_step = TRUE;

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment -> no new buffer and no new events (still pending two custom events) */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));
  fail_unless (g_list_length (buffers) == 0 && g_list_length (events) == 0);

  /* push a first buffer -> no new buffer and no new events (still pending two custom events) */
  buffer = create_test_buffer (0);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  fail_unless (g_list_length (buffers) == 0 && g_list_length (events) == 0);

  /* ouput only one subframe -> 2 buffers(header + subframe) and 3 events (stream-start, caps, segment) */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 1);
  fail_unless (g_list_length (buffers) == 2 && g_list_length (events) == 3);
  fail_unless (GST_BUFFER_FLAG_IS_SET ((GstBuffer *) buffers->data,
          GST_BUFFER_FLAG_HEADER));
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  0)->data)) == GST_EVENT_STREAM_START);
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  1)->data)) == GST_EVENT_CAPS);
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  2)->data)) == GST_EVENT_SEGMENT);

  /* output 3 last subframes -> 2 more buffers and no new events */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 3);
  fail_unless (g_list_length (buffers) == 5 && g_list_length (events) == 3);

  /* push a new buffer -> no new buffer and no new events */
  buffer = create_test_buffer (1);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  fail_unless (g_list_length (buffers) == 5 && g_list_length (events) == 3);

  /* push an event in between -> no new buffer and no new event */
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new_empty ("custom1"))));
  fail_unless (g_list_length (buffers) == 5 && g_list_length (events) == 3);

  /* output 1 subframe -> one new buffer and no new events */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 1);
  fail_unless (g_list_length (buffers) == 6 && g_list_length (events) == 3);

  /* push another custom event in between , no new event should appear until the next frame is handled */
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new_empty ("custom2"))));
  fail_unless (g_list_length (buffers) == 6 && g_list_length (events) == 3);

  /* output 2 subframes -> 2 new buffers and no new events */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 2);
  fail_unless (g_list_length (buffers) == 8 && g_list_length (events) == 3);

  /* output 1 last subframe -> 1 new buffers and no new events */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 1);
  fail_unless (g_list_length (buffers) == 9 && g_list_length (events) == 3);

  /* push a third buffer -> no new buffer and no new events (still pending two custom events) */
  buffer = create_test_buffer (2);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  fail_unless (g_list_length (buffers) == 9 && g_list_length (events) == 3);

  /* output 1 subframes -> 1 new buffer and 2 custom events from the last input frame */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 1);
  fail_unless (g_list_length (buffers) == 10 && g_list_length (events) == 5);
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  3)->data)) == GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  4)->data)) == GST_EVENT_CUSTOM_DOWNSTREAM);

  /* push another custom event in between , no new event should appear until eos */
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new_empty ("custom3"))));
  fail_unless (g_list_length (buffers) == 10 && g_list_length (events) == 5);

  /* output 3 subframes -> 3 new buffer and no new events */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 3);
  fail_unless (g_list_length (buffers) == 13 && g_list_length (events) == 5);

  /* push a force key-unit event */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));

  /* Create a new buffer which should be a key unit -> no new buffer and no new event */
  buffer = create_test_buffer (3);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  fail_unless (g_list_length (buffers) == 13 && g_list_length (events) == 5);

  /*  output 2 subframes -> 3 new buffer(one header and two subframes and two events key-unit and custom3  */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 2);
  fail_unless (g_list_length (buffers) == 16 && g_list_length (events) == 7);

  /*  output 2 subframes -> 2 new buffer corresponding the two last subframes */
  gst_video_encoder_tester_output_step_by_step (GST_VIDEO_ENCODER (enc),
      enc_tester->last_frame, 2);
  fail_unless (g_list_length (buffers) == 18 && g_list_length (events) == 7);

  /* push eos event -> 1 new event ( eos) */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));
  fail_unless (g_list_length (buffers) == 18 && g_list_length (events) == 8);

  /* check the order of the last events received */
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  6)->data)) == GST_EVENT_CUSTOM_DOWNSTREAM);
  fail_unless (GST_EVENT_TYPE ((GstEvent *) (g_list_nth (events,
                  7)->data)) == GST_EVENT_EOS);

  /* check that only last subframe owns the GST_VIDEO_BUFFER_FLAG_MARKER flag */
  header_found = 0;
  for (iter = buffers, i = 0; iter; iter = g_list_next (iter), i++) {
    buffer = (GstBuffer *) (iter->data);
    if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_HEADER)) {
      if ((i - header_found) % subframes == (subframes - 1))
        fail_unless (GST_BUFFER_FLAG_IS_SET (buffer,
                GST_VIDEO_BUFFER_FLAG_MARKER));
      else
        fail_unless (!GST_BUFFER_FLAG_IS_SET (buffer,
                GST_VIDEO_BUFFER_FLAG_MARKER));
    } else {
      fail_unless (!GST_BUFFER_FLAG_IS_SET (buffer,
              GST_VIDEO_BUFFER_FLAG_MARKER));
      header_found++;
    }

    /* Only the 0th (header), 1st, 13th (header) and 14th buffer should be keyframes */
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
      fail_if (i == 0 || i == 1 || i == 13 || i == 14);
    } else {
      fail_unless (i == 0 || i == 1 || i == 13 || i == 14);
    }
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

GST_START_TEST (videoencoder_force_keyunit_handling)
{
  GstSegment segment;
  GstBuffer *buffer;
  GList *l;
  gint i;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push the first buffer */
  buffer = create_test_buffer (0);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 1);

  buffer = create_test_buffer (1);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 2);

  /* send a force-keyunit event, the next buffer should be a keyframe now */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));

  buffer = create_test_buffer (2);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 3);

  buffer = create_test_buffer (3);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 4);

  /* send multiple force-keyunit events now, this should still only cause a
   * single keyframe */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));

  buffer = create_test_buffer (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 5);

  buffer = create_test_buffer (5);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 6);

  /* send a force-keyunit event for the running time of the next buffer */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (6, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));

  buffer = create_test_buffer (6);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 7);

  buffer = create_test_buffer (7);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 8);

  /* send a force-keyunit event for the running time of the next buffer
   * and another one right before. This should only cause a single keyframe
   * again */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (8, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (8, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N) - 10 * GST_MSECOND, TRUE, 1)));

  buffer = create_test_buffer (8);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 9);

  buffer = create_test_buffer (9);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 10);

  /* send a force-keyunit event for the 12th buffer, see below */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (12, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));

  /* send two force-keyunit events. This should only cause a single keyframe
   * again */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (10, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (10, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N) - 10 * GST_MSECOND, TRUE, 1)));

  buffer = create_test_buffer (10);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 11);

  buffer = create_test_buffer (11);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 12);

  /* we already sent a force-keyunit event for the 12th buffer long ago */
  buffer = create_test_buffer (12);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 13);

  /* we already received a keyframe after the given time, so the next frame
   * is not going to be another keyframe */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (12, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));

  buffer = create_test_buffer (13);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = NULL;

  fail_unless_equals_int (g_list_length (buffers), 14);

  /* every second buffer should be a keyframe */
  for (l = buffers, i = 0; l; l = l->next, i++) {
    if (i % 2 == 0)
      fail_if (GST_BUFFER_FLAG_IS_SET (l->data, GST_BUFFER_FLAG_DELTA_UNIT));
    else
      fail_unless (GST_BUFFER_FLAG_IS_SET (l->data,
              GST_BUFFER_FLAG_DELTA_UNIT));
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

GST_START_TEST (videoencoder_force_keyunit_min_interval)
{
  GstSegment segment;
  GstBuffer *buffer;
  GList *l;
  gint i;

  setup_videoencodertester ();

  gst_pad_set_active (mysrcpad, TRUE);
  /* Only one keyframe request every 3 frames at most */
  g_object_set (enc, "min-force-key-unit-interval", 100 * GST_MSECOND, NULL);
  gst_element_set_state (enc, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push the first two buffers */
  buffer = create_test_buffer (0);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  buffer = create_test_buffer (1);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* send a force-keyunit event, the next buffer should not be a keyframe yet */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));

  buffer = create_test_buffer (2);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* this buffer should be a keyframe */
  buffer = create_test_buffer (3);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* send two force-keyunit event, the 6th buffer should be a keyframe */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 1)));

  buffer = create_test_buffer (4);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (5);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (6);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* send a force-keyunit event for the 9th buffer, this should happen */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (9, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));
  buffer = create_test_buffer (7);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (8);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (9);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* send a force-keyunit event for the 11th buffer, this should happen on the
   * 12th */
  fail_unless (gst_pad_push_event (mysinkpad,
          gst_video_event_new_upstream_force_key_unit
          (gst_util_uint64_scale_round (11, GST_SECOND * TEST_VIDEO_FPS_D,
                  TEST_VIDEO_FPS_N), TRUE, 1)));
  buffer = create_test_buffer (10);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (11);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  buffer = create_test_buffer (12);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  fail_unless_equals_int (g_list_length (buffers), 13);

  /* every third buffer should be a keyframe */
  for (l = buffers, i = 0; l; l = l->next, i++) {
    if (i % 3 == 0)
      fail_if (GST_BUFFER_FLAG_IS_SET (l->data, GST_BUFFER_FLAG_DELTA_UNIT));
    else
      fail_unless (GST_BUFFER_FLAG_IS_SET (l->data,
              GST_BUFFER_FLAG_DELTA_UNIT));
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videoencodertest ();
}

GST_END_TEST;

GST_START_TEST (videoencoder_hdr_metadata)
{
  const gchar *mdi_str =
      "35399:14599:8500:39850:6550:2300:15634:16450:10000000:1";
  const gchar *cll_str = "1000:50";
  gint i;

  /* Check that HDR metadata get passed to src pad no matter if negotiate gets
   * called from gst_video_encoder_finish_frame() or GstVideoEncoder::set_format
   */
  for (i = 1; i >= 0; --i) {
    GstVideoMasteringDisplayInfo mdi;
    GstVideoContentLightLevel cll;
    GstSegment segment;
    GstCaps *caps;
    GstStructure *s;
    const gchar *str;

    setup_videoencodertester ();
    GST_VIDEO_ENCODER_TESTER (enc)->negotiate_in_set_format = i;

    gst_pad_set_active (mysrcpad, TRUE);
    gst_element_set_state (enc, GST_STATE_PLAYING);
    gst_pad_set_active (mysinkpad, TRUE);

    fail_unless (gst_pad_push_event (mysrcpad,
            gst_event_new_stream_start ("id")));

    gst_video_mastering_display_info_from_string (&mdi, mdi_str);
    gst_video_content_light_level_from_string (&cll, cll_str);

    caps = create_test_caps ();
    gst_video_mastering_display_info_add_to_caps (&mdi, caps);
    gst_video_content_light_level_add_to_caps (&cll, caps);

    fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));
    gst_caps_unref (caps);

    gst_segment_init (&segment, GST_FORMAT_TIME);
    fail_unless (gst_pad_push_event (mysrcpad,
            gst_event_new_segment (&segment)));

    gst_pad_push (mysrcpad, create_test_buffer (0));

    caps = gst_pad_get_current_caps (mysinkpad);

    s = gst_caps_get_structure (caps, 0);
    fail_unless (str = gst_structure_get_string (s, "mastering-display-info"));
    fail_unless_equals_string (str, mdi_str);

    fail_unless (str = gst_structure_get_string (s, "content-light-level"));
    fail_unless_equals_string (str, cll_str);

    gst_caps_unref (caps);

    cleanup_videoencodertest ();
  }
}

GST_END_TEST;

static Suite *
gst_videoencoder_suite (void)
{
  Suite *s = suite_create ("GstVideoEncoder");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, videoencoder_playback);

  tcase_add_test (tc, videoencoder_tags_before_eos);
  tcase_add_test (tc, videoencoder_events_before_eos);
  tcase_add_test (tc, videoencoder_flush_events);
  tcase_add_test (tc, videoencoder_pre_push_fails);
  tcase_add_test (tc, videoencoder_qos);
  tcase_add_test (tc, videoencoder_playback_subframes);
  tcase_add_test (tc, videoencoder_playback_events_subframes);
  tcase_add_test (tc, videoencoder_force_keyunit_handling);
  tcase_add_test (tc, videoencoder_force_keyunit_min_interval);
  tcase_add_test (tc, videoencoder_hdr_metadata);

  return s;
}

GST_CHECK_MAIN (gst_videoencoder);
