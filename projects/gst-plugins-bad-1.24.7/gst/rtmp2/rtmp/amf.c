/* GStreamer RTMP Library
 * Copyright (C) 2014 David Schleef <ds@schleef.org>
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
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

#include "amf.h"
#include "rtmputils.h"
#include <string.h>
#include <gst/gst.h>

#define MAX_RECURSION_DEPTH 16

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_amf_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_amf_debug_category

static GBytes *empty_bytes;

static void
init_static (void)
{
  static gsize done = 0;
  if (g_once_init_enter (&done)) {
    empty_bytes = g_bytes_new_static ("", 0);
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_amf_debug_category, "rtmpamf", 0,
        "debug category for the amf parser");
    g_once_init_leave (&done, 1);
  }
}

const gchar *
gst_amf_type_get_nick (GstAmfType type)
{
  switch (type) {
    case GST_AMF_TYPE_INVALID:
      return "invalid";
    case GST_AMF_TYPE_NUMBER:
      return "number";
    case GST_AMF_TYPE_BOOLEAN:
      return "boolean";
    case GST_AMF_TYPE_STRING:
      return "string";
    case GST_AMF_TYPE_OBJECT:
      return "object";
    case GST_AMF_TYPE_MOVIECLIP:
      return "movieclip";
    case GST_AMF_TYPE_NULL:
      return "null";
    case GST_AMF_TYPE_UNDEFINED:
      return "undefined";
    case GST_AMF_TYPE_REFERENCE:
      return "reference";
    case GST_AMF_TYPE_ECMA_ARRAY:
      return "ecma-array";
    case GST_AMF_TYPE_OBJECT_END:
      return "object-end";
    case GST_AMF_TYPE_STRICT_ARRAY:
      return "strict-array";
    case GST_AMF_TYPE_DATE:
      return "date";
    case GST_AMF_TYPE_LONG_STRING:
      return "long-string";
    case GST_AMF_TYPE_UNSUPPORTED:
      return "unsupported";
    case GST_AMF_TYPE_RECORDSET:
      return "recordset";
    case GST_AMF_TYPE_XML_DOCUMENT:
      return "xml-document";
    case GST_AMF_TYPE_TYPED_OBJECT:
      return "typed-object";
    case GST_AMF_TYPE_AVMPLUS_OBJECT:
      return "avmplus-object";
    default:
      return "unknown";
  }
}

typedef struct
{
  gchar *name;
  GstAmfNode *value;
} AmfObjectField;

static void
amf_object_field_clear (gpointer ptr)
{
  AmfObjectField *field = ptr;
  g_clear_pointer (&field->name, g_free);
  g_clear_pointer (&field->value, gst_amf_node_free);
}

struct _GstAmfNode
{
  GstAmfType type;
  union
  {
    gint v_int;
    gdouble v_double;
    GBytes *v_bytes;
    GArray *v_fields;
    GPtrArray *v_elements;
  } value;
};

static inline const AmfObjectField *
get_field (const GstAmfNode * node, guint index)
{
  return &g_array_index (node->value.v_fields, const AmfObjectField, index);
}

static inline void
append_field (GstAmfNode * node, gchar * name, GstAmfNode * value)
{
  AmfObjectField field = {
    .name = name,
    .value = value,
  };
  g_array_append_val (node->value.v_fields, field);
}

static inline const GstAmfNode *
get_element (const GstAmfNode * node, guint index)
{
  return g_ptr_array_index (node->value.v_elements, index);
}

static inline void
append_element (GstAmfNode * node, GstAmfNode * value)
{
  g_ptr_array_add (node->value.v_elements, value);
}

static GstAmfNode *
node_new (GstAmfType type)
{
  GstAmfNode *node;

  init_static ();

  node = g_malloc0 (sizeof *node);
  node->type = type;

  switch (type) {
    case GST_AMF_TYPE_STRING:
    case GST_AMF_TYPE_LONG_STRING:
      node->value.v_bytes = g_bytes_ref (empty_bytes);
      break;

    case GST_AMF_TYPE_OBJECT:
    case GST_AMF_TYPE_ECMA_ARRAY:
      node->value.v_fields =
          g_array_new (FALSE, FALSE, sizeof (AmfObjectField));
      g_array_set_clear_func (node->value.v_fields, amf_object_field_clear);
      break;

    case GST_AMF_TYPE_STRICT_ARRAY:
      node->value.v_elements =
          g_ptr_array_new_with_free_func (gst_amf_node_free);
      break;

    default:
      break;
  }

  return node;
}

GstAmfNode *
gst_amf_node_new_null (void)
{
  return node_new (GST_AMF_TYPE_NULL);
}

GstAmfNode *
gst_amf_node_new_boolean (gboolean value)
{
  GstAmfNode *node = node_new (GST_AMF_TYPE_BOOLEAN);
  node->value.v_int = !!value;
  return node;
}

GstAmfNode *
gst_amf_node_new_number (gdouble value)
{
  GstAmfNode *node = node_new (GST_AMF_TYPE_NUMBER);
  node->value.v_double = value;
  return node;
}

GstAmfNode *
gst_amf_node_new_string (const gchar * value, gssize size)
{
  GstAmfNode *node = node_new (GST_AMF_TYPE_STRING);
  gst_amf_node_set_string (node, value, size);
  return node;
}

GstAmfNode *
gst_amf_node_new_take_string (gchar * value, gssize size)
{
  GstAmfNode *node = node_new (GST_AMF_TYPE_STRING);
  gst_amf_node_take_string (node, value, size);
  return node;
}

GstAmfNode *
gst_amf_node_new_object (void)
{
  return node_new (GST_AMF_TYPE_OBJECT);
}

GstAmfNode *
gst_amf_node_copy (const GstAmfNode * node)
{
  GstAmfNode *copy;

  g_return_val_if_fail (node, NULL);

  copy = node_new (node->type);

  switch (node->type) {
    case GST_AMF_TYPE_STRING:
    case GST_AMF_TYPE_LONG_STRING:
      copy->value.v_bytes = g_bytes_ref (node->value.v_bytes);
      break;

    case GST_AMF_TYPE_OBJECT:
    case GST_AMF_TYPE_ECMA_ARRAY:{
      guint i, len = gst_amf_node_get_num_fields (node);
      for (i = 0; i < len; i++) {
        const AmfObjectField *field = get_field (node, i);
        append_field (copy, g_strdup (field->name),
            gst_amf_node_copy (field->value));
      }
      break;
    }

    case GST_AMF_TYPE_STRICT_ARRAY:{
      guint i, len = gst_amf_node_get_num_elements (node);
      for (i = 0; i < len; i++) {
        const GstAmfNode *elem = get_element (node, i);
        append_element (copy, gst_amf_node_copy (elem));
      }
      break;
    }

    default:
      copy->value = node->value;
      break;
  }

  return copy;
}

void
gst_amf_node_free (gpointer ptr)
{
  GstAmfNode *node = ptr;

  switch (node->type) {
    case GST_AMF_TYPE_STRING:
    case GST_AMF_TYPE_LONG_STRING:
      g_bytes_unref (node->value.v_bytes);
      break;

    case GST_AMF_TYPE_OBJECT:
    case GST_AMF_TYPE_ECMA_ARRAY:
      g_array_unref (node->value.v_fields);
      break;

    case GST_AMF_TYPE_STRICT_ARRAY:
      g_ptr_array_unref (node->value.v_elements);
      break;

    default:
      break;
  }

  g_free (node);
}

GstAmfType
gst_amf_node_get_type (const GstAmfNode * node)
{
  g_return_val_if_fail (node, GST_AMF_TYPE_INVALID);
  return node->type;
}

gboolean
gst_amf_node_get_boolean (const GstAmfNode * node)
{
  g_return_val_if_fail (gst_amf_node_get_type (node) == GST_AMF_TYPE_BOOLEAN,
      FALSE);
  return node->value.v_int;
}

gdouble
gst_amf_node_get_number (const GstAmfNode * node)
{
  g_return_val_if_fail (gst_amf_node_get_type (node) == GST_AMF_TYPE_NUMBER,
      FALSE);
  return node->value.v_double;
}

gchar *
gst_amf_node_get_string (const GstAmfNode * node, gsize * out_size)
{
  gsize size;
  const gchar *data = gst_amf_node_peek_string (node, &size);

  if (out_size) {
    *out_size = size;
    return g_memdup2 (data, size);
  } else {
    return g_strndup (data, size);
  }
}

const gchar *
gst_amf_node_peek_string (const GstAmfNode * node, gsize * size)
{
  GstAmfType type = gst_amf_node_get_type (node);
  g_return_val_if_fail (type == GST_AMF_TYPE_STRING ||
      type == GST_AMF_TYPE_LONG_STRING, FALSE);
  return g_bytes_get_data (node->value.v_bytes, size);
}

const GstAmfNode *
gst_amf_node_get_field (const GstAmfNode * node, const gchar * name)
{
  guint i, len = gst_amf_node_get_num_fields (node);

  g_return_val_if_fail (name, NULL);

  for (i = 0; i < len; i++) {
    const AmfObjectField *field = get_field (node, i);
    if (strcmp (field->name, name) == 0) {
      return field->value;
    }
  }

  return NULL;
}

const GstAmfNode *
gst_amf_node_get_field_by_index (const GstAmfNode * node, guint index)
{
  guint len = gst_amf_node_get_num_fields (node);
  g_return_val_if_fail (index < len, NULL);
  return get_field (node, index)->value;
}

guint
gst_amf_node_get_num_fields (const GstAmfNode * node)
{
  GstAmfType type = gst_amf_node_get_type (node);
  g_return_val_if_fail (type == GST_AMF_TYPE_OBJECT ||
      type == GST_AMF_TYPE_ECMA_ARRAY, 0);
  return node->value.v_fields->len;
}

const GstAmfNode *
gst_amf_node_get_element (const GstAmfNode * node, guint index)
{
  guint len = gst_amf_node_get_num_elements (node);
  g_return_val_if_fail (index < len, NULL);
  return get_element (node, index);
}

guint
gst_amf_node_get_num_elements (const GstAmfNode * node)
{
  GstAmfType type = gst_amf_node_get_type (node);
  g_return_val_if_fail (type == GST_AMF_TYPE_STRICT_ARRAY, 0);
  return node->value.v_elements->len;
}

void
gst_amf_node_set_boolean (GstAmfNode * node, gboolean value)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_BOOLEAN);
  node->value.v_int = !!value;
}

void
gst_amf_node_set_number (GstAmfNode * node, gdouble value)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_NUMBER);
  node->value.v_double = value;
}

void
gst_amf_node_take_string (GstAmfNode * node, gchar * value, gssize size)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_STRING ||
      node->type == GST_AMF_TYPE_LONG_STRING);

  g_return_if_fail (value);

  if (size < 0) {
    size = strlen (value);
  }

  if (size > G_MAXUINT32) {
    GST_WARNING ("Long string too long (%" G_GSSIZE_FORMAT "), truncating",
        size);
    size = G_MAXUINT32;
    value[size] = 0;
  }

  if (size > G_MAXUINT16) {
    node->type = GST_AMF_TYPE_LONG_STRING;
  }

  g_bytes_unref (node->value.v_bytes);
  node->value.v_bytes = g_bytes_new_take (value, size);
}

void
gst_amf_node_set_string (GstAmfNode * node, const gchar * value, gssize size)
{
  gchar *copy;

  g_return_if_fail (value);

  if (size < 0) {
    size = strlen (value);
    copy = g_memdup2 (value, size + 1);
  } else {
    copy = g_memdup2 (value, size);
  }

  gst_amf_node_take_string (node, copy, size);
}

void
gst_amf_node_append_field (GstAmfNode * node, const gchar * name,
    const GstAmfNode * value)
{
  gst_amf_node_append_take_field (node, name, gst_amf_node_copy (value));
}

void
gst_amf_node_append_take_field (GstAmfNode * node, const gchar * name,
    GstAmfNode * value)
{
  g_return_if_fail (node->type == GST_AMF_TYPE_OBJECT ||
      node->type == GST_AMF_TYPE_ECMA_ARRAY);
  g_return_if_fail (name);
  append_field (node, g_strdup (name), value);
}

void
gst_amf_node_append_field_number (GstAmfNode * node, const gchar * name,
    gdouble value)
{
  gst_amf_node_append_take_field (node, name, gst_amf_node_new_number (value));
}

void
gst_amf_node_append_field_boolean (GstAmfNode * node, const gchar * name,
    gboolean value)
{
  gst_amf_node_append_take_field (node, name, gst_amf_node_new_boolean (value));
}

void
gst_amf_node_append_field_string (GstAmfNode * node, const gchar * name,
    const gchar * value, gssize size)
{
  gst_amf_node_append_take_field (node, name,
      gst_amf_node_new_string (value, size));
}

void
gst_amf_node_append_field_take_string (GstAmfNode * node, const gchar * name,
    gchar * value, gssize size)
{
  gst_amf_node_append_take_field (node, name,
      gst_amf_node_new_take_string (value, size));
}

/* Dumper *******************************************************************/

