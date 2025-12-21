/* GStreamer
 *
 * unit test for the audiocdsrc base class
 *
 * Copyright (C) <2005> Tim-Philipp Müller <tim centricular net>
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

/* TODO:
 *  - test different modes (when seeking to tracks in track mode, buffer
 *    timestamps should start from 0, when seeking to tracks in disc mode,
 *    buffer timestamps should increment, etc.)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/check/gstbufferstraw.h>

#include <gst/audio/gstaudiocdsrc.h>
#include <string.h>

#define CD_FRAMESIZE_RAW 2352

#define GST_TYPE_CD_FOO_SRC            (gst_cd_foo_src_get_type())
#define GST_CD_FOO_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CD_FOO_SRC,GstCdFooSrc))
#define GST_CD_FOO_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CD_FOO_SRC,GstCdFooSrcClass))
#define GST_IS_CD_FOO_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CD_FOO_SRC))
#define GST_IS_CD_FOO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CD_FOO_SRC))
#define GST_CD_FOO_SRC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CDDA_BASAE_SRC, GstCdFooSrcClass))

typedef struct _GstCdFooSrc GstCdFooSrc;
typedef struct _GstCdFooSrcClass GstCdFooSrcClass;


/* Neue Heimat (CD 2) */
static GstAudioCdSrcTrack nh_cd2_tracks[] = {
  {TRUE, 1, 0, 20664, NULL,},
  {TRUE, 2, 20665, 52377, NULL,},
  {TRUE, 3, 52378, 84100, NULL,},
  {TRUE, 4, 84101, 105401, NULL,},
  {TRUE, 5, 105402, 123060, NULL,},
  {TRUE, 6, 123061, 146497, NULL,},
  {TRUE, 7, 146498, 175693, NULL,},
  {TRUE, 8, 175694, 203272, NULL,},
  {TRUE, 9, 203273, 217909, NULL,},
  {TRUE, 10, 217910, 240938, NULL,},
  {TRUE, 11, 240939, 256169, NULL,},
  {TRUE, 12, 256170, 282237, NULL,},
  {TRUE, 13, 282238, 307606, NULL,},
  {TRUE, 14, 307607, 337245, NULL,}
};

/* Offspring - Smash */
static GstAudioCdSrcTrack offspring_tracks[] = {
  {TRUE, 1, 0, 1924, NULL,},
  {TRUE, 2, 1925, 12947, NULL,},
  {TRUE, 3, 12948, 29739, NULL,},
  {TRUE, 4, 29740, 47202, NULL,},
  {TRUE, 5, 47203, 63134, NULL,},
  {TRUE, 6, 63135, 77954, NULL,},
  {TRUE, 7, 77955, 92789, NULL,},
  {TRUE, 8, 92790, 112127, NULL,},
  {TRUE, 9, 112128, 124372, NULL,},
  {TRUE, 10, 124373, 133574, NULL,},
  {TRUE, 11, 133575, 143484, NULL,},
  {TRUE, 12, 143485, 149279, NULL,},
  {TRUE, 13, 149280, 162357, NULL,},
  {TRUE, 14, 162358, 210372, NULL,}
};

/* this matches the sample TOC from the DiscIDCalculation
 * page in the Musicbrainz wiki. It's a tricky one because
 * it's got a data track as well. */
static GstAudioCdSrcTrack mb_sample_tracks[] = {
  {TRUE, 1, 0, 18640, NULL,},
  {TRUE, 2, 18641, 34666, NULL,},
  {TRUE, 3, 34667, 56349, NULL,},
  {TRUE, 4, 56350, 77005, NULL,},
  {TRUE, 5, 77006, 106093, NULL,},
  {TRUE, 6, 106094, 125728, NULL,},
  {TRUE, 7, 125729, 149784, NULL,},
  {TRUE, 8, 149785, 168884, NULL,},
  {TRUE, 9, 168885, 185909, NULL,},
  {TRUE, 10, 185910, 205828, NULL,},
  {TRUE, 11, 205829, 230141, NULL,},
  {TRUE, 12, 230142, 246658, NULL,},
  {TRUE, 13, 246659, 265613, NULL,},
  {TRUE, 14, 265614, 289478, NULL,},
  {FALSE, 15, 289479, 325731, NULL,}
};

