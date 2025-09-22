/*
 * Copyright (c) 2021 Andrew G. Morgan <morgan@kernel.org>
 *
 * The purpose of this file is to provide an executable mode for the
 * pam_cap.so binary. If you run it directly, all it does is print
 * version information.
 *
 * It accepts the optional --help argument which causes the executable
 * to display a summary of all the supported, pam stacked, module
 * arguments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libcap/execable.h"

SO_MAIN(int argc, char **argv)
{
    const char *cmd = "<pam_cap.so>";
    if (argv != NULL) {
	cmd = argv[0];
    }

    printf(
	"%s (version " LIBCAP_VERSION ") is a PAM module to specify\n"
	"inheritable (IAB) capabilities via the libpam authentication\n"
	"abstraction. See the pam_cap License file for licensing information.\n"
	"\n"
	"Release notes and feature documentation for libcap and pam_cap.so\n"
	"can be found at:\n"
	"\n"
	"    https://sites.google.com/site/fullycapable/\n", cmd);
    if (argc <= 1) {
	return;
    }

    if (argc > 2 || argv[1] == NULL || strcmp(argv[1], "--help")) {
	printf("\n%s only supports the optional argument --help\n", cmd);
	exit(1);
    }

    printf("\n"
	   "%s supports the following module arguments:\n"
	   "\n"
	   "debug         - verbose logging (ignored for now)\n"
	   "config=<file> - override the default config with file\n"
	   "keepcaps      - workaround for apps that setuid without this\n"
	   "autoauth      - pam_cap.so to always succeed for the 'auth' phase\n"
	   "default=<iab> - fallback IAB value if there is no '*' rule\n"
	   "defer         - apply IAB value at pam_exit (not via setcred)\n",
	cmd);
}
