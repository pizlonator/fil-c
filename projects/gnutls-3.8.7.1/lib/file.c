/*
 * Copyright (C) 2005-2015 Free Software Foundation, Inc.
 * Copyright (C) 2015 Nikos Mavrogiannopoulos, Inc.
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

#include "gnutls_int.h"
#include "file.h"
#include <read-file.h>

int _gnutls_file_exists(const char *file)
{
	FILE *fp;

	fp = fopen(file, "re");
	if (fp == NULL)
		return -1;

	fclose(fp);
	return 0;
}

/**
 * gnutls_load_file:
 * @filename: the name of the file to load
 * @data: Where the file will be stored
 *
 * This function will load a file into a datum. The data are
 * zero terminated but the terminating null is not included in length.
 * The returned data are allocated using gnutls_malloc().
 *
 * Note that this function is not designed for reading sensitive materials,
 * such as private keys, on practical applications. When the reading fails
 * in the middle, the partially loaded content might remain on memory.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 * Since 3.1.0
 **/
int gnutls_load_file(const char *filename, gnutls_datum_t *data)
{
	size_t len;

	data->data = (void *)read_file(filename, RF_BINARY, &len);
	if (data->data == NULL)
		return GNUTLS_E_FILE_ERROR;

	if (malloc != gnutls_malloc) {
		void *tmp = gnutls_malloc(len);

		memcpy(tmp, data->data, len);
		free(data->data);
		data->data = tmp;
	}

	data->size = len;

	return 0;
}
