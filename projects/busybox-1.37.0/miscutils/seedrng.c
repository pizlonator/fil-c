// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2022 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * SeedRNG is a simple program made for seeding the Linux kernel random number
 * generator from seed files. It is is useful in light of the fact that the
 * Linux kernel RNG cannot be initialized from shell scripts, and new seeds
 * cannot be safely generated from boot time shell scripts either. It should
 * be run once at init time and once at shutdown time. It can be run at other
 * times on a timer as well. Whenever it is run, it writes existing seed files
 * into the RNG pool, and then creates a new seed file. If the RNG is
 * initialized at the time of creating a new seed file, then that new seed file
 * is marked as "creditable", which means it can be used to initialize the RNG.
 * Otherwise, it is marked as "non-creditable", in which case it is still used
 * to seed the RNG's pool, but will not initialize the RNG. In order to ensure
 * that entropy only ever stays the same or increases from one seed file to the
 * next, old seed values are hashed together with new seed values when writing
 * new seed files.
 *
 * This is based on code from <https://git.zx2c4.com/seedrng/about/>.
 */
//config:config SEEDRNG
//config:	bool "seedrng (9.1 kb)"
//config:	default y
//config:	help
//config:	Seed the kernel RNG from seed files, meant to be called
//config:	once during startup, once during shutdown, and optionally
//config:	at some periodic interval in between.

//applet:IF_SEEDRNG(APPLET(seedrng, BB_DIR_USR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SEEDRNG) += seedrng.o

//usage:#define seedrng_trivial_usage
//usage:	"[-d DIR] [-n]"
//usage:#define seedrng_full_usage "\n\n"
//usage:	"Seed the kernel RNG from seed files"
//usage:	"\n"
//usage:	"\n	-d DIR	Use seed files in DIR (default: /var/lib/seedrng)"
//usage:	"\n	-n	Do not credit randomness, even if creditable"

#include "libbb.h"

#include <linux/random.h>
#include <sys/file.h>

/* Fix up glibc <= 2.24 not having getrandom() */
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ <= 24
#include <sys/syscall.h>
static ssize_t getrandom(void *buffer, size_t length, unsigned flags)
{
# if defined(__NR_getrandom)
	return syscall(__NR_getrandom, buffer, length, flags);
# else
	errno = ENOSYS;
	return -1;
# endif
}
#else
#include <sys/random.h>
#endif

/* Apparently some headers don't ship with this yet. */
#ifndef GRND_NONBLOCK
#define GRND_NONBLOCK 0x0001
#endif

#ifndef GRND_INSECURE
#define GRND_INSECURE 0x0004
#endif

#define DEFAULT_SEED_DIR         "/var/lib/seedrng"
#define CREDITABLE_SEED_NAME     "seed.credit"
#define NON_CREDITABLE_SEED_NAME "seed.no-credit"

enum {
	MIN_SEED_LEN = SHA256_OUTSIZE,
	/* kernels < 5.18 could return short reads from getrandom()
	 * if signal is pending and length is > 256.
	 * Let's limit our reads to 256 bytes.
	 */
	MAX_SEED_LEN = 256,
};

static size_t determine_optimal_seed_len(void)
{
	char poolsize_str[12];
	unsigned poolsize;
	int n;

	n = open_read_close("/proc/sys/kernel/random/poolsize", poolsize_str, sizeof(poolsize_str) - 1);
	if (n < 0) {
		bb_perror_msg("can't determine pool size, assuming %u bits", MIN_SEED_LEN * 8);
		return MIN_SEED_LEN;
	}
	poolsize_str[n] = '\0';
	poolsize = (bb_strtou(poolsize_str, NULL, 10) + 7) / 8;
	return MAX(MIN(poolsize, MAX_SEED_LEN), MIN_SEED_LEN);
}

static bool read_new_seed(uint8_t *seed, size_t len)
{
	bool is_creditable;
	ssize_t ret;

	ret = getrandom(seed, len, GRND_NONBLOCK);
	if (ret == (ssize_t)len) {
		return true;
	}
	if (ret < 0 && errno == ENOSYS) {
		int fd = xopen("/dev/random", O_RDONLY);
		struct pollfd random_fd;
		random_fd.fd = fd;
		random_fd.events = POLLIN;
		is_creditable = poll(&random_fd, 1, 0) == 1;
//This is racy. is_creditable can be set to true here, but other process
//can consume "good" random data from /dev/urandom before we do it below.
		close(fd);
	} else {
		if (getrandom(seed, len, GRND_INSECURE) == (ssize_t)len)
			return false;
		is_creditable = false;
	}

	/* Either getrandom() is not implemented, or
	 * getrandom(GRND_INSECURE) did not give us LEN bytes.
	 * Fallback to reading /dev/urandom.
	 */
	errno = 0;
	if (open_read_close("/dev/urandom", seed, len) != (ssize_t)len)
		bb_perror_msg_and_die("can't read '%s'", "/dev/urandom");
	return is_creditable;
}

