/* GStreamer
 * Copyright (C) <2004> David Schleef <david at schleef dot org>
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * gstvalue.c: Unit tests for GstValue
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

#define GLIB_DISABLE_DEPRECATION_WARNINGS
#include <gst/check/gstcheck.h>


GST_START_TEST (test_deserialize_buffer)
{
  GValue value = { 0 };
  GstBuffer *buf;
  guint8 data[8];
  guint64 val;

  g_value_init (&value, GST_TYPE_BUFFER);
  fail_unless (gst_value_deserialize (&value, "1234567890abcdef"));
  /* does not increase the refcount */
  buf = GST_BUFFER (g_value_get_boxed (&value));
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "buffer", 1);

  /* does not increase the refcount */
  buf = gst_value_get_buffer (&value);
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "buffer", 1);

  gst_buffer_extract (buf, 0, data, 8);
  val = GST_READ_UINT64_BE (data);
  fail_unless_equals_uint64 (val, G_GUINT64_CONSTANT (0x1234567890abcdef));

  /* cleanup */
  g_value_unset (&value);
}

GST_END_TEST;

/* create and serialize a buffer */
GST_START_TEST (test_serialize_buffer)
{
  GValue value = { 0 };
  GstBuffer *buf;
  gchar *serialized;
  const guint8 buf_data[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef };
  gint len;

  len = sizeof (buf_data);
  buf = gst_buffer_new_and_alloc (len);

  gst_buffer_fill (buf, 0, (gchar *) buf_data, len);

  ASSERT_MINI_OBJECT_REFCOUNT (buf, "buffer", 1);

  /* and assign buffer to mini object */
  g_value_init (&value, GST_TYPE_BUFFER);
  gst_value_take_buffer (&value, buf);
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "buffer", 1);

  /* now serialize it */
  serialized = gst_value_serialize (&value);
  GST_DEBUG ("serialized buffer to %s", serialized);
  fail_unless (serialized != NULL);
  fail_unless_equals_string (serialized, "1234567890abcdef");

  /* refcount should not change */
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "buffer", 1);

  /* cleanup */
  g_free (serialized);
  g_value_unset (&value);

  /* take NULL buffer */
  g_value_init (&value, GST_TYPE_BUFFER);
  GST_DEBUG ("setting NULL buffer");
  gst_value_take_buffer (&value, NULL);

  /* now serialize it */
  GST_DEBUG ("serializing NULL buffer");
  serialized = gst_value_serialize (&value);
  /* should return NULL */
  fail_unless (serialized == NULL);

  g_free (serialized);
  g_value_unset (&value);
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gint64)
{
  GValue value = { 0 };
  const char *strings[] = {
    "12345678901",
    "-12345678901",
    "1152921504606846976",
    "-1152921504606846976",
  };
  gint64 results[] = {
    12345678901LL,
    -12345678901LL,
    1152921504606846976LL,
    -1152921504606846976LL,
  };
  int i;

  g_value_init (&value, G_TYPE_INT64);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_int64 (&value) == results[i],
        "resulting value is %" G_GINT64_FORMAT ", not %" G_GINT64_FORMAT
        ", for string %s (%d)", g_value_get_int64 (&value),
        results[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_guint64)
{
  GValue value = { 0 };
  const char *strings[] = {
    "0xffffffffffffffff",
    "9223372036854775810",
    "-9223372036854775810",
    "-1",
    "1",
    "-0",
  };
  guint64 results[] = {
    0xffffffffffffffffULL,
    9223372036854775810ULL,
    9223372036854775806ULL,
    (guint64) - 1,
    1,
    0,
  };
  int i;

  g_value_init (&value, G_TYPE_UINT64);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_uint64 (&value) == results[i],
        "resulting value is %" G_GUINT64_FORMAT ", not %" G_GUINT64_FORMAT
        ", for string %s (%d)", g_value_get_uint64 (&value),
        results[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_guchar)
{
  GValue value = { 0 };
  const char *strings[] = {
    "0xff",
    "255",
    "-1",
    "1",
    "-0",
  };
  guchar results[] = {
    0xff,
    255,
    (guchar) - 1,
    1,
    0,
  };
  int i;

  g_value_init (&value, G_TYPE_UCHAR);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_uchar (&value) == results[i],
        "resulting value is %u not %u, for string %s (%d)",
        g_value_get_uchar (&value), results[i], strings[i], i);
  }

  /* test serialisation as well while we're at it */
  {
    gchar *str;
    GValue value = { 0 };
    g_value_init (&value, G_TYPE_UCHAR);

    g_value_set_uchar (&value, 255);
    str = gst_value_serialize (&value);

    fail_unless_equals_string (str, "255");
    g_free (str);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gstfraction)
{
  GValue value = { 0 };
  const char *strings[] = {
    "4/5",
    "-8/9"
  };
  gint64 result_numers[] = {
    4,
    -8
  };
  gint64 result_denoms[] = {
    5,
    9
  };

  int i;

  g_value_init (&value, GST_TYPE_FRACTION);
  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (gst_value_get_fraction_numerator (&value) == result_numers[i],
        "resulting numerator value is %d, not %d"
        ", for string %s (%d)", gst_value_get_fraction_numerator (&value),
        result_numers[i], strings[i], i);
    fail_unless (gst_value_get_fraction_denominator (&value) ==
        result_denoms[i], "resulting denominator value is %d, not %d"
        ", for string %s (%d)", gst_value_get_fraction_denominator (&value),
        result_denoms[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gint)
{
  GValue value = { 0 };
  const char *strings[] = {
    "123456",
    "-123456",
    "0xFFFF",
    "0x0000FFFF",
    /* a positive long long, serializing to highest possible positive sint */
    "0x7FFFFFFF",
    /* a positive long long, serializing to lowest possible negative sint */
    "0x80000000",
    /* a negative long long, serializing to lowest possible negative sint */
    "0xFFFFFFFF80000000",
    "0xFF000000",
    /* a positive long long serializing to -1 */
    "0xFFFFFFFF",
    "0xFFFFFFFF",
    /* a negative long long serializing to -1 */
    "0xFFFFFFFFFFFFFFFF",
    "0xFFFFFFFFFFFFFFFF",
    "0xEFFFFFFF",
  };
  /* some casts need to be explicit because of unsigned -> signed */
  gint results[] = {
    123456,
    -123456,
    0xFFFF,
    0xFFFF,
    0x7FFFFFFF,
    (gint) 0x80000000,
    (gint) 0x80000000,
    (gint) 0xFF000000,
    -1,
    (gint) 0xFFFFFFFF,
    -1,
    (gint) 0xFFFFFFFFFFFFFFFFLL,
    (gint) 0xEFFFFFFF,
  };
  int i;

  g_value_init (&value, G_TYPE_INT);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_int (&value) == results[i],
        "resulting value is %d, not %d, for string %s (%d)",
        g_value_get_int (&value), results[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gint_failures)
{
  GValue value = { 0 };
  const char *strings[] = {
    "-",                        /* not a complete number */
    "- TEST",                   /* not a complete number */
    "0x0000000100000000",       /* lowest long long that cannot fit in 32 bits */
    "0xF000000000000000",
    "0xFFFFFFF000000000",
    "0xFFFFFFFF00000000",
    "0x10000000000000000",      /* first number too long to fit into a long long */
    /* invent a new processor first before trying to make this one pass */
    "0x10000000000000000000000000000000000000000000",
  };
  int i;

  g_value_init (&value, G_TYPE_INT);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_if (gst_value_deserialize (&value, strings[i]),
        "deserialized %s (%d), while it should have failed", strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_guint)
{
  GValue value = { 0 };
  const char *strings[] = {
    "123456",
    "-123456",
    "0xFFFF",
    "0x0000FFFF",
    /* a positive long long, serializing to highest possible positive sint */
    "0x7FFFFFFF",
    /* a positive long long, serializing to lowest possible negative sint */
    "0x80000000",
    "2147483648",
    /* a negative long long, serializing to lowest possible negative sint */
    "0xFFFFFFFF80000000",
    /* a value typically used for rgb masks */
    "0xFF000000",
    /* a positive long long serializing to highest possible positive uint */
    "0xFFFFFFFF",
    "0xFFFFFFFF",
    /* a negative long long serializing to highest possible positive uint */
    "0xFFFFFFFFFFFFFFFF",
    "0xEFFFFFFF",
  };
  guint results[] = {
    123456,
    (guint) - 123456,
    0xFFFF,
    0xFFFF,
    0x7FFFFFFF,
    0x80000000,
    (guint) 2147483648LL,
    0x80000000,
    0xFF000000,
    0xFFFFFFFF,
    G_MAXUINT,
    (guint) 0xFFFFFFFFFFFFFFFFLL,
    0xEFFFFFFF,
  };
  int i;

  g_value_init (&value, G_TYPE_UINT);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_uint (&value) == results[i],
        "resulting value is %d, not %d, for string %s (%d)",
        g_value_get_uint (&value), results[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_guint_failures)
{
  GValue value = { 0 };
  const char *strings[] = {
    "-",                        /* not a complete number */
    "- TEST",                   /* not a complete number */
#if 0
/* FIXME: these values should not be deserializable, since they overflow
 * the target format */
    "0x0000000100000000",       /* lowest long long that cannot fit in 32 bits */
    "0xF000000000000000",
    "0xFFFFFFF000000000",
    "0xFFFFFFFF00000000",
    "0x10000000000000000",      /* first number too long to fit into a long long */
    /* invent a new processor first before trying to make this one pass */
    "0x10000000000000000000000000000000000000000000",
#endif
  };
  int i;

  g_value_init (&value, G_TYPE_UINT);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_if (gst_value_deserialize (&value, strings[i]),
        "deserialized %s (%d), while it should have failed", strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_serialize_flags)
{
  GValue value = { 0 };
  gchar *string;
  GstSeekFlags flags[] = {
    0,
    GST_SEEK_FLAG_NONE,
    GST_SEEK_FLAG_FLUSH,
    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
  };
  const char *results[] = {
    "GST_SEEK_FLAG_NONE",
    "GST_SEEK_FLAG_NONE",
    "GST_SEEK_FLAG_FLUSH",
    "GST_SEEK_FLAG_FLUSH+GST_SEEK_FLAG_ACCURATE",
  };
  int i;

  g_value_init (&value, GST_TYPE_SEEK_FLAGS);

  for (i = 0; i < G_N_ELEMENTS (flags); ++i) {
    g_value_set_flags (&value, flags[i]);
    string = gst_value_serialize (&value);
    fail_if (string == NULL, "could not serialize flags %d", i);
    fail_unless (strcmp (string, results[i]) == 0,
        "resulting value is %s, not %s, for flags #%d", string, results[i], i);
    g_free (string);
  }
}

GST_END_TEST;

GST_START_TEST (test_serialize_flags_invalid)
{
  GValue value = { 0 };
  gchar *string;

  g_value_init (&value, GST_TYPE_SEEK_FLAGS);

  /* Invalid value */
  g_value_set_flags (&value, 1 << 20);
  ASSERT_CRITICAL (string = gst_value_serialize (&value));
  fail_if (string == NULL, "could not serialize invalid flags");
  fail_unless (strcmp (string, "0") == 0,
      "resulting value is %s, not 0, for invalid flags", string);
  g_free (string);

  /* Valid & invalid value */
  g_value_set_flags (&value, GST_SEEK_FLAG_FLUSH | 1 << 20);
  ASSERT_CRITICAL (string = gst_value_serialize (&value));
  fail_if (string == NULL, "could not serialize invalid flags");
  fail_unless (strcmp (string, "0") == 0,
      "resulting value is %s, not 0, for invalid flags", string);
  g_free (string);
}

GST_END_TEST;

GST_START_TEST (test_deserialize_flags)
{
  GValue value = { 0 };
  const char *strings[] = {
    "",
    "0",
    "GST_SEEK_FLAG_NONE",
    "GST_SEEK_FLAG_FLUSH",
    "0xf",
    "15",
    "GST_SEEK_FLAG_FLUSH+GST_SEEK_FLAG_ACCURATE",
  };
  GstSeekFlags results[] = {
    GST_SEEK_FLAG_NONE,
    GST_SEEK_FLAG_NONE,
    GST_SEEK_FLAG_NONE,
    GST_SEEK_FLAG_FLUSH,
    0xf,
    15,
    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
  };
  int i;

  g_value_init (&value, GST_TYPE_SEEK_FLAGS);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_flags (&value) == results[i],
        "resulting value is %d, not %d, for string %s (%d)",
        g_value_get_flags (&value), results[i], strings[i], i);
  }

  fail_if (gst_value_deserialize (&value, "foo"),
      "flag deserializing for bogus value should have failed!");
  fail_if (gst_value_deserialize (&value, "GST_SEEK_FLAG_FLUSH+foo"),
      "flag deserializing for bogus value should have failed!");
  fail_if (gst_value_deserialize (&value,
          "GST_SEEK_FLAG_FLUSH+foo+GST_SEEK_FLAG_ACCURATE"),
      "flag deserializing for bogus value should have failed!");
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gtype)
{
  GValue value = { 0 };
  const char *strings[] = {
    "gchararray",
    "gint",
  };
  GType results[] = {
    G_TYPE_STRING,
    G_TYPE_INT,
  };
  int i;

  g_value_init (&value, G_TYPE_GTYPE);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (g_value_get_gtype (&value) == results[i],
        "resulting value is %" G_GSIZE_FORMAT ", not %" G_GSIZE_FORMAT
        ", for string %s (%d)",
        g_value_get_gtype (&value), results[i], strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_gtype_failures)
{
  GValue value = { 0 };
  const char *strings[] = {
    "-",                        /* not a gtype */
  };
  int i;

  g_value_init (&value, G_TYPE_GTYPE);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_if (gst_value_deserialize (&value, strings[i]),
        "deserialized %s (%d), while it should have failed", strings[i], i);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_bitmask)
{
  GValue value = { 0 };
  const char *strings[] = {
    "0xffffffffffffffff",
    "0x1234567890ABCDEF",
  };
  guint64 results[] = {
    0xffffffffffffffffULL,
    0x1234567890ABCDEFULL,
  };
  int i;

  g_value_init (&value, GST_TYPE_BITMASK);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    fail_unless (gst_value_deserialize (&value, strings[i]),
        "could not deserialize %s (%d)", strings[i], i);
    fail_unless (gst_value_get_bitmask (&value) == results[i],
        "resulting value is 0x%016" G_GINT64_MODIFIER "x, not 0x%016"
        G_GINT64_MODIFIER "x, for string %s (%d)",
        gst_value_get_bitmask (&value), results[i], strings[i], i);
  }
}

GST_END_TEST;

static void
check_flagset_mask_serialisation (GValue * value, guint test_flags,
    guint test_mask)
{
  gchar *string;
  gst_value_set_flagset (value, test_flags, test_mask);

  /* Normalise our test flags against the mask now for easier testing,
   * as that's what we expect to get back from the flagset after it
   * normalises internally */
  test_flags &= test_mask;

  /* Check the values got stored correctly */
  fail_unless (gst_value_get_flagset_flags (value) == test_flags,
      "resulting flags value is 0x%u, not 0x%x",
      gst_value_get_flagset_flags (value), test_flags);
  fail_unless (gst_value_get_flagset_mask (value) == test_mask,
      "resulting mask is 0x%u, not 0x%x",
      gst_value_get_flagset_mask (value), test_mask);

  string = gst_value_serialize (value);
  fail_if (string == NULL, "could not serialize flagset");

  GST_DEBUG ("Serialized flagset to: %s", string);

  fail_unless (gst_value_deserialize (value, string),
      "could not deserialize %s", string);

  fail_unless (gst_value_get_flagset_flags (value) == test_flags,
      "resulting flags value is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_flags (value), test_flags, string);

  fail_unless (gst_value_get_flagset_mask (value) == test_mask,
      "resulting mask is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_mask (value), test_mask, string);

  g_free (string);
}

GST_START_TEST (test_flagset)
{
  GValue value = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  GValue dest = G_VALUE_INIT;
  gchar *string;
  GType test_flagset_type;
  guint test_flags, test_mask;

  /* Test serialisation of abstract type */
  g_value_init (&value, GST_TYPE_FLAG_SET);

  test_flags = 0xf1f1;
  test_mask = 0xffff;

  gst_value_set_flagset (&value, test_flags, test_mask);
  string = gst_value_serialize (&value);
  fail_if (string == NULL, "could not serialize flagset");

  fail_unless (gst_value_deserialize (&value, string),
      "could not deserialize %s", string);

  fail_unless (gst_value_get_flagset_flags (&value) == test_flags,
      "resulting value is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_flags (&value), test_flags, string);

  fail_unless (gst_value_get_flagset_mask (&value) == test_mask,
      "resulting value is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_mask (&value), test_mask, string);

  g_free (string);
  g_value_unset (&value);

  /* Check we can't wrap a random non-flags type */
  ASSERT_CRITICAL (gst_flagset_register (GST_TYPE_OBJECT));

  test_flagset_type = gst_flagset_register (GST_TYPE_SEEK_FLAGS);

  fail_unless (g_type_is_a (test_flagset_type, GST_TYPE_FLAG_SET));

  g_value_init (&value, test_flagset_type);

  test_flags =
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE |
      GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
  test_mask =
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE |
      GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;

  check_flagset_mask_serialisation (&value, test_flags, test_mask);
  /* Check serialisation works with the generic 'exact' flag */
  check_flagset_mask_serialisation (&value, test_flags,
      GST_FLAG_SET_MASK_EXACT);

  /* Check deserialisation of flagset in 'flags' form, without
   * the hex strings at the start */
  test_flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE;
  test_mask = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE |
      GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
  string = g_strdup ("+flush+trickmode/trickmode-no-audio");

  fail_unless (gst_value_deserialize (&value, string),
      "could not deserialize %s", string);

  GST_DEBUG ("Deserialized %s to 0x%x:0x%x", string,
      gst_value_get_flagset_flags (&value),
      gst_value_get_flagset_mask (&value));

  fail_unless (gst_value_get_flagset_flags (&value) == test_flags,
      "resulting flags value is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_flags (&value), (test_flags & test_mask), string);

  fail_unless (gst_value_get_flagset_mask (&value) == test_mask,
      "resulting mask is 0x%u, not 0x%x, for string %s",
      gst_value_get_flagset_mask (&value), test_mask, string);

  g_free (string);
  g_value_unset (&value);

  /* Test that fixating don't-care fields works, using our
   * sub-type flagset for good measure  */
  g_value_init (&value, test_flagset_type);
  gst_value_set_flagset (&value, test_flags, test_mask);

  fail_unless (gst_value_fixate (&dest, &value));
  fail_unless (gst_value_get_flagset_flags (&dest) == test_flags);
  fail_unless (gst_value_get_flagset_mask (&dest) == GST_FLAG_SET_MASK_EXACT);

  g_value_unset (&value);

  /* Intersection tests */
  g_value_init (&value, GST_TYPE_FLAG_SET);
  g_value_init (&value2, test_flagset_type);

  /* We want Accurate, but not Snap-Before */
  gst_value_set_flagset (&value, GST_SEEK_FLAG_ACCURATE,
      GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_SNAP_BEFORE);

  /* This only cares that things are flushing */
  gst_value_set_flagset (&value2, GST_SEEK_FLAG_FLUSH, GST_SEEK_FLAG_FLUSH);

  test_flags = GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH;
  test_mask =
      GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SNAP_BEFORE;

  /* GstFlagSet should always intersect with itself */
  g_value_unset (&dest);
  fail_unless (gst_value_can_intersect (&value, &value));
  fail_unless (gst_value_intersect (&dest, &value, &value));

  /* GstFlagSet subtype should intersect with itself */
  g_value_unset (&dest);
  fail_unless (gst_value_can_intersect (&value2, &value2));
  fail_unless (gst_value_intersect (&dest, &value2, &value2));

  /* Check we can intersect custom flagset subtype with flagset */
  g_value_unset (&dest);
  fail_unless (gst_value_can_intersect (&value2, &value));
  fail_unless (gst_value_intersect (&dest, &value2, &value));

  /* and in the other order */
  g_value_unset (&dest);
  fail_unless (gst_value_can_intersect (&value, &value2));
  fail_unless (gst_value_intersect (&dest, &value, &value2));

  fail_unless (gst_value_get_flagset_flags (&dest) == test_flags,
      "resulting flags value is 0x%u, not 0x%x",
      gst_value_get_flagset_flags (&dest), test_flags);

  fail_unless (gst_value_get_flagset_mask (&dest) == test_mask,
      "resulting mask is 0x%u, not 0x%x",
      gst_value_get_flagset_mask (&dest), test_mask);

  gst_value_set_flagset (&value,
      GST_SEEK_FLAG_ACCURATE, GST_SEEK_FLAG_ACCURATE);
  gst_value_set_flagset (&value2, GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH,
      GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_SNAP_BEFORE | GST_SEEK_FLAG_FLUSH);
  /* Check that accurate alone is a subset of accurate+!snap_before+flush,
   * but not vice-versa */
  fail_unless (gst_value_is_subset (&value, &value2));
  fail_if (gst_value_is_subset (&value2, &value));

  g_value_unset (&dest);
  g_value_unset (&value);
  g_value_unset (&value2);

  /* Check that we reject flagset looking string that holds 64 bit integers. */
  g_value_init (&value, GST_TYPE_FLAG_SET);
  string = g_strdup ("AB24:0x0100000000000002");

  fail_if (gst_value_deserialize (&value, string),
      "matched something that isn't a flagset %s", string);

  g_free (string);
  g_value_unset (&value);
}

GST_END_TEST;


GST_START_TEST (test_string)
{
  const gchar *try[] = {
    "Dude",
    "Hi, I'm a string",
    "tüüüt!",
    "\"\""                      /* Empty string */
  };
  gchar *tmp;
  GValue v = { 0, };
  guint i;

  g_value_init (&v, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (try); i++) {
    g_value_set_string (&v, try[i]);
    tmp = gst_value_serialize (&v);
    fail_if (tmp == NULL, "couldn't serialize: %s\n", try[i]);
    fail_unless (gst_value_deserialize (&v, tmp),
        "couldn't deserialize: %s\n", tmp);
    g_free (tmp);

    fail_unless (g_str_equal (g_value_get_string (&v), try[i]),
        "\nserialized  : %s\ndeserialized: %s", try[i],
        g_value_get_string (&v));
  }
  /* NULL strings should not be serializable */
  g_value_set_string (&v, NULL);
  fail_unless (gst_value_serialize (&v) == NULL);
  g_value_unset (&v);
}

GST_END_TEST;

GST_START_TEST (test_deserialize_string)
{
  struct
  {
    const gchar *from;
    const gchar *to;
  } tests[] = {
    {
        "\"foo\"", "foo"}, {
        "\"foo\\%\"", "foo%"}, {
        "\"0123456789_-+/:.\"", "0123456789_-+/:."}, {
        "\"Hello\\ World\"", "Hello World"}, {
        "\"Hello\\ World", "\"Hello\\ World"}, {
        "\"\\", "\"\\"}, {
        "\"\\0", "\"\\0"}, {
        "\"t\\303\\274t\"", "tüt"}, {
          /* utf8 octal sequence */
        "", ""},                /* empty strings */
    {
        "\"\"", ""}, {          /* quoted empty string -> empty string */
        "\" \"", " "}, {        /* allow spaces to be not escaped */
        "tüüt", "tüüt"},    /* allow special chars to be not escaped */
    /* Expected FAILURES: */
    {
        "\"\\0\"", NULL}, {     /* unfinished escaped character */
        "\"", NULL}, {          /* solitary quote */
        "\"\\380\"", NULL}, {   /* invalid octal sequence */
        "\"\\344\\204\\062\"", NULL}, {
          /* invalid utf8: wrong end byte */
        "\"\\344\\204\"", NULL} /* invalid utf8: wrong number of bytes */
  };
  guint i;
  GValue v = { 0, };

  g_value_init (&v, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (tests); i++) {
    if (gst_value_deserialize (&v, tests[i].from)) {
      fail_if (tests[i].to == NULL,
          "I got %s instead of a failure", g_value_get_string (&v));
      fail_unless (g_str_equal (g_value_get_string (&v), tests[i].to),
          "\nwanted: %s\ngot    : %s", tests[i].to, g_value_get_string (&v));
    } else {
      fail_if (tests[i].to != NULL, "failed, but wanted: %s", tests[i].to);
    }
  }
  g_value_unset (&v);
}

GST_END_TEST;

GST_START_TEST (test_value_compare)
{
  GValue value1 = { 0 };
  GValue value2 = { 0 };
  GValue tmp = { 0 };
  GstAllocationParams alloc_params = { 0 };

  g_value_init (&value1, G_TYPE_INT);
  g_value_set_int (&value1, 10);
  g_value_init (&value2, G_TYPE_INT);
  g_value_set_int (&value2, 20);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  g_value_init (&value1, G_TYPE_DOUBLE);
  g_value_set_double (&value1, 10);
  g_value_init (&value2, G_TYPE_DOUBLE);
  g_value_set_double (&value2, 20);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  g_value_init (&value1, G_TYPE_STRING);
  g_value_set_string (&value1, "a");
  g_value_init (&value2, G_TYPE_STRING);
  g_value_set_string (&value2, "b");
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  /* Test some NULL string comparisons */
  g_value_set_string (&value2, NULL);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value2, &value2) == GST_VALUE_EQUAL);

  g_value_unset (&value1);
  g_value_unset (&value2);

  /* comparing 2/3 with 3/4 */
  g_value_init (&value1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value1, 2, 3);
  g_value_init (&value2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value2, 3, 4);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* comparing -4/5 with 2/-3 */
  g_value_init (&value1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value1, -4, 5);
  g_value_init (&value2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value2, 2, -3);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* comparing 10/100 with 200/2000 */
  g_value_init (&value1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value1, 10, 100);
  g_value_init (&value2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value2, 200, 2000);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* comparing -4/5 with 2/-3 */
  g_value_init (&value1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value1, -4, 5);
  g_value_init (&value2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value2, 2, -3);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_LESS_THAN);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_GREATER_THAN);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* Check that lists are equal regardless of order */
  g_value_init (&value1, GST_TYPE_LIST);
  g_value_init (&tmp, G_TYPE_INT);
  g_value_set_int (&tmp, 1);
  gst_value_list_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 2);
  gst_value_list_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 3);
  gst_value_list_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 4);
  gst_value_list_append_value (&value1, &tmp);

  g_value_init (&value2, GST_TYPE_LIST);
  g_value_set_int (&tmp, 4);
  gst_value_list_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 3);
  gst_value_list_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 2);
  gst_value_list_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 1);
  gst_value_list_append_value (&value2, &tmp);

  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "value lists with different order were not equal when they should be");
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL,
      "value lists with same order were not equal when they should be");
  fail_unless (gst_value_compare (&value2, &value2) == GST_VALUE_EQUAL,
      "value lists with same order were not equal when they should be");

  /* Carry over the lists to this next check: */
  /* Lists with different sizes are unequal */
  g_value_set_int (&tmp, 1);
  gst_value_list_append_value (&value2, &tmp);

  fail_if (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "Value lists with different size were equal when they shouldn't be");

  /* Carry over the lists to this next check: */
  /* Lists with same size but list1 contains one more element not in list2 */
  g_value_set_int (&tmp, 5);
  gst_value_list_append_value (&value1, &tmp);

  fail_if (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "Value lists with different elements were equal when they shouldn't be");
  fail_if (gst_value_compare (&value2, &value1) == GST_VALUE_EQUAL,
      "Value lists with different elements were equal when they shouldn't be");

  g_value_unset (&value1);
  g_value_unset (&value2);
  g_value_unset (&tmp);

  /* Arrays are only equal when in the same order */
  g_value_init (&value1, GST_TYPE_ARRAY);
  g_value_init (&tmp, G_TYPE_INT);
  g_value_set_int (&tmp, 1);
  gst_value_array_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 2);
  gst_value_array_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 3);
  gst_value_array_append_value (&value1, &tmp);
  g_value_set_int (&tmp, 4);
  gst_value_array_append_value (&value1, &tmp);

  g_value_init (&value2, GST_TYPE_ARRAY);
  g_value_set_int (&tmp, 4);
  gst_value_array_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 3);
  gst_value_array_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 2);
  gst_value_array_append_value (&value2, &tmp);
  g_value_set_int (&tmp, 1);
  gst_value_array_append_value (&value2, &tmp);

  fail_if (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "Value arrays with different order were equal when they shouldn't be");
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL,
      "Identical value arrays were not equal when they should be");
  fail_unless (gst_value_compare (&value2, &value2) == GST_VALUE_EQUAL,
      "Identical value arrays were not equal when they should be");

  /* Carry over the arrays to this next check: */
  /* Arrays with different sizes are unequal */
  g_value_unset (&value2);
  g_value_init (&value2, GST_TYPE_ARRAY);
  g_value_copy (&value1, &value2);

  g_value_set_int (&tmp, 1);
  gst_value_array_append_value (&value2, &tmp);

  fail_if (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "Value arrays with different size were equal when they shouldn't be");
  /* order should not matter */
  fail_if (gst_value_compare (&value2, &value1) == GST_VALUE_EQUAL,
      "Value arrays with different size were equal when they shouldn't be");

  g_value_unset (&value1);
  g_value_unset (&value2);
  g_value_unset (&tmp);

  g_value_init (&value1, G_TYPE_VALUE_ARRAY);
  g_value_init (&value2, G_TYPE_VALUE_ARRAY);

  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL,
      "Empty Value arrays aren't equals when they should");

  g_value_unset (&value1);
  g_value_unset (&value2);

  g_value_init (&value1, GST_TYPE_BITMASK);
  gst_value_set_bitmask (&value1, 0x123);
  g_value_init (&value2, GST_TYPE_BITMASK);
  gst_value_set_bitmask (&value2, 0x321);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value2, &value1) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* Check that we can compare objects */
  g_value_init (&value1, GST_TYPE_BIN);
  g_value_take_object (&value1, gst_bin_new (NULL));
  g_value_init (&value2, GST_TYPE_BIN);
  g_value_take_object (&value2, gst_bin_new (NULL));
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* Check that we can compare allocation params */
  g_value_init (&value1, GST_TYPE_ALLOCATION_PARAMS);
  g_value_set_boxed (&value1, &alloc_params);
  g_value_init (&value2, GST_TYPE_ALLOCATION_PARAMS);
  alloc_params.align = 1;
  g_value_set_boxed (&value2, &alloc_params);
  fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_UNORDERED);
  fail_unless (gst_value_compare (&value1, &value1) == GST_VALUE_EQUAL);
  g_value_unset (&value1);
  g_value_unset (&value2);

  /* Check that we can compare structure */
  {
    GstStructure *s = gst_structure_new_empty ("test");

    g_value_init (&value1, GST_TYPE_STRUCTURE);
    g_value_init (&value2, GST_TYPE_STRUCTURE);
    fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL);

    gst_value_set_structure (&value1, s);
    fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_UNORDERED);
    gst_value_set_structure (&value2, s);
    fail_unless (gst_value_compare (&value1, &value2) == GST_VALUE_EQUAL);
    g_value_unset (&value1);
    g_value_unset (&value2);
    gst_structure_free (s);
  }
}

