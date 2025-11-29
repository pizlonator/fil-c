/* gtkbuilderparser.c
 * Copyright (C) 2019 Red Hat,
 *                         Alexander Larsson <alexander.larsson@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include "gtkbuilderprivate.h"
#include "gtkbuilder.h"
#include "gtkbuildableprivate.h"

/*****************************************  Record a GMarkup parser call ***************************/

typedef enum
{
  RECORD_TYPE_ELEMENT,
  RECORD_TYPE_END_ELEMENT,
  RECORD_TYPE_TEXT,
} RecordDataType;

/* All strings are owned by the string chunk */
typedef struct {
  GList link;

  const char *string;
  int len;
  int count;
  int offset;
  int text_offset;
  gboolean include_len;
} RecordDataString;

typedef struct {
  RecordDataType type;
  GList link;
} RecordDataNode;

typedef struct RecordDataElement RecordDataElement;

struct RecordDataElement {
  RecordDataNode base;

  RecordDataElement *parent;
  GQueue children;
  int n_attributes;
  gboolean preserve_whitespace;
  RecordDataString *name;
  RecordDataString *attributes[];
};

typedef struct {
  RecordDataNode base;

  RecordDataString *string;
} RecordDataText;

typedef struct {
  GHashTable *strings;
  GStringChunk *chunks;
  GQueue string_list;
  RecordDataElement *root;
  RecordDataElement *current;
} RecordData;

static gpointer
record_data_node_new (RecordDataElement *parent,
                      RecordDataType     type,
                      gsize              size)
{
  RecordDataNode *node = g_malloc0 (size);

  node->type = type;
  node->link.data = node;

  if (parent)
    g_queue_push_tail_link (&parent->children, &node->link);

  return node;
}

static gboolean
text_is_important (const char *name)
{
  const char *elements[] = {
    "property",
    "attribute",
    "col",
    "action-widget",
    "item",
    "mime-type",
    "pattern",
    "suffix",
    "mark",
    NULL
  };

  return g_strv_contains (elements, name);
}

static RecordDataElement *
record_data_element_new (RecordDataElement *parent,
                         RecordDataString  *name,
                         gsize              n_attributes)
{
  RecordDataElement *element;

  element = record_data_node_new (parent,
                                  RECORD_TYPE_ELEMENT,
                                  sizeof (RecordDataElement) +
                                  sizeof (RecordDataString) * n_attributes);
  element->parent = parent;
  element->name = name;
  element->preserve_whitespace = name && text_is_important (name->string);
  element->n_attributes = n_attributes;

  return element;
}

static void
record_data_element_append_text (RecordDataElement *parent,
                                 RecordDataString  *string)
{
  RecordDataText *text;

  text = record_data_node_new (parent,
                               RECORD_TYPE_TEXT,
                               sizeof (RecordDataText));
  text->string = string;
}

static void
record_data_node_free (RecordDataNode *node)
{
  GList *l, *next;
  RecordDataText *text;
  RecordDataElement *element;

  switch (node->type)
    {
    case RECORD_TYPE_ELEMENT:
      element = (RecordDataElement *)node;

      l = element->children.head;
      while (l)
        {
          next = l->next;
          record_data_node_free (l->data);
          l = next;
        }

      g_free (element);
      break;
    case RECORD_TYPE_TEXT:
      text = (RecordDataText *)node;
      g_free (text);
      break;
    case RECORD_TYPE_END_ELEMENT:
    default:
      g_assert_not_reached ();
    }
}

static gboolean
record_data_string_equal (gconstpointer _a,
                          gconstpointer _b)
{
  const RecordDataString *a = _a;
  const RecordDataString *b = _b;

  return a->len == b->len &&
         memcmp (a->string, b->string, a->len) == 0;
}

/* Copied from g_bytes_hash() */
static guint
record_data_string_hash (gconstpointer _a)
{
  const RecordDataString *a = _a;
  const signed char *p, *e;
  guint32 h = 5381;

  for (p = (signed char *)a->string, e = (signed char *)a->string + a->len; p != e; p++)
    h = (h << 5) + h + *p;

  return h;
}

static int
record_data_string_compare (gconstpointer _a,
                            gconstpointer _b,
                            gpointer      user_data)
{
  const RecordDataString *a = _a;
  const RecordDataString *b = _b;

  return b->count - a->count;
}

