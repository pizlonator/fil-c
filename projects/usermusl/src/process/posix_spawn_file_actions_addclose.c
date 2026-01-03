#include <spawn.h>
#include <stdlib.h>
#include <errno.h>
#include "fdop.h"
#include <pizlonated_syscalls.h>
#include <sys/resource.h>

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd)
{
	struct rlimit nofile_limit;
	ZASSERT(!zsys_getrlimit(RLIMIT_NOFILE, &nofile_limit));
	if (fd < 0 || fd >= nofile_limit.rlim_cur) return EBADF;
	struct fdop *op = malloc(sizeof *op);
	if (!op) return ENOMEM;
	op->cmd = FDOP_CLOSE;
	op->fd = fd;
	if ((op->next = fa->__actions)) op->next->prev = op;
	op->prev = 0;
	fa->__actions = op;
	return 0;
}