/* Nicola Conte - Other Directions (also
 * tricky due to the extra data track) */
static GstAudioCdSrcTrack nconte_odir_tracks[] = {
  {TRUE, 1, 0, 17852, NULL,},
  {TRUE, 2, 17853, 39956, NULL,},
  {TRUE, 3, 39957, 68449, NULL,},
  {TRUE, 4, 68450, 88725, NULL,},
  {TRUE, 5, 88726, 106413, NULL,},
  {TRUE, 6, 106414, 131966, NULL,},
  {TRUE, 7, 131967, 152372, NULL,},
  {TRUE, 8, 152373, 168602, NULL,},
  {TRUE, 9, 168603, 190348, NULL,},
  {TRUE, 10, 190349, 209044, NULL,},
  {TRUE, 11, 209045, 235586, NULL,},
  {TRUE, 12, 235587, 253830, NULL,},
  {TRUE, 13, 253831, 272213, NULL,},
  {FALSE, 14, 272214, 332849, NULL,}
};

/* Pink Martini - Sympathique (11 track version) */
static GstAudioCdSrcTrack pm_symp_tracks[] = {
  {TRUE, 1, 0, 21667, NULL,},
  {TRUE, 2, 21668, 49576, NULL,},
  {TRUE, 3, 49577, 62397, NULL,},
  {TRUE, 4, 62398, 81087, NULL,},
  {TRUE, 5, 81088, 106595, NULL,},
  {TRUE, 6, 106596, 122012, NULL,},
  {TRUE, 7, 122013, 138469, NULL,},
  {TRUE, 8, 138470, 157306, NULL,},
  {TRUE, 9, 157307, 179635, NULL,},
  {TRUE, 10, 179636, 203673, NULL,},
  {TRUE, 11, 203674, 213645, NULL,}
};

#define NUM_TEST_DISCS 5

struct _test_disc
{
  GstAudioCdSrcTrack *tracks;
  guint num_tracks;
  guint32 cddb_discid;
  const gchar *musicbrainz_discid;
};

/* FIXME: now we just need to find out how to treat
 * data tracks for the cddb id calculation .... */
static struct _test_disc test_discs[NUM_TEST_DISCS] = {
  {nh_cd2_tracks, G_N_ELEMENTS (nh_cd2_tracks), 0xae11900e,
      NULL},
  {mb_sample_tracks, G_N_ELEMENTS (mb_sample_tracks), 0x00000000,
      "MUtMmKN402WPj3_VFsgUelxpc8U-"},
  {offspring_tracks, G_N_ELEMENTS (offspring_tracks), 0xc20af40e,
      "ahg7JUcfR3vCYBphSDIogOOWrr0-"},
  {nconte_odir_tracks, G_N_ELEMENTS (nconte_odir_tracks), 0x00000000,
        /* hKx_PejjG47X161ND_Sh0HyqaS0- according to libmusicbrainz, but that's
         * wrong according to the wiki docs (or not?) (neither discid is listed) */
      "fboaOQtfqwENv8WyXa9tRyvyUbQ-"},
  {pm_symp_tracks, G_N_ELEMENTS (pm_symp_tracks), 0xa00b200b,
      "iP0DOLdr4vt_IfKSIXoRUR.q_Wc-"}
};

struct _GstCdFooSrc
{
  GstAudioCdSrc audiocdsrc;

  struct _test_disc *cur_test;
  guint cur_disc;
};

struct _GstCdFooSrcClass
{
  GstAudioCdSrcClass parent_class;
};

GType gst_cd_foo_src_get_type (void);
G_DEFINE_TYPE (GstCdFooSrc, gst_cd_foo_src, GST_TYPE_AUDIO_CD_SRC);