GST_END_TEST;

GST_START_TEST (test_value_intersect)
{
  GValue dest = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };
  GValue item = { 0 };
  gboolean ret;

  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 10);
  g_value_init (&src2, G_TYPE_INT);
  g_value_set_int (&src2, 20);
  ret = gst_value_intersect (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  g_value_init (&src1, G_TYPE_STRING);
  g_value_set_static_string (&src1, "YUY2");
  g_value_init (&src2, GST_TYPE_LIST);
  g_value_init (&item, G_TYPE_STRING);
  g_value_set_static_string (&item, "YUY2");
  gst_value_list_append_value (&src2, &item);
  g_value_set_static_string (&item, "I420");
  gst_value_list_append_value (&src2, &item);
  g_value_set_static_string (&item, "ABCD");
  gst_value_list_append_value (&src2, &item);

  fail_unless (gst_value_intersect (&dest, &src1, &src2));
  fail_unless (G_VALUE_HOLDS_STRING (&dest));
  fail_unless (g_str_equal (g_value_get_string (&dest), "YUY2"));

  g_value_unset (&src1);
  g_value_unset (&src2);
  g_value_unset (&dest);
}

GST_END_TEST;


GST_START_TEST (test_value_subtract_int)
{
  GValue dest = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };
  const GValue *tmp;
  gboolean ret;

  /*  int <-> int
   */
  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 10);
  g_value_init (&src2, G_TYPE_INT);
  g_value_set_int (&src2, 20);
  /* subtract as in sets, result is 10 */
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* same values, yields empty set */
  ret = gst_value_subtract (&dest, &src1, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  int <-> int_range
   */

  /* would yield an empty set */
  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 10);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 0, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a list of two ranges. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 0);
  fail_unless (gst_value_get_int_range_max (tmp) == 9);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 11);
  fail_unless (gst_value_get_int_range_max (tmp) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 1, empty set */
  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 10);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a new range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 11);
  fail_unless (gst_value_get_int_range_max (&dest) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 2, empty set */
  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 20);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a new range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 10);
  fail_unless (gst_value_get_int_range_max (&dest) == 19);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* case 3, valid set */
  g_value_init (&src1, G_TYPE_INT);
  g_value_set_int (&src1, 0);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_HOLDS_INT (&dest) == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* and the other way around, should keep the range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 10);
  fail_unless (gst_value_get_int_range_max (&dest) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  int_range <-> int_range
   */

  /* same range, empty set */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* non overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 30, 40);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 10);
  fail_unless (gst_value_get_int_range_max (&dest) == 20);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 30);
  fail_unless (gst_value_get_int_range_max (&dest) == 40);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* completely overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 10, 30);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 21);
  fail_unless (gst_value_get_int_range_max (&dest) == 30);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* partially overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 15, 30);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 10);
  fail_unless (gst_value_get_int_range_max (&dest) == 14);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);
  fail_unless (gst_value_get_int_range_min (&dest) == 21);
  fail_unless (gst_value_get_int_range_max (&dest) == 30);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole { int_range, int_range } */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 15, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 10);
  fail_unless (gst_value_get_int_range_max (tmp) == 14);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 21);
  fail_unless (gst_value_get_int_range_max (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int, int } */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 11, 29);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (G_VALUE_HOLDS_INT (tmp) == TRUE);
  fail_unless (g_value_get_int (tmp) == 10);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (G_VALUE_HOLDS_INT (tmp) == TRUE);
  fail_unless (g_value_get_int (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int, int_range } */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 11, 28);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (G_VALUE_HOLDS_INT (tmp) == TRUE);
  fail_unless (g_value_get_int (tmp) == 10);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 29);
  fail_unless (gst_value_get_int_range_max (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int_range, int } */
  g_value_init (&src1, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (&src2, 12, 29);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int_range_min (tmp) == 10);
  fail_unless (gst_value_get_int_range_max (tmp) == 11);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (G_VALUE_HOLDS_INT (tmp) == TRUE);
  fail_unless (g_value_get_int (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);
}

GST_END_TEST;

GST_START_TEST (test_value_subtract_int64)
{
  GValue dest = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };
  const GValue *tmp;
  gboolean ret;

  /*  int64 <-> int64
   */
  g_value_init (&src1, G_TYPE_INT64);
  g_value_set_int64 (&src1, 10);
  g_value_init (&src2, G_TYPE_INT64);
  g_value_set_int64 (&src2, 20);
  /* subtract as in sets, result is 10 */
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* same values, yields empty set */
  ret = gst_value_subtract (&dest, &src1, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  int64 <-> int64_range
   */

  /* would yield an empty set */
  g_value_init (&src1, G_TYPE_INT64);
  g_value_set_int64 (&src1, 10);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 0, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a list of two ranges. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 0);
  fail_unless (gst_value_get_int64_range_max (tmp) == 9);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 11);
  fail_unless (gst_value_get_int64_range_max (tmp) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 1, empty set */
  g_value_init (&src1, G_TYPE_INT64);
  g_value_set_int64 (&src1, 10);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a new range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 11);
  fail_unless (gst_value_get_int64_range_max (&dest) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 2, empty set */
  g_value_init (&src1, G_TYPE_INT64);
  g_value_set_int64 (&src1, 20);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should create a new range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 10);
  fail_unless (gst_value_get_int64_range_max (&dest) == 19);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* case 3, valid set */
  g_value_init (&src1, G_TYPE_INT64);
  g_value_set_int64 (&src1, 0);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_HOLDS_INT64 (&dest) == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* and the other way around, should keep the range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 10);
  fail_unless (gst_value_get_int64_range_max (&dest) == 20);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  int64_range <-> int64_range
   */

  /* same range, empty set */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 10, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* non overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 30, 40);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 10);
  fail_unless (gst_value_get_int64_range_max (&dest) == 20);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 30);
  fail_unless (gst_value_get_int64_range_max (&dest) == 40);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* completely overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 10, 30);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 21);
  fail_unless (gst_value_get_int64_range_max (&dest) == 30);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* partially overlapping ranges */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 20);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 15, 30);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 10);
  fail_unless (gst_value_get_int64_range_max (&dest) == 14);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);
  fail_unless (gst_value_get_int64_range_min (&dest) == 21);
  fail_unless (gst_value_get_int64_range_max (&dest) == 30);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole { int64_range, int64_range } */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 15, 20);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 10);
  fail_unless (gst_value_get_int64_range_max (tmp) == 14);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 21);
  fail_unless (gst_value_get_int64_range_max (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int64, int64 } */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 11, 29);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (G_VALUE_HOLDS_INT64 (tmp) == TRUE);
  fail_unless (g_value_get_int64 (tmp) == 10);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (G_VALUE_HOLDS_INT64 (tmp) == TRUE);
  fail_unless (g_value_get_int64 (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int64, int64_range } */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 11, 28);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (G_VALUE_HOLDS_INT64 (tmp) == TRUE);
  fail_unless (g_value_get_int64 (tmp) == 10);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 29);
  fail_unless (gst_value_get_int64_range_max (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole, { int64_range, int64 } */
  g_value_init (&src1, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src1, 10, 30);
  g_value_init (&src2, GST_TYPE_INT64_RANGE);
  gst_value_set_int64_range (&src2, 12, 29);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_int64_range_min (tmp) == 10);
  fail_unless (gst_value_get_int64_range_max (tmp) == 11);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (G_VALUE_HOLDS_INT64 (tmp) == TRUE);
  fail_unless (g_value_get_int64 (tmp) == 30);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);
}

