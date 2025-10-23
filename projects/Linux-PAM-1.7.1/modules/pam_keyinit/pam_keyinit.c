/*
 * pam_keyinit: Initialise the session keyring on login through a PAM module
 *
 * Copyright (C) 2006 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include <sys/syscall.h>
#include <stdatomic.h>

#define KEY_SPEC_SESSION_KEYRING	(-3) /* ID for session keyring */
#define KEY_SPEC_USER_KEYRING		(-4) /* ID for UID-specific keyring */
#define KEY_SPEC_USER_SESSION_KEYRING	(-5) /* - key ID for UID-session keyring */

#define KEYCTL_GET_KEYRING_ID		0 /* ask for a keyring's ID */
#define KEYCTL_JOIN_SESSION_KEYRING	1 /* start named session keyring */
#define KEYCTL_REVOKE			3 /* revoke a key */
#define KEYCTL_LINK			8 /* link a key into a keyring */

static _Thread_local int my_session_keyring = 0;
static _Atomic int session_counter = 0;
static _Thread_local int do_revoke = 0;
static _Thread_local uid_t revoke_as_uid;
static _Thread_local gid_t revoke_as_gid;
static _Thread_local int xdebug = 0;

static void debug(pam_handle_t *pamh, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void debug(pam_handle_t *pamh, const char *fmt, ...)
{
	va_list va;

	if (xdebug) {
		va_start(va, fmt);
		pam_vsyslog(pamh, LOG_DEBUG, fmt, va);
		va_end(va);
	}
}

static void error(pam_handle_t *pamh, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void error(pam_handle_t *pamh, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	pam_vsyslog(pamh, LOG_ERR, fmt, va);
	va_end(va);
}

static int pam_setreuid(uid_t ruid, uid_t euid)
{
#if defined(SYS_setreuid32)
    return syscall(SYS_setreuid32, ruid, euid);
#else
    return syscall(SYS_setreuid, ruid, euid);
#endif
}

static int pam_setregid(gid_t rgid, gid_t egid)
{
#if defined(SYS_setregid32)
    return syscall(SYS_setregid32, rgid, egid);
#else
    return syscall(SYS_setregid, rgid, egid);
#endif
}

static int pam_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
#if defined(SYS_setresuid32)
    return syscall(SYS_setresuid32, ruid, euid, suid);
#else
    return syscall(SYS_setresuid, ruid, euid, suid);
#endif
}

/*
 * initialise the session keyring for this process
 */
static int init_keyrings(pam_handle_t *pamh, int force, int error_ret)
{
	int session, usession, ret;

	if (!force) {
		/* get the IDs of the session keyring and the user session
		 * keyring */
		session = syscall(__NR_keyctl,
				  KEYCTL_GET_KEYRING_ID,
				  KEY_SPEC_SESSION_KEYRING,
				  0);
		debug(pamh, "GET SESSION = %d", session);
		if (session < 0) {
			/* don't worry about keyrings if facility not
			 * installed */
			if (errno == ENOSYS)
				return PAM_SUCCESS;
			return error_ret;
		}

		usession = syscall(__NR_keyctl,
				   KEYCTL_GET_KEYRING_ID,
				   KEY_SPEC_USER_SESSION_KEYRING,
				   0);
		debug(pamh, "GET SESSION = %d", usession);
		if (usession < 0)
			return error_ret;

		/* if the user session keyring is our keyring, then we don't
		 * need to do anything if we're not forcing */
		if (session != usession)
			return PAM_SUCCESS;
	}

	/* create a session keyring, discarding the old one */
	ret = syscall(__NR_keyctl,
		      KEYCTL_JOIN_SESSION_KEYRING,
		      NULL);
	debug(pamh, "JOIN = %d", ret);
	if (ret < 0)
		return error_ret;

	my_session_keyring = ret;

	/* make a link from the session keyring to the user keyring */
	ret = syscall(__NR_keyctl,
		      KEYCTL_LINK,
		      KEY_SPEC_USER_KEYRING,
		      KEY_SPEC_SESSION_KEYRING);

	return ret < 0 ? error_ret : PAM_SUCCESS;
}

/*
 * revoke the session keyring for this process
 */
static int kill_keyrings(pam_handle_t *pamh, int error_ret)
{
	uid_t old_uid;
	gid_t old_gid;
	int ret = PAM_SUCCESS;

	/* revoke the session keyring we created earlier */
	if (my_session_keyring > 0) {
		debug(pamh, "REVOKE %d", my_session_keyring);

		old_uid = geteuid();
		old_gid = getegid();
		debug(pamh, "UID:%d [%d]  GID:%d [%d]",
		      revoke_as_uid, old_uid, revoke_as_gid, old_gid);

		/* switch to the real UID and GID so that we have permission to
		 * revoke the key */
		if (revoke_as_gid != old_gid && pam_setregid(-1, revoke_as_gid) < 0) {
			error(pamh, "Unable to change GID to %d temporarily\n", revoke_as_gid);
			return error_ret;
		}

		if (revoke_as_uid != old_uid && pam_setresuid(-1, revoke_as_uid, old_uid) < 0) {
			error(pamh, "Unable to change UID to %d temporarily\n", revoke_as_uid);
			if (getegid() != old_gid && pam_setregid(-1, old_gid) < 0)
				error(pamh, "Unable to change GID back to %d\n", old_gid);
			return error_ret;
		}

		if (syscall(__NR_keyctl, KEYCTL_REVOKE, my_session_keyring) < 0) {
			ret = error_ret;
		}

		/* return to the original UID and GID (probably root) */
		if (revoke_as_uid != old_uid && pam_setreuid(-1, old_uid) < 0) {
			error(pamh, "Unable to change UID back to %d\n", old_uid);
			ret = error_ret;
		}

		if (revoke_as_gid != old_gid && pam_setregid(-1, old_gid) < 0) {
			error(pamh, "Unable to change GID back to %d\n", old_gid);
			ret = error_ret;
		}

		my_session_keyring = 0;
	}
	return ret;
}

static int do_keyinit(pam_handle_t *pamh, int argc, const char **argv, int error_ret)
{
	struct passwd *pw;
	const char *username;
	int ret, loop, force = 0;
	uid_t old_uid, uid;
	gid_t old_gid, gid;

	for (loop = 0; loop < argc; loop++) {
		if (strcmp(argv[loop], "force") == 0)
			force = 1;
		else if (strcmp(argv[loop], "debug") == 0)
			xdebug = 1;
		else if (strcmp(argv[loop], "revoke") == 0)
			do_revoke = 1;
	}

	/* don't do anything if already created a keyring (will be called
	 * multiple times if mentioned more than once in a pam script)
	 */
	if (my_session_keyring > 0)
		return PAM_SUCCESS;

	/* look up the target UID */
	ret = pam_get_user(pamh, &username, "key user");
	if (ret != PAM_SUCCESS)
		return ret;

	pw = pam_modutil_getpwnam(pamh, username);
	if (!pw) {
		pam_syslog(pamh, LOG_NOTICE, "Unable to look up user \"%s\"\n",
			   username);
		return PAM_USER_UNKNOWN;
	}

	revoke_as_uid = uid = pw->pw_uid;
	old_uid = getuid();
	revoke_as_gid = gid = pw->pw_gid;
	old_gid = getgid();
	debug(pamh, "UID:%d [%d]  GID:%d [%d]", uid, old_uid, gid, old_gid);

	/* switch to the real UID and GID so that the keyring ends up owned by
	 * the right user */
	if (gid != old_gid && pam_setregid(gid, -1) < 0) {
		error(pamh, "Unable to change GID to %d temporarily\n", gid);
		return error_ret;
	}

	if (uid != old_uid && pam_setreuid(uid, -1) < 0) {
		error(pamh, "Unable to change UID to %d temporarily\n", uid);
		if (pam_setregid(old_gid, -1) < 0)
			error(pamh, "Unable to change GID back to %d\n", old_gid);
		return error_ret;
	}

	ret = init_keyrings(pamh, force, error_ret);

	/* return to the original UID and GID (probably root) */
	if (uid != old_uid && pam_setreuid(old_uid, -1) < 0) {
		error(pamh, "Unable to change UID back to %d\n", old_uid);
		ret = error_ret;
	}

	if (gid != old_gid && pam_setregid(old_gid, -1) < 0) {
		error(pamh, "Unable to change GID back to %d\n", old_gid);
		ret = error_ret;
	}

	return ret;
}

/*
 * Dummy
 */
int pam_sm_authenticate(pam_handle_t *pamh UNUSED, int flags UNUSED,
                   int argc UNUSED, const char **argv UNUSED)
{
  return PAM_IGNORE;
}

/*
 * since setcred and open_session are called in different orders, a
 * session ring is invoked by the first of these functions called.
 */
int pam_sm_setcred(pam_handle_t *pamh, int flags,
                   int argc, const char **argv)
{
	if (flags & PAM_ESTABLISH_CRED) {
		debug(pamh, "ESTABLISH_CRED");
		return do_keyinit(pamh, argc, argv, PAM_CRED_ERR);
	}
	if (flags & PAM_DELETE_CRED && my_session_keyring > 0 && do_revoke) {
		debug(pamh, "DELETE_CRED");
		return kill_keyrings(pamh, PAM_CRED_ERR);
	}
	return PAM_IGNORE;
}

int pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
			int argc, const char **argv)
{
	session_counter++;

	debug(pamh, "OPEN %d", session_counter);

	return do_keyinit(pamh, argc, argv, PAM_SESSION_ERR);
}

/*
 * close a PAM session by revoking the session keyring if requested
 */
int pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
			 int argc UNUSED, const char **argv UNUSED)
{
	debug(pamh, "CLOSE %d,%d,%d",
	      session_counter, my_session_keyring, do_revoke);

	session_counter--;

	if (session_counter <= 0 && my_session_keyring > 0 && do_revoke)
		kill_keyrings(pamh, PAM_SESSION_ERR);

	return PAM_SUCCESS;
}
