/* GStreamer
 *
 * unit tests for the tag support library
 *
 * Copyright (C) 2006-2011 Tim-Philipp Müller <tim centricular net>
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

#include <gst/check/gstcheck.h>

#include <gst/tag/tag.h>
#include <gst/base/gstbytewriter.h>
#include <string.h>
#include <locale.h>

GST_START_TEST (test_parse_extended_comment)
{
  gchar *key, *val, *lang;

  /* first check the g_return_val_if_fail conditions */
  ASSERT_CRITICAL (gst_tag_parse_extended_comment (NULL, NULL, NULL, NULL,
          FALSE));
  ASSERT_CRITICAL (gst_tag_parse_extended_comment ("\377\000", NULL, NULL, NULL,
          FALSE));

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("a=b", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key != NULL);
  fail_unless (lang == NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (key, "a");
  fail_unless_equals_string (val, "b");
  g_free (key);
  g_free (lang);
  g_free (val);

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("a[l]=b", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key != NULL);
  fail_unless (lang != NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (key, "a");
  fail_unless_equals_string (lang, "l");
  fail_unless_equals_string (val, "b");
  g_free (key);
  g_free (lang);
  g_free (val);

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("foo=bar", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key != NULL);
  fail_unless (lang == NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (key, "foo");
  fail_unless_equals_string (val, "bar");
  g_free (key);
  g_free (lang);
  g_free (val);

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("foo[fr]=bar", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key != NULL);
  fail_unless (lang != NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (key, "foo");
  fail_unless_equals_string (lang, "fr");
  fail_unless_equals_string (val, "bar");
  g_free (key);
  g_free (lang);
  g_free (val);

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("foo=[fr]bar", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key != NULL);
  fail_unless (lang == NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (key, "foo");
  fail_unless_equals_string (val, "[fr]bar");
  g_free (key);
  g_free (lang);
  g_free (val);

  /* test NULL for output locations */
  fail_unless (gst_tag_parse_extended_comment ("foo[fr]=bar", NULL, NULL, NULL,
          FALSE) == TRUE);

  /* test strict mode (key must be specified) */
  fail_unless (gst_tag_parse_extended_comment ("foo[fr]=bar", NULL, NULL, NULL,
          TRUE) == TRUE);
  fail_unless (gst_tag_parse_extended_comment ("foo=bar", NULL, NULL, NULL,
          TRUE) == TRUE);
  fail_unless (gst_tag_parse_extended_comment ("foobar", NULL, NULL, NULL,
          TRUE) == FALSE);

  /* test non-strict mode (if there's no key, that's fine too) */
  fail_unless (gst_tag_parse_extended_comment ("foobar", NULL, NULL, NULL,
          FALSE) == TRUE);
  fail_unless (gst_tag_parse_extended_comment ("[fr]bar", NULL, NULL, NULL,
          FALSE) == TRUE);

  key = val = lang = NULL;
  fail_unless (gst_tag_parse_extended_comment ("[fr]bar", &key, &lang, &val,
          FALSE) == TRUE);
  fail_unless (key == NULL);
  fail_unless (lang == NULL);
  fail_unless (val != NULL);
  fail_unless_equals_string (val, "[fr]bar");
  g_free (key);
  g_free (lang);
  g_free (val);
}

GST_END_TEST;

#define ASSERT_TAG_LIST_HAS_STRING(list,field,string)                      \
  {                                                                        \
    gboolean got_match = FALSE;                                            \
    guint i, size;                                                         \
                                                                           \
    fail_unless (gst_tag_list_get_tag_size (list,field) > 0);              \
    size = gst_tag_list_get_tag_size (list,field);                         \
    for (i = 0; i < size; ++i) {                                           \
      gchar *___s = NULL;                                                  \
                                                                           \
      fail_unless (gst_tag_list_get_string_index (list, field, i, &___s)); \
      fail_unless (___s != NULL);                                          \
      if (g_str_equal (___s, string)) {                                    \
        got_match = TRUE;                                                  \
        g_free (___s);                                                     \
        break;                                                             \
      }                                                                    \
      g_free (___s);                                                       \
    }                                                                      \
    fail_unless (got_match);                                               \
  }

#define ASSERT_TAG_LIST_HAS_UINT(list,field,num)                           \
  {                                                                        \
    guint ___n;                                                            \
                                                                           \
    fail_unless (gst_tag_list_get_tag_size (list,field) > 0);              \
    fail_unless (gst_tag_list_get_tag_size (list,field) == 1);             \
    fail_unless (gst_tag_list_get_uint_index (list, field, 0, &___n));     \
    fail_unless_equals_int (___n, num);                                    \
  }

#define MATCH_DOUBLE(p1, p2) ((p1 < p2 + 1e-6) && (p2 < p1 + 1e-6))
#define ASSERT_TAG_LIST_HAS_DOUBLE(list,field,d)                           \
  {                                                                        \
    gdouble ___d;                                                          \
                                                                           \
    fail_unless (gst_tag_list_get_tag_size (list,field) > 0);              \
    fail_unless (gst_tag_list_get_tag_size (list,field) == 1);             \
    fail_unless (gst_tag_list_get_double_index (list, field, 0, &___d));   \
    fail_unless (MATCH_DOUBLE (d, ___d),                                   \
        "%f does not match expected %f", ___d, d);                         \
  }

GST_START_TEST (test_musicbrainz_tag_registration)
{
  GstTagList *list;

  gst_tag_register_musicbrainz_tags ();

  list = gst_tag_list_new_empty ();

  /* musicbrainz tags aren't registered yet */
  gst_vorbis_tag_add (list, "MUSICBRAINZ_TRACKID", "123456");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_ARTISTID", "234567");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_ALBUMID", "345678");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_ALBUMARTISTID", "4567890");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_RELEASETRACKID", "4567891");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_RELEASEGROUPID", "4567892");
  gst_vorbis_tag_add (list, "MUSICBRAINZ_TRMID", "5678901");
  /* MUSICBRAINZ_SORTNAME = GST_TAG_ARTIST_SORTNAME now */
  gst_vorbis_tag_add (list, "MUSICBRAINZ_SORTNAME", "Five, 678901");

  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_TRACKID, "123456");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_ARTISTID, "234567");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_ALBUMID, "345678");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_ALBUMARTISTID,
      "4567890");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_RELEASETRACKID,
      "4567891");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_RELEASEGROUPID,
      "4567892");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_MUSICBRAINZ_TRMID, "5678901");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ARTIST_SORTNAME, "Five, 678901");

  gst_tag_list_unref (list);
}

GST_END_TEST;

