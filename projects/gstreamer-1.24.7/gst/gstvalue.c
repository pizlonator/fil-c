/* GStreamer
 * Copyright (C) <2003> David A. Schleef <ds@schleef.org>
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

/**
 * SECTION:gstvalue
 * @title: GstValue
 * @short_description: GValue implementations specific
 * to GStreamer
 *
 * GValue implementations specific to GStreamer.
 *
 * Note that operations on the same #GValue from multiple threads may lead to
 * undefined behaviour.
 */

/* Suppress warnings for GValueAraray */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gst_private.h"
#include "glib-compat-private.h"
#include <gst/gst.h>
#include <gobject/gvaluecollector.h>
#include "gstutils.h"
#include "gstquark.h"

/* GstValueUnionFunc:
 * @dest: a #GValue for the result
 * @value1: a #GValue operand
 * @value2: a #GValue operand
 *
 * Used by gst_value_union() to perform unification for a specific #GValue
 * type. Register a new implementation with gst_value_register_union_func().
 *
 * Returns: %TRUE if a union was successful
 */
typedef gboolean (*GstValueUnionFunc) (GValue * dest,
    const GValue * value1, const GValue * value2);

/* GstValueIntersectFunc:
 * @dest: (out caller-allocates): a #GValue for the result
 * @value1: a #GValue operand
 * @value2: a #GValue operand
 *
 * Used by gst_value_intersect() to perform intersection for a specific #GValue
 * type. If the intersection is non-empty, the result is
 * placed in @dest and %TRUE is returned.  If the intersection is
 * empty, @dest is unmodified and %FALSE is returned.
 * Register a new implementation with gst_value_register_intersect_func().
 *
 * Returns: %TRUE if the values can intersect
 */
typedef gboolean (*GstValueIntersectFunc) (GValue * dest,
    const GValue * value1, const GValue * value2);

/* GstValueSubtractFunc:
 * @dest: (out caller-allocates): a #GValue for the result
 * @minuend: a #GValue operand
 * @subtrahend: a #GValue operand
 *
 * Used by gst_value_subtract() to perform subtraction for a specific #GValue
 * type. Register a new implementation with gst_value_register_subtract_func().
 *
 * Returns: %TRUE if the subtraction is not empty
 */
typedef gboolean (*GstValueSubtractFunc) (GValue * dest,
    const GValue * minuend, const GValue * subtrahend);

static void gst_value_register_union_func (GType type1,
    GType type2, GstValueUnionFunc func);
static void gst_value_register_intersect_func (GType type1,
    GType type2, GstValueIntersectFunc func);
static void gst_value_register_subtract_func (GType minuend_type,
    GType subtrahend_type, GstValueSubtractFunc func);

static gboolean _priv_gst_value_parse_list (gchar * s, gchar ** after,
    GValue * value, GType type, GParamSpec * pspec);
static gboolean _priv_gst_value_parse_array (gchar * s, gchar ** after,
    GValue * value, GType type, GParamSpec * pspec);

typedef struct _GstValueUnionInfo GstValueUnionInfo;
struct _GstValueUnionInfo
{
  GType type1;
  GType type2;
  GstValueUnionFunc func;
};

typedef struct _GstValueIntersectInfo GstValueIntersectInfo;
struct _GstValueIntersectInfo
{
  GType type1;
  GType type2;
  GstValueIntersectFunc func;
};

typedef struct _GstValueSubtractInfo GstValueSubtractInfo;
struct _GstValueSubtractInfo
{
  GType minuend;
  GType subtrahend;
  GstValueSubtractFunc func;
};

struct _GstFlagSetClass
{
  GTypeClass parent;
  GType flags_type;             /* Type of the GFlags this flagset carries (can be 0) */
};

typedef struct _GstFlagSetClass GstFlagSetClass;

typedef struct _GstValueAbbreviation GstValueAbbreviation;

struct _GstValueAbbreviation
{
  const gchar *type_name;
  GType type;
};

/* Actual internal implementation of "GstValueList" and
 * "GstValueArray" */
typedef struct _GstValueList GstValueList;

struct _GstValueList
{
  /* These 2 fields must remain the same so that they match the public
   * GArray structure (which was the former implementation) just in
   * case someone calls `gst_value_peek_pointer` to access the
   * array/list (such as in gststructure.c) */
  GValue *fields;
  guint len;

  guint allocated;
  GValue arr[1];
};

#define FUNDAMENTAL_TYPE_ID_MAX \
    (G_TYPE_FUNDAMENTAL_MAX >> G_TYPE_FUNDAMENTAL_SHIFT)
#define FUNDAMENTAL_TYPE_ID(type) \
    ((uintptr_t) (type) >> G_TYPE_FUNDAMENTAL_SHIFT)

#define VALUE_LIST_ARRAY(v) ((GstValueList *) (v)->data[0].v_pointer)
#define VALUE_LIST_SIZE(v) (VALUE_LIST_ARRAY(v)->len)
#define VALUE_LIST_GET_VALUE(v, index) ((const GValue *) &(VALUE_LIST_ARRAY(v)->fields[index]))
#define VALUE_LIST_IS_USING_DYNAMIC_ARRAY(array) ((array)->fields != &(array)->arr[0])

static GArray *gst_value_table;
static GHashTable *gst_value_hash;
static GstValueTable *gst_value_tables_fundamental[FUNDAMENTAL_TYPE_ID_MAX + 1];
static GArray *gst_value_union_funcs;
static GArray *gst_value_intersect_funcs;
static GArray *gst_value_subtract_funcs;

/* Forward declarations */
static gchar *gst_value_serialize_fraction (const GValue * value);
static gint gst_value_compare_fraction (const GValue * value1,
    const GValue * value2);

static GstValueCompareFunc gst_value_get_compare_func (const GValue * value1);

static gchar *gst_string_wrap (const gchar * s);
static gchar *gst_string_unwrap (const gchar * s);

static void gst_value_move (GValue * dest, GValue * src);
static void _gst_value_list_append_and_take_value (GValue * value,
    GValue * append_value);
static void _gst_value_array_append_and_take_value (GValue * value,
    GValue * append_value);

static inline GstValueTable *
gst_value_hash_lookup_type (GType type)
{
  if (G_LIKELY (G_TYPE_IS_FUNDAMENTAL (type)))
    return gst_value_tables_fundamental[FUNDAMENTAL_TYPE_ID (type)];
  else
    return g_hash_table_lookup (gst_value_hash, (gpointer) type);
}

static void
gst_value_hash_add_type (GType type, const GstValueTable * table)
{
  if (G_TYPE_IS_FUNDAMENTAL (type))
    gst_value_tables_fundamental[FUNDAMENTAL_TYPE_ID (type)] = (gpointer) table;

  g_hash_table_insert (gst_value_hash, (gpointer) type, (gpointer) table);
}

/********
 * list *
 ********/

static void
resize_value_list (GstValueList * vlist)
{
  guint want_alloc;

  if (G_UNLIKELY (vlist->allocated > (G_MAXUINT / 2)))
    g_error ("Growing GstValueList would result in overflow");

  want_alloc = MAX (GST_ROUND_UP_8 (vlist->len + 1), vlist->allocated * 2);

  if (VALUE_LIST_IS_USING_DYNAMIC_ARRAY (vlist)) {
    vlist->fields = g_renew (GValue, vlist->fields, want_alloc);
  } else {
    vlist->fields = g_new0 (GValue, want_alloc);
    memcpy (vlist->fields, &vlist->arr[0], vlist->len * sizeof (GValue));
    GST_CAT_LOG (GST_CAT_PERFORMANCE, "Exceeding pre-allocated array");
  }
  vlist->allocated = want_alloc;
}

/* Replacement for g_array_append_val */
static void
_gst_value_list_append_val (GstValueList * vlist, GValue * val)
{
  /* resize if needed */
  if (G_UNLIKELY (vlist->len == vlist->allocated))
    resize_value_list (vlist);

  /* Finally set value */
  vlist->fields[vlist->len++] = *val;
}

/* Replacement for g_array_prepend_val */
static void
_gst_value_list_prepend_val (GstValueList * vlist, GValue * val)
{
  /* resize if needed */
  if (G_UNLIKELY (vlist->len == vlist->allocated))
    resize_value_list (vlist);

  /* Shift everything */
  memmove (&vlist->fields[1], &vlist->fields[0],
      (vlist->len) * sizeof (GValue));

  vlist->fields[0] = *val;
  vlist->len++;
}

static GstValueList *
_gst_value_list_new (guint prealloc)
{
  guint n_alloc;
  GstValueList *res;

  if (prealloc == 0)
    prealloc = 1;

  n_alloc = GST_ROUND_UP_8 (prealloc);
  res = g_malloc0 (sizeof (GstValueList) + (n_alloc - 1) * sizeof (GValue));

  res->len = 0;
  res->allocated = n_alloc;
  res->fields = &res->arr[0];

  return res;
}

static void
_gst_value_list_init (GValue * value, guint prealloc)
{
  value->g_type = GST_TYPE_LIST;
  memset (value->data, 0, sizeof (value->data));
  value->data[0].v_pointer = _gst_value_list_new (prealloc);
}

/**
 * gst_value_list_init:
 * @value: A zero-filled (uninitialized) #GValue structure
 * @prealloc: The number of entries to pre-allocate in the list
 *
 * Initializes and pre-allocates a #GValue of type #GST_TYPE_LIST.
 *
 * Returns: (transfer none): The #GValue structure that has been passed in
 *
 * Since: 1.18
 */

GValue *
gst_value_list_init (GValue * value, guint prealloc)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (value) == 0, NULL);

  _gst_value_list_init (value, prealloc);

  return value;
}

static void
_gst_value_array_init (GValue * value, guint prealloc)
{
  value->g_type = GST_TYPE_ARRAY;
  memset (value->data, 0, sizeof (value->data));
  value->data[0].v_pointer = _gst_value_list_new (prealloc);
}

/**
 * gst_value_array_init:
 * @value: A zero-filled (uninitialized) #GValue structure
 * @prealloc: The number of entries to pre-allocate in the array
 *
 * Initializes and pre-allocates a #GValue of type #GST_TYPE_ARRAY.
 *
 * Returns: (transfer none): The #GValue structure that has been passed in
 *
 * Since: 1.18
 */

GValue *
gst_value_array_init (GValue * value, guint prealloc)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (G_VALUE_TYPE (value) == 0, NULL);

  _gst_value_array_init (value, prealloc);

  return value;
}

/* two helper functions to serialize/stringify any type of list
 * regular lists are done with { }, arrays with < >
 */
gchar *
_priv_gst_value_serialize_any_list (const GValue * value, const gchar * begin,
    const gchar * end, gboolean print_type, GstSerializeFlags flags)
{
  guint i;
  GstValueList *vlist = value->data[0].v_pointer;
  GString *s;
  GValue *v;
  gchar *s_val;
  guint alen = vlist->len;

  /* estimate minimum string length to minimise re-allocs in GString */
  s = g_string_sized_new (2 + (6 * alen) + 2);
  g_string_append (s, begin);
  for (i = 0; i < alen; i++) {
    gboolean nested_structs_brackets;
    v = &vlist->fields[i];
    nested_structs_brackets = !(flags & GST_SERIALIZE_FLAG_BACKWARD_COMPAT)
        && (GST_VALUE_HOLDS_STRUCTURE (v) || GST_VALUE_HOLDS_CAPS (v));
    if (!nested_structs_brackets) {
      s_val = gst_value_serialize (v);
    } else {
      if (GST_VALUE_HOLDS_STRUCTURE (v))
        s_val =
            gst_structure_serialize_full (gst_value_get_structure (v), flags);
      else if (GST_VALUE_HOLDS_CAPS (v))
        s_val = gst_caps_serialize (gst_value_get_caps (v), flags);
    }
    if (s_val != NULL) {
      if (print_type) {
        g_string_append_c (s, '(');
        g_string_append (s, _priv_gst_value_gtype_to_abbr (G_VALUE_TYPE (v)));
        g_string_append_c (s, ')');
      }

      if (nested_structs_brackets)
        g_string_append_c (s, '[');
      g_string_append (s, s_val);
      if (nested_structs_brackets)
        g_string_append_c (s, ']');
      g_free (s_val);
      if (i < alen - 1) {
        g_string_append_len (s, ", ", 2);
      }
    } else {
      g_critical ("Could not serialize list/array value of type '%s'",
          G_VALUE_TYPE_NAME (v));
    }
  }
  g_string_append (s, end);
  return g_string_free (s, FALSE);
}

static void
gst_value_transform_any_list_string (const GValue * src_value,
    GValue * dest_value, const gchar * begin, const gchar * end)
{
  GValue *list_value;
  GstValueList *array;
  GString *s;
  guint i;
  gchar *list_s;
  guint alen;

  array = src_value->data[0].v_pointer;
  alen = array->len;

  /* estimate minimum string length to minimise re-allocs in GString */
  s = g_string_sized_new (2 + (10 * alen) + 2);
  g_string_append (s, begin);
  for (i = 0; i < alen; i++) {
    list_value = &array->fields[i];

    if (i != 0) {
      g_string_append_len (s, ", ", 2);
    }
    list_s = g_strdup_value_contents (list_value);
    g_string_append (s, list_s);
    g_free (list_s);
  }
  g_string_append (s, end);

  dest_value->data[0].v_pointer = g_string_free (s, FALSE);
}

static gchar *
_gst_value_serialize_g_value_array (const GValue * value, const gchar * begin,
    const gchar * end)
{
  guint i;
  GValueArray *array = value->data[0].v_pointer;
  GString *s;
  GValue *v;
  gchar *s_val;
  guint alen = 0;

  if (array)
    alen = array->n_values;

  /* estimate minimum string length to minimise re-allocs in GString */
  s = g_string_sized_new (2 + (6 * alen) + 2);
  g_string_append (s, begin);
  for (i = 0; i < alen; i++) {
    v = g_value_array_get_nth (array, i);
    s_val = gst_value_serialize (v);
    if (s_val != NULL) {
      g_string_append (s, s_val);
      g_free (s_val);
      if (i < alen - 1) {
        g_string_append_len (s, ", ", 2);
      }
    } else {
      g_critical ("Could not serialize list/array value of type '%s'",
          G_VALUE_TYPE_NAME (v));
    }
  }
  g_string_append (s, end);
  return g_string_free (s, FALSE);
}

static void
_gst_value_transform_g_value_array_string (const GValue * src_value,
    GValue * dest_value, const gchar * begin, const gchar * end)
{
  GValue *list_value;
  GValueArray *array;
  GString *s;
  guint i;
  gchar *list_s;
  guint alen;

  array = src_value->data[0].v_pointer;
  alen = array->n_values;

  /* estimate minimum string length to minimise re-allocs in GString */
  s = g_string_sized_new (2 + (10 * alen) + 2);
  g_string_append (s, begin);
  for (i = 0; i < alen; i++) {
    list_value = g_value_array_get_nth (array, i);

    if (i != 0) {
      g_string_append_len (s, ", ", 2);
    }
    list_s = g_strdup_value_contents (list_value);
    g_string_append (s, list_s);
    g_free (list_s);
  }
  g_string_append (s, end);

  dest_value->data[0].v_pointer = g_string_free (s, FALSE);
}

/*
 * helper function to see if a type is fixed. Is used internally here and
 * there. Do not export, since it doesn't work for types where the content
 * decides the fixedness (e.g. GST_TYPE_ARRAY).
 */
static gboolean
gst_type_is_fixed (GType type)
{
  /* the basic int, string, double types */
  if (type <= G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_GLIB_LAST)) {
    return TRUE;
  }
  /* our fundamental types that are certainly not fixed */
  if (type == GST_TYPE_INT_RANGE || type == GST_TYPE_DOUBLE_RANGE ||
      type == GST_TYPE_INT64_RANGE ||
      type == GST_TYPE_LIST || type == GST_TYPE_FRACTION_RANGE ||
      type == GST_TYPE_STRUCTURE) {
    return FALSE;
  }
  /* other (boxed) types that are fixed */
  if (type == GST_TYPE_BUFFER) {
    return TRUE;
  }
  /* heavy checks */
  if (G_TYPE_IS_FUNDAMENTAL (type) || G_TYPE_FUNDAMENTAL (type) <=
      G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_GLIB_LAST)) {
    return TRUE;
  }

  return FALSE;
}

/* GValue functions usable for both regular lists and arrays */
static void
gst_value_init_list_or_array (GValue * value)
{
  value->data[0].v_pointer = _gst_value_list_new (0);
}

static GstValueList *
copy_gst_value_list (const GstValueList * src)
{
  GstValueList *dest;
  guint i, len;

  len = src->len;
  dest = _gst_value_list_new (len);
  dest->len = len;
  for (i = 0; i < len; i++) {
    gst_value_init_and_copy (&dest->fields[i], &src->fields[i]);
  }

  return dest;
}

static void
gst_value_copy_list_or_array (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_pointer =
      copy_gst_value_list (VALUE_LIST_ARRAY (src_value));
}

static void
gst_value_free_list_or_array (GValue * value)
{
  guint i, len;
  GstValueList *src = VALUE_LIST_ARRAY (value);
  len = src->len;

  if ((value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS) == 0) {
    for (i = 0; i < len; i++) {
      g_value_unset (&src->fields[i]);
    }
    if (VALUE_LIST_IS_USING_DYNAMIC_ARRAY (src)) {
      g_free (src->fields);
    }
    g_free (src);
  }
}

static gpointer
gst_value_list_or_array_peek_pointer (const GValue * value)
{
  return value->data[0].v_pointer;
}

static gchar *
gst_value_collect_list_or_array (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  if (collect_flags & G_VALUE_NOCOPY_CONTENTS) {
    value->data[0].v_pointer = collect_values[0].v_pointer;
    value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
  } else {
    value->data[0].v_pointer =
        copy_gst_value_list ((GstValueList *) collect_values[0].v_pointer);
  }
  return NULL;
}

static gchar *
gst_value_lcopy_list_or_array (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  GstValueList **dest = collect_values[0].v_pointer;

  g_return_val_if_fail (dest != NULL,
      g_strdup_printf ("value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (value->data[0].v_pointer != NULL,
      g_strdup_printf ("invalid value given for `%s'",
          G_VALUE_TYPE_NAME (value)));

  if (collect_flags & G_VALUE_NOCOPY_CONTENTS) {
    *dest = (GstValueList *) value->data[0].v_pointer;
  } else {
    *dest = copy_gst_value_list (VALUE_LIST_ARRAY (value));
  }
  return NULL;
}

static gboolean
gst_value_list_or_array_get_basic_type (const GValue * value, GType * type)
{
  if (G_UNLIKELY (value == NULL))
    return FALSE;

  if (GST_VALUE_HOLDS_LIST (value) || GST_VALUE_HOLDS_ARRAY (value)) {
    if (VALUE_LIST_SIZE (value) == 0)
      return FALSE;
    return gst_value_list_or_array_get_basic_type (VALUE_LIST_GET_VALUE (value,
            0), type);
  }

  *type = G_VALUE_TYPE (value);

  return TRUE;
}

#define IS_RANGE_COMPAT(type1,type2,t1,t2) \
  (((t1) == (type1) && (t2) == (type2)) || ((t2) == (type1) && (t1) == (type2)))

static gboolean
gst_value_list_or_array_are_compatible (const GValue * value1,
    const GValue * value2)
{
  GType basic_type1, basic_type2;

  /* empty or same type is OK */
  if (!gst_value_list_or_array_get_basic_type (value1, &basic_type1) ||
      !gst_value_list_or_array_get_basic_type (value2, &basic_type2) ||
      basic_type1 == basic_type2)
    return TRUE;

  /* ranges are distinct types for each bound type... */
  if (IS_RANGE_COMPAT (G_TYPE_INT, GST_TYPE_INT_RANGE, basic_type1,
          basic_type2))
    return TRUE;
  if (IS_RANGE_COMPAT (G_TYPE_INT64, GST_TYPE_INT64_RANGE, basic_type1,
          basic_type2))
    return TRUE;
  if (IS_RANGE_COMPAT (G_TYPE_DOUBLE, GST_TYPE_DOUBLE_RANGE, basic_type1,
          basic_type2))
    return TRUE;
  if (IS_RANGE_COMPAT (GST_TYPE_FRACTION, GST_TYPE_FRACTION_RANGE, basic_type1,
          basic_type2))
    return TRUE;

  return FALSE;
}

static inline void
_gst_value_list_append_and_take_value (GValue * value, GValue * append_value)
{
  _gst_value_list_append_val (VALUE_LIST_ARRAY (value), append_value);
  memset (append_value, 0, sizeof (GValue));
}

/**
 * gst_value_list_append_and_take_value:
 * @value: a #GValue of type #GST_TYPE_LIST
 * @append_value: (transfer full): the value to append
 *
 * Appends @append_value to the GstValueList in @value.
 *
 * Since: 1.2
 */
void
gst_value_list_append_and_take_value (GValue * value, GValue * append_value)
{
  g_return_if_fail (GST_VALUE_HOLDS_LIST (value));
  g_return_if_fail (G_IS_VALUE (append_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          append_value));

  _gst_value_list_append_and_take_value (value, append_value);
}

/**
 * gst_value_list_append_value:
 * @value: a #GValue of type #GST_TYPE_LIST
 * @append_value: (transfer none): the value to append
 *
 * Appends @append_value to the GstValueList in @value.
 */
void
gst_value_list_append_value (GValue * value, const GValue * append_value)
{
  GValue val = { 0, };

  g_return_if_fail (GST_VALUE_HOLDS_LIST (value));
  g_return_if_fail (G_IS_VALUE (append_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          append_value));

  gst_value_init_and_copy (&val, append_value);
  _gst_value_list_append_val (VALUE_LIST_ARRAY (value), &val);
}

/**
 * gst_value_list_prepend_value:
 * @value: a #GValue of type #GST_TYPE_LIST
 * @prepend_value: the value to prepend
 *
 * Prepends @prepend_value to the GstValueList in @value.
 */
void
gst_value_list_prepend_value (GValue * value, const GValue * prepend_value)
{
  GValue val = { 0, };

  g_return_if_fail (GST_VALUE_HOLDS_LIST (value));
  g_return_if_fail (G_IS_VALUE (prepend_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          prepend_value));

  gst_value_init_and_copy (&val, prepend_value);
  _gst_value_list_prepend_val (VALUE_LIST_ARRAY (value), &val);
}

/**
 * gst_value_list_concat:
 * @dest: (out caller-allocates): an uninitialized #GValue to take the result
 * @value1: a #GValue
 * @value2: a #GValue
 *
 * Concatenates copies of @value1 and @value2 into a list.  Values that are not
 * of type #GST_TYPE_LIST are treated as if they were lists of length 1.
 * @dest will be initialized to the type #GST_TYPE_LIST.
 */
void
gst_value_list_concat (GValue * dest, const GValue * value1,
    const GValue * value2)
{
  guint i, value1_length, value2_length;
  GstValueList *vlist;

  g_return_if_fail (dest != NULL);
  g_return_if_fail (G_VALUE_TYPE (dest) == 0);
  g_return_if_fail (G_IS_VALUE (value1));
  g_return_if_fail (G_IS_VALUE (value2));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value1, value2));

  value1_length =
      (GST_VALUE_HOLDS_LIST (value1) ? VALUE_LIST_SIZE (value1) : 1);
  value2_length =
      (GST_VALUE_HOLDS_LIST (value2) ? VALUE_LIST_SIZE (value2) : 1);

  _gst_value_list_init (dest, value1_length + value2_length);
  vlist = VALUE_LIST_ARRAY (dest);
  vlist->len = value1_length + value2_length;

  if (GST_VALUE_HOLDS_LIST (value1)) {
    for (i = 0; i < value1_length; i++) {
      gst_value_init_and_copy (&vlist->fields[i],
          VALUE_LIST_GET_VALUE (value1, i));
    }
  } else {
    gst_value_init_and_copy (&vlist->fields[0], value1);
  }

  if (GST_VALUE_HOLDS_LIST (value2)) {
    for (i = 0; i < value2_length; i++) {
      gst_value_init_and_copy (&vlist->fields[i + value1_length],
          VALUE_LIST_GET_VALUE (value2, i));
    }
  } else {
    gst_value_init_and_copy (&vlist->fields[value1_length], value2);
  }
}

/* same as gst_value_list_concat() but takes ownership of GValues */
static void
gst_value_list_concat_and_take_values (GValue * dest, GValue * val1,
    GValue * val2)
{
  guint i, val1_length, val2_length;
  gboolean val1_is_list;
  gboolean val2_is_list;
  GstValueList *vlist;

  g_assert (dest != NULL);
  g_assert (G_VALUE_TYPE (dest) == 0);
  g_assert (G_IS_VALUE (val1));
  g_assert (G_IS_VALUE (val2));
  g_assert (gst_value_list_or_array_are_compatible (val1, val2));

  val1_is_list = GST_VALUE_HOLDS_LIST (val1);
  val1_length = (val1_is_list ? VALUE_LIST_SIZE (val1) : 1);

  val2_is_list = GST_VALUE_HOLDS_LIST (val2);
  val2_length = (val2_is_list ? VALUE_LIST_SIZE (val2) : 1);

  /* Overidding the default initialization to have a list of the target size */
  _gst_value_list_init (dest, val1_length + val2_length);
  vlist = VALUE_LIST_ARRAY (dest);
  vlist->len = val1_length + val2_length;

  if (val1_is_list) {
    for (i = 0; i < val1_length; i++) {
      vlist->fields[i] = *VALUE_LIST_GET_VALUE (val1, i);
    }
    VALUE_LIST_ARRAY (val1)->len = 0;
    g_value_unset (val1);
  } else {
    vlist->fields[0] = *val1;
    G_VALUE_TYPE (val1) = G_TYPE_INVALID;
  }

  if (val2_is_list) {
    for (i = 0; i < val2_length; i++) {
      const GValue *v2 = VALUE_LIST_GET_VALUE (val2, i);
      vlist->fields[i + val1_length] = *v2;
    }

    VALUE_LIST_ARRAY (val2)->len = 0;
    g_value_unset (val2);
  } else {
    vlist->fields[val1_length] = *val2;
    G_VALUE_TYPE (val2) = G_TYPE_INVALID;
  }
}

/**
 * gst_value_list_merge:
 * @dest: (out caller-allocates): an uninitialized #GValue to take the result
 * @value1: a #GValue
 * @value2: a #GValue
 *
 * Merges copies of @value1 and @value2.  Values that are not
 * of type #GST_TYPE_LIST are treated as if they were lists of length 1.
 *
 * The result will be put into @dest and will either be a list that will not
 * contain any duplicates, or a non-list type (if @value1 and @value2
 * were equal).
 */
void
gst_value_list_merge (GValue * dest, const GValue * value1,
    const GValue * value2)
{
  guint i, j, k, value1_length, value2_length, skipped;
  const GValue *src;
  gboolean skip;
  GstValueList *vlist;

  g_return_if_fail (dest != NULL);
  g_return_if_fail (G_VALUE_TYPE (dest) == 0);
  g_return_if_fail (G_IS_VALUE (value1));
  g_return_if_fail (G_IS_VALUE (value2));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value1, value2));

  value1_length =
      (GST_VALUE_HOLDS_LIST (value1) ? VALUE_LIST_SIZE (value1) : 1);
  value2_length =
      (GST_VALUE_HOLDS_LIST (value2) ? VALUE_LIST_SIZE (value2) : 1);

  _gst_value_list_init (dest, value1_length + value2_length);
  vlist = VALUE_LIST_ARRAY (dest);
  vlist->len = value1_length + value2_length;

  if (GST_VALUE_HOLDS_LIST (value1)) {
    for (i = 0; i < value1_length; i++) {
      gst_value_init_and_copy (&vlist->fields[i], VALUE_LIST_GET_VALUE (value1,
              i));
    }
  } else {
    gst_value_init_and_copy (&vlist->fields[0], value1);
  }

  j = value1_length;
  skipped = 0;
  if (GST_VALUE_HOLDS_LIST (value2)) {
    for (i = 0; i < value2_length; i++) {
      skip = FALSE;
      src = VALUE_LIST_GET_VALUE (value2, i);
      for (k = 0; k < value1_length; k++) {
        if (gst_value_compare (&vlist->fields[k], src) == GST_VALUE_EQUAL) {
          skip = TRUE;
          skipped++;
          break;
        }
      }
      if (!skip) {
        gst_value_init_and_copy (&vlist->fields[j], src);
        j++;
      }
    }
  } else {
    skip = FALSE;
    for (k = 0; k < value1_length; k++) {
      if (gst_value_compare (&vlist->fields[k], value2) == GST_VALUE_EQUAL) {
        skip = TRUE;
        skipped++;
        break;
      }
    }
    if (!skip) {
      gst_value_init_and_copy (&vlist->fields[j], value2);
    }
  }
  if (skipped) {
    guint new_size = value1_length + (value2_length - skipped);

    if (new_size > 1) {
      /* shrink list */
      vlist->len = new_size;
    } else {
      GValue single_dest;

      /* size is 1, take single value in list and make it new dest */
      single_dest = vlist->fields[0];

      /* clean up old value allocations: must set array size to 0, because
       * allocated values are not inited meaning g_value_unset() will not
       * work on them */
      vlist->len = 0;
      g_value_unset (dest);

      /* the single value is our new result */
      *dest = single_dest;
    }
  }
}

/**
 * gst_value_list_get_size:
 * @value: a #GValue of type #GST_TYPE_LIST
 *
 * Gets the number of values contained in @value.
 *
 * Returns: the number of values
 */
guint
gst_value_list_get_size (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_LIST (value), 0);

  return VALUE_LIST_SIZE (value);
}

/**
 * gst_value_list_get_value:
 * @value: a #GValue of type #GST_TYPE_LIST
 * @index: index of value to get from the list
 *
 * Gets the value that is a member of the list contained in @value and
 * has the index @index.
 *
 * Returns: (transfer none): the value at the given index
 */
const GValue *
gst_value_list_get_value (const GValue * value, guint index)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_LIST (value), NULL);
  g_return_val_if_fail (index < VALUE_LIST_SIZE (value), NULL);

  return VALUE_LIST_GET_VALUE (value, index);
}

/**
 * gst_value_array_append_value:
 * @value: a #GValue of type #GST_TYPE_ARRAY
 * @append_value: the value to append
 *
 * Appends @append_value to the GstValueArray in @value.
 */
void
gst_value_array_append_value (GValue * value, const GValue * append_value)
{
  GValue val = { 0, };

  g_return_if_fail (GST_VALUE_HOLDS_ARRAY (value));
  g_return_if_fail (G_IS_VALUE (append_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          append_value));

  gst_value_init_and_copy (&val, append_value);
  _gst_value_list_append_val (VALUE_LIST_ARRAY (value), &val);
}

static inline void
_gst_value_array_append_and_take_value (GValue * value, GValue * append_value)
{
  _gst_value_list_append_val (VALUE_LIST_ARRAY (value), append_value);
  memset (append_value, 0, sizeof (GValue));
}

/**
 * gst_value_array_append_and_take_value:
 * @value: a #GValue of type #GST_TYPE_ARRAY
 * @append_value: (transfer full): the value to append
 *
 * Appends @append_value to the GstValueArray in @value.
 *
 * Since: 1.2
 */
void
gst_value_array_append_and_take_value (GValue * value, GValue * append_value)
{
  g_return_if_fail (GST_VALUE_HOLDS_ARRAY (value));
  g_return_if_fail (G_IS_VALUE (append_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          append_value));

  _gst_value_array_append_and_take_value (value, append_value);
}

/**
 * gst_value_array_prepend_value:
 * @value: a #GValue of type #GST_TYPE_ARRAY
 * @prepend_value: the value to prepend
 *
 * Prepends @prepend_value to the GstValueArray in @value.
 */
void
gst_value_array_prepend_value (GValue * value, const GValue * prepend_value)
{
  GValue val = { 0, };

  g_return_if_fail (GST_VALUE_HOLDS_ARRAY (value));
  g_return_if_fail (G_IS_VALUE (prepend_value));
  g_return_if_fail (gst_value_list_or_array_are_compatible (value,
          prepend_value));

  gst_value_init_and_copy (&val, prepend_value);
  _gst_value_list_prepend_val (VALUE_LIST_ARRAY (value), &val);
}

