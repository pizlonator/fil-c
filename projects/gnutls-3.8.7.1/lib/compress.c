/*
 * Copyright (C) 2017-2022 Red Hat, Inc.
 *
 * Authors: Nikos Mavrogiannopoulos,
 *          Zoltan Fridrich
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

#include "compress.h"

#ifdef _WIN32
#define RTLD_NOW 0
#define RTLD_GLOBAL 0
#else
#include <dlfcn.h>
#endif

#ifndef Z_LIBRARY_SONAME
#define Z_LIBRARY_SONAME "none"
#endif

#ifndef BROTLIENC_LIBRARY_SONAME
#define BROTLIENC_LIBRARY_SONAME "none"
#endif

#ifndef BROTLIDEC_LIBRARY_SONAME
#define BROTLIDEC_LIBRARY_SONAME "none"
#endif

#ifndef ZSTD_LIBRARY_SONAME
#define ZSTD_LIBRARY_SONAME "none"
#endif

#ifdef HAVE_LIBZ
#include "dlwrap/zlib.h"
#endif

#ifdef HAVE_LIBBROTLI
#include "dlwrap/brotlienc.h"
#include "dlwrap/brotlidec.h"
#endif

#ifdef HAVE_LIBZSTD
#include "dlwrap/zstd.h"
#endif

#ifdef HAVE_LIBZ
static void zlib_deinit(void)
{
	gnutls_zlib_unload_library();
}

static int zlib_init(void)
{
	if (gnutls_zlib_ensure_library(Z_LIBRARY_SONAME,
				       RTLD_NOW | RTLD_GLOBAL) < 0)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	return 0;
}
#endif /* HAVE_LIBZ */

#ifdef HAVE_LIBBROTLI

static void brotli_deinit(void)
{
	gnutls_brotlienc_unload_library();
	gnutls_brotlidec_unload_library();
}

static int brotli_init(void)
{
	if (gnutls_brotlienc_ensure_library(BROTLIENC_LIBRARY_SONAME,
					    RTLD_NOW | RTLD_GLOBAL) < 0)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	if (gnutls_brotlidec_ensure_library(BROTLIDEC_LIBRARY_SONAME,
					    RTLD_NOW | RTLD_GLOBAL) < 0)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	return 0;
}
#endif /* HAVE_LIBBROTLI */

#ifdef HAVE_LIBZSTD

static void zstd_deinit(void)
{
	gnutls_zstd_unload_library();
}

static int zstd_init(void)
{
	if (gnutls_zstd_ensure_library(ZSTD_LIBRARY_SONAME,
				       RTLD_NOW | RTLD_GLOBAL) < 0)
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	return 0;
}
#endif /* HAVE_LIBZSTD */

typedef struct {
	gnutls_compression_method_t id;
	const char *name;
	int (*init)(void);
	void (*deinit)(void);
} comp_entry;

static comp_entry comp_algs[] = {
	{ GNUTLS_COMP_NULL, "NULL", NULL, NULL },
#ifdef HAVE_LIBZ
	{ GNUTLS_COMP_ZLIB, "ZLIB", zlib_init, zlib_deinit },
#endif
#ifdef HAVE_LIBBROTLI
	{ GNUTLS_COMP_BROTLI, "BROTLI", brotli_init, brotli_deinit },
#endif
#ifdef HAVE_LIBZSTD
	{ GNUTLS_COMP_ZSTD, "ZSTD", zstd_init, zstd_deinit },
#endif
	{ GNUTLS_COMP_UNKNOWN, NULL, NULL, NULL }
};

static const gnutls_compression_method_t alg_list[] = { GNUTLS_COMP_NULL,
#ifdef HAVE_LIBZ
							GNUTLS_COMP_ZLIB,
#endif
#ifdef HAVE_LIBBROTLI
							GNUTLS_COMP_BROTLI,
#endif
#ifdef HAVE_LIBZSTD
							GNUTLS_COMP_ZSTD,
#endif
							0 };

/* Initialize given compression method
 *
 * Calling any of the compression functions without first initializing
 * the respective compression method results in undefined behavior.
 */
int _gnutls_compression_init_method(gnutls_compression_method_t method)
{
	comp_entry *p;

	for (p = comp_algs; p->name; ++p)
		if (p->id == method)
			return p->init ? p->init() : GNUTLS_E_INVALID_REQUEST;

	return GNUTLS_E_INVALID_REQUEST;
}

/* Deinitialize all compression methods
 * 
 * If no compression methods were initialized,
 * this function does nothing.
 */
void _gnutls_compression_deinit(void)
{
	comp_entry *p;

	for (p = comp_algs; p->name; ++p)
		if (p->deinit)
			p->deinit();
}

/**
 * gnutls_compression_get_name:
 * @algorithm: is a Compression algorithm
 *
 * Convert a #gnutls_compression_method_t value to a string.
 *
 * Returns: a pointer to a string that contains the name of the
 *   specified compression algorithm, or %NULL.
 **/
const char *gnutls_compression_get_name(gnutls_compression_method_t algorithm)
{
	const comp_entry *p;

	for (p = comp_algs; p->name; ++p)
		if (p->id == algorithm)
			return p->name;

	return NULL;
}

