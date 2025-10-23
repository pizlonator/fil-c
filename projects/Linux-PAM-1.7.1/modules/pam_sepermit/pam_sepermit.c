/******************************************************************************
 * A module for Linux-PAM that allows/denies access based on SELinux state.
 *
 * Copyright (c) 2007, 2008, 2009 Red Hat, Inc.
 * Originally written by Tomas Mraz <tmraz@redhat.com>
 * Contributions by Dan Walsh <dwalsh@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#include <selinux/selinux.h>

#include "pam_inline.h"

#define SEPERMIT_CONF_FILE	(SCONFIG_DIR "/sepermit.conf")
#ifdef VENDOR_SCONFIG_DIR
# define SEPERMIT_VENDOR_CONF_FILE	(VENDOR_SCONFIG_DIR "/sepermit.conf");
#endif
#define MODULE "pam_sepermit"
#define OPT_DELIM ":"

struct lockfd {
	uid_t uid;
	int fd;
        int debug;
};

#define PROC_BASE "/proc"
#define MAX_NAMES (int)(sizeof(unsigned long)*8)

static int
match_process_uid(pid_t pid, uid_t uid)
{
	char *buf;
	size_t n;
	uid_t puid;
	FILE *f;
	int re = 0;

	if ((buf = pam_asprintf(PROC_BASE "/%d/status", pid)) == NULL)
		return 0;
	n = strlen(buf) + 1;
	if (!(f = fopen (buf, "r"))) {
		free(buf);
		return 0;
	}

	while (getline(&buf, &n, f) != -1) {
		if (sscanf (buf, "Uid:\t%d", &puid)) {
			re = uid == puid;
			break;
		}
	}
	free(buf);
	fclose(f);
	return re;
}

static int
check_running (pam_handle_t *pamh, uid_t uid, int killall, int debug)
{
	DIR *dir;
	struct dirent *de;
	pid_t *pid_table, pid, self;
	int i;
	int pids, max_pids;
	int running = 0;
	self = getpid();
	if (!(dir = opendir(PROC_BASE))) {
		pam_syslog(pamh, LOG_ERR, "Failed to open proc directory file %s:", PROC_BASE);
		return -1;
	}
	max_pids = 256;
	pid_table = malloc(max_pids * sizeof (pid_t));
	if (!pid_table) {
		(void)closedir(dir);
		pam_syslog(pamh, LOG_CRIT, "Memory allocation error");
		return -1;
	}
	pids = 0;
	while ((de = readdir (dir)) != NULL) {
		if (!(pid = (pid_t)atoi(de->d_name)) || pid == self)
			continue;

		if (pids == max_pids) {
			pid_t *npt;

			if (!(npt = realloc(pid_table, 2*pids*sizeof(pid_t)))) {
				free(pid_table);
				(void)closedir(dir);
				pam_syslog(pamh, LOG_CRIT, "Memory allocation error");
				return -1;
			}
			pid_table = npt;
			max_pids *= 2;
		}
		pid_table[pids++] = pid;
	}

	(void)closedir(dir);

	for (i = 0; i < pids; i++) {
		pid_t id;

		if (match_process_uid(pid_table[i], uid) == 0)
			continue;
		id = pid_table[i];

		if (killall) {
			if (debug)
				pam_syslog(pamh, LOG_NOTICE, "Attempting to kill %d", id);
			kill(id, SIGKILL);
		}
		running++;
	}

	free(pid_table);
	return running;
}

/*
 * This function reads the loginuid from the /proc system. It returns
 * (uid_t)-1 on failure.
 */
static uid_t get_loginuid(pam_handle_t *pamh)
{
	int fd, count;
	char loginuid[24];
	char *eptr;
	uid_t rv = (uid_t)-1;

	fd = open("/proc/self/loginuid", O_NOFOLLOW|O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT) {
			pam_syslog(pamh, LOG_ERR,
				   "Cannot open /proc/self/loginuid: %m");
		}
		return rv;
	}
	if ((count = pam_modutil_read(fd, loginuid, sizeof(loginuid)-1)) < 1) {
		close(fd);
		return rv;
	}
	loginuid[count] = '\0';
	close(fd);

	errno = 0;
	rv = strtoul(loginuid, &eptr, 10);
	if (errno != 0 || eptr == loginuid)
		rv = (uid_t) -1;

	return rv;
}

