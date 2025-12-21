/* GStreamer
 *
 * unit tests for xmp config library
 *
 * Copyright (C) 2011 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
#include <gst/tag/xmpwriter.h>

#include <string.h>

#define TEST_ELEMENT_TYPE (test_element_get_type())

typedef struct TestElement TestElement;
typedef struct TestElementClass TestElementClass;

struct TestElement
{
  GstElement parent;
};

struct TestElementClass
{
  GstElementClass parent_class;
};

GType test_element_get_type (void);

static void init_interface (GType type);

G_DEFINE_TYPE_WITH_CODE (TestElement, test_element, GST_TYPE_ELEMENT,
    init_interface (g_define_type_id));

static void
init_interface (GType type)
{
  static const GInterfaceInfo tagxmpwriter_info = {
    NULL,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type, GST_TYPE_TAG_XMP_WRITER,
      &tagxmpwriter_info);
}

static void
test_element_class_init (TestElementClass * klass)
{
}

static void
test_element_init (TestElement * this)
{
}

static gboolean
gst_buffer_equals (GstBuffer * buf_a, GstBuffer * buf_b)
{
  gboolean res;
  GstMapInfo map1, map2;

  gst_buffer_map (buf_a, &map1, GST_MAP_READ);
  gst_buffer_map (buf_b, &map2, GST_MAP_READ);

  if (map1.size == map2.size) {
    res = memcmp (map1.data, map2.data, map1.size) == 0;
  } else {
    res = FALSE;
  }
  gst_buffer_unmap (buf_a, &map1);
  gst_buffer_unmap (buf_b, &map2);

  return res;
}

static GstTagList *
create_taglist (void)
{
  return gst_tag_list_new (GST_TAG_ARTIST, "artist",
      GST_TAG_TITLE, "title", GST_TAG_COPYRIGHT, "copyright", NULL);
}

GST_START_TEST (test_no_xmp)
{
  GstTagList *taglist = create_taglist ();
  GstElement *test_element =
      (GstElement *) g_object_new (TEST_ELEMENT_TYPE, NULL);

  gst_tag_xmp_writer_remove_all_schemas (GST_TAG_XMP_WRITER (test_element));

  fail_unless (gst_tag_xmp_writer_tag_list_to_xmp_buffer (GST_TAG_XMP_WRITER
          (test_element), taglist, TRUE) == NULL);

  gst_object_unref (test_element);
  gst_tag_list_unref (taglist);
}

GST_END_TEST;


GST_START_TEST (test_default)
{
  GstTagList *taglist = create_taglist ();
  GstElement *test_element =
      (GstElement *) g_object_new (TEST_ELEMENT_TYPE, NULL);
  GstBuffer *buf;
  GstBuffer *buf2;

  buf =
      gst_tag_xmp_writer_tag_list_to_xmp_buffer (GST_TAG_XMP_WRITER
      (test_element), taglist, TRUE);
  buf2 = gst_tag_list_to_xmp_buffer (taglist, TRUE, NULL);
  fail_unless (gst_buffer_equals (buf, buf2));

  gst_object_unref (test_element);
  gst_buffer_unref (buf);
  gst_buffer_unref (buf2);
  gst_tag_list_unref (taglist);
}

GST_END_TEST;


GST_START_TEST (test_disable)
{
  GstTagList *taglist;
  GstTagList *taglist2;
  GstElement *test_element =
      (GstElement *) g_object_new (TEST_ELEMENT_TYPE, NULL);
  GstBuffer *buf;
  const gchar *str;

  taglist = gst_tag_list_new (GST_TAG_ARTIST, "artist", NULL);

  /* add a tag that is mapped on xmp schema (as of Mar, 21th 2011) */
  gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, GST_TAG_USER_RATING, 5,
      NULL);

  buf =
      gst_tag_xmp_writer_tag_list_to_xmp_buffer (GST_TAG_XMP_WRITER
      (test_element), taglist, TRUE);
  taglist2 = gst_tag_list_from_xmp_buffer (buf);
  fail_unless (gst_tag_list_is_equal (taglist, taglist2));
  gst_tag_list_unref (taglist2);
  gst_buffer_unref (buf);

  gst_tag_xmp_writer_remove_schema (GST_TAG_XMP_WRITER (test_element), "xap");
  buf =
      gst_tag_xmp_writer_tag_list_to_xmp_buffer (GST_TAG_XMP_WRITER
      (test_element), taglist, TRUE);
  taglist2 = gst_tag_list_from_xmp_buffer (buf);

  /* artist should be there, but rating shouldn't */
  fail_unless (gst_tag_list_peek_string_index (taglist2, GST_TAG_ARTIST, 0,
          &str));
  fail_unless (gst_tag_list_get_value_index (taglist2, GST_TAG_USER_RATING,
          0) == NULL);

  gst_tag_list_unref (taglist2);
  gst_buffer_unref (buf);

  gst_object_unref (test_element);
  gst_tag_list_unref (taglist);
}

GST_END_TEST;


static Suite *
xmp_config_suite (void)
{
  Suite *s = suite_create ("xmpconfig interface");
  TCase *tc_chain = tcase_create ("configuration");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_no_xmp);
  tcase_add_test (tc_chain, test_default);
  tcase_add_test (tc_chain, test_disable);

  return s;
}

GST_CHECK_MAIN (xmp_config);
