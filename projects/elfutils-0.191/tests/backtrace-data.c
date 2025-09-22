/* Test custom provided Dwfl_Thread_Callbacks vector.
   Copyright (C) 2013 Red Hat, Inc.
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

/* Test custom provided Dwfl_Thread_Callbacks vector.  Test mimics what
   a ptrace based vector would do.  */

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
#if defined(__x86_64__) && defined(__linux__)
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <fcntl.h>
#include <string.h>
#include ELFUTILS_HEADER(dwfl)
#endif
#include "system.h"

#if !defined(__x86_64__) || !defined(__linux__)

int
main (int argc __attribute__ ((unused)), char **argv)
{
  fprintf (stderr, "%s: x86_64 linux only test\n",
          argv[0]);
  return 77;
}

#else /* __x86_64__ && __linux__ */

/* The only arch specific code is set_initial_registers.  */

static int
find_elf (Dwfl_Module *mod __attribute__ ((unused)),
	  void **userdata __attribute__ ((unused)),
	  const char *modname __attribute__ ((unused)),
	  Dwarf_Addr base __attribute__ ((unused)),
	  char **file_name __attribute__ ((unused)),
	  Elf **elfp __attribute__ ((unused)))
{
  /* Not used as modules are reported explicitly.  */
  assert (0);
}

static bool
memory_read (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Word *result,
	     void *dwfl_arg __attribute__ ((unused)))
{
  pid_t child = dwfl_pid (dwfl);

  errno = 0;
  long l = ptrace (PTRACE_PEEKDATA, child, (void *) (uintptr_t) addr, NULL);

  // The unwinder can ask for an invalid address.
  // Don't assert on that but just politely refuse.
  if (errno != 0) {
      errno = 0;
      return false;
  }
  *result = l;

  return true;
}

/* Return filename and VMA address *BASEP where its mapping starts which
   contains ADDR.  */

static char *
maps_lookup (pid_t pid, Dwarf_Addr addr, GElf_Addr *basep)
{
  char *fname;
  int i = asprintf (&fname, "/proc/%ld/maps", (long) pid);
  assert (i > 0);
  FILE *f = fopen (fname, "r");
  assert (f);
  free (fname);
  for (;;)
    {
      // 37e3c22000-37e3c23000 rw-p 00022000 00:11 49532 /lib64/ld-2.14.90.so */
      unsigned long start, end, offset;
      i = fscanf (f, "%lx-%lx %*s %lx %*x:%*x %*u", &start, &end, &offset);
      if (i != 3)
          break;
      char *filename = strdup ("");
      assert (filename);
      size_t filename_len = 0;
      for (;;)
	{
	  int c = fgetc (f);
	  assert (c != EOF);
	  if (c == '\n')
	    break;
	  if (c == ' ' && *filename == '\0')
	    continue;
	  filename = realloc (filename, filename_len + 2);
	  assert (filename);
	  filename[filename_len++] = c;
	  filename[filename_len] = '\0';
	}
      if (start <= addr && addr < end)
	{
	  i = fclose (f);
	  assert (i == 0);

	  *basep = start - offset;
	  return filename;
	}
      free (filename);
    }
  *basep = 0;
  return NULL;
}

/* Add module containing ADDR to the DWFL address space.

   dwfl_report_elf call here violates Dwfl manipulation as one should call
   dwfl_report only between dwfl_report_begin_add and dwfl_report_end.
   Current elfutils implementation does not mind as dwfl_report_begin_add is
   empty.  */

static Dwfl_Module *
report_module (Dwfl *dwfl, pid_t child, Dwarf_Addr addr)
{
  GElf_Addr base;
  char *long_name = maps_lookup (child, addr, &base);
  if (!long_name)
      return NULL; // not found
  Dwfl_Module *mod = dwfl_report_elf (dwfl, long_name, long_name, -1,
				      base, false /* add_p_vaddr */);
  assert (mod);
  free (long_name);
  assert (dwfl_addrmodule (dwfl, addr) == mod);
  return mod;
}

static pid_t
next_thread (Dwfl *dwfl, void *dwfl_arg __attribute__ ((unused)),
	     void **thread_argp)
{
  if (*thread_argp != NULL)
    return 0;
  /* Put arbitrary non-NULL value into *THREAD_ARGP as a marker so that this
     function returns non-zero PID only once.  */
  *thread_argp = thread_argp;
  return dwfl_pid (dwfl);
}

