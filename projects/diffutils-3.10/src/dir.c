/* Read, sort and compare two directories.  Used for GNU DIFF.

   Copyright (C) 1988-1989, 1992-1995, 1998, 2001-2002, 2004, 2006-2007,
   2009-2013, 2015-2023 Free Software Foundation, Inc.

   This file is part of GNU DIFF.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "diff.h"
#include <error.h>
#include <exclude.h>
#include <filenamecat.h>
#include <setjmp.h>
#include <xalloc.h>

/* Read the directory named by DIR and store into DIRDATA a sorted vector
   of filenames for its contents.  DIR->desc == -1 means this directory is
   known to be nonexistent, so set DIRDATA to an empty vector.
   Return -1 (setting errno) if error, 0 otherwise.  */

struct dirdata
{
  size_t nnames;	/* Number of names.  */
  char const **names;	/* Sorted names of files in dir, followed by 0.  */
  char *data;	/* Allocated storage for file names.  */
};

/* Whether file names in directories should be compared with
   locale-specific sorting.  */
static bool locale_specific_sorting;

/* Where to go if locale-specific sorting fails.  */
static jmp_buf failed_locale_specific_sorting;

static bool dir_loop (struct comparison const *, int);


/* Read a directory and get its vector of names.  */

static bool
dir_read (struct file_data const *dir, struct dirdata *dirdata)
{
  register struct dirent *next;
  register size_t i;

  /* Address of block containing the files that are described.  */
  char const **names;

  /* Number of files in directory.  */
  size_t nnames;

  /* Allocated and used storage for file name data.  */
  char *data;
  size_t data_alloc, data_used;

  dirdata->names = 0;
  dirdata->data = 0;
  nnames = 0;
  data = 0;

  if (dir->desc != -1)
    {
      /* Open the directory and check for errors.  */
      register DIR *reading = opendir (dir->name);
      if (!reading)
        return false;

      /* Initialize the table of filenames.  */

      data_alloc = 512;
      data_used = 0;
      dirdata->data = data = xmalloc (data_alloc);

      /* Read the directory entries, and insert the subfiles
         into the 'data' table.  */

      while ((errno = 0, (next = readdir (reading)) != 0))
        {
          char *d_name = next->d_name;
          size_t d_size = _D_EXACT_NAMLEN (next) + 1;

          /* Ignore "." and "..".  */
          if (d_name[0] == '.'
              && (d_name[1] == 0 || (d_name[1] == '.' && d_name[2] == 0)))
            continue;

          if (excluded_file_name (excluded, d_name))
            continue;

          while (data_alloc < data_used + d_size)
            {
              if (PTRDIFF_MAX / 2 <= data_alloc)
                xalloc_die ();
              dirdata->data = data = xrealloc (data, data_alloc *= 2);
            }

          memcpy (data + data_used, d_name, d_size);
          data_used += d_size;
          nnames++;
        }
      if (errno)
        {
          int e = errno;
          closedir (reading);
          errno = e;
          return false;
        }
#if CLOSEDIR_VOID
      closedir (reading);
#else
      if (closedir (reading) != 0)
        return false;
#endif
    }

  /* Create the 'names' table from the 'data' table.  */
  dirdata->names = names = xnmalloc (nnames + 1, sizeof *names);
  dirdata->nnames = nnames;
  for (i = 0;  i < nnames;  i++)
    {
      names[i] = data;
      data += strlen (data) + 1;
    }
  names[nnames] = 0;
  return true;
}

/* Compare strings in a locale-specific way, returning a value
   compatible with strcmp.  */

static int
compare_collated (char const *name1, char const *name2)
{
  int r;
  errno = 0;
  if (ignore_file_name_case)
    r = strcasecoll (name1, name2);
  else
    r = strcoll (name1, name2);
  if (errno)
    {
      error (0, errno, _("cannot compare file names '%s' and '%s'"),
             name1, name2);
      longjmp (failed_locale_specific_sorting, 1);
    }
  return r;
}

/* Compare file names, returning a value compatible with strcmp.  */

static int
compare_names (char const *name1, char const *name2)
{
  if (locale_specific_sorting)
    {
      int diff = compare_collated (name1, name2);
      if (diff || ignore_file_name_case)
        return diff;
    }
  return file_name_cmp (name1, name2);
}

/* Compare names FILE1 and FILE2 when sorting a directory.
   Prefer filtered comparison, breaking ties with file_name_cmp.  */

static int
compare_names_for_qsort (void const *file1, void const *file2)
{
  char const *const *f1 = file1;
  char const *const *f2 = file2;
  char const *name1 = *f1;
  char const *name2 = *f2;
  if (locale_specific_sorting)
    {
      int diff = compare_collated (name1, name2);
      if (diff)
        return diff;
    }
  return file_name_cmp (name1, name2);
}

/* Compare the contents of two directories named in CMP.
   This is a top-level routine; it does everything necessary for diff
   on two directories.

   CMP->file[0].desc == -1 says directory CMP->file[0] doesn't exist,
   but pretend it is empty.  Likewise for CMP->file[1].

   HANDLE_FILE is a caller-provided subroutine called to handle each file.
   It gets three operands: CMP, name of file in dir 0, name of file in dir 1.
   These names are relative to the original working directory.

   For a file that appears in only one of the dirs, one of the name-args
   to HANDLE_FILE is zero.

   Returns the maximum of all the values returned by HANDLE_FILE,
   or EXIT_TROUBLE if trouble is encountered in opening files.  */

