/*
 * Check pam_listfile return values.
 *
 * Copyright (c) 2023 Dmitry V. Levin <ldv@strace.io>
 */

#include "test_assert.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>

#define MODULE_NAME "pam_listfile"
#define TEST_NAME "tst-" MODULE_NAME "-retval"

static const char service_file[] = TEST_NAME ".service";
static const char list_file[] = TEST_NAME ".list";
static struct pam_conv conv;

int
main(void)
{
	pam_handle_t *pamh = NULL;
	FILE *fp;

	char cwd[PATH_MAX];
	ASSERT_NE(NULL, getcwd(cwd, sizeof(cwd)));

	struct passwd *pw;
	ASSERT_NE(NULL, (pw = getpwuid(geteuid())));

	struct group *gr;
	ASSERT_NE(NULL, (gr = getgrgid(getegid())));

	/* invalid onerr= specified */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr\n"
			     "account required %s/" LTDIR "%s.so onerr=\n"
			     "password required %s/" LTDIR "%s.so onerr=0\n"
			     "session required %s/" LTDIR "%s.so onerr=1\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* unknown option, implicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so one=1\n"
			     "account required %s/" LTDIR "%s.so two=2\n"
			     "password required %s/" LTDIR "%s.so three=3\n"
			     "session required %s/" LTDIR "%s.so four=4\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* unknown option, onerr=succeed after unknown option */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so one=1 onerr=succeed\n"
			     "account required %s/" LTDIR "%s.so two=2 onerr=succeed\n"
			     "password required %s/" LTDIR "%s.so three=3 onerr=succeed\n"
			     "session required %s/" LTDIR "%s.so four=4 onerr=succeed\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* unknown option, onerr=succeed before unknown option */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=succeed one=1\n"
			     "account required %s/" LTDIR "%s.so onerr=succeed two=2\n"
			     "password required %s/" LTDIR "%s.so onerr=succeed three=3\n"
			     "session required %s/" LTDIR "%s.so onerr=succeed four=4\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no item= or invalid item= specified, implicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so\n"
			     "account required %s/" LTDIR "%s.so item\n"
			     "password required %s/" LTDIR "%s.so item=\n"
			     "session required %s/" LTDIR "%s.so item=bad\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no item= or invalid item= specified, explicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=fail\n"
			     "account required %s/" LTDIR "%s.so onerr=fail item\n"
			     "password required %s/" LTDIR "%s.so onerr=fail item=\n"
			     "session required %s/" LTDIR "%s.so onerr=fail item=bad\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no item= or invalid item= specified, onerr=succeed */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=succeed\n"
			     "account required %s/" LTDIR "%s.so onerr=succeed item\n"
			     "password required %s/" LTDIR "%s.so onerr=succeed item=\n"
			     "session required %s/" LTDIR "%s.so onerr=succeed item=bad\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no file= specified, implicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user\n"
			     "account required %s/" LTDIR "%s.so item=group\n"
			     "password required %s/" LTDIR "%s.so item=ruser\n"
			     "session required %s/" LTDIR "%s.so item=rhost\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no file= specified, onerr=succeed */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=succeed item=user\n"
			     "account required %s/" LTDIR "%s.so onerr=succeed item=group\n"
			     "password required %s/" LTDIR "%s.so onerr=succeed item=ruser\n"
			     "session required %s/" LTDIR "%s.so onerr=succeed item=rhost\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no sense= or invalid sense= specified, implicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file=.\n"
			     "account required %s/" LTDIR "%s.so item=group file=. sense\n"
			     "password required %s/" LTDIR "%s.so item=shell file=. sense=\n"
			     "session required %s/" LTDIR "%s.so item=tty file=. sense=bad\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* no sense= or invalid sense= specified, onerr=succeed */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=succeed item=user file=.\n"
			     "account required %s/" LTDIR "%s.so onerr=succeed item=group file=. sense\n"
			     "password required %s/" LTDIR "%s.so onerr=succeed item=shell file=. sense=\n"
			     "session required %s/" LTDIR "%s.so onerr=succeed item=tty file=. sense=bad\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* invalid apply= specified, implicit onerr=fail */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=ruser file=. sense=allow apply=\n"
			     "account required %s/" LTDIR "%s.so item=rhost file=. sense=allow apply=\n"
			     "password required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=@\n"
			     "session required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SERVICE_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* invalid apply= specified, onerr=succeed */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so onerr=succeed item=ruser file=. sense=allow apply=\n"
			     "account required %s/" LTDIR "%s.so onerr=succeed item=rhost file=. sense=allow apply=\n"
			     "password required %s/" LTDIR "%s.so onerr=succeed item=tty file=. sense=allow apply=@\n"
			     "session required %s/" LTDIR "%s.so onerr=succeed item=tty file=. sense=allow apply=\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, "", &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* PAM_IGNORE -> PAM_PERM_DENIED */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=rhost file=. sense=allow apply=:\n"
			     "account required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=@:\n"
			     "password required %s/" LTDIR "%s.so item=rhost file=. sense=allow apply=:\n"
			     "session required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=@:\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_PERM_DENIED, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item not set, sense=allow */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=rhost file=. sense=allow apply=%s\n"
			     "account required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=%s\n"
			     "password required %s/" LTDIR "%s.so item=rhost file=. sense=allow apply=@%s\n"
			     "session required %s/" LTDIR "%s.so item=tty file=. sense=allow apply=@%s\n",
			     cwd, MODULE_NAME, pw->pw_name,
			     cwd, MODULE_NAME, pw->pw_name,
			     cwd, MODULE_NAME, gr->gr_name,
			     cwd, MODULE_NAME, gr->gr_name));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_AUTH_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item not set, sense=deny */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=ruser file=. sense=deny\n"
			     "account required %s/" LTDIR "%s.so item=rhost file=. sense=deny\n"
			     "password required %s/" LTDIR "%s.so item=tty file=. sense=deny\n"
			     "session required %s/" LTDIR "%s.so item=ruser file=. sense=deny\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* file does not exist, not a regular file, or world writable */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file= sense=allow\n"
			     "account required %s/" LTDIR "%s.so item=user file=. sense=allow onerr=succeed\n"
			     "password required %s/" LTDIR "%s.so item=user file=/ sense=allow onerr=succeed\n"
			     "session required %s/" LTDIR "%s.so item=user file=/dev/null sense=allow onerr=succeed\n",
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME,
			     cwd, MODULE_NAME));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SERVICE_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item is listed, sense=allow */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "account required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "password required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "session required %s/" LTDIR "%s.so item=user file=%s sense=allow\n",
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_NE(NULL, fp = fopen(list_file, "w"));
	ASSERT_LT(0, fprintf(fp, "%s\n", pw->pw_name));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item is listed, sense=deny */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "account required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "password required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "session required %s/" LTDIR "%s.so item=user file=%s sense=deny\n",
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_NE(NULL, fp = fopen(list_file, "w"));
	ASSERT_LT(0, fprintf(fp, "%s\n", pw->pw_name));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_AUTH_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item is not listed, sense=allow */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "account required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "password required %s/" LTDIR "%s.so item=user file=%s sense=allow\n"
			     "session required %s/" LTDIR "%s.so item=user file=%s sense=allow\n",
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_NE(NULL, fp = fopen(list_file, "w"));
	ASSERT_LT(0, fprintf(fp, ":\n"));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_AUTH_ERR, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_PERM_DENIED, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_AUTH_ERR, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* item is not listed, sense=deny */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "auth required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "account required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "password required %s/" LTDIR "%s.so item=user file=%s sense=deny\n"
			     "session required %s/" LTDIR "%s.so item=user file=%s sense=deny\n",
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_NE(NULL, fp = fopen(list_file, "w"));
	ASSERT_LT(0, fprintf(fp, ":\n"));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	/* Perform a test dedicated to configuration file parsing. */
	ASSERT_NE(NULL, fp = fopen(service_file, "w"));
	ASSERT_LT(0, fprintf(fp, "#%%PAM-1.0\n"
			     "# support line continuations, ignore escaped newlines in comments \\\n"
			     "auth required \\\n"
			     "     %s/" LTDIR "%s.so \\  \n"
			     "     item=user \\\t\t\n"
			     "     file=%s \\ \t \t\n"
			     "     sense=deny\n"
			     "\t \t # allow unneeded whitespace, ignore escaped newlines in comments \\ \n"
			     "   account\t required  %s/" LTDIR "%s.so item=user file=%s sense=deny%c\\\n"
			     "line after NUL byte continues up to here\n"
			     "# trim trailing comments, ignore escaped newlines in comments \\\t\n"
			     "password required %s/" LTDIR "%s.so item=user file=%s sense=deny # foo=bar\n"
			     "# support %*s long lines\n"
			     "session required %*s/" LTDIR "%s.so item=user file=%s sense=deny",
			     cwd, MODULE_NAME, list_file,
			     cwd, MODULE_NAME, list_file, '\0',
			     cwd, MODULE_NAME, list_file,
			     8192, " ",
			     65536, cwd, MODULE_NAME, list_file));
	ASSERT_EQ(0, fclose(fp));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_start_confdir(service_file, pw->pw_name, &conv, ".", &pamh));
	ASSERT_NE(NULL, pamh);
	ASSERT_EQ(PAM_SUCCESS, pam_authenticate(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_setcred(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_acct_mgmt(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_chauthtok(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_open_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_close_session(pamh, 0));
	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 0));
	pamh = NULL;

	ASSERT_EQ(0, unlink(list_file));
	ASSERT_EQ(0, unlink(service_file));

	return 0;
}
