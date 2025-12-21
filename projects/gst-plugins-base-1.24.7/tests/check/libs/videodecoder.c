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

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

#define RESTRICTED_CAPS_WIDTH 800
#define RESTRICTED_CAPS_HEIGHT 600
#define RESTRICTED_CAPS_FPS_N 30
#define RESTRICTED_CAPS_FPS_D 1
static GstStaticPadTemplate sinktemplate_restricted =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, width=(int)800, height=(int)600,"
        " framerate=(fraction)30/1")
    );

static GstStaticPadTemplate sinktemplate_with_range =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, width=(int)[1,800], height=(int)[1,600],"
        " framerate=(fraction)[1/1, 30/1]")
    );

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-test-custom")
    );

static GstPad *mysrcpad, *mysinkpad;
static GstElement *dec;
static GList *events = NULL;

#define TEST_VIDEO_WIDTH 640
#define TEST_VIDEO_HEIGHT 480
#define TEST_VIDEO_FPS_N 30
#define TEST_VIDEO_FPS_D 1

#define GST_VIDEO_DECODER_TESTER_TYPE gst_video_decoder_tester_get_type()
static GType gst_video_decoder_tester_get_type (void);

typedef struct _GstVideoDecoderTester GstVideoDecoderTester;
typedef struct _GstVideoDecoderTesterClass GstVideoDecoderTesterClass;

struct _GstVideoDecoderTester
{
  GstVideoDecoder parent;

  guint64 last_buf_num;
  guint64 last_kf_num;
  gboolean set_output_state;
  gboolean subframe_mode;
};

struct _GstVideoDecoderTesterClass
{
  GstVideoDecoderClass parent_class;
};

G_DEFINE_TYPE (GstVideoDecoderTester, gst_video_decoder_tester,
    GST_TYPE_VIDEO_DECODER);

static gboolean
gst_video_decoder_tester_start (GstVideoDecoder * dec)
{
  GstVideoDecoderTester *dectester = (GstVideoDecoderTester *) dec;

  dectester->last_buf_num = -1;
  dectester->last_kf_num = -1;
  dectester->set_output_state = TRUE;

  return TRUE;
}

static gboolean
gst_video_decoder_tester_stop (GstVideoDecoder * dec)
{
  return TRUE;
}

static gboolean
gst_video_decoder_tester_flush (GstVideoDecoder * dec)
{
  GstVideoDecoderTester *dectester = (GstVideoDecoderTester *) dec;

  dectester->last_buf_num = -1;
  dectester->last_kf_num = -1;

  return TRUE;
}

static gboolean
gst_video_decoder_tester_set_format (GstVideoDecoder * dec,
    GstVideoCodecState * state)
{
  GstVideoDecoderTester *dectester = (GstVideoDecoderTester *) dec;

  if (dectester->set_output_state) {
    GstVideoCodecState *res = gst_video_decoder_set_output_state (dec,
        GST_VIDEO_FORMAT_GRAY8, TEST_VIDEO_WIDTH, TEST_VIDEO_HEIGHT, NULL);
    gst_video_codec_state_unref (res);
  }

  return TRUE;
}

static GstFlowReturn
gst_video_decoder_tester_handle_frame (GstVideoDecoder * dec,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoderTester *dectester = (GstVideoDecoderTester *) dec;
  guint64 input_num;
  guint8 *data;
  gint size;
  GstMapInfo map;
  gboolean last_subframe = GST_BUFFER_FLAG_IS_SET (frame->input_buffer,
      GST_VIDEO_BUFFER_FLAG_MARKER);

  if (gst_video_decoder_get_subframe_mode (dec) && !last_subframe) {
    if (!GST_CLOCK_TIME_IS_VALID (frame->pts))
      return gst_video_decoder_drop_subframe (dec, frame);
    goto done;
  }

  gst_buffer_map (frame->input_buffer, &map, GST_MAP_READ);

  input_num = *((guint64 *) map.data);

  if ((input_num == dectester->last_buf_num + 1
          && dectester->last_buf_num != -1)
      || !GST_BUFFER_FLAG_IS_SET (frame->input_buffer,
          GST_BUFFER_FLAG_DELTA_UNIT) || last_subframe) {

    /* the output is gray8 */
    size = TEST_VIDEO_WIDTH * TEST_VIDEO_HEIGHT;
    data = g_malloc0 (size);

    memcpy (data, map.data, sizeof (guint64));

    frame->output_buffer = gst_buffer_new_wrapped (data, size);
    frame->pts = GST_BUFFER_PTS (frame->input_buffer);
    frame->duration = GST_BUFFER_DURATION (frame->input_buffer);
    dectester->last_buf_num = input_num;
    if (!GST_BUFFER_FLAG_IS_SET (frame->input_buffer,
            GST_BUFFER_FLAG_DELTA_UNIT))
      dectester->last_kf_num = input_num;
  }

  gst_buffer_unmap (frame->input_buffer, &map);
  if (GST_CLOCK_TIME_IS_VALID (frame->pts)) {

    if (gst_video_decoder_get_subframe_mode (dec) && last_subframe)
      gst_video_decoder_have_last_subframe (dec, frame);

    if (frame->output_buffer)
      return gst_video_decoder_finish_frame (dec, frame);
  } else {
    return gst_video_decoder_drop_frame (dec, frame);

  }


done:
  gst_video_codec_frame_unref (frame);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_video_decoder_tester_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos)
{
  gint av;

  av = gst_adapter_available (adapter);

  /* and pass along all */
  gst_video_decoder_add_to_frame (decoder, av);
  return gst_video_decoder_have_frame (decoder);
}

