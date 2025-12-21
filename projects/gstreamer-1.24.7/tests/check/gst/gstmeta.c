/* GStreamer
 *
 * unit test for GstMeta
 *
 * Copyright (C) <2009> Wim Taymans <wim.taymans@gmail.com>
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
# include "config.h"
#endif

#include <gst/check/gstcheck.h>

/* test metadata for PTS/DTS and duration */
typedef struct
{
  GstMeta meta;

  GstClockTime pts;
  GstClockTime dts;
  GstClockTime duration;
  GstClockTime clock_rate;
} GstMetaTest;

static GType gst_meta_test_api_get_type (void);
#define GST_META_TEST_API_TYPE (gst_meta_test_api_get_type())

static const GstMetaInfo *gst_meta_test_get_info (void);
#define GST_META_TEST_INFO (gst_meta_test_get_info())

#define GST_META_TEST_GET(buf) ((GstMetaTest *)gst_buffer_get_meta(buf,GST_META_TEST_API_TYPE))
#define GST_META_TEST_ADD(buf) ((GstMetaTest *)gst_buffer_add_meta(buf,GST_META_TEST_INFO,NULL))

typedef struct
{
  GstMeta meta;
} GstMetaFoo;

static GType gst_meta_foo_api_get_type (void);
#define GST_META_FOO_API_TYPE (gst_meta_foo_api_get_type())

static const GstMetaInfo *gst_meta_foo_get_info (void);
#define GST_META_FOO_INFO (gst_meta_foo_get_info())

#define GST_META_FOO_GET(buf) ((GstMetaFoo *)gst_buffer_get_meta(buf,GST_META_FOO_API_TYPE))
#define GST_META_FOO_ADD(buf) ((GstMetaFoo *)gst_buffer_add_meta(buf,GST_META_FOO_INFO,NULL))

#if 0
/* unused currently. This is a user function to fill the metadata with default
 * values. We don't call this from the init function because the user is mostly
 * likely going to override the values immediately after */
static void
gst_meta_test_init (GstMetaTest * meta)
{
  meta->pts = GST_CLOCK_TIME_NONE;
  meta->dts = GST_CLOCK_TIME_NONE;
  meta->duration = GST_CLOCK_TIME_NONE;
  meta->clock_rate = GST_SECOND;
}
#endif

static gboolean
test_init_func (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GST_DEBUG ("init called on buffer %p, meta %p", buffer, meta);
  /* nothing to init really, the init function is mostly for allocating
   * additional memory or doing special setup as part of adding the metadata to
   * the buffer*/
  return TRUE;
}

static void
test_free_func (GstMeta * meta, GstBuffer * buffer)
{
  GST_DEBUG ("free called on buffer %p, meta %p", buffer, meta);
  /* nothing to free really */
}

static gboolean
test_transform_func (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstMetaTest *test, *tmeta = (GstMetaTest *) meta;

  GST_DEBUG ("transform %s called from buffer %p to %p, meta %p",
      g_quark_to_string (type), buffer, transbuf, meta);

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy_data = data;

    test = GST_META_TEST_ADD (transbuf);

    if (copy_data->offset == 0) {
      /* same offset, copy timestamps */
      test->pts = tmeta->pts;
      test->dts = tmeta->dts;
      if (!copy_data->region) {
        fail_unless (gst_buffer_get_size (buffer) == copy_data->size);
        /* same size, copy duration */
        test->duration = tmeta->duration;
      } else {
        fail_unless (gst_buffer_get_size (buffer) > copy_data->size);
        /* else clear */
        test->duration = GST_CLOCK_TIME_NONE;
      }
    } else {
      fail_unless (copy_data->region == TRUE);
      test->pts = -1;
      test->dts = -1;
      test->duration = -1;
    }
    test->clock_rate = tmeta->clock_rate;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

static GType
gst_meta_test_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "timing", NULL };

  if (g_once_init_enter_pointer (&type)) {
    GType _type = gst_meta_api_type_register ("GstMetaTestAPI", tags);
    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}

