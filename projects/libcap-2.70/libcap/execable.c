#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>

#include "execable.h"

static void usage(int status)
{
    printf("\nusage: libcap.so [--help|--usage|--summary]\n");
    exit(status);
}

static void summary(void)
{
    cap_value_t bits = cap_max_bits(), c;
    cap_mode_t mode = cap_get_mode();

    printf("\nCurrent mode: %s\n", cap_mode_name(mode));
    printf("Number of cap values known to: this libcap=%d, running kernel=%d\n",
	   CAP_LAST_CAP+1, bits);

    if (bits > CAP_LAST_CAP+1) {
	printf("=> Consider upgrading libcap to name:");
	for (c = CAP_LAST_CAP+1; c < bits; c++) {
	    printf(" %d", c);
	}
    } else if (bits < CAP_LAST_CAP+1) {
	printf("=> Newer kernels also provide support for:");
	for (c = bits; c <= CAP_LAST_CAP; c++) {
	    char *name = cap_to_name(c);
	    printf(" %s", name);
	    cap_free(name);
	}
    } else {
	return;
    }
    printf("\n");
}

SO_MAIN(int argc, char **argv)
{
    int i;
    const char *cmd = "This library";

    if (argv != NULL && argv[0] != NULL) {
	cmd = argv[0];
    }
    printf("%s is the shared library version: " LIBRARY_VERSION ".\n"
	   "See the License file for distribution information.\n"
	   "More information on this library is available from:\n"
	   "\n"
	   "    https://sites.google.com/site/fullycapable/\n", cmd);

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "--usage") || !strcmp(argv[i], "--help")) {
	    usage(0);
	}
	if (!strcmp(argv[i], "--summary")) {
	    summary();
	    continue;
	}
	usage(1);
    }
}
