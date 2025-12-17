/*
 * Copyright (C) 2018 Red Hat, Inc.
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
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>

/* Test legal and illegal use of gnutls_cipher_* and gnutls_aead_cipher_*
 * API. This test is written using fork, because some of the test
 * cases may hit assertion failure in Nettle and crash the process.
 */

#if defined(WIN32)
int main(int argc, char **argv)
{
	exit(77);
}
#else

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include "utils.h"

#define AES_GCM_ENCRYPT_PLAINTEXT_MAX ((1ULL << 36) - 32)
#if SIZE_MAX >= AES_GCM_ENCRYPT_PLAINTEXT_MAX
#define TEST_AES_GCM_ENCRYPT_PLAINTEXT_SIZE 1
#endif

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d>| %s", level, str);
}

/* (Non-AEAD) Test a happy path where everything works */
static void test_cipher_happy(int algo)
{
	int ret;
	gnutls_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t data[128];
	gnutls_datum_t key, iv;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(data, 0xfa, sizeof(data));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n");
	}

	ret = gnutls_cipher_init(&ch, algo, &key, &iv);
	if (ret < 0)
		fail("gnutls_cipher_init failed\n");

	ret = gnutls_cipher_encrypt(ch, data, sizeof(data));
	if (ret < 0)
		fail("gnutls_cipher_encrypt failed\n");

	gnutls_cipher_deinit(ch);

	gnutls_global_deinit();
}

/* Test whether an invalid call to gnutls_cipher_encrypt() is caught */
static void test_cipher_invalid_partial(int algo)
{
	int ret;
	gnutls_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t data[128];
	gnutls_datum_t key, iv;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(data, 0xfa, sizeof(data));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n"); /*errcode 1 */
	}

	ret = gnutls_cipher_init(&ch, algo, &key, &iv);
	if (ret < 0)
		fail("gnutls_cipher_init failed\n"); /*errcode 1 */

	/* try encrypting in a way that violates nettle's block conventions */
	ret = gnutls_cipher_encrypt(ch, data, sizeof(data) - 1);
	if (ret >= 0)
		fail("succeeded in encrypting partial data on block cipher\n");
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("wrong kind of error on decrypting onto a short buffer,"
		     "%s instead of GNUTLS_E_INVALID_REQUEST\n",
		     gnutls_strerror_name(ret));

	gnutls_cipher_deinit(ch);

	gnutls_global_deinit();
}

/* Test AEAD encryption/decryption */
static void test_aead_happy(int algo)
{
	int ret;
	gnutls_aead_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t auth[32];
	uint8_t ctext[128 + 32];
	size_t ctext_len;
	uint8_t ptext[128];
	uint8_t otext[128];
	size_t ptext_len;
	gnutls_datum_t key, iv;
	size_t tag_len;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	ptext_len = sizeof(ptext);
	tag_len = gnutls_cipher_get_tag_size(algo);

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(ptext, 0xfa, sizeof(ptext));
	memset(otext, 0xfc, sizeof(otext));
	memset(ctext, 0xfa, sizeof(ctext));
	memset(auth, 0xfb, sizeof(auth));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0)
		fail("Cannot initialize library\n");

	ret = gnutls_aead_cipher_init(&ch, algo, &key);
	if (ret < 0)
		fail("gnutls_aead_cipher_init failed\n");

	ctext_len = sizeof(ctext);
	ret = gnutls_aead_cipher_encrypt(ch, iv.data, iv.size, auth,
					 sizeof(auth), tag_len, ptext,
					 sizeof(ptext), ctext, &ctext_len);
	if (ret < 0)
		fail("could not encrypt data\n");

	if (ctext_len != sizeof(ptext) + tag_len)
		fail("output ciphertext length mismatch\n");

	ret = gnutls_aead_cipher_decrypt(ch, iv.data, iv.size, auth,
					 sizeof(auth), tag_len, ctext,
					 ctext_len, ptext, &ptext_len);
	if (ret < 0)
		fail("could not decrypt data: %s\n", gnutls_strerror(ret));

	if (!memcmp(ptext, otext, sizeof(ptext)))
		fail("mismatch of decrypted data\n");

	gnutls_aead_cipher_deinit(ch);

	gnutls_global_deinit();
	return;
}

