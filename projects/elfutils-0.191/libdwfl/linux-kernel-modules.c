/* Standard libdwfl callbacks for debugging the running Linux kernel.
   Copyright (C) 2005-2011, 2013, 2014, 2015 Red Hat, Inc.
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

/* In case we have a bad fts we include this before config.h because it
   can't handle _FILE_OFFSET_BITS.
   Everything we need here is fine if its declarations just come first.
   Also, include sys/types.h before fts. On some systems fts.h is not self
   contained. */
#ifdef BAD_FTS
  #include <sys/types.h>
  #include <fts.h>
#endif

#include <config.h>
#include <system.h>

#include "libelfP.h"
#include "libdwflP.h"
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>

/* If fts.h is included before config.h, its indirect inclusions may not
   give us the right LFS aliases of these functions, so map them manually.  */
#ifdef BAD_FTS
  #ifdef _FILE_OFFSET_BITS
    #define open open64
    #define fopen fopen64
  #endif
#else
  #include <sys/types.h>
  #include <fts.h>
#endif


#define KERNEL_MODNAME	"kernel"

#define MODULEDIRFMT	"/lib/modules/%s"

#define KNOTESFILE	"/sys/kernel/notes"
#define	MODNOTESFMT	"/sys/module/%s/notes"
#define KSYMSFILE	"/proc/kallsyms"
#define MODULELIST	"/proc/modules"
#define	SECADDRDIRFMT	"/sys/module/%s/sections/"
#define MODULE_SECT_NAME_LEN 32	/* Minimum any linux/module.h has had.  */


static const char *vmlinux_suffixes[] =
  {
    ".gz",
#ifdef USE_BZLIB
    ".bz2",
#endif
#ifdef USE_LZMA
    ".xz",
#endif
  };

/* Try to open the given file as it is or under the debuginfo directory.  */
static int
try_kernel_name (Dwfl *dwfl, char **fname, bool try_debug)
{
  if (*fname == NULL)
    return -1;

  /* Don't bother trying *FNAME itself here if the path will cause it to be
     tried because we give its own basename as DEBUGLINK_FILE.  */
  int fd = ((((dwfl->callbacks->debuginfo_path
	       ? *dwfl->callbacks->debuginfo_path : NULL)
	      ?: DEFAULT_DEBUGINFO_PATH)[0] == ':') ? -1
	    : TEMP_FAILURE_RETRY (open (*fname, O_RDONLY)));

  if (fd < 0)
    {
      Dwfl_Module fakemod = { .dwfl = dwfl };

      if (try_debug)
	/* Passing NULL for DEBUGLINK_FILE searches for both the basenamer
	   "vmlinux" and the default of basename + ".debug", to look for
	   "vmlinux.debug" files.  */
	fd = INTUSE(dwfl_standard_find_debuginfo) (&fakemod, NULL, NULL, 0,
						   *fname, NULL, 0,
						   &fakemod.debug.name);
      else
	/* Try the file's unadorned basename as DEBUGLINK_FILE,
	   to look only for "vmlinux" files.  */
	fd = INTUSE(dwfl_standard_find_debuginfo) (&fakemod, NULL, NULL, 0,
						   *fname, xbasename (*fname),
						   0, &fakemod.debug.name);

      if (fakemod.debug.name != NULL)
	{
	  free (*fname);
	  *fname = fakemod.debug.name;
	}
    }

  if (fd < 0)
    for (size_t i = 0;
	 i < sizeof vmlinux_suffixes / sizeof vmlinux_suffixes[0] && fd < 0;
	 ++i)
      {
	char *zname;
	if (asprintf (&zname, "%s%s", *fname, vmlinux_suffixes[i]) > 0)
	  {
	    fd = TEMP_FAILURE_RETRY (open (zname, O_RDONLY));
	    if (fd < 0)
	      free (zname);
	    else
	      {
		free (*fname);
		*fname = zname;
	      }
	  }
      }

  if (fd < 0)
    {
      free (*fname);
      *fname = NULL;
    }

  return fd;
}

static inline const char *
kernel_release (void)
{
#ifdef __linux__
  /* Cache the `uname -r` string we'll use.  */
  static struct utsname utsname;
  if (utsname.release[0] == '\0' && uname (&utsname) != 0)
    return NULL;
  return utsname.release;
#else
  /* Used for finding the running linux kernel, which isn't supported
     on non-linux kernel systems.  */
  errno = ENOTSUP;
  return NULL;
#endif
}

