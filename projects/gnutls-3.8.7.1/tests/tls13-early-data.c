/*
 * Copyright (C) 2012-2018 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos, Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

int main(void)
{
	exit(77);
}

#else

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/dtls.h>
#include <signal.h>
#include <assert.h>

#include "cert-common.h"
#include "utils.h"
#include "virt-time.h"
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define TRACE_CLIENT 1
#define TRACE_SERVER 2

/* To reproduce the entries in {client,server}-secrets.h, set this to
 * either TRACE_CLIENT or TRACE_SERVER.
 */
#define TRACE 0

/* This program tests the robustness of record sending with padding.
 */

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

/* A very basic TLS client.
 */

#define MAX_BUF 1024
#define MSG "Hello TLS"
#define EARLY_MSG "Hello TLS, it's early"

extern unsigned int _gnutls_global_version;

/* This test makes connection 3 times with different ciphersuites:
 * first with TLS_AES_128_GCM_SHA256, then
 * TLS_CHACHA20_POLY1305_SHA256 two times.  The reason for doing this
 * is to check that the early data is encrypted with the ciphersuite
 * selected during the initial handshake, not the resuming handshakes.
 */
#define SESSIONS 3
#define TLS13_AES_128_GCM \
	"NONE:+VERS-TLS1.3:+AES-128-GCM:+AEAD:+SIGN-RSA-PSS-RSAE-SHA384:+GROUP-SECP256R1:%NO_SHUFFLE_EXTENSIONS"
#define TLS13_CHACHA20_POLY1305 \
	"NONE:+VERS-TLS1.3:+CHACHA20-POLY1305:+AEAD:+SIGN-RSA-PSS-RSAE-SHA384:+GROUP-SECP256R1:%NO_SHUFFLE_EXTENSIONS"

static const gnutls_datum_t hrnd = {
	(void *)"\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	32
};

static const gnutls_datum_t hsrnd = {
	(void *)"\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	32
};

static int gnutls_rnd_works;

int __attribute__((visibility("protected")))
gnutls_rnd(gnutls_rnd_level_t level, void *data, size_t len)
{
	gnutls_rnd_works = 1;

	memset(data, 0xff, len);

	/* Flip the first byte to avoid infinite loop in the RSA
	 * blinding code of Nettle */
	if (len > 0)
		memset(data, 0x0, 1);
	return 0;
}

#define MAX_SECRET_SIZE 64
#define MAX_SECRET_COUNT 10

struct secret {
	gnutls_record_encryption_level_t level;
	size_t secret_size;
	const uint8_t *secret_read;
	const uint8_t *secret_write;
	uint8_t secret_read_buf[MAX_SECRET_SIZE];
	uint8_t secret_write_buf[MAX_SECRET_SIZE];
};

#include "client-secrets.h"
#include "server-secrets.h"

struct secrets_expected {
	const struct secret *secrets;
	size_t count;
};

#define SIZEOF(array) (sizeof(array) / sizeof(array[0]))

static const struct secrets_expected client_normal[SESSIONS] = {
	{ client_normal_0, SIZEOF(client_normal_0) },
	{ client_normal_1, SIZEOF(client_normal_1) },
	{ client_normal_2, SIZEOF(client_normal_2) },
};

static const struct secrets_expected client_small[SESSIONS] = {
	{ client_small_0, SIZEOF(client_small_0) },
	{ client_small_1, SIZEOF(client_small_1) },
	{ client_small_2, SIZEOF(client_small_2) },
};

static const struct secrets_expected client_empty[SESSIONS] = {
	{ client_empty_0, SIZEOF(client_empty_0) },
	{ client_empty_1, SIZEOF(client_empty_1) },
	{ client_empty_2, SIZEOF(client_empty_2) },
};

static const struct secrets_expected client_explicit[SESSIONS] = {
	{ client_explicit_0, SIZEOF(client_explicit_0) },
	{ client_explicit_1, SIZEOF(client_explicit_1) },
	{ client_explicit_2, SIZEOF(client_explicit_2) },
};

static const struct secrets_expected server_normal[SESSIONS] = {
	{ server_normal_0, SIZEOF(server_normal_0) },
	{ server_normal_1, SIZEOF(server_normal_1) },
	{ server_normal_2, SIZEOF(server_normal_2) },
};

