/* GStreamer unit tests for the -base typefind functions
 *
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
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
#include <gst/base/gsttypefindhelper.h>

static GstCaps *
typefind_data (const guint8 * data, gsize data_size,
    GstTypeFindProbability * prob)
{
  GstBuffer *buf;
  GstCaps *caps;

  GST_MEMDUMP ("typefind data", data, data_size);
  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (guint8 *) data, data_size, 0, data_size, NULL, NULL));
  GST_BUFFER_OFFSET (buf) = 0;

  caps = gst_type_find_helper_for_buffer (NULL, buf, prob);
  GST_INFO ("caps: %" GST_PTR_FORMAT ", probability=%u", caps, *prob);

  gst_buffer_unref (buf);

  return caps;
}

GST_START_TEST (test_quicktime_mpeg4video)
{
  /* quicktime redirect file which starts with what could also be interpreted
   * as an MPEG-4 video object layer start code */
  const guint8 qt_redirect_396042[] =
      { 0x00, 0x00, 0x01, 0x22, 0x6d, 0x6f, 0x6f, 0x76, 0x00, 0x00, 0x01, 0x1a,
    0x72, 0x6d, 0x72, 0x61, 0x00, 0x00, 0x00, 0x86, 0x72, 0x6d, 0x64, 0x61,
    0x00, 0x00, 0x00, 0x54, 0x72, 0x64, 0x72, 0x66, 0x00, 0x00, 0x00, 0x00,
    0x75, 0x72, 0x6c, 0x20, 0x00, 0x00, 0x00, 0x40, 0x68, 0x74, 0x74, 0x70,
    0x3a, 0x2f, 0x2f, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2e, 0x71, 0x74,
    0x76, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
    0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x2f, 0x6a, 0x61, 0x6e, 0x2f, 0x6a,
    0x34, 0x37, 0x64, 0x35, 0x32, 0x6f, 0x6f, 0x2f, 0x71, 0x74, 0x37, 0x72,
    0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x2e, 0x6d, 0x6f, 0x76, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x72, 0x6d, 0x64, 0x72, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0a, 0xf0, 0x00, 0x00, 0x00, 0x1a, 0x72, 0x6d, 0x76, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x71, 0x74, 0x69, 0x6d, 0x06, 0x50, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x72, 0x6d,
    0x64, 0x61, 0x00, 0x00, 0x00, 0x5a, 0x72, 0x64, 0x72, 0x66, 0x00, 0x00,
    0x00, 0x00, 0x75, 0x72, 0x6c, 0x20, 0x00, 0x00, 0x00, 0x46, 0x68, 0x74,
    0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2e,
    0x71, 0x74, 0x76, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f,
    0x6d, 0x2f, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x2f, 0x6a, 0x61, 0x6e,
    0x2f, 0x6a, 0x34, 0x37, 0x64, 0x35, 0x32, 0x6f, 0x6f, 0x2f, 0x38, 0x38,
    0x34, 0x38, 0x31, 0x32, 0x35, 0x5f, 0x32, 0x5f, 0x33, 0x35, 0x30, 0x5f,
    0x72, 0x65, 0x66, 0x2e, 0x6d, 0x6f, 0x76, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x72, 0x6d, 0x64, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0xf0,
    0x00, 0x00, 0x00, 0x1a, 0x72, 0x6d, 0x76, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x71, 0x74, 0x69, 0x6d, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
  };
  GstTypeFindProbability prob;
  const gchar *type;
  GstBuffer *buf;
  GstCaps *caps = NULL;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (gpointer) qt_redirect_396042,
          sizeof (qt_redirect_396042), 0, sizeof (qt_redirect_396042), NULL,
          NULL));

  GST_BUFFER_OFFSET (buf) = 0;

  caps = gst_type_find_helper_for_buffer (NULL, buf, &prob);
  fail_unless (caps != NULL);
  GST_LOG ("Found type: %" GST_PTR_FORMAT, caps);

  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (type, "video/quicktime");
  fail_unless (prob > GST_TYPE_FIND_MINIMUM && prob <= GST_TYPE_FIND_MAXIMUM);

  gst_buffer_unref (buf);
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_broken_flac_in_ogg)
{
  const guint8 flac_id_packet[4] = { 'f', 'L', 'a', 'C' };
  GstTypeFindProbability prob;
  const gchar *type;
  GstBuffer *buf;
  GstCaps *caps = NULL;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (gpointer) flac_id_packet,
          sizeof (flac_id_packet), 0, sizeof (flac_id_packet), NULL, NULL));

  GST_BUFFER_OFFSET (buf) = 0;

  caps = gst_type_find_helper_for_buffer (NULL, buf, &prob);
  fail_unless (caps != NULL);
  GST_LOG ("Found type: %" GST_PTR_FORMAT, caps);

  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (type, "audio/x-flac");
  fail_unless (prob > GST_TYPE_FIND_MINIMUM && prob <= GST_TYPE_FIND_MAXIMUM);

  gst_buffer_unref (buf);
  gst_caps_unref (caps);
}

