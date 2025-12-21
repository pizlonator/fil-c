/* GStreamer unit tests for subparse
 * Copyright (C) 2006-2008 Tim-Philipp Müller <tim centricular net>
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
#include <gst/check/gstharness.h>

#include <string.h>

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { pango-markup, utf8 }")
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstElement *subparse;
static GstPad *mysrcpad, *mysinkpad;

static GstBuffer *
buffer_from_static_string (const gchar * s)
{
  GstBuffer *buf;
  gsize len;

  len = strlen (s);

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (gpointer) s, len, 0, len, NULL, NULL));

  return buf;
}

typedef struct
{
  const gchar *in;
  GstClockTime from_ts;
  GstClockTime to_ts;
  const gchar *out;
} SubParseInputChunk;

static SubParseInputChunk srt_input[] = {
  {
        "1\n00:00:01,000 --> 00:00:02,000\nOne\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "One"}, {
        "2\n00:00:02,000 --> 00:00:03,000\nTwo\n\n",
      2 * GST_SECOND, 3 * GST_SECOND, "Two"}, {
        "3\n00:00:03,000 --> 00:00:04,000\nThree\n\n",
      3 * GST_SECOND, 4 * GST_SECOND, "Three"}, {
        "4\n00:00:04,000 --> 00:00:05,000\nFour\n\n",
      4 * GST_SECOND, 5 * GST_SECOND, "Four"}, {
        "5\n00:00:05,000 --> 00:00:06,000\nFive\n\n",
      5 * GST_SECOND, 6 * GST_SECOND, "Five"}, {
        /* markup should be preserved */
        "6\n00:00:06,000 --> 00:00:07,000\n<i>Six</i>\n\n",
      6 * GST_SECOND, 7 * GST_SECOND, "<i>Six</i>"}, {
        /* open markup tags should be closed */
        "7\n00:00:07,000 --> 00:00:08,000\n<i>Seven\n\n",
      7 * GST_SECOND, 8 * GST_SECOND, "<i>Seven</i>"}, {
        /* open markup tags should be closed (II) */
        "8\n00:00:08,000 --> 00:00:09,000\n<b><i>Eight\n\n",
      8 * GST_SECOND, 9 * GST_SECOND, "<b><i>Eight</i></b>"}, {
        /* broken markup should be fixed */
        "9\n00:00:09,000 --> 00:00:10,000\n</b>\n\n",
      9 * GST_SECOND, 10 * GST_SECOND, ""}, {
        "10\n00:00:10,000 --> 00:00:11,000\n</b></i>\n\n",
      10 * GST_SECOND, 11 * GST_SECOND, ""}, {
        "11\n00:00:11,000 --> 00:00:12,000\n<i>xyz</b></i>\n\n",
      11 * GST_SECOND, 12 * GST_SECOND, "<i>xyz</i>"}, {
        "12\n00:00:12,000 --> 00:00:13,000\n<i>xyz</b>\n\n",
      12 * GST_SECOND, 13 * GST_SECOND, "<i>xyz</i>"}, {
        "13\n00:00:13,000 --> 00:00:14,000\n<i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i>Keep them comiiiiiing\n\n",
        13 * GST_SECOND, 14 * GST_SECOND,
      "<i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i><i>Keep them comiiiiiing</i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i></i>"}, {
        /* skip a few chunk numbers here, the numbers shouldn't matter */
        "24\n00:01:00,000 --> 00:02:00,000\nYep, still here\n\n",
      60 * GST_SECOND, 120 * GST_SECOND, "Yep, still here"}, {
        /* make sure stuff is escaped properly, but allowed markup stays intact */
        "25\n00:03:00,000 --> 00:04:00,000\ngave <i>Rock & Roll</i> to\n\n",
      180 * GST_SECOND, 240 * GST_SECOND, "gave <i>Rock &amp; Roll</i> to"}, {
        "26\n00:04:00,000 --> 00:05:00,000\n<i>Rock & Roll</i>\n\n",
      240 * GST_SECOND, 300 * GST_SECOND, "<i>Rock &amp; Roll</i>"}, {
        "27\n00:06:00,000 --> 00:08:00,000\nRock & Roll\n\n",
      360 * GST_SECOND, 480 * GST_SECOND, "Rock &amp; Roll"}, {
        "28\n00:10:00,000 --> 00:11:00,000\n"
        "<font \"#0000FF\"><joj>This is </xxx>in blue but <5</font>\n\n",
      600 * GST_SECOND, 660 * GST_SECOND, "This is in blue but &lt;5"}, {
        /* closing tags should be recognised properly even if there's a space */
        "29\n00:11:00,000 --> 00:12:00,000\n" "<i>italics</ i>\n\n",
      660 * GST_SECOND, 720 * GST_SECOND, "<i>italics</i>"}, {
        /* closing tags should be escaped and fixed up if not recognised */
        "30\n00:12:00,000 --> 00:12:01,000\n" "<i>italics</ x>\n\n",
      720 * GST_SECOND, 721 * GST_SECOND, "<i>italics&lt;/ x&gt;</i>"},
};

