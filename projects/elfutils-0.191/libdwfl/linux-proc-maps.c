/* Standard libdwfl callbacks for debugging a live Linux process.
   Copyright (C) 2005-2010, 2013, 2014, 2016 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwflP.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <endian.h>
#include "system.h"


#define PROCMAPSFMT	"/proc/%d/maps"
#define PROCMEMFMT	"/proc/%d/mem"
#define PROCAUXVFMT	"/proc/%d/auxv"
#define PROCEXEFMT	"/proc/%d/exe"


/* Return ELFCLASS64 or ELFCLASS32 for the main ELF executable.  Return
   ELFCLASSNONE for an error.  */

static unsigned char
get_pid_class (pid_t pid)
{
  char *fname;
  if (asprintf (&fname, PROCEXEFMT, pid) < 0)
    return ELFCLASSNONE;

  int fd = open (fname, O_RDONLY);
  free (fname);
  if (fd < 0)
    return ELFCLASSNONE;

  unsigned char buf[EI_CLASS + 1];
  ssize_t nread = pread_retry (fd, &buf, sizeof buf, 0);
  close (fd);
  if (nread != sizeof buf || buf[EI_MAG0] != ELFMAG0
      || buf[EI_MAG1] != ELFMAG1 || buf[EI_MAG2] != ELFMAG2
      || buf[EI_MAG3] != ELFMAG3
      || (buf[EI_CLASS] != ELFCLASS64 && buf[EI_CLASS] != ELFCLASS32))
    return ELFCLASSNONE;

  return buf[EI_CLASS];
}

/* Search /proc/PID/auxv for the AT_SYSINFO_EHDR tag.

   It would be easiest to call get_pid_class and parse everything according to
   the 32-bit or 64-bit class.  But this would bring the overhead of syscalls
   to open and read the "/proc/%d/exe" file.

   Therefore this function tries to parse the "/proc/%d/auxv" content both
   ways, as if it were the 32-bit format and also if it were the 64-bit format.
   Only if it gives some valid data in both cases get_pid_class gets called.
   In most cases only one of the format bit sizes gives valid data and the
   get_pid_class call overhead can be saved.  */

static int
grovel_auxv (pid_t pid, Dwfl *dwfl, GElf_Addr *sysinfo_ehdr)
{
  char *fname;
  if (asprintf (&fname, PROCAUXVFMT, pid) < 0)
    return ENOMEM;

  int fd = open (fname, O_RDONLY);
  free (fname);
  if (fd < 0)
    return errno == ENOENT ? 0 : errno;

  GElf_Addr sysinfo_ehdr64 = 0;
  GElf_Addr sysinfo_ehdr32 = 0;
  GElf_Addr segment_align64 = dwfl->segment_align;
  GElf_Addr segment_align32 = dwfl->segment_align;
  off_t offset = 0;
  ssize_t nread;
  union
  {
    Elf64_auxv_t a64[64];
    Elf32_auxv_t a32[128];
  } d;
  do
    {
      eu_static_assert (sizeof d.a64 == sizeof d.a32);
      nread = pread_retry (fd, d.a64, sizeof d.a64, offset);
      if (nread < 0)
	{
	  int ret = errno;
	  close (fd);
	  return ret;
	}
      for (size_t a32i = 0; a32i < nread / sizeof d.a32[0]; a32i++)
	{
	  const Elf32_auxv_t *a32 = d.a32 + a32i;
	  switch (a32->a_type)
	  {
	    case AT_SYSINFO_EHDR:
	      sysinfo_ehdr32 = a32->a_un.a_val;
	      break;
	    case AT_PAGESZ:
	      segment_align32 = a32->a_un.a_val;
	      break;
	  }
	}
      for (size_t a64i = 0; a64i < nread / sizeof d.a64[0]; a64i++)
	{
	  const Elf64_auxv_t *a64 = d.a64 + a64i;
	  switch (a64->a_type)
	  {
	    case AT_SYSINFO_EHDR:
	      sysinfo_ehdr64 = a64->a_un.a_val;
	      break;
	    case AT_PAGESZ:
	      segment_align64 = a64->a_un.a_val;
	      break;
	  }
	}
      offset += nread;
    }
  while (nread == sizeof d.a64);

  close (fd);

  bool valid64 = sysinfo_ehdr64 != 0 || segment_align64 != dwfl->segment_align;
  bool valid32 = sysinfo_ehdr32 != 0 || segment_align32 != dwfl->segment_align;

  unsigned char pid_class = ELFCLASSNONE;
  if (valid64 && valid32)
    pid_class = get_pid_class (pid);

  if (pid_class == ELFCLASS64 || (valid64 && ! valid32))
    {
      *sysinfo_ehdr = sysinfo_ehdr64;
      dwfl->segment_align = segment_align64;
      return 0;
    }
  if (pid_class == ELFCLASS32 || (! valid64 && valid32))
    {
      *sysinfo_ehdr = sysinfo_ehdr32;
      dwfl->segment_align = segment_align32;
      return 0;
    }
  return ENOEXEC;
}

