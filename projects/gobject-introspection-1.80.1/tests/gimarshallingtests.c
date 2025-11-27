/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *vim: tabstop=4 shiftwidth=4 expandtab
 */

/* This file gets installed, so we can't assume config.h is available */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gimarshallingtests.h"

#include <string.h>

static void gi_marshalling_tests_boxed_struct_free (GIMarshallingTestsBoxedStruct *v);

/* Booleans */

gboolean
gi_marshalling_tests_boolean_return_true (void)
{
  return TRUE;
}

gboolean
gi_marshalling_tests_boolean_return_false (void)
{
  return FALSE;
}

void
gi_marshalling_tests_boolean_in_true (gboolean v)
{
  g_assert (v == TRUE);
}

void
gi_marshalling_tests_boolean_in_false (gboolean v)
{
  g_assert (v == FALSE);
}

/**
 * gi_marshalling_tests_boolean_out_true:
 * @v: (out):
 */
void
gi_marshalling_tests_boolean_out_true (gboolean *v)
{
  *v = TRUE;
}

/**
 * gi_marshalling_tests_boolean_out_false:
 * @v: (out):
 */
void
gi_marshalling_tests_boolean_out_false (gboolean *v)
{
  *v = FALSE;
}

/**
 * gi_marshalling_tests_boolean_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_boolean_out_uninitialized (gboolean *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_boolean_inout_true_false:
 * @v: (inout):
 */
void
gi_marshalling_tests_boolean_inout_true_false (gboolean *v)
{
  g_assert (*v == TRUE);
  *v = FALSE;
}

/**
 * gi_marshalling_tests_boolean_inout_false_true:
 * @v: (inout):
 */
void
gi_marshalling_tests_boolean_inout_false_true (gboolean *v)
{
  g_assert (*v == FALSE);
  *v = TRUE;
}


/* Integers */

gint8
gi_marshalling_tests_int8_return_max (void)
{
  return G_MAXINT8;
}

gint8
gi_marshalling_tests_int8_return_min (void)
{
  return G_MININT8;
}

void
gi_marshalling_tests_int8_in_max (gint8 v)
{
  g_assert_cmpint (v, ==, G_MAXINT8);
}

void
gi_marshalling_tests_int8_in_min (gint8 v)
{
  g_assert_cmpint (v, ==, G_MININT8);
}

/**
 * gi_marshalling_tests_int8_out_max:
 * @v: (out):
 */
void
gi_marshalling_tests_int8_out_max (gint8 *v)
{
  *v = G_MAXINT8;
}

/**
 * gi_marshalling_tests_int8_out_min:
 * @v: (out):
 */
void
gi_marshalling_tests_int8_out_min (gint8 *v)
{
  *v = G_MININT8;
}

/**
 * gi_marshalling_tests_int8_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_int8_out_uninitialized (gint8 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_int8_inout_max_min:
 * @v: (inout):
 */
void
gi_marshalling_tests_int8_inout_max_min (gint8 *v)
{
  g_assert_cmpint (*v, ==, G_MAXINT8);
  *v = G_MININT8;
}

/**
 * gi_marshalling_tests_int8_inout_min_max:
 * @v: (inout):
 */
void
gi_marshalling_tests_int8_inout_min_max (gint8 *v)
{
  g_assert_cmpint (*v, ==, G_MININT8);
  *v = G_MAXINT8;
}


guint8
gi_marshalling_tests_uint8_return (void)
{
  return G_MAXUINT8;
}

void
gi_marshalling_tests_uint8_in (guint8 v)
{
  g_assert_cmpuint (v, ==, G_MAXUINT8);
}

/**
 * gi_marshalling_tests_uint8_out:
 * @v: (out):
 */
void
gi_marshalling_tests_uint8_out (guint8 *v)
{
  *v = G_MAXUINT8;
}

/**
 * gi_marshalling_tests_uint8_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_uint8_out_uninitialized (guint8 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_uint8_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_uint8_inout (guint8 *v)
{
  g_assert_cmpuint (*v, ==, G_MAXUINT8);
  *v = 0;
}


gint16
gi_marshalling_tests_int16_return_max (void)
{
  return G_MAXINT16;
}

gint16
gi_marshalling_tests_int16_return_min (void)
{
  return G_MININT16;
}

void
gi_marshalling_tests_int16_in_max (gint16 v)
{
  g_assert_cmpint (v, ==, G_MAXINT16);
}

void
gi_marshalling_tests_int16_in_min (gint16 v)
{
  g_assert_cmpint (v, ==, G_MININT16);
}

/**
 * gi_marshalling_tests_int16_out_max:
 * @v: (out):
 */
void
gi_marshalling_tests_int16_out_max (gint16 *v)
{
  *v = G_MAXINT16;
}

/**
 * gi_marshalling_tests_int16_out_min:
 * @v: (out):
 */
void
gi_marshalling_tests_int16_out_min (gint16 *v)
{
  *v = G_MININT16;
}

/**
 * gi_marshalling_tests_int16_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_int16_out_uninitialized (gint16 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_int16_inout_max_min:
 * @v: (inout):
 */
void
gi_marshalling_tests_int16_inout_max_min (gint16 *v)
{
  g_assert_cmpint (*v, ==, G_MAXINT16);
  *v = G_MININT16;
}

/**
 * gi_marshalling_tests_int16_inout_min_max:
 * @v: (inout):
 */
void
gi_marshalling_tests_int16_inout_min_max (gint16 *v)
{
  g_assert_cmpint (*v, ==, G_MININT16);
  *v = G_MAXINT16;
}


guint16
gi_marshalling_tests_uint16_return (void)
{
  return G_MAXUINT16;
}

void
gi_marshalling_tests_uint16_in (guint16 v)
{
  g_assert_cmpuint (v, ==, G_MAXUINT16);
}

/**
 * gi_marshalling_tests_uint16_out:
 * @v: (out):
 */
void
gi_marshalling_tests_uint16_out (guint16 *v)
{
  *v = G_MAXUINT16;
}

/**
 * gi_marshalling_tests_uint16_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_uint16_out_uninitialized (guint16 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_uint16_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_uint16_inout (guint16 *v)
{
  g_assert_cmpuint (*v, ==, G_MAXUINT16);
  *v = 0;
}


gint32
gi_marshalling_tests_int32_return_max (void)
{
  return G_MAXINT32;
}

gint32
gi_marshalling_tests_int32_return_min (void)
{
  return G_MININT32;
}

void
gi_marshalling_tests_int32_in_max (gint32 v)
{
  g_assert_cmpint (v, ==, G_MAXINT32);
}

void
gi_marshalling_tests_int32_in_min (gint32 v)
{
  g_assert_cmpint (v, ==, G_MININT32);
}

/**
 * gi_marshalling_tests_int32_out_max:
 * @v: (out):
 */
void
gi_marshalling_tests_int32_out_max (gint32 *v)
{
  *v = G_MAXINT32;
}

/**
 * gi_marshalling_tests_int32_out_min:
 * @v: (out):
 */
void
gi_marshalling_tests_int32_out_min (gint32 *v)
{
  *v = G_MININT32;
}

/**
 * gi_marshalling_tests_int32_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_int32_out_uninitialized (gint32 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_int32_inout_max_min:
 * @v: (inout):
 */
void
gi_marshalling_tests_int32_inout_max_min (gint32 *v)
{
  g_assert_cmpint (*v, ==, G_MAXINT32);
  *v = G_MININT32;
}

/**
 * gi_marshalling_tests_int32_inout_min_max:
 * @v: (inout):
 */
void
gi_marshalling_tests_int32_inout_min_max (gint32 *v)
{
  g_assert_cmpint (*v, ==, G_MININT32);
  *v = G_MAXINT32;
}


guint32
gi_marshalling_tests_uint32_return (void)
{
  return G_MAXUINT32;
}

void
gi_marshalling_tests_uint32_in (guint32 v)
{
  g_assert_cmpuint (v, ==, G_MAXUINT32);
}

/**
 * gi_marshalling_tests_uint32_out:
 * @v: (out):
 */
void
gi_marshalling_tests_uint32_out (guint32 *v)
{
  *v = G_MAXUINT32;
}

/**
 * gi_marshalling_tests_uint32_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_uint32_out_uninitialized (guint32 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_uint32_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_uint32_inout (guint32 *v)
{
  g_assert_cmpuint (*v, ==, G_MAXUINT32);
  *v = 0;
}


gint64
gi_marshalling_tests_int64_return_max (void)
{
  return G_MAXINT64;
}

gint64
gi_marshalling_tests_int64_return_min (void)
{
  return G_MININT64;
}

void
gi_marshalling_tests_int64_in_max (gint64 v)
{
  g_assert_cmpint (v, ==, G_MAXINT64);
}

void
gi_marshalling_tests_int64_in_min (gint64 v)
{
  g_assert_cmpint (v, ==, G_MININT64);
}

/**
 * gi_marshalling_tests_int64_out_max:
 * @v: (out):
 */
void
gi_marshalling_tests_int64_out_max (gint64 *v)
{
  *v = G_MAXINT64;
}

/**
 * gi_marshalling_tests_int64_out_min:
 * @v: (out):
 */
void
gi_marshalling_tests_int64_out_min (gint64 *v)
{
  *v = G_MININT64;
}

/**
 * gi_marshalling_tests_int64_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_int64_out_uninitialized (gint64 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_int64_inout_max_min:
 * @v: (inout):
 */
void
gi_marshalling_tests_int64_inout_max_min (gint64 *v)
{
  g_assert_cmpint (*v, ==, G_MAXINT64);
  *v = G_MININT64;
}

/**
 * gi_marshalling_tests_int64_inout_min_max:
 * @v: (inout):
 */
void
gi_marshalling_tests_int64_inout_min_max (gint64 *v)
{
  g_assert_cmpint (*v, ==, G_MININT64);
  *v = G_MAXINT64;
}


guint64
gi_marshalling_tests_uint64_return (void)
{
  return G_MAXUINT64;
}

void
gi_marshalling_tests_uint64_in (guint64 v)
{
  g_assert_cmpuint (v, ==, G_MAXUINT64);
}

/**
 * gi_marshalling_tests_uint64_out:
 * @v: (out):
 */
void
gi_marshalling_tests_uint64_out (guint64 *v)
{
  *v = G_MAXUINT64;
}

/**
 * gi_marshalling_tests_uint64_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_uint64_out_uninitialized (guint64 *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_uint64_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_uint64_inout (guint64 *v)
{
  g_assert_cmpuint (*v, ==, G_MAXUINT64);
  *v = 0;
}


gshort
gi_marshalling_tests_short_return_max (void)
{
  return G_MAXSHORT;
}

gshort
gi_marshalling_tests_short_return_min (void)
{
  return G_MINSHORT;
}

void
gi_marshalling_tests_short_in_max (gshort short_)
{
  g_assert_cmpint (short_, ==, G_MAXSHORT);
}

void
gi_marshalling_tests_short_in_min (gshort short_)
{
  g_assert_cmpint (short_, ==, G_MINSHORT);
}

/**
 * gi_marshalling_tests_short_out_max:
 * @short_: (out):
 */
void
gi_marshalling_tests_short_out_max (gshort *short_)
{
  *short_ = G_MAXSHORT;
}

/**
 * gi_marshalling_tests_short_out_min:
 * @short_: (out):
 */
void
gi_marshalling_tests_short_out_min (gshort *short_)
{
  *short_ = G_MINSHORT;
}

/**
 * gi_marshalling_tests_short_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_short_out_uninitialized (gshort *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_short_inout_max_min:
 * @short_: (inout):
 */
void
gi_marshalling_tests_short_inout_max_min (gshort *short_)
{
  g_assert_cmpint (*short_, ==, G_MAXSHORT);
  *short_ = G_MINSHORT;
}

/**
 * gi_marshalling_tests_short_inout_min_max:
 * @short_: (inout):
 */
void
gi_marshalling_tests_short_inout_min_max (gshort *short_)
{
  g_assert_cmpint (*short_, ==, G_MINSHORT);
  *short_ = G_MAXSHORT;
}


gushort
gi_marshalling_tests_ushort_return (void)
{
  return G_MAXUSHORT;
}

void
gi_marshalling_tests_ushort_in (gushort ushort_)
{
  g_assert_cmpuint (ushort_, ==, G_MAXUSHORT);
}

/**
 * gi_marshalling_tests_ushort_out:
 * @ushort_: (out):
 */
void
gi_marshalling_tests_ushort_out (gushort *ushort_)
{
  *ushort_ = G_MAXUSHORT;
}

/**
 * gi_marshalling_tests_ushort_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_ushort_out_uninitialized (gushort *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ushort_inout:
 * @ushort_: (inout):
 */
void
gi_marshalling_tests_ushort_inout (gushort *ushort_)
{
  g_assert_cmpuint (*ushort_, ==, G_MAXUSHORT);
  *ushort_ = 0;
}


gint
gi_marshalling_tests_int_return_max (void)
{
  return G_MAXINT;
}

gint
gi_marshalling_tests_int_return_min (void)
{
  return G_MININT;
}

void
gi_marshalling_tests_int_in_max (gint int_)
{
  g_assert_cmpint (int_, ==, G_MAXINT);
}

void
gi_marshalling_tests_int_in_min (gint int_)
{
  g_assert_cmpint (int_, ==, G_MININT);
}

/**
 * gi_marshalling_tests_int_out_max:
 * @int_: (out):
 */
void
gi_marshalling_tests_int_out_max (gint *int_)
{
  *int_ = G_MAXINT;
}

/**
 * gi_marshalling_tests_int_out_min:
 * @int_: (out):
 */
void
gi_marshalling_tests_int_out_min (gint *int_)
{
  *int_ = G_MININT;
}

/**
 * gi_marshalling_tests_int_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_int_out_uninitialized (gint *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_int_inout_max_min:
 * @int_: (inout):
 */
void
gi_marshalling_tests_int_inout_max_min (gint *int_)
{
  g_assert_cmpint (*int_, ==, G_MAXINT);
  *int_ = G_MININT;
}

/**
 * gi_marshalling_tests_int_inout_min_max:
 * @int_: (inout):
 */
void
gi_marshalling_tests_int_inout_min_max (gint *int_)
{
  g_assert_cmpint (*int_, ==, G_MININT);
  *int_ = G_MAXINT;
}


guint
gi_marshalling_tests_uint_return (void)
{
  return G_MAXUINT;
}

void
gi_marshalling_tests_uint_in (guint uint_)
{
  g_assert_cmpuint (uint_, ==, G_MAXUINT);
}

/**
 * gi_marshalling_tests_uint_out:
 * @uint_: (out):
 */
void
gi_marshalling_tests_uint_out (guint *uint_)
{
  *uint_ = G_MAXUINT;
}

/**
 * gi_marshalling_tests_uint_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_uint_out_uninitialized (guint *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_uint_inout:
 * @uint_: (inout):
 */
void
gi_marshalling_tests_uint_inout (guint *uint_)
{
  g_assert_cmpuint (*uint_, ==, G_MAXUINT);
  *uint_ = 0;
}


glong
gi_marshalling_tests_long_return_max (void)
{
  return G_MAXLONG;
}

glong
gi_marshalling_tests_long_return_min (void)
{
  return G_MINLONG;
}

void
gi_marshalling_tests_long_in_max (glong long_)
{
  g_assert_cmpint (long_, ==, G_MAXLONG);
}

void
gi_marshalling_tests_long_in_min (glong long_)
{
  g_assert_cmpint (long_, ==, G_MINLONG);
}

/**
 * gi_marshalling_tests_long_out_max:
 * @long_: (out):
 */
void
gi_marshalling_tests_long_out_max (glong *long_)
{
  *long_ = G_MAXLONG;
}

/**
 * gi_marshalling_tests_long_out_min:
 * @long_: (out):
 */
void
gi_marshalling_tests_long_out_min (glong *long_)
{
  *long_ = G_MINLONG;
}

/**
 * gi_marshalling_tests_long_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_long_out_uninitialized (glong *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_long_inout_max_min:
 * @long_: (inout):
 */
void
gi_marshalling_tests_long_inout_max_min (glong *long_)
{
  g_assert_cmpint (*long_, ==, G_MAXLONG);
  *long_ = G_MINLONG;
}

/**
 * gi_marshalling_tests_long_inout_min_max:
 * @long_: (inout):
 */
void
gi_marshalling_tests_long_inout_min_max (glong *long_)
{
  g_assert_cmpint (*long_, ==, G_MINLONG);
  *long_ = G_MAXLONG;
}


gulong
gi_marshalling_tests_ulong_return (void)
{
  return G_MAXULONG;
}

void
gi_marshalling_tests_ulong_in (gulong ulong_)
{
  g_assert_cmpuint (ulong_, ==, G_MAXULONG);
}

/**
 * gi_marshalling_tests_ulong_out:
 * @ulong_: (out):
 */
void
gi_marshalling_tests_ulong_out (gulong *ulong_)
{
  *ulong_ = G_MAXULONG;
}

/**
 * gi_marshalling_tests_ulong_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_ulong_out_uninitialized (gulong *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ulong_inout:
 * @ulong_: (inout):
 */
void
gi_marshalling_tests_ulong_inout (gulong *ulong_)
{
  g_assert_cmpuint (*ulong_, ==, G_MAXULONG);
  *ulong_ = 0;
}


gssize
gi_marshalling_tests_ssize_return_max (void)
{
  return G_MAXSSIZE;
}

gssize
gi_marshalling_tests_ssize_return_min (void)
{
  return G_MINSSIZE;
}

void
gi_marshalling_tests_ssize_in_max (gssize ssize)
{
  g_assert_cmpint (ssize, ==, G_MAXSSIZE);
}

void
gi_marshalling_tests_ssize_in_min (gssize ssize)
{
  g_assert_cmpint (ssize, ==, G_MINSSIZE);
}

/**
 * gi_marshalling_tests_ssize_out_max:
 * @ssize: (out):
 */
void
gi_marshalling_tests_ssize_out_max (gssize *ssize)
{
  *ssize = G_MAXSSIZE;
}

/**
 * gi_marshalling_tests_ssize_out_min:
 * @ssize: (out):
 */
void
gi_marshalling_tests_ssize_out_min (gssize *ssize)
{
  *ssize = G_MINSSIZE;
}

/**
 * gi_marshalling_tests_ssize_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_ssize_out_uninitialized (gssize *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ssize_inout_max_min:
 * @ssize: (inout):
 */
void
gi_marshalling_tests_ssize_inout_max_min (gssize *ssize)
{
  g_assert_cmpint (*ssize, ==, G_MAXSSIZE);
  *ssize = G_MINSSIZE;
}

/**
 * gi_marshalling_tests_ssize_inout_min_max:
 * @ssize: (inout):
 */
void
gi_marshalling_tests_ssize_inout_min_max (gssize *ssize)
{
  g_assert_cmpint (*ssize, ==, G_MINSSIZE);
  *ssize = G_MAXSSIZE;
}


gsize
gi_marshalling_tests_size_return (void)
{
  return G_MAXSIZE;
}

void
gi_marshalling_tests_size_in (gsize size)
{
  g_assert_cmpuint (size, ==, G_MAXSIZE);
}

/**
 * gi_marshalling_tests_size_out:
 * @size: (out):
 */
void
gi_marshalling_tests_size_out (gsize *size)
{
  *size = G_MAXSIZE;
}

/**
 * gi_marshalling_tests_size_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_size_out_uninitialized (gsize *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_size_inout:
 * @size: (inout):
 */
void
gi_marshalling_tests_size_inout (gsize *size)
{
  g_assert_cmpuint (*size, ==, G_MAXSIZE);
  *size = 0;
}


gfloat
gi_marshalling_tests_float_return (void)
{
  return G_MAXFLOAT;
}

void
gi_marshalling_tests_float_in (gfloat v)
{
  g_assert_cmpfloat (v, ==, G_MAXFLOAT);
}

/**
 * gi_marshalling_tests_float_out:
 * @v: (out):
 */
void
gi_marshalling_tests_float_out (gfloat *v)
{
  *v = G_MAXFLOAT;
}

#define NONCANONICAL_NAN_BIT_PATTERN_32 0xfffb1236;
#define NONCANONICAL_NAN_BIT_PATTERN_64 0xfffb1236fedcba98;

static gfloat noncanonical_nan_float(void) {
  gfloat retval;

  if (sizeof (gfloat) == sizeof (guint32))
    {
      guint32 bit_pattern = NONCANONICAL_NAN_BIT_PATTERN_32;
      memcpy (&retval, &bit_pattern, sizeof(gfloat));
    }
  else
    {
      g_assert (sizeof (gfloat) == sizeof (guint64) && "gfloat must be 32 or 64 bits");
      guint64 bit_pattern = NONCANONICAL_NAN_BIT_PATTERN_64;
      memcpy (&retval, &bit_pattern, sizeof (gfloat));
    }

  return retval;
}

/**
 * gi_marshalling_tests_float_noncanonical_nan_out:
 * @v: (out):
 */
void
gi_marshalling_tests_float_noncanonical_nan_out (gfloat *v)
{
  *v = noncanonical_nan_float();
}

/**
 * gi_marshalling_tests_float_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_float_out_uninitialized (gfloat *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_float_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_float_inout (gfloat *v)
{
  g_assert_cmpfloat (*v, ==, G_MAXFLOAT);
  *v = G_MINFLOAT;
}


gdouble
gi_marshalling_tests_double_return (void)
{
  return G_MAXDOUBLE;
}

void
gi_marshalling_tests_double_in (gdouble v)
{
  g_assert_cmpfloat (v, ==, G_MAXDOUBLE);
}

/**
 * gi_marshalling_tests_double_out:
 * @v: (out):
 */
void
gi_marshalling_tests_double_out (gdouble *v)
{
  *v = G_MAXDOUBLE;
}

static gdouble noncanonical_nan_double(void)
{
  gdouble retval;

  g_assert (sizeof (gdouble) == sizeof (guint64) && "gdouble must be 64 bits");

  guint64 bit_pattern = NONCANONICAL_NAN_BIT_PATTERN_64;
  memcpy (&retval, &bit_pattern, sizeof (gdouble));

  return retval;
}

/**
 * gi_marshalling_tests_double_noncanonical_nan_out:
 * @v: (out):
 */
void
gi_marshalling_tests_double_noncanonical_nan_out (gdouble *v)
{
  *v = noncanonical_nan_double();
}

/**
 * gi_marshalling_tests_double_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_double_out_uninitialized (gdouble *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_double_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_double_inout (gdouble *v)
{
  g_assert_cmpfloat (*v, ==, G_MAXDOUBLE);
  *v = G_MINDOUBLE;
}


time_t
gi_marshalling_tests_time_t_return (void)
{
  return 1234567890;
}

void
gi_marshalling_tests_time_t_in (time_t v)
{
  g_assert_cmpuint (v, ==, 1234567890);
}

/**
 * gi_marshalling_tests_time_t_out:
 * @v: (out):
 */
void
gi_marshalling_tests_time_t_out (time_t *v)
{
  *v = 1234567890;
}

