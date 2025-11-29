/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "gtkcomposetable.h"
#include "gtkimcontextsimple.h"
#include "gtkprivate.h"


#define GTK_COMPOSE_TABLE_MAGIC "GtkComposeTable"
#define GTK_COMPOSE_TABLE_VERSION (4)

extern const GtkComposeTable builtin_compose_table;

/* Maximum length of sequences we parse */

#define MAX_COMPOSE_LEN 20

/* Implemented from g_str_hash() */
static guint32
data_hash (gconstpointer v, int length)
{
  const guint16 *p, *head;
  unsigned char c;
  guint32 h = 5381;

  for (p = v, head = v; (p - head) < length; p++)
    {
      c = 0x00ff & (*p >> 8);
      h = (h << 5) + h + c;
      c = 0x00ff & *p;
      h = (h << 5) + h + c;
    }

  return h;
}

static guint32
sequence_hash (gconstpointer v)
{
  const gunichar *p = v;
  int i;

  for (i = 0; p[i]; i++) ;

  return data_hash (v, i);
}

static gboolean
sequence_equal (gconstpointer v1,
                gconstpointer v2)
{
  const gunichar *p1 = v1;
  const gunichar *p2 = v2;
  int i;

  for (i = 0; p1[i] && p2[i] && p1[i] == p2[i]; i++) ;

  return p1[i] == p2[i];
}

typedef struct {
  GHashTable *sequences;
  GList *files;
  const char *compose_file;
  gboolean found_include;
} GtkComposeParser;

static GtkComposeParser *
parser_new (void)
{
  GtkComposeParser *parser;

  parser = g_new (GtkComposeParser, 1);

  parser->sequences = g_hash_table_new_full (sequence_hash, sequence_equal, g_free, g_free);
  parser->files = NULL;
  parser->compose_file = NULL;
  parser->found_include = FALSE;

  return parser;
}

static void
parser_free (GtkComposeParser *parser)
{
  g_hash_table_unref (parser->sequences);
  g_list_free_full (parser->files, g_free);
  g_free (parser);
}

static gboolean
is_codepoint (const char *str)
{
  int i;

  /* 'U' is not code point but 'U00C0' is code point */
  if (str[0] == '\0' || str[0] != 'U' || str[1] == '\0')
    return FALSE;

  for (i = 1; str[i] != '\0'; i++)
    {
      if (!g_ascii_isxdigit (str[i]))
        return FALSE;
    }

  return TRUE;
}

static char *
parse_compose_value (const char *val,
                     const char *line)
{
  const char *p;
  GString *value;
  gunichar ch;
  char *endp;

  value = g_string_new ("");

  if (val[0] != '"')
    {
      g_warning ("Only strings supported after ':': %s: %s", val, line);
      goto fail;
    }

  p = val + 1;
  while (*p)
    {
      if (*p == '\"')
        {
          return g_string_free (value, FALSE);
        }

      if (p[1] == '\0')
        {
          g_warning ("Missing closing '\"': %s: %s", val, line);
          goto fail;
        }
      else if (*p == '\\')
        {
          if (p[1] == '"')
            {
              g_string_append_c (value, '"');
              p += 2;
            }
          else if (p[1] == '\\')
            {
              g_string_append_c (value, '\\');
              p += 2;
            }
          else if (p[1] >= '0' && p[1] < '8')
            {
              ch = g_ascii_strtoll (p + 1, &endp, 8);
              if (ch == 0)
                {
                  g_warning ("Invalid escape sequence: %s: %s", val, line);
                  goto fail;
                }
              g_string_append_unichar (value, ch);
              p = endp;
            }
          else if (p[1] == 'x' || p[1] == 'X')
            {
              ch = g_ascii_strtoll (p + 2, &endp, 16);
              if (ch == 0)
                {
                  g_warning ("Invalid escape sequence: %s: %s", val, line);
                  goto fail;
                }
              g_string_append_unichar (value, ch);
              p = endp;
            }
          else
            {
              g_warning ("Invalid escape sequence: %s: %s", val, line);
              goto fail;
            }
        }
      else
        {
          ch = g_utf8_get_char (p);
          g_string_append_unichar (value, ch);
          p = g_utf8_next_char (p);
        }
    }

fail:
  g_string_free (value, TRUE);
  return NULL;
}