static gboolean
foo_init_func (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GST_DEBUG ("init called on buffer %p, foo meta %p", buffer, meta);
  return TRUE;
}

static void
foo_free_func (GstMeta * meta, GstBuffer * buffer)
{
  GST_DEBUG ("free called on buffer %p, foo meta %p", buffer, meta);
}


static const GstMetaInfo *
gst_meta_test_get_info (void)
{
  static const GstMetaInfo *meta_test_info = NULL;

  if (g_once_init_enter_pointer ((GstMetaInfo **) & meta_test_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_META_TEST_API_TYPE,
        "GstMetaTest",
        sizeof (GstMetaTest),
        test_init_func, test_free_func, test_transform_func);
    g_once_init_leave_pointer ((GstMetaInfo **) & meta_test_info, (GstMetaInfo *) mi);
  }
  return meta_test_info;
}

static gboolean
foo_transform_func (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GST_DEBUG ("transform %s called from buffer %p to %p, meta %p",
      g_quark_to_string (type), buffer, transbuf, meta);

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GST_META_FOO_ADD (transbuf);
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

static GType
gst_meta_foo_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter_pointer (&type)) {
    GType _type = gst_meta_api_type_register ("GstMetaFooAPI", tags);
    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}

static const GstMetaInfo *
gst_meta_foo_get_info (void)
{
  static const GstMetaInfo *meta_foo_info = NULL;

  if (g_once_init_enter_pointer ((GstMetaInfo **) & meta_foo_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_META_FOO_API_TYPE,
        "GstMetaFoo",
        sizeof (GstMetaFoo),
        foo_init_func, foo_free_func, foo_transform_func);
    g_once_init_leave_pointer ((GstMetaInfo **) & meta_foo_info, (GstMetaInfo *) mi);
  }
  return meta_foo_info;
}

GST_START_TEST (test_meta_test)
{
  GstBuffer *buffer, *copy, *subbuf;
  GstMetaTest *meta;
  GstMapInfo info;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  fail_unless (gst_buffer_map (buffer, &info, GST_MAP_WRITE));
  fail_if (info.data == NULL);
  memset (info.data, 0, 4);
  gst_buffer_unmap (buffer, &info);

  /* add some metadata */
  meta = GST_META_TEST_ADD (buffer);
  fail_if (meta == NULL);
  /* fill some values */
  meta->pts = 1000;
  meta->dts = 2000;
  meta->duration = 1000;
  meta->clock_rate = 1000;

  /* copy of the buffer */
  copy = gst_buffer_copy (buffer);
  /* get metadata of the buffer */
  meta = GST_META_TEST_GET (copy);
  fail_if (meta == NULL);
  fail_if (meta->pts != 1000);
  fail_if (meta->dts != 2000);
  fail_if (meta->duration != 1000);
  fail_if (meta->clock_rate != 1000);
  gst_buffer_unref (copy);

  /* make subbuffer */
  subbuf = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, 0, 1);
  /* get metadata of the buffer */
  meta = GST_META_TEST_GET (subbuf);
  fail_if (meta == NULL);
  fail_if (meta->pts != 1000);
  fail_if (meta->dts != 2000);
  fail_if (meta->duration != -1);
  fail_if (meta->clock_rate != 1000);
  gst_buffer_unref (subbuf);

  /* make another subbuffer */
  subbuf = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, 1, 3);
  /* get metadata of the buffer */
  meta = GST_META_TEST_GET (subbuf);
  fail_if (meta == NULL);
  fail_if (meta->pts != -1);
  fail_if (meta->dts != -1);
  fail_if (meta->duration != -1);
  fail_if (meta->clock_rate != 1000);
  gst_buffer_unref (subbuf);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

static gboolean
foreach_meta (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  /* try to remove */
  *meta = NULL;
  return TRUE;
}

