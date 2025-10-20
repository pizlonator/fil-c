/*
 * Copyright (c) 2008 Thorsten Kukuk <kukuk@suse.de>
 * Copyright (c) 2013 Red Hat, Inc.
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

#ifndef __OPASSWD_H__
#define __OPASSWD_H__

#define PAM_PWHISTORY_RUN_HELPER PAM_CRED_INSUFFICIENT

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#define SELINUX_ENABLED (is_selinux_enabled()>0)
#else
#define SELINUX_ENABLED 0
#endif

#ifdef HELPER_COMPILE
#define PAMH_ARG_DECL(fname, ...) fname(__VA_ARGS__)
#else
#define PAMH_ARG_DECL(fname, ...) fname(pam_handle_t *pamh, __VA_ARGS__)
#endif

#ifdef HELPER_COMPILE
void
PAM_FORMAT((printf, 2, 3))
helper_log_err(int err, const char *format, ...);
#endif

PAMH_ARG_DECL(int check_old_pass, const char *user, const char *newpass,
              const char *filename, int debug);

PAMH_ARG_DECL(int save_old_pass, const char *user, int howmany,
              const char *filename, int debug);

#endif /* __OPASSWD_H__ */
