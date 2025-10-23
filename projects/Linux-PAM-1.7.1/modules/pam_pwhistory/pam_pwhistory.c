/*
 * pam_pwhistory module
 *
 * Copyright (c) 2008, 2012 Thorsten Kukuk
 * Author: Thorsten Kukuk <kukuk@thkukuk.de>
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

#include <config.h>

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include <security/_pam_macros.h>

#include "opasswd.h"
#include "pam_inline.h"
#include "pam_i18n.h"
#include "pwhistory_config.h"



static void
parse_option (pam_handle_t *pamh, const char *argv, options_t *options)
{
  const char *str;

  if (strcasecmp (argv, "try_first_pass") == 0)
    /* ignore */;
  else if (strcasecmp (argv, "use_first_pass") == 0)
    /* ignore */;
  else if (strcasecmp (argv, "use_authtok") == 0)
    /* ignore, handled by pam_get_authtok */;
  else if (strcasecmp (argv, "debug") == 0)
    options->debug = 1;
  else if ((str = pam_str_skip_icase_prefix(argv, "remember=")) != NULL)
    {
      options->remember = strtol(str, NULL, 10);
      if (options->remember < 0)
        options->remember = 0;
      if (options->remember > 400)
        options->remember = 400;
    }
  else if ((str = pam_str_skip_icase_prefix(argv, "retry=")) != NULL)
    {
      options->tries = strtol(str, NULL, 10);
      if (options->tries < 0)
        options->tries = 1;
    }
  else if (strcasecmp (argv, "enforce_for_root") == 0)
    options->enforce_for_root = 1;
  else if (pam_str_skip_icase_prefix(argv, "authtok_type=") != NULL)
    { /* ignore, for pam_get_authtok */; }
  else if ((str = pam_str_skip_icase_prefix(argv, "file=")) != NULL)
    {
      if (*str != '/')
        {
          pam_syslog (pamh, LOG_ERR,
                      "pam_pwhistory: file path should be absolute: %s", argv);
        }
      else
        options->filename = str;
    }
  else
    pam_syslog (pamh, LOG_ERR, "pam_pwhistory: unknown option: %s", argv);
}

#ifdef WITH_SELINUX
static int
run_save_helper(pam_handle_t *pamh, const char *user,
		int howmany, const char *filename, int debug)
{
  int retval, child;
  struct sigaction newsa, oldsa;

  memset(&newsa, '\0', sizeof(newsa));
  newsa.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &newsa, &oldsa);

  child = fork();
  if (child == 0)
    {
      static char *envp[] = { NULL };
      char *args[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

      if (pam_modutil_sanitize_helper_fds(pamh, PAM_MODUTIL_PIPE_FD,
          PAM_MODUTIL_PIPE_FD,
          PAM_MODUTIL_PIPE_FD) < 0)
        {
          _exit(PAM_SYSTEM_ERR);
        }

      /* exec binary helper */
      DIAG_PUSH_IGNORE_CAST_QUAL;
      args[0] = (char *)PWHISTORY_HELPER;
      args[1] = (char *)"save";
      args[2] = (char *)user;
      args[3] = (char *)((filename != NULL) ? filename : "");
      DIAG_POP_IGNORE_CAST_QUAL;
      if ((args[4] = pam_asprintf("%d", howmany)) == NULL ||
          (args[5] = pam_asprintf("%d", debug)) == NULL)
        {
          pam_syslog(pamh, LOG_ERR, "asprintf: %m");
          _exit(PAM_SYSTEM_ERR);
        }

      execve(args[0], args, envp);

      pam_syslog(pamh, LOG_ERR, "helper binary execve failed: %s: %m", args[0]);

      _exit(PAM_SYSTEM_ERR);
    }
  else if (child > 0)
    {
      /* wait for child */
      int rc = 0;
      while ((rc = waitpid (child, &retval, 0)) == -1 &&
              errno == EINTR);
      if (rc < 0)
        {
          pam_syslog(pamh, LOG_ERR, "pwhistory_helper save: waitpid: %m");
          retval = PAM_SYSTEM_ERR;
        }
      else if (!WIFEXITED(retval))
        {
          pam_syslog(pamh, LOG_ERR, "pwhistory_helper save abnormal exit: %d", retval);
          retval = PAM_SYSTEM_ERR;
        }
      else
        {
          retval = WEXITSTATUS(retval);
        }
    }
  else
    {
      pam_syslog(pamh, LOG_ERR, "fork failed: %m");
      retval = PAM_SYSTEM_ERR;
    }

  sigaction(SIGCHLD, &oldsa, NULL);   /* restore old signal handler */

  return retval;
}