GST_START_TEST (test_meta_locked)
{
  GstBuffer *buffer;
  GstMetaTest *meta;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta = GST_META_TEST_ADD (buffer);
  fail_if (meta == NULL);
  GST_META_FLAG_SET (meta, GST_META_FLAG_LOCKED);

  ASSERT_CRITICAL (gst_buffer_remove_meta (buffer, (GstMeta *) meta));
  ASSERT_CRITICAL (gst_buffer_foreach_meta (buffer, foreach_meta, NULL));

  GST_META_FLAG_UNSET (meta, GST_META_FLAG_LOCKED);

  gst_buffer_remove_meta (buffer, (GstMeta *) meta);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

static gboolean
foreach_meta_remove_one (GstBuffer * buffer, GstMeta ** meta,
    gpointer to_remove)
{
  if (*meta == to_remove) {
    *meta = NULL;
  }

  return TRUE;
}

static gint
count_buffer_meta (GstBuffer * buffer)
{
  gint ret = 0;
  gpointer state = NULL;

  while (gst_buffer_iterate_meta (buffer, &state))
    ret++;

  return ret;
}

GST_START_TEST (test_meta_foreach_remove_one_of_one)
{
  GstBuffer *buffer;
  GstMetaTest *meta1;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);

  fail_unless_equals_int (count_buffer_meta (buffer), 1);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_one, meta1);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_foreach_remove_head_of_three)
{
  GstBuffer *buffer;
  GstMetaTest *meta1, *meta2, *meta3;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_if (meta2 == NULL);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_if (meta3 == NULL);

  fail_unless_equals_int (count_buffer_meta (buffer), 3);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_one, meta3);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta1);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta2);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_foreach_remove_middle_of_three)
{
  GstBuffer *buffer;
  GstMetaTest *meta1, *meta2, *meta3;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_if (meta2 == NULL);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_if (meta3 == NULL);

  fail_unless_equals_int (count_buffer_meta (buffer), 3);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_one, meta2);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta1);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta3);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_foreach_remove_tail_of_three)
{
  GstBuffer *buffer;
  GstMetaTest *meta1, *meta2, *meta3;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_if (meta2 == NULL);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_if (meta3 == NULL);

  fail_unless_equals_int (count_buffer_meta (buffer), 3);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_one, meta1);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta2);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta3);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

static gboolean
foreach_meta_remove_unpooled (GstBuffer * buffer, GstMeta ** meta,
    gpointer unused)
{
  if (!GST_META_FLAG_IS_SET (*meta, GST_META_FLAG_POOLED)) {
    *meta = NULL;
  }

  return TRUE;
}

GST_START_TEST (test_meta_foreach_remove_head_and_tail_of_three)
{
  GstBuffer *buffer;
  GstMetaTest *meta1, *meta2, *meta3;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_if (meta2 == NULL);
  GST_META_FLAG_SET (meta2, GST_META_FLAG_POOLED);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_if (meta3 == NULL);

  fail_unless_equals_int (count_buffer_meta (buffer), 3);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_unpooled, NULL);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta2);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;