static void seed_from_file_if_exists(const char *filename, int dfd, bool credit, sha256_ctx_t *hash)
{
	struct {
		int entropy_count;
		int buf_size;
		uint8_t buf[MAX_SEED_LEN];
	} req;
	ssize_t seed_len;

	seed_len = open_read_close(filename, req.buf, sizeof(req.buf));
	if (seed_len < 0) {
		if (errno != ENOENT)
			bb_perror_msg_and_die("can't read '%s'", filename);
		return;
	}
	xunlink(filename);
	if (seed_len != 0) {
		int fd;

		/* We are going to use this data to seed the RNG:
		 * we believe it to genuinely containing entropy.
		 * If this just-unlinked file survives
		 * (if machine crashes before deletion is recorded on disk)
		 * and we reuse it after reboot, this assumption
		 * would be violated, and RNG may end up generating
		 * the same data. fsync the directory
		 * to make sure file is gone:
		 */
		if (fsync(dfd) != 0)
			bb_simple_perror_msg_and_die("I/O error");

//Length is not random, and taking its address spills variable to stack
//		sha256_hash(hash, &seed_len, sizeof(seed_len));
		sha256_hash(hash, req.buf, seed_len);

		req.buf_size = seed_len;
		seed_len *= 8;
		req.entropy_count = credit ? seed_len : 0;
		printf("Seeding %u bits %s crediting\n",
				(unsigned)seed_len, credit ? "and" : "without");
		fd = xopen("/dev/urandom", O_RDONLY);
		xioctl(fd, RNDADDENTROPY, &req);
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
	}
}

int seedrng_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int seedrng_main(int argc UNUSED_PARAM, char **argv)
{
	const char *seed_dir;
	int fd, dfd;
	int i;
	unsigned opts;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable;
	struct timespec timestamp[2];
	sha256_ctx_t hash;

	enum {
		OPT_n = (1 << 0), /* must be 1 */
		OPT_d = (1 << 1),
	};
#if ENABLE_LONG_OPTS
	static const char longopts[] ALIGN1 =
		"skip-credit\0" No_argument       "n"
		"seed-dir\0"    Required_argument "d"
		;
#endif

	seed_dir = DEFAULT_SEED_DIR;
	opts = getopt32long(argv, "nd:", longopts, &seed_dir);
	umask(0077);
	if (getuid() != 0)
		bb_simple_error_msg_and_die(bb_msg_you_must_be_root);

	if (mkdir(seed_dir, 0700) < 0 && errno != EEXIST)
		bb_perror_msg_and_die("can't create directory '%s'", seed_dir);
	dfd = xopen(seed_dir, O_DIRECTORY | O_RDONLY);
	xfchdir(dfd);
	/* Concurrent runs of this tool might feed the same data to RNG twice.
	 * Avoid concurrent runs by taking a blocking lock on the directory.
	 * Not checking for errors. Looking at manpage,
	 * ENOLCK "The kernel ran out of memory for allocating lock records"
	 * seems to be the only one which is possible - and if that happens,
	 * machine is OOMing (much worse problem than inability to lock...).
	 * Also, typically configured Linux machines do not fail GFP_KERNEL
	 * allocations (they trigger memory reclaim instead).
	 */
	flock(dfd, LOCK_EX); /* blocks while another instance runs */

	sha256_begin(&hash);
//Hashing in a constant string doesn't add any entropy
//	sha256_hash(&hash, "SeedRNG v1 Old+New Prefix", 25);
	clock_gettime(CLOCK_REALTIME, &timestamp[0]);
	clock_gettime(CLOCK_BOOTTIME, &timestamp[1]);
	sha256_hash(&hash, timestamp, sizeof(timestamp));

	for (i = 0; i <= 1; i++) {
		seed_from_file_if_exists(
			i == 0 ? NON_CREDITABLE_SEED_NAME : CREDITABLE_SEED_NAME,
			dfd,
			/*credit?*/ (opts ^ OPT_n) & i, /* 0, then 1 unless -n */
			&hash);
	}

	new_seed_len = determine_optimal_seed_len();
	new_seed_creditable = read_new_seed(new_seed, new_seed_len);
//Length is not random, and taking its address spills variable to stack
//	sha256_hash(&hash, &new_seed_len, sizeof(new_seed_len));
	sha256_hash(&hash, new_seed, new_seed_len);
	sha256_end(&hash, new_seed + new_seed_len - SHA256_OUTSIZE);

	printf("Saving %u bits of %screditable seed for next boot\n",
		(unsigned)new_seed_len * 8, new_seed_creditable ? "" : "non-");
	fd = xopen3(NON_CREDITABLE_SEED_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	xwrite(fd, new_seed, new_seed_len);
	if (new_seed_creditable) {
		/* More paranoia when we create a file which we believe contains
		 * genuine entropy: make sure disk is not full, quota isn't exceeded, etc:
		 */
		if (fsync(fd) < 0)
			bb_perror_msg_and_die("can't write '%s'", NON_CREDITABLE_SEED_NAME);
		xrename(NON_CREDITABLE_SEED_NAME, CREDITABLE_SEED_NAME);
	}
	return EXIT_SUCCESS;
}
