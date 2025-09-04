/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += iterate_on_dir.o

#include "libbb.h"

/* Iterate a function on each entry of a directory */
int FAST_FUNC iterate_on_dir(const char *dir_name,
		int FAST_FUNC (*func)(const char *, struct dirent *, void *),
		void *private)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(dir_name);
	if (dir == NULL) {
		return -1;
	}
	while ((de = readdir(dir)) != NULL) {
		func(dir_name, de, private);
	}
	closedir(dir);
	return 0;
}
