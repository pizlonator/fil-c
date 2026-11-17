#include <locale.h>
#include <info-cleanup.h>

int main (int argc, char **argv)
{
	g_autofree char *str = NULL;

	setlocale (LC_ALL, "");

	if (argc != 2) {
		g_print ("Usage: %s DEVICE-NAME\n", argv[0]);
		return 1;
	}

	str = info_cleanup (argv[1]);
	g_print ("%s ➯ %s\n", argv[1], str);
	return 0;
}
