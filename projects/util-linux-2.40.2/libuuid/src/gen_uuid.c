/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1996, 1997, 1998, 1999 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#define UUID MYUUID
#endif
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif
#if defined(__linux__) && defined(HAVE_SYS_SYSCALL_H)
#include <sys/syscall.h>
#endif
#include <pthread.h>
#include <signal.h>

#include "all-io.h"
#include "uuidP.h"
#include "uuidd.h"
#include "randutils.h"
#include "strutils.h"
#include "c.h"
#include "md5.h"
#include "sha1.h"

#define THREAD_LOCAL static __thread

#ifdef _WIN32
static void gettimeofday (struct timeval *tv, void *dummy)
{
	FILETIME	ftime;
	uint64_t	n;

	GetSystemTimeAsFileTime (&ftime);
	n = (((uint64_t) ftime.dwHighDateTime << 32)
	     + (uint64_t) ftime.dwLowDateTime);
	if (n) {
		n /= 10;
		n -= ((369 * 365 + 89) * (uint64_t) 86400) * 1000000;
	}

	tv->tv_sec = n / 1000000;
	tv->tv_usec = n % 1000000;
}

static int getuid (void)
{
	return 1;
}
#endif

/*
 * Get the ethernet hardware address, if we can find it...
 *
 * XXX for a windows version, probably should use GetAdaptersInfo:
 * http://www.codeguru.com/cpp/i-n/network/networkinformation/article.php/c5451
 * commenting out get_node_id just to get gen_uuid to compile under windows
 * is not the right way to go!
 */
static int get_node_id(unsigned char *node_id)
{
#ifdef HAVE_NET_IF_H
	int		sd;
	struct ifreq	ifr, *ifrp;
	struct ifconf	ifc;
	char buf[1024];
	int		n, i;
	unsigned char	*a = NULL;
#ifdef HAVE_NET_IF_DL_H
	struct sockaddr_dl *sdlp;
#endif

/*
 * BSD 4.4 defines the size of an ifreq to be
 * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
 * However, under earlier systems, sa_len isn't present, so the size is
 * just sizeof(struct ifreq)
 */
#ifdef HAVE_SA_LEN
#define ifreq_size(i) max(sizeof(struct ifreq),\
     sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
#else
#define ifreq_size(i) sizeof(struct ifreq)
#endif /* HAVE_SA_LEN */

	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sd < 0) {
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl (sd, SIOCGIFCONF, (char *)&ifc) < 0) {
		close(sd);
		return -1;
	}
	n = ifc.ifc_len;
	for (i = 0; i < n; i+= ifreq_size(*ifrp) ) {
		ifrp = (struct ifreq *)((char *) ifc.ifc_buf+i);
		strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);
#ifdef SIOCGIFHWADDR
		if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
			continue;
		a = (unsigned char *) &ifr.ifr_hwaddr.sa_data;
#else
#ifdef SIOCGENADDR
		if (ioctl(sd, SIOCGENADDR, &ifr) < 0)
			continue;
		a = (unsigned char *) ifr.ifr_enaddr;
#else
#ifdef HAVE_NET_IF_DL_H
		sdlp = (struct sockaddr_dl *) &ifrp->ifr_addr;
		if ((sdlp->sdl_family != AF_LINK) || (sdlp->sdl_alen != 6))
			continue;
		a = (unsigned char *) &sdlp->sdl_data[sdlp->sdl_nlen];
#else
		/*
		 * XXX we don't have a way of getting the hardware
		 * address
		 */
		close(sd);
		return 0;
#endif /* HAVE_NET_IF_DL_H */
#endif /* SIOCGENADDR */
#endif /* SIOCGIFHWADDR */
		if (a == NULL || (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5]))
			continue;
		if (node_id) {
			memcpy(node_id, a, 6);
			close(sd);
			return 1;
		}
	}
	close(sd);
#endif
	return 0;
}

enum { STATE_FD_ERROR = -1, STATE_FD_INIT = -2 };

static int state_fd_init(const char *clock_file, FILE **fp)
{
	mode_t save_umask;
	int state_fd;
	FILE *state_f;

	save_umask = umask(0);
	state_fd = open(clock_file, O_RDWR|O_CREAT|O_CLOEXEC, 0660);
	(void) umask(save_umask);
	if (state_fd != -1) {
		state_f = fdopen(state_fd, "r+" UL_CLOEXECSTR);
		if (!state_f) {
			close(state_fd);
			state_fd = STATE_FD_ERROR;
		} else
			*fp = state_f;
	}
	return state_fd;
}

