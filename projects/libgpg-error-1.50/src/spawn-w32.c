/* spawn-w32.c - Fork and exec helpers for W32.
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

#if !defined(HAVE_W32_SYSTEM)
#error This code is only used on W32.
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_STAT
# include <sys/stat.h>
#endif
#define WIN32_LEAN_AND_MEAN  /* We only need the OS core stuff.  */
#include <windows.h>

#define NEED_STRUCT_SPAWN_CB_ARG
#include "gpgrt-int.h"

/* Define to 1 do enable debugging.  */
#define DEBUG_W32_SPAWN 0


/* It seems Vista doesn't grok X_OK and so fails access() tests.
 * Previous versions interpreted X_OK as F_OK anyway, so we'll just
 * use F_OK directly. */
#undef X_OK
#define X_OK F_OK

/* For HANDLE and the internal file descriptor (fd) of this module:
 * HANDLE can be represented by an intptr_t which should be true for
 * all systems (HANDLE is defined as void *).  Further, we assume that
 * -1 denotes an invalid handle.
 *
 * Note that a C run-time file descriptor (the exposed one to API) is
 * always represented by an int.
 */
#define fd_to_handle(a)  ((HANDLE)(a))
#define handle_to_fd(a)  ((intptr_t)(a))


/* Definition for the gpgrt_spawn_actions_t.  Note that there is a
 * different one for Unices.  */
struct gpgrt_spawn_actions {
  void *hd[3];
  void **inherit_hds;
  char *env;
};


/* Definition for the gpgrt_process_t.  Note that there is a different
 * one for Unices.  */
struct gpgrt_process {
  const char *pgmname;
  unsigned int terminated:1;  /* or detached */
  unsigned int flags;
  HANDLE hProcess;
  HANDLE hd_in;
  HANDLE hd_out;
  HANDLE hd_err;
  int exitcode;
};



/* Return the maximum number of currently allowed open file
 * descriptors.  Only useful on POSIX systems but returns a value on
 * other systems too.  */
int
get_max_fds (void)
{
  int max_fds = -1;

#ifdef OPEN_MAX
  if (max_fds == -1)
    max_fds = OPEN_MAX;
#endif

  if (max_fds == -1)
    max_fds = 256;  /* Arbitrary limit.  */

  return max_fds;
}


/* Helper function to build_w32_commandline. */
static char *
build_w32_commandline_copy (char *buffer, const char *string)
{
  char *p = buffer;
  const char *s;

  if (!*string) /* Empty string. */
    p = stpcpy (p, "\"\"");
  else if (strpbrk (string, " \t\n\v\f\""))
    {
      /* Need to do some kind of quoting.  */
      p = stpcpy (p, "\"");
      for (s=string; *s; s++)
        {
          *p++ = *s;
          if (*s == '\"')
            *p++ = *s;
        }
      *p++ = '\"';
      *p = 0;
    }
  else
    p = stpcpy (p, string);

  return p;
}


/* Build a command line for use with W32's CreateProcess.  On success
 * CMDLINE gets the address of a newly allocated string.  */
static gpg_err_code_t
build_w32_commandline (const char *pgmname, const char * const *argv,
                       char **cmdline)
{
  int i, n;
  const char *s;
  char *buf, *p;

  *cmdline = NULL;
  n = 0;
  s = pgmname;
  n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting */
  for (; *s; s++)
    if (*s == '\"')
      n++;  /* Need to double inner quotes.  */
  for (i=0; (s=argv[i]); i++)
    {
      n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting */
      for (; *s; s++)
        if (*s == '\"')
          n++;  /* Need to double inner quotes.  */
    }
  n++;

  buf = p = xtrymalloc (n);
  if (!buf)
    return _gpg_err_code_from_syserror ();

  p = build_w32_commandline_copy (p, pgmname);
  for (i=0; argv[i]; i++)
    {
      *p++ = ' ';
      p = build_w32_commandline_copy (p, argv[i]);
    }

  *cmdline= buf;
  return 0;
}


#define INHERIT_READ    1
#define INHERIT_WRITE   2
#define INHERIT_BOTH    (INHERIT_READ|INHERIT_WRITE)