static inline void
dump_indent (GString * string, gint indent, guint depth)
{
  if (indent < 0) {
    g_string_append_c (string, ' ');
  } else {
    guint i;
    g_string_append_c (string, '\n');
    for (i = 0; i < indent + depth * 2; i++) {
      g_string_append_c (string, ' ');
    }
  }
}

static inline void
dump_bytes (GString * string, GBytes * value)
{
  gsize size;
  const gchar *data = g_bytes_get_data (value, &size);
  gst_rtmp_string_print_escaped (string, data, size);
}

static void
dump_node (GString * string, const GstAmfNode * node, gint indent,
    guint recursion_depth)
{
  const gchar *object_delim = "{}";

  switch (gst_amf_node_get_type (node)) {
    case GST_AMF_TYPE_NUMBER:
      g_string_append_printf (string, "%g", node->value.v_double);
      break;

    case GST_AMF_TYPE_BOOLEAN:
      g_string_append (string, node->value.v_int ? "True" : "False");
      break;

    case GST_AMF_TYPE_LONG_STRING:
      g_string_append_c (string, 'L');
      /* no break */
    case GST_AMF_TYPE_STRING:
      dump_bytes (string, node->value.v_bytes);
      break;

    case GST_AMF_TYPE_ECMA_ARRAY:
      object_delim = "[]";
      /* no break */
    case GST_AMF_TYPE_OBJECT:{
      guint i, len = gst_amf_node_get_num_fields (node);
      g_string_append_c (string, object_delim[0]);
      if (len) {
        for (i = 0; i < len; i++) {
          const AmfObjectField *field = get_field (node, i);
          dump_indent (string, indent, recursion_depth + 1);
          gst_rtmp_string_print_escaped (string, field->name, -1);
          g_string_append_c (string, ':');
          g_string_append_c (string, ' ');
          dump_node (string, field->value, indent, recursion_depth + 1);
          if (i < len - 1) {
            g_string_append_c (string, ',');
          }
        }
        dump_indent (string, indent, recursion_depth);
      }
      g_string_append_c (string, object_delim[1]);
      break;
    }

    case GST_AMF_TYPE_STRICT_ARRAY:{
      guint i, len = gst_amf_node_get_num_elements (node);
      g_string_append_c (string, '(');
      if (len) {
        for (i = 0; i < len; i++) {
          const GstAmfNode *value = get_element (node, i);
          dump_indent (string, indent, recursion_depth + 1);
          dump_node (string, value, indent, recursion_depth + 1);
          if (i < len - 1) {
            g_string_append_c (string, ',');
          }
        }
        dump_indent (string, indent, recursion_depth);
      }
      g_string_append_c (string, ')');
      break;
    }

    default:
      g_string_append (string, gst_amf_type_get_nick (node->type));
      break;
  }
}