/* Test whether an invalid gnutls_cipher_add_auth() is caught */
static void test_aead_invalid_add_auth(int algo)
{
	int ret;
	gnutls_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t data[128];
	gnutls_datum_t key, iv;

	if (algo == GNUTLS_CIPHER_CHACHA20_POLY1305)
		return;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(data, 0xfa, sizeof(data));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n"); /*errcode 1 */
	}

	ret = gnutls_cipher_init(&ch, algo, &key, &iv);
	if (ret < 0)
		fail("gnutls_cipher_init failed\n"); /*errcode 1 */

	ret = gnutls_cipher_add_auth(ch, data, sizeof(data) - 1);
	if (ret < 0)
		fail("could not add auth data\n");

	ret = gnutls_cipher_add_auth(ch, data, 16);
	if (ret >= 0)
		fail("succeeded in adding auth data after partial data were given\n");
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("wrong kind of error on decrypting onto a short buffer,"
		     "%s instead of GNUTLS_E_INVALID_REQUEST\n",
		     gnutls_strerror_name(ret));

	gnutls_cipher_deinit(ch);

	gnutls_global_deinit();
	return;
}

/* Test whether an invalid call to gnutls_cipher_encrypt() is caught */
static void test_aead_invalid_partial_encrypt(int algo)
{
	int ret;
	gnutls_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t data[128];
	gnutls_datum_t key, iv;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(data, 0xfa, sizeof(data));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n"); /*errcode 1 */
	}

	ret = gnutls_cipher_init(&ch, algo, &key, &iv);
	if (ret < 0)
		fail("gnutls_cipher_init failed\n"); /*errcode 1 */

	/* try encrypting in a way that violates nettle's AEAD conventions */
	ret = gnutls_cipher_encrypt(ch, data, sizeof(data) - 1);
	if (ret < 0)
		fail("could not encrypt data\n");

	ret = gnutls_cipher_encrypt(ch, data, sizeof(data));
	if (ret >= 0)
		fail("succeeded in encrypting partial data after partial data were given\n");
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("wrong kind of error on decrypting onto a short buffer,"
		     "%s instead of GNUTLS_E_INVALID_REQUEST\n",
		     gnutls_strerror_name(ret));

	gnutls_cipher_deinit(ch);

	gnutls_global_deinit();
	return;
}

/* Test whether an invalid call to gnutls_aead_cipher_decrypt() is caught */
static void test_aead_invalid_short_decrypt(int algo)
{
	int ret;
	gnutls_aead_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t auth[32];
	uint8_t ctext[128 + 32];
	size_t ctext_len;
	uint8_t ptext[128];
	size_t ptext_len;
	gnutls_datum_t key, iv;
	size_t tag_len;

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	tag_len = gnutls_cipher_get_tag_size(algo);

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(ptext, 0xfa, sizeof(ptext));
	memset(ctext, 0xfa, sizeof(ctext));
	memset(auth, 0xfb, sizeof(auth));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0)
		fail("Cannot initialize library\n");

	ret = gnutls_aead_cipher_init(&ch, algo, &key);
	if (ret < 0)
		fail("gnutls_aead_cipher_init failed\n");

	ctext_len = sizeof(ctext);
	ret = gnutls_aead_cipher_encrypt(ch, iv.data, iv.size, auth,
					 sizeof(auth), tag_len, ptext,
					 sizeof(ptext), ctext, &ctext_len);
	if (ret < 0)
		fail("could not encrypt data\n");

	if (ctext_len != sizeof(ptext) + tag_len)
		fail("output ciphertext length mismatch\n");

	ptext_len = 0;
	ret = gnutls_aead_cipher_decrypt(ch, iv.data, iv.size, auth,
					 sizeof(auth), tag_len, ctext,
					 ctext_len, ptext, &ptext_len);
	if (ret >= 0)
		fail("succeeded in decrypting data onto a short buffer\n");
	if (ret != GNUTLS_E_SHORT_MEMORY_BUFFER)
		fail("wrong kind of error on decrypting onto a short buffer,"
		     "%s instead of GNUTLS_E_SHORT_MEMORY_BUFFER\n",
		     gnutls_strerror_name(ret));

	gnutls_aead_cipher_deinit(ch);

	gnutls_global_deinit();
	return;
}

#ifdef TEST_AES_GCM_ENCRYPT_PLAINTEXT_SIZE
/* Test whether an invalid call to gnutls_cipher_encrypt() with too
 * long message is caught */