/* Create pipe.  FLAGS indicates which ends are inheritable.  */
static int
create_inheritable_pipe (HANDLE filedes[2], int flags)
{
  HANDLE r, w;
  SECURITY_ATTRIBUTES sec_attr;

  memset (&sec_attr, 0, sizeof sec_attr );
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = TRUE;

  _gpgrt_pre_syscall ();
  if (!CreatePipe (&r, &w, &sec_attr, 0))
    {
      _gpgrt_post_syscall ();
      return -1;
    }
  _gpgrt_post_syscall ();

  if ((flags & INHERIT_READ) == 0)
    if (! SetHandleInformation (r, HANDLE_FLAG_INHERIT, 0))
      goto fail;

  if ((flags & INHERIT_WRITE) == 0)
    if (! SetHandleInformation (w, HANDLE_FLAG_INHERIT, 0))
      goto fail;

  filedes[0] = r;
  filedes[1] = w;
  return 0;

 fail:
  _gpgrt_log_info ("SetHandleInformation failed: ec=%d\n",
                    (int)GetLastError ());
  CloseHandle (r);
  CloseHandle (w);
  return -1;
}


static HANDLE
w32_open_null (int for_write)
{
  HANDLE hfile;

  hfile = CreateFileW (L"nul",
                       for_write? GENERIC_WRITE : GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
  if (hfile == INVALID_HANDLE_VALUE)
    _gpgrt_log_debug ("can't open 'nul': ec=%d\n", (int)GetLastError ());
  return hfile;
}


static gpg_err_code_t
do_create_pipe_and_estream (int filedes[2],
                            estream_t *r_fp, int direction, int nonblock)
{
  gpg_err_code_t err = 0;
  int flags;
  HANDLE fds[2];
  gpgrt_syshd_t syshd;

  if (direction < 0)
    flags = INHERIT_WRITE;
  else if (direction > 0)
    flags = INHERIT_READ;
  else
    flags = INHERIT_BOTH;

  filedes[0] = filedes[1] = -1;
  err = GPG_ERR_GENERAL;
  if (!create_inheritable_pipe (fds, flags))
    {
      filedes[0] = _open_osfhandle (handle_to_fd (fds[0]), O_RDONLY);
      if (filedes[0] == -1)
        {
          _gpgrt_log_info ("failed to translate osfhandle %p\n", fds[0]);
          CloseHandle (fds[1]);
        }
      else
        {
          filedes[1] = _open_osfhandle (handle_to_fd (fds[1]), O_APPEND);
          if (filedes[1] == -1)
            {
              _gpgrt_log_info ("failed to translate osfhandle %p\n", fds[1]);
              close (filedes[0]);
              filedes[0] = -1;
              CloseHandle (fds[1]);
            }
          else
            err = 0;
        }
    }

  if (! err && r_fp)
    {
      syshd.type = ES_SYSHD_HANDLE;
      if (direction < 0)
        {
          syshd.u.handle = fds[0];
          *r_fp = _gpgrt_sysopen (&syshd, nonblock? "r,nonblock" : "r");
        }
      else
        {
          syshd.u.handle = fds[1];
          *r_fp = _gpgrt_sysopen (&syshd, nonblock? "w,nonblock" : "w");
        }
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
    }

  return err;
}


/* Create a pipe.  The DIRECTION parameter gives the type of the created pipe:
 *   DIRECTION < 0 := Inbound pipe: On Windows the write end is inheritable.
 *   DIRECTION > 0 := Outbound pipe: On Windows the read end is inheritable.
 * If R_FP is NULL a standard pipe and no stream is created, DIRECTION
 * should then be 0.  */
gpg_err_code_t
_gpgrt_make_pipe (int filedes[2], estream_t *r_fp, int direction, int nonblock)
{
  if (r_fp && direction)
    return do_create_pipe_and_estream (filedes, r_fp, direction, nonblock);
  else
    return do_create_pipe_and_estream (filedes, NULL, 0, 0);
}


/*
 * Check if STARTUPINFOEXW supports PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
 */
static int
check_windows_version (void)
{
  static int is_vista_or_later = -1;

  OSVERSIONINFO osvi;

  if (is_vista_or_later == -1)
    {
      memset (&osvi,0,sizeof(osvi));
      osvi.dwOSVersionInfoSize = sizeof(osvi);
      GetVersionEx (&osvi);

      /* The feature is available on Vista or later.  */
      is_vista_or_later = (osvi.dwMajorVersion >= 6);
    }

  return is_vista_or_later;
}


static gpg_err_code_t
spawn_detached (const char *pgmname, char *cmdline, gpgrt_spawn_actions_t act)
{
  SECURITY_ATTRIBUTES sec_attr;
  PROCESS_INFORMATION pi = { NULL, 0, 0, 0 };
  STARTUPINFOEXW si;
  int cr_flags;
  wchar_t *wcmdline = NULL;
  wchar_t *wpgmname = NULL;
  gpg_err_code_t ec;
  int ret;
  BOOL ask_inherit = FALSE;
  int i;

  ec = _gpgrt_access (pgmname, X_OK);
  if (ec)
    {
      xfree (cmdline);
      return ec;
    }

  memset (&si, 0, sizeof si);

  i = 0;
  if (act->hd[0] != INVALID_HANDLE_VALUE)
    i++;
  if (act->hd[1] != INVALID_HANDLE_VALUE)
    i++;
  if (act->hd[2] != INVALID_HANDLE_VALUE)
    i++;

  if (i != 0 || act->inherit_hds)
    {
      SIZE_T attr_list_size = 0;
      HANDLE hd[32];
      HANDLE *hd_p = act->inherit_hds;
      int j = 0;

      if (act->hd[0] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[0];
      if (act->hd[1] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[1];
      if (act->hd[2] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[2];
      if (hd_p)
        {
          while (*hd_p != INVALID_HANDLE_VALUE)
            if (j < DIM (hd))
              hd[j++] = *hd_p++;
            else
              {
                _gpgrt_log_info ("gpgrt_spawn_detached: too many handles\n");
                break;
              }
        }

      if (j)
        {
          if (check_windows_version ())
            {
              InitializeProcThreadAttributeList (NULL, 1, 0, &attr_list_size);
              si.lpAttributeList = xtrymalloc (attr_list_size);
              if (si.lpAttributeList == NULL)
                {
                  xfree (cmdline);
                  return _gpg_err_code_from_syserror ();
                }
              InitializeProcThreadAttributeList (si.lpAttributeList, 1, 0,
                                                 &attr_list_size);
              UpdateProcThreadAttribute (si.lpAttributeList, 0,
                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                         hd, sizeof (HANDLE) * j, NULL, NULL);
            }
          ask_inherit = TRUE;
        }
    }

  /* Prepare security attributes.  */
  memset (&sec_attr, 0, sizeof sec_attr );
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = FALSE;

  /* Start the process.  */
  si.StartupInfo.cb = sizeof (si);
  si.StartupInfo.dwFlags = ((i > 0 ? STARTF_USESTDHANDLES : 0)
                            | STARTF_USESHOWWINDOW);
  si.StartupInfo.wShowWindow = DEBUG_W32_SPAWN? SW_SHOW : SW_MINIMIZE;
  si.StartupInfo.hStdInput  = act->hd[0];
  si.StartupInfo.hStdOutput = act->hd[1];
  si.StartupInfo.hStdError  = act->hd[2];

  cr_flags = (CREATE_DEFAULT_ERROR_MODE
              | GetPriorityClass (GetCurrentProcess ())
              | CREATE_NEW_PROCESS_GROUP
              | DETACHED_PROCESS);

  /* Take care: CreateProcessW may modify wpgmname */
  if (!(wpgmname = _gpgrt_utf8_to_wchar (pgmname)))
    ret = 0;
  else if (!(wcmdline = _gpgrt_utf8_to_wchar (cmdline)))
    ret = 0;
  else
    ret = CreateProcessW (wpgmname,      /* Program to start.  */
                          wcmdline,      /* Command line arguments.  */
                          &sec_attr,     /* Process security attributes.  */
                          &sec_attr,     /* Thread security attributes.  */
                          ask_inherit,   /* Inherit handles.  */
                          cr_flags,      /* Creation flags.  */
                          act->env,      /* Environment.  */
                          NULL,          /* Use current drive/directory.  */
                          (STARTUPINFOW *)&si,    /* Startup information. */
                          &pi            /* Returns process information.  */
                          );
  if (!ret)
    {
      if (!wpgmname || !wcmdline)
        _gpgrt_log_info ("gpgrt_spawn_detached: "
                         "CreateProcess failed (utf8_to_wchar): %s\n",
                          strerror (errno));
      else
        _gpgrt_log_info ("gpgrt_spawn_detached: "
                         "CreateProcess(detached) failed: %d\n",
                          (int)GetLastError ());
      xfree (wpgmname);
      xfree (wcmdline);
      xfree (cmdline);
      return GPG_ERR_GENERAL;
    }
  if (si.lpAttributeList)
    DeleteProcThreadAttributeList (si.lpAttributeList);
  xfree (wpgmname);
  xfree (wcmdline);
  xfree (cmdline);

  /* log_debug ("CreateProcess(detached) ready: hProcess=%p hThread=%p" */
  /*           " dwProcessID=%d dwThreadId=%d\n", */
  /*           pi.hProcess, pi.hThread, */
  /*          (int) pi.dwProcessId, (int) pi.dwThreadId); */

  /* Note: AllowSetForegroundWindow doesn't make sense for background
     process.  */

  CloseHandle (pi.hThread);
  CloseHandle (pi.hProcess);
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
    act->hd[i] = INVALID_HANDLE_VALUE;

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
_gpgrt_spawn_actions_set_envvars (gpgrt_spawn_actions_t act, char *env)
{
  act->env = env;
}

void
_gpgrt_spawn_actions_set_redirect (gpgrt_spawn_actions_t act,
                                   void *in, void *out, void *err)
{
  act->hd[0] = in;
  act->hd[1] = out;
  act->hd[2] = err;
}

void
_gpgrt_spawn_actions_set_inherit_handles (gpgrt_spawn_actions_t act,
                                          void **handles)
{
  act->inherit_hds = handles;
}


gpg_err_code_t
_gpgrt_process_spawn (const char *pgmname, const char *argv[],
                      unsigned int flags, gpgrt_spawn_actions_t act,
                      gpgrt_process_t *r_process)
{
  gpg_err_code_t ec;
  gpgrt_process_t process;
  SECURITY_ATTRIBUTES sec_attr;
  PROCESS_INFORMATION pi = { NULL, 0, 0, 0 };
  STARTUPINFOEXW si;
  int cr_flags;
  char *cmdline;
  wchar_t *wcmdline = NULL;
  wchar_t *wpgmname = NULL;
  int ret;
  HANDLE hd_in[2];
  HANDLE hd_out[2];
  HANDLE hd_err[2];
  int i;
  BOOL ask_inherit = FALSE;
  BOOL allow_foreground_window = FALSE;
  struct gpgrt_spawn_actions act_default;

  if (!act)
    {
      memset (&act_default, 0, sizeof (act_default));
      for (i = 0; i <= 2; i++)
        act_default.hd[i] = INVALID_HANDLE_VALUE;
      act = &act_default;
    }

  /* Build the command line.  */
  ec = build_w32_commandline (pgmname, argv, &cmdline);
  if (ec)
    return ec;

  if ((flags & GPGRT_PROCESS_DETACHED))
    {
      if ((flags & GPGRT_PROCESS_STDFDS_SETTING))
        {
          xfree (cmdline);
          return GPG_ERR_INV_FLAG;
        }

      /* In detached case, it must be no R_PROCESS.  */
      if (r_process || pgmname == NULL)
        {
          xfree (cmdline);
          return GPG_ERR_INV_ARG;
        }

      return spawn_detached (pgmname, cmdline, act);
    }

  if (r_process)
    *r_process = NULL;

  if (pgmname == NULL)
    {
      xfree (cmdline);
      return GPG_ERR_INV_ARG;
    }

  process = xtrymalloc (sizeof (struct gpgrt_process));
  if (process == NULL)
    {
      xfree (cmdline);
      return _gpg_err_code_from_syserror ();
    }

  process->pgmname = pgmname;
  process->flags = flags;

  if ((flags & GPGRT_PROCESS_STDINOUT_SOCKETPAIR))
    {
      xfree (process);
      xfree (cmdline);
      return GPG_ERR_NOT_SUPPORTED;
    }

  if ((flags & GPGRT_PROCESS_STDIN_PIPE))
    {
      ec = create_inheritable_pipe (hd_in, INHERIT_READ);
      if (ec)
        {
          xfree (process);
          xfree (cmdline);
          return ec;
        }
    }
  else if ((flags & GPGRT_PROCESS_STDIN_KEEP))
    {
      hd_in[0] = GetStdHandle (STD_INPUT_HANDLE);
      hd_in[1] = INVALID_HANDLE_VALUE;
    }
  else
    {
      hd_in[0] = w32_open_null (0);
      hd_in[1] = INVALID_HANDLE_VALUE;
    }

  if ((flags & GPGRT_PROCESS_STDOUT_PIPE))
    {
      ec = create_inheritable_pipe (hd_out, INHERIT_WRITE);
      if (ec)
        {
          if (hd_in[0] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_in[0]);
          if (hd_in[1] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_in[1]);
          xfree (process);
          xfree (cmdline);
          return ec;
        }
    }
  else if ((flags & GPGRT_PROCESS_STDOUT_KEEP))
    {
      hd_out[0] = INVALID_HANDLE_VALUE;
      hd_out[1] = GetStdHandle (STD_OUTPUT_HANDLE);
    }
  else
    {
      hd_out[0] = INVALID_HANDLE_VALUE;
      hd_out[1] = w32_open_null (1);
    }

  if ((flags & GPGRT_PROCESS_STDERR_PIPE))
    {
      ec = create_inheritable_pipe (hd_err, INHERIT_WRITE);
      if (ec)
        {
          if (hd_in[0] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_in[0]);
          if (hd_in[1] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_in[1]);
          if (hd_out[0] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_out[0]);
          if (hd_out[1] != INVALID_HANDLE_VALUE)
            CloseHandle (hd_out[1]);
          xfree (process);
          xfree (cmdline);
          return ec;
        }
    }
  else if ((flags & GPGRT_PROCESS_STDERR_KEEP))
    {
      hd_err[0] = INVALID_HANDLE_VALUE;
      hd_err[1] = GetStdHandle (STD_ERROR_HANDLE);
    }
  else
    {
      hd_err[0] = INVALID_HANDLE_VALUE;
      hd_err[1] = w32_open_null (1);
    }

  memset (&si, 0, sizeof si);

  if (act->hd[0] == INVALID_HANDLE_VALUE)
    act->hd[0] = hd_in[0];
  if (act->hd[1] == INVALID_HANDLE_VALUE)
    act->hd[1] = hd_out[1];
  if (act->hd[2] == INVALID_HANDLE_VALUE)
    act->hd[2] = hd_err[1];

  i = 0;
  if (act->hd[0] != INVALID_HANDLE_VALUE)
    i++;
  if (act->hd[1] != INVALID_HANDLE_VALUE)
    i++;
  if (act->hd[2] != INVALID_HANDLE_VALUE)
    i++;

  if (i != 0 || act->inherit_hds)
    {
      SIZE_T attr_list_size = 0;
      HANDLE hd[32];
      HANDLE *hd_p = act->inherit_hds;
      int j = 0;

      if (act->hd[0] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[0];
      if (act->hd[1] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[1];
      if (act->hd[2] != INVALID_HANDLE_VALUE)
        hd[j++] = act->hd[2];
      if (hd_p)
        {
          while (*hd_p != INVALID_HANDLE_VALUE)
            if (j < DIM (hd))
              hd[j++] = *hd_p++;
            else
              {
                _gpgrt_log_info ("gpgrt_process_spawn: too many handles\n");
                break;
              }
        }

      if (j)
        {
          if (check_windows_version ())
            {
              InitializeProcThreadAttributeList (NULL, 1, 0, &attr_list_size);
              si.lpAttributeList = xtrymalloc (attr_list_size);
              if (si.lpAttributeList == NULL)
                {
                  if ((flags & GPGRT_PROCESS_STDIN_PIPE)
                      || !(flags & GPGRT_PROCESS_STDIN_KEEP))
                    CloseHandle (hd_in[0]);
                  if ((flags & GPGRT_PROCESS_STDIN_PIPE))
                    CloseHandle (hd_in[1]);
                  if ((flags & GPGRT_PROCESS_STDOUT_PIPE))
                    CloseHandle (hd_out[0]);
                  if ((flags & GPGRT_PROCESS_STDOUT_PIPE)
                      || !(flags & GPGRT_PROCESS_STDOUT_KEEP))
                    CloseHandle (hd_out[1]);
                  if ((flags & GPGRT_PROCESS_STDERR_PIPE))
                    CloseHandle (hd_err[0]);
                  if ((flags & GPGRT_PROCESS_STDERR_PIPE)
                      || !(flags & GPGRT_PROCESS_STDERR_KEEP))
                    CloseHandle (hd_err[1]);
                  xfree (process);
                  xfree (cmdline);
                  return _gpg_err_code_from_syserror ();
                }
              InitializeProcThreadAttributeList (si.lpAttributeList, 1, 0,
                                                 &attr_list_size);
              UpdateProcThreadAttribute (si.lpAttributeList, 0,
                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                         hd, sizeof (HANDLE) * j, NULL, NULL);
            }
          ask_inherit = TRUE;
        }
    }

  /* Prepare security attributes.  */
  memset (&sec_attr, 0, sizeof sec_attr );
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = FALSE;

  /* Start the process.  */
  si.StartupInfo.cb = sizeof (si);
  si.StartupInfo.dwFlags = ((i > 0 ? STARTF_USESTDHANDLES : 0)
                            | STARTF_USESHOWWINDOW);
  si.StartupInfo.wShowWindow = DEBUG_W32_SPAWN? SW_SHOW : SW_HIDE;
  si.StartupInfo.hStdInput  = act->hd[0];
  si.StartupInfo.hStdOutput = act->hd[1];
  si.StartupInfo.hStdError  = act->hd[2];

  /* log_debug ("CreateProcess, path='%s' cmdline='%s'\n", pgmname, cmdline); */
  cr_flags = (CREATE_DEFAULT_ERROR_MODE
              | ((flags & GPGRT_PROCESS_NO_CONSOLE) ? DETACHED_PROCESS : 0)
              | GetPriorityClass (GetCurrentProcess ())
              | CREATE_SUSPENDED);
  if (!(wpgmname = _gpgrt_utf8_to_wchar (pgmname)))
    ret = 0;
  else if (!(wcmdline = _gpgrt_utf8_to_wchar (cmdline)))
    ret = 0;
  else
    ret = CreateProcessW (wpgmname,      /* Program to start.  */
                          wcmdline,      /* Command line arguments.  */
                          &sec_attr,     /* Process security attributes.  */
                          &sec_attr,     /* Thread security attributes.  */
                          ask_inherit,   /* Inherit handles.  */
                          cr_flags,      /* Creation flags.  */
                          act->env,      /* Environment.  */
                          NULL,          /* Use current drive/directory.  */
                          (STARTUPINFOW *)&si, /* Startup information. */
                          &pi            /* Returns process information.  */
                          );
  if (!ret)
    {
      if (!wpgmname || !wcmdline)
        _gpgrt_log_info ("CreateProcess failed (utf8_to_wchar): %s\n",
                          strerror (errno));
      else
        _gpgrt_log_info ("CreateProcess failed: ec=%d\n",
                          (int)GetLastError ());
      if ((flags & GPGRT_PROCESS_STDIN_PIPE)
          || !(flags & GPGRT_PROCESS_STDIN_KEEP))
        CloseHandle (hd_in[0]);
      if ((flags & GPGRT_PROCESS_STDIN_PIPE))
        CloseHandle (hd_in[1]);
      if ((flags & GPGRT_PROCESS_STDOUT_PIPE))
        CloseHandle (hd_out[0]);
      if ((flags & GPGRT_PROCESS_STDOUT_PIPE)
          || !(flags & GPGRT_PROCESS_STDOUT_KEEP))
        CloseHandle (hd_out[1]);
      if ((flags & GPGRT_PROCESS_STDERR_PIPE))
        CloseHandle (hd_err[0]);
      if ((flags & GPGRT_PROCESS_STDERR_PIPE)
          || !(flags & GPGRT_PROCESS_STDERR_KEEP))
        CloseHandle (hd_err[1]);
      xfree (wpgmname);
      xfree (wcmdline);
      xfree (process);
      xfree (cmdline);
      return GPG_ERR_GENERAL;
    }

  if (si.lpAttributeList)
    DeleteProcThreadAttributeList (si.lpAttributeList);
  xfree (wpgmname);
  xfree (wcmdline);
  xfree (cmdline);

  if ((flags & GPGRT_PROCESS_STDIN_PIPE)
      || !(flags & GPGRT_PROCESS_STDIN_KEEP))
    CloseHandle (hd_in[0]);
  if ((flags & GPGRT_PROCESS_STDOUT_PIPE)
      || !(flags & GPGRT_PROCESS_STDOUT_KEEP))
    CloseHandle (hd_out[1]);
  if ((flags & GPGRT_PROCESS_STDERR_PIPE)
      || !(flags & GPGRT_PROCESS_STDERR_KEEP))
    CloseHandle (hd_err[1]);

  /* log_debug ("CreateProcess ready: hProcess=%p hThread=%p" */
  /*           " dwProcessID=%d dwThreadId=%d\n", */
  /*           pi.hProcess, pi.hThread, */
  /*           (int) pi.dwProcessId, (int) pi.dwThreadId); */

  if (allow_foreground_window)
    {
      /* Fixme: For unknown reasons AllowSetForegroundWindow returns
       * an invalid argument error if we pass it the correct
       * processID.  As a workaround we use -1 (ASFW_ANY).  */
      if (!AllowSetForegroundWindow (ASFW_ANY /*pi.dwProcessId*/))
        _gpgrt_log_info ("AllowSetForegroundWindow() failed: ec=%d\n",
                         (int)GetLastError ());
    }

  /* Process has been created suspended; resume it now. */
  _gpgrt_pre_syscall ();
  ResumeThread (pi.hThread);
  CloseHandle (pi.hThread);
  _gpgrt_post_syscall ();

  process->hProcess = pi.hProcess;
  process->hd_in = hd_in[1];
  process->hd_out = hd_out[0];
  process->hd_err = hd_err[0];
  process->exitcode = -1;
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

gpg_err_code_t
_gpgrt_process_get_fds (gpgrt_process_t process, unsigned int flags,
                        int *r_fd_in, int *r_fd_out, int *r_fd_err)
{
  (void)flags;
  if (r_fd_in)
    {
      *r_fd_in = _open_osfhandle ((intptr_t)process->hd_in, O_APPEND);
      process->hd_in = INVALID_HANDLE_VALUE;
    }
  if (r_fd_out)
    {
      *r_fd_out = _open_osfhandle ((intptr_t)process->hd_out, O_RDONLY);
      process->hd_out = INVALID_HANDLE_VALUE;
    }
  if (r_fd_err)
    {
      *r_fd_err = _open_osfhandle ((intptr_t)process->hd_err, O_RDONLY);
      process->hd_err = INVALID_HANDLE_VALUE;
    }

  return 0;
}

gpg_err_code_t
_gpgrt_process_get_streams (gpgrt_process_t process, unsigned int flags,
                            estream_t *r_fp_in, estream_t *r_fp_out,
                            estream_t *r_fp_err)
{
  int nonblock = (flags & GPGRT_PROCESS_STREAM_NONBLOCK)? 1: 0;
  es_syshd_t syshd;

  syshd.type = ES_SYSHD_HANDLE;
  if (r_fp_in)
    {
      syshd.u.handle = process->hd_in;
      *r_fp_in = _gpgrt_sysopen (&syshd, nonblock? "w,nonblock" : "w");
      process->hd_in = INVALID_HANDLE_VALUE;
    }
  if (r_fp_out)
    {
      syshd.u.handle = process->hd_out;
      *r_fp_out = _gpgrt_sysopen (&syshd, nonblock? "r,nonblock" : "r");
      process->hd_out = INVALID_HANDLE_VALUE;
    }
  if (r_fp_err)
    {
      syshd.u.handle = process->hd_err;
      *r_fp_err = _gpgrt_sysopen (&syshd, nonblock? "r,nonblock" : "r");
      process->hd_err = INVALID_HANDLE_VALUE;
    }
  return 0;
}

static gpg_err_code_t
process_kill (gpgrt_process_t process, unsigned int exitcode)
{
  gpg_err_code_t ec = 0;

  _gpgrt_pre_syscall ();
  if (TerminateProcess (process->hProcess, exitcode))
    ec = _gpg_err_code_from_syserror ();
  _gpgrt_post_syscall ();
  return ec;
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

        *r_id = (int)GetProcessId (process->hProcess);
        return 0;
      }

    case GPGRT_PROCESS_GET_EXIT_ID:
      {
        int *r_exit_status = va_arg (arg_ptr, int *);
        unsigned long exit_code;

        *r_exit_status = -1;

        if (!process->terminated)
          return GPG_ERR_UNFINISHED;

        if (process->hProcess == INVALID_HANDLE_VALUE)
          return 0;

        if (GetExitCodeProcess (process->hProcess, &exit_code) == 0)
          return _gpg_err_code_from_syserror ();

        *r_exit_status = (int)exit_code;
        return 0;
      }

    case GPGRT_PROCESS_GET_P_HANDLE:
      {
        HANDLE *r_hProcess = va_arg (arg_ptr, HANDLE *);

        if (r_hProcess == NULL)
          return GPG_ERR_INV_VALUE;

        *r_hProcess = process->hProcess;
        process->hProcess = INVALID_HANDLE_VALUE;
        return 0;
      }

    case GPGRT_PROCESS_GET_HANDLES:
      {
        HANDLE *r_hd_in = va_arg (arg_ptr, HANDLE *);
        HANDLE *r_hd_out = va_arg (arg_ptr, HANDLE *);
        HANDLE *r_hd_err = va_arg (arg_ptr, HANDLE *);

        if (r_hd_in)
          {
            *r_hd_in = process->hd_in;
            process->hd_in = INVALID_HANDLE_VALUE;
          }
        if (r_hd_out)
          {
            *r_hd_out = process->hd_out;
            process->hd_out = INVALID_HANDLE_VALUE;
          }
        if (r_hd_err)
          {
            *r_hd_err = process->hd_err;
            process->hd_err = INVALID_HANDLE_VALUE;
          }
        return 0;
      }

    case GPGRT_PROCESS_GET_EXIT_CODE:
      {
        unsigned long *r_exitcode = va_arg (arg_ptr, unsigned long *);

        if (!process->terminated)
          return GPG_ERR_UNFINISHED;

        if (process->hProcess == INVALID_HANDLE_VALUE)
          {
            *r_exitcode = (unsigned long)-1;
            return 0;
          }

        if (GetExitCodeProcess (process->hProcess, r_exitcode) == 0)
          return _gpg_err_code_from_syserror ();
        return 0;
      }

    case GPGRT_PROCESS_KILL_WITH_EC:
      {
        unsigned int exitcode = va_arg (arg_ptr, unsigned int);

        if (process->terminated)
          return 0;

        if (process->hProcess == INVALID_HANDLE_VALUE)
          return 0;

        return process_kill (process, exitcode);
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
  int code;

  if (process->hProcess == INVALID_HANDLE_VALUE)
    return 0;

  _gpgrt_pre_syscall ();
  code = WaitForSingleObject (process->hProcess, hang? INFINITE : 0);
  _gpgrt_post_syscall ();

  switch (code)
    {
    case WAIT_TIMEOUT:
      ec = GPG_ERR_TIMEOUT; /* Still running.  */
      break;

    case WAIT_FAILED:
      _gpgrt_log_info (_("waiting for process failed: ec=%d\n"),
                       (int)GetLastError ());
      ec = GPG_ERR_GENERAL;
      break;

    case WAIT_OBJECT_0:
      process->terminated = 1;
      ec = 0;
      break;

    default:
      _gpgrt_log_debug ("WaitForSingleObject returned unexpected code %d\n",
                        code);
      ec = GPG_ERR_GENERAL;
      break;
    }

  return ec;
}

gpg_err_code_t
_gpgrt_process_terminate (gpgrt_process_t process)
{
  return process_kill (process, 1);
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

  CloseHandle (process->hProcess);
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