static void
gst_video_decoder_tester_class_init (GstVideoDecoderTesterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *videodecoder_class = GST_VIDEO_DECODER_CLASS (klass);

  static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-test-custom"));

  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw"));

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  gst_element_class_set_metadata (element_class,
      "VideoDecoderTester", "Decoder/Video", "yep", "me");

  videodecoder_class->start = gst_video_decoder_tester_start;
  videodecoder_class->stop = gst_video_decoder_tester_stop;
  videodecoder_class->flush = gst_video_decoder_tester_flush;
  videodecoder_class->handle_frame = gst_video_decoder_tester_handle_frame;
  videodecoder_class->set_format = gst_video_decoder_tester_set_format;
  videodecoder_class->parse = gst_video_decoder_tester_parse;
}

static void
gst_video_decoder_tester_init (GstVideoDecoderTester * tester)
{
}

static gboolean
_mysinkpad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  events = g_list_append (events, event);
  return TRUE;
}

static void
setup_videodecodertester (GstStaticPadTemplate * sinktmpl,
    GstStaticPadTemplate * srctmpl)
{
  if (sinktmpl == NULL)
    sinktmpl = &sinktemplate;
  if (srctmpl == NULL)
    srctmpl = &srctemplate;

  dec = g_object_new (GST_VIDEO_DECODER_TESTER_TYPE, NULL);
  mysrcpad = gst_check_setup_src_pad (dec, srctmpl);
  mysinkpad = gst_check_setup_sink_pad (dec, sinktmpl);

  gst_pad_set_event_function (mysinkpad, _mysinkpad_event);
}

static void
cleanup_videodecodertest (void)
{
  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (dec);
  gst_check_teardown_sink_pad (dec);
  gst_check_teardown_element (dec);

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

static void
send_startup_events (void)
{
  GstCaps *caps;

  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_stream_start ("randomvalue")));

  /* push caps */
  caps =
      gst_caps_new_simple ("video/x-test-custom", "width", G_TYPE_INT,
      TEST_VIDEO_WIDTH, "height", G_TYPE_INT, TEST_VIDEO_HEIGHT, "framerate",
      GST_TYPE_FRACTION, TEST_VIDEO_FPS_N, TEST_VIDEO_FPS_D, NULL);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);
}

#define NUM_BUFFERS 1000
#define NUM_SUB_BUFFERS 4

GST_START_TEST (videodecoder_playback)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GList *iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
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

  cleanup_videodecodertest ();
}

GST_END_TEST;


GST_START_TEST (videodecoder_playback_with_events)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i;
  GList *iter;
  GList *events_iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
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

  /* check that all buffers were received by our source pad */
  iter = buffers;
  for (i = 0; i < NUM_BUFFERS; i++) {
    if (i % 10 == 0) {
      guint tag_v;
      GstEvent *tag_event = events_iter->data;
      GstTagList *taglist = NULL;

      gst_event_parse_tag (tag_event, &taglist);

      fail_unless (gst_tag_list_get_uint (taglist, GST_TAG_TRACK_NUMBER,
              &tag_v));
      fail_unless (tag_v == i);

      events_iter = g_list_next (events_iter);
    } else {
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
      iter = g_list_next (iter);
    }
  }
  fail_unless (iter == NULL);

  /* check that EOS was received */
  {
    GstEvent *eos = events_iter->data;

    fail_unless (GST_EVENT_TYPE (eos) == GST_EVENT_EOS);
    events_iter = g_list_next (events_iter);
  }

  fail_unless (events_iter == NULL);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videodecodertest ();
}

