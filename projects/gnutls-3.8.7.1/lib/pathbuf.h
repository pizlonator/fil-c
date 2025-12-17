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

#ifndef GNUTLS_LIB_PATHBUF_H
#define GNUTLS_LIB_PATHBUF_H

#include "pathmax.h"
#ifdef PATH_MAX
#define GNUTLS_PATH_MAX PATH_MAX
#else
#define GNUTLS_PATH_MAX 1024
#endif

struct gnutls_pathbuf_st {
	char base[GNUTLS_PATH_MAX + 1];
	char *ptr; /* API */
	size_t len; /* API: NOT including NUL */
	size_t cap; /* including NUL */
};

/* Initialize BUFFER with the content BASE.  */
int _gnutls_pathbuf_init(struct gnutls_pathbuf_st *buffer, const char *base);

/* Append COMPONENT to BUFFER, separated with a "/".  */
int _gnutls_pathbuf_append(struct gnutls_pathbuf_st *buffer,
			   const char *component);

/* Truncate the length of BUFFER to LEN.  */
int _gnutls_pathbuf_truncate(struct gnutls_pathbuf_st *buffer, size_t len);

/* Deinitialize BUFFER.  */
void _gnutls_pathbuf_deinit(struct gnutls_pathbuf_st *buffer);

#endif /* GNUTLS_LIB_PATHBUF_H */
