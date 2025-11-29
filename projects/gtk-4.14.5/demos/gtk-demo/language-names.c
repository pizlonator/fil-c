#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <hb-ot.h>

#include "language-names.h"

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else

#ifndef ISO_CODES_PREFIX
#define ISO_CODES_PREFIX "/usr"
#endif

#define ISO_CODES_DATADIR ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"
#endif

static GHashTable *language_map;

#ifdef G_OS_WIN32
/* if we are using native Windows use native Windows API for language names */
static BOOL CALLBACK
get_win32_all_locales_scripts (LPWSTR locale_w, DWORD flags, LPARAM param)
{
  wchar_t *langname_w = NULL;
  wchar_t locale_abbrev_w[9];
  gchar *langname, *locale_abbrev, *locale;
  gint i;
  const LCTYPE iso639_lctypes[] = { LOCALE_SISO639LANGNAME, LOCALE_SISO639LANGNAME2 };
  GHashTable *ht_scripts_langs = (GHashTable *) param;
  PangoLanguage *lang;

  gint langname_size, locale_abbrev_size;
  langname_size = GetLocaleInfoEx (locale_w, LOCALE_SLOCALIZEDDISPLAYNAME, langname_w, 0);
  if (langname_size == 0)
    return FALSE;

  langname_w = g_new0 (wchar_t, langname_size);

  if (langname_size == 0)
    return FALSE;

  GetLocaleInfoEx (locale_w, LOCALE_SLOCALIZEDDISPLAYNAME, langname_w, langname_size);
  langname = g_utf16_to_utf8 (langname_w, -1, NULL, NULL, NULL);
  locale = g_utf16_to_utf8 (locale_w, -1, NULL, NULL, NULL);
  lang = pango_language_from_string (locale);
  if (g_hash_table_lookup (ht_scripts_langs, lang) == NULL)
    g_hash_table_insert (ht_scripts_langs, lang, langname);

  /*
   * Track 3+-letter ISO639-2/3 language codes as well (these have a max length of 9 including terminating NUL)
   * ISO639-2: iso639_lctypes[0] = LOCALE_SISO639LANGNAME
   * ISO639-3: iso639_lctypes[1] = LOCALE_SISO639LANGNAME2
   */
  for (i = 0; i < 2; i++)
    {
      locale_abbrev_size = GetLocaleInfoEx (locale_w, iso639_lctypes[i], locale_abbrev_w, 0);
      if (locale_abbrev_size > 0)
        {
          GetLocaleInfoEx (locale_w, iso639_lctypes[i], locale_abbrev_w, locale_abbrev_size);

          locale_abbrev = g_utf16_to_utf8 (locale_abbrev_w, -1, NULL, NULL, NULL);
          lang = pango_language_from_string (locale_abbrev);
          if (g_hash_table_lookup (ht_scripts_langs, lang) == NULL)
            g_hash_table_insert (ht_scripts_langs, lang, langname);

          g_free (locale_abbrev);
        }
    }

  g_free (locale);
  g_free (langname_w);

  return TRUE;
}

#else /* non-Windows */

static char *
get_first_item_in_semicolon_list (const char *list)
{
  char **items;
  char  *item;

  items = g_strsplit (list, "; ", 2);

  item = g_strdup (items[0]);
  g_strfreev (items);

  return item;
}

static char *
capitalize_utf8_string (const char *str)
{
  char first[8] = { 0 };

  if (!str)
    return NULL;

  g_unichar_to_utf8 (g_unichar_totitle (g_utf8_get_char (str)), first);

  return g_strconcat (first, g_utf8_offset_to_pointer (str, 1), NULL);
}

static char *
get_display_name (const char *language)
{
  const char  *translated;
  char *tmp;
  char *name;

  translated = dgettext ("iso_639", language);

  tmp = get_first_item_in_semicolon_list (translated);
  name = capitalize_utf8_string (tmp);
  g_free (tmp);

  return name;
}

static void
languages_parse_start_tag (GMarkupParseContext  *ctx,
                           const char           *element_name,
                           const char          **attr_names,
                           const char          **attr_values,
                           gpointer              user_data,
                           GError              **error)
{
  const char *ccode_longB;
  const char *ccode_longT;
  const char *ccode;
  const char *ccode_id;
  const char *lang_name;
  char *display_name;

  if (!(g_str_equal (element_name, "iso_639_entry") ||
        g_str_equal (element_name, "iso_639_3_entry")) ||
        attr_names == NULL ||
        attr_values == NULL)
    return;

  ccode = NULL;
  ccode_longB = NULL;
  ccode_longT = NULL;
  ccode_id = NULL;
  lang_name = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "iso_639_1_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2)
                return;
              ccode = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2B_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                return;
              ccode_longB = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2T_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                return;
              ccode_longT = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "id"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2 &&
                  strlen (*attr_values) != 3)
                return;
              ccode_id = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "name"))
        {
          lang_name = *attr_values;
        }

      ++attr_names;
      ++attr_values;
    }

  if (lang_name == NULL)
    return;

  display_name = get_display_name (lang_name);

  if (ccode != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode),
                         g_strdup (display_name));

  if (ccode_longB != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_longB),
                         g_strdup (display_name));

  if (ccode_longT != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_longT),
                         g_strdup (display_name));

  if (ccode_id != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_id),
                         g_strdup (display_name));

  g_free (display_name);
}

static void
languages_variant_init (const char *variant)
{
  gboolean res;
  gsize    buf_len;
  char *buf = NULL;
  char *filename = NULL;
  GError *error = NULL;

  bindtextdomain (variant, ISO_CODES_LOCALESDIR);
  bind_textdomain_codeset (variant, "UTF-8");

  error = NULL;
  filename = g_strconcat (ISO_CODES_DATADIR, "/", variant, ".xml", NULL);
  res = g_file_get_contents (filename, &buf, &buf_len, &error);
  if (res)
    {
      GMarkupParseContext *ctx = NULL;
      GMarkupParser parser = { languages_parse_start_tag, NULL, NULL, NULL, NULL };

      ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

      g_free (error);
      error = NULL;
      res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);
      g_free (ctx);

      if (!res)
        g_warning ("Failed to parse '%s': %s\n", filename, error->message);
    }
  else
    g_warning ("Failed to load '%s': %s\n", filename, error->message);

  g_clear_error (&error);
  g_free (filename);
  g_free (buf);
}
#endif

static void
languages_init (void)
{
  if (language_map)
    return;

  language_map = g_hash_table_new_full (NULL, NULL, NULL, g_free);

#ifdef G_OS_WIN32
  g_return_if_fail (EnumSystemLocalesEx (&get_win32_all_locales_scripts, LOCALE_ALL, (LPARAM) language_map, NULL));
#else
  languages_variant_init ("iso_639");
  languages_variant_init ("iso_639_3");
#endif
}

const char *
get_language_name (PangoLanguage *language)
{
  languages_init ();

  return (const char *) g_hash_table_lookup (language_map, language);
}

const char *
get_language_name_for_tag (guint32 tag)
{
  hb_language_t lang;
  const char *s;

  lang = hb_ot_tag_to_language (tag);
  s = hb_language_to_string (lang);

  if (strcmp (s, "und-fonipa") == 0)
    return "International Phonetic Alphabet";
  else if (strcmp (s, "und-fonnapa") == 0)
    return "North-American Phonetic Alphabet";
  else if (strcmp (s, "ro-md") == 0)
    return "Moldavian";

  return get_language_name (pango_language_from_string (s));
}