static const struct secrets_expected server_small[SESSIONS] = {
	{ server_small_0, SIZEOF(server_small_0) },
	{ server_small_1, SIZEOF(server_small_1) },
	{ server_small_2, SIZEOF(server_small_2) },
};

static const struct secrets_expected server_empty[SESSIONS] = {
	{ server_empty_0, SIZEOF(server_empty_0) },
	{ server_empty_1, SIZEOF(server_empty_1) },
	{ server_empty_2, SIZEOF(server_empty_2) },
};

static const struct secrets_expected server_explicit[SESSIONS] = {
	{ server_explicit_0, SIZEOF(server_explicit_0) },
	{ server_explicit_1, SIZEOF(server_explicit_1) },
	{ server_explicit_2, SIZEOF(server_explicit_2) },
};

struct fixture {
	const char *name;
	unsigned int cflags;
	unsigned int sflags;
	gnutls_datum_t early_data;
	size_t max_early_data_size;
	bool expect_early_data;
	const struct secrets_expected *client_secrets;
	const struct secrets_expected *server_secrets;
};

static const struct fixture fixtures[] = {
	{
		.name = "normal",
		.cflags = 0,
		.sflags = 0,
		.early_data = { (uint8_t *)EARLY_MSG, sizeof(EARLY_MSG) },
		.max_early_data_size = MAX_BUF,
		.expect_early_data = true,
		.client_secrets = client_normal,
		.server_secrets = server_normal,
	},
	{
		.name = "small",
		.cflags = 0,
		.sflags = 0,
		.early_data = { (uint8_t *)EARLY_MSG, sizeof(EARLY_MSG) },
		.max_early_data_size = 10,
		.expect_early_data = true,
		.client_secrets = client_small,
		.server_secrets = server_small,
	},
	{
		.name = "empty",
		.cflags = 0,
		.sflags = 0,
		.early_data = { NULL, 0 },
		.max_early_data_size = MAX_BUF,
		.expect_early_data = false,
		.client_secrets = client_empty,
		.server_secrets = server_empty,
	},
	{
		.name = "explicit",
		.cflags = GNUTLS_ENABLE_EARLY_DATA,
		.sflags = 0,
		.early_data = { NULL, 0 },
		.max_early_data_size = MAX_BUF,
		.expect_early_data = false,
		.client_secrets = client_explicit,
		.server_secrets = server_explicit,
	},
};

#if TRACE
static void print_secret(FILE *out, struct secret *secret)
{
	const char *level;

	switch (secret->level) {
	case GNUTLS_ENCRYPTION_LEVEL_INITIAL:
		level = "GNUTLS_ENCRYPTION_LEVEL_INITIAL";
		break;
	case GNUTLS_ENCRYPTION_LEVEL_EARLY:
		level = "GNUTLS_ENCRYPTION_LEVEL_EARLY";
		break;
	case GNUTLS_ENCRYPTION_LEVEL_HANDSHAKE:
		level = "GNUTLS_ENCRYPTION_LEVEL_HANDSHAKE";
		break;
	case GNUTLS_ENCRYPTION_LEVEL_APPLICATION:
		level = "GNUTLS_ENCRYPTION_LEVEL_APPLICATION";
		break;
	}

	fprintf(out, "\t\t%s,\n\t\t%zu,\n", level, secret->secret_size);
	if (secret->secret_read) {
		size_t i;

		fputs("\t\t(const uint8_t *)\"", out);
		for (i = 0; i < secret->secret_size; i++) {
			fprintf(out, "\\x%.2x", secret->secret_read[i]);
		}
		fputs("\",\n", out);
	} else {
		fputs("\t\tNULL,\n", out);
	}
	if (secret->secret_write) {
		size_t i;

		fputs("\t\t(const uint8_t *)\"", out);
		for (i = 0; i < secret->secret_size; i++) {
			fprintf(out, "\\x%.2x", secret->secret_write[i]);
		}
		fputs("\",\n", out);
	} else {
		fputs("\t\tNULL,\n", out);
	}
}

static void print_secrets(FILE *out, const char *side, const char *name, int t,
			  struct secret *secrets, size_t count)
{
	size_t i;

