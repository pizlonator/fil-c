/* Classification of ELF files.
   Copyright (C) 2019 Red Hat, Inc.
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
#include <system.h>

#include <argp.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include ELFUTILS_HEADER(elf)
#include ELFUTILS_HEADER(dwelf)
#include "printversion.h"

/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Set by parse_opt.  */
static int verbose;

/* Set by the main function.  */
static const char *current_path;

/* Set by open_file.  */
static int file_fd = -1;

/* Set by issue or elf_issue.  */
static bool issue_found;

/* Non-fatal issue occurred while processing the current_path.  */
static void
issue (int e, const char *msg)
{
  if (verbose >= 0)
    {
      if (current_path == NULL)
	error (0, e, "%s", msg);
      else
	error (0, e, "%s '%s'", msg, current_path);
    }
  issue_found = true;
}

/* Non-fatal issue occurred while processing the current ELF.  */
static void
elf_issue (const char *msg)
{
  if (verbose >= 0)
    error (0, 0, "%s: %s: '%s'", msg, elf_errmsg (-1), current_path);
  issue_found = true;
}

/* Set by parse_opt.  */
static bool flag_only_regular_files;

static bool
open_file (void)
{
  if (verbose > 1)
    fprintf (stderr, "debug: processing file: %s\n", current_path);

  file_fd = open (current_path, O_RDONLY | (flag_only_regular_files
					    ? O_NOFOLLOW : 0));
  if (file_fd < 0)
    {
      if (!flag_only_regular_files || errno != ELOOP)
	issue (errno, N_("opening"));
      return false;
    }

  struct stat st;
  if (fstat (file_fd, &st) != 0)
    {
      issue (errno, N_("reading"));
      return false;
    }

  /* Don't even bother with directories.  */
  if (S_ISDIR (st.st_mode)
      || (flag_only_regular_files && !S_ISREG (st.st_mode)))
    return false;

  return true;
}

static void
close_file (void)
{
  if (file_fd >= 0)
    {
      close (file_fd);
      file_fd = -1;
    }
}

/* Set by open_elf.  */
static Elf *elf;

/* Set by parse_opt.  */
static bool flag_compressed;

static bool
open_elf (void)
{
  if (!open_file ())
    {
      /* Make sure the file descriptor is gone.  */
      close_file ();
      return false;
    }

  if (flag_compressed)
    elf = dwelf_elf_begin (file_fd);
  else
    elf = elf_begin (file_fd, ELF_C_READ, NULL);

  if (elf == NULL)
    {
      elf_issue ("opening ELF file");
      close_file ();
      return false;
    }

  return true;
}

static void
close_elf (void)
{
  if (elf != NULL)
    {
      elf_end (elf);
      elf = NULL;
    }

  close_file ();
}

static const char *
elf_kind_string (int kind)
{
  switch (kind)
    {
    case ELF_K_NONE:
      return "ELF_K_NONE";
    case ELF_K_AR:
      return "ELF_K_AR";
    case ELF_K_COFF:
      return "ELF_K_COFF"; /* libelf doesn't really support this.  */
    case ELF_K_ELF:
      return "ELF_K_ELF";
    default:
      return "<unknown>";
    }
}

static const char *
elf_type_string (int type)
{
  switch (type)
    {
    case ET_NONE:
      return "ET_NONE";
    case ET_REL:
      return "ET_REL";
    case ET_EXEC:
      return "ET_EXEC";
    case ET_DYN:
      return "ET_DYN";
    case ET_CORE:
      return "ET_CORE";
    default:
      return "<unknown>";
    }
}

static int elf_type;
static bool has_program_load;
static bool has_sections;
static bool has_bits_alloc;
static bool has_program_interpreter;
static bool has_dynamic;
static bool has_soname;
static bool has_pie_flag;
static bool has_dt_debug;
static bool has_symtab;
static bool has_debug_sections;
static bool has_modinfo;
static bool has_gnu_linkonce_this_module;

