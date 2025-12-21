/* GStreamer unit tests for decodebin
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2011 Hewlett-Packard Development Company, L.P.
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
#include <gst/base/gstbaseparse.h>

static const gchar dummytext[] =
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown "
    "Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick "
    "Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog "
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy "
    "Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a "
    "Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps "
    "over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox "
    "jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown "
    "Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick "
    "Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog "
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy "
    "Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a "
    "Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps "
    "over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox ";

static void
src_need_data_cb (GstElement * src, guint size, gpointer data)
{
  GstBuffer *buf;
  GstFlowReturn ret;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (gpointer) dummytext, sizeof (dummytext), 0,
          sizeof (dummytext), NULL, NULL));

  GST_BUFFER_OFFSET (buf) = 0;

  g_signal_emit_by_name (src, "push-buffer", buf, &ret);
  gst_buffer_unref (buf);

  fail_unless (ret == GST_FLOW_OK);
}

static void
decodebin_pad_added_cb (GstElement * decodebin, GstPad * pad, gboolean * p_flag)
{
  /* we should not be reached */
  fail_unless (decodebin == NULL, "pad-added should not be emitted");
}

/* make sure that decodebin errors out instead of creating a new decoded pad
 * if the entire stream is a plain text file */
GST_START_TEST (test_text_plain_streams)
{
  GstElement *pipe, *src, *decodebin;
  GstMessage *msg;

  pipe = gst_pipeline_new (NULL);
  fail_unless (pipe != NULL, "failed to create pipeline");

  src = gst_element_factory_make ("appsrc", "src");
  fail_unless (src != NULL, "Failed to create appsrc element");

  g_object_set (src, "emit-signals", TRUE, NULL);
  g_object_set (src, "num-buffers", 1, NULL);
  g_signal_connect (src, "need-data", G_CALLBACK (src_need_data_cb), NULL);

  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  fail_unless (decodebin != NULL, "Failed to create decodebin element");

  g_signal_connect (decodebin, "pad-added",
      G_CALLBACK (decodebin_pad_added_cb), NULL);

  fail_unless (gst_bin_add (GST_BIN (pipe), src));
  fail_unless (gst_bin_add (GST_BIN (pipe), decodebin));
  fail_unless (gst_element_link (src, decodebin), "can't link src<->decodebin");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* it should error out at some point */
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, -1);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

static void
pad_added_plug_fakesink_cb (GstElement * decodebin, GstPad * srcpad,
    GstElement * pipeline)
{
  GstElement *sink;
  GstPad *sinkpad;

  GST_LOG ("Linking fakesink");

  sink = gst_element_factory_make ("fakesink", "sink");
  fail_unless (sink != NULL, "Failed to create fakesink element");

  gst_bin_add (GST_BIN (pipeline), sink);
  gst_element_sync_state_with_parent (sink);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless_equals_int (gst_pad_link (srcpad, sinkpad), GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);
}