GST_START_TEST (test_meta_foreach_remove_several)
{
  GstBuffer *buffer;
  GstMetaTest *meta1, *meta2, *meta3, *meta4, *meta5;
  gpointer state = NULL;

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_if (meta1 == NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_if (meta2 == NULL);
  GST_META_FLAG_SET (meta2, GST_META_FLAG_POOLED);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_if (meta3 == NULL);
  meta4 = GST_META_TEST_ADD (buffer);
  fail_if (meta4 == NULL);
  meta5 = GST_META_TEST_ADD (buffer);
  fail_if (meta5 == NULL);
  GST_META_FLAG_SET (meta5, GST_META_FLAG_POOLED);

  fail_unless_equals_int (count_buffer_meta (buffer), 5);

  gst_buffer_foreach_meta (buffer, foreach_meta_remove_unpooled, NULL);

  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta2);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == (GstMeta *) meta5);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_iterate)
{
  GstBuffer *buffer;
  GstMeta *m1, *m2, *m3, *m_found;
  GList *metas;
  gpointer state;

  /* buffer with single meta */
  buffer = gst_buffer_new_and_alloc (4);
  m1 = (GstMeta *) GST_META_TEST_ADD (buffer);
  fail_unless (m1 != NULL);

  state = NULL;
  fail_unless (gst_buffer_iterate_meta (buffer, &state) != NULL);
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  state = NULL;
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_TEST_API_TYPE) != NULL);
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_TEST_API_TYPE) == NULL);

  state = NULL;
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_FOO_API_TYPE) == NULL);

  state = NULL;
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_TEST_API_TYPE) != NULL);
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_TEST_API_TYPE) == NULL);

  /* buffer with multiple metas */
  m2 = (GstMeta *) GST_META_FOO_ADD (buffer);
  fail_unless (m2 != NULL);
  m3 = (GstMeta *) GST_META_TEST_ADD (buffer);
  fail_unless (m3 != NULL);

  /* create a list with metas, we don't know what order buffer_iterate has */
  metas = g_list_prepend (g_list_prepend (g_list_prepend (NULL, m1), m2), m3);

  state = NULL;
  m_found = gst_buffer_iterate_meta (buffer, &state);
  fail_unless (m_found != NULL);
  metas = g_list_remove (metas, m_found);
  m_found = gst_buffer_iterate_meta (buffer, &state);
  fail_unless (m_found != NULL);
  metas = g_list_remove (metas, m_found);
  m_found = gst_buffer_iterate_meta (buffer, &state);
  fail_unless (m_found != NULL);
  metas = g_list_remove (metas, m_found);

  /* should only have 3 metas, so the 4th time we should get NULL back */
  fail_unless (gst_buffer_iterate_meta (buffer, &state) == NULL);

  /* list should be empty now, we should have seen each meta once */
  fail_unless (metas == NULL);

  /* same test as above with iterate_filtered */

  /* create a list with metas, we don't know what order buffer_iterate has */
  metas = g_list_prepend (g_list_prepend (g_list_prepend (NULL, m1), m2), m3);

  state = NULL;
  m_found =
      gst_buffer_iterate_meta_filtered (buffer, &state, GST_META_TEST_API_TYPE);
  fail_unless (m_found != NULL);
  metas = g_list_remove (metas, m_found);
  m_found =
      gst_buffer_iterate_meta_filtered (buffer, &state, GST_META_TEST_API_TYPE);
  fail_unless (m_found != NULL);
  metas = g_list_remove (metas, m_found);

  /* should only have 2 Test metas, so now we should get NULL back */
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_TEST_API_TYPE) == NULL);

  /* but there should also still be a Foo meta */
  fail_unless_equals_int (g_list_length (metas), 1);
  fail_unless (metas->data == m2);
  metas = g_list_remove (metas, m2);

  state = NULL;
  m_found =
      gst_buffer_iterate_meta_filtered (buffer, &state, GST_META_FOO_API_TYPE);
  fail_unless (m_found == m2);

  /* only have 1 Foo meta, so now we should get NULL back */
  fail_unless (gst_buffer_iterate_meta_filtered (buffer, &state,
          GST_META_FOO_API_TYPE) == NULL);

  gst_buffer_unref (buffer);


}

GST_END_TEST;

#define test_meta_compare_seqnum(a,b) \
    gst_meta_compare_seqnum((GstMeta*)(a),(GstMeta*)(b))

