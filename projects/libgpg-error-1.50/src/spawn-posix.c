/* exechelp.c - Fork and exec helpers for POSIX
 * Copyright (C) 2004, 2007-2009, 2010 Free Software Foundation, Inc.
 * Copyright (C) 2004, 2006-2012, 2014-2017 g10 Code GmbH
 *
 * This file is part of Libgpg-error.
 *
 * Libgpg-error is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgpg-error is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * This file was originally a part of GnuPG.
 */

#include <config.h>

#if defined(HAVE_W32_SYSTEM)
#error This code is only used on POSIX
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/wait.h>

#ifdef HAVE_GETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif /*HAVE_GETRLIMIT*/

#ifdef HAVE_STAT
# include <sys/stat.h>
#endif

#if __linux__
# include <sys/types.h>
# include <dirent.h>
#endif /*__linux__ */

#include "gpgrt-int.h"


/* Definition for the gpgrt_spawn_actions_t.  Note that there is a
 * different one for Windows.  */
struct gpgrt_spawn_actions {
  int fd[3];
  const int *except_fds;
  char **environ;
  void (*atfork) (void *);
  void *atfork_arg;
};


/* Definition for the gpgrt_process_t.  Note that there is a different
 * one for Windows.  */
struct gpgrt_process {
  const char *pgmname;
  unsigned int terminated   :1; /* or detached */
  unsigned int flags;
  pid_t pid;
  int fd_in;
  int fd_out;
  int fd_err;
  int wstatus;
};



/* Return the maximum number of currently allowed open file
 * descriptors.  Only useful on POSIX systems but returns a value on
 * other systems too.  */
static int
get_max_fds (void)
{
  int max_fds = -1;
#ifdef HAVE_GETRLIMIT
  struct rlimit rl;

  /* Under Linux we can figure out the highest used file descriptor by
   * reading /proc/PID/fd.  This is in the common cases much fast than
   * for example doing 4096 close calls where almost all of them will
   * fail.  On a system with a limit of 4096 files and only 8 files
   * open with the highest number being 10, we speedup close_all_fds
   * from 125ms to 0.4ms including readdir.
   *
   * Another option would be to close the file descriptors as returned
   * from reading that directory - however then we need to snapshot
   * that list before starting to close them.  */
#ifdef __linux__
  {
    DIR *dir = NULL;
    struct dirent *dir_entry;
    const char *s;
    int x;

    dir = opendir ("/proc/self/fd");
    if (dir)
      {
        while ((dir_entry = readdir (dir)))
          {
            s = dir_entry->d_name;
            if ( *s < '0' || *s > '9')
              continue;
            x = atoi (s);
            if (x > max_fds)
              max_fds = x;
          }
        closedir (dir);
      }
    if (max_fds != -1)
      return max_fds + 1;
    }
#endif /* __linux__ */


# ifdef RLIMIT_NOFILE
  if (!getrlimit (RLIMIT_NOFILE, &rl))
    max_fds = rl.rlim_max;
# endif

# ifdef RLIMIT_OFILE
  if (max_fds == -1 && !getrlimit (RLIMIT_OFILE, &rl))
    max_fds = rl.rlim_max;

# endif
#endif /*HAVE_GETRLIMIT*/

#ifdef _SC_OPEN_MAX
  if (max_fds == -1)
    {
      long int scres = sysconf (_SC_OPEN_MAX);
      if (scres >= 0)
        max_fds = scres;
    }
#endif

#ifdef _POSIX_OPEN_MAX
  if (max_fds == -1)
    max_fds = _POSIX_OPEN_MAX;
#endif

#ifdef OPEN_MAX
  if (max_fds == -1)
    max_fds = OPEN_MAX;
#endif

  if (max_fds == -1)
    max_fds = 256;  /* Arbitrary limit.  */

  /* AIX returns INT32_MAX instead of a proper value.  We assume that
     this is always an error and use an arbitrary limit.  */
#ifdef INT32_MAX
  if (max_fds == INT32_MAX)
    max_fds = 256;
#endif

  return max_fds;
}


/* Close all file descriptors starting with descriptor FIRST.  If
 * EXCEPT is not NULL, it is expected to be a list of file descriptors
 * which shall not be closed.  This list shall be sorted in ascending
 * order with the end marked by -1.  */