GST_START_TEST (test_vorbis_tags)
{
  GstTagList *list;

  list = gst_tag_list_new_empty ();

  /* NULL pointers aren't allowed */
  ASSERT_CRITICAL (gst_vorbis_tag_add (NULL, "key", "value"));
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, NULL, "value"));
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, "key", NULL));

  /* must be UTF-8 */
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, "key", "v\377lue"));
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, "k\377y", "value"));

  /* key can't have a '=' in it */
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, "k=y", "value"));
  ASSERT_CRITICAL (gst_vorbis_tag_add (list, "key=", "value"));

  /* should be allowed in values though */
  gst_vorbis_tag_add (list, "keeey", "va=ue");

  /* add some tags */
  gst_vorbis_tag_add (list, "TITLE", "Too");
  gst_vorbis_tag_add (list, "ALBUM", "Aoo");
  gst_vorbis_tag_add (list, "ARTIST", "Alboo");
  gst_vorbis_tag_add (list, "PERFORMER", "Perfoo");
  gst_vorbis_tag_add (list, "COPYRIGHT", "Copyfoo");
  gst_vorbis_tag_add (list, "DESCRIPTION", "Descoo");
  gst_vorbis_tag_add (list, "LICENSE", "Licoo");
  gst_vorbis_tag_add (list, "LICENSE",
      "http://creativecommons.org/licenses/by/3.0/");
  gst_vorbis_tag_add (list, "LOCATION", "Bristol, UK");
  gst_vorbis_tag_add (list, "ORGANIZATION", "Orgoo");
  gst_vorbis_tag_add (list, "GENRE", "Goo");
  gst_vorbis_tag_add (list, "CONTACT", "Coo");
  gst_vorbis_tag_add (list, "COMMENT", "Stroodle is good");
  gst_vorbis_tag_add (list, "COMMENT", "Peroxysulfid stroodles the brain");
  gst_vorbis_tag_add (list, "ACOUSTID_ID", "5678913");
  gst_vorbis_tag_add (list, "ACOUSTID_FINGERPRINT", "5678912");

  gst_vorbis_tag_add (list, "TRACKNUMBER", "5");
  gst_vorbis_tag_add (list, "TRACKTOTAL", "77");
  gst_vorbis_tag_add (list, "DISCNUMBER", "1");
  gst_vorbis_tag_add (list, "DISCTOTAL", "2");
  gst_vorbis_tag_add (list, "DATE", "1954-12-31");

  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_TITLE, "Too");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ALBUM, "Aoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ARTIST, "Alboo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_PERFORMER, "Perfoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_COPYRIGHT, "Copyfoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_DESCRIPTION, "Descoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LICENSE, "Licoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LICENSE_URI,
      "http://creativecommons.org/licenses/by/3.0/");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_GEO_LOCATION_NAME, "Bristol, UK");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ORGANIZATION, "Orgoo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_GENRE, "Goo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_CONTACT, "Coo");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_COMMENT,
      "Peroxysulfid stroodles the brain");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ACOUSTID_ID, "5678913");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ACOUSTID_FINGERPRINT, "5678912");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_COMMENT, "Stroodle is good");
  ASSERT_TAG_LIST_HAS_UINT (list, GST_TAG_TRACK_NUMBER, 5);
  ASSERT_TAG_LIST_HAS_UINT (list, GST_TAG_TRACK_COUNT, 77);
  ASSERT_TAG_LIST_HAS_UINT (list, GST_TAG_ALBUM_VOLUME_NUMBER, 1);
  ASSERT_TAG_LIST_HAS_UINT (list, GST_TAG_ALBUM_VOLUME_COUNT, 2);

  {
    GstDateTime *dt = NULL;

    fail_unless (gst_tag_list_get_date_time (list, GST_TAG_DATE_TIME, &dt));
    fail_unless (dt != NULL);
    fail_unless (gst_date_time_get_day (dt) == 31);
    fail_unless (gst_date_time_get_month (dt) == 12);
    fail_unless (gst_date_time_get_year (dt) == 1954);
    fail_unless (!gst_date_time_has_time (dt));

    gst_date_time_unref (dt);
  }

  /* unknown vorbis comments should go into a GST_TAG_EXTENDED_COMMENT */
  gst_vorbis_tag_add (list, "CoEdSub_ID", "98172AF-973-10-B");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_EXTENDED_COMMENT,
      "CoEdSub_ID=98172AF-973-10-B");
  gst_vorbis_tag_add (list, "RuBuWuHash", "1337BA42F91");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_EXTENDED_COMMENT,
      "RuBuWuHash=1337BA42F91");

  gst_vorbis_tag_add (list, "REPLAYGAIN_REFERENCE_LOUDNESS", "89.");
  ASSERT_TAG_LIST_HAS_DOUBLE (list, GST_TAG_REFERENCE_LEVEL, 89.);
  gst_vorbis_tag_add (list, "REPLAYGAIN_TRACK_GAIN", "+12.36");
  ASSERT_TAG_LIST_HAS_DOUBLE (list, GST_TAG_TRACK_GAIN, +12.36);
  gst_vorbis_tag_add (list, "REPLAYGAIN_TRACK_PEAK", "0.96349");
  ASSERT_TAG_LIST_HAS_DOUBLE (list, GST_TAG_TRACK_PEAK, 0.96349);
  gst_vorbis_tag_add (list, "REPLAYGAIN_ALBUM_GAIN", "+10.12");
  ASSERT_TAG_LIST_HAS_DOUBLE (list, GST_TAG_ALBUM_GAIN, +10.12);
  /* now check that we can parse floating point numbers with any separator
   * (',' or '.') regardless of the current locale */
  gst_vorbis_tag_add (list, "REPLAYGAIN_ALBUM_PEAK", "0,98107");
  ASSERT_TAG_LIST_HAS_DOUBLE (list, GST_TAG_ALBUM_PEAK, 0.98107);
  gst_vorbis_tag_add (list, "LICENSE", "http://foo.com/license-1.html");

  /* make sure we can convert back and forth without loss */
  {
    GstTagList *new_list, *even_newer_list;
    GstBuffer *buf, *buf2;
    gchar *vendor_id = NULL;

    buf = gst_tag_list_to_vorbiscomment_buffer (list,
        (const guint8 *) "\003vorbis", 7, "libgstunittest");
    fail_unless (buf != NULL);
    new_list = gst_tag_list_from_vorbiscomment_buffer (buf,
        (const guint8 *) "\003vorbis", 7, &vendor_id);
    fail_unless (new_list != NULL);
    fail_unless (vendor_id != NULL);
    g_free (vendor_id);
    vendor_id = NULL;

    GST_LOG ("new_list = %" GST_PTR_FORMAT, new_list);
    fail_unless (gst_tag_list_is_equal (list, new_list));

    buf2 = gst_tag_list_to_vorbiscomment_buffer (new_list,
        (const guint8 *) "\003vorbis", 7, "libgstunittest");
    fail_unless (buf2 != NULL);
    even_newer_list = gst_tag_list_from_vorbiscomment_buffer (buf2,
        (const guint8 *) "\003vorbis", 7, &vendor_id);
    fail_unless (even_newer_list != NULL);
    fail_unless (vendor_id != NULL);
    g_free (vendor_id);
    vendor_id = NULL;

    GST_LOG ("even_newer_list = %" GST_PTR_FORMAT, even_newer_list);
    fail_unless (gst_tag_list_is_equal (new_list, even_newer_list));

    gst_tag_list_unref (new_list);
    gst_tag_list_unref (even_newer_list);
    gst_buffer_unref (buf);
    gst_buffer_unref (buf2);
  }

  /* there can only be one language per taglist ... */
  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "fr");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "fr");

  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "[fr]");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "fr");

  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "French [fr]");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "fr");

  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "[eng] English");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "eng");

  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "eng");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "eng");

  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "[eng]");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "eng");

  /* free-form *sigh* */
  gst_tag_list_unref (list);
  list = gst_tag_list_new_empty ();
  gst_vorbis_tag_add (list, "LANGUAGE", "English");
  ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_LANGUAGE_CODE, "English");

  /* now, while we still have a taglist, test _to_vorbiscomment_buffer() */
  {
    GstBuffer *buf1, *buf2;
    GstMapInfo map1, map2;

    ASSERT_CRITICAL (gst_tag_list_to_vorbiscomment_buffer (NULL,
            (const guint8 *) "x", 1, "x"));

    buf1 = gst_tag_list_to_vorbiscomment_buffer (list, NULL, 0, NULL);
    fail_unless (buf1 != NULL);

    buf2 = gst_tag_list_to_vorbiscomment_buffer (list,
        (const guint8 *) "foo", 3, NULL);
    fail_unless (buf2 != NULL);

    gst_buffer_map (buf1, &map1, GST_MAP_READ);
    gst_buffer_map (buf2, &map2, GST_MAP_READ);

    fail_unless (memcmp (map1.data, map2.data + 3, map1.size) == 0);

    gst_buffer_unmap (buf2, &map2);
    gst_buffer_unmap (buf1, &map1);

    gst_buffer_unref (buf1);
    gst_buffer_unref (buf2);
  }

  gst_tag_list_unref (list);

  /* make sure gst_tag_list_from_vorbiscomment_buffer() works with an
   * empty ID (for Speex) */
  {
    const guint8 speex_comments_buf1[] = { 0x03, 0x00, 0x00, 0x00, 'f', 'o',
      'o', 0x00, 0x00, 0x00, 0x00
    };
    GstBuffer *buf;
    gchar *vendor = NULL;

    buf = gst_buffer_new ();
    gst_buffer_append_memory (buf,
        gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
            (gpointer) speex_comments_buf1,
            sizeof (speex_comments_buf1), 0, sizeof (speex_comments_buf1), NULL,
            NULL));

    /* make sure it doesn't memcmp over the end of the buffer */
    fail_unless (gst_tag_list_from_vorbiscomment_buffer (buf,
            (const guint8 *) "averylongstringbrownfoxjumpoverthefence", 39,
            &vendor) == NULL);
    fail_unless (vendor == NULL);

    /* make sure it bails out if the ID doesn't match */
    fail_unless (gst_tag_list_from_vorbiscomment_buffer (buf,
            (guint8 *) "short", 4, &vendor) == NULL);
    fail_unless (vendor == NULL);

    /* now read properly */
    list = gst_tag_list_from_vorbiscomment_buffer (buf, NULL, 0, &vendor);
    fail_unless (vendor != NULL);
    fail_unless_equals_string (vendor, "foo");
    fail_unless (list != NULL);
    fail_unless (gst_tag_list_n_tags (list) == 0);
    g_free (vendor);
    gst_tag_list_unref (list);

    /* now again without vendor */
    list = gst_tag_list_from_vorbiscomment_buffer (buf, NULL, 0, NULL);
    fail_unless (list != NULL);
    fail_unless (gst_tag_list_n_tags (list) == 0);
    gst_tag_list_unref (list);

    gst_buffer_unref (buf);
  }

  /* the same with an ID */
  {
    const guint8 vorbis_comments_buf[] = { 0x03, 'v', 'o', 'r', 'b', 'i', 's',
      0x03, 0x00, 0x00, 0x00, 'f', 'o', 'o', 0x01, 0x00, 0x00, 0x00,
      strlen ("ARTIST=foo bar"), 0x00, 0x00, 0x00, 'A', 'R', 'T', 'I', 'S',
      'T', '=', 'f', 'o', 'o', ' ', 'b', 'a', 'r'
    };
    GstBuffer *buf;
    gchar *vendor = NULL;

    buf = gst_buffer_new ();
    gst_buffer_append_memory (buf,
        gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
            (gpointer) vorbis_comments_buf,
            sizeof (vorbis_comments_buf), 0, sizeof (vorbis_comments_buf), NULL,
            NULL));

    /* make sure it doesn't memcmp over the end of the buffer */
    fail_unless (gst_tag_list_from_vorbiscomment_buffer (buf,
            (const guint8 *) "averylongstringbrownfoxjumpoverthefence", 39,
            &vendor) == NULL);
    fail_unless (vendor == NULL);

    /* make sure it bails out if the ID doesn't match */
    fail_unless (gst_tag_list_from_vorbiscomment_buffer (buf,
            (guint8 *) "short", 4, &vendor) == NULL);
    fail_unless (vendor == NULL);

    /* now read properly */
    list = gst_tag_list_from_vorbiscomment_buffer (buf,
        (guint8 *) "\003vorbis", 7, &vendor);
    fail_unless (vendor != NULL);
    fail_unless_equals_string (vendor, "foo");
    fail_unless (list != NULL);
    fail_unless (gst_tag_list_n_tags (list) == 1);
    ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ARTIST, "foo bar");
    g_free (vendor);
    gst_tag_list_unref (list);

    /* now again without vendor */
    list = gst_tag_list_from_vorbiscomment_buffer (buf,
        (guint8 *) "\003vorbis", 7, NULL);
    fail_unless (list != NULL);
    fail_unless (gst_tag_list_n_tags (list) == 1);
    ASSERT_TAG_LIST_HAS_STRING (list, GST_TAG_ARTIST, "foo bar");
    gst_tag_list_unref (list);

    gst_buffer_unref (buf);
  }

  /* check date with time */
  {
    GstDateTime *dt = NULL;

    list = gst_tag_list_new_empty ();
    gst_vorbis_tag_add (list, "DATE", "2006-09-25 22:02:38");

    fail_unless (gst_tag_list_get_date_time (list, GST_TAG_DATE_TIME, &dt));
    fail_unless (dt != NULL);
    fail_unless (gst_date_time_get_day (dt) == 25);
    fail_unless (gst_date_time_get_month (dt) == 9);
    fail_unless (gst_date_time_get_year (dt) == 2006);
    fail_unless (gst_date_time_has_time (dt));

    gst_date_time_unref (dt);
    gst_tag_list_unref (list);
  }

  /* check date with month/day of 00-00 */
  {
    GstDateTime *dt = NULL;

    list = gst_tag_list_new_empty ();
    gst_vorbis_tag_add (list, "DATE", "1992-00-00");

    fail_unless (gst_tag_list_get_date_time (list, GST_TAG_DATE_TIME, &dt));
    fail_unless (dt != NULL);
    fail_unless (gst_date_time_get_year (dt) == 1992);
    fail_unless (!gst_date_time_has_month (dt));
    fail_unless (!gst_date_time_has_day (dt));
    fail_unless (!gst_date_time_has_time (dt));

    gst_date_time_unref (dt);
    gst_tag_list_unref (list);
  }

  /* check date with valid month, but day of 00 */
  {
    GstDateTime *dt = NULL;

    list = gst_tag_list_new_empty ();
    gst_vorbis_tag_add (list, "DATE", "1992-05-00");

    fail_unless (gst_tag_list_get_date_time (list, GST_TAG_DATE_TIME, &dt));
    fail_unless (dt != NULL);
    fail_unless (gst_date_time_get_year (dt) == 1992);
    fail_unless (gst_date_time_get_month (dt) == 5);
    fail_unless (!gst_date_time_has_day (dt));
    fail_unless (!gst_date_time_has_time (dt));

    gst_date_time_unref (dt);
    gst_tag_list_unref (list);
  }
}