GST_END_TEST;

static GstCaps *
typefind_test_file (const gchar * filename)
{
  GstBuffer *buf;
  GError *err = NULL;
  GstCaps *caps = NULL;
  gchar *path, *data = NULL;
  gsize data_len;

  path = g_build_filename (GST_TEST_FILES_PATH, filename, NULL);
  GST_LOG ("reading file '%s'", path);
  if (!g_file_get_contents (path, &data, &data_len, &err)) {
    g_error ("error loading test file: %s", err->message);
    g_clear_error (&err);
  }

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          (gpointer) data, data_len, 0, data_len, NULL, NULL));

  GST_BUFFER_OFFSET (buf) = 0;

  caps = gst_type_find_helper_for_buffer (NULL, buf, NULL);
  fail_unless (caps != NULL);
  GST_LOG ("Found type: %" GST_PTR_FORMAT, caps);

  gst_buffer_unref (buf);
  g_free (data);
  g_free (path);

  return caps;
}

GST_START_TEST (test_jpeg_not_ac3)
{
  const gchar *type;
  GstCaps *caps = NULL;

  caps = typefind_test_file ("partialframe.mjpeg");
  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (type, "image/jpeg");

  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_mpegts)
{
  GstStructure *s;
  gboolean systemstream = FALSE;
  GstCaps *caps = NULL;
  gint packetsize = -1;

  caps = typefind_test_file ("623663.mts");
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/mpegts"));
  fail_unless (gst_structure_has_field (s, "systemstream"));
  fail_unless (gst_structure_get_boolean (s, "systemstream", &systemstream));
  fail_unless_equals_int (systemstream, TRUE);
  fail_unless (gst_structure_has_field (s, "packetsize"));
  fail_unless (gst_structure_get_int (s, "packetsize", &packetsize));
  fail_unless_equals_int (packetsize, 192);

  gst_caps_unref (caps);
}

GST_END_TEST;

struct ac3_frmsize
{
  unsigned frmsizecod;
  unsigned frmsize;
};

static void
make_ac3_packet (guint8 * data, guint bytesize, guint bsid)
{
  /* Actually not a fully valid packet; if the typefinder starts to
   * check e.g. the CRCs, this test needs to be improved as well. */
  const guint8 ac3_header[] = {
    0x0b, 0x77,                 /* syncword */
    0x00, 0x00,                 /* crc1 */
    0x00,                       /* fscod 0xc0, frmsizecod 0x3f */
    0x00                        /* bsid 0xf8, bsmod 0x07 */
  };
  const struct ac3_frmsize frmsize[] = {
    {17, 256}, {26, 640}        /* small subset of supported sizes */
  };
  guint wordsize = bytesize >> 1, frmsizecod = 0;
  int i;

  fail_unless ((bytesize & 0x01) == 0);
  fail_unless (bytesize >= sizeof (ac3_header));

  for (i = 0; i < G_N_ELEMENTS (frmsize); i++) {
    if (frmsize[i].frmsize == wordsize) {
      frmsizecod = frmsize[i].frmsizecod;
      break;
    }
  }

  fail_unless (frmsizecod);

  memcpy (data, ac3_header, sizeof (ac3_header));
  data[4] = (data[4] & ~0x3f) | (frmsizecod & 0x3f);
  data[5] = (bsid & 0x1f) << 3;
  memset (data + 6, 0, bytesize - 6);
}

GST_START_TEST (test_ac3)
{
  GstTypeFindProbability prob;
  const gchar *type;
  GstBuffer *buf;
  GstCaps *caps = NULL;
  guint bsid;

  for (bsid = 0; bsid < 32; bsid++) {
    GstMapInfo map;

    buf = gst_buffer_new_and_alloc ((256 + 640) * 2);
    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    make_ac3_packet (map.data, 256 * 2, bsid);
    make_ac3_packet (map.data + 256 * 2, 640 * 2, bsid);
    gst_buffer_unmap (buf, &map);

    caps = gst_type_find_helper_for_buffer (NULL, buf, &prob);
    if (bsid <= 8) {
      fail_unless (caps != NULL);
      GST_LOG ("Found type for BSID %u: %" GST_PTR_FORMAT, bsid, caps);

      type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
      fail_unless_equals_string (type, "audio/x-ac3");
      fail_unless (prob > GST_TYPE_FIND_MINIMUM
          && prob <= GST_TYPE_FIND_MAXIMUM);
      gst_caps_unref (caps);
    } else {
      fail_unless (caps == NULL);
    }

    gst_buffer_unref (buf);
  }
}