static void
sepermit_unlock(pam_handle_t *pamh, void *plockfd, int error_status UNUSED)
{
	struct lockfd *lockfd = plockfd;
	struct flock fl;

	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;

	if (lockfd->debug)
		pam_syslog(pamh, LOG_ERR, "Unlocking fd: %d uid: %d", lockfd->fd, lockfd->uid);

	/* Don't kill uid==0 */
	if (lockfd->uid)
		/* This is a DOS but it prevents an app from forking to prevent killing */
		while(check_running(pamh, lockfd->uid, 1, lockfd->debug) > 0)
			continue;

	(void)fcntl(lockfd->fd, F_SETLK, &fl);
	(void)close(lockfd->fd);
	free(lockfd);
}

static int
sepermit_lock(pam_handle_t *pamh, const char *user, int debug)
{
	char buf[PATH_MAX];
	struct flock fl;

	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;

	struct passwd *pw = pam_modutil_getpwnam( pamh, user );
	if (!pw) {
		pam_syslog(pamh, LOG_NOTICE, "Unable to find uid for user %s",
			   user);
		return -1;
	}
	if (check_running(pamh, pw->pw_uid, 0, debug) > 0)  {
		pam_syslog(pamh, LOG_ERR, "User %s processes are running. Exclusive login not allowed", user);
		return -1;
	}

	pam_sprintf(buf, "%s/%d.lock", SEPERMIT_LOCKDIR, pw->pw_uid);
	int fd = open(buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		pam_syslog(pamh, LOG_ERR, "Unable to open lock file %s/%d.lock", SEPERMIT_LOCKDIR, pw->pw_uid);
		return -1;
	}

	/* Need to close on exec */
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	if (fcntl(fd, F_SETLK, &fl) == -1) {
		pam_syslog(pamh, LOG_ERR, "User %s with exclusive login already logged in", user);
		close(fd);
		return -1;
	}
	struct lockfd *lockfd=calloc(1, sizeof(struct lockfd));
	if (!lockfd) {
		close(fd);
		pam_syslog(pamh, LOG_CRIT, "Memory allocation error");
		return -1;
	}
	lockfd->uid = pw->pw_uid;
	lockfd->debug = debug;
	lockfd->fd=fd;
        pam_set_data(pamh, MODULE, lockfd, sepermit_unlock);
	return 0;
}

