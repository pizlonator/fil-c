/*
 * Copyright (c) 2013 Red Hat, Inc.
 * Author: Tomas Mraz <tmraz@redhat.com>
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
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <security/_pam_types.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include "opasswd.h"
#include "pam_inline.h"


static int
check_history(const char *user, const char *filename, const char *debug)
{
  char pass[PAM_MAX_RESP_SIZE + 1];
  char *passwords[] = { pass };
  int npass;
  int dbg = atoi(debug); /* no need to be too fancy here */
  int retval;

  /* read the password from stdin (a pipe from the pam_pwhistory module) */
  npass = pam_read_passwords(STDIN_FILENO, 1, passwords);

  if (npass != 1)
    { /* is it a valid password? */
      helper_log_err(LOG_DEBUG, "no password supplied");
      return PAM_AUTHTOK_ERR;
    }

  retval = check_old_pass(user, pass, filename, dbg);

  pam_overwrite_array(pass);	/* clear memory of the password */

  return retval;
}

static int
save_history(const char *user, const char *filename, const char *howmany, const char *debug)
{
  int num = atoi(howmany);
  int dbg = atoi(debug); /* no need to be too fancy here */
  int retval;

  retval = save_old_pass(user, num, filename, dbg);

  return retval;
}

int
main(int argc, char *argv[])
{
  const char *option;
  const char *user;
  const char *filename;

  /*
   * we establish that this program is running with non-tty stdin.
   * this is to discourage casual use.
   */

  if (isatty(STDIN_FILENO) || argc < 5)
    {
      fprintf(stderr,
            "This binary is not designed for running in this way.\n");
      return PAM_SYSTEM_ERR;
    }

  option = argv[1];
  user = argv[2];
  filename = (argv[3][0] != '\0') ? argv[3] : NULL;

  if (strcmp(option, "check") == 0 && argc == 5)
    return check_history(user, filename, argv[4]);
  else if (strcmp(option, "save") == 0 && argc == 6)
    return save_history(user, filename, argv[4], argv[5]);

  fprintf(stderr, "This binary is not designed for running in this way.\n");

  return PAM_SYSTEM_ERR;
}