static GstBuffer *gst_cd_foo_src_read_sector (GstAudioCdSrc * src, gint sector);
static gboolean gst_cd_foo_src_open (GstAudioCdSrc * src, const gchar * device);
static void gst_cd_foo_src_close (GstAudioCdSrc * src);

static void
gst_cd_foo_src_init (GstCdFooSrc * src)
{
  src->cur_disc = 0;
}

static void
gst_cd_foo_src_class_init (GstCdFooSrcClass * klass)
{
  GstAudioCdSrcClass *audiocdsrc_class = GST_AUDIO_CD_SRC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_metadata (element_class,
      "CD Audio (cdda) Source, FooBar", "Source/File",
      "Read audio from CD", "Foo Bar <foo@bar.com>");

  audiocdsrc_class->open = gst_cd_foo_src_open;
  audiocdsrc_class->close = gst_cd_foo_src_close;
  audiocdsrc_class->read_sector = gst_cd_foo_src_read_sector;
}

static gboolean
gst_cd_foo_src_open (GstAudioCdSrc * audiocdsrc, const gchar * device)
{
  GstAudioCdSrcTrack *tracks;
  GstCdFooSrc *src;
  gint i;

  src = GST_CD_FOO_SRC (audiocdsrc);

  /* if this fails, the test is wrong */
  g_assert (src->cur_disc < NUM_TEST_DISCS);

  src->cur_test = &test_discs[src->cur_disc];

  /* add tracks */
  tracks = src->cur_test->tracks;
  for (i = 0; i < src->cur_test->num_tracks; ++i) {
    gst_audio_cd_src_add_track (GST_AUDIO_CD_SRC (src), &tracks[i]);
  }

  return TRUE;
}

static void
gst_cd_foo_src_close (GstAudioCdSrc * audiocdsrc)
{
  GstCdFooSrc *src = GST_CD_FOO_SRC (audiocdsrc);

  if (src->cur_test->cddb_discid != 0) {
    GST_FIXME ("Fix DISCID comparison: extract discid");
#if 0
    g_assert (audiocdsrc->discid == src->cur_test->cddb_discid);
#endif
  }

  if (src->cur_test->musicbrainz_discid != NULL) {
    GST_FIXME ("Fix MB DISCID comparison: extract musicbrainz discid");
#if 0
    g_assert (g_str_equal (audiocdsrc->mb_discid,
            src->cur_test->musicbrainz_discid));
#endif
  }
}

static GstBuffer *
gst_cd_foo_src_read_sector (GstAudioCdSrc * audiocdsrc, gint sector)
{
  GstBuffer *buf;

  buf = gst_buffer_new_and_alloc (CD_FRAMESIZE_RAW);
  gst_buffer_memset (buf, 0, 0, CD_FRAMESIZE_RAW);

  return buf;
}

static inline gboolean
tag_list_has_tag (GstTagList * list, const gchar * tag, GType type)
{
  const GValue *val = gst_tag_list_get_value_index (list, tag, 0);

  if (val == NULL) {
    GST_LOG ("no tag '%s' in taglist %" GST_PTR_FORMAT, tag, list);
    return FALSE;
  }

  if (!G_VALUE_HOLDS (val, type)) {
    GST_LOG ("tag '%s' in taglist %" GST_PTR_FORMAT " is not of type %s",
        tag, list, g_type_name (type));
    return FALSE;
  }

  return TRUE;
}

static void
test_uri_parse (const gchar * uri, const gchar * device, gint track)
{
  GstElement *foosrc;
  gchar *set_device = NULL;
  gint set_track = 0;

  foosrc = gst_element_factory_make ("cdfoosrc", "cdfoosrc");
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc), uri, NULL),
      "couldn't set uri %s", uri);
  g_object_get (foosrc, "device", &set_device, "track", &set_track, NULL);
  fail_unless (set_device != NULL);
  fail_unless (strcmp (set_device, device) == 0,
      "device set was %s, expected %s", set_device, device);
  fail_unless (set_track == track, "track set was %d, expected %d", set_track,
      track);
  g_free (set_device);
  gst_object_unref (foosrc);
}

