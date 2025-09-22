// This program is for testing purposes only.
// usage is "[program] owner [u|g] start count
// Exits 0 if owner has subid range starting start, of size count
// Exits 1 otherwise.

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "atoi/str2i.h"
#include "defines.h"
#include "prototypes.h"
#include "subordinateio.h"
#include "idmapping.h"
#include "shadowlog.h"

static const char Prog[] = "check_subid_range";

int main(int argc, char **argv)
{
	char *owner;
	unsigned long start, count;
	bool check_uids;
	log_set_progname(Prog);
	log_set_logfd(stderr);

	if (argc != 5)
		exit(1);

	owner = argv[1];
	check_uids = argv[2][0] == 'u';
	errno = 0;
	if (str2ul(&start, argv[3]) == -1)
		exit(1);
	if (str2ul(&count, argv[4]) == -1)
		exit(1);
	if (check_uids) {
		if (have_sub_uids(owner, start, count))
			exit(0);
		exit(1);
	}
	if (have_sub_gids(owner, start, count))
		exit(0);
	exit(1);
}