void
_gpgrt_close_all_fds (int first, const int *except)
{
  int max_fd = get_max_fds ();
  int fd, i, except_start;

  if (except)
    {
      except_start = 0;
      for (fd=first; fd < max_fd; fd++)
        {
          for (i=except_start; except[i] != -1; i++)
            {
              if (except[i] == fd)
                {
                  /* If we found the descriptor in the exception list
                     we can start the next compare run at the next
                     index because the exception list is ordered.  */
                except_start = i + 1;
                break;
                }
            }
          if (except[i] == -1)
            close (fd);
        }
    }
  else
    {
      for (fd=first; fd < max_fd; fd++)
        close (fd);
    }

  _gpg_err_set_errno (0);
}

/* Helper for _gpgrt_make_pipe.  */
static gpg_err_code_t
do_create_pipe (int filedes[2])
{
  gpg_error_t err = 0;

  _gpgrt_pre_syscall ();
  if (pipe (filedes) == -1)
    {
      err = _gpg_err_code_from_syserror ();
      filedes[0] = filedes[1] = -1;
    }
  _gpgrt_post_syscall ();

  return err;
}


/* Helper for _gpgrt_make_pipe.  */
static gpg_err_code_t
do_create_pipe_and_estream (int filedes[2], estream_t *r_fp,
                            int outbound, int nonblock)
{
  gpg_err_code_t err;

  _gpgrt_pre_syscall ();
  if (pipe (filedes) == -1)
    {
      err = _gpg_err_code_from_syserror ();
      _gpgrt_log_info (_("error creating a pipe: %s\n"), _gpg_strerror (err));
      filedes[0] = filedes[1] = -1;
      *r_fp = NULL;
      return err;
    }
  _gpgrt_post_syscall ();

  if (!outbound)
    *r_fp = _gpgrt_fdopen (filedes[0], nonblock? "r,nonblock" : "r");
  else
    *r_fp = _gpgrt_fdopen (filedes[1], nonblock? "w,nonblock" : "w");
  if (!*r_fp)
    {
      err = _gpg_err_code_from_syserror ();
      _gpgrt_log_info (_("error creating a stream for a pipe: %s\n"),
                        _gpg_strerror (err));
      close (filedes[0]);
      close (filedes[1]);
      filedes[0] = filedes[1] = -1;
      return err;
    }
  return 0;
}


/* Create a pipe.  The DIRECTION parameter gives the type of the created pipe:
 *   DIRECTION < 0 := Inbound pipe: On Windows the write end is inheritable.
 *   DIRECTION > 0 := Outbound pipe: On Windows the read end is inheritable.
 * If R_FP is NULL a standard pipe and no stream is created, DIRECTION
 * should then be 0.  */
gpg_err_code_t
_gpgrt_make_pipe (int filedes[2], estream_t *r_fp, int direction,
                  int nonblock)
{
  if (r_fp && direction)
    return do_create_pipe_and_estream (filedes, r_fp,
                                       (direction > 0), nonblock);
  else
    return do_create_pipe (filedes);
}


static gpg_err_code_t
do_create_socketpair (int filedes[2])
{
  gpg_error_t err = 0;

  _gpgrt_pre_syscall ();
  if (socketpair (AF_LOCAL, SOCK_STREAM, 0, filedes) == -1)
    {
      err = _gpg_err_code_from_syserror ();
      filedes[0] = filedes[1] = -1;
    }
  _gpgrt_post_syscall ();

  return err;
}

static int
posix_open_null (int for_write)
{
  int fd;

  fd = open ("/dev/null", for_write? O_WRONLY : O_RDONLY);
  if (fd == -1)
    _gpgrt_log_fatal ("failed to open '/dev/null': %s\n", strerror (errno));
  return fd;
}


