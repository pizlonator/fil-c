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
#include <gst/audio/audio.h>
#include <gst/app/app.h>

#define TEST_AUDIO_RATE 44100
#define TEST_AUDIO_CHANNELS 2
#define TEST_AUDIO_FORMAT "S16LE"

#define GST_AUDIO_ENCODER_TESTER_TYPE gst_audio_encoder_tester_get_type()
static GType gst_audio_encoder_tester_get_type (void);

typedef struct _GstAudioEncoderTester GstAudioEncoderTester;
typedef struct _GstAudioEncoderTesterClass GstAudioEncoderTesterClass;

struct _GstAudioEncoderTester
{
  GstAudioEncoder parent;
};

struct _GstAudioEncoderTesterClass
{
  GstAudioEncoderClass parent_class;
};

G_DEFINE_TYPE (GstAudioEncoderTester, gst_audio_encoder_tester,
    GST_TYPE_AUDIO_ENCODER);

static gboolean
gst_audio_encoder_tester_start (GstAudioEncoder * enc)
{
  return TRUE;
}

static gboolean
gst_audio_encoder_tester_stop (GstAudioEncoder * enc)
{
  return TRUE;
}

static gboolean
gst_audio_encoder_tester_set_format (GstAudioEncoder * enc, GstAudioInfo * info)
{
  GstCaps *caps;

  caps = gst_caps_new_simple ("audio/x-test-custom", "rate", G_TYPE_INT,
      TEST_AUDIO_RATE, "channels", G_TYPE_INT, TEST_AUDIO_CHANNELS, NULL);
  gst_audio_encoder_set_output_format (enc, caps);
  gst_caps_unref (caps);

  return TRUE;
}

static GstFlowReturn
gst_audio_encoder_tester_handle_frame (GstAudioEncoder * enc,
    GstBuffer * buffer)
{
  guint8 *data;
  GstMapInfo map;
  guint64 input_num;
  GstBuffer *output_buffer;

  if (buffer == NULL)
    return GST_FLOW_OK;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  input_num = *((guint64 *) map.data);
  gst_buffer_unmap (buffer, &map);

  data = g_malloc (sizeof (guint64));
  *(guint64 *) data = input_num;

  output_buffer = gst_buffer_new_wrapped (data, sizeof (guint64));
  GST_BUFFER_PTS (output_buffer) = GST_BUFFER_PTS (buffer);
  GST_BUFFER_DURATION (output_buffer) = GST_BUFFER_DURATION (buffer);

  return gst_audio_encoder_finish_frame (enc, output_buffer, TEST_AUDIO_RATE);
}

static void
gst_audio_encoder_tester_class_init (GstAudioEncoderTesterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *audioencoder_class = GST_AUDIO_ENCODER_CLASS (klass);

  static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("audio/x-raw"));

  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("audio/x-test-custom"));

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  gst_element_class_set_metadata (element_class,
      "AudioEncoderTester", "Encoder/Audio", "yep", "me");

  audioencoder_class->start = gst_audio_encoder_tester_start;
  audioencoder_class->stop = gst_audio_encoder_tester_stop;
  audioencoder_class->handle_frame = gst_audio_encoder_tester_handle_frame;
  audioencoder_class->set_format = gst_audio_encoder_tester_set_format;
}

static void
gst_audio_encoder_tester_init (GstAudioEncoderTester * tester)
{
}

static GstHarness *
setup_audioencodertester (void)
{
  GstHarness *h;
  GstElement *enc;

  static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("audio/x-test-custom")
      );
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("audio/x-raw")
      );

  enc = g_object_new (GST_AUDIO_ENCODER_TESTER_TYPE, NULL);
  h = gst_harness_new_full (enc, &srctemplate, "sink", &sinktemplate, "src");

  gst_harness_set_src_caps (h,
      gst_caps_new_simple ("audio/x-raw",
          "rate", G_TYPE_INT, TEST_AUDIO_RATE,
          "channels", G_TYPE_INT, TEST_AUDIO_CHANNELS,
          "format", G_TYPE_STRING, TEST_AUDIO_FORMAT,
          "layout", G_TYPE_STRING, "interleaved", NULL));

  gst_object_unref (enc);
  return h;
}

static GstBuffer *
create_test_buffer (guint64 num)
{
  GstBuffer *buffer;
  guint64 *data;
  gsize size;
  guint64 samples;

  samples = TEST_AUDIO_RATE;
  size = 2 * 2 * samples;

  data = g_malloc0 (size);
  *data = num;

  buffer = gst_buffer_new_wrapped (data, size);

  GST_BUFFER_PTS (buffer) = num * GST_SECOND;
  GST_BUFFER_DURATION (buffer) = GST_SECOND;

  return buffer;
}

