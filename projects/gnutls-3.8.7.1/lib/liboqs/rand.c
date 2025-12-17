/*
 * Copyright (C) 2024 Red Hat, Inc.
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

#include "config.h"

#include "liboqs/rand.h"

#include "dlwrap/oqs.h"
#include "fips.h"
#include <gnutls/crypto.h>
#include <stdint.h>

static void rand_bytes(uint8_t *data, size_t size)
{
	if (gnutls_rnd(GNUTLS_RND_RANDOM, data, size) < 0)
		_gnutls_switch_lib_state(LIB_STATE_ERROR);
}

void _gnutls_liboqs_rand_init(void)
{
	GNUTLS_OQS_FUNC(OQS_randombytes_custom_algorithm)(rand_bytes);
}

void _gnutls_liboqs_rand_deinit(void)
{
}