GST_START_TEST (test_reuse_without_decoders)
{
  GstElement *pipe, *src, *decodebin, *sink;

  pipe = gst_pipeline_new (NULL);
  fail_unless (pipe != NULL, "failed to create pipeline");

  src = gst_element_factory_make ("audiotestsrc", "src");
  fail_unless (src != NULL, "Failed to create audiotestsrc element");

  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  fail_unless (decodebin != NULL, "Failed to create decodebin element");

  g_signal_connect (decodebin, "pad-added",
      G_CALLBACK (pad_added_plug_fakesink_cb), pipe);

  fail_unless (gst_bin_add (GST_BIN (pipe), src));
  fail_unless (gst_bin_add (GST_BIN (pipe), decodebin));
  fail_unless (gst_element_link (src, decodebin), "can't link src<->decodebin");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* wait for state change to complete */
  fail_unless_equals_int (gst_element_get_state (pipe, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* there shouldn't be any errors */
  fail_if (gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, 0) != NULL);

  GST_DEBUG ("Resetting pipeline");

  /* reset */
  gst_element_set_state (pipe, GST_STATE_READY);

  sink = gst_bin_get_by_name (GST_BIN (pipe), "sink");
  gst_bin_remove (GST_BIN (pipe), sink);
  gst_element_set_state (sink, GST_STATE_NULL);
  gst_object_unref (sink);

  GST_LOG ("second try");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* wait for state change to complete */
  fail_unless_equals_int (gst_element_get_state (pipe, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* there shouldn't be any errors */
  fail_if (gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, 0) != NULL);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

/* Fake mp3 parser for test */
typedef GstBaseParse TestMpegAudioParse;
typedef GstBaseParseClass TestMpegAudioParseClass;

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion=1, layer=[1,3], parsed=(b)true")
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion=1")
    );

static GType test_mpeg_audio_parse_get_type (void);
static gboolean test_mpeg_audio_parse_start (GstBaseParse * parse);
static gboolean test_mpeg_audio_parse_stop (GstBaseParse * parse);
static GstFlowReturn test_mpeg_audio_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);

G_DEFINE_TYPE (TestMpegAudioParse, test_mpeg_audio_parse, GST_TYPE_BASE_PARSE);

static void
test_mpeg_audio_parse_class_init (TestMpegAudioParseClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_metadata (element_class, "MPEG1 Audio Parser",
      "Codec/Parser/Audio", "Pretends to parse mpeg1 audio stream",
      "Foo Bar <foo@bar.com>");

  parse_class->start = test_mpeg_audio_parse_start;
  parse_class->stop = test_mpeg_audio_parse_stop;
  parse_class->handle_frame = test_mpeg_audio_parse_handle_frame;
}

static gint num_parse_instances = 0;

static void
test_mpeg_audio_parse_init (TestMpegAudioParse * mp3parse)
{
  /* catch decodebin plugging parsers in a loop early */
  fail_unless (++num_parse_instances < 10);
}

static gboolean
test_mpeg_audio_parse_start (GstBaseParse * parse)
{
  gst_base_parse_set_min_frame_size (parse, 6);
  return TRUE;
}

static gboolean
test_mpeg_audio_parse_stop (GstBaseParse * parse)
{
  return TRUE;
}

static GstFlowReturn
test_mpeg_audio_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  guint8 data[2];

  gst_buffer_extract (frame->buffer, 0, data, 2);

  if ((GST_READ_UINT16_BE (data) & 0xffe0) == 0xffe0) {
    if (GST_BUFFER_OFFSET (frame->buffer) == 0) {
      GstCaps *caps;

      caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          "mpegaudioversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3,
          "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 2, NULL);
      gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), caps);
      gst_caps_unref (caps);
    }

    /* this framesize is hard-coded for ../test.mp3 */
    return gst_base_parse_finish_frame (parse, frame, 1045);
  } else {
    *skipsize = 1;
    return GST_FLOW_OK;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testmpegaudioparse", GST_RANK_NONE,
      test_mpeg_audio_parse_get_type ());
}

GST_START_TEST (test_mp3_parser_loop)
{
  GstStateChangeReturn sret;
  GstPluginFeature *feature;
  GstMessage *msg;
  GstElement *pipe, *src, *dec;
  gchar *path;

  num_parse_instances = 0;

  gst_plugin_register_static (GST_VERSION_MAJOR, GST_VERSION_MINOR,
      "fakemp3parse", "fakemp3parse", plugin_init, VERSION, "LGPL",
      "gst-plugins-base", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);

  feature = gst_registry_find_feature (gst_registry_get (),
      "testmpegaudioparse", GST_TYPE_ELEMENT_FACTORY);

  gst_plugin_feature_set_rank (feature, GST_RANK_PRIMARY + 100);

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("filesrc", NULL);
  fail_unless (src != NULL);

  path = g_build_filename (GST_TEST_FILES_PATH, "test.mp3", NULL);
  g_object_set (src, "location", path, NULL);
  g_free (path);

  dec = gst_element_factory_make ("decodebin", NULL);
  fail_unless (dec != NULL);

  gst_bin_add_many (GST_BIN (pipe), src, dec, NULL);
  gst_element_link_many (src, dec, NULL);

  sret = gst_element_set_state (pipe, GST_STATE_PLAYING);
  fail_unless_equals_int (sret, GST_STATE_CHANGE_ASYNC);

  /* wait for unlinked error */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipe),
      GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);

  /* make sure out parser got plugged at all though */
  fail_unless_equals_int (num_parse_instances, 1);

  /* don't want to interfere with any other of the other tests */
  gst_plugin_feature_set_rank (feature, GST_RANK_NONE);
  gst_object_unref (feature);
}

GST_END_TEST;

/* Fake parser/decoder for parser_negotiation test */
static GType gst_fake_h264_parser_get_type (void);
static GType gst_fake_h264_decoder_get_type (void);

#undef parent_class
#define parent_class fake_h264_parser_parent_class
typedef struct _GstFakeH264Parser GstFakeH264Parser;
typedef GstElementClass GstFakeH264ParserClass;

struct _GstFakeH264Parser
{
  GstElement parent;
};

G_DEFINE_TYPE (GstFakeH264Parser, gst_fake_h264_parser, GST_TYPE_ELEMENT);

static void
gst_fake_h264_parser_class_init (GstFakeH264ParserClass * klass)
{
  static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-h264"));
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-h264, "
          "stream-format=(string) { avc, byte-stream }"));
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_set_metadata (element_class,
      "FakeH264Parser", "Codec/Parser/Converter/Video", "yep", "me");
}