GST_START_TEST (test_discid_calculations)
{
  GstElement *foosrc, *pipeline, *sink;
  gint i;

  fail_unless (gst_element_register (NULL, "cdfoosrc", GST_RANK_SECONDARY,
          GST_TYPE_CD_FOO_SRC));

  pipeline = gst_pipeline_new ("pipeline");

  sink = gst_element_factory_make ("fakesink", "sink");
  fail_unless (sink != NULL, "couldn't create fakesink");

  foosrc = gst_element_factory_make ("cdfoosrc", "cdfoosrc");
  fail_unless (foosrc != NULL, "couldn't create cdfoosrc");

  gst_bin_add (GST_BIN (pipeline), foosrc);
  gst_bin_add (GST_BIN (pipeline), sink);
  fail_unless (gst_element_link (foosrc, sink));

  for (i = 0; i < G_N_ELEMENTS (test_discs); ++i) {
    GstTagList *tags = NULL;
    GstMessage *msg;

    GST_LOG ("Testing disc layout %u ...", i);
    GST_CD_FOO_SRC (foosrc)->cur_disc = i;
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    msg =
        gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipeline),
        GST_CLOCK_TIME_NONE, GST_MESSAGE_TAG);
    gst_message_parse_tag (msg, &tags);
    fail_unless (tags != NULL);
    fail_unless (tag_list_has_tag (tags, "track-count", G_TYPE_UINT));
    fail_unless (tag_list_has_tag (tags, "track-number", G_TYPE_UINT));
    fail_unless (tag_list_has_tag (tags, "duration", G_TYPE_UINT64));
    fail_unless (tag_list_has_tag (tags, "discid", G_TYPE_STRING));
    fail_unless (tag_list_has_tag (tags, "discid-full", G_TYPE_STRING));
    fail_unless (tag_list_has_tag (tags, "musicbrainz-discid", G_TYPE_STRING));
    fail_unless (tag_list_has_tag (tags, "musicbrainz-discid-full",
            G_TYPE_STRING));
    gst_tag_list_unref (tags);
    gst_message_unref (msg);

    msg =
        gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipeline),
        GST_CLOCK_TIME_NONE, GST_MESSAGE_ASYNC_DONE);
    gst_message_unref (msg);

    gst_element_set_state (pipeline, GST_STATE_NULL);
  }

  gst_object_unref (pipeline);

  gst_task_cleanup_all ();
}

GST_END_TEST;

GST_START_TEST (test_buffer_timestamps)
{
  GstElement *foosrc, *pipeline, *fakesink;
  GstClockTime prev_ts, prev_duration, ts;
  GstPad *sinkpad;
  gint i;

  fail_unless (gst_element_register (NULL, "cdfoosrc", GST_RANK_SECONDARY,
          GST_TYPE_CD_FOO_SRC));

  pipeline = gst_pipeline_new ("pipeline");
  foosrc = gst_element_factory_make ("cdfoosrc", "cdfoosrc");
  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  gst_bin_add_many (GST_BIN (pipeline), foosrc, fakesink, NULL);
  fail_unless (gst_element_link (foosrc, fakesink));
  sinkpad = gst_element_get_static_pad (fakesink, "sink");

  GST_CD_FOO_SRC (foosrc)->cur_disc = 0;

  gst_buffer_straw_start_pipeline (pipeline, sinkpad);

  prev_ts = GST_CLOCK_TIME_NONE;
  prev_duration = GST_CLOCK_TIME_NONE;

  for (i = 0; i < 100; ++i) {
    GstBuffer *buf;

    buf = gst_buffer_straw_get_buffer (pipeline, sinkpad);
    GST_LOG ("buffer, ts=%" GST_TIME_FORMAT ", dur=%" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));
    ts = GST_BUFFER_TIMESTAMP (buf);
    fail_unless (GST_CLOCK_TIME_IS_VALID (ts));
    fail_unless (GST_BUFFER_DURATION_IS_VALID (buf));
    if (i > 0) {
      fail_unless (GST_CLOCK_TIME_IS_VALID (prev_ts));
      fail_unless (GST_CLOCK_TIME_IS_VALID (prev_duration));
      fail_unless ((prev_ts + prev_duration) == ts);
    }
    prev_ts = ts;
    prev_duration = GST_BUFFER_DURATION (buf);
    gst_buffer_unref (buf);
  }

  gst_buffer_straw_stop_pipeline (pipeline, sinkpad);

  gst_task_cleanup_all ();
  gst_object_unref (pipeline);
  gst_object_unref (sinkpad);
}

