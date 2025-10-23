/*
 * Check pam_sepermit return values and conf= option.
 *
 * Copyright (c) 2020-2022 Dmitry V. Levin <ldv@altlinux.org>
 */

#include "test_assert.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_sepermit"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char missing_file[] = TEST_NAME ".missing";
static const char config_file[] = TEST_NAME ".conf";
static struct pam_conv conv;

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;
	char cwd[PATH_MAX];

	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	/* PAM_USER_UNKNOWN */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0,
		  fprintf(fp, "#%%PAM-1.0\n"
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
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_USER_UNKNOWN, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_NE(NULL, fp = fopen(config_file, "w"));
	ASSERT_LT(0, fprintf(fp, "nosuchuser:ignore\n"));
	ASSERT_EQ(0, fclose(fp));

	/*
	 * conf= specifies an existing file,
	 * PAM_IGNORE -> PAM_PERM_DENIED
	 */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0,
		  fprintf(fp, "#%%PAM-1.0\n"
			      "auth required %s/" LTDIR "%s.so conf=%s\n"
			      "account required %s/" LTDIR "%s.so conf=%s\n"
			      "password required %s/" LTDIR "%s.so conf=%s\n"
			      "session required %s/" LTDIR "%s.so conf=%s\n",
			  cwd, MODULE_NAME, config_file,
			  cwd, MODULE_NAME, config_file,
			  cwd, MODULE_NAME, config_file,
			  cwd, MODULE_NAME, config_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/*
	 * conf= specifies an existing file,
	 * PAM_IGNORE -> PAM_SUCCESS
	 */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0,
		  fprintf(fp, "#%%PAM-1.0\n"
			      "auth required %s/" LTDIR "%s.so conf=%s\n"
			      "auth required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			      "account required %s/" LTDIR "%s.so conf=%s\n"
			      "account required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			      "password required %s/" LTDIR "%s.so conf=%s\n"
			      "password required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			      "session required %s/" LTDIR "%s.so conf=%s\n"
			      "session required %s/../pam_permit/" LTDIR "pam_permit.so\n",
			  cwd, MODULE_NAME, config_file, cwd,
			  cwd, MODULE_NAME, config_file, cwd,
			  cwd, MODULE_NAME, config_file, cwd,
			  cwd, MODULE_NAME, config_file, cwd));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/*
	 * conf= specifies a missing file,
	 * PAM_IGNORE -> PAM_PERM_DENIED
	 */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0,
		  fprintf(fp, "#%%PAM-1.0\n"
			      "auth required %s/" LTDIR "%s.so conf=%s\n"
			      "account required %s/" LTDIR "%s.so conf=%s\n"
			      "password required %s/" LTDIR "%s.so conf=%s\n"
			      "session required %s/" LTDIR "%s.so conf=%s\n",
			  cwd, MODULE_NAME, missing_file,
			  cwd, MODULE_NAME, missing_file,
			  cwd, MODULE_NAME, missing_file,
			  cwd, MODULE_NAME, missing_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "root", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_MODULE_UNKNOWN, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* cleanup */
	ASSERT_EQ(0, unlink(config_file));
	ASSERT_EQ(0, unlink(service_file));

	return 0;
}
