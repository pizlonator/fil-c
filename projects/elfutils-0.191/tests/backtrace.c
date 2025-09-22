/* Test program for unwinding of frames.
   Copyright (C) 2013, 2014, 2016 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <dwarf.h>
#ifdef __linux__
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <fcntl.h>
#include <string.h>
#include <argp.h>
#include ELFUTILS_HEADER(dwfl)
#endif
#include "system.h"

#ifndef __linux__

int
main (int argc __attribute__ ((unused)), char **argv)
{
  fprintf (stderr, "%s: Unwinding not supported for this architecture\n",
	   argv[0]);
  return 77;
}

#else /* __linux__ */

static int
dump_modules (Dwfl_Module *mod, void **userdata __attribute__ ((unused)),
	      const char *name, Dwarf_Addr start,
	      void *arg __attribute__ ((unused)))
{
  Dwarf_Addr end;
  dwfl_module_info (mod, NULL, NULL, &end, NULL, NULL, NULL, NULL);
  printf ("%#" PRIx64 "\t%#" PRIx64 "\t%s\n", (uint64_t) start, (uint64_t) end,
	  name);
  return DWARF_CB_OK;
}

static bool use_raise_jmp_patching;
static pid_t check_tid;

static void
callback_verify (pid_t tid, unsigned frameno, Dwarf_Addr pc,
		 const char *symname, Dwfl *dwfl)
{
  static bool seen_main = false;
  if (symname && *symname == '.')
    symname++;
  if (symname && strcmp (symname, "main") == 0)
    seen_main = true;
  if (pc == 0)
    {
      assert (seen_main);
      return;
    }
  if (check_tid == 0)
    check_tid = tid;
  if (tid != check_tid)
    {
      // For the main thread we are only interested if we can unwind till
      // we see the "main" symbol.
      return;
    }
  Dwfl_Module *mod;
  /* Skip frames for which there isn't a function name.  */
  static int nulls_seen = 0;
  if (symname == NULL)
    {
      nulls_seen++;
      return;
    }
  frameno -= nulls_seen;
  /* See case 4. Special case to help out simple frame pointer unwinders. */
  static bool duplicate_sigusr2 = false;
  if (duplicate_sigusr2)
    frameno--;
  static bool reduce_frameno = false;
  if (reduce_frameno)
    frameno--;
  static bool pthread_kill_seen = false;
  if (pthread_kill_seen)
    frameno--;
  if (! use_raise_jmp_patching && frameno >= 2)
    frameno += 2;
  const char *symname2 = NULL;
  switch (frameno)
  {
    case 0:
      if (! reduce_frameno && symname
	       && (strcmp (symname, "__kernel_vsyscall") == 0
		   || strcmp (symname, "__libc_do_syscall") == 0))
	reduce_frameno = true;
      else if (! pthread_kill_seen && symname
	       && strstr (symname, "pthread_kill") != NULL)
	pthread_kill_seen = true;
      else
	{
	  if (!symname || strcmp (symname, "raise") != 0)
	    {
	      fprintf (stderr,
		       "case 0: expected symname 'raise' got '%s'\n", symname);
	      abort ();
	    }
	}
      break;
    case 1:
      if (symname == NULL || strcmp (symname, "sigusr2") != 0)
	{
	  fprintf (stderr,
		   "case 1: expected symname 'sigusr2' got '%s'\n", symname);
	  abort ();
	}
      break;
    case 2: // x86_64 only
      /* __restore_rt - glibc maybe does not have to have this symbol.  */
      break;
    case 3: // use_raise_jmp_patching
      if (use_raise_jmp_patching)
	{
	  /* Verify we trapped on the very first instruction of jmp.  */
	  if (symname == NULL || strcmp (symname, "jmp") != 0)
	    {
	      fprintf (stderr,
		       "case 3: expected symname 'raise' got '%s'\n", symname);
	      abort ();
	    }
	  mod = dwfl_addrmodule (dwfl, pc - 1);
	  if (mod)
	    symname2 = dwfl_module_addrname (mod, pc - 1);
	  if (symname2 == NULL || strcmp (symname2, "jmp") != 0)
	    {
	      fprintf (stderr,
		       "case 3: expected symname2 'jmp' got '%s'\n", symname2);
	      abort ();
	    }
	  break;
	}
      FALLTHROUGH;
    case 4:
      /* Some simple frame unwinders get this wrong and think sigusr2
	 is calling itself again. Allow it and just pretend there is
	 an extra sigusr2 frame. */
      if (symname != NULL && strcmp (symname, "sigusr2") == 0)
	{
	  duplicate_sigusr2 = true;
	  break;
	}
      if (symname == NULL || strcmp (symname, "stdarg") != 0)
	{
	  fprintf (stderr,
		   "case 4: expected symname 'stdarg' got '%s'\n", symname);
	  abort ();
	}
      break;
    case 5:
      /* Verify we trapped on the very last instruction of child.  */
      if (symname == NULL || strcmp (symname, "backtracegen") != 0)
	{
	  fprintf (stderr,
		   "case 5: expected symname 'backtracegen' got '%s'\n",
		   symname);
	  abort ();
	}
      mod = dwfl_addrmodule (dwfl, pc);
      if (mod)
	symname2 = dwfl_module_addrname (mod, pc);

      // Note that the following assert might in theory even fail on x86_64,
      // there is no guarantee that the compiler doesn't reorder the
      // instructions or even inserts some padding instructions at the end
      // (which apparently happens on ppc64).
      if (use_raise_jmp_patching)
	{
          if (symname2 != NULL && strcmp (symname2, "backtracegen") == 0)
	    {
	      fprintf (stderr,
		       "use_raise_jmp_patching didn't expect symname2 "
		       "'backtracegen'\n");
	      abort ();
	    }
	}
      break;
  }
}

