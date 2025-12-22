/* GStreamer
 *
 * unit test for hlsdemux
 *
 * Copyright (C) <2012> Fluendo S.A <support@fluendo.com>
 *  Authors: Andoni Morales Alastruey <amorales@fluendo.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gst/check/gstcheck.h>

#undef GST_CAT_DEFAULT
#include "m3u8.h"
#include "m3u8.c"

GST_DEBUG_CATEGORY (hls_debug);

static const gchar *INVALID_PLAYLIST = "#EXTM3 UINVALID";

static const gchar *ON_DEMAND_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/004.ts\n\
#EXT-X-ENDLIST";

static const gchar *DOUBLES_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10.321,Test\n\
http://media.example.com/001.ts\n\
#EXTINF:9.6789,Test\n\
http://media.example.com/002.ts\n\
#EXTINF:10.2344,Test\n\
http://media.example.com/003.ts\n\
#EXTINF:9.92,Test\n\
http://media.example.com/004.ts\n\
#EXT-X-ENDLIST";

static const gchar *LIVE_PLAYLIST = "#EXTM3U\n\
#EXT-X-TARGETDURATION:8\n\
#EXT-X-MEDIA-SEQUENCE:2680\n\
\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2680.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2681.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2682.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2683.ts";

static const gchar *LIVE_ROTATED_PLAYLIST = "#EXTM3U\n\
#EXT-X-TARGETDURATION:8\n\
#EXT-X-MEDIA-SEQUENCE:3001\n\
\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence3001.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence3002.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence3003.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence3004.ts";

static const gchar *VARIANT_PLAYLIST = "#EXTM3U \n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\n\
http://example.com/low.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\n\
http://example.com/mid.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\n\
http://example.com/hi.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n\
http://example.com/audio-only.m3u8";

static const gchar *VARIANT_PLAYLIST_WITH_URI_MISSING = "#EXTM3U \n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\n\
http://example.com/low.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\n\
\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\n\
http://example.com/hi.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n\
http://example.com/audio-only.m3u8";

static const gchar *EMPTY_LINES_VARIANT_PLAYLIST = "#EXTM3U \n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\n\n\
http://example.com/low.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\n\n\
http://example.com/mid.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\n\n\
http://example.com/hi.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n\n\
http://example.com/audio-only.m3u8";

static const gchar *WINDOWS_EMPTY_LINES_VARIANT_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\r\n\r\n\
http://example.com/low.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\r\n\r\n\
http://example.com/mid.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\r\n\r\n\
http://example.com/hi.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\r\n\r\n\
http://example.com/audio-only.m3u8";

static const gchar *EMPTY_LINES_PLAYLIST = "#EXTM3U \n\n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Testr\n\n\
http://media.example.com/001.ts\n\n\
#EXTINF:10,Test\n\n\
http://media.example.com/002.ts\n\n\
#EXTINF:10,Test\n\n\
http://media.example.com/003.ts\n\n\
#EXTINF:10,Test\n\n\
http://media.example.com/004.ts\n\n\
#EXT-X-ENDLIST";

static const gchar *WINDOWS_EMPTY_LINES_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-TARGETDURATION:10\r\n\r\n\
#EXTINF:10,Test\r\n\r\n\
http://media.example.com/001.ts\r\n\r\n\
#EXTINF:10,Test\r\n\r\n\
http://media.example.com/002.ts\r\n\r\n\
#EXTINF:10,Test\r\n\r\n\
http://media.example.com/003.ts\r\n\r\n\
#EXTINF:10,Test\r\n\r\n\
http://media.example.com/004.ts\r\n\r\n\
#EXT-X-ENDLIST";

static const gchar *BYTE_RANGES_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:40\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@100\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@2000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@3000\n\
http://media.example.com/all.ts\n\
#EXT-X-ENDLIST";

static const gchar *BYTE_RANGES_ACC_OFFSET_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:40\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXT-X-ENDLIST";

#if 0
static const gchar *ALTERNATE_AUDIO_PLAYLIST = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",\
  DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\" \n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsche\",\
  DEFAULT=NO,AUTOSELECT=YES,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/german-audio.m3u8\"\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Commentary\",\
  DEFAULT=NO,AUTOSELECT=NO,\
  URI=\"http://localhost/commentary/audio-only.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
low/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
mid/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
hi/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\",AUDIO=\"aac\"\n\
main/english-audio.m3u8";

static const gchar *ALT_AUDIO_PLAYLIST_WITH_VIDEO_AUDIO = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",\
  DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\" \n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsche\",\
  DEFAULT=NO,AUTOSELECT=YES,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/german-audio.m3u8\"\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Commentary\",\
  DEFAULT=NO,AUTOSELECT=NO,\
  URI=\"http://localhost/commentary/audio-only.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
low/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
mid/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
hi/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\",AUDIO=\"aac\"\n\
main/english-audio.m3u8";

static const gchar *ON_DEMAND_LOW_VIDEO_ONLY_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAND_MID_VIDEO_ONLY_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAND_ENGLISH_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAND_GERMAN_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *SUBTITLES_PLAYLIST = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"English\",\
  DEFAULT=YES,LANGUAGE=\"en\",\
  URI=\"http://localhost/main/subs-en.m3u8\"\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Deutsche\",\
  DEFAULT=NO,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/subs-de.m3u8\"\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Spanish\",\
  DEFAULT=NO,LANGUAGE=\"es\",\
  URI=\"http://localhost/main/subs-es.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
low/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
mid/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
hi/video-audio.m3u8";
#endif

static const gchar *AES_128_ENCRYPTED_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-001.ts\n\
#EXT-X-KEY:METHOD=NONE\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-002.ts\n\
#EXT-X-KEY:METHOD=AES-128,URI=\"https://priv.example.com/key.bin\"\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-003.ts\n\
#EXT-X-KEY:METHOD=AES-128,URI=\"https://priv.example.com/key2.bin\",IV=0x00000000000000000000000000000001\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-004.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-005.ts\n\
#EXT-X-ENDLIST";

static const gchar *WINDOWS_LINE_ENDINGS_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-TARGETDURATION:10\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/001.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/002.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/003.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/004.ts\r\n\
#EXT-X-ENDLIST";

static const gchar *WINDOWS_LINE_ENDINGS_VARIANT_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\r\n\
http://example.com/low.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\r\n\
http://example.com/mid.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\r\n\
http://example.com/hi.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\r\n\
http://example.com/audio-only.m3u8";

static const gchar *MAP_TAG_PLAYLIST = "#EXTM3U \n\
#EXT-X-VERSION:7\n\
#EXT-X-MAP:URI=\"init1.mp4\",BYTERANGE=\"50@50\"\n\
#EXTINF:6.00000,\n\
#EXT-X-BYTERANGE:100@50\n\
main.mp4\n\
#EXTINF:6.00000,\n\
#EXT-X-BYTERANGE:100@150\n\
main.mp4\n\
#EXT-X-MAP:URI=\"init2.mp4\"\n\
#EXTINF:6.00000,\n\
#EXT-X-BYTERANGE:100@300\n\
main.mp4\n\
#EXT-X-ENDLIST";

static GstHLSMasterPlaylist *
load_playlist (const gchar * data)
{
  GstHLSMasterPlaylist *master;

  master = gst_hls_master_playlist_new_from_data (g_strdup (data),
      "http://localhost/test.m3u8");
  fail_unless (master != NULL);

  return master;
}

GST_START_TEST (test_load_main_playlist_invalid)
{
  GstHLSMasterPlaylist *master;

  master =
      gst_hls_master_playlist_new_from_data (g_strdup (INVALID_PLAYLIST), NULL);
  fail_unless (master == NULL);
}

GST_END_TEST;

GST_START_TEST (test_load_main_playlist_rendition)
{
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *variant;

  master = load_playlist (ON_DEMAND_PLAYLIST);
  variant = master->default_variant;

  assert_equals_int (g_list_length (variant->m3u8->files), 4);
  assert_equals_int (master->version, 0);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

static void
do_test_load_main_playlist_variant (const gchar * playlist)
{
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *stream;
  GList *tmp;

  master = gst_hls_master_playlist_new_from_data (g_strdup (playlist), NULL);
  fail_unless (master != NULL);

  assert_equals_int (g_list_length (master->variants), 4);

  /* Audio-Only */
  tmp = g_list_first (master->variants);
  stream = tmp->data;
  assert_equals_int (stream->bandwidth, 65000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (stream->uri, "http://example.com/audio-only.m3u8");
  assert_equals_string (stream->codecs, "mp4a.40.5");

  /* Low */
  tmp = g_list_next (tmp);
  stream = tmp->data;
  assert_equals_int (stream->bandwidth, 128000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (stream->uri, "http://example.com/low.m3u8");

  /* Mid */
  tmp = g_list_next (tmp);
  stream = tmp->data;
  assert_equals_int (stream->bandwidth, 256000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (stream->uri, "http://example.com/mid.m3u8");

  /* High */
  tmp = g_list_next (tmp);
  stream = tmp->data;
  assert_equals_int (stream->bandwidth, 768000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (stream->uri, "http://example.com/hi.m3u8");

  /* Check the first playlist is selected */
  assert_equals_int (master->default_variant != NULL, TRUE);
  assert_equals_int (master->default_variant->bandwidth, 128000);

  gst_hls_master_playlist_unref (master);
}

GST_START_TEST (test_load_main_playlist_variant)
{
  do_test_load_main_playlist_variant (VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_main_playlist_variant_with_missing_uri)
{
  GstHLSMasterPlaylist *master;

  master = load_playlist (VARIANT_PLAYLIST_WITH_URI_MISSING);
  assert_equals_int (g_list_length (master->variants), 3);
  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_load_windows_line_endings_variant_playlist)
{
  do_test_load_main_playlist_variant (WINDOWS_LINE_ENDINGS_VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_main_playlist_with_empty_lines)
{
  do_test_load_main_playlist_variant (EMPTY_LINES_VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_windows_main_playlist_with_empty_lines)
{
  do_test_load_main_playlist_variant (WINDOWS_EMPTY_LINES_VARIANT_PLAYLIST);
}

GST_END_TEST;

static void
check_on_demand_playlist (const gchar * data)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;

  master = load_playlist (data);
  pl = master->default_variant->m3u8;

  /* Sequence should be 0 as it's an ondemand playlist */
  assert_equals_int (pl->sequence, 0);
  /* Check that we are not live */
  assert_equals_int (gst_m3u8_is_live (pl), FALSE);
  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/001.ts");
  assert_equals_int (file->sequence, 0);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/004.ts");
  assert_equals_int (file->sequence, 3);

  gst_hls_master_playlist_unref (master);
}

GST_START_TEST (test_on_demand_playlist)
{
  check_on_demand_playlist (ON_DEMAND_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_windows_line_endings_playlist)
{
  check_on_demand_playlist (WINDOWS_LINE_ENDINGS_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_empty_lines_playlist)
{
  check_on_demand_playlist (EMPTY_LINES_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_windows_empty_lines_playlist)
{
  check_on_demand_playlist (WINDOWS_EMPTY_LINES_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_live_playlist)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;
  gint64 start = -1;
  gint64 stop = -1;

  master = load_playlist (LIVE_PLAYLIST);

  pl = master->default_variant->m3u8;
  /* Check that we are live */
  assert_equals_int (gst_m3u8_is_live (pl), TRUE);
  assert_equals_int (pl->sequence, 2680);
  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri,
      "https://priv.example.com/fileSequence2680.ts");
  assert_equals_int (file->sequence, 2680);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri,
      "https://priv.example.com/fileSequence2683.ts");
  assert_equals_int (file->sequence, 2683);
  fail_unless (gst_m3u8_get_seek_range (pl, &start, &stop));
  assert_equals_int64 (start, 0);
  assert_equals_float (stop / (double) GST_SECOND, 8.0);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

/* This test is for live sreams in which we pause the stream for more than the
 * DVR window and we resume playback. The playlist has rotated completely and
 * there is a jump in the media sequence that must be handled correctly. */
GST_START_TEST (test_live_playlist_rotated)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;
  gboolean ret;

  master = load_playlist (LIVE_PLAYLIST);
  pl = master->default_variant->m3u8;

  assert_equals_int (pl->sequence, 2680);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_int (file->sequence, 2680);

  ret = gst_m3u8_update (pl, g_strdup (LIVE_ROTATED_PLAYLIST));
  assert_equals_int (ret, TRUE);
  file = gst_m3u8_get_next_fragment (pl, TRUE, NULL, NULL, NULL);
  fail_unless (file != NULL);
  gst_m3u8_media_file_unref (file);

  /* FIXME: Sequence should last - 3. Should it? */
  assert_equals_int (pl->sequence, 3001);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_int (file->sequence, 3001);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_playlist_with_doubles_duration)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;
  gint64 start = -1;
  gint64 stop = -1;

  master = load_playlist (DOUBLES_PLAYLIST);
  pl = master->default_variant->m3u8;

  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 0));
  assert_equals_float (file->duration / (double) GST_SECOND, 10.321);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 1));
  assert_equals_float (file->duration / (double) GST_SECOND, 9.6789);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 2));
  assert_equals_float (file->duration / (double) GST_SECOND, 10.2344);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 3));
  assert_equals_float (file->duration / (double) GST_SECOND, 9.92);
  fail_unless (gst_m3u8_get_seek_range (pl, &start, &stop));
  assert_equals_int64 (start, 0);
  assert_equals_float (stop / (double) GST_SECOND,
      10.321 + 9.6789 + 10.2344 + 9.92);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_playlist_with_encryption)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;
  guint8 iv1[16] = { 0, };
  guint8 iv2[16] = { 0, };

  iv1[15] = 1;
  iv2[15] = 2;

  master = load_playlist (AES_128_ENCRYPTED_PLAYLIST);
  pl = master->default_variant->m3u8;

  assert_equals_int (g_list_length (pl->files), 5);

  /* Check all media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 0));
  fail_unless (file->key == NULL);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 1));
  fail_unless (file->key == NULL);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 2));
  fail_unless (file->key != NULL);
  assert_equals_string (file->key, "https://priv.example.com/key.bin");
  fail_unless (memcmp (&file->iv, iv2, 16) == 0);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 3));
  fail_unless (file->key != NULL);
  assert_equals_string (file->key, "https://priv.example.com/key2.bin");
  fail_unless (memcmp (&file->iv, iv1, 16) == 0);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 4));
  fail_unless (file->key != NULL);
  assert_equals_string (file->key, "https://priv.example.com/key2.bin");
  fail_unless (memcmp (&file->iv, iv1, 16) == 0);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;


GST_START_TEST (test_update_invalid_playlist)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  gboolean ret;

  /* Test updates in on-demand playlists */
  master = load_playlist (ON_DEMAND_PLAYLIST);
  pl = master->default_variant->m3u8;
  assert_equals_int (g_list_length (pl->files), 4);
  ret = gst_m3u8_update (pl, g_strdup ("#INVALID"));
  assert_equals_int (ret, FALSE);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_update_playlist)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  gchar *live_pl;
  gboolean ret;

  /* Test updates in on-demand playlists */
  master = load_playlist (ON_DEMAND_PLAYLIST);
  pl = master->default_variant->m3u8;
  assert_equals_int (g_list_length (pl->files), 4);
  ret = gst_m3u8_update (pl, g_strdup (ON_DEMAND_PLAYLIST));
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 4);
  gst_hls_master_playlist_unref (master);

  /* Test updates in live playlists */
  master = load_playlist (LIVE_PLAYLIST);
  pl = master->default_variant->m3u8;
  assert_equals_int (g_list_length (pl->files), 4);
  /* Add a new entry to the playlist and check the update */
  live_pl = g_strdup_printf ("%s\n%s\n%s", LIVE_PLAYLIST, "#EXTINF:8",
      "https://priv.example.com/fileSequence2683.ts");
  ret = gst_m3u8_update (pl, live_pl);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 5);
  /* Test sliding window */
  ret = gst_m3u8_update (pl, g_strdup (LIVE_PLAYLIST));
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 4);
  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_playlist_media_files)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;

  master = load_playlist (ON_DEMAND_PLAYLIST);
  pl = master->default_variant->m3u8;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/001.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 0);
  assert_equals_int (file->size, -1);
  assert_equals_string (file->title, "Test");

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_playlist_byte_range_media_files)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *file;

  master = load_playlist (BYTE_RANGES_PLAYLIST);
  pl = master->default_variant->m3u8;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 100);
  assert_equals_int (file->size, 1000);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 3);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 3000);
  assert_equals_int (file->size, 1000);

  gst_hls_master_playlist_unref (master);


  master = load_playlist (BYTE_RANGES_ACC_OFFSET_PLAYLIST);
  pl = master->default_variant->m3u8;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 0);
  assert_equals_int (file->size, 1000);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 3);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 3000);
  assert_equals_int (file->size, 1000);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_get_next_fragment)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;
  GstM3U8MediaFile *mf;
  gboolean discontinuous;
  GstClockTime timestamp;

  master = load_playlist (BYTE_RANGES_PLAYLIST);
  pl = master->default_variant->m3u8;

  /* Check the next fragment */
  mf = gst_m3u8_get_next_fragment (pl, TRUE, &timestamp, NULL, &discontinuous);
  fail_unless (mf != NULL);
  assert_equals_int (discontinuous, FALSE);
  assert_equals_string (mf->uri, "http://media.example.com/all.ts");
  assert_equals_uint64 (timestamp, 0);
  assert_equals_uint64 (mf->duration, 10 * GST_SECOND);
  assert_equals_uint64 (mf->offset, 100);
  assert_equals_uint64 (mf->offset + mf->size, 1100);
  gst_m3u8_media_file_unref (mf);

  gst_m3u8_advance_fragment (pl, TRUE);

  /* Check next media segments */
  mf = gst_m3u8_get_next_fragment (pl, TRUE, &timestamp, NULL, &discontinuous);
  fail_unless (mf != NULL);
  assert_equals_int (discontinuous, FALSE);
  assert_equals_string (mf->uri, "http://media.example.com/all.ts");
  assert_equals_uint64 (timestamp, 10 * GST_SECOND);
  assert_equals_uint64 (mf->duration, 10 * GST_SECOND);
  assert_equals_uint64 (mf->offset, 1000);
  assert_equals_uint64 (mf->offset + mf->size, 2000);
  gst_m3u8_media_file_unref (mf);

  gst_m3u8_advance_fragment (pl, TRUE);

  /* Check next media segments */
  mf = gst_m3u8_get_next_fragment (pl, TRUE, &timestamp, NULL, &discontinuous);
  assert_equals_int (discontinuous, FALSE);
  assert_equals_string (mf->uri, "http://media.example.com/all.ts");
  assert_equals_uint64 (timestamp, 20 * GST_SECOND);
  assert_equals_uint64 (mf->duration, 10 * GST_SECOND);
  assert_equals_uint64 (mf->offset, 2000);
  assert_equals_uint64 (mf->offset + mf->size, 3000);
  gst_m3u8_media_file_unref (mf);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_get_duration)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;

  /* Test duration for on-demand playlists */
  master = load_playlist (ON_DEMAND_PLAYLIST);
  pl = master->default_variant->m3u8;

  assert_equals_uint64 (gst_m3u8_get_duration (pl), 40 * GST_SECOND);
  gst_hls_master_playlist_unref (master);

  /* Test duration for live playlists */
  master = load_playlist (LIVE_PLAYLIST);
  pl = master->default_variant->m3u8;
  assert_equals_uint64 (gst_m3u8_get_duration (pl), GST_CLOCK_TIME_NONE);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_get_target_duration)
{
  GstHLSMasterPlaylist *master;
  GstM3U8 *pl;

  master = load_playlist (ON_DEMAND_PLAYLIST);
  pl = master->default_variant->m3u8;

  assert_equals_uint64 (gst_m3u8_get_target_duration (pl), 10 * GST_SECOND);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;


GST_START_TEST (test_get_stream_for_bitrate)
{
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *stream;

  master = load_playlist (VARIANT_PLAYLIST);
  stream = gst_hls_master_playlist_get_variant_for_bitrate (master, NULL, 0);

  assert_equals_int (stream->bandwidth, 65000);

  stream =
      gst_hls_master_playlist_get_variant_for_bitrate (master, NULL,
      G_MAXINT32);
  assert_equals_int (stream->bandwidth, 768000);
  stream =
      gst_hls_master_playlist_get_variant_for_bitrate (master, NULL, 300000);
  assert_equals_int (stream->bandwidth, 256000);
  stream =
      gst_hls_master_playlist_get_variant_for_bitrate (master, NULL, 500000);
  assert_equals_int (stream->bandwidth, 256000);
  stream =
      gst_hls_master_playlist_get_variant_for_bitrate (master, NULL, 255000);
  assert_equals_int (stream->bandwidth, 128000);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

#if 0
static void
do_test_seek (GstM3U8Client * client, guint seek_pos, gint pos)
{
  GstClockTime cur_pos;
  gboolean ret;

  ret = gst_m3u8_client_seek (client, seek_pos * GST_SECOND);
  if (pos == -1) {
    assert_equals_int (ret, FALSE);
    return;
  }
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_position (client, &cur_pos, NULL);
  assert_equals_uint64 (cur_pos, pos * GST_SECOND);
}

GST_START_TEST (test_seek)
{
  GstM3U8Client *client;

  master = load_playlist (ON_DEMAND_PLAYLIST);

  /* Test seek in the middle of a fragment */
  do_test_seek (client, 1, 0);
  do_test_seek (client, 11, 10);
  do_test_seek (client, 22, 20);
  do_test_seek (client, 39, 30);

  /* Test exact seeks */
  do_test_seek (client, 0, 0);
  do_test_seek (client, 10, 10);
  do_test_seek (client, 20, 20);
  do_test_seek (client, 30, 30);

  /* Test invalid seeks (end if list should be 30 + 10) */
  do_test_seek (client, 39, 30);
  do_test_seek (client, 40, -1);
  gst_hls_master_playlist_unref (master);

  /* Test seeks on a live playlist */
  master = load_playlist (LIVE_PLAYLIST);
  do_test_seek (client, 0, 0);

  do_test_seek (client, 8, 8);
  do_test_seek (client, 20, 16);
  do_test_seek (client, 30, 24);

  do_test_seek (client, 3000, -1);
  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_alternate_audio_playlist)
{
  GstM3U8Client *client;
  GstM3U8Media *media;
  GList *alternates;

  master = load_playlist (ALTERNATE_AUDIO_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 4);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      1);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 3);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->video_alternates), 0);

  alternates =
      g_hash_table_lookup (client->main->audio_rendition_groups, "aac");
  assert_equals_int (alternates != NULL, TRUE);
  media = GST_M3U8_MEDIA (g_list_nth_data (alternates, 0));
  assert_equals_int (media->media_type, GST_M3U8_MEDIA_TYPE_AUDIO);
  assert_equals_string (media->group_id, "aac");
  assert_equals_string (media->name, "English");
  assert_equals_string (media->language, "en");
  assert_equals_string (media->uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_string (media->uri, GST_M3U8 (media->playlist)->uri);
  assert_equals_int (media->is_default, TRUE);
  assert_equals_int (media->autoselect, TRUE);

  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 3);
  /* Check the list of audio alternates */
  alternates = gst_m3u8_client_get_alternates (client,
      GST_M3U8_MEDIA_TYPE_AUDIO);
  assert_equals_int (g_list_length (alternates), 3);
  /* Default comes always first */
  assert_equals_string (g_list_nth_data (alternates, 0), "English");
  assert_equals_string (g_list_nth_data (alternates, 1), "Commentary");
  assert_equals_string (g_list_nth_data (alternates, 2), "Deutsche");

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_subtitles_playlist)
{
  GstM3U8Client *client;
  GstM3U8Media *media;
  GList *alternates;

  master = load_playlist (SUBTITLES_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 3);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->subtt_rendition_groups),
      1);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 0);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->video_alternates), 0);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->subtt_alternates), 3);

  alternates =
      g_hash_table_lookup (client->main->subtt_rendition_groups, "subs");
  assert_equals_int (alternates != NULL, TRUE);
  media = GST_M3U8_MEDIA (g_list_nth_data (alternates, 0));
  assert_equals_int (media->media_type, GST_M3U8_MEDIA_TYPE_SUBTITLES);
  assert_equals_string (media->group_id, "subs");
  assert_equals_string (media->name, "English");
  assert_equals_string (media->language, "en");
  assert_equals_string (media->uri, "http://localhost/main/subs-en.m3u8");
  assert_equals_string (media->uri, GST_M3U8 (media->playlist)->uri);
  assert_equals_int (media->is_default, TRUE);
  assert_equals_int (media->autoselect, FALSE);

  /* Check the list of subtitles */
  alternates = gst_m3u8_client_get_alternates (client,
      GST_M3U8_MEDIA_TYPE_SUBTITLES);
  assert_equals_int (g_list_length (alternates), 3);
  assert_equals_string (g_list_nth_data (alternates, 0), "Deutsche");
  assert_equals_string (g_list_nth_data (alternates, 1), "Spanish");
  assert_equals_string (g_list_nth_data (alternates, 2), "English");

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;
GST_START_TEST (test_select_subs_alternate)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  gboolean ret;

  /* Check with a playlist with alternative audio renditions where the video
   * stream is video-only and therefor we always have 2 playlists, one for
   * video and another one for audio */
  master = load_playlist (SUBTITLES_PLAYLIST);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-de.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      "English");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-en.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      "Spanish");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-es.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;
