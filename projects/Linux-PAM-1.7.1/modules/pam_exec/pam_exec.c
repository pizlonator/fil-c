/*
 * Copyright (c) 2006, 2008 Thorsten Kukuk <kukuk@thkukuk.de>
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

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include <security/_pam_macros.h>
#include "pam_inline.h"
#include "pam_i18n.h"

#define ENV_ITEM(n) { (n), #n }
static struct {
  int item;
  const char *name;
} env_items[] = {
  ENV_ITEM(PAM_SERVICE),
  ENV_ITEM(PAM_USER),
  ENV_ITEM(PAM_TTY),
  ENV_ITEM(PAM_RHOST),
  ENV_ITEM(PAM_RUSER),
};

/* move_fd_to_non_stdio copies the given file descriptor to something other
 * than stdin, stdout, or stderr.  Assumes that the caller will close all
 * unwanted fds after calling. */
static int
move_fd_to_non_stdio (pam_handle_t *pamh, int fd)
{
  while (fd < 3)
    {
      fd = dup(fd);
      if (fd == -1)
	{
	  int err = errno;
	  pam_syslog (pamh, LOG_ERR, "dup failed: %m");
	  _exit (err);
	}
    }
  return fd;
}

static int
call_exec (const char *pam_type, pam_handle_t *pamh,
	   int argc, const char **argv)
{
  int debug = 0;
  int call_setuid = 0;
  int quiet = 0;
  int quiet_log = 0;
  int expose_authtok = 0;
  int use_stdout = 0;
  int optargc;
  const char *logfile = NULL;
  char authtok[PAM_MAX_RESP_SIZE] = {};
  pid_t pid;
  int fds[2];
  int stdout_fds[2];
  FILE *stdout_file = NULL;
  int retval;
  const char *name;
  struct sigaction newsa, oldsa;

  if (argc < 1) {
    pam_syslog (pamh, LOG_ERR,
		"This module needs at least one argument");
    return PAM_SERVICE_ERR;
  }

  for (optargc = 0; optargc < argc; optargc++)
    {
      const char *str;

      if (argv[optargc][0] == '/') /* paths starts with / */
	break;

      if (strcasecmp (argv[optargc], "debug") == 0)
	debug = 1;
      else if (strcasecmp (argv[optargc], "stdout") == 0)
	use_stdout = 1;
      else if ((str = pam_str_skip_icase_prefix (argv[optargc], "log=")) != NULL)
	logfile = str;
      else if ((str = pam_str_skip_icase_prefix (argv[optargc], "type=")) != NULL)
	{
	  if (strcmp (pam_type, str) != 0)
	    return PAM_IGNORE;
	}
      else if (strcasecmp (argv[optargc], "seteuid") == 0)
	call_setuid = 1;
      else if (strcasecmp (argv[optargc], "quiet") == 0)
	quiet = 1;
      else if (strcasecmp (argv[optargc], "quiet_log") == 0)
	quiet_log = 1;
      else if (strcasecmp (argv[optargc], "expose_authtok") == 0)
	expose_authtok = 1;
      else
	break; /* Unknown option, assume program to execute. */
    }

  /* Request user name to be available. */

  retval = pam_get_user(pamh, &name, NULL);
  if (retval != PAM_SUCCESS)
    {
      if (retval == PAM_CONV_AGAIN)
        retval = PAM_INCOMPLETE;
      return retval;
    }

  if (expose_authtok == 1)
    {
      if (strcmp (pam_type, "auth") != 0 && strcmp (pam_type, "password") != 0)
	{
	  pam_syslog (pamh, LOG_ERR,
		      "expose_authtok not supported for type %s", pam_type);
	  expose_authtok = 0;
	}
      else
	{
	  const void *void_pass;

	  retval = pam_get_item (pamh, PAM_AUTHTOK, &void_pass);
	  if (retval != PAM_SUCCESS)
	    {
	      if (debug)
		pam_syslog (pamh, LOG_DEBUG,
			    "pam_get_item (PAM_AUTHTOK) failed, return %d",
			    retval);
	      return retval;
	    }
	  else if (void_pass == NULL)
	    {
	      char *resp = NULL;

	      retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF,
				   &resp, _("Password: "));

	      if (retval != PAM_SUCCESS)
		{
		  pam_overwrite_string (resp);
		  _pam_drop (resp);
		  if (retval == PAM_CONV_AGAIN)
		    retval = PAM_INCOMPLETE;
		  return retval;
		}

	      if (resp)
		{
		  pam_set_item (pamh, PAM_AUTHTOK, resp);
		  strncpy (authtok, resp, sizeof(authtok) - 1);
		  pam_overwrite_string (resp);
		  _pam_drop (resp);
		}
	    }
	  else
	    strncpy (authtok, void_pass, sizeof(authtok) - 1);

	  if (pipe(fds) != 0)
	    {
	      pam_overwrite_array(authtok);
	      pam_syslog (pamh, LOG_ERR, "Could not create pipe: %m");
	      return PAM_SYSTEM_ERR;
	    }
	}
    }

  if (use_stdout)
    {
      if (pipe(stdout_fds) != 0)
	{
	  pam_overwrite_array(authtok);
	  pam_syslog (pamh, LOG_ERR, "Could not create pipe: %m");
	  return PAM_SYSTEM_ERR;
	}
      stdout_file = fdopen(stdout_fds[0], "r");
      if (!stdout_file)
	{
	  pam_overwrite_array(authtok);
	  pam_syslog (pamh, LOG_ERR, "Could not fdopen pipe: %m");
	  return PAM_SYSTEM_ERR;
	}
    }

  if (optargc >= argc) {
    pam_overwrite_array(authtok);
    pam_syslog (pamh, LOG_ERR, "No path given as argument");
    return PAM_SERVICE_ERR;
  }

  memset(&newsa, '\0', sizeof(newsa));
  newsa.sa_handler = SIG_DFL;
  if (sigaction(SIGCHLD, &newsa, &oldsa) == -1) {
    pam_overwrite_array(authtok);
    pam_syslog(pamh, LOG_ERR, "failed to reset SIGCHLD handler: %m");
    return PAM_SYSTEM_ERR;
  }

  pid = fork();
  if (pid == -1) {
    pam_overwrite_array(authtok);
    return PAM_SYSTEM_ERR;
  }
  if (pid > 0) /* parent */
    {
      int status = 0;
      pid_t rc;

      if (expose_authtok) /* send the password to the child */
	{
	  if (debug)
	    pam_syslog (pamh, LOG_DEBUG, "send password to child");
	  if (write(fds[1], authtok, strlen(authtok)) == -1)
	    pam_syslog (pamh, LOG_ERR,
			      "sending password to child failed: %m");

          close(fds[0]);       /* close here to avoid possible SIGPIPE above */
          close(fds[1]);
	}

      pam_overwrite_array(authtok);

      if (use_stdout)
	{
	  char *buf = NULL;
	  size_t n = 0;
	  close(stdout_fds[1]);
	  while (getline(&buf, &n, stdout_file) != -1)
	    {
	      buf[strcspn(buf, "\n")] = '\0';
	      pam_info(pamh, "%s", buf);
	    }
	  free(buf);
	  fclose(stdout_file);
	}

      while ((rc = waitpid (pid, &status, 0)) == -1 &&
	     errno == EINTR);
      sigaction(SIGCHLD, &oldsa, NULL);   /* restore old signal handler */
      if (rc == (pid_t)-1)
	{
	  pam_syslog (pamh, LOG_ERR, "waitpid returns with -1: %m");
	  return PAM_SYSTEM_ERR;
	}
      else if (status != 0)
	{
	  if (WIFEXITED(status))
	    {
		if (!quiet_log)
	      pam_syslog (pamh, LOG_ERR, "%s failed: exit code %d",
			  argv[optargc], WEXITSTATUS(status));
		if (!quiet)
	      pam_error (pamh, _("%s failed: exit code %d"),
			 argv[optargc], WEXITSTATUS(status));
	    }
	  else if (WIFSIGNALED(status))
	    {
		if (!quiet_log)
	      pam_syslog (pamh, LOG_ERR, "%s failed: caught signal %d%s",
			  argv[optargc], WTERMSIG(status),
			  WCOREDUMP(status) ? " (core dumped)" : "");
		if (!quiet)
	      pam_error (pamh, _("%s failed: caught signal %d%s"),
			 argv[optargc], WTERMSIG(status),
			 WCOREDUMP(status) ? " (core dumped)" : "");
	    }
	  else
	    {
		if (!quiet_log)
	      pam_syslog (pamh, LOG_ERR, "%s failed: unknown status 0x%x",
			  argv[optargc], status);
		if (!quiet)
	      pam_error (pamh, _("%s failed: unknown status 0x%x"),
			 argv[optargc], status);
	    }
	  return PAM_SYSTEM_ERR;
	}
      return PAM_SUCCESS;
    }
  else /* child */
    {
      const char **arggv;
      int i;
      char **envlist;
      int envlen, nitems;
      char *envstr;
      enum pam_modutil_redirect_fd redirect_stdin =
	      expose_authtok ? PAM_MODUTIL_IGNORE_FD : PAM_MODUTIL_PIPE_FD;
      enum pam_modutil_redirect_fd redirect_stdout =
	      (use_stdout || logfile) ? PAM_MODUTIL_IGNORE_FD : PAM_MODUTIL_NULL_FD;

      pam_overwrite_array(authtok);

      /* First, move all the pipes off of stdin, stdout, and stderr, to ensure
       * that calls to dup2 won't close them. */

      if (expose_authtok)
	{
	  fds[0] = move_fd_to_non_stdio(pamh, fds[0]);
	  close(fds[1]);
	}

      if (use_stdout)
	{
	  stdout_fds[1] = move_fd_to_non_stdio(pamh, stdout_fds[1]);
	  close(stdout_fds[0]);
	}

      /* Set up stdin. */

      if (expose_authtok)
	{
	  /* reopen stdin as pipe */
	  if (dup2(fds[0], STDIN_FILENO) == -1)
	    {
	      int err = errno;
	      pam_syslog (pamh, LOG_ERR, "dup2 of STDIN failed: %m");
	      _exit (err);
	    }
	}

      /* Set up stdout. */

      if (use_stdout)
	{
	  if (dup2(stdout_fds[1], STDOUT_FILENO) == -1)
	    {
	      int err = errno;
	      pam_syslog (pamh, LOG_ERR, "dup2 to stdout failed: %m");
	      _exit (err);
	    }
	}
      else if (logfile)
	{
	  time_t tm = time (NULL);
	  char *buffer;

	  close (STDOUT_FILENO);
	  if ((i = open (logfile, O_CREAT|O_APPEND|O_WRONLY,
			 S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
	    {
	      int err = errno;
	      pam_syslog (pamh, LOG_ERR, "open of %s failed: %m",
			  logfile);
	      _exit (err);
	    }
	  if (i != STDOUT_FILENO)
	    {
	      if (dup2 (i, STDOUT_FILENO) == -1)
		{
		  int err = errno;
		  pam_syslog (pamh, LOG_ERR, "dup2 failed: %m");
		  _exit (err);
		}
	      close (i);
	    }
	  if ((buffer = pam_asprintf ("*** %s", ctime (&tm))) != NULL)
	    {
	      pam_modutil_write (STDOUT_FILENO, buffer, strlen (buffer));
	      free (buffer);
	    }
	}

      if ((use_stdout || logfile) &&
	  dup2 (STDOUT_FILENO, STDERR_FILENO) == -1)
	{
	  int err = errno;
	  pam_syslog (pamh, LOG_ERR, "dup2 failed: %m");
	  _exit (err);
	}

      if (pam_modutil_sanitize_helper_fds(pamh, redirect_stdin,
					  redirect_stdout, redirect_stdout) < 0)
	_exit(1);

      if (call_setuid)
	if (setuid (geteuid ()) == -1)
	  {
	    int err = errno;
	    pam_syslog (pamh, LOG_ERR, "setuid(%lu) failed: %m",
			(unsigned long) geteuid ());
	    _exit (err);
	  }

      if (setsid () == -1)
	{
	  int err = errno;
	  pam_syslog (pamh, LOG_ERR, "setsid failed: %m");
	  _exit (err);
	}

      arggv = calloc ((size_t) argc + 1, sizeof (char *));
      if (arggv == NULL)
	_exit (ENOMEM);

      for (i = 0; i < (argc - optargc); i++)
        arggv[i] = argv[i+optargc];
      arggv[i] = NULL;

      /*
       * Set up the child's environment list.  It consists of the PAM
       * environment, plus a few hand-picked PAM items.
       */
      envlist = pam_getenvlist(pamh);
      for (envlen = 0; envlist[envlen] != NULL; ++envlen)
        /* nothing */ ;
      nitems = PAM_ARRAY_SIZE(env_items);
      /* + 2 because of PAM_TYPE and NULL entry */
      envlist = realloc(envlist, (envlen + nitems + 2) * sizeof(*envlist));
      if (envlist == NULL)
      {
        pam_syslog (pamh, LOG_CRIT, "realloc environment failed: %m");
        _exit (ENOMEM);
      }
      for (i = 0; i < nitems; ++i)
      {
        const void *item;

        if (pam_get_item(pamh, env_items[i].item, &item) != PAM_SUCCESS || item == NULL)
          continue;
        if ((envstr = pam_asprintf("%s=%s", env_items[i].name, (const char *)item)) == NULL)
        {
          pam_syslog (pamh, LOG_CRIT, "prepare environment failed: %m");
          _exit (ENOMEM);
        }
        envlist[envlen++] = envstr;
        envlist[envlen] = NULL;
      }

      if ((envstr = pam_asprintf("PAM_TYPE=%s", pam_type)) == NULL)
        {
          pam_syslog (pamh, LOG_CRIT, "prepare environment failed: %m");
          _exit (ENOMEM);
        }
      envlist[envlen++] = envstr;
      envlist[envlen] = NULL;

      if (debug)
	pam_syslog (pamh, LOG_DEBUG, "Calling %s ...", arggv[0]);

      DIAG_PUSH_IGNORE_CAST_QUAL;
      execve (arggv[0], (char **) arggv, envlist);
      DIAG_POP_IGNORE_CAST_QUAL;
      i = errno;
      pam_syslog (pamh, LOG_ERR, "execve(%s,...) failed: %m", arggv[0]);
      _exit (i);
    }
  return PAM_SYSTEM_ERR; /* will never be reached. */
}

int
pam_sm_authenticate (pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
  return call_exec ("auth", pamh, argc, argv);
}

int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
		int argc UNUSED, const char **argv UNUSED)
{
  return PAM_IGNORE;
}

/* password updating functions */

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
		 int argc, const char **argv)
{
  if (flags & PAM_PRELIM_CHECK)
    return PAM_SUCCESS;
  return call_exec ("password", pamh, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
  return call_exec ("account", pamh, argc, argv);
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
  return call_exec ("open_session", pamh, argc, argv);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
  return call_exec ("close_session", pamh, argc, argv);
}
