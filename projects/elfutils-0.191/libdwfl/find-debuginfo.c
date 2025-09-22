/* Standard find_debuginfo callback for libdwfl.
   Copyright (C) 2005-2010, 2014, 2015, 2019 Red Hat, Inc.
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
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "system.h"


/* Try to open [DIR/][SUBDIR/]DEBUGLINK, return file descriptor or -1.
   On success, *DEBUGINFO_FILE_NAME has the malloc'd name of the open file.  */
static int
try_open (const struct stat *main_stat,
	  const char *dir, const char *subdir, const char *debuglink,
	  char **debuginfo_file_name)
{
  char *fname;
  if (dir == NULL && subdir == NULL)
    {
      fname = strdup (debuglink);
      if (unlikely (fname == NULL))
	return -1;
    }
  else if ((subdir == NULL ? asprintf (&fname, "%s/%s", dir, debuglink)
	    : dir == NULL ? asprintf (&fname, "%s/%s", subdir, debuglink)
	    : asprintf (&fname, "%s/%s/%s", dir, subdir, debuglink)) < 0)
    return -1;

  struct stat st;
  int fd = TEMP_FAILURE_RETRY (open (fname, O_RDONLY));
  if (fd < 0)
    free (fname);
  else if (fstat (fd, &st) == 0
	   && st.st_ino == main_stat->st_ino
	   && st.st_dev == main_stat->st_dev)
    {
      /* This is the main file by another name.  Don't look at it again.  */
      free (fname);
      close (fd);
      errno = ENOENT;
      fd = -1;
    }
  else
    *debuginfo_file_name = fname;

  return fd;
}

/* Return true iff the FD's contents CRC matches DEBUGLINK_CRC.  */
static inline bool
check_crc (int fd, GElf_Word debuglink_crc)
{
  uint32_t file_crc;
  return (__libdwfl_crc32_file (fd, &file_crc) == 0
	  && file_crc == debuglink_crc);
}

static bool
validate (Dwfl_Module *mod, int fd, bool check, GElf_Word debuglink_crc)
{
  /* For alt debug files always check the build-id from the Dwarf and alt.  */
  if (mod->dw != NULL)
    {
      bool valid = false;
      const void *build_id;
      const char *altname;
      ssize_t build_id_len = INTUSE(dwelf_dwarf_gnu_debugaltlink) (mod->dw,
								   &altname,
								   &build_id);
      if (build_id_len > 0)
	{
	  /* We need to open an Elf handle on the file so we can check its
	     build ID note for validation.  Backdoor the handle into the
	     module data structure since we had to open it early anyway.  */
	  Dwfl_Error error = __libdw_open_file (&fd, &mod->alt_elf,
						false, false);
	  if (error != DWFL_E_NOERROR)
	    __libdwfl_seterrno (error);
	  else
	    {
	      const void *alt_build_id;
	      ssize_t alt_len = INTUSE(dwelf_elf_gnu_build_id) (mod->alt_elf,
								&alt_build_id);
	      if (alt_len > 0 && alt_len == build_id_len
		  && memcmp (build_id, alt_build_id, alt_len) == 0)
		valid = true;
	      else
		{
		  /* A mismatch!  */
		  elf_end (mod->alt_elf);
		  mod->alt_elf = NULL;
		  close (fd);
		  fd = -1;
		}
	    }
	}
      return valid;
    }

  /* If we have a build ID, check only that.  */
  if (mod->build_id_len > 0)
    {
      /* We need to open an Elf handle on the file so we can check its
	 build ID note for validation.  Backdoor the handle into the
	 module data structure since we had to open it early anyway.  */

      mod->debug.valid = false;
      Dwfl_Error error = __libdw_open_file (&fd, &mod->debug.elf, false, false);
      if (error != DWFL_E_NOERROR)
	__libdwfl_seterrno (error);
      else if (likely (__libdwfl_find_build_id (mod, false,
						mod->debug.elf) == 2))
	/* Also backdoor the gratuitous flag.  */
	mod->debug.valid = true;
      else
	{
	  /* A mismatch!  */
	  elf_end (mod->debug.elf);
	  mod->debug.elf = NULL;
	  close (fd);
	  fd = -1;
	}

      return mod->debug.valid;
    }

  return !check || check_crc (fd, debuglink_crc);
}