GST_END_TEST;

GST_START_TEST (test_uri_parsing)
{
  GstElement *foosrc;

  fail_unless (gst_element_register (NULL, "cdfoosrc", GST_RANK_SECONDARY,
          GST_TYPE_CD_FOO_SRC));

  /* wrong protocol */
  foosrc = gst_element_factory_make ("cdfoosrc", "cdfoosrc");
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "xyz://", NULL) == FALSE);
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "cddaq://", NULL) == FALSE);

  /* cdda://track */
  test_uri_parse ("cdda://", "/dev/cdrom", 1);
  test_uri_parse ("cdda://2", "/dev/cdrom", 2);
  test_uri_parse ("cdda://47", "/dev/cdrom", 47);
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "cdda://-1", NULL) == FALSE);
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "cdda://what", NULL) == FALSE);

  /* cdda://device#track */
  test_uri_parse ("cdda:///dev/hdb#1", "/dev/hdb", 1);
  test_uri_parse ("cdda://anything#8", "anything", 8);
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "cdda:///dev/hdb#nonsense", NULL) == FALSE);
  fail_unless (gst_uri_handler_set_uri (GST_URI_HANDLER (foosrc),
          "cdda:///dev/hdb#-2", NULL) == FALSE);

  /* cdda://track#device (device should be ignored - FIXME 0.11) */
  test_uri_parse ("cdda://8#/dev/hdb", "/dev/cdrom", 8);

  gst_object_unref (foosrc);
}

GST_END_TEST;

GST_START_TEST (test_properties)
{
  GstElement *foosrc;
  gchar *device;
  guint track;

  fail_unless (gst_element_register (NULL, "cdfoosrc", GST_RANK_SECONDARY,
          GST_TYPE_CD_FOO_SRC));

  foosrc = gst_element_factory_make ("cdfoosrc", "cdfoosrc");

  g_object_set (foosrc, "device", "/dev/cdrom", NULL);
  g_object_get (foosrc, "device", &device, "track", &track, NULL);
  fail_unless (g_str_equal (device, "/dev/cdrom"));
  fail_unless_equals_int (track, 1);
  g_free (device);

  g_object_set (foosrc, "device", "/dev/cdrom1", "track", 17, NULL);
  g_object_get (foosrc, "device", &device, "track", &track, NULL);
  fail_unless (g_str_equal (device, "/dev/cdrom1"));
  fail_unless_equals_int (track, 17);
  g_free (device);

  g_object_set (foosrc, "track", 17, "device", "/dev/cdrom1", NULL);
  g_object_get (foosrc, "device", &device, "track", &track, NULL);
  fail_unless (g_str_equal (device, "/dev/cdrom1"));
  fail_unless_equals_int (track, 17);
  g_free (device);

  g_object_set (foosrc, "track", 12, NULL);
  g_object_get (foosrc, "device", &device, "track", &track, NULL);
  fail_unless (g_str_equal (device, "/dev/cdrom1"));
  fail_unless_equals_int (track, 12);
  g_free (device);

  gst_object_unref (foosrc);
}

GST_END_TEST;

static Suite *
audiocdsrc_suite (void)
{
  Suite *s = suite_create ("audiocdsrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_discid_calculations);
  tcase_add_test (tc_chain, test_buffer_timestamps);
  tcase_add_test (tc_chain, test_uri_parsing);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (audiocdsrc)