static gboolean
gst_fake_h264_parser_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstElement *self = GST_ELEMENT (parent);
  GstPad *otherpad = gst_element_get_static_pad (self, "src");
  GstCaps *accepted_caps;
  GstStructure *s;
  const gchar *stream_format;
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      accepted_caps = gst_pad_get_allowed_caps (otherpad);
      accepted_caps = gst_caps_truncate (accepted_caps);

      s = gst_caps_get_structure (accepted_caps, 0);
      stream_format = gst_structure_get_string (s, "stream-format");
      if (!stream_format)
        gst_structure_set (s, "stream-format", G_TYPE_STRING, "avc", NULL);

      gst_pad_set_caps (otherpad, accepted_caps);
      gst_caps_unref (accepted_caps);
      gst_event_unref (event);
      event = NULL;
      break;
    default:
      break;
  }

  if (event)
    ret = gst_pad_push_event (otherpad, event);
  gst_object_unref (otherpad);

  return ret;
}

static GstFlowReturn
gst_fake_h264_parser_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf)
{
  GstElement *self = GST_ELEMENT (parent);
  GstPad *otherpad = gst_element_get_static_pad (self, "src");
  GstFlowReturn ret = GST_FLOW_OK;

  buf = gst_buffer_make_writable (buf);

  ret = gst_pad_push (otherpad, buf);

  gst_object_unref (otherpad);

  return ret;
}

static void
gst_fake_h264_parser_init (GstFakeH264Parser * self)
{
  GstPad *pad;

  pad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_event_function (pad, gst_fake_h264_parser_sink_event);
  gst_pad_set_chain_function (pad, gst_fake_h264_parser_sink_chain);
  gst_element_add_pad (GST_ELEMENT (self), pad);

  pad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_GET_CLASS (self), "src"), "src");
  gst_element_add_pad (GST_ELEMENT (self), pad);
}

#undef parent_class
#define parent_class fake_h264_decoder_parent_class
typedef struct _GstFakeH264Decoder GstFakeH264Decoder;
typedef GstElementClass GstFakeH264DecoderClass;

struct _GstFakeH264Decoder
{
  GstElement parent;
};

G_DEFINE_TYPE (GstFakeH264Decoder, gst_fake_h264_decoder, GST_TYPE_ELEMENT);

static void
gst_fake_h264_decoder_class_init (GstFakeH264DecoderClass * klass)
{
  static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-h264, " "stream-format=(string) byte-stream"));
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw"));
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);
  gst_element_class_set_metadata (element_class,
      "FakeH264Decoder", "Codec/Decoder/Video", "yep", "me");
}

static gboolean
gst_fake_h264_decoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstElement *self = GST_ELEMENT (parent);
  GstPad *otherpad = gst_element_get_static_pad (self, "src");
  GstCaps *caps;
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      caps = gst_caps_new_empty_simple ("video/x-raw");
      gst_pad_set_caps (otherpad, caps);
      gst_caps_unref (caps);
      gst_event_unref (event);
      event = NULL;
      break;
    default:
      break;
  }

  if (event)
    ret = gst_pad_push_event (otherpad, event);
  gst_object_unref (otherpad);

  return ret;
}

static GstFlowReturn
gst_fake_h264_decoder_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf)
{
  GstElement *self = GST_ELEMENT (parent);
  GstPad *otherpad = gst_element_get_static_pad (self, "src");
  GstFlowReturn ret = GST_FLOW_OK;

  buf = gst_buffer_make_writable (buf);

  ret = gst_pad_push (otherpad, buf);

  gst_object_unref (otherpad);

  return ret;
}

static void
gst_fake_h264_decoder_init (GstFakeH264Decoder * self)
{
  GstPad *pad;

  pad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_event_function (pad, gst_fake_h264_decoder_sink_event);
  gst_pad_set_chain_function (pad, gst_fake_h264_decoder_sink_chain);
  gst_element_add_pad (GST_ELEMENT (self), pad);

  pad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_GET_CLASS (self), "src"), "src");
  gst_element_add_pad (GST_ELEMENT (self), pad);
}

