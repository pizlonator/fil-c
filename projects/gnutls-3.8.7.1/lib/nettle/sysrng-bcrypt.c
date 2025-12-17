/*
 * Copyright (C) 2010-2016 Free Software Foundation, Inc.
 * Copyright (C) 2015-2016 Red Hat, Inc.
 * Copyright (C) 2000, 2001, 2008 Niels Möller
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

/* Here are the common parts of the random generator layer. 
 * Some of this code was based on the LSH 
 * random generator (the trivia and device source functions for POSIX)
 * and modified to fit gnutls' needs. Relicenced with permission. 
 * Original author Niels Möller.
 */

#include "gnutls_int.h"
#include "errors.h"
#include "locks.h"
#include "num.h"
#include <nettle/yarrow.h>
#include <errno.h>
#include "rnd-common.h"
#include <hash-pjw-bare.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* The windows randomness gatherer.
 */

#include <windows.h>
#include <bcrypt.h>

get_entropy_func _rnd_get_system_entropy = NULL;

static BCRYPT_ALG_HANDLE device_fd = 0;

static int _rnd_get_system_entropy_win32(void *rnd, size_t size)
{
	NTSTATUS err = BCryptGenRandom(device_fd, rnd, size, 0);
	if (!BCRYPT_SUCCESS(err)) {
		_gnutls_debug_log("Error in BCryptGenRandom: %ld\n", err);
		return GNUTLS_E_RANDOM_DEVICE_ERROR;
	}

	return 0;
}

int _rnd_system_entropy_init(void)
{
	NTSTATUS err = BCryptOpenAlgorithmProvider(
		&device_fd, BCRYPT_RNG_ALGORITHM, NULL, 0);
	if (!BCRYPT_SUCCESS(err)) {
		_gnutls_debug_log("error in BCryptOpenAlgorithmProvider!\n");
		return GNUTLS_E_RANDOM_DEVICE_ERROR;
	}

	_rnd_get_system_entropy = _rnd_get_system_entropy_win32;
	return 0;
}

void _rnd_system_entropy_deinit(void)
{
	BCryptCloseAlgorithmProvider(device_fd, 0);
}