/**
 * gi_marshalling_tests_time_t_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_time_t_out_uninitialized (time_t *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_time_t_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_time_t_inout (time_t *v)
{
  g_assert_cmpuint (*v, ==, 1234567890);
  *v = 0;
}


off_t
gi_marshalling_tests_off_t_return (void)
{
  return 1234567890;
}

void
gi_marshalling_tests_off_t_in (off_t v)
{
  g_assert_cmpuint (v, ==, 1234567890);
}

/**
 * gi_marshalling_tests_off_t_out:
 * @v: (out):
 */
void
gi_marshalling_tests_off_t_out (off_t *v)
{
  *v = 1234567890;
}

/**
 * gi_marshalling_tests_off_t_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_off_t_out_uninitialized (off_t *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_off_t_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_off_t_inout (off_t *v)
{
  g_assert_cmpuint (*v, ==, 1234567890);
  *v = 0;
}


GType
gi_marshalling_tests_gtype_return (void)
{
  return G_TYPE_NONE;
}

GType
gi_marshalling_tests_gtype_string_return (void)
{
  return G_TYPE_STRING;
}

void
gi_marshalling_tests_gtype_in (GType gtype)
{
  g_assert (gtype == G_TYPE_NONE);
}

void
gi_marshalling_tests_gtype_string_in (GType gtype)
{
  g_assert (gtype == G_TYPE_STRING);
}


/**
 * gi_marshalling_tests_gtype_out:
 * @gtype: (out):
 */
void
gi_marshalling_tests_gtype_out (GType *gtype)
{
  *gtype = G_TYPE_NONE;
}

/**
 * gi_marshalling_tests_gtype_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_gtype_out_uninitialized (GType *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gtype_string_out:
 * @gtype: (out):
 */
void
gi_marshalling_tests_gtype_string_out (GType *gtype)
{
  *gtype = G_TYPE_STRING;
}

/**
 * gi_marshalling_tests_gtype_inout:
 * @gtype: (inout):
 */
void
gi_marshalling_tests_gtype_inout (GType *gtype)
{
  g_assert (*gtype == G_TYPE_NONE);
  *gtype = G_TYPE_INT;
}


const gchar *
gi_marshalling_tests_utf8_none_return (void)
{
  return GI_MARSHALLING_TESTS_CONSTANT_UTF8;
}

gchar *
gi_marshalling_tests_utf8_full_return (void)
{
  return g_strdup (GI_MARSHALLING_TESTS_CONSTANT_UTF8);
}

void
gi_marshalling_tests_utf8_none_in (const gchar *utf8)
{
  g_assert_cmpstr (GI_MARSHALLING_TESTS_CONSTANT_UTF8, ==, utf8);
}

/**
 * gi_marshalling_tests_utf8_as_uint8array_in:
 * @array: (array length=len) (element-type guint8): Byte data that happens to be UTF-8
 * @len: Length
 *
 * Takes data that happens to be UTF-8 as a byte array, to test
 * binding conversion from their string type (e.g. JavaScript's
 * UTF-16) to UTF-8.
 */
void
gi_marshalling_tests_utf8_as_uint8array_in (const guint8 *array, gsize len)
{
  gsize orig_len = strlen (GI_MARSHALLING_TESTS_CONSTANT_UTF8);
  g_assert_cmpint (orig_len, ==, len);
  g_assert (memcmp (GI_MARSHALLING_TESTS_CONSTANT_UTF8, array, len) == 0);
}

/**
 * gi_marshalling_tests_utf8_none_out:
 * @utf8: (out) (transfer none):
 */
void
gi_marshalling_tests_utf8_none_out (const gchar **utf8)
{
  *utf8 = GI_MARSHALLING_TESTS_CONSTANT_UTF8;
}

/**
 * gi_marshalling_tests_utf8_none_out_uninitialized:
 * @v: (out) (transfer none):
 */
gboolean
gi_marshalling_tests_utf8_none_out_uninitialized (const gchar **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_utf8_full_out:
 * @utf8: (out) (transfer full):
 */
void
gi_marshalling_tests_utf8_full_out (gchar **utf8)
{
  *utf8 = g_strdup (GI_MARSHALLING_TESTS_CONSTANT_UTF8);
}

/**
 * gi_marshalling_tests_utf8_dangling_out:
 * @utf8: (out) (transfer full):
 */
void
gi_marshalling_tests_utf8_dangling_out (gchar **utf8 G_GNUC_UNUSED)
{
  /* Intentionally don't touch the pointer to see how
     the bindings handle this case.  Bindings should be
     robust against broken C functions and can initialize
     even OUT vlues to NULL.
   */
}

/**
 * gi_marshalling_tests_utf8_none_inout:
 * @utf8: (inout) (transfer none):
 */
void
gi_marshalling_tests_utf8_none_inout (const gchar **utf8)
{
  g_assert_cmpstr (GI_MARSHALLING_TESTS_CONSTANT_UTF8, ==, *utf8);
  *utf8 = "";
}

/**
 * gi_marshalling_tests_utf8_full_inout:
 * @utf8: (inout) (transfer full):
 */
void
gi_marshalling_tests_utf8_full_inout (gchar **utf8)
{
  g_assert_cmpstr (GI_MARSHALLING_TESTS_CONSTANT_UTF8, ==, *utf8);
  g_free (*utf8);
  *utf8 = g_strdup ("");
}


/**
 * gi_marshalling_tests_init_function:
 * @n_args: (inout) (allow-none): number of args
 * @argv: (inout) (array length=n_args) (allow-none): args
 *
 * This is like gtk_init().
 */
gboolean
gi_marshalling_tests_init_function (gint *n_args, char ***argv)
{
  if (n_args == NULL)
    return TRUE;

  if (*n_args == 0)
    return TRUE;
  (*n_args)--;
  g_assert (argv != NULL);
  /* we have transfer ownership full, so we need to free the element ourself */
  g_free ((*argv)[*n_args]);
  (*argv)[*n_args] = NULL;
  return TRUE;
}

/**
 * gi_marshalling_tests_array_fixed_int_return:
 *
 * Returns: (array fixed-size=4):
 */
const gint *
gi_marshalling_tests_array_fixed_int_return (void)
{
  static gint ints[] = { -1, 0, 1, 2 };
  return ints;
}

/**
 * gi_marshalling_tests_array_fixed_short_return:
 *
 * Returns: (array fixed-size=4):
 */
const gshort *
gi_marshalling_tests_array_fixed_short_return (void)
{
  static gshort shorts[] = { -1, 0, 1, 2 };
  return shorts;
}

/**
 * gi_marshalling_tests_array_fixed_int_in:
 * @ints: (array fixed-size=4):
 */
void
gi_marshalling_tests_array_fixed_int_in (const gint *ints)
{
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);
}

/**
 * gi_marshalling_tests_array_fixed_caller_allocated_out:
 * @ints: (out caller-allocates) (array fixed-size=4):
 */
void
gi_marshalling_tests_array_fixed_caller_allocated_out (gint *ints)
{
  ints[0] = -1;
  ints[1] = 0;
  ints[2] = 1;
  ints[3] = 2;
}

/**
 * gi_marshalling_tests_array_fixed_short_in:
 * @shorts: (array fixed-size=4):
 */
void
gi_marshalling_tests_array_fixed_short_in (const gshort *shorts)
{
  g_assert_cmpint (shorts[0], ==, -1);
  g_assert_cmpint (shorts[1], ==, 0);
  g_assert_cmpint (shorts[2], ==, 1);
  g_assert_cmpint (shorts[3], ==, 2);
}

/**
 * gi_marshalling_tests_array_fixed_out:
 * @ints: (out) (array fixed-size=4) (transfer none):
 */
void
gi_marshalling_tests_array_fixed_out (gint **ints)
{
  static gint values[] = { -1, 0, 1, 2 };
  *ints = values;
}

/**
 * gi_marshalling_tests_array_fixed_out_uninitialized:
 * @v: (out) (array fixed-size=4) (transfer none):
 */
gboolean
gi_marshalling_tests_array_fixed_out_uninitialized (gint **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_array_fixed_out_struct:
 * @structs: (out) (array fixed-size=2) (transfer none):
 */
void
gi_marshalling_tests_array_fixed_out_struct (GIMarshallingTestsSimpleStruct **structs)
{
  static GIMarshallingTestsSimpleStruct *values;

  if (values == NULL)
    {
      values = g_new (GIMarshallingTestsSimpleStruct, 2);

      values[0].long_ = 7;
      values[0].int8 = 6;

      values[1].long_ = 6;
      values[1].int8 = 7;
    }

  *structs = values;
}

/**
 * gi_marshalling_tests_array_fixed_out_struct_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_array_fixed_out_struct_uninitialized (GIMarshallingTestsSimpleStruct **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_array_fixed_caller_allocated_struct_out:
 * @structs: (out caller-allocates) (array fixed-size=4):
 */
void
gi_marshalling_tests_array_fixed_caller_allocated_struct_out (GIMarshallingTestsSimpleStruct *structs)
{
    structs[0].long_ = -2;
    structs[0].int8 = -1;
    structs[1].long_ = 1;
    structs[1].int8 = 2;
    structs[2].long_ = 3;
    structs[2].int8 = 4;
    structs[3].long_ = 5;
    structs[3].int8 = 6;
}

/**
 * gi_marshalling_tests_array_fixed_inout:
 * @ints: (inout) (array fixed-size=4) (transfer none):
 */
void
gi_marshalling_tests_array_fixed_inout (gint **ints)
{
  static gint values[] = { 2, 1, 0, -1 };

  g_assert_cmpint ((*ints)[0], ==, -1);
  g_assert_cmpint ((*ints)[1], ==, 0);
  g_assert_cmpint ((*ints)[2], ==, 1);
  g_assert_cmpint ((*ints)[3], ==, 2);

  *ints = values;
}


/**
 * gi_marshalling_tests_array_return:
 *
 * Returns: (array length=length):
 */
const gint *
gi_marshalling_tests_array_return (gint *length)
{
  static gint ints[] = { -1, 0, 1, 2 };

  *length = 4;
  return ints;
}

/**
 * gi_marshalling_tests_array_return_etc:
 * @first:
 * @length: (out):
 * @last:
 * @sum: (out):
 *
 * Returns: (array length=length):
 */
const gint *
gi_marshalling_tests_array_return_etc (gint first, gint *length, gint last, gint *sum)
{
  static gint ints[] = { -1, 0, 1, 2 };

  ints[0] = first;
  ints[3] = last;
  *sum = first + last;
  *length = 4;
  return ints;
}

/**
 * gi_marshalling_tests_array_in:
 * @ints: (array length=length):
 * @length:
 */
void
gi_marshalling_tests_array_in (const gint *ints, gint length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);
}

/**
 * gi_marshalling_tests_array_in_len_before:
 * @length:
 * @ints: (array length=length):
 */
void
gi_marshalling_tests_array_in_len_before (gint length, const gint *ints)
{
  gi_marshalling_tests_array_in (ints, length);
}

/**
 * gi_marshalling_tests_array_in_len_zero_terminated:
 * @ints: (array length=length zero-terminated):
 * @length:
 */
void
gi_marshalling_tests_array_in_len_zero_terminated (const gint *ints, gint length)
{
  g_assert_cmpint (length, ==, 4);

  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);

  /* One past the end, null terminator */
  g_assert_cmpint (ints[4], ==, 0);
}

/**
 * gi_marshalling_tests_array_string_in:
 * @strings: (array length=length):
 */
void
gi_marshalling_tests_array_string_in (const gchar **strings, gint length)
{
  g_assert_cmpint (length, ==, 2);
  g_assert_cmpstr (strings[0], ==, "foo");
  g_assert_cmpstr (strings[1], ==, "bar");
}

/**
 * gi_marshalling_tests_array_uint8_in:
 * @chars: (array length=length):
 */
void
gi_marshalling_tests_array_uint8_in (const guint8 *chars, gint length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert (chars[0] == 'a');
  g_assert (chars[1] == 'b');
  g_assert (chars[2] == 'c');
  g_assert (chars[3] == 'd');
}

/**
 * gi_marshalling_tests_array_int64_in:
 * @ints: (array length=length):
 * @length:
 */
void
gi_marshalling_tests_array_int64_in (const gint64 *ints, gint length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);
}

/**
 * gi_marshalling_tests_array_uint64_in:
 * @ints: (array length=length):
 * @length:
 */
void
gi_marshalling_tests_array_uint64_in (const guint64 *ints, gint length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);
}

/**
 * gi_marshalling_tests_array_unichar_in:
 * @chars: (array length=length):
 * @length:
 */
void
gi_marshalling_tests_array_unichar_in (const gunichar *chars, gint length)
{
  int ix;
  static const gunichar expected[] = GI_MARSHALLING_TESTS_CONSTANT_UCS4;
  g_assert_cmpint (length, ==, 12);
  for (ix = 0; ix < length; ix++)
    g_assert_cmpuint (chars[ix], ==, expected[ix]);
}

/**
 * gi_marshalling_tests_array_bool_in:
 * @bools: (array length=length):
 * @length:
 */
void
gi_marshalling_tests_array_bool_in (const gboolean *bools, gint length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (bools[0], ==, TRUE);
  g_assert_cmpint (bools[1], ==, FALSE);
  g_assert_cmpint (bools[2], ==, TRUE);
  g_assert_cmpint (bools[3], ==, TRUE);
}

/**
 * gi_marshalling_tests_array_struct_in:
 * @structs: (array length=length):
 */
void
gi_marshalling_tests_array_struct_in (GIMarshallingTestsBoxedStruct **structs, gint length)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpint (structs[0]->long_, ==, 1);
  g_assert_cmpint (structs[1]->long_, ==, 2);
  g_assert_cmpint (structs[2]->long_, ==, 3);
}

/**
 * gi_marshalling_tests_array_struct_value_in:
 * @structs: (array length=length):
 */
void
gi_marshalling_tests_array_struct_value_in (GIMarshallingTestsBoxedStruct *structs, gint length)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpint (structs[0].long_, ==, 1);
  g_assert_cmpint (structs[1].long_, ==, 2);
  g_assert_cmpint (structs[2].long_, ==, 3);
}

/**
 * gi_marshalling_tests_array_simple_struct_in:
 * @structs: (array length=length):
 */
void
gi_marshalling_tests_array_simple_struct_in (GIMarshallingTestsSimpleStruct *structs, gint length)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpint (structs[0].long_, ==, 1);
  g_assert_cmpint (structs[1].long_, ==, 2);
  g_assert_cmpint (structs[2].long_, ==, 3);
}

/**
 * gi_marshalling_tests_multi_array_key_value_in:
 * @keys: (array length=length):
 * @values: (array length=length):
 */
void
gi_marshalling_tests_multi_array_key_value_in (gint length, const gchar **keys, const GValue *values)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpstr ("one", ==, keys[0]);
  g_assert_cmpint (g_value_get_int (&values[0]), ==, 1);
  g_assert_cmpstr ("two", ==, keys[1]);
  g_assert_cmpint (g_value_get_int (&values[1]), ==, 2);
  g_assert_cmpstr ("three", ==, keys[2]);
  g_assert_cmpint (g_value_get_int (&values[2]), ==, 3);

}

/**
 * gi_marshalling_tests_array_struct_take_in:
 * @structs: (array length=length) (transfer full):
 */
void
gi_marshalling_tests_array_struct_take_in (GIMarshallingTestsBoxedStruct **structs, gint length)
{
  gi_marshalling_tests_array_struct_in (structs, length);

  /* only really useful if run in valgrind actually */
  gi_marshalling_tests_boxed_struct_free (structs[0]);
  gi_marshalling_tests_boxed_struct_free (structs[1]);
  gi_marshalling_tests_boxed_struct_free (structs[2]);
  g_free (structs);
}

/**
 * gi_marshalling_tests_array_enum_in:
 * @_enum: (array length=length) (transfer none):
 * @length:
 */
void
gi_marshalling_tests_array_enum_in (GIMarshallingTestsEnum *v, gint length)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpint (v[0], ==, GI_MARSHALLING_TESTS_ENUM_VALUE1);
  g_assert_cmpint (v[1], ==, GI_MARSHALLING_TESTS_ENUM_VALUE2);
  g_assert_cmpint (v[2], ==, GI_MARSHALLING_TESTS_ENUM_VALUE3);
}

/**
 * gi_marshalling_tests_array_flags_in:
 * @flags: (array length=length) (transfer none):
 * @length:
 */
void
gi_marshalling_tests_array_flags_in (GIMarshallingTestsFlags *v, gint length)
{
  g_assert_cmpint (length, ==, 3);
  g_assert_cmpint (v[0], ==, GI_MARSHALLING_TESTS_FLAGS_VALUE1);
  g_assert_cmpint (v[1], ==, GI_MARSHALLING_TESTS_FLAGS_VALUE2);
  g_assert_cmpint (v[2], ==, GI_MARSHALLING_TESTS_FLAGS_VALUE3);
}

/**
 * gi_marshalling_tests_array_in_guint64_len:
 * @ints: (array length=length) (transfer none):
 * @length:
 */
void
gi_marshalling_tests_array_in_guint64_len (const gint *ints, guint64 length)
{
  g_assert_cmpint (length, ==, 4);

  gi_marshalling_tests_array_in (ints, length);
}

/**
 * gi_marshalling_tests_array_in_guint8_len:
 * @ints: (array length=length) (transfer none):
 * @length:
 */
void
gi_marshalling_tests_array_in_guint8_len (const gint *ints, guint8 length)
{
  g_assert_cmpint (length, ==, 4);

  gi_marshalling_tests_array_in (ints, length);
}

/**
 * gi_marshalling_tests_array_out:
 * @ints: (out) (array length=length) (transfer none):
 */
void
gi_marshalling_tests_array_out (gint **ints, gint *length)
{
  static gint values[] = { -1, 0, 1, 2 };

  *length = 4;
  *ints = values;
}

/**
 * gi_marshalling_tests_array_out_uninitialized:
 * @v: (out) (array length=length) (transfer none):
 * @length:
 */
gboolean
gi_marshalling_tests_array_out_uninitialized (gint **v G_GNUC_UNUSED, gint *length G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_array_out_etc:
 * @first:
 * @ints: (out) (array length=length) (transfer none):
 * @length: (out):
 * @last:
 * @sum: (out):
 */
void
gi_marshalling_tests_array_out_etc (gint first, gint **ints, gint *length, gint last, gint *sum)
{
  static gint values[] = { -1, 0, 1, 2 };

  values[0] = first;
  values[3] = last;
  *sum = first + last;
  *length = 4;
  *ints = values;
}

/**
 * gi_marshalling_tests_array_bool_out:
 * @bools: (out) (array length=length) (transfer none):
 */
void
gi_marshalling_tests_array_bool_out (const gboolean **bools, gint *length)
{
  static const gboolean values[] = { TRUE, FALSE, TRUE, TRUE };

  *length = 4;
  *bools = values;
}

/**
 * gi_marshalling_tests_array_unichar_out:
 * @chars: (out) (array length=length) (transfer none):
 */
void
gi_marshalling_tests_array_unichar_out (const gunichar **chars, gint *length)
{
  static const gunichar values[] = GI_MARSHALLING_TESTS_CONSTANT_UCS4;
  *length = 12;
  *chars = values;
}

/**
 * gi_marshalling_tests_array_inout:
 * @ints: (inout) (array length=length) (transfer none):
 * @length: (inout):
 */
void
gi_marshalling_tests_array_inout (gint **ints, gint *length)
{
  static gint values[] = { -2, -1, 0, 1, 2 };

  g_assert_cmpint (*length, ==, 4);
  g_assert_cmpint ((*ints)[0], ==, -1);
  g_assert_cmpint ((*ints)[1], ==, 0);
  g_assert_cmpint ((*ints)[2], ==, 1);
  g_assert_cmpint ((*ints)[3], ==, 2);

  *length = 5;
  *ints = values;
}

/**
 * gi_marshalling_tests_array_inout_etc:
 * @first:
 * @ints: (inout) (array length=length) (transfer none):
 * @length: (inout):
 * @last:
 * @sum: (out):
 */
void
gi_marshalling_tests_array_inout_etc (gint first, gint **ints, gint *length, gint last, gint *sum)
{
  static gint values[] = { -2, -1, 0, 1, 2 };

  g_assert_cmpint (*length, ==, 4);
  g_assert_cmpint ((*ints)[0], ==, -1);
  g_assert_cmpint ((*ints)[1], ==, 0);
  g_assert_cmpint ((*ints)[2], ==, 1);
  g_assert_cmpint ((*ints)[3], ==, 2);

  values[0] = first;
  values[4] = last;
  *sum = first + last;
  *length = 5;
  *ints = values;
}

/**
 * gi_marshalling_tests_array_in_nonzero_nonlen:
 * @first:
 * @chars: (array):
 */
void
gi_marshalling_tests_array_in_nonzero_nonlen (gint          first G_GNUC_UNUSED,
                                              const guint8 *chars)
{
  g_assert (chars[0] == 'a');
  g_assert (chars[1] == 'b');
  g_assert (chars[2] == 'c');
  g_assert (chars[3] == 'd');
}

/**
 * gi_marshalling_tests_array_zero_terminated_return:
 *
 * Returns: (array zero-terminated) (transfer none):
 */
const gchar **
gi_marshalling_tests_array_zero_terminated_return (void)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  return values;
}

/**
 * gi_marshalling_tests_array_zero_terminated_return_null:
 *
 * Returns: (array zero-terminated) (transfer none):
 */
gchar **
gi_marshalling_tests_array_zero_terminated_return_null (void)
{
  return NULL;
}

/**
 * gi_marshalling_tests_array_zero_terminated_return_struct:
 *
 * Returns: (array zero-terminated) (transfer full):
 */
GIMarshallingTestsBoxedStruct **
gi_marshalling_tests_array_zero_terminated_return_struct (void)
{
  GIMarshallingTestsBoxedStruct **ret = (GIMarshallingTestsBoxedStruct **) g_new (gpointer, 4);

  ret[0] = gi_marshalling_tests_boxed_struct_new ();
  ret[0]->long_ = 42;

  ret[1] = gi_marshalling_tests_boxed_struct_new ();
  ret[1]->long_ = 43;

  ret[2] = gi_marshalling_tests_boxed_struct_new ();
  ret[2]->long_ = 44;

  ret[3] = NULL;

  return ret;
}

/**
 * gi_marshalling_tests_array_zero_terminated_return_unichar:
 *
 * Returns: (array zero-terminated) (transfer full):
 */
gunichar *
gi_marshalling_tests_array_zero_terminated_return_unichar (void)
{
  static const gunichar value[] = GI_MARSHALLING_TESTS_CONSTANT_UCS4;
  gunichar *retval = g_new0(gunichar, 13);
  memcpy (retval, value, 12 * sizeof (gunichar));
  return retval;
}

/**
 * gi_marshalling_tests_array_zero_terminated_in:
 * @utf8s: (array zero-terminated) (transfer none):
 */