/* Assume that the gettimeofday() has microsecond granularity */
#define MAX_ADJUSTMENT 10
/* Reserve a clock_seq value for the 'continuous clock' implementation */
#define CLOCK_SEQ_CONT 0

/*
 * Get clock from global sequence clock counter.
 *
 * Return -1 if the clock counter could not be opened/locked (in this case
 * pseudorandom value is returned in @ret_clock_seq), otherwise return 0.
 */
static int get_clock(uint32_t *clock_high, uint32_t *clock_low,
		     uint16_t *ret_clock_seq, int *num)
{
	THREAD_LOCAL int		adjustment = 0;
	THREAD_LOCAL struct timeval	last = {0, 0};
	THREAD_LOCAL int		state_fd = STATE_FD_INIT;
	THREAD_LOCAL FILE		*state_f;
	THREAD_LOCAL uint16_t		clock_seq;
	struct timeval			tv;
	uint64_t			clock_reg;
	int				ret = 0;

	if (state_fd == STATE_FD_INIT)
		state_fd = state_fd_init(LIBUUID_CLOCK_FILE, &state_f);

	if (state_fd >= 0) {
		rewind(state_f);
		while (flock(state_fd, LOCK_EX) < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			fclose(state_f);
			close(state_fd);
			state_fd = STATE_FD_ERROR;
			ret = -1;
			break;
		}
	} else
		ret = -1;

	if (state_fd >= 0) {
		unsigned int cl;
		unsigned long tv1, tv2;
		int a;

		if (fscanf(state_f, "clock: %04x tv: %lu %lu adj: %d\n",
			   &cl, &tv1, &tv2, &a) == 4) {
			clock_seq = cl & 0x3FFF;
			last.tv_sec = tv1;
			last.tv_usec = tv2;
			adjustment = a;
		}
		// reset in case of reserved CLOCK_SEQ_CONT
		if (clock_seq == CLOCK_SEQ_CONT) {
			last.tv_sec = 0;
			last.tv_usec = 0;
		}
	}

	if ((last.tv_sec == 0) && (last.tv_usec == 0)) {
		do {
			ul_random_get_bytes(&clock_seq, sizeof(clock_seq));
			clock_seq &= 0x3FFF;
		} while (clock_seq == CLOCK_SEQ_CONT);
		gettimeofday(&last, NULL);
		last.tv_sec--;
	}

try_again:
	gettimeofday(&tv, NULL);
	if ((tv.tv_sec < last.tv_sec) ||
	    ((tv.tv_sec == last.tv_sec) &&
	     (tv.tv_usec < last.tv_usec))) {
		do {
			clock_seq = (clock_seq+1) & 0x3FFF;
		} while (clock_seq == CLOCK_SEQ_CONT);
		adjustment = 0;
		last = tv;
	} else if ((tv.tv_sec == last.tv_sec) &&
	    (tv.tv_usec == last.tv_usec)) {
		if (adjustment >= MAX_ADJUSTMENT)
			goto try_again;
		adjustment++;
	} else {
		adjustment = 0;
		last = tv;
	}

	clock_reg = tv.tv_usec*10 + adjustment;
	clock_reg += ((uint64_t) tv.tv_sec)*10000000;
	clock_reg += (((uint64_t) 0x01B21DD2) << 32) + 0x13814000;

	if (num && (*num > 1)) {
		adjustment += *num - 1;
		last.tv_usec += adjustment / 10;
		adjustment = adjustment % 10;
		last.tv_sec += last.tv_usec / 1000000;
		last.tv_usec = last.tv_usec % 1000000;
	}

	if (state_fd >= 0) {
		rewind(state_f);
		fprintf(state_f,
			      "clock: %04x tv: %016ld %08ld adj: %08d                   \n",
			      clock_seq, (long)last.tv_sec, (long)last.tv_usec, adjustment);
		fflush(state_f);
		rewind(state_f);
		flock(state_fd, LOCK_UN);
	}

	*clock_high = clock_reg >> 32;
	*clock_low = clock_reg;
	*ret_clock_seq = clock_seq;
	return ret;
}

/*
 * Get current time in 100ns ticks.
 */
static uint64_t get_clock_counter(void)
{
	struct timeval tv;
	uint64_t clock_reg;

	gettimeofday(&tv, NULL);
	clock_reg = tv.tv_usec*10;
	clock_reg += ((uint64_t) tv.tv_sec) * 10000000ULL;

	return clock_reg;
}