	fprintf(out, "static const struct secret %s_%s_%d[] = {\n", side, name,
		t);
	for (i = 0; i < count; i++) {
		fputs("\t{\n", out);
		print_secret(out, &secrets[i]);
		fputs("\t},\n", out);
	}
	fputs("};\n\n", out);
}
#endif

static void check_secrets(const struct secret *secrets, size_t count,
			  const struct secrets_expected *expected)
{
	size_t i;

	if (count != expected->count) {
		fail("unexpected number of secrets: %zu != %zu\n", count,
		     expected->count);
	}

	for (i = 0; i < count; i++) {
		if (secrets[i].level != expected->secrets[i].level) {
			fail("unexpected secret level: %d != %d\n",
			     secrets[i].level, expected->secrets[i].level);
		}
		if (secrets[i].secret_size !=
		    expected->secrets[i].secret_size) {
			fail("unexpected secret size: %zu != %zu\n",
			     secrets[i].secret_size,
			     expected->secrets[i].secret_size);
		}
		if ((secrets[i].secret_read == NULL) !=
		    (expected->secrets[i].secret_read == NULL)) {
			fail("unexpected secret for read: %p != %p\n",
			     secrets[i].secret_read,
			     expected->secrets[i].secret_read);
		}
		if (expected->secrets[i].secret_read &&
		    memcmp(secrets[i].secret_read,
			   expected->secrets[i].secret_read,
			   secrets[i].secret_size) != 0) {
			fail("unexpected secret for read\n");
		}
		if ((secrets[i].secret_write == NULL) !=
		    (expected->secrets[i].secret_write == NULL)) {
			fail("unexpected secret for write: %p != %p\n",
			     secrets[i].secret_write,
			     expected->secrets[i].secret_write);
		}
		if (expected->secrets[i].secret_write &&
		    memcmp(secrets[i].secret_write,
			   expected->secrets[i].secret_write,
			   secrets[i].secret_size) != 0) {
			fail("unexpected secret for write\n");
		}
	}
}

struct callback_data {
	int t;
	size_t secret_callback_called;
	struct secret secrets[MAX_SECRET_COUNT];
};

static int secret_callback(gnutls_session_t session,
			   gnutls_record_encryption_level_t level,
			   const void *secret_read, const void *secret_write,
			   size_t secret_size)
{
	struct callback_data *data = gnutls_session_get_ptr(session);
	struct secret *secret = &data->secrets[data->secret_callback_called];

	if (data->t == 0) {
		if (level == GNUTLS_ENCRYPTION_LEVEL_EARLY) {
			fail("early secret is set on initial connection\n");
		}
	} else {
		if (level == GNUTLS_ENCRYPTION_LEVEL_EARLY) {
			gnutls_cipher_algorithm_t cipher_algo;
			gnutls_digest_algorithm_t digest_algo;

			cipher_algo = gnutls_early_cipher_get(session);
			if (cipher_algo != GNUTLS_CIPHER_AES_128_GCM) {
				fail("unexpected cipher used for early data: %s != %s\n",
				     gnutls_cipher_get_name(cipher_algo),
				     gnutls_cipher_get_name(
					     GNUTLS_CIPHER_AES_128_GCM));
			}

			digest_algo = gnutls_early_prf_hash_get(session);
			if (digest_algo != GNUTLS_DIG_SHA256) {
				fail("unexpected PRF hash used for early data: %s != %s\n",
				     gnutls_digest_get_name(digest_algo),
				     gnutls_digest_get_name(GNUTLS_DIG_SHA256));
			}
		}
	}

	if (secret_size > MAX_SECRET_SIZE) {
		fail("secret is too long\n");
	}

	secret->secret_size = secret_size;
	secret->level = level;
	if (secret_read) {
		memcpy(secret->secret_read_buf, secret_read, secret_size);
		secret->secret_read = secret->secret_read_buf;
	}
	if (secret_write) {
		memcpy(secret->secret_write_buf, secret_write, secret_size);
		secret->secret_write = secret->secret_write_buf;
	}

	data->secret_callback_called++;
	if (data->secret_callback_called > MAX_SECRET_COUNT) {
		fail("secret func called too many times");
	}

	return 0;
}