static RecordDataString *
record_data_string_lookup (RecordData *data,
                           const char *str,
                           gssize      len)
{
  RecordDataString *s, tmp;
  gboolean include_len = len >= 0;

  if (len < 0)
    len = strlen (str);

  tmp.string = str;
  tmp.len = len;

  s = g_hash_table_lookup (data->strings, &tmp);
  if (s)
    {
      s->count++;
      s->include_len |= include_len;
      return s;
    }

  s = g_new (RecordDataString, 1);
  /* The string is zero terminated */
  s->string = g_string_chunk_insert_len (data->chunks, str, len);
  s->len = len;
  s->count = 1;
  s->include_len = include_len;
  s->link.data = s;
  s->link.next = NULL;
  s->link.prev = NULL;

  g_hash_table_add (data->strings, s);
  g_queue_push_tail_link (&data->string_list, &s->link);
  return s;
}

static void
record_start_element (GMarkupParseContext  *context,
                      const char           *element_name,
                      const char          **names,
                      const char          **values,
                      gpointer              user_data,
                      GError              **error)
{
  gsize n_attrs = g_strv_length ((char **)names);
  RecordData *data = user_data;
  RecordDataElement *child;
  RecordDataString *name, **attr_names, **attr_values;
  int i;

  name = record_data_string_lookup (data, element_name, -1);
  child = record_data_element_new (data->current, name, n_attrs);
  data->current = child;

  attr_names = &child->attributes[0];
  attr_values = &child->attributes[n_attrs];
  for (i = 0; i < n_attrs; i++)
    {
      attr_names[i] = record_data_string_lookup (data, names[i], -1);
      attr_values[i] = record_data_string_lookup (data, values[i], -1);
    }
}

static void
record_end_element (GMarkupParseContext  *context,
                    const char           *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  RecordData *data = user_data;

  data->current = data->current->parent;
}

static gboolean
is_whitespace (const char *text,
               gsize       text_len)
{
  const char *end;
  const char *p;

  end = text + text_len;
  for (p = text; p < end; p = g_utf8_next_char (p))
    {
      if (!g_unichar_isspace (g_utf8_get_char (p)))
        return FALSE;
    }

  return TRUE;
}

static void
record_text (GMarkupParseContext  *context,
             const char           *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  RecordData *data = user_data;
  RecordDataString *string;

  if (text_len == 0)
    return;

  if (!data->current->preserve_whitespace && is_whitespace (text, text_len))
    return;

  string = record_data_string_lookup (data, text, text_len);
  record_data_element_append_text (data->current, string);
}

static const GMarkupParser record_parser =
{
  record_start_element,
  record_end_element,
  record_text,
  NULL, // passthrough, not stored
  NULL, // error, fails immediately
};