/* starts with chunk number 0 (not exactly according to spec) */
static SubParseInputChunk srt_input0[] = {
  {
        "0\n00:00:01,000 --> 00:00:02,000\nOne\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "One"}, {
        "1\n00:00:02,000 --> 00:00:03,000\nTwo\n\n",
      2 * GST_SECOND, 3 * GST_SECOND, "Two"}, {
        "2\n00:00:03,000 --> 00:00:04,000\nThree\n\n",
      3 * GST_SECOND, 4 * GST_SECOND, "Three"}
};

/* has spaces instead of doubled zeroes (not exactly according to spec) */
static SubParseInputChunk srt_input1[] = {
  {
        "1\n 0: 0:26, 26 --> 0: 0:28, 17\nI cant see.\n\n",
        26 * GST_SECOND + 26 * GST_MSECOND,
      28 * GST_SECOND + 17 * GST_MSECOND, "I cant see."},
  {
        "2\n 0: 0:30, 30 --> 0: 0:33, 22\nI really cant see.\n\n",
        30 * GST_SECOND + 30 * GST_MSECOND,
      33 * GST_SECOND + 22 * GST_MSECOND, "I really cant see."},
  {
        "3\n 0: 0:40, 40 --> 0: 0:44, 44\nI still cant see anything.\n\n",
        40 * GST_SECOND + 40 * GST_MSECOND,
      44 * GST_SECOND + 44 * GST_MSECOND, "I still cant see anything."}
};

/* has UTF-8 BOM at the start */
static SubParseInputChunk srt_input2[] = {
  {
        "\xef\xbb\xbf" "1\n00:00:00,000 --> 00:00:03,50\nJust testing.\n\n",
      0, 3 * GST_SECOND + 500 * GST_MSECOND, "Just testing."}
};

/* starts with chunk number 0 and has less than three digits after the comma
 * and a few extra spaces before the arrow or at the end of the line */
static SubParseInputChunk srt_input3[] = {
  {
        "0\n00:00:01,0 --> 00:00:02,0\nOne\n\n",
      1000 * GST_MSECOND, 2000 * GST_MSECOND, "One"}, {
        "1\n00:00:02,5   --> 00:00:03,  5 \nTwo\n\n",
      2500 * GST_MSECOND, 3005 * GST_MSECOND, "Two"}, {
        "2\n00:00:03, 9 --> 00:00:04,0   \nThree\n\n",
      3090 * GST_MSECOND, 4000 * GST_MSECOND, "Three"}
};

/* Some WebVTT chunks, this format is similar to SRT but should be
 * parsed differently nonetheless, the WebVTT tags should be stripped
 * off. */
