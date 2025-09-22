/*
 * SPDX-FileCopyrightText:  2023, Iker Pedrosa <ipedrosa@redhat.com>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "attr.h"
#include "defines.h"
#include "prototypes.h"

#include <systemd/sd-login.h>

int get_session_host (char **out)
{
    char *host = NULL;
    char *session = NULL;
    int ret;

    ret = sd_pid_get_session (getpid(), &session);
    if (ret < 0) {
        return ret;
    }
    ret = sd_session_get_remote_host (session, &host);
    if (ret < 0) {
        goto done;
    }

    *out = host;

done:
    free (session);
    return ret;
}

unsigned long active_sessions_count(const char *name, MAYBE_UNUSED unsigned long limit)
{
    struct passwd *pw;
    unsigned long count = 0;

    pw = prefix_getpwnam(name);
    if (pw == NULL) {
        return 0;
    }

    count = sd_uid_get_sessions(pw->pw_uid, 0, NULL);

    return count;
}