GST_END_TEST;

static void
make_eac3_packet (guint8 * data, guint bytesize, guint bsid)
{
  /* Actually not a fully valid packet; if the typefinder starts to
   * check e.g. the CRCs, this test needs to be improved as well. */
  const guint8 eac3_header[] = {
    0x0b, 0x77,                 /* syncword */
    0x00,                       /* strmtyp 0xc0, substreamid 0x38,
                                 * frmsize 0x07 (3 high bits) */
    0x00,                       /* frmsize (low bits -> 11 total) */
    0x00,                       /* fscod 0xc0, fscod2/numblocks 0x30,
                                 * acmod 0x0e, lfeon 0x01 */
    0x00                        /* bsid 0xf8, dialnorm 0x07 (3 high bits) */
  };
  guint wordsize = bytesize >> 1;

  fail_unless ((bytesize & 0x01) == 0);
  fail_unless (bytesize >= sizeof (eac3_header));

  memcpy (data, eac3_header, sizeof (eac3_header));
  data[2] = (data[2] & ~0x07) | ((((wordsize - 1) & 0x700) >> 8) & 0xff);
  data[3] = (wordsize - 1) & 0xff;
  data[5] = (bsid & 0x1f) << 3;
  memset (data + 6, 0, bytesize - 6);
}

GST_START_TEST (test_eac3)
{
  GstTypeFindProbability prob;
  const gchar *type;
  GstBuffer *buf;
  GstCaps *caps = NULL;
  guint bsid;

  for (bsid = 0; bsid <= 32; bsid++) {
    GstMapInfo map;

    buf = gst_buffer_new_and_alloc (558 + 384);
    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    make_eac3_packet (map.data, 558, bsid);
    make_eac3_packet (map.data + 558, 384, bsid);
    gst_buffer_unmap (buf, &map);

    caps = gst_type_find_helper_for_buffer (NULL, buf, &prob);
    if (bsid > 10 && bsid <= 16) {
      /* Only BSIs 11..16 are valid for Annex E */
      fail_unless (caps != NULL);
      GST_LOG ("Found type for BSID %u: %" GST_PTR_FORMAT, bsid, caps);

      type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
      fail_unless_equals_string (type, "audio/x-eac3");
      fail_unless (prob > GST_TYPE_FIND_MINIMUM
          && prob <= GST_TYPE_FIND_MAXIMUM);
      gst_caps_unref (caps);
    } else {
      /* Invalid E-AC-3 BSID, must not be detected as anything: */
      fail_unless (caps == NULL);
    }

    gst_buffer_unref (buf);
  }
}

GST_END_TEST;

#define TEST_RANDOM_DATA_SIZE (4*1024)

/* typefind random data, to make sure all typefinders are called */
GST_START_TEST (test_random_data)
{
  GstTypeFindProbability prob;
  const gchar *seed_env;
  GstBuffer *buf;
  GstCaps *caps;
  guint32 seed;
  guint8 *data;
  gint i;

  seed_env = g_getenv ("GST_TYPEFIND_TEST_SEED");
  if (seed_env != NULL)
    seed = atoi (seed_env);
  else
    seed = (guint32) time (NULL);

  g_random_set_seed (seed);

  data = g_malloc (TEST_RANDOM_DATA_SIZE);
  for (i = 0; i < TEST_RANDOM_DATA_SIZE; ++i)
    data[i] = g_random_int () & 0xff;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          data, TEST_RANDOM_DATA_SIZE, 0, TEST_RANDOM_DATA_SIZE, NULL, NULL));

  GST_BUFFER_OFFSET (buf) = 0;

  caps = gst_type_find_helper_for_buffer (NULL, buf, &prob);
  GST_INFO ("caps: %" GST_PTR_FORMAT ", probability=%u", caps, prob);
  /* for now we just print an error log message */
  if (caps != NULL /* && prob >= GST_TYPE_FIND_LIKELY */ ) {
    GST_ERROR ("typefinder thinks random data is %" GST_PTR_FORMAT ", with a "
        "probability of %u (seed was %u)", caps, prob, seed);
    gst_caps_unref (caps);
  }

  gst_buffer_unref (buf);
  g_free (data);
}

GST_END_TEST;