static SubParseInputChunk srt_input4[] = {
  {
        "1\n00:00:01,000 --> 00:00:02,000\n<v>some text\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "some text"}
  ,
  {
        "1\n00:00:01,000 --> 00:00:02,000\n<b.loud>some text\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "<b>some text</b>"}
  ,
  {
        "1\n00:00:01,000 --> 00:00:02,000\n<ruby>base text<rt>annotation</rt></ruby>\n\n",
        1 * GST_SECOND, 2 * GST_SECOND,
      "base textannotation"}
  ,
};

/* Test broken timestamp */
static SubParseInputChunk srt_input5[] = {
  {
        "1\n00:00:01,000 --> 00:00:02,000\n<v>some text\n\n",
      1 * GST_SECOND, 2 * GST_SECOND, "some text"}
  ,
  {
        "2\n00:02:00,000 --> 00:03:0\n<v>some other text\n\n3\n00:00:03,000 --> 00:00:04,000\n<v>some more text\n\n",
      3 * GST_SECOND, 4 * GST_SECOND, "some more text"}
  ,
};

/* Test with no newline at the end */
static SubParseInputChunk srt_input6[] = {
  {
        "1\n00:00:01,000 --> 00:00:02,000\nLast cue, no newline at the end",
      1 * GST_SECOND, 2 * GST_SECOND, "Last cue, no newline at the end"}
  ,
};


static void
setup_subparse (void)
{
  GstSegment segment;
  subparse = gst_check_setup_element ("subparse");

  mysrcpad = gst_check_setup_src_pad (subparse, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (subparse, &sinktemplate);

  gst_pad_set_active (mysrcpad, TRUE);

  gst_segment_init (&segment, GST_FORMAT_BYTES);
  gst_pad_push_event (mysrcpad, gst_event_new_stream_start ("test"));
  gst_pad_push_event (mysrcpad, gst_event_new_segment (&segment));
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless_equals_int (gst_element_set_state (subparse, GST_STATE_PLAYING),
      GST_STATE_CHANGE_SUCCESS);
}

static void
teardown_subparse (void)
{
  GST_DEBUG ("cleaning up");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_sink_pad (subparse);
  gst_check_teardown_src_pad (subparse);
  gst_check_teardown_element (subparse);
  subparse = NULL;
  mysrcpad = NULL;
  mysinkpad = NULL;
}

static void
test_srt_do_test (SubParseInputChunk * input, guint start_idx, guint num)
{
  guint n;
  GstCaps *outcaps;

  GST_LOG ("srt test: start_idx = %u, num = %u", start_idx, num);

  setup_subparse ();

  for (n = start_idx; n < start_idx + num; ++n) {
    GstBuffer *buf;

    buf = buffer_from_static_string (input[n].in);
    fail_unless_equals_int (gst_pad_push (mysrcpad, buf), GST_FLOW_OK);
  }

  gst_pad_push_event (mysrcpad, gst_event_new_eos ());

  fail_unless_equals_int (g_list_length (buffers), num);

  outcaps = gst_pad_get_current_caps (mysinkpad);

  for (n = start_idx; n < start_idx + num; ++n) {
    const GstStructure *buffer_caps_struct;
    GstBuffer *buf;
    GstMapInfo map;

    buf = g_list_nth_data (buffers, n - start_idx);
    fail_unless (buf != NULL);
    fail_unless (GST_BUFFER_TIMESTAMP_IS_VALID (buf), NULL);
    fail_unless (GST_BUFFER_DURATION_IS_VALID (buf), NULL);
    fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf), input[n].from_ts);
    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buf),
        input[n].to_ts - input[n].from_ts);

    gst_buffer_map (buf, &map, GST_MAP_READ);
    /* can be NULL */
    if (map.data != NULL) {
      /* shouldn't have trailing newline characters */
      fail_if (map.size > 0 && map.data[map.size - 1] == '\n');
      /* shouldn't include NUL-terminator in data size */
      fail_if (map.size > 0 && map.data[map.size - 1] == '\0');
      /* but should still have a  NUL-terminator behind the declared data */
      fail_unless_equals_int (map.data[map.size], '\0');
      /* make sure out string matches expected string */
      fail_unless_equals_string ((gchar *) map.data, input[n].out);
    }
    gst_buffer_unmap (buf, &map);
    /* check caps */
    fail_unless (outcaps != NULL);
    buffer_caps_struct = gst_caps_get_structure (outcaps, 0);
    fail_unless (gst_structure_has_name (buffer_caps_struct, "text/x-raw"));
    fail_unless_equals_string (gst_structure_get_string (buffer_caps_struct,
            "format"), "pango-markup");
  }
  gst_caps_unref (outcaps);

  teardown_subparse ();
}

static void
test_vtt_do_test (SubParseInputChunk * input, guint start_idx, guint num)
{
  guint n;

  GST_LOG ("vtt test: start_idx = %u, num = %u", start_idx, num);

  setup_subparse ();

  for (n = start_idx; n < start_idx + num; ++n) {
    GstBuffer *buf;
    gchar *data = g_strconcat ("WEBVTT FILE\n", input[n].in, NULL);
    buf = buffer_from_static_string (data);
    fail_unless_equals_int (gst_pad_push (mysrcpad, buf), GST_FLOW_OK);
    g_free (data);
  }

  gst_pad_push_event (mysrcpad, gst_event_new_eos ());

  fail_unless_equals_int (g_list_length (buffers), num);

  for (n = start_idx; n < start_idx + num; ++n) {
    const GstStructure *buffer_caps_struct;
    GstMapInfo map;
    GstBuffer *buf;
    GstCaps *outcaps;
    gchar *out;
    guint out_size;

    buf = g_list_nth_data (buffers, n - start_idx);
    fail_unless (buf != NULL);
    fail_unless (GST_BUFFER_TIMESTAMP_IS_VALID (buf), NULL);
    fail_unless (GST_BUFFER_DURATION_IS_VALID (buf), NULL);
    fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf), input[n].from_ts);
    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buf),
        input[n].to_ts - input[n].from_ts);
    fail_unless (gst_buffer_map (buf, &map, GST_MAP_READ));
    out = (gchar *) map.data;

    out_size = gst_buffer_get_size (buf);
    /* shouldn't have trailing newline characters */
    fail_if (out_size > 0 && out[out_size - 1] == '\n');
    /* shouldn't include NUL-terminator in data size */
    fail_if (out_size > 0 && out[out_size - 1] == '\0');
    /* but should still have a  NUL-terminator behind the declared data */
    fail_unless_equals_int (out[out_size], '\0');
    /* make sure out string matches expected string */
    fail_unless_equals_string (out, input[n].out);

    gst_buffer_unmap (buf, &map);

    /* check caps */
    outcaps = gst_pad_get_current_caps (mysinkpad);
    fail_unless (outcaps != NULL);
    buffer_caps_struct = gst_caps_get_structure (outcaps, 0);
    fail_unless_equals_string (gst_structure_get_name (buffer_caps_struct),
        "text/x-raw");
    fail_unless_equals_string (gst_structure_get_string (buffer_caps_struct,
            "format"), "pango-markup");
    gst_caps_unref (outcaps);
  }

  teardown_subparse ();
}