void
gst_amf_node_dump (const GstAmfNode * node, gint indent, GString * string)
{
  dump_node (string, node, indent, 0);
}

static void
dump_argument (const GstAmfNode * node, guint n)
{
  if (G_UNLIKELY (GST_LEVEL_LOG <= _gst_debug_min) &&
      GST_LEVEL_LOG <= gst_debug_category_get_threshold (GST_CAT_DEFAULT)) {
    GString *string = g_string_new (NULL);
    gst_amf_node_dump (node, -1, string);
    GST_LOG ("Argument #%u: %s", n, string->str);
    g_string_free (string, TRUE);
  }
}

/* Parser *******************************************************************/

typedef struct
{
  const guint8 *data;
  gsize size, offset;
  guint8 recursion_depth;
} AmfParser;

static GstAmfNode *parse_value (AmfParser * parser);

static inline guint8
parse_u8 (AmfParser * parser)
{
  guint8 value;
  value = parser->data[parser->offset];
  parser->offset += sizeof value;
  return value;
}

static inline guint16
parse_u16 (AmfParser * parser)
{
  guint16 value;
  value = GST_READ_UINT16_BE (parser->data + parser->offset);
  parser->offset += sizeof value;
  return value;
}

static inline guint32
parse_u32 (AmfParser * parser)
{
  guint32 value;
  value = GST_READ_UINT32_BE (parser->data + parser->offset);
  parser->offset += sizeof value;
  return value;
}