GST_END_TEST;

GST_START_TEST (test_id3_tags)
{
  guint i;

  fail_unless (gst_tag_id3_genre_count () > 0);

  for (i = 0; i < gst_tag_id3_genre_count (); ++i) {
    const gchar *genre;

    genre = gst_tag_id3_genre_get (i);
    GST_LOG ("genre: %s", genre);
    fail_unless (genre != NULL);
  }

  {
    /* TODO: GstTagList *gst_tag_list_new_from_id3v1 (const guint8 *data) */
  }

  /* gst_tag_from_id3_tag */
  fail_unless (gst_tag_from_id3_tag ("TALB") != NULL);
  ASSERT_CRITICAL (gst_tag_from_id3_tag (NULL));
  fail_unless (gst_tag_from_id3_tag ("R2D2") == NULL);
  fail_unless_equals_string (gst_tag_from_id3_tag ("WCOP"),
      GST_TAG_COPYRIGHT_URI);

  /* gst_tag_from_id3_user_tag */
  ASSERT_CRITICAL (gst_tag_from_id3_user_tag (NULL, "foo"));
  ASSERT_CRITICAL (gst_tag_from_id3_user_tag ("foo", NULL));
  fail_unless (gst_tag_from_id3_user_tag ("R2D2", "R2D2") == NULL);

  /* gst_tag_to_id3_tag */
  ASSERT_CRITICAL (gst_tag_to_id3_tag (NULL));
  fail_unless (gst_tag_to_id3_tag ("R2D2") == NULL);
  fail_unless (gst_tag_to_id3_tag (GST_TAG_ARTIST) != NULL);
  fail_unless_equals_string (gst_tag_to_id3_tag (GST_TAG_COPYRIGHT_URI),
      "WCOP");

  fail_unless (GST_TYPE_TAG_IMAGE_TYPE != 0);
  fail_unless (g_type_name (GST_TYPE_TAG_IMAGE_TYPE) != NULL);
}

GST_END_TEST;


GST_START_TEST (test_id3v1_utf8_tag)
{
  const guint8 id3v1[128] = {
    /* marker */
    'T', 'A', 'G',
    /* title (30 bytes) */
    'D', 0xc3, 0xad, 'v', 'k', 'a', ' ', 's',
    ' ', 'p', 'e', 'r', 'l', 'a', 'm', 'i',
    ' ', 'v', 'e', ' ', 'v', 'l', 'a', 's',
    'e', 'c', 'h', 0, 0, 0,
    /* artist (30 bytes) */
    'A', 'l', 'e', 0xc5, 0xa1, ' ', 'B', 'r', 'i', 'c', 'h', 't', 'a',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* album (30 bytes) */
    'B', 'e', 's', 't', ' ', 'o', 'f', ' ', '(', 'P', 'r', 'o', 's', 't',
    0xc4, 0x9b, ' ', 0xc3, 0xba, 0xc5, 0xbe, 'a', 's', 'n', 0xc3, 0xbd, ')',
    0, 0, 0,
    /* year (4 bytes) */
    '2', '0', '0', '0',
    /* comment (28 bytes) */
    '-', '-', '-', ' ', 0xc4, 0x8d, 'e', 's', 'k', 0xc3, 0xa9, ' ', 'p',
    0xc3, 0xad, 's', 'n', 'i', 0xc4, 0x8d, 'k', 'y', ' ', '-', '-', '-',
    0, 0,
    /* track number */
    0, 0,
    /* genre */
    0x11
  };
  GstDateTime *dt;
  GstTagList *tags;
  gchar *s;

  /* set this, to make sure UTF-8 strings are really interpreted properly
   * as UTF-8, regardless of the locale set */
  g_setenv ("GST_ID3V1_TAG_ENCODING", "WINDOWS-1250", TRUE);

  tags = gst_tag_list_new_from_id3v1 (id3v1);
  fail_unless (tags != NULL);

  GST_LOG ("Got tags: %" GST_PTR_FORMAT, tags);

  s = NULL;
  fail_unless (gst_tag_list_get_string (tags, GST_TAG_TITLE, &s));
  fail_unless (s != NULL);
  fail_unless_equals_string (s, "Dívka s perlami ve vlasech");
  g_free (s);

  s = NULL;
  fail_unless (gst_tag_list_get_string (tags, GST_TAG_ARTIST, &s));
  fail_unless (s != NULL);
  fail_unless_equals_string (s, "Aleš Brichta");
  g_free (s);

  s = NULL;
  fail_unless (gst_tag_list_get_string (tags, GST_TAG_ALBUM, &s));
  fail_unless (s != NULL);
  fail_unless_equals_string (s, "Best of (Prostě úžasný)");
  g_free (s);

  dt = NULL;
  fail_unless (gst_tag_list_get_date_time (tags, GST_TAG_DATE_TIME, &dt));
  fail_unless (dt != NULL);
  fail_unless_equals_int (gst_date_time_get_year (dt), 2000);
  fail_if (gst_date_time_has_month (dt));
  fail_if (gst_date_time_has_day (dt));
  fail_if (gst_date_time_has_time (dt));
  gst_date_time_unref (dt);
  dt = NULL;

  gst_tag_list_unref (tags);

  g_unsetenv ("GST_ID3V1_TAG_ENCODING");
}

