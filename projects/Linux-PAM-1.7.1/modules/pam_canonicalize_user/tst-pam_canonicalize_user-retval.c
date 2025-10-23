/*
 * Check pam_canonicalize_user return values.
 *
 * Copyright (c) 2023 Dmitry V. Levin <ldv@strace.io>
 */

#include "test_assert.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_canonicalize_user"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static struct pam_conv null_conv;

static int
again_conv_func(int num_msg UNUSED, const struct pam_message **msg UNUSED,
		struct pam_response **resp UNUSED, void *appdata_ptr UNUSED)
{
	return PAM_CONV_AGAIN;
}

static struct pam_conv again_conv = { .conv = again_conv_func };

#ifdef HAVE_GETPWNAM_R

int
getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result)
{
	if (strcmp(name, "root") == 0 ||
	    strcmp(name, "ROOT") == 0)
		return getpwuid_r(0, pwd, buf, buflen, result);

	*result = NULL;
	return 0;
}

#else /* !HAVE_GETPWNAM_R */

struct passwd *
getpwnam(const char *name)
{
	if (strcmp(name, "root") == 0 ||
	    strcmp(name, "ROOT") == 0)
		return getpwuid(0);

	errno = 0;
	return NULL;
}

#endif /* !HAVE_GETPWNAM_R */

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;

	char cwd[PATH_MAX];
	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	struct passwd *pw;
	ASSERT_NE(NULL, (pw = getpwuid(0)));
	ASSERT_EQ(0, strcmp("root", pw->pw_name));

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
		  pam_start_confdir(service_file, ":", &null_conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_IGNORE -> PAM_PERM_DENIED */
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
		  pam_start_confdir(service_file, "root", &null_conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_IGNORE -> PAM_SUCCESS */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so\n"
			     "auth required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			     "account required %s/" LTDIR "%s.so\n"
			     "account required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			     "password required %s/" LTDIR "%s.so\n"
			     "password required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			     "session required %s/" LTDIR "%s.so\n"
			     "session required %s/../pam_permit/" LTDIR "pam_permit.so\n",
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root", &null_conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_INCOMPLETE */
	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, NULL, &again_conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_INCOMPLETE, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_ABORT, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_ABORT, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_ABORT, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_ABORT, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_ABORT, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_IGNORE -> PAM_SUCCESS, "ROOT" -> "root" */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so\n"
			     "auth required %s/../pam_succeed_if/" LTDIR "pam_succeed_if.so user = root\n"
			     "account required %s/" LTDIR "%s.so\n"
			     "account required %s/../pam_succeed_if/" LTDIR "pam_succeed_if.so user = root\n"
			     "password required %s/" LTDIR "%s.so\n"
			     "password required %s/../pam_succeed_if/" LTDIR "pam_succeed_if.so user = root\n"
			     "session required %s/" LTDIR "%s.so\n"
			     "session required %s/../pam_succeed_if/" LTDIR "pam_succeed_if.so user = root\n",
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd,
			     cwd, MODULE_NAME, cwd));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "ROOT", &null_conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(0, unlink(service_file));

	return 0;
}
