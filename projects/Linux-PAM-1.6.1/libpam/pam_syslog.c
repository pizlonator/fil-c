/*
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
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_ext.h>

#include "pam_private.h"

#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif

static const char *
_pam_choice2str (int choice)
{
  switch (choice)
    {
    case PAM_AUTHENTICATE:
      return "auth";
    case PAM_SETCRED:
      return "setcred";
    case PAM_ACCOUNT:
      return "account";
    case PAM_OPEN_SESSION:
    case PAM_CLOSE_SESSION:
      return "session";
    case PAM_CHAUTHTOK:
      return "chauthtok";
    }
  return "";
}

void
pam_vsyslog (const pam_handle_t *pamh, int priority,
	     const char *fmt, va_list args)
{
  char *msgbuf1 = NULL, *msgbuf2 = NULL;
  int save_errno = errno;

  if (pamh && pamh->mod_name)
    {
      if (asprintf (&msgbuf1, "%s(%s:%s):", pamh->mod_name,
		    pamh->service_name?pamh->service_name:"<unknown>",
		    _pam_choice2str (pamh->choice)) < 0)
	{
	  syslog (LOG_AUTHPRIV|LOG_ERR, "asprintf: %m");
	  return;
	}
    }

  errno = save_errno;
  if (vasprintf (&msgbuf2, fmt, args) < 0)
    {
      syslog (LOG_AUTHPRIV|LOG_ERR, "vasprintf: %m");
      _pam_drop (msgbuf1);
      return;
    }

  errno = save_errno;
  syslog (LOG_AUTHPRIV|priority, "%s %s",
	  (msgbuf1 ? msgbuf1 : _PAM_SYSTEM_LOG_PREFIX), msgbuf2);

  _pam_drop (msgbuf1);
  _pam_drop (msgbuf2);
}

void
pam_syslog (const pam_handle_t *pamh, int priority,
	    const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  pam_vsyslog (pamh, priority, fmt, args);
  va_end (args);
}