GST_END_TEST;

GST_START_TEST (test_id3v2_priv_tag)
{
  const guint8 id3v2[] = {
    0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x3f, 0x50, 0x52, 0x49, 0x56, 0x00, 0x00,
    0x00, 0x35, 0x00, 0x00, 0x63, 0x6f, 0x6d, 0x2e,
    0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x73, 0x74,
    0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67, 0x2e,
    0x74, 0x72, 0x61, 0x6e, 0x73, 0x70, 0x6f, 0x72,
    0x74, 0x53, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x54,
    0x69, 0x6d, 0x65, 0x73, 0x74, 0x61, 0x6d, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0xbb,
    0xa0
  };
  const GstStructure *s;
  GstTagList *tags;
  GstSample *sample = NULL;
  GstBuffer *buf;
  GstMapInfo map;
  gchar *owner = NULL;

  buf = gst_buffer_new_allocate (NULL, sizeof (id3v2), NULL);
  gst_buffer_fill (buf, 0, id3v2, sizeof (id3v2));

  tags = gst_tag_list_from_id3v2_tag (buf);
  gst_buffer_unref (buf);

  fail_if (tags == NULL, "Failed to parse ID3 tag");

  GST_LOG ("tags: %" GST_PTR_FORMAT, tags);

  if (!gst_tag_list_get_sample (tags, GST_TAG_PRIVATE_DATA, &sample))
    g_error ("Failed to get PRIVATE_DATA tag");

  s = gst_sample_get_info (sample);
  buf = gst_sample_get_buffer (sample);

  if (!gst_structure_has_name (s, "ID3PrivateFrame"))
    g_error ("wrong info name");

  gst_structure_get (s, "owner", G_TYPE_STRING, &owner, NULL);
  fail_unless (owner != NULL);

  fail_unless_equals_string (owner,
      "com.apple.streaming.transportStreamTimestamp");

  fail_unless_equals_int (gst_buffer_get_size (buf), 8);

  gst_buffer_map (buf, &map, GST_MAP_READ);
  GST_MEMDUMP ("private data", map.data, map.size);
  fail_unless_equals_uint64 (GST_READ_UINT64_BE (map.data), 0x0dbba0);
  gst_buffer_unmap (buf, &map);
  g_free (owner);

  gst_sample_unref (sample);
  gst_tag_list_unref (tags);
}

GST_END_TEST;

static GstTagList *
parse_id3v2_tag_from_data (const guint8 * id3v2, gsize id3v2_size)
{
  GstTagList *tags;
  GstBuffer *buf;

  GST_MEMDUMP ("id3v2 tag", id3v2, id3v2_size);

  buf = gst_buffer_new_allocate (NULL, id3v2_size, NULL);
  gst_buffer_fill (buf, 0, id3v2, id3v2_size);
  tags = gst_tag_list_from_id3v2_tag (buf);
  gst_buffer_unref (buf);

  return tags;
}

GST_START_TEST (test_id3v2_extended_header)
{
  const guint8 id3v24_exthdr[1024] = {
    0x49, 0x44, 0x33, 0x04, 0x00, 0x40, 0x00, 0x00, 0x07, 0x76,
    0x00, 0x00, 0x00, 0x0c, 0x01, 0x20, 0x05, 0x0b, 0x7f, 0x06,
    0x43, 0x22, 0x54, 0x53, 0x53, 0x45, 0x00, 0x00, 0x00, 0x0e,
    0x00, 0x00, 0x03, 0x4c, 0x61, 0x76, 0x66, 0x35, 0x37, 0x2e,
    0x37, 0x31, 0x2e, 0x31, 0x30, 0x30, 0x54, 0x49, 0x54, 0x32,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x03, 0x53, 0x69, 0x6c,
    0x65, 0x6e, 0x63, 0x65, 0x54, 0x50, 0x45, 0x31, 0x00, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x03, 0x4e, 0x6f, 0x20, 0x6f, 0x6e,
    0x65, 0x54, 0x50, 0x45, 0x32, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x03, 0x4e, 0x6f, 0x6e, 0x65, 0x54, 0x41, 0x4c, 0x42,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x03, 0x4e, 0x65, 0x69,
    0x74, 0x68, 0x65, 0x72, 0x54, 0x44, 0x52, 0x43, 0x00, 0x00,
    0x00, 0x05, 0x00, 0x00, 0x03, 0x32, 0x30, 0x31, 0x38, 0x54,
    0x52, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x03,
    0x30, 0x31, 0x2f, 0x30, 0x31, 0x54, 0x43, 0x4f, 0x4e, 0x00,
    0x00, 0x00, 0x06, 0x00, 0x00, 0x03, 0x28, 0x31, 0x34, 0x38,
    0x29,
  };
  const guint8 id3v2_exthdr[] = {
    0x49, 0x44, 0x33, 0x03, 0x00, 0x40, 0x00, 0x00, 0x00, 0x1b,
    0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x50, 0x45, 0x31, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x47, 0x65, 0x6f, 0x72, 0x67, 0x65
  };
  const guint8 id3v2_no_exthdr[] = {
    0x49, 0x44, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x54, 0x50, 0x45, 0x31, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x47, 0x65, 0x6f, 0x72, 0x67, 0x65
  };
  GstTagList *tags;

  tags = parse_id3v2_tag_from_data (id3v2_exthdr, sizeof (id3v2_exthdr));
  fail_if (tags == NULL, "Failed to parse ID3 v2.3 tag with extension header");
  GST_LOG ("tags: %" GST_PTR_FORMAT, tags);
  fail_unless_equals_int (gst_tag_list_n_tags (tags), 1);
  gst_tag_list_unref (tags);

  tags = parse_id3v2_tag_from_data (id3v24_exthdr, sizeof (id3v24_exthdr));
  fail_if (tags == NULL, "Failed to parse ID3 v2.4 tag with extension header");
  GST_LOG ("tags: %" GST_PTR_FORMAT, tags);
  fail_unless_equals_int (gst_tag_list_n_tags (tags), 9);
  gst_tag_list_unref (tags);

  tags = parse_id3v2_tag_from_data (id3v2_no_exthdr, sizeof (id3v2_no_exthdr));
  fail_if (tags == NULL, "Failed to parse ID3 tag without extension header");
  GST_LOG ("tags: %" GST_PTR_FORMAT, tags);
  fail_unless_equals_int (gst_tag_list_n_tags (tags), 1);
  gst_tag_list_unref (tags);
}

GST_END_TEST;

GST_START_TEST (test_id3v2_string_list_utf16)
{
  const guint8 id3v2[] = {
    0x49, 0x44, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x43, 0x4f, 0x4d, 0x4d, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00,
    0x01, 0x65, 0x6e, 0x67, 0xff, 0xfe, 0x00, 0x00, 0xff, 0xfe,
    0x4e, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6d, 0x00, 0x20, 0x00,
    0x4d, 0x00, 0x50, 0x00, 0x33, 0x00, 0x20, 0x00, 0x4d, 0x00,
    0x75, 0x00, 0x73, 0x00, 0x69, 0x00, 0x63, 0x00, 0x20, 0x00,
    0x4c, 0x00, 0x69, 0x00, 0x62, 0x00, 0x72, 0x00, 0x61, 0x00,
    0x72, 0x00, 0x79, 0x00
  };
  GstTagList *tags;
  gchar *comment = NULL;

  tags = parse_id3v2_tag_from_data (id3v2, sizeof (id3v2));
  fail_if (tags == NULL,
      "Failed to parse ID3 tag with UTF-16 strings and BOMs");

  GST_LOG ("tags: %" GST_PTR_FORMAT, tags);

  gst_tag_list_get_string (tags, GST_TAG_COMMENT, &comment);
  fail_unless (comment != NULL, "Expected comment tag");
  GST_MEMDUMP ("comment string UTF-8", (guint8 *) comment, strlen (comment));
  fail_unless_equals_string (comment, "Naim MP3 Music Library");
  g_free (comment);
  gst_tag_list_unref (tags);
}

GST_END_TEST;

