/* Based on setproctitle.c from OpenSSH 10.3p1, which is based on conf.c from UCB sendmail 8.8.8 */

/*
 * Copyright 2003 Damien Miller
 * Copyright (c) 1983, 1995-1997 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026 Filip Pizlo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_setproctitle.h"

#if PAS_ENABLE_FILC

#include <stdlib.h>
#include <string.h>

/* We're only taking the Linux-specific implementation of setproctitle.
   
   If we ever want to support other OSes, we'll have to port more of what OpenSSH's setproctitle.c
   does. That will be easy, since MOST of that file is the Linux support (Linux is hardest).

   NOTE: the OpenSSH checked into the Fil-C repo has a different setproctitle.c, which is hacked to
   use Fil-C API! */

static char *argv_start = NULL;
static size_t argv_env_len = 0;

void filc_setproctitle_initialize(int argc, char** argv)
{
    extern char **environ;
    char *lastargv = NULL;
    char **envp = environ;
    int i;

    /*
     * NB: This assumes that argv has already been copied out of the
     * way. This is true for sshd, but may not be true for other
     * programs. Beware.
     */

    if (argc == 0 || argv == NULL || argv[0] == NULL)
        return;

    /* Fail if we can't allocate room for the new environment */
    for (i = 0; envp != NULL && envp[i] != NULL; i++)
        ;
    if ((environ = calloc(i + 1, sizeof(*environ))) == NULL) {
        environ = envp;	/* put it back */
        return;
    }

    /*
     * Find the last argv string or environment variable within
     * our process memory area.
     */
    for (i = 0; i < argc; i++) {
        if (argv[i] == NULL)
            continue;
        if (lastargv == NULL || lastargv + 1 == argv[i])
            lastargv = argv[i] + strlen(argv[i]);
    }
    for (i = 0; envp != NULL && envp[i] != NULL; i++) {
        if (lastargv && lastargv + 1 == envp[i])
            lastargv = envp[i] + strlen(envp[i]);
    }

    argv[1] = NULL;
    argv_start = argv[0];
    argv_env_len = (lastargv > argv[0]) ? (size_t)(lastargv - argv[0] - 1) : 0;

    /*
     * Copy environment
     * XXX - will truncate env on strdup fail
     */
    for (i = 0; envp != NULL && envp[i] != NULL; i++)
        environ[i] = strdup(envp[i]);
    environ[i] = NULL;
}

void filc_setproctitle(const char* ptitle)
{
    PAS_ASSERT(ptitle);
    if (!argv_start || !argv_env_len)
        return;
    size_t len = 0;
    len = strlcpy(argv_start, ptitle, argv_env_len);
    for(; len < argv_env_len; len++)
        argv_start[len] = 0;
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