GST_START_TEST (test_srt)
{
  test_srt_do_test (srt_input, 0, G_N_ELEMENTS (srt_input));

  /* make sure everything works fine if we don't start with chunk 1 */
  test_srt_do_test (srt_input, 1, G_N_ELEMENTS (srt_input) - 1);
  test_srt_do_test (srt_input, 2, G_N_ELEMENTS (srt_input) - 2);
  test_srt_do_test (srt_input, 3, G_N_ELEMENTS (srt_input) - 3);
  test_srt_do_test (srt_input, 4, G_N_ELEMENTS (srt_input) - 4);

  /* try with empty input, immediate EOS */
  test_srt_do_test (srt_input, 5, G_N_ELEMENTS (srt_input) - 5);

  /* try with chunk number 0 (which is not exactly according to spec) */
  test_srt_do_test (srt_input0, 0, G_N_ELEMENTS (srt_input0));

  /* try with spaces instead of doubled zeroes (which is not exactly according to spec) */
  test_srt_do_test (srt_input1, 0, G_N_ELEMENTS (srt_input1));

  /* try with UTF-8 BOM at the start */
  test_srt_do_test (srt_input2, 0, G_N_ELEMENTS (srt_input2));

  /* try with fewer than three post-comma digits, and some extra spaces */
  test_srt_do_test (srt_input3, 0, G_N_ELEMENTS (srt_input3));

  /* try with some WebVTT chunks */
  test_srt_do_test (srt_input4, 0, G_N_ELEMENTS (srt_input4));

  /* try with some broken/cut-off timestamp */
  test_srt_do_test (srt_input5, 0, G_N_ELEMENTS (srt_input5));

  /* try without an empty line at the end */
  test_srt_do_test (srt_input6, 0, G_N_ELEMENTS (srt_input6));
}

GST_END_TEST;


GST_START_TEST (test_webvtt)
{
  SubParseInputChunk webvtt_input[] = {
    {
          "1\n00:00:01.000 --> 00:00:02.000 D:vertical T:50%\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 D:vertical   T:50%\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 D:vertical\tT:50%\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 D:vertical-lr\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 L:-123\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 L:123\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 L:12%\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 L:12% S:35% A:start\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 A:middle\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000 A:end\nOne\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\nOne & Two\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One &amp; Two"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\nOne < Two\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "One &lt; Two"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\n<v Spoke>Live long and prosper\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "<v Spoke>Live long and prosper</v>"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\n<v The Joker>HAHAHA\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "<v The Joker>HAHAHA</v>"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\n<c.someclass>some text\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "<c.someclass>some text</c>"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\n<b.loud>some text\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "<b.loud>some text</b>"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:02.000\n<ruby>base text<rt>annotation</rt></ruby>\n\n",
          1 * GST_SECOND, 2 * GST_SECOND,
        "<ruby>base text<rt>annotation</rt></ruby>"}
    ,
    {
          "1\n00:00:01.000 --> 00:00:03.000\nOne... <00:00:00,200>Two... <00:00:00,500>Three...\n\n",
          1 * GST_SECOND, 3 * GST_SECOND,
        "One... &lt;00:00:00,200&gt;Two... &lt;00:00:00,500&gt;Three..."}
    ,
    {"1\n00:00:02.000 --> 00:00:03.000\nHello\nWorld\n\n",
        2 * GST_SECOND, 3 * GST_SECOND, "Hello\nWorld"}
    ,
  };

  /* Test with no hour component */
  SubParseInputChunk webvtt_input1[] = {
    {
          "1\n00:01.000 --> 00:02.000 D:vertical T:50%\nNo hour component\n\n",
        1 * GST_SECOND, 2 * GST_SECOND, "No hour component"}
  };

  /* Test with no newline at the end */
  SubParseInputChunk webvtt_input2[] = {
    {
          "1\n00:00:01,000 --> 00:00:02,000\nLast cue, no newline at the end",
        1 * GST_SECOND, 2 * GST_SECOND, "Last cue, no newline at the end"}
    ,
  };

  test_vtt_do_test (webvtt_input, 0, G_N_ELEMENTS (webvtt_input));
  test_vtt_do_test (webvtt_input1, 0, G_N_ELEMENTS (webvtt_input1));
  test_vtt_do_test (webvtt_input2, 0, G_N_ELEMENTS (webvtt_input2));
}

GST_END_TEST;

