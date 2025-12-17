/*
 * Copyright (C) 2008-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
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

/* This file handles all the internal functions that cope with random data
 */

#include "gnutls_int.h"
#include "errors.h"
#include "random.h"
#include "locks.h"
#include "fips.h"

#include "gl_linkedhash_list.h"
#include "gl_list.h"
#include "glthread/tls.h"
#include "gthreads.h"

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
extern gnutls_crypto_rnd_st _gnutls_fuzz_rnd_ops;
#endif

/* A global list of all allocated contexts.
 * A safety measure in case thread specific
 * context cannot be freed on thread exit
 */
GNUTLS_STATIC_MUTEX(gnutls_rnd_list_mutex);
static gl_list_t list;

/* Key used to locate and manage thread specific random generator context
 */
static gl_tls_key_t ctx_key;

/* Flag to indicate initialization
 */
static _Thread_local unsigned rnd_initialized = 0;

static void free_ctx(const void *ctx)
{
	if (ctx && _gnutls_rnd_ops.deinit)
		_gnutls_rnd_ops.deinit((void *)ctx);
}

static void delete_ctx(void *ctx)
{
	(void)gnutls_static_mutex_lock(&gnutls_rnd_list_mutex);
	gl_list_remove(list, ctx);
	gnutls_static_mutex_unlock(&gnutls_rnd_list_mutex);
}

static inline int _gnutls_rnd_init(void)
{
	int ret;
	void *ctx;
	gl_list_node_t node;

	if (likely(rnd_initialized))
		return 0;

	if (_gnutls_rnd_ops.init == NULL) {
		rnd_initialized = 1;
		return 0;
	}

	if (_gnutls_rnd_ops.init(&ctx) < 0)
		return gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);

	if (glthread_tls_set(&ctx_key, ctx)) {
		_gnutls_rnd_ops.deinit(ctx);
		return gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);
	}

	ret = gnutls_static_mutex_lock(&gnutls_rnd_list_mutex);
	if (ret < 0)
		return gnutls_assert_val(ret);
	node = gl_list_nx_add_last(list, ctx);
	gnutls_static_mutex_unlock(&gnutls_rnd_list_mutex);
	if (node == NULL) {
		_gnutls_rnd_ops.deinit(ctx);
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
	}

	rnd_initialized = 1;
	return 0;
}

int _gnutls_rnd_preinit(void)
{
	int ret;

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
#warning Insecure PRNG is enabled
	ret = gnutls_crypto_rnd_register(100, &_gnutls_fuzz_rnd_ops);
	if (ret < 0)
		return ret;

#elif defined(ENABLE_FIPS140)
	/* The FIPS140 random generator is only enabled when we are compiled
	 * with FIPS support, _and_ the system is in FIPS installed state.
	 */
	if (_gnutls_fips_mode_enabled()) {
		ret = gnutls_crypto_rnd_register(100, &_gnutls_fips_rnd_ops);
		if (ret < 0)
			return ret;
	}
#endif

	ret = _rnd_system_entropy_init();
	if (ret < 0)
		return gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);

	ret = glthread_tls_key_init(&ctx_key, delete_ctx);
	if (ret)
		return gnutls_assert_val(GNUTLS_E_RANDOM_FAILED);

	list = gl_list_nx_create_empty(GL_LINKEDHASH_LIST, NULL, NULL, free_ctx,
				       false);
	if (list == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	return 0;
}

void _gnutls_rnd_deinit(void)
{
	gl_list_free(list);
	glthread_tls_key_destroy(&ctx_key);
	rnd_initialized = 0;
	_rnd_system_entropy_deinit();
}

/**
 * gnutls_rnd:
 * @level: a security level
 * @data: place to store random bytes
 * @len: The requested size
 *
 * This function will generate random data and store it to output
 * buffer. The value of @level should be one of %GNUTLS_RND_NONCE,
 * %GNUTLS_RND_RANDOM and %GNUTLS_RND_KEY. See the manual and
 * %gnutls_rnd_level_t for detailed information.
 *
 * This function is thread-safe and also fork-safe.
 *
 * Returns: Zero on success, or a negative error code on error.
 *
 * Since: 2.12.0
 **/
int gnutls_rnd(gnutls_rnd_level_t level, void *data, size_t len)
{
	int ret;
	FAIL_IF_LIB_ERROR;

	ret = _gnutls_rnd_init();
	if (unlikely(ret < 0))
		return gnutls_assert_val(ret);

	if (likely(len > 0))
		return _gnutls_rnd_ops.rnd(gl_tls_get(ctx_key), level, data,
					   len);

	return 0;
}

/**
 * gnutls_rnd_refresh:
 *
 * This function refreshes the random generator state.
 * That is the current precise time, CPU usage, and
 * other values are input into its state.
 *
 * On a slower rate input from /dev/urandom is mixed too.
 *
 * Since: 3.1.7
 **/
void gnutls_rnd_refresh(void)
{
	if (rnd_initialized && _gnutls_rnd_ops.rnd_refresh)
		_gnutls_rnd_ops.rnd_refresh(gl_tls_get(ctx_key));
}