static gdouble
parse_number (AmfParser * parser)
{
  gdouble value;

  if (sizeof value > parser->size - parser->offset) {
    GST_ERROR ("number too long");
    return 0.0;
  }

  value = GST_READ_DOUBLE_BE (parser->data + parser->offset);
  parser->offset += sizeof value;
  return value;
}

static gboolean
parse_boolean (AmfParser * parser)
{
  guint8 value;

  if (sizeof value > parser->size - parser->offset) {
    GST_ERROR ("boolean too long");
    return FALSE;
  }

  value = parse_u8 (parser);
  return !!value;
}

static inline GBytes *
read_string (AmfParser * parser, gsize size)
{
  gchar *string;

  if (size == 0) {
    return g_bytes_ref (empty_bytes);
  }

  if (size > parser->size - parser->offset) {
    GST_ERROR ("string too long (%" G_GSIZE_FORMAT ")", size);
    return NULL;
  }

  /* Null-terminate all incoming strings for internal safety */
  if (parser->data[parser->offset + size - 1] == 0) {
    string = g_malloc (size);
  } else {
    string = g_malloc (size + 1);
    string[size] = 0;
  }

  memcpy (string, parser->data + parser->offset, size);

  parser->offset += size;
  return g_bytes_new_take (string, size);
}

