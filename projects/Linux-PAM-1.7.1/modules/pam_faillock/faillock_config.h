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

/*
 * faillock_config.h - load configuration options from file
 *
 */

#ifndef _FAILLOCK_CONFIG_H
#define _FAILLOCK_CONFIG_H

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#include <security/pam_ext.h>

#define FAILLOCK_FLAG_DENY_ROOT		0x1
#define FAILLOCK_FLAG_AUDIT			0x2
#define FAILLOCK_FLAG_SILENT		0x4
#define FAILLOCK_FLAG_NO_LOG_INFO	0x8
#define FAILLOCK_FLAG_UNLOCKED		0x10
#define FAILLOCK_FLAG_LOCAL_ONLY	0x20
#define FAILLOCK_FLAG_NO_DELAY		0x40

#define MAX_TIME_INTERVAL			604800 /* 7 days */

struct options {
	unsigned int action;
	unsigned int flags;
	unsigned short deny;
	unsigned int fail_interval;
	unsigned int unlock_time;
	unsigned int root_unlock_time;
	char *dir;
	const char *user;
	char *admin_group;
	int failures;
	uint64_t latest_time;
	uid_t uid;
	int is_admin;
	uint64_t now;
	int fatal_error;

	unsigned int reset;
	const char *progname;
	int legacy_output; /* show failure info in pam_tally2 style */
};

int read_config_file(pam_handle_t *pamh, struct options *opts,
					 const char *cfgfile);
void set_conf_opt(pam_handle_t *pamh, struct options *opts, const char *name,
		  const char *value);
const char *get_tally_dir(const struct options *opts);

#endif /* _FAILLOCK_CONFIG_H */
