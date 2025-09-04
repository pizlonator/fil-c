/*
 * blockdev implementation for busybox
 *
 * Copyright (C) 2010 Sergey Naumov <sknaumov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config BLOCKDEV
//config:	bool "blockdev (2.6 kb)"
//config:	default y
//config:	help
//config:	Performs some ioctls with block devices.

//applet:IF_BLOCKDEV(APPLET_NOEXEC(blockdev, blockdev, BB_DIR_SBIN, BB_SUID_DROP, blockdev))

//kbuild:lib-$(CONFIG_BLOCKDEV) += blockdev.o

//usage:#define blockdev_trivial_usage
//usage:	"OPTION BLOCKDEV"
//usage:#define blockdev_full_usage "\n\n"
//usage:       "	--setro		Set ro"
//usage:     "\n	--setrw		Set rw"
//usage:     "\n	--getro		Get ro"
//usage:     "\n	--getss		Get sector size"
//usage:     "\n	--getbsz	Get block size"
//usage:     "\n	--setbsz BYTES	Set block size"
//usage:     "\n	--getsz		Get device size in 512-byte sectors"
///////:     "\n	--getsize	Get device size in sectors (deprecated)"
///////^^^^^ supported, but not shown in help ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//usage:     "\n	--getsize64	Get device size in bytes"
//usage:     "\n	--getra		Get readahead in 512-byte sectors"
//usage:     "\n	--setra SECTORS	Set readahead"
//usage:     "\n	--flushbufs	Flush buffers"
//usage:     "\n	--rereadpt	Reread partition table"
// util-linux 2.31 also has:
//	--getdiscardzeroes	BLKDISCARDZEROES	Get discard zeroes support status
//	--getpbsz		BLKPBSZGET	Get physical block (sector) size
//	--getiomin		BLKIOMIN	Get minimum I/O size
//	--getioopt		BLKIOOPT	Get optimal I/O size
//	--getalignoff		BLKALIGNOFF	Get alignment offset in bytes
//	--getmaxsect		BLKSECTGET	Get max sectors per request
//	--setra SECTORS		BLKRASET	Set readahead
//	--getra			BLKRAGET	Get readahead
//	--setfra SECTORS	BLKFRASET	Set filesystem readahead
//	--getfra		BLKFRAGET	Get filesystem readahead

#include "libbb.h"
#include <linux/fs.h>

/* Takes less space is separate arrays than one array of struct */
static const char bdcmd_names[] ALIGN1 =
	"setro"     "\0"
#define CMD_SETRO 0
	"setrw"     "\0"
	"getro"     "\0"
	"getss"     "\0"
	"getbsz"    "\0"
	"setbsz"    "\0"
#define CMD_SETBSZ 5
	"getsz"     "\0"
	"getsize"   "\0"
	"getsize64" "\0"
	"getra"     "\0"
	"setra"     "\0"
#define CMD_SETRA 10
	"flushbufs" "\0"
	"rereadpt"  "\0"
;
static const uint32_t bdcmd_ioctl[] ALIGN4 = {
	BLKROSET,       //setro
	BLKROSET,       //setrw
	BLKROGET,       //getro
	BLKSSZGET,      //getss
	BLKBSZGET,      //getbsz
	BLKBSZSET,      //setbsz
	BLKGETSIZE64,   //getsz
	BLKGETSIZE,     //getsize
	BLKGETSIZE64,   //getsize64
	BLKRAGET,       //getra
	BLKRASET,       //setra
	BLKFLSBUF,      //flushbufs
	BLKRRPART,      //rereadpt
};
enum {
	ARG_NONE   = 0,
	ARG_INT    = 1,
	ARG_ULONG  = 2,
	/* Yes, BLKGETSIZE64 takes pointer to uint64_t, not ullong! */
	ARG_U64    = 3,
	ARG_MASK   = 3,