static int
find_debuginfo_in_path (Dwfl_Module *mod, const char *file_name,
			const char *debuglink_file, GElf_Word debuglink_crc,
			char **debuginfo_file_name)
{
  bool cancheck = debuglink_crc != (GElf_Word) 0;

  const char *file_basename = file_name == NULL ? NULL : xbasename (file_name);
  char *localname = NULL;

  /* We invent a debuglink .debug name if NULL, but then want to try the
     basename too.  */
  bool debuglink_null = debuglink_file == NULL;
  if (debuglink_null)
    {
      /* For a alt debug multi file we need a name, for a separate debug
	 name we may be able to fall back on file_basename.debug.  */
      if (file_basename == NULL || mod->dw != NULL)
	{
	  errno = 0;
	  return -1;
	}

      size_t len = strlen (file_basename);
      localname = malloc (len + sizeof ".debug");
      if (unlikely (localname == NULL))
	return -1;
      memcpy (localname, file_basename, len);
      memcpy (&localname[len], ".debug", sizeof ".debug");
      debuglink_file = localname;
      cancheck = false;
    }

  /* Look for a file named DEBUGLINK_FILE in the directories
     indicated by the debug directory path setting.  */

  const Dwfl_Callbacks *const cb = mod->dwfl->callbacks;
  char *localpath = strdup ((cb->debuginfo_path ? *cb->debuginfo_path : NULL)
			    ?: DEFAULT_DEBUGINFO_PATH);
  if (unlikely (localpath == NULL))
    {
      free (localname);
      return -1;
    }

  /* A leading - or + in the whole path sets whether to check file CRCs.  */
  bool defcheck = true;
  char *path = localpath;
  if (path[0] == '-' || path[0] == '+')
    {
      defcheck = path[0] == '+';
      ++path;
    }

  /* XXX dev/ino should be cached in struct dwfl_file.  */
  struct stat main_stat;
  if (unlikely ((mod->main.fd != -1 ? fstat (mod->main.fd, &main_stat)
		 : file_name != NULL ? stat (file_name, &main_stat)
		 : -1) < 0))
    {
      main_stat.st_dev = 0;
      main_stat.st_ino = 0;
    }

  char *file_dirname = (file_basename == file_name ? NULL
			: strndup (file_name, file_basename - 1 - file_name));
  if (file_basename != file_name && file_dirname == NULL)
    {
      free (localpath);
      free (localname);
      return -1;
    }
  char *p;
  while ((p = strsep (&path, ":")) != NULL)
    {
      /* A leading - or + says whether to check file CRCs for this element.  */
      bool check = defcheck;
      if (*p == '+' || *p == '-')
	check = *p++ == '+';
      check = check && cancheck;

      /* Try the basename too, if we made up the debuglink name and this
	 is not the main directory.  */
      bool try_file_basename;

      const char *dir, *subdir, *file;
      switch (p[0])
	{
	case '\0':
	  /* An empty entry says to try the main file's directory.  */
	  dir = file_dirname;
	  subdir = NULL;
	  file = debuglink_file;
	  try_file_basename = false;
	  break;
	case '/':
	  /* An absolute path says to look there for a subdirectory
	     named by the main file's absolute directory.  This cannot
	     be applied to a relative file name.  For alt debug files
	     it means to look for the basename file in that dir or the
	     .dwz subdir (see below).  */
	  if (mod->dw == NULL
	      && (file_dirname == NULL || file_dirname[0] != '/'))
	    continue;
	  dir = p;
	  if (mod->dw == NULL)
	    {
	      subdir = file_dirname;
	      /* We want to explore all sub-subdirs.  Chop off one slash
		 at a time.  */
	    explore_dir:
	      subdir = strchr (subdir, '/');
	      if (subdir != NULL)
		subdir = subdir + 1;
	      if (subdir && *subdir == 0)
		continue;
	      file = debuglink_file;
	    }
	  else
	    {
	      subdir = NULL;
	      file = xbasename (debuglink_file);
	    }
	  try_file_basename = debuglink_null;
	  break;
	default:
	  /* A relative path says to try a subdirectory of that name
	     in the main file's directory.  */
	  dir = file_dirname;
	  subdir = p;
	  file = debuglink_file;
	  try_file_basename = debuglink_null;
	  break;
	}

      char *fname = NULL;
      int fd = try_open (&main_stat, dir, subdir, file, &fname);
      if (fd < 0 && try_file_basename)
	fd = try_open (&main_stat, dir, subdir, file_basename, &fname);
      if (fd < 0)
	switch (errno)
	  {
	  case ENOENT:
	  case ENOTDIR:
	    /* If we are looking for the alt file also try the .dwz subdir.
	       But only if this is the empty or absolute path.  */
	    if (mod->dw != NULL && (p[0] == '\0' || p[0] == '/'))
	      {
		fd = try_open (&main_stat, dir, ".dwz",
			       xbasename (file), &fname);
		if (fd < 0)
		  {
		    if (errno != ENOENT && errno != ENOTDIR)
		      goto fail_free;
		    else
		      continue;
		  }
		break;
	      }
	    /* If possible try again with a sub-subdir.  */
	    if (mod->dw == NULL && subdir)
	      goto explore_dir;
	    continue;
	  default:
	    goto fail_free;
	  }
      if (validate (mod, fd, check, debuglink_crc))
	{
	  free (localpath);
	  free (localname);
	  free (file_dirname);
	  *debuginfo_file_name = fname;
	  return fd;
	}
      free (fname);
      close (fd);
    }

  /* No dice.  */
  errno = 0;
fail_free:
  free (localpath);
  free (localname);
  free (file_dirname);
  return -1;
}

