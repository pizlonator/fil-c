/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += print_numbered_lines.o

#include "libbb.h"

int FAST_FUNC print_numbered_lines(struct number_state *ns, const char *filename)
{
	FILE *fp = fopen_or_warn_stdin(filename);
	unsigned N;
	char *line;

	if (!fp)
		return EXIT_FAILURE;

	N = ns->start;
	while ((line = xmalloc_fgetline(fp)) != NULL) {
		if (ns->all
		 || (ns->nonempty && line[0])
		) {
			printf("%*u%s", ns->width, N, ns->sep);
			N += ns->inc;
		} else if (ns->empty_str)
			fputs_stdout(ns->empty_str);
		puts(line);
		free(line);
	}
	ns->start = N;

	fclose(fp);

	return EXIT_SUCCESS;
}
