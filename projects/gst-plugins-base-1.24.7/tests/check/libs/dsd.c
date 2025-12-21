/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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
#include <gst/audio/gstdsd.h>


#define NUM_CHANNELS (2)
#define NUM_BYTES_PER_CHANNEL (16)
#define NUM_PATTERN_BYTES (NUM_CHANNELS * NUM_BYTES_PER_CHANNEL)


typedef struct
{
  GstDsdFormat format;
  guint8 bytes[NUM_CHANNELS * NUM_BYTES_PER_CHANNEL];
} DsdTestPattern;

/* The following test patterns contain bytes 0x00 to 0x0F for the left channel
 * and 0x80 to 0x8F for the right channel. These bytes are grouped in the test
 * patterns according to their (non-)interleaved layout and grouping format. */

/* *INDENT-OFF* */

static const DsdTestPattern interleaved_dsd_test_patterns[] = {
  {
    GST_DSD_FORMAT_U8,
    {
      0x00, 0x80, 0x01, 0x81, 0x02, 0x82, 0x03, 0x83, 0x04, 0x84, 0x05, 0x85, 0x06, 0x86, 0x07, 0x87,
      0x08, 0x88, 0x09, 0x89, 0x0A, 0x8A, 0x0B, 0x8B, 0x0C, 0x8C, 0x0D, 0x8D, 0x0E, 0x8E, 0x0F, 0x8F
    }
  },
  {
    GST_DSD_FORMAT_U16LE,
    {
      0x01, 0x00,   0x81, 0x80,   0x03, 0x02,   0x83, 0x82,   0x05, 0x04,   0x85, 0x84,   0x07, 0x06,   0x87, 0x86,
      0x09, 0x08,   0x89, 0x88,   0x0B, 0x0A,   0x8B, 0x8A,   0x0D, 0x0C,   0x8D, 0x8C,   0x0F, 0x0E,   0x8F, 0x8E
    }
  },
  {
    GST_DSD_FORMAT_U16BE,
    {
      0x00, 0x01,   0x80, 0x81,   0x02, 0x03,   0x82, 0x83,   0x04, 0x05,   0x84, 0x85,   0x06, 0x07,   0x86, 0x87,
      0x08, 0x09,   0x88, 0x89,   0x0A, 0x0B,   0x8A, 0x8B,   0x0C, 0x0D,   0x8C, 0x8D,   0x0E, 0x0F,   0x8E, 0x8F
    }
  },
  {
    GST_DSD_FORMAT_U32LE,
    {
      0x03, 0x02, 0x01, 0x00,   0x83, 0x82, 0x81, 0x80,   0x07, 0x06, 0x05, 0x04,   0x87, 0x86, 0x85, 0x84,
      0x0B, 0x0A, 0x09, 0x08,   0x8B, 0x8A, 0x89, 0x88,   0x0F, 0x0E, 0x0D, 0x0C,   0x8F, 0x8E, 0x8D, 0x8C
    }
  },
  {
    GST_DSD_FORMAT_U32BE,
    {
      0x00, 0x01, 0x02, 0x03,   0x80, 0x81, 0x82, 0x83,   0x04, 0x05, 0x06, 0x07,   0x84, 0x85, 0x86, 0x87,
      0x08, 0x09, 0x0A, 0x0B,   0x88, 0x89, 0x8A, 0x8B,   0x0C, 0x0D, 0x0E, 0x0F,   0x8C, 0x8D, 0x8E, 0x8F
    }
  }
};

/* *INDENT-ON* */

static const int num_interleaved_dsd_test_patterns =
    sizeof (interleaved_dsd_test_patterns) / sizeof (DsdTestPattern);

/* *INDENT-OFF* */