static gunichar *
parse_compose_sequence (const char *seq,
                        const char *line)
{
  char **words = g_strsplit (seq, "<", -1);
  int i;
  int n = 0;
  gunichar *sequence = NULL;

  if (g_strv_length (words) < 2)
    {
      g_warning ("key sequence format is <a> <b>...: %s", line);
      goto fail;
    }

  for (i = 1; words[i] != NULL; i++)
    {
      char *start = words[i];
      char *end = strchr (words[i], '>');
      char *match;
      guint keyval;

      if (words[i][0] == '\0')
             continue;

      if (start == NULL || end == NULL || end <= start)
        {
          g_warning ("key sequence format is <a> <b>...: %s", line);
          goto fail;
        }

      match = g_strndup (start, end - start);

      sequence = g_realloc (sequence, sizeof (gunichar) * (n + 2));

      if (is_codepoint (match))
        {
          keyval = gdk_unicode_to_keyval ((gunichar) g_ascii_strtoll (match + 1, NULL, 16));
          if (keyval > 0xffff)
            g_warning ("Can't handle >16bit keyvals");

          sequence[n] = (guint16) keyval;
          sequence[n + 1] = 0;
        }
      else
        {
          keyval = gdk_keyval_from_name (match);
          if (keyval > 0xffff)
            g_warning ("Can't handle >16bit keyvals");

          sequence[n] = (guint16) keyval;
          sequence[n + 1] = 0;
        }

      if (keyval == GDK_KEY_VoidSymbol)
        g_warning ("Could not get code point of keysym %s", match);
      g_free (match);
      n++;
    }

  if (0 == n || n > MAX_COMPOSE_LEN)
    {
      g_warning ("Suspicious compose sequence length (%d). Are you sure this is right?: %s",
                 n, line);
      goto fail;
    }

  g_strfreev (words);

  return sequence;

fail:
  g_strfreev (words);
  g_free (sequence);
  return NULL;
}

static void parser_parse_file (GtkComposeParser *parser,
                               const char       *path);

char *
gtk_compose_table_get_x11_compose_file_dir (void)
{
  char * compose_file_dir;

#if defined (X11_DATA_PREFIX)
  compose_file_dir = g_strdup (X11_DATA_PREFIX "/share/X11/locale");
#else
  compose_file_dir = g_build_filename (_gtk_get_datadir (), "X11", "locale", NULL);
#endif

  return compose_file_dir;
}

/* Substitute %H, %L and %S */
static char *
handle_substitutions (const char *start,
                      int         length)
{
  GString *s;
  const char *locale_name;
   const char *p;

  s = g_string_new ("");

  locale_name = getenv ("LANG");

  for (p = start; *p && p < start + length; p++)
    {
      if (*p != '%')
        {
          g_string_append_c (s, *p);
        }
      else
        {
          char *x11_compose_file_dir;
          char *path;

          switch (p[1])
            {
            case 'H':
              p++;
              g_string_append (s, g_get_home_dir ());
              break;
            case 'L':
              p++;
              x11_compose_file_dir = gtk_compose_table_get_x11_compose_file_dir ();
              path = g_build_filename (x11_compose_file_dir, locale_name, "Compose", NULL);
              g_string_append (s, path);
              g_free (path);
              g_free (x11_compose_file_dir);
              break;
            case 'S':
              p++;
              x11_compose_file_dir = gtk_compose_table_get_x11_compose_file_dir ();
              g_string_append (s, x11_compose_file_dir);
              g_free (x11_compose_file_dir);
              break;
            default: ;
              /* do nothing, next iteration handles p[1] */
            }
        }
    }

  return g_string_free (s, FALSE);
}

static void
add_sequence (gunichar   *sequence,
              int         len,
              const char *value,
              gpointer    data)
{
  GtkComposeParser *parser = data;
  gunichar *seq;

  seq = g_new (gunichar, len + 1);
  memcpy (seq, sequence, (len + 1) * sizeof (gunichar));

  g_hash_table_replace (parser->sequences, seq, g_strdup (value));
}

static void
parser_add_default_sequences (GtkComposeParser *parser)
{
  const GtkComposeTable *table = &builtin_compose_table;

  gtk_compose_table_foreach (table, add_sequence, parser);
}

static void
parser_handle_include (GtkComposeParser *parser,
                       const char       *line)
{
  const char *p;
  const char *start, *end;
  char *path;

  parser->found_include = TRUE;

  p = line + strlen ("include ");

  while (g_ascii_isspace (*p))
    p++;

  if (*p != '"')
    goto error;

  p++;

  start = p;

  while (*p && *p != '"')
    p++;

  if (*p != '"')
    goto error;

  end = p;

  p++;

  while (g_ascii_isspace (*p))
    p++;

  if (*p && *p != '#')
    goto error;

  if (end - start == 2 &&
      strncmp ("%L", start, end - start) == 0)
    {
      parser_add_default_sequences (parser);
    }
  else
    {
      path = handle_substitutions (start, end - start);
      parser_parse_file (parser, path);
      g_free (path);
    }

  return;

error:
  g_warning ("Could not parse include: %s", line);
}

static void
parser_parse_line (GtkComposeParser *parser,
                   const char       *line)
{
  char **components = NULL;
  gunichar *sequence = NULL;
  char *value = NULL;

  if (line[0] == '\0' || line[0] == '#')
    return;

  if (g_str_has_prefix (line, "include "))
    {
      parser_handle_include (parser, line);
      return;
    }

  components = g_strsplit (line, ":", 2);

  if (components[1] == NULL)
    {
      g_warning ("No delimiter ':': %s", line);
      goto fail;
    }

  sequence = parse_compose_sequence (g_strstrip (components[0]), line);
  if (sequence == NULL)
    goto fail;

  value = parse_compose_value (g_strstrip (components[1]), line);
  if (value == NULL)
    goto fail;

  g_strfreev (components);

  g_hash_table_replace (parser->sequences, sequence, value);

  return;

fail:
  g_strfreev (components);
  g_free (sequence);
  g_free (value);
}

