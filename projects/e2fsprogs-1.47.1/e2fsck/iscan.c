/*
 * Test to see how quickly we can scan the inode table (not doing
 * anything else)
 */

#include "config.h"
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <unistd.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/ioctl.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <sys/time.h>
#include <sys/resource.h>

#if EXT2_FLAT_INCLUDES
#include "ext2_fs.h"
#include "ext2fs.h"
#include "blkid.h"
#else
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"
#endif

#include "et/com_err.h"
#include "../version.h"

struct resource_track {
	struct timeval time_start;
	struct timeval user_start;
	struct timeval system_start;
	void	*brk_start;
	unsigned long long bytes_read;
	unsigned long long bytes_written;
};

extern int isatty(int);

const char * program_name = "iscan";
const char * device_name = NULL;

int yflag = 0;
int nflag = 0;
int preen = 0;
int inode_buffer_blocks = 0;
int invalid_bitmaps = 0;

struct resource_track	global_rtrack;

void init_resource_track(struct resource_track *track, io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
	io_stats io_start = 0;

	track->brk_start = sbrk(0);
	gettimeofday(&track->time_start, 0);
#ifdef HAVE_GETRUSAGE
#ifdef sun
	memset(&r, 0, sizeof(struct rusage));
#endif
	getrusage(RUSAGE_SELF, &r);
	track->user_start = r.ru_utime;
	track->system_start = r.ru_stime;
#else
	track->user_start.tv_sec = track->user_start.tv_usec = 0;
	track->system_start.tv_sec = track->system_start.tv_usec = 0;
#endif
	track->bytes_read = 0;
	track->bytes_written = 0;
	if (channel && channel->manager && channel->manager->get_stats)
		channel->manager->get_stats(channel, &io_start);
	if (io_start) {
		track->bytes_read = io_start->bytes_read;
		track->bytes_written = io_start->bytes_written;
	}
}

static float timeval_subtract(struct timeval *tv1,
				       struct timeval *tv2)
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

void print_resource_track(const char *desc,
			  struct resource_track *track, io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
	struct timeval time_end;

	gettimeofday(&time_end, 0);

	if (desc)
		printf("%s: ", desc);

#define kbytes(x)	(((unsigned long long)(x) + 1023) / 1024)
#ifdef HAVE_MALLINFO2
	if (1) {
		struct mallinfo2 malloc_info = mallinfo2();

		printf("Memory used: %lluk/%lluk (%lluk/%lluk), ",
		       kbytes(malloc_info.arena), kbytes(malloc_info.hblkhd),
		       kbytes(malloc_info.uordblks),
		       kbytes(malloc_info.fordblks));
	} else
#elif defined HAVE_MALLINFO
	/* don't use mallinfo() if over 2GB used, since it returns "int" */
	if ((unsigned long)((char *)sbrk(0) - (char *)track->brk_start) <
	    2UL << 30) {
		struct mallinfo	malloc_info = mallinfo();

		printf("Memory used: %lluk/%lluk (%lluk/%lluk), ",
		       kbytes(malloc_info.arena), kbytes(malloc_info.hblkhd),
		       kbytes(malloc_info.uordblks),
		       kbytes(malloc_info.fordblks));
	} else
#endif
		printf("Memory used: %lluk, ",
		       kbytes(((char *)sbrk(0)) - ((char *)track->brk_start)));

#ifdef HAVE_GETRUSAGE
	getrusage(RUSAGE_SELF, &r);

	printf("time: %5.2f/%5.2f/%5.2f\n",
	       timeval_subtract(&time_end, &track->time_start),
	       timeval_subtract(&r.ru_utime, &track->user_start),
	       timeval_subtract(&r.ru_stime, &track->system_start));
#else
	printf("elapsed time: %6.3f\n",
	       timeval_subtract(&time_end, &track->time_start));
#endif
#define mbytes(x)	(((x) + 1048575) / 1048576)
	if (channel && channel->manager && channel->manager->get_stats) {
		io_stats delta = 0;
		unsigned long long bytes_read = 0;
		unsigned long long bytes_written = 0;

		if (desc)
			printf("%s: ", desc);

		channel->manager->get_stats(channel, &delta);
		if (delta) {
			bytes_read = delta->bytes_read - track->bytes_read;
			bytes_written = delta->bytes_written -
				track->bytes_written;
		}
		printf("I/O read: %lluMB, write: %lluMB, "
		       "rate: %.2fMB/s\n",
		       mbytes(bytes_read), mbytes(bytes_written),
		       (double)mbytes(bytes_read + bytes_written) /
		       timeval_subtract(&time_end, &track->time_start));
	}
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: %s [-F] [-I inode_buffer_blocks] device\n",
		program_name);
	exit(1);
}

static void PRS(int argc, char *argv[])
{
	int		flush = 0;
	int		c;
#ifdef MTRACE
	extern void	*mallwatch;
#endif
	errcode_t	retval;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	initialize_ext2_error_table();

	if (argc && *argv)
		program_name = *argv;
	while ((c = getopt (argc, argv, "FI")) != EOF)
		switch (c) {
		case 'F':
			flush = 1;
			break;
		case 'I':
			inode_buffer_blocks = atoi(optarg);
			break;
		default:
			usage ();
		}
	device_name = argv[optind];
	if (flush) {
		int	fd = open(device_name, O_RDONLY, 0);

		if (fd < 0) {
			com_err("open", errno,
			    "while opening %s for flushing", device_name);
			exit(1);
		}
		if ((retval = ext2fs_sync_device(fd, 1))) {
			com_err("ext2fs_sync_device", retval,
				"while trying to flush %s", device_name);
			exit(1);
		}
		close(fd);
	}
}

int main (int argc, char *argv[])
{
	errcode_t	retval = 0;
	int		exit_value = 0;
	ext2_filsys	fs;
	ext2_ino_t	ino;
	__u32	num_inodes = 0;
	struct ext2_inode inode;
	ext2_inode_scan	scan;

	PRS(argc, argv);

	retval = ext2fs_open(device_name, 0,
			     0, 0, unix_io_manager, &fs);
	if (retval) {
		com_err(program_name, retval, "while trying to open '%s'",
			device_name);
		exit(1);
	}

	init_resource_track(&global_rtrack, fs->io);

	retval = ext2fs_open_inode_scan(fs, inode_buffer_blocks, &scan);
	if (retval) {
		com_err(program_name, retval, "while opening inode scan");
		exit(1);
	}

	while (1) {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval) {
			com_err(program_name, retval,
				"while getting next inode");
			exit(1);
		}
		if (ino == 0)
			break;
		num_inodes++;
	}

	print_resource_track(NULL, &global_rtrack, fs->io);
	printf("%u inodes scanned.\n", num_inodes);

	exit(0);
}
