/*
 * Test program to trigger various ext4 ioctl's
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#if (!defined(EXT4_IOC_ALLOC_DA_BLKS) && defined(__linux__))
#define EXT4_IOC_ALLOC_DA_BLKS		_IO('f', 12)
#endif

#if (!defined(EXT4_IOC_SWAP_BOOT) && defined(__linux__))
#define EXT4_IOC_SWAP_BOOT		_IO('f', 17)
#endif

#if (!defined(EXT4_IOC_PRECACHE_EXTENTS) && defined(__linux__))
#define EXT4_IOC_PRECACHE_EXTENTS	_IO('f', 18)
#endif

#if (!defined(EXT4_IOC_CLEAR_ES_CACHE) && defined(__linux__))
#define EXT4_IOC_CLEAR_ES_CACHE		_IO('f', 40)
#endif


#define EXT4_F_RW	0x0001

struct cmd {
	const char	*cmd;
	unsigned long	ioc;
	int		flags;
};

struct cmd cmds[] = {
	{ "alloc_da_blks", EXT4_IOC_ALLOC_DA_BLKS, EXT4_F_RW },
	{ "precache", EXT4_IOC_PRECACHE_EXTENTS, 0 },
	{ "swap_boot", EXT4_IOC_SWAP_BOOT, EXT4_F_RW },
	{ "clear_es_cache", EXT4_IOC_CLEAR_ES_CACHE, EXT4_F_RW },
	{ NULL, 0 }
};

const char *progname;

void usage()
{
	struct cmd *p;

	fprintf(stderr, "Usage: %s <cmd> <file>\n\n", progname);
	fprintf(stderr, "Available commands:\n");
	for (p = cmds; p->cmd; p++) {
		fprintf(stderr, "\t%s\n", p->cmd);
	}
	exit(1);
}

int do_single_cmd(const char *fn, struct cmd *p)
{
	int	fd;
	int	oflags = O_RDONLY;

	if (p->flags & EXT4_F_RW)
		oflags = O_RDWR;
	fd = open(fn, oflags, 0);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	if (ioctl(fd, p->ioc) < 0) {
		perror("ioctl");
		return 1;
	}
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	int	i, fails = 0;
	struct cmd *p;

	progname = argv[0];
	if (argc < 3 || strcmp(argv[1], "help") == 0)
		usage();
	for (p = cmds; p->cmd; p++) {
		if (strcmp(argv[1], p->cmd) == 0)
			break;
	}
	if (p->cmd == NULL) {
		fprintf(stderr, "Invalid command: %s\n", argv[1]);
		usage();
	}
	for (i = 2; i < argc; i++)
		fails += do_single_cmd(argv[i], p);
	return fails;
}