void
gi_marshalling_tests_array_zero_terminated_in (gchar **utf8s)
{
  g_assert (g_strv_length (utf8s));
  g_assert_cmpstr (utf8s[0], ==, "0");
  g_assert_cmpstr (utf8s[1], ==, "1");
  g_assert_cmpstr (utf8s[2], ==, "2");
}

/**
 * gi_marshalling_tests_array_zero_terminated_out:
 * @utf8s: (out) (array zero-terminated) (transfer none):
 */
void
gi_marshalling_tests_array_zero_terminated_out (const gchar ***utf8s)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  *utf8s = values;
}

/**
 * gi_marshalling_tests_array_zero_terminated_out_uninitialized:
 * @v: (out) (array zero-terminated) (transfer none):
 */
gboolean
gi_marshalling_tests_array_zero_terminated_out_uninitialized (const gchar ***v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_array_zero_terminated_inout:
 * @utf8s: (inout) (array zero-terminated) (transfer none):
 */
void
gi_marshalling_tests_array_zero_terminated_inout (const gchar ***utf8s)
{
  static const gchar *values[] = { "-1", "0", "1", "2", NULL };

  g_assert (g_strv_length ((gchar **) (*utf8s)));
  g_assert_cmpstr ((*utf8s)[0], ==, "0");
  g_assert_cmpstr ((*utf8s)[1], ==, "1");
  g_assert_cmpstr ((*utf8s)[2], ==, "2");

  *utf8s = values;
}

/**
 * gi_marshalling_tests_array_gvariant_none_in:
 * @variants: (array zero-terminated) (transfer none):
 *
 * Returns: (array zero-terminated) (transfer none):
 */
GVariant **
gi_marshalling_tests_array_gvariant_none_in (GVariant **variants)
{
  /* Use a static container to detect if someone tries to free it */
  static GVariant *private_container[3] = { NULL, NULL, NULL };

  if (private_container[0] == NULL)
    {
      private_container[0] = g_variant_new_int32 (27);
      private_container[1] = g_variant_new_string ("Hello");
    }

  g_assert (variants != NULL);
  g_assert_cmpint (g_variant_get_int32 (variants[0]), ==, 27);
  g_assert_cmpstr (g_variant_get_string (variants[1], NULL), ==, "Hello");
  g_assert (variants[2] == NULL);

  return private_container;
}

/**
 * gi_marshalling_tests_array_gvariant_container_in:
 * @variants: (array zero-terminated) (transfer container):
 *
 * Returns: (array zero-terminated) (transfer container):
 */
GVariant **
gi_marshalling_tests_array_gvariant_container_in (GVariant **variants)
{
  GVariant **container;

  g_assert (variants != NULL);
  g_assert_cmpint (g_variant_get_int32 (variants[0]), ==, 27);
  g_assert_cmpstr (g_variant_get_string (variants[1], NULL), ==, "Hello");
  g_assert (variants[2] == NULL);

  container = g_new0 (GVariant *, 3);
  /* This is a floating reference, so it's fine for transfer container */
  container[0] = g_variant_new_int32 (g_variant_get_int32 (variants[0]));
  container[1] = variants[1];
  g_free (variants);

  return container;
}

/**
 * gi_marshalling_tests_array_gvariant_full_in:
 * @variants: (array zero-terminated) (transfer full):
 *
 * Returns: (array zero-terminated) (transfer full):
 */
GVariant **
gi_marshalling_tests_array_gvariant_full_in (GVariant **variants)
{
  GVariant **container;

  g_assert (variants != NULL);
  g_assert_cmpint (g_variant_get_int32 (variants[0]), ==, 27);
  g_assert_cmpstr (g_variant_get_string (variants[1], NULL), ==, "Hello");
  g_assert (variants[2] == NULL);

  /* To catch different behaviors we reconstruct one variant from scratch,
   * while taking the refernce of the other. Both approaches are legal with full
   * transfer in and out */
  container = g_new0 (GVariant *, 3);
  container[0] = g_variant_ref_sink (
    g_variant_new_int32 (g_variant_get_int32 (variants[0])));
  g_variant_unref (variants[0]);

  /* In case the variant is floating, we want to transform it into a full
   * reference, so that's fully owned by the container like if the case
   * above, otherwise we just steal it since it has already a strong reference.
   */
  container[1] = g_variant_take_ref (variants[1]);
  g_free (variants);

  return container;
}

/**
 * gi_marshalling_tests_garray_int_none_return:
 *
 * Returns: (element-type gint) (transfer none):
 */
GArray *
gi_marshalling_tests_garray_int_none_return (void)
{
  static GArray *v = NULL;
  gint i;

  if (v == NULL)
    {
      v = g_array_new (TRUE, TRUE, sizeof (gint));
      for (i = -1; i < 3; i++)
        g_array_append_val (v, i);
    }

  return v;
}

/**
 * gi_marshalling_tests_garray_uint64_none_return:
 *
 * Returns: (element-type guint64) (transfer none):
 */
GArray *
gi_marshalling_tests_garray_uint64_none_return (void)
{
  static GArray *array = NULL;
  guint64 i;

  if (array == NULL)
    {
      array = g_array_new (TRUE, TRUE, sizeof (guint64));
      i = 0;
      g_array_append_val (array, i);
      i = G_MAXUINT64;
      g_array_append_val (array, i);
    }

  return array;
}

/**
 * gi_marshalling_tests_garray_utf8_none_return:
 *
 * Returns: (element-type utf8) (transfer none):
 */
GArray *
gi_marshalling_tests_garray_utf8_none_return (void)
{
  static GArray *array = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  if (array == NULL)
    {
      array = g_array_new (TRUE, TRUE, sizeof (gchar *));
      for (i = 0; values[i]; i++)
        g_array_append_val (array, values[i]);
    }

  return array;
}

/**
 * gi_marshalling_tests_garray_utf8_container_return:
 *
 * Returns: (element-type utf8) (transfer container):
 */
GArray *
gi_marshalling_tests_garray_utf8_container_return (void)
{
  GArray *array = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  array = g_array_new (TRUE, TRUE, sizeof (gchar *));
  for (i = 0; values[i]; i++)
    g_array_append_val (array, values[i]);

  return array;
}

/**
 * gi_marshalling_tests_garray_utf8_full_return:
 *
 * Returns: (element-type utf8) (transfer full):
 */
GArray *
gi_marshalling_tests_garray_utf8_full_return (void)
{
  GArray *array = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  array = g_array_new (TRUE, TRUE, sizeof (gchar *));
  for (i = 0; values[i]; i++)
    {
      gchar *str = g_strdup (values[i]);
      g_array_append_val (array, str);
    }

  return array;
}

/**
 * gi_marshalling_tests_garray_boxed_struct_full_return:
 *
 * Returns: (element-type GIMarshallingTestsBoxedStruct) (transfer full):
 */
GArray *
gi_marshalling_tests_garray_boxed_struct_full_return (void)
{
  GArray *array = NULL;
  static const glong long_values[] = { 42, 43, 44 };
  gint i;

  array = g_array_new (TRUE, TRUE, sizeof (GIMarshallingTestsBoxedStruct));
  g_array_set_size (array, 3);
  for (i = 0; i < 3; i++)
    {
      GIMarshallingTestsBoxedStruct *new_struct;
      new_struct = &g_array_index (array, GIMarshallingTestsBoxedStruct, i);
      memset (new_struct, 0, sizeof (GIMarshallingTestsSimpleStruct));
      new_struct->long_ = long_values[i];
    }

  return array;
}

/**
 * gi_marshalling_tests_garray_int_none_in:
 * @array_: (element-type gint) (transfer none):
 */
void
gi_marshalling_tests_garray_int_none_in (GArray *array_)
{
  g_assert_cmpint (array_->len, ==, 4);
  g_assert_cmpint (g_array_index (array_, gint, 0), ==, -1);
  g_assert_cmpint (g_array_index (array_, gint, 1), ==, 0);
  g_assert_cmpint (g_array_index (array_, gint, 2), ==, 1);
  g_assert_cmpint (g_array_index (array_, gint, 3), ==, 2);
}

/**
 * gi_marshalling_tests_garray_uint64_none_in:
 * @array_: (element-type guint64) (transfer none):
 */
void
gi_marshalling_tests_garray_uint64_none_in (GArray *array_)
{
  g_assert_cmpint (array_->len, ==, 2);
  g_assert_cmpint (g_array_index (array_, guint64, 0), ==, 0);
  g_assert_cmpint (g_array_index (array_, guint64, 1), ==, G_MAXUINT64);
}

/**
 * gi_marshalling_tests_garray_utf8_none_in:
 * @array_: (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_garray_utf8_none_in (GArray *array_)
{
  g_assert_cmpint (array_->len, ==, 3);
  g_assert_cmpstr (g_array_index (array_, gchar *, 0), ==, "0");
  g_assert_cmpstr (g_array_index (array_, gchar *, 1), ==, "1");
  g_assert_cmpstr (g_array_index (array_, gchar *, 2), ==, "2");
}

/**
 * gi_marshalling_tests_garray_utf8_none_out:
 * @array_: (out) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_garray_utf8_none_out (GArray **array_)
{
  static GArray *internal = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  if (internal == NULL)
    {
      internal = g_array_new (TRUE, TRUE, sizeof (gchar *));
      for (i = 0; values[i]; i++)
        g_array_append_val (internal, values[i]);
    }

  *array_ = internal;
}

/**
 * gi_marshalling_tests_garray_utf8_none_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer none):
 */
gboolean
gi_marshalling_tests_garray_utf8_none_out_uninitialized (GArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_garray_utf8_container_out:
 * @array_: (out) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_garray_utf8_container_out (GArray **array_)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  *array_ = NULL;

  *array_ = g_array_new (TRUE, TRUE, sizeof (gchar *));
  for (i = 0; values[i]; i++)
    g_array_append_val (*array_, values[i]);
}

/**
 * gi_marshalling_tests_garray_utf8_container_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer container):
 */
gboolean
gi_marshalling_tests_garray_utf8_container_out_uninitialized (GArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_garray_utf8_full_out:
 * @array_: (out) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_garray_utf8_full_out (GArray **array_)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  *array_ = NULL;

  *array_ = g_array_new (TRUE, TRUE, sizeof (gchar *));
  for (i = 0; values[i]; i++)
    {
      gchar *str = g_strdup (values[i]);
      g_array_append_val (*array_, str);
    }
}

/**
 * gi_marshalling_tests_garray_utf8_full_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer full):
 */
gboolean
gi_marshalling_tests_garray_utf8_full_out_uninitialized (GArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_garray_utf8_full_out_caller_allocated:
 * @array_: (out caller-allocates) (array) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_garray_utf8_full_out_caller_allocated (GArray *array_)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  g_array_set_size (array_, 0);
  for (i = 0; values[i]; i++)
    {
      gchar *str = g_strdup (values[i]);
      g_array_append_val (array_, str);
    }
}

/**
 * gi_marshalling_tests_garray_utf8_none_inout:
 * @array_: (inout) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_garray_utf8_none_inout (GArray **array_)
{
  static GArray *internal = NULL;
  static const gchar *values[] = { "-2", "-1", "0", "1", NULL };
  gint i;

  g_assert_cmpint ((*array_)->len, ==, 3);
  g_assert_cmpstr (g_array_index (*array_, gchar *, 0), ==, "0");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 1), ==, "1");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 2), ==, "2");

  if (internal == NULL)
    {
      internal = g_array_new (TRUE, TRUE, sizeof (gchar *));
      for (i = 0; values[i]; i++)
        g_array_append_val (internal, values[i]);
    }

  *array_ = internal;
}

/**
 * gi_marshalling_tests_garray_utf8_container_inout:
 * @array_: (inout) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_garray_utf8_container_inout (GArray **array_)
{
  static const gchar *val1 = "-2";
  static const gchar *val2 = "-1";
  static const gchar *val3 = "0";
  static const gchar *val4 = "1";
  GArray *result;

  g_assert_cmpint ((*array_)->len, ==, 3);
  g_assert_cmpstr (g_array_index (*array_, gchar *, 0), ==, "0");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 1), ==, "1");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 2), ==, "2");

  result = g_array_new (TRUE, TRUE, sizeof (gchar *));
  g_array_append_val (result, val1);
  g_array_append_val (result, val2);
  g_array_append_val (result, val3);
  g_array_append_val (result, val4);

  g_array_unref (*array_);
  *array_ = result;
}

/**
 * gi_marshalling_tests_garray_utf8_full_inout:
 * @array_: (inout) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_garray_utf8_full_inout (GArray **array_)
{
  static const gchar *val1 = "-1";
  static const gchar *val2 = "-2";
  gchar *val;
  GArray *result;

  g_assert_cmpint ((*array_)->len, ==, 3);
  g_assert_cmpstr (g_array_index (*array_, gchar *, 0), ==, "0");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 1), ==, "1");
  g_assert_cmpstr (g_array_index (*array_, gchar *, 2), ==, "2");

  result = g_array_new (TRUE, TRUE, sizeof (gchar *));
  val = g_strdup (val2);
  g_array_append_val (result, val);
  val = g_strdup (val1);
  g_array_append_val (result, val);
  val = g_strdup ("0");
  g_array_append_val (result, val);
  val = g_strdup ("1");
  g_array_append_val (result, val);

  g_array_unref (*array_);
  *array_ = result;
}

/**
 * gi_marshalling_tests_garray_bool_none_in:
 * @array_: (element-type gboolean) (transfer none):
 */
void
gi_marshalling_tests_garray_bool_none_in (GArray *array_)
{
  g_assert_cmpint (array_->len, ==, 4);
  g_assert_cmpint (g_array_index (array_, gboolean, 0), ==, TRUE);
  g_assert_cmpint (g_array_index (array_, gboolean, 1), ==, FALSE);
  g_assert_cmpint (g_array_index (array_, gboolean, 2), ==, TRUE);
  g_assert_cmpint (g_array_index (array_, gboolean, 3), ==, TRUE);
}

/**
 * gi_marshalling_tests_garray_unichar_none_in:
 * @array_: (element-type gunichar) (transfer none):
 */
void
gi_marshalling_tests_garray_unichar_none_in (GArray *array_)
{
  unsigned ix;
  static const gunichar expected[] = GI_MARSHALLING_TESTS_CONSTANT_UCS4;
  g_assert_cmpint (array_->len, ==, 12);
  for (ix = 0; ix < array_->len; ix++)
    g_assert_cmpuint (g_array_index (array_, gunichar, ix), ==, expected[ix]);
}

/**
 * gi_marshalling_tests_gptrarray_utf8_none_return:
 *
 * Returns: (element-type utf8) (transfer none):
 */
GPtrArray *
gi_marshalling_tests_gptrarray_utf8_none_return (void)
{
  static GPtrArray *parray = NULL;
  static const gchar *values[] = { "0", "1", "2" };
  gint i;

  if (parray == NULL)
    {
      parray = g_ptr_array_new ();
      for (i = 0; i < 3; i++)
        g_ptr_array_add (parray, (gpointer) values[i]);
    }

  return parray;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_container_return:
 *
 * Returns: (element-type utf8) (transfer container):
 */
GPtrArray *
gi_marshalling_tests_gptrarray_utf8_container_return (void)
{
  GPtrArray *parray = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  parray = g_ptr_array_new ();
  for (i = 0; values[i]; i++)
    g_ptr_array_add (parray, (gpointer) values[i]);

  return parray;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_full_return:
 *
 * Returns: (element-type utf8) (transfer full):
 */
GPtrArray *
gi_marshalling_tests_gptrarray_utf8_full_return (void)
{
  GPtrArray *parray = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  parray = g_ptr_array_new ();
  for (i = 0; values[i]; i++)
    {
      gchar *str = g_strdup (values[i]);
      g_ptr_array_add (parray, (gpointer) str);
    }

  return parray;
}

/**
 * gi_marshalling_tests_gptrarray_boxed_struct_full_return:
 *
 * Returns: (element-type GIMarshallingTestsBoxedStruct) (transfer full):
 */
GPtrArray *
gi_marshalling_tests_gptrarray_boxed_struct_full_return (void)
{
  GPtrArray *parray = NULL;
  static const glong long_values[] = { 42, 43, 44 };
  gint i;

  parray = g_ptr_array_new ();
  for (i = 0; i < 3; i++)
    {
      GIMarshallingTestsBoxedStruct *new_struct = gi_marshalling_tests_boxed_struct_new ();
      new_struct->long_ = long_values[i];
      g_ptr_array_add (parray, (gpointer) new_struct);
    }

  return parray;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_none_in:
 * @parray_: (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gptrarray_utf8_none_in (GPtrArray *parray_)
{
  g_assert_cmpint (parray_->len, ==, 3);
  g_assert_cmpstr (g_ptr_array_index (parray_, 0), ==, "0");
  g_assert_cmpstr (g_ptr_array_index (parray_, 1), ==, "1");
  g_assert_cmpstr (g_ptr_array_index (parray_, 2), ==, "2");
}

/**
 * gi_marshalling_tests_gptrarray_utf8_none_out:
 * @parray_: (out) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gptrarray_utf8_none_out (GPtrArray **parray_)
{
  static GPtrArray *internal = NULL;
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  if (internal == NULL)
    {
      internal = g_ptr_array_new ();
      for (i = 0; values[i]; i++)
        g_ptr_array_add (internal, (gpointer) values[i]);
    }

  *parray_ = internal;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_none_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer none):
 */
gboolean
gi_marshalling_tests_gptrarray_utf8_none_out_uninitialized (GPtrArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_container_out:
 * @parray_: (out) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_gptrarray_utf8_container_out (GPtrArray **parray_)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  *parray_ = NULL;

  *parray_ = g_ptr_array_new ();
  for (i = 0; values[i]; i++)
    g_ptr_array_add (*parray_, (gpointer) values[i]);
}

/**
 * gi_marshalling_tests_gptrarray_utf8_container_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer container):
 */
gboolean
gi_marshalling_tests_gptrarray_utf8_container_out_uninitialized (GPtrArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_full_out:
 * @parray_: (out) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_gptrarray_utf8_full_out (GPtrArray **parray_)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  gint i;

  *parray_ = NULL;

  *parray_ = g_ptr_array_new ();
  for (i = 0; values[i]; i++)
    {
      gchar *str = g_strdup (values[i]);
      g_ptr_array_add (*parray_, (gpointer) str);
    }
}

/**
 * gi_marshalling_tests_gptrarray_utf8_full_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer full):
 */
gboolean
gi_marshalling_tests_gptrarray_utf8_full_out_uninitialized (GPtrArray **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_none_inout:
 * @parray_: (inout) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gptrarray_utf8_none_inout (GPtrArray **parray_)
{
  static GPtrArray *internal = NULL;
  static const gchar *values[] = { "-2", "-1", "0", "1", NULL };
  gint i;

  g_assert_cmpint ((*parray_)->len, ==, 3);
  g_assert_cmpstr (g_ptr_array_index (*parray_, 0), ==, "0");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 1), ==, "1");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 2), ==, "2");

  if (internal == NULL)
    {
      internal = g_ptr_array_new ();
      for (i = 0; values[i]; i++)
        g_ptr_array_add (internal, (gpointer) values[i]);
    }

  *parray_ = internal;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_container_inout:
 * @parray_: (inout) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_gptrarray_utf8_container_inout (GPtrArray **parray_)
{
  static const gchar *val1 = "-2";
  static const gchar *val2 = "-1";
  static const gchar *val3 = "0";
  static const gchar *val4 = "1";
  GPtrArray *result;

  g_assert_cmpint ((*parray_)->len, ==, 3);
  g_assert_cmpstr (g_ptr_array_index (*parray_, 0), ==, "0");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 1), ==, "1");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 2), ==, "2");

  result = g_ptr_array_new ();
  g_ptr_array_add (result, (gpointer) val1);
  g_ptr_array_add (result, (gpointer) val2);
  g_ptr_array_add (result, (gpointer) val3);
  g_ptr_array_add (result, (gpointer) val4);

  g_ptr_array_unref (*parray_);
  *parray_ = result;
}

/**
 * gi_marshalling_tests_gptrarray_utf8_full_inout:
 * @parray_: (inout) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_gptrarray_utf8_full_inout (GPtrArray **parray_)
{
  static const gchar *val1 = "-1";
  static const gchar *val2 = "-2";
  gchar *val;
  GPtrArray *result;

  g_assert_cmpint ((*parray_)->len, ==, 3);
  g_assert_cmpstr (g_ptr_array_index (*parray_, 0), ==, "0");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 1), ==, "1");
  g_assert_cmpstr (g_ptr_array_index (*parray_, 2), ==, "2");

  result = g_ptr_array_new ();
  val = g_strdup (val2);
  g_ptr_array_add (result, (gpointer) val);
  val = g_strdup (val1);
  g_ptr_array_add (result, (gpointer) val);
  val = g_strdup ("0");
  g_ptr_array_add (result, (gpointer) val);
  val = g_strdup ("1");
  g_ptr_array_add (result, (gpointer) val);

  g_ptr_array_unref (*parray_);
  *parray_ = result;
}

/**
 * gi_marshalling_tests_bytearray_full_return:
 *
 * Returns: (transfer full):
 */
GByteArray *
gi_marshalling_tests_bytearray_full_return (void)
{
  GByteArray *array = NULL;
  guint8 data[] = { '\0', '1', '\xFF', '3' };

  array = g_byte_array_new ();
  g_byte_array_append (array, (const guint8 *) data, G_N_ELEMENTS (data));

  return array;

}

/**
 * gi_marshalling_tests_bytearray_none_in:
 * @v: (element-type gint8) (transfer none):
 */
void
gi_marshalling_tests_bytearray_none_in (GByteArray *v)
{
  g_assert_cmpuint (v->len, ==, 4);
  g_assert_cmpuint (g_array_index (v, unsigned char, 0), ==, 0);
  g_assert_cmpuint (g_array_index (v, unsigned char, 1), ==, 49);
  g_assert_cmpuint (g_array_index (v, unsigned char, 2), ==, 0xFF);
  g_assert_cmpuint (g_array_index (v, unsigned char, 3), ==, 51);
}

/**
 * gi_marshalling_tests_gbytes_full_return:
 *
 * Returns: (transfer full):
 */
GBytes *
gi_marshalling_tests_gbytes_full_return (void)
{
  static guint8 data[] = { 0, 49, 0xFF, 51 };

  return g_bytes_new_static (data, G_N_ELEMENTS (data));
}

/**
 * gi_marshalling_tests_gbytes_none_in:
 */
void
gi_marshalling_tests_gbytes_none_in (GBytes *v)
{
  const guint8 *data;
  gsize len;
  data = g_bytes_get_data (v, &len);

  g_assert_cmpuint (len, ==, 4);
  g_assert_cmpuint (data[0], ==, 0);
  g_assert_cmpuint (data[1], ==, 49);
  g_assert_cmpuint (data[2], ==, 0xFF);
  g_assert_cmpuint (data[3], ==, 51);
}

/**
 * gi_marshalling_tests_gstrv_return:
 *
 * Returns: (transfer full): an array of strings
 */