static int
find_kernel_elf (Dwfl *dwfl, const char *release, char **fname)
{
  /* First try to find an uncompressed vmlinux image.  Possibly
     including debuginfo.  */
  if (release == NULL
      || ((release[0] == '/'
	   ? asprintf (fname, "%s/vmlinux", release)
	   : asprintf (fname, "/boot/vmlinux-%s", release)) < 0))
    return -1;

  int fd = try_kernel_name (dwfl, fname, true);
  if (fd < 0 && release[0] != '/')
    {
      free (*fname);
      if (asprintf (fname, MODULEDIRFMT "/vmlinux", release) < 0)
	return -1;
      fd = try_kernel_name (dwfl, fname, true);
    }

  /* There might be a compressed vmlinuz image.  Probably without
     debuginfo, but try to find it under the debug path also, just in
     case.  */
  if (fd < 0)
    {
      free (*fname);
      if ((release[0] == '/'
           ? asprintf (fname, "%s/vmlinuz", release)
           : asprintf (fname, "/boot/vmlinuz-%s", release)) < 0)
        return -1;

      fd = try_kernel_name (dwfl, fname, true);
      if (fd < 0 && release[0] != '/')
	{
	  free (*fname);
	  if (asprintf (fname, MODULEDIRFMT "/vmlinuz", release) < 0)
	    return -1;
	  fd = try_kernel_name (dwfl, fname, true);
	}
    }

  return fd;
}

static int
get_release (Dwfl *dwfl, const char **release)
{
  if (dwfl == NULL)
    return -1;

  const char *release_string = release == NULL ? NULL : *release;
  if (release_string == NULL)
    {
      release_string = kernel_release ();
      if (release_string == NULL)
	return errno;
      if (release != NULL)
	*release = release_string;
    }

  return 0;
}

static int
report_kernel (Dwfl *dwfl, const char **release,
	       int (*predicate) (const char *module, const char *file))
{
  int result = get_release (dwfl, release);
  if (unlikely (result != 0))
    return result;

  if (release == NULL || *release == NULL)
    return EINVAL;

  char *fname;
  int fd = find_kernel_elf (dwfl, *release, &fname);

  if (fd < 0)
    result = ((predicate != NULL && !(*predicate) (KERNEL_MODNAME, NULL))
	      ? 0 : errno ?: ENOENT);
  else
    {
      bool report = true;

      if (predicate != NULL)
	{
	  /* Let the predicate decide whether to use this one.  */
	  int want = (*predicate) (KERNEL_MODNAME, fname);
	  if (want < 0)
	    result = errno;
	  report = want > 0;
	}

      if (report)
	{
	  /* Note that on some architectures (e.g. x86_64) the vmlinux
	     is ET_EXEC, while on others (e.g. ppc64) it is ET_DYN.
	     In both cases the phdr p_vaddr load address will be non-zero.
	     We want the image to be placed as if it was ET_DYN, so
	     pass true for add_p_vaddr which will do the right thing
	     (in combination with a zero base) in either case.  */
	  Dwfl_Module *mod = INTUSE(dwfl_report_elf) (dwfl, KERNEL_MODNAME,
						      fname, fd, 0, true);
	  if (mod == NULL)
	    result = -1;
	  else
	    /* The kernel is ET_EXEC, but always treat it as relocatable.  */
	    mod->e_type = ET_DYN;
	}

      free (fname);

      if (!report || result < 0)
	close (fd);
    }

  return result;
}

/* Look for a kernel debug archive.  If we find one, report all its modules.
   If not, return ENOENT.  */
static int
report_kernel_archive (Dwfl *dwfl, const char **release,
		       int (*predicate) (const char *module, const char *file))
{
  int result = get_release (dwfl, release);
  if (unlikely (result != 0))
    return result;

  if (release == NULL || *release == NULL)
    return EINVAL;

  char *archive;
  int res = (((*release)[0] == '/')
	     ? asprintf (&archive, "%s/debug.a", *release)
	     : asprintf (&archive, MODULEDIRFMT "/debug.a", *release));
  if (unlikely (res < 0))
    return ENOMEM;

  int fd = try_kernel_name (dwfl, &archive, false);
  if (fd < 0)
    result = errno ?: ENOENT;
  else
    {
      /* We have the archive file open!  */
      Dwfl_Module *last = __libdwfl_report_offline (dwfl, NULL, archive, fd,
						    true, predicate);
      if (unlikely (last == NULL))
	result = -1;
      else
	{
	  /* Find the kernel and move it to the head of the list.  */
	  Dwfl_Module **tailp = &dwfl->modulelist, **prevp = tailp;
	  for (Dwfl_Module *m = *prevp; m != NULL; m = *(prevp = &m->next))
	    if (!m->gc && m->e_type != ET_REL && !strcmp (m->name, "kernel"))
	      {
		*prevp = m->next;
		m->next = *tailp;
		*tailp = m;
		break;
	      }
	}
    }

  free (archive);
  return result;
}