static void
parser_read_file (GtkComposeParser *parser,
                  const char       *compose_file)
{
  char *contents = NULL;
  char **lines = NULL;
  gsize length = 0;
  GError *error = NULL;

  if (!g_file_get_contents (compose_file, &contents, &length, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  lines = g_strsplit (contents, "\n", -1);
  for (int i = 0; lines[i] != NULL; i++)
    parser_parse_line (parser, lines[i]);

  g_strfreev (lines);
  g_free (contents);
}

/* Remove sequences that can be handled algorithmically,
 * sequences with non-BMP keys, and sequences that produce
 * empty strings.
 */
static void
parser_remove_duplicates (GtkComposeParser *parser)
{
  GHashTableIter iter;
  gunichar *sequence;
  char *value;
  GString *output;

  output = g_string_new ("");

  g_hash_table_iter_init (&iter, parser->sequences);
  while (g_hash_table_iter_next (&iter, (gpointer *)&sequence, (gpointer *)&value))
    {
      static guint keysyms[MAX_COMPOSE_LEN + 1];
      int i;
      int n_compose = 0;
      gboolean remove_sequence = FALSE;

      if (value[0] == '\0')
        {
          remove_sequence = TRUE;
          goto next;
        }

      for (i = 0; i < MAX_COMPOSE_LEN + 1; i++)
        keysyms[i] = 0;

      for (i = 0; i < MAX_COMPOSE_LEN + 1; i++)
        {
          guint codepoint = sequence[i];
          keysyms[i] = codepoint;

          if (codepoint == 0)
            break;

          if (codepoint > 0xffff)
            {
              remove_sequence = TRUE;
              goto next;
            }

          n_compose++;
        }

      if (gtk_check_algorithmically (keysyms, n_compose, output))
        {
          if (strcmp (value, output->str) == 0)
            remove_sequence = TRUE;
        }

next:
      if (remove_sequence)
        g_hash_table_iter_remove (&iter);
    }

  g_string_free (output, TRUE);
}

static void
parser_compute_max_compose_len (GtkComposeParser *parser,
                                int              *max_compose_len,
                                int              *n_first,
                                int              *size)
{
  GHashTableIter iter;
  gunichar *sequence;
  char *value;
  int max = 0;
  int count = 0;
  GHashTable *first;

  first = g_hash_table_new (NULL, NULL);

  g_hash_table_iter_init (&iter, parser->sequences);
  while (g_hash_table_iter_next (&iter, (gpointer *)&sequence, (gpointer *)&value))
    {
      g_hash_table_add (first, GUINT_TO_POINTER (sequence[0]));

      for (int i = 0; i < MAX_COMPOSE_LEN + 1; i++)
        {
          if (sequence[i] == 0)
            {
              count += i;
              if (max < i)
                max = i;
              break;
            }
        }
    }

  *max_compose_len = max;
  *n_first = g_hash_table_size (first);
  *size = count;

  g_hash_table_unref (first);
}

static inline int
sequence_length (gpointer a)
{
  gunichar *seq = a;
  int i;

  for (i = 0; seq[i]; i++) ;

  return i;
}

static int
sequence_compare (gpointer a,
                  gpointer b,
                  gpointer data)
{
  gunichar *seq_a = a;
  gunichar *seq_b = b;
  int i;
  gunichar code_a, code_b;
  int len_a, len_b;

  code_a = seq_a[0];
  code_b = seq_b[0];

  if (code_a != code_b)
    return code_a - code_b;

  len_a = sequence_length (a);
  len_b = sequence_length (b);

  if (len_a != len_b)
    return len_a - len_b;

  for (i = 1; i < len_a; i++)
    {
      code_a = seq_a[i];
      code_b = seq_b[i];

      if (code_a != code_b)
        return code_a - code_b;
    }

  return 0;
}

guint32
gtk_compose_table_data_hash (const guint16 *data,
                             int            max_seq_len,
                             int            n_seqs)
{
  gsize n_index_stride;
  gsize length;

  n_index_stride = max_seq_len + 2;
  if (!g_size_checked_mul (&length, n_index_stride, n_seqs))
    {
      g_critical ("Overflow in the compose sequences");
      return 0;
    }

  return data_hash (data, length);
}

static char *
gtk_compose_hash_get_cache_path (guint32 hash)
{
  char *basename = NULL;
  char *dir = NULL;
  char *path = NULL;

  basename = g_strdup_printf ("%08x.cache", hash);

  dir = g_build_filename (g_get_user_cache_dir (), "gtk-4.0", "compose", NULL);
  path = g_build_filename (dir, basename, NULL);
  if (g_mkdir_with_parents (dir, 0755) != 0)
    {
      g_warning ("Failed to mkdir %s", dir);
      g_free (path);
      path = NULL;
    }

  g_free (dir);
  g_free (basename);

  return path;
}

static char *
gtk_compose_table_serialize (GtkComposeTable *compose_table,
                             gsize           *count)
{
  char *p, *contents;
  gsize header_length, total_length;
  guint16 bytes;
  const char *header = GTK_COMPOSE_TABLE_MAGIC;
  const guint16 version = GTK_COMPOSE_TABLE_VERSION;
  guint16 max_seq_len = compose_table->max_seq_len;
  guint16 n_index_size = compose_table->n_index_size;
  guint16 data_size = compose_table->data_size;
  guint16 n_chars = compose_table->n_chars;
  guint32 i;

  g_return_val_if_fail (compose_table != NULL, NULL);
  g_return_val_if_fail (max_seq_len > 0, NULL);
  g_return_val_if_fail (n_index_size > 0, NULL);

  header_length = strlen (header);
  total_length = header_length + sizeof (guint16) * (5 + data_size) + n_chars;
  if (count)
    *count = total_length;

  p = contents = g_malloc (total_length);

  memcpy (p, header, header_length);
  p += header_length;

#define APPEND_GUINT16(elt) \
  bytes = GUINT16_TO_BE (elt); \
  memcpy (p, &bytes, sizeof (guint16)); \
  p += sizeof (guint16);

  APPEND_GUINT16 (version);
  APPEND_GUINT16 (max_seq_len);
  APPEND_GUINT16 (n_index_size);
  APPEND_GUINT16 (data_size);
  APPEND_GUINT16 (n_chars);

  for (i = 0; i < data_size; i++)
    {
      APPEND_GUINT16 (compose_table->data[i]);
    }

  if (compose_table->n_chars > 0)
    memcpy (p, compose_table->char_data, compose_table->n_chars);

#undef APPEND_GUINT16

  return contents;
}

static GtkComposeTable *
gtk_compose_table_load_cache (const char *compose_file,
                              gboolean   *found_old_cache)
{
  guint32 hash;
  char *path = NULL;
  char *contents = NULL;
  char *p;
  GStatBuf original_buf;
  GStatBuf cache_buf;
  gsize total_length;
  GError *error = NULL;
  guint16 bytes;
  guint16 version;
  guint16 max_seq_len;
  guint16 n_index_size;
  guint16 data_size;
  guint16 n_chars;
  guint32 i;
  guint16 *data = NULL;
  char *char_data = NULL;
  GtkComposeTable *retval;

  *found_old_cache = FALSE;

  hash = g_str_hash (compose_file);
  if ((path = gtk_compose_hash_get_cache_path (hash)) == NULL)
    return NULL;
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    goto out_load_cache;

  g_stat (path, &cache_buf);
  g_lstat (compose_file, &original_buf);
  if (original_buf.st_mtime > cache_buf.st_mtime)
    goto out_load_cache;
  g_stat (compose_file, &original_buf);
  if (original_buf.st_mtime > cache_buf.st_mtime)
    goto out_load_cache;
  if (!g_file_get_contents (path, &contents, &total_length, &error))
    {
      g_warning ("Failed to get cache content %s: %s", path, error->message);
      g_error_free (error);
      goto out_load_cache;
    }

#define GET_GUINT16(elt) \
  memcpy (&bytes, p, sizeof (guint16)); \
  elt = GUINT16_FROM_BE (bytes); \
  p += sizeof (guint16);

  p = contents;
  if (g_ascii_strncasecmp (p, GTK_COMPOSE_TABLE_MAGIC,
                           strlen (GTK_COMPOSE_TABLE_MAGIC)) != 0)
    {
      g_warning ("The file is not a GtkComposeTable cache file %s", path);
      goto out_load_cache;
    }

  p += strlen (GTK_COMPOSE_TABLE_MAGIC);
  if (p - contents > total_length)
    {
      g_warning ("Broken cache content %s at head", path);
      goto out_load_cache;
    }

  GET_GUINT16 (version);
  if (version != GTK_COMPOSE_TABLE_VERSION)
    {
      if (version < GTK_COMPOSE_TABLE_VERSION)
        *found_old_cache = TRUE;
      goto out_load_cache;
    }

  GET_GUINT16 (max_seq_len);
  GET_GUINT16 (n_index_size);
  GET_GUINT16 (data_size);
  GET_GUINT16 (n_chars);

  if (max_seq_len == 0 || data_size == 0)
    {
      g_warning ("cache size is not correct %d %d", max_seq_len, data_size);
      goto out_load_cache;
    }

  data = g_new0 (guint16, data_size);

  for (i = 0; i < data_size; i++)
    {
      GET_GUINT16 (data[i]);
    }

  if (n_chars > 0)
    {
      char_data = g_new (char, n_chars + 1);
      memcpy (char_data, p, n_chars);
      char_data[n_chars] = '\0';
    }

  retval = g_new0 (GtkComposeTable, 1);
  retval->data = data;
  retval->max_seq_len = max_seq_len;
  retval->n_index_size = n_index_size;
  retval->data_size = data_size;
  retval->char_data = char_data;
  retval->n_chars = n_chars;
  retval->id = hash;

  g_free (contents);
  g_free (path);

  return retval;

#undef GET_GUINT16

out_load_cache:
  g_free (data);
  g_free (char_data);
  g_free (contents);
  g_free (path);
  return NULL;
}

static void
gtk_compose_table_save_cache (GtkComposeTable *compose_table)
{
  char *path = NULL;
  char *contents = NULL;
  GError *error = NULL;
  gsize length = 0;

  if ((path = gtk_compose_hash_get_cache_path (compose_table->id)) == NULL)
    return;

  contents = gtk_compose_table_serialize (compose_table, &length);
  if (contents == NULL)
    {
      g_warning ("Failed to serialize compose table %s", path);
      goto out_save_cache;
    }
  if (!g_file_set_contents (path, contents, length, &error))
    {
      g_warning ("Failed to save compose table %s: %s", path, error->message);
      g_error_free (error);
      goto out_save_cache;
    }

out_save_cache:
  g_free (contents);
  g_free (path);
}

static GtkComposeTable *
parser_get_compose_table (GtkComposeParser *parser)
{
  guint16 *data;
  GtkComposeTable *table;
  guint16 encoded_value;
  GString *char_data;
  int max_compose_len;
  GList *sequences;
  GList *list;
  int i;
  int size;
  int n_first;
  int first_pos;
  int rest_pos;
  int index_rowstride;
  int n_sequences;
  gunichar current_first;

  parser_remove_duplicates (parser);

  if (g_hash_table_size (parser->sequences) == 0)
    return NULL;

  parser_compute_max_compose_len (parser, &max_compose_len, &n_first, &size);

  sequences = g_hash_table_get_keys (parser->sequences);

  sequences = g_list_sort_with_data (sequences,
                                     (GCompareDataFunc) sequence_compare,
                                     NULL);

  index_rowstride = max_compose_len + 2;
  data = g_new0 (guint16, n_first * index_rowstride + size);

  char_data = g_string_new ("");

  n_sequences = 0;
  current_first = 0;
  first_pos = 0;
  rest_pos = n_first * index_rowstride;

  for (list = sequences; list != NULL; list = list->next)
    {
      gunichar *sequence = list->data;
      char *value = g_hash_table_lookup (parser->sequences, sequence);
      int len = sequence_length (sequence);

      g_assert (1 <= len && len <= max_compose_len);

      /* Encode the value. If the value is a single
       * character with a value smaller than 1 << 15,
       * we just use it directly.
       * Otherwise, we store the value as string and
       * put the offset into the table, with the high
       * bit set.
       */
      if (g_utf8_strlen (value, -1) == 1 &&
          g_utf8_get_char (value) < 0x8000)
        {
          encoded_value = (guint16) g_utf8_get_char (value);
        }
      else
        {
          if (char_data->len > 0)
            g_string_append_c (char_data, 0);

          if (char_data->len >= 0x8000)
            {
              g_warning ("GTK can't handle compose tables this large (%s)", parser->compose_file ? parser->compose_file : "");
              g_free (data);
              g_string_free (char_data, TRUE);
              return NULL;
            }

          encoded_value = (guint16) (char_data->len | 0x8000);
          g_string_append (char_data, value);
        }

      if (sequence[0] != current_first)
        {
          g_assert (sequence[0] <= 0xffff);
          if (current_first != 0)
            first_pos += index_rowstride;
          current_first = (guint16)sequence[0];

          data[first_pos] = (guint16)sequence[0];

          for (i = 1; i < index_rowstride; i++)
            data[first_pos + i] = rest_pos;
        }

      for (i = 1; i < len; i++)
        {
          g_assert (sequence[i] != 0);
          g_assert (sequence[i] <= 0xffff);
          data[rest_pos + i - 1] = (guint16) sequence[i];
        }

      g_assert (encoded_value != 0);
      data[rest_pos + len - 1] = encoded_value;

      n_sequences++;

      rest_pos += len;

      if (rest_pos >= 0x8000)
        {
          g_warning ("GTK can't handle compose tables this large (%s)", parser->compose_file ? parser->compose_file : "");
          g_free (data);
          g_string_free (char_data, TRUE);
          return NULL;
        }

      for (i = len + 1; i < index_rowstride; i++)
        data[first_pos + i] = rest_pos;

      for (i = 1; i + 1 < index_rowstride; i++)
        g_assert (data[first_pos + i] <= data[first_pos + i + 1]);
    }

  g_assert (first_pos + index_rowstride == n_first * index_rowstride);
  g_assert (rest_pos == n_first * index_rowstride + size);

  if (char_data->len > 0)
    g_string_append_c (char_data, 0);

  table = g_new0 (GtkComposeTable, 1);
  table->data = data;
  table->data_size = n_first * index_rowstride + size;
  table->max_seq_len = max_compose_len;
  table->n_index_size = n_first;
  table->n_chars = char_data->len;
  table->char_data = g_string_free (char_data, FALSE);
  table->n_sequences = n_sequences;
  table->id = g_str_hash (parser->compose_file);

  g_list_free (sequences);

  return table;
}

static char *
canonicalize_filename (const char *parent_path,
                       const char *path)
{
  GFile *file;
  char *retval;

  if (path[0] != '/' && parent_path)
    {
      GFile *orig = g_file_new_for_path (parent_path);
      GFile *parent = g_file_get_parent (orig);
      file = g_file_resolve_relative_path (parent, path);
      g_object_unref (parent);
      g_object_unref (orig);
    }
  else
    {
      file = g_file_new_for_path (path);
    }

  retval = g_file_get_path (file);

  g_object_unref (file);

  return retval;
}

static void
parser_parse_file (GtkComposeParser *parser,
                   const char       *compose_file)
{
  char *path;

  // stash the name for the table hash
  if (parser->compose_file == NULL)
    parser->compose_file = compose_file;

  path = canonicalize_filename (parser->compose_file, compose_file);

  if (g_list_find_custom (parser->files, path, (GCompareFunc)strcmp))
    {
      g_warning ("include cycle detected: %s", compose_file);
      g_free (path);
      return;
    }

  parser->files = g_list_prepend (parser->files, path);

  parser_read_file (parser, path);

  parser->files = g_list_remove (parser->files, path);
}

GtkComposeTable *
gtk_compose_table_parse (const char *compose_file,
                         gboolean   *found_include)
{
  GtkComposeParser *parser;
  GtkComposeTable *compose_table;

  parser = parser_new ();
  parser_parse_file (parser, compose_file);
  compose_table = parser_get_compose_table (parser);
  if (found_include)
    *found_include = parser->found_include;
  parser_free (parser);

  return compose_table;
}

static gboolean
rewrite_compose_file (const char *compose_file)
{
  static const char *prefix =
    "# GTK has rewritten this file to add the line:\n"
    "\n"
    "include \"%L\"\n"
    "\n"
    "# This is necessary to add your own Compose sequences\n"
    "# in addition to the builtin sequences of GTK. If this\n"
    "# is not what you want, just remove that line.\n"
    "#\n"
    "# A backup of the previous file contents has been made.\n"
    "\n"
    "\n";

  char *path = NULL;
  char *content = NULL;
  gsize content_len;
  GFile *file = NULL;
  GOutputStream *stream = NULL;
  gboolean ret = FALSE;

  path = canonicalize_filename (NULL, compose_file);

  if (!g_file_get_contents (path, &content, &content_len, NULL))
    goto out;

  file = g_file_new_for_path (path);
  stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, TRUE, 0, NULL, NULL));

  if (stream == NULL)
    goto out;

  if (!g_output_stream_write (stream, prefix, strlen (prefix), NULL, NULL))
    goto out;

  if (!g_output_stream_write (stream, content, content_len, NULL, NULL))
    goto out;

  if (!g_output_stream_close (stream, NULL, NULL))
    goto out;

  ret = TRUE;

out:
  g_clear_object (&stream);
  g_clear_object (&file);
  g_clear_pointer (&path, g_free);
  g_clear_pointer (&content, g_free);

  return ret;
}

