#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "pp-utils.h"

static void
test_shift (gconstpointer data)
{
  const char *contents = data;
  guint   i;
  char   *str;
  char  **lines;

  lines = g_strsplit (contents, "\n", -1);
  if (lines == NULL)
    {
      g_warning ("Test file is empty");
      g_test_fail ();
      return;
    }

  for (i = 0; lines[i] != NULL; i++)
    {
      char **items;
      char  *utf8;

      if (*lines[i] == '#')
        continue;

      if (*lines[i] == '\0')
        break;

      items = g_strsplit (lines[i], "\t", -1);
      str = g_strdup (items[0]);
      shift_string_left (str);
      utf8 = g_locale_from_utf8 (items[0], -1, NULL, NULL, NULL);
      if (g_strcmp0 (str, items[1]) != 0)
        {
          g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
                    utf8, items[1], str);
          g_test_fail ();
        }
      else
        {
          g_debug ("Result for '%s' matches '%s'",
                   utf8, str);
        }

      g_free (str);
      g_free (utf8);

      g_strfreev (items);
    }

  g_strfreev (lines);

}

int
main (int argc, char **argv)
{
  char   *locale;
  char   *contents;

  /* Running in some locales will
   * break the tests as "ü" will be transliterated to
   * "ue" in de_DE, and 'u"' in the C locale.
   *
   * Work around that by forcing en_US with UTF-8 in
   * our tests
   * https://bugzilla.gnome.org/show_bug.cgi?id=650342 */

  locale = setlocale (LC_ALL, "en_US.UTF-8");
  if (locale == NULL)
    {
      g_debug("Missing en_US.UTF-8 locale, ignoring test.");
      return 0;
    }

  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  g_test_init (&argc, &argv, NULL);

  if (g_file_get_contents (TEST_SRCDIR "/shift-test.txt", &contents, NULL, NULL) == FALSE)
    {
      g_warning ("Failed to load '%s'", TEST_SRCDIR "/shift-test.txt");
      return 1;
    }

  g_test_add_data_func ("/printers/shift", contents, test_shift);

  return g_test_run ();
}