GST_END_TEST;

GST_START_TEST (videodecoder_flush_events)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i;
  GList *events_iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
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

  cleanup_videodecodertest ();
}

GST_END_TEST;


/* Check https://bugzilla.gnome.org/show_bug.cgi?id=721835 */
GST_START_TEST (videodecoder_playback_first_frames_not_decoded)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i = 0;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push a buffer, to have the segment attached to it.
   * unfortunately this buffer can't be decoded as it isn't a keyframe */
  buffer = create_test_buffer (i++);
  GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  /* now be evil and ask this frame to be released
   * this frame has the segment event attached to it, and the
   * segment shouldn't disappear with it */
  {
    GList *l, *ol;

    ol = l = gst_video_decoder_get_frames (GST_VIDEO_DECODER (dec));
    fail_unless (g_list_length (l) == 1);
    while (l) {
      GstVideoCodecFrame *tmp = l->data;

      gst_video_decoder_release_frame (GST_VIDEO_DECODER (dec), tmp);

      l = g_list_next (l);
    }
    g_list_free (ol);
  }

  buffer = create_test_buffer (i++);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  fail_unless (g_list_length (buffers) == 1);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videodecodertest ();
}

GST_END_TEST;

GST_START_TEST (videodecoder_buffer_after_segment)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GstClockTime pos;
  GList *iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.stop = GST_SECOND;
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers until we fill our segment */
  i = 0;
  pos = 0;
  while (pos < GST_SECOND) {
    buffer = create_test_buffer (i++);

    pos = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* pushing the next buffer should result in EOS */
  buffer = create_test_buffer (i);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_EOS);

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that all buffers were received by our source pad */
  fail_unless (g_list_length (buffers) == i);
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

  cleanup_videodecodertest ();
}

GST_END_TEST;

/* make sure that the segment event is pushed before the gap */
GST_START_TEST (videodecoder_first_data_is_gap)
{
  GstSegment segment;
  GList *events_iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push a gap */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_gap (0,
              GST_SECOND)));
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

  /* Make sure the gap was pushed */
  {
    GstEvent *gap = events_iter->data;
    fail_unless (GST_EVENT_TYPE (gap) == GST_EVENT_GAP);
    events_iter = g_list_next (events_iter);
  }
  fail_unless (events_iter == NULL);

  cleanup_videodecodertest ();
}

GST_END_TEST;

static void
videodecoder_backwards_playback (gboolean subframe)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GList *iter;
  guint num_subframes = 1;
  guint num_buffers;

  if (subframe)
    num_subframes = 2;
  num_buffers = NUM_BUFFERS / num_subframes;

  setup_videodecodertester (NULL, NULL);

  if (num_subframes > 1) {
    gst_video_decoder_set_subframe_mode (GST_VIDEO_DECODER (dec), TRUE);
  }

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment with -1 rate */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.rate = -1.0;
  segment.stop = (num_buffers + 1) * gst_util_uint64_scale_round (GST_SECOND,
      TEST_VIDEO_FPS_D, TEST_VIDEO_FPS_N);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers, the data is actually a number so we can track them */
  i = num_buffers * num_subframes;
  while (i > 0) {
    gint target = i;
    gint j;

    /* push groups of 10 buffers
     * every number that is divisible by 10 is set as a discont,
     * if it is divisible by 20 it is also a keyframe
     *
     * The logic here is that the current i is the target, and then
     * it pushes buffers from 'target - 10' up to target.
     */
    for (j = MAX (target - 10, 0); j < target; j++) {
      GstBuffer *buffer = create_test_buffer (j / num_subframes);
      if ((j + 1) % num_subframes == 0)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_MARKER);
      if (j % 10 == 0)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
      if (j % 20 != 0)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

      fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
      i--;
    }
  }

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* check that all buffers were received by our source pad */
  fail_unless (g_list_length (buffers) == num_buffers);
  i = num_buffers - 1;
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
    i--;
  }

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videodecodertest ();
}

GST_START_TEST (videodecoder_backwards_playback_normal)
{
  videodecoder_backwards_playback (FALSE);
}

GST_END_TEST;

GST_START_TEST (videodecoder_backwards_playback_subframes)
{
  videodecoder_backwards_playback (TRUE);
}

GST_END_TEST;