GST_START_TEST (test_select_alternate)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  gboolean ret;

  /* Check with a playlist with alternative audio renditions where the video
   * stream is video-only and therefor we always have 2 playlists, one for
   * video and another one for audio */
  master = load_playlist (ALTERNATE_AUDIO_PLAYLIST);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Check that selecting the audio-only fallback stream we only have the audio
   * uri */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->data));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);

  gst_hls_master_playlist_unref (master);

  /* Now check with a playlist with alternative audio renditions where the
   * video * stream has the default audio rendition muxed and therefore we
   * only have 2 playlists when the audio alternative rendition is not the
   * default one */
  master = load_playlist (ALT_AUDIO_PLAYLIST_WITH_VIDEO_AUDIO);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Check that selecting the audio-only fallback stream we only have the audio
   * uri */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->data));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);

  /* Get back to the audio-video stream */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->next->data));
  /* Now set a different audio and check that we have 2 playlists */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_simulation)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  GstFragment *a_frag, *v_frag, *s_frag;
  gboolean ret;

  master = load_playlist (ALTERNATE_AUDIO_PLAYLIST);
  /* The default selection should be audio-only, which only has audio and not
   * video */
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Update the playlists */
  ret = gst_m3u8_update (client,
      g_strdup (ON_DEMAND_LOW_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAND_ENGLISH_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (client->selected_stream->selected_video->
          files), 4);
  assert_equals_int (g_list_length (client->selected_stream->selected_audio->
          files), 4);

  /* Get the first fragment */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-001.ts");
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-001.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Get the next fragment */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-002.ts");
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-002.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Switch to German audio */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  /* Get the new uri's */
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  /* On demand  so the uri does not need to be downloaded again */
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);
  /* Update the new uri's */
  ret =
      gst_m3u8_update (client,
      g_strdup (ON_DEMAND_LOW_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAND_GERMAN_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-003.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-003.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Switch to a higher bitrate */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 260000));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/mid/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);
  ret =
      gst_m3u8_update (client,
      g_strdup (ON_DEMAND_MID_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAND_GERMAN_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-004.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-004.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Seek to the beginning */
  gst_m3u8_client_seek (client, 0);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-001.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-001.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Select English audio again */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "English");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-002.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-002.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Go to the audio-only fallback */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 20000));
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag == NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-003.ts");
  g_object_unref (a_frag);

  /* Go to mid again */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 260000));
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-004.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-004.ts");
  g_object_unref (a_frag);
  g_object_unref (v_frag);

  /* End of stream */
  ret = gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (ret, FALSE);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;