GStrv
gi_marshalling_tests_gstrv_return (void)
{
  GStrv values = g_new0 (gchar *, 4);
  values[0] = g_strdup ("0");
  values[1] = g_strdup ("1");
  values[2] = g_strdup ("2");
  values[3] = NULL;
  return values;
}

/**
 * gi_marshalling_tests_gstrv_in:
 * @g_strv:
 */
void
gi_marshalling_tests_gstrv_in (GStrv g_strv)
{
  g_assert_cmpint (g_strv_length (g_strv), ==, 3);
  g_assert_cmpstr (g_strv[0], ==, "0");
  g_assert_cmpstr (g_strv[1], ==, "1");
  g_assert_cmpstr (g_strv[2], ==, "2");
}

/**
 * gi_marshalling_tests_gstrv_out:
 * @g_strv: (out) (transfer none):
 */
void
gi_marshalling_tests_gstrv_out (GStrv *g_strv)
{
  static const gchar *values[] = { "0", "1", "2", NULL };
  *g_strv = (gchar **) values;
}

/**
 * gi_marshalling_tests_gstrv_out_uninitialized:
 * @v: (out) (transfer none):
 */
gboolean
gi_marshalling_tests_gstrv_out_uninitialized (GStrv *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gstrv_inout:
 * @g_strv: (inout) (transfer none):
 */
void
gi_marshalling_tests_gstrv_inout (GStrv *g_strv)
{
  static const gchar *values[] = { "-1", "0", "1", "2", NULL };

  g_assert (g_strv_length (*g_strv) == 3);
  g_assert (strcmp ((*g_strv)[0], "0") == 0);
  g_assert (strcmp ((*g_strv)[1], "1") == 0);
  g_assert (strcmp ((*g_strv)[2], "2") == 0);

  *g_strv = (gchar **) values;
}

/**
 * gi_marshalling_tests_glist_int_none_return:
 *
 * Returns: (element-type gint) (transfer none):
 */
GList *
gi_marshalling_tests_glist_int_none_return (void)
{
  static GList *list = NULL;

  if (list == NULL)
    {
      list = g_list_append (list, GINT_TO_POINTER (-1));
      list = g_list_append (list, GINT_TO_POINTER (0));
      list = g_list_append (list, GINT_TO_POINTER (1));
      list = g_list_append (list, GINT_TO_POINTER (2));
    }

  return list;
}

/**
 * gi_marshalling_tests_glist_uint32_none_return:
 *
 * Returns: (element-type guint32) (transfer none):
 */
GList *
gi_marshalling_tests_glist_uint32_none_return (void)
{
  static GList *list = NULL;

  if (list == NULL)
    {
      list = g_list_append (list, GUINT_TO_POINTER (0));
      list = g_list_append (list, GUINT_TO_POINTER (G_MAXUINT32));
    }

  return list;
}

/**
 * gi_marshalling_tests_glist_utf8_none_return:
 *
 * Returns: (element-type utf8) (transfer none):
 */
GList *
gi_marshalling_tests_glist_utf8_none_return (void)
{
  static GList *list = NULL;

  if (list == NULL)
    {
      list = g_list_append (list, (gpointer) "0");
      list = g_list_append (list, (gpointer) "1");
      list = g_list_append (list, (gpointer) "2");
    }

  return list;
}

/**
 * gi_marshalling_tests_glist_utf8_container_return:
 *
 * Returns: (element-type utf8) (transfer container):
 */
GList *
gi_marshalling_tests_glist_utf8_container_return (void)
{
  GList *list = NULL;

  list = g_list_append (list, (gpointer) "0");
  list = g_list_append (list, (gpointer) "1");
  list = g_list_append (list, (gpointer) "2");

  return list;
}

/**
 * gi_marshalling_tests_glist_utf8_full_return:
 *
 * Returns: (element-type utf8) (transfer full):
 */
GList *
gi_marshalling_tests_glist_utf8_full_return (void)
{
  GList *list = NULL;

  list = g_list_append (list, g_strdup ("0"));
  list = g_list_append (list, g_strdup ("1"));
  list = g_list_append (list, g_strdup ("2"));

  return list;
}

/**
 * gi_marshalling_tests_glist_int_none_in:
 * @list: (element-type gint) (transfer none):
 */
void
gi_marshalling_tests_glist_int_none_in (GList *list)
{
  g_assert_cmpint (g_list_length (list), ==, 4);
  g_assert_cmpint (GPOINTER_TO_INT (g_list_nth_data (list, 0)), ==, -1);
  g_assert_cmpint (GPOINTER_TO_INT (g_list_nth_data (list, 1)), ==, 0);
  g_assert_cmpint (GPOINTER_TO_INT (g_list_nth_data (list, 2)), ==, 1);
  g_assert_cmpint (GPOINTER_TO_INT (g_list_nth_data (list, 3)), ==, 2);
}

/**
 * gi_marshalling_tests_glist_uint32_none_in:
 * @list: (element-type guint32) (transfer none):
 */
void
gi_marshalling_tests_glist_uint32_none_in (GList *list)
{
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert_cmpint (GPOINTER_TO_UINT (g_list_nth_data (list, 0)), ==, 0);
  g_assert_cmpint (GPOINTER_TO_UINT (g_list_nth_data (list, 1)), ==, G_MAXUINT32);
}

/**
 * gi_marshalling_tests_glist_utf8_none_in:
 * @list: (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_glist_utf8_none_in (GList *list)
{
  g_assert_cmpint (g_list_length (list), ==, 3);
  g_assert_cmpint (strcmp (g_list_nth_data (list, 0), "0"), ==, 0);
  g_assert_cmpint (strcmp (g_list_nth_data (list, 1), "1"), ==, 0);
  g_assert_cmpint (strcmp (g_list_nth_data (list, 2), "2"), ==, 0);
}

/**
 * gi_marshalling_tests_glist_utf8_none_out:
 * @list: (out) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_glist_utf8_none_out (GList **list)
{
  static GList *values = NULL;

  if (values == NULL)
    {
      values = g_list_append (values, (gpointer) "0");
      values = g_list_append (values, (gpointer) "1");
      values = g_list_append (values, (gpointer) "2");
    }

  *list = values;
}

/**
 * gi_marshalling_tests_glist_utf8_none_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer none):
 */
gboolean
gi_marshalling_tests_glist_utf8_none_out_uninitialized (GList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_glist_utf8_container_out:
 * @list: (out) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_glist_utf8_container_out (GList **list)
{
  *list = NULL;

  *list = g_list_append (*list, (gpointer) "0");
  *list = g_list_append (*list, (gpointer) "1");
  *list = g_list_append (*list, (gpointer) "2");
}

/**
 * gi_marshalling_tests_glist_utf8_container_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer container):
 */
gboolean
gi_marshalling_tests_glist_utf8_container_out_uninitialized (GList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_glist_utf8_full_out:
 * @list: (out) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_glist_utf8_full_out (GList **list)
{
  *list = NULL;

  *list = g_list_append (*list, g_strdup ("0"));
  *list = g_list_append (*list, g_strdup ("1"));
  *list = g_list_append (*list, g_strdup ("2"));
}

/**
 * gi_marshalling_tests_glist_utf8_full_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer full):
 */
gboolean
gi_marshalling_tests_glist_utf8_full_out_uninitialized (GList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_glist_utf8_none_inout:
 * @list: (inout) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_glist_utf8_none_inout (GList **list)
{
  static GList *values = NULL;

  g_assert_cmpint (g_list_length (*list), ==, 3);
  g_assert_cmpstr (g_list_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_list_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_list_nth_data (*list, 2), ==, "2");

  if (values == NULL)
    {
      values = g_list_append (values, (gpointer) "-2");
      values = g_list_append (values, (gpointer) "-1");
      values = g_list_append (values, (gpointer) "0");
      values = g_list_append (values, (gpointer) "1");
    }

  *list = values;
}

/**
 * gi_marshalling_tests_glist_utf8_container_inout:
 * @list: (inout) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_glist_utf8_container_inout (GList **list)
{
  GList *result = NULL;

  g_assert_cmpint (g_list_length (*list), ==, 3);
  g_assert_cmpstr (g_list_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_list_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_list_nth_data (*list, 2), ==, "2");

  result = g_list_prepend (result, (gpointer) "1");
  result = g_list_prepend (result, (gpointer) "0");
  result = g_list_prepend (result, (gpointer) "-1");
  result = g_list_prepend (result, (gpointer) "-2");

  g_list_free (*list);
  *list = result;
}

/**
 * gi_marshalling_tests_glist_utf8_full_inout:
 * @list: (inout) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_glist_utf8_full_inout (GList **list)
{
  GList *result = NULL;

  g_assert_cmpint (g_list_length (*list), ==, 3);
  g_assert_cmpstr (g_list_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_list_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_list_nth_data (*list, 2), ==, "2");

  result = g_list_prepend (result, g_strdup ("1"));
  result = g_list_prepend (result, g_strdup ("0"));
  result = g_list_prepend (result, g_strdup ("-1"));
  result = g_list_prepend (result, g_strdup ("-2"));

  g_list_free_full (*list, g_free);
  *list = result;
}


/**
 * gi_marshalling_tests_gslist_int_none_return:
 *
 * Returns: (element-type gint) (transfer none):
 */
GSList *
gi_marshalling_tests_gslist_int_none_return (void)
{
  static GSList *list = NULL;

  if (list == NULL)
    {
      list = g_slist_prepend (list, GINT_TO_POINTER (-1));
      list = g_slist_prepend (list, GINT_TO_POINTER (0));
      list = g_slist_prepend (list, GINT_TO_POINTER (1));
      list = g_slist_prepend (list, GINT_TO_POINTER (2));
      list = g_slist_reverse (list);
    }

  return list;
}

/**
 * gi_marshalling_tests_gslist_utf8_none_return:
 *
 * Returns: (element-type utf8) (transfer none):
 */
GSList *
gi_marshalling_tests_gslist_utf8_none_return (void)
{
  static GSList *list = NULL;

  if (list == NULL)
    {
      list = g_slist_prepend (list, (gpointer) "0");
      list = g_slist_prepend (list, (gpointer) "1");
      list = g_slist_prepend (list, (gpointer) "2");
      list = g_slist_reverse (list);
    }

  return list;
}

/**
 * gi_marshalling_tests_gslist_utf8_container_return:
 *
 * Returns: (element-type utf8) (transfer container):
 */
GSList *
gi_marshalling_tests_gslist_utf8_container_return (void)
{
  GSList *list = NULL;

  list = g_slist_prepend (list, (gpointer) "0");
  list = g_slist_prepend (list, (gpointer) "1");
  list = g_slist_prepend (list, (gpointer) "2");
  list = g_slist_reverse (list);

  return list;
}

/**
 * gi_marshalling_tests_gslist_utf8_full_return:
 *
 * Returns: (element-type utf8) (transfer full):
 */
GSList *
gi_marshalling_tests_gslist_utf8_full_return (void)
{
  GSList *list = NULL;

  list = g_slist_prepend (list, g_strdup ("0"));
  list = g_slist_prepend (list, g_strdup ("1"));
  list = g_slist_prepend (list, g_strdup ("2"));
  list = g_slist_reverse (list);

  return list;
}

/**
 * gi_marshalling_tests_gslist_int_none_in:
 * @list: (element-type gint) (transfer none):
 */
void
gi_marshalling_tests_gslist_int_none_in (GSList *list)
{
  g_assert_cmpint (g_slist_length (list), ==, 4);
  g_assert_cmpint (GPOINTER_TO_INT (g_slist_nth_data (list, 0)), ==, -1);
  g_assert_cmpint (GPOINTER_TO_INT (g_slist_nth_data (list, 1)), ==, 0);
  g_assert_cmpint (GPOINTER_TO_INT (g_slist_nth_data (list, 2)), ==, 1);
  g_assert_cmpint (GPOINTER_TO_INT (g_slist_nth_data (list, 3)), ==, 2);
}

/**
 * gi_marshalling_tests_gslist_utf8_none_in:
 * @list: (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gslist_utf8_none_in (GSList *list)
{
  g_assert_cmpint (g_slist_length (list), ==, 3);
  g_assert_cmpstr (g_slist_nth_data (list, 0), ==, "0");
  g_assert_cmpstr (g_slist_nth_data (list, 1), ==, "1");
  g_assert_cmpstr (g_slist_nth_data (list, 2), ==, "2");
}

/**
 * gi_marshalling_tests_gslist_utf8_none_out:
 * @list: (out) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gslist_utf8_none_out (GSList **list)
{
  static GSList *values = NULL;

  if (values == NULL)
    {
      values = g_slist_prepend (values, (gpointer) "0");
      values = g_slist_prepend (values, (gpointer) "1");
      values = g_slist_prepend (values, (gpointer) "2");
      values = g_slist_reverse (values);
    }

  *list = values;
}

/**
 * gi_marshalling_tests_gslist_utf8_none_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer none):
 */
gboolean
gi_marshalling_tests_gslist_utf8_none_out_uninitialized (GSList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gslist_utf8_container_out:
 * @list: (out) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_gslist_utf8_container_out (GSList **list)
{
  *list = NULL;

  *list = g_slist_prepend (*list, (gpointer) "0");
  *list = g_slist_prepend (*list, (gpointer) "1");
  *list = g_slist_prepend (*list, (gpointer) "2");
  *list = g_slist_reverse (*list);
}

/**
 * gi_marshalling_tests_gslist_utf8_container_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer container):
 */
gboolean
gi_marshalling_tests_gslist_utf8_container_out_uninitialized (GSList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gslist_utf8_full_out:
 * @list: (out) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_gslist_utf8_full_out (GSList **list)
{
  *list = NULL;

  *list = g_slist_prepend (*list, g_strdup ("0"));
  *list = g_slist_prepend (*list, g_strdup ("1"));
  *list = g_slist_prepend (*list, g_strdup ("2"));
  *list = g_slist_reverse (*list);
}

/**
 * gi_marshalling_tests_gslist_utf8_full_out_uninitialized:
 * @v: (out) (element-type utf8) (transfer full):
 */
gboolean
gi_marshalling_tests_gslist_utf8_full_out_uninitialized (GSList **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gslist_utf8_none_inout:
 * @list: (inout) (element-type utf8) (transfer none):
 */
void
gi_marshalling_tests_gslist_utf8_none_inout (GSList **list)
{
  static GSList *values = NULL;

  g_assert_cmpint (g_slist_length (*list), ==, 3);
  g_assert_cmpstr (g_slist_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_slist_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_slist_nth_data (*list, 2), ==, "2");

  if (values == NULL)
    {
      values = g_slist_prepend (values, (gpointer) "-2");
      values = g_slist_prepend (values, (gpointer) "-1");
      values = g_slist_prepend (values, (gpointer) "0");
      values = g_slist_prepend (values, (gpointer) "1");
      values = g_slist_reverse (values);
    }

  *list = values;
}

/**
 * gi_marshalling_tests_gslist_utf8_container_inout:
 * @list: (inout) (element-type utf8) (transfer container):
 */
void
gi_marshalling_tests_gslist_utf8_container_inout (GSList **list)
{
  GSList *result = NULL;

  g_assert_cmpint (g_slist_length (*list), ==, 3);
  g_assert_cmpstr (g_slist_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_slist_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_slist_nth_data (*list, 2), ==, "2");

  result = g_slist_prepend (result, (gpointer) "1");
  result = g_slist_prepend (result, (gpointer) "0");
  result = g_slist_prepend (result, (gpointer) "-1");
  result = g_slist_prepend (result, (gpointer) "-2");

  g_slist_free (*list);
  *list = result;
}

/**
 * gi_marshalling_tests_gslist_utf8_full_inout:
 * @list: (inout) (element-type utf8) (transfer full):
 */
void
gi_marshalling_tests_gslist_utf8_full_inout (GSList **list)
{
  GSList *result = NULL;

  g_assert_cmpint (g_slist_length (*list), ==, 3);
  g_assert_cmpstr (g_slist_nth_data (*list, 0), ==, "0");
  g_assert_cmpstr (g_slist_nth_data (*list, 1), ==, "1");
  g_assert_cmpstr (g_slist_nth_data (*list, 2), ==, "2");

  result = g_slist_prepend (result, g_strdup ("1"));
  result = g_slist_prepend (result, g_strdup ("0"));
  result = g_slist_prepend (result, g_strdup ("-1"));
  result = g_slist_prepend (result, g_strdup ("-2"));

  g_slist_free_full (*list, g_free);
  *list = result;
}


/**
 * gi_marshalling_tests_ghashtable_int_none_return:
 *
 * Returns: (element-type gint gint) (transfer none):
 */
GHashTable *
gi_marshalling_tests_ghashtable_int_none_return (void)
{
  static GHashTable *hash_table = NULL;

  if (hash_table == NULL)
    {
      hash_table = g_hash_table_new (NULL, NULL);
      g_hash_table_insert (hash_table, GINT_TO_POINTER (-1), GINT_TO_POINTER (1));
      g_hash_table_insert (hash_table, GINT_TO_POINTER (0), GINT_TO_POINTER (0));
      g_hash_table_insert (hash_table, GINT_TO_POINTER (1), GINT_TO_POINTER (-1));
      g_hash_table_insert (hash_table, GINT_TO_POINTER (2), GINT_TO_POINTER (-2));
    }

  return hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_none_return:
 *
 * Returns: (element-type utf8 utf8) (transfer none):
 */
GHashTable *
gi_marshalling_tests_ghashtable_utf8_none_return (void)
{
  static GHashTable *hash_table = NULL;

  if (hash_table == NULL)
    {
      hash_table = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (hash_table, (gpointer) "-1", (gpointer) "1");
      g_hash_table_insert (hash_table, (gpointer) "0",  (gpointer) "0");
      g_hash_table_insert (hash_table, (gpointer) "1",  (gpointer) "-1");
      g_hash_table_insert (hash_table, (gpointer) "2",  (gpointer) "-2");
    }

  return hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_container_return:
 *
 * Returns: (element-type utf8 utf8) (transfer container):
 */
GHashTable *
gi_marshalling_tests_ghashtable_utf8_container_return (void)
{
  GHashTable *hash_table = NULL;

  hash_table = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (hash_table, (gpointer) "-1", (gpointer) "1");
  g_hash_table_insert (hash_table, (gpointer) "0",  (gpointer) "0");
  g_hash_table_insert (hash_table, (gpointer) "1",  (gpointer) "-1");
  g_hash_table_insert (hash_table, (gpointer) "2",  (gpointer) "-2");

  return hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_full_return:
 *
 * Returns: (element-type utf8 utf8) (transfer full):
 */
GHashTable *
gi_marshalling_tests_ghashtable_utf8_full_return (void)
{
  GHashTable *hash_table = NULL;

  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (hash_table, g_strdup ("-1"), g_strdup ("1"));
  g_hash_table_insert (hash_table, g_strdup ("0"), g_strdup ("0"));
  g_hash_table_insert (hash_table, g_strdup ("1"), g_strdup ("-1"));
  g_hash_table_insert (hash_table, g_strdup ("2"), g_strdup ("-2"));

  return hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_int_none_in:
 * @hash_table: (element-type gint gint) (transfer none):
 */
void
gi_marshalling_tests_ghashtable_int_none_in (GHashTable *hash_table)
{
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (hash_table, GINT_TO_POINTER (-1))), ==, 1);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (hash_table, GINT_TO_POINTER (0))), ==, 0);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (hash_table, GINT_TO_POINTER (1))), ==, -1);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (hash_table, GINT_TO_POINTER (2))), ==, -2);
}

/**
 * gi_marshalling_tests_ghashtable_utf8_none_in:
 * @hash_table: (element-type utf8 utf8) (transfer none):
 */
void
gi_marshalling_tests_ghashtable_utf8_none_in (GHashTable *hash_table)
{
  g_assert_cmpstr (g_hash_table_lookup (hash_table, "-1"), ==, "1");
  g_assert_cmpstr (g_hash_table_lookup (hash_table, "0"), ==, "0");
  g_assert_cmpstr (g_hash_table_lookup (hash_table, "1"), ==, "-1");
  g_assert_cmpstr (g_hash_table_lookup (hash_table, "2"), ==, "-2");
}

/**
 * gi_marshalling_tests_ghashtable_double_in:
 * @hash_table: (element-type utf8 double) (transfer none):
 *
 * Meant to test a value type that doesn't fit inside a pointer.
 */
void
gi_marshalling_tests_ghashtable_double_in (GHashTable *hash_table)
{
  double *value;

  value = g_hash_table_lookup (hash_table, "-1");
  g_assert_cmpfloat_with_epsilon (*value, -0.1, 0.01);
  value = g_hash_table_lookup (hash_table, "0");
  g_assert_cmpfloat (*value, ==, 0.0);
  value = g_hash_table_lookup (hash_table, "1");
  g_assert_cmpfloat_with_epsilon (*value, 0.1, 0.01);
  value = g_hash_table_lookup (hash_table, "2");
  g_assert_cmpfloat_with_epsilon (*value, 0.2, 0.01);
}

/**
 * gi_marshalling_tests_ghashtable_float_in:
 * @hash_table: (element-type utf8 float) (transfer none):
 *
 * Meant to test a value type that doesn't fit inside a pointer.
 */
void
gi_marshalling_tests_ghashtable_float_in (GHashTable *hash_table)
{
  float *value;

  value = g_hash_table_lookup (hash_table, "-1");
  g_assert_cmpfloat_with_epsilon (*value, -0.1f, 0.01f);
  value = g_hash_table_lookup (hash_table, "0");
  g_assert_cmpfloat (*value, ==, 0.0f);
  value = g_hash_table_lookup (hash_table, "1");
  g_assert_cmpfloat_with_epsilon (*value, 0.1f, 0.01f);
  value = g_hash_table_lookup (hash_table, "2");
  g_assert_cmpfloat_with_epsilon (*value, 0.2f, 0.01f);
}

/**
 * gi_marshalling_tests_ghashtable_int64_in:
 * @hash_table: (element-type utf8 gint64) (transfer none):
 *
 * Meant to test a value type that doesn't fit inside a pointer.
 */
void
gi_marshalling_tests_ghashtable_int64_in (GHashTable *hash_table)
{
  gint64 *value;

  value = g_hash_table_lookup (hash_table, "-1");
  g_assert_cmpint (*value, ==, -1);
  value = g_hash_table_lookup (hash_table, "0");
  g_assert_cmpint (*value, ==, 0);
  value = g_hash_table_lookup (hash_table, "1");
  g_assert_cmpint (*value, ==, 1);
  value = g_hash_table_lookup (hash_table, "2");
  g_assert_cmpint (*value, ==, (gint64) G_MAXUINT32 + 1);
}

/**
 * gi_marshalling_tests_ghashtable_uint64_in:
 * @hash_table: (element-type utf8 guint64) (transfer none):
 *
 * Meant to test a value type that doesn't fit inside a pointer.
 */
void
gi_marshalling_tests_ghashtable_uint64_in (GHashTable *hash_table)
{
  guint64 *value;

  value = g_hash_table_lookup (hash_table, "-1");
  g_assert_cmpuint (*value, ==, (guint64) G_MAXUINT32 + 1);
  value = g_hash_table_lookup (hash_table, "0");
  g_assert_cmpuint (*value, ==, 0);
  value = g_hash_table_lookup (hash_table, "1");
  g_assert_cmpuint (*value, ==, 1);
  value = g_hash_table_lookup (hash_table, "2");
  g_assert_cmpuint (*value, ==, 2);
}

/**
 * gi_marshalling_tests_ghashtable_utf8_none_out:
 * @hash_table: (out) (element-type utf8 utf8) (transfer none):
 */
void
gi_marshalling_tests_ghashtable_utf8_none_out (GHashTable **hash_table)
{
  static GHashTable *new_hash_table = NULL;

  if (new_hash_table == NULL)
    {
      new_hash_table = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (new_hash_table, (gpointer) "-1", (gpointer) "1");
      g_hash_table_insert (new_hash_table, (gpointer) "0",  (gpointer) "0");
      g_hash_table_insert (new_hash_table, (gpointer) "1",  (gpointer) "-1");
      g_hash_table_insert (new_hash_table, (gpointer) "2",  (gpointer) "-2");
    }

  *hash_table = new_hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_none_out_uninitialized:
 * @v: (out) (element-type utf8 utf8) (transfer none):
 */
gboolean
gi_marshalling_tests_ghashtable_utf8_none_out_uninitialized (GHashTable **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_container_out:
 * @hash_table: (out) (element-type utf8 utf8) (transfer container):
 */
void
gi_marshalling_tests_ghashtable_utf8_container_out (GHashTable **hash_table)
{
  *hash_table = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (*hash_table, (gpointer) "-1", (gpointer) "1");
  g_hash_table_insert (*hash_table, (gpointer) "0",  (gpointer) "0");
  g_hash_table_insert (*hash_table, (gpointer) "1",  (gpointer) "-1");
  g_hash_table_insert (*hash_table, (gpointer) "2",  (gpointer) "-2");
}

/**
 * gi_marshalling_tests_ghashtable_utf8_container_out_uninitialized:
 * @v: (out) (element-type utf8 utf8) (transfer container):
 */
gboolean
gi_marshalling_tests_ghashtable_utf8_container_out_uninitialized (GHashTable **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_full_out:
 * @hash_table: (out) (element-type utf8 utf8) (transfer full):
 */
void
gi_marshalling_tests_ghashtable_utf8_full_out (GHashTable **hash_table)
{
  *hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (*hash_table, g_strdup ("-1"), g_strdup ("1"));
  g_hash_table_insert (*hash_table, g_strdup ("0"), g_strdup ("0"));
  g_hash_table_insert (*hash_table, g_strdup ("1"), g_strdup ("-1"));
  g_hash_table_insert (*hash_table, g_strdup ("2"), g_strdup ("-2"));
}

/**
 * gi_marshalling_tests_ghashtable_utf8_full_out_uninitialized:
 * @v: (out) (element-type utf8 utf8) (transfer full):
 */
gboolean
gi_marshalling_tests_ghashtable_utf8_full_out_uninitialized (GHashTable **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_none_inout:
 * @hash_table: (inout) (element-type utf8 utf8) (transfer none):
 */
void
gi_marshalling_tests_ghashtable_utf8_none_inout (GHashTable **hash_table)
{
  static GHashTable *new_hash_table = NULL;

  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "-1"), ==, "1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "0"), ==, "0");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "1"), ==, "-1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "2"), ==, "-2");

  if (new_hash_table == NULL)
    {
      new_hash_table = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (new_hash_table, (gpointer) "-1", (gpointer) "1");
      g_hash_table_insert (new_hash_table, (gpointer) "0",  (gpointer) "0");
      g_hash_table_insert (new_hash_table, (gpointer) "1",  (gpointer) "1");
    }

  *hash_table = new_hash_table;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_container_inout:
 * @hash_table: (inout) (element-type utf8 utf8) (transfer container):
 */
void
gi_marshalling_tests_ghashtable_utf8_container_inout (GHashTable **hash_table)
{
  GHashTable *result = g_hash_table_new (g_str_hash, g_str_equal);

  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "-1"), ==, "1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "0"), ==, "0");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "1"), ==, "-1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "2"), ==, "-2");

  g_hash_table_insert (result, (gpointer) "-1", (gpointer) "1");
  g_hash_table_insert (result, (gpointer) "0",  (gpointer) "0");
  g_hash_table_insert (result, (gpointer) "1",  (gpointer) "1");

  g_hash_table_unref (*hash_table);
  *hash_table = result;
}

/**
 * gi_marshalling_tests_ghashtable_utf8_full_inout:
 * @hash_table: (inout) (element-type utf8 utf8) (transfer full):
 */
void
gi_marshalling_tests_ghashtable_utf8_full_inout (GHashTable **hash_table)
{
  GHashTable *result = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, g_free);

  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "-1"), ==, "1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "0"), ==, "0");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "1"), ==, "-1");
  g_assert_cmpstr (g_hash_table_lookup (*hash_table, "2"), ==, "-2");

  g_hash_table_insert (result, g_strdup ("-1"), g_strdup ("1"));
  g_hash_table_insert (result, g_strdup ("0"), g_strdup ("0"));
  g_hash_table_insert (result, g_strdup ("1"), g_strdup ("1"));

  g_hash_table_unref (*hash_table);
  *hash_table = result;
}