/**
 * gnutls_compression_get_id:
 * @name: is a compression method name
 *
 * The names are compared in a case insensitive way.
 *
 * Returns: an id of the specified in a string compression method, or
 *   %GNUTLS_COMP_UNKNOWN on error.
 **/
gnutls_compression_method_t gnutls_compression_get_id(const char *name)
{
	const comp_entry *p;

	for (p = comp_algs; p->name; ++p)
		if (!strcasecmp(p->name, name))
			return p->id;

	return GNUTLS_COMP_UNKNOWN;
}

/**
 * gnutls_compression_list:
 *
 * Get a list of compression methods.
 *
 * Returns: a zero-terminated list of #gnutls_compression_method_t
 *   integers indicating the available compression methods.
 **/
const gnutls_compression_method_t *gnutls_compression_list(void)
{
	return alg_list;
}

/*************************/
/* Compression functions */
/*************************/

size_t _gnutls_compress_bound(gnutls_compression_method_t alg, size_t src_len)
{
	switch (alg) {
#ifdef HAVE_LIBZ
	case GNUTLS_COMP_ZLIB:
		return GNUTLS_ZLIB_FUNC(compressBound)(src_len);
#endif
#ifdef HAVE_LIBBROTLI
	case GNUTLS_COMP_BROTLI:
		return GNUTLS_BROTLIENC_FUNC(BrotliEncoderMaxCompressedSize)(
			src_len);
#endif
#ifdef HAVE_LIBZSTD
	case GNUTLS_COMP_ZSTD:
		return GNUTLS_ZSTD_FUNC(ZSTD_compressBound)(src_len);
#endif
	default:
		return 0;
	}
	return 0;
}

int _gnutls_compress(gnutls_compression_method_t alg, uint8_t *dst,
		     size_t dst_len, const uint8_t *src, size_t src_len)
{
	int ret = GNUTLS_E_COMPRESSION_FAILED;

	switch (alg) {
#ifdef HAVE_LIBZ
	case GNUTLS_COMP_ZLIB: {
		int err;
		uLongf comp_len = dst_len;

		err = GNUTLS_ZLIB_FUNC(compress)(dst, &comp_len, src, src_len);
		if (err != Z_OK)
			return gnutls_assert_val(GNUTLS_E_COMPRESSION_FAILED);
		ret = comp_len;
	} break;
#endif
#ifdef HAVE_LIBBROTLI
	case GNUTLS_COMP_BROTLI: {
		BROTLI_BOOL err;
		size_t comp_len = dst_len;

		err = GNUTLS_BROTLIENC_FUNC(BrotliEncoderCompress)(
			BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW,
			BROTLI_DEFAULT_MODE, src_len, src, &comp_len, dst);
		if (!err)
			return gnutls_assert_val(GNUTLS_E_COMPRESSION_FAILED);
		ret = comp_len;
	} break;
#endif
#ifdef HAVE_LIBZSTD
	case GNUTLS_COMP_ZSTD: {
		size_t comp_len;

		comp_len = GNUTLS_ZSTD_FUNC(ZSTD_compress)(
			dst, dst_len, src, src_len, ZSTD_CLEVEL_DEFAULT);
		if (GNUTLS_ZSTD_FUNC(ZSTD_isError)(comp_len))
			return gnutls_assert_val(GNUTLS_E_COMPRESSION_FAILED);
		ret = comp_len;
	} break;
#endif
	default:
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}

#ifdef COMPRESSION_DEBUG
	_gnutls_debug_log("Compression ratio: %f\n",
			  (float)((float)ret / (float)src_len));
#endif

	return ret;
}

int _gnutls_decompress(gnutls_compression_method_t alg, uint8_t *dst,
		       size_t dst_len, const uint8_t *src, size_t src_len)
{
	int ret = GNUTLS_E_DECOMPRESSION_FAILED;

	switch (alg) {
#ifdef HAVE_LIBZ
	case GNUTLS_COMP_ZLIB: {
		int err;
		uLongf plain_len = dst_len;

		err = GNUTLS_ZLIB_FUNC(uncompress)(dst, &plain_len, src,
						   src_len);
		if (err != Z_OK)
			return gnutls_assert_val(GNUTLS_E_DECOMPRESSION_FAILED);
		ret = plain_len;
	} break;
#endif
#ifdef HAVE_LIBBROTLI
	case GNUTLS_COMP_BROTLI: {
		BrotliDecoderResult err;
		size_t plain_len = dst_len;

		err = GNUTLS_BROTLIDEC_FUNC(
			BrotliDecoderDecompress)(src_len, src, &plain_len, dst);
		if (err != BROTLI_DECODER_RESULT_SUCCESS)
			return gnutls_assert_val(GNUTLS_E_DECOMPRESSION_FAILED);
		ret = plain_len;
	} break;
#endif
#ifdef HAVE_LIBZSTD
	case GNUTLS_COMP_ZSTD: {
		size_t plain_len;

		plain_len = GNUTLS_ZSTD_FUNC(ZSTD_decompress)(dst, dst_len, src,
							      src_len);
		if (GNUTLS_ZSTD_FUNC(ZSTD_isError)(plain_len))
			return gnutls_assert_val(GNUTLS_E_DECOMPRESSION_FAILED);
		ret = plain_len;
	} break;
#endif
	default:
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
	}

	return ret;
}