/**
 * gst_value_array_get_size:
 * @value: a #GValue of type #GST_TYPE_ARRAY
 *
 * Gets the number of values contained in @value.
 *
 * Returns: the number of values
 */
guint
gst_value_array_get_size (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_ARRAY (value), 0);

  return VALUE_LIST_SIZE (value);
}

/**
 * gst_value_array_get_value:
 * @value: a #GValue of type #GST_TYPE_ARRAY
 * @index: index of value to get from the array
 *
 * Gets the value that is a member of the array contained in @value and
 * has the index @index.
 *
 * Returns: (transfer none): the value at the given index
 */
const GValue *
gst_value_array_get_value (const GValue * value, guint index)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_ARRAY (value), NULL);
  g_return_val_if_fail (index < VALUE_LIST_SIZE (value), NULL);

  return VALUE_LIST_GET_VALUE (value, index);
}

static void
gst_value_transform_list_string (const GValue * src_value, GValue * dest_value)
{
  gst_value_transform_any_list_string (src_value, dest_value, "{ ", " }");
}

static void
gst_value_transform_array_string (const GValue * src_value, GValue * dest_value)
{
  gst_value_transform_any_list_string (src_value, dest_value, "< ", " >");
}

static void
gst_value_transform_g_value_array_string (const GValue * src_value,
    GValue * dest_value)
{
  _gst_value_transform_g_value_array_string (src_value, dest_value, "< ", " >");
}

static void
gst_value_transform_g_value_array_any_list (const GValue * src_value,
    GValue * dest_value)
{
  const GValueArray *varray;
  GstValueList *vlist;
  gint i;

  varray = g_value_get_boxed (src_value);

  /* GLib will unset the value, memset to 0 the data instead of doing a proper
   * reset. That's why we need to allocate the array here */
  vlist = dest_value->data[0].v_pointer =
      _gst_value_list_new (varray->n_values);

  for (i = 0; i < varray->n_values; i++) {
    GValue val = G_VALUE_INIT;
    gst_value_init_and_copy (&val, &varray->values[i]);
    _gst_value_list_append_val (vlist, &val);
  }
}

static void
gst_value_transform_any_list_g_value_array (const GValue * src_value,
    GValue * dest_value)
{
  GValueArray *varray;
  GstValueList *vlist;
  gint i;

  vlist = VALUE_LIST_ARRAY (src_value);
  varray = g_value_array_new (vlist->len);

  for (i = 0; i < vlist->len; i++)
    g_value_array_append (varray, &vlist->fields[i]);

  g_value_take_boxed (dest_value, varray);
}

/* Do an unordered compare of the contents of a list */
static gint
gst_value_compare_value_list (const GValue * value1, const GValue * value2)
{
  guint i, j;
  GstValueList *vlist1 = VALUE_LIST_ARRAY (value1);
  GstValueList *vlist2 = VALUE_LIST_ARRAY (value2);
  GValue *v1;
  GValue *v2;
  gint len, to_remove;
  guint8 *removed;
  GstValueCompareFunc compare;

  /* get length and do initial length check. */
  len = vlist1->len;
  if (len != vlist2->len)
    return GST_VALUE_UNORDERED;

  /* Empty lists are equal */
  if (len == 0)
    return GST_VALUE_EQUAL;

  /* We know lists are not empty. do sanity check on first values */
  if (G_VALUE_TYPE (&vlist1->fields[0]) != G_VALUE_TYPE (&vlist2->fields[0]))
    return GST_VALUE_UNORDERED;

  /* Get the compare function */
  if (!(compare = gst_value_get_compare_func (&vlist1->fields[0])))
    return GST_VALUE_UNORDERED;

  /* place to mark removed value indices of array2 */
  removed = g_newa (guint8, len);
  memset (removed, 0, len);
  to_remove = len;

  /* loop over array1, all items should be in array2. When we find an
   * item in array2, remove it from array2 by marking it as removed */
  for (i = 0; i < len; i++) {
    v1 = &vlist1->fields[i];

    for (j = 0; j < len; j++) {
      /* item is removed, we can skip it */
      if (removed[j])
        continue;
      v2 = &vlist2->fields[j];
      /* Note: compare function can be called directly since we know the types
       * are identical */
      if (compare (v1, v2) == GST_VALUE_EQUAL) {
        /* mark item as removed now that we found it in array2 and
         * decrement the number of remaining items in array2. */
        removed[j] = 1;
        to_remove--;
        break;
      }
    }
    /* item in array1 and not in array2, UNORDERED */
    if (j == len)
      return GST_VALUE_UNORDERED;
  }
  /* if not all items were removed, array2 contained something not in array1 */
  if (to_remove != 0)
    return GST_VALUE_UNORDERED;

  /* arrays are equal */
  return GST_VALUE_EQUAL;
}

/* Perform an ordered comparison of the contents of an array */
static gint
gst_value_compare_value_array (const GValue * value1, const GValue * value2)
{
  guint i;
  GstValueList *vlist1 = VALUE_LIST_ARRAY (value1);
  GstValueList *vlist2 = VALUE_LIST_ARRAY (value2);
  guint len = vlist1->len;
  GValue *v1;
  GValue *v2;

  if (len != vlist2->len)
    return GST_VALUE_UNORDERED;

  for (i = 0; i < len; i++) {
    v1 = &vlist1->fields[i];
    v2 = &vlist2->fields[i];
    if (gst_value_compare (v1, v2) != GST_VALUE_EQUAL)
      return GST_VALUE_UNORDERED;
  }

  return GST_VALUE_EQUAL;
}

static gint
gst_value_compare_g_value_array (const GValue * value1, const GValue * value2)
{
  guint i;
  GValueArray *array1 = value1->data[0].v_pointer;
  GValueArray *array2 = value2->data[0].v_pointer;
  guint len = array1 ? array1->n_values : 0;
  GValue *v1;
  GValue *v2;

  if (len != (array2 ? array2->n_values : 0))
    return GST_VALUE_UNORDERED;

  for (i = 0; i < len; i++) {
    v1 = g_value_array_get_nth (array1, i);
    v2 = g_value_array_get_nth (array2, i);
    if (gst_value_compare (v1, v2) != GST_VALUE_EQUAL)
      return GST_VALUE_UNORDERED;
  }

  return GST_VALUE_EQUAL;
}

static gchar *
gst_value_serialize_value_list (const GValue * value)
{
  return _priv_gst_value_serialize_any_list (value, "{ ", " }", TRUE,
      GST_SERIALIZE_FLAG_BACKWARD_COMPAT);
}

static gboolean
gst_value_deserialize_value_list (GValue * dest, const gchar * s,
    GParamSpec * pspec)
{
  gchar *s2 = (gchar *) s;
  return _priv_gst_value_parse_list (s2, &s2, dest, G_TYPE_INVALID, pspec);
}

static gchar *
gst_value_serialize_value_array (const GValue * value)
{
  return _priv_gst_value_serialize_any_list (value, "< ", " >", TRUE,
      GST_SERIALIZE_FLAG_BACKWARD_COMPAT);
}

static gboolean
gst_value_deserialize_value_array (GValue * dest, const gchar * s,
    GParamSpec * pspec)
{
  gchar *s2 = (gchar *) s;
  return _priv_gst_value_parse_array (s2, &s2, dest, G_TYPE_INVALID, pspec);
}

static gchar *
gst_value_serialize_g_value_array (const GValue * value)
{
  return _gst_value_serialize_g_value_array (value, "< ", " >");
}

static gboolean
gst_value_deserialize_g_value_array (GValue * dest, const gchar * s)
{
  g_warning ("gst_value_deserialize_g_value_array: unimplemented");
  return FALSE;
}

/*************
 * int range *
 *
 * Values in the range are defined as any value greater or equal
 * to min*step, AND lesser or equal to max*step.
 * For step == 1, this falls back to the traditional range semantics.
 *
 * data[0] = (min << 32) | (max)
 * data[1] = step
 *
 *************/

#define INT_RANGE_MIN(v) ((gint) (((v)->data[0].v_uint64) >> 32))
#define INT_RANGE_MAX(v) ((gint) (((v)->data[0].v_uint64) & 0xffffffff))
#define INT_RANGE_STEP(v) ((v)->data[1].v_int)

static void
gst_value_init_int_range (GValue * value)
{
  G_STATIC_ASSERT (sizeof (gint) <= 2 * sizeof (guint64));

  value->data[0].v_uint64 = 0;
  value->data[1].v_int = 1;
}

static void
gst_value_copy_int_range (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_uint64 = src_value->data[0].v_uint64;
  dest_value->data[1].v_int = src_value->data[1].v_int;
}

static gchar *
gst_value_collect_int_range (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  g_return_val_if_fail (n_collect_values == 2,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[0].v_int < collect_values[1].v_int,
      g_strdup_printf ("range start is not smaller than end for `%s'",
          G_VALUE_TYPE_NAME (value)));

  gst_value_set_int_range_step (value, collect_values[0].v_int,
      collect_values[1].v_int, 1);

  return NULL;
}

static gchar *
gst_value_lcopy_int_range (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  guint32 *int_range_start = collect_values[0].v_pointer;
  guint32 *int_range_end = collect_values[1].v_pointer;

  g_return_val_if_fail (int_range_start != NULL,
      g_strdup_printf ("start value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (int_range_end != NULL,
      g_strdup_printf ("end value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));

  *int_range_start = INT_RANGE_MIN (value);
  *int_range_end = INT_RANGE_MAX (value);

  return NULL;
}

/**
 * gst_value_set_int_range_step:
 * @value: a GValue initialized to GST_TYPE_INT_RANGE
 * @start: the start of the range
 * @end: the end of the range
 * @step: the step of the range
 *
 * Sets @value to the range specified by @start, @end and @step.
 */
void
gst_value_set_int_range_step (GValue * value, gint start, gint end, gint step)
{
  guint64 sstart, sstop;

  g_return_if_fail (GST_VALUE_HOLDS_INT_RANGE (value));
  g_return_if_fail (start < end);
  g_return_if_fail (step > 0);
  g_return_if_fail (start % step == 0);
  g_return_if_fail (end % step == 0);

  sstart = (guint) (start / step);
  sstop = (guint) (end / step);
  value->data[0].v_uint64 = (sstart << 32) | sstop;
  value->data[1].v_int = step;
}

/**
 * gst_value_set_int_range:
 * @value: a GValue initialized to GST_TYPE_INT_RANGE
 * @start: the start of the range
 * @end: the end of the range
 *
 * Sets @value to the range specified by @start and @end.
 */
void
gst_value_set_int_range (GValue * value, gint start, gint end)
{
  gst_value_set_int_range_step (value, start, end, 1);
}

/**
 * gst_value_get_int_range_min:
 * @value: a GValue initialized to GST_TYPE_INT_RANGE
 *
 * Gets the minimum of the range specified by @value.
 *
 * Returns: the minimum of the range
 */
gint
gst_value_get_int_range_min (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT_RANGE (value), 0);

  return INT_RANGE_MIN (value) * INT_RANGE_STEP (value);
}

/**
 * gst_value_get_int_range_max:
 * @value: a GValue initialized to GST_TYPE_INT_RANGE
 *
 * Gets the maximum of the range specified by @value.
 *
 * Returns: the maximum of the range
 */
gint
gst_value_get_int_range_max (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT_RANGE (value), 0);

  return INT_RANGE_MAX (value) * INT_RANGE_STEP (value);
}

/**
 * gst_value_get_int_range_step:
 * @value: a GValue initialized to GST_TYPE_INT_RANGE
 *
 * Gets the step of the range specified by @value.
 *
 * Returns: the step of the range
 */
gint
gst_value_get_int_range_step (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT_RANGE (value), 0);

  return INT_RANGE_STEP (value);
}

static void
gst_value_transform_int_range_string (const GValue * src_value,
    GValue * dest_value)
{
  if (INT_RANGE_STEP (src_value) == 1)
    dest_value->data[0].v_pointer = g_strdup_printf ("[%d,%d]",
        INT_RANGE_MIN (src_value), INT_RANGE_MAX (src_value));
  else
    dest_value->data[0].v_pointer = g_strdup_printf ("[%d,%d,%d]",
        INT_RANGE_MIN (src_value) * INT_RANGE_STEP (src_value),
        INT_RANGE_MAX (src_value) * INT_RANGE_STEP (src_value),
        INT_RANGE_STEP (src_value));
}

static gint
gst_value_compare_int_range (const GValue * value1, const GValue * value2)
{
#if 0
  /* Compare the ranges. (Kept for clarity for the below comparision) */
  if (INT_RANGE_MIN (value1) != INT_RANGE_MIN (value2) ||
      INT_RANGE_MAX (value1) != INT_RANGE_MAX (value2))
    return GST_VALUE_UNORDERED;
#else
  /* The MIN and MAX of the range are actually stored packed into one 64bit
   * value. We can therefore compare them directly */
  if (value1->data[0].v_uint64 != value2->data[0].v_uint64)
    return GST_VALUE_UNORDERED;
#endif

  /* The extents are equal */
  /* If there is only one value (min == max), we ignore the step for
   * comparison */
  if (INT_RANGE_MIN (value1) == INT_RANGE_MAX (value1))
    return GST_VALUE_EQUAL;

  /* Else the ranges are only equal if their step is also equal */
  if (INT_RANGE_STEP (value1) == INT_RANGE_STEP (value2))
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_int_range (const GValue * value)
{
  if (INT_RANGE_STEP (value) == 1)
    return g_strdup_printf ("[ %d, %d ]", INT_RANGE_MIN (value),
        INT_RANGE_MAX (value));
  else
    return g_strdup_printf ("[ %d, %d, %d ]",
        INT_RANGE_MIN (value) * INT_RANGE_STEP (value),
        INT_RANGE_MAX (value) * INT_RANGE_STEP (value), INT_RANGE_STEP (value));
}

static gboolean
gst_value_deserialize_int_range (GValue * dest, const gchar * s)
{
  g_warning ("unimplemented");
  return FALSE;
}

/***************
 * int64 range *
 *
 * Values in the range are defined as any value greater or equal
 * to min*step, AND lesser or equal to max*step.
 * For step == 1, this falls back to the traditional range semantics.
 ***************/

#define INT64_RANGE_MIN(v) (((gint64 *)((v)->data[0].v_pointer))[0])
#define INT64_RANGE_MAX(v) (((gint64 *)((v)->data[0].v_pointer))[1])
#define INT64_RANGE_STEP(v) (((gint64 *)((v)->data[0].v_pointer))[2])

static void
gst_value_init_int64_range (GValue * value)
{
  gint64 *vals = g_malloc0 (3 * sizeof (gint64));
  value->data[0].v_pointer = vals;
  INT64_RANGE_MIN (value) = 0;
  INT64_RANGE_MAX (value) = 0;
  INT64_RANGE_STEP (value) = 1;
}

static void
gst_value_free_int64_range (GValue * value)
{
  g_return_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value));
  g_free (value->data[0].v_pointer);
  value->data[0].v_pointer = NULL;
}

static void
gst_value_copy_int64_range (const GValue * src_value, GValue * dest_value)
{
  gint64 *vals = (gint64 *) dest_value->data[0].v_pointer;
  gint64 *src_vals = (gint64 *) src_value->data[0].v_pointer;

  if (vals == NULL) {
    gst_value_init_int64_range (dest_value);
  }

  if (src_vals != NULL) {
    INT64_RANGE_MIN (dest_value) = INT64_RANGE_MIN (src_value);
    INT64_RANGE_MAX (dest_value) = INT64_RANGE_MAX (src_value);
    INT64_RANGE_STEP (dest_value) = INT64_RANGE_STEP (src_value);
  }
}

static gchar *
gst_value_collect_int64_range (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  gint64 *vals = value->data[0].v_pointer;

  g_return_val_if_fail (n_collect_values == 2,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));

  g_return_val_if_fail (collect_values[0].v_int64 < collect_values[1].v_int64,
      g_strdup_printf ("range start is not smaller than end for `%s'",
          G_VALUE_TYPE_NAME (value)));

  if (vals == NULL) {
    gst_value_init_int64_range (value);
  }

  gst_value_set_int64_range_step (value, collect_values[0].v_int64,
      collect_values[1].v_int64, 1);

  return NULL;
}

static gchar *
gst_value_lcopy_int64_range (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  guint64 *int_range_start = collect_values[0].v_pointer;
  guint64 *int_range_end = collect_values[1].v_pointer;
  guint64 *int_range_step = collect_values[2].v_pointer;
  gint64 *vals = (gint64 *) value->data[0].v_pointer;

  g_return_val_if_fail (int_range_start != NULL,
      g_strdup_printf ("start value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (int_range_end != NULL,
      g_strdup_printf ("end value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (int_range_step != NULL,
      g_strdup_printf ("step value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));

  g_return_val_if_fail (vals != NULL,
      g_strdup_printf ("Uninitialised `%s' passed", G_VALUE_TYPE_NAME (value)));

  *int_range_start = INT64_RANGE_MIN (value);
  *int_range_end = INT64_RANGE_MAX (value);
  *int_range_step = INT64_RANGE_STEP (value);

  return NULL;
}

/**
 * gst_value_set_int64_range_step:
 * @value: a GValue initialized to GST_TYPE_INT64_RANGE
 * @start: the start of the range
 * @end: the end of the range
 * @step: the step of the range
 *
 * Sets @value to the range specified by @start, @end and @step.
 */
void
gst_value_set_int64_range_step (GValue * value, gint64 start, gint64 end,
    gint64 step)
{
  g_return_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value));
  g_return_if_fail (start < end);
  g_return_if_fail (step > 0);
  g_return_if_fail (start % step == 0);
  g_return_if_fail (end % step == 0);

  INT64_RANGE_MIN (value) = start / step;
  INT64_RANGE_MAX (value) = end / step;
  INT64_RANGE_STEP (value) = step;
}

/**
 * gst_value_set_int64_range:
 * @value: a GValue initialized to GST_TYPE_INT64_RANGE
 * @start: the start of the range
 * @end: the end of the range
 *
 * Sets @value to the range specified by @start and @end.
 */
void
gst_value_set_int64_range (GValue * value, gint64 start, gint64 end)
{
  gst_value_set_int64_range_step (value, start, end, 1);
}

/**
 * gst_value_get_int64_range_min:
 * @value: a GValue initialized to GST_TYPE_INT64_RANGE
 *
 * Gets the minimum of the range specified by @value.
 *
 * Returns: the minimum of the range
 */
gint64
gst_value_get_int64_range_min (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value), 0);

  return INT64_RANGE_MIN (value) * INT64_RANGE_STEP (value);
}

/**
 * gst_value_get_int64_range_max:
 * @value: a GValue initialized to GST_TYPE_INT64_RANGE
 *
 * Gets the maximum of the range specified by @value.
 *
 * Returns: the maximum of the range
 */
gint64
gst_value_get_int64_range_max (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value), 0);

  return INT64_RANGE_MAX (value) * INT64_RANGE_STEP (value);
}

/**
 * gst_value_get_int64_range_step:
 * @value: a GValue initialized to GST_TYPE_INT64_RANGE
 *
 * Gets the step of the range specified by @value.
 *
 * Returns: the step of the range
 */
gint64
gst_value_get_int64_range_step (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value), 0);

  return INT64_RANGE_STEP (value);
}

static void
gst_value_transform_int64_range_string (const GValue * src_value,
    GValue * dest_value)
{
  if (INT64_RANGE_STEP (src_value) == 1)
    dest_value->data[0].v_pointer =
        g_strdup_printf ("(gint64)[%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT "]",
        INT64_RANGE_MIN (src_value), INT64_RANGE_MAX (src_value));
  else
    dest_value->data[0].v_pointer =
        g_strdup_printf ("(gint64)[%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT
        ",%" G_GINT64_FORMAT "]",
        INT64_RANGE_MIN (src_value) * INT64_RANGE_STEP (src_value),
        INT64_RANGE_MAX (src_value) * INT64_RANGE_STEP (src_value),
        INT64_RANGE_STEP (src_value));
}

static gint
gst_value_compare_int64_range (const GValue * value1, const GValue * value2)
{
  /* Compare the ranges. */
  if (INT64_RANGE_MIN (value1) != INT64_RANGE_MIN (value2) ||
      INT64_RANGE_MAX (value1) != INT64_RANGE_MAX (value2))
    return GST_VALUE_UNORDERED;

  /* The extents are equal */
  /* If there is only one value (min == max), we ignore the step for
   * comparison */
  if (INT64_RANGE_MIN (value1) == INT64_RANGE_MAX (value1))
    return GST_VALUE_EQUAL;

  /* Else the ranges are only equal if their step is also equal */
  if (INT64_RANGE_STEP (value1) == INT64_RANGE_STEP (value2))
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_int64_range (const GValue * value)
{
  if (INT64_RANGE_STEP (value) == 1)
    return g_strdup_printf ("[ %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT " ]",
        INT64_RANGE_MIN (value), INT64_RANGE_MAX (value));
  else
    return g_strdup_printf ("[ %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT ", %"
        G_GINT64_FORMAT " ]",
        INT64_RANGE_MIN (value) * INT64_RANGE_STEP (value),
        INT64_RANGE_MAX (value) * INT64_RANGE_STEP (value),
        INT64_RANGE_STEP (value));
}

static gboolean
gst_value_deserialize_int64_range (GValue * dest, const gchar * s)
{
  g_warning ("unimplemented");
  return FALSE;
}

/****************
 * double range *
 ****************/

static void
gst_value_init_double_range (GValue * value)
{
  value->data[0].v_double = 0;
  value->data[1].v_double = 0;
}

static void
gst_value_copy_double_range (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_double = src_value->data[0].v_double;
  dest_value->data[1].v_double = src_value->data[1].v_double;
}

static gchar *
gst_value_collect_double_range (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  g_return_val_if_fail (n_collect_values == 2,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[0].v_double < collect_values[1].v_double,
      g_strdup_printf ("range start is not smaller than end for `%s'",
          G_VALUE_TYPE_NAME (value)));

  value->data[0].v_double = collect_values[0].v_double;
  value->data[1].v_double = collect_values[1].v_double;

  return NULL;
}