static int
my_exec (const char *pgmname, const char *argv[], gpgrt_spawn_actions_t act)
{
  int i;

  /* Assign /dev/null to unused FDs.  */
  for (i = 0; i <= 2; i++)
    if (act->fd[i] == -1)
      act->fd[i] = posix_open_null (i);

  /* Connect the standard files.  */
  for (i = 0; i <= 2; i++)
    if (act->fd[i] != i)
      {
        if (dup2 (act->fd[i], i) == -1)
          _gpgrt_log_fatal ("dup2 std%s failed: %s\n",
                            i==0?"in":i==1?"out":"err", strerror (errno));
        /*
         * We don't close act->fd[i] here, but close them by
         * close_all_fds.  Note that there may be same one in three of
         * act->fd[i].
         */
      }

  /* Close all other files.  */
  _gpgrt_close_all_fds (3, act->except_fds);

  if (act->environ)
    environ = act->environ;

  if (act->atfork)
    act->atfork (act->atfork_arg);

  /* This use case is for assuan pipe connect with no PGMNAME */
  if (pgmname == NULL)
    return 0;

  execv (pgmname, (char *const *)argv);
  /* No way to print anything, as we have may have closed all streams. */
  _exit (127);
  return -1;
}


static gpg_err_code_t
spawn_detached (const char *pgmname, const char *argv[],
                gpgrt_spawn_actions_t act)
{
  gpg_err_code_t ec;
  pid_t pid;

  if (access (pgmname, X_OK))
    {
      ec = _gpg_err_code_from_syserror ();
      xfree (argv);
      return ec;
    }

  _gpgrt_pre_syscall ();
  pid = fork ();
  _gpgrt_post_syscall ();
  if (pid == (pid_t)(-1))
    {
      ec = _gpg_err_code_from_syserror ();
      _gpgrt_log_info (_("error forking process: %s\n"), _gpg_strerror (ec));
      xfree (argv);
      return ec;
    }

  if (!pid)
    {
      pid_t pid2;

      if (setsid() == -1 || chdir ("/"))
        _exit (1);

      pid2 = fork (); /* Double fork to let init take over the new child. */
      if (pid2 == (pid_t)(-1))
        _exit (1);
      if (pid2)
        _exit (0);  /* Let the parent exit immediately. */

      my_exec (pgmname, argv, act);
      /*NOTREACHED*/
    }

  _gpgrt_pre_syscall ();
  if (waitpid (pid, NULL, 0) == -1)
    {
      _gpgrt_post_syscall ();
      ec = _gpg_err_code_from_syserror ();
      _gpgrt_log_info ("waitpid failed in gpgrt_spawn_process_detached: %s",
                        _gpg_strerror (ec));
      return ec;
    }
  else
    _gpgrt_post_syscall ();

  return 0;
}

gpg_err_code_t
_gpgrt_spawn_actions_new (gpgrt_spawn_actions_t *r_act)
{
  gpgrt_spawn_actions_t act;
  int i;

  *r_act = NULL;

  act = xtrycalloc (1, sizeof (struct gpgrt_spawn_actions));
  if (act == NULL)
    return _gpg_err_code_from_syserror ();

  for (i = 0; i <= 2; i++)
    act->fd[i] = -1;

  *r_act = act;
  return 0;
}

void
_gpgrt_spawn_actions_release (gpgrt_spawn_actions_t act)
{
  if (!act)
    return;

  xfree (act);
}

void
_gpgrt_spawn_actions_set_environ (gpgrt_spawn_actions_t act,
                                  char **environ_for_child)
{
  act->environ = environ_for_child;
}

void
_gpgrt_spawn_actions_set_atfork (gpgrt_spawn_actions_t act,
                                 void (*atfork)(void *), void *arg)
{
  act->atfork = atfork;
  act->atfork_arg = arg;
}

void
_gpgrt_spawn_actions_set_redirect (gpgrt_spawn_actions_t act,
                                   int in, int out, int err)
{
  act->fd[0] = in;
  act->fd[1] = out;
  act->fd[2] = err;
}

void
_gpgrt_spawn_actions_set_inherit_fds (gpgrt_spawn_actions_t act,
                                      const int *fds)
{
  act->except_fds = fds;
}