GtkComposeTable *
gtk_compose_table_new_with_file (const char *compose_file)
{
  GtkComposeTable *compose_table;
  gboolean found_old_cache = FALSE;
  gboolean found_include = FALSE;

  g_assert (compose_file != NULL);

  compose_table = gtk_compose_table_load_cache (compose_file, &found_old_cache);
  if (compose_table != NULL)
    return compose_table;

parse:
  compose_table = gtk_compose_table_parse (compose_file, &found_include);

  /* This is where we apply heuristics to avoid breaking users existing configurations
   * with the change to not always add the default sequences.
   *
   * If we find a cache that was generated before 4.4, and the Compose file
   * does not have an include, and doesn't contain so many sequences that it
   * is probably a copy of the system one, we take steps to keep things working,
   * and thell the user about it.
   */
  if (found_old_cache && !found_include &&
      compose_table != NULL && compose_table->n_sequences < 100)
    {
      if (rewrite_compose_file (compose_file))
        {
          g_warning ("\nSince GTK 4.4, Compose files replace the builtin\n"
                     "compose sequences. To keep them and add your own\n"
                     "sequences on top, the line:\n"
                     "\n"
                     "  include \"%%L\"\n"
                     "\n"
                     "has been added to the Compose file\n%s.\n", compose_file);
          goto parse;
        }
      else
        {
          g_warning ("\nSince GTK 4.4, Compose files replace the builtin\n"
                     "compose sequences. To keep them and add your own\n"
                     "sequences on top, you need to add the line:\n"
                     "\n"
                     "  include \"%%L\"\n"
                     "\n"
                     "to the Compose file\n%s.\n", compose_file);
        }
    }

  if (compose_table != NULL)
    gtk_compose_table_save_cache (compose_table);

  return compose_table;
}