/*
 * Get continuous clock value.
 *
 * Return -1 if there is no valid clock counter available,
 * otherwise return 0.
 *
 * This implementation doesn't deliver clock counters based on
 * the current time because last_clock_reg is only incremented
 * by the number of requested UUIDs.
 * max_clock_offset is used to limit the offset of last_clock_reg.
 * used/reserved UUIDs are written to LIBUUID_CLOCK_CONT_FILE.
 */
static int get_clock_cont(uint32_t *clock_high,
			  uint32_t *clock_low,
			  int num,
			  uint32_t max_clock_offset)
{
	/* all 64bit clock_reg values in this function represent '100ns ticks'
	 * due to the combination of tv_usec + MAX_ADJUSTMENT */

	/* time offset according to RFC 4122. 4.1.4. */
	const uint64_t reg_offset = (((uint64_t) 0x01B21DD2) << 32) + 0x13814000;
	static uint64_t last_clock_reg = 0;
	static uint64_t saved_clock_reg = 0;
	static int state_fd = STATE_FD_INIT;
	static FILE *state_f = NULL;
	uint64_t clock_reg, next_clock_reg;

	if (state_fd == STATE_FD_ERROR)
		return -1;

	clock_reg = get_clock_counter();

	if (state_fd == STATE_FD_INIT) {
		struct stat st;

		state_fd = state_fd_init(LIBUUID_CLOCK_CONT_FILE, &state_f);
		if (state_fd == STATE_FD_ERROR)
			return -1;

		if (fstat(state_fd, &st))
			goto error;

		if (st.st_size) {
			rewind(state_f);
			if (fscanf(state_f, "cont: %"SCNu64"\n", &last_clock_reg) != 1)
				goto error;
		} else
			last_clock_reg = clock_reg;

		saved_clock_reg = last_clock_reg;
	}

	if (max_clock_offset) {
		uint64_t co = 10000000ULL * (uint64_t)max_clock_offset;	// clock_offset in [100ns]

		if ((last_clock_reg + co) < clock_reg)
			last_clock_reg = clock_reg - co;
	}

	clock_reg += MAX_ADJUSTMENT;

	next_clock_reg = last_clock_reg + (uint64_t)num;
	if (next_clock_reg >= clock_reg)
		return -1;

	if (next_clock_reg >= saved_clock_reg) {
		uint64_t cl = next_clock_reg + 100000000ULL;	// 10s interval in [100ns]
		int l;

		rewind(state_f);
		l = fprintf(state_f, "cont: %020"PRIu64"                   \n", cl);
		if (l < 30 || fflush(state_f))
			goto error;
		saved_clock_reg = cl;
	}

	*clock_high = (last_clock_reg + reg_offset) >> 32;
	*clock_low = last_clock_reg + reg_offset;
	last_clock_reg = next_clock_reg;

	return 0;

error:
	if (state_fd >= 0)
		close(state_fd);
	if (state_f)
		fclose(state_f);
	state_fd = STATE_FD_ERROR;
	state_f = NULL;
	return -1;
}

#if defined(HAVE_UUIDD) && defined(HAVE_SYS_UN_H)

/*
 * Try using the uuidd daemon to generate the UUID
 *
 * Returns 0 on success, non-zero on failure.
 */
static int get_uuid_via_daemon(int op, uuid_t out, int *num)
{
	char op_buf[64];
	int op_len;
	int s;
	ssize_t ret;
	int32_t reply_len = 0, expected = 16;
	struct sockaddr_un srv_addr;

	if (sizeof(UUIDD_SOCKET_PATH) > sizeof(srv_addr.sun_path))
		return -1;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;

	srv_addr.sun_family = AF_UNIX;
	xstrncpy(srv_addr.sun_path, UUIDD_SOCKET_PATH, sizeof(srv_addr.sun_path));

	if (connect(s, (const struct sockaddr *) &srv_addr,
		    sizeof(struct sockaddr_un)) < 0)
		goto fail;

	op_buf[0] = op;
	op_len = 1;
	if (op == UUIDD_OP_BULK_TIME_UUID) {
		memcpy(op_buf+1, num, sizeof(*num));
		op_len += sizeof(*num);
		expected += sizeof(*num);
	}

	ret = write(s, op_buf, op_len);
	if (ret < 1)
		goto fail;

	ret = read_all(s, (char *) &reply_len, sizeof(reply_len));
	if (ret < 0)
		goto fail;

	if (reply_len != expected)
		goto fail;

	ret = read_all(s, op_buf, reply_len);

	if (op == UUIDD_OP_BULK_TIME_UUID)
		memcpy(op_buf+16, num, sizeof(int));

	memcpy(out, op_buf, 16);

	close(s);
	return ((ret == expected) ? 0 : -1);

fail:
	close(s);
	return -1;
}

