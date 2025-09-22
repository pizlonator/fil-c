/*	$OpenBSD: arc4random.c,v 1.58 2022/07/31 13:41:45 tb Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ChaCha based random number generator for OpenBSD.
 */

/*
 * OPENBSD ORIGINAL: lib/libc/crypt/arc4random.c
 *                   lib/libc/crypt/arc4random.h
 */

#include "config.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#if defined(HAVE_OPENSSL)
#include <openssl/rand.h>
#endif

#define KEYSTREAM_ONLY
#include "chacha_private.h"

#define minimum(a, b) ((a) < (b) ? (a) : (b))

#if defined(__GNUC__) || defined(_MSC_VER)
#define inline __inline
#else				/* __GNUC__ || _MSC_VER */
#define inline
#endif				/* !__GNUC__ && !_MSC_VER */

#define KEYSZ	32
#define IVSZ	8
#define BLOCKSZ	64
#define RSBUFSZ	(16*BLOCKSZ)

#define REKEY_BASE	(1024*1024) /* NB. should be a power of 2 */

/* Marked MAP_INHERIT_ZERO, so zero'd out in fork children. */
static struct _rs {
	size_t		rs_have;	/* valid bytes at end of rs_buf */
	size_t		rs_count;	/* bytes till reseed */
} *rs;

/* Maybe be preserved in fork children, if _rs_allocate() decides. */
static struct _rsx {
	chacha_ctx	rs_chacha;	/* chacha context for random keystream */
	u_char		rs_buf[RSBUFSZ];	/* keystream blocks */
} *rsx;

static int _dhcpcd_rand_fd = -1;	/* /dev/urandom fd */

static int _dhcpcd_getentropy(void *, size_t);
static inline int _rs_allocate(struct _rs **, struct _rsx **);

/* dhcpcd needs to hold onto the fd at fork due to privsep */
#if 0
static inline void _rs_forkdetect(void);
#else
#define _rs_forkdetect()
#define _rs_forkhandler()
#endif

/* Inline "arc4random.h" */
#include <sys/types.h>
#include <sys/mman.h>

static inline void _rs_rekey(u_char *dat, size_t datlen);

/* dhcpcd isn't multithreaded */
#define _ARC4_LOCK()
#define _ARC4_UNLOCK()

static int
_dhcpcd_getentropy(void *buf, size_t length)
{
	struct timeval	 tv;
	uint8_t		*rand = (uint8_t *)buf;

#if defined (HAVE_OPENSSL)
	if (RAND_priv_bytes(buf, (int)length) == 1)
		return (0);
#endif

	if (length < sizeof(tv)) {
		gettimeofday(&tv, NULL);
		memcpy(buf, &tv, sizeof(tv));
		length -= sizeof(tv);
		rand += sizeof(tv);
	}
	if (_dhcpcd_rand_fd == -1)
		_dhcpcd_rand_fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK);
	if (_dhcpcd_rand_fd != -1) {
		/* coverity[check_return] */
		(void)read(_dhcpcd_rand_fd, rand, length);
	}

	/* Never fail. If there is an error reading from /dev/urandom,
         * just use what is on the stack. */
	return (0);
}

static inline void
_getentropy_fail(void)
{
	raise(SIGKILL);
}

#if 0
static volatile sig_atomic_t _rs_forked;

static inline void
_rs_forkhandler(void)
{
	_rs_forked = 1;
}