GST_START_TEST (videodecoder_backwards_buffer_after_segment)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint64 i;
  GstClockTime pos;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment with -1 rate */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.rate = -1.0;
  segment.start = GST_SECOND;
  segment.stop = (NUM_BUFFERS + 1) * gst_util_uint64_scale_round (GST_SECOND,
      TEST_VIDEO_FPS_D, TEST_VIDEO_FPS_N);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push buffers, the data is actually a number so we can track them */
  i = NUM_BUFFERS;
  pos = segment.stop;
  while (pos >= GST_SECOND) {
    gint target = i;
    gint j;

    g_assert (i > 0);

    /* push groups of 10 buffers
     * every number that is divisible by 10 is set as a discont,
     * if it is divisible by 20 it is also a keyframe
     *
     * The logic here is that the current i is the target, and then
     * it pushes buffers from 'target - 10' up to target.
     */
    for (j = MAX (target - 10, 0); j < target; j++) {
      buffer = create_test_buffer (j);

      pos = MIN (GST_BUFFER_TIMESTAMP (buffer), pos);
      if (j % 10 == 0)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
      if (j % 20 != 0)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

      fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
      i--;
    }
  }

  /* push a discont buffer so it flushes the decoding */
  buffer = create_test_buffer (i - 10);
  GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_EOS);

  /* check that the last received buffer doesn't contain a
   * timestamp before the segment */
  buffer = g_list_last (buffers)->data;
  fail_unless (GST_BUFFER_TIMESTAMP (buffer) <= segment.start
      && GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer) >
      segment.start);

  /* flush our decoded data queue */
  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  fail_unless (buffers == NULL);

  cleanup_videodecodertest ();
}

GST_END_TEST;


GST_START_TEST (videodecoder_query_caps_with_fixed_caps_peer)
{
  GstCaps *caps;
  GstCaps *filter;
  GstStructure *structure;
  gint width, height, fps_n, fps_d;

  setup_videodecodertester (&sinktemplate_restricted, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  caps = gst_pad_peer_query_caps (mysrcpad, NULL);
  fail_unless (caps != NULL);

  structure = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_get_int (structure, "width", &width));
  fail_unless (gst_structure_get_int (structure, "height", &height));
  fail_unless (gst_structure_get_fraction (structure, "framerate", &fps_n,
          &fps_d));
  /* match our restricted caps values */
  fail_unless (width == RESTRICTED_CAPS_WIDTH);
  fail_unless (height == RESTRICTED_CAPS_HEIGHT);
  fail_unless (fps_n == RESTRICTED_CAPS_FPS_N);
  fail_unless (fps_d == RESTRICTED_CAPS_FPS_D);
  gst_caps_unref (caps);

  filter = gst_caps_new_simple ("video/x-custom-test", "width", G_TYPE_INT,
      1000, "height", G_TYPE_INT, 1000, "framerate", GST_TYPE_FRACTION,
      1000, 1, NULL);
  caps = gst_pad_peer_query_caps (mysrcpad, filter);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_is_empty (caps));
  gst_caps_unref (caps);
  gst_caps_unref (filter);

  cleanup_videodecodertest ();
}

GST_END_TEST;

static void
_get_int_range (GstStructure * s, const gchar * field, gint * min_v,
    gint * max_v)
{
  const GValue *value;

  value = gst_structure_get_value (s, field);
  fail_unless (value != NULL);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (value));

  *min_v = gst_value_get_int_range_min (value);
  *max_v = gst_value_get_int_range_max (value);
}

static void
_get_fraction_range (GstStructure * s, const gchar * field, gint * fps_n_min,
    gint * fps_d_min, gint * fps_n_max, gint * fps_d_max)
{
  const GValue *value;
  const GValue *min_v, *max_v;

  value = gst_structure_get_value (s, field);
  fail_unless (value != NULL);
  fail_unless (GST_VALUE_HOLDS_FRACTION_RANGE (value));

  min_v = gst_value_get_fraction_range_min (value);
  fail_unless (GST_VALUE_HOLDS_FRACTION (min_v));
  *fps_n_min = gst_value_get_fraction_numerator (min_v);
  *fps_d_min = gst_value_get_fraction_denominator (min_v);

  max_v = gst_value_get_fraction_range_max (value);
  fail_unless (GST_VALUE_HOLDS_FRACTION (max_v));
  *fps_n_max = gst_value_get_fraction_numerator (max_v);
  *fps_d_max = gst_value_get_fraction_denominator (max_v);
}