static int
frame_callback (Dwfl_Frame *state, void *frame_arg)
{
  int *framenop = frame_arg;
  Dwarf_Addr pc;
  bool isactivation;

  if (*framenop > 16)
    {
      error (0, 0, "Too many frames: %d\n", *framenop);
      return DWARF_CB_ABORT;
    }

  if (! dwfl_frame_pc (state, &pc, &isactivation))
    {
      error (0, 0, "%s", dwfl_errmsg (-1));
      return DWARF_CB_ABORT;
    }
  Dwarf_Addr pc_adjusted = pc - (isactivation ? 0 : 1);

  /* Get PC->SYMNAME.  */
  Dwfl_Thread *thread = dwfl_frame_thread (state);
  Dwfl *dwfl = dwfl_thread_dwfl (thread);
  Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
  const char *symname = NULL;
  if (mod)
    symname = dwfl_module_addrname (mod, pc_adjusted);

  printf ("#%2d %#" PRIx64 "%4s\t%s\n", *framenop, (uint64_t) pc,
	  ! isactivation ? "- 1" : "", symname ?: "<null>");
  pid_t tid = dwfl_thread_tid (thread);
  callback_verify (tid, *framenop, pc, symname, dwfl);
  (*framenop)++;

  return DWARF_CB_OK;
}

static int
thread_callback (Dwfl_Thread *thread, void *thread_arg __attribute__((unused)))
{
  printf ("TID %ld:\n", (long) dwfl_thread_tid (thread));
  int frameno = 0;
  switch (dwfl_thread_getframes (thread, frame_callback, &frameno))
    {
    case 0:
      break;
    case DWARF_CB_ABORT:
      return DWARF_CB_ABORT;
    case -1:
      error (0, 0, "dwfl_thread_getframes: %s", dwfl_errmsg (-1));
      /* All platforms do not have yet proper unwind termination.  */
      break;
    default:
      abort ();
    }
  return DWARF_CB_OK;
}