static void
do_test (SubParseInputChunk * input, guint num, const gchar * format)
{
  guint n;
  GstCaps *outcaps;

  setup_subparse ();

  for (n = 0; n < num; ++n) {
    GstBuffer *buf;

    buf = buffer_from_static_string (input[n].in);
    fail_unless_equals_int (gst_pad_push (mysrcpad, buf), GST_FLOW_OK);
  }

  gst_pad_push_event (mysrcpad, gst_event_new_eos ());

  fail_unless_equals_int (g_list_length (buffers), num);

  outcaps = gst_pad_get_current_caps (mysinkpad);

  for (n = 0; n < num; ++n) {
    const GstStructure *buffer_caps_struct;
    GstBuffer *buf;
    GstMapInfo map;

    buf = g_list_nth_data (buffers, n);
    fail_unless (buf != NULL);

    /* check timestamp */
    fail_unless (GST_BUFFER_TIMESTAMP_IS_VALID (buf), NULL);
    fail_unless_equals_uint64 (GST_BUFFER_TIMESTAMP (buf), input[n].from_ts);

    /* might not be able to put a duration on the last buffer */
    if (input[n].to_ts != GST_CLOCK_TIME_NONE) {
      /* check duration */
      fail_unless (GST_BUFFER_DURATION_IS_VALID (buf), NULL);
      fail_unless_equals_uint64 (GST_BUFFER_DURATION (buf),
          input[n].to_ts - input[n].from_ts);
    }

    gst_buffer_map (buf, &map, GST_MAP_READ);
    /* can be NULL */
    if (map.data != NULL) {
      /* shouldn't have trailing newline characters */
      fail_if (map.size > 0 && map.data[map.size - 1] == '\n');
      /* shouldn't include NUL-terminator in data size */
      fail_if (map.size > 0 && map.data[map.size - 1] == '\0');
      /* but should still have a  NUL-terminator behind the declared data */
      fail_unless_equals_int (map.data[map.size], '\0');
      /* make sure out string matches expected string */
      fail_unless_equals_string ((gchar *) map.data, input[n].out);
    }
    gst_buffer_unmap (buf, &map);
    /* check caps */
    fail_unless (outcaps != NULL);
    buffer_caps_struct = gst_caps_get_structure (outcaps, 0);
    fail_unless (gst_structure_has_name (buffer_caps_struct, "text/x-raw"));
    fail_unless_equals_string (gst_structure_get_string (buffer_caps_struct,
            "format"), format);
  }
  gst_caps_unref (outcaps);

  teardown_subparse ();
}

static void
test_tmplayer_do_test (SubParseInputChunk * input, guint num)
{
  do_test (input, num, "utf8");
}

static void
test_microdvd_do_test (SubParseInputChunk * input, guint num)
{
  do_test (input, num, "pango-markup");
}