GST_END_TEST;

GST_START_TEST (test_value_subtract_double)
{
  GValue dest = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };
  const GValue *tmp;
  gboolean ret;

  /*  double <-> double
   */
  g_value_init (&src1, G_TYPE_DOUBLE);
  g_value_set_double (&src1, 10.0);
  g_value_init (&src2, G_TYPE_DOUBLE);
  g_value_set_double (&src2, 20.0);
  /* subtract as in sets, result is 10 */
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* same values, yields empty set */
  ret = gst_value_subtract (&dest, &src1, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  double <-> double_range
   */

  /* would yield an empty set */
  g_value_init (&src1, G_TYPE_DOUBLE);
  g_value_set_double (&src1, 10.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 0.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, we cannot create open ranges
   * so the result is the range again */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 0.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 20.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 1, empty set */
  g_value_init (&src1, G_TYPE_DOUBLE);
  g_value_set_double (&src1, 10.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should keep same range as
   * we don't have open ranges. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 10.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 20.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 2, empty set */
  g_value_init (&src1, G_TYPE_DOUBLE);
  g_value_set_double (&src1, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should keep same range as
   * we don't have open ranges. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 10.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 20.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* case 3, valid set */
  g_value_init (&src1, G_TYPE_DOUBLE);
  g_value_set_double (&src1, 0.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_HOLDS_DOUBLE (&dest) == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* and the other way around, should keep the range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 10.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 20.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  double_range <-> double_range
   */

  /* Check equality */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 15.0);
  /* They are not equal (higher bound is different */
  fail_if (gst_value_compare (&src1, &src2) == GST_VALUE_EQUAL);
  g_value_unset (&src1);
  /* They are not equal (lower bound is different */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 5.0, 15.0);
  fail_if (gst_value_compare (&src1, &src2) == GST_VALUE_EQUAL);
  g_value_unset (&src1);
  /* And finally check equality */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 15.0);
  fail_unless (gst_value_compare (&src1, &src2) == GST_VALUE_EQUAL);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* same range, empty set */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* non overlapping ranges */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 30.0, 40.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 10.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 20.0);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 30.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 40.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* completely overlapping ranges */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 10.0, 30.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 20.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 30.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* partially overlapping ranges */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 20.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 15.0, 30.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 10.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 15.0);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_DOUBLE_RANGE);
  fail_unless (gst_value_get_double_range_min (&dest) == 20.0);
  fail_unless (gst_value_get_double_range_max (&dest) == 30.0);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole { double_range, double_range } */
  g_value_init (&src1, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src1, 10.0, 30.0);
  g_value_init (&src2, GST_TYPE_DOUBLE_RANGE);
  gst_value_set_double_range (&src2, 15.0, 20.0);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_DOUBLE_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_double_range_min (tmp) == 10.0);
  fail_unless (gst_value_get_double_range_max (tmp) == 15.0);
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_DOUBLE_RANGE (tmp) == TRUE);
  fail_unless (gst_value_get_double_range_min (tmp) == 20.0);
  fail_unless (gst_value_get_double_range_max (tmp) == 30.0);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);
}

GST_END_TEST;