	FL_USRARG   = 4, /* argument is provided by user */
	FL_NORESULT = 8,
	FL_SCALE512 = 16,
};
static const uint8_t bdcmd_flags[] ALIGN1 = {
	ARG_INT + FL_NORESULT,             //setro
	ARG_INT + FL_NORESULT,             //setrw
	ARG_INT,                           //getro
	ARG_INT,                           //getss
	ARG_INT,                           //getbsz
	ARG_INT + FL_NORESULT + FL_USRARG, //setbsz
	ARG_U64 + FL_SCALE512,             //getsz
	ARG_ULONG,                         //getsize
	ARG_U64,                           //getsize64
	ARG_ULONG,                         //getra
	ARG_ULONG + FL_NORESULT,           //setra
	ARG_NONE + FL_NORESULT,            //flushbufs
	ARG_NONE + FL_NORESULT,            //rereadpt
};

static unsigned find_cmd(const char *s)
{
	if (s[0] == '-' && s[1] == '-') {
		int n = index_in_strings(bdcmd_names, s + 2);
		if (n >= 0)
			return n;
	}
	bb_show_usage();
}

int blockdev_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int blockdev_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned bdcmd;
	unsigned flags;
	int fd;
	uint64_t u64;
	union {
		int i;
		unsigned long lu;
		uint64_t u64;
	} ioctl_val_on_stack;

	argv++;
	if (!argv[0] || !argv[1]) /* must have at least 2 args */
		bb_show_usage();

	bdcmd = find_cmd(*argv);
	/* setrw translates to BLKROSET(0), most other ioctls don't care... */
	/* ...setro will do BLKROSET(1) */
	u64 = (bdcmd == CMD_SETRO);
	if (bdcmd == CMD_SETBSZ || bdcmd == CMD_SETRA) {
		/* ...setbsz is BLKBSZSET(bytes) */
		/* ...setra is BLKRASET(512 bytes) */
		u64 = xatoi_positive(*++argv);
	}

	argv++;
	if (!argv[0] || argv[1])
		bb_show_usage();
	fd = xopen(argv[0], O_RDONLY);

	ioctl_val_on_stack.u64 = u64;
	flags = bdcmd_flags[bdcmd];
#if BB_BIG_ENDIAN
	/* Store data properly wrt data size.
	 * (1) It's no-op for little-endian.
	 * (2) it's no-op for 0 and -1.
	 * --setro uses arg != 0 and != -1, and it is ARG_INT
	 * --setbsz USER_VAL is also ARG_INT
	 * --setra USER_VAL is ARG_ULONG, but it is passed by value,
	 * not reference (see below in ioctl call).
	 * Thus, we don't need to handle ARG_ULONG.
	 */
	switch (flags & ARG_MASK) {
	case ARG_INT:
		ioctl_val_on_stack.i = (int)u64;
		break;
# if 0 /* unused */
	case ARG_ULONG:
		ioctl_val_on_stack.lu = (unsigned long)u64;
		break;
# endif
	}
#endif

	if (ioctl(fd, bdcmd_ioctl[bdcmd],
			bdcmd == CMD_SETRA
				? (void*)(uintptr_t)u64 /* BLKRASET passes _value_, not pointer to it */
				: &ioctl_val_on_stack.u64
		) == -1)
		bb_simple_perror_msg_and_die(*argv);

	/* Fetch it into register(s) */
	u64 = ioctl_val_on_stack.u64;

	if (flags & FL_SCALE512)
		u64 >>= 9;

	/* Zero- or one-extend the value if needed, then print */
	switch (flags & (ARG_MASK+FL_NORESULT)) {
	case ARG_INT:
		/* Smaller code when we use long long
		 * (gcc tail-merges printf call)
		 */
		printf("%lld\n", (long long)(int)u64);
		break;
	case ARG_ULONG:
		u64 = (unsigned long)u64;
		/* FALLTHROUGH */
	case ARG_U64:
		printf("%llu\n", (unsigned long long)u64);
		break;
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	return EXIT_SUCCESS;
}
