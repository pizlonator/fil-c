/*
 * Check pam_mkhomedir return values.
 *
 * Copyright (c) 2020 Dmitry V. Levin <ldv@altlinux.org>
 */

#include "test_assert.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_mkhomedir"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char user_empty[] = "";
static const char user_missing[] = ":";
static struct pam_conv conv;

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;
	struct passwd *pw;
	struct stat st;
	char cwd[PATH_MAX];

	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	/* PAM_USER_UNKNOWN */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so\n"
			     "account required %s/" LTDIR "%s.so\n"
			     "password required %s/" LTDIR "%s.so\n"
			     "session required %s/" LTDIR "%s.so\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_empty,
				    &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, user_missing,
				    &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_SUCCESS */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so debug\n"
			     "account required %s/" LTDIR "%s.so debug\n"
			     "password required %s/" LTDIR "%s.so debug\n"
			     "session required %s/" LTDIR "%s.so debug\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	if ((pw = getpwuid(geteuid())) != NULL &&
	    pw->pw_dir != NULL &&
	    stat(pw->pw_dir, &st) == 0 &&
	    (st.st_mode & S_IFMT) == S_IFDIR) {
		ASSERT_EQ(PAM_SUCCESS,
			  pam_start_confdir(service_file, pw->pw_name,
					    &conv, ".", &pamh));
		ASSERT_NE(NULL, pamh);
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_authenticate(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_setcred(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
		ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
		ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
		pamh = NULL;
	}

	ASSERT_EQ(0, unlink(service_file));

	return 0;
}