/* Test arithmetic subtraction of fractions */
GST_START_TEST (test_value_subtract_fraction)
{
  GValue result = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };

  /* Subtract 1/4 from 1/2 */
  g_value_init (&src1, GST_TYPE_FRACTION);
  g_value_init (&src2, GST_TYPE_FRACTION);
  g_value_init (&result, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 1, 2);
  gst_value_set_fraction (&src2, 1, 4);
  fail_unless (gst_value_fraction_subtract (&result, &src1, &src2) == TRUE);
  fail_unless (gst_value_get_fraction_numerator (&result) == 1);
  fail_unless (gst_value_get_fraction_denominator (&result) == 4);

  g_value_unset (&src1);
  g_value_unset (&src2);
  g_value_unset (&result);

  /* Subtract 1/12 from 7/8 */
  g_value_init (&src1, GST_TYPE_FRACTION);
  g_value_init (&src2, GST_TYPE_FRACTION);
  g_value_init (&result, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 7, 8);
  gst_value_set_fraction (&src2, 1, 12);
  fail_unless (gst_value_fraction_subtract (&result, &src1, &src2) == TRUE);
  fail_unless (gst_value_get_fraction_numerator (&result) == 19);
  fail_unless (gst_value_get_fraction_denominator (&result) == 24);

  g_value_unset (&src1);
  g_value_unset (&src2);
  g_value_unset (&result);

  /* Subtract 12/13 from 4/3 */
  g_value_init (&src1, GST_TYPE_FRACTION);
  g_value_init (&src2, GST_TYPE_FRACTION);
  g_value_init (&result, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 4, 3);
  gst_value_set_fraction (&src2, 12, 13);
  fail_unless (gst_value_fraction_subtract (&result, &src1, &src2) == TRUE);
  fail_unless (gst_value_get_fraction_numerator (&result) == 16);
  fail_unless (gst_value_get_fraction_denominator (&result) == 39);

  g_value_unset (&src1);
  g_value_unset (&src2);
  g_value_unset (&result);

  /* Subtract 1/12 from 7/8 */
}

GST_END_TEST;

/* Test set subtraction operations on fraction ranges */
GST_START_TEST (test_value_subtract_fraction_range)
{
  GValue dest = { 0 };
  GValue src1 = { 0 };
  GValue src2 = { 0 };
  GValue cmp = { 0 };
  const GValue *tmp;
  gboolean ret;

  /* Value for tests */
  g_value_init (&cmp, GST_TYPE_FRACTION);

  /*  fraction <-> fraction
   */
  g_value_init (&src1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 10, 1);
  g_value_init (&src2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src2, 20, 1);
  gst_value_set_fraction (&src1, 10, 1);

  /* subtract as in sets, result is 10 */
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* same values, yields empty set */
  ret = gst_value_subtract (&dest, &src1, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  fraction <-> fraction_range
   */

  /* would yield an empty set */
  g_value_init (&src1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 10, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 0, 1, 20, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, we cannot create open ranges
   * so the result is the range again */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 0, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* border case 1, empty set */
  g_value_init (&src1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 10, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 10, 1, 20, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);

  /* and the other way around, should keep same range as
   * we don't have open ranges. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 10, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* case 2, valid set */
  g_value_init (&src1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&src1, 0, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 10, 1, 20, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION);
  fail_unless (gst_value_compare (&dest, &src1) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* and the other way around, should keep the range. */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  fail_unless (gst_value_compare (&dest, &src2) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /*  fraction_range <-> fraction_range
   */

  /* same range, empty set */
  g_value_init (&src1, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src1, 10, 2, 20, 2);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 10, 2, 20, 2);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* non overlapping ranges */
  g_value_init (&src1, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src1, 10, 2, 10, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 30, 2, 40, 2);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 5, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 10, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);

  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 15, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* completely overlapping ranges */
  g_value_init (&src1, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src1, 10, 1, 20, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 10, 1, 30, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == FALSE);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 30, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* partially overlapping ranges */
  g_value_init (&src1, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src1, 10, 1, 20, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 15, 1, 30, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 10, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 15, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (&dest),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 30, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (&dest),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  g_value_unset (&src1);
  g_value_unset (&src2);

  /* create a hole { double_range, double_range } */
  g_value_init (&src1, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src1, 10, 1, 30, 1);
  g_value_init (&src2, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (&src2, 15, 1, 20, 1);
  ret = gst_value_subtract (&dest, &src1, &src2);
  fail_unless (ret == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_LIST);
  /* 1st list entry */
  tmp = gst_value_list_get_value (&dest, 0);
  fail_unless (GST_VALUE_HOLDS_FRACTION_RANGE (tmp) == TRUE);
  gst_value_set_fraction (&cmp, 10, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (tmp),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 15, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (tmp),
          &cmp) == GST_VALUE_EQUAL);
  /* 2nd list entry */
  tmp = gst_value_list_get_value (&dest, 1);
  fail_unless (GST_VALUE_HOLDS_FRACTION_RANGE (tmp) == TRUE);
  gst_value_set_fraction (&cmp, 20, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_min (tmp),
          &cmp) == GST_VALUE_EQUAL);
  gst_value_set_fraction (&cmp, 30, 1);
  fail_unless (gst_value_compare (gst_value_get_fraction_range_max (tmp),
          &cmp) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  /* the other way */
  ret = gst_value_subtract (&dest, &src2, &src1);
  fail_unless (ret == FALSE);
  g_value_unset (&src1);
  g_value_unset (&src2);

  g_value_unset (&cmp);
}

GST_END_TEST;

/* Test set subtraction operations on fraction lists */
GST_START_TEST (test_value_subtract_fraction_list)
{
  GValue list1 = { 0 };
  GValue list2 = { 0 };
  GValue val1 = { 0 };
  GValue val2 = { 0 };
  GValue tmp = { 0 };
  gboolean ret;

  g_value_init (&list1, GST_TYPE_LIST);
  g_value_init (&val1, GST_TYPE_FRACTION);
  gst_value_set_fraction (&val1, 15, 2);
  gst_value_list_append_value (&list1, &val1);
  g_value_init (&tmp, GST_TYPE_FRACTION);
  gst_value_set_fraction (&tmp, 5, 1);
  gst_value_list_append_value (&list1, &tmp);
  g_value_unset (&tmp);

  g_value_init (&list2, GST_TYPE_LIST);
  g_value_init (&val2, GST_TYPE_FRACTION);
  gst_value_set_fraction (&val2, 15, 1);
  gst_value_list_append_value (&list2, &val2);
  g_value_init (&tmp, GST_TYPE_FRACTION);
  gst_value_set_fraction (&tmp, 5, 1);
  gst_value_list_append_value (&list2, &tmp);
  g_value_unset (&tmp);

  /* should subtract all common elements */
  ret = gst_value_subtract (&tmp, &list1, &list2);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&tmp, &val1) == GST_VALUE_EQUAL);
  g_value_unset (&val1);
  g_value_unset (&tmp);

  ret = gst_value_subtract (&tmp, &list2, &list1);
  fail_unless (ret == TRUE);
  fail_unless (gst_value_compare (&tmp, &val2) == GST_VALUE_EQUAL);
  g_value_unset (&val2);
  g_value_unset (&tmp);

  g_value_unset (&list1);
  g_value_unset (&list2);
}

GST_END_TEST;

GST_START_TEST (test_date)
{
  GstStructure *s;
  GDate *date, *date2;
  gchar *str;

  date = g_date_new_dmy (22, 9, 2005);

  s = gst_structure_new ("media/x-type", "SOME_DATE_TAG", G_TYPE_DATE,
      date, NULL);

  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TAG", G_TYPE_DATE));
  fail_unless (gst_structure_get_date (s, "SOME_DATE_TAG", &date2));
  fail_unless (date2 != NULL);
  fail_unless (g_date_valid (date2));
  fail_unless (g_date_compare (date, date2) == 0);

  g_date_free (date);
  g_date_free (date2);
  date = NULL;
  date2 = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless (g_str_equal (str,
          "media/x-type, SOME_DATE_TAG=(date)2005-09-22;"));

  s = gst_structure_from_string (str, NULL);
  g_free (str);
  str = NULL;

  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "media/x-type"));
  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TAG", G_TYPE_DATE));
  fail_unless (gst_structure_get_date (s, "SOME_DATE_TAG", &date));
  fail_unless (date != NULL);
  fail_unless (g_date_valid (date));
  fail_unless (g_date_get_day (date) == 22);
  fail_unless (g_date_get_month (date) == 9);
  fail_unless (g_date_get_year (date) == 2005);
  g_date_free (date);
  date = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless (g_str_equal (str,
          "media/x-type, SOME_DATE_TAG=(date)2005-09-22;"));
  g_free (str);
  str = NULL;
}

GST_END_TEST;

static gboolean
date_time_equal (GstDateTime * a, GstDateTime * b)
{
  if (gst_date_time_get_year (a) != gst_date_time_get_year (b) ||
      gst_date_time_get_month (a) != gst_date_time_get_month (b) ||
      gst_date_time_get_day (a) != gst_date_time_get_day (b))
    return FALSE;

  if (gst_date_time_get_hour (a) != gst_date_time_get_hour (b) ||
      gst_date_time_get_minute (a) != gst_date_time_get_minute (b) ||
      gst_date_time_get_second (a) != gst_date_time_get_second (b) ||
      gst_date_time_get_microsecond (a) != gst_date_time_get_microsecond (b))
    return FALSE;

  if (gst_date_time_get_time_zone_offset (a) !=
      gst_date_time_get_time_zone_offset (b))
    return FALSE;

  return TRUE;
}

GST_START_TEST (test_date_time)
{
  GstStructure *s;
  GstDateTime *datetime, *datetime2;
  GValue val = { 0, };
  gchar *str;

  /* utc timezone */
  datetime = gst_date_time_new (0, 2010, 6, 23, 7, 40, 10);

  s = gst_structure_new ("media/x-type", "SOME_DATE_TIME_TAG",
      GST_TYPE_DATE_TIME, datetime, NULL);

  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime2));
  fail_unless (datetime2 != NULL);
  fail_unless (date_time_equal (datetime, datetime2));

  gst_date_time_unref (datetime);
  gst_date_time_unref (datetime2);
  datetime = NULL;
  datetime2 = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10Z;");

  s = gst_structure_from_string (str, NULL);
  g_free (str);
  str = NULL;

  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "media/x-type"));
  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime));
  fail_unless (datetime != NULL);
  fail_unless (gst_date_time_get_year (datetime) == 2010);
  fail_unless (gst_date_time_get_month (datetime) == 6);
  fail_unless (gst_date_time_get_day (datetime) == 23);
  fail_unless (gst_date_time_get_hour (datetime) == 7);
  fail_unless (gst_date_time_get_minute (datetime) == 40);
  fail_unless (gst_date_time_get_second (datetime) == 10);
  fail_unless (gst_date_time_get_microsecond (datetime) == 0);
  fail_unless (gst_date_time_get_time_zone_offset (datetime) == 0);
  gst_date_time_unref (datetime);
  datetime = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10Z;");
  g_free (str);
  str = NULL;

  /* with timezone */
  datetime = gst_date_time_new (-3.0, 2010, 6, 23, 7, 40, 10.000001);

  s = gst_structure_new ("media/x-type", "SOME_DATE_TIME_TAG",
      GST_TYPE_DATE_TIME, datetime, NULL);

  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime2));
  fail_unless (datetime2 != NULL);
  fail_unless (date_time_equal (datetime, datetime2));

  gst_date_time_unref (datetime);
  gst_date_time_unref (datetime2);
  datetime = NULL;
  datetime2 = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10.000001-0300;");

  s = gst_structure_from_string (str, NULL);
  g_free (str);
  str = NULL;

  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "media/x-type"));
  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime));
  fail_unless (datetime != NULL);
  fail_unless (gst_date_time_get_year (datetime) == 2010);
  fail_unless (gst_date_time_get_month (datetime) == 6);
  fail_unless (gst_date_time_get_day (datetime) == 23);
  fail_unless (gst_date_time_get_hour (datetime) == 7);
  fail_unless (gst_date_time_get_minute (datetime) == 40);
  fail_unless (gst_date_time_get_second (datetime) == 10);
  fail_unless (gst_date_time_get_microsecond (datetime) == 1);
  fail_unless (gst_date_time_get_time_zone_offset (datetime) == -3);
  gst_date_time_unref (datetime);
  datetime = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;
  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10.000001-0300;");

  g_free (str);
  str = NULL;

  /* with positive timezone */
  datetime = gst_date_time_new (2.0, 2010, 6, 23, 7, 40, 10.000001);

  s = gst_structure_new ("media/x-type", "SOME_DATE_TIME_TAG",
      GST_TYPE_DATE_TIME, datetime, NULL);

  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime2));
  fail_unless (datetime2 != NULL);
  fail_unless (date_time_equal (datetime, datetime2));

  gst_date_time_unref (datetime);
  gst_date_time_unref (datetime2);
  datetime = NULL;
  datetime2 = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;

  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10.000001+0200;");

  s = gst_structure_from_string (str, NULL);
  g_free (str);
  str = NULL;

  fail_unless (s != NULL);
  fail_unless (gst_structure_has_name (s, "media/x-type"));
  fail_unless (gst_structure_has_field_typed (s, "SOME_DATE_TIME_TAG",
          GST_TYPE_DATE_TIME));
  fail_unless (gst_structure_get_date_time (s, "SOME_DATE_TIME_TAG",
          &datetime));
  fail_unless (datetime != NULL);
  fail_unless (gst_date_time_get_year (datetime) == 2010);
  fail_unless (gst_date_time_get_month (datetime) == 6);
  fail_unless (gst_date_time_get_day (datetime) == 23);
  fail_unless (gst_date_time_get_hour (datetime) == 7);
  fail_unless (gst_date_time_get_minute (datetime) == 40);
  fail_unless (gst_date_time_get_second (datetime) == 10);
  fail_unless (gst_date_time_get_microsecond (datetime) == 1);
  fail_unless (gst_date_time_get_time_zone_offset (datetime) == 2);
  gst_date_time_unref (datetime);
  datetime = NULL;

  str = gst_structure_to_string (s);
  gst_structure_free (s);
  s = NULL;
  fail_unless_equals_string (str,
      "media/x-type, SOME_DATE_TIME_TAG=(datetime)2010-06-23T07:40:10.000001+0200;");

  g_free (str);
  str = NULL;

  /* test partial dates */
  datetime = gst_date_time_new (0.0, 2010, -1, -1, -1, -1, -1.0);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "2010");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (gst_date_time_has_month (datetime));
  fail_if (gst_date_time_has_day (datetime));
  fail_if (gst_date_time_has_time (datetime));
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (0.0, 2010, 9, -1, -1, -1, -1.0);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "2010-09");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (gst_date_time_has_day (datetime));
  fail_if (gst_date_time_has_time (datetime));
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (0.0, 1983, 11, 30, -1, -1, -1.0);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "1983-11-30");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (!gst_date_time_has_day (datetime));
  fail_if (gst_date_time_has_time (datetime));
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (0.0, 1983, 11, 30, 3, 52, -1.0);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "1983-11-30T03:52Z");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (!gst_date_time_has_day (datetime));
  fail_if (!gst_date_time_has_time (datetime));
  fail_if (gst_date_time_has_second (datetime));
  fail_unless_equals_float (gst_date_time_get_time_zone_offset (datetime), 0.0);
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (-4.5, 1983, 11, 30, 3, 52, -1.0);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "1983-11-30T03:52-0430");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (!gst_date_time_has_day (datetime));
  fail_if (!gst_date_time_has_time (datetime));
  fail_if (gst_date_time_has_second (datetime));
  fail_unless_equals_float (gst_date_time_get_time_zone_offset (datetime),
      -4.5);
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (4.5, 1983, 11, 30, 14, 52, 9);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "1983-11-30T14:52:09+0430");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (!gst_date_time_has_day (datetime));
  fail_if (!gst_date_time_has_time (datetime));
  fail_if (!gst_date_time_has_second (datetime));
  fail_unless_equals_float (gst_date_time_get_time_zone_offset (datetime), 4.5);
  g_value_unset (&val);
  g_free (str);

  datetime = gst_date_time_new (-4.5, 1983, 11, 30, 14, 52, 9.702);
  g_value_init (&val, GST_TYPE_DATE_TIME);
  g_value_take_boxed (&val, datetime);
  str = gst_value_serialize (&val);
  g_value_reset (&val);
  fail_unless_equals_string (str, "1983-11-30T14:52:09.702-0430");
  fail_unless (gst_value_deserialize (&val, str));
  datetime = g_value_get_boxed (&val);
  fail_if (!gst_date_time_has_year (datetime));
  fail_if (!gst_date_time_has_month (datetime));
  fail_if (!gst_date_time_has_day (datetime));
  fail_if (!gst_date_time_has_time (datetime));
  fail_if (!gst_date_time_has_second (datetime));
  fail_unless_equals_float (gst_date_time_get_time_zone_offset (datetime),
      -4.5);
  g_value_unset (&val);
  g_free (str);
}

