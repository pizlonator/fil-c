#include <stdio.h>
#include <string.h>

#include "bigcrypt.h"

int
main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s password salt\n",
			strchr(argv[0], '/') ?
			(strchr(argv[0], '/') + 1) :
			argv[0]);
		return 0;
	}
	fprintf(stdout, "%s\n", bigcrypt(argv[1], argv[2]));
	return 0;
}