static bool
run_classify (void)
{
  /* Reset to unanalyzed default.  */
  elf_type = 0;
  has_program_load = false;
  has_sections = false;
  has_bits_alloc = false;
  has_program_interpreter = false;
  has_dynamic = false;
  has_soname = false;
  has_pie_flag = false;
  has_dt_debug = false;
  has_symtab = false;
  has_debug_sections = false;
  has_modinfo = false;
  has_gnu_linkonce_this_module = false;

  int kind = elf_kind (elf);
  if (verbose > 0)
    fprintf (stderr, "info: %s: ELF kind: %s (0x%x)\n", current_path,
	     elf_kind_string (kind), kind);
  if (kind != ELF_K_ELF)
    return true;

  GElf_Ehdr ehdr_storage;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_storage);
  if (ehdr == NULL)
    {
      elf_issue (N_("ELF header"));
      return false;
    }
  elf_type = ehdr->e_type;

  /* Examine program headers.  */
  GElf_Phdr dyn_seg = { .p_type = 0 };
  {
    size_t nphdrs;
    if (elf_getphdrnum (elf, &nphdrs) != 0)
      {
	elf_issue (N_("program headers"));
	return false;
      }
    for (size_t phdr_idx = 0; phdr_idx < nphdrs; ++phdr_idx)
      {
	GElf_Phdr phdr_storage;
	GElf_Phdr *phdr = gelf_getphdr (elf, phdr_idx, &phdr_storage);
	if (phdr == NULL)
	  {
	    elf_issue (N_("program header"));
	    return false;
	  }
	if (phdr->p_type == PT_DYNAMIC)
	  {
	    dyn_seg = *phdr;
	    has_dynamic = true;
	  }
	if (phdr->p_type == PT_INTERP)
	  has_program_interpreter = true;
	if (phdr->p_type == PT_LOAD)
	  has_program_load = true;
      }
  }

  /* Do we have sections?  */
  {
    size_t nshdrs;
    if (elf_getshdrnum (elf, &nshdrs) != 0)
      {
	elf_issue (N_("section headers"));
	return false;
      }
    if (nshdrs > 0)
      has_sections = true;
  }

  {
    size_t shstrndx;
    if (unlikely (elf_getshdrstrndx (elf, &shstrndx) < 0))
      {
	elf_issue (N_("section header string table index"));
	return false;
      }

    Elf_Scn *scn = NULL;
    while (true)
      {
        scn = elf_nextscn (elf, scn);
        if (scn == NULL)
          break;
        GElf_Shdr shdr_storage;
        GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_storage);
        if (shdr == NULL)
	  {
            elf_issue (N_("could not obtain section header"));
	    return false;
	  }
        const char *section_name = elf_strptr (elf, shstrndx, shdr->sh_name);
        if (section_name == NULL)
	  {
            elf_issue(N_("could not obtain section name"));
	    return false;
	  }
        if (verbose > 2)
          fprintf (stderr, "debug: section header %s (type %d) found\n",
                   section_name, shdr->sh_type);
        if (shdr->sh_type == SHT_SYMTAB)
          {
            if (verbose > 1)
              fputs ("debug: symtab section found\n", stderr);
            has_symtab = true;
          }
	/* NOBITS and NOTE sections can be in any file.  We want to be
	   sure there is at least one other allocated section.  */
	if (shdr->sh_type != SHT_NOBITS
	    && shdr->sh_type != SHT_NOTE
	    && (shdr->sh_flags & SHF_ALLOC) != 0)
	  {
	    if (verbose > 1 && !has_bits_alloc)
	      fputs ("debug: allocated (non-nobits/note) section found\n",
		     stderr);
	    has_bits_alloc = true;
	  }
        if (startswith (section_name, ".debug_")
	    || startswith (section_name, ".zdebug_"))
          {
            if (verbose > 1 && !has_debug_sections)
              fputs ("debug: .debug_* section found\n", stderr);
            has_debug_sections = true;
          }
	if (strcmp (section_name, ".modinfo") == 0)
	  {
	    if (verbose > 1)
	      fputs ("debug: .modinfo section found\n", stderr);
	    has_modinfo = true;
	  }
	if (strcmp (section_name, ".gnu.linkonce.this_module") == 0)
	  {
	    if (verbose > 1)
	      fputs ("debug: .gnu.linkonce.this_module section found\n",
		     stderr);
	    has_gnu_linkonce_this_module = true;
	  }
      }
  }

  /* Examine the dynamic section.  */
  if (has_dynamic)
    {
      Elf_Data *data = elf_getdata_rawchunk (elf, dyn_seg.p_offset,
					     dyn_seg.p_filesz,
					     ELF_T_DYN);
      if (data != NULL)
	for (int dyn_idx = 0; ; ++dyn_idx)
	  {
	    GElf_Dyn dyn_storage;
	    GElf_Dyn *dyn = gelf_getdyn (data, dyn_idx, &dyn_storage);
	    if (dyn == NULL)
	      break;
	    if (verbose > 2)
	      fprintf (stderr, "debug: dynamic entry %d"
		       " with tag %llu found\n",
		       dyn_idx, (unsigned long long int) dyn->d_tag);
	    if (dyn->d_tag == DT_SONAME)
	      has_soname = true;
	    if (dyn->d_tag == DT_FLAGS_1 && (dyn->d_un.d_val & DF_1_PIE))
	      has_pie_flag = true;
	    if (dyn->d_tag == DT_DEBUG)
	      has_dt_debug = true;
	    if (dyn->d_tag == DT_NULL)
	      break;
	  }
    }

  if (verbose > 0)
    {
      fprintf (stderr, "info: %s: ELF type: %s (0x%x)\n", current_path,
	       elf_type_string (elf_type), elf_type);
      if (has_program_load)
        fprintf (stderr, "info: %s: PT_LOAD found\n", current_path);
      if (has_sections)
	fprintf (stderr, "info: %s: has sections\n", current_path);
      if (has_bits_alloc)
	fprintf (stderr, "info: %s: allocated (real) section found\n",
		 current_path);
      if (has_program_interpreter)
        fprintf (stderr, "info: %s: program interpreter found\n",
                 current_path);
      if (has_dynamic)
        fprintf (stderr, "info: %s: dynamic segment found\n", current_path);
      if (has_soname)
        fprintf (stderr, "info: %s: soname found\n", current_path);
      if (has_pie_flag)
        fprintf (stderr, "info: %s: DF_1_PIE flag found\n", current_path);
      if (has_dt_debug)
        fprintf (stderr, "info: %s: DT_DEBUG found\n", current_path);
      if (has_symtab)
        fprintf (stderr, "info: %s: symbol table found\n", current_path);
      if (has_debug_sections)
        fprintf (stderr, "info: %s: .debug_* section found\n", current_path);
      if (has_modinfo)
        fprintf (stderr, "info: %s: .modinfo section found\n", current_path);
      if (has_gnu_linkonce_this_module)
        fprintf (stderr,
		 "info: %s: .gnu.linkonce.this_module section found\n",
		 current_path);
    }

  return true;
}