GST_START_TEST (test_meta_seqnum)
{
  GstMetaTest *meta1, *meta2, *meta3;
  GstBuffer *buffer;

  buffer = gst_buffer_new_and_alloc (4);
  fail_unless (buffer != NULL);

  /* add some metadata */
  meta1 = GST_META_TEST_ADD (buffer);
  fail_unless (meta1 != NULL);
  meta2 = GST_META_TEST_ADD (buffer);
  fail_unless (meta2 != NULL);
  meta3 = GST_META_TEST_ADD (buffer);
  fail_unless (meta3 != NULL);

  fail_unless (test_meta_compare_seqnum (meta1, meta2) < 0);
  fail_unless (test_meta_compare_seqnum (meta2, meta3) < 0);
  fail_unless (test_meta_compare_seqnum (meta1, meta3) < 0);

  fail_unless_equals_int (test_meta_compare_seqnum (meta1, meta1), 0);
  fail_unless_equals_int (test_meta_compare_seqnum (meta2, meta2), 0);
  fail_unless_equals_int (test_meta_compare_seqnum (meta3, meta3), 0);

  fail_unless (test_meta_compare_seqnum (meta2, meta1) > 0);
  fail_unless (test_meta_compare_seqnum (meta3, meta2) > 0);
  fail_unless (test_meta_compare_seqnum (meta3, meta1) > 0);

  /* Check that gst_meta_compare_seqnum() works correctly as a GCompareFunc */
  {
    GList *list;

    /* Make list: 3, 1, 2 */
    list = g_list_prepend (NULL, meta2);
    list = g_list_prepend (list, meta1);
    list = g_list_prepend (list, meta3);

    list = g_list_sort (list, (GCompareFunc) gst_meta_compare_seqnum);

    fail_unless (g_list_nth_data (list, 0) == meta1);
    fail_unless (g_list_nth_data (list, 1) == meta2);
    fail_unless (g_list_nth_data (list, 2) == meta3);

    g_list_free (list);
  }

  /* clean up */
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_custom)
{
  GstBuffer *buffer, *trans_buf;
  const GstMetaInfo *info;
  GstCustomMeta *meta, *trans_meta;
  GstMeta *it;
  GstStructure *s, *trans_s, *expected;
  gpointer state = NULL;
  const gchar *tags[] = { "test-tag", NULL };

  info = gst_meta_register_custom ("test-custom", tags, NULL, NULL, NULL);

  fail_unless (info != NULL);

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta = gst_buffer_add_custom_meta (buffer, "test-custom");
  fail_if (meta == NULL);

  fail_unless (gst_custom_meta_has_name ((GstCustomMeta *) meta,
          "test-custom"));

  expected = gst_structure_new_empty ("test-custom");
  s = gst_custom_meta_get_structure (meta);
  fail_unless (gst_structure_is_equal (s, expected));
  gst_structure_free (expected);

  gst_structure_set (s, "test-field", G_TYPE_INT, 42, NULL);

  gst_buffer_ref (buffer);

  ASSERT_CRITICAL (gst_structure_set (s, "test-field", G_TYPE_INT, 43, NULL));

  /* Test that a copied buffer's meta structure has the correct refcount */
  trans_buf = gst_buffer_copy (buffer);
  trans_meta = gst_buffer_get_custom_meta (trans_buf, "test-custom");
  trans_s = gst_custom_meta_get_structure (trans_meta);
  gst_structure_set (trans_s, "test-field", G_TYPE_INT, 43, NULL);

  gst_buffer_unref (buffer);

  expected = gst_structure_new ("test-custom",
      "test-field", G_TYPE_INT, 42, NULL);
  fail_unless (gst_structure_is_equal (s, expected));
  gst_structure_free (expected);

  it = gst_buffer_iterate_meta (buffer, &state);

  fail_unless ((GstCustomMeta *) it == meta);

  fail_unless (it->info == info);

  /* clean up */
  gst_buffer_unref (buffer);
  gst_buffer_unref (trans_buf);
}

GST_END_TEST;

static gboolean
transform_custom (GstBuffer * transbuf, GstMeta * meta, GstBuffer * buffer,
    GQuark type, gpointer data, gint * user_data)
{
  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstStructure *s;
    GstCustomMeta *custom;

    custom = (GstCustomMeta *) gst_buffer_add_meta (transbuf, meta->info, NULL);
    s = gst_custom_meta_get_structure (custom);
    gst_structure_set (s, "test-field", G_TYPE_INT, *user_data, NULL);
  } else {
    return FALSE;
  }

  return TRUE;
}

