/*
 * Copyright (c) 2022 Tomas Mraz <tm@t8m.info>
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <security/pam_modules.h>

#include "faillock_config.h"
#include "faillock.h"

#define FAILLOCK_DEFAULT_CONF SCONFIG_DIR "/faillock.conf"
#ifdef VENDOR_SCONFIG_DIR
#define VENDOR_FAILLOCK_DEFAULT_CONF VENDOR_SCONFIG_DIR "/faillock.conf"
#endif

static void PAM_FORMAT((printf, 3, 4)) PAM_NONNULL((3))
config_log(const pam_handle_t *pamh, int priority, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (pamh) {
		pam_vsyslog(pamh, priority, fmt, args);
	} else {
		char *buf = NULL;

		if (vasprintf(&buf, fmt, args) < 0) {
			fprintf(stderr, "vasprintf: %m");
			va_end(args);
			return;
		}
		fprintf(stderr, "%s\n", buf);
		free(buf);
	}
	va_end(args);
}

/* parse a single configuration file */
int
read_config_file(pam_handle_t *pamh, struct options *opts, const char *cfgfile)
{
	char *linebuf = NULL;
	size_t n = 0;
	const char *fname = (cfgfile != NULL) ? cfgfile : FAILLOCK_DEFAULT_CONF;
	FILE *f = fopen(fname, "r");

#ifdef VENDOR_FAILLOCK_DEFAULT_CONF
	if (f == NULL && errno == ENOENT && cfgfile == NULL) {
		/*
		 * If the default configuration file in /etc does not exist,
		 * try the vendor configuration file as fallback.
		 */
		f = fopen(VENDOR_FAILLOCK_DEFAULT_CONF, "r");
	}
#endif /* VENDOR_FAILLOCK_DEFAULT_CONF */

	if (f == NULL) {
		/* ignore non-existent default config file */
		if (errno == ENOENT && cfgfile == NULL)
			return PAM_SUCCESS;
		return PAM_SERVICE_ERR;
	}

	while (getline(&linebuf, &n, f) != -1) {
		size_t len;
		char *ptr;
		char *name;
		int eq;

		len = strlen(linebuf);
		if (len && linebuf[len - 1] != '\n' && !feof(f)) {
			free(linebuf);
			(void) fclose(f);
			return PAM_SERVICE_ERR;
		}

		if ((ptr=strchr(linebuf, '#')) != NULL) {
			*ptr = '\0';
		} else {
			ptr = linebuf + len;
		}

		/* drop terminating whitespace including the \n */
		while (ptr > linebuf) {
			if (!isspace((unsigned char)*(ptr-1))) {
				*ptr = '\0';
				break;
			}
			--ptr;
		}

		/* skip initial whitespace */
		for (ptr = linebuf; isspace((unsigned char)*ptr); ptr++);
		if (*ptr == '\0')
			continue;

		/* grab the key name */
		eq = 0;
		name = ptr;
		while (*ptr != '\0') {
			if (isspace((unsigned char)*ptr) || *ptr == '=') {
				eq = *ptr == '=';
				*ptr = '\0';
				++ptr;
				break;
			}
			++ptr;
		}

		/* grab the key value */
		while (*ptr != '\0') {
			if (*ptr != '=' || eq) {
				if (!isspace((unsigned char)*ptr)) {
					break;
				}
			} else {
				eq = 1;
			}
			++ptr;
		}

		/* set the key:value pair on opts */
		set_conf_opt(pamh, opts, name, ptr);
	}

	free(linebuf);
	(void)fclose(f);
	return PAM_SUCCESS;
}

void
set_conf_opt(pam_handle_t *pamh, struct options *opts, const char *name,
			 const char *value)
{
	if (strcmp(name, "dir") == 0) {
		if (value[0] != '/') {
			config_log(pamh, LOG_ERR,
					"Tally directory is not absolute path (%s); keeping value",
					value);
		} else {
			free(opts->dir);
			opts->dir = strdup(value);
			if (opts->dir == NULL) {
				opts->fatal_error = 1;
				config_log(pamh, LOG_CRIT, "Error allocating memory: %m");
			}
		}
	}
	else if (strcmp(name, "deny") == 0) {
		if (sscanf(value, "%hu", &opts->deny) != 1) {
			config_log(pamh, LOG_ERR,
				"Bad number supplied for deny argument");
		}
	}
	else if (strcmp(name, "fail_interval") == 0) {
		unsigned int temp;
		if (sscanf(value, "%u", &temp) != 1 ||
			temp > MAX_TIME_INTERVAL) {
			config_log(pamh, LOG_ERR,
				"Bad number supplied for fail_interval argument");
		} else {
			opts->fail_interval = temp;
		}
	}
	else if (strcmp(name, "unlock_time") == 0) {
		unsigned int temp;

		if (strcmp(value, "never") == 0) {
			opts->unlock_time = 0;
		}
		else if (sscanf(value, "%u", &temp) != 1 ||
			temp > MAX_TIME_INTERVAL) {
			config_log(pamh, LOG_ERR,
				"Bad number supplied for unlock_time argument");
		}
		else {
			opts->unlock_time = temp;
		}
	}
	else if (strcmp(name, "root_unlock_time") == 0) {
		unsigned int temp;

		if (strcmp(value, "never") == 0) {
			opts->root_unlock_time = 0;
		}
		else if (sscanf(value, "%u", &temp) != 1 ||
			temp > MAX_TIME_INTERVAL) {
			config_log(pamh, LOG_ERR,
				"Bad number supplied for root_unlock_time argument");
		} else {
			opts->root_unlock_time = temp;
		}
	}
	else if (strcmp(name, "admin_group") == 0) {
		free(opts->admin_group);
		opts->admin_group = strdup(value);
		if (opts->admin_group == NULL) {
			opts->fatal_error = 1;
			config_log(pamh, LOG_CRIT, "Error allocating memory: %m");
		}
	}
	else if (strcmp(name, "even_deny_root") == 0) {
		opts->flags |= FAILLOCK_FLAG_DENY_ROOT;
	}
	else if (strcmp(name, "audit") == 0) {
		opts->flags |= FAILLOCK_FLAG_AUDIT;
	}
	else if (strcmp(name, "silent") == 0) {
		opts->flags |= FAILLOCK_FLAG_SILENT;
	}
	else if (strcmp(name, "no_log_info") == 0) {
		opts->flags |= FAILLOCK_FLAG_NO_LOG_INFO;
	}
	else if (strcmp(name, "local_users_only") == 0) {
		opts->flags |= FAILLOCK_FLAG_LOCAL_ONLY;
	}
	else if (strcmp(name, "nodelay") == 0) {
		opts->flags |= FAILLOCK_FLAG_NO_DELAY;
	}
	else {
		config_log(pamh, LOG_ERR, "Unknown option: %s", name);
	}
}

const char *get_tally_dir(const struct options *opts)
{
	return (opts->dir != NULL) ? opts->dir : FAILLOCK_DEFAULT_TALLYDIR;
}