static void test_aead_invalid_too_long_encrypt(int algo)
{
	int ret;
	gnutls_cipher_hd_t ch;
	uint8_t key16[64];
	uint8_t iv16[32];
	uint8_t data[128];
	gnutls_datum_t key, iv;

	if (algo != GNUTLS_CIPHER_AES_128_GCM &&
	    algo != GNUTLS_CIPHER_AES_192_GCM &&
	    algo != GNUTLS_CIPHER_AES_256_GCM) {
		return;
	}

	key.data = key16;
	key.size = gnutls_cipher_get_key_size(algo);
	assert(key.size <= sizeof(key16));

	iv.data = iv16;
	iv.size = gnutls_cipher_get_iv_size(algo);
	assert(iv.size <= sizeof(iv16));

	memset(iv.data, 0xff, iv.size);
	memset(key.data, 0xfe, key.size);
	memset(data, 0xfa, sizeof(data));

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n"); /*errcode 1 */
	}

	ret = gnutls_cipher_init(&ch, algo, &key, &iv);
	if (ret < 0)
		fail("gnutls_cipher_init failed\n"); /*errcode 1 */

	/* Test exceeding AES-GCM plaintext limit */
	ret = gnutls_cipher_encrypt(ch, data, sizeof(data));
	if (ret < 0)
		fail("could not encrypt data\n");

	/* A few blocks larger than AES_GCM_ENCRYPT_PLAINTEXT_MAX combined with
	 * the previous call.  Use NULL for PLAINTEXT so the access to the first
	 * block always results in page fault (in case the limit is not
	 * enforced).
	 */
	ret = gnutls_cipher_encrypt(ch, NULL, AES_GCM_ENCRYPT_PLAINTEXT_MAX);
	if (ret >= 0)
		fail("succeeded in encrypting too long data\n");
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("wrong kind of error on encrypting too long data,"
		     "%s instead of GNUTLS_E_INVALID_REQUEST\n",
		     gnutls_strerror_name(ret));

	gnutls_cipher_deinit(ch);

	gnutls_global_deinit();
	return;
}
#endif

static void check_status(int status)
{
	if (WEXITSTATUS(status) != 0 ||
	    (WIFSIGNALED(status) && WTERMSIG(status) != SIGABRT)) {
		if (WIFSIGNALED(status)) {
			fail("Child died with signal %d\n", WTERMSIG(status));
		} else {
			fail("Child died with status %d\n",
			     WEXITSTATUS(status));
		}
	}
}

typedef void subtest(int algo);

static void fork_subtest(subtest func, int algo)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
		return;
	}

	if (child) {
		int status;
		/* parent */
		wait(&status);
		check_status(status);
	} else {
		func(algo);
		exit(0);
	}
};

static void start(const char *name, int algo, unsigned aead)
{
	success("trying %s\n", name);

	signal(SIGPIPE, SIG_IGN);

	success("trying %s: test_cipher_happy\n", name);
	fork_subtest(test_cipher_happy, algo);

	if (!aead) {
		success("trying %s: test_cipher_invalid_partial\n", name);
		fork_subtest(test_cipher_invalid_partial, algo);
	}

	if (aead) {
		success("trying %s: test_aead_happy\n", name);
		fork_subtest(test_aead_happy, algo);

		success("trying %s: test_aead_invalid_add_auth\n", name);
		fork_subtest(test_aead_invalid_add_auth, algo);

		success("trying %s: test_aead_invalid_partial_encrypt\n", name);
		fork_subtest(test_aead_invalid_partial_encrypt, algo);

		success("trying %s: test_aead_invalid_short_decrypt\n", name);
		fork_subtest(test_aead_invalid_short_decrypt, algo);

#if TEST_AES_GCM_ENCRYPT_PLAINTEXT_SIZE
		success("trying %s: test_aead_invalid_too_long_encrypt\n",
			name);
		fork_subtest(test_aead_invalid_too_long_encrypt, algo);
#endif
	}
}

void doit(void)
{
	start("aes128-gcm", GNUTLS_CIPHER_AES_128_GCM, 1);
	start("aes192-gcm", GNUTLS_CIPHER_AES_192_GCM, 1);
	start("aes256-gcm", GNUTLS_CIPHER_AES_256_GCM, 1);
	start("aes128-cbc", GNUTLS_CIPHER_AES_128_CBC, 0);
	start("aes192-cbc", GNUTLS_CIPHER_AES_192_CBC, 0);
	start("aes256-cbc", GNUTLS_CIPHER_AES_256_CBC, 0);
	if (!gnutls_fips140_mode_enabled()) {
		start("3des-cbc", GNUTLS_CIPHER_3DES_CBC, 0);
		start("camellia128-gcm", GNUTLS_CIPHER_CAMELLIA_128_GCM, 1);
		start("camellia256-gcm", GNUTLS_CIPHER_CAMELLIA_256_GCM, 1);
		start("chacha20-poly1305", GNUTLS_CIPHER_CHACHA20_POLY1305, 1);
	}
}

#endif