GST_START_TEST (videodecoder_query_caps_with_range_caps_peer)
{
  GstCaps *caps;
  GstCaps *filter;
  GstStructure *structure;
  gint width, height, fps_n, fps_d;
  gint width_min, height_min, fps_n_min, fps_d_min;
  gint width_max, height_max, fps_n_max, fps_d_max;

  setup_videodecodertester (&sinktemplate_with_range, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  caps = gst_pad_peer_query_caps (mysrcpad, NULL);
  fail_unless (caps != NULL);

  structure = gst_caps_get_structure (caps, 0);
  _get_int_range (structure, "width", &width_min, &width_max);
  _get_int_range (structure, "height", &height_min, &height_max);
  _get_fraction_range (structure, "framerate", &fps_n_min, &fps_d_min,
      &fps_n_max, &fps_d_max);
  fail_unless (width_min == 1);
  fail_unless (width_max == RESTRICTED_CAPS_WIDTH);
  fail_unless (height_min == 1);
  fail_unless (height_max == RESTRICTED_CAPS_HEIGHT);
  fail_unless (fps_n_min == 1);
  fail_unless (fps_d_min == 1);
  fail_unless (fps_n_max == RESTRICTED_CAPS_FPS_N);
  fail_unless (fps_d_max == RESTRICTED_CAPS_FPS_D);
  gst_caps_unref (caps);

  /* query with a fixed filter */
  filter = gst_caps_new_simple ("video/x-test-custom", "width", G_TYPE_INT,
      RESTRICTED_CAPS_WIDTH, "height", G_TYPE_INT, RESTRICTED_CAPS_HEIGHT,
      "framerate", GST_TYPE_FRACTION, RESTRICTED_CAPS_FPS_N,
      RESTRICTED_CAPS_FPS_D, NULL);
  caps = gst_pad_peer_query_caps (mysrcpad, filter);
  fail_unless (caps != NULL);
  structure = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_get_int (structure, "width", &width));
  fail_unless (gst_structure_get_int (structure, "height", &height));
  fail_unless (gst_structure_get_fraction (structure, "framerate", &fps_n,
          &fps_d));
  fail_unless (width == RESTRICTED_CAPS_WIDTH);
  fail_unless (height == RESTRICTED_CAPS_HEIGHT);
  fail_unless (fps_n == RESTRICTED_CAPS_FPS_N);
  fail_unless (fps_d == RESTRICTED_CAPS_FPS_D);
  gst_caps_unref (caps);
  gst_caps_unref (filter);

  /* query with a fixed filter that will lead to empty result */
  filter = gst_caps_new_simple ("video/x-test-custom", "width", G_TYPE_INT,
      1000, "height", G_TYPE_INT, 1000, "framerate", GST_TYPE_FRACTION,
      1000, 1, NULL);
  caps = gst_pad_peer_query_caps (mysrcpad, filter);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_is_empty (caps));
  gst_caps_unref (caps);
  gst_caps_unref (filter);

  cleanup_videodecodertest ();
}

GST_END_TEST;

#define GETCAPS_CAPS_STR "video/x-test-custom, somefield=(string)getcaps"
static GstCaps *
_custom_video_decoder_getcaps (GstVideoDecoder * dec, GstCaps * filter)
{
  return gst_caps_from_string (GETCAPS_CAPS_STR);
}

GST_START_TEST (videodecoder_query_caps_with_custom_getcaps)
{
  GstCaps *caps;
  GstVideoDecoderClass *klass;
  GstCaps *expected_caps;

  setup_videodecodertester (&sinktemplate_restricted, NULL);

  klass = GST_VIDEO_DECODER_CLASS (GST_VIDEO_DECODER_GET_CLASS (dec));
  klass->getcaps = _custom_video_decoder_getcaps;

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  caps = gst_pad_peer_query_caps (mysrcpad, NULL);
  fail_unless (caps != NULL);

  expected_caps = gst_caps_from_string (GETCAPS_CAPS_STR);
  fail_unless (gst_caps_is_equal (expected_caps, caps));
  gst_caps_unref (expected_caps);
  gst_caps_unref (caps);

  cleanup_videodecodertest ();
}

GST_END_TEST;

static const gchar *test_default_caps[][2] = {
  {
        "video/x-test-custom",
      "video/x-raw, format=I420, width=1280, height=720, framerate=0/1, multiview-mode=mono"}, {
        "video/x-test-custom, width=1000",
      "video/x-raw, format=I420, width=1000, height=720, framerate=0/1, multiview-mode=mono"}, {
        "video/x-test-custom, height=500",
      "video/x-raw, format=I420, width=1280, height=500, framerate=0/1, multiview-mode=mono"}, {
        "video/x-test-custom, framerate=10/1",
      "video/x-raw, format=I420, width=1280, height=720, framerate=10/1, multiview-mode=mono"}, {
        "video/x-test-custom, pixel-aspect-ratio=2/1",
      "video/x-raw, format=I420, width=1280, height=720, framerate=0/1,"
        "pixel-aspect-ratio=2/1, multiview-mode=mono"}
};