GST_START_TEST (test_meta_custom_transform)
{
  GstBuffer *buffer, *buffer_copy;
  const GstMetaInfo *info;
  GstCustomMeta *meta;
  GstStructure *s, *expected;
  const gchar *tags[] = { "test-tag", NULL };
  gint *user_data;

  /* That memory should be deallocated at gst_deinit time */
  user_data = g_malloc (sizeof (gint));
  *user_data = 42;
  info =
      gst_meta_register_custom ("test-custom-transform", tags,
      (GstCustomMetaTransformFunction) transform_custom, user_data, g_free);

  fail_unless (info != NULL);

  buffer = gst_buffer_new_and_alloc (4);
  fail_if (buffer == NULL);

  /* add some metadata */
  meta = gst_buffer_add_custom_meta (buffer, "test-custom-transform");
  fail_if (meta == NULL);

  buffer_copy = gst_buffer_copy (buffer);
  meta = gst_buffer_get_custom_meta (buffer_copy, "test-custom-transform");
  fail_unless (meta != NULL);
  expected = gst_structure_new ("test-custom-transform",
      "test-field", G_TYPE_INT, 42, NULL);
  s = gst_custom_meta_get_structure (meta);
  fail_unless (gst_structure_is_equal (s, expected));
  gst_structure_free (expected);

  /* clean up */
  gst_buffer_unref (buffer_copy);
  gst_buffer_unref (buffer);
}

GST_END_TEST;

GST_START_TEST (test_meta_custom_serialize)
{
  const GstMetaInfo *info;
  GstCustomMeta *meta;
  GstBuffer *buffer;

  info = gst_meta_register_custom_simple ("test-custom-serialize");
  fail_unless (info != NULL);

  /* add some metadata */
  buffer = gst_buffer_new ();
  meta = gst_buffer_add_custom_meta (buffer, "test-custom-serialize");
  gst_structure_set (meta->structure, "test-field", G_TYPE_INT, 42, NULL);

  /* Serialize */
  GByteArray *data = g_byte_array_new ();
  fail_unless (gst_meta_serialize_simple ((GstMeta *) meta, data));
  gst_buffer_unref (buffer);

  /* Create a new buffer */
  buffer = gst_buffer_new ();
  guint32 consumed;
  meta = (GstCustomMeta *) gst_meta_deserialize (buffer, data->data, data->len,
      &consumed);
  fail_unless (meta);
  fail_unless (consumed == data->len);

  /* Check meta's content */
  fail_unless (gst_custom_meta_has_name (meta, "test-custom-serialize"));
  gint val;
  fail_unless (gst_structure_get_int (meta->structure, "test-field", &val));
  fail_unless_equals_int (val, 42);

  /* Add field that cannot be serialized */
  GstElement *bin = gst_bin_new ("mybin");
  gst_structure_set (meta->structure, "test-field-obj", GST_TYPE_BIN, bin,
      NULL);
  g_byte_array_set_size (data, 0);
  fail_if (gst_meta_serialize_simple ((GstMeta *) meta, data));
  fail_if (data->len != 0);
  gst_object_unref (bin);
  gst_buffer_unref (buffer);
  g_byte_array_unref (data);
}

GST_END_TEST;

static Suite *
gst_buffermeta_suite (void)
{
  Suite *s = suite_create ("GstMeta");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_meta_test);
  tcase_add_test (tc_chain, test_meta_locked);
  tcase_add_test (tc_chain, test_meta_foreach_remove_one_of_one);
  tcase_add_test (tc_chain, test_meta_foreach_remove_head_of_three);
  tcase_add_test (tc_chain, test_meta_foreach_remove_middle_of_three);
  tcase_add_test (tc_chain, test_meta_foreach_remove_tail_of_three);
  tcase_add_test (tc_chain, test_meta_foreach_remove_head_and_tail_of_three);
  tcase_add_test (tc_chain, test_meta_foreach_remove_several);
  tcase_add_test (tc_chain, test_meta_iterate);
  tcase_add_test (tc_chain, test_meta_seqnum);
  tcase_add_test (tc_chain, test_meta_custom);
  tcase_add_test (tc_chain, test_meta_custom_transform);
  tcase_add_test (tc_chain, test_meta_custom_serialize);

  return s;
}

GST_CHECK_MAIN (gst_buffermeta);
