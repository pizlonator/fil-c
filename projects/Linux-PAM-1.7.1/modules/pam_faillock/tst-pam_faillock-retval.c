/*
 * Check pam_faillock return values.
 */

#include "test_assert.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_faillock"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char config_filename[] = TEST_NAME ".conf";
static const char user_name[] = "root";
static struct pam_conv conv;

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;
	char cwd[PATH_MAX];

	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	ASSERT_NE(NULL, fp = fopen(config_filename, "w"));
	ASSERT_LT(0, fprintf(fp,
		"deny = 2\n"
		"unlock_time = 5\n"
		"root_unlock_time = 5\n"));
	ASSERT_EQ(0, fclose(fp));

	/* root has access */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			"auth required %s/../pam_permit/" LTDIR "pam_permit.so\n"
			"auth required %s/" LTDIR "%s.so authsucc even_deny_root dir=%s conf=%s\n"
			"account required %s/" LTDIR "%s.so dir=%s\n"
			"password required %s/" LTDIR "%s.so dir=%s\n"
			"session required %s/" LTDIR "%s.so dir=%s\n",
			cwd,
			cwd, MODULE_NAME, cwd, config_filename,
			cwd, MODULE_NAME, cwd,
			cwd, MODULE_NAME, cwd,
			cwd, MODULE_NAME, cwd));

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
	ASSERT_EQ(0, unlink(service_file));
	pamh = NULL;

	/* root tries to login 2 times without success*/
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			"auth requisite %s/" LTDIR "%s.so dir=%s preauth even_deny_root conf=%s\n"
			"auth [success=1 default=bad] %s/../pam_debug/" LTDIR "pam_debug.so auth=perm_denied cred=success\n"
			"auth [default=die] %s/" LTDIR "%s.so dir=%s authfail even_deny_root conf=%s\n"
			"auth sufficient %s/" LTDIR "%s.so dir=%s authsucc even_deny_root conf=%s\n",
			cwd, MODULE_NAME, cwd, config_filename,
			cwd,
			cwd, MODULE_NAME, cwd, config_filename,
			cwd, MODULE_NAME, cwd, config_filename));

	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;
	ASSERT_EQ(0, unlink(service_file));

	/* root is locked for 5 sec*/
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			"auth requisite %s/" LTDIR "%s.so dir=%s preauth even_deny_root conf=%s\n"
			"auth [success=1 default=bad] %s/../pam_debug/" LTDIR "pam_debug.so auth=success cred=success\n"
			"auth [default=die] %s/" LTDIR "%s.so dir=%s authfail even_deny_root conf=%s\n"
			"auth sufficient %s/" LTDIR "%s.so dir=%s authsucc even_deny_root conf=%s\n",
			cwd, MODULE_NAME, cwd, config_filename,
			cwd,
			cwd, MODULE_NAME, cwd, config_filename,
			cwd, MODULE_NAME, cwd, config_filename));

	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		pam_start_confdir(service_file, user_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_AUTH_ERR, pam_authenticate(pamh, 0));

	/* waiting at least 5 sec --> login is working again*/
	sleep(6);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));

	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	ASSERT_EQ(0, unlink(service_file));
	pamh = NULL;

	ASSERT_EQ(0,unlink(user_name));
	ASSERT_EQ(0,unlink(config_filename));

	return 0;
}