static void
dump (Dwfl *dwfl)
{
  ptrdiff_t ptrdiff = dwfl_getmodules (dwfl, dump_modules, NULL, 0);
  assert (ptrdiff == 0);
  bool err = false;
  switch (dwfl_getthreads (dwfl, thread_callback, NULL))
    {
    case 0:
      break;
    case DWARF_CB_ABORT:
      err = true;
      break;
    case -1:
      error (0, 0, "dwfl_getthreads: %s", dwfl_errmsg (-1));
      err = true;
      break;
    default:
      abort ();
    }
  callback_verify (0, 0, 0, NULL, dwfl);
  if (err)
    exit (EXIT_FAILURE);
}

struct see_exec_module
{
  Dwfl_Module *mod;
  char selfpath[PATH_MAX + 1];
};

static int
see_exec_module (Dwfl_Module *mod, void **userdata __attribute__ ((unused)),
		 const char *name __attribute__ ((unused)),
		 Dwarf_Addr start __attribute__ ((unused)), void *arg)
{
  struct see_exec_module *data = arg;
  if (strcmp (name, data->selfpath) != 0)
    return DWARF_CB_OK;
  assert (data->mod == NULL);
  data->mod = mod;
  return DWARF_CB_ABORT;
}

/* We used to do this on x86_64 only (see backtrace-child why we now don't):
     PC will get changed to function 'jmp' by backtrace.c function
     prepare_thread.  Then SIGUSR2 will be signalled to backtrace-child
     which will invoke function sigusr2.
     This is all done so that signal interrupts execution of the very first
     instruction of a function.  Properly handled unwind should not slip into
     the previous unrelated function.  */

#ifdef __x86_64__
/* #define RAISE_JMP_PATCHING 1 */
#endif

static void
prepare_thread (pid_t pid2 __attribute__ ((unused)),
		void (*jmp) (void) __attribute__ ((unused)))
{
#ifndef RAISE_JMP_PATCHING
  abort ();
#else /* RAISE_JMP_PATCHING */
  long l;
  struct user_regs_struct user_regs;
  errno = 0;
  l = ptrace (PTRACE_GETREGS, pid2, 0, (intptr_t) &user_regs);
  assert (l == 0);
  user_regs.rip = (intptr_t) jmp;
  l = ptrace (PTRACE_SETREGS, pid2, 0, (intptr_t) &user_regs);
  assert (l == 0);
  l = ptrace (PTRACE_CONT, pid2, NULL, (void *) (intptr_t) SIGUSR2);
  int status;
  pid_t got = waitpid (pid2, &status, __WALL);
  assert (got == pid2);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);
#endif /* RAISE_JMP_PATCHING */
}

#include <asm/unistd.h>
#include <unistd.h>

static void
report_pid (Dwfl *dwfl, pid_t pid)
{
  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));

  result = dwfl_linux_proc_attach (dwfl, pid, true);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_attach: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_attach");
}

static Dwfl *
pid_to_dwfl (pid_t pid)
{
  static char *debuginfo_path;
  static const Dwfl_Callbacks proc_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,

      .find_elf = dwfl_linux_proc_find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&proc_callbacks);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));
  report_pid (dwfl, pid);
  return dwfl;
}