/**
 * gi_marshalling_tests_gvalue_return:
 *
 * Returns: (transfer none):
 */
GValue *
gi_marshalling_tests_gvalue_return (void)
{
  static GValue *value = NULL;

  if (value == NULL)
    {
      value = g_new0 (GValue, 1);
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, 42);
    }

  return value;
}

/**
 * gi_marshalling_tests_gvalue_noncanonical_nan_float:
 *
 * Returns: (transfer none):
 */
GValue *
gi_marshalling_tests_gvalue_noncanonical_nan_float (void)
{
  static GValue *value = NULL;

  if (value == NULL)
    {
      value = g_new0 (GValue, 1);
      g_value_init (value, G_TYPE_FLOAT);
      g_value_set_float (value, noncanonical_nan_float());
    }

  return value;
}

/**
 * gi_marshalling_tests_gvalue_noncanonical_nan_double:
 *
 * Returns: (transfer none):
 */
GValue *
gi_marshalling_tests_gvalue_noncanonical_nan_double (void)
{
  static GValue *value = NULL;

  if (value == NULL)
    {
      value = g_new0 (GValue, 1);
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_double (value, noncanonical_nan_double());
    }

  return value;
}

/**
 * gi_marshalling_tests_gvalue_in:
 * @value: (transfer none):
 */
void
gi_marshalling_tests_gvalue_in (GValue *value)
{
  g_assert_cmpint (g_value_get_int (value), ==, 42);
}

/**
 * gi_marshalling_tests_gvalue_int64_in:
 * @value: (transfer none):
 */
void
gi_marshalling_tests_gvalue_int64_in (GValue *value)
{
  g_assert_cmpint (g_value_get_int64 (value), ==, G_MAXINT64);
}

/**
 * gi_marshalling_tests_gvalue_in_with_type:
 * @value: (transfer none):
 * @type:
 */
void
gi_marshalling_tests_gvalue_in_with_type (GValue *value, GType type)
{
  g_assert (g_type_is_a (G_VALUE_TYPE (value), type));
}

/**
 * gi_marshalling_tests_gvalue_in_with_modification:
 * @value: (transfer none):
 *
 * Expects a GValue passed by reference which is then modified by
 * this function.
 */
void
gi_marshalling_tests_gvalue_in_with_modification (GValue *value)
{
  g_assert_cmpint (g_value_get_int (value), ==, 42);
  g_value_set_int (value, 24);
}

/**
 * gi_marshalling_tests_gvalue_in_enum:
 * @value: (transfer none):
 */
void
gi_marshalling_tests_gvalue_in_enum (GValue *value)
{
  if (!G_VALUE_HOLDS_ENUM (value))
    g_critical ("Expected enum, got %s", G_VALUE_TYPE_NAME (value));
  g_assert (g_value_get_enum (value) == GI_MARSHALLING_TESTS_ENUM_VALUE3);
}

/**
 * gi_marshalling_tests_gvalue_in_flags:
 * @value: (transfer none):
 */
void
gi_marshalling_tests_gvalue_in_flags (GValue *value)
{
  if (!G_VALUE_HOLDS_FLAGS (value))
    g_critical ("Expected flags, got %s", G_VALUE_TYPE_NAME (value));
  g_assert_cmpint (g_value_get_flags (value), ==, GI_MARSHALLING_TESTS_FLAGS_VALUE3);
}

/**
 * gi_marshalling_tests_gvalue_out:
 * @value: (out) (transfer none):
 */
void
gi_marshalling_tests_gvalue_out (GValue **value)
{
  static GValue *new_value = NULL;

  if (new_value == NULL)
    {
      new_value = g_new0 (GValue, 1);
      g_value_init (new_value, G_TYPE_INT);
      g_value_set_int (new_value, 42);
    }

  *value = new_value;
}

/**
 * gi_marshalling_tests_gvalue_out_uninitialized:
 * @v: (out) (transfer none):
 */
gboolean
gi_marshalling_tests_gvalue_out_uninitialized (GValue **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gvalue_int64_out:
 * @value: (out) (transfer none):
 */
void
gi_marshalling_tests_gvalue_int64_out (GValue **value)
{
  static GValue *new_value = NULL;

  if (new_value == NULL)
    {
      new_value = g_new0 (GValue, 1);
      g_value_init (new_value, G_TYPE_INT64);
      g_value_set_int64 (new_value, G_MAXINT64);
    }

  *value = new_value;
}

/**
 * gi_marshalling_tests_gvalue_out_caller_allocates:
 * @value: (out) (transfer none):
 */
void
gi_marshalling_tests_gvalue_out_caller_allocates (GValue *value)
{
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, 42);
}

/**
 * gi_marshalling_tests_gvalue_inout:
 * @value: (inout) (transfer none):
 */
void
gi_marshalling_tests_gvalue_inout (GValue **value)
{
  g_assert_cmpint (g_value_get_int (*value), ==, 42);
  g_value_unset (*value);
  g_value_init (*value, G_TYPE_STRING);
  g_value_set_string (*value, "42");
}

/**
 * gi_marshalling_tests_gvalue_flat_array:
 * @n_values: number of values
 * @values: (array length=n_values): an array containing values
 */
void
gi_marshalling_tests_gvalue_flat_array (guint n_values, const GValue *values)
{
  g_assert (n_values == 3);

  g_assert_cmpint (g_value_get_int (&values[0]), ==, 42);
  g_assert_cmpstr (g_value_get_string (&values[1]), ==, "42");
  g_assert_cmpint (g_value_get_boolean (&values[2]), ==, TRUE);
}

/**
 * gi_marshalling_tests_return_gvalue_flat_array:
 *
 * Returns: (array fixed-size=3) (transfer full): a flat GValue array
 */
GValue *
gi_marshalling_tests_return_gvalue_flat_array (void)
{
  GValue *array = g_new0 (GValue, 3);

  g_value_init (&array[0], G_TYPE_INT);
  g_value_set_int (&array[0], 42);

  g_value_init (&array[1], G_TYPE_STRING);
  g_value_set_static_string (&array[1], "42");

  g_value_init (&array[2], G_TYPE_BOOLEAN);
  g_value_set_boolean (&array[2], TRUE);

  return array;
}

/**
 * gi_marshalling_tests_return_gvalue_zero_terminated_array:
 *
 * Returns: (array zero-terminated) (transfer full): a flat GValue array
 */
GValue *
gi_marshalling_tests_return_gvalue_zero_terminated_array (void)
{
  GValue *array = g_new0 (GValue, 4);

  g_value_init (&array[0], G_TYPE_INT);
  g_value_set_int (&array[0], 42);

  g_value_init (&array[1], G_TYPE_STRING);
  g_value_set_static_string (&array[1], "42");

  g_value_init (&array[2], G_TYPE_BOOLEAN);
  g_value_set_boolean (&array[2], TRUE);

  return array;
}

/**
 * gi_marshalling_tests_gvalue_round_trip:
 * @value: The first GValue
 *
 * Returns: (transfer none):
 */
GValue *
gi_marshalling_tests_gvalue_round_trip (GValue *value)
{
  return value;
}

/**
 * gi_marshalling_tests_gvalue_copy:
 * @value: The first GValue
 *
 * Returns: (transfer full):
 */
GValue *
gi_marshalling_tests_gvalue_copy (GValue *value)
{
  GValue *return_value = g_new0 (GValue, 1);

  g_value_init (return_value, G_VALUE_TYPE (value));
  g_value_copy (value, return_value);

  return return_value;
}

/**
 * gi_marshalling_tests_gvalue_flat_array_round_trip:
 * @one: The first GValue
 * @two: The second GValue
 * @three: The third GValue
 *
 * Returns: (array fixed-size=3) (transfer full): a flat array of [@one, @two, @three]
 */
GValue *
gi_marshalling_tests_gvalue_flat_array_round_trip (const GValue one, const GValue two, const GValue three)
{
  GValue *array = g_new (GValue, 3);
  array[0] = one;
  array[1] = two;
  array[2] = three;

  return array;
}

/**
 * gi_marshalling_tests_gclosure_in:
 * @closure: (transfer none):
 */
void
gi_marshalling_tests_gclosure_in (GClosure *closure)
{
  GValue return_value = { 0, };

  g_value_init (&return_value, G_TYPE_INT);

  g_closure_invoke (closure, &return_value, 0, NULL, NULL);

  g_assert_cmpint (g_value_get_int (&return_value), ==, 42);

  g_value_unset (&return_value);
}

static gint
_closure_return_42 (void)
{
  return 42;
}

static void
_marshal_INT__VOID (GClosure     *closure,
                    GValue       *return_value,
                    guint         n_param_values G_GNUC_UNUSED,
                    const GValue *param_values G_GNUC_UNUSED,
                    gpointer      invocation_hint G_GNUC_UNUSED,
                    gpointer      marshal_data G_GNUC_UNUSED)
{
  typedef gint (*GMarshalFunc_INT__VOID) (void);
  register GMarshalFunc_INT__VOID callback;
  register GCClosure *cc = (GCClosure *) closure;

  callback = (GMarshalFunc_INT__VOID) cc->callback;
  g_value_set_int (return_value, callback ());
}

/**
 * gi_marshalling_tests_gclosure_return:
 *
 * Return: a #GClosure
 */
GClosure *
gi_marshalling_tests_gclosure_return (void)
{
  GClosure *closure = g_cclosure_new ((GCallback) _closure_return_42,
                                      NULL, NULL);
  g_closure_set_marshal (closure, _marshal_INT__VOID);

  return closure;
}


/**
 * gi_marshalling_tests_callback_return_value_only:
 * @callback: (scope call):
 */
glong gi_marshalling_tests_callback_return_value_only (GIMarshallingTestsCallbackReturnValueOnly callback)
{
  return callback ();
}

/**
 * gi_marshalling_tests_callback_one_out_parameter:
 * @callback: (scope call):
 * @a: (out):
 */
void gi_marshalling_tests_callback_one_out_parameter (GIMarshallingTestsCallbackOneOutParameter callback, gfloat *a)
{
  callback (a);
}

/**
 * gi_marshalling_tests_callback_multiple_out_parameters:
 * @callback: (scope call):
 * @a: (out):
 * @b: (out):
 */
void
  gi_marshalling_tests_callback_multiple_out_parameters
  (GIMarshallingTestsCallbackMultipleOutParameters callback, gfloat *a, gfloat *b)
{
  callback (a, b);
}

/**
 * gi_marshalling_tests_callback_return_value_and_one_out_parameter:
 * @callback: (scope call):
 * @a: (out):
 */
glong
  gi_marshalling_tests_callback_return_value_and_one_out_parameter
  (GIMarshallingTestsCallbackReturnValueAndOneOutParameter callback, glong *a)
{
  return callback (a);
}

/**
 * gi_marshalling_tests_callback_return_value_and_multiple_out_parameters:
 * @callback: (scope call):
 * @a: (out):
 * @b: (out):
 */
glong
  gi_marshalling_tests_callback_return_value_and_multiple_out_parameters
  (GIMarshallingTestsCallbackReturnValueAndMultipleOutParameters callback, glong *a, glong *b)
{
  return callback (a, b);
}



/**
 * gi_marshalling_tests_pointer_in_return:
 *
 * Returns: The same pointer
 */
gpointer
gi_marshalling_tests_pointer_in_return (gpointer pointer)
{
  return pointer;
}

GType
gi_marshalling_tests_genum_get_type (void)
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0))
    {
      static const GEnumValue values[] = {
        {GI_MARSHALLING_TESTS_GENUM_VALUE1,
         "GI_MARSHALLING_TESTS_GENUM_VALUE1", "value1"},
        {GI_MARSHALLING_TESTS_GENUM_VALUE2,
         "GI_MARSHALLING_TESTS_GENUM_VALUE2", "value2"},
        {GI_MARSHALLING_TESTS_GENUM_VALUE3,
         "GI_MARSHALLING_TESTS_GENUM_VALUE3", "value3"},
        {0, NULL, NULL}
      };
      type = g_enum_register_static (g_intern_static_string ("GIMarshallingTestsGEnum"), values);
    }

  return type;
}

GIMarshallingTestsGEnum
gi_marshalling_tests_genum_returnv (void)
{
  return GI_MARSHALLING_TESTS_GENUM_VALUE3;
}

void
gi_marshalling_tests_genum_in (GIMarshallingTestsGEnum v)
{
  g_assert_cmpint (v, ==, GI_MARSHALLING_TESTS_GENUM_VALUE3);
}

/**
 * gi_marshalling_tests_genum_out:
 * @v: (out):
 */
void
gi_marshalling_tests_genum_out (GIMarshallingTestsGEnum *v)
{
  *v = GI_MARSHALLING_TESTS_GENUM_VALUE3;
}

/**
 * gi_marshalling_tests_genum_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_genum_out_uninitialized (GIMarshallingTestsGEnum *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_genum_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_genum_inout (GIMarshallingTestsGEnum *v)
{
  g_assert_cmpint (*v, ==, GI_MARSHALLING_TESTS_GENUM_VALUE3);
  *v = GI_MARSHALLING_TESTS_GENUM_VALUE1;
}


GIMarshallingTestsEnum
gi_marshalling_tests_enum_returnv (void)
{
  return GI_MARSHALLING_TESTS_ENUM_VALUE3;
}

void
gi_marshalling_tests_enum_in (GIMarshallingTestsEnum v)
{
  g_assert_cmpint (v, ==, GI_MARSHALLING_TESTS_ENUM_VALUE3);
}

/**
 * gi_marshalling_tests_enum_out:
 * @v: (out):
 */
void
gi_marshalling_tests_enum_out (GIMarshallingTestsEnum *v)
{
  *v = GI_MARSHALLING_TESTS_ENUM_VALUE3;
}

/**
 * gi_marshalling_tests_enum_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_enum_out_uninitialized (GIMarshallingTestsEnum **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_enum_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_enum_inout (GIMarshallingTestsEnum *v)
{
  g_assert_cmpint (*v, ==, GI_MARSHALLING_TESTS_ENUM_VALUE3);
  *v = GI_MARSHALLING_TESTS_ENUM_VALUE1;
}


GType
gi_marshalling_tests_flags_get_type (void)
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0))
    {
      static const GFlagsValue values[] = {
        {GI_MARSHALLING_TESTS_FLAGS_VALUE1,
         "GI_MARSHALLING_TESTS_FLAGS_VALUE1", "value1"},
        {GI_MARSHALLING_TESTS_FLAGS_VALUE2,
         "GI_MARSHALLING_TESTS_FLAGS_VALUE2", "value2"},
        {GI_MARSHALLING_TESTS_FLAGS_VALUE3,
         "GI_MARSHALLING_TESTS_FLAGS_VALUE3", "value3"},
        {GI_MARSHALLING_TESTS_FLAGS_MASK, "GI_MARSHALLING_TESTS_FLAGS_MASK",
         "mask"},
        {GI_MARSHALLING_TESTS_FLAGS_MASK2, "GI_MARSHALLING_TESTS_FLAGS_MASK2",
         "mask2"},
        {0, NULL, NULL}
      };
      type = g_flags_register_static (g_intern_static_string ("GIMarshallingTestsFlags"), values);
    }

  return type;
}

GIMarshallingTestsFlags
gi_marshalling_tests_flags_returnv (void)
{
  return GI_MARSHALLING_TESTS_FLAGS_VALUE2;
}

void
gi_marshalling_tests_flags_in (GIMarshallingTestsFlags v)
{
  g_assert (v == GI_MARSHALLING_TESTS_FLAGS_VALUE2);
}

void
gi_marshalling_tests_flags_in_zero (GIMarshallingTestsFlags v)
{
  g_assert (v == 0);
}

/**
 * gi_marshalling_tests_flags_out:
 * @v: (out):
 */
void
gi_marshalling_tests_flags_out (GIMarshallingTestsFlags *v)
{
  *v = GI_MARSHALLING_TESTS_FLAGS_VALUE2;
}

/**
 * gi_marshalling_tests_flags_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_flags_out_uninitialized (GIMarshallingTestsFlags *v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_flags_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_flags_inout (GIMarshallingTestsFlags *v)
{
  g_assert (*v == GI_MARSHALLING_TESTS_FLAGS_VALUE2);
  *v = GI_MARSHALLING_TESTS_FLAGS_VALUE1;
}


GIMarshallingTestsNoTypeFlags
gi_marshalling_tests_no_type_flags_returnv (void)
{
  return GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2;
}

void
gi_marshalling_tests_no_type_flags_in (GIMarshallingTestsNoTypeFlags v)
{
  g_assert (v == GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2);
}

void
gi_marshalling_tests_no_type_flags_in_zero (GIMarshallingTestsNoTypeFlags v)
{
  g_assert (v == 0);
}

/**
 * gi_marshalling_tests_no_type_flags_out:
 * @v: (out):
 */
void
gi_marshalling_tests_no_type_flags_out (GIMarshallingTestsNoTypeFlags *v)
{
  *v = GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2;
}

/**
 * gi_marshalling_tests_no_type_flags_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_no_type_flags_out_uninitialized (GIMarshallingTestsNoTypeFlags **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_no_type_flags_inout:
 * @v: (inout):
 */
void
gi_marshalling_tests_no_type_flags_inout (GIMarshallingTestsNoTypeFlags *v)
{
  g_assert (*v == GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2);
  *v = GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE1;
}


/**
 * gi_marshalling_tests_simple_struct_returnv:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsSimpleStruct *
gi_marshalling_tests_simple_struct_returnv (void)
{
  static GIMarshallingTestsSimpleStruct *struct_ = NULL;

  if (struct_ == NULL)
    {
      struct_ = g_new (GIMarshallingTestsSimpleStruct, 1);

      struct_->long_ = 6;
      struct_->int8 = 7;
    }

  return struct_;
}

/**
 * gi_marshalling_tests_simple_struct_inv:
 * @struct_: (transfer none):
 */
