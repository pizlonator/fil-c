/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2009 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

char** FAST_FUNC skip_dash_dash(char **argv)
{
	argv++;
	if (argv[0] && argv[0][0] == '-' && argv[0][1] == '-' && argv[0][2] == '\0')
		argv++;
	return argv;
}

char* FAST_FUNC single_argv(char **argv)
{
	argv = skip_dash_dash(argv);
	if (!argv[0] || argv[1])
		bb_show_usage();
	return argv[0];
}
