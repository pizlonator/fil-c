#include <locale.h>
#include <gtk/gtk.h>
#include "cc-datetime-resources.h"
#include "cc-timezone-map.h"

static void
test_timezone (void)
{
  g_autoptr(GHashTable) ht = NULL;
  CcTimezoneMap *map;
  TzDB *tz_db;
  guint i;

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  map = cc_timezone_map_new ();
  tz_db = tz_load_db ();

  g_assert_nonnull (tz_db);
  g_assert_nonnull (tz_db->locations);

  for (i = 0; tz_db->locations && i < tz_db->locations->len; i++)
    {
      g_autofree gchar *clean_tz = NULL;
      TzLocation *location = NULL;

      location = g_ptr_array_index (tz_db->locations, i);
      clean_tz = tz_info_get_clean_name (tz_db, location->zone);

      if (!cc_timezone_map_set_timezone (map, location->zone))
        {
          if (!g_hash_table_contains (ht, clean_tz))
            {
              if (g_strcmp0 (clean_tz, location->zone) == 0)
                g_printerr ("Failed to locate timezone '%s'\n", location->zone);
              else
                g_printerr ("Failed to locate timezone '%s' (original name: '%s')\n", clean_tz, location->zone);
              g_hash_table_insert (ht, g_strdup (clean_tz), GINT_TO_POINTER (TRUE));
            }

          /* We don't warn for those, we'll just fallback
           * in the panel code */
          if (!g_str_equal (clean_tz, "posixrules") && !g_str_equal (clean_tz, "Factory"))
            g_test_fail ();
        }
    }

  tz_db_free (tz_db);
}

gint
main (gint    argc,
      gchar **argv)
{
  setlocale (LC_ALL, "");
  gtk_init (NULL, NULL);
  g_test_init (&argc, &argv, NULL);

  g_resources_register (cc_datetime_get_resource ());

  g_setenv ("G_DEBUG", "fatal_warnings", FALSE);

  g_test_add_func ("/datetime/timezone", test_timezone);

  return g_test_run ();
}