static bool
is_elf (void)
{
  return elf_kind (elf) != ELF_K_NONE;
}

static bool
is_elf_file (void)
{
  return elf_kind (elf) == ELF_K_ELF;
}

static bool
is_elf_archive (void)
{
  return elf_kind (elf) == ELF_K_AR;
}

static bool
is_core (void)
{
  return elf_kind (elf) == ELF_K_ELF && elf_type == ET_CORE;
}

/* Return true if the file is a loadable object, which basically means
   it is an ELF file, but not a relocatable object or a core dump
   file.  (The kernel and various userspace components can load ET_REL
   files, but we disregard that for our classification purposes.)  */
static bool
is_loadable (void)
{
  return elf_kind (elf) == ELF_K_ELF
    && (elf_type == ET_EXEC || elf_type == ET_DYN)
    && has_program_load
    && (!has_sections || has_bits_alloc); /* It isn't debug-only.  */
}

/* Return true if the file is an ELF file which has a symbol table or
   .debug_* sections (and thus can be stripped further).  */
static bool
is_unstripped (void)
{
  return elf_kind (elf) != ELF_K_NONE
    && (elf_type == ET_REL || elf_type == ET_EXEC || elf_type == ET_DYN)
    && (has_symtab || has_debug_sections);
}

/* Return true if the file contains only debuginfo, but no loadable
   program bits.  Then it is most likely a separate .debug file, a dwz
   multi-file or a .dwo file.  Note that it can still be loadable,
   but in that case the phdrs shouldn't be trusted.  */
static bool
is_debug_only (void)
{
  return elf_kind (elf) != ELF_K_NONE
    && (elf_type == ET_REL || elf_type == ET_EXEC || elf_type == ET_DYN)
    && (has_debug_sections || has_symtab)
    && !has_bits_alloc;
}

