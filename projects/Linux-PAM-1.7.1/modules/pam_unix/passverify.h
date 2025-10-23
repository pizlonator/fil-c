/*
 * Copyright information at end of file.
 */

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <security/pam_modules.h>

#define PAM_UNIX_RUN_HELPER PAM_CRED_INSUFFICIENT

#define OLD_PASSWORDS_FILE      SCONFIG_DIR "/opasswd"

int
is_pwd_shadowed(const struct passwd *pwd);

char *
crypt_md5_wrapper(const char *pass_new);

int
unix_selinux_confined(void);

int
lock_pwdf(void);

void
unlock_pwdf(void);

#ifdef HELPER_COMPILE
int
save_old_password(const char *forwho, const char *oldpass,
		  int howmany);
#else
int
save_old_password(pam_handle_t *pamh, const char *forwho, const char *oldpass,
		  int howmany);
#endif

#ifdef HELPER_COMPILE
void
PAM_FORMAT((printf, 2, 3))
helper_log_err(int err, const char *format,...);

int
helper_verify_password(const char *name, const char *p, int nullok);

void
setup_signals(void);

char *
getuidname(uid_t uid);

#endif

#ifdef HELPER_COMPILE
#define PAMH_ARG_DECL(fname, ...)	fname(__VA_ARGS__)
#define PAMH_ARG(...)			__VA_ARGS__
#else
#define PAMH_ARG_DECL(fname, ...)	fname(pam_handle_t *pamh, __VA_ARGS__)
#define PAMH_ARG(...)			pamh, __VA_ARGS__
#endif

PAMH_ARG_DECL(int verify_pwd_hash,
	const char *p, char *hash, unsigned int nullok);

PAMH_ARG_DECL(char * create_password_hash,
	const char *password, unsigned long long ctrl, int rounds);

PAMH_ARG_DECL(int get_account_info,
	const char *name, struct passwd **pwd, struct spwd **spwdent);

PAMH_ARG_DECL(int get_pwd_hash,
	const char *name, struct passwd **pwd, char **hash);

PAMH_ARG_DECL(int check_shadow_expiry,
	struct spwd *spent, int *daysleft);

PAMH_ARG_DECL(int unix_update_passwd,
	const char *forwho, const char *towhat);

PAMH_ARG_DECL(int unix_update_shadow,
	const char *forwho, char *towhat);

/* ****************************************************************** *
 * Copyright (c) Red Hat, Inc. 2007.
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