static void
marshal_uint32 (GString *str,
                guint32  v)
{
  /*
    We encode in a variable length format similar to
    utf8:

    v size      byte 1    byte 2    byte 3   byte 4  byte 5
    7 bit:    0xxxxxxx
    14 bit:   10xxxxxx  xxxxxxxx
    21 bit:   110xxxxx  xxxxxxxx  xxxxxxxx
    28 bit:   1110xxxx  xxxxxxxx  xxxxxxxx xxxxxxxx
    32 bit:   11110000  xxxxxxxx  xxxxxxxx xxxxxxxx xxxxxxx
  */

  if (v < 128)
    {
      g_string_append_c (str, (guchar)v);
    }
  else if (v < (1<<14))
    {
      g_string_append_c (str, (guchar)(v >> 8) | 0x80);
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else if (v < (1<<21))
    {
      g_string_append_c (str, (guchar)(v >> 16) | 0xc0);
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else if (v < (1<<28))
    {
      g_string_append_c (str, (guchar)(v >> 24) | 0xe0);
      g_string_append_c (str, (guchar)((v >> 16) & 0xff));
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
  else
    {
      g_string_append_c (str, 0xf0);
      g_string_append_c (str, (guchar)((v >> 24) & 0xff));
      g_string_append_c (str, (guchar)((v >> 16) & 0xff));
      g_string_append_c (str, (guchar)((v >> 8) & 0xff));
      g_string_append_c (str, (guchar)(v & 0xff));
    }
}

static int
marshal_uint32_len (guint32 v)
{
  if (v < 128)
    return 1;

  if (v < (1<<14))
    return 2;

  if (v < (1<<21))
    return 3;

  if (v < (1<<28))
    return 4;

  return 5;
}

static void
marshal_tree (GString        *marshaled,
              RecordDataNode *node)
{
  GList *l;
  int i;
  RecordDataText *text;
  RecordDataElement *element;
  RecordDataString **attr_names, **attr_values;

  switch (node->type)
    {
    case RECORD_TYPE_ELEMENT:
      element = (RecordDataElement *)node;

      marshal_uint32 (marshaled, RECORD_TYPE_ELEMENT);
      marshal_uint32 (marshaled, element->name->offset);
      marshal_uint32 (marshaled, element->n_attributes);

      attr_names = &element->attributes[0];
      attr_values = &element->attributes[element->n_attributes];
      for (i = 0; i < element->n_attributes; i++)
        {
          marshal_uint32 (marshaled, attr_names[i]->offset);
          marshal_uint32 (marshaled, attr_values[i]->offset);
        }

      for (l = element->children.head; l != NULL; l = l->next)
        marshal_tree (marshaled, l->data);

      marshal_uint32 (marshaled, RECORD_TYPE_END_ELEMENT);
      break;
    case RECORD_TYPE_TEXT:
      text = (RecordDataText *)node;
      marshal_uint32 (marshaled, RECORD_TYPE_TEXT);
      marshal_uint32 (marshaled, text->string->text_offset);
      break;
    case RECORD_TYPE_END_ELEMENT:
    default:
      g_assert_not_reached ();
    }
}

static void
marshal_root (GString        *marshaled,
              RecordDataNode *node)
{
  GList *l;
  RecordDataElement *element = (RecordDataElement *)node;

  for (l = element->children.head; l != NULL; l = l->next)
    marshal_tree (marshaled, l->data);
}

/**
 * _gtk_buildable_parser_precompile:
 * @text: chunk of text to parse
 * @text_len: length of @text in bytes
 *
 * Converts the xml format typically used by GtkBuilder to a
 * binary form that is more efficient to parse. This is a custom
 * format that is only supported by GtkBuilder.
 *
 * returns: A `GBytes` with the precompiled data
 **/
GBytes *
_gtk_buildable_parser_precompile (const char  *text,
                                  gssize       text_len,
                                  GError     **error)
{
  GMarkupParseContext *ctx;
  RecordData data = { 0 };
  GList *l;
  GString *marshaled;
  int offset;
  RecordDataString *node;

  data.strings = g_hash_table_new (record_data_string_hash, record_data_string_equal);
  data.chunks = g_string_chunk_new (512);
  data.root = record_data_element_new (NULL, NULL, 0);
  data.current = data.root;

  ctx = g_markup_parse_context_new (&record_parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);

  if (!g_markup_parse_context_parse (ctx, text, text_len, error) ||
      !g_markup_parse_context_end_parse (ctx, error))
    {
      record_data_node_free (&data.root->base);
      g_string_chunk_free (data.chunks);
      g_hash_table_destroy (data.strings);
      g_markup_parse_context_free (ctx);
      return NULL;
    }

  g_markup_parse_context_free (ctx);

  g_queue_sort (&data.string_list, record_data_string_compare, NULL);

  offset = 0;
  for (l = data.string_list.head; l != NULL; l = l->next)
    {
      RecordDataString *s = l->data;

      if (s->include_len)
        {
          s->text_offset = offset;
          offset += marshal_uint32_len (s->len);
        }

      s->offset = offset;
      offset += s->len + 1;
    }

  marshaled = g_string_sized_new (4 + offset + 32);
  /* Magic marker */
  g_string_append_len (marshaled, "GBU\0", 4);
  marshal_uint32 (marshaled, offset);

  for (l = data.string_list.head; l != NULL; l = l->next)
    {
      RecordDataString *s = l->data;

      if (s->include_len)
        marshal_uint32 (marshaled, s->len);

      g_string_append_len (marshaled, s->string, s->len + 1);
    }

  marshal_root (marshaled, &data.root->base);

  node = (RecordDataString *) data.string_list.head;
  while (node)
    {
      RecordDataString *next = (RecordDataString *) node->link.next;
      g_free (node);
      node = next;
    }

  record_data_node_free (&data.root->base);
  g_string_chunk_free (data.chunks);
  g_hash_table_destroy (data.strings);

  return g_string_free_to_bytes (marshaled);
}

/*****************************************  Replay GMarkup parser callbacks ***************************/

static guint32
demarshal_uint32 (const char **tree)
{
  const guchar *p = (const guchar *)*tree;
  guchar c = *p;
  /* see marshal_uint32 for format */

  if (c < 128) /* 7 bit */
    {
      *tree += 1;
      return c;
    }
  else if ((c & 0xc0) == 0x80) /* 14 bit */
    {
      *tree += 2;
      return (c & 0x3f) << 8 | p[1];
    }
  else if ((c & 0xe0) == 0xc0) /* 21 bit */
    {
      *tree += 3;
      return (c & 0x1f) << 16 | p[1] << 8 | p[2];
    }
  else if ((c & 0xf0) == 0xe0) /* 28 bit */
    {
      *tree += 4;
      return (c & 0xf) << 24 | p[1] << 16 | p[2] << 8 | p[3];
    }
  else
    {
      *tree += 5;
      return p[1] << 24 | p[2] << 16 | p[3] << 8 | p[4];
    }
}

static const char *
demarshal_string (const char **tree,
                  const char  *strings)
{
  guint32 offset = demarshal_uint32 (tree);

  return strings + offset;
}

static const char *
demarshal_text (const char **tree,
                const char  *strings,
                guint32     *len)
{
  guint32 offset = demarshal_uint32 (tree);
  const char *str = strings + offset;

  *len = demarshal_uint32 (&str);
  return str;
}

static void
propagate_error (GtkBuildableParseContext *context,
                 GError                  **dest,
                 GError                   *src)
{
  (*context->internal_callbacks->error) (NULL, src, context);
  g_propagate_error (dest, src);
}

static gboolean
replay_start_element (GtkBuildableParseContext  *context,
                      const char               **tree,
                      const char                *strings,
                      GError                   **error)
{
  const char *element_name;
  guint32 i, n_attrs;
  const char **attr_names;
  const char **attr_values;
  GError *tmp_error = NULL;

  element_name = demarshal_string (tree, strings);
  n_attrs = demarshal_uint32 (tree);

  attr_names = g_newa (const char *, n_attrs + 1);
  attr_values = g_newa (const char *, n_attrs + 1);
  for (i = 0; i < n_attrs; i++)
    {
      attr_names[i] = demarshal_string (tree, strings);
      attr_values[i] = demarshal_string (tree, strings);
    }
  attr_names[i] = NULL;
  attr_values[i] = NULL;

  (* context->internal_callbacks->start_element) (NULL,
                                                  element_name,
                                                  attr_names,
                                                  attr_values,
                                                  context,
                                                  &tmp_error);

  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
replay_end_element (GtkBuildableParseContext  *context,
                    const char               **tree,
                    const char                *strings,
                    GError                   **error)
{
  GError *tmp_error = NULL;

  (* context->internal_callbacks->end_element) (NULL,
                                                gtk_buildable_parse_context_get_element (context),
                                                context,
                                                &tmp_error);
  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
replay_text (GtkBuildableParseContext  *context,
             const char               **tree,
             const char                *strings,
             GError                   **error)
{
  guint32 len;
  const char *text;
  GError *tmp_error = NULL;

  text = demarshal_text (tree, strings, &len);

  (*context->internal_callbacks->text) (NULL,
                                        text,
                                        len,
                                        context,
                                        &tmp_error);

  if (tmp_error)
    {
      propagate_error (context, error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

gboolean
_gtk_buildable_parser_is_precompiled (const char *data,
                                      gssize      data_len)
{
  return
    data_len > 4 &&
    data[0] == 'G' &&
    data[1] == 'B' &&
    data[2] == 'U' &&
    data[3] == 0;
}

gboolean
_gtk_buildable_parser_replay_precompiled (GtkBuildableParseContext  *context,
                                          const char                *data,
                                          gssize                     data_len,
                                          GError                   **error)
{
  const char *data_end = data + data_len;
  guint32 type, len;
  const char *strings;
  const char *tree;

  data = data + 4; /* Skip header */

  len = demarshal_uint32 (&data);

  strings = data;
  data = data + len;
  tree = data;

  while (tree < data_end)
    {
      gboolean res;
      type = demarshal_uint32 (&tree);

      switch (type)
        {
        case RECORD_TYPE_ELEMENT:
          res = replay_start_element (context, &tree, strings, error);
          break;
        case RECORD_TYPE_END_ELEMENT:
          res = replay_end_element (context, &tree, strings, error);
          break;
        case RECORD_TYPE_TEXT:
          res = replay_text (context, &tree, strings, error);
          break;
        default:
          g_assert_not_reached ();
        }

      if (!res)
        return FALSE;
    }

  return TRUE;
}