GST_START_TEST (test_language_utils)
{
  gchar **lang_codes, **c;

#define ASSERT_STRINGS_EQUAL fail_unless_equals_string

  lang_codes = gst_tag_get_language_codes ();
  fail_unless (lang_codes != NULL);
  fail_unless (*lang_codes != NULL);

  for (c = lang_codes; c != NULL && *c != NULL; ++c) {
    const gchar *lang_name, *c1, *c2t, *c2b;

    lang_name = gst_tag_get_language_name (*c);
    fail_unless (lang_name != NULL);
    fail_unless (g_utf8_validate (lang_name, -1, NULL));

    c1 = gst_tag_get_language_code_iso_639_1 (*c);
    fail_unless (c1 != NULL);
    fail_unless (g_utf8_validate (c1, -1, NULL));

    c2t = gst_tag_get_language_code_iso_639_2T (*c);
    fail_unless (c2t != NULL);
    fail_unless (g_utf8_validate (c2t, -1, NULL));

    c2b = gst_tag_get_language_code_iso_639_2B (*c);
    fail_unless (c2b != NULL);
    fail_unless (g_utf8_validate (c2b, -1, NULL));

    ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 (*c), *c);
    ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 (c2t), *c);
    ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 (c2b), *c);

    GST_DEBUG ("[%s] %s %s %s : %s", *c, c1, c2t, c2b, lang_name);

  }
  g_strfreev (lang_codes);

  fail_unless (gst_tag_get_language_name ("de") != NULL);
  fail_unless (gst_tag_get_language_name ("deu") != NULL);
  fail_unless (gst_tag_get_language_name ("ger") != NULL);
  fail_unless_equals_string (gst_tag_get_language_name ("deu"),
      gst_tag_get_language_name ("ger"));
  fail_unless_equals_string (gst_tag_get_language_name ("de"),
      gst_tag_get_language_name ("ger"));
  fail_unless (gst_tag_get_language_name ("de") !=
      gst_tag_get_language_name ("fr"));

  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code ("deu"), "de");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code ("de"), "de");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code ("ger"), "de");

  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 ("deu"), "de");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 ("de"), "de");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_1 ("ger"), "de");

  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2T ("de"), "deu");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2T ("deu"), "deu");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2T ("ger"), "deu");

  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2B ("de"), "ger");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2B ("deu"), "ger");
  ASSERT_STRINGS_EQUAL (gst_tag_get_language_code_iso_639_2B ("ger"), "ger");

  fail_unless (gst_tag_check_language_code ("de"));
  fail_unless (gst_tag_check_language_code ("deu"));
  fail_unless (gst_tag_check_language_code ("ger"));
  fail_if (gst_tag_check_language_code ("xxx"));
  fail_if (gst_tag_check_language_code ("und"));
  fail_if (gst_tag_check_language_code ("un"));
  fail_if (gst_tag_check_language_code (""));
  fail_if (gst_tag_check_language_code ("\377"));
  fail_if (gst_tag_check_language_code ("deutsch"));
}

GST_END_TEST;

#define SPECIFIC_L "http://creativecommons.org/licenses/by-nc-sa/2.5/scotland/"
#define GENERIC_L "http://creativecommons.org/licenses/by/1.0/"
#define DERIVED_L "http://creativecommons.org/licenses/sampling+/1.0/tw/"

GST_START_TEST (test_license_utils)
{
  GHashTable *ht;
  GError *err = NULL;
  gchar **liblicense_refs, **r;
  gchar **lrefs, **l;
  gchar *path, *data = NULL;
  gsize data_len;

  gst_debug_set_threshold_for_name ("tag-licenses", GST_LEVEL_NONE);

  /* test jurisdiction-specific license */
  fail_unless_equals_int (gst_tag_get_license_flags (SPECIFIC_L), 0x01010703);
  fail_unless_equals_string (gst_tag_get_license_nick (SPECIFIC_L),
      "CC BY-NC-SA 2.5 SCOTLAND");
  fail_unless_equals_string (gst_tag_get_license_version (SPECIFIC_L), "2.5");
  fail_unless_equals_string (gst_tag_get_license_jurisdiction (SPECIFIC_L),
      "scotland");

  g_setenv ("GST_TAG_LICENSE_TRANSLATIONS_LANG", "C", TRUE);
  fail_unless_equals_string (gst_tag_get_license_title (SPECIFIC_L),
      "Attribution-NonCommercial-ShareAlike");
  fail_unless (gst_tag_get_license_description (SPECIFIC_L) == NULL);

  /* test generic license */
  fail_unless_equals_int (gst_tag_get_license_flags (GENERIC_L), 0x01000307);
  fail_unless_equals_string (gst_tag_get_license_nick (GENERIC_L), "CC BY 1.0");
  fail_unless_equals_string (gst_tag_get_license_version (GENERIC_L), "1.0");
  fail_unless (gst_tag_get_license_jurisdiction (GENERIC_L) == NULL);

  g_setenv ("GST_TAG_LICENSE_TRANSLATIONS_LANG", "C", TRUE);
  fail_unless_equals_string (gst_tag_get_license_title (GENERIC_L),
      "Attribution");
  fail_unless_equals_string (gst_tag_get_license_description (GENERIC_L),
      "You must attribute the work in the manner specified by the author or licensor.");

#ifdef ENABLE_NLS
  g_setenv ("GST_TAG_LICENSE_TRANSLATIONS_LANG", "fr", TRUE);
  fail_unless_equals_string (gst_tag_get_license_title (GENERIC_L),
      "Paternité");
  fail_unless_equals_string (gst_tag_get_license_description (GENERIC_L),
      "L'offrant autorise les autres à reproduire, distribuer et communiquer cette création au public. En échange, les personnes qui acceptent ce contrat doivent citer le nom de l'auteur original.");
#endif

  /* test derived (for a certain jurisdiction) license */
  fail_unless_equals_int (gst_tag_get_license_flags (DERIVED_L), 0x0100030d);
  fail_unless_equals_string (gst_tag_get_license_nick (DERIVED_L),
      "CC SAMPLING+ 1.0 TW");
  fail_unless_equals_string (gst_tag_get_license_version (DERIVED_L), "1.0");
  fail_unless_equals_string (gst_tag_get_license_jurisdiction (DERIVED_L),
      "tw");

  g_setenv ("GST_TAG_LICENSE_TRANSLATIONS_LANG", "C", TRUE);
  fail_unless_equals_string (gst_tag_get_license_title (DERIVED_L),
      "Sampling Plus");
  fail_unless_equals_string (gst_tag_get_license_description (GENERIC_L),
      "You must attribute the work in the manner specified by the author or licensor.");

  /* test all we know about */
  lrefs = gst_tag_get_licenses ();
  fail_unless (lrefs != NULL);
  fail_unless (*lrefs != NULL);

  GST_INFO ("%d licenses", g_strv_length (lrefs));
  fail_unless (g_strv_length (lrefs) >= 376);

  ht = g_hash_table_new (g_str_hash, g_str_equal);

  for (l = lrefs; l != NULL && *l != NULL; ++l) {
    const gchar *ref, *nick, *title, *desc G_GNUC_UNUSED;

    ref = (const gchar *) *l;
    nick = gst_tag_get_license_nick (ref);
    title = gst_tag_get_license_title (ref);
    desc = gst_tag_get_license_description (ref);
    fail_unless (nick != NULL, "no nick for license '%s'", ref);
    fail_unless (title != NULL, "no title for license '%s'", ref);
    GST_LOG ("ref: %s [nick %s]", ref, (nick) ? nick : "none");
    GST_TRACE ("    %s : %s", title, (desc) ? desc : "(no description)");

    /* make sure the list contains no duplicates */
    fail_if (g_hash_table_lookup (ht, (gpointer) ref) != NULL);
    g_hash_table_insert (ht, (gpointer) ref, (gpointer) "meep");
  }
  g_hash_table_destroy (ht);

  /* trailing slash shouldn't make a difference */
  fail_unless_equals_int (gst_tag_get_license_flags
      ("http://creativecommons.org/licenses/by-nd/1.0/"),
      gst_tag_get_license_flags
      ("http://creativecommons.org/licenses/by-nd/1.0"));
  fail_unless_equals_string (gst_tag_get_license_nick
      ("http://creativecommons.org/licenses/by-nd/1.0/"),
      gst_tag_get_license_nick
      ("http://creativecommons.org/licenses/by-nd/1.0"));
  fail_unless_equals_int (gst_tag_get_license_flags
      ("http://creativecommons.org/licenses/by-nd/2.5/ca/"),
      gst_tag_get_license_flags
      ("http://creativecommons.org/licenses/by-nd/2.5/ca"));
  fail_unless_equals_string (gst_tag_get_license_nick
      ("http://creativecommons.org/licenses/by-nd/2.5/ca/"),
      gst_tag_get_license_nick
      ("http://creativecommons.org/licenses/by-nd/2.5/ca"));

  /* unknown licenses */
  fail_unless (gst_tag_get_license_nick
      ("http://creativecommons.org/licenses/by-nd/25/ca/") == NULL);
  fail_unless (gst_tag_get_license_flags
      ("http://creativecommons.org/licenses/by-nd/25/ca") == 0);
  fail_unless (gst_tag_get_license_jurisdiction
      ("http://creativecommons.org/licenses/by-nd/25/ca/") == NULL);
  fail_unless (gst_tag_get_license_jurisdiction
      ("http://creativecommons.org/licenses/by-nd/25/ca") == NULL);
  fail_unless (gst_tag_get_license_title
      ("http://creativecommons.org/licenses/by-nd/25/ca") == NULL);
  fail_unless (gst_tag_get_license_jurisdiction
      ("http://creativecommons.org/licenses/by-nd/25/ca") == NULL);

  /* unknown prefixes even */
  fail_unless (gst_tag_get_license_nick
      ("http://copycats.org/licenses/by-nd/2.5/ca/") == NULL);
  fail_unless (gst_tag_get_license_flags
      ("http://copycats.org/licenses/by-nd/2.5/ca") == 0);
  fail_unless (gst_tag_get_license_jurisdiction
      ("http://copycats.org/licenses/by-nd/2.5/ca/") == NULL);
  fail_unless (gst_tag_get_license_title
      ("http://copycats.org/licenses/by-nd/2.5/ca/") == NULL);
  fail_unless (gst_tag_get_license_description
      ("http://copycats.org/licenses/by-nd/2.5/ca/") == NULL);

  /* read list of liblicense refs from file */
  path = g_build_filename (GST_TEST_FILES_PATH, "license-uris", NULL);
  GST_LOG ("reading file '%s'", path);
  if (!g_file_get_contents (path, &data, &data_len, &err)) {
    g_error ("error loading test file: %s", err->message);
  }

  while (data_len > 0 && data[data_len - 1] == '\n') {
    data[--data_len] = '\0';
  }

  liblicense_refs = g_strsplit (data, "\n", -1);
  g_free (data);
  g_free (path);

  fail_unless (g_strv_length (lrefs) >= g_strv_length (liblicense_refs));

  for (r = liblicense_refs; r != NULL && *r != NULL; ++r) {
    GstTagLicenseFlags flags;
    const gchar *version, *nick, *jur;
    const gchar *ref = *r;

    GST_LOG ("liblicense ref: %s", ref);

    version = gst_tag_get_license_version (ref);
    if (strstr (ref, "publicdomain") != NULL)
      fail_unless (version == NULL);
    else
      fail_unless (version != NULL, "expected version for license %s", ref);

    flags = gst_tag_get_license_flags (ref);
    fail_unless (flags != 0, "expected non-zero flags for license %s", ref);

    nick = gst_tag_get_license_nick (ref);
    fail_unless (nick != NULL, "expected nick for license %s", ref);

    jur = gst_tag_get_license_jurisdiction (ref);
    if (g_str_has_suffix (ref, "de/")) {
      fail_unless_equals_string (jur, "de");
    } else if (g_str_has_suffix (ref, "scotland")) {
      fail_unless_equals_string (jur, "scotland");
    } else if (g_str_has_suffix (ref, ".0") || g_str_has_suffix (ref, ".1")) {
      fail_unless (jur == NULL);
    }
  }

  g_strfreev (liblicense_refs);
  g_strfreev (lrefs);
}