GtkComposeTable *
gtk_compose_table_new_with_data (const guint16 *data,
                                 int            max_seq_len,
                                 int            n_seqs)
{
  GtkComposeParser *parser;
  GtkComposeTable *compose_table;
  int i;

  parser = parser_new ();

  for (i = 0; i < n_seqs; i++)
    {
      const guint16 *seq = data + i * (max_seq_len + 2);
      guint16 *sequence;
      gunichar ch;
      char buf[8] = { 0, };

      sequence = g_new0 (guint16, max_seq_len + 1);
      memcpy (sequence, seq, sizeof (guint16) * max_seq_len);

      ch = ((gunichar)seq[max_seq_len]) << 16 | (gunichar)seq[max_seq_len + 1];
      g_unichar_to_utf8 (ch, buf);

      g_hash_table_replace (parser->sequences, sequence, g_strdup (buf));
    }

  compose_table = parser_get_compose_table (parser);
  parser_free (parser);

  return compose_table;
}

static int
compare_seq (const void *key, const void *value)
{
  int i = 0;
  const guint *keysyms = key;
  const guint16 *seq = value;

  while (keysyms[i])
    {
      if (keysyms[i] < seq[i])
        return -1;
      else if (keysyms[i] > seq[i])
        return 1;

      i++;
    }

  return 0;
}

