#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixoutputstream.h>
#include <fcntl.h>
#endif


/* This is the guts of gtk_text_buffer_insert_markup,
 * copied here so we can make an incremental version.
 */
static void
insert_tags_for_attributes (GtkTextBuffer     *buffer,
                            PangoAttrIterator *iter,
                            GtkTextIter       *start,
                            GtkTextIter       *end)
{
  GtkTextTagTable *table;
  GSList *attrs, *l;
  GtkTextTag *tag;
  char name[256];
  float fg_alpha, bg_alpha;

  table = gtk_text_buffer_get_tag_table (buffer);

#define LANGUAGE_ATTR(attr_name) \
    { \
      const char *language = pango_language_to_string (((PangoAttrLanguage*)attr)->value); \
      g_snprintf (name, 256, "language=%s", language); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, language, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define STRING_ATTR(attr_name) \
    { \
      const char *string = ((PangoAttrString*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%s", string); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, string, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define INT_ATTR(attr_name) \
    { \
      int value = ((PangoAttrInt*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%d", value); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, value, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define FONT_ATTR(attr_name) \
    { \
      PangoFontDescription *desc = ((PangoAttrFontDesc*)attr)->desc; \
      char *str = pango_font_description_to_string (desc); \
      g_snprintf (name, 256, "font-desc=%s", str); \
      g_free (str); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, desc, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define FLOAT_ATTR(attr_name) \
    { \
      float value = ((PangoAttrFloat*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%g", value); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, value, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define RGBA_ATTR(attr_name, alpha_value) \
    { \
      PangoColor *color; \
      GdkRGBA rgba; \
      color = &((PangoAttrColor*)attr)->color; \
      rgba.red = color->red / 65535.; \
      rgba.green = color->green / 65535.; \
      rgba.blue = color->blue / 65535.; \
      rgba.alpha = alpha_value; \
      char *str = gdk_rgba_to_string (&rgba); \
      g_snprintf (name, 256, #attr_name "=%s", str); \
      g_free (str); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, &rgba, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define VOID_ATTR(attr_name) \
    { \
      tag = gtk_text_tag_table_lookup (table, #attr_name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (#attr_name); \
          g_object_set (tag, #attr_name, TRUE, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

  fg_alpha = bg_alpha = 1.;

  attrs = pango_attr_iterator_get_attrs (iter);
  for (l = attrs; l; l = l->next)
    {
      PangoAttribute *attr = l->data;

      switch ((int)attr->klass->type)
        {
        case PANGO_ATTR_FOREGROUND_ALPHA:
          fg_alpha = ((PangoAttrInt*)attr)->value / 65535.;
          break;

        case PANGO_ATTR_BACKGROUND_ALPHA:
          bg_alpha = ((PangoAttrInt*)attr)->value / 65535.;
          break;

        default:
          break;
        }
    }

  for (l = attrs; l; l = l->next)
    {
      PangoAttribute *attr = l->data;

      switch (attr->klass->type)
        {
        case PANGO_ATTR_LANGUAGE:
          LANGUAGE_ATTR (language);
          break;

        case PANGO_ATTR_FAMILY:
          STRING_ATTR (family);
          break;

        case PANGO_ATTR_STYLE:
          INT_ATTR (style);
          break;

        case PANGO_ATTR_WEIGHT:
          INT_ATTR (weight);
          break;

        case PANGO_ATTR_VARIANT:
          INT_ATTR (variant);
          break;

        case PANGO_ATTR_STRETCH:
          INT_ATTR (stretch);
          break;

        case PANGO_ATTR_SIZE:
          INT_ATTR (size);
          break;

        case PANGO_ATTR_FONT_DESC:
          FONT_ATTR (font-desc);
          break;

        case PANGO_ATTR_FOREGROUND:
          RGBA_ATTR (foreground_rgba, fg_alpha);
          break;

        case PANGO_ATTR_BACKGROUND:
          RGBA_ATTR (background_rgba, bg_alpha);
          break;

        case PANGO_ATTR_UNDERLINE:
          INT_ATTR (underline);
          break;

        case PANGO_ATTR_UNDERLINE_COLOR:
          RGBA_ATTR (underline_rgba, fg_alpha);
          break;

        case PANGO_ATTR_OVERLINE:
          INT_ATTR (overline);
          break;

        case PANGO_ATTR_OVERLINE_COLOR:
          RGBA_ATTR (overline_rgba, fg_alpha);
          break;

        case PANGO_ATTR_STRIKETHROUGH:
          INT_ATTR (strikethrough);
          break;

        case PANGO_ATTR_STRIKETHROUGH_COLOR:
          RGBA_ATTR (strikethrough_rgba, fg_alpha);
          break;

        case PANGO_ATTR_RISE:
          INT_ATTR (rise);
          break;

        case PANGO_ATTR_SCALE:
          FLOAT_ATTR (scale);
          break;

        case PANGO_ATTR_FALLBACK:
          INT_ATTR (fallback);
          break;

        case PANGO_ATTR_LETTER_SPACING:
          INT_ATTR (letter_spacing);
          break;

        case PANGO_ATTR_FONT_FEATURES:
          STRING_ATTR (font_features);
          break;

        case PANGO_ATTR_ALLOW_BREAKS:
          INT_ATTR (allow_breaks);
          break;

        case PANGO_ATTR_SHOW:
          INT_ATTR (show_spaces);
          break;

        case PANGO_ATTR_INSERT_HYPHENS:
          INT_ATTR (insert_hyphens);
          break;

        case PANGO_ATTR_LINE_HEIGHT:
          FLOAT_ATTR (line_height);
          break;

        case PANGO_ATTR_ABSOLUTE_LINE_HEIGHT:
          break;

        case PANGO_ATTR_WORD:
          VOID_ATTR (word);
          break;

        case PANGO_ATTR_SENTENCE:
          VOID_ATTR (sentence);
          break;

        case PANGO_ATTR_BASELINE_SHIFT:
          INT_ATTR (baseline_shift);
          break;

        case PANGO_ATTR_FONT_SCALE:
          INT_ATTR (font_scale);
          break;

        case PANGO_ATTR_SHAPE:
        case PANGO_ATTR_ABSOLUTE_SIZE:
        case PANGO_ATTR_GRAVITY:
        case PANGO_ATTR_GRAVITY_HINT:
        case PANGO_ATTR_FOREGROUND_ALPHA:
        case PANGO_ATTR_BACKGROUND_ALPHA:
          break;

        case PANGO_ATTR_TEXT_TRANSFORM:
          INT_ATTR (text_transform);
          break;

        case PANGO_ATTR_INVALID:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  g_slist_free_full (attrs, (GDestroyNotify)pango_attribute_destroy);

#undef LANGUAGE_ATTR
#undef STRING_ATTR
#undef INT_ATTR
#undef FONT_ATTR
#undef FLOAT_ATTR
#undef RGBA_ATTR
}

typedef struct
{
  GMarkupParseContext *parser;
  char *markup;
  gsize pos;
  gsize len;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  PangoAttrList *attributes;
  char *text;
  PangoAttrIterator *attr;
} MarkupData;

static void
free_markup_data (MarkupData *mdata)
{
  g_free (mdata->markup);
  g_clear_pointer (&mdata->parser, g_markup_parse_context_free);
  gtk_text_buffer_delete_mark (mdata->buffer, mdata->mark);
  g_clear_pointer (&mdata->attr, pango_attr_iterator_destroy);
  g_clear_pointer (&mdata->attributes, pango_attr_list_unref);
  g_free (mdata->text);
  g_object_unref (mdata->buffer);
  g_free (mdata);
}

static gboolean
insert_markup_idle (gpointer data)
{
  MarkupData *mdata = data;
  gint64 begin;

  begin = g_get_monotonic_time ();

  do
    {
      int start, end;
      int start_offset;
      GtkTextIter start_iter;

      if (g_get_monotonic_time () - begin > G_TIME_SPAN_MILLISECOND)
        {
          g_idle_add (insert_markup_idle, data);
          return G_SOURCE_REMOVE;
        }

      pango_attr_iterator_range (mdata->attr, &start, &end);

      if (end == G_MAXINT) /* last chunk */
        end = start - 1; /* resulting in -1 to be passed to _insert */

      start_offset = gtk_text_iter_get_offset (&mdata->iter);
      gtk_text_buffer_insert (mdata->buffer, &mdata->iter, mdata->text + start, end - start);
      gtk_text_buffer_get_iter_at_offset (mdata->buffer, &start_iter, start_offset);

      insert_tags_for_attributes (mdata->buffer, mdata->attr, &start_iter, &mdata->iter);

      gtk_text_buffer_get_iter_at_mark (mdata->buffer, &mdata->iter, mdata->mark);
    }
  while (pango_attr_iterator_next (mdata->attr));

  free_markup_data (mdata);
  return G_SOURCE_REMOVE;
}

static gboolean
parse_markup_idle (gpointer data)
{
  MarkupData *mdata = data;
  gint64 begin;
  GError *error = NULL;

  begin = g_get_monotonic_time ();

  do {
    if (g_get_monotonic_time () - begin > G_TIME_SPAN_MILLISECOND)
      {
        g_idle_add (parse_markup_idle, data);
        return G_SOURCE_REMOVE;
      }

    if (!g_markup_parse_context_parse (mdata->parser,
                                       mdata->markup + mdata->pos,
                                       MIN (4096, mdata->len - mdata->pos),
                                       &error))
      {
        g_warning ("Invalid markup string: %s", error->message);
        g_error_free (error);
        free_markup_data (mdata);
        return G_SOURCE_REMOVE;
      }

    mdata->pos += 4096;
  } while (mdata->pos < mdata->len);

  if (!pango_markup_parser_finish (mdata->parser,
                                   &mdata->attributes,
                                   &mdata->text,
                                   NULL,
                                   &error))
    {
      g_warning ("Invalid markup string: %s", error->message);
      g_error_free (error);
      free_markup_data (mdata);
      return G_SOURCE_REMOVE;
    }

  if (!mdata->attributes)
    {
      gtk_text_buffer_insert (mdata->buffer, &mdata->iter, mdata->text, -1);
      free_markup_data (mdata);
      return G_SOURCE_REMOVE;
    }

  mdata->attr = pango_attr_list_get_iterator (mdata->attributes);
  insert_markup_idle (data);

  return G_SOURCE_REMOVE;
}

/* Takes a ref on @buffer while it is operating,
 * and consumes @markup.
 */
static void
insert_markup (GtkTextBuffer *buffer,
               GtkTextIter   *iter,
               char          *markup,
               int            len)
{
  MarkupData *data;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  data = g_new0 (MarkupData, 1);

  data->buffer = g_object_ref (buffer);
  data->iter = *iter;
  data->markup = markup;
  data->len = len;

  data->parser = pango_markup_parser_new (0);
  data->pos = 0;

  /* create mark with right gravity */
  data->mark = gtk_text_buffer_create_mark (buffer, NULL, iter, FALSE);

  parse_markup_idle (data);
}

static void
fontify_finish (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  GtkTextBuffer *buffer = data;
  GBytes *stdout_buf = NULL;
  GBytes *stderr_buf = NULL;
  GError *error = NULL;

  if (!g_subprocess_communicate_finish (subprocess,
                                        result,
                                        &stdout_buf,
                                        &stderr_buf,
                                        &error))
    {
      g_clear_pointer (&stdout_buf, g_bytes_unref);
      g_clear_pointer (&stderr_buf, g_bytes_unref);

      g_warning ("%s", error->message);
      g_clear_error (&error);

      g_object_unref (subprocess);
      g_object_unref (buffer);
      return;
    }

  if (g_subprocess_get_exit_status (subprocess) != 0)
    {
      if (stderr_buf)
        g_warning ("%s", (char *)g_bytes_get_data (stderr_buf, NULL));
      g_clear_pointer (&stderr_buf, g_bytes_unref);
    }

  g_object_unref (subprocess);

  g_clear_pointer (&stderr_buf, g_bytes_unref);

  if (stdout_buf)
    {
      char *markup;
      gsize len;
      char *p;
      GtkTextIter start;

      gtk_text_buffer_set_text (buffer, "", 0);

      /* highlight puts a span with font and size around its output,
       * which we don't want.
       */
      markup = g_bytes_unref_to_data (stdout_buf, &len);
      for (p = markup + strlen ("<span "); *p != '>'; p++) *p = ' ';

      gtk_text_buffer_get_start_iter (buffer, &start);
      insert_markup (buffer, &start, markup, len);
   }

  g_object_unref (buffer);
}

void
fontify (const char    *format,
         GtkTextBuffer *source_buffer)
{
  GSubprocess *subprocess;
  char *format_arg;
  GtkSettings *settings;
  char *theme;
  gboolean prefer_dark;
  const char *style_arg;
  char *text;
  GtkTextIter start, end;
  GBytes *bytes;
  GError *error = NULL;

  settings = gtk_settings_get_default ();
  g_object_get (settings,
                "gtk-theme-name", &theme,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);

  if (prefer_dark || strcmp (theme, "HighContrastInverse") == 0)
    style_arg = "--style=edit-vim-dark";
  else
    style_arg = "--style=edit-kwrite";

  g_free (theme);

  format_arg = g_strconcat ("--syntax=", format, NULL);
  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                 G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                 G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                 &error,
                                 "highlight",
                                 format_arg,
                                 "--out-format=pango",
                                 style_arg,
                                 NULL);
  g_free (format_arg);

  if (!subprocess)
    {
      if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT))
        {
          static gboolean warned = FALSE;

          if (!warned)
            {
              warned = TRUE;
              g_message ("For syntax highlighting, install the “highlight” program");
            }
        }
      else
        g_warning ("%s", error->message);

      g_clear_error (&error);

      return;
    }

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  text = gtk_text_buffer_get_text (source_buffer, &start, &end, TRUE);
  bytes = g_bytes_new_take (text, strlen (text));

#ifdef HAVE_GIO_UNIX
  /* Work around https://gitlab.gnome.org/GNOME/glib/-/issues/2182 */
  if (G_IS_UNIX_OUTPUT_STREAM (g_subprocess_get_stdin_pipe (subprocess)))
    {
      GOutputStream *stdin_pipe = g_subprocess_get_stdin_pipe (subprocess);
      int fd = g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (stdin_pipe));
      fcntl (fd, F_SETFL, O_NONBLOCK);
    }
#endif

  g_subprocess_communicate_async (subprocess,
                                  bytes,
                                  NULL,
                                  fontify_finish,
                                  g_object_ref (source_buffer));
  g_bytes_unref (bytes);
}
