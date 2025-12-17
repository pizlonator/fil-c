/*
 * Copyright (C) 2010-2016 Free Software Foundation, Inc.
 * Copyright (C) 2015-2016 Red Hat, Inc.
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

#include "config.h"
#include "system.h"
#include "gnutls_int.h"
#include "errors.h"

#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "glthread/lock.h"

/* System specific lock function wrappers.
 */

/* Thread stuff */

static int gnutls_system_mutex_init(void **priv)
{
	gl_lock_t *lock = malloc(sizeof(gl_lock_t));

	if (!lock) {
		return GNUTLS_E_MEMORY_ERROR;
	}

	if (glthread_lock_init(lock)) {
		free(lock);
		return gnutls_assert_val(GNUTLS_E_LOCKING_ERROR);
	}

	*priv = lock;
	return 0;
}

static int gnutls_system_mutex_deinit(void **priv)
{
	if (glthread_lock_destroy((gl_lock_t *)*priv)) {
		return gnutls_assert_val(GNUTLS_E_LOCKING_ERROR);
	}
	free(*priv);
	return 0;
}

static int gnutls_system_mutex_lock(void **priv)
{
	if (glthread_lock_lock((gl_lock_t *)*priv)) {
		return gnutls_assert_val(GNUTLS_E_LOCKING_ERROR);
	}
	return 0;
}

static int gnutls_system_mutex_unlock(void **priv)
{
	if (glthread_lock_unlock((gl_lock_t *)*priv)) {
		return gnutls_assert_val(GNUTLS_E_LOCKING_ERROR);
	}
	return 0;
}

mutex_init_func gnutls_mutex_init = gnutls_system_mutex_init;
mutex_deinit_func gnutls_mutex_deinit = gnutls_system_mutex_deinit;
mutex_lock_func gnutls_mutex_lock = gnutls_system_mutex_lock;
mutex_unlock_func gnutls_mutex_unlock = gnutls_system_mutex_unlock;