void
gi_marshalling_tests_simple_struct_inv (GIMarshallingTestsSimpleStruct *struct_)
{
  g_assert_cmpint (struct_->long_, ==, 6);
  g_assert_cmpint (struct_->int8, ==, 7);
}

void
gi_marshalling_tests_simple_struct_method (GIMarshallingTestsSimpleStruct *struct_)
{
  g_assert_cmpint (struct_->long_, ==, 6);
  g_assert_cmpint (struct_->int8, ==, 7);
}


GType
gi_marshalling_tests_pointer_struct_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      type = g_pointer_type_register_static ("GIMarshallingTestsPointerStruct");
    }

  return type;
}

/**
 * gi_marshalling_tests_pointer_struct_returnv:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsPointerStruct *
gi_marshalling_tests_pointer_struct_returnv (void)
{
  static GIMarshallingTestsPointerStruct *struct_ = NULL;

  if (struct_ == NULL)
    {
      struct_ = g_new (GIMarshallingTestsPointerStruct, 1);

      struct_->long_ = 42;
    }

  return struct_;
}

/**
 * gi_marshalling_tests_pointer_struct_inv:
 * @struct_: (transfer none):
 */
void
gi_marshalling_tests_pointer_struct_inv (GIMarshallingTestsPointerStruct *struct_)
{
  g_assert_cmpint (struct_->long_, ==, 42);
}

static GIMarshallingTestsBoxedStruct *
gi_marshalling_tests_boxed_struct_copy (GIMarshallingTestsBoxedStruct *struct_)
{
  GIMarshallingTestsBoxedStruct *new_struct;

  if (struct_ == NULL)
    return NULL;

  new_struct = g_slice_new (GIMarshallingTestsBoxedStruct);

  *new_struct = *struct_;
  new_struct->string_ = g_strdup (struct_->string_);
  new_struct->g_strv = g_strdupv (struct_->g_strv);

  return new_struct;
}

static void
gi_marshalling_tests_boxed_struct_free (GIMarshallingTestsBoxedStruct *struct_)
{
  if (struct_ != NULL)
    {
      g_free (struct_->string_);
      g_clear_pointer (&struct_->g_strv, g_strfreev);
      g_slice_free (GIMarshallingTestsBoxedStruct, struct_);
    }
}

GType
gi_marshalling_tests_boxed_struct_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      type = g_boxed_type_register_static ("GIMarshallingTestsBoxedStruct",
                                           (GBoxedCopyFunc)
                                           gi_marshalling_tests_boxed_struct_copy,
                                           (GBoxedFreeFunc) gi_marshalling_tests_boxed_struct_free);
    }

  return type;
}

static GType
gi_marshalling_tests_boxed_glist_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      type = g_boxed_type_register_static ("GIMarshallingTestsBoxedGList",
                                           (GBoxedCopyFunc) g_list_copy, (GBoxedFreeFunc) g_list_free);
    }

  return type;
}

GIMarshallingTestsBoxedStruct *
gi_marshalling_tests_boxed_struct_new (void)
{
  return g_slice_new0 (GIMarshallingTestsBoxedStruct);
}

/**
 * gi_marshalling_tests_boxed_struct_returnv:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsBoxedStruct *
gi_marshalling_tests_boxed_struct_returnv (void)
{
  static GIMarshallingTestsBoxedStruct *struct_ = NULL;

  if (struct_ == NULL)
    {
      struct_ = g_new (GIMarshallingTestsBoxedStruct, 1);

      struct_->long_ = 42;
      struct_->string_ = g_strdup ("hello");
      struct_->g_strv = g_new0 (gchar *, 4);
      struct_->g_strv[0] = g_strdup ("0");
      struct_->g_strv[1] = g_strdup ("1");
      struct_->g_strv[2] = g_strdup ("2");
      struct_->g_strv[3] = NULL;
    }

  return struct_;
}

/**
 * gi_marshalling_tests_boxed_struct_inv:
 * @struct_: (transfer none):
 */
void
gi_marshalling_tests_boxed_struct_inv (GIMarshallingTestsBoxedStruct *struct_)
{
  g_assert_cmpint (struct_->long_, ==, 42);
}

/**
 * gi_marshalling_tests_boxed_struct_out:
 * @struct_: (out) (transfer none):
 */
void
gi_marshalling_tests_boxed_struct_out (GIMarshallingTestsBoxedStruct **struct_)
{
  static GIMarshallingTestsBoxedStruct *new_struct = NULL;

  if (new_struct == NULL)
    {
      new_struct = g_new0 (GIMarshallingTestsBoxedStruct, 1);

      new_struct->long_ = 42;
    }

  *struct_ = new_struct;
}

/**
 * gi_marshalling_tests_boxed_struct_out_uninitialized:
 * @v: (out) (transfer none):
 */
gboolean
gi_marshalling_tests_boxed_struct_out_uninitialized (GIMarshallingTestsBoxedStruct **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_boxed_struct_inout:
 * @struct_: (inout) (transfer full):
 */
void
gi_marshalling_tests_boxed_struct_inout (GIMarshallingTestsBoxedStruct **struct_)
{
  g_assert_cmpint ((*struct_)->long_, ==, 42);

  g_boxed_free (gi_marshalling_tests_boxed_struct_get_type(), *struct_);
  (*struct_) = g_slice_new0 (GIMarshallingTestsBoxedStruct);
  (*struct_)->long_ = 0;
}

static GIMarshallingTestsUnion *
gi_marshalling_tests_union_copy (GIMarshallingTestsUnion *union_)
{
  GIMarshallingTestsUnion *new_union;

  new_union = g_slice_new (GIMarshallingTestsUnion);

  *new_union = *union_;

  return new_union;
}

static void
gi_marshalling_tests_union_free (GIMarshallingTestsUnion *union_)
{
  g_slice_free (GIMarshallingTestsUnion, union_);
}

GType
gi_marshalling_tests_union_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      type = g_boxed_type_register_static ("GIMarshallingTestsUnion",
                                           (GBoxedCopyFunc)
                                           gi_marshalling_tests_union_copy,
                                           (GBoxedFreeFunc) gi_marshalling_tests_union_free);
    }

  return type;
}

/**
 * gi_marshalling_tests_union_returnv:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsUnion *
gi_marshalling_tests_union_returnv (void)
{
  static GIMarshallingTestsUnion *union_ = NULL;

  if (union_ == NULL)
    {
      union_ = g_new (GIMarshallingTestsUnion, 1);

      union_->long_ = 42;
    }

  return union_;
}

/**
 * gi_marshalling_tests_union_inv:
 * @union_: (transfer none):
 */
void
gi_marshalling_tests_union_inv (GIMarshallingTestsUnion *union_)
{
  g_assert_cmpint (union_->long_, ==, 42);
}

void
gi_marshalling_tests_union_method (GIMarshallingTestsUnion *union_)
{
  g_assert_cmpint (union_->long_, ==, 42);
}

/**
 * gi_marshalling_tests_structured_union_new:
 * @type: Type of #GIMarshallingTestsStructuredUnion to create
 */
GIMarshallingTestsStructuredUnion *
gi_marshalling_tests_structured_union_new (GIMarshallingTestsStructuredUnionType type)
{
  GIMarshallingTestsStructuredUnion *new_union;

  new_union = g_new0 (GIMarshallingTestsStructuredUnion, 1);
  new_union->type = type;

  switch (type)
    {
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NONE:
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SIMPLE_STRUCT:
      new_union->simple_struct.parent.long_ = 6;
      new_union->simple_struct.parent.int8 = 7;
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NESTED_STRUCT:
      new_union->nested_struct.parent.simple_struct.long_ = 6;
      new_union->nested_struct.parent.simple_struct.int8 = 7;
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT:
      new_union->boxed_struct.parent.long_ = 42;
      new_union->boxed_struct.parent.string_ = g_strdup ("hello");
      new_union->boxed_struct.parent.g_strv = g_new0 (gchar *, 4);
      new_union->boxed_struct.parent.g_strv[0] = g_strdup ("0");
      new_union->boxed_struct.parent.g_strv[1] = g_strdup ("1");
      new_union->boxed_struct.parent.g_strv[2] = g_strdup ("2");
      new_union->boxed_struct.parent.g_strv[3] = NULL;
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT_PTR:
      new_union->boxed_struct_ptr.parent = g_boxed_copy (
        gi_marshalling_tests_boxed_struct_get_type(),
        gi_marshalling_tests_boxed_struct_returnv ());
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_POINTER_STRUCT:
      new_union->pointer_struct.parent.long_ = 42;
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SINGLE_UNION:
      new_union->single_union.parent.union_.long_ = 42;
      break;

    default:
      g_free (new_union);
      g_return_val_if_reached (NULL);
    }

  return new_union;
}

static GIMarshallingTestsStructuredUnion *
gi_marshalling_tests_structured_union_copy (GIMarshallingTestsStructuredUnion *union_)
{
  GIMarshallingTestsStructuredUnion *new_union;

  new_union = g_new (GIMarshallingTestsStructuredUnion, 1);
  *new_union = *union_;

  switch (union_->type)
    {
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT:
      new_union->boxed_struct.parent.string_ = g_strdup (union_->boxed_struct.parent.string_);
      if (union_->boxed_struct.parent.g_strv)
        {
          guint length = g_strv_length (union_->boxed_struct.parent.g_strv);
          guint i;

          new_union->boxed_struct.parent.g_strv = g_new0 (gchar *, length + 1);
          for (i = 0; i < length; i++)
              new_union->boxed_struct.parent.g_strv[i] = g_strdup (union_->boxed_struct.parent.g_strv[i]);
          new_union->boxed_struct.parent.g_strv[i] = NULL;
        }
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT_PTR:
      new_union->boxed_struct_ptr.parent = g_boxed_copy (
        gi_marshalling_tests_boxed_struct_get_type(), union_->boxed_struct_ptr.parent);
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NONE:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SINGLE_UNION:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_POINTER_STRUCT:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SIMPLE_STRUCT:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NESTED_STRUCT:
      break;

    default:
      g_return_val_if_reached (new_union);
    }

  return new_union;
}

static void
gi_marshalling_tests_structured_union_free (GIMarshallingTestsStructuredUnion *union_)
{
  switch (union_->type)
    {
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT:
      g_free (union_->boxed_struct.parent.string_);
      g_strfreev (union_->boxed_struct.parent.g_strv);
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT_PTR:
      g_boxed_free (gi_marshalling_tests_boxed_struct_get_type(), union_->boxed_struct_ptr.parent);
      break;

    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NONE:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SINGLE_UNION:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_POINTER_STRUCT:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SIMPLE_STRUCT:
    case GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NESTED_STRUCT:
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (union_);
}

GType gi_marshalling_tests_structured_union_get_type (void)
{
    static GType type = 0;

  if (type == 0)
    {
      type = g_boxed_type_register_static ("GIMarshallingTestsStructuredUnion",
                                           (GBoxedCopyFunc)
                                           gi_marshalling_tests_structured_union_copy,
                                           (GBoxedFreeFunc) gi_marshalling_tests_structured_union_free);
    }

  return type;
}

GIMarshallingTestsStructuredUnionType
gi_marshalling_tests_structured_union_type (GIMarshallingTestsStructuredUnion *structured_union)
{
    return structured_union->type;
}


enum
{
  PROP_0,
  PROP_INT_
};

static void
  gi_marshalling_tests_object_real_method_with_default_implementation (GIMarshallingTestsObject *self, gint8 in);

G_DEFINE_TYPE (GIMarshallingTestsObject, gi_marshalling_tests_object, G_TYPE_OBJECT);

static void
gi_marshalling_tests_object_init (GIMarshallingTestsObject *self G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (gi_marshalling_tests_object_parent_class)->finalize (object);
}

static void
gi_marshalling_tests_object_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  g_return_if_fail (GI_MARSHALLING_TESTS_IS_OBJECT (object));

  switch (prop_id)
    {
    case PROP_INT_:
      GI_MARSHALLING_TESTS_OBJECT (object)->int_ = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gi_marshalling_tests_object_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  g_return_if_fail (GI_MARSHALLING_TESTS_IS_OBJECT (object));

  switch (prop_id)
    {
    case PROP_INT_:
      g_value_set_int (value, GI_MARSHALLING_TESTS_OBJECT (object)->int_);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gi_marshalling_tests_object_class_init (GIMarshallingTestsObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
#if 0
  GObjectClass *parent_class = G_OBJECT_CLASS (klass);
#endif

  object_class->finalize = gi_marshalling_tests_object_finalize;
  object_class->set_property = gi_marshalling_tests_object_set_property;
  object_class->get_property = gi_marshalling_tests_object_get_property;

  g_object_class_install_property (object_class, PROP_INT_,
                                   g_param_spec_int ("int", "Integer",
                                                     "An integer", G_MININT,
                                                     G_MAXINT, 0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  klass->method_with_default_implementation = gi_marshalling_tests_object_real_method_with_default_implementation;
}


void
gi_marshalling_tests_object_static_method (void)
{
}

void
gi_marshalling_tests_object_method (GIMarshallingTestsObject *object)
{
  g_return_if_fail (GI_MARSHALLING_TESTS_IS_OBJECT (object));
  g_assert_cmpint (object->int_, ==, 42);
}

void
gi_marshalling_tests_object_overridden_method (GIMarshallingTestsObject *object)
{
  g_return_if_fail (GI_MARSHALLING_TESTS_IS_OBJECT (object));
  g_assert_cmpint (object->int_, ==, 0);
}

GIMarshallingTestsObject *
gi_marshalling_tests_object_new (gint int_)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, "int", int_, NULL);
}

GIMarshallingTestsObject *
gi_marshalling_tests_object_new_fail (gint     int_ G_GNUC_UNUSED,
                                      GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  g_set_error_literal (error,
                       g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN),
                       GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE,
                       GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE);

  return NULL;
}

/**
 * gi_marshalling_tests_object_method_array_in:
 * @ints: (array length=length):
 */
void
gi_marshalling_tests_object_method_array_in (GIMarshallingTestsObject *self G_GNUC_UNUSED,
                                             const gint               *ints,
                                             gint                      length)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);
}

/**
 * gi_marshalling_tests_object_method_array_out:
 * @ints: (out) (array length=length) (transfer none):
 */
void
gi_marshalling_tests_object_method_array_out (GIMarshallingTestsObject *self G_GNUC_UNUSED,
                                              gint                    **ints,
                                              gint                     *length)
{
  static gint values[] = { -1, 0, 1, 2 };

  *length = 4;
  *ints = values;
}

/**
 * gi_marshalling_tests_object_method_array_inout:
 * @ints: (inout) (array length=length) (transfer none):
 * @length: (inout):
 */
void
gi_marshalling_tests_object_method_array_inout (GIMarshallingTestsObject *self G_GNUC_UNUSED,
                                                gint                    **ints,
                                                gint                     *length)
{
  static gint values[] = { -2, -1, 0, 1, 2 };

  g_assert_cmpint (*length, ==, 4);
  g_assert_cmpint ((*ints)[0], ==, -1);
  g_assert_cmpint ((*ints)[1], ==, 0);
  g_assert_cmpint ((*ints)[2], ==, 1);
  g_assert_cmpint ((*ints)[3], ==, 2);

  *length = 5;
  *ints = values;
}

/**
 * gi_marshalling_tests_object_method_array_return:
 *
 * Returns: (array length=length):
 */
const gint *
gi_marshalling_tests_object_method_array_return (GIMarshallingTestsObject *self G_GNUC_UNUSED,
                                                 gint                     *length)
{
  static gint ints[] = { -1, 0, 1, 2 };

  *length = 4;
  return ints;
}

/**
 * gi_marshalling_tests_object_method_int8_in:
 * @in: (in):
 */
void
gi_marshalling_tests_object_method_int8_in (GIMarshallingTestsObject *self, gint8 in)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_int8_in (self, in);
}

/**
 * gi_marshalling_tests_object_method_int8_out:
 * @out: (out):
 */
void
gi_marshalling_tests_object_method_int8_out (GIMarshallingTestsObject *self, gint8 *out)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_int8_out (self, out);
}

/**
 * gi_marshalling_tests_object_method_int8_arg_and_out_caller:
 * @out: (out):
 */
void
  gi_marshalling_tests_object_method_int8_arg_and_out_caller (GIMarshallingTestsObject *self, gint8 arg, gint8 *out)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_int8_arg_and_out_caller (self, arg, out);
}

/**
 * gi_marshalling_tests_object_method_int8_arg_and_out_callee:
 * @out: (out):
 */
void
  gi_marshalling_tests_object_method_int8_arg_and_out_callee (GIMarshallingTestsObject *self, gint8 arg, gint8 **out)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_int8_arg_and_out_callee (self, arg, out);
}

/**
 * gi_marshalling_tests_object_method_str_arg_out_ret:
 * @out: (out):
 *
 * Returns: (transfer none)
 */
const gchar *
gi_marshalling_tests_object_method_str_arg_out_ret (GIMarshallingTestsObject *self, const gchar *arg, guint *out)
{
  return GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_str_arg_out_ret (self, arg, out);
}

/**
 * gi_marshalling_tests_object_method_with_default_implementation:
 * @in: (in):
 */
void gi_marshalling_tests_object_method_with_default_implementation (GIMarshallingTestsObject *self, gint8 in)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->method_with_default_implementation (self, in);
}

static void
  gi_marshalling_tests_object_real_method_with_default_implementation (GIMarshallingTestsObject *self, gint8 in)
{
  GValue val = { 0, };
  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, in);
  g_object_set_property (G_OBJECT (self), "int", &val);
}

/**
 * gi_marshalling_tests_object_vfunc_with_callback: (virtual vfunc_with_callback)
 * @callback: (scope call) (closure callback_data):
 * @callback_data: (allow-none):
 */
void
gi_marshalling_tests_object_vfunc_with_callback (GIMarshallingTestsObject        *self G_GNUC_UNUSED,
                                                 GIMarshallingTestsCallbackIntInt callback G_GNUC_UNUSED,
                                                 void                            *callback_data G_GNUC_UNUSED)
{

}

static int
_callback (int val, void *user_data)
{
  g_assert (user_data == (gpointer) 0xdeadbeef);
  return val;
}

void
gi_marshalling_tests_object_call_vfunc_with_callback (GIMarshallingTestsObject *object)
{
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (object)->vfunc_with_callback (object, _callback, (void *) 0xdeadbeef);
}

/**
 * gi_marshalling_tests_object_none_return:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsObject *
gi_marshalling_tests_object_none_return (void)
{
  static GIMarshallingTestsObject *object = NULL;

  if (object == NULL)
    {
      object = g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
    }

  return object;
}

/**
 * gi_marshalling_tests_object_full_return:
 *
 * Returns: (transfer full):
 */
GIMarshallingTestsObject *
gi_marshalling_tests_object_full_return (void)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
}

/**
 * gi_marshalling_tests_object_none_in:
 * @object: (transfer none):
 */
void
gi_marshalling_tests_object_none_in (GIMarshallingTestsObject *object)
{
  g_assert_cmpint (object->int_, ==, 42);
}

/**
 * gi_marshalling_tests_object_none_out:
 * @object: (out) (transfer none):
 */
void
gi_marshalling_tests_object_none_out (GIMarshallingTestsObject **object)
{
  static GIMarshallingTestsObject *new_object = NULL;

  if (new_object == NULL)
    {
      new_object = g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
    }

  *object = new_object;
}

/**
 * gi_marshalling_tests_object_none_out_uninitialized:
 * @v: (out) (transfer none):
 */