static size_t
check_suffix (const FTSENT *f, size_t namelen)
{
#define TRY(sfx)							\
  if ((namelen ? f->fts_namelen == namelen + sizeof sfx - 1		\
       : f->fts_namelen >= sizeof sfx)					\
      && !memcmp (f->fts_name + f->fts_namelen - (sizeof sfx - 1),	\
		  sfx, sizeof sfx))					\
    return sizeof sfx - 1

  TRY (".ko");
  TRY (".ko.gz");
#if USE_BZLIB
  TRY (".ko.bz2");
#endif
#if USE_LZMA
  TRY (".ko.xz");
#endif
#if USE_ZSTD
  TRY (".ko.zst");
#endif

  return 0;

#undef	TRY
}

/* Report a kernel and all its modules found on disk, for offline use.
   If RELEASE starts with '/', it names a directory to look in;
   if not, it names a directory to find under /lib/modules/;
   if null, /lib/modules/`uname -r` is used.
   Returns zero on success, -1 if dwfl_report_module failed,
   or an errno code if finding the files on disk failed.  */
int
dwfl_linux_kernel_report_offline (Dwfl *dwfl, const char *release,
				  int (*predicate) (const char *module,
						    const char *file))
{
  int result = report_kernel_archive (dwfl, &release, predicate);
  if (result != ENOENT)
    return result;

  /* First report the kernel.  */
  result = report_kernel (dwfl, &release, predicate);
  if (result == 0)
    {
      /* Do "find /lib/modules/RELEASE -name *.ko".  */

      char *modulesdir[] = { NULL, NULL };
      if (release[0] == '/')
	modulesdir[0] = (char *) release;
      else
	{
	  if (asprintf (&modulesdir[0], MODULEDIRFMT, release) < 0)
	    return errno;
	}

      FTS *fts = fts_open (modulesdir, FTS_NOSTAT | FTS_LOGICAL, NULL);
      if (modulesdir[0] == (char *) release)
	modulesdir[0] = NULL;
      if (fts == NULL)
	{
	  free (modulesdir[0]);
	  return errno;
	}

      FTSENT *f;
      while ((f = fts_read (fts)) != NULL)
	{
	  /* Skip a "source" subtree, which tends to be large.
	     This insane hard-coding of names is what depmod does too.  */
	  if (f->fts_namelen == sizeof "source" - 1
	      && !strcmp (f->fts_name, "source"))
	    {
	      fts_set (fts, f, FTS_SKIP);
	      continue;
	    }

	  switch (f->fts_info)
	    {
	    case FTS_F:
	    case FTS_SL:
	    case FTS_NSOK:;
	      /* See if this file name matches "*.ko".  */
	      const size_t suffix = check_suffix (f, 0);
	      if (suffix)
		{
		  /* We have a .ko file to report.  Following the algorithm
		     by which the kernel makefiles set KBUILD_MODNAME, we
		     replace all ',' or '-' with '_' in the file name and
		     call that the module name.  Modules could well be
		     built using different embedded names than their file
		     names.  To handle that, we would have to look at the
		     __this_module.name contents in the module's text.  */

		  char *name = strndup (f->fts_name, f->fts_namelen - suffix);
		  if (unlikely (name == NULL))
		    {
		      __libdwfl_seterrno (DWFL_E_NOMEM);
		      result = -1;
		      break;
		    }
		  for (size_t i = 0; i < f->fts_namelen - suffix; ++i)
		    if (name[i] == '-' || name[i] == ',')
		      name[i] = '_';

		  if (predicate != NULL)
		    {
		      /* Let the predicate decide whether to use this one.  */
		      int want = (*predicate) (name, f->fts_path);
		      if (want < 0)
			{
			  result = -1;
			  free (name);
			  break;
			}
		      if (!want)
			{
			  free (name);
			  continue;
			}
		    }

		  if (dwfl_report_offline (dwfl, name, f->fts_path, -1) == NULL)
		    {
		      free (name);
		      result = -1;
		      break;
		    }
		  free (name);
		}
	      continue;

	    case FTS_ERR:
	    case FTS_DNR:
	    case FTS_NS:
	      result = f->fts_errno;
	      break;

	    case FTS_SLNONE:
	    default:
	      continue;
	    }

	  /* We only get here in error cases.  */
	  break;
	}
      fts_close (fts);
      free (modulesdir[0]);
    }

  return result;
}
INTDEF (dwfl_linux_kernel_report_offline)