gpg_err_code_t
_gpgrt_process_spawn (const char *pgmname, const char *argv1[],
                      unsigned int flags, gpgrt_spawn_actions_t act,
                      gpgrt_process_t *r_process)
{
  gpg_err_code_t ec;
  gpgrt_process_t process;
  int fd_in[2];
  int fd_out[2];
  int fd_err[2];
  pid_t pid;
  const char **argv;
  int i, j;
  struct gpgrt_spawn_actions act_default;

  if (!act)
    {
      memset (&act_default, 0, sizeof (act_default));
      for (i = 0; i <= 2; i++)
        act_default.fd[i] = -1;
      act = &act_default;
    }

  if (r_process)
    *r_process = NULL;

  /* Create the command line argument array.  */
  i = 0;
  if (argv1)
    while (argv1[i])
      i++;
  argv = xtrycalloc (i+2, sizeof *argv);
  if (!argv)
    return _gpg_err_code_from_syserror ();
  if (pgmname)
    argv[0] = strrchr (pgmname, '/');
  if (argv[0])
    argv[0]++;
  else
    argv[0] = pgmname;

  if (argv1)
    for (i=0, j=1; argv1[i]; i++, j++)
      argv[j] = argv1[i];

  if ((flags & GPGRT_PROCESS_DETACHED))
    {
      if ((flags & GPGRT_PROCESS_STDFDS_SETTING))
        {
          xfree (argv);
          return GPG_ERR_INV_FLAG;
        }

      /* In detached case, it must be no R_PROCESS.  */
      if (r_process || pgmname == NULL)
        {
          xfree (argv);
          return GPG_ERR_INV_ARG;
        }

      if (!(flags & GPGRT_PROCESS_NO_EUID_CHECK))
        {
          if (getuid() != geteuid())
            {
              xfree (argv);
              return GPG_ERR_FORBIDDEN;
            }
        }

      return spawn_detached (pgmname, argv, act);
    }

  process = xtrycalloc (1, sizeof (struct gpgrt_process));
  if (process == NULL)
    {
      xfree (argv);
      return _gpg_err_code_from_syserror ();
    }

  process->pgmname = pgmname;
  process->flags = flags;

  if ((flags & GPGRT_PROCESS_STDINOUT_SOCKETPAIR))
    {
      ec = do_create_socketpair (fd_in);
      if (ec)
        {
          xfree (process);
          xfree (argv);
          return ec;
        }
      fd_out[0] = dup (fd_in[0]);
      fd_out[1] = dup (fd_in[1]);
    }
  else
    {
      if ((flags & GPGRT_PROCESS_STDIN_PIPE))
        {
          ec = do_create_pipe (fd_in);
          if (ec)
            {
              xfree (process);
              xfree (argv);
              return ec;
            }
        }
      else if ((flags & GPGRT_PROCESS_STDIN_KEEP))
        {
          fd_in[0] = 0;
          fd_in[1] = -1;
        }
      else
        {
          fd_in[0] = -1;
          fd_in[1] = -1;
        }

      if ((flags & GPGRT_PROCESS_STDOUT_PIPE))
        {
          ec = do_create_pipe (fd_out);
          if (ec)
            {
              if (fd_in[0] >= 0 && fd_in[0] != 0)
                close (fd_in[0]);
              if (fd_in[1] >= 0)
                close (fd_in[1]);
              xfree (process);
              xfree (argv);
              return ec;
            }
        }
      else if ((flags & GPGRT_PROCESS_STDOUT_KEEP))
        {
          fd_out[0] = -1;
          fd_out[1] = 1;
        }
      else
        {
          fd_out[0] = -1;
          fd_out[1] = -1;
        }
    }

  if ((flags & GPGRT_PROCESS_STDERR_PIPE))
    {
      ec = do_create_pipe (fd_err);
      if (ec)
        {
          if (fd_in[0] >= 0 && fd_in[0] != 0)
            close (fd_in[0]);
          if (fd_in[1] >= 0)
            close (fd_in[1]);
          if (fd_out[0] >= 0)
            close (fd_out[0]);
          if (fd_out[1] >= 0 && fd_out[1] != 1)
            close (fd_out[1]);
          xfree (process);
          xfree (argv);
          return ec;
        }
    }
  else if ((flags & GPGRT_PROCESS_STDERR_KEEP))
    {
      fd_err[0] = -1;
      fd_err[1] = 2;
    }
  else
    {
      fd_err[0] = -1;
      fd_err[1] = -1;
    }

  _gpgrt_pre_syscall ();
  pid = fork ();
  _gpgrt_post_syscall ();
  if (pid == (pid_t)(-1))
    {
      ec = _gpg_err_code_from_syserror ();
      _gpgrt_log_info (_("error forking process: %s\n"), _gpg_strerror (ec));
      if (fd_in[0] >= 0 && fd_in[0] != 0)
        close (fd_in[0]);
      if (fd_in[1] >= 0)
        close (fd_in[1]);
      if (fd_out[0] >= 0)
        close (fd_out[0]);
      if (fd_out[1] >= 0 && fd_out[1] != 1)
        close (fd_out[1]);
      if (fd_err[0] >= 0)
        close (fd_err[0]);
      if (fd_err[1] >= 0 && fd_err[1] != 2)
        close (fd_err[1]);
      xfree (process);
      xfree (argv);
      return ec;
    }

  if (!pid)
    {
      if (fd_in[1] >= 0)
        close (fd_in[1]);
      if (fd_out[0] >= 0)
        close (fd_out[0]);
      if (fd_err[0] >= 0)
        close (fd_err[0]);

      if (act->fd[0] < 0)
        act->fd[0] = fd_in[0];
      if (act->fd[1] < 0)
        act->fd[1] = fd_out[1];
      if (act->fd[2] < 0)
        act->fd[2] = fd_err[1];

      /* Run child. */
      if (!my_exec (pgmname, argv, act))
        {
          xfree (process);
          xfree (argv);
          *r_process = NULL;
          return 0;
        }

      /*NOTREACHED*/
    }

  xfree (argv);
  process->pid = pid;

  if (fd_in[0] >= 0 && fd_in[0] != 0)
    close (fd_in[0]);
  if (fd_out[1] >= 0 && fd_out[1] != 1)
    close (fd_out[1]);
  if (fd_err[1] >= 0 && fd_err[1] != 2)
    close (fd_err[1]);
  process->fd_in = fd_in[1];
  process->fd_out = fd_out[0];
  process->fd_err = fd_err[0];
  process->wstatus = -1;
  process->terminated = 0;

  if (r_process == NULL)
    {
      ec = _gpgrt_process_wait (process, 1);
      _gpgrt_process_release (process);
      return ec;
    }

  *r_process = process;
  return 0;
}