GST_START_TEST (videodecoder_default_caps_on_gap_before_buffer)
{
  GstVideoDecoderTester *dec =
      g_object_new (GST_VIDEO_DECODER_TESTER_TYPE, NULL);
  GstHarness *h =
      gst_harness_new_with_element (GST_ELEMENT (dec), "sink", "src");
  GstEvent *event;
  GstCaps *caps1, *caps2;
  GstVideoInfo info1, info2;

  /* Don't set output state since we want trigger the default output caps */
  dec->set_output_state = FALSE;
  gst_harness_set_src_caps_str (h, test_default_caps[__i__][0]);

  fail_unless (gst_harness_push_event (h, gst_event_new_gap (0, GST_SECOND)));

  fail_unless_equals_int (gst_harness_events_received (h), 4);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_STREAM_START);
  gst_event_unref (event);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_CAPS);
  gst_event_unref (event);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT);
  gst_event_unref (event);

  event = gst_harness_pull_event (h);
  fail_unless (GST_EVENT_TYPE (event) == GST_EVENT_GAP);
  gst_event_unref (event);

  caps1 = gst_pad_get_current_caps (h->sinkpad);
  caps2 = gst_caps_from_string (test_default_caps[__i__][1]);
  gst_video_info_from_caps (&info1, caps1);
  gst_video_info_from_caps (&info2, caps2);

  gst_caps_unref (caps1);
  gst_caps_unref (caps2);

  fail_unless (gst_video_info_is_equal (&info1, &info2));

  gst_harness_teardown (h);
  gst_object_unref (dec);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_event_order)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i = 0;
  GList *events_iter;

  setup_videodecodertester (NULL, NULL);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push 5 buffer with one event each. All buffers except the last
   * one are dropped in some way, so the events are collected in various
   * places. The order must be preserved.
   * With the first buffer the segment event is added to the pending event
   * list to ensure that incorrect ordering can be detected for later
   * events.
   */
  for (i = 0; i < 9; i++) {
    if (i % 2 == 0) {
      buffer = create_test_buffer (i);
      if (i < 8)
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
      if (i < 6) {
        GList *l, *ol;

        ol = l = gst_video_decoder_get_frames (GST_VIDEO_DECODER (dec));
        fail_unless (g_list_length (l) == 1);
        while (l) {
          GstVideoCodecFrame *tmp = l->data;

          if (i < 4)
            gst_video_decoder_release_frame (GST_VIDEO_DECODER (dec), tmp);
          else
            gst_video_decoder_drop_frame (GST_VIDEO_DECODER (dec), tmp);

          l = g_list_next (l);
        }
        g_list_free (ol);
      }
    } else {
      GstTagList *tags;
      tags = gst_tag_list_new (GST_TAG_TRACK_NUMBER, i, NULL);
      fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_tag (tags)));
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

  /* Check the order of the tag events */
  for (i = 1; i < 9; i += 2) {
    guint tag_v;
    GstEvent *tag_event = events_iter->data;
    GstTagList *taglist = NULL;

    fail_unless (GST_EVENT_TYPE (tag_event) == GST_EVENT_TAG);
    gst_event_parse_tag (tag_event, &taglist);

    fail_unless (gst_tag_list_get_uint (taglist, GST_TAG_TRACK_NUMBER, &tag_v));
    fail_unless (tag_v == i);

    events_iter = g_list_next (events_iter);
  }

  /* check that EOS was received */
  {
    GstEvent *eos = events_iter->data;

    fail_unless (GST_EVENT_TYPE (eos) == GST_EVENT_EOS);
    events_iter = g_list_next (events_iter);
  }

  fail_unless (events_iter == NULL);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videodecodertest ();
}

GST_END_TEST;

/*
 * MODE_META_COPY: takes an extra ref to the input buffer to check metas
 *                 are copied to a writable buffer.
 *                 see: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/4912
 */
typedef enum
{
  MODE_NONE = 0,
  MODE_SUBFRAMES = 1,
  MODE_PACKETIZED = 1 << 1,
  MODE_META_ROI = 1 << 2,
  MODE_META_COPY = 1 << 3,
} SubframeMode;