static bool
is_shared (void)
{
  if (!is_loadable ())
    return false;

  /* The ELF type is very clear: this is an executable.  */
  if (elf_type == ET_EXEC)
    return false;

  /* If there is no dynamic section, the file cannot be loaded as a
     shared object.  */
  if (!has_dynamic)
    return false;

  /* If the object is marked as PIE, it is definitely an executable,
     and not a loadlable shared object.  */
  if (has_pie_flag)
    return false;

  /* Treat a DT_SONAME tag as a strong indicator that this is a shared
     object.  */
  if (has_soname)
    return true;

  /* This is probably a PIE program: there is no soname, but a program
     interpreter.  In theory, this file could be also a DSO with a
     soname implied by its file name that can be run as a program.
     This situation is impossible to resolve in the general case. */
  if (has_program_interpreter)
    return false;

  /* Roland McGrath mentions in
     <https://www.sourceware.org/ml/libc-alpha/2015-03/msg00605.html>,
     that “we defined a PIE as an ET_DYN with a DT_DEBUG”.  This
     matches current binutils behavior (version 2.32).  DT_DEBUG is
     added if bfd_link_executable returns true or if bfd_link_pic
     returns false, depending on the architectures.  However, DT_DEBUG
     is not documented as being specific to executables, therefore use
     it only as a low-priority discriminator.  */
  if (has_dt_debug)
    return false;

  return true;
}

static bool
is_executable (void)
{
  if (!is_loadable ())
    return false;

  /* A loadable object which is not a shared object is treated as an
     executable.  */
  return !is_shared ();
}

/* Like is_executable, but the object can also be a shared library at
   the same time.  */
static bool
is_program (void)
{
  if (!is_loadable ())
    return false;

  /* The ELF type is very clear: this is an executable.  */
  if (elf_type == ET_EXEC)
    return true;

  /* If the object is marked as PIE, it is definitely an executable,
     and not a loadlable shared object.  */
  if (has_pie_flag)
    return true;

  /* This is probably a PIE program. It isn't ET_EXEC, but has a
     program interpreter. In theory, this file could be also a DSO
     with a soname. This situation is impossible to resolve in the
     general case. See is_shared. This is different from
     is_executable.  */
  if (has_program_interpreter)
    return true;

  /* Roland McGrath mentions in
     <https://www.sourceware.org/ml/libc-alpha/2015-03/msg00605.html>,
     that “we defined a PIE as an ET_DYN with a DT_DEBUG”.  This
     matches current binutils behavior (version 2.32).  DT_DEBUG is
     added if bfd_link_executable returns true or if bfd_link_pic
     returns false, depending on the architectures.  However, DT_DEBUG
     is not documented as being specific to executables, therefore use
     it only as a low-priority discriminator.  */
  if (has_dt_debug)
    return true;

  return false;
}

/* Like is_shared but the library could also be an executable.  */
static bool
is_library  (void)
{
  /* Only ET_DYN can be shared libraries.  */
  if (elf_type != ET_DYN)
    return false;

  if (!is_loadable ())
    return false;

  /* Without a PT_DYNAMIC segment the library cannot be loaded.  */
  if (!has_dynamic)
    return false;

  /* This really is a (PIE) executable.  See is_shared.  */
  if (has_pie_flag || has_dt_debug)
    return false;

  /* It could still (also) be a (PIE) executable, but most likely you
     can dlopen it just fine.  */
  return true;
}

/* Returns true if the file is a linux kernel module (is ET_REL and
   has the two magic sections .modinfo and .gnu.linkonce.this_module).  */
static bool
is_linux_kernel_module (void)
{
  return (elf_kind (elf) == ELF_K_ELF
	  && elf_type == ET_REL
	  && has_modinfo
	  && has_gnu_linkonce_this_module);
}

enum classify_requirement { do_not_care, required, forbidden };

enum classify_check
{
  classify_elf,
  classify_elf_file,
  classify_elf_archive,
  classify_core,
  classify_unstripped,
  classify_executable,
  classify_program,
  classify_shared,
  classify_library,
  classify_linux_kernel_module,
  classify_debug_only,
  classify_loadable,

  classify_check_last = classify_loadable
};

enum
{
  classify_check_offset = 1000,
  classify_check_not_offset = 2000,