static inline bool
do_report (Dwfl *dwfl, char **plast_file, Dwarf_Addr low, Dwarf_Addr high)
{
  if (*plast_file != NULL)
    {
      Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, *plast_file,
						     low, high);
      free (*plast_file);
      *plast_file = NULL;
      if (unlikely (mod == NULL))
        return true;
    }
  return false;
}

#define report() do_report(dwfl, &last_file, low, high)

static int
proc_maps_report (Dwfl *dwfl, FILE *f, GElf_Addr sysinfo_ehdr, pid_t pid)
{
  unsigned int last_dmajor = -1, last_dminor = -1;
  uint64_t last_ino = -1;
  char *last_file = NULL;
  Dwarf_Addr low = 0, high = 0;

  char *line = NULL;
  size_t linesz;
  ssize_t len;
  while ((len = getline (&line, &linesz, f)) > 0)
    {
      if (line[len - 1] == '\n')
	line[len - 1] = '\0';

      Dwarf_Addr start, end, offset;
      unsigned int dmajor, dminor;
      uint64_t ino;
      int nread = -1;
      if (sscanf (line, "%" PRIx64 "-%" PRIx64 " %*s %" PRIx64
		  " %x:%x %" PRIu64 " %n",
		  &start, &end, &offset, &dmajor, &dminor, &ino, &nread) < 6
	  || nread <= 0)
	{
	  free (line);
	  free (last_file);
	  return ENOEXEC;
	}

      /* If this is the special mapping AT_SYSINFO_EHDR pointed us at,
	 report the last one and then this special one.  */
      if (start == sysinfo_ehdr && start != 0)
	{
	  if (report ())
	    {
	    bad_report:
	      free (line);
	      return -1;
	    }

	  low = start;
	  high = end;
	  if (asprintf (&last_file, "[vdso: %d]", (int) pid) < 0
	      || report ())
	    goto bad_report;
	}

      char *file = line + nread + strspn (line + nread, " \t");
      if (file[0] != '/' || (ino == 0 && dmajor == 0 && dminor == 0))
	/* This line doesn't indicate a file mapping.  */
	continue;

      if (last_file != NULL
	  && ino == last_ino && dmajor == last_dmajor && dminor == last_dminor)
	{
	  /* This is another portion of the same file's mapping.  */
	  if (strcmp (last_file, file) != 0)
	    {
	      free (last_file);
	      goto bad_report;
	    }
	  high = end;
	}
      else
	{
	  /* This is a different file mapping.  Report the last one.  */
	  if (report ())
	    goto bad_report;
	  low = start;
	  high = end;
	  last_file = strdup (file);
	  last_ino = ino;
	  last_dmajor = dmajor;
	  last_dminor = dminor;
	}
    }
  free (line);

  int result = ferror_unlocked (f) ? errno : feof_unlocked (f) ? 0 : ENOEXEC;

  /* Report the final one.  */
  bool lose = report ();

  return result != 0 ? result : lose ? -1 : 0;
}

int
dwfl_linux_proc_maps_report (Dwfl *dwfl, FILE *f)
{
  return proc_maps_report (dwfl, f, 0, 0);
}
INTDEF (dwfl_linux_proc_maps_report)