/* State of read_address used by intuit_kernel_bounds. */
struct read_address_state {
  FILE *f;
  char *line;
  size_t linesz;
  size_t n;
  char *p;
  const char *type;
};

static inline bool
read_address (struct read_address_state *state, Dwarf_Addr *addr)
{
  if ((state->n = getline (&state->line, &state->linesz, state->f)) < 1 ||
      state->line[state->n - 2] == ']')
    return false;
  *addr = strtoull (state->line, &state->p, 16);
  state->p += strspn (state->p, " \t");
  state->type = strsep (&state->p, " \t\n");
  if (state->type == NULL)
    return false;
  return state->p != NULL && state->p != state->line;
}


/* Grovel around to guess the bounds of the runtime kernel image.  */
static int
intuit_kernel_bounds (Dwarf_Addr *start, Dwarf_Addr *end, Dwarf_Addr *notes)
{
  struct read_address_state state = { NULL, NULL, 0, 0, NULL, NULL };

  *notes = 0;

  state.f = fopen (KSYMSFILE, "r");
  if (state.f == NULL)
    return errno;

  (void) __fsetlocking (state.f, FSETLOCKING_BYCALLER);

  int result;
  do
    result = read_address (&state, start) ? 0 : -1;
  while (result == 0 && strchr ("TtRr", *state.type) == NULL);

  if (result == 0)
    {
      Dwarf_Addr addr;
      *end = *start;
      while (read_address (&state, &addr) && addr >= *end)
	{
	  *end = addr;
	  if (*notes == 0 && !strcmp (state.p, "__start_notes\n"))
	    *notes = *end;
	}

      Dwarf_Addr round_kernel = sysconf (_SC_PAGESIZE);
      *start &= -(Dwarf_Addr) round_kernel;
      *end += round_kernel - 1;
      *end &= -(Dwarf_Addr) round_kernel;
      if (*start >= *end || *end - *start < round_kernel)
	result = -1;
    }
  free (state.line);

  if (result == -1)
    result = ferror_unlocked (state.f) ? errno : ENOEXEC;

  fclose (state.f);

  return result;
}


/* Look for a build ID note in NOTESFILE and associate the ID with MOD.  */
static int
check_notes (Dwfl_Module *mod, const char *notesfile,
	     Dwarf_Addr vaddr, const char *secname)
{
  int fd = open (notesfile, O_RDONLY);
  if (fd < 0)
    return 1;

  assert (sizeof (Elf32_Nhdr) == sizeof (GElf_Nhdr));
  assert (sizeof (Elf64_Nhdr) == sizeof (GElf_Nhdr));
  union
  {
    GElf_Nhdr nhdr;
    unsigned char data[8192];
  } buf;

  ssize_t n = read (fd, buf.data, sizeof buf);
  close (fd);

  if (n <= 0)
    return 1;

  unsigned char *p = buf.data;
  size_t len = 0;
  while (p < &buf.data[n])
    {
      /* No translation required since we are reading the native kernel.  */
      GElf_Nhdr *nhdr = (void *) p;
      len += sizeof *nhdr;
      p += len;
      unsigned char *name = p;
      unsigned char *bits;
      /* This is somewhat ugly, GNU Property notes use different padding,
	 but all we have is the file content, so we have to actually check
	 the name and type.  */
      if (nhdr->n_type == NT_GNU_PROPERTY_TYPE_0
          && nhdr->n_namesz == sizeof "GNU"
          && name + nhdr->n_namesz < &buf.data[n]
          && !memcmp (name, "GNU", sizeof "GNU"))
	{
	  len += nhdr->n_namesz;
	  len = NOTE_ALIGN8 (len);
	  p = buf.data + len;
	  bits = p;
	  len += nhdr->n_descsz;
	  len = NOTE_ALIGN8 (len);
	  p = buf.data + len;
	}
      else
	{
	  len += nhdr->n_namesz;
	  len = NOTE_ALIGN4 (len);
	  p = buf.data + len;
	  bits = p;
	  len += nhdr->n_descsz;
	  len = NOTE_ALIGN4 (len);
	  p = buf.data + len;
	}

      if (p <= &buf.data[n]
	  && nhdr->n_type == NT_GNU_BUILD_ID
	  && nhdr->n_namesz == sizeof "GNU"
	  && !memcmp (name, "GNU", sizeof "GNU"))
	{
	  /* Found it.  For a module we must figure out its VADDR now.  */

	  if (secname != NULL
	      && (INTUSE(dwfl_linux_kernel_module_section_address)
		  (mod, NULL, mod->name, 0, secname, 0, NULL, &vaddr) != 0
		  || vaddr == (GElf_Addr) -1l))
	    vaddr = 0;

	  if (vaddr != 0)
	    vaddr += bits - buf.data;
	  return INTUSE(dwfl_module_report_build_id) (mod, bits,
						      nhdr->n_descsz, vaddr);
	}
    }

  return 0;
}