  classify_flag_stdin = 3000,
  classify_flag_stdin0,
  classify_flag_no_stdin,
  classify_flag_print,
  classify_flag_print0,
  classify_flag_no_print,
  classify_flag_matching,
  classify_flag_not_matching,
};

static bool
classify_check_positive (int key)
{
  return key >= classify_check_offset
    && key <= classify_check_offset + classify_check_last;
}

static bool
classify_check_negative (int key)
{
  return key >= classify_check_not_offset
    && key <= classify_check_not_offset + classify_check_last;
}

/* Set by parse_opt.  */
static enum classify_requirement requirements[classify_check_last + 1];
static enum { no_stdin, do_stdin, do_stdin0 } flag_stdin;
static enum { no_print, do_print, do_print0 } flag_print;
static bool flag_print_matching = true;

static error_t
parse_opt (int key, char *arg __attribute__ ((unused)),
           struct argp_state *state __attribute__ ((unused)))
{
  if (classify_check_positive (key))
    requirements[key - classify_check_offset] = required;
  else if (classify_check_negative (key))
    requirements[key - classify_check_not_offset] = forbidden;
  else
    switch (key)
      {
      case 'v':
        ++verbose;
        break;

      case 'q':
	--verbose;
	break;

      case 'z':
	flag_compressed = true;
	break;

      case 'f':
	flag_only_regular_files = true;
	break;

      case classify_flag_stdin:
        flag_stdin = do_stdin;
        break;

      case classify_flag_stdin0:
        flag_stdin = do_stdin0;
        break;

      case classify_flag_no_stdin:
        flag_stdin = no_stdin;
        break;

      case classify_flag_print:
        flag_print = do_print;
        break;

      case classify_flag_print0:
        flag_print = do_print0;
        break;

      case classify_flag_no_print:
        flag_print = no_print;
        break;

      case classify_flag_matching:
        flag_print_matching = true;
        break;

      case classify_flag_not_matching:
        flag_print_matching = false;
        break;

      default:
        return ARGP_ERR_UNKNOWN;
      }

  return 0;
}

/* Perform requested checks against the file at current_path.  If
   necessary, sets *STATUS to 1 if checks failed.  */
static void
process_current_path (int *status)
{
  bool checks_passed = true;

  if (open_elf () && run_classify ())
    {
      bool checks[] =
        {
	 [classify_elf] = is_elf (),
	 [classify_elf_file] = is_elf_file (),
	 [classify_elf_archive] = is_elf_archive (),
	 [classify_core] = is_core (),
	 [classify_unstripped] = is_unstripped (),
	 [classify_executable] = is_executable (),
	 [classify_program] = is_program (),
	 [classify_shared] = is_shared (),
	 [classify_library] = is_library (),
	 [classify_linux_kernel_module] = is_linux_kernel_module (),
	 [classify_debug_only] = is_debug_only (),
	 [classify_loadable] = is_loadable (),
	};

      if (verbose > 1)
        {
	  if (checks[classify_elf])
	    fprintf (stderr, "debug: %s: elf\n", current_path);
	  if (checks[classify_elf_file])
	    fprintf (stderr, "debug: %s: elf_file\n", current_path);
	  if (checks[classify_elf_archive])
	    fprintf (stderr, "debug: %s: elf_archive\n", current_path);
	  if (checks[classify_core])
	    fprintf (stderr, "debug: %s: core\n", current_path);
          if (checks[classify_unstripped])
            fprintf (stderr, "debug: %s: unstripped\n", current_path);
          if (checks[classify_executable])
            fprintf (stderr, "debug: %s: executable\n", current_path);
          if (checks[classify_program])
            fprintf (stderr, "debug: %s: program\n", current_path);
          if (checks[classify_shared])
            fprintf (stderr, "debug: %s: shared\n", current_path);
          if (checks[classify_library])
            fprintf (stderr, "debug: %s: library\n", current_path);
	  if (checks[classify_linux_kernel_module])
	    fprintf (stderr, "debug: %s: linux kernel module\n", current_path);
	  if (checks[classify_debug_only])
	    fprintf (stderr, "debug: %s: debug-only\n", current_path);
          if (checks[classify_loadable])
            fprintf (stderr, "debug: %s: loadable\n", current_path);
        }

      for (enum classify_check check = 0;
           check <= classify_check_last; ++check)
        switch (requirements[check])
          {
          case required:
            if (!checks[check])
              checks_passed = false;
            break;
          case forbidden:
            if (checks[check])
              checks_passed = false;
            break;
          case do_not_care:
            break;
          }
    }
  else if (file_fd == -1)
    checks_passed = false; /* There is nothing to check, bad file.  */
  else
    {
      for (enum classify_check check = 0;
           check <= classify_check_last; ++check)
        if (requirements[check] == required)
          checks_passed = false;
    }

  close_elf ();

  switch (flag_print)
    {
    case do_print:
      if (checks_passed == flag_print_matching)
        puts (current_path);
      break;
    case do_print0:
      if (checks_passed == flag_print_matching)
        if (fwrite (current_path, strlen (current_path) + 1, 1, stdout) < 1)
	  issue (errno, N_("writing to standard output"));
      break;
    case no_print:
      if (!checks_passed)
        *status = 1;
      break;
    }
}