int
dwfl_linux_proc_report (Dwfl *dwfl, pid_t pid)
{
  if (dwfl == NULL)
    return -1;

  /* We'll notice the AT_SYSINFO_EHDR address specially when we hit it.  */
  GElf_Addr sysinfo_ehdr = 0;
  int result = grovel_auxv (pid, dwfl, &sysinfo_ehdr);
  if (result != 0)
    return result;

  char *fname;
  if (asprintf (&fname, PROCMAPSFMT, pid) < 0)
    return ENOMEM;

  FILE *f = fopen (fname, "r");
  free (fname);
  if (f == NULL)
    return errno;

  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  result = proc_maps_report (dwfl, f, sysinfo_ehdr, pid);

  fclose (f);

  return result;
}
INTDEF (dwfl_linux_proc_report)

static ssize_t
read_proc_memory (void *arg, void *data, GElf_Addr address,
		  size_t minread, size_t maxread)
{
  const int fd = *(const int *) arg;

  /* This code relies on the fact the Linux kernel accepts negative
     offsets when seeking /dev/$$/mem files, as a special case. In
     particular pread cannot be used here, because it will always
     return EINVAL when passed a negative offset.  */

  if (lseek (fd, (off_t) address, SEEK_SET) == -1)
    return -1;

  ssize_t nread = read (fd, data, maxread);

  if (nread > 0 && (size_t) nread < minread)
    nread = 0;
  return nread;
}

extern Elf *elf_from_remote_memory (GElf_Addr ehdr_vma,
				    GElf_Xword pagesize,
				    GElf_Addr *loadbasep,
				    ssize_t (*read_memory) (void *arg,
							    void *data,
							    GElf_Addr address,
							    size_t minread,
							    size_t maxread),
				    void *arg);


/* Dwfl_Callbacks.find_elf */

int
dwfl_linux_proc_find_elf (Dwfl_Module *mod __attribute__ ((unused)),
			  void **userdata __attribute__ ((unused)),
			  const char *module_name, Dwarf_Addr base,
			  char **file_name, Elf **elfp)
{
  int pid = -1;
  if (module_name[0] == '/')
    {
      /* When this callback is used together with dwfl_linux_proc_report
	 then we might see mappings of special character devices.  Make
	 sure we only open and return regular files.  Special devices
	 might hang on open or read.  (deleted) files are super special.
	 The image might come from memory if we are attached.  */
      struct stat sb;
      if (stat (module_name, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
	{
	  if (strcmp (strrchr (module_name, ' ') ?: "", " (deleted)") == 0)
	    pid = INTUSE(dwfl_pid) (mod->dwfl);
	  else
	    return -1;
	}

      if (pid == -1)
	{
	  int fd = open (module_name, O_RDONLY);
	  if (fd >= 0)
	    {
	      *file_name = strdup (module_name);
	      if (*file_name == NULL)
		{
		  close (fd);
		  return ENOMEM;
		}
	    }
	  return fd;
	}
    }

  if (pid != -1 || sscanf (module_name, "[vdso: %d]", &pid) == 1)
    {
      /* Special case for in-memory ELF image.  */

      bool detach = false;
      bool tid_was_stopped = false;
      struct __libdwfl_pid_arg *pid_arg = __libdwfl_get_pid_arg (mod->dwfl);
      if (pid_arg != NULL && ! pid_arg->assume_ptrace_stopped)
	{
	  /* If any thread is already attached we are fine.  Read
	     through that thread.  It doesn't have to be the main
	     thread pid.  */
	  pid_t tid = pid_arg->tid_attached;
	  if (tid != 0)
	    pid = tid;
	  else
	    detach = __libdwfl_ptrace_attach (pid, &tid_was_stopped);
	}

      char *fname;
      if (asprintf (&fname, PROCMEMFMT, pid) < 0)
	goto detach;

      int fd = open (fname, O_RDONLY);
      free (fname);
      if (fd < 0)
	goto detach;

      *elfp = elf_from_remote_memory (base, sysconf (_SC_PAGESIZE), NULL,
				      &read_proc_memory, &fd);

      close (fd);

      *file_name = NULL;

    detach:
      if (detach)
	__libdwfl_ptrace_detach (pid, tid_was_stopped);
      return -1;
    }

  return -1;
}
INTDEF (dwfl_linux_proc_find_elf)