static void client(int sds[], const struct fixture *fixture)
{
	int ret;
	char buffer[MAX_BUF + 1];
	gnutls_certificate_credentials_t x509_cred;
	gnutls_session_t session;
	int t;
	gnutls_datum_t session_data = { NULL, 0 };

	global_init();

	/* date --date='TZ="UTC" 2021-04-29' +%s */
	virt_time_init_at(1619654400);

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(7);
	}

	gnutls_certificate_allocate_credentials(&x509_cred);

	for (t = 0; t < SESSIONS; t++) {
		int sd = sds[t];
		struct callback_data callback_data;

		assert(gnutls_init(&session, GNUTLS_CLIENT | fixture->cflags) >=
		       0);
		assert(gnutls_priority_set_direct(
			       session,
			       t == 0 ? TLS13_AES_128_GCM :
					TLS13_CHACHA20_POLY1305,
			       NULL) >= 0);

		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				       x509_cred);

		gnutls_transport_set_int(session, sd);
		assert(gnutls_handshake_set_random(session, &hrnd) >= 0);

		memset(&callback_data, 0, sizeof(callback_data));
		callback_data.t = t;
		gnutls_session_set_ptr(session, &callback_data);
		gnutls_handshake_set_secret_function(session, secret_callback);

		if (t > 0) {
			assert(gnutls_session_set_data(session,
						       session_data.data,
						       session_data.size) >= 0);
			/* The server should have advertised the same maximum. */
			if (gnutls_record_get_max_early_data_size(session) !=
			    fixture->max_early_data_size)
				fail("client: max_early_data_size mismatch %d != %d\n",
				     (int)gnutls_record_get_max_early_data_size(
					     session),
				     (int)fixture->max_early_data_size);
			assert(gnutls_record_send_early_data(
				       session, fixture->early_data.data,
				       MIN(fixture->early_data.size,
					   fixture->max_early_data_size)) >= 0);
		}

		/* Perform the TLS handshake
		 */
		gnutls_handshake_set_timeout(session, get_timeout());
		do {
			ret = gnutls_handshake(session);
		} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

		if (ret < 0) {
			fail("client: Handshake failed: %s\n",
			     gnutls_strerror(ret));
		} else {
			if (debug)
				success("client: Handshake was completed\n");
		}

		if (!gnutls_rnd_works) {
			success("client: gnutls_rnd() could not be overridden\n");
		} else {
#if TRACE == TRACE_CLIENT
			print_secrets(stderr, "client", fixture->name, t,
				      callback_data.secrets,
				      callback_data.secret_callback_called);
#endif
			check_secrets(callback_data.secrets,
				      callback_data.secret_callback_called,
				      &fixture->client_secrets[t]);
		}

		ret = gnutls_cipher_get(session);
		if ((t == 0 && ret != GNUTLS_CIPHER_AES_128_GCM) ||
		    (t > 0 && ret != GNUTLS_CIPHER_CHACHA20_POLY1305)) {
			fail("negotiated unexpected cipher: %s\n",
			     gnutls_cipher_get_name(ret));
		}

		if (t == 0) {
			/* get the session data size */
			ret = gnutls_session_get_data2(session, &session_data);
			if (ret < 0)
				fail("client: Getting resume data failed\n");
		}

		if (t > 0) {
			if (!gnutls_session_is_resumed(session)) {
				fail("client: session_is_resumed error (%d)\n",
				     t);
			}
		}

		gnutls_record_send(session, MSG, strlen(MSG));

		do {
			ret = gnutls_record_recv(session, buffer,
						 sizeof(buffer));
		} while (ret == GNUTLS_E_AGAIN);
		if (ret == 0) {
			if (debug)
				success("client: Peer has closed the TLS connection\n");
			goto end;
		} else if (ret < 0) {
			fail("client: Error: %s\n", gnutls_strerror(ret));
		}

		gnutls_bye(session, GNUTLS_SHUT_WR);

		close(sd);

		gnutls_deinit(session);
	}

end:
	gnutls_free(session_data.data);
	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();
}

static pid_t child;

#define MAX_CLIENT_HELLO_RECORDED 10

struct storage_st {
	gnutls_datum_t entries[MAX_CLIENT_HELLO_RECORDED];
	size_t num_entries;
};