static gpg_err_code_t
process_kill (gpgrt_process_t process, int sig)
{
  gpg_err_code_t ec = 0;
  pid_t pid = process->pid;

  _gpgrt_pre_syscall ();
  if (kill (pid, sig) < 0)
    ec = _gpg_err_code_from_syserror ();
  _gpgrt_post_syscall ();
  return ec;
}

gpg_err_code_t
_gpgrt_process_terminate (gpgrt_process_t process)
{
  return process_kill (process, SIGTERM);
}

gpg_err_code_t
_gpgrt_process_get_fds (gpgrt_process_t process, unsigned int flags,
                        int *r_fd_in, int *r_fd_out, int *r_fd_err)
{
  (void)flags;
  if (r_fd_in)
    {
      *r_fd_in = process->fd_in;
      process->fd_in = -1;
    }
  if (r_fd_out)
    {
      *r_fd_out = process->fd_out;
      process->fd_out = -1;
    }
  if (r_fd_err)
    {
      *r_fd_err = process->fd_err;
      process->fd_err = -1;
    }

  return 0;
}

gpg_err_code_t
_gpgrt_process_get_streams (gpgrt_process_t process, unsigned int flags,
                            gpgrt_stream_t *r_fp_in, gpgrt_stream_t *r_fp_out,
                            gpgrt_stream_t *r_fp_err)
{
  int nonblock = (flags & GPGRT_PROCESS_STREAM_NONBLOCK)? 1: 0;

  if (r_fp_in)
    {
      *r_fp_in = _gpgrt_fdopen (process->fd_in, nonblock? "w,nonblock" : "w");
      process->fd_in = -1;
    }
  if (r_fp_out)
    {
      *r_fp_out = _gpgrt_fdopen (process->fd_out, nonblock? "r,nonblock" : "r");
      process->fd_out = -1;
    }
  if (r_fp_err)
    {
      *r_fp_err = _gpgrt_fdopen (process->fd_err, nonblock? "r,nonblock" : "r");
      process->fd_err = -1;
    }
  return 0;
}