/* Look for a build ID for the kernel.  */
static int
check_kernel_notes (Dwfl_Module *kernelmod, GElf_Addr vaddr)
{
  return check_notes (kernelmod, KNOTESFILE, vaddr, NULL) < 0 ? -1 : 0;
}

/* Look for a build ID for a loaded kernel module.  */
static int
check_module_notes (Dwfl_Module *mod)
{
  char *dirs[2] = { NULL, NULL };
  if (asprintf (&dirs[0], MODNOTESFMT, mod->name) < 0)
    return ENOMEM;

  FTS *fts = fts_open (dirs, FTS_NOSTAT | FTS_LOGICAL, NULL);
  if (fts == NULL)
    {
      free (dirs[0]);
      return 0;
    }

  int result = 0;
  FTSENT *f;
  while ((f = fts_read (fts)) != NULL)
    {
      switch (f->fts_info)
	{
	case FTS_F:
	case FTS_SL:
	case FTS_NSOK:
	  result = check_notes (mod, f->fts_accpath, 0, f->fts_name);
	  if (result > 0)	/* Nothing found.  */
	    {
	      result = 0;
	      continue;
	    }
	  break;

	case FTS_ERR:
	case FTS_DNR:
	  result = f->fts_errno;
	  break;

	case FTS_NS:
	case FTS_SLNONE:
	default:
	  continue;
	}

      /* We only get here when finished or in error cases.  */
      break;
    }
  fts_close (fts);
  free (dirs[0]);

  return result;
}

int
dwfl_linux_kernel_report_kernel (Dwfl *dwfl)
{
  Dwarf_Addr start = 0;
  Dwarf_Addr end = 0;

  #define report() \
    (INTUSE(dwfl_report_module) (dwfl, KERNEL_MODNAME, start, end))

  /* This is a bit of a kludge.  If we already reported the kernel,
     don't bother figuring it out again--it never changes.  */
  for (Dwfl_Module *m = dwfl->modulelist; m != NULL; m = m->next)
    if (!strcmp (m->name, KERNEL_MODNAME))
      {
	start = m->low_addr;
	end = m->high_addr;
	return report () == NULL ? -1 : 0;
      }

  /* Try to figure out the bounds of the kernel image without
     looking for any vmlinux file.  */
  Dwarf_Addr notes;
  int result = intuit_kernel_bounds (&start, &end, &notes);
  if (result == 0)
    {
      Dwfl_Module *mod = report ();
      return unlikely (mod == NULL) ? -1 : check_kernel_notes (mod, notes);
    }
  if (result != ENOENT)
    return result;

  /* Find the ELF file for the running kernel and dwfl_report_elf it.  */
  return report_kernel (dwfl, NULL, NULL);
}
INTDEF (dwfl_linux_kernel_report_kernel)