static int
compare_seq_index (const void *key, const void *value)
{
  const guint *keysyms = key;
  const guint16 *seq = value;

  if (keysyms[0] < seq[0])
    return -1;
  else if (keysyms[0] > seq[0])
    return 1;

  return 0;
}

/*
 * gtk_compose_table_check:
 * @table: the table to check
 * @compose_buffer: the key vals to match
 * @n_compose: number of non-zero key vals in @compose_buffer
 * @compose_finish: (out): return location for whether there may be longer matches
 * @compose_match: (out): return location for whether there is a match
 * @output: (out) (caller-allocates): return location for the match values
 *
 * Looks for matches for a key sequence in @table.
 *
 * Returns: %TRUE if there were any matches, %FALSE otherwise
 */
gboolean
gtk_compose_table_check (const GtkComposeTable *table,
                         const guint           *compose_buffer,
                         int                    n_compose,
                         gboolean              *compose_finish,
                         gboolean              *compose_match,
                         GString               *output)
{
  int len;
  guint16 *seq_index;
  guint16 *seq;
  int i;
  gboolean match;
  gunichar value;

  if (compose_finish)
    *compose_finish = FALSE;
  if (compose_match)
    *compose_match = FALSE;

  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val.
   */
  if (n_compose > table->max_seq_len)
    return FALSE;

  seq_index = bsearch (compose_buffer,
                       table->data,
                       table->n_index_size,
                       sizeof (guint16) * (table->max_seq_len + 2),
                       compare_seq_index);

  if (!seq_index)
    return FALSE;

  match = FALSE;

  if (n_compose == 1)
    {
      if (seq_index[2] - seq_index[1] > 0)
        {
          seq = table->data + seq_index[1];

          value = seq[0];

          if ((value & (1 << 15)) != 0)
            g_string_append (output, &table->char_data[value & ~(1 << 15)]);
          else
            g_string_append_unichar (output, value);

          if (compose_match)
            *compose_match = TRUE;
        }

      return TRUE;
    }

  for (i = n_compose - 1; i < table->max_seq_len; i++)
    {
      len = i + 1;

      if (seq_index[len + 1] - seq_index[len] > 0)
        {
          seq = bsearch (compose_buffer + 1,
                         table->data + seq_index[len],
                         (seq_index[len + 1] - seq_index[len]) / len,
                         sizeof (guint16) * len,
                         compare_seq);

          if (seq)
            {
              if (i == n_compose - 1)
                {
                  value = seq[len - 1];

                  if ((value & (1 << 15)) != 0)
                    g_string_append (output, &table->char_data[value & ~(1 << 15)]);
                  else
                    g_string_append_unichar (output, value);
                  match = TRUE;
                }
              else
                {
                  if (match)
                    {
                      if (compose_match)
                        *compose_match = TRUE;
                    }

                  return TRUE;
                }
            }
        }
    }

  if (match)
    {
      if (compose_match)
        *compose_match = TRUE;
      if (compose_finish)
        *compose_finish = TRUE;

      return TRUE;
    }

  return FALSE;
}

