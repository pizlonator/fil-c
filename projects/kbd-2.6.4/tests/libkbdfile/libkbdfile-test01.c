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
	kbdfile_free(fp);
	return EXIT_SUCCESS;
}