static GBytes *
parse_string (AmfParser * parser)
{
  guint16 size;

  if (sizeof size > parser->size - parser->offset) {
    GST_ERROR ("string size too long");
    return NULL;
  }

  size = parse_u16 (parser);
  return read_string (parser, size);
}

static GBytes *
parse_long_string (AmfParser * parser)
{
  guint32 size;

  if (sizeof size > parser->size - parser->offset) {
    GST_ERROR ("long string size too long");
    return NULL;
  }

  size = parse_u32 (parser);
  return read_string (parser, size);
}

static guint32
parse_object (AmfParser * parser, GstAmfNode * node)
{
  guint32 n_fields = 0;

  while (TRUE) {
    GBytes *name;
    gsize size;
    GstAmfNode *value;

    name = parse_string (parser);
    if (!name) {
      GST_ERROR ("object too long");
      break;
    }

    value = parse_value (parser);
    if (!value) {
      GST_ERROR ("object too long");
      g_bytes_unref (name);
      break;
    }

    if (gst_amf_node_get_type (value) == GST_AMF_TYPE_OBJECT_END) {
      g_bytes_unref (name);
      gst_amf_node_free (value);
      break;
    }

    if (g_bytes_get_size (name) == 0) {
      GST_ERROR ("empty object field name");
      g_bytes_unref (name);
      gst_amf_node_free (value);
      break;
    }

    append_field (node, g_bytes_unref_to_data (name, &size), value);
    n_fields++;
  };

  return n_fields;
}

