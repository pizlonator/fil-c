#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "hostname-helper.h"

static void
test_hostname (void)
{
	g_autofree gchar *contents = NULL;
	guint i;
	g_auto(GStrv) lines = NULL;

	if (g_file_get_contents (TEST_SRCDIR "/hostnames-test.txt", &contents, NULL, NULL) == FALSE) {
		g_warning ("Failed to load '%s'", TEST_SRCDIR "/hostnames-test.txt");
		g_test_fail ();
		return;
	}

	lines = g_strsplit (contents, "\n", -1);
	if (lines == NULL) {
		g_warning ("Test file is empty");
		g_test_fail ();
		return;
	}

	for (i = 0; lines[i] != NULL; i++) {
		g_auto(GStrv) items = NULL;
		g_autofree gchar *utf8 = NULL;
		g_autofree gchar *result1 = NULL;
		g_autofree gchar *result2 = NULL;

		if (*lines[i] == '#' || *lines[i] == '\0')
			continue;

		items = g_strsplit (lines[i], "\t", -1);
		utf8 = g_locale_from_utf8 (items[0], -1, NULL, NULL, NULL);

		result1 = pretty_hostname_to_static (items[0], FALSE);
		if (g_strcmp0 (result1, items[2]) != 0) {
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 utf8, items[2], result1);
			g_test_fail ();
		} else {
			g_debug ("Result for '%s' matches '%s'",
				 utf8, result1);
		}

		result2 = pretty_hostname_to_static (items[0], TRUE);
		if (g_strcmp0 (result2, items[1]) != 0) {
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 utf8, items[1], result2);
			g_test_fail ();
		} else {
			g_debug ("Result for '%s' matches '%s'",
				 utf8, result2);
		}
	}
}

static void
test_ssid (void)
{
	g_autofree gchar *contents = NULL;
	guint i;
	g_auto(GStrv) lines = NULL;

	if (g_file_get_contents (TEST_SRCDIR "/ssids-test.txt", &contents, NULL, NULL) == FALSE) {
		g_warning ("Failed to load '%s'", TEST_SRCDIR "/ssids-test.txt");
		g_test_fail ();
		return;
	}

	lines = g_strsplit (contents, "\n", -1);
	if (lines == NULL) {
		g_warning ("Test file is empty");
		g_test_fail ();
		return;
	}

	for (i = 0; lines[i] != NULL; i++) {
		g_autofree gchar *ssid = NULL;
		g_auto(GStrv) items = NULL;

		if (*lines[i] == '#')
			continue;
		if (*lines[i] == '\0')
			break;

		items = g_strsplit (lines[i], "\t", -1);
		ssid = pretty_hostname_to_ssid (items[0]);
		g_assert_cmpstr (ssid, ==, items[1]);
	}
}

int main (int argc, char **argv)
{
	char *locale;

	/* Running in some locales will
	 * break the tests as "ü" will be transliterated to
	 * "ue" in de_DE, and 'u"' in the C locale.
	 *
	 * Work around that by forcing en_US with UTF-8 in
	 * our tests
	 * https://bugzilla.gnome.org/show_bug.cgi?id=650342 */
	locale = setlocale (LC_ALL, "en_US.UTF-8");
	if (locale == NULL) {
		g_debug("Missing en_US.UTF-8 locale, ignoring test.");
		return 0;
	}
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/common/hostname", test_hostname);
	g_test_add_func ("/common/ssid", test_ssid);

	return g_test_run ();
}