static void
parser_negotiation_pad_added_cb (GstElement * dec, GstPad * pad,
    gpointer user_data)
{
  GstBin *pipe = user_data;
  GstElement *sink;
  GstPad *sinkpad;

  sink = gst_element_factory_make ("fakesink", NULL);
  gst_bin_add (pipe, sink);
  gst_element_sync_state_with_parent (sink);
  sinkpad = gst_element_get_static_pad (sink, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
}

GST_START_TEST (test_parser_negotiation)
{
  GstStateChangeReturn sret;
  GstMessage *msg;
  GstCaps *caps;
  GstElement *pipe, *src, *filter, *dec;

  gst_element_register (NULL, "fakeh264parse", GST_RANK_PRIMARY + 101,
      gst_fake_h264_parser_get_type ());
  gst_element_register (NULL, "fakeh264dec", GST_RANK_PRIMARY + 100,
      gst_fake_h264_decoder_get_type ());

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("fakesrc", NULL);
  fail_unless (src != NULL);
  g_object_set (G_OBJECT (src), "num-buffers", 5, "sizetype", 2, "filltype", 2,
      "can-activate-pull", FALSE, NULL);

  filter = gst_element_factory_make ("capsfilter", NULL);
  fail_unless (filter != NULL);
  caps = gst_caps_from_string ("video/x-h264");
  g_object_set (G_OBJECT (filter), "caps", caps, NULL);
  gst_caps_unref (caps);

  dec = gst_element_factory_make ("decodebin", NULL);
  fail_unless (dec != NULL);

  g_signal_connect (dec, "pad-added",
      G_CALLBACK (parser_negotiation_pad_added_cb), pipe);

  gst_bin_add_many (GST_BIN (pipe), src, filter, dec, NULL);
  gst_element_link_many (src, filter, dec, NULL);

  sret = gst_element_set_state (pipe, GST_STATE_PLAYING);
  fail_unless_equals_int (sret, GST_STATE_CHANGE_ASYNC);

  /* wait for EOS or error */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipe),
      GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

GST_START_TEST (test_buffering_aggregation)
{
  GstElement *pipe, *decodebin;
  GstMessage *msg;
  GstElement *mq0, *mq1, *mq2;
  gint perc;

  pipe = gst_pipeline_new (NULL);
  fail_unless (pipe != NULL, "failed to create pipeline");

  decodebin = gst_element_factory_make ("decodebin", "decodebin");
  fail_unless (decodebin != NULL, "Failed to create decodebin element");

  fail_unless (gst_bin_add (GST_BIN (pipe), decodebin));

  /* to simulate the buffering scenarios we stuff 2 multiqueues inside
   * decodebin. This is hacky, but should make decodebin handle its buffering
   * messages all the same */
  mq0 = gst_element_factory_make ("multiqueue", NULL);
  mq1 = gst_element_factory_make ("multiqueue", NULL);
  mq2 = gst_element_factory_make ("multiqueue", NULL);

  fail_unless (gst_bin_add (GST_BIN (decodebin), mq0));
  fail_unless (gst_bin_add (GST_BIN (decodebin), mq1));
  fail_unless (gst_bin_add (GST_BIN (decodebin), mq2));

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* currently we should have no buffering messages */
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg == NULL);

  /* only a single element buffering, the buffering percent should be the
   * same as it */
  gst_element_post_message (mq0, gst_message_new_buffering (GST_OBJECT (mq0),
          50));
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == (GstObject *) mq0);
  gst_message_parse_buffering (msg, &perc);
  fail_unless (perc == 50);
  gst_message_unref (msg);

  /* two elements buffering, the buffering percent should be the
   * lowest one */
  gst_element_post_message (mq1, gst_message_new_buffering (GST_OBJECT (mq1),
          20));
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == (GstObject *) mq1);
  gst_message_parse_buffering (msg, &perc);
  fail_unless (perc == 20);
  gst_message_unref (msg);

  /* a 100% message should be ignored */
  gst_element_post_message (mq2, gst_message_new_buffering (GST_OBJECT (mq2),
          100));
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == (GstObject *) mq1);
  gst_message_parse_buffering (msg, &perc);
  fail_unless (perc == 20);
  gst_message_unref (msg);

  /* a new buffering message is posted with a higher value, go with the 20 */
  gst_element_post_message (mq2, gst_message_new_buffering (GST_OBJECT (mq2),
          80));
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == (GstObject *) mq1);
  gst_message_parse_buffering (msg, &perc);
  fail_unless (perc == 20);
  gst_message_unref (msg);

  /* The mq1 finishes buffering, new buffering status is now 50% from mq0 */
  gst_element_post_message (mq1, gst_message_new_buffering (GST_OBJECT (mq1),
          100));
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_BUFFERING, 0);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == (GstObject *) mq0);
  gst_message_parse_buffering (msg, &perc);
  fail_unless (perc == 50);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

static Suite *
decodebin_suite (void)
{
  Suite *s = suite_create ("decodebin");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_text_plain_streams);
  tcase_add_test (tc_chain, test_reuse_without_decoders);
  tcase_add_test (tc_chain, test_mp3_parser_loop);
  tcase_add_test (tc_chain, test_parser_negotiation);
  tcase_add_test (tc_chain, test_buffering_aggregation);

  return s;
}

GST_CHECK_MAIN (decodebin);