GST_START_TEST (test_hls_m3u8)
{
  const gchar *type;
  GstCaps *caps = NULL;

  caps = typefind_test_file ("hls.m3u8");
  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (type, "application/x-hls");

  gst_caps_unref (caps);
}

GST_END_TEST;

static const gchar MANIFEST[] =
    "<?xml version=\"1.0\" encoding=\"utf-16\"?>\n"
    "<!--Created with Expression Encoder version 2.1.1216.0-->\n"
    "<SmoothStreamingMedia\n"
    "  MajorVersion=\"1\"\n"
    "  MinorVersion=\"0\"\n"
    "  Duration=\"5965419999\">\n"
    "  <StreamIndex\n"
    "    Type=\"video\"\n"
    "    Subtype=\"WVC1\"\n"
    "    Chunks=\"299\"\n"
    "    Url=\"QualityLevels({bitrate})/Fragments(video={start time})\">\n"
    "    <QualityLevel\n"
    "      Bitrate=\"2750000\"\n"
    "      FourCC=\"WVC1\"\n" "      Width=\"1280\"\n" "      Height=\"720\"\n";

static guint8 *
generate_utf16 (guint off_lo, guint off_hi)
{
  guint8 *utf16;
  gsize len, i;

  len = strlen (MANIFEST);
  /* BOM + UTF-16 string */
  utf16 = g_malloc (2 + len * 2);
  utf16[off_lo] = 0xff;
  utf16[off_hi] = 0xfe;
  for (i = 0; i < len; ++i) {
    utf16[2 + (2 * i) + off_lo] = MANIFEST[i];
    utf16[2 + (2 * i) + off_hi] = 0x00;
  }

  return utf16;
}

/* Test that we can typefind UTF16-LE and UTF16-BE variants
 * of smooth streaming manifests (even without iconv) */
GST_START_TEST (test_manifest_typefinding)
{
  GstTypeFindProbability prob;
  const gchar *media_type;
  GstCaps *caps;
  guint8 *utf16;

  utf16 = generate_utf16 (0, 1);
  prob = 0;
  caps = typefind_data (utf16, 2 + strlen (MANIFEST) * 2, &prob);
  fail_unless (caps != NULL);
  media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (media_type, "application/vnd.ms-sstr+xml");
  fail_unless_equals_int (prob, GST_TYPE_FIND_MAXIMUM);
  gst_caps_unref (caps);
  g_free (utf16);

  utf16 = generate_utf16 (1, 0);
  prob = 0;
  caps = typefind_data (utf16, 2 + strlen (MANIFEST) * 2, &prob);
  fail_unless (caps != NULL);
  media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (media_type, "application/vnd.ms-sstr+xml");
  fail_unless_equals_int (prob, GST_TYPE_FIND_MAXIMUM);
  gst_caps_unref (caps);
  g_free (utf16);
}

GST_END_TEST;

GST_START_TEST (test_webvtt)
{
  GstTypeFindProbability prob;
  const gchar *media_type;
  GstCaps *caps;
  guint8 webvtt[] = {
    'W', 'E', 'B', 'V', 'T', 'T', '\n', '\n'
  };
  guint8 webvtt_with_bom[] = {
    0xef, 0xbb, 0xbf, 'W', 'E', 'B', 'V', 'T', 'T', '\n', '\n'
  };

  prob = 0;
  caps = typefind_data (webvtt, sizeof (webvtt), &prob);
  fail_unless (caps != NULL);
  media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (media_type, "application/x-subtitle-vtt");
  fail_unless_equals_int (prob, GST_TYPE_FIND_MAXIMUM);
  gst_caps_unref (caps);

  prob = 0;
  caps = typefind_data (webvtt_with_bom, sizeof (webvtt_with_bom), &prob);
  fail_unless (caps != NULL);
  media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  fail_unless_equals_string (media_type, "application/x-subtitle-vtt");
  fail_unless_equals_int (prob, GST_TYPE_FIND_MAXIMUM);
  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
typefindfunctions_suite (void)
{
  Suite *s = suite_create ("typefindfunctions");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_quicktime_mpeg4video);
  tcase_add_test (tc_chain, test_broken_flac_in_ogg);
  tcase_add_test (tc_chain, test_jpeg_not_ac3);
  tcase_add_test (tc_chain, test_mpegts);
  tcase_add_test (tc_chain, test_ac3);
  tcase_add_test (tc_chain, test_eac3);
  tcase_add_test (tc_chain, test_random_data);
  tcase_add_test (tc_chain, test_hls_m3u8);
  tcase_add_test (tc_chain, test_manifest_typefinding);
  tcase_add_test (tc_chain, test_webvtt);

  return s;
}

GST_CHECK_MAIN (typefindfunctions);