static const DsdTestPattern non_interleaved_dsd_test_patterns[] = {
  {
    GST_DSD_FORMAT_U8,
    {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F
    }
  },
  {
    GST_DSD_FORMAT_U16LE,
    {
      0x01, 0x00,   0x03, 0x02,   0x05, 0x04,   0x07, 0x06,   0x09, 0x08,   0x0B, 0x0A,   0x0D, 0x0C,   0x0F, 0x0E,
      0x81, 0x80,   0x83, 0x82,   0x85, 0x84,   0x87, 0x86,   0x89, 0x88,   0x8B, 0x8A,   0x8D, 0x8C,   0x8F, 0x8E
    }
  },
  {
    GST_DSD_FORMAT_U16BE,
    {
      0x00, 0x01,   0x02, 0x03,   0x04, 0x05,   0x06, 0x07,   0x08, 0x09,   0x0A, 0x0B,   0x0C, 0x0D,   0x0E, 0x0F,
      0x80, 0x81,   0x82, 0x83,   0x84, 0x85,   0x86, 0x87,   0x88, 0x89,   0x8A, 0x8B,   0x8C, 0x8D,   0x8E, 0x8F,
    }
  },
  {
    GST_DSD_FORMAT_U32LE,
    {
      0x03, 0x02, 0x01, 0x00,   0x07, 0x06, 0x05, 0x04,   0x0B, 0x0A, 0x09, 0x08,   0x0F, 0x0E, 0x0D, 0x0C,
      0x83, 0x82, 0x81, 0x80,   0x87, 0x86, 0x85, 0x84,   0x8B, 0x8A, 0x89, 0x88,   0x8F, 0x8E, 0x8D, 0x8C,
    }
  },
  {
    GST_DSD_FORMAT_U32BE,
    {
      0x00, 0x01, 0x02, 0x03,   0x04, 0x05, 0x06, 0x07,   0x08, 0x09, 0x0A, 0x0B,   0x0C, 0x0D, 0x0E, 0x0F,
      0x80, 0x81, 0x82, 0x83,   0x84, 0x85, 0x86, 0x87,   0x88, 0x89, 0x8A, 0x8B,   0x8C, 0x8D, 0x8E, 0x8F,
    }
  },
};

/* *INDENT-ON* */

static const int num_non_interleaved_dsd_test_patterns =
    sizeof (non_interleaved_dsd_test_patterns) / sizeof (DsdTestPattern);

static const gsize dsd_plane_offsets[NUM_CHANNELS] = { 0, 16 };