static int
run_check_helper(pam_handle_t *pamh, const char *user,
		 const char *newpass, const char *filename, int debug)
{
  int retval, child, fds[2];
  struct sigaction newsa, oldsa;

  /* create a pipe for the password */
  if (pipe(fds) != 0)
    return PAM_SYSTEM_ERR;

  memset(&newsa, '\0', sizeof(newsa));
  newsa.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &newsa, &oldsa);

  child = fork();
  if (child == 0)
    {
      static char *envp[] = { NULL };
      char *args[] = { NULL, NULL, NULL, NULL, NULL, NULL };

      /* reopen stdin as pipe */
      if (dup2(fds[0], STDIN_FILENO) != STDIN_FILENO)
        {
          pam_syslog(pamh, LOG_ERR, "dup2 of %s failed: %m", "stdin");
          _exit(PAM_SYSTEM_ERR);
        }

      if (pam_modutil_sanitize_helper_fds(pamh, PAM_MODUTIL_IGNORE_FD,
          PAM_MODUTIL_PIPE_FD,
          PAM_MODUTIL_PIPE_FD) < 0)
        {
          _exit(PAM_SYSTEM_ERR);
        }

      /* exec binary helper */
      DIAG_PUSH_IGNORE_CAST_QUAL;
      args[0] = (char *)PWHISTORY_HELPER;
      args[1] = (char *)"check";
      args[2] = (char *)user;
      args[3] = (char *)((filename != NULL) ? filename : "");
      DIAG_POP_IGNORE_CAST_QUAL;
      if ((args[4] = pam_asprintf("%d", debug)) == NULL)
        {
          pam_syslog(pamh, LOG_ERR, "asprintf: %m");
          _exit(PAM_SYSTEM_ERR);
        }

      execve(args[0], args, envp);

      pam_syslog(pamh, LOG_ERR, "helper binary execve failed: %s: %m", args[0]);

      _exit(PAM_SYSTEM_ERR);
    }
  else if (child > 0)
    {
      /* wait for child */
      int rc = 0;
      if (newpass == NULL)
        newpass = "";

      /* send the password to the child */
      if (write(fds[1], newpass, strlen(newpass)+1) == -1)
        {
          pam_syslog(pamh, LOG_ERR, "Cannot send password to helper: %m");
          retval = PAM_SYSTEM_ERR;
        }
      newpass = NULL;
      close(fds[0]);       /* close here to avoid possible SIGPIPE above */
      close(fds[1]);
      while ((rc = waitpid (child, &retval, 0)) == -1 &&
              errno == EINTR);
      if (rc < 0)
        {
          pam_syslog(pamh, LOG_ERR, "pwhistory_helper check: waitpid: %m");
          retval = PAM_SYSTEM_ERR;
        }
      else if (!WIFEXITED(retval))
        {
          pam_syslog(pamh, LOG_ERR, "pwhistory_helper check abnormal exit: %d", retval);
          retval = PAM_SYSTEM_ERR;
        }
      else
        {
          retval = WEXITSTATUS(retval);
        }
    }
  else
    {
      pam_syslog(pamh, LOG_ERR, "fork failed: %m");
      close(fds[0]);
      close(fds[1]);
      retval = PAM_SYSTEM_ERR;
    }

  sigaction(SIGCHLD, &oldsa, NULL);   /* restore old signal handler */

  return retval;
}
#endif

/* This module saves the current hashed password in /etc/security/opasswd
   and then compares the new password with all entries in this file. */

int
pam_sm_chauthtok (pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  const char *newpass;
  const char *user;
    int retval, tries;
  options_t options;

  memset (&options, 0, sizeof (options));

  /* Set some default values, which could be overwritten later.  */
  options.remember = 10;
  options.tries = 1;

  parse_config_file(pamh, argc, argv, &options);

  /* Parse parameters for module */
  for ( ; argc-- > 0; argv++)
    parse_option (pamh, *argv, &options);

  if (options.debug)
    pam_syslog (pamh, LOG_DEBUG, "pam_sm_chauthtok entered");

  if (options.remember == 0)
    return PAM_IGNORE;

  retval = pam_get_user (pamh, &user, NULL);
  if (retval != PAM_SUCCESS)
    return retval;

  if (flags & PAM_PRELIM_CHECK)
    {
      if (options.debug)
	pam_syslog (pamh, LOG_DEBUG,
		    "pam_sm_chauthtok(PAM_PRELIM_CHECK)");

      return PAM_SUCCESS;
    }

  retval = save_old_pass (pamh, user, options.remember, options.filename, options.debug);

#ifdef WITH_SELINUX
  if (retval == PAM_PWHISTORY_RUN_HELPER)
      retval = run_save_helper(pamh, user, options.remember, options.filename, options.debug);
#endif

  if (retval != PAM_SUCCESS)
    return retval;

  newpass = NULL;
  tries = 0;
  while ((newpass == NULL) && (tries < options.tries))
    {
      retval = pam_get_authtok (pamh, PAM_AUTHTOK, &newpass, NULL);
      if (retval != PAM_SUCCESS && retval != PAM_TRY_AGAIN)
	{
	  if (retval == PAM_CONV_AGAIN)
	    retval = PAM_INCOMPLETE;
	  return retval;
	}
      tries++;

      if (options.debug)
	{
	  if (newpass)
	    pam_syslog (pamh, LOG_DEBUG, "got new auth token");
	  else
	    pam_syslog (pamh, LOG_DEBUG, "got no auth token");
	}

      if (newpass == NULL || retval == PAM_TRY_AGAIN)
	continue;

      if (options.debug)
	pam_syslog (pamh, LOG_DEBUG, "check against old password file");

      retval = check_old_pass (pamh, user, newpass, options.filename, options.debug);
#ifdef WITH_SELINUX
      if (retval == PAM_PWHISTORY_RUN_HELPER)
	  retval = run_check_helper(pamh, user, newpass, options.filename, options.debug);
#endif

      if (retval != PAM_SUCCESS)
	{
	  if (getuid() || options.enforce_for_root ||
	      (flags & PAM_CHANGE_EXPIRED_AUTHTOK))
	    {
	      pam_error (pamh,
		         _("Password has been already used. Choose another."));
	      newpass = NULL;
	      /* Remove password item, else following module will use it */
	      pam_set_item (pamh, PAM_AUTHTOK, (void *) NULL);
	    }
	  else
	    pam_info (pamh,
		       _("Password has been already used."));
	}
    }

  if (newpass == NULL && tries >= options.tries)
    {
      if (options.debug)
	pam_syslog (pamh, LOG_DEBUG, "Aborted, too many tries");
      return PAM_MAXTRIES;
    }

  return PAM_SUCCESS;
}
