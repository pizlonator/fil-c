/******************************************************************************
 * Check user type based on login.defs.
 *
 * Copyright (c) 2020 Red Hat, Inc.
 * Written by Pavel Březina <pbrezina@redhat.com>
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
 *
 */

#include "config.h"

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <errno.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#define LOGIN_DEFS "/etc/login.defs"

enum pam_usertype_op {
    OP_IS_SYSTEM,
    OP_IS_REGULAR,

    OP_SENTINEL
};

struct pam_usertype_opts {
    enum pam_usertype_op op;
    int use_uid;
    int audit;
};

static int
pam_usertype_parse_args(struct pam_usertype_opts *opts,
                        pam_handle_t *pamh,
                        int argc,
                        const char **argv)
{
    int i;

    memset(opts, 0, sizeof(struct pam_usertype_opts));
    opts->op = OP_SENTINEL;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "use_uid") == 0) {
            opts->use_uid = 1;
        } else if (strcmp(argv[i], "audit") == 0) {
            opts->audit = 1;
        } else if (strcmp(argv[i], "issystem") == 0) {
            opts->op = OP_IS_SYSTEM;
        } else if (strcmp(argv[i], "isregular") == 0) {
            opts->op = OP_IS_REGULAR;
        } else {
            pam_syslog(pamh, LOG_WARNING, "Unknown argument: %s", argv[i]);
            /* Just continue. */
        }
    }

    if (opts->op == OP_SENTINEL) {
        pam_syslog(pamh, LOG_ERR, "Operation not specified");
        return PAM_SERVICE_ERR;
    }

    return PAM_SUCCESS;
}

static int
pam_usertype_get_uid(struct pam_usertype_opts *opts,
                     pam_handle_t *pamh,
                     uid_t *_uid)
{
    struct passwd *pwd;
    const char *username;
    int ret;

    /* Get uid of user that runs the application. */
    if (opts->use_uid) {
        pwd = pam_modutil_getpwuid(pamh, getuid());
        if (pwd == NULL) {
            pam_syslog(pamh, LOG_ERR,
                       "error retrieving information about user %lu",
                       (unsigned long)getuid());
            return PAM_USER_UNKNOWN;
        }

        *_uid = pwd->pw_uid;
        return PAM_SUCCESS;
    }

    /* Get uid of user that is being authenticated. */
    ret = pam_get_user(pamh, &username, NULL);
    if (ret != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
                   pam_strerror(pamh, ret));
        return ret == PAM_CONV_AGAIN ? PAM_INCOMPLETE : ret;
    }

    pwd = pam_modutil_getpwnam(pamh, username);
    if (pwd == NULL) {
        if (opts->audit) {
            pam_syslog(pamh, LOG_NOTICE,
                       "error retrieving information about user %s", username);
        }

        pam_modutil_getpwnam(pamh, "root");

        return PAM_USER_UNKNOWN;
    }
    pam_modutil_getpwnam(pamh, "pam_usertype_non_existent:");

    *_uid = pwd->pw_uid;

    return PAM_SUCCESS;
}

#define MAX_UID_VALUE 0xFFFFFFFFUL

static uid_t
pam_usertype_get_id(pam_handle_t *pamh,
                    const char *key,
                    uid_t default_value)
{
    unsigned long ul;
    char *value;
    char *ep;
    uid_t uid;

    value = pam_modutil_search_key(pamh, LOGIN_DEFS, key);
    if (value == NULL) {
        return default_value;
    }

    /* taken from get_lastlog_uid_max() */
    ep = value + strlen(value);
    while (ep > value && isspace((unsigned char)*(--ep))) {
        *ep = '\0';
    }

    errno = 0;
    ul = strtoul(value, &ep, 10);
    if (!(ul >= MAX_UID_VALUE
        || (uid_t)ul >= MAX_UID_VALUE
        || (errno != 0 && ul == 0)
        || value == ep
        || *ep != '\0')) {
        uid = (uid_t)ul;
    } else {
        uid = default_value;
    }

    free(value);

    return uid;
}

static int
pam_usertype_is_system(pam_handle_t *pamh, uid_t uid)
{
    uid_t uid_min;
    uid_t sys_max;

    if (uid == (uid_t)-1) {
        pam_syslog(pamh, LOG_WARNING, "invalid uid");
        return PAM_USER_UNKNOWN;
    }

    if (uid == PAM_USERTYPE_OVERFLOW_UID) {
        /* nobody */
        return PAM_SUCCESS;
    }

    uid_min = pam_usertype_get_id(pamh, "UID_MIN", PAM_USERTYPE_UIDMIN);
    sys_max = pam_usertype_get_id(pamh, "SYS_UID_MAX", uid_min - 1);

    if (uid <= sys_max && uid < uid_min) {
        return PAM_SUCCESS;
    }

    return PAM_AUTH_ERR;
}

static int
pam_usertype_is_regular(pam_handle_t *pamh, uid_t uid)
{
    int ret;

    ret = pam_usertype_is_system(pamh, uid);
    switch (ret) {
    case PAM_SUCCESS:
        return PAM_AUTH_ERR;
    case PAM_USER_UNKNOWN:
        return PAM_USER_UNKNOWN;
    default:
        return PAM_SUCCESS;
    }
}

static int
pam_usertype_evaluate(struct pam_usertype_opts *opts,
                      pam_handle_t *pamh,
                      uid_t uid)
{
    switch (opts->op) {
    case OP_IS_SYSTEM:
        return pam_usertype_is_system(pamh, uid);
    case OP_IS_REGULAR:
        return pam_usertype_is_regular(pamh, uid);
    default:
        pam_syslog(pamh, LOG_ERR, "Unknown operation: %d", opts->op);
        return PAM_SERVICE_ERR;
    }
}

/**
 * Arguments:
 * - issystem: uid less than SYS_UID_MAX
 * - isregular: not issystem
 * - use_uid: use user that runs application not that is being authenticate (same as in pam_succeed_if)
 * - audit: log unknown users to syslog
 */
static int
pam_usertype(pam_handle_t *pamh, int argc, const char **argv)
{
    struct pam_usertype_opts opts;
    uid_t uid = -1;
    int ret;

    ret = pam_usertype_parse_args(&opts, pamh, argc, argv);
    if (ret != PAM_SUCCESS) {
        return ret;
    }

    ret = pam_usertype_get_uid(&opts, pamh, &uid);
    if (ret != PAM_SUCCESS) {
        return ret;
    }

    return pam_usertype_evaluate(&opts, pamh, uid);
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
               int argc UNUSED, const char **argv UNUSED)
{
	return PAM_IGNORE;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
	return pam_usertype(pamh, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
	return pam_usertype(pamh, argc, argv);
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
	return pam_usertype(pamh, argc, argv);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
	return pam_usertype(pamh, argc, argv);
}

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
	return pam_usertype(pamh, argc, argv);
}
