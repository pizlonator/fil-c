/*
 * Copyright (C) 2017 Nikos Mavrogiannopoulos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#define HANDSHAKE_MAX_RETRY_COUNT 10

typedef struct mem_st {
	const uint8_t *data;
	size_t size;
} mem_st;

static ssize_t error_push(gnutls_transport_ptr_t tr, const void *data,
			  size_t len)
{
	return -1;
}

static ssize_t error_pull(gnutls_transport_ptr_t tr, void *data, size_t len)
{
	return -1;
}

static int handshake_discard(gnutls_session_t session,
			     gnutls_record_encryption_level_t level,
			     gnutls_handshake_description_t htype,
			     const void *data, size_t data_size)
{
	return 0;
}

static int handshake_pull(gnutls_session_t session, mem_st *data)
{
	uint32_t level, size;
	int ret;

	if (data->size < 4) {
		return -1;
	}

	level = ((unsigned)data->data[0] << 24) | (data->data[1] << 16) |
		(data->data[2] << 8) | data->data[3];

	data->size -= 4;
	data->data += 4;

	if (data->size < 4) {
		return -1;
	}

	size = ((unsigned)data->data[0] << 24) | (data->data[1] << 16) |
	       (data->data[2] << 8) | data->data[3];

	data->size -= 4;
	data->data += 4;

	if (size > data->size) {
		return -1;
	}

	ret = gnutls_handshake_write(session,
				     (gnutls_record_encryption_level_t)level,
				     data->data, size);
	data->size -= size;
	data->data += size;

	return ret;
}

#endif