int
dwfl_standard_find_debuginfo (Dwfl_Module *mod,
			      void **userdata __attribute__ ((unused)),
			      const char *modname __attribute__ ((unused)),
			      GElf_Addr base __attribute__ ((unused)),
			      const char *file_name,
			      const char *debuglink_file,
			      GElf_Word debuglink_crc,
			      char **debuginfo_file_name)
{
  if (mod == NULL)
    return -1;

  /* First try by build ID if we have one.  If that succeeds or fails
     other than just by finding nothing, that's all we do.  */
  const unsigned char *bits = NULL;
  GElf_Addr vaddr;
  int bits_len;
  if ((bits_len = INTUSE(dwfl_module_build_id) (mod, &bits, &vaddr)) > 0)
    {
      /* Dropping most arguments means we cannot rely on them in
	 dwfl_build_id_find_debuginfo.  But leave it that way since
	 some user code out there also does this, so we'll have to
	 handle it anyway.  */
      int fd = INTUSE(dwfl_build_id_find_debuginfo) (mod,
						     NULL, NULL, 0,
						     NULL, NULL, 0,
						     debuginfo_file_name);

      /* Did the build_id callback find something or report an error?
         Then we are done.  Otherwise fallback on path based search.  */
      if (fd >= 0
	  || (mod->dw == NULL && mod->debug.elf != NULL)
	  || (mod->dw != NULL && mod->alt_elf != NULL)
	  || errno != 0)
	return fd;
    }

  /* Failing that, search the path by name.  */
  int fd = find_debuginfo_in_path (mod, file_name,
				   debuglink_file, debuglink_crc,
				   debuginfo_file_name);

  if (fd < 0 && errno == 0 && file_name != NULL)
    {
      /* If FILE_NAME is a symlink, the debug file might be associated
	 with the symlink target name instead.  */

      char *canon = realpath (file_name, NULL);
      if (canon != NULL && strcmp (file_name, canon))
	fd = find_debuginfo_in_path (mod, canon,
				     debuglink_file, debuglink_crc,
				     debuginfo_file_name);
      free (canon);
    }

#ifdef ENABLE_LIBDEBUGINFOD
  /* Still nothing? Try if we can use the debuginfod client.
     But note that we might be looking for the alt file.
     We use the same trick as dwfl_build_id_find_debuginfo.
     If the debug file (dw) is already set, then we must be
     looking for the altfile. But we cannot use the actual
     file/path name given as hint. We'll have to lookup the
     alt file "build-id". Because the debuginfod client only
     handles build-ids.  */
  if (fd < 0)
    {
      if (mod->dw != NULL)
	{
	  const char *altname;
	  bits_len = INTUSE(dwelf_dwarf_gnu_debugaltlink) (mod->dw, &altname,
							   (const void **)
							   &bits);
	}

      if (bits_len > 0)
	fd = __libdwfl_debuginfod_find_debuginfo (mod->dwfl, bits, bits_len);
    }
#endif

  return fd;
}
INTDEF (dwfl_standard_find_debuginfo)