#else /* !defined(HAVE_UUIDD) && defined(HAVE_SYS_UN_H) */
static int get_uuid_via_daemon(int op __attribute__((__unused__)),
				uuid_t out __attribute__((__unused__)),
				int *num __attribute__((__unused__)))
{
	return -1;
}
#endif

static int __uuid_generate_time_internal(uuid_t out, int *num, uint32_t cont_offset)
{
	static unsigned char node_id[6];
	static int has_init = 0;
	struct uuid uu;
	uint32_t	clock_mid;
	int ret;

	if (!has_init) {
		if (get_node_id(node_id) <= 0) {
			ul_random_get_bytes(node_id, 6);
			/*
			 * Set multicast bit, to prevent conflicts
			 * with IEEE 802 addresses obtained from
			 * network cards
			 */
			node_id[0] |= 0x01;
		}
		has_init = 1;
	}
	if (cont_offset) {
		ret = get_clock_cont(&clock_mid, &uu.time_low, *num, cont_offset);
		uu.clock_seq = CLOCK_SEQ_CONT;
		if (ret != 0)	/* fallback to previous implpementation */
			ret = get_clock(&clock_mid, &uu.time_low, &uu.clock_seq, num);
	} else {
		ret = get_clock(&clock_mid, &uu.time_low, &uu.clock_seq, num);
	}
	uu.clock_seq |= 0x8000;
	uu.time_mid = (uint16_t) clock_mid;
	uu.time_hi_and_version = ((clock_mid >> 16) & 0x0FFF) | 0x1000;
	memcpy(uu.node, node_id, 6);
	uuid_pack(&uu, out);
	return ret;
}

int __uuid_generate_time(uuid_t out, int *num)
{
	return __uuid_generate_time_internal(out, num, 0);
}

int __uuid_generate_time_cont(uuid_t out, int *num, uint32_t cont_offset)
{
	return __uuid_generate_time_internal(out, num, cont_offset);
}

#define CS_MIN		(1<<6)
#define CS_MAX		(1<<18)
#define CS_FACTOR	2

/*
 * Generate time-based UUID and store it to @out
 *
 * Tries to guarantee uniqueness of the generated UUIDs by obtaining them from the uuidd daemon,
 * or, if uuidd is not usable, by using the global clock state counter (see get_clock()).
 * If neither of these is possible (e.g. because of insufficient permissions), it generates
 * the UUID anyway, but returns -1. Otherwise, returns 0.
 */

/* thread local cache for uuidd based requests */
THREAD_LOCAL struct {
	int		num;
	int		cache_size;
	int		last_used;
	struct uuid	uu;
	time_t		last_time;
} uuidd_cache = {
	.cache_size = CS_MIN,
};

static void reset_uuidd_cache(void)
{
	memset(&uuidd_cache, 0, sizeof(uuidd_cache));
	uuidd_cache.cache_size = CS_MIN;
}

static int uuid_generate_time_generic(uuid_t out) {
	static volatile sig_atomic_t atfork_registered;
	time_t	now;

	if (!atfork_registered) {
		pthread_atfork(NULL, NULL, reset_uuidd_cache);
		atfork_registered = 1;
	}

	if (uuidd_cache.num > 0) { /* expire cache */
		now = time(NULL);
		if (now > uuidd_cache.last_time+1) {
			uuidd_cache.last_used = uuidd_cache.cache_size - uuidd_cache.num;
			uuidd_cache.num = 0;
		}
	}
	if (uuidd_cache.num <= 0) { /* fill cache */
		/*
		 * num + OP_BULK provides a local cache in each application.
		 * Start with a small cache size to cover short running applications
		 * and adjust the cache size over the runntime.
		 */
		if ((uuidd_cache.last_used == uuidd_cache.cache_size) && (uuidd_cache.cache_size < CS_MAX))
			uuidd_cache.cache_size *= CS_FACTOR;
		else if ((uuidd_cache.last_used < (uuidd_cache.cache_size / CS_FACTOR)) && (uuidd_cache.cache_size > CS_MIN))
			uuidd_cache.cache_size /= CS_FACTOR;

		uuidd_cache.num = uuidd_cache.cache_size;

		if (get_uuid_via_daemon(UUIDD_OP_BULK_TIME_UUID,
					out, &uuidd_cache.num) == 0) {
			uuidd_cache.last_time = time(NULL);
			uuid_unpack(out, &uuidd_cache.uu);
			uuidd_cache.num--;
			return 0;
		}
		/* request to daemon failed, reset cache */
		reset_uuidd_cache();
	}
	if (uuidd_cache.num > 0) { /* serve uuid from cache */
		uuidd_cache.uu.time_low++;
		if (uuidd_cache.uu.time_low == 0) {
			uuidd_cache.uu.time_mid++;
			if (uuidd_cache.uu.time_mid == 0)
				uuidd_cache.uu.time_hi_and_version++;
		}
		uuidd_cache.num--;
		uuid_pack(&uuidd_cache.uu, out);
		if (uuidd_cache.num == 0)
			uuidd_cache.last_used = uuidd_cache.cache_size;
		return 0;
	}

	return __uuid_generate_time(out, NULL);
}