GST_START_TEST (test_dsd_conversion_interleaved_to_interleaved)
{
  int in_idx, out_idx;
  guint8 actual_output_pattern[NUM_PATTERN_BYTES];

  for (in_idx = 0; in_idx < num_interleaved_dsd_test_patterns; ++in_idx) {
    for (out_idx = 0; out_idx < num_interleaved_dsd_test_patterns; ++out_idx) {
      guint8 const *input_pattern = interleaved_dsd_test_patterns[in_idx].bytes;
      guint8 const *expected_output_pattern =
          interleaved_dsd_test_patterns[out_idx].bytes;
      GstDsdFormat input_format = interleaved_dsd_test_patterns[in_idx].format;
      GstDsdFormat output_format =
          interleaved_dsd_test_patterns[out_idx].format;
      gboolean conversion_ok;

      gst_dsd_convert (input_pattern, actual_output_pattern, input_format,
          output_format, GST_AUDIO_LAYOUT_INTERLEAVED,
          GST_AUDIO_LAYOUT_INTERLEAVED, NULL, NULL, NUM_PATTERN_BYTES,
          NUM_CHANNELS, FALSE);

      conversion_ok = memcmp (actual_output_pattern, expected_output_pattern,
          NUM_PATTERN_BYTES) == 0;

      if (!conversion_ok) {
        GST_MEMDUMP ("expected:", expected_output_pattern, NUM_PATTERN_BYTES);
        GST_MEMDUMP ("actual  :", actual_output_pattern, NUM_PATTERN_BYTES);
      }

      fail_unless (conversion_ok, "DSD conversion output incorrect");
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_dsd_conversion_interleaved_to_non_interleaved)
{
  int in_idx, out_idx;
  guint8 actual_output_pattern[NUM_PATTERN_BYTES];

  for (in_idx = 0; in_idx < num_interleaved_dsd_test_patterns; ++in_idx) {
    for (out_idx = 0; out_idx < num_non_interleaved_dsd_test_patterns;
        ++out_idx) {
      guint8 const *input_pattern = interleaved_dsd_test_patterns[in_idx].bytes;
      guint8 const *expected_output_pattern =
          non_interleaved_dsd_test_patterns[out_idx].bytes;
      GstDsdFormat input_format = interleaved_dsd_test_patterns[in_idx].format;
      GstDsdFormat output_format =
          non_interleaved_dsd_test_patterns[out_idx].format;
      gboolean conversion_ok;

      gst_dsd_convert (input_pattern, actual_output_pattern, input_format,
          output_format, GST_AUDIO_LAYOUT_INTERLEAVED,
          GST_AUDIO_LAYOUT_NON_INTERLEAVED, NULL, dsd_plane_offsets,
          NUM_PATTERN_BYTES, NUM_CHANNELS, FALSE);

      conversion_ok = memcmp (actual_output_pattern, expected_output_pattern,
          NUM_PATTERN_BYTES) == 0;

      if (!conversion_ok) {
        GST_MEMDUMP ("expected:", expected_output_pattern, NUM_PATTERN_BYTES);
        GST_MEMDUMP ("actual  :", actual_output_pattern, NUM_PATTERN_BYTES);
      }

      fail_unless (conversion_ok, "DSD conversion output incorrect");
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_dsd_conversion_non_interleaved_to_interleaved)
{
  int in_idx, out_idx;
  guint8 actual_output_pattern[NUM_PATTERN_BYTES];

  for (in_idx = 0; in_idx < num_non_interleaved_dsd_test_patterns; ++in_idx) {
    for (out_idx = 0; out_idx < num_interleaved_dsd_test_patterns; ++out_idx) {
      guint8 const *input_pattern =
          non_interleaved_dsd_test_patterns[in_idx].bytes;
      guint8 const *expected_output_pattern =
          interleaved_dsd_test_patterns[out_idx].bytes;
      GstDsdFormat input_format =
          non_interleaved_dsd_test_patterns[in_idx].format;
      GstDsdFormat output_format =
          interleaved_dsd_test_patterns[out_idx].format;
      gboolean conversion_ok;

      gst_dsd_convert (input_pattern,
          actual_output_pattern,
          input_format,
          output_format,
          GST_AUDIO_LAYOUT_NON_INTERLEAVED,
          GST_AUDIO_LAYOUT_INTERLEAVED,
          dsd_plane_offsets, NULL, NUM_PATTERN_BYTES, NUM_CHANNELS, FALSE);

      conversion_ok = memcmp (actual_output_pattern, expected_output_pattern,
          NUM_PATTERN_BYTES) == 0;

      if (!conversion_ok) {
        GST_MEMDUMP ("expected:", expected_output_pattern, NUM_PATTERN_BYTES);
        GST_MEMDUMP ("actual  :", actual_output_pattern, NUM_PATTERN_BYTES);
      }

      fail_unless (conversion_ok, "DSD conversion output incorrect");
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_dsd_conversion_non_interleaved_to_non_interleaved)
{
  int in_idx, out_idx;
  guint8 actual_output_pattern[NUM_PATTERN_BYTES];

  for (in_idx = 0; in_idx < num_non_interleaved_dsd_test_patterns; ++in_idx) {
    for (out_idx = 0; out_idx < num_non_interleaved_dsd_test_patterns;
        ++out_idx) {
      guint8 const *input_pattern =
          non_interleaved_dsd_test_patterns[in_idx].bytes;
      guint8 const *expected_output_pattern =
          non_interleaved_dsd_test_patterns[out_idx].bytes;
      GstDsdFormat input_format =
          non_interleaved_dsd_test_patterns[in_idx].format;
      GstDsdFormat output_format =
          non_interleaved_dsd_test_patterns[out_idx].format;
      gboolean conversion_ok;

      gst_dsd_convert (input_pattern,
          actual_output_pattern,
          input_format,
          output_format,
          GST_AUDIO_LAYOUT_NON_INTERLEAVED,
          GST_AUDIO_LAYOUT_NON_INTERLEAVED,
          dsd_plane_offsets,
          dsd_plane_offsets, NUM_PATTERN_BYTES, NUM_CHANNELS, FALSE);

      conversion_ok = memcmp (actual_output_pattern, expected_output_pattern,
          NUM_PATTERN_BYTES) == 0;

      if (!conversion_ok) {
        GST_MEMDUMP ("expected:", expected_output_pattern, NUM_PATTERN_BYTES);
        GST_MEMDUMP ("actual  :", actual_output_pattern, NUM_PATTERN_BYTES);
      }

      fail_unless (conversion_ok, "DSD conversion output incorrect");
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_dsd_info_from_caps)
{
  GstDsdInfo info;
  GstCaps *full_caps, *minimal_caps;

  full_caps = gst_caps_new_simple (GST_DSD_MEDIA_TYPE,
      "format", G_TYPE_STRING, "DSDU16LE",
      "rate", G_TYPE_INT, GST_DSD_MAKE_DSD_RATE_44x (128),
      "channels", G_TYPE_INT, 2,
      "layout", G_TYPE_STRING, "non-interleaved",
      "channel-mask", GST_TYPE_BITMASK, 0x3, NULL);
  fail_unless (gst_dsd_info_from_caps (&info, full_caps));
  fail_unless_equals_int (info.format, GST_DSD_FORMAT_U16LE);
  fail_unless_equals_int (info.rate, GST_DSD_MAKE_DSD_RATE_44x (128));
  fail_unless_equals_int (info.channels, 2);
  fail_unless_equals_int (info.layout, GST_AUDIO_LAYOUT_NON_INTERLEAVED);
  fail_unless_equals_int (info.positions[0],
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
  fail_unless_equals_int (info.positions[1],
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
  gst_caps_unref (full_caps);

  minimal_caps = gst_caps_new_simple (GST_DSD_MEDIA_TYPE,
      "format", G_TYPE_STRING, "DSDU16LE",
      "rate", G_TYPE_INT, GST_DSD_MAKE_DSD_RATE_44x (128),
      "channels", G_TYPE_INT, 2, NULL);
  fail_unless (gst_dsd_info_from_caps (&info, minimal_caps));
  fail_unless_equals_int (info.format, GST_DSD_FORMAT_U16LE);
  fail_unless_equals_int (info.rate, GST_DSD_MAKE_DSD_RATE_44x (128));
  fail_unless_equals_int (info.channels, 2);
  fail_unless_equals_int (info.layout, GST_AUDIO_LAYOUT_INTERLEAVED);
  fail_unless_equals_int (info.positions[0],
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
  fail_unless_equals_int (info.positions[1],
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
  gst_caps_unref (minimal_caps);
}

GST_END_TEST;

GST_START_TEST (test_dsd_info_to_caps)
{
  GstStructure *str;
  GstCaps *caps;
  gint rate;
  gint channels;
  guint64 channel_mask;
  GstDsdInfo info = {
    .format = GST_DSD_FORMAT_U16LE,
    .rate = GST_DSD_MAKE_DSD_RATE_44x (64),
    .channels = 2,
    .layout = GST_AUDIO_LAYOUT_INTERLEAVED,
    .positions = {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}
    ,
    .flags = GST_AUDIO_FLAG_NONE
  };

  caps = gst_dsd_info_to_caps (&info);
  fail_if (caps == NULL, "caps were not created");
  fail_unless_equals_int (gst_caps_get_size (caps), 1);

  str = gst_caps_get_structure (caps, 0);
  fail_if (str == NULL, "could not get structure");
  fail_unless (gst_structure_has_name (str, GST_DSD_MEDIA_TYPE));
  fail_unless (gst_structure_has_field_typed (str, "format", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (str, "format"),
      "DSDU16LE");
  fail_unless (gst_structure_get_int (str, "rate", &rate));
  fail_unless_equals_int (rate, GST_DSD_MAKE_DSD_RATE_44x (64));
  fail_unless (gst_structure_get_int (str, "channels", &channels));
  fail_unless_equals_int (channels, 2);
  fail_unless_equals_string (gst_structure_get_string (str, "layout"),
      "interleaved");
  fail_unless (gst_structure_get (str, "channel-mask", GST_TYPE_BITMASK,
          &channel_mask, NULL));
  fail_unless_equals_uint64 (channel_mask, 0x3);

  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
dsd_suite (void)
{
  Suite *s = suite_create ("dsd");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_dsd_conversion_interleaved_to_interleaved);
  tcase_add_test (tc_chain, test_dsd_conversion_interleaved_to_non_interleaved);
  tcase_add_test (tc_chain, test_dsd_conversion_non_interleaved_to_interleaved);
  tcase_add_test (tc_chain,
      test_dsd_conversion_non_interleaved_to_non_interleaved);
  tcase_add_test (tc_chain, test_dsd_info_from_caps);
  tcase_add_test (tc_chain, test_dsd_info_to_caps);

  return s;
}

GST_CHECK_MAIN (dsd);
