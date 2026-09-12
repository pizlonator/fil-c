/* Temporary file handling for tests.
   Copyright (C) 1998-2026 Free Software Foundation, Inc.
   Copyright The GNU Tools Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* This is required to get an mkstemp which can create large files on
   some 32-bit platforms. */
#define _FILE_OFFSET_BITS 64

#include <support/check.h>
#include <support/temp_file.h>
#include <support/temp_file-internal.h>
#include <support/support.h>

#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xunistd.h>

/* List of temporary files.  */
static struct temp_name_list
{
  struct temp_name_list *next;
  char *name;
  pid_t owner;
  bool toolong;
} *temp_name_list;

/* Location of the temporary files.  Set by the test skeleton via
   support_set_test_dir.  The string is not be freed.  */
static const char *test_dir = _PATH_TMP;

/* Name of subdirectories in a too long temporary directory tree.  */
static char toolong_subdir[NAME_MAX + 1];
static bool toolong_initialized;
static size_t toolong_path_max;

static void
add_temp_file_internal (const char *name, bool toolong)
{
  struct temp_name_list *newp
    = (struct temp_name_list *) xcalloc (sizeof (*newp), 1);
  char *newname = strdup (name);
  if (newname != NULL)
    {
      newp->name = newname;
      newp->next = temp_name_list;
      newp->owner = getpid ();
      newp->toolong = toolong;
      temp_name_list = newp;
    }
  else
    free (newp);
}

void
add_temp_file (const char *name)
{
  add_temp_file_internal (name, false);
}

int
create_temp_file_in_dir (const char *base, const char *dir, char **filename)
{
  char *fname;
  int fd;

  fname = xasprintf ("%s/%sXXXXXX", dir, base);

  fd = mkstemp (fname);
  if (fd == -1)
    {
      printf ("cannot open temporary file '%s': %m\n", fname);
      free (fname);
      return -1;
    }

  add_temp_file (fname);
  if (filename != NULL)
    *filename = fname;
  else
    free (fname);

  return fd;
}

int
create_temp_file (const char *base, char **filename)
{
  return create_temp_file_in_dir (base, test_dir, filename);
}

static char *
create_temp_directory_internal (const char *base, bool toolong)
{
  char *path = xasprintf ("%s/%sXXXXXX", test_dir, base);
  if (mkdtemp (path) == NULL)
    {
      printf ("error: mkdtemp (\"%s\"): %m", path);
      exit (1);
    }
  add_temp_file_internal (path, toolong);
  return path;
}

char *
support_create_temp_directory (const char *base)
{
  return create_temp_directory_internal (base, false);
}

static void
ensure_toolong_initialized (void)
{
  if (!toolong_initialized)
    FAIL_EXIT1 ("uninitialized toolong directory tree\n");
}

static void
initialize_toolong (const char *base)
{
  long name_max = pathconf (base, _PC_NAME_MAX);
  name_max = (name_max < 0 ? 64
	      : (name_max < sizeof (toolong_subdir) ? name_max
		 : sizeof (toolong_subdir) - 1));

  long path_max = pathconf (base, _PC_PATH_MAX);
  path_max = (path_max < 0 ? 1024
	      : path_max <= PTRDIFF_MAX ? path_max : PTRDIFF_MAX);

  /* Sanity check to ensure that the test does not create temporary directories
     in different filesystems because this API doesn't support it.  */
  if (toolong_initialized)
    {
      if (name_max != strlen (toolong_subdir))
	FAIL_UNSUPPORTED ("name_max: Temporary directories in different"
			  " filesystems not supported yet\n");
      if (path_max != toolong_path_max)
	FAIL_UNSUPPORTED ("path_max: Temporary directories in different"
			  " filesystems not supported yet\n");
      return;
    }

  toolong_path_max = path_max;

  size_t len = name_max;
  memset (toolong_subdir, 'X', len);
  toolong_initialized = true;
}

char *
support_create_and_chdir_toolong_temp_directory (const char *basename)
{
  char *base = create_temp_directory_internal (basename, true);
  xchdir (base);

  initialize_toolong (base);

  size_t sz = strlen (toolong_subdir);

  /* Create directories and descend into them so that the final path is larger
     than PATH_MAX.  */
  for (size_t i = 0; i <= toolong_path_max / sz; i++)
    {
      int ret = mkdir (toolong_subdir, S_IRWXU);
      if (ret != 0 && errno == ENAMETOOLONG)
	FAIL_UNSUPPORTED ("Filesystem does not support creating too long "
			  "directory trees\n");
      else if (ret != 0)
	FAIL_EXIT1 ("Failed to create directory tree: %m\n");
      xchdir (toolong_subdir);
    }
  return base;
}

void
support_chdir_toolong_temp_directory (const char *base)
{
  ensure_toolong_initialized ();

  xchdir (base);

  size_t sz = strlen (toolong_subdir);
  for (size_t i = 0; i <= toolong_path_max / sz; i++)
    xchdir (toolong_subdir);
}

/* Helper functions called by the test skeleton follow.  */

static void
remove_toolong_subdirs (const char *base)
{
  ensure_toolong_initialized ();

  if (chdir (base) != 0)
    {
      printf ("warning: toolong cleanup base failed: chdir (\"%s\"): %m\n",
	      base);
      return;
    }

  /* Descend.  */
  int levels = 0;
  size_t sz = strlen (toolong_subdir);
  for (levels = 0; levels <= toolong_path_max / sz; levels++)
    if (chdir (toolong_subdir) != 0)
      {
	printf ("warning: toolong cleanup failed: chdir (\"%s\"): %m\n",
		toolong_subdir);
	break;
      }

  /* Ascend and remove.  */
  while (--levels >= 0)
    {
      if (chdir ("..") != 0)
	{
	  printf ("warning: toolong cleanup failed: chdir (\"..\"): %m\n");
	  return;
	}
      if (remove (toolong_subdir) != 0)
	{
	  printf ("warning: could not remove subdirectory: %s: %m\n",
		  toolong_subdir);
	  return;
	}
    }
}

void
support_delete_temp_files (void)
{
  pid_t pid = getpid ();
  while (temp_name_list != NULL)
    {
      /* Only perform the removal if the path was registered in the same
	 process, as identified by the PID.  (This assumes that the
	 parent process which registered the temporary file sticks
	 around, to prevent PID reuse.)  */
      if (temp_name_list->owner == pid)
	{
	  if (temp_name_list->toolong)
	    remove_toolong_subdirs (temp_name_list->name);

	  if (remove (temp_name_list->name) != 0)
	    printf ("warning: could not remove temporary file: %s: %m\n",
		    temp_name_list->name);
	}
      free (temp_name_list->name);

      struct temp_name_list *next = temp_name_list->next;
      free (temp_name_list);
      temp_name_list = next;
    }
}

void
support_print_temp_files (FILE *f)
{
  if (temp_name_list != NULL)
    {
      struct temp_name_list *n;
      fprintf (f, "temp_files=(\n");
      for (n = temp_name_list; n != NULL; n = n->next)
        fprintf (f, "  '%s'\n", n->name);
      fprintf (f, ")\n");
    }
}

void
support_set_test_dir (const char *path)
{
  test_dir = path;
}