int
diff_dirs (struct comparison const *cmp,
           int (*handle_file) (struct comparison const *,
                               char const *, char const *))
{
  struct dirdata dirdata[2];
  int volatile val = EXIT_SUCCESS;
  int i;

  if ((cmp->file[0].desc == -1 || dir_loop (cmp, 0))
      && (cmp->file[1].desc == -1 || dir_loop (cmp, 1)))
    {
      error (0, 0, _("%s: recursive directory loop"),
             cmp->file[cmp->file[0].desc == -1].name);
      return EXIT_TROUBLE;
    }

  /* Get contents of both dirs.  */
  for (i = 0; i < 2; i++)
    if (! dir_read (&cmp->file[i], &dirdata[i]))
      {
        perror_with_name (cmp->file[i].name);
        val = EXIT_TROUBLE;
      }

  if (val == EXIT_SUCCESS)
    {
      char const **volatile names[2];
      names[0] = dirdata[0].names;
      names[1] = dirdata[1].names;

      /* Use locale-specific sorting if possible, else native byte order.  */
      locale_specific_sorting = true;
      if (setjmp (failed_locale_specific_sorting))
        locale_specific_sorting = false;

      /* Sort the directories.  */
      for (i = 0; i < 2; i++)
        qsort (names[i], dirdata[i].nnames, sizeof *dirdata[i].names,
               compare_names_for_qsort);

      /* If '-S name' was given, and this is the topmost level of comparison,
         ignore all file names less than the specified starting name.  */

      if (starting_file && ! cmp->parent)
        {
          while (*names[0] && compare_names (*names[0], starting_file) < 0)
            names[0]++;
          while (*names[1] && compare_names (*names[1], starting_file) < 0)
            names[1]++;
        }

      /* Loop while files remain in one or both dirs.  */
      while (*names[0] || *names[1])
        {
          /* Compare next name in dir 0 with next name in dir 1.
             At the end of a dir,
             pretend the "next name" in that dir is very large.  */
          int nameorder = (!*names[0] ? 1 : !*names[1] ? -1
                           : compare_names (*names[0], *names[1]));

          /* Prefer a file_name_cmp match if available.  This algorithm is
             O(N**2), where N is the number of names in a directory
             that compare_names says are all equal, but in practice N
             is so small it's not worth tuning.  */
          if (nameorder == 0 && ignore_file_name_case)
            {
              int raw_order = file_name_cmp (*names[0], *names[1]);
              if (raw_order != 0)
                {
                  int greater_side = raw_order < 0;
                  int lesser_side = 1 - greater_side;
                  char const **lesser = names[lesser_side];
                  char const *greater_name = *names[greater_side];
                  char const **p;

                  for (p = lesser + 1;
                       *p && compare_names (*p, greater_name) == 0;
                       p++)
                    {
                      int c = file_name_cmp (*p, greater_name);
                      if (0 <= c)
                        {
                          if (c == 0)
                            {
                              memmove (lesser + 1, lesser,
                                       (char *) p - (char *) lesser);
                              *lesser = greater_name;
                            }
                          break;
                        }
                    }
                }
            }

          int v1 = (*handle_file) (cmp,
                                   0 < nameorder ? 0 : *names[0]++,
                                   nameorder < 0 ? 0 : *names[1]++);
          if (val < v1)
            val = v1;
        }
    }

  for (i = 0; i < 2; i++)
    {
      free (dirdata[i].names);
      free (dirdata[i].data);
    }

  return val;
}

/* Return nonzero if CMP is looping recursively in argument I.  */

static bool ATTRIBUTE_PURE
dir_loop (struct comparison const *cmp, int i)
{
  struct comparison const *p = cmp;
  while ((p = p->parent))
    if (0 < same_file (&p->file[i].stat, &cmp->file[i].stat))
      return true;
  return false;
}

/* Find a matching filename in a directory.  */

char *
find_dir_file_pathname (char const *dir, char const *file)
{
  /* IF_LINT due to GCC bug 21161.  */
  char const * IF_LINT (volatile) match = file;

  char *val;
  struct dirdata dirdata;
  dirdata.names = nullptr;
  dirdata.data = nullptr;

  if (ignore_file_name_case)
    {
      struct file_data filedata;
      filedata.name = dir;
      filedata.desc = 0;

      if (dir_read (&filedata, &dirdata))
        {
          locale_specific_sorting = true;
          if (setjmp (failed_locale_specific_sorting))
            match = file; /* longjmp may mess up MATCH.  */
          else
            {
              for (char const **p = dirdata.names; *p; p++)
                if (compare_names (*p, file) == 0)
                  {
                    if (file_name_cmp (*p, file) == 0)
                      {
                        match = *p;
                        break;
                      }
                    if (match == file)
                      match = *p;
                  }
            }
        }
    }

  val = file_name_concat (dir, match, nullptr);
  free (dirdata.names);
  free (dirdata.data);
  return val;
}