GST_END_TEST;

GST_START_TEST (test_xmp_formatting)
{
  GstTagList *list;
  GstBuffer *buf;
  GstMapInfo map;
  const gchar *text;
  gsize len;

  /* test data */
  list = gst_tag_list_new (GST_TAG_TITLE, "test title",
      GST_TAG_DESCRIPTION, "test description",
      GST_TAG_KEYWORDS, "keyword1", GST_TAG_KEYWORDS, "keyword2", NULL);

  buf = gst_tag_list_to_xmp_buffer (list, FALSE, NULL);
  fail_unless (buf != NULL);

  gst_buffer_map (buf, &map, GST_MAP_READ);
  text = (gchar *) map.data;
  len = map.size;

  /* check the content */
  fail_unless (g_strrstr_len (text, len, "<?xpacket begin") == text);
  fail_unless (g_strrstr_len (text, len, ">test title<") != NULL);
  fail_unless (g_strrstr_len (text, len, ">test description<") != NULL);
  fail_unless (g_strrstr_len (text, len, ">keyword1<") != NULL);
  fail_unless (g_strrstr_len (text, len, ">keyword2<") != NULL);
  fail_unless (g_strrstr_len (text, len, "<?xpacket end") != NULL);
  gst_buffer_unmap (buf, &map);

  gst_buffer_unref (buf);
  gst_tag_list_unref (list);
}

GST_END_TEST;


GST_START_TEST (test_xmp_parsing)
{
  GstTagList *list;
  GstBuffer *buf;
  guint i, j, result_size;
  gchar *text;
  const gchar *xmp_header =
      "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>"
      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"GStreamer\">"
      "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">";

  /* We used to write an extra trailing \n after the footer, keep compatibility
   * with our old generated media by checking that it still can be parsed */
  const gchar *xmp_footers[] = {
    "</rdf:RDF>" "</x:xmpmeta>" "<?xpacket end=\"r\"?>",
    "</rdf:RDF>" "</x:xmpmeta>" "<?xpacket end=\"r\"?>\n",
    NULL
  };

  struct
  {
    const gchar *xmp_data;
    gint result_size;
    gint result_test;
  } test_data[] = {
    {
        "", -1, -1}, {
        "<rdf:Description rdf:about=\"\" />", 0, -1}, {
        "<rdf:Description rdf:about=\"\"></rdf:Description>", 0, -1}, {
        "<rdf:Description    rdf:about=\"\"    ></rdf:Description>", 0, -1}, {
          "<rdf:Description rdf:about=\"\"><dc:description>test</dc:description></rdf:Description>",
        1, 0}, {
          "<rdf:Description rdf:about=\"\" dc:description=\"test\"></rdf:Description>",
        1, 0}, {
        NULL, -1, -1}
  };

  /* test data */
  j = 0;
  i = 0;
  while (xmp_footers[j]) {
    while (test_data[i].xmp_data) {
      gsize len;

      GST_DEBUG ("trying test-data %u", i);

      text =
          g_strconcat (xmp_header, test_data[i].xmp_data, xmp_footers[j], NULL);

      buf = gst_buffer_new ();
      len = strlen (text) + 1;
      gst_buffer_append_memory (buf,
          gst_memory_new_wrapped (0, text, len, 0, len, NULL, NULL));

      list = gst_tag_list_from_xmp_buffer (buf);
      if (test_data[i].result_size >= 0) {
        fail_unless (list != NULL);

        result_size = gst_tag_list_n_tags (list);
        fail_unless (result_size == test_data[i].result_size);

        /* check the taglist content */
        switch (test_data[i].result_test) {
          case 0:
            ASSERT_TAG_LIST_HAS_STRING (list, "description", "test");
            break;
          default:
            break;
        }
      }
      if (list)
        gst_tag_list_unref (list);

      gst_buffer_unref (buf);
      g_free (text);
      i++;
    }
    j++;
  }
}

GST_END_TEST;

static void
do_xmp_tag_serialization_deserialization (GstTagList * taglist,
    const gchar ** schemas)
{
  GstTagList *taglist2;
  GstBuffer *buf;

  buf = gst_tag_list_to_xmp_buffer (taglist, TRUE, schemas);
  taglist2 = gst_tag_list_from_xmp_buffer (buf);

  fail_unless (gst_tag_list_is_equal (taglist, taglist2));

  gst_buffer_unref (buf);
  gst_tag_list_unref (taglist2);
}

static void
do_simple_xmp_tag_serialization_deserialization (const gchar * gsttag,
    GValue * value)
{
  GstTagList *taglist = gst_tag_list_new_empty ();

  gst_tag_list_add_value (taglist, GST_TAG_MERGE_REPLACE, gsttag, value);

  do_xmp_tag_serialization_deserialization (taglist, NULL);
  gst_tag_list_unref (taglist);
}

