/*
 * Copyright (C) 2010-2022 Free Software Foundation, Inc.
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Daiki Ueno
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

#include "pathbuf.h"
#include "gnutls_int.h"
#include <limits.h>
#include "intprops.h"

static int pathbuf_reserve(struct gnutls_pathbuf_st *buffer, size_t to_add)
{
	size_t len;
	char *ptr;

	len = buffer->len;

	if (INT_ADD_OVERFLOW(len, to_add)) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}
	len += to_add;

	/* NUL terminator.  */
	if (INT_ADD_OVERFLOW(len, 1)) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}
	len++;

	if (len <= buffer->cap) {
		return 0;
	}

	if (buffer->ptr == buffer->base) {
		ptr = gnutls_strdup(buffer->ptr);
		if (!ptr) {
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
		}
		buffer->ptr = ptr;
	}

	ptr = gnutls_realloc(buffer->ptr, len);
	if (!ptr) {
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
	}

	buffer->ptr = ptr;
	buffer->cap = len;

	return 0;
}

int _gnutls_pathbuf_init(struct gnutls_pathbuf_st *buffer, const char *base)
{
	size_t len;
	int ret;

	memset(buffer, 0, sizeof(*buffer));
	buffer->cap = sizeof(buffer->base);
	buffer->ptr = buffer->base;

	len = strlen(base);

	ret = pathbuf_reserve(buffer, len);
	if (ret < 0) {
		return ret;
	}

	strcpy(buffer->ptr, base);
	buffer->len = len;

	return 0;
}

int _gnutls_pathbuf_append(struct gnutls_pathbuf_st *buffer,
			   const char *component)
{
	size_t len;
	char *p;
	int ret;

	len = strlen(component);

	/* Path separator.  */
	if (INT_ADD_OVERFLOW(len, 1)) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}
	len++;

	ret = pathbuf_reserve(buffer, len);
	if (ret < 0) {
		return ret;
	}

	p = stpcpy(&buffer->ptr[buffer->len], "/");
	strcpy(p, component);

	/* Overflow is already checked in the call to pathbuf_reserve
	 * above.
	 */
	buffer->len += len;

	return 0;
}

int _gnutls_pathbuf_truncate(struct gnutls_pathbuf_st *buffer, size_t len)
{
	if (len > buffer->len) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}
	buffer->len = len;
	buffer->ptr[len] = '\0';
	return 0;
}

void _gnutls_pathbuf_deinit(struct gnutls_pathbuf_st *buffer)
{
	if (buffer->ptr != buffer->base) {
		gnutls_free(buffer->ptr);
	}
	memset(buffer, 0, sizeof(*buffer));
}