static void
videodecoder_playback_subframe_mode (SubframeMode mode)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i;
  GList *iter;
  gint num_buffers = NUM_BUFFERS;
  gint num_subframes = 1;
  GList *list;
  gint num_roi_metas = 0;

  setup_videodecodertester (NULL, NULL);

  /* Allow to test combination of subframes and packetized configuration
   * 0-0: no subframes not packetized.
   * 0-1: subframes not packetized.
   * 1-0: no subframes packetized.
   * 1-1: subframes and packetized.
   */
  if (mode & MODE_SUBFRAMES) {
    gst_video_decoder_set_subframe_mode (GST_VIDEO_DECODER (dec), TRUE);
    num_subframes = NUM_SUB_BUFFERS;
  } else {
    gst_video_decoder_set_subframe_mode (GST_VIDEO_DECODER (dec), FALSE);
    num_subframes = 1;
  }
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec),
      mode & MODE_PACKETIZED ? TRUE : FALSE);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push header only in packetized subframe mode */
  if (mode == (MODE_PACKETIZED | MODE_SUBFRAMES)) {
    buffer = gst_buffer_new_and_alloc (0);
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_HEADER);
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < num_buffers; i++) {
    buffer = create_test_buffer (i / num_subframes);
    if ((i + 1) % num_subframes == 0)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_MARKER);
    if (mode & MODE_META_ROI)
      gst_buffer_add_video_region_of_interest_meta (buffer, "face", 0, 0, 10,
          10);

    /* Take an extra ref to check that we ensure buffer is writable when copying metas
     * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/4912
     */
    if (mode & MODE_META_COPY) {
      gst_buffer_ref (buffer);
    }
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
    fail_unless (gst_pad_push_event (mysrcpad,
            gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
                gst_structure_new_empty ("custom1"))));
    if (mode & MODE_META_COPY) {
      gst_buffer_unref (buffer);
    }
  }
  /* Send EOS */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* Test that no frames or pending events are remaining in the base class */
  list = gst_video_decoder_get_frames (GST_VIDEO_DECODER (dec));
  fail_unless (g_list_length (list) == 0);
  g_list_free_full (list, (GDestroyNotify) gst_video_codec_frame_unref);

  /* check that all buffers were received by our source pad 1 output buffer for 4 input buffer */
  fail_unless (g_list_length (buffers) == num_buffers / num_subframes);

  i = 0;
  for (iter = buffers; iter; iter = g_list_next (iter)) {
    GstMapInfo map;
    guint num;
    GstMeta *meta;
    gpointer state = NULL;

    buffer = iter->data;
    while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
      if (meta->info->api == GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE)
        num_roi_metas++;
    }
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    /* Test that the buffer is carrying the expected value 'num' */
    num = *(guint64 *) map.data;

    fail_unless (i == num);
    /* Test that the buffer metadata are correct */
    fail_unless (GST_BUFFER_PTS (buffer) == gst_util_uint64_scale_round (i,
            GST_SECOND * TEST_VIDEO_FPS_D, TEST_VIDEO_FPS_N));
    fail_unless (GST_BUFFER_DURATION (buffer) ==
        gst_util_uint64_scale_round (GST_SECOND, TEST_VIDEO_FPS_D,
            TEST_VIDEO_FPS_N));


    gst_buffer_unmap (buffer, &map);
    i++;
  }

  if (mode &= MODE_META_ROI)
    fail_unless (num_roi_metas == num_buffers);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;

  cleanup_videodecodertest ();
}