GST_START_TEST (test_xmp_tags_serialization_deserialization)
{
  GValue value = { 0 };
  GstDateTime *datetime;

  gst_tag_register_musicbrainz_tags ();

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, "my string");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_ARTIST, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_COPYRIGHT, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DESCRIPTION, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_KEYWORDS, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_TITLE, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_VIDEO_CODEC, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_GEO_LOCATION_COUNTRY,
      &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_GEO_LOCATION_CITY,
      &value);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_SUBLOCATION, &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DEVICE_MANUFACTURER,
      &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DEVICE_MODEL,
      &value);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_APPLICATION_NAME,
      &value);

  g_value_set_static_string (&value, "rotate-0");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-0");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-180");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-180");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-270");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-90");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-90");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-270");
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_DOUBLE);

  g_value_set_double (&value, 0.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LATITUDE, &value);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);
  g_value_set_double (&value, 10.5);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LATITUDE, &value);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);
  g_value_set_double (&value, -32.375);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LATITUDE, &value);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);

  g_value_set_double (&value, 0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);
  g_value_set_double (&value, 100);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);
  g_value_set_double (&value, 500.25);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);
  g_value_set_double (&value, -12.75);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);

  g_value_set_double (&value, 0.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);
  g_value_set_double (&value, 10.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);
  g_value_set_double (&value, 786.125);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);
  g_value_set_double (&value, -2.5);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);

  g_value_set_double (&value, 0.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, &value);
  g_value_set_double (&value, 180.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, &value);
  g_value_set_double (&value, 359.99);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, &value);

  g_value_set_double (&value, 0.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, &value);
  g_value_set_double (&value, 90.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, &value);
  g_value_set_double (&value, 359.99);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, &value);

  g_value_set_double (&value, 0.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);
  g_value_set_double (&value, 1.0);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);
  g_value_set_double (&value, -2.5);
  do_simple_xmp_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_DATE_TIME);
  datetime = gst_date_time_new_ymd (2010, 3, 22);
  g_value_take_boxed (&value, datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, 0);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_USER_RATING, &value);
  g_value_set_uint (&value, 100);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_USER_RATING, &value);
  g_value_set_uint (&value, 22);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_USER_RATING, &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_DATE_TIME);
  datetime = gst_date_time_new (0, 2010, 6, 22, 12, 5, 10);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new (0, 2010, 6, 22, 12, 5, 10.000125);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new (0, 2010, 6, 22, 12, 5, 10.000001);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new (0, 2010, 6, 22, 12, 5, 10.123456);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new (-3, 2010, 6, 22, 12, 5, 10.123456);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new (5, 2010, 6, 22, 12, 5, 10.123456);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  datetime = gst_date_time_new_local_time (2010, 12, 2, 12, 5, 10.000043);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_xmp_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  g_value_unset (&value);
}

GST_END_TEST;


GST_START_TEST (test_xmp_compound_tags)
{
  const gchar *schemas[] = { "Iptc4xmpExt", NULL };
  GstTagList *taglist = gst_tag_list_new_empty ();

  gst_tag_list_add (taglist, GST_TAG_MERGE_APPEND, GST_TAG_KEYWORDS, "k1",
      GST_TAG_KEYWORDS, "k2", GST_TAG_TITLE, "title", GST_TAG_KEYWORDS, "k3",
      NULL);
  do_xmp_tag_serialization_deserialization (taglist, NULL);
  gst_tag_list_unref (taglist);

  taglist = gst_tag_list_new_empty ();
  gst_tag_list_add (taglist, GST_TAG_MERGE_APPEND, GST_TAG_GEO_LOCATION_COUNTRY,
      "Brazil", GST_TAG_GEO_LOCATION_CITY, "Campina Grande", NULL);
  do_xmp_tag_serialization_deserialization (taglist, schemas);
  gst_tag_list_unref (taglist);
}

GST_END_TEST;


GST_START_TEST (test_exif_parsing)
{
  GstTagList *taglist;
  GstBuffer *buf;
  GstByteWriter writer;
  gboolean res = TRUE;
  const gchar *str = NULL;

  gst_byte_writer_init (&writer);

  /* write the IFD */
  /* 1 entry */
  res &= gst_byte_writer_put_uint16_le (&writer, 1);

  /* copyright tag */
  /* tag id */
  res &= gst_byte_writer_put_uint16_le (&writer, 0x8298);
  /* tag type */
  res &= gst_byte_writer_put_uint16_le (&writer, 0x2);
  /* count */
  res &= gst_byte_writer_put_uint32_le (&writer, strlen ("my copyright") + 1);
  /* offset */
  res &= gst_byte_writer_put_uint32_le (&writer, 8 + 14);

  /* data */
  res &= gst_byte_writer_put_string (&writer, "my copyright");

  fail_unless (res, "Failed to write tag");

  buf = gst_byte_writer_reset_and_get_buffer (&writer);

  taglist = gst_tag_list_from_exif_buffer (buf, G_LITTLE_ENDIAN, 8);

  fail_unless (gst_tag_list_get_tag_size (taglist, GST_TAG_COPYRIGHT) == 1);
  gst_tag_list_peek_string_index (taglist, GST_TAG_COPYRIGHT, 0, &str);
  fail_unless_equals_string (str, "my copyright");

  gst_tag_list_unref (taglist);
  gst_buffer_unref (buf);
}

GST_END_TEST;


static void
do_exif_tag_serialization_deserialization (GstTagList * taglist)
{
  GstTagList *taglist2;
  GstBuffer *buf;

  /* LE */
  buf = gst_tag_list_to_exif_buffer (taglist, G_LITTLE_ENDIAN, 0);
  taglist2 = gst_tag_list_from_exif_buffer (buf, G_LITTLE_ENDIAN, 0);
  gst_buffer_unref (buf);

  fail_unless (gst_tag_list_is_equal (taglist, taglist2));
  gst_tag_list_unref (taglist2);

  /* BE */
  buf = gst_tag_list_to_exif_buffer (taglist, G_BIG_ENDIAN, 0);
  taglist2 = gst_tag_list_from_exif_buffer (buf, G_BIG_ENDIAN, 0);
  gst_buffer_unref (buf);

  fail_unless (gst_tag_list_is_equal (taglist, taglist2));
  gst_tag_list_unref (taglist2);

  /* APP1 */
  buf = gst_tag_list_to_exif_buffer_with_tiff_header (taglist);
  taglist2 = gst_tag_list_from_exif_buffer_with_tiff_header (buf);
  gst_buffer_unref (buf);

  fail_unless (gst_tag_list_is_equal (taglist, taglist2));
  gst_tag_list_unref (taglist2);
}

static void
do_simple_exif_tag_serialization_deserialization (const gchar * gsttag,
    GValue * value)
{
  GstTagList *taglist = gst_tag_list_new_empty ();

  gst_tag_list_add_value (taglist, GST_TAG_MERGE_REPLACE, gsttag, value);
  do_exif_tag_serialization_deserialization (taglist);

  gst_tag_list_unref (taglist);
}

/*
 * Adds tags from multiple ifd tables and tries serializing them
 */
GST_START_TEST (test_exif_multiple_tags)
{
  GstTagList *taglist;
  GstDateTime *datetime;
  GValue value = { 0 };

  gst_tag_register_musicbrainz_tags ();

  taglist = gst_tag_list_new (GST_TAG_ARTIST, "artist",
      GST_TAG_DEVICE_MANUFACTURER, "make",
      GST_TAG_DEVICE_MODEL, "model", GST_TAG_GEO_LOCATION_LATITUDE, 45.5,
      GST_TAG_GEO_LOCATION_LONGITUDE, -10.25,
      GST_TAG_IMAGE_HORIZONTAL_PPI, 300.0,
      GST_TAG_IMAGE_VERTICAL_PPI, 300.0, NULL);

  g_value_init (&value, GST_TYPE_DATE_TIME);
  datetime = gst_date_time_new_local_time (2010, 6, 22, 12, 5, 10);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  gst_tag_list_add_value (taglist, GST_TAG_MERGE_APPEND, GST_TAG_DATE_TIME,
      &value);
  g_value_unset (&value);

  do_exif_tag_serialization_deserialization (taglist);

  gst_tag_list_unref (taglist);
}

GST_END_TEST;


