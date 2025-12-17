/*
 * Copyright (C) 2004-2016 Free Software Foundation, Inc.
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * Author: Simon Josefsson, Nikos Mavrogiannopoulos
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

#ifndef GNUTLS_TESTS_UTILS_H
#define GNUTLS_TESTS_UTILS_H

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/pkcs11.h>

#ifndef __attribute__
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#define __attribute__(Spec) /* empty */
#endif
#endif

#ifdef NDEBUG
#error tests cannot be compiled with NDEBUG defined
#endif

#ifndef FALLTHROUGH
#if _GNUTLS_GCC_VERSION >= 70100
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif
#endif

/* number of elements within an array */
#define countof(a) (sizeof(a) / sizeof(*(a)))

inline static int global_init(void)
{
#ifdef ENABLE_PKCS11
	gnutls_pkcs11_init(GNUTLS_PKCS11_FLAG_MANUAL, NULL);
#endif
	return gnutls_global_init();
}

extern int debug;
extern int error_count;
extern int break_on_error;

extern const char *pkcs3;
extern const char *pkcs3_2048;
extern const char *pkcs3_3072;

#define fail(format, ...) \
	_fail("%s:%d: " format, __func__, __LINE__, ##__VA_ARGS__)

extern void _fail(const char *format, ...) __attribute__((format(printf, 1, 2)))
__attribute__((__noreturn__));
extern void fail_ignore(const char *format, ...)
	__attribute__((format(printf, 1, 2))) __attribute__((__noreturn__));
extern void success(const char *format, ...)
	__attribute__((format(printf, 1, 2)));

/* assumes test_name is defined */
#define test_fail(fmt, ...) fail("%s: " fmt, test_name, ##__VA_ARGS__)

#define test_success(fmt, ...) success("%s: " fmt, test_name, ##__VA_ARGS__)

extern void c_print(const unsigned char *str, size_t len);
extern void escapeprint(const char *str, size_t len);
extern void hexprint(const void *str, size_t len);
extern void binprint(const void *str, size_t len);
int disable_system_calls(void);
void sec_sleep(int sec);

int test_cli_serv_anon(gnutls_anon_server_credentials_t server_cred,
		       gnutls_anon_client_credentials_t client_cred,
		       const char *prio);

int test_cli_serv_psk(gnutls_psk_server_credentials_t server_cred,
		      gnutls_psk_client_credentials_t client_cred,
		      const char *prio);

typedef void callback_func(gnutls_session_t, void *priv);
void test_cli_serv(gnutls_certificate_credentials_t server_cred,
		   gnutls_certificate_credentials_t client_cred,
		   const char *prio, const char *host, void *priv,
		   callback_func *client_cb, callback_func *server_cb);

int _test_cli_serv(gnutls_certificate_credentials_t server_cred,
		   gnutls_certificate_credentials_t client_cred,
		   const char *serv_prio, const char *cli_prio,
		   const char *host, void *priv, callback_func *client_cb,
		   callback_func *server_cb,
		   unsigned expect_verification_failure, unsigned require_cert,
		   int serv_err, int cli_err);

void print_dh_params_info(gnutls_session_t);

void test_cli_serv_cert(gnutls_certificate_credentials_t server_cred,
			gnutls_certificate_credentials_t client_cred,
			const char *serv_prio, const char *cli_prio,
			const char *host);

void test_cli_serv_expect(gnutls_certificate_credentials_t server_cred,
			  gnutls_certificate_credentials_t client_cred,
			  const char *serv_prio, const char *cli_prio,
			  const char *host, int serv_err, int cli_err);

/* verification failed */
unsigned test_cli_serv_vf(gnutls_certificate_credentials_t server_cred,
			  gnutls_certificate_credentials_t client_cred,
			  const char *prio, const char *host);

#define TMPNAME_SIZE 128
char *get_tmpname(char s[TMPNAME_SIZE]);
void track_temp_files(void);
void delete_temp_files(void);

int tcp_connect(const char *addr, unsigned port);

/* This must be implemented elsewhere. */
extern void doit(void);

/* calls fail() if status indicates an error  */
inline static void _check_wait_status(int status, unsigned sigonly)
{
#if defined WEXITSTATUS && defined WIFSIGNALED
	if (WEXITSTATUS(status) != 0 ||
	    (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM)) {
		if (WIFSIGNALED(status)) {
			fail("Child died with signal %d\n", WTERMSIG(status));
		} else {
			if (!sigonly) {
				if (WEXITSTATUS(status) == 77)
					_exit(77);
				fail("Child died with status %d\n",
				     WEXITSTATUS(status));
			}
		}
	}
#endif
}

inline static void check_wait_status(int status)
{
	_check_wait_status(status, 0);
}

inline static void check_wait_status_for_sig(int status)
{
	_check_wait_status(status, 1);
}

inline static unsigned int get_timeout(void)
{
	const char *envvar;
	unsigned long int ul;

	envvar = getenv("GNUTLS_TEST_TIMEOUT");
	if (!envvar || *envvar == '\0')
		return 20 * 1000;

	ul = strtoul(envvar, NULL, 10);
	assert(ul <= UINT_MAX);

	return (unsigned int)ul;
}

inline static unsigned int get_dtls_retransmit_timeout(void)
{
	const char *envvar;
	unsigned long int ul;

	envvar = getenv("GNUTLS_TEST_DTLS_RETRANSMIT_TIMEOUT");
	if (!envvar || *envvar == '\0')
		return get_timeout() / 10;

	ul = strtoul(envvar, NULL, 10);
	assert(ul <= UINT_MAX);

	return (unsigned int)ul;
}

static inline const char *
fips_operation_state_to_string(gnutls_fips140_operation_state_t state)
{
	switch (state) {
	case GNUTLS_FIPS140_OP_INITIAL:
		return "INITIAL";
	case GNUTLS_FIPS140_OP_APPROVED:
		return "APPROVED";
	case GNUTLS_FIPS140_OP_NOT_APPROVED:
		return "NOT_APPROVED";
	case GNUTLS_FIPS140_OP_ERROR:
		return "ERROR";
	default:
		/*NOTREACHED*/ assert(0);
		return NULL;
	}
}

static inline void fips_push_context(gnutls_fips140_context_t context)
{
	if (gnutls_fips140_mode_enabled()) {
		int ret;

		ret = gnutls_fips140_push_context(context);
		if (ret < 0) {
			fail("gnutls_fips140_push_context failed\n");
		}
	}
}

static inline void
fips_pop_context(gnutls_fips140_context_t context,
		 gnutls_fips140_operation_state_t expected_state)
{
	gnutls_fips140_operation_state_t state;

	if (gnutls_fips140_mode_enabled()) {
		int ret;

		ret = gnutls_fips140_pop_context();
		if (ret < 0) {
			fail("gnutls_fips140_context_pop failed\n");
		}
		state = gnutls_fips140_get_operation_state(context);
		if (state != expected_state) {
			fail("operation state is not %s (%s)\n",
			     fips_operation_state_to_string(expected_state),
			     fips_operation_state_to_string(state));
		}
	}
}

/* To use those convenient macros, define fips_context variable. */
#define FIPS_PUSH_CONTEXT() fips_push_context(fips_context)
#define FIPS_POP_CONTEXT(state) \
	fips_pop_context(fips_context, GNUTLS_FIPS140_OP_##state)

#endif /* GNUTLS_TESTS_UTILS_H */
