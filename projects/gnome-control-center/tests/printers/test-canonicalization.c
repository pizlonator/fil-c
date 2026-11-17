#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "pp-utils.h"

static void
test_canonicalization (gconstpointer data)
{
  const char *contents = data;
  guint   i, j;
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

      if (*lines[i] == '#')
        continue;

      if (*lines[i] == '\0')
        break;

      items = g_strsplit (lines[i], "\t", -1);
      if (g_strv_length (items) == 6)
        {
          PpPrintDevice  *device;
          gchar         **already_present_printers;
          gchar          *canonicalized_name;
          GList          *devices = NULL;

          already_present_printers = g_strsplit (items[0], " ", -1);

          for (j = 0; already_present_printers[j] != NULL; j++)
            devices = g_list_append (devices, g_strdup (already_present_printers[j]));

          device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                 "device-id", items[1],
                                 "device-make-and-model", items[2],
                                 "device-original-name", items[3],
                                 "device-info", items[4],
                                 NULL);

          canonicalized_name =
            canonicalize_device_name (devices, NULL, NULL, 0, device);

          if (g_strcmp0 (canonicalized_name, items[5]) != 0)
            {
              g_error ("Result for ('%s', '%s', '%s', '%s') doesn't match '%s' (got: '%s')",
                        items[1], items[2], items[3], items[4], items[5], canonicalized_name);
              g_test_fail ();
            }
          else
            {
              g_debug ("Result for ('%s', '%s', '%s', '%s') matches '%s'",
                       items[1], items[2], items[3], items[4], canonicalized_name);
            }

          g_free (canonicalized_name);
          g_object_unref (device);
          g_list_free_full (devices, (GDestroyNotify) g_free);
          g_strfreev (already_present_printers);
        }
      else
        {
          g_warning ("Line number %u has not correct number of items!", i);
          g_test_fail ();
        }

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

  if (g_file_get_contents (TEST_SRCDIR "/canonicalization-test.txt", &contents, NULL, NULL) == FALSE)
    {
      g_warning ("Failed to load '%s'", TEST_SRCDIR "/canonicalization-test.txt");
      return 1;
    }

  g_test_add_data_func ("/printers/canonicalization", contents, test_canonicalization);

  return g_test_run ();
}