static inline bool
subst_name (char from, char to,
            const char * const module_name,
            char * const alternate_name,
            const size_t namelen)
{
  const char *n = memchr (module_name, from, namelen);
  if (n == NULL)
    return false;
  char *a = mempcpy (alternate_name, module_name, n - module_name);
  *a++ = to;
  ++n;
  const char *p;
  while ((p = memchr (n, from, namelen - (n - module_name))) != NULL)
    {
      a = mempcpy (a, n, p - n);
      *a++ = to;
      n = p + 1;
    }
  memcpy (a, n, namelen - (n - module_name) + 1);
  return true;
}

/* Dwfl_Callbacks.find_elf for the running Linux kernel and its modules.  */

int
dwfl_linux_kernel_find_elf (Dwfl_Module *mod,
			    void **userdata __attribute__ ((unused)),
			    const char *module_name,
			    Dwarf_Addr base __attribute__ ((unused)),
			    char **file_name, Elf **elfp)
{
  if (mod->build_id_len > 0)
    {
      int fd = INTUSE(dwfl_build_id_find_elf) (mod, NULL, NULL, 0,
					       file_name, elfp);
      if (fd >= 0 || mod->main.elf != NULL || errno != 0)
	return fd;
    }

  const char *release = kernel_release ();
  if (release == NULL)
    return errno;

  if (!strcmp (module_name, KERNEL_MODNAME))
    return find_kernel_elf (mod->dwfl, release, file_name);

  /* Do "find /lib/modules/`uname -r` -name MODULE_NAME.ko".  */

  char *modulesdir[] = { NULL, NULL };
  if (asprintf (&modulesdir[0], MODULEDIRFMT, release) < 0)
    return -1;

  FTS *fts = fts_open (modulesdir, FTS_NOSTAT | FTS_LOGICAL, NULL);
  if (fts == NULL)
    {
      free (modulesdir[0]);
      return -1;
    }

  size_t namelen = strlen (module_name);

  /* This is a kludge.  There is no actual necessary relationship between
     the name of the .ko file installed and the module name the kernel
     knows it by when it's loaded.  The kernel's only idea of the module
     name comes from the name embedded in the object's magic
     .gnu.linkonce.this_module section.

     In practice, these module names match the .ko file names except for
     some using '_' and some using '-'.  So our cheap kludge is to look for
     two files when either a '_' or '-' appears in a module name, one using
     only '_' and one only using '-'.  */

  char *alternate_name = malloc (namelen + 1);
  if (unlikely (alternate_name == NULL))
    {
      free (modulesdir[0]);
      return ENOMEM;
    }
  if (!subst_name ('-', '_', module_name, alternate_name, namelen) &&
      !subst_name ('_', '-', module_name, alternate_name, namelen))
    alternate_name[0] = '\0';

  FTSENT *f;
  int error = ENOENT;
  while ((f = fts_read (fts)) != NULL)
    {
      /* Skip a "source" subtree, which tends to be large.
	 This insane hard-coding of names is what depmod does too.  */
      if (f->fts_namelen == sizeof "source" - 1
	  && !strcmp (f->fts_name, "source"))
	{
	  fts_set (fts, f, FTS_SKIP);
	  continue;
	}

      error = ENOENT;
      switch (f->fts_info)
	{
	case FTS_F:
	case FTS_SL:
	case FTS_NSOK:
	  /* See if this file name is "MODULE_NAME.ko".  */
	  if (check_suffix (f, namelen)
	      && (!memcmp (f->fts_name, module_name, namelen)
		  || !memcmp (f->fts_name, alternate_name, namelen)))
	    {
	      int fd = open (f->fts_accpath, O_RDONLY);
	      *file_name = strdup (f->fts_path);
	      fts_close (fts);
	      free (modulesdir[0]);
	      free (alternate_name);
	      if (fd < 0)
		free (*file_name);
	      else if (*file_name == NULL)
		{
		  close (fd);
		  fd = -1;
		}
	      return fd;
	    }
	  break;

	case FTS_ERR:
	case FTS_DNR:
	case FTS_NS:
	  error = f->fts_errno;
	  break;

	case FTS_SLNONE:
	default:
	  break;
	}
    }

  fts_close (fts);
  free (modulesdir[0]);
  free (alternate_name);
  errno = error;
  return -1;
}
INTDEF (dwfl_linux_kernel_find_elf)


/* Dwfl_Callbacks.section_address for kernel modules in the running Linux.
   We read the information from /sys/module directly.  */

