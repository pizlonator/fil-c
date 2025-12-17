/*
 * Copyright (C) 2010-2016 Free Software Foundation, Inc.
 * Copyright (C) 2015-2016 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

/* The Linux style system random generator: That is,
 * getrandom() -> /dev/urandom, where "->" indicates fallback.
 */

#ifndef RND_NO_INCLUDES
#include "gnutls_int.h"
#include "errors.h"
#include "num.h"
#include <errno.h>
#include "rnd-common.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* gnulib wants to claim strerror even if it cannot provide it. WTF */
#undef strerror

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

get_entropy_func _rnd_get_system_entropy = NULL;

#if defined(__linux__)
#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#else
#include <sys/syscall.h>
#undef getrandom
#if defined(SYS_getrandom)
#define getrandom(dst, s, flags) \
	syscall(SYS_getrandom, (void *)dst, (size_t)s, (unsigned int)flags)
#else
static ssize_t _getrandom0(void *buf, size_t buflen, unsigned int flags)
{
	errno = ENOSYS;
	return -1;
}

#define getrandom(dst, s, flags) _getrandom0(dst, s, flags)
#endif
#endif

static unsigned have_getrandom(void)
{
	char c;
	int ret;
	ret = getrandom(&c, 1, 1 /*GRND_NONBLOCK */);
	if (ret == 1 || (ret == -1 && errno == EAGAIN))
		return 1;
	return 0;
}

/* returns exactly the amount of bytes requested */
static int force_getrandom(void *buf, size_t buflen, unsigned int flags)
{
	int left = buflen;
	int ret;
	uint8_t *p = buf;

	while (left > 0) {
		ret = getrandom(p, left, flags);
		if (ret == -1) {
			if (errno != EINTR)
				return ret;
		}

		if (ret > 0) {
			left -= ret;
			p += ret;
		}
	}

	return buflen;
}

static int _rnd_get_system_entropy_getrandom(void *_rnd, size_t size)
{
	int ret;
	ret = force_getrandom(_rnd, size, 0);
	if (ret == -1) {
		int e = errno;
		gnutls_assert();
		_gnutls_debug_log("Failed to use getrandom: %s\n", strerror(e));
		return GNUTLS_E_RANDOM_DEVICE_ERROR;
	}

	return 0;
}
#else /* not linux */
#define have_getrandom() 0
#endif

static int _rnd_get_system_entropy_urandom(void *_rnd, size_t size)
{
	uint8_t *rnd = _rnd;
	uint32_t done;
	int urandom_fd;

	urandom_fd = open("/dev/urandom", O_RDONLY);
	if (urandom_fd < 0) {
		_gnutls_debug_log("Cannot open /dev/urandom!\n");
		return GNUTLS_E_RANDOM_DEVICE_ERROR;
	}

	for (done = 0; done < size;) {
		int res;
		do {
			res = read(urandom_fd, rnd + done, size - done);
		} while (res < 0 && errno == EINTR);

		if (res <= 0) {
			int e = errno;
			if (res < 0) {
				_gnutls_debug_log(
					"Failed to read /dev/urandom: %s\n",
					strerror(e));
			} else {
				_gnutls_debug_log(
					"Failed to read /dev/urandom: end of file\n");
			}

			close(urandom_fd);
			return GNUTLS_E_RANDOM_DEVICE_ERROR;
		}

		done += res;
	}

	close(urandom_fd);
	return 0;
}

int _rnd_system_entropy_init(void)
{
	int urandom_fd;

#if defined(__linux__)
	/* Enable getrandom() usage if available */
	if (have_getrandom()) {
		_rnd_get_system_entropy = _rnd_get_system_entropy_getrandom;
		_gnutls_debug_log("getrandom random generator was selected\n");
		return 0;
	} else {
		_gnutls_debug_log("getrandom is not available\n");
	}
#endif

	/* Fallback: /dev/urandom */

	/* Check that we can open it */
	urandom_fd = open("/dev/urandom", O_RDONLY);
	if (urandom_fd < 0) {
		_gnutls_debug_log(
			"Cannot open /dev/urandom during initialization!\n");
		return gnutls_assert_val(GNUTLS_E_RANDOM_DEVICE_ERROR);
	}
	close(urandom_fd);

	_rnd_get_system_entropy = _rnd_get_system_entropy_urandom;
	_gnutls_debug_log("/dev/urandom random generator was selected\n");

	return 0;
}

void _rnd_system_entropy_deinit(void)
{
	/* A no-op now when we open and close /dev/urandom every time */
	return;
}