void
gtk_compose_table_get_prefix (const GtkComposeTable *table,
                              const guint           *compose_buffer,
                              int                    n_compose,
                              int                   *prefix)
{
  int index_stride = table->max_seq_len + 2;
  int p = 0;

  for (int idx = 0; idx < table->n_index_size; idx++)
    {
      const guint16 *seq_index = table->data + (idx * index_stride);

      if (seq_index[0] == compose_buffer[0])
        {
          p = 1;

          for (int i = 1; i < table->max_seq_len + 1; i++)
            {
              int len = i;

              for (int j = seq_index[i]; j < seq_index[i + 1]; j += len)
                {
                  int k;

                  for (k = 0; k < MIN (len, n_compose) - 1; k++)
                    {
                      if (compose_buffer[k + 1] != table->data[j + k])
                        break;
                    }
                  p = MAX (p, k + 1);
                }
            }

          break;
        }
    }

  *prefix = p;
}

void
gtk_compose_table_foreach (const GtkComposeTable      *table,
                           GtkComposeSequenceCallback  callback,
                           gpointer                    data)
{
  int index_stride = table->max_seq_len + 2;
  gunichar *sequence;

  sequence = g_newa (gunichar, table->max_seq_len + 1);

  for (int idx = 0; idx < table->n_index_size; idx++)
    {
      const guint16 *seq_index = table->data + (idx * index_stride);

      for (int i = 1; i <= table->max_seq_len; i++)
        {
          int len = i;

          g_assert (seq_index[i] <= seq_index[i + 1]);
          g_assert (seq_index[i + 1] <= table->data_size);
          g_assert ((seq_index[i + 1] - seq_index[i]) % len == 0);

          for (int j = seq_index[i]; j < seq_index[i + 1]; j += len)
            {
              char buf[8] = { 0, };
              guint16 encoded_value;
              char *value;

              sequence[0] = seq_index[0];
              for (int k = 1; k < len; k++)
                sequence[k] = (gunichar) table->data[j + k - 1];
              sequence[len] = 0;

              encoded_value = table->data[j + len - 1];
              g_assert (encoded_value != 0);
              if ((encoded_value & (1 << 15)) != 0)
                {
                  int char_offset = encoded_value & ~(1 << 15);
                  g_assert (char_offset < table->n_chars);
                  value = &table->char_data[char_offset];
                }
              else
                {
                  g_unichar_to_utf8 ((gunichar)encoded_value, buf);
                  value = buf;
                }

              callback (sequence, len, value, data);
            }
        }
    }
}