GST_START_TEST (test_tmplayer_multiline)
{
  static SubParseInputChunk tmplayer_multiline_input[] = {
    {
          "00:00:10,1=This is the Earth at a time\n"
          "00:00:10,2=when the dinosaurs roamed...\n" "00:00:13,1=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14,1=a lush and fertile planet.\n" "00:00:16,1=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_multiline_input,
      G_N_ELEMENTS (tmplayer_multiline_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_multiline_with_bogus_lines)
{
  static SubParseInputChunk tmplayer_multiline_b_input[] = {
    {
          "00:00:10,1=This is the Earth at a time\n"
          "Yooboo wabahablablahuguug bogus line hello test 1-2-3-4\n"
          "00:00:10,2=when the dinosaurs roamed...\n" "00:00:13,1=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14,1=a lush and fertile planet.\n" "00:00:16,1=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_multiline_b_input,
      G_N_ELEMENTS (tmplayer_multiline_b_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style1)
{
  static SubParseInputChunk tmplayer_style1_input[] = {
    {
          "00:00:10:This is the Earth at a time|when the dinosaurs roamed...\n"
          "00:00:13:\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14:a lush and fertile planet.\n" "00:00:16:\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style1_input,
      G_N_ELEMENTS (tmplayer_style1_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style2)
{
  static SubParseInputChunk tmplayer_style2_input[] = {
    {
          "00:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "00:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "00:00:14=a lush and fertile planet.\n" "00:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style2_input,
      G_N_ELEMENTS (tmplayer_style2_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style3)
{
  static SubParseInputChunk tmplayer_style3_input[] = {
    {
          "0:00:10:This is the Earth at a time|when the dinosaurs roamed...\n"
          "0:00:13:\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14:a lush and fertile planet.\n" "0:00:16:\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style3_input,
      G_N_ELEMENTS (tmplayer_style3_input));
}

GST_END_TEST;

/* also tests the max_duration stuff (see second-last chunk which is supposed
 * to be clipped to 5s duration) */
GST_START_TEST (test_tmplayer_style3b)
{
  static SubParseInputChunk tmplayer_style3b_input[] = {
    {
          "0:00:10:This is the Earth at a time|when the dinosaurs roamed...\n",
          10 * GST_SECOND, 14 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14:a lush and fertile planet.\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}, {
          "0:00:16:And they liked it a lot.\n",
        16 * GST_SECOND, (16 + 5) * GST_SECOND, "And they liked it a lot."}, {
          "0:00:30:Last line.",
        30 * GST_SECOND, GST_CLOCK_TIME_NONE, "Last line."}
  };

  test_tmplayer_do_test (tmplayer_style3b_input,
      G_N_ELEMENTS (tmplayer_style3b_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style4)
{
  static SubParseInputChunk tmplayer_style4_input[] = {
    {
          "0:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "0:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14=a lush and fertile planet.\n" "0:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style4_input,
      G_N_ELEMENTS (tmplayer_style4_input));
}

GST_END_TEST;

GST_START_TEST (test_tmplayer_style4_with_bogus_lines)
{
  static SubParseInputChunk tmplayer_style4b_input[] = {
    {
          "0:00:10=This is the Earth at a time|when the dinosaurs roamed...\n"
          "# This is a bogus line with a comment and should just be skipped\n"
          "0:00:13=\n",
          10 * GST_SECOND, 13 * GST_SECOND,
        "This is the Earth at a time\nwhen the dinosaurs roamed..."}, {
          "0:00:14=a lush and fertile planet.\n"
          "                                                            \n"
          "0:00:16=\n",
          14 * GST_SECOND, 16 * GST_SECOND,
        "a lush and fertile planet."}
  };

  test_tmplayer_do_test (tmplayer_style4b_input,
      G_N_ELEMENTS (tmplayer_style4b_input));
}

GST_END_TEST;

GST_START_TEST (test_microdvd_with_italics)
{
  static SubParseInputChunk microdvd_italics[] = {
    {
          "{1}{1}25.000 movie info: XVID  608x256 25.0fps 699.0 MB|"
          "/SubEdit b.4060(http://subedit.com.pl)/\n"
          "{100}{200}/italics/|not italics\n",
          4 * GST_SECOND, 8 * GST_SECOND,
        "<span style=\"italic\">italics</span>\n" "<span>not italics</span>"}
  };

  test_microdvd_do_test (microdvd_italics, G_N_ELEMENTS (microdvd_italics));
}

GST_END_TEST;

GST_START_TEST (test_microdvd_with_fps)
{
  static SubParseInputChunk microdvd_input[] = {
    {
          "{1}{1}12.500\n{100}{200}- Hi, Eddie.|- Hiya, Scotty.\n",
          8 * GST_SECOND, 16 * GST_SECOND,
        "<span>- Hi, Eddie.</span>\n<span>- Hiya, Scotty.</span>"}, {
          "{1250}{1350}- Cold enough for you?|- Well, I'm only faintly alive. "
          "It's 25 below\n",
          100 * GST_SECOND, 108 * GST_SECOND,
        "<span>- Cold enough for you?</span>\n"
          "<span>- Well, I&apos;m only faintly alive. It&apos;s 25 below</span>"}
  };

  test_microdvd_do_test (microdvd_input, G_N_ELEMENTS (microdvd_input));

  /* and the same with ',' instead of '.' as floating point divider */
  microdvd_input[0].in =
      "{1}{1}12,500\n{100}{200}- Hi, Eddie.|- Hiya, Scotty.\n";
  test_microdvd_do_test (microdvd_input, G_N_ELEMENTS (microdvd_input));
}

GST_END_TEST;

GST_START_TEST (test_mpl2)
{
  SubParseInputChunk mpl2_input[] = {
    {
          "[123][456] This is the Earth at a time|when the dinosaurs roamed\n",
          (123 * GST_SECOND) / 10, (456 * GST_SECOND) / 10,
        "This is the Earth at a time\nwhen the dinosaurs roamed"}, {
          "[1234][5678]a lush and fertile planet.\n",
          (1234 * GST_SECOND) / 10, (5678 * GST_SECOND) / 10,
        "a lush and fertile planet."}, {
          "[12345][27890] /Italic|Normal\n",
          (12345 * GST_SECOND) / 10, (27890 * GST_SECOND) / 10,
        "<i>Italic</i>\nNormal"}, {
          "[32345][37890]/Italic|/Italic\n",
          (32345 * GST_SECOND) / 10, (37890 * GST_SECOND) / 10,
        "<i>Italic</i>\n<i>Italic</i>"}, {
          "[42345][47890] Normal|/Italic",
          (42345 * GST_SECOND) / 10, (47890 * GST_SECOND) / 10,
        "Normal\n<i>Italic</i>"}
  };

  do_test (mpl2_input, G_N_ELEMENTS (mpl2_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_subviewer)
{
  SubParseInputChunk subviewer_input[] = {
    {
          "[INFORMATION]\n"
          "[TITLE]xxxxxxxxxx\n"
          "[AUTHOR]xxxxxxxx\n"
          "[SOURCE]xxxxxxxxxxxxxxxx\n"
          "[FILEPATH]\n"
          "[DELAY]0\n"
          "[COMMENT]\n"
          "[END INFORMATION]\n"
          "[SUBTITLE]\n"
          "[COLF]&HFFFFFF,[STYLE]bd,[SIZE]18,[FONT]Arial\n"
          "00:00:41.00,00:00:44.40\n"
          "The Age of Gods was closing.\n"
          "Eternity had come to an end.\n"
          "\n", 41 * GST_SECOND, 44 * GST_SECOND + 40 * GST_MSECOND,
        "The Age of Gods was closing.\nEternity had come to an end."}, {
          "00:00:55.00,00:00:58.40\n"
          "The heavens shook as the armies\n"
          "of Falis, God of Light...\n\n", 55 * GST_SECOND,
          58 * GST_SECOND + 40 * GST_MSECOND,
        "The heavens shook as the armies\nof Falis, God of Light..."}
  };

  do_test (subviewer_input, G_N_ELEMENTS (subviewer_input), "utf8");
}

GST_END_TEST;

GST_START_TEST (test_subviewer2)
{
  SubParseInputChunk subviewer2_input[] = {
    {
          "[INFORMATION]\n"
          "[TITLE]xxxxxxxxxx\n"
          "[AUTHOR]xxxxxxxxxx\n"
          "[SOURCE]xxxxxxxxxx\n"
          "[PRG]\n"
          "[FILEPATH]\n"
          "[DELAY]0\n"
          "[CD TRACK]0\n"
          "[COMMENT]\n"
          "[END INFORMATION]\n"
          "[SUBTITLE]\n"
          "[COLF]&H00FFFF,[STYLE]no,[SIZE]12,[FONT]Courier New\n"
          "00:00:07.00,00:00:11.91\n"
          "THERE IS A PLACE ON EARTH WHERE IT[br]IS STILL THE MORNING OF LIFE...\n\n",
          7 * GST_SECOND, 11 * GST_SECOND + 91 * GST_MSECOND,
        "THERE IS A PLACE ON EARTH WHERE IT\nIS STILL THE MORNING OF LIFE..."}, {
          "00:00:12.48,00:00:15.17\n"
          "AND THE GREAT HERDS RUN FREE.[br]SO WHAT?!\n\n",
          12 * GST_SECOND + 48 * GST_MSECOND,
          15 * GST_SECOND + 17 * GST_MSECOND,
        "AND THE GREAT HERDS RUN FREE.\nSO WHAT?!"}
  };

  do_test (subviewer2_input, G_N_ELEMENTS (subviewer2_input), "utf8");
}

GST_END_TEST;

GST_START_TEST (test_dks)
{
  SubParseInputChunk dks_input[] = {
    {
          "[00:00:07]THERE IS A PLACE ON EARTH WHERE IT[br]IS STILL THE MORNING OF LIFE...\n[00:00:12]\n",
          7 * GST_SECOND, 12 * GST_SECOND,
        "THERE IS A PLACE ON EARTH WHERE IT\nIS STILL THE MORNING OF LIFE..."}, {
          "[00:00:13]AND THE GREAT HERDS RUN FREE.[br]SO WHAT?!\n[00:00:15]\n",
          13 * GST_SECOND, 15 * GST_SECOND,
        "AND THE GREAT HERDS RUN FREE.\nSO WHAT?!"}
  };

  do_test (dks_input, G_N_ELEMENTS (dks_input), "utf8");
}

GST_END_TEST;

GST_START_TEST (test_sami)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<HEAD>\n"
          "    <TITLE>Subtitle</TITLE>\n"
          "    <STYLE TYPE=\"text/css\">\n"
          "    <!--\n"
          "        P {margin-left:8pt; margin-right:8pt; margin-bottom:2pt; margin-top:2pt; text-align:center; font-size:12pt; font-weight:normal; color:black;}\n"
          "        .CC {Name:English; lang:en-AU; SAMIType:CC;}\n"
          "        #STDPrn {Name:Standard Print;}\n"
          "        #LargePrn {Name:Large Print; font-size:24pt;}\n"
          "        #SmallPrn {Name:Small Print; font-size:16pt;}\n"
          "    -->\n"
          "    </Style>\n"
          "</HEAD>\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=CC>\n"
          "            This is a comment.<br>\n"
          "            This is a second comment.\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "This is a comment.\nThis is a second comment."},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n"
          "            This is a third comment.<br>\n"
          "            This is a fourth comment.\n" "</BODY>\n" "</SAMI>\n",
          2000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "This is a third comment.\nThis is a fourth comment."}
  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_sami_xml_entities)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=CC>\n" "            &lt;Hello&gt; &amp;\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "&lt;Hello&gt; &amp;"},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n"
          "            &quot;World&apos;\n" "</BODY>\n" "</SAMI>\n",
          2000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "&quot;World&apos;"}

  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_sami_html_entities)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=CC>\n" "            &nbsp; &plusmn; &acute;\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "\xc2\xa0 \xc2\xb1 \xc2\xb4"},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n" "            &Alpha; &omega;\n",
          2000 * GST_MSECOND, 3000 * GST_MSECOND,
        "\xce\x91 \xcf\x89"},
    {"    <SYNC Start=3000>\n"
          "        <P Class=CC>\n"
          "            &#xa0; &#177; &#180;\n" "</BODY>\n" "</SAMI>\n",
          3000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "\xc2\xa0 \xc2\xb1 \xc2\xb4"}
  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_sami_bad_entities)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=CC>\n" "            &nbsp &\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "\xc2\xa0 &amp;"},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n"
          "            &#xa0 &#177 &#180;\n" "</BODY>\n" "</SAMI>\n",
          2000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "\xc2\xa0 \xc2\xb1 \xc2\xb4"}
  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_sami_comment)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<!--\n"
          "=======\n"
          "foo bar\n"
          "=======\n"
          "-->\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=\"C====\">\n" "            &nbsp &\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "\xc2\xa0 &amp;"},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n"
          "            &#xa0 &#177 &#180;\n" "</BODY>\n" "</SAMI>\n",
          2000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "\xc2\xa0 \xc2\xb1 \xc2\xb4"}
  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_sami_self_contained_tags)
{
  SubParseInputChunk sami_input[] = {
    {"<SAMI>\n"
          "<BODY>\n"
          "    <SYNC Start=1000>\n"
          "        <P Class=CC>\n"
          "            This line has a self-closing format tag<i /> and more.\n",
          1000 * GST_MSECOND, 2000 * GST_MSECOND,
        "This line has a self-closing format tag<i></i>and more."},
    {"    <SYNC Start=2000>\n"
          "        <P Class=CC>\n"
          "            This is a third comment.<br>\n"
          "            This is a fourth comment.\n" "</BODY>\n" "</SAMI>\n",
          2000 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "This is a third comment.\nThis is a fourth comment."}
  };

  do_test (sami_input, G_N_ELEMENTS (sami_input), "pango-markup");
}

GST_END_TEST;

GST_START_TEST (test_lrc)
{
  SubParseInputChunk lrc_input[] = {
    {"[ar:123]\n" "[ti:Title]\n" "[al:Album]\n" "[00:02.23]Line 1\n",
          2230 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "Line 1"},
    {"[00:05.10]Line 2\n",
          5100 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "Line 2"},
    {"[00:06.123]Line 3\n",
          6123 * GST_MSECOND, GST_CLOCK_TIME_NONE,
        "Line 3"}
  };

  do_test (lrc_input, G_N_ELEMENTS (lrc_input), "utf8");
}

GST_END_TEST;

GST_START_TEST (test_raw_conversion)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;

  h = gst_harness_new ("subparse");

  gst_harness_set_src_caps_str (h, "application/x-subtitle");
  gst_harness_set_sink_caps_str (h, "text/x-raw, format=utf8");

  buffer = buffer_from_static_string (srt_input[5].in);

  buffer = gst_harness_push_and_pull (h, buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless_equals_int (map.size, 3);
  fail_unless_equals_string ((gchar *) map.data, "Six");
  gst_buffer_unmap (buffer, &map);

  gst_clear_buffer (&buffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

/* TODO:
 *  - add/modify tests so that lines aren't dogfed to the parsers in complete
 *    lines or sets of complete lines, but rather in random chunks
 */

static Suite *
subparse_suite (void)
{
  Suite *s = suite_create ("subparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_srt);
  tcase_add_test (tc_chain, test_webvtt);
  tcase_add_test (tc_chain, test_tmplayer_multiline);
  tcase_add_test (tc_chain, test_tmplayer_multiline_with_bogus_lines);
  tcase_add_test (tc_chain, test_tmplayer_style1);
  tcase_add_test (tc_chain, test_tmplayer_style2);
  tcase_add_test (tc_chain, test_tmplayer_style3);
  tcase_add_test (tc_chain, test_tmplayer_style3b);
  tcase_add_test (tc_chain, test_tmplayer_style4);
  tcase_add_test (tc_chain, test_tmplayer_style4_with_bogus_lines);
  tcase_add_test (tc_chain, test_microdvd_with_fps);
  tcase_add_test (tc_chain, test_microdvd_with_italics);
  tcase_add_test (tc_chain, test_mpl2);
  tcase_add_test (tc_chain, test_subviewer);
  tcase_add_test (tc_chain, test_subviewer2);
  tcase_add_test (tc_chain, test_dks);
  tcase_add_test (tc_chain, test_sami);
  tcase_add_test (tc_chain, test_sami_xml_entities);
  tcase_add_test (tc_chain, test_sami_html_entities);
  tcase_add_test (tc_chain, test_sami_bad_entities);
  tcase_add_test (tc_chain, test_sami_comment);
  tcase_add_test (tc_chain, test_sami_self_contained_tags);
  tcase_add_test (tc_chain, test_lrc);
  tcase_add_test (tc_chain, test_raw_conversion);
  return s;
}

GST_CHECK_MAIN (subparse);