GST_END_TEST;

GST_START_TEST (test_fraction_range)
{
  GValue range = { 0, };
  GValue start = { 0, };
  GValue end = { 0, };
  GValue src = { 0, };
  GValue dest = { 0, };
  GValue range2 = { 0, };

  g_value_init (&range, GST_TYPE_FRACTION_RANGE);
  g_value_init (&range2, GST_TYPE_FRACTION_RANGE);
  g_value_init (&start, GST_TYPE_FRACTION);
  g_value_init (&end, GST_TYPE_FRACTION);
  g_value_init (&src, GST_TYPE_FRACTION);

  gst_value_set_fraction (&src, 1, 2);

  /* Check that a intersection of fraction & range = fraction */
  gst_value_set_fraction (&start, 1, 4);
  gst_value_set_fraction (&end, 2, 3);
  gst_value_set_fraction_range (&range, &start, &end);

  fail_unless (gst_value_intersect (&dest, &src, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION);
  fail_unless (gst_value_compare (&dest, &src) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* Check that union of fraction + range = range */
  fail_unless (gst_value_union (&dest, &src, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  fail_unless (gst_value_compare (&dest, &range) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* Check that union of fraction that is not in the range fails */
  gst_value_set_fraction (&src, 1, 20);
  fail_unless (gst_value_union (&dest, &src, &range) == FALSE);
  g_value_unset (&dest);

  /* Check that a intersection selects the overlapping range */
  gst_value_set_fraction (&start, 1, 3);
  gst_value_set_fraction (&end, 2, 3);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  fail_unless (gst_value_compare (&dest, &range2) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* Fully enclosed union: [1/4, 2/3] ∪ [1/3, 2/3] = [1/4, 2/3] */
  fail_unless (gst_value_union (&dest, &range, &range2) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  fail_unless (gst_value_compare (&dest, &range) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  /* Same, but swapped args */
  fail_unless (gst_value_union (&dest, &range2, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  fail_unless (gst_value_compare (&dest, &range) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* Extend union: [1/5, 1/2] ∪ [1/3, 2/3] = [1/5, 2/3] */
  gst_value_set_fraction (&start, 1, 5);
  gst_value_set_fraction (&end, 1, 2);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_union (&dest, &range, &range2) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&start, 1, 5);
  gst_value_set_fraction (&end, 2, 3);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_compare (&dest, &range2) == GST_VALUE_EQUAL);
  g_value_unset (&dest);
  /* Same, but swapped args */
  gst_value_set_fraction (&start, 1, 5);
  gst_value_set_fraction (&end, 1, 2);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_union (&dest, &range2, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction (&start, 1, 5);
  gst_value_set_fraction (&end, 2, 3);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_compare (&dest, &range2) == GST_VALUE_EQUAL);
  g_value_unset (&dest);

  /* Check that non intersection ranges don't intersect */
  gst_value_set_fraction (&start, 4, 2);
  gst_value_set_fraction (&end, 5, 2);
  gst_value_set_fraction_range (&range2, &start, &end);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == FALSE);

  /* [1/4, 2/3] ∪ [4/2, 5/2] should fail */
  fail_unless (gst_value_union (&dest, &range, &range2) == FALSE);
  /* Same, but swapped args */
  fail_unless (gst_value_union (&dest, &range2, &range) == FALSE);

  g_value_unset (&start);
  g_value_unset (&end);
  g_value_unset (&range);
  g_value_unset (&range2);
  g_value_unset (&src);
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_format_enum)
{
  GstStructure *s, *s2;
  GstFormat foobar_fmt;
  gchar *str, *str2, *end = NULL;

  /* make sure custom formats are serialised properly as well */
  foobar_fmt = gst_format_register ("foobar", "GST_FORMAT_FOOBAR");
  fail_unless (foobar_fmt != GST_FORMAT_UNDEFINED);

  s = gst_structure_new ("foo/bar", "format1", GST_TYPE_FORMAT,
      GST_FORMAT_BYTES, "format2", GST_TYPE_FORMAT, GST_FORMAT_TIME,
      "format3", GST_TYPE_FORMAT, GST_FORMAT_DEFAULT, "format4",
      GST_TYPE_FORMAT, foobar_fmt, NULL);

  str = gst_structure_to_string (s);
  GST_LOG ("Got structure string '%s'", GST_STR_NULL (str));
  fail_unless (str != NULL);
  fail_unless (strstr (str, "time") != NULL);
  fail_unless (strstr (str, "byte") != NULL);
  fail_unless (strstr (str, "default") != NULL);
  fail_unless (strstr (str, "FOOBAR") != NULL);

  s2 = gst_structure_from_string (str, &end);
  fail_unless (s2 != NULL);

  str2 = gst_structure_to_string (s2);
  fail_unless (str2 != NULL);

  fail_unless (g_str_equal (str, str2));

  g_free (str);
  g_free (str2);
  gst_structure_free (s);
  gst_structure_free (s2);
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_value_array)
{
  GValue v = G_VALUE_INIT, v2 = G_VALUE_INIT, v3 = G_VALUE_INIT;
  gchar *str = NULL;

  g_value_init (&v, GST_TYPE_ARRAY);
  g_value_init (&v2, GST_TYPE_ARRAY);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 1);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  g_value_init (&v3, G_TYPE_DOUBLE);
  g_value_set_double (&v3, 0);
  gst_value_array_append_value (&v2, &v3);
  g_value_unset (&v3);
  gst_value_array_append_value (&v, &v2);
  g_value_unset (&v2);

  str = gst_value_serialize (&v);

  g_value_init (&v2, GST_TYPE_ARRAY);
  fail_unless (gst_value_deserialize (&v2, str));
  fail_unless (gst_value_compare (&v, &v2) == 0);

  g_value_unset (&v2);
  g_value_unset (&v);
  g_free (str);
}

GST_END_TEST;

GST_START_TEST (test_compare_caps)
{
  GValue value = { 0 }
  , value2 = {
    0
  };

  g_value_init (&value, GST_TYPE_CAPS);
  g_value_init (&value2, GST_TYPE_CAPS);
  g_value_take_boxed (&value, NULL);
  g_value_take_boxed (&value2, NULL);

  fail_unless_equals_int (gst_value_compare (&value, &value2), GST_VALUE_EQUAL);

  g_value_take_boxed (&value, gst_caps_new_empty_simple ("something"));

  fail_unless_equals_int (gst_value_compare (&value, &value2),
      GST_VALUE_UNORDERED);

  g_value_unset (&value);
  g_value_unset (&value2);

}

GST_END_TEST;

static void
_test_serialize_deserialize_boxed_in_structure (const gpointer boxed_value,
    GType type)
{
  GValue value = G_VALUE_INIT, str_val = G_VALUE_INIT;
  const GValue *value_after;
  GstStructure *s, *s2;
  const gchar *first_str_val = "first \" string", *second_str_val =
      "second \" string";
  gchar *str, *str2, *end;

  g_value_init (&value, type);
  g_value_init (&str_val, G_TYPE_STRING);
  g_value_set_boxed (&value, boxed_value);

  s = gst_structure_new_empty ("test-struct");
  g_value_set_string (&str_val, first_str_val);
  gst_structure_set_value (s, "first", &str_val);
  gst_structure_set_value (s, "test-value", &value);
  g_value_set_string (&str_val, second_str_val);
  gst_structure_set_value (s, "second", &str_val);

  /* serialize the values in the structure */
  str = gst_structure_to_string (s);
  fail_unless (str != NULL);
  GST_DEBUG ("Got structure string '%s'", str);

  /* recreate the structure */
  s2 = gst_structure_from_string (str, &end);
  fail_unless (s2 != NULL);
  fail_unless (end[0] == '\0');

  /* make sure the new structure serializes to the same string */
  str2 = gst_structure_to_string (s2);
  fail_unless_equals_string (str, str2);

  /* test for equality if values can be compared */
  value_after = gst_structure_get_value (s2, "test-value");
  fail_unless (value_after != NULL);
  fail_unless (G_VALUE_TYPE (value_after) == G_VALUE_TYPE (&value));
  if (gst_value_can_compare (&value, value_after))
    fail_unless (gst_value_compare (&value, value_after) == GST_VALUE_EQUAL);

  /* test to make sure that the string values are still present, and
   * haven't been gobbled by the value serialization */
  fail_unless_equals_string (gst_structure_get_string (s2, "first"),
      first_str_val);
  fail_unless_equals_string (gst_structure_get_string (s2, "second"),
      second_str_val);

  /* cleanup */
  gst_structure_free (s);
  gst_structure_free (s2);
  g_free (str);
  g_free (str2);
  g_value_unset (&value);
  g_value_unset (&str_val);
}

GST_START_TEST (test_serialize_deserialize_caps)
{
  GValue value = G_VALUE_INIT, value2 = G_VALUE_INIT;
  GstCaps *incaps = gst_caps_new_simple ("caps/internal",
      "in-field", G_TYPE_INT, 20, "in-field2",
      G_TYPE_STRING, "some in ternal field", NULL);
  GstCaps *test_caps[] = {
    gst_caps_new_simple ("test/caps",
        "foo", G_TYPE_INT, 10, "bar", G_TYPE_STRING, "test",
        "int-caps", GST_TYPE_CAPS, incaps, NULL),
    gst_caps_new_any (),
    gst_caps_new_empty ()
  };
  GstCaps *caps, *caps2;
  gchar *serialized;
  gint i;

  gst_caps_unref (incaps);
  g_value_init (&value, GST_TYPE_CAPS);
  g_value_init (&value2, GST_TYPE_CAPS);

  for (i = 0; i < G_N_ELEMENTS (test_caps); i++) {
    caps = test_caps[i];
    fail_if (GST_CAPS_REFCOUNT_VALUE (caps) != 1);

    /* and assign caps to gvalue */
    g_value_take_boxed (&value, caps);
    fail_if (GST_CAPS_REFCOUNT_VALUE (caps) != 1);

    /* now serialize it */
    serialized = gst_value_serialize (&value);
    GST_DEBUG ("serialized caps to %s", serialized);
    fail_unless (serialized != NULL);

    /* refcount should not change */
    fail_if (GST_CAPS_REFCOUNT_VALUE (caps) != 1);

    /* now deserialize again */
    fail_unless (gst_value_deserialize (&value2, serialized));

    caps2 = g_value_get_boxed (&value2);
    fail_if (GST_CAPS_REFCOUNT_VALUE (caps2) != 1);

    /* they should be equal */
    fail_unless (gst_caps_is_equal (caps, caps2));
    fail_unless (gst_caps_is_any (caps) == gst_caps_is_any (caps2));

    _test_serialize_deserialize_boxed_in_structure (caps, GST_TYPE_CAPS);
    /* cleanup */
    g_free (serialized);
  }
  g_value_unset (&value);
  g_value_unset (&value2);
}

GST_END_TEST;

GST_START_TEST (test_int_range)
{
  GValue range = { 0, };
  GValue start = { 0, };
  GValue end = { 0, };
  GValue src = { 0, };
  GValue dest = { 0, };
  GValue range2 = { 0, };

  g_value_init (&range, GST_TYPE_INT_RANGE);
  g_value_init (&range2, GST_TYPE_INT_RANGE);
  g_value_init (&start, G_TYPE_INT);
  g_value_init (&end, G_TYPE_INT);
  g_value_init (&src, G_TYPE_INT);

  g_value_set_int (&src, 2);

  /* Check that a intersection of int & range = int */
  gst_value_set_int_range (&range, 1, 5);

  fail_unless (gst_value_intersect (&dest, &src, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == G_TYPE_INT);
  fail_unless (gst_value_compare (&dest, &src) == GST_VALUE_EQUAL);

  /* Check that a intersection selects the overlapping range */
  gst_value_set_int_range (&range2, 2, 3);
  g_value_unset (&dest);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT_RANGE);

  fail_unless (gst_value_compare (&dest, &range2) == GST_VALUE_EQUAL);

  /* Check that non intersection ranges don't intersect */
  gst_value_set_int_range (&range2, 6, 7);
  g_value_unset (&dest);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == FALSE);

  gst_value_set_int_range (&range, -7, -6);
  fail_unless_equals_int (gst_value_get_int_range_min (&range), -7);
  fail_unless_equals_int (gst_value_get_int_range_max (&range), -6);
  gst_value_set_int_range (&range, -7, 7);
  fail_unless_equals_int (gst_value_get_int_range_min (&range), -7);
  fail_unless_equals_int (gst_value_get_int_range_max (&range), 7);

  g_value_unset (&start);
  g_value_unset (&end);
  g_value_unset (&range);
  g_value_unset (&range2);
  g_value_unset (&src);
}

GST_END_TEST;

GST_START_TEST (test_int64_range)
{
  GValue range = { 0, };
  GValue start = { 0, };
  GValue end = { 0, };
  GValue src = { 0, };
  GValue dest = { 0, };
  GValue range2 = { 0, };

  g_value_init (&range, GST_TYPE_INT64_RANGE);
  g_value_init (&range2, GST_TYPE_INT64_RANGE);
  g_value_init (&start, G_TYPE_INT64);
  g_value_init (&end, G_TYPE_INT64);
  g_value_init (&src, G_TYPE_INT64);

  g_value_set_int64 (&src, 2);

  /* Check that a intersection of int64 & range = int64 */
  gst_value_set_int64_range (&range, 1, 5);

  fail_unless (gst_value_intersect (&dest, &src, &range) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == G_TYPE_INT64);
  fail_unless (gst_value_compare (&dest, &src) == GST_VALUE_EQUAL);

  /* Check that a intersection selects the overlapping range */
  gst_value_set_int64_range (&range2, 2, 3);
  g_value_unset (&dest);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == TRUE);
  fail_unless (G_VALUE_TYPE (&dest) == GST_TYPE_INT64_RANGE);

  fail_unless (gst_value_compare (&dest, &range2) == GST_VALUE_EQUAL);

  /* Check that non intersection ranges don't intersect */
  gst_value_set_int64_range (&range2, 6, 7);
  g_value_unset (&dest);
  fail_unless (gst_value_intersect (&dest, &range, &range2) == FALSE);

  g_value_unset (&start);
  g_value_unset (&end);
  g_value_unset (&range);
  g_value_unset (&range2);
  g_value_unset (&src);
}

GST_END_TEST;

GST_START_TEST (test_serialize_int64_range)
{
  int i = 0;

  gint64 int64_ranges[] = {
    0, 5,
    0, G_MAXINT,
    5, G_MAXINT32,
    5, G_MAXINT64,
  };
  gint int64_ranges_size = sizeof (int64_ranges) / sizeof (int64_ranges[0]) / 2;

  gchar *int64_range_strings[] = {
    g_strdup ("[ 0, 5 ]"),
    g_strdup_printf ("[ 0, %" G_GINT64_FORMAT " ]", (gint64) G_MAXINT),
    g_strdup_printf ("[ 5, %" G_GINT64_FORMAT " ]", (gint64) G_MAXINT32),
    g_strdup_printf ("[ 5, %" G_GINT64_FORMAT " ]", G_MAXINT64),
  };
  gint int64_range_strings_size =
      sizeof (int64_range_strings) / sizeof (int64_range_strings[0]);

  fail_unless (int64_ranges_size == int64_range_strings_size);

  while (i + 1 < (int64_ranges_size * 2)) {
    if ((i + 1) % 2) {
      gchar *str;
      gchar *str2;
      GValue value = { 0 };
      const GValue *deserialized_value;
      int idx = i / 2;
      GstStructure *s;

      g_value_init (&value, GST_TYPE_INT64_RANGE);

      /* check serialization */
      gst_value_set_int64_range (&value, int64_ranges[i], int64_ranges[i + 1]);
      str = gst_value_serialize (&value);
      fail_unless (strcmp (str, int64_range_strings[idx]) == 0);
      g_free (int64_range_strings[idx]);
      g_value_unset (&value);

      /* now deserialize again to an int64 range */
      s = gst_structure_new ("foo/bar", "range", GST_TYPE_INT64_RANGE,
          int64_ranges[i], int64_ranges[i + 1], NULL);
      deserialized_value = gst_structure_get_value (s, "range");
      fail_unless (GST_VALUE_HOLDS_INT64_RANGE (deserialized_value) == TRUE);
      str2 = gst_value_serialize (deserialized_value);

      fail_unless (gst_value_get_int64_range_min (deserialized_value) ==
          int64_ranges[i]);
      fail_unless (gst_value_get_int64_range_max (deserialized_value) ==
          int64_ranges[i + 1]);

      gst_structure_free (s);
      g_free (str);
      g_free (str2);
    }
    i++;
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_int_range)
{
  GstStructure *s;
  gchar *str, *str2;
  gchar *end = NULL;
  const GValue *deserialized_value;

  /* check a valid int_range deserialization */
  str = g_strdup_printf ("foo/bar, range=[ 1, %d ];", G_MAXINT);
  s = gst_structure_from_string (str, &end);
  fail_unless (*end == '\0');
  deserialized_value = gst_structure_get_value (s, "range");
  fail_unless (GST_VALUE_HOLDS_INT_RANGE (deserialized_value) == TRUE);
  fail_unless (gst_value_get_int_range_min (deserialized_value) == 1);
  fail_unless (gst_value_get_int_range_max (deserialized_value) == G_MAXINT);
  gst_structure_free (s);
  end = NULL;
  g_free (str);

  /* check invalid int_range deserialization */
  str =
      g_strdup_printf ("foo/bar, range=[ 1, %" G_GINT64_FORMAT " ];",
      (gint64) G_MAXINT + 1);
  s = NULL;
  ASSERT_CRITICAL (s = gst_structure_from_string (str, &end));
  g_free (str);
  if (s)
    gst_structure_free (s);
  str =
      g_strdup_printf ("foo/bar, range=[ %" G_GINT64_FORMAT ", %"
      G_GINT64_FORMAT " ];", (gint64) G_MAXINT, (gint64) G_MAXINT + 1);
  ASSERT_CRITICAL (s = gst_structure_from_string (str, NULL));
  end = NULL;
  g_free (str);
  if (s)
    gst_structure_free (s);

  /* check a valid int64_range deserialization. Those ranges need to
   * be explicit about their storage type. */
  str = g_strdup_printf ("foo/bar, range=(gint64)[ 1, %d ];", G_MAXINT);
  s = gst_structure_from_string (str, &end);
  fail_unless (*end == '\0');
  deserialized_value = gst_structure_get_value (s, "range");
  fail_unless (GST_VALUE_HOLDS_INT64_RANGE (deserialized_value) == TRUE);
  fail_unless (gst_value_get_int64_range_min (deserialized_value) == 1);
  fail_unless (gst_value_get_int64_range_max (deserialized_value) == G_MAXINT);
  str2 = gst_structure_to_string (s);
  fail_unless (strcmp (str, str2) == 0);
  gst_structure_free (s);
  end = NULL;
  g_free (str);
  g_free (str2);

  /* check invalid int64_range (starting with a gint) deserialization */
  str =
      g_strdup_printf ("foo/bar, range=(gint64)[ 1, %" G_GUINT64_FORMAT " ];",
      (guint64) G_MAXINT64 + 1);
  s = NULL;
  ASSERT_CRITICAL (s = gst_structure_from_string (str, &end));
  if (s) {
    fail_unless (*end == '\0');
    gst_structure_free (s);
    end = NULL;
  }
  g_free (str);

  /* check invalid int64_range deserialization into a int64_range */
  str =
      g_strdup_printf ("foo/bar, range=(gint64)[ %" G_GINT64_FORMAT ", %"
      G_GUINT64_FORMAT " ];", (gint64) G_MAXINT, (guint64) G_MAXINT64 + 1);
  ASSERT_CRITICAL (s = gst_structure_from_string (str, NULL));
  g_free (str);
  if (s)
    gst_structure_free (s);

  /* check invalid int64_range deserialization into a int_range */
  str =
      g_strdup_printf ("foo/bar, range=[ %" G_GINT64_FORMAT ", %"
      G_GUINT64_FORMAT " ];", (gint64) G_MAXINT, (guint64) G_MAXINT64 + 1);
  s = gst_structure_from_string (str, &end);
  fail_unless (s == NULL);
  fail_unless (end == NULL);
  g_free (str);
}

GST_END_TEST;

GST_START_TEST (test_stepped_range_collection)
{
  GstStructure *s;
  const GValue *v;

  s = gst_structure_new ("foo/bar", "range", GST_TYPE_INT_RANGE, 8, 12, NULL);
  fail_unless (s != NULL);
  v = gst_structure_get_value (s, "range");
  fail_unless (v != NULL);
  fail_unless (gst_value_get_int_range_min (v) == 8);
  fail_unless (gst_value_get_int_range_max (v) == 12);
  fail_unless (gst_value_get_int_range_step (v) == 1);
  gst_structure_free (s);

  s = gst_structure_new ("foo/bar", "range", GST_TYPE_INT64_RANGE, (gint64) 8,
      (gint64) 12, NULL);
  fail_unless (s != NULL);
  v = gst_structure_get_value (s, "range");
  fail_unless (v != NULL);
  fail_unless (gst_value_get_int64_range_min (v) == 8);
  fail_unless (gst_value_get_int64_range_max (v) == 12);
  fail_unless (gst_value_get_int64_range_step (v) == 1);
  gst_structure_free (s);
}

GST_END_TEST;

GST_START_TEST (test_stepped_int_range_parsing)
{
  gchar *str;
  guint n;
  gchar *end = NULL;
  GstStructure *s;

  static const gchar *good_ranges[] = {
    "[0, 1, 1]",
    "[-2, 2, 2]",
    "[16, 4096, 16]",
  };

  static const gchar *bad_ranges[] = {
    "[0, 1, -1]",
    "[1, 2, 2]",
    "[2, 3, 2]",
    "[0, 0, 0]",
    "[0, 0, 1]",
    "[1, 2, 0]",
    "[1, 1, 1]",
  };

  /* check we can parse good ranges */
  for (n = 0; n < G_N_ELEMENTS (good_ranges); ++n) {
    str = g_strdup_printf ("foo/bar, range=%s", good_ranges[n]);
    s = gst_structure_from_string (str, &end);
    fail_unless (s != NULL);
    fail_unless (*end == '\0');
    gst_structure_free (s);
    s = NULL;
    g_free (str);
  }

  /* check we cannot parse bad ranges */
  for (n = 0; n < G_N_ELEMENTS (bad_ranges); ++n) {
    str = g_strdup_printf ("foo/bar, range=%s", bad_ranges[n]);
    ASSERT_CRITICAL (s = gst_structure_from_string (str, &end));
    if (s)
      gst_structure_free (s);
    g_free (str);
  }
}

GST_END_TEST;

GST_START_TEST (test_stepped_int_range_ops)
{
  gchar *str1, *str2, *str3;
  guint n;
  GstStructure *s1, *s2, *s3;
  const GValue *v1, *v2, *v3;

  static const struct
  {
    const gchar *set1;
    const gchar *op;
    const gchar *set2;
    const gchar *result;
  } ranges[] = {
    {
        "[16, 4096, 16]", "inter", "[100, 200, 10]", "160"}, {
        "[16, 4096, 16]", "inter", "[100, 200, 100]", NULL}, {
        "[16, 4096, 16]", "inter", "[0, 512, 256]", "[256, 512, 256]"}, {
        "[16, 32, 16]", "union", "[32, 96, 16]", "[16, 96, 16]"}, {
        "[16, 32, 16]", "union", "[48, 96, 16]", "[16, 96, 16]"}, {
        "[112, 192, 16]", "union", "[48, 96, 16]", "[48, 192, 16]"}, {
        "[16, 32, 16]", "union", "[64, 96, 16]", NULL}, {
        "[112, 192, 16]", "union", "[48, 96, 8]", NULL}, {
        "[10, 20, 5]", "union", "10", "[10, 20, 5]"}, {
        "[10, 20, 5]", "union", "20", "[10, 20, 5]"}, {
        "[10, 20, 5]", "union", "15", "[10, 20, 5]"}, {
        "[10, 20, 5]", "union", "5", "[5, 20, 5]"}, {
        "[10, 20, 5]", "union", "12", NULL}, {
        "[10, 20, 5]", "union", "30", NULL}, {
        "[10, 20, 5]", "union", "25", "[10, 25, 5]"},
  };

  for (n = 0; n < G_N_ELEMENTS (ranges); ++n) {
    gchar *end = NULL;
    GValue dest = { 0 };
    gboolean ret;

    str1 = g_strdup_printf ("foo/bar, range=%s", ranges[n].set1);
    s1 = gst_structure_from_string (str1, &end);
    fail_unless (s1 != NULL);
    fail_unless (*end == '\0');
    v1 = gst_structure_get_value (s1, "range");
    fail_unless (v1 != NULL);

    str2 = g_strdup_printf ("foo/bar, range=%s", ranges[n].set2);
    s2 = gst_structure_from_string (str2, &end);
    fail_unless (s2 != NULL);
    fail_unless (*end == '\0');
    v2 = gst_structure_get_value (s2, "range");
    fail_unless (v2 != NULL);

    if (!strcmp (ranges[n].op, "inter")) {
      ret = gst_value_intersect (&dest, v1, v2);
    } else if (!strcmp (ranges[n].op, "union")) {
      ret = gst_value_union (&dest, v1, v2);
    } else {
      fail_unless (FALSE);
      ret = FALSE;
    }

    if (ranges[n].result) {
      fail_unless (ret);
    } else {
      fail_unless (!ret);
    }

    if (ret) {
      str3 = g_strdup_printf ("foo/bar, range=%s", ranges[n].result);
      s3 = gst_structure_from_string (str3, &end);
      fail_unless (s3 != NULL);
      fail_unless (*end == '\0');
      v3 = gst_structure_get_value (s3, "range");
      fail_unless (v3 != NULL);

      if (gst_value_compare (&dest, v3) != GST_VALUE_EQUAL) {
        GST_ERROR ("%s %s %s yielded %s, expected %s", str1, ranges[n].op, str2,
            gst_value_serialize (&dest), gst_value_serialize (v3));
        fail_unless (FALSE);
      }

      gst_structure_free (s3);
      g_free (str3);

      g_value_unset (&dest);
    }

    gst_structure_free (s2);
    g_free (str2);
    gst_structure_free (s1);
    g_free (str1);
  }
}

GST_END_TEST;

GST_START_TEST (test_structure_basic)
{
  GstStructure *s1, *s2;
  GValue v1 = G_VALUE_INIT, v2 = G_VALUE_INIT;

  /* sanity test */
  s1 = gst_structure_from_string ("foo,bar=1", NULL);
  g_value_init (&v1, GST_TYPE_STRUCTURE);
  gst_value_set_structure (&v1, s1);
  fail_unless (gst_structure_is_equal (s1, gst_value_get_structure (&v1)));

  s2 = gst_structure_copy (s1);
  g_value_init (&v2, GST_TYPE_STRUCTURE);
  gst_value_set_structure (&v2, s2);

  /* can do everything but subtract */
  fail_unless (gst_value_can_compare (&v1, &v2));
  fail_unless (gst_value_can_intersect (&v1, &v2));
  fail_unless (!gst_value_can_subtract (&v1, &v2));
  fail_unless (gst_value_can_union (&v1, &v2));

  gst_structure_free (s1);
  gst_structure_free (s2);
  g_value_unset (&v1);
  g_value_unset (&v2);
}

GST_END_TEST;

GST_START_TEST (test_structure_single_ops)
{
  static const struct
  {
    const gchar *str1;
    gboolean is_fixed;
    gboolean can_fixate;
  } single_struct[] = {
    {
        "foo,bar=(int)1", TRUE, TRUE}, {
        "foo,bar=(int)[1,2]", FALSE, TRUE},
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (single_struct); i++) {
    GstStructure *s1 = gst_structure_from_string (single_struct[i].str1, NULL);
    GValue v1 = G_VALUE_INIT;
    GValue v2 = G_VALUE_INIT;

    fail_unless (s1 != NULL);

    GST_DEBUG ("checking structure %" GST_PTR_FORMAT, s1);

    g_value_init (&v1, GST_TYPE_STRUCTURE);
    gst_value_set_structure (&v1, s1);

    fail_unless (gst_value_is_fixed (&v1) == single_struct[i].is_fixed);
    fail_unless (gst_value_fixate (&v2, &v1) == single_struct[i].can_fixate);
    if (single_struct[i].can_fixate)
      g_value_unset (&v2);

    g_value_unset (&v1);
    gst_structure_free (s1);
  }
}

GST_END_TEST;

GST_START_TEST (test_structure_ops)
{
  struct
  {
    const gchar *str1;
    const gchar *str2;
    const gchar *op;
    gint ret;
    GType str_type;
  } comparisons[] = {
    /* *INDENT-OFF* */
    {"foo,bar=(int)1", "foo,bar=(int)1", "compare", GST_VALUE_EQUAL, 0},
    {"foo,bar=(int)1", "foo,bar=(int)1", "is_subset", TRUE, 0},
    {"foo,bar=(int)1", "foo,bar=(int)1", "intersect", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)1", "foo,bar=(int)1", "union", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)[1,2]", "foo,bar=(int)1", "compare", GST_VALUE_UNORDERED, 0},
    {"foo,bar=(int)[1,2]", "foo,bar=(int)1", "is_subset", FALSE, 0},
    {"foo,bar=(int)[1,2]", "foo,bar=(int)1", "intersect", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)[1,2]", "foo,bar=(int)1", "union", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)1", "foo,bar=(int)[1,2]", "compare", GST_VALUE_UNORDERED, 0},
    {"foo,bar=(int)1", "foo,bar=(int)[1,2]", "is_subset", TRUE, 0},
    {"foo,bar=(int)1", "foo,bar=(int)[1,2]", "intersect", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)1", "foo,bar=(int)[1,2]", "union", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)1", "foo,bar=(int)2", "compare", GST_VALUE_UNORDERED, 0},
    {"foo,bar=(int)1", "foo,bar=(int)2", "is_subset", FALSE, 0},
    {"foo,bar=(int)1", "foo,bar=(int)2", "intersect", FALSE, 0},
    {"foo,bar=(int)1", "foo,bar=(int)2", "union", TRUE, GST_TYPE_STRUCTURE},
    {"foo,bar=(int)1", "baz,bar=(int)1", "compare", GST_VALUE_UNORDERED, 0},
    {"foo,bar=(int)1", "baz,bar=(int)1", "is_subset", FALSE, 0},
    {"foo,bar=(int)1", "baz,bar=(int)1", "intersect", FALSE, 0},
#if 0
    /* deserializing lists is not implemented (but this should still work!) */
    {"foo,bar=(int)1", "baz,bar=(int)1", "union", TRUE, G_TYPE_LIST},
#endif
    /* *INDENT-ON* */
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (comparisons); i++) {
    GstStructure *s1 = gst_structure_from_string (comparisons[i].str1, NULL);
    GstStructure *s2 = gst_structure_from_string (comparisons[i].str2, NULL);
    GValue v1 = G_VALUE_INIT, v2 = G_VALUE_INIT, v3 = G_VALUE_INIT;

    fail_unless (s1 != NULL);
    fail_unless (s2 != NULL);

    GST_DEBUG ("checking %s with structure1 %" GST_PTR_FORMAT " structure2 %"
        GST_PTR_FORMAT " is %d", comparisons[i].op, s1, s2, comparisons[i].ret);

    g_value_init (&v1, GST_TYPE_STRUCTURE);
    gst_value_set_structure (&v1, s1);
    g_value_init (&v2, GST_TYPE_STRUCTURE);
    gst_value_set_structure (&v2, s2);

    if (g_strcmp0 (comparisons[i].op, "compare") == 0) {
      fail_unless (gst_value_compare (&v1, &v2) == comparisons[i].ret);
    } else if (g_strcmp0 (comparisons[i].op, "is_subset") == 0) {
      fail_unless (gst_value_is_subset (&v1, &v2) == comparisons[i].ret);
    } else {
      if (g_strcmp0 (comparisons[i].op, "intersect") == 0) {
        fail_unless (gst_value_intersect (&v3, &v1, &v2) == comparisons[i].ret);
      } else if (g_strcmp0 (comparisons[i].op, "union") == 0) {
        fail_unless (gst_value_union (&v3, &v1, &v2) == comparisons[i].ret);
      }
      if (comparisons[i].ret) {
        GValue result = G_VALUE_INIT;
        gchar *str;

        str = gst_value_serialize (&v3);
        GST_LOG ("result %s", str);

        g_value_init (&result, comparisons[i].str_type);
        fail_unless (gst_value_deserialize (&result, str));
        g_free (str);
        fail_unless (gst_value_compare (&result, &v3) == GST_VALUE_EQUAL);
        g_value_unset (&v3);
        g_value_unset (&result);
      }
    }

    gst_structure_free (s1);
    gst_structure_free (s2);
    g_value_unset (&v1);
    g_value_unset (&v2);
  }
}

GST_END_TEST;

static gpointer
get_serialize_deserialize_boxed (const gpointer boxed, GType type)
{
  gchar *serialized, *cmp;
  GValue value = G_VALUE_INIT, value2 = G_VALUE_INIT;
  gpointer ret;

  g_value_init (&value, type);
  g_value_init (&value2, type);

  g_value_set_boxed (&value, boxed);
  serialized = gst_value_serialize (&value);
  fail_unless (serialized != NULL);
  GST_DEBUG ("serialized to %s", serialized);
  fail_unless (gst_value_deserialize (&value2, serialized));
  cmp = gst_value_serialize (&value2);
  fail_unless_equals_string (cmp, serialized);

  ret = g_value_dup_boxed (&value2);

  g_free (serialized);
  g_free (cmp);
  g_value_unset (&value);
  g_value_unset (&value2);
  return ret;
}

GST_START_TEST (test_serialize_deserialize_structure)
{
  GstStructure *instr = gst_structure_new ("structure/internal",
      "in-field", G_TYPE_INT, 20, "in-field2",
      G_TYPE_STRING, "some in ternal field", NULL);
  GstStructure *test_str[] = {
    gst_structure_new ("test/structure",
        "foo", G_TYPE_INT, 10, "bar", G_TYPE_STRING, "test",
        "int-str", GST_TYPE_STRUCTURE, instr, NULL),
    gst_structure_new_empty ("empty")
  };
  gint i;
  GstStructure *str, *str2;

  gst_structure_free (instr);
  for (i = 0; i < G_N_ELEMENTS (test_str); i++) {
    str = test_str[i];
    str2 = get_serialize_deserialize_boxed (str, GST_TYPE_STRUCTURE);
    fail_unless (gst_structure_is_equal (str, str2));

    _test_serialize_deserialize_boxed_in_structure (str, GST_TYPE_STRUCTURE);

    gst_structure_free (str);
    gst_structure_free (str2);
  }
}

GST_END_TEST;

static void
setup_test_value_array (GValue * value)
{
  GValueArray *array;
  GValue v = G_VALUE_INIT;

  g_value_init (&v, G_TYPE_INT);
  g_value_init (value, G_TYPE_VALUE_ARRAY);

  array = g_value_array_new (3);
  g_value_set_int (&v, 1);
  g_value_array_append (array, &v);
  g_value_set_int (&v, 2);
  g_value_array_append (array, &v);
  g_value_set_int (&v, 3);
  g_value_array_append (array, &v);

  g_value_take_boxed (value, array);
}

static void
test_revert_array_transform (GValue * v1, GValue * v2)
{
  GValueArray *array;

  g_value_reset (v1);

  fail_unless (g_value_transform (v2, v1));
  array = g_value_get_boxed (v1);
  fail_unless (array->n_values == 3);
  fail_unless (g_value_get_int (g_value_array_get_nth (array, 0)) == 1);
  fail_unless (g_value_get_int (g_value_array_get_nth (array, 1)) == 2);
  fail_unless (g_value_get_int (g_value_array_get_nth (array, 2)) == 3);
}

GST_START_TEST (test_transform_array)
{
  GValue v1 = G_VALUE_INIT, v2 = G_VALUE_INIT;

  setup_test_value_array (&v1);

  g_value_init (&v2, GST_TYPE_ARRAY);

  fail_unless (g_value_transform (&v1, &v2));
  fail_unless (gst_value_array_get_size (&v2) == 3);
  fail_unless (g_value_get_int (gst_value_array_get_value (&v2, 0)) == 1);
  fail_unless (g_value_get_int (gst_value_array_get_value (&v2, 1)) == 2);
  fail_unless (g_value_get_int (gst_value_array_get_value (&v2, 2)) == 3);

  test_revert_array_transform (&v1, &v2);

  g_value_unset (&v1);
  g_value_unset (&v2);
}

GST_END_TEST;

GST_START_TEST (test_transform_list)
{
  GValue v1 = G_VALUE_INIT, v2 = G_VALUE_INIT;

  setup_test_value_array (&v1);

  g_value_init (&v2, GST_TYPE_LIST);

  fail_unless (g_value_transform (&v1, &v2));
  fail_unless (gst_value_list_get_size (&v2) == 3);
  fail_unless (g_value_get_int (gst_value_list_get_value (&v2, 0)) == 1);
  fail_unless (g_value_get_int (gst_value_list_get_value (&v2, 1)) == 2);
  fail_unless (g_value_get_int (gst_value_list_get_value (&v2, 2)) == 3);

  test_revert_array_transform (&v1, &v2);

  g_value_unset (&v1);
  g_value_unset (&v2);
}

GST_END_TEST;

GST_START_TEST (test_serialize_null_aray)
{
  gchar *serialized;
  GValue v = G_VALUE_INIT;

  g_value_init (&v, G_TYPE_VALUE_ARRAY);

  g_value_set_boxed (&v, NULL);
  serialized = gst_value_serialize (&v);
  fail_unless_equals_string (serialized, "<  >");
  g_value_unset (&v);
  g_free (serialized);
}

GST_END_TEST;

GST_START_TEST (test_deserialize_array)
{
  GValue value = { 0 };
  const gchar *strings[] = {
    "{ test, }",
    "{ , }",
    "{ test,, }",
    "{ , , }",
  };
  gint results_size[] = { 1, 0, -1, -1 };       /* -1 means deserialization should fail */
  int i;

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    /* Workaround a bug in our parser that would lead to segfaults
     * when deserializing container types using static strings */
    gchar *str = g_strdup (strings[i]);
    g_value_init (&value, GST_TYPE_LIST);

    if (results_size[i] == -1) {
      fail_if (gst_value_deserialize (&value, str),
          "Should not be able to deserialize %s (%d) as list", str, i);
    } else {
      fail_unless (gst_value_deserialize (&value, str),
          "could not deserialize %s (%d)", str, i);
      fail_unless (gst_value_list_get_size (&value) == results_size[i],
          "Wrong array size: %d. expected %d",
          gst_value_array_get_size (&value), results_size[i]);
    }

    g_value_unset (&value);
    g_free (str);
  }
}

GST_END_TEST;

#define TEST_FLAGS_TYPE (test_flags_get_type())
static GType
test_flags_get_type (void)
{
  static const GFlagsValue values[] = {
    {1, "One", "one"},
    {1 << 1, "Two", "two"},
    {1 << 3, "Eight", "eight"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter_pointer ((gpointer *) & id)) {
    GType _id;

    _id = g_flags_register_static ("TestFlags", values);

    g_once_init_leave_pointer ((gpointer *) & id, _id);
  }

  return id;
}

#define _RESULT(i) result_##i
#define RESULT(i) _RESULT(i)

GST_START_TEST (test_deserialize_with_pspec)
{
  GValue value = { 0 };
  GParamSpec *pspec;
  const gchar *strings[] = {
    "< one, 0>",
    "< one+eight, two >",
    "< 9, 0>",
  };
  int i;

  gint results[3][2] = {
    {1, 0},
    {9, 2},
    {9, 0}
  };

  pspec = gst_param_spec_array ("flags-array",
      "Flags Array", "An array of flags",
      g_param_spec_flags ("flags", "Flags", "Flags", TEST_FLAGS_TYPE, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  for (i = 0; i < G_N_ELEMENTS (strings); ++i) {
    int j;
    gchar *str = g_strdup (strings[i]);
    g_value_init (&value, GST_TYPE_ARRAY);

    fail_unless (gst_value_deserialize_with_pspec (&value, str, pspec),
        "could not deserialize %s (%d)", str, i);

    fail_unless (gst_value_array_get_size (&value) ==
        G_N_ELEMENTS (results[i]));

    for (j = 0; j < G_N_ELEMENTS (results[i]); j++) {
      const GValue *elem_value = gst_value_array_get_value (&value, j);
      fail_unless (G_VALUE_TYPE (elem_value) == TEST_FLAGS_TYPE);
      fail_unless_equals_int (g_value_get_flags (elem_value), results[i][j]);
    }

    g_value_unset (&value);
    g_free (str);
  }

  g_param_spec_unref (pspec);
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_segment)
{
  GstSegment *seg, *seg2;

  seg = gst_segment_new ();
  gst_segment_init (seg, GST_FORMAT_DEFAULT);
  fail_unless (gst_segment_do_seek (seg, 1.2, GST_FORMAT_DEFAULT,
          GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 20, GST_SEEK_TYPE_SET, 30,
          NULL));
  seg2 = get_serialize_deserialize_boxed (seg, GST_TYPE_SEGMENT);
  fail_unless (gst_segment_is_equal (seg, seg2));

  _test_serialize_deserialize_boxed_in_structure (seg, GST_TYPE_SEGMENT);

  gst_segment_free (seg);
  gst_segment_free (seg2);
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_caps_features)
{
  GstCapsFeatures *test_feats[] = {
    gst_caps_features_new ("abc:val1", "xyz:val2", NULL),
    gst_caps_features_new ("feat:val", NULL),
    gst_caps_features_new_any (),
    gst_caps_features_new_empty ()
  };
  gint i;
  GstCapsFeatures *feats, *feats2;

  for (i = 0; i < G_N_ELEMENTS (test_feats); i++) {
    feats = test_feats[i];
    fail_unless (feats != NULL);
    feats2 = get_serialize_deserialize_boxed (feats, GST_TYPE_CAPS_FEATURES);
    fail_unless (gst_caps_features_is_equal (feats, feats2));
    fail_unless (gst_caps_features_is_any (feats) ==
        gst_caps_features_is_any (feats2));

    _test_serialize_deserialize_boxed_in_structure (feats,
        GST_TYPE_CAPS_FEATURES);

    gst_caps_features_free (feats);
    gst_caps_features_free (feats2);
  }
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_tag_list)
{
  GstTagList *test_tags[] = {
    gst_tag_list_new (GST_TAG_TITLE, "A Title", GST_TAG_ARTIST, "Art櫱",
        GST_TAG_TRACK_NUMBER, 1, NULL),
    gst_tag_list_new_empty ()
  };
  gint i;
  GstTagList *tags, *tags2;

  for (i = 0; i < G_N_ELEMENTS (test_tags); i++) {
    tags = test_tags[i];
    fail_unless (tags != NULL);
    tags2 = get_serialize_deserialize_boxed (tags, GST_TYPE_TAG_LIST);
    fail_unless (gst_tag_list_is_equal (tags, tags2));

    _test_serialize_deserialize_boxed_in_structure (tags, GST_TYPE_TAG_LIST);

    gst_tag_list_unref (tags);
    gst_tag_list_unref (tags2);
  }
}

GST_END_TEST;

GST_START_TEST (test_deserialize_serialize_nested_structures)
{
  gint i;
  gchar *structure_str;
  GstStructure *structure, *structure2;

  /* *INDENT-OFF* */
  struct
  {
    const gchar *serialized_struct;
    gboolean should_fail;
    const gchar *path_to_bool;
    const gchar *subcaps_str;
  } tests_data[] = {
      {"s, substruct=[sub, is-deepest=true]", FALSE, "substruct"},
      {"s, substruct=(structure) [sub, is-deepest=true]", FALSE, "substruct"},
      {"s, substruct=[sub, is-substruct=true, subsubstruct=[subsub, is-deepest=true]]", FALSE, "substruct/subsubstruct"},
      {"s, substruct=[sub, is-substruct=true, subsubstruct=[subsub, subsubsubstruct=[subsubsub, is-deepest=true]]]", FALSE, "substruct/subsubstruct/subsubsubstruct"},
      {"s, substruct=[sub, an-array={a, b}, subsubstruct=[subsub, a-range=[1,2], a-string=\"this is a \\\"string\\\"\"]]", FALSE, NULL},
      {"s, sub-caps=[nested-caps(some:Feature), is-caps=true; second, caps-structure=true]", FALSE, NULL, "nested-caps(some:Feature), is-caps=true; second, caps-structure=true"},
      {"s, sub-caps=[nested-caps(some:Feature)]", FALSE, NULL, "nested-caps(some:Feature)"},
      {"s, array=(structure){[struct, n=1], [struct, n=2]}"},
      /* Broken structure with substructures */
      {"s, substruct=[sub, is-substruct=true", TRUE},
      {"s, substruct=[sub, is-substruct=true, sub=\"yes]", TRUE},
      {"s, substruct=[sub, a-broken-string=$broken]", TRUE},
      {"s, sub-caps=(int)[nested-caps(some:Feature)]", TRUE},
  };
  /* *INDENT-ON* */

  for (i = 0; i < G_N_ELEMENTS (tests_data); i++) {
    structure = gst_structure_new_from_string (tests_data[i].serialized_struct);
    if (tests_data[i].should_fail) {
      fail_if (structure, "%s not be deserialized",
          tests_data[i].serialized_struct);
      continue;
    }
    fail_unless (structure, "%s could not be deserialized",
        tests_data[i].serialized_struct);
    structure_str = gst_structure_to_string (structure);
    structure2 = gst_structure_new_from_string (structure_str);
    fail_unless (gst_structure_is_equal (structure, structure2));
    g_free (structure_str);

    if (tests_data[i].path_to_bool) {
      const GstStructure *tmpstruct = structure;
      gchar **tmpstrv = g_strsplit (tests_data[i].path_to_bool, "/", -1);
      gint j;

      for (j = 0; tmpstrv[j]; j++) {
        const GValue *v = gst_structure_get_value (tmpstruct, tmpstrv[j]);

        fail_unless (v, "Could not find '%s' in %s", tmpstrv[j],
            gst_structure_to_string (tmpstruct));
        tmpstruct = gst_value_get_structure (v);

        fail_unless (GST_IS_STRUCTURE (tmpstruct));
        if (!tmpstrv[j + 1]) {
          gboolean tmp;

          fail_unless (gst_structure_get_boolean (tmpstruct, "is-deepest", &tmp)
              && tmp);
        }
      }
      g_strfreev (tmpstrv);
    }
    if (tests_data[i].subcaps_str) {
      const GValue *v = gst_structure_get_value (structure, "sub-caps");
      const GstCaps *caps = gst_value_get_caps (v);
      GstCaps *caps2 = gst_caps_from_string (tests_data[i].subcaps_str);

      fail_unless (gst_caps_is_equal (caps, caps2));
      gst_caps_unref (caps2);
    }

    /* Ensure that doing a round trip works as expected */
    structure_str = gst_structure_to_string (structure2);
    gst_structure_free (structure2);
    structure2 = gst_structure_new_from_string (structure_str);
    fail_unless (gst_structure_is_equal (structure, structure2));
    gst_structure_free (structure);
    gst_structure_free (structure2);
    g_free (structure_str);
  }
}

GST_END_TEST;

GST_START_TEST (test_serialize_deserialize_sample)
{
  GstSample *samp, *samp2;
  gsize buff_len = 8;
  const gchar buff_str[8] = "buf\ndat";
  gchar *buff_val;
  GstBuffer *buff = gst_buffer_new_wrapped (g_strdup (buff_str), buff_len);
  GstCaps *caps = gst_caps_new_simple ("caps", "Int", G_TYPE_INT, 20,
      "String", G_TYPE_STRING, "a string", NULL);
  GstSegment *seg = gst_segment_new ();

  gst_segment_init (seg, GST_FORMAT_DEFAULT);
  fail_unless (gst_segment_do_seek (seg, 1.2, GST_FORMAT_DEFAULT,
          GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 20, GST_SEEK_TYPE_SET, 30,
          NULL));
  fail_unless (buff != NULL);
  fail_unless (caps != NULL);
  samp = gst_sample_new (buff, caps, seg,
      gst_structure_new ("structure", "Float", G_TYPE_FLOAT, -2.5, NULL));
  gst_buffer_unref (buff);
  gst_caps_unref (caps);

  samp2 = get_serialize_deserialize_boxed (samp, GST_TYPE_SAMPLE);
  fail_unless (gst_caps_is_equal (gst_sample_get_caps (samp),
          gst_sample_get_caps (samp2)));
  fail_unless (gst_structure_is_equal (gst_sample_get_info (samp),
          gst_sample_get_info (samp2)));
  fail_unless (gst_segment_is_equal (gst_sample_get_segment (samp),
          gst_sample_get_segment (samp2)));

  buff = gst_sample_get_buffer (samp);
  gst_buffer_extract_dup (buff, 0, -1, (gpointer *) & buff_val, &buff_len);
  fail_unless_equals_int (buff_len, 8);
  fail_unless_equals_string (buff_val, buff_str);
  g_free (buff_val);
  buff = gst_sample_get_buffer (samp2);
  gst_buffer_extract_dup (buff, 0, -1, (gpointer *) & buff_val, &buff_len);
  fail_unless_equals_int (buff_len, 8);
  fail_unless_equals_string (buff_val, buff_str);
  g_free (buff_val);

  _test_serialize_deserialize_boxed_in_structure (samp, GST_TYPE_SAMPLE);

  gst_sample_unref (samp);
  gst_sample_unref (samp2);
  gst_segment_free (seg);
}

GST_END_TEST;

static Suite *
gst_value_suite (void)
{
  Suite *s = suite_create ("GstValue");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_deserialize_buffer);
  tcase_add_test (tc_chain, test_serialize_buffer);
  tcase_add_test (tc_chain, test_deserialize_gint);
  tcase_add_test (tc_chain, test_deserialize_gint_failures);
  tcase_add_test (tc_chain, test_deserialize_guint);
  tcase_add_test (tc_chain, test_deserialize_guint_failures);
  tcase_add_test (tc_chain, test_deserialize_gint64);
  tcase_add_test (tc_chain, test_deserialize_guint64);
  tcase_add_test (tc_chain, test_deserialize_guchar);
  tcase_add_test (tc_chain, test_deserialize_gstfraction);
  tcase_add_test (tc_chain, test_deserialize_gtype);
  tcase_add_test (tc_chain, test_deserialize_gtype_failures);
  tcase_add_test (tc_chain, test_deserialize_bitmask);
  tcase_add_test (tc_chain, test_deserialize_array);
  tcase_add_test (tc_chain, test_serialize_flags);
  tcase_add_test (tc_chain, test_serialize_flags_invalid);
  tcase_add_test (tc_chain, test_deserialize_flags);
  tcase_add_test (tc_chain, test_serialize_deserialize_format_enum);
  tcase_add_test (tc_chain, test_serialize_deserialize_value_array);
  tcase_add_test (tc_chain, test_string);
  tcase_add_test (tc_chain, test_deserialize_string);
  tcase_add_test (tc_chain, test_value_compare);
  tcase_add_test (tc_chain, test_value_intersect);
  tcase_add_test (tc_chain, test_value_subtract_int);
  tcase_add_test (tc_chain, test_value_subtract_int64);
  tcase_add_test (tc_chain, test_value_subtract_double);
  tcase_add_test (tc_chain, test_value_subtract_fraction);
  tcase_add_test (tc_chain, test_value_subtract_fraction_range);
  tcase_add_test (tc_chain, test_value_subtract_fraction_list);
  tcase_add_test (tc_chain, test_date);
  tcase_add_test (tc_chain, test_date_time);
  tcase_add_test (tc_chain, test_fraction_range);
  tcase_add_test (tc_chain, test_serialize_deserialize_caps);
  tcase_add_test (tc_chain, test_compare_caps);
  tcase_add_test (tc_chain, test_int_range);
  tcase_add_test (tc_chain, test_int64_range);
  tcase_add_test (tc_chain, test_serialize_int64_range);
  tcase_add_test (tc_chain, test_deserialize_int_range);
  tcase_add_test (tc_chain, test_stepped_range_collection);
  tcase_add_test (tc_chain, test_stepped_int_range_parsing);
  tcase_add_test (tc_chain, test_stepped_int_range_ops);
  tcase_add_test (tc_chain, test_flagset);
  tcase_add_test (tc_chain, test_structure_basic);
  tcase_add_test (tc_chain, test_structure_single_ops);
  tcase_add_test (tc_chain, test_structure_ops);
  tcase_add_test (tc_chain, test_serialize_deserialize_structure);
  tcase_add_test (tc_chain, test_transform_array);
  tcase_add_test (tc_chain, test_transform_list);
  tcase_add_test (tc_chain, test_serialize_null_aray);
  tcase_add_test (tc_chain, test_deserialize_with_pspec);
  tcase_add_test (tc_chain, test_deserialize_serialize_nested_structures);
  tcase_add_test (tc_chain, test_serialize_deserialize_segment);
  tcase_add_test (tc_chain, test_serialize_deserialize_caps_features);
  tcase_add_test (tc_chain, test_serialize_deserialize_tag_list);
  tcase_add_test (tc_chain, test_serialize_deserialize_sample);

  return s;
}

GST_CHECK_MAIN (gst_value);
