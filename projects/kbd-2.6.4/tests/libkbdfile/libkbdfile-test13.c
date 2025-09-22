#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <kbdfile.h>
#include "libcommon.h"

int
main(int __attribute__((unused)) argc, char **argv)
{
	set_progname(argv[0]);

	struct kbdfile *fp = kbdfile_new(NULL);
	if (!fp)
		kbd_error(EXIT_FAILURE, 0, "unable to create kbdfile");

	const char *const dirpath[]  = { "", DATADIR "/data/findfile/test_1/consolefonts/", 0 };
	const char *const suffixes[] = { "", ".psfu", ".psf", ".cp", ".fnt", 0 };

	const char *expect = DATADIR "/data/findfile/test_1/consolefonts/simple-1.psf.gz";

	int rc = 0;

	rc = kbdfile_find("simple-1.psf.gz", dirpath, suffixes, fp);

	if (rc != 0)
		kbd_error(EXIT_FAILURE, 0, "unable to find file");

	if (strcmp(expect, kbdfile_get_pathname(fp)) != 0)
		kbd_error(EXIT_FAILURE, 0, "unexpected file: %s (expected %s)", kbdfile_get_pathname(fp), expect);

	if (!kbdfile_is_compressed(fp))
		kbd_error(EXIT_FAILURE, 0, "not compressed: %s\n", kbdfile_get_pathname(fp));

	kbdfile_free(fp);

	return EXIT_SUCCESS;
}