gpg_err_code_t
_gpgrt_process_vctl (gpgrt_process_t process, unsigned int request,
                     va_list arg_ptr)
{
  switch (request)
    {
    case GPGRT_PROCESS_NOP:
      return 0;

    case GPGRT_PROCESS_GET_PROC_ID:
      {
        int *r_id = va_arg (arg_ptr, int *);

        if (r_id == NULL)
          return GPG_ERR_INV_VALUE;

        *r_id = (int)process->pid;
        return 0;
      }

    case GPGRT_PROCESS_GET_EXIT_ID:
      {
        int status = process->wstatus;
        int *r_exit_status = va_arg (arg_ptr, int *);

        if (!process->terminated)
          return GPG_ERR_UNFINISHED;

        if (WIFEXITED (status))
          {
            if (r_exit_status)
              *r_exit_status = WEXITSTATUS (status);
          }
        else
          *r_exit_status = -1;

        return 0;
      }

    case GPGRT_PROCESS_GET_PID:
      {
        pid_t *r_pid = va_arg (arg_ptr, pid_t *);

        if (r_pid == NULL)
          return GPG_ERR_INV_VALUE;

        *r_pid = process->pid;
        return 0;
      }

    case GPGRT_PROCESS_GET_WSTATUS:
      {
        int status = process->wstatus;
        int *r_if_exited = va_arg (arg_ptr, int *);
        int *r_if_signaled = va_arg (arg_ptr, int *);
        int *r_exit_status = va_arg (arg_ptr, int *);
        int *r_termsig = va_arg (arg_ptr, int *);

        if (!process->terminated)
          return GPG_ERR_UNFINISHED;

        if (WIFEXITED (status))
          {
            if (r_if_exited)
              *r_if_exited = 1;
            if (r_if_signaled)
              *r_if_signaled = 0;
            if (r_exit_status)
              *r_exit_status = WEXITSTATUS (status);
            if (r_termsig)
              *r_termsig = 0;
          }
        else if (WIFSIGNALED (status))
          {
            if (r_if_exited)
              *r_if_exited = 0;
            if (r_if_signaled)
              *r_if_signaled = 1;
            if (r_exit_status)
              *r_exit_status = 0;
            if (r_termsig)
              *r_termsig = WTERMSIG (status);
          }

        return 0;
      }

    case GPGRT_PROCESS_KILL:
      {
        int sig = va_arg (arg_ptr, int);

        return process_kill (process, sig);
      }

    default:
      break;
    }

  return GPG_ERR_UNKNOWN_COMMAND;
}

gpg_err_code_t
_gpgrt_process_wait (gpgrt_process_t process, int hang)
{
  gpg_err_code_t ec;
  int status;
  pid_t pid;

  if (process->terminated)
    /* Already terminated.  */
    return 0;

  _gpgrt_pre_syscall ();
  while ((pid = waitpid (process->pid, &status, hang? 0: WNOHANG))
         == (pid_t)(-1) && errno == EINTR);
  _gpgrt_post_syscall ();

  if (pid == (pid_t)(-1))
    {
      ec = _gpg_err_code_from_syserror ();
      _gpgrt_log_info (_("waiting for process %d failed: %s\n"),
                        (int)pid, _gpg_strerror (ec));
    }
  else if (!pid)
    {
      ec = GPG_ERR_TIMEOUT; /* Still running.  */
    }
  else
    {
      process->terminated = 1;
      process->wstatus = status;
      ec = 0;
    }

  return ec;
}

void
_gpgrt_process_release (gpgrt_process_t process)
{
  if (!process)
    return;

  if (!process->terminated)
    {
      _gpgrt_process_terminate (process);
      _gpgrt_process_wait (process, 1);
    }

  xfree (process);
}

gpg_err_code_t
_gpgrt_process_wait_list (gpgrt_process_t *process_list, int count, int hang)
{
  gpg_err_code_t ec = 0;
  int i;

  for (i = 0; i < count; i++)
    {
      if (process_list[i]->terminated)
        continue;

      ec = _gpgrt_process_wait (process_list[i], hang);
      if (ec)
        break;
    }

  return ec;
}