#define NUM_BUFFERS 100
GST_START_TEST (audioencoder_playback)
{
  GstBuffer *buffer;
  guint64 i;
  guint buffers_available;

  GstHarness *h = setup_audioencodertester ();

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < NUM_BUFFERS; i++) {
    fail_unless (gst_harness_push (h, create_test_buffer (i)) == GST_FLOW_OK);
  }

  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  /* check that all buffers were received by our source pad */
  buffers_available = gst_harness_buffers_in_queue (h);
  fail_unless_equals_int (NUM_BUFFERS, buffers_available);

  for (i = 0; i < buffers_available; i++) {
    GstMapInfo map;
    guint64 num;

    buffer = gst_harness_pull (h);

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    num = *(guint64 *) map.data;
    fail_unless (i == num);
    fail_unless (GST_BUFFER_PTS (buffer) == i * GST_SECOND);
    fail_unless (GST_BUFFER_DURATION (buffer) == GST_SECOND);

    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;


GST_START_TEST (audioencoder_flush_events)
{
  guint i;

  GstHarness *h = setup_audioencodertester ();

  /* push buffers, the data is actually a number so we can track them */
  for (i = 0; i < NUM_BUFFERS; i++) {
    if (i % 10 == 0) {
      GstTagList *tags;

      tags = gst_tag_list_new (GST_TAG_TRACK_NUMBER, i, NULL);
      fail_unless (gst_harness_push_event (h, gst_event_new_tag (tags)));
    } else {
      fail_unless (gst_harness_push (h, create_test_buffer (i)) == GST_FLOW_OK);
    }
  }

  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  /* make sure the usual events have been received */
  {
    GstEvent *sstart = gst_harness_pull_event (h);
    fail_unless (GST_EVENT_TYPE (sstart) == GST_EVENT_STREAM_START);
    gst_event_unref (sstart);
  }
  {
    GstEvent *caps_event = gst_harness_pull_event (h);
    fail_unless (GST_EVENT_TYPE (caps_event) == GST_EVENT_CAPS);
    gst_event_unref (caps_event);
  }
  {
    GstEvent *segment_event = gst_harness_pull_event (h);
    fail_unless (GST_EVENT_TYPE (segment_event) == GST_EVENT_SEGMENT);
    gst_event_unref (segment_event);
  }

  /* check that EOS was received */
  fail_unless (GST_PAD_IS_EOS (h->srcpad));
  fail_unless (gst_harness_push_event (h, gst_event_new_flush_start ()));
  fail_unless (GST_PAD_IS_EOS (h->srcpad));

  /* Check that we have tags */
  {
    GstEvent *tags = gst_pad_get_sticky_event (h->srcpad, GST_EVENT_TAG, 0);

    fail_unless (tags != NULL);
    gst_event_unref (tags);
  }

  /* Check that we still have a segment set */
  {
    GstEvent *segment =
        gst_pad_get_sticky_event (h->srcpad, GST_EVENT_SEGMENT, 0);

    fail_unless (segment != NULL);
    gst_event_unref (segment);
  }

  fail_unless (gst_harness_push_event (h, gst_event_new_flush_stop (TRUE)));
  fail_if (GST_PAD_IS_EOS (h->srcpad));

  /* Check that the segment was flushed on FLUSH_STOP */
  {
    GstEvent *segment =
        gst_pad_get_sticky_event (h->srcpad, GST_EVENT_SEGMENT, 0);

    fail_unless (segment == NULL);
  }

  /* Check the tags were not lost on FLUSH_STOP */
  {
    GstEvent *tags = gst_pad_get_sticky_event (h->srcpad, GST_EVENT_TAG, 0);

    fail_unless (tags != NULL);
    gst_event_unref (tags);

  }

  gst_harness_teardown (h);
}

GST_END_TEST;

/* make sure tags sent right before eos are pushed */
GST_START_TEST (audioencoder_tags_before_eos)
{
  GstTagList *tags;
  GstEvent *event;

  GstHarness *h = setup_audioencodertester ();

  /* push buffer */
  fail_unless (gst_harness_push (h, create_test_buffer (0)) == GST_FLOW_OK);

  /* clean received events list */
  while ((event = gst_harness_try_pull_event (h)))
    gst_event_unref (event);

  /* push a tag event */
  tags = gst_tag_list_new (GST_TAG_COMMENT, "test-comment", NULL);
  fail_unless (gst_harness_push_event (h, gst_event_new_tag (tags)));

  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  /* check that the tag was received */
  {
    GstEvent *tag_event = gst_harness_pull_event (h);
    gchar *str;

    fail_unless (GST_EVENT_TYPE (tag_event) == GST_EVENT_TAG);
    gst_event_parse_tag (tag_event, &tags);
    fail_unless (gst_tag_list_get_string (tags, GST_TAG_COMMENT, &str));
    fail_unless (strcmp (str, "test-comment") == 0);
    g_free (str);
    gst_event_unref (tag_event);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

/* make sure events sent right before eos are pushed */
GST_START_TEST (audioencoder_events_before_eos)
{
  GstMessage *msg;
  GstEvent *event;

  GstHarness *h = setup_audioencodertester ();

  /* push buffer */
  fail_unless (gst_harness_push (h, create_test_buffer (0)) == GST_FLOW_OK);

  /* clean received events list */
  while ((event = gst_harness_try_pull_event (h)))
    gst_event_unref (event);

  /* push a serialized event */
  msg = gst_message_new_element (GST_OBJECT (h->element),
      gst_structure_new_empty ("test"));
  fail_unless (gst_harness_push_event (h,
          gst_event_new_sink_message ("sink-test", msg)));
  gst_message_unref (msg);

  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  /* check that the tag was received */
  {
    GstEvent *msg_event = gst_harness_pull_event (h);
    const GstStructure *structure;

    fail_unless (GST_EVENT_TYPE (msg_event) == GST_EVENT_SINK_MESSAGE);
    fail_unless (gst_event_has_name (msg_event, "sink-test"));
    gst_event_parse_sink_message (msg_event, &msg);
    structure = gst_message_get_structure (msg);
    fail_unless (gst_structure_has_name (structure, "test"));
    gst_message_unref (msg);
    gst_event_unref (msg_event);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
gst_audioencoder_suite (void)
{
  Suite *s = suite_create ("GstAudioEncoder");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, audioencoder_playback);

  tcase_add_test (tc, audioencoder_tags_before_eos);
  tcase_add_test (tc, audioencoder_events_before_eos);
  tcase_add_test (tc, audioencoder_flush_events);

  return s;
}

GST_CHECK_MAIN (gst_audioencoder);