/*
 * Generate time-based UUID and store it to @out.
 *
 * Discards return value from uuid_generate_time_generic()
 */
void uuid_generate_time(uuid_t out)
{
	(void)uuid_generate_time_generic(out);
}


int uuid_generate_time_safe(uuid_t out)
{
	return uuid_generate_time_generic(out);
}


int __uuid_generate_random(uuid_t out, int *num)
{
	uuid_t	buf;
	struct uuid uu;
	int i, n, r = 0;

	if (!num || !*num)
		n = 1;
	else
		n = *num;

	for (i = 0; i < n; i++) {
		if (ul_random_get_bytes(buf, sizeof(buf)))
			r = -1;
		uuid_unpack(buf, &uu);

		uu.clock_seq = (uu.clock_seq & 0x3FFF) | 0x8000;
		uu.time_hi_and_version = (uu.time_hi_and_version & 0x0FFF)
			| 0x4000;
		uuid_pack(&uu, out);
		out += sizeof(uuid_t);
	}

	return r;
}

void uuid_generate_random(uuid_t out)
{
	int	num = 1;
	/* No real reason to use the daemon for random uuid's -- yet */

	__uuid_generate_random(out, &num);
}

/*
 * This is the generic front-end to __uuid_generate_random and
 * uuid_generate_time.  It uses __uuid_generate_random output
 * only if high-quality randomness is available.
 */
void uuid_generate(uuid_t out)
{
	int num = 1;

	if (__uuid_generate_random(out, &num))
		uuid_generate_time(out);
}

/*
 * Generate an MD5 hashed (predictable) UUID based on a well-known UUID
 * providing the namespace and an arbitrary binary string.
 */
void uuid_generate_md5(uuid_t out, const uuid_t ns, const char *name, size_t len)
{
	UL_MD5_CTX ctx;
	char hash[UL_MD5LENGTH];
	uuid_t buf;
	struct uuid uu;

	ul_MD5Init(&ctx);
	ul_MD5Update(&ctx, ns, sizeof(uuid_t));
	ul_MD5Update(&ctx, (const unsigned char *)name, len);
	ul_MD5Final((unsigned char *)hash, &ctx);

	assert(sizeof(buf) <= sizeof(hash));

	memcpy(buf, hash, sizeof(buf));
	uuid_unpack(buf, &uu);

	uu.clock_seq = (uu.clock_seq & 0x3FFF) | 0x8000;
	uu.time_hi_and_version = (uu.time_hi_and_version & 0x0FFF) | 0x3000;
	uuid_pack(&uu, out);
}

/*
 * Generate a SHA1 hashed (predictable) UUID based on a well-known UUID
 * providing the namespace and an arbitrary binary string.
 */
void uuid_generate_sha1(uuid_t out, const uuid_t ns, const char *name, size_t len)
{
	UL_SHA1_CTX ctx;
	char hash[UL_SHA1LENGTH];
	uuid_t buf;
	struct uuid uu;

	ul_SHA1Init(&ctx);
	ul_SHA1Update(&ctx, ns, sizeof(uuid_t));
	ul_SHA1Update(&ctx, (const unsigned char *)name, len);
	ul_SHA1Final((unsigned char *)hash, &ctx);

	assert(sizeof(buf) <= sizeof(hash));

	memcpy(buf, hash, sizeof(buf));
	uuid_unpack(buf, &uu);

	uu.clock_seq = (uu.clock_seq & 0x3FFF) | 0x8000;
	uu.time_hi_and_version = (uu.time_hi_and_version & 0x0FFF) | 0x5000;
	uuid_pack(&uu, out);
}