gboolean
gi_marshalling_tests_object_none_out_uninitialized (GIMarshallingTestsObject **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_object_full_out:
 * @object: (out) (transfer full):
 */
void
gi_marshalling_tests_object_full_out (GIMarshallingTestsObject **object)
{
  *object = g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
}

/**
 * gi_marshalling_tests_object_full_out_uninitialized:
 * @v: (out) (transfer full):
 */
gboolean
gi_marshalling_tests_object_full_out_uninitialized (GIMarshallingTestsObject **v G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_object_none_inout:
 * @object: (inout) (transfer none):
 */
void
gi_marshalling_tests_object_none_inout (GIMarshallingTestsObject **object)
{
  static GIMarshallingTestsObject *new_object = NULL;

  g_assert_cmpint ((*object)->int_, ==, 42);

  if (new_object == NULL)
    {
      new_object = g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
      new_object->int_ = 0;
    }

  *object = new_object;
}

/**
 * gi_marshalling_tests_object_full_inout:
 * @object: (inout) (transfer full):
 */
void
gi_marshalling_tests_object_full_inout (GIMarshallingTestsObject **object)
{
  g_assert_cmpint ((*object)->int_, ==, 42);

  g_object_unref (*object);
  *object = g_object_new (GI_MARSHALLING_TESTS_TYPE_OBJECT, NULL);
}

/**
 * gi_marshalling_tests_object_int8_in:
 * @in: (in):
 */
void
gi_marshalling_tests_object_int8_in (GIMarshallingTestsObject *object, gint8 in)
{
  gi_marshalling_tests_object_method_int8_in (object, in);
}

/**
 * gi_marshalling_tests_object_int8_out:
 * @out: (out):
 */
void
gi_marshalling_tests_object_int8_out (GIMarshallingTestsObject *object, gint8 *out)
{
  gi_marshalling_tests_object_method_int8_out (object, out);
}

/**
 * gi_marshalling_tests_object_vfunc_return_value_only:
 */
glong
gi_marshalling_tests_object_vfunc_return_value_only (GIMarshallingTestsObject *self)
{
  /* make sure that local variables don't get smashed */
  glong return_value;
  gulong local = 0x12345678;
  return_value = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_value_only (self);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_one_out_parameter:
 * @a: (out):
 */
void
gi_marshalling_tests_object_vfunc_one_out_parameter (GIMarshallingTestsObject *self, gfloat *a)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_one_out_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_one_inout_parameter:
 * @a: (inout):
 */
void
gi_marshalling_tests_object_vfunc_one_inout_parameter (GIMarshallingTestsObject *self, gfloat *a)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_one_inout_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_multiple_inout_parameters:
 * @a: (inout):
 * @b: (inout):
 */
void
gi_marshalling_tests_object_vfunc_multiple_inout_parameters (GIMarshallingTestsObject *self, gfloat *a, gfloat *b)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_multiple_inout_parameters (self, a, b);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_multiple_out_parameters:
 * @a: (out):
 * @b: (out):
 */
void gi_marshalling_tests_object_vfunc_multiple_out_parameters (GIMarshallingTestsObject *self, gfloat *a, gfloat *b)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_multiple_out_parameters (self, a, b);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_caller_allocated_out_parameter:
 * @a: (out):
 */
void gi_marshalling_tests_object_vfunc_caller_allocated_out_parameter (GIMarshallingTestsObject *self, GValue *a)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_caller_allocated_out_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_array_out_parameter:
 * @a: (out) (array zero-terminated):
 */
void gi_marshalling_tests_object_vfunc_array_out_parameter (GIMarshallingTestsObject *self, gfloat **a)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_array_out_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_return_value_and_one_out_parameter:
 * @a: (out):
 */
glong gi_marshalling_tests_object_vfunc_return_value_and_one_out_parameter (GIMarshallingTestsObject *self, glong *a)
{
  /* make sure that local variables don't get smashed */
  gulong return_value;
  gulong local = 0x12345678;
  return_value = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_value_and_one_out_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_return_value_and_multiple_out_parameters:
 * @a: (out):
 * @b: (out):
 */
glong
  gi_marshalling_tests_object_vfunc_return_value_and_multiple_out_parameters
  (GIMarshallingTestsObject *self, glong *a, glong *b)
{
  gulong return_value;
  gulong local = 0x12345678;
  return_value =
    GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_value_and_multiple_out_parameters (self, a, b);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_return_value_and_one_inout_parameter:
 * @a: (inout):
 */
glong gi_marshalling_tests_object_vfunc_return_value_and_one_inout_parameter (GIMarshallingTestsObject *self, glong *a)
{
  /* make sure that local variables don't get smashed */
  gulong return_value;
  gulong local = 0x12345678;
  return_value = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_value_and_one_inout_parameter (self, a);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_return_value_and_multiple_inout_parameters:
 * @a: (inout):
 * @b: (inout):
 */
glong
  gi_marshalling_tests_object_vfunc_return_value_and_multiple_inout_parameters
  (GIMarshallingTestsObject *self, glong *a, glong *b)
{
  gulong return_value;
  gulong local = 0x12345678;
  return_value =
    GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_value_and_multiple_inout_parameters (self, a, b);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_callback_owned_boxed:
 * @callback: (scope call) (closure callback_data):
 * @callback_data: (allow-none):
 */
glong
gi_marshalling_tests_callback_owned_boxed (GIMarshallingTestsCallbackOwnedBoxed callback,
                                           void *callback_data)
{
  static GIMarshallingTestsBoxedStruct *box = NULL;
  glong ret;

  if (!box)
    box = gi_marshalling_tests_boxed_struct_new ();
  box->long_++;
  callback (box, callback_data);
  ret = box->long_;
  return ret;
}

gboolean
gi_marshalling_tests_object_vfunc_meth_with_error (GIMarshallingTestsObject *self, gint x, GError **error)
{
  gulong local = 0x12345678;
  gboolean ret = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_meth_with_err (self,
                                                                                    x,
                                                                                    error);
  g_assert_cmpint (local, ==, 0x12345678);
  return ret;
}

/**
 * gi_marshalling_tests_object_vfunc_return_enum:
 */
GIMarshallingTestsEnum
gi_marshalling_tests_object_vfunc_return_enum (GIMarshallingTestsObject *self)
{
  /* make sure that local variables don't get smashed */
  GIMarshallingTestsEnum return_value;
  glong local = 0x12345678;
  return_value = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_enum (self);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_out_enum:
 * @_enum: (out):
 */
void
gi_marshalling_tests_object_vfunc_out_enum (GIMarshallingTestsObject *self, GIMarshallingTestsEnum *_enum)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_out_enum (self, _enum);
  g_assert_cmpint (local, ==, 0x12345678);
}

/**
 * gi_marshalling_tests_object_vfunc_return_flags:
 */
GIMarshallingTestsFlags
gi_marshalling_tests_object_vfunc_return_flags (GIMarshallingTestsObject *self)
{
  /* make sure that local variables don't get smashed */
  GIMarshallingTestsFlags return_value;
  glong local = 0x12345678;
  return_value = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_flags (self);
  g_assert_cmpint (local, ==, 0x12345678);
  return return_value;
}

/**
 * gi_marshalling_tests_object_vfunc_out_flags:
 * @flags: (out):
 */
void
gi_marshalling_tests_object_vfunc_out_flags (GIMarshallingTestsObject *self, GIMarshallingTestsFlags *flags)
{
  /* make sure that local variables don't get smashed */
  gulong local = 0x12345678;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_out_flags (self, flags);
  g_assert_cmpuint (local, ==, 0x12345678);
}


/* NOTE:
 *
 * The following (get_ref_info_for_*) methods are designed to call vfuncs related
 * to object argument marshaling. They do not pass the resulting objects through them
 * as regular vfunc wrapper method do, but rather return reference count and floating
 * information back to the callers. This is useful because callers can do testing of
 * expected reference counts in isolation and from the perspective of C. This is important
 * because if there are bugs in the reverse marshaling, they can obfuscate or compound
 * bugs in marshaling from the vfuncs.
 */

/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_none:
 * @ref_count: (out): Ref count of the object returned from the vfunc directly after vfunc call.
 * @is_floating: (out): Floating state object returned from the vfunc directly after vfunc call.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_none
  (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating)
{
  GObject *object = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_object_transfer_none (self);
  *ref_count = object->ref_count;
  *is_floating = g_object_is_floating (object);

  /* Attempt to sink and unref the returned object and avoid any potential leaks */
  g_object_ref_sink (object);
  g_object_unref (object);
}

/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_full:
 * @ref_count: (out): Ref count of the object returned from the vfunc directly after vfunc call.
 * @is_floating: (out): Floating state object returned from the vfunc directly after vfunc call.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_full
  (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating)
{
  GObject *object = GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_return_object_transfer_full (self);
  *ref_count = object->ref_count;
  *is_floating = g_object_is_floating (object);
  g_object_unref (object);
}

/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_none:
 * @ref_count: (out): Ref count of the object returned from the vfunc directly after vfunc call.
 * @is_floating: (out): Floating state object returned from the vfunc directly after vfunc call.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_none
  (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating)
{
  GObject *object = NULL;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_out_object_transfer_none (self, &object);
  *ref_count = object->ref_count;
  *is_floating = g_object_is_floating (object);

  /* Attempt to sink and unref the returned object and avoid any potential leaks */
  g_object_ref_sink (object);
  g_object_unref (object);
}

/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_full:
 * @ref_count: (out): Ref count of the object returned from the vfunc directly after vfunc call.
 * @is_floating: (out): Floating state object returned from the vfunc directly after vfunc call.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_full
  (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating)
{
  GObject *object = NULL;
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_out_object_transfer_full (self, &object);
  *ref_count = object->ref_count;
  *is_floating = g_object_is_floating (object);
  g_object_unref (object);
}

static void
_vfunc_in_object_destroy_callback (gboolean *destroy_called,
                                   GObject  *where_the_object_was G_GNUC_UNUSED)
{
  *destroy_called = TRUE;
}

/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_none:
 * @type: GType of object to create and pass as in argument to the vfunc
 * @ref_count: (out): Ref count of the in object directly after vfunc call.
 * @is_floating: (out): Floating state of in object directly after vfunc call.
 *
 * Calls vfunc_in_object_transfer_none with a new object of the given type.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_none
  (GIMarshallingTestsObject *self, GType type, guint *ref_count, gboolean *is_floating)
{
  static gboolean destroy_called;
  GObject *object;
  destroy_called = FALSE;

  object = g_object_new (type, NULL);
  g_object_weak_ref (object, (GWeakNotify) _vfunc_in_object_destroy_callback, &destroy_called);

  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_in_object_transfer_none (self, object);
  if (destroy_called)
    {
      *ref_count = 0;
      *is_floating = FALSE;
    }
  else
    {
      *ref_count = object->ref_count;
      *is_floating = g_object_is_floating (object);
      g_object_unref (object);
    }
}


/**
 * gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_full:
 * @type: GType of object to create and pass as in argument to the vfunc
 * @ref_count: (out): Ref count of the in object directly after vfunc call.
 * @is_floating: (out): Floating state of in object directly after vfunc call.
 */
void
  gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_full
  (GIMarshallingTestsObject *self, GType type, guint *ref_count, gboolean *is_floating)
{
  static gboolean destroy_called;
  GObject *object;
  destroy_called = FALSE;

  object = g_object_new (type, NULL);
  g_object_weak_ref (object, (GWeakNotify) _vfunc_in_object_destroy_callback, &destroy_called);

  /* Calling the vfunc takes ownership of the object, so we use a weak_ref to determine
   * if the object gets destroyed after the call and appropriately return 0 as the ref count.
   */
  GI_MARSHALLING_TESTS_OBJECT_GET_CLASS (self)->vfunc_in_object_transfer_full (self, object);
  if (destroy_called)
    {
      *ref_count = 0;
      *is_floating = FALSE;
    }
  else
    {
      *ref_count = object->ref_count;
      *is_floating = g_object_is_floating (object);
    }
}


G_DEFINE_TYPE (GIMarshallingTestsSubObject, gi_marshalling_tests_sub_object, GI_MARSHALLING_TESTS_TYPE_OBJECT);

static void
gi_marshalling_tests_sub_object_init (GIMarshallingTestsSubObject *self G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_sub_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (gi_marshalling_tests_sub_object_parent_class)->finalize (object);
}

static void
method_deep_hierarchy (GIMarshallingTestsObject *self, gint8 in)
{
  GValue val = { 0, };
  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, in);
  g_object_set_property (G_OBJECT (self), "int", &val);
}

static void
gi_marshalling_tests_sub_object_class_init (GIMarshallingTestsSubObjectClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gi_marshalling_tests_sub_object_finalize;
  GI_MARSHALLING_TESTS_OBJECT_CLASS (klass)->method_deep_hierarchy = method_deep_hierarchy;
}

void
gi_marshalling_tests_sub_object_sub_method (GIMarshallingTestsSubObject *object)
{
  g_assert_cmpint (GI_MARSHALLING_TESTS_OBJECT (object)->int_, ==, 0);
}

void gi_marshalling_tests_sub_object_overwritten_method (GIMarshallingTestsSubObject *object)
{
  g_assert_cmpint (GI_MARSHALLING_TESTS_OBJECT (object)->int_, ==, 0);
}

G_DEFINE_TYPE (GIMarshallingTestsSubSubObject,
               gi_marshalling_tests_sub_sub_object, GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT);

static void
gi_marshalling_tests_sub_sub_object_init (GIMarshallingTestsSubSubObject *self G_GNUC_UNUSED)
{
}

static void gi_marshalling_tests_sub_sub_object_class_init (GIMarshallingTestsSubSubObjectClass *klass G_GNUC_UNUSED)
{
}

/* Interfaces */

static void
gi_marshalling_tests_interface_class_init (void *g_iface G_GNUC_UNUSED,
                                           void *data G_GNUC_UNUSED)
{
}

GType
gi_marshalling_tests_interface_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      /* Not adding prerequisite here for test purposes */
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            "GIMarshallingTestsInterface",
                                            sizeof
                                            (GIMarshallingTestsInterfaceIface),
                                            (GClassInitFunc) gi_marshalling_tests_interface_class_init, 0, NULL, 0);
    }

  return type;
}

/**
 * gi_marshalling_tests_interface_test_int8_in:
 * @in: (in):
 */
void
gi_marshalling_tests_interface_test_int8_in (GIMarshallingTestsInterface *self, gint8 in)
{
  GI_MARSHALLING_TESTS_INTERFACE_GET_IFACE (self)->test_int8_in (self, in);
}

/**
 * gi_marshalling_tests_test_interface_test_int8_in:
 * @in: (in):
 */
void
gi_marshalling_tests_test_interface_test_int8_in (GIMarshallingTestsInterface *test_iface, gint8 in)
{
  gi_marshalling_tests_interface_test_int8_in (test_iface, in);
}


static void test_interface_init (GIMarshallingTestsInterfaceIface *iface);

G_DEFINE_TYPE_WITH_CODE (GIMarshallingTestsInterfaceImpl, gi_marshalling_tests_interface_impl, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE(GI_MARSHALLING_TESTS_TYPE_INTERFACE, test_interface_init))

static void
gi_marshalling_tests_interface_impl_test_int8_in (GIMarshallingTestsInterface *self G_GNUC_UNUSED,
                                                  gint8                        in G_GNUC_UNUSED)
{
}

static void test_interface_init (GIMarshallingTestsInterfaceIface *iface)
{
  iface->test_int8_in = gi_marshalling_tests_interface_impl_test_int8_in;
}

static void
gi_marshalling_tests_interface_impl_init (GIMarshallingTestsInterfaceImpl *self G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_interface_impl_class_init (GIMarshallingTestsInterfaceImplClass *klass G_GNUC_UNUSED)
{
}

/**
 * gi_marshalling_tests_interface_impl_get_as_interface:
 *
 * Returns: (transfer none):
 */
GIMarshallingTestsInterface *
gi_marshalling_tests_interface_impl_get_as_interface (GIMarshallingTestsInterfaceImpl *self)
{
  return (GIMarshallingTestsInterface *) self;
}

static void
gi_marshalling_tests_interface2_class_init (void *g_iface G_GNUC_UNUSED,
                                            void *data G_GNUC_UNUSED)
{
}

GType
gi_marshalling_tests_interface2_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            "GIMarshallingTestsInterface2",
                                            sizeof
                                            (GIMarshallingTestsInterface2Iface),
                                            (GClassInitFunc) gi_marshalling_tests_interface2_class_init, 0, NULL, 0);
    }

  return type;
}

static void
gi_marshalling_tests_interface3_class_init (void *g_iface G_GNUC_UNUSED,
                                            void *data G_GNUC_UNUSED)
{
}

GType
gi_marshalling_tests_interface3_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            "GIMarshallingTestsInterface3",
                                            sizeof
                                            (GIMarshallingTestsInterface3Iface),
                                            (GClassInitFunc) gi_marshalling_tests_interface3_class_init, 0, NULL, 0);
    }

  return type;
}

/**
 * gi_marshalling_tests_interface3_test_variant_array_in:
 * @in: (array length=n_in):
 * @n_in:
 */
void
  gi_marshalling_tests_interface3_test_variant_array_in
  (GIMarshallingTestsInterface3 *self, GVariant **in, gsize n_in)
{
  GI_MARSHALLING_TESTS_INTERFACE3_GET_IFACE (self)->test_variant_array_in (self, in, n_in);
}

/**
 * gi_marshalling_tests_int_out_out:
 * @int0: (out):
 * @int1: (out):
 */
void
gi_marshalling_tests_int_out_out (gint *int0, gint *int1)
{
  *int0 = 6;
  *int1 = 7;
}

/**
 * gi_marshalling_tests_int_three_in_three_out:
 * @a: (in):
 * @b: (in):
 * @c: (in):
 * @out0: (out):
 * @out1: (out):
 * @out2: (out):
 */
void
gi_marshalling_tests_int_three_in_three_out (gint a, gint b, gint c, gint *out0, gint *out1, gint *out2)
{
  *out0 = a;
  *out1 = b;
  *out2 = c;
}

/**
 * gi_marshalling_tests_int_return_out:
 * @int_: (out):
 */
gint
gi_marshalling_tests_int_return_out (gint *int_)
{
  *int_ = 7;
  return 6;
}

/**
* gi_marshalling_tests_int_two_in_utf8_two_in_with_allow_none:
* @a: (in): Must be 1
* @b: (in): Must be 2
* @c: (in) (allow-none): Must be "3" or NULL
* @d: (in) (allow-none): Must be "4" or NULL
*/
void
gi_marshalling_tests_int_two_in_utf8_two_in_with_allow_none (gint a, gint b, const gchar *c, const gchar *d)
{
    g_assert_cmpint (a, ==, 1);
    g_assert_cmpint (b, ==, 2);
    if (c != NULL)
        g_assert_cmpstr (c, ==, "3");
    if (d != NULL)
        g_assert_cmpstr (d, ==, "4");
}

/**
* gi_marshalling_tests_int_one_in_utf8_two_in_one_allows_none:
* @a: (in): Must be 1
* @b: (in) (allow-none): Must be "2" or NULL
* @c: (in): Must be "3"
*/
void
gi_marshalling_tests_int_one_in_utf8_two_in_one_allows_none (gint a, const gchar *b, const gchar *c)
{
    g_assert_cmpint (a, ==, 1);
    if (b != NULL)
        g_assert_cmpstr (b, ==, "2");
    g_assert_cmpstr (c, ==, "3");
}

/**
 * gi_marshalling_tests_array_in_utf8_two_in:
 * @ints: (array length=length):
 * @length:
 * @a: (in) (allow-none): Must be "1" or NULL
 * @b: (in) (allow-none): Must be "2" or NULL
 */
void
gi_marshalling_tests_array_in_utf8_two_in (const gint *ints, gint length, const gchar *a, const gchar *b)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);

  if (a != NULL)
      g_assert_cmpstr (a, ==, "1");
  if (b != NULL)
      g_assert_cmpstr (b, ==, "2");
}

/**
 * gi_marshalling_tests_array_in_utf8_two_in_out_of_order:
 * @length:
 * @a: (in) (allow-none): Must be "1" or NULL
 * @ints: (array length=length):
 * @b: (in) (allow-none): Must be "2" or NULL
 */
void
gi_marshalling_tests_array_in_utf8_two_in_out_of_order (gint length, const gchar *a, const gint *ints, const gchar *b)
{
  g_assert_cmpint (length, ==, 4);
  g_assert_cmpint (ints[0], ==, -1);
  g_assert_cmpint (ints[1], ==, 0);
  g_assert_cmpint (ints[2], ==, 1);
  g_assert_cmpint (ints[3], ==, 2);

  if (a != NULL)
      g_assert_cmpstr (a, ==, "1");
  if (b != NULL)
      g_assert_cmpstr (b, ==, "2");
}

/* GError */

void
gi_marshalling_tests_gerror (GError **error)
{
  GQuark quark = g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN);
  g_set_error_literal (error, quark,
                       GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE, GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE);
}

/**
 * gi_marshalling_tests_gerror_array_in:
 * @in_ints: (array zero-terminated):
 */
void
gi_marshalling_tests_gerror_array_in (gint    *in_ints G_GNUC_UNUSED,
                                      GError **error)
{
  GQuark quark = g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN);
  g_set_error_literal (error, quark,
                       GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE, GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE);
}

/**
 * gi_marshalling_tests_gerror_out:
 * @error: (out) (allow-none) (transfer full): location for the GError.
 * @debug: (out) (allow-none) (transfer full): location for the debug message
 *
 * Inspired by gst_message_parse_error.
 */
void
gi_marshalling_tests_gerror_out (GError **error, gchar **debug)
{
  GQuark quark = g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN);
  g_set_error_literal (error, quark,
                       GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE, GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE);

  if (debug != NULL)
    {
      *debug = g_strdup (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DEBUG_MESSAGE);
    }
}

/**
 * gi_marshalling_tests_gerror_out_uninitialized:
 * @v: (out) (allow-none) (transfer full):
 * @v2: (out) (allow-none) (transfer full):
 */
gboolean
gi_marshalling_tests_gerror_out_uninitialized (GError **v G_GNUC_UNUSED, gchar **v2 G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gerror_out_transfer_none:
 * @err: (out) (allow-none) (transfer none): location for the GError.
 * @debug: (out) (allow-none) (transfer none): location for the debug message
 *
 * A variant of gi_marshalling_tests_gerror_out() which returns data the caller
 * must not free.
 */
void
gi_marshalling_tests_gerror_out_transfer_none (GError **err, const gchar **debug)
{
  static GError error = { 0,
    GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE,
    (gchar *) GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE
  };
  error.domain = g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN);
  *err = &error;
  *debug = GI_MARSHALLING_TESTS_CONSTANT_GERROR_DEBUG_MESSAGE;
}

/**
 * gi_marshalling_tests_gerror_out_transfer_none_uninitialized:
 * @v: (out) (allow-none) (transfer none):
 * @v2: (out) (allow-none) (transfer none):
 */
gboolean
gi_marshalling_tests_gerror_out_transfer_none_uninitialized (GError **v G_GNUC_UNUSED, const gchar **v2 G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * gi_marshalling_tests_gerror_return:
 *
 * Yet another variant of gi_marshalling_tests_gerror_out().
 *
 * Returns: (transfer full): a GError
 */
GError *
gi_marshalling_tests_gerror_return (void)
{
  GQuark quark = g_quark_from_static_string (GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN);

  return g_error_new_literal (quark, GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE, GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE);
}

static GIMarshallingTestsOverridesStruct *
gi_marshalling_tests_overrides_struct_copy (GIMarshallingTestsOverridesStruct *struct_)
{
  GIMarshallingTestsOverridesStruct *new_struct;

  new_struct = g_slice_new (GIMarshallingTestsOverridesStruct);

  *new_struct = *struct_;

  return new_struct;
}

static void
gi_marshalling_tests_overrides_struct_free (GIMarshallingTestsOverridesStruct *struct_)
{
  g_slice_free (GIMarshallingTestsOverridesStruct, struct_);
}

GType
gi_marshalling_tests_overrides_struct_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      type =
        g_boxed_type_register_static ("GIMarshallingTestsOverridesStruct",
                                      (GBoxedCopyFunc)
                                      gi_marshalling_tests_overrides_struct_copy,
                                      (GBoxedFreeFunc) gi_marshalling_tests_overrides_struct_free);
    }

  return type;
}

GIMarshallingTestsOverridesStruct *
gi_marshalling_tests_overrides_struct_new (void)
{
  return g_slice_new (GIMarshallingTestsOverridesStruct);
}

glong gi_marshalling_tests_overrides_struct_method (GIMarshallingTestsOverridesStruct *self G_GNUC_UNUSED)
{
  return 42;
}


/**
 * gi_marshalling_tests_overrides_struct_returnv:
 *
 * Returns: (transfer full):
 */
GIMarshallingTestsOverridesStruct *
gi_marshalling_tests_overrides_struct_returnv (void)
{
  return gi_marshalling_tests_overrides_struct_new ();
}


G_DEFINE_TYPE (GIMarshallingTestsOverridesObject, gi_marshalling_tests_overrides_object, G_TYPE_OBJECT);

static void
gi_marshalling_tests_overrides_object_init (GIMarshallingTestsOverridesObject *self G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_overrides_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (gi_marshalling_tests_overrides_object_parent_class)->finalize (object);
}

static void gi_marshalling_tests_overrides_object_class_init (GIMarshallingTestsOverridesObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
#if 0
  GObjectClass *parent_class = G_OBJECT_CLASS (klass);
#endif

  object_class->finalize = gi_marshalling_tests_overrides_object_finalize;
}

GIMarshallingTestsOverridesObject *
gi_marshalling_tests_overrides_object_new (void)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT, NULL);
}

glong gi_marshalling_tests_overrides_object_method (GIMarshallingTestsOverridesObject *self G_GNUC_UNUSED)
{
  return 42;
}

/**
 * gi_marshalling_tests_overrides_object_returnv:
 *
 * Returns: (transfer full):
 */
GIMarshallingTestsOverridesObject *
gi_marshalling_tests_overrides_object_returnv (void)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT, NULL);
}

/**
 * gi_marshalling_tests_filename_list_return:
 *
 * Returns: (transfer none) (element-type filename): List of filenames
 */