static void
exec_dump (const char *exec)
{
  pid_t pid = fork ();
  switch (pid)
  {
    case -1:
      abort ();
    case 0:
      execl (exec, exec, "--ptraceme", NULL);
      abort ();
    default:
      break;
  }

  /* Catch the main thread.  Catch it first otherwise the /proc evaluation of
     PID may have caught still ourselves before executing execl above.  */
  errno = 0;
  int status;
  pid_t got = waitpid (pid, &status, 0);
  assert (got == pid);
  assert (WIFSTOPPED (status));
  // Main thread will signal SIGUSR2.  Other thread will signal SIGUSR1.
  assert (WSTOPSIG (status) == SIGUSR2);

  /* Catch the spawned thread.  Do not use __WCLONE as we could get racy
     __WCLONE, probably despite pthread_create already had to be called the new
     task is not yet alive enough for waitpid.  */
  pid_t pid2 = waitpid (-1, &status, __WALL);
  assert (pid2 > 0);
  assert (pid2 != pid);
  assert (WIFSTOPPED (status));
  // Main thread will signal SIGUSR2.  Other thread will signal SIGUSR1.
  assert (WSTOPSIG (status) == SIGUSR1);

  Dwfl *dwfl = pid_to_dwfl (pid);
  char *selfpathname;
  int i = asprintf (&selfpathname, "/proc/%ld/exe", (long) pid);
  assert (i > 0);
  struct see_exec_module data;
  ssize_t ssize = readlink (selfpathname, data.selfpath,
			    sizeof (data.selfpath));
  free (selfpathname);
  assert (ssize > 0 && ssize < (ssize_t) sizeof (data.selfpath));
  data.selfpath[ssize] = '\0';
  data.mod = NULL;
  dwfl_getmodules (dwfl, see_exec_module, &data, 0);
  assert (data.mod != NULL);
  GElf_Addr loadbase;
  Elf *elf = dwfl_module_getelf (data.mod, &loadbase);
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
  assert (ehdr != NULL);
  /* It is false also on x86_64 with i386 inferior.  */
#ifndef RAISE_JMP_PATCHING
  use_raise_jmp_patching = false;
#else /* RAISE_JMP_PATCHING_ */
  use_raise_jmp_patching = ehdr->e_machine == EM_X86_64;
#endif /* __x86_64__ */
  void (*jmp) (void) = 0;
  if (use_raise_jmp_patching)
    {
      // Find inferior symbol named "jmp".
      int nsym = dwfl_module_getsymtab (data.mod);
      int symi;
      for (symi = 1; symi < nsym; ++symi)
	{
	  GElf_Sym symbol;
	  const char *symbol_name = dwfl_module_getsym (data.mod, symi, &symbol, NULL);
	  if (symbol_name == NULL)
	    continue;
	  switch (GELF_ST_TYPE (symbol.st_info))
	    {
	    case STT_SECTION:
	    case STT_FILE:
	    case STT_TLS:
	      continue;
	    default:
	      if (strcmp (symbol_name, "jmp") != 0)
		continue;
	      break;
	    }
	  /* LOADBASE is already applied here.  */
	  jmp = (void (*) (void)) (uintptr_t) symbol.st_value;
	  break;
	}
      assert (symi < nsym);
      prepare_thread (pid2, jmp);
    }
  dwfl_end (dwfl);
  check_tid = pid2;
  dwfl = pid_to_dwfl (pid);
  dump (dwfl);
  dwfl_end (dwfl);
}

#define OPT_BACKTRACE_EXEC 0x100

static const struct argp_option options[] =
  {
    { "backtrace-exec", OPT_BACKTRACE_EXEC, "EXEC", 0, N_("Run executable"), 0 },
    { NULL, 0, NULL, 0, NULL, 0 }
  };


static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = state->input;
      break;

    case OPT_BACKTRACE_EXEC:
      exec_dump (arg);
      exit (0);

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

int
main (int argc __attribute__ ((unused)), char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  elf_version (EV_CURRENT);

  Dwfl *dwfl = NULL;
  const struct argp_child argp_children[] =
    {
      { .argp = dwfl_standard_argp () },
      { .argp = NULL }
    };
  const struct argp argp =
    {
      options, parse_opt, NULL, NULL, argp_children, NULL, NULL
    };
  (void) argp_parse (&argp, argc, argv, 0, NULL, &dwfl);
  assert (dwfl != NULL);
  /* We want to make sure the dwfl was properly attached.  */
  if (dwfl_pid (dwfl) < 0)
    error (2, 0, "dwfl_pid: %s", dwfl_errmsg (-1));
  dump (dwfl);
  dwfl_end (dwfl);
  return 0;
}

#endif /* ! __linux__ */

