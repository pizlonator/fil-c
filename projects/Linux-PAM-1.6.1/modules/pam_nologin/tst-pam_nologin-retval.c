/*
 * Check pam_nologin return values.
 *
 * Copyright (c) 2020 Dmitry V. Levin <ldv@altlinux.org>
 */

#include "test_assert.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_nologin"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char missing_file[] = TEST_NAME ".missing";
static const char empty_file[] = "/dev/null";
static const char user_name[] = "";
static struct pam_conv conv;

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;
	struct passwd *pw;
	char cwd[PATH_MAX];

	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	/* PAM_IGNORE -> PAM_PERM_DENIED */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/.libs/%s.so file=%s\n"
			     "account required %s/.libs/%s.so file=%s\n"
			     "password required %s/.libs/%s.so file=%s\n"
			     "session required %s/.libs/%s.so file=%s\n",
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_IGNORE -> PAM_SUCCESS */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/.libs/%s.so file=%s\n"
			     "auth required %s/../pam_permit/.libs/pam_permit.so\n"
			     "account required %s/.libs/%s.so file=%s\n"
			     "account required %s/../pam_permit/.libs/pam_permit.so\n"
			     "password required %s/.libs/%s.so file=%s\n"
			     "password required %s/../pam_permit/.libs/pam_permit.so\n"
			     "session required %s/.libs/%s.so file=%s\n"
			     "session required %s/../pam_permit/.libs/pam_permit.so\n",
			     cwd, MODULE_NAME, missing_file, cwd,
			     cwd, MODULE_NAME, missing_file, cwd,
			     cwd, MODULE_NAME, missing_file, cwd,
			     cwd, MODULE_NAME, missing_file, cwd));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* successok -> PAM_SUCCESS */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/.libs/%s.so successok file=%s\n"
			     "account required %s/.libs/%s.so successok file=%s\n"
			     "password required %s/.libs/%s.so successok file=%s\n"
			     "session required %s/.libs/%s.so successok file=%s\n",
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file,
			     cwd, MODULE_NAME, missing_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_USER_UNKNOWN */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/.libs/%s.so file=%s\n"
			     "account required %s/.libs/%s.so file=%s\n"
			     "password required %s/.libs/%s.so file=%s\n"
			     "session required %s/.libs/%s.so file=%s\n",
			     cwd, MODULE_NAME, empty_file,
			     cwd, MODULE_NAME, empty_file,
			     cwd, MODULE_NAME, empty_file,
			     cwd, MODULE_NAME, empty_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* uid == 0 */
	if ((pw = getpwuid(0)) != NULL) {
		/* successok -> PAM_SUCCESS */
		ASSERT_NE(NULL, fp = fopen(service_file, "w"));
		ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
				     "auth required %s/.libs/%s.so successok file=%s\n"
				     "account required %s/.libs/%s.so successok file=%s\n"
				     "password required %s/.libs/%s.so successok file=%s\n"
				     "session required %s/.libs/%s.so successok file=%s\n",
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file));
		ASSERT_EQ(0, fclose(fp));

		ASSERT_EQ(PAM_SUCCESS,
			  pam_start_confdir(service_file, pw->pw_name,
					    &conv, ".", &pamh));
		ASSERT_NE(NULL, pamh);
		ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
		pamh = NULL;

		/* PAM_SYSTEM_ERR */
		ASSERT_NE(NULL, fp = fopen(service_file, "w"));
		ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
				     "auth required %s/.libs/%s.so file=%s\n"
				     "account required %s/.libs/%s.so file=%s\n"
				     "password required %s/.libs/%s.so file=%s\n"
				     "session required %s/.libs/%s.so file=%s\n",
				     cwd, MODULE_NAME, ".",
				     cwd, MODULE_NAME, ".",
				     cwd, MODULE_NAME, ".",
				     cwd, MODULE_NAME, "."));
		ASSERT_EQ(0, fclose(fp));

		ASSERT_EQ(PAM_SUCCESS,
			  pam_start_confdir(service_file, pw->pw_name,
					    &conv, ".", &pamh));
		ASSERT_NE(NULL, pamh);
		ASSERT_EQ(PAM_SYSTEM_ERR, pam_authenticate(pamh, 0));
		ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
		ASSERT_EQ(PAM_SYSTEM_ERR, pam_acct_mgmt(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
		pamh = NULL;
	}

	/* uid != 0 */
	if (geteuid() != 0 && (pw = getpwuid(geteuid())) != NULL) {
		/* PAM_AUTH_ERR */
		ASSERT_NE(NULL, fp = fopen(service_file, "w"));
		ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
				     "auth required %s/.libs/%s.so file=%s\n"
				     "account required %s/.libs/%s.so file=%s\n"
				     "password required %s/.libs/%s.so file=%s\n"
				     "session required %s/.libs/%s.so file=%s\n",
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file,
				     cwd, MODULE_NAME, empty_file));
		ASSERT_EQ(0, fclose(fp));

		ASSERT_EQ(PAM_SUCCESS,
			  pam_start_confdir(service_file, pw->pw_name,
					    &conv, ".", &pamh));
		ASSERT_NE(NULL, pamh);
		ASSERT_EQ(PAM_AUTH_ERR, pam_authenticate(pamh, 0));
		ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
		ASSERT_EQ(PAM_AUTH_ERR, pam_acct_mgmt(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
		pamh = NULL;
	}

	ASSERT_EQ(0, unlink(service_file));

	return 0;
}