static void
parse_ecma_array (AmfParser * parser, GstAmfNode * node)
{
  guint32 n_elements, n_read;

  if (sizeof n_elements > parser->size - parser->offset) {
    GST_ERROR ("array size too long");
    return;
  }

  n_elements = parse_u32 (parser);

  /* FIXME This is weird.  The one time I've seen this, the encoded value
   * was 0, but the number of elements was 1. */
  if (n_elements == 0) {
    GST_DEBUG ("Interpreting ECMA array length 0 as 1");
    n_elements = 1;
  }

  n_read = parse_object (parser, node);

  if (n_read != n_elements) {
    GST_WARNING ("Expected array with %" G_GUINT32_FORMAT " elements,"
        " but read %" G_GUINT32_FORMAT, n_elements, n_read);
  }
}

static void
parse_strict_array (AmfParser * parser, GstAmfNode * node)
{
  GstAmfNode *value = NULL;
  guint32 n_elements, i;

  if (sizeof n_elements > parser->size - parser->offset) {
    GST_ERROR ("array size too long");
    return;
  }

  n_elements = parse_u32 (parser);

  for (i = 0; i < n_elements; i++) {
    value = parse_value (parser);
    if (!value) {
      GST_ERROR ("array too long");
      break;
    }

    append_element (node, value);
  }
}

static GstAmfNode *
parse_value (AmfParser * parser)
{
  GstAmfNode *node = NULL;
  GstAmfType type;

  if (1 > parser->size - parser->offset) {
    GST_ERROR ("value too long");
    return NULL;
  }

  type = parse_u8 (parser);
  node = node_new (type);
  GST_TRACE ("parsing AMF type %d (%s)", type, gst_amf_type_get_nick (type));

  parser->recursion_depth++;
  if (parser->recursion_depth > MAX_RECURSION_DEPTH) {
    GST_ERROR ("maximum recursion depth %d reached", parser->recursion_depth);
    return node;
  }

  switch (type) {
    case GST_AMF_TYPE_NUMBER:
      node->value.v_double = parse_number (parser);
      break;
    case GST_AMF_TYPE_BOOLEAN:
      node->value.v_int = parse_boolean (parser);
      break;
    case GST_AMF_TYPE_STRING:
      node->value.v_bytes = parse_string (parser);
      break;
    case GST_AMF_TYPE_LONG_STRING:
      node->value.v_bytes = parse_long_string (parser);
      break;
    case GST_AMF_TYPE_OBJECT:
      parse_object (parser, node);
      break;
    case GST_AMF_TYPE_ECMA_ARRAY:
      parse_ecma_array (parser, node);
      break;
    case GST_AMF_TYPE_STRICT_ARRAY:
      parse_strict_array (parser, node);
      break;
    case GST_AMF_TYPE_NULL:
    case GST_AMF_TYPE_UNDEFINED:
    case GST_AMF_TYPE_OBJECT_END:
    case GST_AMF_TYPE_UNSUPPORTED:
      break;
    default:
      GST_ERROR ("unimplemented AMF type %d (%s)", type,
          gst_amf_type_get_nick (type));
      break;
  }

  parser->recursion_depth--;
  return node;
}

GstAmfNode *
gst_amf_node_parse (const guint8 * data, gsize size, guint8 ** endptr)
{
  AmfParser parser = {
    .data = data,
    .size = size,
  };
  GstAmfNode *node;

  g_return_val_if_fail (data, NULL);
  g_return_val_if_fail (size, NULL);

  init_static ();

  GST_TRACE ("Starting parse with %" G_GSIZE_FORMAT " bytes", parser.size);

  node = parse_value (&parser);
  if (!node || gst_amf_node_get_type (node) == GST_AMF_TYPE_INVALID) {
    GST_ERROR ("invalid value");
    goto out;
  }

  if (G_UNLIKELY (GST_LEVEL_LOG <= _gst_debug_min) &&
      GST_LEVEL_LOG <= gst_debug_category_get_threshold (GST_CAT_DEFAULT)) {
    GString *string = g_string_new (NULL);
    gst_amf_node_dump (node, -1, string);
    GST_LOG ("Parsed value: %s", string->str);
    g_string_free (string, TRUE);
  }

  GST_TRACE ("Done parsing; consumed %" G_GSIZE_FORMAT " bytes and left %"
      G_GSIZE_FORMAT " bytes", parser.offset, parser.size - parser.offset);

out:
  if (endptr) {
    *endptr = (guint8 *) parser.data + parser.offset;
  }

  return node;
}

