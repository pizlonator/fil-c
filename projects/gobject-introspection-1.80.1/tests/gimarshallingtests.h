/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * vim: tabstop=4 shiftwidth=4 expandtab
 */

#include <sys/types.h>  /* off_t */
#include <time.h>       /* time_t */

#include <glib-object.h>

#include "gitestmacros.h"

#ifndef __GI_MARSHALLING_TESTS_H__
#define __GI_MARSHALLING_TESTS_H__

typedef struct _GIMarshallingTestsSimpleStruct GIMarshallingTestsSimpleStruct;
typedef struct _GIMarshallingTestsBoxedStruct GIMarshallingTestsBoxedStruct;

/* Constants */

#define GI_MARSHALLING_TESTS_CONSTANT_NUMBER 42
#define GI_MARSHALLING_TESTS_CONSTANT_UTF8   "const \xe2\x99\xa5 utf8"
#define GI_MARSHALLING_TESTS_CONSTANT_UCS4   { 0x63, 0x6f, 0x6e, 0x73, 0x74, \
                                               0x20, 0x2665, 0x20, 0x75, 0x74, \
                                               0x66, 0x38 }

/* Booleans */

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_boolean_return_true (void);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_boolean_return_false (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_in_true (gboolean v);

_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_in_false (gboolean v);


_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_out_true (gboolean *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_out_false (gboolean *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_boolean_out_uninitialized (gboolean *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_inout_true_false (gboolean *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_boolean_inout_false_true (gboolean *v);


/* Integers */

_GI_TEST_EXTERN
gint8 gi_marshalling_tests_int8_return_max (void);

_GI_TEST_EXTERN
gint8 gi_marshalling_tests_int8_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_int8_in_max (gint8 v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int8_in_min (gint8 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_int8_out_max (gint8 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int8_out_min (gint8 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_int8_out_uninitialized (gint8 *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_int8_inout_max_min (gint8 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int8_inout_min_max (gint8 *v);



_GI_TEST_EXTERN
guint8 gi_marshalling_tests_uint8_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint8_in (guint8 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint8_out (guint8 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_uint8_out_uninitialized (guint8 *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_uint8_inout (guint8 *v);


_GI_TEST_EXTERN
gint16 gi_marshalling_tests_int16_return_max (void);

_GI_TEST_EXTERN
gint16 gi_marshalling_tests_int16_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_int16_in_max (gint16 v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int16_in_min (gint16 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_int16_out_max (gint16 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int16_out_min (gint16 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_int16_out_uninitialized (gint16 *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_int16_inout_max_min (gint16 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int16_inout_min_max (gint16 *v);



_GI_TEST_EXTERN
guint16 gi_marshalling_tests_uint16_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint16_in (guint16 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint16_out (guint16 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_uint16_out_uninitialized (guint16 *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_uint16_inout (guint16 *v);



_GI_TEST_EXTERN
gint32 gi_marshalling_tests_int32_return_max (void);

_GI_TEST_EXTERN
gint32 gi_marshalling_tests_int32_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_int32_in_max (gint32 v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int32_in_min (gint32 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_int32_out_max (gint32 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int32_out_min (gint32 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_int32_out_uninitialized (gint32 *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_int32_inout_max_min (gint32 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int32_inout_min_max (gint32 *v);



_GI_TEST_EXTERN
guint32 gi_marshalling_tests_uint32_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint32_in (guint32 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint32_out (guint32 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_uint32_out_uninitialized (guint32 *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_uint32_inout (guint32 *v);



_GI_TEST_EXTERN
gint64 gi_marshalling_tests_int64_return_max (void);

_GI_TEST_EXTERN
gint64 gi_marshalling_tests_int64_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_int64_in_max (gint64 v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int64_in_min (gint64 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_int64_out_max (gint64 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int64_out_min (gint64 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_int64_out_uninitialized (gint64 *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_int64_inout_max_min (gint64 *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_int64_inout_min_max (gint64 *v);



_GI_TEST_EXTERN
guint64 gi_marshalling_tests_uint64_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint64_in (guint64 v);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint64_out (guint64 *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_uint64_out_uninitialized (guint64 *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_uint64_inout (guint64 *v);



_GI_TEST_EXTERN
gshort gi_marshalling_tests_short_return_max (void);

_GI_TEST_EXTERN
gshort gi_marshalling_tests_short_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_short_in_max (gshort short_);

_GI_TEST_EXTERN
void gi_marshalling_tests_short_in_min (gshort short_);


_GI_TEST_EXTERN
void gi_marshalling_tests_short_out_max (gshort *short_);

_GI_TEST_EXTERN
void gi_marshalling_tests_short_out_min (gshort *short_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_short_out_uninitialized (gshort *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_short_inout_max_min (gshort *short_);

_GI_TEST_EXTERN
void gi_marshalling_tests_short_inout_min_max (gshort *short_);



_GI_TEST_EXTERN
gushort gi_marshalling_tests_ushort_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_ushort_in (gushort ushort_);


_GI_TEST_EXTERN
void gi_marshalling_tests_ushort_out (gushort *ushort_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ushort_out_uninitialized (gushort *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_ushort_inout (gushort *ushort_);



_GI_TEST_EXTERN
gint gi_marshalling_tests_int_return_max (void);

_GI_TEST_EXTERN
gint gi_marshalling_tests_int_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_int_in_max (gint int_);

_GI_TEST_EXTERN
void gi_marshalling_tests_int_in_min (gint int_);


_GI_TEST_EXTERN
void gi_marshalling_tests_int_out_max (gint *int_);

_GI_TEST_EXTERN
void gi_marshalling_tests_int_out_min (gint *int_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_int_out_uninitialized (gint *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_int_inout_max_min (gint *int_);

_GI_TEST_EXTERN
void gi_marshalling_tests_int_inout_min_max (gint *int_);


_GI_TEST_EXTERN
guint gi_marshalling_tests_uint_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint_in (guint uint_);


_GI_TEST_EXTERN
void gi_marshalling_tests_uint_out (guint *uint_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_uint_out_uninitialized (guint *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_uint_inout (guint *uint_);


_GI_TEST_EXTERN
glong gi_marshalling_tests_long_return_max (void);

_GI_TEST_EXTERN
glong gi_marshalling_tests_long_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_long_in_max (glong long_);

_GI_TEST_EXTERN
void gi_marshalling_tests_long_in_min (glong long_);


_GI_TEST_EXTERN
void gi_marshalling_tests_long_out_max (glong *long_);

_GI_TEST_EXTERN
void gi_marshalling_tests_long_out_min (glong *long_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_long_out_uninitialized (glong *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_long_inout_max_min (glong *long_);

_GI_TEST_EXTERN
void gi_marshalling_tests_long_inout_min_max (glong *long_);


_GI_TEST_EXTERN
gulong gi_marshalling_tests_ulong_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_ulong_in (gulong ulong_);


_GI_TEST_EXTERN
void gi_marshalling_tests_ulong_out (gulong *ulong_);

_GI_TEST_EXTERN
void gi_marshalling_tests_ulong_inout (gulong *ulong_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ulong_out_uninitialized (gulong *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
gssize gi_marshalling_tests_ssize_return_max (void);

_GI_TEST_EXTERN
gssize gi_marshalling_tests_ssize_return_min (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_in_max (gssize ssize);

_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_in_min (gssize ssize);


_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_out_max (gssize *ssize);

_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_out_min (gssize *ssize);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ssize_out_uninitialized (gssize *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_inout_max_min (gssize *ssize);

_GI_TEST_EXTERN
void gi_marshalling_tests_ssize_inout_min_max (gssize *ssize);



_GI_TEST_EXTERN
gsize gi_marshalling_tests_size_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_size_in (gsize size);


_GI_TEST_EXTERN
void gi_marshalling_tests_size_out (gsize *size);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_size_out_uninitialized (gsize *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_size_inout (gsize *size);


/* Floating-point */

_GI_TEST_EXTERN
gfloat gi_marshalling_tests_float_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_float_in (gfloat v);


_GI_TEST_EXTERN
void gi_marshalling_tests_float_out (gfloat *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_float_noncanonical_nan_out (gfloat *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_float_out_uninitialized (gfloat *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_float_inout (gfloat *v);



_GI_TEST_EXTERN
gdouble gi_marshalling_tests_double_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_double_in (gdouble v);


_GI_TEST_EXTERN
void gi_marshalling_tests_double_out (gdouble *v);

_GI_TEST_EXTERN
void gi_marshalling_tests_double_noncanonical_nan_out (gdouble *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_double_out_uninitialized (gdouble *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_double_inout (gdouble *v);


/* Timestamps */

_GI_TEST_EXTERN
time_t gi_marshalling_tests_time_t_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_time_t_in (time_t v);


_GI_TEST_EXTERN
void gi_marshalling_tests_time_t_out (time_t *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_time_t_out_uninitialized (time_t *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_time_t_inout (time_t *v);


/* Platform file offset (avoid this, use goffset instead) */

_GI_TEST_EXTERN
off_t gi_marshalling_tests_off_t_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_off_t_in (off_t v);


_GI_TEST_EXTERN
void gi_marshalling_tests_off_t_out (off_t *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_off_t_out_uninitialized (off_t *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_off_t_inout (off_t *v);


/* GType */

_GI_TEST_EXTERN
GType gi_marshalling_tests_gtype_return (void);


_GI_TEST_EXTERN
GType gi_marshalling_tests_gtype_string_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_gtype_in (GType gtype);


_GI_TEST_EXTERN
void gi_marshalling_tests_gtype_string_in (GType gtype);


_GI_TEST_EXTERN
void gi_marshalling_tests_gtype_out (GType *gtype);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gtype_out_uninitialized (GType *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_gtype_string_out (GType *gtype);


_GI_TEST_EXTERN
void gi_marshalling_tests_gtype_inout (GType *gtype);


/* UTF-8 */

_GI_TEST_EXTERN
const gchar *gi_marshalling_tests_utf8_none_return (void);

_GI_TEST_EXTERN
gchar *gi_marshalling_tests_utf8_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_none_in (const gchar *utf8);

_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_full_in (gchar *utf8);


_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_none_out (const gchar **utf8);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_utf8_none_out_uninitialized (const gchar **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_full_out (gchar **utf8);


_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_dangling_out (gchar **utf8);


_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_none_inout (const gchar **utf8);

_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_full_inout (gchar **utf8);


_GI_TEST_EXTERN
GSList *gi_marshalling_tests_filename_list_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_utf8_as_uint8array_in (const guint8 *array,
                                                 gsize         len);


/* Enum */

typedef enum
{
  GI_MARSHALLING_TESTS_ENUM_VALUE1,
  GI_MARSHALLING_TESTS_ENUM_VALUE2,
  GI_MARSHALLING_TESTS_ENUM_VALUE3 = 42
} GIMarshallingTestsEnum;

typedef enum
{
  GI_MARSHALLING_TESTS_SECOND_ENUM_SECONDVALUE1,
  GI_MARSHALLING_TESTS_SECOND_ENUM_SECONDVALUE2,
} GIMarshallingTestsSecondEnum;


_GI_TEST_EXTERN
GIMarshallingTestsEnum gi_marshalling_tests_enum_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_enum_in (GIMarshallingTestsEnum v);


_GI_TEST_EXTERN
void gi_marshalling_tests_enum_out (GIMarshallingTestsEnum *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_enum_out_uninitialized (GIMarshallingTestsEnum **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_enum_inout (GIMarshallingTestsEnum *v);


/* GEnum */

typedef enum
{
  GI_MARSHALLING_TESTS_GENUM_VALUE1,
  GI_MARSHALLING_TESTS_GENUM_VALUE2,
  GI_MARSHALLING_TESTS_GENUM_VALUE3 = 42
} GIMarshallingTestsGEnum;

_GI_TEST_EXTERN
GType gi_marshalling_tests_genum_get_type (void) G_GNUC_CONST;
#define GI_MARSHALLING_TESTS_TYPE_GENUM (gi_marshalling_tests_genum_get_type ())


_GI_TEST_EXTERN
GIMarshallingTestsGEnum gi_marshalling_tests_genum_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_genum_in (GIMarshallingTestsGEnum v);


_GI_TEST_EXTERN
void gi_marshalling_tests_genum_out (GIMarshallingTestsGEnum *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_genum_out_uninitialized (GIMarshallingTestsGEnum *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_genum_inout (GIMarshallingTestsGEnum *v);


/* GFlags */

typedef enum
{
  GI_MARSHALLING_TESTS_FLAGS_VALUE1 = 1 << 0,
  GI_MARSHALLING_TESTS_FLAGS_VALUE2 = 1 << 1,
  GI_MARSHALLING_TESTS_FLAGS_VALUE3 = 1 << 2,
  GI_MARSHALLING_TESTS_FLAGS_MASK = GI_MARSHALLING_TESTS_FLAGS_VALUE1 |
                                    GI_MARSHALLING_TESTS_FLAGS_VALUE2,
  GI_MARSHALLING_TESTS_FLAGS_MASK2 = GI_MARSHALLING_TESTS_FLAGS_MASK
} GIMarshallingTestsFlags;

_GI_TEST_EXTERN
GType gi_marshalling_tests_flags_get_type (void) G_GNUC_CONST;
#define GI_MARSHALLING_TESTS_TYPE_FLAGS (gi_marshalling_tests_flags_get_type ())


_GI_TEST_EXTERN
GIMarshallingTestsFlags gi_marshalling_tests_flags_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_flags_in (GIMarshallingTestsFlags v);

_GI_TEST_EXTERN
void gi_marshalling_tests_flags_in_zero (GIMarshallingTestsFlags v);


_GI_TEST_EXTERN
void gi_marshalling_tests_flags_out (GIMarshallingTestsFlags *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_flags_out_uninitialized (GIMarshallingTestsFlags *v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_flags_inout (GIMarshallingTestsFlags *v);

/* Flags with no GType */

typedef enum
{
  GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE1 = 1 << 0,
  GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2 = 1 << 1,
  GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE3 = 1 << 2,
  GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_MASK = GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE1 |
                                            GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_VALUE2,
  GI_MARSHALLING_TESTS_NO_TYPE_FLAGS_MASK2 = GI_MARSHALLING_TESTS_FLAGS_MASK
} GIMarshallingTestsNoTypeFlags;


_GI_TEST_EXTERN
GIMarshallingTestsNoTypeFlags gi_marshalling_tests_no_type_flags_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_no_type_flags_in (GIMarshallingTestsNoTypeFlags v);

_GI_TEST_EXTERN
void gi_marshalling_tests_no_type_flags_in_zero (GIMarshallingTestsNoTypeFlags v);


_GI_TEST_EXTERN
void gi_marshalling_tests_no_type_flags_out (GIMarshallingTestsNoTypeFlags *v);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_no_type_flags_out_uninitialized (GIMarshallingTestsNoTypeFlags **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_no_type_flags_inout (GIMarshallingTestsNoTypeFlags *v);

/* Arrays */


_GI_TEST_EXTERN
gboolean gi_marshalling_tests_init_function (gint *n_args, char ***argv);

/* Fixed-size */

_GI_TEST_EXTERN
const gint *gi_marshalling_tests_array_fixed_int_return (void);

_GI_TEST_EXTERN
const gshort *gi_marshalling_tests_array_fixed_short_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_int_in (const gint *ints);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_caller_allocated_out (gint *ints);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_short_in (const gshort *shorts);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_out (gint **ints);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_array_fixed_out_uninitialized (gint **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_out_struct (GIMarshallingTestsSimpleStruct **structs);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_array_fixed_out_struct_uninitialized (GIMarshallingTestsSimpleStruct **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_caller_allocated_struct_out (GIMarshallingTestsSimpleStruct *structs);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_fixed_inout (gint **ints);

/* Variable-size */


_GI_TEST_EXTERN
const gint *gi_marshalling_tests_array_return (gint *length);

_GI_TEST_EXTERN
const gint *gi_marshalling_tests_array_return_etc (gint first, gint *length, gint last, gint *sum);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_in (const gint *ints, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_len_before (gint length, const gint *ints);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_len_zero_terminated (const gint *ints, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_string_in (const gchar **strings, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_uint8_in (const guint8 *chars, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_int64_in (const gint64 *ints, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_uint64_in (const guint64 *ints, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_unichar_in (const gunichar *chars, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_bool_in (const gboolean *bools, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_struct_in (GIMarshallingTestsBoxedStruct **structs, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_struct_value_in (GIMarshallingTestsBoxedStruct *structs, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_struct_take_in (GIMarshallingTestsBoxedStruct **structs, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_simple_struct_in (GIMarshallingTestsSimpleStruct *structs, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_multi_array_key_value_in (gint length, const gchar **keys, const GValue *values);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_enum_in (GIMarshallingTestsEnum *_enum, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_flags_in (GIMarshallingTestsFlags *flags, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_guint64_len (const gint *ints, guint64 length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_guint8_len (const gint *ints, guint8 length);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_out (gint **ints, gint *length);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_array_out_uninitialized (gint **v G_GNUC_UNUSED, gint *length G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_out_etc (gint first, gint **ints, gint *length, gint last, gint *sum);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_bool_out (const gboolean **bools, gint *length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_unichar_out (const gunichar **chars, gint *length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_inout (gint **ints, gint *length);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_inout_etc (gint first, gint **ints, gint *length, gint last, gint *sum);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_nonzero_nonlen (gint first, const guint8 *chars);

/* Zero-terminated */


_GI_TEST_EXTERN
const gchar **gi_marshalling_tests_array_zero_terminated_return (void);

_GI_TEST_EXTERN
gchar **gi_marshalling_tests_array_zero_terminated_return_null (void);

_GI_TEST_EXTERN
GIMarshallingTestsBoxedStruct **gi_marshalling_tests_array_zero_terminated_return_struct (void);

_GI_TEST_EXTERN
gunichar *gi_marshalling_tests_array_zero_terminated_return_unichar (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_zero_terminated_in (gchar **utf8s);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_zero_terminated_out (const gchar ***utf8s);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_array_zero_terminated_out_uninitialized (const gchar ***v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_array_zero_terminated_inout (const gchar ***utf8s);


_GI_TEST_EXTERN
GVariant **gi_marshalling_tests_array_gvariant_none_in (GVariant **variants);


_GI_TEST_EXTERN
GVariant **gi_marshalling_tests_array_gvariant_container_in (GVariant **variants);


_GI_TEST_EXTERN
GVariant **gi_marshalling_tests_array_gvariant_full_in (GVariant **variants);


/* GArray */

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_int_none_return (void);

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_uint64_none_return (void);

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_utf8_none_return (void);

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_utf8_container_return (void);

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_utf8_full_return (void);

_GI_TEST_EXTERN
GArray *gi_marshalling_tests_garray_boxed_struct_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_garray_int_none_in (GArray *array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_uint64_none_in (GArray *array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_none_in (GArray *array_);


_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_none_out (GArray **array_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_garray_utf8_none_out_uninitialized (GArray **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_container_out (GArray **array_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_garray_utf8_container_out_uninitialized (GArray **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_full_out (GArray **array_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_garray_utf8_full_out_uninitialized (GArray **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_full_out_caller_allocated (GArray *array_);


_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_none_inout (GArray **array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_container_inout (GArray **array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_utf8_full_inout (GArray **array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_bool_none_in (GArray *array_);

_GI_TEST_EXTERN
void gi_marshalling_tests_garray_unichar_none_in (GArray *array_);

/* GPtrArray */

_GI_TEST_EXTERN
GPtrArray *gi_marshalling_tests_gptrarray_utf8_none_return (void);

_GI_TEST_EXTERN
GPtrArray *gi_marshalling_tests_gptrarray_utf8_container_return (void);

_GI_TEST_EXTERN
GPtrArray *gi_marshalling_tests_gptrarray_utf8_full_return (void);

_GI_TEST_EXTERN
GPtrArray *gi_marshalling_tests_gptrarray_boxed_struct_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_none_in (GPtrArray *parray_);


_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_none_out (GPtrArray **parray_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gptrarray_utf8_none_out_uninitialized (GPtrArray **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_container_out (GPtrArray **parray_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gptrarray_utf8_container_out_uninitialized (GPtrArray **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_full_out (GPtrArray **parray_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gptrarray_utf8_full_out_uninitialized (GPtrArray **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_none_inout (GPtrArray **parray_);

_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_container_inout (GPtrArray **parray_);

_GI_TEST_EXTERN
void gi_marshalling_tests_gptrarray_utf8_full_inout (GPtrArray **parray_);

/* GByteArray */

_GI_TEST_EXTERN
GByteArray *gi_marshalling_tests_bytearray_full_return (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_bytearray_none_in (GByteArray* v);

/* GBytes */

_GI_TEST_EXTERN
GBytes *gi_marshalling_tests_gbytes_full_return (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_gbytes_none_in (GBytes* v);

/* GStrv */

_GI_TEST_EXTERN
GStrv gi_marshalling_tests_gstrv_return (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_gstrv_in (GStrv g_strv);

_GI_TEST_EXTERN
void gi_marshalling_tests_gstrv_out (GStrv *g_strv);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gstrv_out_uninitialized (GStrv *v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gstrv_inout (GStrv *g_strv);

/* GList */

_GI_TEST_EXTERN
GList *gi_marshalling_tests_glist_int_none_return (void);

_GI_TEST_EXTERN
GList *gi_marshalling_tests_glist_uint32_none_return (void);

_GI_TEST_EXTERN
GList *gi_marshalling_tests_glist_utf8_none_return (void);

_GI_TEST_EXTERN
GList *gi_marshalling_tests_glist_utf8_container_return (void);

_GI_TEST_EXTERN
GList *gi_marshalling_tests_glist_utf8_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_glist_int_none_in (GList *list);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_uint32_none_in (GList *list);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_none_in (GList *list);


_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_none_out (GList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_glist_utf8_none_out_uninitialized (GList **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_container_out (GList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_glist_utf8_container_out_uninitialized (GList **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_full_out (GList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_glist_utf8_full_out_uninitialized (GList **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_none_inout (GList **list);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_container_inout (GList **list);

_GI_TEST_EXTERN
void gi_marshalling_tests_glist_utf8_full_inout (GList **list);


/* GSList */

_GI_TEST_EXTERN
GSList *gi_marshalling_tests_gslist_int_none_return (void);

_GI_TEST_EXTERN
GSList *gi_marshalling_tests_gslist_utf8_none_return (void);

_GI_TEST_EXTERN
GSList *gi_marshalling_tests_gslist_utf8_container_return (void);

_GI_TEST_EXTERN
GSList *gi_marshalling_tests_gslist_utf8_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_int_none_in (GSList *list);

_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_none_in (GSList *list);


_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_none_out (GSList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gslist_utf8_none_out_uninitialized (GSList **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_container_out (GSList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gslist_utf8_container_out_uninitialized (GSList **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_full_out (GSList **list);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gslist_utf8_full_out_uninitialized (GSList **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_none_inout (GSList **list);

_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_container_inout (GSList **list);

_GI_TEST_EXTERN
void gi_marshalling_tests_gslist_utf8_full_inout (GSList **list);


/* GHashTable */

_GI_TEST_EXTERN
GHashTable *gi_marshalling_tests_ghashtable_int_none_return (void);

_GI_TEST_EXTERN
GHashTable *gi_marshalling_tests_ghashtable_utf8_none_return (void);

_GI_TEST_EXTERN
GHashTable *gi_marshalling_tests_ghashtable_utf8_container_return (void);

_GI_TEST_EXTERN
GHashTable *gi_marshalling_tests_ghashtable_utf8_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_int_none_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_none_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_double_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_float_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_int64_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_uint64_in (GHashTable *hash_table);


_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_container_in (GHashTable *hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_full_in (GHashTable *hash_table);


_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_none_out (GHashTable **hash_table);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ghashtable_utf8_none_out_uninitialized (GHashTable **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_container_out (GHashTable **hash_table);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ghashtable_utf8_container_out_uninitialized (GHashTable **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_full_out (GHashTable **hash_table);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_ghashtable_utf8_full_out_uninitialized (GHashTable **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_none_inout (GHashTable **hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_container_inout (GHashTable **hash_table);

_GI_TEST_EXTERN
void gi_marshalling_tests_ghashtable_utf8_full_inout (GHashTable **hash_table);


/* GValue */

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_return (void);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_noncanonical_nan_float (void);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_noncanonical_nan_double (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_in (GValue *value);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_int64_in (GValue *value);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_in_with_type (GValue *value, GType type);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_in_with_modification (GValue *value);


_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_in_enum (GValue *value);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_in_flags (GValue *value);


_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_out (GValue **value);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gvalue_out_uninitialized (GValue **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_int64_out (GValue **value);

_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_out_caller_allocates (GValue *value);


_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_inout (GValue **value);


_GI_TEST_EXTERN
void gi_marshalling_tests_gvalue_flat_array (guint         n_values,
                                             const GValue *values);


_GI_TEST_EXTERN
GValue *gi_marshalling_tests_return_gvalue_flat_array (void);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_return_gvalue_zero_terminated_array (void);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_round_trip (GValue *value);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_copy (GValue *value);

_GI_TEST_EXTERN
GValue *gi_marshalling_tests_gvalue_flat_array_round_trip (const GValue one,
                                                           const GValue two,
                                                           const GValue three);

/* GClosure */

_GI_TEST_EXTERN
void gi_marshalling_tests_gclosure_in (GClosure *closure);

_GI_TEST_EXTERN
GClosure *gi_marshalling_tests_gclosure_return (void);

/* Callback return values */

/**
 * GIMarshallingTestsCallbackReturnValueOnly:
 */
typedef glong (* GIMarshallingTestsCallbackReturnValueOnly) (void);


_GI_TEST_EXTERN
glong gi_marshalling_tests_callback_return_value_only (GIMarshallingTestsCallbackReturnValueOnly callback);

/**
 * GIMarshallingTestsCallbackOneOutParameter:
 * @a: (out):
 */
typedef void (* GIMarshallingTestsCallbackOneOutParameter) (gfloat *a);


_GI_TEST_EXTERN
void gi_marshalling_tests_callback_one_out_parameter (GIMarshallingTestsCallbackOneOutParameter  callback,
                                                      gfloat                                    *a);

/**
 * GIMarshallingTestsCallbackMultipleOutParameters:
 * @a: (out):
 * @b: (out):
 */
typedef void (* GIMarshallingTestsCallbackMultipleOutParameters) (gfloat *a, gfloat *b);


_GI_TEST_EXTERN
void gi_marshalling_tests_callback_multiple_out_parameters (GIMarshallingTestsCallbackMultipleOutParameters  callback,
                                                            gfloat                                          *a,
                                                            gfloat                                          *b);

/**
 * GIMarshallingTestsCallbackReturnValueAndOneOutParameter:
 * @a: (out):
 */
typedef glong (* GIMarshallingTestsCallbackReturnValueAndOneOutParameter) (glong *a);


_GI_TEST_EXTERN
glong gi_marshalling_tests_callback_return_value_and_one_out_parameter (GIMarshallingTestsCallbackReturnValueAndOneOutParameter  callback,
                                                                        glong                                                   *a);

/**
 * GIMarshallingTestsCallbackReturnValueAndMultipleOutParameters:
 * @a: (out):
 * @b: (out):
 */
typedef glong (* GIMarshallingTestsCallbackReturnValueAndMultipleOutParameters) (glong *a, glong *b);


_GI_TEST_EXTERN
glong gi_marshalling_tests_callback_return_value_and_multiple_out_parameters (GIMarshallingTestsCallbackReturnValueAndMultipleOutParameters  callback,
                                                                              glong                                                         *a,
                                                                              glong                                                         *b);

/**
 * GIMarshallingTestsCallbackOwnedBoxed
* @box: (transfer none): the boxed structure.
 */
typedef void (* GIMarshallingTestsCallbackOwnedBoxed) (GIMarshallingTestsBoxedStruct *box,
						       void                      *user_data);


_GI_TEST_EXTERN
glong gi_marshalling_tests_callback_owned_boxed (GIMarshallingTestsCallbackOwnedBoxed  callback,
                                                 void *callback_data);

/* Pointer */


_GI_TEST_EXTERN
gpointer gi_marshalling_tests_pointer_in_return (gpointer pointer);

/* Structure */

struct _GIMarshallingTestsSimpleStruct {
    glong long_;
    gint8 int8;
};

typedef struct {
    GIMarshallingTestsSimpleStruct simple_struct;
} GIMarshallingTestsNestedStruct;

typedef struct {
    GIMarshallingTestsNestedStruct *pointer;
} GIMarshallingTestsNotSimpleStruct;


_GI_TEST_EXTERN
GIMarshallingTestsSimpleStruct *gi_marshalling_tests_simple_struct_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_simple_struct_inv (GIMarshallingTestsSimpleStruct *struct_);


_GI_TEST_EXTERN
void gi_marshalling_tests_simple_struct_method (GIMarshallingTestsSimpleStruct *struct_);


typedef struct {
    glong long_;
} GIMarshallingTestsPointerStruct;


_GI_TEST_EXTERN
GType gi_marshalling_tests_pointer_struct_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsPointerStruct *gi_marshalling_tests_pointer_struct_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_pointer_struct_inv (GIMarshallingTestsPointerStruct *struct_);

struct _GIMarshallingTestsBoxedStruct {
    glong long_;
    gchar *string_;
    GStrv g_strv;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_boxed_struct_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsBoxedStruct *gi_marshalling_tests_boxed_struct_new (void);


_GI_TEST_EXTERN
GIMarshallingTestsBoxedStruct *gi_marshalling_tests_boxed_struct_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_boxed_struct_inv (GIMarshallingTestsBoxedStruct *struct_);


_GI_TEST_EXTERN
void gi_marshalling_tests_boxed_struct_out (GIMarshallingTestsBoxedStruct **struct_);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_boxed_struct_out_uninitialized (GIMarshallingTestsBoxedStruct **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_boxed_struct_inout (GIMarshallingTestsBoxedStruct **struct_);

typedef union {
    glong long_;
} GIMarshallingTestsUnion;

typedef union {
    glong long_;
    gsize size;
    const gchar *str;
} GIMarshallingTestsUnregisteredUnion;


_GI_TEST_EXTERN
GType gi_marshalling_tests_union_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsUnion *gi_marshalling_tests_union_returnv (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_union_inv (GIMarshallingTestsUnion *union_);


_GI_TEST_EXTERN
void gi_marshalling_tests_union_method (GIMarshallingTestsUnion *union_);

typedef enum
{
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NONE,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SIMPLE_STRUCT,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_NESTED_STRUCT,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_POINTER_STRUCT,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_BOXED_STRUCT_PTR,
  GI_MARSHALLING_TESTS_STRUCTURED_UNION_TYPE_SINGLE_UNION,
} GIMarshallingTestsStructuredUnionType;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsSimpleStruct parent;
} GIMarshallingTestsStructuredUnionSimpleStruct;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsNestedStruct parent;
} GIMarshallingTestsStructuredUnionNestedStruct;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsPointerStruct parent;
} GIMarshallingTestsStructuredUnionPointerStruct;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsBoxedStruct parent;
} GIMarshallingTestsStructuredUnionBoxedStruct;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsBoxedStruct *parent;
} GIMarshallingTestsStructuredUnionBoxedStructPtr;

typedef struct {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsUnion union_;
} GIMarshallingTestsStructuredUnionUnionStruct;

typedef union {
    GIMarshallingTestsStructuredUnionUnionStruct parent;
} GIMarshallingTestsStructuredUnionSingleUnion;

typedef union {
    /*< private >*/
    GIMarshallingTestsStructuredUnionType type;

    GIMarshallingTestsStructuredUnionSimpleStruct simple_struct;
    GIMarshallingTestsStructuredUnionNestedStruct nested_struct;
    GIMarshallingTestsStructuredUnionPointerStruct pointer_struct;
    GIMarshallingTestsStructuredUnionBoxedStruct boxed_struct;
    GIMarshallingTestsStructuredUnionBoxedStructPtr boxed_struct_ptr;
    GIMarshallingTestsStructuredUnionSingleUnion single_union;
} GIMarshallingTestsStructuredUnion;

_GI_TEST_EXTERN
GType gi_marshalling_tests_structured_union_get_type (void) G_GNUC_CONST;

_GI_TEST_EXTERN
GIMarshallingTestsStructuredUnion *gi_marshalling_tests_structured_union_new (GIMarshallingTestsStructuredUnionType type);

/* we want this to clash with a field value name */
_GI_TEST_EXTERN
GIMarshallingTestsStructuredUnionType gi_marshalling_tests_structured_union_type (GIMarshallingTestsStructuredUnion *structured_union);

typedef union {
    GIMarshallingTestsStructuredUnionType type;
    GIMarshallingTestsStructuredUnionSimpleStruct simple_struct1;
    GIMarshallingTestsStructuredUnionSimpleStruct simple_struct2;
} GIMarshallingTestsStructuredUnionSingleType;

 /* Object */

#define GI_MARSHALLING_TESTS_TYPE_OBJECT             (gi_marshalling_tests_object_get_type ())
#define GI_MARSHALLING_TESTS_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_OBJECT, GIMarshallingTestsObject))
#define GI_MARSHALLING_TESTS_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_OBJECT, GIMarshallingTestsObjectClass))
#define GI_MARSHALLING_TESTS_IS_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_OBJECT))
#define GI_MARSHALLING_TESTS_IS_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_OBJECT))
#define GI_MARSHALLING_TESTS_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_OBJECT, GIMarshallingTestsObjectClass))

typedef struct _GIMarshallingTestsObjectClass GIMarshallingTestsObjectClass;
typedef struct _GIMarshallingTestsObject GIMarshallingTestsObject;

typedef int (* GIMarshallingTestsCallbackIntInt) (int val, void *user_data);

struct _GIMarshallingTestsObjectClass
{
	GObjectClass parent_class;

    /**
     * GIMarshallingTestsObjectClass::method_int8_in:
     * @in: (in):
     */
    void (* method_int8_in) (GIMarshallingTestsObject *self, gint8 in);

    /**
     * GIMarshallingTestsObjectClass::method_int8_out:
     * @out: (out):
     */
    void (* method_int8_out) (GIMarshallingTestsObject *self, gint8 *out);

    /**
     * GIMarshallingTestsObjectClass::method_int8_arg_and_out_caller:
     * @out: (out):
     */
    void (* method_int8_arg_and_out_caller) (GIMarshallingTestsObject *self, gint8 arg, gint8 *out);

    /**
     * GIMarshallingTestsObjectClass::method_int8_arg_and_out_callee:
     * @out: (out):
     */
    void (* method_int8_arg_and_out_callee) (GIMarshallingTestsObject *self, gint8 arg, gint8 **out);

    /**
     * GIMarshallingTestsObjectClass::method_str_arg_out_ret:
     * @out: (out):
     *
     * Returns: (transfer none)
     */
    const gchar* (* method_str_arg_out_ret) (GIMarshallingTestsObject *self, const gchar* arg, guint *out);

    /**
     * GIMarshallingTestsObjectClass::method_with_default_implementation:
     * @in: (in):
     */
    void (* method_with_default_implementation) (GIMarshallingTestsObject *self, gint8 in);

    /**
     * GIMarshallingTestsObjectClass::method_deep_hierarchy:
     * @in: (in):
     */
    void (* method_deep_hierarchy) (GIMarshallingTestsObject *self, gint8 in);

    void (* vfunc_with_callback) (GIMarshallingTestsObject *self,
                                  GIMarshallingTestsCallbackIntInt callback,
                                  void *callback_data);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_value_only:
     */
    glong (* vfunc_return_value_only) (GIMarshallingTestsObject *self);

    /**
     * GIMarshallingTestsObjectClass::vfunc_one_out_parameter:
     * @a: (out):
     */
    void  (* vfunc_one_out_parameter) (GIMarshallingTestsObject *self, gfloat *a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_multiple_out_parameters:
     * @a: (out):
     * @b: (out):
     */
    void  (* vfunc_multiple_out_parameters) (GIMarshallingTestsObject *self, gfloat *a, gfloat *b);

    /**
     * GIMarshallingTestsObjectClass::vfunc_one_inout_parameter:
     * @a: (inout):
     */
    void  (* vfunc_one_inout_parameter) (GIMarshallingTestsObject *self, gfloat *a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_multiple_inout_parameters:
     * @a: (inout):
     * @b: (inout):
     */
    void  (* vfunc_multiple_inout_parameters) (GIMarshallingTestsObject *self, gfloat *a, gfloat *b);

    /**
     * GIMarshallingTestsObjectClass::vfunc_caller_allocated_out_parameter:
     * @a: (out):
     */
    void  (* vfunc_caller_allocated_out_parameter) (GIMarshallingTestsObject *self, GValue *a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_array_out_parameter:
     * @a: (out) (array zero-terminated):
     */
    void  (* vfunc_array_out_parameter) (GIMarshallingTestsObject *self, gfloat **a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_value_and_one_out_parameter:
     * @a: (out):
     */
    glong (* vfunc_return_value_and_one_out_parameter) (GIMarshallingTestsObject *self, glong *a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_value_and_multiple_out_parameters:
     * @a: (out):
     * @b: (out):
     */
    glong (* vfunc_return_value_and_multiple_out_parameters) (GIMarshallingTestsObject *self, glong *a, glong *b);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_value_and_one_inout_parameter:
     * @a: (inout):
     */
    glong (* vfunc_return_value_and_one_inout_parameter) (GIMarshallingTestsObject *self, glong *a);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_value_and_multiple_inout_parameters:
     * @a: (inout):
     * @b: (inout):
     */
    glong (* vfunc_return_value_and_multiple_inout_parameters) (GIMarshallingTestsObject *self, glong *a, glong *b);

    /**
     * GIMarshallingTestsObjectClass::vfunc_meth_with_err:
     * @x:
     * @error: A #GError
     */
    gboolean (*vfunc_meth_with_err) (GIMarshallingTestsObject *object, gint x, GError **error);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_enum:
     */
    GIMarshallingTestsEnum (* vfunc_return_enum) (GIMarshallingTestsObject *self);

    /**
     * GIMarshallingTestsObjectClass::vfunc_out_enum:
     * @_enum: (out):
     */
    void (* vfunc_out_enum) (GIMarshallingTestsObject *self, GIMarshallingTestsEnum *_enum);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_object_transfer_none:
     *
     * Returns: (transfer none)
     */
    GObject* (* vfunc_return_object_transfer_none) (GIMarshallingTestsObject *self);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_object_transfer_full:
     *
     * Returns: (transfer full)
     */
    GObject* (* vfunc_return_object_transfer_full) (GIMarshallingTestsObject *self);

    /**
     * GIMarshallingTestsObjectClass::vfunc_out_object_transfer_none:
     * @object: (out) (transfer none):
     */
    void (* vfunc_out_object_transfer_none) (GIMarshallingTestsObject *self, GObject **object);

    /**
     * GIMarshallingTestsObjectClass::vfunc_out_object_transfer_full:
     * @object: (out) (transfer full):
     */
    void (* vfunc_out_object_transfer_full) (GIMarshallingTestsObject *self, GObject **object);

    /**
     * GIMarshallingTestsObjectClass::vfunc_in_object_transfer_none:
     * @object: (in) (transfer none):
     */
    void (* vfunc_in_object_transfer_none) (GIMarshallingTestsObject *self, GObject *object);

    /**
     * GIMarshallingTestsObjectClass::vfunc_in_object_transfer_full:
     * @object: (in) (transfer full):
     */
    void (* vfunc_in_object_transfer_full) (GIMarshallingTestsObject *self, GObject *object);

    /**
     * GIMarshallingTestsObjectClass::vfunc_return_flags:
     */
    GIMarshallingTestsFlags (* vfunc_return_flags) (GIMarshallingTestsObject *self);

    /**
     * GIMarshallingTestsObjectClass::vfunc_out_flags:
     * @flags: (out):
     */
    void (* vfunc_out_flags) (GIMarshallingTestsObject *self, GIMarshallingTestsFlags *flags);
};

struct _GIMarshallingTestsObject
{
	GObject parent_instance;

    gint int_;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_object_get_type (void) G_GNUC_CONST;

_GI_TEST_EXTERN
void gi_marshalling_tests_object_static_method (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method (GIMarshallingTestsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_overridden_method (GIMarshallingTestsObject *object);

_GI_TEST_EXTERN
GIMarshallingTestsObject *gi_marshalling_tests_object_new (gint int_);
GIMarshallingTestsObject *gi_marshalling_tests_object_new_fail (gint int_, GError **error);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_array_in (GIMarshallingTestsObject *object, const gint *ints, gint length);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_array_out (GIMarshallingTestsObject *object, gint **ints, gint *length);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_array_inout (GIMarshallingTestsObject *object, gint **ints, gint *length);

_GI_TEST_EXTERN
const gint *gi_marshalling_tests_object_method_array_return (GIMarshallingTestsObject *object, gint *length);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_int8_in (GIMarshallingTestsObject *object, gint8 in);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_int8_out (GIMarshallingTestsObject *object, gint8 *out);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_int8_arg_and_out_caller (GIMarshallingTestsObject *object, gint8 arg, gint8 *out);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_int8_arg_and_out_callee (GIMarshallingTestsObject *object, gint8 arg, gint8 **out);

_GI_TEST_EXTERN
const gchar* gi_marshalling_tests_object_method_str_arg_out_ret (GIMarshallingTestsObject *object, const gchar* arg, guint *out);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_with_default_implementation (GIMarshallingTestsObject *object, gint8 in);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_method_variant_array_in (GIMarshallingTestsObject *object, GVariant **in, gsize n_in);


_GI_TEST_EXTERN
glong gi_marshalling_tests_object_vfunc_return_value_only (GIMarshallingTestsObject *self);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_one_out_parameter (GIMarshallingTestsObject *self, gfloat *a);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_multiple_out_parameters (GIMarshallingTestsObject *self, gfloat *a, gfloat *b);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_one_inout_parameter (GIMarshallingTestsObject *self, gfloat *a);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_multiple_inout_parameters (GIMarshallingTestsObject *self, gfloat *a, gfloat *b);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_caller_allocated_out_parameter (GIMarshallingTestsObject *self, GValue *a);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_array_out_parameter (GIMarshallingTestsObject *self, gfloat **a);

_GI_TEST_EXTERN
glong gi_marshalling_tests_object_vfunc_return_value_and_one_out_parameter (GIMarshallingTestsObject *self, glong *a);

_GI_TEST_EXTERN
glong gi_marshalling_tests_object_vfunc_return_value_and_multiple_out_parameters (GIMarshallingTestsObject *self, glong *a, glong *b);

_GI_TEST_EXTERN
glong gi_marshalling_tests_object_vfunc_return_value_and_one_inout_parameter (GIMarshallingTestsObject *self, glong *a);

_GI_TEST_EXTERN
glong gi_marshalling_tests_object_vfunc_return_value_and_multiple_inout_parameters (GIMarshallingTestsObject *self, glong *a, glong *b);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_object_vfunc_meth_with_error (GIMarshallingTestsObject *object, gint x, GError **error);


_GI_TEST_EXTERN
GIMarshallingTestsEnum gi_marshalling_tests_object_vfunc_return_enum (GIMarshallingTestsObject *self);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_out_enum (GIMarshallingTestsObject *self, GIMarshallingTestsEnum *_enum);

_GI_TEST_EXTERN
GIMarshallingTestsFlags gi_marshalling_tests_object_vfunc_return_flags (GIMarshallingTestsObject *self);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_out_flags (GIMarshallingTestsObject *self, GIMarshallingTestsFlags *flags);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_none (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_return_object_transfer_full (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_none (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_out_object_transfer_full (GIMarshallingTestsObject *self, guint *ref_count, gboolean *is_floating);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_none (GIMarshallingTestsObject *self, GType type, guint *ref_count, gboolean *is_floating);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_get_ref_info_for_vfunc_in_object_transfer_full (GIMarshallingTestsObject *self, GType type, guint *ref_count, gboolean *is_floating);


_GI_TEST_EXTERN
GIMarshallingTestsObject *gi_marshalling_tests_object_none_return (void);

_GI_TEST_EXTERN
GIMarshallingTestsObject *gi_marshalling_tests_object_full_return (void);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_none_in (GIMarshallingTestsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_full_in (GIMarshallingTestsObject *object);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_none_out (GIMarshallingTestsObject **object);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_object_none_out_uninitialized (GIMarshallingTestsObject **v G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_full_out (GIMarshallingTestsObject **object);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_object_full_out_uninitialized (GIMarshallingTestsObject **v G_GNUC_UNUSED);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_none_inout (GIMarshallingTestsObject **object);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_full_inout (GIMarshallingTestsObject **object);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_int8_in (GIMarshallingTestsObject *object, gint8 in);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_int8_out (GIMarshallingTestsObject *object, gint8 *out);


_GI_TEST_EXTERN
void gi_marshalling_tests_object_vfunc_with_callback (GIMarshallingTestsObject *object,
                                                      GIMarshallingTestsCallbackIntInt callback,
                                                      void *callback_data);

_GI_TEST_EXTERN
void gi_marshalling_tests_object_call_vfunc_with_callback (GIMarshallingTestsObject *object);

#define GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT             (gi_marshalling_tests_sub_object_get_type ())
#define GI_MARSHALLING_TESTS_SUB_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT, GIMarshallingTestsSubObject))
#define GI_MARSHALLING_TESTS_SUB_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT, GIMarshallingTestsSubObjectClass))
#define GI_MARSHALLING_TESTS_IS_SUB_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT))
#define GI_MARSHALLING_TESTS_IS_SUB_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT))
#define GI_MARSHALLING_TESTS_SUB_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_OBJECT, GIMarshallingTestsSubObjectClass))

typedef struct _GIMarshallingTestsSubObjectClass GIMarshallingTestsSubObjectClass;
typedef struct _GIMarshallingTestsSubObject GIMarshallingTestsSubObject;

struct _GIMarshallingTestsSubObjectClass
{
	GIMarshallingTestsObjectClass parent_class;
};

struct _GIMarshallingTestsSubObject
{
	GIMarshallingTestsObject parent_instance;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_sub_object_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
void gi_marshalling_tests_sub_object_sub_method (GIMarshallingTestsSubObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_sub_object_overwritten_method (GIMarshallingTestsSubObject *object);

#define GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT             (gi_marshalling_tests_sub_sub_object_get_type ())
#define GI_MARSHALLING_TESTS_SUB_SUB_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT, GIMarshallingTestsSubSubObject))
#define GI_MARSHALLING_TESTS_SUB_SUB_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT, GIMarshallingTestsSubSubObjectClass))
#define GI_MARSHALLING_TESTS_IS_SUB_SUB_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT))
#define GI_MARSHALLING_TESTS_IS_SUB_SUB_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT))
#define GI_MARSHALLING_TESTS_SUB_SUB_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_SUB_SUB_OBJECT, GIMarshallingTestsSubSubObjectClass))

typedef struct _GIMarshallingTestsSubSubObjectClass GIMarshallingTestsSubSubObjectClass;
typedef struct _GIMarshallingTestsSubSubObject GIMarshallingTestsSubSubObject;

struct _GIMarshallingTestsSubSubObjectClass
{
	GIMarshallingTestsSubObjectClass parent_class;
};

struct _GIMarshallingTestsSubSubObject
{
	GIMarshallingTestsSubObject parent_instance;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_sub_sub_object_get_type (void) G_GNUC_CONST;

/* Interfaces */

#define GI_MARSHALLING_TESTS_TYPE_INTERFACE              (gi_marshalling_tests_interface_get_type ())
#define GI_MARSHALLING_TESTS_INTERFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE, GIMarshallingTestsInterface))
#define GI_MARSHALLING_TESTS_IS_INTERFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE))
#define GI_MARSHALLING_TESTS_INTERFACE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE, GIMarshallingTestsInterfaceIface))

typedef struct _GIMarshallingTestsInterface GIMarshallingTestsInterface;
typedef struct _GIMarshallingTestsInterfaceIface GIMarshallingTestsInterfaceIface;

struct _GIMarshallingTestsInterfaceIface {
    GTypeInterface base_iface;

    /**
     * GIMarshallingTestsInterfaceIface::test_int8_in:
     * @in: (in):
     */
    void (* test_int8_in) (GIMarshallingTestsInterface *self, gint8 in);
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_interface_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
void gi_marshalling_tests_interface_test_int8_in (GIMarshallingTestsInterface *self, gint8 in);


_GI_TEST_EXTERN
void gi_marshalling_tests_test_interface_test_int8_in (GIMarshallingTestsInterface *test_iface, gint8 in);

/* GIMarshallingTestsInterfaceImpl is a class that implements
   GIMarshallingTestsInterface */

#define GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL     (gi_marshalling_tests_interface_impl_get_type ())
#define GI_MARSHALLING_TESTS_INTERFACE_IMPL(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL, GIMarshallingTestsInterfaceImpl))
#define GI_MARSHALLING_TESTS_INTERFACE_IMPL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL, GIMarshallingTestsInterfaceImplClass))
#define GI_MARSHALLING_TESTS_IS_INTERFACE_IMPL(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL))
#define GI_MARSHALLING_TESTS_IS_INTERFACE_IMPL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL))
#define GI_MARSHALLING_TESTS_INTERFACE_IMPL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE_IMPL, GIMarshallingTestsInterfaceImplClass))


typedef struct _GIMarshallingTestsInterfaceImplClass GIMarshallingTestsInterfaceImplClass;
typedef struct _GIMarshallingTestsInterfaceImpl GIMarshallingTestsInterfaceImpl;

struct _GIMarshallingTestsInterfaceImplClass
{
    GObjectClass parent_class;
};

struct _GIMarshallingTestsInterfaceImpl
{
    GObject parent_instance;

    gint int_;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_interface_impl_get_type (void) G_GNUC_CONST;

_GI_TEST_EXTERN
GIMarshallingTestsInterface *gi_marshalling_tests_interface_impl_get_as_interface (GIMarshallingTestsInterfaceImpl *self);

/* GIMarshallingTestsInterface2 allows us testing vfunc clashes when a class'
   vfunc implementation ambiguously relates to its prototype */

#define GI_MARSHALLING_TESTS_TYPE_INTERFACE2              (gi_marshalling_tests_interface2_get_type ())
#define GI_MARSHALLING_TESTS_INTERFACE2(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE2, GIMarshallingTestsInterface2))
#define GI_MARSHALLING_TESTS_IS_INTERFACE2(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE2))
#define GI_MARSHALLING_TESTS_INTERFACE2_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE2, GIMarshallingTestsInterface2Iface))

typedef struct _GIMarshallingTestsInterface2 GIMarshallingTestsInterface2;
typedef struct _GIMarshallingTestsInterface2Iface GIMarshallingTestsInterface2Iface;

struct _GIMarshallingTestsInterface2Iface {
    GTypeInterface base_iface;

    /**
     * GIMarshallingTestsInterface2Iface::test_int8_in:
     * @in: (in):
     */
    void (* test_int8_in) (GIMarshallingTestsInterface2 *self, gint8 in);
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_interface2_get_type (void) G_GNUC_CONST;

/* GIMarshallingTestsInterface3 tests passing arrays of variants from C to @lang */

#define GI_MARSHALLING_TESTS_TYPE_INTERFACE3              (gi_marshalling_tests_interface3_get_type ())
#define GI_MARSHALLING_TESTS_INTERFACE3(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE3, GIMarshallingTestsInterface3))
#define GI_MARSHALLING_TESTS_IS_INTERFACE3(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GI_MARSHALLING_TESTS_TYPE_INTERFACE3))
#define GI_MARSHALLING_TESTS_INTERFACE3_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GI_MARSHALLING_TESTS_TYPE_INTERFACE3, GIMarshallingTestsInterface3Iface))

typedef struct _GIMarshallingTestsInterface3 GIMarshallingTestsInterface3;
typedef struct _GIMarshallingTestsInterface3Iface GIMarshallingTestsInterface3Iface;

struct _GIMarshallingTestsInterface3Iface {
    GTypeInterface base_iface;

    /**
     * GIMarshallingTestsInterface3::test_variant_array_in:
     * @in: (in) (array length=n_in):
     */
    void (* test_variant_array_in) (GIMarshallingTestsInterface3 *self, GVariant **in, gsize n_in);
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_interface3_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
void gi_marshalling_tests_interface3_test_variant_array_in (GIMarshallingTestsInterface3 *self, GVariant **in, gsize n_in);

/* Multiple output arguments */


_GI_TEST_EXTERN
void gi_marshalling_tests_int_out_out (gint *int0, gint *int1);

_GI_TEST_EXTERN
void gi_marshalling_tests_int_three_in_three_out(gint a, gint b, gint c,
                                                 gint *out0, gint *out1, gint *out2);

_GI_TEST_EXTERN
gint gi_marshalling_tests_int_return_out (gint *int_);

/* Default arguments */
_GI_TEST_EXTERN
void gi_marshalling_tests_int_two_in_utf8_two_in_with_allow_none  (gint a, gint b, const gchar *c, const gchar *d);

_GI_TEST_EXTERN
void gi_marshalling_tests_int_one_in_utf8_two_in_one_allows_none  (gint a, const gchar *b, const gchar *c);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_utf8_two_in (const gint *ints, gint length, const gchar *a, const gchar *b);

_GI_TEST_EXTERN
void gi_marshalling_tests_array_in_utf8_two_in_out_of_order (gint length, const gchar *a, const gint *ints, const gchar *b);

/* GError */

#define GI_MARSHALLING_TESTS_CONSTANT_GERROR_DOMAIN "gi-marshalling-tests-gerror-domain"
#define GI_MARSHALLING_TESTS_CONSTANT_GERROR_CODE 5
#define GI_MARSHALLING_TESTS_CONSTANT_GERROR_MESSAGE "gi-marshalling-tests-gerror-message"
#define GI_MARSHALLING_TESTS_CONSTANT_GERROR_DEBUG_MESSAGE "we got an error, life is shit"


_GI_TEST_EXTERN
void gi_marshalling_tests_gerror(GError **error);

_GI_TEST_EXTERN
void gi_marshalling_tests_gerror_array_in(gint *in_ints, GError **error);

_GI_TEST_EXTERN
void gi_marshalling_tests_gerror_out(GError **error, gchar **debug);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gerror_out_uninitialized (GError **v G_GNUC_UNUSED, gchar **v2 G_GNUC_UNUSED);

_GI_TEST_EXTERN
void gi_marshalling_tests_gerror_out_transfer_none(GError **err, const gchar **debug);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_gerror_out_transfer_none_uninitialized (GError **v G_GNUC_UNUSED, const gchar **v2 G_GNUC_UNUSED);

_GI_TEST_EXTERN
GError *gi_marshalling_tests_gerror_return(void);

/* GParamSpec */
_GI_TEST_EXTERN
void gi_marshalling_tests_param_spec_in_bool(const GParamSpec *param);

_GI_TEST_EXTERN
GParamSpec *gi_marshalling_tests_param_spec_return (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_param_spec_out(GParamSpec **param);

_GI_TEST_EXTERN
gboolean gi_marshalling_tests_param_spec_out_uninitialized (GParamSpec **v G_GNUC_UNUSED);

/* Overrides */

#define GI_MARSHALLING_TESTS_OVERRIDES_CONSTANT 42


typedef struct {
    glong long_;
} GIMarshallingTestsOverridesStruct;

_GI_TEST_EXTERN
GType gi_marshalling_tests_overrides_struct_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsOverridesStruct *gi_marshalling_tests_overrides_struct_new (void);


_GI_TEST_EXTERN
glong gi_marshalling_tests_overrides_struct_method (GIMarshallingTestsOverridesStruct *struct_);


_GI_TEST_EXTERN
GIMarshallingTestsOverridesStruct *gi_marshalling_tests_overrides_struct_returnv (void);


#define GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT             (gi_marshalling_tests_overrides_object_get_type ())
#define GI_MARSHALLING_TESTS_OVERRIDES_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT, GIMarshallingTestsOverridesObject))
#define GI_MARSHALLING_TESTS_OVERRIDES_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT, GIMarshallingTestsOverridesObjectClass))
#define GI_MARSHALLING_TESTS_IS_OVERRIDES_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT))
#define GI_MARSHALLING_TESTS_IS_OVERRIDES_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT))
#define GI_MARSHALLING_TESTS_OVERRIDES_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_OVERRIDES_OBJECT, GIMarshallingTestsOverridesObjectClass))

typedef struct _GIMarshallingTestsOverridesObjectClass GIMarshallingTestsOverridesObjectClass;
typedef struct _GIMarshallingTestsOverridesObject GIMarshallingTestsOverridesObject;

struct _GIMarshallingTestsOverridesObjectClass
{
    GObjectClass parent_class;
};

struct _GIMarshallingTestsOverridesObject
{
    GObject parent_instance;

    glong long_;
};

_GI_TEST_EXTERN
GType gi_marshalling_tests_overrides_object_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsOverridesObject *gi_marshalling_tests_overrides_object_new (void);


_GI_TEST_EXTERN
glong gi_marshalling_tests_overrides_object_method (GIMarshallingTestsOverridesObject *object);


_GI_TEST_EXTERN
GIMarshallingTestsOverridesObject *gi_marshalling_tests_overrides_object_returnv (void);

/* Properties Object */

#define GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT (gi_marshalling_tests_properties_object_get_type ())
#define GI_MARSHALLING_TESTS_PROPERTIES_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT, GIMarshallingTestsPropertiesObject))
#define GI_MARSHALLING_TESTS_PROPERTIES_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT, GIMarshallingTestsPropertiesObjectClass))
#define GI_MARSHALLING_TESTS_IS_PROPERTIES_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT))
#define GI_MARSHALLING_TESTS_IS_PROPERTIES_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT))
#define GI_MARSHALLING_TESTS_PROPERTIES_OBJECT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_PROPERTIES_OBJECT, GIMarshallingTestsPropertiesObjectClass))

typedef struct _GIMarshallingTestsPropertiesObject GIMarshallingTestsPropertiesObject;
typedef struct _GIMarshallingTestsPropertiesObjectClass GIMarshallingTestsPropertiesObjectClass;

struct _GIMarshallingTestsPropertiesObject {
    GObject parent_instance;

    gboolean some_boolean;
    gchar some_char;
    guchar some_uchar;
    gint some_int;
    guint some_uint;
    glong some_long;
    gulong some_ulong;
    gint64 some_int64;
    guint64 some_uint64;
    gfloat some_float;
    gdouble some_double;
    gchar *some_string;
    gchar **some_strv;
    GIMarshallingTestsBoxedStruct* some_boxed_struct;
    GList* some_boxed_glist;
    GValue *some_gvalue;
    GVariant *some_variant;
    GObject *some_object;
    GIMarshallingTestsFlags some_flags;
    GIMarshallingTestsGEnum some_enum;
    GByteArray *some_byte_array;
};

struct _GIMarshallingTestsPropertiesObjectClass {
    GObjectClass parent_class;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_properties_object_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsPropertiesObject *gi_marshalling_tests_properties_object_new (void);

/* Signals object */

#define GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT (gi_marshalling_tests_signals_object_get_type ())
#define GI_MARSHALLING_TESTS_SIGNALS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT, GIMarshallingTestsSignalsObject))
#define GI_MARSHALLING_TESTS_SIGNALS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT, GIMarshallingTestsSignalsObjectClass))
#define GI_MARSHALLING_TESTS_IS_SIGNALS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT))
#define GI_MARSHALLING_TESTS_IS_SIGNALS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT))
#define GI_MARSHALLING_TESTS_SIGNALS_OBJECT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_MARSHALLING_TESTS_TYPE_SIGNALS_OBJECT, GIMarshallingTestsSignalsObjectClass))

typedef struct _GIMarshallingTestsSignalsObject GIMarshallingTestsSignalsObject;
typedef struct _GIMarshallingTestsSignalsObjectClass GIMarshallingTestsSignalsObjectClass;

struct _GIMarshallingTestsSignalsObject {
    GObject parent_instance;
};

struct _GIMarshallingTestsSignalsObjectClass {
    GObjectClass parent_class;
};


_GI_TEST_EXTERN
GType gi_marshalling_tests_signals_object_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
GIMarshallingTestsSignalsObject *gi_marshalling_tests_signals_object_new (void);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8 (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8_container (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_utf8_full (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct_container (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_gptrarray_boxed_struct_full (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_hash_table_utf8_int (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_hash_table_utf8_int_container (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_hash_table_utf8_int_full (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_struct (GIMarshallingTestsSignalsObject *object);

_GI_TEST_EXTERN
void gi_marshalling_tests_signals_object_emit_boxed_struct_full (GIMarshallingTestsSignalsObject *object);

#endif /* _GI_MARSHALLING_TESTS_H_ */