static bool
set_initial_registers (Dwfl_Thread *thread,
		       void *thread_arg __attribute__ ((unused)))
{
  pid_t child = dwfl_pid (dwfl_thread_dwfl (thread));

  struct user_regs_struct user_regs;
  long l = ptrace (PTRACE_GETREGS, child, NULL, &user_regs);
  assert (l == 0);

  Dwarf_Word dwarf_regs[17];
  dwarf_regs[0] = user_regs.rax;
  dwarf_regs[1] = user_regs.rdx;
  dwarf_regs[2] = user_regs.rcx;
  dwarf_regs[3] = user_regs.rbx;
  dwarf_regs[4] = user_regs.rsi;
  dwarf_regs[5] = user_regs.rdi;
  dwarf_regs[6] = user_regs.rbp;
  dwarf_regs[7] = user_regs.rsp;
  dwarf_regs[8] = user_regs.r8;
  dwarf_regs[9] = user_regs.r9;
  dwarf_regs[10] = user_regs.r10;
  dwarf_regs[11] = user_regs.r11;
  dwarf_regs[12] = user_regs.r12;
  dwarf_regs[13] = user_regs.r13;
  dwarf_regs[14] = user_regs.r14;
  dwarf_regs[15] = user_regs.r15;
  dwarf_regs[16] = user_regs.rip;
  bool ok = dwfl_thread_state_registers (thread, 0, 17, dwarf_regs);
  assert (ok);

  /* x86_64 has PC contained in its CFI subset of DWARF register set so
     elfutils will figure out the real PC value from REGS.
     So no need to explicitly call dwfl_thread_state_register_pc.  */

  return true;
}

static const Dwfl_Thread_Callbacks callbacks =
{
  next_thread,
  NULL, /* get_thread */
  memory_read,
  set_initial_registers,
  NULL, /* detach */
  NULL, /* thread_detach */
};

static int
frame_callback (Dwfl_Frame *state, void *arg)
{
  unsigned *framenop = arg;
  Dwarf_Addr pc;
  bool isactivation;
  if (! dwfl_frame_pc (state, &pc, &isactivation))
    {
      error (1, 0, "%s", dwfl_errmsg (-1));
      return 1;
    }
  Dwarf_Addr pc_adjusted = pc - (isactivation ? 0 : 1);

  /* Get PC->SYMNAME.  */
  Dwfl *dwfl = dwfl_thread_dwfl (dwfl_frame_thread (state));
  Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
  if (mod == NULL)
    mod = report_module (dwfl, dwfl_pid (dwfl), pc_adjusted);
  const char *symname = NULL;
  symname = dwfl_module_addrname (mod, pc_adjusted);

  printf ("#%2u %#" PRIx64 "%4s\t%s\n", (*framenop)++, (uint64_t) pc,
	  ! isactivation ? "- 1" : "", symname);
  return DWARF_CB_OK;
}

static int
thread_callback (Dwfl_Thread *thread, void *thread_arg __attribute__ ((unused)))
{
  unsigned frameno = 0;
  switch (dwfl_thread_getframes (thread, frame_callback, &frameno))
    {
    case 0:
      break;
    case -1:
      error (1, 0, "dwfl_thread_getframes: %s", dwfl_errmsg (-1));
      break;
    default:
      abort ();
    }
  return DWARF_CB_OK;
}

int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  elf_version (EV_CURRENT);

  pid_t child = fork ();
  switch (child)
  {
    case -1:
      assert (0);
      break;
    case 0:;
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert (l == 0);
      raise (SIGUSR1);
      return 0;
    default:
      break;
  }

  int status;
  pid_t pid = waitpid (child, &status, 0);
  assert (pid == child);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);

  static char *debuginfo_path;
  static const Dwfl_Callbacks offline_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,
      .section_address = dwfl_offline_section_address,
      .find_elf = find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&offline_callbacks);
  assert (dwfl);

  struct user_regs_struct user_regs;
  long l = ptrace (PTRACE_GETREGS, child, NULL, &user_regs);
  assert (l == 0);
  report_module (dwfl, child, user_regs.rip);

  bool ok = dwfl_attach_state (dwfl, EM_NONE, child, &callbacks, NULL);
  assert (ok);

  /* Multiple threads are not handled here.  */
  int err = dwfl_getthreads (dwfl, thread_callback, NULL);
  assert (! err);

  dwfl_end (dwfl);
  kill (child, SIGKILL);
  pid = waitpid (child, &status, 0);
  assert (pid == child);
  assert (WIFSIGNALED (status));
  assert (WTERMSIG (status) == SIGKILL);

  return EXIT_SUCCESS;
}

#endif /* x86_64 */