GST_START_TEST (test_exif_tags_serialization_deserialization)
{
  GValue value = { 0 };
  GstDateTime *datetime = NULL;
  GstBuffer *buf = NULL;
  gint i;
  GstTagList *taglist;
  GstMapInfo map;
  guint8 *data;

  gst_tag_register_musicbrainz_tags ();

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, "my string");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_COPYRIGHT, &value);
  g_value_set_static_string (&value, "ty");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_ARTIST, &value);
  g_value_set_static_string (&value, "Company Software 1.2b (info)");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_APPLICATION_NAME,
      &value);

  /* non ascii chars */
  g_value_set_static_string (&value, "AaÄäEeËëIiÏïOoÖöUuÜü");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_ARTIST, &value);
  g_value_set_static_string (&value, "Äë");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_ARTIST, &value);

  /* image orientation tests */
  g_value_set_static_string (&value, "rotate-0");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-0");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-180");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-180");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-270");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-90");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "flip-rotate-90");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);
  g_value_set_static_string (&value, "rotate-270");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_IMAGE_ORIENTATION,
      &value);

  /* exposure program */
  g_value_set_static_string (&value, "undefined");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "manual");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "normal");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "aperture-priority");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "shutter-priority");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "creative");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "action");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "portrait");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);
  g_value_set_static_string (&value, "landscape");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, &value);

  /* exposure mode */
  g_value_set_static_string (&value, "auto-exposure");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_MODE, &value);
  g_value_set_static_string (&value, "manual-exposure");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_MODE, &value);
  g_value_set_static_string (&value, "auto-bracket");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_MODE, &value);

  /* scene capture type */
  g_value_set_static_string (&value, "standard");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, &value);
  g_value_set_static_string (&value, "portrait");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, &value);
  g_value_set_static_string (&value, "landscape");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, &value);
  g_value_set_static_string (&value, "night-scene");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, &value);

  g_value_set_static_string (&value, "none");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, &value);
  g_value_set_static_string (&value, "high-gain-up");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, &value);
  g_value_set_static_string (&value, "low-gain-up");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, &value);
  g_value_set_static_string (&value, "high-gain-down");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, &value);
  g_value_set_static_string (&value, "low-gain-down");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, &value);

  g_value_set_static_string (&value, "auto");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_WHITE_BALANCE, &value);
  g_value_set_static_string (&value, "manual");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_WHITE_BALANCE, &value);

  g_value_set_static_string (&value, "normal");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_CONTRAST,
      &value);
  g_value_set_static_string (&value, "hard");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_CONTRAST,
      &value);
  g_value_set_static_string (&value, "soft");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_CONTRAST,
      &value);

  g_value_set_static_string (&value, "normal");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SATURATION, &value);
  g_value_set_static_string (&value, "low-saturation");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SATURATION, &value);
  g_value_set_static_string (&value, "high-saturation");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SATURATION, &value);

  g_value_set_static_string (&value, "normal");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SHARPNESS,
      &value);
  g_value_set_static_string (&value, "hard");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SHARPNESS,
      &value);
  g_value_set_static_string (&value, "soft");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SHARPNESS,
      &value);

  g_value_set_static_string (&value, "unknown");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "average");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "center-weighted-average");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "spot");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "multi-spot");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "pattern");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "partial");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);
  g_value_set_static_string (&value, "other");
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_METERING_MODE, &value);

  g_value_set_static_string (&value, "dsc");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SOURCE,
      &value);
  g_value_set_static_string (&value, "other");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SOURCE,
      &value);
  g_value_set_static_string (&value, "transparent-scanner");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SOURCE,
      &value);
  g_value_set_static_string (&value, "reflex-scanner");
  do_simple_exif_tag_serialization_deserialization (GST_TAG_CAPTURING_SOURCE,
      &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, 40.3456784);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LATITUDE, &value);
  g_value_set_double (&value, -12.1250865);

  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LATITUDE, &value);
  g_value_set_double (&value, 0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);
  g_value_set_double (&value, 65.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);
  g_value_set_double (&value, -0.75);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_LONGITUDE, &value);

  g_value_set_double (&value, 0.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, &value);
  g_value_set_double (&value, 180.5);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, &value);
  g_value_set_double (&value, 0.12345);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, &value);
  g_value_set_double (&value, 359.9);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, &value);

  g_value_set_double (&value, 0.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);
  g_value_set_double (&value, 321.456);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);
  g_value_set_double (&value, -12.56);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_ELEVATION, &value);

  g_value_set_double (&value, 0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);
  g_value_set_double (&value, 100 / 3.6);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, &value);

  g_value_set_double (&value, 0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_HORIZONTAL_ERROR, &value);
  g_value_set_double (&value, 50.25);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_GEO_LOCATION_HORIZONTAL_ERROR, &value);

  g_value_set_double (&value, 0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_DIGITAL_ZOOM_RATIO, &value);
  g_value_set_double (&value, 2.5);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_DIGITAL_ZOOM_RATIO, &value);
  g_value_set_double (&value, 8.75);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_DIGITAL_ZOOM_RATIO, &value);

  g_value_set_double (&value, 20.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_FOCAL_LENGTH, &value);
  g_value_set_double (&value, 5.5);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_FOCAL_LENGTH, &value);

  g_value_set_double (&value, 16);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_FOCAL_RATIO, &value);
  g_value_set_double (&value, 2.7);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_FOCAL_LENGTH, &value);

  g_value_set_double (&value, 96.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_IMAGE_HORIZONTAL_PPI, &value);
  g_value_set_double (&value, 300.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_IMAGE_HORIZONTAL_PPI, &value);
  g_value_set_double (&value, 87.5);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_IMAGE_VERTICAL_PPI, &value);
  g_value_set_double (&value, 600.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_IMAGE_VERTICAL_PPI, &value);

  g_value_set_double (&value, 0.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);
  g_value_set_double (&value, 1.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);
  g_value_set_double (&value, -2.5);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, &value);

  g_value_set_double (&value, 50.0);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_FOCAL_LENGTH_35_MM, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, 400);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_ISO_SPEED, &value);
  g_value_set_int (&value, 1600);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_ISO_SPEED, &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_DATE_TIME);
  datetime = gst_date_time_new_local_time (2010, 6, 22, 12, 5, 10);
  g_value_set_boxed (&value, datetime);
  gst_date_time_unref (datetime);
  do_simple_exif_tag_serialization_deserialization (GST_TAG_DATE_TIME, &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_SAMPLE);
  buf = gst_buffer_new_and_alloc (1024);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;
  for (i = 0; i < 1024; i++)
    data[i] = i % 255;
  gst_buffer_unmap (buf, &map);
  gst_value_take_sample (&value, gst_sample_new (buf, NULL, NULL, NULL));
  gst_buffer_unref (buf);
  do_simple_exif_tag_serialization_deserialization (GST_TAG_APPLICATION_DATA,
      &value);
  g_value_unset (&value);

  g_value_init (&value, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value, 1, 1);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SHUTTER_SPEED, &value);
  gst_value_set_fraction (&value, 1, 30);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SHUTTER_SPEED, &value);
  gst_value_set_fraction (&value, 1, 200);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SHUTTER_SPEED, &value);
  gst_value_set_fraction (&value, 1, 8000);
  do_simple_exif_tag_serialization_deserialization
      (GST_TAG_CAPTURING_SHUTTER_SPEED, &value);
  g_value_unset (&value);

  /* flash is a little bit more tricky, because 2 tags are merged into 1 in
   * exif */
  taglist = gst_tag_list_new (GST_TAG_CAPTURING_FLASH_FIRED, FALSE,
      GST_TAG_CAPTURING_FLASH_MODE, "auto", NULL);
  do_exif_tag_serialization_deserialization (taglist);
  gst_tag_list_unref (taglist);

  taglist = gst_tag_list_new (GST_TAG_CAPTURING_FLASH_FIRED, TRUE,
      GST_TAG_CAPTURING_FLASH_MODE, "auto", NULL);
  do_exif_tag_serialization_deserialization (taglist);
  gst_tag_list_unref (taglist);

  taglist = gst_tag_list_new (GST_TAG_CAPTURING_FLASH_FIRED, FALSE,
      GST_TAG_CAPTURING_FLASH_MODE, "never", NULL);
  do_exif_tag_serialization_deserialization (taglist);
  gst_tag_list_unref (taglist);

  taglist = gst_tag_list_new (GST_TAG_CAPTURING_FLASH_FIRED, TRUE,
      GST_TAG_CAPTURING_FLASH_MODE, "always", NULL);
  do_exif_tag_serialization_deserialization (taglist);
  gst_tag_list_unref (taglist);
}

GST_END_TEST;

static Suite *
tag_suite (void)
{
  Suite *s = suite_create ("tag support library");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_musicbrainz_tag_registration);
  tcase_add_test (tc_chain, test_parse_extended_comment);
  tcase_add_test (tc_chain, test_vorbis_tags);
  tcase_add_test (tc_chain, test_id3_tags);
  tcase_add_test (tc_chain, test_id3v1_utf8_tag);
  tcase_add_test (tc_chain, test_id3v2_priv_tag);
  tcase_add_test (tc_chain, test_id3v2_extended_header);
  tcase_add_test (tc_chain, test_id3v2_string_list_utf16);
  tcase_add_test (tc_chain, test_language_utils);
  tcase_add_test (tc_chain, test_license_utils);
  tcase_add_test (tc_chain, test_xmp_formatting);
  tcase_add_test (tc_chain, test_xmp_parsing);
  tcase_add_test (tc_chain, test_xmp_tags_serialization_deserialization);
  tcase_add_test (tc_chain, test_xmp_compound_tags);
  tcase_add_test (tc_chain, test_exif_parsing);
  tcase_add_test (tc_chain, test_exif_tags_serialization_deserialization);
  tcase_add_test (tc_chain, test_exif_multiple_tags);
  return s;
}

GST_CHECK_MAIN (tag);
