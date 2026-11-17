#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include "date-endian.h"

static int verbose = 0;

static void
print_endianess (const char *lang)
{
	DateEndianess endianess;

	if (lang != NULL) {
		setlocale (LC_TIME, lang);
		endianess = date_endian_get_for_lang (lang, verbose);
	} else {
		endianess = date_endian_get_default (verbose);
	}
	if (verbose)
		g_print ("\t\t%s\n", date_endian_to_string (endianess));
}

static void
test_endianess (void)
{
	g_autoptr(GDir) dir = NULL;
	const char *name;

	dir = g_dir_open ("/usr/share/i18n/locales/", 0, NULL);
	if (dir == NULL) {
		/* Try with /usr/share/locale/
		 * https://bugzilla.gnome.org/show_bug.cgi?id=646780 */
		dir = g_dir_open ("/usr/share/locale/", 0, NULL);
		if (dir == NULL) {
			g_assert_not_reached ();
		}
	}

	while ((name = g_dir_read_name (dir)) != NULL)
		print_endianess (name);
}

int main (int argc, char **argv)
{
	setlocale (LC_ALL, "");
	bind_textdomain_codeset ("libc", "UTF-8");

	g_test_init (&argc, &argv, NULL);

	g_setenv ("G_DEBUG", "fatal_warnings", FALSE);

	if (argv[1] != NULL) {
		verbose = 1;

		if (g_str_equal (argv[1], "-c"))
			print_endianess (NULL);
		else
			print_endianess (argv[1]);
		return 0;
	}

	g_test_add_func ("/datetime/endianess", test_endianess);

	return g_test_run ();
}