GPtrArray *
gst_amf_parse_command (const guint8 * data, gsize size,
    gdouble * transaction_id, gchar ** command_name)
{
  AmfParser parser = {
    .data = data,
    .size = size,
  };
  GstAmfNode *node1 = NULL, *node2 = NULL;
  GPtrArray *args = NULL;

  g_return_val_if_fail (data, NULL);
  g_return_val_if_fail (size, NULL);

  init_static ();

  GST_TRACE ("Starting parse with %" G_GSIZE_FORMAT " bytes", parser.size);

  node1 = parse_value (&parser);
  if (!node1 || gst_amf_node_get_type (node1) != GST_AMF_TYPE_STRING) {
    GST_ERROR ("no command name");
    goto out;
  }

  node2 = parse_value (&parser);
  if (!node2 || gst_amf_node_get_type (node2) != GST_AMF_TYPE_NUMBER) {
    GST_ERROR ("no transaction ID");
    goto out;
  }

  GST_LOG ("Parsing command '%s', transid %.0f",
      gst_amf_node_peek_string (node1, NULL), gst_amf_node_get_number (node2));

  args = g_ptr_array_new_with_free_func (gst_amf_node_free);

  while (parser.offset < parser.size) {
    GstAmfNode *node = parse_value (&parser);
    if (!node) {
      break;
    }

    dump_argument (node, args->len);
    g_ptr_array_add (args, node);
  }

  GST_TRACE ("Done parsing; consumed %" G_GSIZE_FORMAT " bytes and left %"
      G_GSIZE_FORMAT " bytes", parser.offset, parser.size - parser.offset);

  if (args->len == 0) {
    GST_ERROR ("no command arguments");
    g_clear_pointer (&args, g_ptr_array_unref);
    goto out;
  }

  if (command_name) {
    *command_name = gst_amf_node_get_string (node1, NULL);
  }

  if (transaction_id) {
    *transaction_id = gst_amf_node_get_number (node2);
  }

out:
  g_clear_pointer (&node1, gst_amf_node_free);
  g_clear_pointer (&node2, gst_amf_node_free);
  return args;
}

/* Serializer ***************************************************************/

static void serialize_value (GByteArray * array, const GstAmfNode * node);