int
dwfl_linux_kernel_module_section_address
(Dwfl_Module *mod __attribute__ ((unused)),
 void **userdata __attribute__ ((unused)),
 const char *modname, Dwarf_Addr base __attribute__ ((unused)),
 const char *secname, Elf32_Word shndx __attribute__ ((unused)),
 const GElf_Shdr *shdr __attribute__ ((unused)),
 Dwarf_Addr *addr)
{
  char *sysfile;
  if (asprintf (&sysfile, SECADDRDIRFMT "%s", modname, secname) < 0)
    return DWARF_CB_ABORT;

  FILE *f = fopen (sysfile, "r");
  free (sysfile);

  if (f == NULL)
    {
      if (errno == ENOENT)
	{
	  /* The .modinfo and .data.percpu sections are never kept
	     loaded in the kernel.  If the kernel was compiled without
	     CONFIG_MODULE_UNLOAD, the .exit.* sections are not
	     actually loaded at all.

	     Setting *ADDR to -1 tells the caller this section is
	     actually absent from memory.  */

	  if (!strcmp (secname, ".modinfo")
	      || !strcmp (secname, ".data.percpu")
	      || startswith (secname, ".exit"))
	    {
	      *addr = (Dwarf_Addr) -1l;
	      return DWARF_CB_OK;
	    }

	  /* The goofy PPC64 module_frob_arch_sections function tweaks
	     the section names as a way to control other kernel code's
	     behavior, and this cruft leaks out into the /sys information.
	     The file name for ".init*" may actually look like "_init*".  */

	  const bool is_init = startswith (secname, ".init");
	  if (is_init)
	    {
	      if (asprintf (&sysfile, SECADDRDIRFMT "_%s",
			    modname, &secname[1]) < 0)
		return ENOMEM;
	      f = fopen (sysfile, "r");
	      free (sysfile);
	      if (f != NULL)
		goto ok;
	    }

	  /* The kernel truncates section names to MODULE_SECT_NAME_LEN - 1.
	     In case that size increases in the future, look for longer
	     truncated names first.  */
	  size_t namelen = strlen (secname);
	  if (namelen >= MODULE_SECT_NAME_LEN)
	    {
	      int len = asprintf (&sysfile, SECADDRDIRFMT "%s",
				  modname, secname);
	      if (len < 0)
		return DWARF_CB_ABORT;
	      char *end = sysfile + len;
	      do
		{
		  *--end = '\0';
		  f = fopen (sysfile, "r");
		  if (is_init && f == NULL && errno == ENOENT)
		    {
		      sysfile[len - namelen] = '_';
		      f = fopen (sysfile, "r");
		      sysfile[len - namelen] = '.';
		    }
		}
	      while (f == NULL && errno == ENOENT
		     && end - &sysfile[len - namelen] >= MODULE_SECT_NAME_LEN);
	      free (sysfile);

	      if (f != NULL)
		goto ok;
	    }
	}

      return DWARF_CB_ABORT;
    }

 ok:
  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  int result = (fscanf (f, "%" PRIx64 "\n", addr) == 1 ? 0
		: ferror_unlocked (f) ? errno : ENOEXEC);
  fclose (f);

  if (result == 0)
    return DWARF_CB_OK;

  errno = result;
  return DWARF_CB_ABORT;
}
INTDEF (dwfl_linux_kernel_module_section_address)

int
dwfl_linux_kernel_report_modules (Dwfl *dwfl)
{
  FILE *f = fopen (MODULELIST, "r");
  if (f == NULL)
    return errno;

  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  int result = 0;
  Dwarf_Addr modaddr;
  unsigned long int modsz;
  char modname[128+1];
  char *line = NULL;
  size_t linesz = 0;
  /* We can't just use fscanf here because it's not easy to distinguish \n
     from other whitespace so as to take the optional word following the
     address but always stop at the end of the line.  */
  while (getline (&line, &linesz, f) > 0
	 && sscanf (line, "%128s %lu %*s %*s %*s %" PRIx64 " %*s\n",
		    modname, &modsz, &modaddr) == 3)
    {
      Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, modname,
						     modaddr, modaddr + modsz);
      if (mod == NULL)
	{
	  result = -1;
	  break;
	}

      result = check_module_notes (mod);
    }
  free (line);

  if (result == 0)
    result = ferror_unlocked (f) ? errno : feof_unlocked (f) ? 0 : ENOEXEC;

  fclose (f);

  return result;
}
INTDEF (dwfl_linux_kernel_report_modules)