#endif

GST_START_TEST (test_url_with_slash_query_param)
{
  static const gchar *MASTER_PLAYLIST = "#EXTM3U \n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f, mp4a.40.2\", RESOLUTION=640x352\n"
      "1251/media.m3u8?acl=/*1054559_h264_1500k.mp4\n";
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *stream;
  GstM3U8 *media;

  master = load_playlist (MASTER_PLAYLIST);

  assert_equals_int (g_list_length (master->variants), 1);
  stream = g_list_nth_data (master->variants, 0);
  media = stream->m3u8;

  assert_equals_string (media->uri,
      "http://localhost/1251/media.m3u8?acl=/*1054559_h264_1500k.mp4");
  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_stream_inf_tag)
{
  static const gchar *MASTER_PLAYLIST = "#EXTM3U \n"
      "#EXT-X-VERSION:4\n"
      "#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=1251135, CODECS=\"avc1.42001f, mp4a.40.2\", RESOLUTION=640x352\n"
      "media.m3u8\n";
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *stream;

  master = load_playlist (MASTER_PLAYLIST);

  assert_equals_int (g_list_length (master->variants), 1);
  stream = g_list_nth_data (master->variants, 0);

  assert_equals_int64 (stream->program_id, 1);
  assert_equals_int64 (stream->width, 640);
  assert_equals_int64 (stream->height, 352);
  assert_equals_int64 (stream->bandwidth, 1251135);
  assert_equals_string (stream->codecs, "avc1.42001f, mp4a.40.2");
  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

GST_START_TEST (test_map_tag)
{
  GstHLSMasterPlaylist *master;
  GstHLSVariantStream *stream;
  GstM3U8 *m3u8;
  GList *files, *walk;
  GstM3U8MediaFile *seg1, *seg2, *seg3;
  GstM3U8InitFile *init1, *init2;

  /* Test EXT-X-MAP tag
   * This M3U8 has two EXT-X-MAP tag.
   * the first one is applied to the 1st and 2nd segments, and the other is
   * applied only to the 3rd segment
   */

  master = load_playlist (MAP_TAG_PLAYLIST);

  assert_equals_int (master->is_simple, TRUE);
  assert_equals_int (g_list_length (master->variants), 1);
  stream = g_list_nth_data (master->variants, 0);

  m3u8 = stream->m3u8;
  fail_unless (m3u8 != NULL);

  files = m3u8->files;
  fail_unless (m3u8 != NULL);
  assert_equals_int (g_list_length (files), 3);
  for (walk = files; walk; walk = g_list_next (walk)) {
    GstM3U8MediaFile *file = (GstM3U8MediaFile *) walk->data;

    GstM3U8InitFile *init_file = file->init_file;
    fail_unless (init_file != NULL);
    fail_unless (init_file->uri != NULL);
  }

  seg1 = g_list_nth_data (files, 0);
  seg2 = g_list_nth_data (files, 1);
  seg3 = g_list_nth_data (files, 2);

  /* Segment 1 and 2 share the identical init segment */
  fail_unless (seg1->init_file == seg2->init_file);
  assert_equals_int (seg1->init_file->ref_count, 2);

  fail_unless (seg2->init_file != seg3->init_file);
  assert_equals_int (seg3->init_file->ref_count, 1);

  init1 = seg1->init_file;
  init2 = seg3->init_file;

  fail_unless (g_strcmp0 (init1->uri, init2->uri));
  assert_equals_int (init1->offset, 50);
  assert_equals_int (init1->size, 50);

  assert_equals_int (init2->offset, 0);
  assert_equals_int (init2->size, -1);

  gst_hls_master_playlist_unref (master);
}

GST_END_TEST;

static Suite *
hlsdemux_suite (void)
{
  Suite *s = suite_create ("hlsdemux_m3u8");
  TCase *tc_m3u8 = tcase_create ("m3u8client");

  GST_DEBUG_CATEGORY_INIT (hls_debug, "hlsdemux_m3u", 0, "hlsdemux m3u test");

  suite_add_tcase (s, tc_m3u8);
  tcase_add_test (tc_m3u8, test_load_main_playlist_invalid);
  tcase_add_test (tc_m3u8, test_load_main_playlist_rendition);
  tcase_add_test (tc_m3u8, test_load_main_playlist_variant);
  tcase_add_test (tc_m3u8, test_load_main_playlist_variant_with_missing_uri);
  tcase_add_test (tc_m3u8, test_load_windows_line_endings_variant_playlist);
  tcase_add_test (tc_m3u8, test_load_main_playlist_with_empty_lines);
  tcase_add_test (tc_m3u8, test_load_windows_main_playlist_with_empty_lines);
  tcase_add_test (tc_m3u8, test_on_demand_playlist);
  tcase_add_test (tc_m3u8, test_windows_line_endings_playlist);
  tcase_add_test (tc_m3u8, test_windows_empty_lines_playlist);
  tcase_add_test (tc_m3u8, test_empty_lines_playlist);
  tcase_add_test (tc_m3u8, test_live_playlist);
  tcase_add_test (tc_m3u8, test_live_playlist_rotated);
  tcase_add_test (tc_m3u8, test_playlist_with_doubles_duration);
  tcase_add_test (tc_m3u8, test_playlist_with_encryption);
  tcase_add_test (tc_m3u8, test_update_invalid_playlist);
  tcase_add_test (tc_m3u8, test_update_playlist);
  tcase_add_test (tc_m3u8, test_playlist_media_files);
  tcase_add_test (tc_m3u8, test_playlist_byte_range_media_files);
  tcase_add_test (tc_m3u8, test_get_next_fragment);
  tcase_add_test (tc_m3u8, test_get_duration);
  tcase_add_test (tc_m3u8, test_get_target_duration);
  tcase_add_test (tc_m3u8, test_get_stream_for_bitrate);
#if 0
  tcase_add_test (tc_m3u8, test_seek);
  tcase_add_test (tc_m3u8, test_alternate_audio_playlist);
  tcase_add_test (tc_m3u8, test_subtitles_playlist);
  tcase_add_test (tc_m3u8, test_select_alternate);
  tcase_add_test (tc_m3u8, test_select_subs_alternate);
  tcase_add_test (tc_m3u8, test_simulation);
#endif
  tcase_add_test (tc_m3u8, test_url_with_slash_query_param);
  tcase_add_test (tc_m3u8, test_stream_inf_tag);
  tcase_add_test (tc_m3u8, test_map_tag);
  return s;
}

GST_CHECK_MAIN (hlsdemux);