/* return 0 when matched, -1 when unmatched, pam error otherwise */
static int
sepermit_match(pam_handle_t *pamh, const char *cfgfile, const char *user,
	       const char *seuser, int debug, int *sense)
{
	FILE *f;
	char *line = NULL;
	char *start;
	size_t len = 0;
	int matched = 0;
	int exclusive = 0;
	int ignore = 0;

	f = fopen(cfgfile, "r");

	if (!f) {
		pam_syslog(pamh, LOG_ERR, "Failed to open config file %s: %m", cfgfile);
		return PAM_SERVICE_ERR;
	}

	while (!matched && getline(&line, &len, f) != -1) {
		size_t n;
		char *sptr;
		char *opt;

		if (line[0] == '#')
			continue;

		start = line;
		while (isspace((unsigned char)*start))
			++start;
		n = strlen(start);
		while (n > 0 && isspace((unsigned char)start[n-1])) {
			--n;
		}
		if (n == 0)
			continue;

		start[n] = '\0';
		start = strtok_r(start, OPT_DELIM, &sptr);

		switch (start[0]) {
			case '@':
				++start;
				if (debug)
					pam_syslog(pamh, LOG_NOTICE, "Matching user %s against group %s", user, start);
				if (pam_modutil_user_in_group_nam_nam(pamh, user, start)) {
					matched = 1;
				}
				break;
			case '%':
				if (seuser == NULL)
					break;
				++start;
				if (debug)
					pam_syslog(pamh, LOG_NOTICE, "Matching seuser %s against seuser %s", seuser, start);
				if (strcmp(seuser, start) == 0) {
					matched = 1;
				}
				break;
			default:
				if (debug)
					pam_syslog(pamh, LOG_NOTICE, "Matching user %s against user %s", user, start);
				if (strcmp(user, start) == 0) {
					matched = 1;
				}
		}
		if (matched)
			while ((opt=strtok_r(NULL, OPT_DELIM, &sptr)) != NULL) {
				if (strcmp(opt, "exclusive") == 0)
					exclusive = 1;
				else if (strcmp(opt, "ignore") == 0)
					ignore = 1;
				else if (debug) {
					pam_syslog(pamh, LOG_NOTICE, "Unknown user option: %s", opt);
				}
			}
	}

	free(line);
	fclose(f);
	if (matched) {
		if (*sense == PAM_SUCCESS) {
			if (ignore)
				*sense = PAM_IGNORE;
			if (geteuid() == 0 && exclusive && get_loginuid(pamh) == (uid_t)-1)
				if (sepermit_lock(pamh, user, debug) < 0)
					*sense = PAM_AUTH_ERR;
		}
		return 0;
	}
	else
		return -1;
}

static int
pam_sepermit(pam_handle_t *pamh, int argc, const char **argv)
{
	int i;
	int rv;
	int debug = 0;
	int sense = PAM_AUTH_ERR;
	const char *user = NULL;
	char *seuser = NULL;
	char *level = NULL;
	const char *cfgfile = NULL;

	/* Parse arguments. */
	for (i = 0; i < argc; i++) {
		const char *str;

		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
		} else if ((str = pam_str_skip_prefix(argv[i], "conf=")) != NULL) {
			cfgfile = str;
		} else {
			pam_syslog(pamh, LOG_ERR, "unknown option: %s", argv[i]);
		}
	}

	if (cfgfile == NULL) {
#ifdef SEPERMIT_VENDOR_CONF_FILE
		struct stat buffer;

		cfgfile = SEPERMIT_CONF_FILE;
		if (stat(cfgfile, &buffer) != 0 && errno == ENOENT)
			cfgfile = SEPERMIT_VENDOR_CONF_FILE;
#else
		cfgfile = SEPERMIT_CONF_FILE;
#endif
	}

	if (debug)
		pam_syslog(pamh, LOG_NOTICE, "Parsing config file: %s", cfgfile);

	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || *user == '\0') {
		pam_syslog(pamh, LOG_NOTICE, "cannot determine user name");
		return PAM_USER_UNKNOWN;
	}

	if (is_selinux_enabled() > 0) {
		if (security_getenforce() == 1) {
			if (debug)
				pam_syslog(pamh, LOG_NOTICE, "Enforcing mode, access will be allowed on match");
			sense = PAM_SUCCESS;
		}
	}

	if (getseuserbyname(user, &seuser, &level) != 0) {
		seuser = NULL;
		level = NULL;
		pam_syslog(pamh, LOG_ERR, "getseuserbyname failed: %m");
	}

	if (debug && sense != PAM_SUCCESS)
		pam_syslog(pamh, LOG_NOTICE, "Access will not be allowed on match");

	rv = sepermit_match(pamh, cfgfile, user, seuser, debug, &sense);

	if (debug)
		pam_syslog(pamh, LOG_NOTICE, "sepermit_match returned: %d", rv);

	free(seuser);
	free(level);

	switch (rv) {
		case -1:
			return PAM_IGNORE;
		case 0:
			return sense;
	}

	return rv;
}

int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
                int argc UNUSED, const char **argv UNUSED)
{
	return PAM_IGNORE;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
	return pam_sepermit(pamh, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
	return pam_sepermit(pamh, argc, argv);
}