static int storage_add(void *ptr, time_t expires, const gnutls_datum_t *key,
		       const gnutls_datum_t *value)
{
	struct storage_st *storage = ptr;
	gnutls_datum_t *datum;
	size_t i;

	for (i = 0; i < storage->num_entries; i++) {
		if (key->size == storage->entries[i].size &&
		    memcmp(storage->entries[i].data, key->data, key->size) ==
			    0) {
			return GNUTLS_E_DB_ENTRY_EXISTS;
		}
	}

	/* If the maximum number of ClientHello exceeded, reject early
	 * data until next time.
	 */
	if (storage->num_entries == MAX_CLIENT_HELLO_RECORDED)
		return GNUTLS_E_DB_ERROR;

	datum = &storage->entries[storage->num_entries];
	datum->data = gnutls_malloc(key->size);
	if (!datum->data)
		return GNUTLS_E_MEMORY_ERROR;
	memcpy(datum->data, key->data, key->size);
	datum->size = key->size;

	storage->num_entries++;

	return 0;
}

static void storage_clear(struct storage_st *storage)
{
	size_t i;

	for (i = 0; i < storage->num_entries; i++)
		gnutls_free(storage->entries[i].data);
	storage->num_entries = 0;
}

static void server(int sds[], const struct fixture *fixture)
{
	int ret;
	char buffer[MAX_BUF + 1];
	gnutls_session_t session;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_datum_t session_ticket_key = { NULL, 0 };
	struct storage_st storage;
	gnutls_anti_replay_t anti_replay;
	int t;

	/* this must be called once in the program
	 */
	global_init();

	/* date --date='TZ="UTC" 2021-04-29' +%s */
	virt_time_init_at(1619654400);

	memset(buffer, 0, sizeof(buffer));
	memset(&storage, 0, sizeof(storage));

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_certificate_allocate_credentials(&x509_cred);
	gnutls_certificate_set_x509_key_mem(x509_cred, &server_cert,
					    &server_key, GNUTLS_X509_FMT_PEM);

	gnutls_session_ticket_key_generate(&session_ticket_key);

	ret = gnutls_anti_replay_init(&anti_replay);
	if (ret < 0)
		fail("server: failed to initialize anti-replay\n");

	gnutls_anti_replay_set_add_function(anti_replay, storage_add);
	gnutls_anti_replay_set_ptr(anti_replay, &storage);

	for (t = 0; t < SESSIONS; t++) {
		int sd = sds[t];
		struct callback_data callback_data;

		assert(gnutls_init(&session,
				   GNUTLS_SERVER | GNUTLS_ENABLE_EARLY_DATA) >=
		       0);

		assert(gnutls_priority_set_direct(
			       session,
			       t == 0 ? TLS13_AES_128_GCM :
					TLS13_CHACHA20_POLY1305,
			       NULL) >= 0);

		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				       x509_cred);

		gnutls_session_ticket_enable_server(session,
						    &session_ticket_key);

		gnutls_anti_replay_enable(session, anti_replay);

		/* on the replay connection, early data is skipped
		 * until max_early_data_size without decryption
		 */
		if (t < 2)
			(void)gnutls_record_set_max_early_data_size(
				session, fixture->max_early_data_size);

		assert(gnutls_handshake_set_random(session, &hsrnd) >= 0);
		gnutls_transport_set_int(session, sd);

		memset(&callback_data, 0, sizeof(callback_data));
		callback_data.t = t;
		gnutls_session_set_ptr(session, &callback_data);
		gnutls_handshake_set_secret_function(session, secret_callback);

		do {
			ret = gnutls_handshake(session);
		} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
		if (ret < 0) {
			close(sd);
			gnutls_deinit(session);
			fail("server: Handshake has failed (%s)\n\n",
			     gnutls_strerror(ret));
		}
		if (debug)
			success("server: Handshake was completed\n");

		if (t > 0) {
			if (!gnutls_session_is_resumed(session)) {
				fail("server: session_is_resumed error (%d)\n",
				     t);
			}
		}

		if (!gnutls_rnd_works) {
			success("server: gnutls_rnd() could not be overridden\n");
			goto skip_early_data;
		}

		ret = gnutls_cipher_get(session);
		if ((t == 0 && ret != GNUTLS_CIPHER_AES_128_GCM) ||
		    (t > 0 && ret != GNUTLS_CIPHER_CHACHA20_POLY1305)) {
			fail("negotiated unexpected cipher: %s\n",
			     gnutls_cipher_get_name(ret));
		}
#if TRACE == TRACE_SERVER
		print_secrets(stderr, "server", fixture->name, t,
			      callback_data.secrets,
			      callback_data.secret_callback_called);
#endif
		check_secrets(callback_data.secrets,
			      callback_data.secret_callback_called,
			      &fixture->server_secrets[t]);

		/* as we reuse the same ticket twice, expect
		 * early data only on the first resumption */
		if (t == 1) {
			if (fixture->expect_early_data &&
			    !(gnutls_session_get_flags(session) &
			      GNUTLS_SFLAGS_EARLY_DATA)) {
				fail("server: early data is not received (%d)\n",
				     t);
			}

			ret = gnutls_record_recv_early_data(session, buffer,
							    sizeof(buffer));
			if (ret < 0) {
				if (fixture->early_data.size == 0 ||
				    fixture->max_early_data_size == 0) {
					if (ret !=
					    GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
						fail("server: unexpected error code when retrieving empty early data: %s\n",
						     gnutls_strerror(ret));
					}
				} else {
					fail("server: failed to retrieve early data: %s\n",
					     gnutls_strerror(ret));
				}
			} else {
				if (fixture->early_data.size == 0 ||
				    fixture->max_early_data_size == 0) {
					fail("server: unexpected early data received: %d\n",
					     ret);
				} else if ((size_t)ret !=
						   MIN(fixture->early_data.size,
						       fixture->max_early_data_size) ||
					   memcmp(buffer,
						  fixture->early_data.data,
						  ret)) {
					fail("server: early data mismatch\n");
				}
			}
		} else if (t == 2) {
			if (fixture->expect_early_data &&
			    gnutls_session_get_flags(session) &
				    GNUTLS_SFLAGS_EARLY_DATA) {
				fail("server: early data is not rejected (%d)\n",
				     t);
			}
		}

	skip_early_data:
		/* see the Getting peer's information example */
		/* print_info(session); */

		for (;;) {
			memset(buffer, 0, MAX_BUF + 1);
			ret = gnutls_record_recv(session, buffer, MAX_BUF);

			if (ret == 0) {
				if (debug)
					success("server: Peer has closed the GnuTLS connection\n");
				break;
			} else if (ret < 0) {
				kill(child, SIGTERM);
				fail("server: Error: %s\n",
				     gnutls_strerror(ret));
			} else if (ret > 0) {
				/* echo data back to the client
				 */
				gnutls_record_send(session, buffer,
						   strlen(buffer));
			}
		}
		/* do not wait for the peer to close the connection.
		 */
		gnutls_bye(session, GNUTLS_SHUT_WR);

		close(sd);

		gnutls_deinit(session);
	}

	gnutls_anti_replay_deinit(anti_replay);

	storage_clear(&storage);

	gnutls_free(session_ticket_key.data);

	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();

	if (debug)
		success("server: finished\n");
}

static void start(const struct fixture *fixture)
{
	int client_sds[SESSIONS], server_sds[SESSIONS];
	int i;
	int ret;

	_gnutls_global_version = 0x030607;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	for (i = 0; i < SESSIONS; i++) {
		int sockets[2];

		ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
		if (ret < 0) {
			perror("socketpair");
			exit(1);
		}

		server_sds[i] = sockets[0];
		client_sds[i] = sockets[1];
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
	}

	if (child) {
		/* parent */
		for (i = 0; i < SESSIONS; i++)
			close(client_sds[i]);
		server(server_sds, fixture);
		kill(child, SIGTERM);
	} else {
		for (i = 0; i < SESSIONS; i++)
			close(server_sds[i]);
		client(client_sds, fixture);
		exit(0);
	}
}

void doit(void)
{
	size_t i;

	/* TLS_CHACHA20_POLY1305_SHA256 is needed for this test */
	if (gnutls_fips140_mode_enabled()) {
		exit(77);
	}

	for (i = 0; i < SIZEOF(fixtures); i++) {
		start(&fixtures[i]);
	}

	if (!gnutls_rnd_works) {
		exit(77);
	}
}

#endif /* _WIN32 */