static void
videodecoder_playback_invalid_ts_subframe_mode (SubframeMode mode)
{
  GstSegment segment;
  GstBuffer *buffer;
  guint i;
  gint num_buffers = NUM_BUFFERS;
  gint num_subframes = 1;
  GList *list;

  setup_videodecodertester (NULL, NULL);

  /* Allow to test combination of subframes and packetized configuration
   * 0-0: no subframes not packetized.
   * 0-1: subframes not packetized.
   * 1-0: no subframes packetized.
   * 1-1: subframes and packetized.
   */
  if (mode & MODE_SUBFRAMES) {
    gst_video_decoder_set_subframe_mode (GST_VIDEO_DECODER (dec), TRUE);
    num_subframes = NUM_SUB_BUFFERS;
  }

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec),
      mode & MODE_PACKETIZED ? TRUE : FALSE);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_element_set_state (dec, GST_STATE_PLAYING);
  gst_pad_set_active (mysinkpad, TRUE);

  send_startup_events ();

  /* push a new segment */
  gst_segment_init (&segment, GST_FORMAT_TIME);

  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment)));

  /* push header only in packetized subframe mode */
  if (mode == (MODE_PACKETIZED | MODE_SUBFRAMES)) {
    buffer = gst_buffer_new_and_alloc (0);
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_HEADER);
    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
  }

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < num_buffers; i++) {
    buffer = create_test_buffer (i / num_subframes);
    GST_BUFFER_PTS (buffer) = GST_CLOCK_TIME_NONE;
    if ((i + 1) % num_subframes == 0)
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_MARKER);

    fail_unless (gst_pad_push (mysrcpad, buffer) == GST_FLOW_OK);
    fail_unless (gst_pad_push_event (mysrcpad,
            gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
                gst_structure_new_empty ("custom1"))));
  }
  /* Send EOS */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));

  /* Test that no frames or pending events are remaining in the base class */
  list = gst_video_decoder_get_frames (GST_VIDEO_DECODER (dec));
  fail_unless (g_list_length (list) == 0);
  g_list_free_full (list, (GDestroyNotify) gst_video_codec_frame_unref);

  /* check that all buffers were received by our source pad 1 output buffer for 4 input buffer */
  fail_unless (g_list_length (buffers) == 0);


  cleanup_videodecodertest ();
}

GST_START_TEST (videodecoder_playback_parsed)
{
  videodecoder_playback_subframe_mode (MODE_NONE);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_packetized)
{
  videodecoder_playback_subframe_mode (MODE_PACKETIZED);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_parsed_subframes)
{
  videodecoder_playback_subframe_mode (MODE_SUBFRAMES);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_packetized_subframes)
{
  videodecoder_playback_subframe_mode (MODE_SUBFRAMES | MODE_PACKETIZED);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_packetized_subframes_metadata)
{
  videodecoder_playback_subframe_mode (MODE_SUBFRAMES |
      MODE_PACKETIZED | MODE_META_ROI);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_packetized_subframes_metadata_copy)
{
  videodecoder_playback_subframe_mode (MODE_SUBFRAMES |
      MODE_PACKETIZED | MODE_META_ROI | MODE_META_COPY);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_invalid_ts_packetized)
{
  videodecoder_playback_invalid_ts_subframe_mode (MODE_PACKETIZED);
}

GST_END_TEST;

GST_START_TEST (videodecoder_playback_invalid_ts_packetized_subframes)
{
  videodecoder_playback_invalid_ts_subframe_mode (MODE_SUBFRAMES |
      MODE_PACKETIZED);
}

GST_END_TEST;



static Suite *
gst_videodecoder_suite (void)
{
  Suite *s = suite_create ("GstVideoDecoder");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, videodecoder_query_caps_with_fixed_caps_peer);
  tcase_add_test (tc, videodecoder_query_caps_with_range_caps_peer);
  tcase_add_test (tc, videodecoder_query_caps_with_custom_getcaps);

  tcase_add_test (tc, videodecoder_playback);
  tcase_add_test (tc, videodecoder_playback_with_events);
  tcase_add_test (tc, videodecoder_playback_first_frames_not_decoded);
  tcase_add_test (tc, videodecoder_buffer_after_segment);
  tcase_add_test (tc, videodecoder_first_data_is_gap);

  tcase_add_test (tc, videodecoder_backwards_playback_normal);
  tcase_add_test (tc, videodecoder_backwards_playback_subframes);
  tcase_add_test (tc, videodecoder_backwards_buffer_after_segment);
  tcase_add_test (tc, videodecoder_flush_events);

  tcase_add_loop_test (tc, videodecoder_default_caps_on_gap_before_buffer, 0,
      G_N_ELEMENTS (test_default_caps));

  tcase_add_test (tc, videodecoder_playback_event_order);
  tcase_add_test (tc, videodecoder_playback_parsed);
  tcase_add_test (tc, videodecoder_playback_packetized);
  tcase_add_test (tc, videodecoder_playback_parsed_subframes);
  tcase_add_test (tc, videodecoder_playback_packetized_subframes);
  tcase_add_test (tc, videodecoder_playback_packetized_subframes_metadata);
  tcase_add_test (tc, videodecoder_playback_packetized_subframes_metadata_copy);
  tcase_add_test (tc, videodecoder_playback_invalid_ts_packetized);
  tcase_add_test (tc, videodecoder_playback_invalid_ts_packetized_subframes);

  return s;
}

GST_CHECK_MAIN (gst_videodecoder);
