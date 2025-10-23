/*
 * Copyright (c) 2022 Iker Pedrosa <ipedrosa@redhat.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>

#include <security/pam_modutil.h>

#include "pam_inline.h"
#include "pwhistory_config.h"

#define PWHISTORY_DEFAULT_CONF SCONFIG_DIR "/pwhistory.conf"

#ifdef VENDOR_SCONFIG_DIR
#define VENDOR_PWHISTORY_DEFAULT_CONF (VENDOR_SCONFIG_DIR "/pwhistory.conf")
#endif

void
parse_config_file(pam_handle_t *pamh, int argc, const char **argv,
                  struct options_t *options)
{
    const char *fname = NULL;
    int i;
    char *val;

    for (i = 0; i < argc; ++i) {
        const char *str = pam_str_skip_prefix(argv[i], "conf=");

        if (str != NULL) {
            fname = str;
        }
    }

    if (fname == NULL) {
        fname = PWHISTORY_DEFAULT_CONF;

#ifdef VENDOR_PWHISTORY_DEFAULT_CONF
        /*
         * Check whether PWHISTORY_DEFAULT_CONF file is available.
         * If it does not exist, fall back to VENDOR_PWHISTORY_DEFAULT_CONF file.
         */
        struct stat buffer;
        if (stat(fname, &buffer) != 0 && errno == ENOENT) {
            fname = VENDOR_PWHISTORY_DEFAULT_CONF;
        }
#endif
    }

    val = pam_modutil_search_key (pamh, fname, "debug");
    if (val != NULL) {
        options->debug = 1;
        free(val);
    }

    val = pam_modutil_search_key (pamh, fname, "enforce_for_root");
    if (val != NULL) {
        options->enforce_for_root = 1;
        free(val);
    }

    val = pam_modutil_search_key (pamh, fname, "remember");
    if (val != NULL) {
        unsigned int temp;
        if (sscanf(val, "%u", &temp) != 1) {
            pam_syslog(pamh, LOG_ERR,
                "Bad number supplied for remember argument");
        } else {
            options->remember = temp;
        }
        free(val);
    }

    val = pam_modutil_search_key (pamh, fname, "retry");
    if (val != NULL) {
        unsigned int temp;
        if (sscanf(val, "%u", &temp) != 1) {
            pam_syslog(pamh, LOG_ERR,
                "Bad number supplied for retry argument");
        } else {
            options->tries = temp;
        }
        free(val);
    }

    val = pam_modutil_search_key (pamh, fname, "file");
    if (val != NULL) {
        if (*val != '/') {
            pam_syslog (pamh, LOG_ERR,
                "File path should be absolute: %s", val);
            free(val);
        } else {
            options->filename = val;
        }
    }
}