GSList *
gi_marshalling_tests_filename_list_return (void)
{
  return NULL;
}

/**
 * gi_marshalling_tests_param_spec_in_bool:
 */
void
gi_marshalling_tests_param_spec_in_bool (const GParamSpec *param)
{
  g_assert (G_IS_PARAM_SPEC (param));
  g_assert_cmpint (G_PARAM_SPEC_VALUE_TYPE (param), ==, G_TYPE_BOOLEAN);
  g_assert_cmpstr (g_param_spec_get_name ((GParamSpec *) param), ==, "mybool");
}

/**
 * gi_marshalling_tests_param_spec_return:
 *
 * Returns: (transfer full): a #GParamSpec
 */
GParamSpec *
gi_marshalling_tests_param_spec_return (void)
{
  return g_param_spec_string ("test-param", "test", "This is a test", "42", G_PARAM_READABLE);
}

/**
 * gi_marshalling_tests_param_spec_out:
 * @param: (out):
 */
void
gi_marshalling_tests_param_spec_out (GParamSpec **param)
{
  *param = g_param_spec_string ("test-param", "test", "This is a test", "42", G_PARAM_READABLE);
}

/**
 * gi_marshalling_tests_param_spec_out_uninitialized:
 * @v: (out):
 */
gboolean
gi_marshalling_tests_param_spec_out_uninitialized (GParamSpec **v G_GNUC_UNUSED)
{
  return FALSE;
}

enum
{
  DUMMY_PROPERTY,
  SOME_BOOLEAN_PROPERTY,
  SOME_CHAR_PROPERTY,
  SOME_UCHAR_PROPERTY,
  SOME_INT_PROPERTY,
  SOME_UINT_PROPERTY,
  SOME_LONG_PROPERTY,
  SOME_ULONG_PROPERTY,
  SOME_INT64_PROPERTY,
  SOME_UINT64_PROPERTY,
  SOME_FLOAT_PROPERTY,
  SOME_STRING_PROPERTY,
  SOME_DOUBLE_PROPERTY,
  SOME_STRV_PROPERTY,
  SOME_BOXED_STRUCT_PROPERTY,
  SOME_VARIANT_PROPERTY,
  SOME_BOXED_GLIST_PROPERTY,
  SOME_GVALUE_PROPERTY,
  SOME_OBJECT_PROPERTY,
  SOME_FLAGS_PROPERTY,
  SOME_ENUM_PROPERTY,
  SOME_BYTE_ARRAY_PROPERTY,
  SOME_READONLY_PROPERTY,
};

G_DEFINE_TYPE (GIMarshallingTestsPropertiesObject, gi_marshalling_tests_properties_object, G_TYPE_OBJECT);

static void gi_marshalling_tests_properties_object_init (GIMarshallingTestsPropertiesObject *self G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_properties_object_finalize (GObject *obj)
{
  GIMarshallingTestsPropertiesObject *self;
  self = GI_MARSHALLING_TESTS_PROPERTIES_OBJECT (obj);

  if (self->some_gvalue) {
    g_boxed_free (G_TYPE_VALUE, self->some_gvalue);
    self->some_gvalue = NULL;
  }

  g_clear_pointer (&self->some_string, g_free);
  g_clear_pointer (&self->some_strv, g_strfreev);
  g_clear_pointer (&self->some_boxed_struct, gi_marshalling_tests_boxed_struct_free);
  g_clear_pointer (&self->some_byte_array, g_byte_array_unref);
  g_clear_pointer (&self->some_variant, g_variant_unref);
  g_clear_pointer (&self->some_boxed_glist, g_list_free);
  g_clear_object (&self->some_object);

  G_OBJECT_CLASS (gi_marshalling_tests_properties_object_parent_class)->finalize (obj);
}

static void
gi_marshalling_tests_properties_object_get_property (GObject *object,
                                                     guint property_id, GValue *value, GParamSpec *pspec)
{
  GIMarshallingTestsPropertiesObject *self;
  self = GI_MARSHALLING_TESTS_PROPERTIES_OBJECT (object);
  switch (property_id)
    {
    case SOME_BOOLEAN_PROPERTY:
      g_value_set_boolean (value, self->some_boolean);
      break;
    case SOME_CHAR_PROPERTY:
      g_value_set_schar (value, self->some_char);
      break;
    case SOME_UCHAR_PROPERTY:
      g_value_set_uchar (value, self->some_uchar);
      break;
    case SOME_INT_PROPERTY:
      g_value_set_int (value, self->some_int);
      break;
    case SOME_UINT_PROPERTY:
      g_value_set_uint (value, self->some_uint);
      break;
    case SOME_LONG_PROPERTY:
      g_value_set_long (value, self->some_long);
      break;
    case SOME_ULONG_PROPERTY:
      g_value_set_ulong (value, self->some_ulong);
      break;
    case SOME_INT64_PROPERTY:
      g_value_set_int64 (value, self->some_int64);
      break;
    case SOME_UINT64_PROPERTY:
      g_value_set_uint64 (value, self->some_uint64);
      break;
    case SOME_FLOAT_PROPERTY:
      g_value_set_float (value, self->some_float);
      break;
    case SOME_DOUBLE_PROPERTY:
      g_value_set_double (value, self->some_double);
      break;
    case SOME_STRING_PROPERTY:
      g_value_set_string (value, self->some_string);
      break;
    case SOME_STRV_PROPERTY:
      g_value_set_boxed (value, self->some_strv);
      break;
    case SOME_BOXED_STRUCT_PROPERTY:
      g_value_set_boxed (value, self->some_boxed_struct);
      break;
    case SOME_BOXED_GLIST_PROPERTY:
      g_value_set_boxed (value, self->some_boxed_glist);
      break;
    case SOME_GVALUE_PROPERTY:
      g_value_set_boxed (value, self->some_gvalue);
      break;
    case SOME_VARIANT_PROPERTY:
      g_value_set_variant (value, self->some_variant);
      break;
    case SOME_OBJECT_PROPERTY:
      g_value_set_object (value, self->some_object);
      break;
    case SOME_FLAGS_PROPERTY:
      g_value_set_flags (value, self->some_flags);
      break;
    case SOME_ENUM_PROPERTY:
      g_value_set_enum (value, self->some_enum);
      break;
    case SOME_BYTE_ARRAY_PROPERTY:
      g_value_set_boxed (value, self->some_byte_array);
      break;
    case SOME_READONLY_PROPERTY:
      g_value_set_int (value, 42);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gi_marshalling_tests_properties_object_set_property (GObject *object,
                                                     guint property_id, const GValue *value, GParamSpec *pspec)
{
  GIMarshallingTestsPropertiesObject *self;
  self = GI_MARSHALLING_TESTS_PROPERTIES_OBJECT (object);
  switch (property_id)
    {
    case SOME_BOOLEAN_PROPERTY:
      self->some_boolean = g_value_get_boolean (value);
      break;
    case SOME_CHAR_PROPERTY:
      self->some_char = g_value_get_schar (value);
      break;
    case SOME_UCHAR_PROPERTY:
      self->some_uchar = g_value_get_uchar (value);
      break;
    case SOME_INT_PROPERTY:
      self->some_int = g_value_get_int (value);
      break;
    case SOME_UINT_PROPERTY:
      self->some_uint = g_value_get_uint (value);
      break;
    case SOME_LONG_PROPERTY:
      self->some_long = g_value_get_long (value);
      break;
    case SOME_ULONG_PROPERTY:
      self->some_ulong = g_value_get_ulong (value);
      break;
    case SOME_INT64_PROPERTY:
      self->some_int64 = g_value_get_int64 (value);
      break;
    case SOME_UINT64_PROPERTY:
      self->some_uint64 = g_value_get_uint64 (value);
      break;
    case SOME_FLOAT_PROPERTY:
      self->some_float = g_value_get_float (value);
      break;
    case SOME_DOUBLE_PROPERTY:
      self->some_double = g_value_get_double (value);
      break;
    case SOME_STRING_PROPERTY:
      g_clear_pointer (&self->some_string, g_free);
      self->some_string = g_value_dup_string (value);
      break;
    case SOME_STRV_PROPERTY:
      g_strfreev (self->some_strv);
      self->some_strv = g_strdupv (g_value_get_boxed (value));
      break;
    case SOME_BOXED_STRUCT_PROPERTY:
      gi_marshalling_tests_boxed_struct_free (self->some_boxed_struct);
      self->some_boxed_struct = gi_marshalling_tests_boxed_struct_copy (g_value_get_boxed (value));
      break;
    case SOME_BOXED_GLIST_PROPERTY:
      g_list_free (self->some_boxed_glist);
      self->some_boxed_glist = g_list_copy (g_value_get_boxed (value));
      break;
    case SOME_GVALUE_PROPERTY:
      if (self->some_gvalue)
        g_boxed_free (G_TYPE_VALUE, self->some_gvalue);
      self->some_gvalue = g_value_dup_boxed (value);
      break;
    case SOME_VARIANT_PROPERTY:
      if (self->some_variant != NULL)
        g_variant_unref (self->some_variant);
      self->some_variant = g_value_get_variant (value);
      if (self->some_variant != NULL)
        g_variant_ref (self->some_variant);
      break;
    case SOME_OBJECT_PROPERTY:
      if (self->some_object != NULL)
        g_object_unref (self->some_object);
      self->some_object = g_value_dup_object (value);
      break;
    case SOME_FLAGS_PROPERTY:
      self->some_flags = g_value_get_flags (value);
      break;
    case SOME_ENUM_PROPERTY:
      self->some_enum = g_value_get_enum (value);
      break;
    case SOME_BYTE_ARRAY_PROPERTY:
      if (self->some_byte_array != NULL)
        g_byte_array_unref (self->some_byte_array);
      self->some_byte_array = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void gi_marshalling_tests_properties_object_class_init (GIMarshallingTestsPropertiesObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gi_marshalling_tests_properties_object_finalize;
  object_class->get_property = gi_marshalling_tests_properties_object_get_property;
  object_class->set_property = gi_marshalling_tests_properties_object_set_property;

  g_object_class_install_property (object_class, SOME_BOOLEAN_PROPERTY,
                                   g_param_spec_boolean ("some-boolean",
                                                         "some-boolean",
                                                         "some-boolean",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_CHAR_PROPERTY,
                                   g_param_spec_char ("some-char",
                                                      "some-char",
                                                      "some-char", G_MININT8,
                                                      G_MAXINT8, 0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_UCHAR_PROPERTY,
                                   g_param_spec_uchar ("some-uchar",
                                                       "some-uchar",
                                                       "some-uchar", 0,
                                                       G_MAXUINT8, 0,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_INT_PROPERTY,
                                   g_param_spec_int ("some-int", "some-int",
                                                     "some-int", G_MININT,
                                                     G_MAXINT, 0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_UINT_PROPERTY,
                                   g_param_spec_uint ("some-uint",
                                                      "some-uint",
                                                      "some-uint", 0,
                                                      G_MAXUINT, 0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_LONG_PROPERTY,
                                   g_param_spec_long ("some-long",
                                                      "some-long",
                                                      "some-long", G_MINLONG,
                                                      G_MAXLONG, 0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_ULONG_PROPERTY,
                                   g_param_spec_ulong ("some-ulong",
                                                       "some-ulong",
                                                       "some-ulong", 0,
                                                       G_MAXULONG, 0,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_INT64_PROPERTY,
                                   g_param_spec_int64 ("some-int64",
                                                       "some-int64",
                                                       "some-int64",
                                                       G_MININT64, G_MAXINT64,
                                                       0, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_UINT64_PROPERTY,
                                   g_param_spec_uint64 ("some-uint64",
                                                        "some-uint64",
                                                        "some-uint64", 0,
                                                        G_MAXUINT64, 0,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_FLOAT_PROPERTY,
                                   g_param_spec_float ("some-float",
                                                       "some-float",
                                                       "some-float",
                                                       -1 * G_MAXFLOAT,
                                                       G_MAXFLOAT, 0,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_DOUBLE_PROPERTY,
                                   g_param_spec_double ("some-double",
                                                        "some-double",
                                                        "some-double",
                                                        -1 * G_MAXDOUBLE,
                                                        G_MAXDOUBLE, 0,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_STRING_PROPERTY,
                                   g_param_spec_string ("some-string",
                                                        "some-string",
                                                        "some-string",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_STRV_PROPERTY,
                                   g_param_spec_boxed ("some-strv",
                                                       "some-strv",
                                                       "some-strv",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_BOXED_STRUCT_PROPERTY,
                                   g_param_spec_boxed ("some-boxed-struct",
                                                       "some-boxed-struct",
                                                       "some-boxed-struct",
                                                       gi_marshalling_tests_boxed_struct_get_type
                                                       (), G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    /**
     * GIMarshallingTestsPropertiesObject:some-boxed-glist: (type GLib.List(gint)) (transfer none):
     */
  g_object_class_install_property (object_class, SOME_BOXED_GLIST_PROPERTY,
                                   g_param_spec_boxed ("some-boxed-glist",
                                                       "some-boxed-glist",
                                                       "some-boxed-glist",
                                                       gi_marshalling_tests_boxed_glist_get_type
                                                       (), G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_GVALUE_PROPERTY,
                                   g_param_spec_boxed ("some-gvalue",
                                                       "some-gvalue",
                                                       "some-gvalue",
                                                       G_TYPE_VALUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_VARIANT_PROPERTY,
                                   g_param_spec_variant ("some-variant",
                                                         "some-variant",
                                                         "some-variant",
                                                         G_VARIANT_TYPE_ANY,
                                                         NULL,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_OBJECT_PROPERTY,
                                   g_param_spec_object ("some-object",
                                                        "some-object",
                                                        "some-object",
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_FLAGS_PROPERTY,
                                   g_param_spec_flags ("some-flags",
                                                       "some-flags",
                                                       "some-flags",
                                                       GI_MARSHALLING_TESTS_TYPE_FLAGS,
                                                       GI_MARSHALLING_TESTS_FLAGS_VALUE1,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_ENUM_PROPERTY,
                                   g_param_spec_enum ("some-enum",
                                                      "some-enum",
                                                      "some-enum",
                                                      GI_MARSHALLING_TESTS_TYPE_GENUM,
                                                      GI_MARSHALLING_TESTS_GENUM_VALUE1,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_BYTE_ARRAY_PROPERTY,
                                   g_param_spec_boxed ("some-byte-array",
                                                       "some-byte-array",
                                                       "some-byte-array",
                                                       G_TYPE_BYTE_ARRAY,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, SOME_READONLY_PROPERTY,
                                   g_param_spec_int ("some-readonly",
                                                     "some-readonly",
                                                     "some-readonly",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READABLE));
}

GIMarshallingTestsPropertiesObject *
gi_marshalling_tests_properties_object_new (void)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT, NULL);
}

G_DEFINE_TYPE (GIMarshallingTestsSignalsObject, gi_marshalling_tests_signals_object, G_TYPE_OBJECT);

static void
gi_marshalling_tests_signals_object_init (GIMarshallingTestsSignalsObject *object G_GNUC_UNUSED)
{
}

static void
gi_marshalling_tests_signals_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (gi_marshalling_tests_signals_object_parent_class)->finalize (object);
}

static void
gi_marshalling_tests_signals_object_class_init (GIMarshallingTestsSignalsObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gi_marshalling_tests_signals_object_finalize;

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-utf8:
   * @self:
   * @arg: (element-type utf8):
   */
  g_signal_new ("some-boxed-gptrarray-utf8",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-utf8-container:
   * @self:
   * @arg: (element-type utf8) (transfer container):
   */
  g_signal_new ("some-boxed-gptrarray-utf8-container",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-utf8-full:
   * @self:
   * @arg: (element-type utf8) (transfer full):
   */
  g_signal_new ("some-boxed-gptrarray-utf8-full",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-boxed-struct:
   * @self:
   * @arg: (element-type GIMarshallingTestsBoxedStruct):
   */
  g_signal_new ("some-boxed-gptrarray-boxed-struct",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-boxed-struct-container:
   * @self:
   * @arg: (element-type GIMarshallingTestsBoxedStruct) (transfer container):
   */
  g_signal_new ("some-boxed-gptrarray-boxed-struct-container",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);
  /**
   * GIMarshallingTestsSignalsObject::some-boxed-gptrarray-boxed-struct-full:
   * @self:
   * @arg: (element-type GIMarshallingTestsBoxedStruct) (transfer full):
   */
  g_signal_new ("some-boxed-gptrarray-boxed-struct-full",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_PTR_ARRAY);

  /**
   * GIMarshallingTestsSignalsObject::some-hash-table-utf8-int:
   * @self:
   * @arg: (type GHashTable) (element-type utf8 int):
   */
  g_signal_new ("some-hash-table-utf8-int",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_HASH_TABLE);

  /**
   * GIMarshallingTestsSignalsObject::some-hash-table-utf8-int-container:
   * @self:
   * @arg: (type GHashTable) (element-type utf8 int) (transfer container):
   */
  g_signal_new ("some-hash-table-utf8-int-container",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_HASH_TABLE);

  /**
   * GIMarshallingTestsSignalsObject::some-hash-table-utf8-int-full:
   * @self:
   * @arg: (type GHashTable) (element-type utf8 int) (transfer full):
   */
  g_signal_new ("some-hash-table-utf8-int-full",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_HASH_TABLE);

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-struct:
   * @self:
   * @arg:
   */
  g_signal_new ("some-boxed-struct",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                gi_marshalling_tests_boxed_struct_get_type ());

  /**
   * GIMarshallingTestsSignalsObject::some-boxed-struct-full:
   * @self:
   * @arg: (transfer full):
   */
  g_signal_new ("some-boxed-struct-full",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                gi_marshalling_tests_boxed_struct_get_type ());
}

GIMarshallingTestsSignalsObject *
gi_marshalling_tests_signals_object_new (void)
{
  return g_object_new (GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT, NULL);
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8 (GIMarshallingTestsSignalsObject *object)
{
  GPtrArray *ptrarray;

  ptrarray = gi_marshalling_tests_gptrarray_utf8_container_return ();
  g_signal_emit_by_name (object, "some-boxed-gptrarray-utf8",
                         ptrarray);
  g_ptr_array_unref (ptrarray);
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8_container (GIMarshallingTestsSignalsObject *object)
{
  g_signal_emit_by_name (object, "some-boxed-gptrarray-utf8-container",
                         gi_marshalling_tests_gptrarray_utf8_container_return ());
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8_full (GIMarshallingTestsSignalsObject *object)
{
  g_signal_emit_by_name (object, "some-boxed-gptrarray-utf8-full",
                         gi_marshalling_tests_gptrarray_utf8_full_return ());
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct (GIMarshallingTestsSignalsObject *object)
{
  GPtrArray *ptrarray;

  ptrarray = gi_marshalling_tests_gptrarray_boxed_struct_full_return ();
  g_signal_emit_by_name (object, "some-boxed-gptrarray-boxed-struct",
                         ptrarray);
  g_ptr_array_set_free_func (ptrarray, (GDestroyNotify) gi_marshalling_tests_boxed_struct_free);
  g_ptr_array_unref (ptrarray);
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct_container (GIMarshallingTestsSignalsObject *object)
{
  GPtrArray *ptrarray;

  ptrarray = gi_marshalling_tests_gptrarray_boxed_struct_full_return ();
  g_ptr_array_set_free_func (ptrarray, (GDestroyNotify) gi_marshalling_tests_boxed_struct_free);
  g_signal_emit_by_name (object, "some-boxed-gptrarray-boxed-struct-container",
                         g_steal_pointer (&ptrarray));
}

void
gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct_full (GIMarshallingTestsSignalsObject *object)
{
  g_signal_emit_by_name (object, "some-boxed-gptrarray-boxed-struct-full",
                         gi_marshalling_tests_gptrarray_boxed_struct_full_return ());
}

void
gi_marshalling_tests_signals_object_emit_hash_table_utf8_int (GIMarshallingTestsSignalsObject *object)
{
  GHashTable *hash_table;

  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_hash_table_insert (hash_table, g_strdup ("-1"), GINT_TO_POINTER (1));
  g_hash_table_insert (hash_table, g_strdup ("0"), GINT_TO_POINTER (0));
  g_hash_table_insert (hash_table, g_strdup ("1"), GINT_TO_POINTER (-1));
  g_hash_table_insert (hash_table, g_strdup ("2"), GINT_TO_POINTER (-2));

  g_signal_emit_by_name (object, "some-hash-table-utf8-int", hash_table);
  g_hash_table_unref (hash_table);
}

void
gi_marshalling_tests_signals_object_emit_hash_table_utf8_int_container (GIMarshallingTestsSignalsObject *object)
{
  GHashTable *hash_table;

  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_hash_table_insert (hash_table, g_strdup ("-1"), GINT_TO_POINTER (1));
  g_hash_table_insert (hash_table, g_strdup ("0"), GINT_TO_POINTER (0));
  g_hash_table_insert (hash_table, g_strdup ("1"), GINT_TO_POINTER (-1));
  g_hash_table_insert (hash_table, g_strdup ("2"), GINT_TO_POINTER (-2));

  g_signal_emit_by_name (object, "some-hash-table-utf8-int-container",
                         g_steal_pointer (&hash_table));
}

void
gi_marshalling_tests_signals_object_emit_hash_table_utf8_int_full (GIMarshallingTestsSignalsObject *object)
{
  GHashTable *hash_table;

  hash_table = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (hash_table, g_strdup ("-1"), GINT_TO_POINTER (1));
  g_hash_table_insert (hash_table, g_strdup ("0"), GINT_TO_POINTER (0));
  g_hash_table_insert (hash_table, g_strdup ("1"), GINT_TO_POINTER (-1));
  g_hash_table_insert (hash_table, g_strdup ("2"), GINT_TO_POINTER (-2));

  g_signal_emit_by_name (object, "some-hash-table-utf8-int-full",
                         g_steal_pointer (&hash_table));
}

void
gi_marshalling_tests_signals_object_emit_boxed_struct (GIMarshallingTestsSignalsObject *object)
{
  GIMarshallingTestsBoxedStruct *boxed = gi_marshalling_tests_boxed_struct_new ();
  boxed->long_ = 99;
  boxed->string_ = g_strdup ("a string");
  boxed->g_strv = g_strdupv ((GStrv) (const char*[]) {"foo", "bar", "baz", NULL });

  g_signal_emit_by_name (object, "some-boxed-struct", boxed);
  g_clear_pointer (&boxed, gi_marshalling_tests_boxed_struct_free);
}

void
gi_marshalling_tests_signals_object_emit_boxed_struct_full (GIMarshallingTestsSignalsObject *object)
{
  GIMarshallingTestsBoxedStruct *boxed = gi_marshalling_tests_boxed_struct_new ();

  boxed->long_ = 99;
  boxed->string_ = g_strdup ("a string");
  boxed->g_strv = g_strdupv ((GStrv) (const char*[]) {"foo", "bar", "baz", NULL });
  g_signal_emit_by_name (object, "some-boxed-struct-full", g_steal_pointer (&boxed));
}