static inline void
_rs_forkdetect(void)
{
	static pid_t _rs_pid = 0;
	pid_t pid = getpid();

        /* XXX unusual calls to clone() can bypass checks */
	if (_rs_pid == 0 || _rs_pid == 1 || _rs_pid != pid || _rs_forked) {
		_rs_pid = pid;
		_rs_forked = 0;
		if (rs)
			memset(rs, 0, sizeof(*rs));
	}
}
#endif

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
	if ((*rsp = mmap(NULL, sizeof(**rsp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return (-1);

	if ((*rsxp = mmap(NULL, sizeof(**rsxp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
		munmap(*rsp, sizeof(**rsp));
		*rsp = NULL;
		return (-1);
	}

	_rs_forkhandler();
	return (0);
}

static inline void
_rs_init(u_char *buf, size_t n)
{
	if (n < KEYSZ + IVSZ)
		return;

	if (rs == NULL) {
		if (_rs_allocate(&rs, &rsx) == -1)
			_exit(1);
	}

	chacha_keysetup(&rsx->rs_chacha, buf, KEYSZ * 8);
	chacha_ivsetup(&rsx->rs_chacha, buf + KEYSZ);
}

static void
_rs_stir(void)
{
	u_char rnd[KEYSZ + IVSZ];
	uint32_t rekey_fuzz = 0;

	if (_dhcpcd_getentropy(rnd, sizeof rnd) == -1)
		_getentropy_fail();

	if (!rs)
		_rs_init(rnd, sizeof(rnd));
	else
		_rs_rekey(rnd, sizeof(rnd));
#if defined(HAVE_EXPLICIT_BZERO)
	explicit_bzero(rnd, sizeof(rnd));	/* discard source seed */
#elif defined(HAVE_MEMSET_EXPLICIT)
	(void)memset_explicit(rnd, 0, sizeof(rnd));
#elif defined(HAVE_MEMSET_S)
	(void)memset_s(rnd, sizeof(rnd), 0, sizeof(rnd));
#else
#warning potentially insecure use of memset discarding the source seed
	(void)memset(rnd, 0, sizeof(rnd));	/* discard source seed */
#endif

	/* invalidate rs_buf */
	rs->rs_have = 0;
	memset(rsx->rs_buf, 0, sizeof(rsx->rs_buf));

	/* rekey interval should not be predictable */
	chacha_encrypt_bytes(&rsx->rs_chacha, (uint8_t *)&rekey_fuzz,
	    (uint8_t *)&rekey_fuzz, sizeof(rekey_fuzz));
	rs->rs_count = REKEY_BASE + (rekey_fuzz % REKEY_BASE);
}

static inline void
_rs_stir_if_needed(size_t len)
{
	_rs_forkdetect();
	if (!rs || rs->rs_count <= len)
		_rs_stir();
	if (rs->rs_count <= len)
		rs->rs_count = 0;
	else
		rs->rs_count -= len;
}

static inline void
_rs_rekey(u_char *dat, size_t datlen)
{
#ifndef KEYSTREAM_ONLY
	memset(rsx->rs_buf, 0, sizeof(rsx->rs_buf));
#endif
	/* fill rs_buf with the keystream */
	chacha_encrypt_bytes(&rsx->rs_chacha, rsx->rs_buf,
	    rsx->rs_buf, sizeof(rsx->rs_buf));
	/* mix in optional user provided data */
	if (dat) {
		size_t i, m;

		m = minimum(datlen, KEYSZ + IVSZ);
		for (i = 0; i < m; i++)
			rsx->rs_buf[i] ^= dat[i];
	}
	/* immediately reinit for backtracking resistance */
	_rs_init(rsx->rs_buf, KEYSZ + IVSZ);
	memset(rsx->rs_buf, 0, KEYSZ + IVSZ);
	rs->rs_have = sizeof(rsx->rs_buf) - KEYSZ - IVSZ;
}

static inline void
_rs_random_buf(void *_buf, size_t n)
{
	u_char *buf = (u_char *)_buf;
	u_char *keystream;
	size_t m;

	_rs_stir_if_needed(n);
	while (n > 0) {
		if (rs->rs_have > 0) {
			m = minimum(n, rs->rs_have);
			keystream = rsx->rs_buf + sizeof(rsx->rs_buf)
			    - rs->rs_have;
			memcpy(buf, keystream, m);
			memset(keystream, 0, m);
			buf += m;
			n -= m;
			rs->rs_have -= m;
		}
		if (rs->rs_have == 0)
			_rs_rekey(NULL, 0);
	}
}

static inline void
_rs_random_u32(uint32_t *val)
{
	u_char *keystream;

	_rs_stir_if_needed(sizeof(*val));
	if (rs->rs_have < sizeof(*val))
		_rs_rekey(NULL, 0);
	keystream = rsx->rs_buf + sizeof(rsx->rs_buf) - rs->rs_have;
	memcpy(val, keystream, sizeof(*val));
	memset(keystream, 0, sizeof(*val));
	rs->rs_have -= sizeof(*val);
}

uint32_t
arc4random(void)
{
	uint32_t val;

	_ARC4_LOCK();
	_rs_random_u32(&val);
	_ARC4_UNLOCK();
	return val;
}

void
arc4random_buf(void *buf, size_t n)
{
	_ARC4_LOCK();
	_rs_random_buf(buf, n);
	_ARC4_UNLOCK();
}
