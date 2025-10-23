/*
 * Check pam_localuser return values.
 *
 * Copyright (c) 2020 Dmitry V. Levin <ldv@altlinux.org>
 */

#include "test_assert.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_localuser"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char passwd_file[] = TEST_NAME ".passwd";
static const char missing_file[] = TEST_NAME ".missing";

static const char alice_line[] = "alice:x:1001:1001:Alice:/home/alice:";
static const char bob_line[] = "bob:x:1002:1002:Bob:/home/bob:";
static const char craig_prefix[] = ":x:1003:1003:";
static const char craig_suffix[] = "craig:/home/craig:";

int
main(void)
{
	static struct pam_conv conv;
	pam_handle_t *pamh = NULL;
	FILE *fp;
	char cwd[PATH_MAX];
	char name[BUFSIZ];

	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	/* default passwd */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so\n",
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root:x", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* missing passwd file */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so file=%s\n",
			     cwd, MODULE_NAME, missing_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* custom passwd file */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so file=%s\n",
			     cwd, MODULE_NAME, passwd_file));
	ASSERT_EQ(0, fclose(fp));

	memcpy(name + (sizeof(name) - sizeof(craig_prefix)),
	       craig_prefix, sizeof(craig_prefix));
	ASSERT_NE(NULL, fp = fopen(passwd_file, "w"));
	ASSERT_LT(0, fprintf(fp, "%s\n%s\n%s%s\n",
			     alice_line, bob_line, name, craig_suffix));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	memset(name, 'x', sizeof(name) - 1);
	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "alice", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "bob", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "alice:x", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "craig", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(0, unlink(service_file));
	ASSERT_EQ(0, unlink(passwd_file));

	return 0;
}