/* Called to process standard input if flag_stdin is not no_stdin.  */
static void
process_stdin (int *status)
{
  char delim;
  if (flag_stdin == do_stdin0)
    delim = '\0';
  else
    delim = '\n';

  char *buffer = NULL;
  size_t buffer_size = 0;
  while (true)
    {
      ssize_t ret = getdelim (&buffer, &buffer_size, delim, stdin);
      if (ferror (stdin))
	{
	  current_path = NULL;
	  issue (errno, N_("reading from standard input"));
	  break;
	}
      if (feof (stdin))
        break;
      if (ret < 0)
        abort ();           /* Cannot happen due to error checks above.  */
      if (delim != '\0' && ret > 0 && buffer[ret - 1] == '\n')
        buffer[ret - 1] = '\0';
      current_path = buffer;
      process_current_path (status);
    }

  free (buffer);
}

int
main (int argc, char **argv)
{
  const struct argp_option options[] =
    {
      { NULL, 0, NULL, OPTION_DOC, N_("Classification options"), 1 },
      { "elf", classify_check_offset + classify_elf, NULL, 0,
        N_("File looks like an ELF object or archive/static library (default)")
	, 1 },
      { "elf-file", classify_check_offset + classify_elf_file, NULL, 0,
        N_("File is an regular ELF object (not an archive/static library)")
	, 1 },
      { "elf-archive", classify_check_offset + classify_elf_archive, NULL, 0,
        N_("File is an ELF archive or static library")
	, 1 },
      { "core", classify_check_offset + classify_core, NULL, 0,
        N_("File is an ELF core dump file")
	, 1 },
      { "unstripped", classify_check_offset + classify_unstripped, NULL, 0,
        N_("File is an ELF file with symbol table or .debug_* sections \
and can be stripped further"), 1 },
      { "executable", classify_check_offset + classify_executable, NULL, 0,
        N_("File is (primarily) an ELF program executable \
(not primarily a DSO)"), 1 },
      { "program", classify_check_offset + classify_program, NULL, 0,
        N_("File is an ELF program executable \
(might also be a DSO)"), 1 },
      { "shared", classify_check_offset + classify_shared, NULL, 0,
        N_("File is (primarily) an ELF shared object (DSO) \
(not primarily an executable)"), 1 },
      { "library", classify_check_offset + classify_library, NULL, 0,
        N_("File is an ELF shared object (DSO) \
(might also be an executable)"), 1 },
      { "linux-kernel-module", (classify_check_offset
				+ classify_linux_kernel_module), NULL, 0,
        N_("File is a linux kernel module"), 1 },
      { "debug-only", (classify_check_offset + classify_debug_only), NULL, 0,
        N_("File is a debug only ELF file \
(separate .debug, .dwo or dwz multi-file)"), 1 },
      { "loadable", classify_check_offset + classify_loadable, NULL, 0,
        N_("File is a loadable ELF object (program or shared object)"), 1 },

      /* Negated versions of the above.  */
      { "not-elf", classify_check_not_offset + classify_elf,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-elf-file", classify_check_not_offset + classify_elf_file,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-elf-archive", classify_check_not_offset + classify_elf_archive,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-core", classify_check_not_offset + classify_core,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-unstripped", classify_check_not_offset + classify_unstripped,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-executable", classify_check_not_offset + classify_executable,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-program", classify_check_not_offset + classify_program,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-shared", classify_check_not_offset + classify_shared,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-library", classify_check_not_offset + classify_library,
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-linux-kernel-module", (classify_check_not_offset
				    + classify_linux_kernel_module),
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-debug-only", (classify_check_not_offset + classify_debug_only),
        NULL, OPTION_HIDDEN, NULL, 1 },
      { "not-loadable", classify_check_not_offset + classify_loadable,
        NULL, OPTION_HIDDEN, NULL, 1 },

      { NULL, 0, NULL, OPTION_DOC, N_("Input flags"), 2 },
      { "file", 'f', NULL, 0,
        N_("Only classify regular (not symlink nor special device) files"), 2 },
      { "stdin", classify_flag_stdin, NULL, 0,
        N_("Also read file names to process from standard input, \
separated by newlines"), 2 },
      { "stdin0", classify_flag_stdin0, NULL, 0,
        N_("Also read file names to process from standard input, \
separated by ASCII NUL bytes"), 2 },
      { "no-stdin", classify_flag_no_stdin, NULL, 0,
        N_("Do not read files from standard input (default)"), 2 },
      { "compressed", 'z', NULL, 0,
	N_("Try to open compressed files or embedded (kernel) ELF images"),
	2 },

      { NULL, 0, NULL, OPTION_DOC, N_("Output flags"), 3 },
      { "print", classify_flag_print, NULL, 0,
        N_("Output names of files, separated by newline"), 3 },
      { "print0", classify_flag_print0, NULL, 0,
        N_("Output names of files, separated by ASCII NUL"), 3 },
      { "no-print", classify_flag_no_print, NULL, 0,
        N_("Do not output file names"), 3 },
      { "matching", classify_flag_matching, NULL, 0,
        N_("If printing file names, print matching files (default)"), 3 },
      { "not-matching", classify_flag_not_matching, NULL, 0,
        N_("If printing file names, print files that do not match"), 3 },

      { NULL, 0, NULL, OPTION_DOC, N_("Additional flags"), 4 },
      { "verbose", 'v', NULL, 0,
        N_("Output additional information (can be specified multiple times)"), 4 },
      { "quiet", 'q', NULL, 0,
        N_("Suppress some error output (counterpart to --verbose)"), 4 },
      { NULL, 0, NULL, 0, NULL, 0 }
    };

  const struct argp argp =
    {
      .options = options,
      .parser = parse_opt,
      .args_doc = N_("FILE..."),
      .doc = N_("\
Determine the type of an ELF file.\
\n\n\
All of the classification options must apply at the same time to a \
particular file.  Classification options can be negated using a \
\"--not-\" prefix.\
\n\n\
Since modern ELF does not clearly distinguish between programs and \
dynamic shared objects, you should normally use either --executable or \
--shared to identify the primary purpose of a file.  \
Only one of the --shared and --executable checks can pass for a file.\
\n\n\
If you want to know whether an ELF object might a program or a \
shared library (but could be both), then use --program or --library. \
Some ELF files will classify as both a program and a library.\
\n\n\
If you just want to know whether an ELF file is loadable (as program \
or library) use --loadable.  Note that files that only contain \
(separate) debug information (--debug-only) are never --loadable (even \
though they might contain program headers).  Linux kernel modules are \
also not --loadable (in the normal sense).\
\n\n\
Without any of the --print options, the program exits with status 0 \
if the requested checks pass for all input files, with 1 if a check \
fails for any file, and 2 if there is an environmental issue (such \
as a file read error or a memory allocation error).\
\n\n\
When printing file names, the program exits with status 0 even if \
no file names are printed, and exits with status 2 if there is an \
environmental issue.\
\n\n\
On usage error (e.g. a bad option was given), the program exits with \
a status code larger than 2.\
\n\n\
The --quiet or -q option suppresses some error warning output, but \
doesn't change the exit status.\
")
    };

  /* Require that the file is an ELF file by default.  User can
     disable with --not-elf.  */
  requirements[classify_elf] = required;

  int remaining;
  if (argp_parse (&argp, argc, argv, 0, &remaining, NULL) != 0)
    return 2;

  elf_version (EV_CURRENT);

  int status = 0;

  for (int i = remaining; i < argc; ++i)
    {
      current_path = argv[i];
      process_current_path (&status);
    }

  if (flag_stdin != no_stdin)
    process_stdin (&status);

  if (issue_found)
    return 2;

  return status;
}