static gchar *
gst_value_lcopy_double_range (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  gdouble *double_range_start = collect_values[0].v_pointer;
  gdouble *double_range_end = collect_values[1].v_pointer;

  g_return_val_if_fail (double_range_start != NULL,
      g_strdup_printf ("start value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (double_range_end != NULL,
      g_strdup_printf ("end value location for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));

  *double_range_start = value->data[0].v_double;
  *double_range_end = value->data[1].v_double;

  return NULL;
}

/**
 * gst_value_set_double_range:
 * @value: a GValue initialized to GST_TYPE_DOUBLE_RANGE
 * @start: the start of the range
 * @end: the end of the range
 *
 * Sets @value to the range specified by @start and @end.
 */
void
gst_value_set_double_range (GValue * value, gdouble start, gdouble end)
{
  g_return_if_fail (GST_VALUE_HOLDS_DOUBLE_RANGE (value));
  g_return_if_fail (start < end);

  value->data[0].v_double = start;
  value->data[1].v_double = end;
}

/**
 * gst_value_get_double_range_min:
 * @value: a GValue initialized to GST_TYPE_DOUBLE_RANGE
 *
 * Gets the minimum of the range specified by @value.
 *
 * Returns: the minimum of the range
 */
gdouble
gst_value_get_double_range_min (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_DOUBLE_RANGE (value), 0);

  return value->data[0].v_double;
}

/**
 * gst_value_get_double_range_max:
 * @value: a GValue initialized to GST_TYPE_DOUBLE_RANGE
 *
 * Gets the maximum of the range specified by @value.
 *
 * Returns: the maximum of the range
 */
gdouble
gst_value_get_double_range_max (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_DOUBLE_RANGE (value), 0);

  return value->data[1].v_double;
}

static void
gst_value_transform_double_range_string (const GValue * src_value,
    GValue * dest_value)
{
  gchar s1[G_ASCII_DTOSTR_BUF_SIZE], s2[G_ASCII_DTOSTR_BUF_SIZE];

  dest_value->data[0].v_pointer = g_strdup_printf ("[%s,%s]",
      g_ascii_dtostr (s1, G_ASCII_DTOSTR_BUF_SIZE,
          src_value->data[0].v_double),
      g_ascii_dtostr (s2, G_ASCII_DTOSTR_BUF_SIZE,
          src_value->data[1].v_double));
}

static gint
gst_value_compare_double_range (const GValue * value1, const GValue * value2)
{
  if (value2->data[0].v_double == value1->data[0].v_double &&
      value2->data[1].v_double == value1->data[1].v_double)
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_double_range (const GValue * value)
{
  gchar d1[G_ASCII_DTOSTR_BUF_SIZE];
  gchar d2[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (d1, G_ASCII_DTOSTR_BUF_SIZE, value->data[0].v_double);
  g_ascii_dtostr (d2, G_ASCII_DTOSTR_BUF_SIZE, value->data[1].v_double);
  return g_strdup_printf ("[ %s, %s ]", d1, d2);
}

static gboolean
gst_value_deserialize_double_range (GValue * dest, const gchar * s)
{
  g_warning ("unimplemented");
  return FALSE;
}

/****************
 * fraction range *
 ****************/

static void
gst_value_init_fraction_range (GValue * value)
{
  GValue *vals;
  GType ftype;

  ftype = GST_TYPE_FRACTION;

  value->data[0].v_pointer = vals = g_malloc0 (2 * sizeof (GValue));
  g_value_init (&vals[0], ftype);
  g_value_init (&vals[1], ftype);
}

static void
gst_value_free_fraction_range (GValue * value)
{
  GValue *vals = (GValue *) value->data[0].v_pointer;

  if (vals != NULL) {
    /* we know the two values contain fractions without internal allocs */
    /* g_value_unset (&vals[0]); */
    /* g_value_unset (&vals[1]); */
    g_free (vals);
    value->data[0].v_pointer = NULL;
  }
}

static void
gst_value_copy_fraction_range (const GValue * src_value, GValue * dest_value)
{
  GValue *vals = (GValue *) dest_value->data[0].v_pointer;
  GValue *src_vals = (GValue *) src_value->data[0].v_pointer;

  if (vals == NULL) {
    gst_value_init_fraction_range (dest_value);
    vals = dest_value->data[0].v_pointer;
  }
  if (src_vals != NULL) {
    g_value_copy (&src_vals[0], &vals[0]);
    g_value_copy (&src_vals[1], &vals[1]);
  }
}

static gchar *
gst_value_collect_fraction_range (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  GValue *vals = (GValue *) value->data[0].v_pointer;

  g_return_val_if_fail (n_collect_values == 4,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[1].v_int != 0,
      g_strdup_printf ("passed '0' as first denominator for `%s'",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[3].v_int != 0,
      g_strdup_printf ("passed '0' as second denominator for `%s'",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (gst_util_fraction_compare (collect_values[0].v_int,
          collect_values[1].v_int, collect_values[2].v_int,
          collect_values[3].v_int) < 0,
      g_strdup_printf ("range start is not smaller than end for `%s'",
          G_VALUE_TYPE_NAME (value)));

  if (vals == NULL) {
    gst_value_init_fraction_range (value);
    vals = value->data[0].v_pointer;
  }

  gst_value_set_fraction (&vals[0], collect_values[0].v_int,
      collect_values[1].v_int);
  gst_value_set_fraction (&vals[1], collect_values[2].v_int,
      collect_values[3].v_int);

  return NULL;
}

static gchar *
gst_value_lcopy_fraction_range (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  gint i;
  gint *dest_values[4];
  GValue *vals = (GValue *) value->data[0].v_pointer;

  g_return_val_if_fail (n_collect_values == 4,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (vals != NULL,
      g_strdup_printf ("Uninitialised `%s' passed", G_VALUE_TYPE_NAME (value)));

  for (i = 0; i < 4; i++) {
    g_return_val_if_fail (collect_values[i].v_pointer != NULL,
        g_strdup_printf ("value location for `%s' passed as NULL",
            G_VALUE_TYPE_NAME (value)));

    dest_values[i] = collect_values[i].v_pointer;
  }

  dest_values[0][0] = gst_value_get_fraction_numerator (&vals[0]);
  dest_values[1][0] = gst_value_get_fraction_denominator (&vals[0]);
  dest_values[2][0] = gst_value_get_fraction_numerator (&vals[1]);
  dest_values[3][0] = gst_value_get_fraction_denominator (&vals[1]);
  return NULL;
}

/**
 * gst_value_set_fraction_range:
 * @value: a GValue initialized to GST_TYPE_FRACTION_RANGE
 * @start: the start of the range (a GST_TYPE_FRACTION GValue)
 * @end: the end of the range (a GST_TYPE_FRACTION GValue)
 *
 * Sets @value to the range specified by @start and @end.
 */
void
gst_value_set_fraction_range (GValue * value, const GValue * start,
    const GValue * end)
{
  GValue *vals;

  g_return_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (value));
  g_return_if_fail (GST_VALUE_HOLDS_FRACTION (start));
  g_return_if_fail (GST_VALUE_HOLDS_FRACTION (end));
  g_return_if_fail (gst_util_fraction_compare (start->data[0].v_int,
          start->data[1].v_int, end->data[0].v_int, end->data[1].v_int) < 0);

  vals = (GValue *) value->data[0].v_pointer;
  if (vals == NULL) {
    gst_value_init_fraction_range (value);
    vals = value->data[0].v_pointer;
  }
  g_value_copy (start, &vals[0]);
  g_value_copy (end, &vals[1]);
}

/**
 * gst_value_set_fraction_range_full:
 * @value: a GValue initialized to GST_TYPE_FRACTION_RANGE
 * @numerator_start: the numerator start of the range
 * @denominator_start: the denominator start of the range
 * @numerator_end: the numerator end of the range
 * @denominator_end: the denominator end of the range
 *
 * Sets @value to the range specified by @numerator_start/@denominator_start
 * and @numerator_end/@denominator_end.
 */
void
gst_value_set_fraction_range_full (GValue * value,
    gint numerator_start, gint denominator_start,
    gint numerator_end, gint denominator_end)
{
  GValue start = { 0 };
  GValue end = { 0 };

  g_return_if_fail (value != NULL);
  g_return_if_fail (denominator_start != 0);
  g_return_if_fail (denominator_end != 0);
  g_return_if_fail (gst_util_fraction_compare (numerator_start,
          denominator_start, numerator_end, denominator_end) < 0);

  g_value_init (&start, GST_TYPE_FRACTION);
  g_value_init (&end, GST_TYPE_FRACTION);

  gst_value_set_fraction (&start, numerator_start, denominator_start);
  gst_value_set_fraction (&end, numerator_end, denominator_end);
  gst_value_set_fraction_range (value, &start, &end);

  /* we know the two values contain fractions without internal allocs */
  /* g_value_unset (&start); */
  /* g_value_unset (&end);   */
}

/* FIXME 2.0: Don't leak the internal representation of fraction
 * ranges but instead return the numerator and denominator
 * separately.
 * This would allow to store fraction ranges as
 *  data[0] = (min_n << 32) | (min_d)
 *  data[1] = (max_n << 32) | (max_d)
 * without requiring an additional allocation for each value.
 */

/**
 * gst_value_get_fraction_range_min:
 * @value: a GValue initialized to GST_TYPE_FRACTION_RANGE
 *
 * Gets the minimum of the range specified by @value.
 *
 * Returns: (nullable): the minimum of the range
 */
const GValue *
gst_value_get_fraction_range_min (const GValue * value)
{
  GValue *vals;

  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (value), NULL);

  vals = (GValue *) value->data[0].v_pointer;
  if (vals != NULL) {
    return &vals[0];
  }

  return NULL;
}

/**
 * gst_value_get_fraction_range_max:
 * @value: a GValue initialized to GST_TYPE_FRACTION_RANGE
 *
 * Gets the maximum of the range specified by @value.
 *
 * Returns: (nullable): the maximum of the range
 */
const GValue *
gst_value_get_fraction_range_max (const GValue * value)
{
  GValue *vals;

  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (value), NULL);

  vals = (GValue *) value->data[0].v_pointer;
  if (vals != NULL) {
    return &vals[1];
  }

  return NULL;
}

static gchar *
gst_value_serialize_fraction_range (const GValue * value)
{
  GValue *vals = (GValue *) value->data[0].v_pointer;
  gchar *retval;

  if (vals == NULL) {
    retval = g_strdup ("[ 0/1, 0/1 ]");
  } else {
    gchar *start, *end;

    start = gst_value_serialize_fraction (&vals[0]);
    end = gst_value_serialize_fraction (&vals[1]);

    retval = g_strdup_printf ("[ %s, %s ]", start, end);
    g_free (start);
    g_free (end);
  }

  return retval;
}

static void
gst_value_transform_fraction_range_string (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_pointer =
      gst_value_serialize_fraction_range (src_value);
}

static gint
gst_value_compare_fraction_range (const GValue * value1, const GValue * value2)
{
  GValue *vals1, *vals2;

  if (value2->data[0].v_pointer == value1->data[0].v_pointer)
    return GST_VALUE_EQUAL;     /* Only possible if both are NULL */

  if (value2->data[0].v_pointer == NULL || value1->data[0].v_pointer == NULL)
    return GST_VALUE_UNORDERED;

  vals1 = (GValue *) value1->data[0].v_pointer;
  vals2 = (GValue *) value2->data[0].v_pointer;
  if (gst_value_compare_fraction (&vals1[0], &vals2[0]) == GST_VALUE_EQUAL &&
      gst_value_compare_fraction (&vals1[1], &vals2[1]) == GST_VALUE_EQUAL)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

static gboolean
gst_value_deserialize_fraction_range (GValue * dest, const gchar * s)
{
  g_warning ("unimplemented");
  return FALSE;
}

/***********
 * GstCaps *
 ***********/

/**
 * gst_value_set_caps:
 * @value: a GValue initialized to GST_TYPE_CAPS
 * @caps: (transfer none): the caps to set the value to
 *
 * Sets the contents of @value to @caps. A reference to the
 * provided @caps will be taken by the @value.
 */
void
gst_value_set_caps (GValue * value, const GstCaps * caps)
{
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (G_VALUE_TYPE (value) == GST_TYPE_CAPS);
  g_return_if_fail (caps == NULL || GST_IS_CAPS (caps));

  g_value_set_boxed (value, caps);
}

/**
 * gst_value_get_caps:
 * @value: a GValue initialized to GST_TYPE_CAPS
 *
 * Gets the contents of @value. The reference count of the returned
 * #GstCaps will not be modified, therefore the caller must take one
 * before getting rid of the @value.
 *
 * Returns: (transfer none): the contents of @value
 */
const GstCaps *
gst_value_get_caps (const GValue * value)
{
  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  g_return_val_if_fail (G_VALUE_TYPE (value) == GST_TYPE_CAPS, NULL);

  return (GstCaps *) g_value_get_boxed (value);
}

static gint
gst_value_compare_caps (const GValue * value1, const GValue * value2)
{
  GstCaps *caps1 = GST_CAPS (gst_value_get_caps (value1));
  GstCaps *caps2 = GST_CAPS (gst_value_get_caps (value2));

  if (caps1 == caps2)
    return GST_VALUE_EQUAL;

  if (!caps1 || !caps2)
    return GST_VALUE_UNORDERED;

  if (gst_caps_is_equal (caps1, caps2))
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_caps (const GValue * value)
{
  GstCaps *caps = g_value_get_boxed (value);
  return priv_gst_string_take_and_wrap (gst_caps_to_string (caps));
}

static gboolean
gst_value_deserialize_caps (GValue * dest, const gchar * s)
{
  GstCaps *caps;

  if (*s != '"') {
    /* this can happen if caps are ANY, EMPTY, or only contains a single
     * empty structure */
    caps = gst_caps_from_string (s);
  } else {
    gchar *str = gst_string_unwrap (s);

    if (G_UNLIKELY (!str))
      return FALSE;

    caps = gst_caps_from_string (str);
    g_free (str);
  }

  if (caps) {
    g_value_take_boxed (dest, caps);
    return TRUE;
  }
  return FALSE;
}

/********************************************
 * Serialization/deserialization of GValues *
 ********************************************/

static GstValueAbbreviation *
_priv_gst_value_get_abbrs (gint * n_abbrs)
{
  static GstValueAbbreviation *abbrs = NULL;
  static gsize num = 0;

  if (g_once_init_enter (&num)) {
    /* dynamically generate the array */
    gsize _num;
    GstValueAbbreviation dyn_abbrs[] = {
      {"int", G_TYPE_INT}
      ,
      {"i", G_TYPE_INT}
      ,
      {"uint", G_TYPE_UINT}
      ,
      {"u", G_TYPE_UINT}
      ,
      {"float", G_TYPE_FLOAT}
      ,
      {"f", G_TYPE_FLOAT}
      ,
      {"double", G_TYPE_DOUBLE}
      ,
      {"d", G_TYPE_DOUBLE}
      ,
      {"buffer", GST_TYPE_BUFFER}
      ,
      {"fraction", GST_TYPE_FRACTION}
      ,
      {"boolean", G_TYPE_BOOLEAN}
      ,
      {"bool", G_TYPE_BOOLEAN}
      ,
      {"b", G_TYPE_BOOLEAN}
      ,
      {"string", G_TYPE_STRING}
      ,
      {"str", G_TYPE_STRING}
      ,
      {"s", G_TYPE_STRING}
      ,
      {"structure", GST_TYPE_STRUCTURE}
      ,
      {"date", G_TYPE_DATE}
      ,
      {"datetime", GST_TYPE_DATE_TIME}
      ,
      {"gdatetime", G_TYPE_DATE_TIME}
      ,
      {"bitmask", GST_TYPE_BITMASK}
      ,
      {"flagset", GST_TYPE_FLAG_SET}
      ,
      {"sample", GST_TYPE_SAMPLE}
      ,
      {"taglist", GST_TYPE_TAG_LIST}
      ,
      {"type", G_TYPE_GTYPE}
      ,
      {"array", GST_TYPE_ARRAY}
      ,
      {"list", GST_TYPE_LIST}
    };
    _num = G_N_ELEMENTS (dyn_abbrs);
    /* permanently allocate and copy the array now */
    abbrs = g_new0 (GstValueAbbreviation, _num);
    memcpy (abbrs, dyn_abbrs, sizeof (GstValueAbbreviation) * _num);
    g_once_init_leave (&num, _num);
  }
  *n_abbrs = num;

  return abbrs;
}

/* given a type_name that could be a type abbreviation or a registered GType,
 * return a matching GType */
static GType
_priv_gst_value_gtype_from_abbr (const char *type_name)
{
  int i;
  GstValueAbbreviation *abbrs;
  gint n_abbrs;
  GType ret;

  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  abbrs = _priv_gst_value_get_abbrs (&n_abbrs);

  for (i = 0; i < n_abbrs; i++) {
    if (strcmp (type_name, abbrs[i].type_name) == 0) {
      return abbrs[i].type;
    }
  }

  /* this is the fallback */
  ret = g_type_from_name (type_name);
  /* If not found, try it as a dynamic type */
  if (G_UNLIKELY (ret == 0))
    ret = gst_dynamic_type_factory_load (type_name);
  return ret;

}

const char *
_priv_gst_value_gtype_to_abbr (GType type)
{
  int i;
  GstValueAbbreviation *abbrs;
  gint n_abbrs;

  g_return_val_if_fail (type != G_TYPE_INVALID, NULL);

  abbrs = _priv_gst_value_get_abbrs (&n_abbrs);

  for (i = 0; i < n_abbrs; i++) {
    if (type == abbrs[i].type) {
      return abbrs[i].type_name;
    }
  }

  return g_type_name (type);
}

/*
 * _priv_gst_value_parse_string:
 * @s: string to parse
 * @end: out-pointer to char behind end of string
 * @next: out-pointer to start of unread data
 * @unescape: @TRUE if the substring is escaped.
 *
 * Find the end of a sub-string. If end == next, the string will not be
 * null-terminated. In all other cases it will be.
 *
 * Note: This function modifies the string in @s (if unescape == @TRUE).
 *
 * Returns: @TRUE if a sub-string was found and @FALSE if the string is not
 * terminated.
 */
gboolean
_priv_gst_value_parse_string (gchar * s, gchar ** end, gchar ** next,
    gboolean unescape)
{
  gchar *w;

  if (*s == 0)
    return FALSE;

  if (*s != '"') {
    int ret = _priv_gst_value_parse_simple_string (s, end);
    *next = *end;

    return ret;
  }

  /* Find the closing quotes */
  if (unescape) {
    w = s;
    s++;
    while (*s != '"') {
      if (G_UNLIKELY (*s == 0))
        return FALSE;
      if (G_UNLIKELY (*s == '\\')) {
        s++;
        if (G_UNLIKELY (*s == 0))
          return FALSE;
      }
      *w = *s;
      w++;
      s++;
    }
    s++;
  } else {
    s++;
    while (*s != '"') {
      if (G_UNLIKELY (*s == 0))
        return FALSE;
      if (G_UNLIKELY (*s == '\\')) {
        s++;
        if (G_UNLIKELY (*s == 0))
          return FALSE;
      }
      s++;
    }
    s++;
    w = s;
  }

  *end = w;
  *next = s;

  return TRUE;
}

static gboolean
_priv_gst_value_parse_range (gchar * s, gchar ** after, GValue * value,
    GType type)
{
  GValue value1 = { 0 };
  GValue value2 = { 0 };
  GValue value3 = { 0 };
  GType range_type;
  gboolean ret, have_step = FALSE;

  if (*s != '[')
    return FALSE;
  s++;

  ret = _priv_gst_value_parse_value (s, &s, &value1, type, NULL);
  if (!ret)
    goto err;

  while (g_ascii_isspace (*s))
    s++;

  if (*s != ',')
    goto err;
  s++;

  while (g_ascii_isspace (*s))
    s++;

  ret = _priv_gst_value_parse_value (s, &s, &value2, type, NULL);
  if (!ret)
    goto err;

  while (g_ascii_isspace (*s))
    s++;

  /* optional step for int and int64 */
  if (G_VALUE_TYPE (&value1) == G_TYPE_INT
      || G_VALUE_TYPE (&value1) == G_TYPE_INT64) {
    if (*s == ',') {
      s++;

      while (g_ascii_isspace (*s))
        s++;

      ret = _priv_gst_value_parse_value (s, &s, &value3, type, NULL);
      if (!ret)
        goto err;

      while (g_ascii_isspace (*s))
        s++;

      have_step = TRUE;
    }
  }

  if (*s != ']')
    goto err;
  s++;

  if (G_VALUE_TYPE (&value1) != G_VALUE_TYPE (&value2))
    return FALSE;
  if (have_step && G_VALUE_TYPE (&value1) != G_VALUE_TYPE (&value3))
    return FALSE;

  if (G_VALUE_TYPE (&value1) == G_TYPE_DOUBLE) {
    range_type = GST_TYPE_DOUBLE_RANGE;
    g_value_init (value, range_type);
    gst_value_set_double_range (value,
        gst_g_value_get_double_unchecked (&value1),
        gst_g_value_get_double_unchecked (&value2));
  } else if (G_VALUE_TYPE (&value1) == G_TYPE_INT) {
    range_type = GST_TYPE_INT_RANGE;
    g_value_init (value, range_type);
    if (have_step)
      gst_value_set_int_range_step (value,
          gst_g_value_get_int_unchecked (&value1),
          gst_g_value_get_int_unchecked (&value2),
          gst_g_value_get_int_unchecked (&value3));
    else
      gst_value_set_int_range (value, gst_g_value_get_int_unchecked (&value1),
          gst_g_value_get_int_unchecked (&value2));
  } else if (G_VALUE_TYPE (&value1) == G_TYPE_INT64) {
    range_type = GST_TYPE_INT64_RANGE;
    g_value_init (value, range_type);
    if (have_step)
      gst_value_set_int64_range_step (value,
          gst_g_value_get_int64_unchecked (&value1),
          gst_g_value_get_int64_unchecked (&value2),
          gst_g_value_get_int64_unchecked (&value3));
    else
      gst_value_set_int64_range (value,
          gst_g_value_get_int64_unchecked (&value1),
          gst_g_value_get_int64_unchecked (&value2));
  } else if (G_VALUE_TYPE (&value1) == GST_TYPE_FRACTION) {
    range_type = GST_TYPE_FRACTION_RANGE;
    g_value_init (value, range_type);
    gst_value_set_fraction_range (value, &value1, &value2);
  } else {
    goto err;
  }

  *after = s;
  return TRUE;

err:
  g_value_unset (value);
  g_value_unset (&value1);
  g_value_unset (&value2);
  g_value_unset (&value3);
  return FALSE;
}

static gboolean
_priv_gst_value_parse_any_list (gchar * s, gchar ** after, GValue * value,
    GType type, char begin, char end, GParamSpec * pspec)
{
  GValue list_value = { 0 };
  gboolean ret;
  GstValueList *vlist = VALUE_LIST_ARRAY (value);
  GParamSpec *element_spec = NULL;

  if (pspec)
    element_spec = GST_PARAM_SPEC_ARRAY_LIST (pspec)->element_spec;

  if (*s != begin)
    return FALSE;
  s++;

  while (g_ascii_isspace (*s))
    s++;

  while (*s != end) {
    if (*s == ',') {
      s++;
      while (g_ascii_isspace (*s))
        s++;

      if (*s == ',')
        return FALSE;

      continue;
    }

    memset (&list_value, 0, sizeof (list_value));

    ret = _priv_gst_value_parse_value (s, &s, &list_value, type, element_spec);
    if (!ret)
      return FALSE;

    _gst_value_list_append_val (vlist, &list_value);

    while (g_ascii_isspace (*s))
      s++;

    if (*s != ',' && *s != end)
      return FALSE;
  }

  s++;

  *after = s;
  return TRUE;
}

static gboolean
_priv_gst_value_parse_list (gchar * s, gchar ** after, GValue * value,
    GType type, GParamSpec * pspec)
{
  return _priv_gst_value_parse_any_list (s, after, value, type, '{', '}',
      pspec);
}

static gboolean
_priv_gst_value_parse_array (gchar * s, gchar ** after, GValue * value,
    GType type, GParamSpec * pspec)
{
  return _priv_gst_value_parse_any_list (s, after, value, type, '<', '>',
      pspec);
}

gboolean
_priv_gst_value_parse_simple_string (gchar * str, gchar ** end)
{
  char *s = str;

  while (G_LIKELY (GST_ASCII_IS_STRING (*s))) {
    s++;
  }

  *end = s;

  return (s != str);
}

static gboolean
_priv_gst_value_parse_struct_or_caps (gchar * str, gchar ** after, GType type,
    GValue * value)
{
  gint openers = 1;
  gboolean ret = FALSE;
  gchar *s = str, t, *start, *end, *next;

  if (*s != '[')
    return FALSE;

  s++;
  str = s;
  for (; *s; s++) {
    if (*s == ']')
      openers--;
    else if (*s == '[')
      openers++;

    if (openers == 0) {
      *after = s + 1;
      break;
    }
  }

  if (*after == NULL)
    return FALSE;

  t = *s;
  *s = '\0';
  g_value_init (value, type);
  if (priv_gst_structure_parse_name (str, &start, &end, &next, TRUE))
    ret = gst_value_deserialize (value, str);
  if (G_UNLIKELY (!ret)) {
    *s = t;
    g_value_unset (value);
  }

  return ret;
}

static gboolean
_priv_gst_value_parse_range_struct_caps (gchar * s, gchar ** after,
    GValue * value, GType type)
{
  gint i;
  gchar *tmp = s;
  gboolean ret = FALSE;
  GType try_types[] = {
    GST_TYPE_STRUCTURE,
    GST_TYPE_CAPS,
  };

  if (type == GST_TYPE_CAPS || type == GST_TYPE_STRUCTURE)
    ret = _priv_gst_value_parse_struct_or_caps (tmp, &tmp, type, value);

  if (ret)
    goto ok;

  tmp = s;
  ret = _priv_gst_value_parse_range (tmp, &tmp, value, type);
  if (ret)
    goto ok;

  if (type != G_TYPE_INVALID)
    return ret;

  for (i = 0; i < G_N_ELEMENTS (try_types); i++) {
    tmp = s;
    ret = _priv_gst_value_parse_struct_or_caps (tmp, &tmp, try_types[i], value);
    if (ret)
      goto ok;
  }

  return ret;

ok:
  *after = tmp;
  return ret;
}

gboolean
_priv_gst_value_parse_value (gchar * str,
    gchar ** after, GValue * value, GType default_type, GParamSpec * pspec)
{
  gchar *type_name;
  gchar *type_end;
  gchar *value_s;
  gchar *value_end;
  gchar *s;
  gchar c;
  int ret = 0;
  GType type = default_type;

  s = str;
  while (g_ascii_isspace (*s))
    s++;

  /* check if there's a (type_name) 'cast' */
  type_name = NULL;

  if (*s == '(') {
    s++;
    while (g_ascii_isspace (*s))
      s++;
    type_name = s;
    if (G_UNLIKELY (!_priv_gst_value_parse_simple_string (s, &type_end)))
      return FALSE;
    s = type_end;
    while (g_ascii_isspace (*s))
      s++;
    if (G_UNLIKELY (*s != ')'))
      return FALSE;
    s++;
    while (g_ascii_isspace (*s))
      s++;

    c = *type_end;
    *type_end = 0;
    type = _priv_gst_value_gtype_from_abbr (type_name);
    GST_DEBUG ("trying type name '%s'", type_name);
    *type_end = c;

    if (G_UNLIKELY (type == G_TYPE_INVALID)) {
      GST_WARNING ("invalid type");
      return FALSE;
    }
  } else if (pspec) {
    type = G_PARAM_SPEC_VALUE_TYPE (pspec);
  }

  while (g_ascii_isspace (*s))
    s++;
  if (*s == '[') {
    ret = _priv_gst_value_parse_range_struct_caps (s, &s, value, type);
  } else if (*s == '{') {
    g_value_init (value, GST_TYPE_LIST);
    ret = _priv_gst_value_parse_list (s, &s, value, type, pspec);
  } else if (*s == '<') {
    g_value_init (value, GST_TYPE_ARRAY);
    ret = _priv_gst_value_parse_array (s, &s, value, type, pspec);
  } else {
    value_s = s;

    if (G_UNLIKELY (type == G_TYPE_INVALID)) {
      GType try_types[] =
          { G_TYPE_INT, G_TYPE_DOUBLE, GST_TYPE_FRACTION, GST_TYPE_FLAG_SET,
        G_TYPE_BOOLEAN, G_TYPE_STRING
      };
      int i;
      int value_size;
      gboolean check_wrapped_non_string;

      if (G_UNLIKELY (!_priv_gst_value_parse_string (s, &value_end, &s, FALSE)))
        return FALSE;
      /* Set NULL terminator for deserialization */
      value_size = value_end - value_s;
      value_s = g_strndup (value_s, value_end - value_s);
      /* Keep old broken behavior where "2" could be interpretted as an int */
      check_wrapped_non_string = value_s[0] == '"' &&
          strlen (value_s) >= 2 && value_end[-1] == '"';

      for (i = 0; i < G_N_ELEMENTS (try_types); i++) {
        g_value_init (value, try_types[i]);
        if (try_types[i] != G_TYPE_STRING && check_wrapped_non_string) {
          value_s[value_size - 1] = '\0';
          ret = gst_value_deserialize (value, value_s + 1);
          value_s[value_size - 1] = '"';
          if (ret) {
            const gchar *type_name = g_type_name (try_types[i]);

            g_warning ("Received a structure string that contains "
                "'=%s'. Reading as a %s value, rather than a string "
                "value. This is undesired behaviour, and with GStreamer 1.22 "
                " onward, this will be interpreted as a string value instead "
                "because it is wrapped in '\"' quotes. If you want to "
                "guarantee this value is read as a string, before this "
                "change, use '=(string)%s' instead. If you want to read "
                "in a %s value, leave its value unquoted.",
                value_s, type_name, value_s, type_name);
            break;
          }
        } else {
          ret = gst_value_deserialize (value, value_s);
          if (ret)
            break;
        }
        g_value_unset (value);
      }
    } else {
      g_value_init (value, type);

      if (G_UNLIKELY (!_priv_gst_value_parse_string (s, &value_end, &s, FALSE)))
        return FALSE;
      /* Set NULL terminator for deserialization */
      value_s = g_strndup (value_s, value_end - value_s);

      ret = gst_value_deserialize_with_pspec (value, value_s, pspec);
      if (G_UNLIKELY (!ret))
        g_value_unset (value);
    }
    g_free (value_s);
  }

  *after = s;

  return ret;
}

/**************
 * GstSegment *
 **************/

static gchar *
gst_value_serialize_segment_internal (const GValue * value, gboolean escape)
{
  GstSegment *seg = g_value_get_boxed (value);
  gchar *t, *res;
  GstStructure *s;

  s = gst_structure_new_id (GST_QUARK (SEGMENT),
      GST_QUARK (FLAGS), GST_TYPE_SEGMENT_FLAGS, seg->flags,
      GST_QUARK (RATE), G_TYPE_DOUBLE, seg->rate,
      GST_QUARK (APPLIED_RATE), G_TYPE_DOUBLE, seg->applied_rate,
      GST_QUARK (FORMAT), GST_TYPE_FORMAT, seg->format,
      GST_QUARK (BASE), G_TYPE_UINT64, seg->base,
      GST_QUARK (OFFSET), G_TYPE_UINT64, seg->offset,
      GST_QUARK (START), G_TYPE_UINT64, seg->start,
      GST_QUARK (STOP), G_TYPE_UINT64, seg->stop,
      GST_QUARK (TIME), G_TYPE_UINT64, seg->time,
      GST_QUARK (POSITION), G_TYPE_UINT64, seg->position,
      GST_QUARK (DURATION), G_TYPE_UINT64, seg->duration, NULL);

  t = gst_structure_to_string (s);
  if (escape) {
    res = g_strdup_printf ("\"%s\"", t);
    g_free (t);
  } else {
    res = t;
  }
  gst_structure_free (s);

  return res;
}

static gchar *
gst_value_serialize_segment (const GValue * value)
{
  return gst_value_serialize_segment_internal (value, TRUE);
}

static gboolean
gst_value_deserialize_segment_internal (GValue * dest, const gchar * s,
    gboolean unescape)
{
  GstStructure *str;
  GstSegment seg;
  gboolean res;
  gsize len;
  gchar *t;

  if (unescape) {
    len = strlen (s);
    if (G_UNLIKELY (*s != '"' || len < 2 || s[len - 1] != '"')) {
      /* "\"" is not an accepted string, so len must be at least 2 */
      GST_ERROR ("Failed deserializing segement: expected string to start and "
          "end with '\"'");
      return FALSE;
    }
    t = g_strdup (s + 1);
    t[len - 2] = '\0';
    /* removed trailing '"' */
    str = gst_structure_from_string (t, NULL);
    g_free (t);
  } else {
    str = gst_structure_from_string (s, NULL);
  }
  if (G_UNLIKELY (str == NULL))
    return FALSE;

  res = gst_structure_id_get (str,
      GST_QUARK (FLAGS), GST_TYPE_SEGMENT_FLAGS, &seg.flags,
      GST_QUARK (RATE), G_TYPE_DOUBLE, &seg.rate,
      GST_QUARK (APPLIED_RATE), G_TYPE_DOUBLE, &seg.applied_rate,
      GST_QUARK (FORMAT), GST_TYPE_FORMAT, &seg.format,
      GST_QUARK (BASE), G_TYPE_UINT64, &seg.base,
      GST_QUARK (OFFSET), G_TYPE_UINT64, &seg.offset,
      GST_QUARK (START), G_TYPE_UINT64, &seg.start,
      GST_QUARK (STOP), G_TYPE_UINT64, &seg.stop,
      GST_QUARK (TIME), G_TYPE_UINT64, &seg.time,
      GST_QUARK (POSITION), G_TYPE_UINT64, &seg.position,
      GST_QUARK (DURATION), G_TYPE_UINT64, &seg.duration, NULL);
  gst_structure_free (str);

  if (res)
    g_value_set_boxed (dest, &seg);

  return res;
}

static gboolean
gst_value_deserialize_segment (GValue * dest, const gchar * s)
{
  return gst_value_deserialize_segment_internal (dest, s, TRUE);
}

/****************
 * GstStructure *
 ****************/

/**
 * gst_value_set_structure:
 * @value: a GValue initialized to GST_TYPE_STRUCTURE
 * @structure: the structure to set the value to
 *
 * Sets the contents of @value to @structure.
 */
void
gst_value_set_structure (GValue * value, const GstStructure * structure)
{
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (G_VALUE_TYPE (value) == GST_TYPE_STRUCTURE);
  g_return_if_fail (structure == NULL || GST_IS_STRUCTURE (structure));

  g_value_set_boxed (value, structure);
}

/**
 * gst_value_get_structure:
 * @value: a GValue initialized to GST_TYPE_STRUCTURE
 *
 * Gets the contents of @value.
 *
 * Returns: (transfer none): the contents of @value
 */
const GstStructure *
gst_value_get_structure (const GValue * value)
{
  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  g_return_val_if_fail (G_VALUE_TYPE (value) == GST_TYPE_STRUCTURE, NULL);

  return (GstStructure *) g_value_get_boxed (value);
}

static gchar *
gst_value_serialize_structure (const GValue * value)
{
  GstStructure *structure = g_value_get_boxed (value);

  return priv_gst_string_take_and_wrap (gst_structure_to_string (structure));
  /* string should always end up being wrapped, since a structure string
   * ends in a ';' character */
}

static gboolean
gst_value_deserialize_structure (GValue * dest, const gchar * s)
{
  GstStructure *structure;

  if (*s != '"') {
    /* the output of gst_value_serialize_structure would never produce
     * such a string, but a user may pass to gst_structure_from_string
     * the string:
     *     name, sub=(GstStructure)sub-name, val=(int)5;
     * and expect sub to be read as an *empty* structure with the name
     * sub-name. Similar to
     *     name, caps=(GstCaps)video/x-raw, val=(int)5;
     * which gst_structure_to_string can produce. */
    structure = gst_structure_from_string (s, NULL);
  } else {
    gchar *str = gst_string_unwrap (s);

    if (G_UNLIKELY (!str))
      return FALSE;

    structure = gst_structure_from_string (str, NULL);
    g_free (str);
  }

  if (G_LIKELY (structure)) {
    g_value_take_boxed (dest, structure);
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_value_compare_structure (const GValue * value1, const GValue * value2)
{
  GstStructure *structure1 = GST_STRUCTURE (g_value_get_boxed (value1));
  GstStructure *structure2 = GST_STRUCTURE (g_value_get_boxed (value2));

  if (structure1 == structure2)
    return GST_VALUE_EQUAL;

  if (!structure1 || !structure2)
    return GST_VALUE_UNORDERED;

  if (gst_structure_is_equal (structure1, structure2))
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

/*******************
 * GstCapsFeatures *
 *******************/

/**
 * gst_value_set_caps_features:
 * @value: a GValue initialized to GST_TYPE_CAPS_FEATURES
 * @features: the features to set the value to
 *
 * Sets the contents of @value to @features.
 */
void
gst_value_set_caps_features (GValue * value, const GstCapsFeatures * features)
{
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (G_VALUE_TYPE (value) == GST_TYPE_CAPS_FEATURES);
  g_return_if_fail (features == NULL || GST_IS_CAPS_FEATURES (features));

  g_value_set_boxed (value, features);
}

/**
 * gst_value_get_caps_features:
 * @value: a GValue initialized to GST_TYPE_CAPS_FEATURES
 *
 * Gets the contents of @value.
 *
 * Returns: (transfer none): the contents of @value
 */
const GstCapsFeatures *
gst_value_get_caps_features (const GValue * value)
{
  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  g_return_val_if_fail (G_VALUE_TYPE (value) == GST_TYPE_CAPS_FEATURES, NULL);

  return (GstCapsFeatures *) g_value_get_boxed (value);
}

static gchar *
gst_value_serialize_caps_features (const GValue * value)
{
  GstCapsFeatures *features = g_value_get_boxed (value);

  return priv_gst_string_take_and_wrap (gst_caps_features_to_string (features));
}

static gboolean
gst_value_deserialize_caps_features (GValue * dest, const gchar * s)
{
  GstCapsFeatures *features;

  if (*s != '"') {
    /* This can happen if gst_caps_features_to_string only returns
     * ALL, NONE, or a single features name, which means it is not
     * actually wrapped by priv_gst_string_take_and_wrap */
    features = gst_caps_features_from_string (s);
  } else {
    gchar *str = gst_string_unwrap (s);

    if (G_UNLIKELY (!str))
      return FALSE;

    features = gst_caps_features_from_string (str);
    g_free (str);
  }

  if (G_LIKELY (features)) {
    g_value_take_boxed (dest, features);
    return TRUE;
  }
  return FALSE;
}

/**************
 * GstTagList *
 **************/
static gint
gst_value_compare_tag_list (const GValue * value1, const GValue * value2)
{
  GstTagList *taglist1 = GST_TAG_LIST (g_value_get_boxed (value1));
  GstTagList *taglist2 = GST_TAG_LIST (g_value_get_boxed (value2));

  if (gst_tag_list_is_equal (taglist1, taglist2))
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gboolean
gst_value_deserialize_tag_list (GValue * dest, const gchar * s)
{
  GstTagList *taglist;

  if (*s != '"') {
    /* the output of gst_value_serialize_tag_list would never produce
     * such a string, but a user may pass to gst_structure_from_string
     * the string:
     *     name, list=(GstTagList)taglist, val=(int)5;
     * and expect list to be read as an *empty* tag list. Similar to
     *     name, caps=(GstCaps)video/x-raw, val=(int)5;
     * which gst_structure_to_string can produce. */
    taglist = gst_tag_list_new_from_string (s);
  } else {
    gchar *str = gst_string_unwrap (s);

    if (G_UNLIKELY (!str))
      return FALSE;

    taglist = gst_tag_list_new_from_string (str);
    g_free (str);
  }

  if (G_LIKELY (taglist != NULL)) {
    g_value_take_boxed (dest, taglist);
    return TRUE;
  }
  return FALSE;
}

static gchar *
gst_value_serialize_tag_list (const GValue * value)
{
  GstTagList *taglist = g_value_get_boxed (value);

  return priv_gst_string_take_and_wrap (gst_tag_list_to_string (taglist));
  /* string should always end up being wrapped, since a taglist (structure)
   * string ends in a ';' character */
}


/*************
 * GstBuffer *
 *************/

static gint
compare_buffer (GstBuffer * buf1, GstBuffer * buf2)
{
  gsize size1, size2;
  GstMapInfo info1, info2;
  gint result, mret;

  if (buf1 == buf2)
    return GST_VALUE_EQUAL;

  size1 = gst_buffer_get_size (buf1);
  size2 = gst_buffer_get_size (buf2);

  if (size1 != size2)
    return GST_VALUE_UNORDERED;

  if (size1 == 0)
    return GST_VALUE_EQUAL;

  if (!gst_buffer_map (buf1, &info1, GST_MAP_READ))
    return GST_VALUE_UNORDERED;

  if (!gst_buffer_map (buf2, &info2, GST_MAP_READ)) {
    gst_buffer_unmap (buf1, &info1);
    return GST_VALUE_UNORDERED;
  }

  mret = memcmp (info1.data, info2.data, info1.size);
  if (mret == 0)
    result = GST_VALUE_EQUAL;
  else if (mret < 0)
    result = GST_VALUE_LESS_THAN;
  else
    result = GST_VALUE_GREATER_THAN;

  gst_buffer_unmap (buf1, &info1);
  gst_buffer_unmap (buf2, &info2);

  return result;
}

static gint
gst_value_compare_buffer (const GValue * value1, const GValue * value2)
{
  GstBuffer *buf1 = gst_value_get_buffer (value1);
  GstBuffer *buf2 = gst_value_get_buffer (value2);

  return compare_buffer (buf1, buf2);
}

static gchar *
gst_value_serialize_buffer (const GValue * value)
{
  GstMapInfo info;
  guint8 *data;
  gint i;
  gchar *string;
  GstBuffer *buffer;

  buffer = gst_value_get_buffer (value);
  if (buffer == NULL)
    return NULL;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ))
    return NULL;

  data = info.data;

  string = g_malloc (info.size * 2 + 1);
  for (i = 0; i < info.size; i++) {
    sprintf (string + i * 2, "%02x", data[i]);
  }
  string[info.size * 2] = 0;

  gst_buffer_unmap (buffer, &info);

  return string;
}

static gboolean
gst_value_deserialize_buffer (GValue * dest, const gchar * s)
{
  GstBuffer *buffer;
  gint len;
  gchar ts[3];
  GstMapInfo info;
  guint8 *data;
  gint i;

  len = strlen (s);
  if (len & 1)
    goto wrong_length;

  buffer = gst_buffer_new_allocate (NULL, len / 2, NULL);
  if (!gst_buffer_map (buffer, &info, GST_MAP_WRITE))
    goto map_failed;
  data = info.data;

  for (i = 0; i < len / 2; i++) {
    if (!isxdigit ((int) s[i * 2]) || !isxdigit ((int) s[i * 2 + 1]))
      goto wrong_char;

    ts[0] = s[i * 2 + 0];
    ts[1] = s[i * 2 + 1];
    ts[2] = 0;

    data[i] = (guint8) strtoul (ts, NULL, 16);
  }
  gst_buffer_unmap (buffer, &info);

  gst_value_take_buffer (dest, buffer);

  return TRUE;

  /* ERRORS */
wrong_length:
  {
    return FALSE;
  }
map_failed:
  {
    return FALSE;
  }
wrong_char:
  {
    gst_buffer_unref (buffer);
    gst_buffer_unmap (buffer, &info);
    return FALSE;
  }
}

/*************
 * GstSample *
 *************/

/* This function is mostly used for comparing image/buffer tags in taglists */
static gint
gst_value_compare_sample (const GValue * value1, const GValue * value2)
{
  GstBuffer *buf1 = gst_sample_get_buffer (gst_value_get_sample (value1));
  GstBuffer *buf2 = gst_sample_get_buffer (gst_value_get_sample (value2));

  /* FIXME: should we take into account anything else such as caps? */
  return compare_buffer (buf1, buf2);
}

static gchar *
gst_value_serialize_sample (const GValue * value)
{
  const GstStructure *info_structure;
  GstSegment *segment;
  GstBuffer *buffer;
  GstCaps *caps;
  GstSample *sample;
  GValue val = { 0, };
  gchar *info_str, *caps_str, *tmp;
  gchar *buf_str, *seg_str, *s;

  sample = g_value_get_boxed (value);

  buffer = gst_sample_get_buffer (sample);
  if (buffer) {
    g_value_init (&val, GST_TYPE_BUFFER);
    g_value_set_boxed (&val, buffer);
    buf_str = gst_value_serialize_buffer (&val);
    g_value_unset (&val);
  } else {
    buf_str = g_strdup ("None");
  }

  caps = gst_sample_get_caps (sample);
  if (caps) {
    tmp = gst_caps_to_string (caps);
    caps_str = g_base64_encode ((guchar *) tmp, strlen (tmp) + 1);
    g_strdelimit (caps_str, "=", '_');
    g_free (tmp);
  } else {
    caps_str = g_strdup ("None");
  }

  segment = gst_sample_get_segment (sample);
  if (segment) {
    g_value_init (&val, GST_TYPE_SEGMENT);
    g_value_set_boxed (&val, segment);
    tmp = gst_value_serialize_segment_internal (&val, FALSE);
    seg_str = g_base64_encode ((guchar *) tmp, strlen (tmp) + 1);
    g_strdelimit (seg_str, "=", '_');
    g_free (tmp);
    g_value_unset (&val);
  } else {
    seg_str = g_strdup ("None");
  }

  info_structure = gst_sample_get_info (sample);
  if (info_structure) {
    tmp = gst_structure_to_string (info_structure);
    info_str = g_base64_encode ((guchar *) tmp, strlen (tmp) + 1);
    g_strdelimit (info_str, "=", '_');
    g_free (tmp);
  } else {
    info_str = g_strdup ("None");
  }

  s = g_strconcat (buf_str, ":", caps_str, ":", seg_str, ":", info_str, NULL);
  g_free (buf_str);
  g_free (caps_str);
  g_free (seg_str);
  g_free (info_str);

  return s;
}

static gboolean
gst_value_deserialize_sample (GValue * dest, const gchar * s)
{
  GValue bval = G_VALUE_INIT, sval = G_VALUE_INIT;
  GstStructure *info;
  GstSample *sample;
  GstCaps *caps = NULL;
  gboolean ret = FALSE;
  gchar **fields;
  gsize outlen;
  gint len;

  GST_TRACE ("deserialize '%s'", s);

  fields = g_strsplit (s, ":", -1);
  len = g_strv_length (fields);
  if (len != 4)
    goto wrong_length;

  g_value_init (&bval, GST_TYPE_BUFFER);
  g_value_init (&sval, GST_TYPE_SEGMENT);

  if (!gst_value_deserialize_buffer (&bval, fields[0]))
    goto fail;

  if (strcmp (fields[1], "None") != 0) {
    g_strdelimit (fields[1], "_", '=');
    g_base64_decode_inplace (fields[1], &outlen);
    GST_TRACE ("caps    : %s", fields[1]);
    caps = gst_caps_from_string (fields[1]);
    if (caps == NULL)
      goto fail;
  }

  if (strcmp (fields[2], "None") != 0) {
    g_strdelimit (fields[2], "_", '=');
    g_base64_decode_inplace (fields[2], &outlen);
    GST_TRACE ("segment : %s", fields[2]);
    if (!gst_value_deserialize_segment_internal (&sval, fields[2], FALSE))
      goto fail;
  }

  if (strcmp (fields[3], "None") != 0) {
    g_strdelimit (fields[3], "_", '=');
    g_base64_decode_inplace (fields[3], &outlen);
    GST_TRACE ("info    : %s", fields[3]);
    info = gst_structure_from_string (fields[3], NULL);
    if (info == NULL)
      goto fail;
  } else {
    info = NULL;
  }

  sample = gst_sample_new (gst_value_get_buffer (&bval), caps,
      g_value_get_boxed (&sval), info);

  g_value_take_boxed (dest, sample);

  ret = TRUE;

fail:
  if (caps)
    gst_caps_unref (caps);
  g_value_unset (&bval);
  g_value_unset (&sval);

wrong_length:

  g_strfreev (fields);

  return ret;
}

/***********
 * boolean *
 ***********/

static gint
gst_value_compare_boolean (const GValue * value1, const GValue * value2)
{
  if ((value1->data[0].v_int != 0) == (value2->data[0].v_int != 0))
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_boolean (const GValue * value)
{
  if (value->data[0].v_int) {
    return g_strdup ("true");
  }
  return g_strdup ("false");
}

static gboolean
gst_value_deserialize_boolean (GValue * dest, const gchar * s)
{
  gboolean ret = FALSE;

  if (g_ascii_strcasecmp (s, "true") == 0 ||
      g_ascii_strcasecmp (s, "yes") == 0 ||
      g_ascii_strcasecmp (s, "t") == 0 || strcmp (s, "1") == 0) {
    g_value_set_boolean (dest, TRUE);
    ret = TRUE;
  } else if (g_ascii_strcasecmp (s, "false") == 0 ||
      g_ascii_strcasecmp (s, "no") == 0 ||
      g_ascii_strcasecmp (s, "f") == 0 || strcmp (s, "0") == 0) {
    g_value_set_boolean (dest, FALSE);
    ret = TRUE;
  }

  return ret;
}

#define CREATE_SERIALIZATION_START(_type,_macro)                        \
static gint                                                             \
gst_value_compare_ ## _type                                             \
(const GValue * value1, const GValue * value2)                          \
{                                                                       \
  g ## _type val1 = g_value_get_ ## _type (value1);                     \
  g ## _type val2 = g_value_get_ ## _type (value2);                     \
  if (val1 > val2)                                                      \
    return GST_VALUE_GREATER_THAN;                                      \
  if (val1 < val2)                                                      \
    return GST_VALUE_LESS_THAN;                                         \
  return GST_VALUE_EQUAL;                                               \
}                                                                       \
                                                                        \
static gchar *                                                          \
gst_value_serialize_ ## _type (const GValue * value)                    \
{                                                                       \
  GValue val = { 0, };                                                  \
  g_value_init (&val, G_TYPE_STRING);                                   \
  if (!g_value_transform (value, &val))                                 \
    g_assert_not_reached ();                                            \
  /* NO_COPY_MADNESS!!! */                                              \
  return (char *) g_value_get_string (&val);                            \
}

/* deserialize the given s into to as a gint64.
 * check if the result is actually storeable in the given size number of
 * bytes.
 */
static gboolean
gst_value_deserialize_int_helper (gint64 * to, const gchar * s,
    gint64 min, gint64 max, gint size)
{
  gboolean ret = FALSE;
  gchar *end;
  guint64 mask = ~0;

  errno = 0;
  *to = g_ascii_strtoull (s, &end, 0);
  /* a range error is a definitive no-no */
  if (errno == ERANGE) {
    return FALSE;
  }

  if (*end == 0) {
    ret = TRUE;
  } else {
    if (g_ascii_strcasecmp (s, "little_endian") == 0) {
      *to = G_LITTLE_ENDIAN;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "big_endian") == 0) {
      *to = G_BIG_ENDIAN;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "byte_order") == 0) {
      *to = G_BYTE_ORDER;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "min") == 0) {
      *to = min;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "max") == 0) {
      *to = max;
      ret = TRUE;
    }
  }
  if (ret) {
    /* by definition, a gint64 fits into a gint64; so ignore those */
    if (size != sizeof (mask)) {
      if (*to >= 0) {
        /* for positive numbers, we create a mask of 1's outside of the range
         * and 0's inside the range.  An and will thus keep only 1 bits
         * outside of the range */
        mask <<= (size * 8);
        if ((mask & *to) != 0) {
          ret = FALSE;
        }
      } else {
        /* for negative numbers, we do a 2's complement version */
        mask <<= ((size * 8) - 1);
        if ((mask & *to) != mask) {
          ret = FALSE;
        }
      }
    }
  }
  return ret;
}

#define CREATE_SERIALIZATION(_type,_macro)                              \
CREATE_SERIALIZATION_START(_type,_macro)                                \
                                                                        \
static gboolean                                                         \
gst_value_deserialize_ ## _type (GValue * dest, const gchar *s)         \
{                                                                       \
  gint64 x;                                                             \
                                                                        \
  if (gst_value_deserialize_int_helper (&x, s, G_MIN ## _macro,         \
      G_MAX ## _macro, sizeof (g ## _type))) {                          \
    g_value_set_ ## _type (dest, /*(g ## _type)*/ x);                   \
    return TRUE;                                                        \
  } else {                                                              \
    return FALSE;                                                       \
  }                                                                     \
}

#define CREATE_USERIALIZATION(_type,_macro)                             \
CREATE_SERIALIZATION_START(_type,_macro)                                \
                                                                        \
static gboolean                                                         \
gst_value_deserialize_ ## _type (GValue * dest, const gchar *s)         \
{                                                                       \
  gint64 x;                                                             \
  gchar *end;                                                           \
  gboolean ret = FALSE;                                                 \
                                                                        \
  errno = 0;                                                            \
  x = g_ascii_strtoull (s, &end, 0);                                    \
  /* a range error is a definitive no-no */                             \
  if (errno == ERANGE) {                                                \
    return FALSE;                                                       \
  }                                                                     \
  /* the cast ensures the range check later on makes sense */           \
  x = (g ## _type) x;                                                   \
  if (*end == 0) {                                                      \
    ret = TRUE;                                                         \
  } else {                                                              \
    if (g_ascii_strcasecmp (s, "little_endian") == 0) {                 \
      x = G_LITTLE_ENDIAN;                                              \
      ret = TRUE;                                                       \
    } else if (g_ascii_strcasecmp (s, "big_endian") == 0) {             \
      x = G_BIG_ENDIAN;                                                 \
      ret = TRUE;                                                       \
    } else if (g_ascii_strcasecmp (s, "byte_order") == 0) {             \
      x = G_BYTE_ORDER;                                                 \
      ret = TRUE;                                                       \
    } else if (g_ascii_strcasecmp (s, "min") == 0) {                    \
      x = 0;                                                            \
      ret = TRUE;                                                       \
    } else if (g_ascii_strcasecmp (s, "max") == 0) {                    \
      x = G_MAX ## _macro;                                              \
      ret = TRUE;                                                       \
    }                                                                   \
  }                                                                     \
  if (ret) {                                                            \
    if (x > G_MAX ## _macro) {                                          \
      ret = FALSE;                                                      \
    } else {                                                            \
      g_value_set_ ## _type (dest, x);                                  \
    }                                                                   \
  }                                                                     \
  return ret;                                                           \
}

CREATE_SERIALIZATION (int, INT);
CREATE_SERIALIZATION (int64, INT64);
CREATE_SERIALIZATION (long, LONG);

CREATE_USERIALIZATION (uint, UINT);
CREATE_USERIALIZATION (uint64, UINT64);
CREATE_USERIALIZATION (ulong, ULONG);

/* FIXME 2.0: remove this again, plugins shouldn't have uchar properties */
#ifndef G_MAXUCHAR
#define G_MAXUCHAR 255
#endif
CREATE_USERIALIZATION (uchar, UCHAR);

/**********
 * double *
 **********/
static gint
gst_value_compare_double (const GValue * value1, const GValue * value2)
{
  if (value1->data[0].v_double > value2->data[0].v_double)
    return GST_VALUE_GREATER_THAN;
  if (value1->data[0].v_double < value2->data[0].v_double)
    return GST_VALUE_LESS_THAN;
  if (value1->data[0].v_double == value2->data[0].v_double)
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_double (const GValue * value)
{
  gchar d[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (d, G_ASCII_DTOSTR_BUF_SIZE, value->data[0].v_double);
  return g_strdup (d);
}

static gboolean
gst_value_deserialize_double (GValue * dest, const gchar * s)
{
  gdouble x;
  gboolean ret = FALSE;
  gchar *end;

  x = g_ascii_strtod (s, &end);
  if (*end == 0) {
    ret = TRUE;
  } else {
    if (g_ascii_strcasecmp (s, "min") == 0) {
      x = -G_MAXDOUBLE;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "max") == 0) {
      x = G_MAXDOUBLE;
      ret = TRUE;
    }
  }
  if (ret) {
    g_value_set_double (dest, x);
  }
  return ret;
}

/*********
 * float *
 *********/

static gint
gst_value_compare_float (const GValue * value1, const GValue * value2)
{
  if (value1->data[0].v_float > value2->data[0].v_float)
    return GST_VALUE_GREATER_THAN;
  if (value1->data[0].v_float < value2->data[0].v_float)
    return GST_VALUE_LESS_THAN;
  if (value1->data[0].v_float == value2->data[0].v_float)
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_float (const GValue * value)
{
  gchar d[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (d, G_ASCII_DTOSTR_BUF_SIZE, value->data[0].v_float);
  return g_strdup (d);
}

static gboolean
gst_value_deserialize_float (GValue * dest, const gchar * s)
{
  gdouble x;
  gboolean ret = FALSE;
  gchar *end;

  x = g_ascii_strtod (s, &end);
  if (*end == 0) {
    ret = TRUE;
  } else {
    if (g_ascii_strcasecmp (s, "min") == 0) {
      x = -G_MAXFLOAT;
      ret = TRUE;
    } else if (g_ascii_strcasecmp (s, "max") == 0) {
      x = G_MAXFLOAT;
      ret = TRUE;
    }
  }
  if (x > G_MAXFLOAT || x < -G_MAXFLOAT)
    ret = FALSE;
  if (ret) {
    g_value_set_float (dest, (float) x);
  }
  return ret;
}

/**********
 * string *
 **********/

static gint
gst_value_compare_string (const GValue * value1, const GValue * value2)
{
  if (G_UNLIKELY (!value1->data[0].v_pointer || !value2->data[0].v_pointer)) {
    /* if only one is NULL, no match - otherwise both NULL == EQUAL */
    if (value1->data[0].v_pointer != value2->data[0].v_pointer)
      return GST_VALUE_UNORDERED;
  } else {
    gint x = strcmp (value1->data[0].v_pointer, value2->data[0].v_pointer);

    if (x < 0)
      return GST_VALUE_LESS_THAN;
    if (x > 0)
      return GST_VALUE_GREATER_THAN;
  }

  return GST_VALUE_EQUAL;
}

static gint
gst_string_measure_wrapping (const gchar * s)
{
  gint len;
  gboolean wrap = FALSE;

  if (G_UNLIKELY (s == NULL))
    return -1;

  /* Special case: the actual string NULL needs wrapping */
  if (G_UNLIKELY (strcmp (s, "NULL") == 0))
    return 4;

  len = 0;
  while (*s) {
    if (GST_ASCII_IS_STRING (*s)) {
      len++;
    } else if (*s < 0x20 || *s >= 0x7f) {
      wrap = TRUE;
      len += 4;
    } else {
      wrap = TRUE;
      len += 2;
    }
    s++;
  }

  /* Wrap the string if we found something that needs
   * wrapping, or the empty string (len == 0) */
  return (wrap || len == 0) ? len : -1;
}

static gchar *
gst_string_wrap_inner (const gchar * s, gint len)
{
  gchar *d, *e;

  e = d = g_malloc (len + 3);

  *e++ = '\"';
  while (*s) {
    if (GST_ASCII_IS_STRING (*s)) {
      *e++ = *s++;
    } else if (*s < 0x20 || *s >= 0x7f) {
      *e++ = '\\';
      *e++ = '0' + ((*(guchar *) s) >> 6);
      *e++ = '0' + (((*s) >> 3) & 0x7);
      *e++ = '0' + ((*s++) & 0x7);
    } else {
      *e++ = '\\';
      *e++ = *s++;
    }
  }
  *e++ = '\"';
  *e = 0;

  g_assert (e - d <= len + 3);
  return d;
}

/* Do string wrapping/escaping */
static gchar *
gst_string_wrap (const gchar * s)
{
  gint len = gst_string_measure_wrapping (s);

  if (G_LIKELY (len < 0))
    return g_strdup (s);

  return gst_string_wrap_inner (s, len);
}

/* Same as above, but take ownership of the string */
gchar *
priv_gst_string_take_and_wrap (gchar * s)
{
  gchar *out;
  gint len = gst_string_measure_wrapping (s);

  if (G_LIKELY (len < 0))
    return s;

  out = gst_string_wrap_inner (s, len);
  g_free (s);

  return out;
}

/*
 * This function takes a string delimited with double quotes (")
 * and unescapes any \xxx octal numbers.
 *
 * If sequences of \y are found where y is not in the range of
 * 0->3, y is copied unescaped.
 *
 * If \xyy is found where x is an octal number but y is not, an
 * error is encountered and %NULL is returned.
 *
 * the input string must be \0 terminated.
 */
static gchar *
gst_string_unwrap (const gchar * s)
{
  gchar *ret;
  gchar *read, *write;

  /* NULL string returns NULL */
  if (s == NULL)
    return NULL;

  /* strings not starting with " are invalid */
  if (*s != '"')
    return NULL;

  /* make copy of original string to hold the result. This
   * string will always be smaller than the original */
  ret = g_strdup (s);
  read = ret;
  write = ret;

  /* need to move to the next position as we parsed the " */
  read++;

  while (*read) {
    if (GST_ASCII_IS_STRING (*read)) {
      /* normal chars are just copied */
      *write++ = *read++;
    } else if (*read == '"') {
      /* quote marks end of string */
      break;
    } else if (*read == '\\') {
      /* got an escape char, move to next position to read a tripplet
       * of octal numbers */
      read++;
      /* is the next char a possible first octal number? */
      if (*read >= '0' && *read <= '3') {
        /* parse other 2 numbers, if one of them is not in the range of
         * an octal number, we error. We also catch the case where a zero
         * byte is found here. */
        if (read[1] < '0' || read[1] > '7' || read[2] < '0' || read[2] > '7')
          goto beach;

        /* now convert the octal number to a byte again. */
        *write++ = ((read[0] - '0') << 6) +
            ((read[1] - '0') << 3) + (read[2] - '0');

        read += 3;
      } else {
        /* if we run into a \0 here, we definitely won't get a quote later */
        if (*read == 0)
          goto beach;
        /* else copy \X sequence */
        *write++ = *read++;
      }
    } else if (*read == '\0') {
      goto beach;
    } else {
      *write++ = *read++;
    }
  }
  /* if the string is not ending in " and zero terminated, we error */
  if (*read != '"' || read[1] != '\0')
    goto beach;

  /* null terminate result string and return */
  *write = '\0';
  return ret;

beach:
  g_free (ret);
  return NULL;
}

static gchar *
gst_value_serialize_string (const GValue * value)
{
  return gst_string_wrap (value->data[0].v_pointer);
}

static gboolean
gst_value_deserialize_string (GValue * dest, const gchar * s)
{
  if (G_UNLIKELY (strcmp (s, "NULL") == 0)) {
    g_value_set_string (dest, NULL);
    return TRUE;
  } else if (G_LIKELY (*s != '"' || s[strlen (s) - 1] != '"')) {
    if (!g_utf8_validate (s, -1, NULL))
      return FALSE;
    g_value_set_string (dest, s);
    return TRUE;
  } else {
    /* strings delimited with double quotes should be unwrapped */
    gchar *str = gst_string_unwrap (s);
    if (G_UNLIKELY (!str))
      return FALSE;
    if (!g_utf8_validate (str, -1, NULL)) {
      g_free (str);
      return FALSE;
    }
    g_value_take_string (dest, str);
  }

  return TRUE;
}

/********
 * enum *
 ********/

static gint
gst_value_compare_enum (const GValue * value1, const GValue * value2)
{
  GEnumValue *en1, *en2;
  GEnumClass *klass1 = (GEnumClass *) g_type_class_ref (G_VALUE_TYPE (value1));
  GEnumClass *klass2 = (GEnumClass *) g_type_class_ref (G_VALUE_TYPE (value2));

  g_return_val_if_fail (klass1, GST_VALUE_UNORDERED);
  g_return_val_if_fail (klass2, GST_VALUE_UNORDERED);
  en1 = g_enum_get_value (klass1, g_value_get_enum (value1));
  en2 = g_enum_get_value (klass2, g_value_get_enum (value2));
  g_type_class_unref (klass1);
  g_type_class_unref (klass2);
  g_return_val_if_fail (en1, GST_VALUE_UNORDERED);
  g_return_val_if_fail (en2, GST_VALUE_UNORDERED);
  if (en1->value < en2->value)
    return GST_VALUE_LESS_THAN;
  if (en1->value > en2->value)
    return GST_VALUE_GREATER_THAN;

  return GST_VALUE_EQUAL;
}

static gchar *
gst_value_serialize_enum (const GValue * value)
{
  GEnumValue *en;
  GEnumClass *klass = (GEnumClass *) g_type_class_ref (G_VALUE_TYPE (value));

  g_return_val_if_fail (klass, NULL);
  en = g_enum_get_value (klass, g_value_get_enum (value));
  g_type_class_unref (klass);

  /* might be one of the custom formats registered later */
  if (G_UNLIKELY (en == NULL && G_VALUE_TYPE (value) == GST_TYPE_FORMAT)) {
    const GstFormatDefinition *format_def;

    format_def = gst_format_get_details ((GstFormat) g_value_get_enum (value));
    g_return_val_if_fail (format_def != NULL, NULL);
    return g_strdup (format_def->description);
  }

  g_return_val_if_fail (en, NULL);
  return g_strdup (en->value_nick);
}

static gint
gst_value_deserialize_enum_iter_cmp (const GValue * format_def_value,
    const gchar * s)
{
  const GstFormatDefinition *format_def =
      g_value_get_pointer (format_def_value);

  if (g_ascii_strcasecmp (s, format_def->nick) == 0)
    return 0;

  return g_ascii_strcasecmp (s, format_def->description);
}

static gboolean
gst_value_deserialize_enum (GValue * dest, const gchar * s)
{
  GEnumValue *en;
  gchar *endptr = NULL;
  GEnumClass *klass = (GEnumClass *) g_type_class_ref (G_VALUE_TYPE (dest));

  g_return_val_if_fail (klass, FALSE);
  if (!(en = g_enum_get_value_by_name (klass, s))) {
    if (!(en = g_enum_get_value_by_nick (klass, s))) {
      gint i = strtol (s, &endptr, 0);

      if (endptr && *endptr == '\0') {
        en = g_enum_get_value (klass, i);
      }
    }
  }
  g_type_class_unref (klass);

  /* might be one of the custom formats registered later */
  if (G_UNLIKELY (en == NULL && G_VALUE_TYPE (dest) == GST_TYPE_FORMAT)) {
    GValue res = { 0, };
    const GstFormatDefinition *format_def;
    GstIterator *iter;
    gboolean found;

    iter = gst_format_iterate_definitions ();

    found = gst_iterator_find_custom (iter,
        (GCompareFunc) gst_value_deserialize_enum_iter_cmp, &res, (gpointer) s);

    if (found) {
      format_def = g_value_get_pointer (&res);
      g_return_val_if_fail (format_def != NULL, FALSE);
      g_value_set_enum (dest, (gint) format_def->value);
      g_value_unset (&res);
    }
    gst_iterator_free (iter);
    return found;
  }

  /* enum name/nick not found */
  if (en == NULL)
    return FALSE;

  g_value_set_enum (dest, en->value);
  return TRUE;
}

/********
 * flags *
 ********/

/* we just compare the value here */
static gint
gst_value_compare_gflags (const GValue * value1, const GValue * value2)
{
  guint fl1, fl2;
  GFlagsClass *klass1 =
      (GFlagsClass *) g_type_class_ref (G_VALUE_TYPE (value1));
  GFlagsClass *klass2 =
      (GFlagsClass *) g_type_class_ref (G_VALUE_TYPE (value2));

  g_return_val_if_fail (klass1, GST_VALUE_UNORDERED);
  g_return_val_if_fail (klass2, GST_VALUE_UNORDERED);
  fl1 = g_value_get_flags (value1);
  fl2 = g_value_get_flags (value2);
  g_type_class_unref (klass1);
  g_type_class_unref (klass2);
  if (fl1 < fl2)
    return GST_VALUE_LESS_THAN;
  if (fl1 > fl2)
    return GST_VALUE_GREATER_THAN;

  return GST_VALUE_EQUAL;
}

/* the different flags are serialized separated with a + */
static gchar *
gst_value_serialize_gflags (const GValue * value)
{
  guint flags;
  GFlagsValue *fl;
  GFlagsClass *klass = (GFlagsClass *) g_type_class_ref (G_VALUE_TYPE (value));
  gchar *result, *tmp;
  gboolean first = TRUE;

  g_return_val_if_fail (klass, NULL);

  flags = g_value_get_flags (value);

  /* if no flags are set, try to serialize to the _NONE string */
  if (!flags) {
    fl = g_flags_get_first_value (klass, flags);
    if (fl)
      return g_strdup (fl->value_name);
    else
      return g_strdup ("0");
  }

  /* some flags are set, so serialize one by one */
  result = g_strdup ("");
  while (flags) {
    fl = g_flags_get_first_value (klass, flags);
    if (fl == NULL) {
      if (flags) {
        g_critical ("Could not serialize invalid flags 0x%x of type %s",
            flags, G_VALUE_TYPE_NAME (value));
        g_free (result);
        result = g_strdup ("0");
      }
      break;
    }

    tmp = g_strconcat (result, (first ? "" : "+"), fl->value_name, NULL);
    g_free (result);
    result = tmp;
    first = FALSE;

    /* clear flag */
    flags &= ~fl->value;
  }
  g_type_class_unref (klass);

  return result;
}

static gboolean
gst_value_gflags_str_to_flags (GFlagsClass * klass, const gchar * s,
    guint * out_flags, guint * out_mask)
{
  GFlagsValue *fl;
  gchar delimiter;
  const gchar *pos = NULL;
  const gchar *next;
  gchar *cur_str, *endptr;
  guint flags = 0;
  guint mask = 0;
  guint val;

  g_return_val_if_fail (klass, FALSE);

  /* split into parts delimited with + or / and
   * compose the set of flags and mask. */
  pos = s;

  if (*pos == '\0')
    goto done;                  /* Empty string, nothing to do */

  /* As a special case if the first char isn't a delimiter, assume
   * it's a '+' - for GFlags strings, which don't start with a
   * delimiter, while GFlagSet always will */
  if (*pos == '/' || *pos == '+') {
    delimiter = *pos;
    pos++;
  } else {
    delimiter = '+';
  }

  do {
    /* Find the next delimiter */
    next = pos;
    while (*next != '\0' && *next != '+' && *next != '/')
      next++;
    cur_str = g_strndup (pos, next - pos);

    if ((fl = g_flags_get_value_by_name (klass, cur_str)))
      val = fl->value;
    else if ((fl = g_flags_get_value_by_nick (klass, cur_str)))
      val = fl->value;
    else {
      val = strtoul (cur_str, &endptr, 0);
      /* direct numeric value */
      if (endptr == NULL || *endptr != '\0') {
        g_free (cur_str);
        return FALSE;           /* Invalid numeric or string we can't convert */
      }
    }
    g_free (cur_str);

    if (val) {
      mask |= val;
      if (delimiter == '+')
        flags |= val;
    }

    /* Advance to the next delimiter */
    pos = next;
    delimiter = *pos;
    pos++;
  } while (delimiter != '\0');

done:
  if (out_flags)
    *out_flags = flags;
  if (out_mask)
    *out_mask = mask;

  return TRUE;
}


static gboolean
gst_value_deserialize_gflags (GValue * dest, const gchar * s)
{
  GFlagsClass *klass = (GFlagsClass *) g_type_class_ref (G_VALUE_TYPE (dest));
  gboolean res = FALSE;
  guint flags = 0;

  if (gst_value_gflags_str_to_flags (klass, s, &flags, NULL)) {
    g_value_set_flags (dest, flags);
    res = TRUE;
  }

  g_type_class_unref (klass);

  return res;
}

/*********
 * gtype *
 *********/

static gint
gst_value_compare_gtype (const GValue * value1, const GValue * value2)
{
  if (value1->data[0].v_pointer == value2->data[0].v_pointer)
    return GST_VALUE_EQUAL;
  return GST_VALUE_UNORDERED;
}

static gchar *
gst_value_serialize_gtype (const GValue * value)
{
  return g_strdup (g_type_name (g_value_get_gtype (value)));
}

static gboolean
gst_value_deserialize_gtype (GValue * dest, const gchar * s)
{
  GType t = g_type_from_name (s);
  gboolean ret = TRUE;

  if (t == G_TYPE_INVALID)
    ret = FALSE;
  if (ret) {
    g_value_set_gtype (dest, t);
  }
  return ret;
}

/****************
 * subset *
 ****************/

static gboolean
gst_value_is_subset_int_range_int_range (const GValue * value1,
    const GValue * value2)
{
  gint gcd;

  g_return_val_if_fail (GST_VALUE_HOLDS_INT_RANGE (value1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_INT_RANGE (value2), FALSE);

  if (INT_RANGE_MIN (value1) * INT_RANGE_STEP (value1) <
      INT_RANGE_MIN (value2) * INT_RANGE_STEP (value2))
    return FALSE;
  if (INT_RANGE_MAX (value1) * INT_RANGE_STEP (value1) >
      INT_RANGE_MAX (value2) * INT_RANGE_STEP (value2))
    return FALSE;

  if (INT_RANGE_MIN (value2) == INT_RANGE_MAX (value2)) {
    if ((INT_RANGE_MIN (value2) * INT_RANGE_STEP (value2)) %
        INT_RANGE_STEP (value1))
      return FALSE;
    return TRUE;
  }

  gcd =
      gst_util_greatest_common_divisor (INT_RANGE_STEP (value1),
      INT_RANGE_STEP (value2));
  if (gcd != MIN (INT_RANGE_STEP (value1), INT_RANGE_STEP (value2)))
    return FALSE;

  return TRUE;
}

static gboolean
gst_value_is_subset_int64_range_int64_range (const GValue * value1,
    const GValue * value2)
{
  gint64 gcd;

  g_return_val_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_INT64_RANGE (value2), FALSE);

  if (INT64_RANGE_MIN (value1) < INT64_RANGE_MIN (value2))
    return FALSE;
  if (INT64_RANGE_MAX (value1) > INT64_RANGE_MAX (value2))
    return FALSE;

  if (INT64_RANGE_MIN (value2) == INT64_RANGE_MAX (value2)) {
    if ((INT64_RANGE_MIN (value2) * INT64_RANGE_STEP (value2)) %
        INT64_RANGE_STEP (value1))
      return FALSE;
    return TRUE;
  }

  gcd =
      gst_util_greatest_common_divisor_int64 (INT64_RANGE_STEP (value1),
      INT64_RANGE_STEP (value2));
  if (gcd != MIN (INT64_RANGE_STEP (value1), INT64_RANGE_STEP (value2)))
    return FALSE;

  return TRUE;
}

/* A flag set is a subset of another if the superset allows the
 * flags of the subset */
static gboolean
gst_value_is_subset_flagset_flagset (const GValue * value1,
    const GValue * value2)
{
  guint f1, f2;
  guint m1, m2;

  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (value1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (value2), FALSE);

  f1 = value1->data[0].v_uint;
  f2 = value2->data[0].v_uint;

  m1 = value1->data[1].v_uint;
  m2 = value2->data[1].v_uint;

  /* Not a subset if masked bits of superset disagree */
  if ((f1 & m1) != (f2 & (m1 & m2)))
    return FALSE;

  return TRUE;
}

static gboolean
gst_value_is_subset_structure_structure (const GValue * value1,
    const GValue * value2)
{
  const GstStructure *s1, *s2;

  g_return_val_if_fail (GST_VALUE_HOLDS_STRUCTURE (value1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_STRUCTURE (value2), FALSE);

  s1 = gst_value_get_structure (value1);
  s2 = gst_value_get_structure (value2);

  return gst_structure_is_subset (s1, s2);
}

static gboolean
gst_value_is_subset_list_list (const GValue * value1, const GValue * value2)
{
  GstValueList *vlist1 = VALUE_LIST_ARRAY (value1);
  GstValueList *vlist2 = VALUE_LIST_ARRAY (value2);
  gint it1, len1, it2, len2;

  len1 = vlist1->len;
  len2 = vlist2->len;

  /* A list can't be a subset of a smaller list */
  if (len1 > len2)
    return FALSE;

  /* Check if all elements of the first list are within the 2nd list */
  for (it1 = 0; it1 < len1; it1++) {
    const GValue *child1 = &vlist1->fields[it1];
    gboolean seen = FALSE;

    for (it2 = 0; it2 < len2; it2++) {
      const GValue *child2 = &vlist2->fields[it2];
      if (gst_value_compare (child1, child2) == GST_VALUE_EQUAL) {
        seen = TRUE;
        break;
      }
    }
    if (!seen)
      return FALSE;
  }

  return TRUE;
}

static gboolean
gst_value_is_subset_list (const GValue * value1, const GValue * value2)
{
  GstValueList *vlist2 = VALUE_LIST_ARRAY (value2);
  gint it2, len2;

  len2 = vlist2->len;

  /* Check whether value1 is within the list */
  for (it2 = 0; it2 < len2; it2++) {
    const GValue *child2 = &vlist2->fields[it2];
    if (gst_value_compare (value1, child2) == GST_VALUE_EQUAL) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * gst_value_is_subset:
 * @value1: a #GValue
 * @value2: a #GValue
 *
 * Check that @value1 is a subset of @value2.
 *
 * Return: %TRUE is @value1 is a subset of @value2
 */
gboolean
gst_value_is_subset (const GValue * value1, const GValue * value2)
{
  GType type1 = G_VALUE_TYPE (value1);
  GType type2 = G_VALUE_TYPE (value2);

  /* special case for int/int64 ranges, since we cannot compute
     the difference for those when they have different steps,
     and it's actually a lot simpler to compute whether a range
     is a subset of another. */
  if (GST_VALUE_HOLDS_INT_RANGE (value1) && GST_VALUE_HOLDS_INT_RANGE (value2)) {
    return gst_value_is_subset_int_range_int_range (value1, value2);
  } else if (GST_VALUE_HOLDS_INT64_RANGE (value1)
      && GST_VALUE_HOLDS_INT64_RANGE (value2)) {
    return gst_value_is_subset_int64_range_int64_range (value1, value2);
  } else if (GST_VALUE_HOLDS_FLAG_SET (value1) &&
      GST_VALUE_HOLDS_FLAG_SET (value2)) {
    return gst_value_is_subset_flagset_flagset (value1, value2);
  } else if (GST_VALUE_HOLDS_STRUCTURE (value1)
      && GST_VALUE_HOLDS_STRUCTURE (value2)) {
    return gst_value_is_subset_structure_structure (value1, value2);
  } else if (type2 == GST_TYPE_LIST) {
    if (type1 == GST_TYPE_LIST)
      return gst_value_is_subset_list_list (value1, value2);
    return gst_value_is_subset_list (value1, value2);
  }

  /*
   * 1 - [1,2] = empty
   * -> !subset
   *
   * [1,2] - 1 = 2
   *  -> 1 - [1,2] = empty
   *  -> subset
   *
   * [1,3] - [1,2] = 3
   * -> [1,2] - [1,3] = empty
   * -> subset
   *
   * {1,2} - {1,3} = 2
   * -> {1,3} - {1,2} = 3
   * -> !subset
   *
   *  First caps subtraction needs to return a non-empty set, second
   *  subtractions needs to give en empty set.
   *  Both substractions are switched below, as it's faster that way.
   */
  if (!gst_value_subtract (NULL, value1, value2)) {
    if (gst_value_subtract (NULL, value2, value1)) {
      return TRUE;
    }
  }
  return FALSE;
}

/*********
 * union *
 *********/

static gboolean
gst_value_union_int_int_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  gint v = src1->data[0].v_int;

  /* check if it's already in the range */
  if (INT_RANGE_MIN (src2) * INT_RANGE_STEP (src2) <= v &&
      INT_RANGE_MAX (src2) * INT_RANGE_STEP (src2) >= v &&
      v % INT_RANGE_STEP (src2) == 0) {
    if (dest)
      gst_value_init_and_copy (dest, src2);
    return TRUE;
  }

  /* check if it extends the range */
  if (v == (INT_RANGE_MIN (src2) - 1) * INT_RANGE_STEP (src2)) {
    if (dest) {
      guint64 new_min = INT_RANGE_MIN (src2) - 1;
      guint64 new_max = INT_RANGE_MAX (src2);

      gst_value_init_and_copy (dest, src2);
      dest->data[0].v_uint64 = (new_min << 32) | (new_max);
    }
    return TRUE;
  }
  if (v == (INT_RANGE_MAX (src2) + 1) * INT_RANGE_STEP (src2)) {
    if (dest) {
      guint64 new_min = INT_RANGE_MIN (src2);
      guint64 new_max = INT_RANGE_MAX (src2) + 1;

      gst_value_init_and_copy (dest, src2);
      dest->data[0].v_uint64 = (new_min << 32) | (new_max);
    }
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_union_int_range_int_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  /* We can union in several special cases:
     1 - one is a subset of another
     2 - same step and not disjoint
     3 - different step, at least one with one value which matches a 'next' or 'previous'
     - anything else ?
   */

  /* 1 - subset */
  if (gst_value_is_subset_int_range_int_range (src1, src2)) {
    if (dest)
      gst_value_init_and_copy (dest, src2);
    return TRUE;
  }
  if (gst_value_is_subset_int_range_int_range (src2, src1)) {
    if (dest)
      gst_value_init_and_copy (dest, src1);
    return TRUE;
  }

  /* 2 - same step and not disjoint */
  if (INT_RANGE_STEP (src1) == INT_RANGE_STEP (src2)) {
    if ((INT_RANGE_MIN (src1) <= INT_RANGE_MAX (src2) + 1 &&
            INT_RANGE_MAX (src1) >= INT_RANGE_MIN (src2) - 1) ||
        (INT_RANGE_MIN (src2) <= INT_RANGE_MAX (src1) + 1 &&
            INT_RANGE_MAX (src2) >= INT_RANGE_MIN (src1) - 1)) {
      if (dest) {
        gint step = INT_RANGE_STEP (src1);
        gint min = step * MIN (INT_RANGE_MIN (src1), INT_RANGE_MIN (src2));
        gint max = step * MAX (INT_RANGE_MAX (src1), INT_RANGE_MAX (src2));
        g_value_init (dest, GST_TYPE_INT_RANGE);
        gst_value_set_int_range_step (dest, min, max, step);
      }
      return TRUE;
    }
  }

  /* 3 - single value matches next or previous */
  if (INT_RANGE_STEP (src1) != INT_RANGE_STEP (src2)) {
    gint n1 = INT_RANGE_MAX (src1) - INT_RANGE_MIN (src1) + 1;
    gint n2 = INT_RANGE_MAX (src2) - INT_RANGE_MIN (src2) + 1;
    if (n1 == 1 || n2 == 1) {
      const GValue *range_value = NULL;
      gint scalar = 0;
      if (n1 == 1) {
        range_value = src2;
        scalar = INT_RANGE_MIN (src1) * INT_RANGE_STEP (src1);
      } else if (n2 == 1) {
        range_value = src1;
        scalar = INT_RANGE_MIN (src2) * INT_RANGE_STEP (src2);
      }

      if (scalar ==
          (INT_RANGE_MIN (range_value) - 1) * INT_RANGE_STEP (range_value)) {
        if (dest) {
          guint64 new_min = (guint)
              ((INT_RANGE_MIN (range_value) -
                  1) * INT_RANGE_STEP (range_value));
          guint64 new_max = (guint)
              (INT_RANGE_MAX (range_value) * INT_RANGE_STEP (range_value));

          gst_value_init_and_copy (dest, range_value);
          dest->data[0].v_uint64 = (new_min << 32) | (new_max);
        }
        return TRUE;
      } else if (scalar ==
          (INT_RANGE_MAX (range_value) + 1) * INT_RANGE_STEP (range_value)) {
        if (dest) {
          guint64 new_min = (guint)
              (INT_RANGE_MIN (range_value) * INT_RANGE_STEP (range_value));
          guint64 new_max = (guint)
              ((INT_RANGE_MAX (range_value) +
                  1) * INT_RANGE_STEP (range_value));
          gst_value_init_and_copy (dest, range_value);
          dest->data[0].v_uint64 = (new_min << 32) | (new_max);
        }
        return TRUE;
      }
    }
  }

  /* If we get there, we did not find a way to make a union that can be
     represented with our simplistic model. */
  return FALSE;
}

static gboolean
gst_value_union_flagset_flagset (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  /* We can union 2 flag sets where they do not disagree on
   * required (masked) flag bits */
  guint64 f1, f2;
  guint64 m1, m2;

  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (src1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (src2), FALSE);

  f1 = src1->data[0].v_uint;
  f2 = src2->data[0].v_uint;

  m1 = src1->data[1].v_uint;
  m2 = src2->data[1].v_uint;

  /* Can't union if masked bits disagree */
  if ((f1 & (m1 & m2)) != (f2 & (m1 & m2)))
    return FALSE;

  if (dest) {
    g_value_init (dest, GST_TYPE_FLAG_SET);
    /* Copy masked bits from src2 to src1 */
    f1 &= ~m2;
    f1 |= (f2 & m2);
    m1 |= m2;
    gst_value_set_flagset (dest, f1, m1);
  }

  return TRUE;
}

/* iterating over the result taking the union with the other structure's value */
static gboolean
structure_field_union_into (GQuark field_id, GValue * val, gpointer user_data)
{
  GstStructure *other = user_data;
  const GValue *other_value;
  GValue res_value = G_VALUE_INIT;

  other_value = gst_structure_id_get_value (other, field_id);
  /* no value in the other struct, just keep this value */
  if (!other_value)
    return TRUE;

  if (!gst_value_union (&res_value, val, other_value))
    return FALSE;

  g_value_unset (val);
  gst_value_move (val, &res_value);
  return TRUE;
}

/* iterating over the other source structure adding missing values */
static gboolean
structure_field_union_from (GQuark field_id, const GValue * other_val,
    gpointer user_data)
{
  GstStructure *result = user_data;
  const GValue *result_value;

  result_value = gst_structure_id_get_value (result, field_id);
  if (!result_value)
    gst_structure_id_set_value (result, field_id, other_val);

  return TRUE;
}

static gboolean
gst_value_union_structure_structure (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  const GstStructure *s1, *s2;
  GstStructure *result;
  gboolean ret;

  g_return_val_if_fail (GST_VALUE_HOLDS_STRUCTURE (src1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_STRUCTURE (src2), FALSE);

  s1 = gst_value_get_structure (src1);
  s2 = gst_value_get_structure (src2);

  /* Can't join two structures with different names into a single structure */
  if (!gst_structure_has_name (s1, gst_structure_get_name (s2))) {
    gst_value_list_concat (dest, src1, src2);
    return TRUE;
  }

  result = gst_structure_copy (s1);
  ret =
      gst_structure_map_in_place (result, structure_field_union_into,
      (gpointer) s2);
  if (!ret)
    goto out;
  ret =
      gst_structure_foreach (s2, structure_field_union_from, (gpointer) result);

  if (ret) {
    g_value_init (dest, GST_TYPE_STRUCTURE);
    gst_value_set_structure (dest, result);
  }

out:
  gst_structure_free (result);
  return ret;
}

static gboolean
gst_value_union_fraction_fraction_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  GValue *vals;
  int f_n, f_d, fr_start_n, fr_start_d, fr_end_n, fr_end_d;

  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (src1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (src2), FALSE);

  /* Fraction */
  f_n = src1->data[0].v_int;
  f_d = src1->data[1].v_int;

  vals = src2->data[0].v_pointer;
  /* Fraction range start */
  fr_start_n = vals[0].data[0].v_int;
  fr_start_d = vals[0].data[1].v_int;
  /* Fraction range end */
  fr_end_n = vals[1].data[0].v_int;
  fr_end_d = vals[1].data[1].v_int;

  /* Check if it's already in the range. This is the only case in which we can
   * successfully perform a union. */
  if (gst_util_fraction_compare (f_n, f_d, fr_start_n, fr_start_d) >= 0 &&
      gst_util_fraction_compare (f_n, f_d, fr_end_n, fr_end_d) <= 0) {
    if (dest)
      gst_value_init_and_copy (dest, src2);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_union_fraction_range_fraction_range (GValue * dest,
    const GValue * src1, const GValue * src2)
{
  GValue *vals1, *vals2;
  int fr1_start_n, fr1_start_d, fr1_end_n, fr1_end_d;
  int fr2_start_n, fr2_start_d, fr2_end_n, fr2_end_d;
  int fr_start_n, fr_start_d, fr_end_n, fr_end_d;

  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (src1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION_RANGE (src2), FALSE);

  vals1 = src1->data[0].v_pointer;
  g_return_val_if_fail (vals1 != NULL, FALSE);

  fr1_start_n = vals1[0].data[0].v_int;
  fr1_start_d = vals1[0].data[1].v_int;
  fr1_end_n = vals1[1].data[0].v_int;
  fr1_end_d = vals1[1].data[1].v_int;

  vals2 = src2->data[0].v_pointer;
  g_return_val_if_fail (vals2 != NULL, FALSE);

  fr2_start_n = vals2[0].data[0].v_int;
  fr2_start_d = vals2[0].data[1].v_int;
  fr2_end_n = vals2[1].data[0].v_int;
  fr2_end_d = vals2[1].data[1].v_int;

  /* Ranges are completely disjoint: end of one range is less than the start of
   * other range */
  if (gst_util_fraction_compare (fr2_end_n, fr2_end_d, fr1_start_n,
          fr1_start_d) < 0
      || gst_util_fraction_compare (fr1_end_n, fr1_end_d, fr2_start_n,
          fr2_start_d) < 0)
    return FALSE;

  /* Ranges overlap, union is trivial */
  if (!dest)
    return TRUE;

  if (gst_util_fraction_compare (fr1_start_n, fr1_start_d, fr2_start_n,
          fr2_start_d) < 0) {
    fr_start_n = fr1_start_n;
    fr_start_d = fr1_start_d;
  } else {
    fr_start_n = fr2_start_n;
    fr_start_d = fr2_start_d;
  }

  if (gst_util_fraction_compare (fr1_end_n, fr1_end_d, fr2_end_n,
          fr2_end_d) > 0) {
    fr_end_n = fr1_end_n;
    fr_end_d = fr1_end_d;
  } else {
    fr_end_n = fr2_end_n;
    fr_end_d = fr2_end_d;
  }

  g_value_init (dest, GST_TYPE_FRACTION_RANGE);
  gst_value_set_fraction_range_full (dest, fr_start_n, fr_start_d, fr_end_n,
      fr_end_d);
  return TRUE;
}

/****************
 * intersection *
 ****************/

static gboolean
gst_value_intersect_int_int_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  if (INT_RANGE_MIN (src2) * INT_RANGE_STEP (src2) <= src1->data[0].v_int &&
      INT_RANGE_MAX (src2) * INT_RANGE_STEP (src2) >= src1->data[0].v_int &&
      src1->data[0].v_int % INT_RANGE_STEP (src2) == 0) {
    if (dest)
      gst_value_init_and_copy (dest, src1);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_intersect_int_range_int_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  gint min;
  gint max;
  gint step;

  step =
      INT_RANGE_STEP (src1) /
      gst_util_greatest_common_divisor (INT_RANGE_STEP (src1),
      INT_RANGE_STEP (src2));
  if (G_MAXINT32 / INT_RANGE_STEP (src2) < step)
    return FALSE;
  step *= INT_RANGE_STEP (src2);

  min =
      MAX (INT_RANGE_MIN (src1) * INT_RANGE_STEP (src1),
      INT_RANGE_MIN (src2) * INT_RANGE_STEP (src2));
  min = (min + step - 1) / step * step;
  max =
      MIN (INT_RANGE_MAX (src1) * INT_RANGE_STEP (src1),
      INT_RANGE_MAX (src2) * INT_RANGE_STEP (src2));
  max = max / step * step;

  if (min < max) {
    if (dest) {
      g_value_init (dest, GST_TYPE_INT_RANGE);
      gst_value_set_int_range_step (dest, min, max, step);
    }
    return TRUE;
  }
  if (min == max) {
    if (dest) {
      g_value_init (dest, G_TYPE_INT);
      g_value_set_int (dest, min);
    }
    return TRUE;
  }

  return FALSE;
}

#define INT64_RANGE_MIN_VAL(v) (INT64_RANGE_MIN (v) * INT64_RANGE_STEP (v))
#define INT64_RANGE_MAX_VAL(v) (INT64_RANGE_MAX (v) * INT64_RANGE_STEP (v))

static gboolean
gst_value_intersect_int64_int64_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  if (INT64_RANGE_MIN_VAL (src2) <= src1->data[0].v_int64 &&
      INT64_RANGE_MAX_VAL (src2) >= src1->data[0].v_int64 &&
      src1->data[0].v_int64 % INT64_RANGE_STEP (src2) == 0) {
    if (dest)
      gst_value_init_and_copy (dest, src1);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_intersect_int64_range_int64_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  gint64 min;
  gint64 max;
  gint64 step;

  step =
      INT64_RANGE_STEP (src1) /
      gst_util_greatest_common_divisor_int64 (INT64_RANGE_STEP (src1),
      INT64_RANGE_STEP (src2));
  if (G_MAXINT64 / INT64_RANGE_STEP (src2) < step)
    return FALSE;
  step *= INT64_RANGE_STEP (src2);

  min =
      MAX (INT64_RANGE_MIN (src1) * INT64_RANGE_STEP (src1),
      INT64_RANGE_MIN (src2) * INT64_RANGE_STEP (src2));
  min = (min + step - 1) / step * step;
  max =
      MIN (INT64_RANGE_MAX (src1) * INT64_RANGE_STEP (src1),
      INT64_RANGE_MAX (src2) * INT64_RANGE_STEP (src2));
  max = max / step * step;

  if (min < max) {
    if (dest) {
      g_value_init (dest, GST_TYPE_INT64_RANGE);
      gst_value_set_int64_range_step (dest, min, max, step);
    }
    return TRUE;
  }
  if (min == max) {
    if (dest) {
      g_value_init (dest, G_TYPE_INT64);
      g_value_set_int64 (dest, min);
    }
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_intersect_double_double_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  if (src2->data[0].v_double <= src1->data[0].v_double &&
      src2->data[1].v_double >= src1->data[0].v_double) {
    if (dest)
      gst_value_init_and_copy (dest, src1);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_intersect_double_range_double_range (GValue * dest,
    const GValue * src1, const GValue * src2)
{
  gdouble min;
  gdouble max;

  min = MAX (src1->data[0].v_double, src2->data[0].v_double);
  max = MIN (src1->data[1].v_double, src2->data[1].v_double);

  if (min < max) {
    if (dest) {
      g_value_init (dest, GST_TYPE_DOUBLE_RANGE);
      gst_value_set_double_range (dest, min, max);
    }
    return TRUE;
  }
  if (min == max) {
    if (dest) {
      g_value_init (dest, G_TYPE_DOUBLE);
      g_value_set_int (dest, (int) min);
    }
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_intersect_list_list (GValue * dest, const GValue * value1,
    const GValue * value2)
{
  guint8 tmpfield[16];          /* Check up to 128 values */
  guint8 *bitfield;
  gboolean alloc_bitfield = FALSE;
  gboolean res = FALSE;
  GValue *tmp;
  GType type1, type2;
  guint it1, len1, start2, it2, len2, itar;
  GstValueList *vlist = NULL;

  /* If they don't have the same basic type, all bets are off :) */
  if (!gst_value_list_or_array_get_basic_type (value1, &type1) ||
      !gst_value_list_or_array_get_basic_type (value2, &type2) ||
      type1 != type2)
    return FALSE;

  len1 = VALUE_LIST_SIZE (value1);
  len2 = VALUE_LIST_SIZE (value2);

  /* Fast-path with no dest (i.e. only interested in knowing whether
   * both lists intersected without wanting the result) */
  if (!dest) {
    for (it1 = 0; it1 < len1; it1++) {
      const GValue *item1 = VALUE_LIST_GET_VALUE (value1, it1);
      for (it2 = 0; it2 < len2; it2++) {
        const GValue *item2 = VALUE_LIST_GET_VALUE (value2, it2);
        if (gst_value_intersect (NULL, item1, item2)) {
          res = TRUE;
          goto beach;
        }
      }
    }
    goto beach;
  }
#define is_visited(idx) (bitfield[(idx) >> 3]    & (1 << ((idx) & 0x7)))
#define mark_visited(idx) (bitfield[(idx) >> 3] |= (1 << ((idx) & 0x7)))

  /* Bitfield to avoid double-visiting */
  if (G_UNLIKELY (len2 > 128)) {
    alloc_bitfield = TRUE;
    bitfield = g_malloc0 ((len2 / 8) + 1);
    GST_CAT_LOG (GST_CAT_PERFORMANCE,
        "Allocation for GstValueList with more than 128 members");
  } else {
    bitfield = &tmpfield[0];
    memset (bitfield, 0, 16);
  }


  /* When doing list<->list intersections, there is a greater
   * probability of ending up with a list than with a single value.
   * Furthermore, the biggest that list can be will the smallest list
   * (i.e. intersects fully).
   * Therefore we pre-allocate a GstValueList of that size. */
  vlist = _gst_value_list_new (MIN (len1, len2));

  itar = 0;
  tmp = &vlist->fields[0];
  start2 = 0;

  for (it1 = 0; it1 < len1; it1++) {
    const GValue *item1 = VALUE_LIST_GET_VALUE (value1, it1);
    for (it2 = start2; it2 < len2; it2++) {
      const GValue *item2;
      if (is_visited (it2))
        continue;
      item2 = VALUE_LIST_GET_VALUE (value2, it2);

      if (gst_value_intersect (tmp, item1, item2)) {
        res = TRUE;
        mark_visited (it2);
        /* Increment our inner-loop starting point */
        if (it2 == start2)
          start2++;

        /* Move our collection value */
        itar += 1;
        tmp = &vlist->fields[itar];
        vlist->len += 1;

        /* We can stop iterating the second part now that we've matched */
        break;
      }
    }
  }

#undef is_visited
#undef mark_visited

  if (res) {
    /* If we end up with a single value in the list, just use that
     * value. Else use the list */
    if (vlist->len == 1) {
      gst_value_move (dest, &vlist->fields[0]);
      g_free (vlist);
    } else {
      dest->g_type = GST_TYPE_LIST;
      dest->data[0].v_pointer = vlist;
    }
  } else {
    g_free (vlist);
  }

beach:
  if (alloc_bitfield)
    g_free (bitfield);
  return res;
}

static gboolean
gst_value_intersect_list (GValue * dest, const GValue * value1,
    const GValue * value2)
{
  guint i, size;
  GValue intersection = { 0, };
  gboolean ret = FALSE;

  /* Use optimized list-list intersection */
  if (G_VALUE_TYPE (value2) == GST_TYPE_LIST) {
    return gst_value_intersect_list_list (dest, value1, value2);
  }

  size = VALUE_LIST_SIZE (value1);
  for (i = 0; i < size; i++) {
    const GValue *cur = VALUE_LIST_GET_VALUE (value1, i);

    /* quicker version when we don't need the resulting set */
    if (!dest) {
      if (gst_value_intersect (NULL, cur, value2)) {
        ret = TRUE;
        break;
      }
      continue;
    }

    if (gst_value_intersect (&intersection, cur, value2)) {
      /* append value */
      if (!ret) {
        gst_value_move (dest, &intersection);
        ret = TRUE;
      } else if (GST_VALUE_HOLDS_LIST (dest)) {
        _gst_value_list_append_and_take_value (dest, &intersection);
      } else {
        GValue temp;

        gst_value_move (&temp, dest);
        gst_value_list_merge (dest, &temp, &intersection);
        g_value_unset (&temp);
        g_value_unset (&intersection);
      }
    }
  }

  return ret;
}

static gboolean
gst_value_intersect_array (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  guint size;
  guint n;
  GValue val = { 0 };

  /* only works on similar-sized arrays */
  size = gst_value_array_get_size (src1);
  if (size != gst_value_array_get_size (src2))
    return FALSE;

  /* quicker value when we don't need the resulting set */
  if (!dest) {
    for (n = 0; n < size; n++) {
      if (!gst_value_intersect (NULL, gst_value_array_get_value (src1, n),
              gst_value_array_get_value (src2, n))) {
        return FALSE;
      }
    }
    return TRUE;
  }

  g_value_init (dest, GST_TYPE_ARRAY);

  for (n = 0; n < size; n++) {
    if (!gst_value_intersect (&val, gst_value_array_get_value (src1, n),
            gst_value_array_get_value (src2, n))) {
      g_value_unset (dest);
      return FALSE;
    }
    _gst_value_array_append_and_take_value (dest, &val);
  }

  return TRUE;
}

static gboolean
gst_value_intersect_fraction_fraction_range (GValue * dest, const GValue * src1,
    const GValue * src2)
{
  gint res1, res2;
  GValue *vals;

  vals = src2->data[0].v_pointer;

  if (vals == NULL)
    return FALSE;

  res1 = gst_value_compare_fraction (&vals[0], src1);
  res2 = gst_value_compare_fraction (&vals[1], src1);

  if ((res1 == GST_VALUE_EQUAL || res1 == GST_VALUE_LESS_THAN) &&
      (res2 == GST_VALUE_EQUAL || res2 == GST_VALUE_GREATER_THAN)) {
    if (dest)
      gst_value_init_and_copy (dest, src1);
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_value_intersect_fraction_range_fraction_range (GValue * dest,
    const GValue * src1, const GValue * src2)
{
  GValue *min;
  GValue *max;
  gint res;
  GValue *vals1, *vals2;

  vals1 = src1->data[0].v_pointer;
  vals2 = src2->data[0].v_pointer;
  g_return_val_if_fail (vals1 != NULL && vals2 != NULL, FALSE);

  /* min = MAX (src1.start, src2.start) */
  res = gst_value_compare_fraction (&vals1[0], &vals2[0]);
  g_return_val_if_fail (res != GST_VALUE_UNORDERED, FALSE);
  if (res == GST_VALUE_LESS_THAN)
    min = &vals2[0];            /* Take the max of the 2 */
  else
    min = &vals1[0];

  /* max = MIN (src1.end, src2.end) */
  res = gst_value_compare_fraction (&vals1[1], &vals2[1]);
  g_return_val_if_fail (res != GST_VALUE_UNORDERED, FALSE);
  if (res == GST_VALUE_GREATER_THAN)
    max = &vals2[1];            /* Take the min of the 2 */
  else
    max = &vals1[1];

  res = gst_value_compare_fraction (min, max);
  g_return_val_if_fail (res != GST_VALUE_UNORDERED, FALSE);
  if (res == GST_VALUE_LESS_THAN) {
    if (dest) {
      g_value_init (dest, GST_TYPE_FRACTION_RANGE);
      vals1 = dest->data[0].v_pointer;
      g_value_copy (min, &vals1[0]);
      g_value_copy (max, &vals1[1]);
    }
    return TRUE;
  }
  if (res == GST_VALUE_EQUAL) {
    if (dest)
      gst_value_init_and_copy (dest, min);
    return TRUE;
  }

  return FALSE;
}

/* Two flagsets intersect if the masked bits in both
 * flagsets are exactly equal */
static gboolean
gst_value_intersect_flagset_flagset (GValue * dest,
    const GValue * src1, const GValue * src2)
{
  guint f1, f2;
  guint m1, m2;
  GType type1, type2, flagset_type;

  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (src1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (src2), FALSE);

  f1 = src1->data[0].v_uint;
  f2 = src2->data[0].v_uint;

  m1 = src1->data[1].v_uint;
  m2 = src2->data[1].v_uint;

  /* Don't intersect if masked bits disagree */
  if ((f1 & (m1 & m2)) != (f2 & (m1 & m2)))
    return FALSE;

  /* Allow intersection with the generic FlagSet type, on one
   * side, but not 2 different subtypes - that makes no sense */
  type1 = G_VALUE_TYPE (src1);
  type2 = G_VALUE_TYPE (src2);
  flagset_type = GST_TYPE_FLAG_SET;

  if (type1 != type2 && type1 != flagset_type && type2 != flagset_type)
    return FALSE;

  if (dest) {
    GType dest_type;

    /* Prefer an output type that matches a sub-type,
     * rather than the generic type */
    if (type1 != flagset_type)
      dest_type = type1;
    else
      dest_type = type2;

    g_value_init (dest, dest_type);

    /* The compatible set is all the bits from src1 that it
     * cares about and all the bits from src2 that it cares
     * about. */
    dest->data[0].v_uint = (f1 & m1) | (f2 & m2);
    dest->data[1].v_uint = m1 | m2;
  }

  return TRUE;
}

static gboolean
gst_value_intersect_structure_structure (GValue * dest,
    const GValue * src1, const GValue * src2)
{
  const GstStructure *s1, *s2;
  GstStructure *d1;

  s1 = gst_value_get_structure (src1);
  s2 = gst_value_get_structure (src2);

  d1 = gst_structure_intersect (s1, s2);
  if (!d1)
    return FALSE;

  if (dest) {
    g_value_init (dest, GST_TYPE_STRUCTURE);
    gst_value_set_structure (dest, d1);
  }

  gst_structure_free (d1);
  return TRUE;
}

/***************
 * subtraction *
 ***************/

static gboolean
gst_value_subtract_int_int_range (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gint min = gst_value_get_int_range_min (subtrahend);
  gint max = gst_value_get_int_range_max (subtrahend);
  gint step = gst_value_get_int_range_step (subtrahend);
  gint val = g_value_get_int (minuend);

  if (step == 0)
    return FALSE;

  /* subtracting a range from an int only works if the int is not in the
   * range */
  if (val < min || val > max || val % step) {
    /* and the result is the int */
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  }
  return FALSE;
}

/* creates a new int range based on input values.
 */
static gboolean
gst_value_create_new_range (GValue * dest, gint min1, gint max1, gint min2,
    gint max2, gint step)
{
  GValue v1 = { 0, };
  GValue v2 = { 0, };
  GValue *pv1, *pv2;            /* yeah, hungarian! */

  g_return_val_if_fail (step > 0, FALSE);
  g_return_val_if_fail (min1 % step == 0, FALSE);
  g_return_val_if_fail (max1 % step == 0, FALSE);
  g_return_val_if_fail (min2 % step == 0, FALSE);
  g_return_val_if_fail (max2 % step == 0, FALSE);

  if (min1 <= max1 && min2 <= max2) {
    pv1 = &v1;
    pv2 = &v2;
  } else if (min1 <= max1) {
    pv1 = dest;
    pv2 = NULL;
  } else if (min2 <= max2) {
    pv1 = NULL;
    pv2 = dest;
  } else {
    return FALSE;
  }

  if (!dest)
    return TRUE;

  if (min1 < max1) {
    g_value_init (pv1, GST_TYPE_INT_RANGE);
    gst_value_set_int_range_step (pv1, min1, max1, step);
  } else if (min1 == max1) {
    g_value_init (pv1, G_TYPE_INT);
    g_value_set_int (pv1, min1);
  }
  if (min2 < max2) {
    g_value_init (pv2, GST_TYPE_INT_RANGE);
    gst_value_set_int_range_step (pv2, min2, max2, step);
  } else if (min2 == max2) {
    g_value_init (pv2, G_TYPE_INT);
    g_value_set_int (pv2, min2);
  }

  if (min1 <= max1 && min2 <= max2) {
    gst_value_list_concat_and_take_values (dest, pv1, pv2);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_int_range_int (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gint min = gst_value_get_int_range_min (minuend);
  gint max = gst_value_get_int_range_max (minuend);
  gint step = gst_value_get_int_range_step (minuend);
  gint val = g_value_get_int (subtrahend);

  g_return_val_if_fail (min < max, FALSE);

  if (step == 0)
    return FALSE;

  /* value is outside of the range, return range unchanged */
  if (val < min || val > max || val % step) {
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  } else {
    /* max must be MAXINT too as val <= max */
    if (val >= G_MAXINT - step + 1) {
      max -= step;
      val -= step;
    }
    /* min must be MININT too as val >= max */
    if (val <= G_MININT + step - 1) {
      min += step;
      val += step;
    }
    if (dest)
      gst_value_create_new_range (dest, min, val - step, val + step, max, step);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_int_range_int_range (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gint min1 = gst_value_get_int_range_min (minuend);
  gint max1 = gst_value_get_int_range_max (minuend);
  gint step1 = gst_value_get_int_range_step (minuend);
  gint min2 = gst_value_get_int_range_min (subtrahend);
  gint max2 = gst_value_get_int_range_max (subtrahend);
  gint step2 = gst_value_get_int_range_step (subtrahend);
  gint step;

  if (step1 != step2) {
    /* ENOIMPL */
    g_assert (FALSE);
    return FALSE;
  }
  step = step1;

  if (step == 0)
    return FALSE;

  if (max2 >= max1 && min2 <= min1) {
    return FALSE;
  } else if (max2 >= max1) {
    return gst_value_create_new_range (dest, min1, MIN (min2 - step, max1),
        step, 0, step);
  } else if (min2 <= min1) {
    return gst_value_create_new_range (dest, MAX (max2 + step, min1), max1,
        step, 0, step);
  } else {
    return gst_value_create_new_range (dest, min1, MIN (min2 - step, max1),
        MAX (max2 + step, min1), max1, step);
  }
}

static gboolean
gst_value_subtract_int64_int64_range (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gint64 min = gst_value_get_int64_range_min (subtrahend);
  gint64 max = gst_value_get_int64_range_max (subtrahend);
  gint64 step = gst_value_get_int64_range_step (subtrahend);
  gint64 val = g_value_get_int64 (minuend);

  if (step == 0)
    return FALSE;
  /* subtracting a range from an int64 only works if the int64 is not in the
   * range */
  if (val < min || val > max || val % step) {
    /* and the result is the int64 */
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  }
  return FALSE;
}

/* creates a new int64 range based on input values.
 */
static gboolean
gst_value_create_new_int64_range (GValue * dest, gint64 min1, gint64 max1,
    gint64 min2, gint64 max2, gint64 step)
{
  GValue v1 = { 0, };
  GValue v2 = { 0, };
  GValue *pv1, *pv2;            /* yeah, hungarian! */

  g_return_val_if_fail (step > 0, FALSE);
  g_return_val_if_fail (min1 % step == 0, FALSE);
  g_return_val_if_fail (max1 % step == 0, FALSE);
  g_return_val_if_fail (min2 % step == 0, FALSE);
  g_return_val_if_fail (max2 % step == 0, FALSE);

  if (min1 <= max1 && min2 <= max2) {
    pv1 = &v1;
    pv2 = &v2;
  } else if (min1 <= max1) {
    pv1 = dest;
    pv2 = NULL;
  } else if (min2 <= max2) {
    pv1 = NULL;
    pv2 = dest;
  } else {
    return FALSE;
  }

  if (!dest)
    return TRUE;

  if (min1 < max1) {
    g_value_init (pv1, GST_TYPE_INT64_RANGE);
    gst_value_set_int64_range_step (pv1, min1, max1, step);
  } else if (min1 == max1) {
    g_value_init (pv1, G_TYPE_INT64);
    g_value_set_int64 (pv1, min1);
  }
  if (min2 < max2) {
    g_value_init (pv2, GST_TYPE_INT64_RANGE);
    gst_value_set_int64_range_step (pv2, min2, max2, step);
  } else if (min2 == max2) {
    g_value_init (pv2, G_TYPE_INT64);
    g_value_set_int64 (pv2, min2);
  }

  if (min1 <= max1 && min2 <= max2) {
    gst_value_list_concat_and_take_values (dest, pv1, pv2);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_int64_range_int64 (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gint64 min = gst_value_get_int64_range_min (minuend);
  gint64 max = gst_value_get_int64_range_max (minuend);
  gint64 step = gst_value_get_int64_range_step (minuend);
  gint64 val = g_value_get_int64 (subtrahend);

  g_return_val_if_fail (min < max, FALSE);

  if (step == 0)
    return FALSE;

  /* value is outside of the range, return range unchanged */
  if (val < min || val > max || val % step) {
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  } else {
    /* max must be MAXINT64 too as val <= max */
    if (val >= G_MAXINT64 - step + 1) {
      max -= step;
      val -= step;
    }
    /* min must be MININT64 too as val >= max */
    if (val <= G_MININT64 + step - 1) {
      min += step;
      val += step;
    }
    if (dest)
      gst_value_create_new_int64_range (dest, min, val - step, val + step, max,
          step);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_int64_range_int64_range (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  gint64 min1 = gst_value_get_int64_range_min (minuend);
  gint64 max1 = gst_value_get_int64_range_max (minuend);
  gint64 step1 = gst_value_get_int64_range_step (minuend);
  gint64 min2 = gst_value_get_int64_range_min (subtrahend);
  gint64 max2 = gst_value_get_int64_range_max (subtrahend);
  gint64 step2 = gst_value_get_int64_range_step (subtrahend);
  gint64 step;

  if (step1 != step2) {
    /* ENOIMPL */
    g_assert (FALSE);
    return FALSE;
  }

  if (step1 == 0)
    return FALSE;

  step = step1;

  if (max2 >= max1 && min2 <= min1) {
    return FALSE;
  } else if (max2 >= max1) {
    return gst_value_create_new_int64_range (dest, min1, MIN (min2 - step,
            max1), step, 0, step);
  } else if (min2 <= min1) {
    return gst_value_create_new_int64_range (dest, MAX (max2 + step, min1),
        max1, step, 0, step);
  } else {
    return gst_value_create_new_int64_range (dest, min1, MIN (min2 - step,
            max1), MAX (max2 + step, min1), max1, step);
  }
}

static gboolean
gst_value_subtract_double_double_range (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gdouble min = gst_value_get_double_range_min (subtrahend);
  gdouble max = gst_value_get_double_range_max (subtrahend);
  gdouble val = g_value_get_double (minuend);

  if (val < min || val > max) {
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_value_subtract_double_range_double (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  /* since we don't have open ranges, we cannot create a hole in
   * a double range. We return the original range */
  if (dest)
    gst_value_init_and_copy (dest, minuend);
  return TRUE;
}

static gboolean
gst_value_subtract_double_range_double_range (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  /* since we don't have open ranges, we have to approximate */
  /* done like with ints */
  gdouble min1 = gst_value_get_double_range_min (minuend);
  gdouble max2 = gst_value_get_double_range_max (minuend);
  gdouble max1 = MIN (gst_value_get_double_range_min (subtrahend), max2);
  gdouble min2 = MAX (gst_value_get_double_range_max (subtrahend), min1);
  GValue v1 = { 0, };
  GValue v2 = { 0, };
  GValue *pv1, *pv2;            /* yeah, hungarian! */

  if (min1 < max1 && min2 < max2) {
    pv1 = &v1;
    pv2 = &v2;
  } else if (min1 < max1) {
    pv1 = dest;
    pv2 = NULL;
  } else if (min2 < max2) {
    pv1 = NULL;
    pv2 = dest;
  } else {
    return FALSE;
  }

  if (!dest)
    return TRUE;

  if (min1 < max1) {
    g_value_init (pv1, GST_TYPE_DOUBLE_RANGE);
    gst_value_set_double_range (pv1, min1, max1);
  }
  if (min2 < max2) {
    g_value_init (pv2, GST_TYPE_DOUBLE_RANGE);
    gst_value_set_double_range (pv2, min2, max2);
  }

  if (min1 < max1 && min2 < max2) {
    gst_value_list_concat_and_take_values (dest, pv1, pv2);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_from_list (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  guint i, size;
  GValue subtraction = { 0, };
  gboolean ret = FALSE;

  size = VALUE_LIST_SIZE (minuend);
  for (i = 0; i < size; i++) {
    const GValue *cur = VALUE_LIST_GET_VALUE (minuend, i);

    /* quicker version when we can discard the result */
    if (!dest) {
      if (gst_value_subtract (NULL, cur, subtrahend)) {
        ret = TRUE;
        break;
      }
      continue;
    }

    if (gst_value_subtract (&subtraction, cur, subtrahend)) {
      if (!ret) {
        gst_value_move (dest, &subtraction);
        ret = TRUE;
      } else if (G_VALUE_TYPE (dest) == GST_TYPE_LIST
          && G_VALUE_TYPE (&subtraction) != GST_TYPE_LIST) {
        _gst_value_list_append_and_take_value (dest, &subtraction);
      } else {
        GValue temp;

        gst_value_move (&temp, dest);
        gst_value_list_concat_and_take_values (dest, &temp, &subtraction);
      }
    }
  }
  return ret;
}

static gboolean
gst_value_subtract_list (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  guint i, size;
  GValue data[2] = { {0,}, {0,} };
  GValue *subtraction = &data[0], *result = &data[1];

  gst_value_init_and_copy (result, minuend);
  size = VALUE_LIST_SIZE (subtrahend);
  for (i = 0; i < size; i++) {
    const GValue *cur = VALUE_LIST_GET_VALUE (subtrahend, i);

    if (gst_value_subtract (subtraction, result, cur)) {
      GValue *temp = result;

      result = subtraction;
      subtraction = temp;
      g_value_unset (subtraction);
    } else {
      g_value_unset (result);
      return FALSE;
    }
  }
  if (dest) {
    gst_value_move (dest, result);
  } else {
    g_value_unset (result);
  }
  return TRUE;
}

static gboolean
gst_value_subtract_fraction_fraction_range (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  const GValue *min = gst_value_get_fraction_range_min (subtrahend);
  const GValue *max = gst_value_get_fraction_range_max (subtrahend);

  /* subtracting a range from an fraction only works if the fraction
   * is not in the range */
  if (gst_value_compare_fraction (minuend, min) == GST_VALUE_LESS_THAN ||
      gst_value_compare_fraction (minuend, max) == GST_VALUE_GREATER_THAN) {
    /* and the result is the value */
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_value_subtract_fraction_range_fraction (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  /* since we don't have open ranges, we cannot create a hole in
   * a range. We return the original range */
  if (dest)
    gst_value_init_and_copy (dest, minuend);
  return TRUE;
}

static gboolean
gst_value_subtract_fraction_range_fraction_range (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  /* since we don't have open ranges, we have to approximate */
  /* done like with ints and doubles. Creates a list of 2 fraction ranges */
  const GValue *min1 = gst_value_get_fraction_range_min (minuend);
  const GValue *max2 = gst_value_get_fraction_range_max (minuend);
  const GValue *max1 = gst_value_get_fraction_range_min (subtrahend);
  const GValue *min2 = gst_value_get_fraction_range_max (subtrahend);
  gint cmp1, cmp2;
  GValue v1 = { 0, };
  GValue v2 = { 0, };
  GValue *pv1, *pv2;            /* yeah, hungarian! */

  g_return_val_if_fail (min1 != NULL && max1 != NULL, FALSE);
  g_return_val_if_fail (min2 != NULL && max2 != NULL, FALSE);

  cmp1 = gst_value_compare_fraction (max2, max1);
  g_return_val_if_fail (cmp1 != GST_VALUE_UNORDERED, FALSE);
  if (cmp1 == GST_VALUE_LESS_THAN)
    max1 = max2;
  cmp1 = gst_value_compare_fraction (min1, min2);
  g_return_val_if_fail (cmp1 != GST_VALUE_UNORDERED, FALSE);
  if (cmp1 == GST_VALUE_GREATER_THAN)
    min2 = min1;

  cmp1 = gst_value_compare_fraction (min1, max1);
  cmp2 = gst_value_compare_fraction (min2, max2);

  if (cmp1 == GST_VALUE_LESS_THAN && cmp2 == GST_VALUE_LESS_THAN) {
    pv1 = &v1;
    pv2 = &v2;
  } else if (cmp1 == GST_VALUE_LESS_THAN) {
    pv1 = dest;
    pv2 = NULL;
  } else if (cmp2 == GST_VALUE_LESS_THAN) {
    pv1 = NULL;
    pv2 = dest;
  } else {
    return FALSE;
  }

  if (!dest)
    return TRUE;

  if (cmp1 == GST_VALUE_LESS_THAN) {
    g_value_init (pv1, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (pv1, min1, max1);
  }
  if (cmp2 == GST_VALUE_LESS_THAN) {
    g_value_init (pv2, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (pv2, min2, max2);
  }

  if (cmp1 == GST_VALUE_LESS_THAN && cmp2 == GST_VALUE_LESS_THAN) {
    gst_value_list_concat_and_take_values (dest, pv1, pv2);
  }
  return TRUE;
}

/**************
 * comparison *
 **************/

/*
 * gst_value_get_compare_func:
 * @value1: a value to get the compare function for
 *
 * Determines the compare function to be used with values of the same type as
 * @value1. The function can be given to gst_value_compare_with_func().
 *
 * Returns: A #GstValueCompareFunc value
 */
static GstValueCompareFunc
gst_value_get_compare_func (const GValue * value1)
{
  GstValueTable *table, *best = NULL;
  guint i;
  GType type1;

  type1 = G_VALUE_TYPE (value1);

  /* this is a fast check */
  best = gst_value_hash_lookup_type (type1);

  /* slower checks */
  if (G_UNLIKELY (!best || !best->compare)) {
    guint len = gst_value_table->len;

    best = NULL;
    for (i = 0; i < len; i++) {
      table = &g_array_index (gst_value_table, GstValueTable, i);
      if (table->compare && g_type_is_a (type1, table->type)) {
        if (!best || g_type_is_a (table->type, best->type))
          best = table;
      }
    }
  }
  if (G_LIKELY (best))
    return best->compare;

  return NULL;
}

static inline gboolean
gst_value_can_compare_unchecked (const GValue * value1, const GValue * value2)
{
  if (G_VALUE_TYPE (value1) != G_VALUE_TYPE (value2))
    return FALSE;

  return gst_value_get_compare_func (value1) != NULL;
}

/**
 * gst_value_can_compare:
 * @value1: a value to compare
 * @value2: another value to compare
 *
 * Determines if @value1 and @value2 can be compared.
 *
 * Returns: %TRUE if the values can be compared
 */
gboolean
gst_value_can_compare (const GValue * value1, const GValue * value2)
{
  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);

  return gst_value_can_compare_unchecked (value1, value2);
}

static gboolean
gst_value_list_equals_range (const GValue * list, const GValue * value)
{
  const GValue *first;
  guint list_size, n;

  /* TODO: compare against an empty list ? No type though... */
  list_size = VALUE_LIST_SIZE (list);
  if (list_size == 0)
    return FALSE;

  /* compare the basic types - they have to match */
  first = VALUE_LIST_GET_VALUE (list, 0);
#define CHECK_TYPES(type,prefix) \
  ((first) && G_VALUE_TYPE(first) == prefix##_TYPE_##type && GST_VALUE_HOLDS_##type##_RANGE (value))
  if (CHECK_TYPES (INT, G)) {
    const gint rmin = gst_value_get_int_range_min (value);
    const gint rmax = gst_value_get_int_range_max (value);
    const gint rstep = gst_value_get_int_range_step (value);
    if (rstep == 0)
      return FALSE;
    /* note: this will overflow for min 0 and max INT_MAX, but this
       would only be equal to a list of INT_MAX elements, which seems
       very unlikely */
    if (list_size != rmax / rstep - rmin / rstep + 1)
      return FALSE;
    for (n = 0; n < list_size; ++n) {
      gint v = g_value_get_int (VALUE_LIST_GET_VALUE (list, n));
      if (v < rmin || v > rmax || v % rstep) {
        return FALSE;
      }
    }
    return TRUE;
  } else if (CHECK_TYPES (INT64, G)) {
    const gint64 rmin = gst_value_get_int64_range_min (value);
    const gint64 rmax = gst_value_get_int64_range_max (value);
    const gint64 rstep = gst_value_get_int64_range_step (value);
    GST_DEBUG ("List/range of int64s");
    if (rstep == 0)
      return FALSE;
    if (list_size != rmax / rstep - rmin / rstep + 1)
      return FALSE;
    for (n = 0; n < list_size; ++n) {
      gint64 v = g_value_get_int64 (VALUE_LIST_GET_VALUE (list, n));
      if (v < rmin || v > rmax || v % rstep)
        return FALSE;
    }
    return TRUE;
  }
#undef CHECK_TYPES

  /* other combinations don't make sense for equality */
  return FALSE;
}

/* "Pure" variant of gst_value_compare which is guaranteed to
 * not have list arguments and therefore does basic comparisons
 */
static inline gint
_gst_value_compare_nolist (const GValue * value1, const GValue * value2)
{
  GstValueCompareFunc compare;

  if (G_VALUE_TYPE (value1) != G_VALUE_TYPE (value2))
    return GST_VALUE_UNORDERED;

  compare = gst_value_get_compare_func (value1);
  if (compare) {
    return compare (value1, value2);
  }

  g_critical ("unable to compare values of type %s\n",
      g_type_name (G_VALUE_TYPE (value1)));
  return GST_VALUE_UNORDERED;
}

/**
 * gst_value_compare:
 * @value1: a value to compare
 * @value2: another value to compare
 *
 * Compares @value1 and @value2.  If @value1 and @value2 cannot be
 * compared, the function returns GST_VALUE_UNORDERED.  Otherwise,
 * if @value1 is greater than @value2, GST_VALUE_GREATER_THAN is returned.
 * If @value1 is less than @value2, GST_VALUE_LESS_THAN is returned.
 * If the values are equal, GST_VALUE_EQUAL is returned.
 *
 * Returns: comparison result
 */
gint
gst_value_compare (const GValue * value1, const GValue * value2)
{
  gboolean value1_is_list;
  gboolean value2_is_list;

  g_return_val_if_fail (G_IS_VALUE (value1), GST_VALUE_LESS_THAN);
  g_return_val_if_fail (G_IS_VALUE (value2), GST_VALUE_GREATER_THAN);

  value1_is_list = G_VALUE_TYPE (value1) == GST_TYPE_LIST;
  value2_is_list = G_VALUE_TYPE (value2) == GST_TYPE_LIST;

  /* Special cases: lists and scalar values ("{ 1 }" and "1" are equal),
     as well as lists and ranges ("{ 1, 2 }" and "[ 1, 2 ]" are equal) */
  if (value1_is_list && !value2_is_list) {
    gint i, n, ret;

    if (gst_value_list_equals_range (value1, value2)) {
      return GST_VALUE_EQUAL;
    }

    n = gst_value_list_get_size (value1);
    if (n == 0)
      return GST_VALUE_UNORDERED;

    for (i = 0; i < n; i++) {
      const GValue *elt;

      elt = gst_value_list_get_value (value1, i);
      ret = gst_value_compare (elt, value2);
      if (ret != GST_VALUE_EQUAL && n == 1)
        return ret;
      else if (ret != GST_VALUE_EQUAL)
        return GST_VALUE_UNORDERED;
    }

    return GST_VALUE_EQUAL;
  } else if (value2_is_list && !value1_is_list) {
    gint i, n, ret;

    if (gst_value_list_equals_range (value2, value1)) {
      return GST_VALUE_EQUAL;
    }

    n = gst_value_list_get_size (value2);
    if (n == 0)
      return GST_VALUE_UNORDERED;

    for (i = 0; i < n; i++) {
      const GValue *elt;

      elt = gst_value_list_get_value (value2, i);
      ret = gst_value_compare (elt, value1);
      if (ret != GST_VALUE_EQUAL && n == 1)
        return ret;
      else if (ret != GST_VALUE_EQUAL)
        return GST_VALUE_UNORDERED;
    }

    return GST_VALUE_EQUAL;
  }

  /* And now handle the generic case */
  return _gst_value_compare_nolist (value1, value2);
}

/* union */

/**
 * gst_value_can_union:
 * @value1: a value to union
 * @value2: another value to union
 *
 * Determines if @value1 and @value2 can be non-trivially unioned.
 * Any two values can be trivially unioned by adding both of them
 * to a GstValueList.  However, certain types have the possibility
 * to be unioned in a simpler way.  For example, an integer range
 * and an integer can be unioned if the integer is a subset of the
 * integer range.  If there is the possibility that two values can
 * be unioned, this function returns %TRUE.
 *
 * Returns: %TRUE if there is a function allowing the two values to
 * be unioned.
 */
gboolean
gst_value_can_union (const GValue * value1, const GValue * value2)
{
  GstValueUnionInfo *union_info;
  guint i, len;

  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);

  len = gst_value_union_funcs->len;

  for (i = 0; i < len; i++) {
    union_info = &g_array_index (gst_value_union_funcs, GstValueUnionInfo, i);
    if (union_info->type1 == G_VALUE_TYPE (value1) &&
        union_info->type2 == G_VALUE_TYPE (value2))
      return TRUE;
    if (union_info->type1 == G_VALUE_TYPE (value2) &&
        union_info->type2 == G_VALUE_TYPE (value1))
      return TRUE;
  }

  return FALSE;
}

/**
 * gst_value_union:
 * @dest: (out caller-allocates): the destination value
 * @value1: a value to union
 * @value2: another value to union
 *
 * Creates a GValue corresponding to the union of @value1 and @value2.
 *
 * Returns: %TRUE if the union succeeded.
 */
gboolean
gst_value_union (GValue * dest, const GValue * value1, const GValue * value2)
{
  const GstValueUnionInfo *union_info;
  guint i, len;
  GType type1, type2;

  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);
  g_return_val_if_fail (gst_value_list_or_array_are_compatible (value1, value2),
      FALSE);

  len = gst_value_union_funcs->len;
  type1 = G_VALUE_TYPE (value1);
  type2 = G_VALUE_TYPE (value2);

  for (i = 0; i < len; i++) {
    union_info = &g_array_index (gst_value_union_funcs, GstValueUnionInfo, i);
    if (union_info->type1 == type1 && union_info->type2 == type2) {
      return union_info->func (dest, value1, value2);
    }
    if (union_info->type1 == type2 && union_info->type2 == type1) {
      return union_info->func (dest, value2, value1);
    }
  }

  gst_value_list_concat (dest, value1, value2);
  return TRUE;
}

/* gst_value_register_union_func: (skip)
 * @type1: a type to union
 * @type2: another type to union
 * @func: a function that implements creating a union between the two types
 *
 * Registers a union function that can create a union between #GValue items
 * of the type @type1 and @type2.
 *
 * Union functions should be registered at startup before any pipelines are
 * started, as gst_value_register_union_func() is not thread-safe and cannot
 * be used at the same time as gst_value_union() or gst_value_can_union().
 */
static void
gst_value_register_union_func (GType type1, GType type2, GstValueUnionFunc func)
{
  GstValueUnionInfo union_info;

  union_info.type1 = type1;
  union_info.type2 = type2;
  union_info.func = func;

  g_array_append_val (gst_value_union_funcs, union_info);
}

/* intersection */

/**
 * gst_value_can_intersect:
 * @value1: a value to intersect
 * @value2: another value to intersect
 *
 * Determines if intersecting two values will produce a valid result.
 * Two values will produce a valid intersection if they have the same
 * type.
 *
 * Returns: %TRUE if the values can intersect
 */
gboolean
gst_value_can_intersect (const GValue * value1, const GValue * value2)
{
  GstValueIntersectInfo *intersect_info;
  guint i, len;
  GType type1, type2;

  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);

  type1 = G_VALUE_TYPE (value1);
  type2 = G_VALUE_TYPE (value2);

  /* practically all GstValue types have a compare function (_can_compare=TRUE)
   * GstStructure and GstCaps have not, but are intersectable */
  if (type1 == type2)
    return TRUE;

  /* special cases */
  if (type1 == GST_TYPE_LIST || type2 == GST_TYPE_LIST)
    return TRUE;

  if (G_UNLIKELY (GST_VALUE_HOLDS_FLAG_SET (value1) &&
          GST_VALUE_HOLDS_FLAG_SET (value2))) {
    GType flagset_type;

    flagset_type = GST_TYPE_FLAG_SET;

    /* Allow intersection with the generic FlagSet type, on one
     * side, but not 2 different subtypes - that makes no sense */
    if (type1 == flagset_type || type2 == flagset_type)
      return TRUE;
  }

  /* check registered intersect functions (only different gtype are checked at
   * this point) */
  len = gst_value_intersect_funcs->len;
  for (i = 0; i < len; i++) {
    intersect_info = &g_array_index (gst_value_intersect_funcs,
        GstValueIntersectInfo, i);
    if ((intersect_info->type1 == type1 && intersect_info->type2 == type2) ||
        (intersect_info->type1 == type2 && intersect_info->type2 == type1))
      return TRUE;
  }

  return gst_value_can_compare_unchecked (value1, value2);
}

/**
 * gst_value_intersect:
 * @dest: (out caller-allocates) (transfer full) (allow-none):
 *   a uninitialized #GValue that will hold the calculated
 *   intersection value. May be %NULL if the resulting set if not
 *   needed.
 * @value1: a value to intersect
 * @value2: another value to intersect
 *
 * Calculates the intersection of two values.  If the values have
 * a non-empty intersection, the value representing the intersection
 * is placed in @dest, unless %NULL.  If the intersection is non-empty,
 * @dest is not modified.
 *
 * Returns: %TRUE if the intersection is non-empty
 */
gboolean
gst_value_intersect (GValue * dest, const GValue * value1,
    const GValue * value2)
{
  GstValueIntersectInfo *intersect_info;
  guint i, len;
  GType type1, type2;

  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);

  type1 = G_VALUE_TYPE (value1);
  type2 = G_VALUE_TYPE (value2);

  /* special cases first */
  if (type1 == GST_TYPE_LIST)
    return gst_value_intersect_list (dest, value1, value2);
  if (type2 == GST_TYPE_LIST)
    return gst_value_intersect_list (dest, value2, value1);

  if (_gst_value_compare_nolist (value1, value2) == GST_VALUE_EQUAL) {
    if (dest)
      gst_value_init_and_copy (dest, value1);
    return TRUE;
  }

  if (type1 == type2) {
    /* Equal type comparison */
    if (type1 == GST_TYPE_INT_RANGE)
      return gst_value_intersect_int_range_int_range (dest, value1, value2);
    if (type1 == GST_TYPE_INT64_RANGE)
      return gst_value_intersect_int64_range_int64_range (dest, value1, value2);
    if (type1 == GST_TYPE_DOUBLE_RANGE)
      return gst_value_intersect_double_range_double_range (dest, value1,
          value2);
    if (type1 == GST_TYPE_ARRAY)
      return gst_value_intersect_array (dest, value1, value2);
    if (type1 == GST_TYPE_FRACTION_RANGE)
      return gst_value_intersect_fraction_range_fraction_range (dest, value1,
          value2);
    if (type1 == GST_TYPE_FLAG_SET)
      return gst_value_intersect_flagset_flagset (dest, value1, value2);
    if (type1 == GST_TYPE_STRUCTURE)
      return gst_value_intersect_structure_structure (dest, value1, value2);
  } else {
    /* Different type comparison */
    len = gst_value_intersect_funcs->len;
    for (i = 0; i < len; i++) {
      intersect_info = &g_array_index (gst_value_intersect_funcs,
          GstValueIntersectInfo, i);
      if (intersect_info->type1 == type1 && intersect_info->type2 == type2) {
        return intersect_info->func (dest, value1, value2);
      }
      if (intersect_info->type1 == type2 && intersect_info->type2 == type1) {
        return intersect_info->func (dest, value2, value1);
      }
    }
  }

  /* Failed to find a direct intersection, check if these are
   * GstFlagSet sub-types. */
  if (G_UNLIKELY (GST_VALUE_HOLDS_FLAG_SET (value1) &&
          GST_VALUE_HOLDS_FLAG_SET (value2))) {
    return gst_value_intersect_flagset_flagset (dest, value1, value2);
  }

  return FALSE;
}



/* gst_value_register_intersect_func: (skip)
 * @type1: the first type to intersect
 * @type2: the second type to intersect
 * @func: the intersection function
 *
 * Registers a function that is called to calculate the intersection
 * of the values having the types @type1 and @type2.
 *
 * Intersect functions should be registered at startup before any pipelines are
 * started, as gst_value_register_intersect_func() is not thread-safe and
 * cannot be used at the same time as gst_value_intersect() or
 * gst_value_can_intersect().
 */
static void
gst_value_register_intersect_func (GType type1, GType type2,
    GstValueIntersectFunc func)
{
  GstValueIntersectInfo intersect_info;

  intersect_info.type1 = type1;
  intersect_info.type2 = type2;
  intersect_info.func = func;

  g_array_append_val (gst_value_intersect_funcs, intersect_info);
}


/* subtraction */

/**
 * gst_value_subtract:
 * @dest: (out caller-allocates) (allow-none): the destination value
 *     for the result if the subtraction is not empty. May be %NULL,
 *     in which case the resulting set will not be computed, which can
 *     give a fair speedup.
 * @minuend: the value to subtract from
 * @subtrahend: the value to subtract
 *
 * Subtracts @subtrahend from @minuend and stores the result in @dest.
 * Note that this means subtraction as in sets, not as in mathematics.
 *
 * Returns: %TRUE if the subtraction is not empty
 */
gboolean
gst_value_subtract (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  GstValueSubtractInfo *info;
  guint i, len;
  GType mtype, stype;

  g_return_val_if_fail (G_IS_VALUE (minuend), FALSE);
  g_return_val_if_fail (G_IS_VALUE (subtrahend), FALSE);

  mtype = G_VALUE_TYPE (minuend);
  stype = G_VALUE_TYPE (subtrahend);

  /* special cases first */
  if (mtype == GST_TYPE_LIST)
    return gst_value_subtract_from_list (dest, minuend, subtrahend);
  if (stype == GST_TYPE_LIST)
    return gst_value_subtract_list (dest, minuend, subtrahend);

  len = gst_value_subtract_funcs->len;
  for (i = 0; i < len; i++) {
    info = &g_array_index (gst_value_subtract_funcs, GstValueSubtractInfo, i);
    if (info->minuend == mtype && info->subtrahend == stype) {
      return info->func (dest, minuend, subtrahend);
    }
  }

  if (_gst_value_compare_nolist (minuend, subtrahend) != GST_VALUE_EQUAL) {
    if (dest)
      gst_value_init_and_copy (dest, minuend);
    return TRUE;
  }

  return FALSE;
}

#if 0
gboolean
gst_value_subtract (GValue * dest, const GValue * minuend,
    const GValue * subtrahend)
{
  gboolean ret = gst_value_subtract2 (dest, minuend, subtrahend);

  g_printerr ("\"%s\"  -  \"%s\"  =  \"%s\"\n", gst_value_serialize (minuend),
      gst_value_serialize (subtrahend),
      ret ? gst_value_serialize (dest) : "---");
  return ret;
}
#endif

/**
 * gst_value_can_subtract:
 * @minuend: the value to subtract from
 * @subtrahend: the value to subtract
 *
 * Checks if it's possible to subtract @subtrahend from @minuend.
 *
 * Returns: %TRUE if a subtraction is possible
 */
gboolean
gst_value_can_subtract (const GValue * minuend, const GValue * subtrahend)
{
  GstValueSubtractInfo *info;
  guint i, len;
  GType mtype, stype;

  g_return_val_if_fail (G_IS_VALUE (minuend), FALSE);
  g_return_val_if_fail (G_IS_VALUE (subtrahend), FALSE);

  mtype = G_VALUE_TYPE (minuend);
  stype = G_VALUE_TYPE (subtrahend);

  /* special cases */
  if (mtype == GST_TYPE_LIST || stype == GST_TYPE_LIST)
    return TRUE;
  if (mtype == GST_TYPE_STRUCTURE || stype == GST_TYPE_STRUCTURE)
    return FALSE;

  len = gst_value_subtract_funcs->len;
  for (i = 0; i < len; i++) {
    info = &g_array_index (gst_value_subtract_funcs, GstValueSubtractInfo, i);
    if (info->minuend == mtype && info->subtrahend == stype)
      return TRUE;
  }

  return gst_value_can_compare_unchecked (minuend, subtrahend);
}

/* gst_value_register_subtract_func: (skip)
 * @minuend_type: type of the minuend
 * @subtrahend_type: type of the subtrahend
 * @func: function to use
 *
 * Registers @func as a function capable of subtracting the values of
 * @subtrahend_type from values of @minuend_type.
 *
 * Subtract functions should be registered at startup before any pipelines are
 * started, as gst_value_register_subtract_func() is not thread-safe and
 * cannot be used at the same time as gst_value_subtract().
 */
static void
gst_value_register_subtract_func (GType minuend_type, GType subtrahend_type,
    GstValueSubtractFunc func)
{
  GstValueSubtractInfo info;

  g_return_if_fail (!gst_type_is_fixed (minuend_type)
      || !gst_type_is_fixed (subtrahend_type));

  info.minuend = minuend_type;
  info.subtrahend = subtrahend_type;
  info.func = func;

  g_array_append_val (gst_value_subtract_funcs, info);
}

/**
 * gst_value_register:
 * @table: structure containing functions to register
 *
 * Registers functions to perform calculations on #GValue items of a given
 * type. Each type can only be added once.
 */
void
gst_value_register (const GstValueTable * table)
{
  GstValueTable *found;

  g_return_if_fail (table != NULL);

  g_array_append_val (gst_value_table, *table);

  found = gst_value_hash_lookup_type (table->type);
  if (found)
    g_warning ("adding type %s multiple times", g_type_name (table->type));

  /* FIXME: we're not really doing the const justice, we assume the table is
   * static */
  gst_value_hash_add_type (table->type, table);
}

/**
 * gst_value_init_and_copy:
 * @dest: (out caller-allocates): the target value
 * @src: the source value
 *
 * Initialises the target value to be of the same type as source and then copies
 * the contents from source to target.
 */
void
gst_value_init_and_copy (GValue * dest, const GValue * src)
{
  GType type;

  g_return_if_fail (G_IS_VALUE (src));
  g_return_if_fail (dest != NULL);

  type = G_VALUE_TYPE (src);
  /* We need to shortcut GstValueList/GstValueArray copying because:
   * * g_value_init would end up allocating something
   * * which g_value_copy would then free and re-alloc.
   *
   * Instead directly call the copy */
  if (type == GST_TYPE_LIST || type == GST_TYPE_ARRAY) {
    dest->g_type = type;
    gst_value_copy_list_or_array (src, dest);
    return;
  }

  g_value_init (dest, type);
  g_value_copy (src, dest);
}

/* move src into dest and clear src */
static void
gst_value_move (GValue * dest, GValue * src)
{
  g_assert (G_IS_VALUE (src));
  g_assert (dest != NULL);

  *dest = *src;
  memset (src, 0, sizeof (GValue));
}

/**
 * gst_value_serialize:
 * @value: a #GValue to serialize
 *
 * tries to transform the given @value into a string representation that allows
 * getting back this string later on using gst_value_deserialize().
 *
 * Free-function: g_free
 *
 * Returns: (transfer full) (nullable): the serialization for @value
 * or %NULL if none exists
 */
gchar *
gst_value_serialize (const GValue * value)
{
  guint i, len;
  GValue s_val = { 0 };
  GstValueTable *table, *best;
  gchar *s;
  GType type;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  type = G_VALUE_TYPE (value);

  best = gst_value_hash_lookup_type (type);

  if (G_UNLIKELY (!best || !best->serialize)) {
    len = gst_value_table->len;
    best = NULL;
    for (i = 0; i < len; i++) {
      table = &g_array_index (gst_value_table, GstValueTable, i);
      if (table->serialize && g_type_is_a (type, table->type)) {
        if (!best || g_type_is_a (table->type, best->type))
          best = table;
      }
    }
  }
  if (G_LIKELY (best))
    return best->serialize (value);

  g_value_init (&s_val, G_TYPE_STRING);
  if (g_value_transform (value, &s_val)) {
    s = gst_string_wrap (g_value_get_string (&s_val));
  } else {
    s = NULL;
  }
  g_value_unset (&s_val);

  return s;
}

/**
 * gst_value_deserialize:
 * @dest: (out caller-allocates): #GValue to fill with contents of
 *     deserialization
 * @src: string to deserialize
 *
 * Tries to deserialize a string into the type specified by the given GValue.
 * If the operation succeeds, %TRUE is returned, %FALSE otherwise.
 *
 * Returns: %TRUE on success
 */
gboolean
gst_value_deserialize (GValue * dest, const gchar * src)
{
  GstValueTable *table, *best;
  guint i, len;
  GType type;

  g_return_val_if_fail (src != NULL, FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest), FALSE);

  type = G_VALUE_TYPE (dest);

  best = gst_value_hash_lookup_type (type);
  if (G_UNLIKELY (!best || (!best->deserialize
              && !best->deserialize_with_pspec))) {
    len = gst_value_table->len;
    best = NULL;
    for (i = 0; i < len; i++) {
      table = &g_array_index (gst_value_table, GstValueTable, i);
      if ((table->deserialize || table->deserialize_with_pspec) &&
          g_type_is_a (type, table->type)) {
        if (!best || g_type_is_a (table->type, best->type))
          best = table;
      }
    }
  }
  if (G_LIKELY (best)) {
    if (best->deserialize_with_pspec)
      return best->deserialize_with_pspec (dest, src, NULL);
    else
      return best->deserialize (dest, src);
  }

  return FALSE;
}

/**
 * gst_value_deserialize_with_pspec:
 * @dest: (out caller-allocates): #GValue to fill with contents of
 *     deserialization
 * @src: string to deserialize
 * @pspec: (nullable): the #GParamSpec describing the expected value
 *
 * Tries to deserialize a string into the type specified by the given GValue.
 * @pspec may be used to guide the deserializing of nested members.
 * If the operation succeeds, %TRUE is returned, %FALSE otherwise.
 *
 * Returns: %TRUE on success
 * Since: 1.20
 */
gboolean
gst_value_deserialize_with_pspec (GValue * dest, const gchar * src,
    GParamSpec * pspec)
{
  GstValueTable *table, *best;
  guint i, len;
  GType type;

  g_return_val_if_fail (src != NULL, FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest), FALSE);

  if (pspec)
    g_return_val_if_fail (G_VALUE_TYPE (dest) ==
        G_PARAM_SPEC_VALUE_TYPE (pspec), FALSE);

  type = G_VALUE_TYPE (dest);

  best = gst_value_hash_lookup_type (type);
  if (G_UNLIKELY (!best || (!best->deserialize
              && !best->deserialize_with_pspec))) {
    len = gst_value_table->len;
    best = NULL;
    for (i = 0; i < len; i++) {
      table = &g_array_index (gst_value_table, GstValueTable, i);
      if ((table->deserialize || table->deserialize_with_pspec) &&
          g_type_is_a (type, table->type)) {
        if (!best || g_type_is_a (table->type, best->type))
          best = table;
      }
    }
  }
  if (G_LIKELY (best)) {
    if (best->deserialize_with_pspec)
      return best->deserialize_with_pspec (dest, src, pspec);
    else
      return best->deserialize (dest, src);
  }

  return FALSE;
}

static gboolean
structure_field_is_fixed (GQuark field_id, const GValue * val,
    gpointer user_data)
{
  return gst_value_is_fixed (val);
}

/**
 * gst_value_is_fixed:
 * @value: the #GValue to check
 *
 * Tests if the given GValue, if available in a GstStructure (or any other
 * container) contains a "fixed" (which means: one value) or an "unfixed"
 * (which means: multiple possible values, such as data lists or data
 * ranges) value.
 *
 * Returns: true if the value is "fixed".
 */

gboolean
gst_value_is_fixed (const GValue * value)
{
  GType type;

  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  type = G_VALUE_TYPE (value);

  /* the most common types are just basic plain glib types */
  if (type <= G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_GLIB_LAST)) {
    return TRUE;
  }

  if (type == GST_TYPE_ARRAY) {
    gint size, n;
    const GValue *kid;

    /* check recursively */
    size = gst_value_array_get_size (value);
    for (n = 0; n < size; n++) {
      kid = gst_value_array_get_value (value, n);
      if (!gst_value_is_fixed (kid))
        return FALSE;
    }
    return TRUE;
  } else if (GST_VALUE_HOLDS_FLAG_SET (value)) {
    /* Flagsets are only fixed if there are no 'don't care' bits */
    return (gst_value_get_flagset_mask (value) == GST_FLAG_SET_MASK_EXACT);
  } else if (GST_VALUE_HOLDS_STRUCTURE (value)) {
    return gst_structure_foreach (gst_value_get_structure (value),
        structure_field_is_fixed, NULL);
  }
  return gst_type_is_fixed (type);
}

/**
 * gst_value_fixate:
 * @dest: the #GValue destination
 * @src: the #GValue to fixate
 *
 * Fixate @src into a new value @dest.
 * For ranges, the first element is taken. For lists and arrays, the
 * first item is fixated and returned.
 * If @src is already fixed, this function returns %FALSE.
 *
 * Returns: %TRUE if @dest contains a fixated version of @src.
 */
gboolean
gst_value_fixate (GValue * dest, const GValue * src)
{
  g_return_val_if_fail (G_IS_VALUE (src), FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  if (G_VALUE_TYPE (src) == GST_TYPE_INT_RANGE) {
    g_value_init (dest, G_TYPE_INT);
    g_value_set_int (dest, gst_value_get_int_range_min (src));
  } else if (G_VALUE_TYPE (src) == GST_TYPE_DOUBLE_RANGE) {
    g_value_init (dest, G_TYPE_DOUBLE);
    g_value_set_double (dest, gst_value_get_double_range_min (src));
  } else if (G_VALUE_TYPE (src) == GST_TYPE_FRACTION_RANGE) {
    gst_value_init_and_copy (dest, gst_value_get_fraction_range_min (src));
  } else if (G_VALUE_TYPE (src) == GST_TYPE_LIST) {
    GValue temp = { 0 };

    /* list could be empty */
    if (gst_value_list_get_size (src) <= 0)
      return FALSE;

    gst_value_init_and_copy (&temp, gst_value_list_get_value (src, 0));

    if (!gst_value_fixate (dest, &temp)) {
      gst_value_move (dest, &temp);
    } else {
      g_value_unset (&temp);
    }
  } else if (G_VALUE_TYPE (src) == GST_TYPE_ARRAY) {
    gboolean res = FALSE;
    guint n, len;

    len = gst_value_array_get_size (src);
    g_value_init (dest, GST_TYPE_ARRAY);
    for (n = 0; n < len; n++) {
      GValue kid = { 0 };
      const GValue *orig_kid = gst_value_array_get_value (src, n);

      if (!gst_value_fixate (&kid, orig_kid))
        gst_value_init_and_copy (&kid, orig_kid);
      else
        res = TRUE;
      _gst_value_array_append_and_take_value (dest, &kid);
    }

    if (!res)
      g_value_unset (dest);

    return res;
  } else if (GST_VALUE_HOLDS_FLAG_SET (src)) {
    guint flags;

    if (gst_value_get_flagset_mask (src) == GST_FLAG_SET_MASK_EXACT)
      return FALSE;             /* Already fixed */

    flags = gst_value_get_flagset_flags (src);
    g_value_init (dest, G_VALUE_TYPE (src));
    gst_value_set_flagset (dest, flags, GST_FLAG_SET_MASK_EXACT);
    return TRUE;
  } else if (GST_VALUE_HOLDS_STRUCTURE (src)) {
    const GstStructure *str = (GstStructure *) gst_value_get_structure (src);
    GstStructure *kid;

    if (!str)
      return FALSE;

    kid = gst_structure_copy (str);
    gst_structure_fixate (kid);
    g_value_init (dest, GST_TYPE_STRUCTURE);
    gst_value_set_structure (dest, kid);
    gst_structure_free (kid);
    return TRUE;
  } else {
    return FALSE;
  }
  return TRUE;
}


/************
 * fraction *
 ************/

/* helper functions */
static void
gst_value_init_fraction (GValue * value)
{
  value->data[0].v_int = 0;
  value->data[1].v_int = 1;
}

static void
gst_value_copy_fraction (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_int = src_value->data[0].v_int;
  dest_value->data[1].v_int = src_value->data[1].v_int;
}

static gchar *
gst_value_collect_fraction (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  g_return_val_if_fail (n_collect_values == 2,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[1].v_int != 0,
      g_strdup_printf ("passed '0' as denominator for `%s'",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[0].v_int >= -G_MAXINT,
      g_strdup_printf
      ("passed value smaller than -G_MAXINT as numerator for `%s'",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (collect_values[1].v_int >= -G_MAXINT,
      g_strdup_printf
      ("passed value smaller than -G_MAXINT as denominator for `%s'",
          G_VALUE_TYPE_NAME (value)));

  gst_value_set_fraction (value,
      collect_values[0].v_int, collect_values[1].v_int);

  return NULL;
}

static gchar *
gst_value_lcopy_fraction (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  gint *numerator = collect_values[0].v_pointer;
  gint *denominator = collect_values[1].v_pointer;

  g_return_val_if_fail (numerator != NULL,
      g_strdup_printf ("numerator for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));
  g_return_val_if_fail (denominator != NULL,
      g_strdup_printf ("denominator for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));

  *numerator = value->data[0].v_int;
  *denominator = value->data[1].v_int;

  return NULL;
}

/**
 * gst_value_set_fraction:
 * @value: a GValue initialized to #GST_TYPE_FRACTION
 * @numerator: the numerator of the fraction
 * @denominator: the denominator of the fraction
 *
 * Sets @value to the fraction specified by @numerator over @denominator.
 * The fraction gets reduced to the smallest numerator and denominator,
 * and if necessary the sign is moved to the numerator.
 */
void
gst_value_set_fraction (GValue * value, gint numerator, gint denominator)
{
  gint gcd = 0;

  g_return_if_fail (GST_VALUE_HOLDS_FRACTION (value));
  g_return_if_fail (denominator != 0);
  g_return_if_fail (denominator >= -G_MAXINT);
  g_return_if_fail (numerator >= -G_MAXINT);

  /* normalize sign */
  if (denominator < 0) {
    numerator = -numerator;
    denominator = -denominator;
  }

  /* check for reduction */
  gcd = gst_util_greatest_common_divisor (numerator, denominator);
  if (gcd) {
    numerator /= gcd;
    denominator /= gcd;
  }

  g_assert (denominator > 0);

  value->data[0].v_int = numerator;
  value->data[1].v_int = denominator;
}

/**
 * gst_value_get_fraction_numerator:
 * @value: a GValue initialized to #GST_TYPE_FRACTION
 *
 * Gets the numerator of the fraction specified by @value.
 *
 * Returns: the numerator of the fraction.
 */
gint
gst_value_get_fraction_numerator (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (value), 0);

  return value->data[0].v_int;
}

/**
 * gst_value_get_fraction_denominator:
 * @value: a GValue initialized to #GST_TYPE_FRACTION
 *
 * Gets the denominator of the fraction specified by @value.
 *
 * Returns: the denominator of the fraction.
 */
gint
gst_value_get_fraction_denominator (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (value), 1);

  return value->data[1].v_int;
}

/**
 * gst_value_fraction_multiply:
 * @product: a GValue initialized to #GST_TYPE_FRACTION
 * @factor1: a GValue initialized to #GST_TYPE_FRACTION
 * @factor2: a GValue initialized to #GST_TYPE_FRACTION
 *
 * Multiplies the two #GValue items containing a #GST_TYPE_FRACTION and sets
 * @product to the product of the two fractions.
 *
 * Returns: %FALSE in case of an error (like integer overflow), %TRUE otherwise.
 */
gboolean
gst_value_fraction_multiply (GValue * product, const GValue * factor1,
    const GValue * factor2)
{
  gint n1, n2, d1, d2;
  gint res_n, res_d;

  g_return_val_if_fail (product != NULL, FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (factor1), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (factor2), FALSE);

  n1 = factor1->data[0].v_int;
  n2 = factor2->data[0].v_int;
  d1 = factor1->data[1].v_int;
  d2 = factor2->data[1].v_int;

  if (!gst_util_fraction_multiply (n1, d1, n2, d2, &res_n, &res_d))
    return FALSE;

  gst_value_set_fraction (product, res_n, res_d);

  return TRUE;
}

/**
 * gst_value_fraction_subtract:
 * @dest: a GValue initialized to #GST_TYPE_FRACTION
 * @minuend: a GValue initialized to #GST_TYPE_FRACTION
 * @subtrahend: a GValue initialized to #GST_TYPE_FRACTION
 *
 * Subtracts the @subtrahend from the @minuend and sets @dest to the result.
 *
 * Returns: %FALSE in case of an error (like integer overflow), %TRUE otherwise.
 */
gboolean
gst_value_fraction_subtract (GValue * dest,
    const GValue * minuend, const GValue * subtrahend)
{
  gint n1, n2, d1, d2;
  gint res_n, res_d;

  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (minuend), FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_FRACTION (subtrahend), FALSE);

  n1 = minuend->data[0].v_int;
  n2 = subtrahend->data[0].v_int;
  d1 = minuend->data[1].v_int;
  d2 = subtrahend->data[1].v_int;

  if (!gst_util_fraction_add (n1, d1, -n2, d2, &res_n, &res_d))
    return FALSE;
  gst_value_set_fraction (dest, res_n, res_d);

  return TRUE;
}

static gchar *
gst_value_serialize_fraction (const GValue * value)
{
  gint32 numerator = value->data[0].v_int;
  gint32 denominator = value->data[1].v_int;
  gboolean positive = TRUE;

  /* get the sign and make components absolute */
  if (numerator < 0) {
    numerator = -numerator;
    positive = !positive;
  }
  if (denominator < 0) {
    denominator = -denominator;
    positive = !positive;
  }

  return g_strdup_printf ("%s%d/%d",
      positive ? "" : "-", numerator, denominator);
}

static gboolean
gst_value_deserialize_fraction (GValue * dest, const gchar * s)
{
  gint num, den;
  gint num_chars;

  if (G_UNLIKELY (s == NULL))
    return FALSE;

  if (G_UNLIKELY (dest == NULL || !GST_VALUE_HOLDS_FRACTION (dest)))
    return FALSE;

  if (sscanf (s, "%d/%d%n", &num, &den, &num_chars) >= 2) {
    if (s[num_chars] != 0)
      return FALSE;
    if (den == 0)
      return FALSE;

    gst_value_set_fraction (dest, num, den);
    return TRUE;
  } else if (g_ascii_strcasecmp (s, "1/max") == 0) {
    gst_value_set_fraction (dest, 1, G_MAXINT);
    return TRUE;
  } else if (sscanf (s, "%d%n", &num, &num_chars) >= 1) {
    if (s[num_chars] != 0)
      return FALSE;
    gst_value_set_fraction (dest, num, 1);
    return TRUE;
  } else if (g_ascii_strcasecmp (s, "min") == 0) {
    gst_value_set_fraction (dest, -G_MAXINT, 1);
    return TRUE;
  } else if (g_ascii_strcasecmp (s, "max") == 0) {
    gst_value_set_fraction (dest, G_MAXINT, 1);
    return TRUE;
  }

  return FALSE;
}

static void
gst_value_transform_fraction_string (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_pointer = gst_value_serialize_fraction (src_value);
}

static void
gst_value_transform_string_fraction (const GValue * src_value,
    GValue * dest_value)
{
  if (!gst_value_deserialize_fraction (dest_value,
          src_value->data[0].v_pointer))
    /* If the deserialize fails, ensure we leave the fraction in a
     * valid, if incorrect, state */
    gst_value_set_fraction (dest_value, 0, 1);
}

static void
gst_value_transform_double_fraction (const GValue * src_value,
    GValue * dest_value)
{
  gdouble src = g_value_get_double (src_value);
  gint n, d;

  gst_util_double_to_fraction (src, &n, &d);
  gst_value_set_fraction (dest_value, n, d);
}

static void
gst_value_transform_float_fraction (const GValue * src_value,
    GValue * dest_value)
{
  gfloat src = g_value_get_float (src_value);
  gint n, d;

  gst_util_double_to_fraction (src, &n, &d);
  gst_value_set_fraction (dest_value, n, d);
}

static void
gst_value_transform_fraction_double (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_double = ((double) src_value->data[0].v_int) /
      ((double) src_value->data[1].v_int);
}

static void
gst_value_transform_fraction_float (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_float = ((float) src_value->data[0].v_int) /
      ((float) src_value->data[1].v_int);
}

static gint
gst_value_compare_fraction (const GValue * value1, const GValue * value2)
{
  gint n1, n2;
  gint d1, d2;
  gint ret;

  n1 = value1->data[0].v_int;
  n2 = value2->data[0].v_int;
  d1 = value1->data[1].v_int;
  d2 = value2->data[1].v_int;

  /* fractions are reduced when set, so we can quickly see if they're equal */
  if (n1 == n2 && d1 == d2)
    return GST_VALUE_EQUAL;

  if (d1 == 0 && d2 == 0)
    return GST_VALUE_UNORDERED;
  else if (d1 == 0)
    return GST_VALUE_GREATER_THAN;
  else if (d2 == 0)
    return GST_VALUE_LESS_THAN;

  ret = gst_util_fraction_compare (n1, d1, n2, d2);
  if (ret == -1)
    return GST_VALUE_LESS_THAN;
  else if (ret == 1)
    return GST_VALUE_GREATER_THAN;

  /* Equality can't happen here because we check for that
   * first already */
  g_return_val_if_reached (GST_VALUE_UNORDERED);
}

/*********
 * GDate *
 *********/

static gint
gst_value_compare_date (const GValue * value1, const GValue * value2)
{
  const GDate *date1 = (const GDate *) g_value_get_boxed (value1);
  const GDate *date2 = (const GDate *) g_value_get_boxed (value2);
  guint32 j1, j2;

  if (date1 == date2)
    return GST_VALUE_EQUAL;

  if ((date1 == NULL || !g_date_valid (date1))
      && (date2 != NULL && g_date_valid (date2))) {
    return GST_VALUE_LESS_THAN;
  }

  if ((date2 == NULL || !g_date_valid (date2))
      && (date1 != NULL && g_date_valid (date1))) {
    return GST_VALUE_GREATER_THAN;
  }

  if (date1 == NULL || date2 == NULL || !g_date_valid (date1)
      || !g_date_valid (date2)) {
    return GST_VALUE_UNORDERED;
  }

  j1 = g_date_get_julian (date1);
  j2 = g_date_get_julian (date2);

  if (j1 == j2)
    return GST_VALUE_EQUAL;
  else if (j1 < j2)
    return GST_VALUE_LESS_THAN;
  else
    return GST_VALUE_GREATER_THAN;
}

static gchar *
gst_value_serialize_date (const GValue * val)
{
  const GDate *date = (const GDate *) g_value_get_boxed (val);

  if (date == NULL || !g_date_valid (date))
    return g_strdup ("9999-99-99");

  return g_strdup_printf ("%04u-%02u-%02u", g_date_get_year (date),
      g_date_get_month (date), g_date_get_day (date));
}

static gboolean
gst_value_deserialize_date (GValue * dest, const gchar * s)
{
  guint year, month, day;

  if (!s || sscanf (s, "%04u-%02u-%02u", &year, &month, &day) != 3)
    return FALSE;

  if (!g_date_valid_dmy (day, month, year))
    return FALSE;

  g_value_take_boxed (dest, g_date_new_dmy (day, month, year));
  return TRUE;
}

/***************
 * GstDateTime *
 ***************/

static gint
gst_value_compare_date_time (const GValue * value1, const GValue * value2)
{
  const GstDateTime *date1 = (const GstDateTime *) g_value_get_boxed (value1);
  const GstDateTime *date2 = (const GstDateTime *) g_value_get_boxed (value2);

  if (date1 == date2)
    return GST_VALUE_EQUAL;

  if ((date1 == NULL) && (date2 != NULL)) {
    return GST_VALUE_LESS_THAN;
  }
  if ((date2 == NULL) && (date1 != NULL)) {
    return GST_VALUE_LESS_THAN;
  }

  /* returns GST_VALUE_* */
  return __gst_date_time_compare (date1, date2);
}

static gchar *
gst_value_serialize_date_time (const GValue * val)
{
  GstDateTime *date = (GstDateTime *) g_value_get_boxed (val);

  if (date == NULL)
    return g_strdup ("null");

  return __gst_date_time_serialize (date, TRUE);
}

static gboolean
gst_value_deserialize_date_time (GValue * dest, const gchar * s)
{
  GstDateTime *datetime;

  if (!s || strcmp (s, "null") == 0) {
    return FALSE;
  }

  datetime = gst_date_time_new_from_iso8601_string (s);
  if (datetime != NULL) {
    g_value_take_boxed (dest, datetime);
    return TRUE;
  }
  GST_WARNING ("Failed to deserialize date time string '%s'", s);
  return FALSE;
}

static void
gst_value_transform_date_string (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_pointer = gst_value_serialize_date (src_value);
}

static void
gst_value_transform_string_date (const GValue * src_value, GValue * dest_value)
{
  gst_value_deserialize_date (dest_value, src_value->data[0].v_pointer);
}

/*************
 * GDateTime *
 *************/

static gint
gst_value_compare_g_date_time (const GValue * value1, const GValue * value2)
{
  const GDateTime *date1 = (const GDateTime *) g_value_get_boxed (value1);
  const GDateTime *date2 = (const GDateTime *) g_value_get_boxed (value2);

  if (date1 == date2)
    return GST_VALUE_EQUAL;

  if ((date1 == NULL) && (date2 != NULL)) {
    return GST_VALUE_LESS_THAN;
  }
  if ((date2 == NULL) && (date1 != NULL)) {
    return GST_VALUE_LESS_THAN;
  }

  return g_date_time_compare (date1, date2);
}

static gchar *
gst_value_serialize_g_date_time (const GValue * val)
{
  GDateTime *date = (GDateTime *) g_value_get_boxed (val);

  if (date == NULL)
    return g_strdup ("null");

  return g_date_time_format_iso8601 (date);
}

static gboolean
gst_value_deserialize_g_date_time (GValue * dest, const gchar * s)
{
  GDateTime *datetime;

  if (!s || strcmp (s, "null") == 0) {
    return FALSE;
  }

  /* The Gstreamer iso8601 parser is a bit more forgiving */
  datetime =
      gst_date_time_to_g_date_time (gst_date_time_new_from_iso8601_string (s));
  if (datetime != NULL) {
    g_value_take_boxed (dest, datetime);
    return TRUE;
  }
  GST_WARNING ("Failed to deserialize date time string '%s' to GLibDateTime",
      s);
  return FALSE;
}

/*********
 * bytes *
 *********/

static gint
gst_value_compare_bytes (const GValue * value1, const GValue * value2)
{
  GBytes *bytes1 = (GBytes *) g_value_get_boxed (value1);
  GBytes *bytes2 = (GBytes *) g_value_get_boxed (value2);

  if (G_UNLIKELY (!bytes1 || !bytes2)) {
    if (bytes1 == bytes2)
      return GST_VALUE_EQUAL;
    else
      return GST_VALUE_UNORDERED;
  }

  return g_bytes_compare (bytes1, bytes2);
}

static gchar *
gst_value_serialize_bytes (const GValue * value)
{
  GBytes *bytes = (GBytes *) g_value_get_boxed (value);
  gsize len = 0;
  const guint8 *data;

  data = bytes ? g_bytes_get_data (bytes, &len) : NULL;

  return g_base64_encode (data, len);
}

static gboolean
gst_value_deserialize_bytes (GValue * dest, const gchar * s)
{
  gsize len;
  guint8 *data;

  if (!s) {
    g_value_set_boxed (dest, g_bytes_new (NULL, 0));
    return TRUE;
  }

  data = g_base64_decode (s, &len);
  g_value_set_boxed (dest, g_bytes_new_take (data, len));
  return TRUE;
}


/************
 * bitmask *
 ************/

/* helper functions */
static void
gst_value_init_bitmask (GValue * value)
{
  value->data[0].v_uint64 = 0;
}

static void
gst_value_copy_bitmask (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_uint64 = src_value->data[0].v_uint64;
}

static gchar *
gst_value_collect_bitmask (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  g_return_val_if_fail (n_collect_values == 1,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));

  gst_value_set_bitmask (value, (guint64) collect_values[0].v_int64);

  return NULL;
}

static gchar *
gst_value_lcopy_bitmask (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  guint64 *bitmask = collect_values[0].v_pointer;

  g_return_val_if_fail (bitmask != NULL,
      g_strdup_printf ("value for `%s' passed as NULL",
          G_VALUE_TYPE_NAME (value)));

  *bitmask = value->data[0].v_uint64;

  return NULL;
}

/**
 * gst_value_set_bitmask:
 * @value: a GValue initialized to #GST_TYPE_BITMASK
 * @bitmask: the bitmask
 *
 * Sets @value to the bitmask specified by @bitmask.
 */
void
gst_value_set_bitmask (GValue * value, guint64 bitmask)
{
  g_return_if_fail (GST_VALUE_HOLDS_BITMASK (value));

  value->data[0].v_uint64 = bitmask;
}

/**
 * gst_value_get_bitmask:
 * @value: a GValue initialized to #GST_TYPE_BITMASK
 *
 * Gets the bitmask specified by @value.
 *
 * Returns: the bitmask.
 */
guint64
gst_value_get_bitmask (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_BITMASK (value), 0);

  return value->data[0].v_uint64;
}

static gchar *
gst_value_serialize_bitmask (const GValue * value)
{
  guint64 bitmask = value->data[0].v_uint64;

  return g_strdup_printf ("0x%016" G_GINT64_MODIFIER "x", bitmask);
}

static gboolean
gst_value_deserialize_bitmask (GValue * dest, const gchar * s)
{
  gchar *endptr = NULL;
  guint64 val;

  if (G_UNLIKELY (s == NULL))
    return FALSE;

  if (G_UNLIKELY (dest == NULL || !GST_VALUE_HOLDS_BITMASK (dest)))
    return FALSE;

  errno = 0;
  val = g_ascii_strtoull (s, &endptr, 16);
  if (val == G_MAXUINT64 && (errno == ERANGE || errno == EINVAL))
    return FALSE;
  if (val == 0 && endptr == s)
    return FALSE;

  gst_value_set_bitmask (dest, val);

  return TRUE;
}

static void
gst_value_transform_bitmask_string (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_pointer = gst_value_serialize_bitmask (src_value);
}

static void
gst_value_transform_string_bitmask (const GValue * src_value,
    GValue * dest_value)
{
  if (!gst_value_deserialize_bitmask (dest_value, src_value->data[0].v_pointer))
    gst_value_set_bitmask (dest_value, 0);
}

static void
gst_value_transform_uint64_bitmask (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_uint64 = src_value->data[0].v_uint64;
}

static void
gst_value_transform_bitmask_uint64 (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_uint64 = src_value->data[0].v_uint64;
}

static gint
gst_value_compare_bitmask (const GValue * value1, const GValue * value2)
{
  guint64 v1, v2;

  v1 = value1->data[0].v_uint64;
  v2 = value2->data[0].v_uint64;

  if (v1 == v2)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

/************
 * flagset *
 ************/

/* helper functions */
static void
gst_value_init_flagset (GValue * value)
{
  value->data[0].v_uint = 0;
  value->data[1].v_uint = 0;
}

static void
gst_value_copy_flagset (const GValue * src_value, GValue * dest_value)
{
  dest_value->data[0].v_uint = src_value->data[0].v_uint;
  dest_value->data[1].v_uint = src_value->data[1].v_uint;
}

static gchar *
gst_value_collect_flagset (GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  g_return_val_if_fail (n_collect_values == 2,
      g_strdup_printf ("not enough value locations for `%s' passed",
          G_VALUE_TYPE_NAME (value)));

  gst_value_set_flagset (value,
      (guint) collect_values[0].v_int, (guint) collect_values[1].v_int);

  return NULL;
}

static gchar *
gst_value_lcopy_flagset (const GValue * value, guint n_collect_values,
    GTypeCValue * collect_values, guint collect_flags)
{
  guint *flags = collect_values[0].v_pointer;
  guint *mask = collect_values[1].v_pointer;

  *flags = value->data[0].v_uint;
  *mask = value->data[1].v_uint;

  return NULL;
}

/**
 * gst_value_set_flagset:
 * @value: a GValue initialized to %GST_TYPE_FLAG_SET
 * @flags: The value of the flags set or unset
 * @mask: The mask indicate which flags bits must match for comparisons
 *
 * Sets @value to the flags and mask values provided in @flags and @mask.
 * The @flags value indicates the values of flags, the @mask represents
 * which bits in the flag value have been set, and which are "don't care"
 *
 * Since: 1.6
 */
void
gst_value_set_flagset (GValue * value, guint flags, guint mask)
{
  g_return_if_fail (GST_VALUE_HOLDS_FLAG_SET (value));

  /* Normalise and only keep flags mentioned in the mask */
  value->data[0].v_uint = flags & mask;
  value->data[1].v_uint = mask;
}

/**
 * gst_value_get_flagset_flags:
 * @value: a GValue initialized to #GST_TYPE_FLAG_SET
 *
 * Retrieve the flags field of a GstFlagSet @value.
 *
 * Returns: the flags field of the flagset instance.
 *
 * Since: 1.6
 */
guint
gst_value_get_flagset_flags (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (value), 0);

  return value->data[0].v_uint;
}

/**
 * gst_value_get_flagset_mask:
 * @value: a GValue initialized to #GST_TYPE_FLAG_SET
 *
 * Retrieve the mask field of a GstFlagSet @value.
 *
 * Returns: the mask field of the flagset instance.
 *
 * Since: 1.6
 */
guint
gst_value_get_flagset_mask (const GValue * value)
{
  g_return_val_if_fail (GST_VALUE_HOLDS_FLAG_SET (value), 1);

  return value->data[1].v_uint;
}

static gchar *
gst_value_serialize_flagset (const GValue * value)
{
  guint flags = value->data[0].v_uint;
  guint mask = value->data[1].v_uint;
  GstFlagSetClass *set_klass =
      (GstFlagSetClass *) g_type_class_ref (G_VALUE_TYPE (value));
  gchar *result;

  result = g_strdup_printf ("%x:%x", flags, mask);

  /* If this flag set class has an associated GFlags GType, and some
   * bits in the mask, serialize the bits in human-readable form to
   * aid debugging */
  if (mask && set_klass->flags_type) {
    GFlagsClass *flags_klass =
        (GFlagsClass *) (g_type_class_ref (set_klass->flags_type));
    GFlagsValue *fl;
    gchar *tmp;
    gboolean first = TRUE;

    g_return_val_if_fail (flags_klass, NULL);

    /* some bits in the mask are set, so serialize one by one, according
     * to whether that bit is set or cleared in the flags value */
    while (mask) {
      fl = g_flags_get_first_value (flags_klass, mask);
      if (fl == NULL) {
        /* No more bits match in the flags mask - time to stop */
        mask = 0;
        break;
      }

      tmp = g_strconcat (result,
          first ? ":" : "",
          (flags & fl->value) ? "+" : "/", fl->value_nick, NULL);
      g_free (result);
      result = tmp;
      first = FALSE;

      /* clear flag */
      mask &= ~fl->value;
    }
    g_type_class_unref (flags_klass);

  }
  g_type_class_unref (set_klass);

  return result;
}

static gboolean
is_valid_flags_string (const gchar * s)
{
  /* We're looking to match +this/that+other-thing/not-this-thing type strings */
  return g_regex_match_simple ("^([\\+\\/][\\w\\d-]+)+$", s, G_REGEX_CASELESS,
      0);
}

static gboolean
gst_value_deserialize_flagset (GValue * dest, const gchar * s)
{
  gboolean res = FALSE;
  guint flags, mask;
  gchar *cur, *next;

  if (G_UNLIKELY (s == NULL))
    return FALSE;

  if (G_UNLIKELY (dest == NULL || !GST_VALUE_HOLDS_FLAG_SET (dest)))
    return FALSE;

  /* Flagset strings look like %x:%x - hex flags : hex bitmask,
   * 32-bit each, or like a concatenated list of flag nicks,
   * with either '+' or '/' in front. The first form
   * may optionally be followed by ':' and a set of text flag descriptions
   * for easier debugging */

  /* Try and interpret as hex form first, as it's the most efficient */
  /* Read the flags first */
  flags = strtoul (s, &next, 16);
  if (G_UNLIKELY ((flags == 0 && errno == EINVAL) || s == next))
    goto try_as_flags_string;
  /* Next char should be a colon */
  if (next[0] == ':')
    next++;

  /* Read the mask */
  cur = next;
  mask = strtoul (cur, &next, 16);
  if (G_UNLIKELY ((mask == 0 && errno == EINVAL) || cur == next))
    goto try_as_flags_string;

  if (g_str_has_prefix (cur, "0x") || g_str_has_prefix (cur, "0X"))
    cur += 2;

  /* Flagsets are 32 bits hex numbers, so do not accept any number that has more
   * then 8 characters. strtoul() accepts unlimited number of leading zeros and
   * 64bit numbers on 64bit platforms.
   */
  if ((next - cur) > 8)
    return FALSE;

  /* Next char should be NULL terminator, or a ':'. If ':', we need the flag string after */
  if (G_UNLIKELY (next[0] == 0)) {
    res = TRUE;
    goto done;
  }

  if (next[0] != ':')
    return FALSE;

  s = next + 1;

  if (g_str_equal (g_type_name (G_VALUE_TYPE (dest)), "GstFlagSet")) {
    /* If we're parsing a generic flag set, that can mean we're guessing
     * at the type in deserialising a GstStructure so at least check that
     * we have a valid-looking string, so we don't cause deserialisation of
     * other types of strings like 00:01:00:00 - https://bugzilla.gnome.org/show_bug.cgi?id=779755 */
    if (is_valid_flags_string (s)) {
      res = TRUE;
      goto done;
    }
    return FALSE;
  }

  /* Otherwise, we already got a hex string for a valid non-generic flagset type */
  res = TRUE;
  goto done;

try_as_flags_string:

  {
    const gchar *set_class = g_type_name (G_VALUE_TYPE (dest));
    GFlagsClass *flags_klass = NULL;
    const gchar *end;

    if (g_str_equal (set_class, "GstFlagSet")) {
      /* There's no hope to parse the fields of generic flag set if we didn't already
       * catch a hex-string above */
      return FALSE;
    }

    /* Flags class is the FlagSet class with 'Set' removed from the end */
    end = g_strrstr (set_class, "Set");

    if (end != NULL) {
      gchar *class_name = g_strndup (set_class, end - set_class);
      GType flags_type = g_type_from_name (class_name);
      if (flags_type == G_TYPE_INVALID) {
        GST_TRACE ("Looking for dynamic type %s", class_name);
        gst_dynamic_type_factory_load (class_name);
      }

      if (flags_type != G_TYPE_INVALID) {
        flags_klass = g_type_class_ref (flags_type);
        GST_TRACE ("Going to parse %s as %s", s, class_name);
      }
      g_free (class_name);
    }

    if (flags_klass) {
      res = gst_value_gflags_str_to_flags (flags_klass, s, &flags, &mask);
      g_type_class_unref (flags_klass);
    }
  }

done:
  if (res)
    gst_value_set_flagset (dest, flags, mask);
  return res;

}

static void
gst_value_transform_flagset_string (const GValue * src_value,
    GValue * dest_value)
{
  dest_value->data[0].v_pointer = gst_value_serialize_flagset (src_value);
}

static void
gst_value_transform_string_flagset (const GValue * src_value,
    GValue * dest_value)
{
  if (!gst_value_deserialize_flagset (dest_value, src_value->data[0].v_pointer)) {
    /* If the deserialize fails, ensure we leave the flags in a
     * valid, if incorrect, state */
    gst_value_set_flagset (dest_value, 0, 0);
  }
}

static gint
gst_value_compare_flagset (const GValue * value1, const GValue * value2)
{
  guint v1, v2;
  guint m1, m2;

  v1 = value1->data[0].v_uint;
  v2 = value2->data[0].v_uint;

  m1 = value1->data[1].v_uint;
  m2 = value2->data[1].v_uint;

  if (v1 == v2 && m1 == m2)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

/***********************
 * GstAllocationParams *
 ***********************/
static gint
gst_value_compare_allocation_params (const GValue * value1,
    const GValue * value2)
{
  GstAllocationParams *v1, *v2;

  v1 = value1->data[0].v_pointer;
  v2 = value2->data[0].v_pointer;

  if (v1 == NULL && v1 == v2)
    return GST_VALUE_EQUAL;

  if (v1 == NULL || v2 == NULL)
    return GST_VALUE_UNORDERED;

  if (v1->flags == v2->flags && v1->align == v2->align &&
      v1->prefix == v2->prefix && v1->padding == v2->padding)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}


/************
 * GObject *
 ************/

static gint
gst_value_compare_object (const GValue * value1, const GValue * value2)
{
  gpointer v1, v2;

  v1 = value1->data[0].v_pointer;
  v2 = value2->data[0].v_pointer;

  if (v1 == v2)
    return GST_VALUE_EQUAL;

  return GST_VALUE_UNORDERED;
}

static void
gst_value_transform_object_string (const GValue * src_value,
    GValue * dest_value)
{
  GstObject *obj;
  gchar *str;

  obj = g_value_get_object (src_value);
  if (obj) {
    str =
        g_strdup_printf ("(%s) %s", G_OBJECT_TYPE_NAME (obj),
        GST_OBJECT_NAME (obj));
  } else {
    str = g_strdup ("NULL");
  }

  dest_value->data[0].v_pointer = str;
}

static GTypeInfo _info = {
  0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL,
};

static GTypeFundamentalInfo _finfo = {
  0
};

#define FUNC_VALUE_GET_TYPE_CLASSED(type, name, csize, flags)   \
GType _gst_ ## type ## _type = 0;                               \
                                                                \
GType gst_ ## type ## _get_type (void)                          \
{                                                               \
  static GType gst_ ## type ## _type = 0;              \
                                                                \
  if (g_once_init_enter (&gst_ ## type ## _type)) {             \
    GType _type;                                                \
    _info.class_size = csize;                                   \
    _finfo.type_flags = flags;                                  \
    _info.value_table = & _gst_ ## type ## _value_table;        \
    _type = g_type_register_fundamental (                       \
        g_type_fundamental_next (),                             \
        name, &_info, &_finfo, 0);                              \
    _gst_ ## type ## _type = _type;                             \
    g_once_init_leave(&gst_ ## type ## _type, _type);           \
  }                                                             \
                                                                \
  return gst_ ## type ## _type;                                 \
}

#define FUNC_VALUE_GET_TYPE(type, name) \
  FUNC_VALUE_GET_TYPE_CLASSED(type, name, 0, 0)

static const GTypeValueTable _gst_int_range_value_table = {
  gst_value_init_int_range,
  NULL,
  gst_value_copy_int_range,
  NULL,
  (char *) "ii",
  gst_value_collect_int_range, (char *) "pp", gst_value_lcopy_int_range
};

FUNC_VALUE_GET_TYPE (int_range, "GstIntRange");

static const GTypeValueTable _gst_int64_range_value_table = {
  gst_value_init_int64_range,
  gst_value_free_int64_range,
  gst_value_copy_int64_range,
  NULL,
  (char *) "qq",
  gst_value_collect_int64_range,
  (char *) "pp", gst_value_lcopy_int64_range
};

FUNC_VALUE_GET_TYPE (int64_range, "GstInt64Range");

static const GTypeValueTable _gst_double_range_value_table = {
  gst_value_init_double_range,
  NULL,
  gst_value_copy_double_range,
  NULL,
  (char *) "dd",
  gst_value_collect_double_range,
  (char *) "pp", gst_value_lcopy_double_range
};

FUNC_VALUE_GET_TYPE (double_range, "GstDoubleRange");

static const GTypeValueTable _gst_fraction_range_value_table = {
  gst_value_init_fraction_range,
  gst_value_free_fraction_range,
  gst_value_copy_fraction_range,
  NULL,
  (char *) "iiii",
  gst_value_collect_fraction_range,
  (char *) "pppp", gst_value_lcopy_fraction_range
};

FUNC_VALUE_GET_TYPE (fraction_range, "GstFractionRange");

static const GTypeValueTable _gst_value_list_value_table = {
  gst_value_init_list_or_array,
  gst_value_free_list_or_array,
  gst_value_copy_list_or_array,
  gst_value_list_or_array_peek_pointer,
  (char *) "p",
  gst_value_collect_list_or_array,
  (char *) "p", gst_value_lcopy_list_or_array
};

FUNC_VALUE_GET_TYPE (value_list, "GstValueList");

static const GTypeValueTable _gst_value_array_value_table = {
  gst_value_init_list_or_array,
  gst_value_free_list_or_array,
  gst_value_copy_list_or_array,
  gst_value_list_or_array_peek_pointer,
  (char *) "p",
  gst_value_collect_list_or_array,
  (char *) "p", gst_value_lcopy_list_or_array
};

FUNC_VALUE_GET_TYPE (value_array, "GstValueArray");

static const GTypeValueTable _gst_fraction_value_table = {
  gst_value_init_fraction,
  NULL,
  gst_value_copy_fraction,
  NULL,
  (char *) "ii",
  gst_value_collect_fraction, (char *) "pp", gst_value_lcopy_fraction
};

FUNC_VALUE_GET_TYPE (fraction, "GstFraction");

static const GTypeValueTable _gst_bitmask_value_table = {
  gst_value_init_bitmask,
  NULL,
  gst_value_copy_bitmask,
  NULL,
  (char *) "q",
  gst_value_collect_bitmask, (char *) "p", gst_value_lcopy_bitmask
};

FUNC_VALUE_GET_TYPE (bitmask, "GstBitmask");

static const GTypeValueTable _gst_flagset_value_table = {
  gst_value_init_flagset,
  NULL,
  gst_value_copy_flagset,
  NULL,
  (char *) "ii",
  gst_value_collect_flagset, (char *) "pp", gst_value_lcopy_flagset
};

FUNC_VALUE_GET_TYPE_CLASSED (flagset, "GstFlagSet",
    sizeof (GstFlagSetClass), G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_DERIVABLE);

GType
gst_g_thread_get_type (void)
{
  return G_TYPE_THREAD;
}

#define SERIAL_VTABLE(t,c,s,d) { t, c, s, d, NULL }
#define SERIAL_VTABLE_PSPEC(t,c,s,d) { t, c, s, NULL, d }

#define REGISTER_SERIALIZATION_CONST(_gtype, _type)                     \
G_STMT_START {                                                          \
  static const GstValueTable gst_value =                                \
    SERIAL_VTABLE (_gtype, gst_value_compare_ ## _type,                 \
    gst_value_serialize_ ## _type, gst_value_deserialize_ ## _type);    \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

#define REGISTER_SERIALIZATION(_gtype, _type)                           \
G_STMT_START {                                                          \
  static GstValueTable gst_value =                                      \
    SERIAL_VTABLE (0, gst_value_compare_ ## _type,                      \
    gst_value_serialize_ ## _type, gst_value_deserialize_ ## _type);    \
  gst_value.type = _gtype;                                              \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

#define REGISTER_SERIALIZATION_WITH_PSPEC(_gtype, _type)                \
G_STMT_START {                                                          \
  static GstValueTable gst_value =                                      \
    SERIAL_VTABLE_PSPEC (0, gst_value_compare_ ## _type,                \
    gst_value_serialize_ ## _type, gst_value_deserialize_ ## _type);    \
  gst_value.type = _gtype;                                              \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

#define REGISTER_SERIALIZATION_NO_COMPARE(_gtype, _type)                \
G_STMT_START {                                                          \
  static GstValueTable gst_value =                                      \
    SERIAL_VTABLE (0, NULL,                                             \
    gst_value_serialize_ ## _type, gst_value_deserialize_ ## _type);    \
  gst_value.type = _gtype;                                              \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

#define REGISTER_SERIALIZATION_COMPARE_ONLY(_gtype, _type)              \
G_STMT_START {                                                          \
  static GstValueTable gst_value =                                      \
    SERIAL_VTABLE (0, gst_value_compare_ ## _type,                      \
        NULL, NULL);                                                    \
  gst_value.type = _gtype;                                              \
  gst_value_register (&gst_value);                                      \
} G_STMT_END

/* These initial sizes are used for the tables
 * below, and save a couple of reallocs at startup */

static const gint GST_VALUE_TABLE_DEFAULT_SIZE = 40;
static const gint GST_VALUE_UNION_TABLE_DEFAULT_SIZE = 8;
static const gint GST_VALUE_INTERSECT_TABLE_DEFAULT_SIZE = 4;
static const gint GST_VALUE_SUBTRACT_TABLE_DEFAULT_SIZE = 16;

void
_priv_gst_value_initialize (void)
{
  gst_value_table =
      g_array_sized_new (FALSE, FALSE, sizeof (GstValueTable),
      GST_VALUE_TABLE_DEFAULT_SIZE);
  gst_value_hash = g_hash_table_new (NULL, NULL);
  gst_value_union_funcs = g_array_sized_new (FALSE, FALSE,
      sizeof (GstValueUnionInfo), GST_VALUE_UNION_TABLE_DEFAULT_SIZE);
  gst_value_intersect_funcs = g_array_sized_new (FALSE, FALSE,
      sizeof (GstValueIntersectInfo), GST_VALUE_INTERSECT_TABLE_DEFAULT_SIZE);
  gst_value_subtract_funcs = g_array_sized_new (FALSE, FALSE,
      sizeof (GstValueSubtractInfo), GST_VALUE_SUBTRACT_TABLE_DEFAULT_SIZE);

  REGISTER_SERIALIZATION (gst_int_range_get_type (), int_range);
  REGISTER_SERIALIZATION (gst_int64_range_get_type (), int64_range);
  REGISTER_SERIALIZATION (gst_double_range_get_type (), double_range);
  REGISTER_SERIALIZATION (gst_fraction_range_get_type (), fraction_range);
  REGISTER_SERIALIZATION (g_value_array_get_type (), g_value_array);
  REGISTER_SERIALIZATION (gst_buffer_get_type (), buffer);
  REGISTER_SERIALIZATION (gst_sample_get_type (), sample);
  REGISTER_SERIALIZATION (gst_fraction_get_type (), fraction);
  REGISTER_SERIALIZATION (gst_caps_get_type (), caps);
  REGISTER_SERIALIZATION (gst_tag_list_get_type (), tag_list);
  REGISTER_SERIALIZATION (G_TYPE_DATE, date);
  REGISTER_SERIALIZATION (G_TYPE_BYTES, bytes);
  REGISTER_SERIALIZATION (gst_date_time_get_type (), date_time);
  REGISTER_SERIALIZATION (G_TYPE_DATE_TIME, g_date_time);
  REGISTER_SERIALIZATION (gst_bitmask_get_type (), bitmask);
  REGISTER_SERIALIZATION (gst_structure_get_type (), structure);
  REGISTER_SERIALIZATION (gst_flagset_get_type (), flagset);

  REGISTER_SERIALIZATION_NO_COMPARE (gst_segment_get_type (), segment);
  REGISTER_SERIALIZATION_NO_COMPARE (gst_caps_features_get_type (),
      caps_features);

  REGISTER_SERIALIZATION_COMPARE_ONLY (gst_allocation_params_get_type (),
      allocation_params);
  REGISTER_SERIALIZATION_COMPARE_ONLY (G_TYPE_OBJECT, object);

  REGISTER_SERIALIZATION_CONST (G_TYPE_DOUBLE, double);
  REGISTER_SERIALIZATION_CONST (G_TYPE_FLOAT, float);

  REGISTER_SERIALIZATION_CONST (G_TYPE_STRING, string);
  REGISTER_SERIALIZATION_CONST (G_TYPE_BOOLEAN, boolean);
  REGISTER_SERIALIZATION_CONST (G_TYPE_ENUM, enum);

  REGISTER_SERIALIZATION_CONST (G_TYPE_FLAGS, gflags);

  REGISTER_SERIALIZATION_CONST (G_TYPE_INT, int);

  REGISTER_SERIALIZATION_CONST (G_TYPE_INT64, int64);
  REGISTER_SERIALIZATION_CONST (G_TYPE_LONG, long);

  REGISTER_SERIALIZATION_CONST (G_TYPE_UINT, uint);
  REGISTER_SERIALIZATION_CONST (G_TYPE_UINT64, uint64);
  REGISTER_SERIALIZATION_CONST (G_TYPE_ULONG, ulong);

  REGISTER_SERIALIZATION_CONST (G_TYPE_UCHAR, uchar);

  REGISTER_SERIALIZATION (G_TYPE_GTYPE, gtype);

  REGISTER_SERIALIZATION_WITH_PSPEC (gst_value_list_get_type (), value_list);
  REGISTER_SERIALIZATION_WITH_PSPEC (gst_value_array_get_type (), value_array);

  g_value_register_transform_func (GST_TYPE_INT_RANGE, G_TYPE_STRING,
      gst_value_transform_int_range_string);
  g_value_register_transform_func (GST_TYPE_INT64_RANGE, G_TYPE_STRING,
      gst_value_transform_int64_range_string);
  g_value_register_transform_func (GST_TYPE_DOUBLE_RANGE, G_TYPE_STRING,
      gst_value_transform_double_range_string);
  g_value_register_transform_func (GST_TYPE_FRACTION_RANGE, G_TYPE_STRING,
      gst_value_transform_fraction_range_string);
  g_value_register_transform_func (GST_TYPE_LIST, G_TYPE_STRING,
      gst_value_transform_list_string);
  g_value_register_transform_func (GST_TYPE_LIST, G_TYPE_VALUE_ARRAY,
      gst_value_transform_any_list_g_value_array);
  g_value_register_transform_func (GST_TYPE_ARRAY, G_TYPE_STRING,
      gst_value_transform_array_string);
  g_value_register_transform_func (GST_TYPE_ARRAY, G_TYPE_VALUE_ARRAY,
      gst_value_transform_any_list_g_value_array);
  g_value_register_transform_func (G_TYPE_VALUE_ARRAY, G_TYPE_STRING,
      gst_value_transform_g_value_array_string);
  g_value_register_transform_func (G_TYPE_VALUE_ARRAY, GST_TYPE_ARRAY,
      gst_value_transform_g_value_array_any_list);
  g_value_register_transform_func (G_TYPE_VALUE_ARRAY, GST_TYPE_LIST,
      gst_value_transform_g_value_array_any_list);
  g_value_register_transform_func (GST_TYPE_FRACTION, G_TYPE_STRING,
      gst_value_transform_fraction_string);
  g_value_register_transform_func (G_TYPE_STRING, GST_TYPE_FRACTION,
      gst_value_transform_string_fraction);
  g_value_register_transform_func (GST_TYPE_FRACTION, G_TYPE_DOUBLE,
      gst_value_transform_fraction_double);
  g_value_register_transform_func (GST_TYPE_FRACTION, G_TYPE_FLOAT,
      gst_value_transform_fraction_float);
  g_value_register_transform_func (G_TYPE_DOUBLE, GST_TYPE_FRACTION,
      gst_value_transform_double_fraction);
  g_value_register_transform_func (G_TYPE_FLOAT, GST_TYPE_FRACTION,
      gst_value_transform_float_fraction);
  g_value_register_transform_func (G_TYPE_DATE, G_TYPE_STRING,
      gst_value_transform_date_string);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DATE,
      gst_value_transform_string_date);
  g_value_register_transform_func (GST_TYPE_OBJECT, G_TYPE_STRING,
      gst_value_transform_object_string);
  g_value_register_transform_func (GST_TYPE_BITMASK, G_TYPE_UINT64,
      gst_value_transform_bitmask_uint64);
  g_value_register_transform_func (GST_TYPE_BITMASK, G_TYPE_STRING,
      gst_value_transform_bitmask_string);
  g_value_register_transform_func (G_TYPE_UINT64, GST_TYPE_BITMASK,
      gst_value_transform_uint64_bitmask);
  g_value_register_transform_func (G_TYPE_STRING, GST_TYPE_BITMASK,
      gst_value_transform_string_bitmask);

  g_value_register_transform_func (GST_TYPE_FLAG_SET, G_TYPE_STRING,
      gst_value_transform_flagset_string);
  g_value_register_transform_func (G_TYPE_STRING, GST_TYPE_FLAG_SET,
      gst_value_transform_string_flagset);

  /* Only register intersection functions for *different* types.
   * Identical type intersection should be specified directly in
   * gst_value_intersect() */
  gst_value_register_intersect_func (G_TYPE_INT, GST_TYPE_INT_RANGE,
      gst_value_intersect_int_int_range);
  gst_value_register_intersect_func (G_TYPE_INT64, GST_TYPE_INT64_RANGE,
      gst_value_intersect_int64_int64_range);
  gst_value_register_intersect_func (G_TYPE_DOUBLE, GST_TYPE_DOUBLE_RANGE,
      gst_value_intersect_double_double_range);
  gst_value_register_intersect_func (GST_TYPE_FRACTION,
      GST_TYPE_FRACTION_RANGE, gst_value_intersect_fraction_fraction_range);

  gst_value_register_subtract_func (G_TYPE_INT, GST_TYPE_INT_RANGE,
      gst_value_subtract_int_int_range);
  gst_value_register_subtract_func (GST_TYPE_INT_RANGE, G_TYPE_INT,
      gst_value_subtract_int_range_int);
  gst_value_register_subtract_func (GST_TYPE_INT_RANGE, GST_TYPE_INT_RANGE,
      gst_value_subtract_int_range_int_range);
  gst_value_register_subtract_func (G_TYPE_INT64, GST_TYPE_INT64_RANGE,
      gst_value_subtract_int64_int64_range);
  gst_value_register_subtract_func (GST_TYPE_INT64_RANGE, G_TYPE_INT64,
      gst_value_subtract_int64_range_int64);
  gst_value_register_subtract_func (GST_TYPE_INT64_RANGE,
      GST_TYPE_INT64_RANGE, gst_value_subtract_int64_range_int64_range);
  gst_value_register_subtract_func (G_TYPE_DOUBLE, GST_TYPE_DOUBLE_RANGE,
      gst_value_subtract_double_double_range);
  gst_value_register_subtract_func (GST_TYPE_DOUBLE_RANGE, G_TYPE_DOUBLE,
      gst_value_subtract_double_range_double);
  gst_value_register_subtract_func (GST_TYPE_DOUBLE_RANGE,
      GST_TYPE_DOUBLE_RANGE, gst_value_subtract_double_range_double_range);
  gst_value_register_subtract_func (GST_TYPE_FRACTION,
      GST_TYPE_FRACTION_RANGE, gst_value_subtract_fraction_fraction_range);
  gst_value_register_subtract_func (GST_TYPE_FRACTION_RANGE,
      GST_TYPE_FRACTION, gst_value_subtract_fraction_range_fraction);
  gst_value_register_subtract_func (GST_TYPE_FRACTION_RANGE,
      GST_TYPE_FRACTION_RANGE,
      gst_value_subtract_fraction_range_fraction_range);

  {
    GType date_type = G_TYPE_DATE;

    g_type_name (date_type);
  }

  gst_value_register_union_func (G_TYPE_INT, GST_TYPE_INT_RANGE,
      gst_value_union_int_int_range);
  gst_value_register_union_func (GST_TYPE_INT_RANGE, GST_TYPE_INT_RANGE,
      gst_value_union_int_range_int_range);
  gst_value_register_union_func (GST_TYPE_FLAG_SET, GST_TYPE_FLAG_SET,
      gst_value_union_flagset_flagset);
  gst_value_register_union_func (GST_TYPE_STRUCTURE, GST_TYPE_STRUCTURE,
      gst_value_union_structure_structure);
  gst_value_register_union_func (GST_TYPE_FRACTION, GST_TYPE_FRACTION_RANGE,
      gst_value_union_fraction_fraction_range);
  gst_value_register_union_func (GST_TYPE_FRACTION_RANGE,
      GST_TYPE_FRACTION_RANGE, gst_value_union_fraction_range_fraction_range);

#if GST_VERSION_NANO == 1
  /* If building from git master, check starting array sizes matched actual size
   * so we can keep the defines in sync and save a few reallocs on startup */
  if (gst_value_table->len > GST_VALUE_TABLE_DEFAULT_SIZE) {
    GST_ERROR ("Wrong initial gst_value_table size. "
        "Please set GST_VALUE_TABLE_DEFAULT_SIZE to %u in gstvalue.c",
        gst_value_table->len);
  }
  if (gst_value_union_funcs->len > GST_VALUE_UNION_TABLE_DEFAULT_SIZE) {
    GST_ERROR ("Wrong initial gst_value_union_funcs table size. "
        "Please set GST_VALUE_UNION_TABLE_DEFAULT_SIZE to %u in gstvalue.c",
        gst_value_union_funcs->len);
  }
  if (gst_value_intersect_funcs->len > GST_VALUE_INTERSECT_TABLE_DEFAULT_SIZE) {
    GST_ERROR ("Wrong initial gst_value_intersect_funcs table size. "
        "Please set GST_VALUE_INTERSECT_TABLE_DEFAULT_SIZE to %u in gstvalue.c",
        gst_value_intersect_funcs->len);
  }
  if (gst_value_subtract_funcs->len > GST_VALUE_SUBTRACT_TABLE_DEFAULT_SIZE) {
    GST_ERROR ("Wrong initial gst_value_subtract_funcs table size. "
        "Please set GST_VALUE_SUBTRACT_TABLE_DEFAULT_SIZE to %u in gstvalue.c",
        gst_value_subtract_funcs->len);
  }
#endif
}

static void
gst_flagset_class_init (gpointer g_class, gpointer class_data)
{
  GstFlagSetClass *f_class = (GstFlagSetClass *) (g_class);
  f_class->flags_type = (GType) GPOINTER_TO_SIZE (class_data);
}

/**
 * gst_flagset_register:
 * @flags_type: a #GType of a #G_TYPE_FLAGS type.
 *
 * Create a new sub-class of #GST_TYPE_FLAG_SET
 * which will pretty-print the human-readable flags
 * when serializing, for easier debugging.
 *
 * Since: 1.6
 */
GType
gst_flagset_register (GType flags_type)
{
  GTypeInfo info = {
    sizeof (GstFlagSetClass),
    NULL, NULL,
    (GClassInitFunc) gst_flagset_class_init,
    NULL, GSIZE_TO_POINTER (flags_type), 0, 0, NULL, NULL
  };
  GType t;
  gchar *class_name;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), 0);

  class_name = g_strdup_printf ("%sSet", g_type_name (flags_type));

  t = g_type_register_static (GST_TYPE_FLAG_SET,
      g_intern_string (class_name), &info, 0);
  g_free (class_name);

  return t;
}
