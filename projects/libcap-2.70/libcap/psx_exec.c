#include <stdio.h>
#include "execable.h"

SO_MAIN(int argc, char **argv)
{
    const char *cmd = "This library";
    if (argv != NULL && argv[0] != NULL) {
	cmd = argv[0];
    }
    printf("%s is the shared library version: " LIBRARY_VERSION ".\n"
	   "See the License file for distribution information.\n"
	   "More information on this library is available from:\n"
	   "\n"
	   "    https://sites.google.com/site/fullycapable/\n", cmd);
}