/* Checks if a keysym is a dead key.
 * Dead key keysym values are defined in ../gdk/gdkkeysyms.h and the
 * first is GDK_KEY_dead_grave. As X.Org is updated, more dead keys
 * are added and we need to update the upper limit.
 */
#define IS_DEAD_KEY(k) \
    ((k) >= GDK_KEY_dead_grave && (k) <= GDK_KEY_dead_greek)

gboolean
gtk_check_algorithmically (const guint *compose_buffer,
                           int          n_compose,
                           GString     *output)

{
  int i;

  g_string_set_size (output, 0);

  for (i = 0; i < n_compose && IS_DEAD_KEY (compose_buffer[i]); i++)
    ;

  /* Can't combine if there's no base character: incomplete sequence */
  if (i == n_compose)
    return TRUE;

  if (i > 0 && i == n_compose - 1)
    {
      GString *input;
      char *nfc;
      gunichar ch;

      ch = gdk_keyval_to_unicode (compose_buffer[i]);

      /* We don't allow combining with non-letters */
      if (!g_unichar_isalpha (ch))
        return FALSE;

      input = g_string_sized_new (4 * n_compose);

      g_string_append_unichar (input, ch);

      i--;
      while (i >= 0)
        {
          switch (compose_buffer[i])
            {
#define CASE(keysym, unicode) \
            case GDK_KEY_dead_##keysym: g_string_append_unichar (input, unicode); break

            CASE (grave, 0x0300);
            CASE (acute, 0x0301);
            CASE (circumflex, 0x0302);
            case GDK_KEY_dead_tilde:
              if (g_unichar_get_script (ch) == G_UNICODE_SCRIPT_GREEK)
                g_string_append_unichar (input, 0x342); /* combining perispomeni */
              else
                g_string_append_unichar (input, 0x303); /* combining tilde */
              break;
            CASE (macron, 0x0304);
            CASE (breve, 0x0306);
            CASE (abovedot, 0x0307);
            CASE (diaeresis, 0x0308);
            CASE (abovering, 0x30A);
            CASE (hook, 0x0309);
            CASE (doubleacute, 0x030B);
            CASE (caron, 0x030C);
            CASE (cedilla, 0x0327);
            CASE (ogonek, 0x0328);      /* Legacy use for dasia, 0x314.*/
            CASE (iota, 0x0345);
            CASE (voiced_sound, 0x3099);        /* Per Markus Kuhn keysyms.txt file. */
            CASE (semivoiced_sound, 0x309A);    /* Per Markus Kuhn keysyms.txt file. */
            CASE (belowdot, 0x0323);
            CASE (horn, 0x031B);        /* Legacy use for psili, 0x313 (or 0x343). */
            CASE (stroke, 0x335);
            CASE (abovecomma, 0x0313);  /* Equivalent to psili */
            CASE (abovereversedcomma, 0x0314);   /* Equivalent to dasia */
            CASE (doublegrave, 0x30F);
            CASE (belowring, 0x325);
            CASE (belowmacron, 0x331);
            CASE (belowcircumflex, 0x32D);
            CASE (belowtilde, 0x330);
            CASE (belowbreve, 0x32e);
            CASE (belowdiaeresis, 0x324);
            CASE (invertedbreve, 0x32f);
            CASE (belowcomma, 0x326);
            CASE (lowline, 0x332);
            CASE (aboveverticalline, 0x30D);
            CASE (belowverticalline, 0x329);
            CASE (longsolidusoverlay, 0x338);
            CASE (a, 0x363);
            CASE (A, 0x363);
            CASE (e, 0x364);
            CASE (E, 0x364);
            CASE (i, 0x365);
            CASE (I, 0x365);
            CASE (o, 0x366);
            CASE (O, 0x366);
            CASE (u, 0x367);
            CASE (U, 0x367);
            CASE (small_schwa, 0x1DEA);
            CASE (capital_schwa, 0x1DEA);
#undef CASE
            default:
              g_string_append_unichar (input, gdk_keyval_to_unicode (compose_buffer[i]));
            }
          i--;
        }

      nfc = g_utf8_normalize (input->str, input->len, G_NORMALIZE_NFC);

      g_string_assign (output, nfc);

      g_free (nfc);

      g_string_free (input, TRUE);

      return TRUE;
    }

  return FALSE;
}