static inline void
serialize_u8 (GByteArray * array, guint8 value)
{
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

static inline void
serialize_u16 (GByteArray * array, guint16 value)
{
  value = GUINT16_TO_BE (value);
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

static inline void
serialize_u32 (GByteArray * array, guint32 value)
{
  value = GUINT32_TO_BE (value);
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

static inline void
serialize_number (GByteArray * array, gdouble value)
{
  value = GDOUBLE_TO_BE (value);
  g_byte_array_append (array, (guint8 *) & value, sizeof value);
}

static inline void
serialize_boolean (GByteArray * array, gboolean value)
{
  serialize_u8 (array, value);
}

static void
serialize_string (GByteArray * array, const gchar * string, gssize size)
{
  if (size < 0) {
    size = strlen (string);
  }

  if (size > G_MAXUINT16) {
    GST_WARNING ("String too long: %" G_GSSIZE_FORMAT, size);
    size = G_MAXUINT16;
  }

  serialize_u16 (array, size);
  g_byte_array_append (array, (guint8 *) string, size);
}

static void
serialize_long_string (GByteArray * array, const gchar * string, gssize size)
{
  if (size < 0) {
    size = strlen (string);
  }

  if (size > G_MAXUINT32) {
    GST_WARNING ("Long string too long: %" G_GSSIZE_FORMAT, size);
    size = G_MAXUINT32;
  }

  serialize_u32 (array, size);
  g_byte_array_append (array, (guint8 *) string, size);
}

static inline void
serialize_bytes (GByteArray * array, GBytes * bytes, gboolean long_string)
{
  gsize size;
  const gchar *data = g_bytes_get_data (bytes, &size);

  if (long_string) {
    serialize_long_string (array, data, size);
  } else {
    serialize_string (array, data, size);
  }
}

static void
serialize_object (GByteArray * array, const GstAmfNode * node)
{
  guint i;

  for (i = 0; i < gst_amf_node_get_num_fields (node); i++) {
    const AmfObjectField *field = get_field (node, i);
    serialize_string (array, field->name, -1);
    serialize_value (array, field->value);
  }
  serialize_u16 (array, 0);
  serialize_u8 (array, GST_AMF_TYPE_OBJECT_END);
}

static void
serialize_ecma_array (GByteArray * array, const GstAmfNode * node)
{
  /* FIXME: Shouldn't this be the field count? */
  serialize_u32 (array, 0);
  serialize_object (array, node);
}

static void
serialize_value (GByteArray * array, const GstAmfNode * node)
{
  serialize_u8 (array, node->type);
  switch (node->type) {
    case GST_AMF_TYPE_NUMBER:
      serialize_number (array, node->value.v_double);
      break;
    case GST_AMF_TYPE_BOOLEAN:
      serialize_boolean (array, node->value.v_int);
      break;
    case GST_AMF_TYPE_STRING:
      serialize_bytes (array, node->value.v_bytes, FALSE);
      break;
    case GST_AMF_TYPE_LONG_STRING:
      serialize_bytes (array, node->value.v_bytes, TRUE);
      break;
    case GST_AMF_TYPE_OBJECT:
      serialize_object (array, node);
      break;
    case GST_AMF_TYPE_ECMA_ARRAY:
      serialize_ecma_array (array, node);
      break;
    case GST_AMF_TYPE_NULL:
    case GST_AMF_TYPE_UNDEFINED:
    case GST_AMF_TYPE_OBJECT_END:
    case GST_AMF_TYPE_UNSUPPORTED:
      break;
    default:
      GST_ERROR ("unimplemented AMF type %d (%s)", node->type,
          gst_amf_type_get_nick (node->type));
      break;
  }
}

GBytes *
gst_amf_node_serialize (const GstAmfNode * node)
{
  GByteArray *array = g_byte_array_new ();

  g_return_val_if_fail (node, NULL);

  init_static ();

  if (G_UNLIKELY (GST_LEVEL_LOG <= _gst_debug_min) &&
      GST_LEVEL_LOG <= gst_debug_category_get_threshold (GST_CAT_DEFAULT)) {
    GString *string = g_string_new (NULL);
    gst_amf_node_dump (node, -1, string);
    GST_LOG ("Serializing value: %s", string->str);
    g_string_free (string, TRUE);
  }

  serialize_value (array, node);

  GST_TRACE ("Done serializing; produced %u bytes", array->len);

  return g_byte_array_free_to_bytes (array);
}

GBytes *
gst_amf_serialize_command (gdouble transaction_id, const gchar * command_name,
    const GstAmfNode * argument, ...)
{
  va_list ap;
  GBytes *ret;

  va_start (ap, argument);
  ret = gst_amf_serialize_command_valist (transaction_id, command_name,
      argument, ap);
  va_end (ap);

  return ret;
}

GBytes *
gst_amf_serialize_command_valist (gdouble transaction_id,
    const gchar * command_name, const GstAmfNode * argument, va_list var_args)
{
  GByteArray *array = g_byte_array_new ();
  guint i = 0;

  g_return_val_if_fail (command_name, NULL);
  g_return_val_if_fail (argument, NULL);

  init_static ();

  GST_LOG ("Serializing command '%s', transid %.0f", command_name,
      transaction_id);

  serialize_u8 (array, GST_AMF_TYPE_STRING);
  serialize_string (array, command_name, -1);
  serialize_u8 (array, GST_AMF_TYPE_NUMBER);
  serialize_number (array, transaction_id);

  while (argument) {
    serialize_value (array, argument);
    dump_argument (argument, i++);

    argument = va_arg (var_args, const GstAmfNode *);
  }

  GST_TRACE ("Done serializing; consumed %u args and produced %u bytes", i,
      array->len);

  return g_byte_array_free_to_bytes (array);
}
