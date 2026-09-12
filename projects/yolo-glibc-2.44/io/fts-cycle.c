/* Detect cycles in file tree walks.

   Copyright (C) 2003-2006, 2009-2026 Free Software Foundation, Inc.

   Written by Jim Meyering.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include "cycle-check.h"
#include "hash.h"

#ifndef FTS_OPEN
# define FTSOBJ FTS
# define FTSENTRY FTSENT
#endif

/* Use each of these to map a device/inode pair to an FTSENT.  */
struct Active_dir
{
  dev_t dev;
  INO_T ino;
  FTSENTRY *fts_ent;
};

static bool
AD_compare (void const *x, void const *y)
{
  struct Active_dir const *ax = x;
  struct Active_dir const *ay = y;
  return ax->ino == ay->ino
      && ax->dev == ay->dev;
}

static size_t
AD_hash (void const *x, size_t table_size)
{
  struct Active_dir const *ax = x;
  return (uintmax_t) ax->ino % table_size;
}

/* Set up the cycle-detection machinery.  */

static bool
setup_dir (FTSOBJ *fts)
{
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      enum { HT_INITIAL_SIZE = 31 };
      fts->fts_cycle.ht = hash_initialize (HT_INITIAL_SIZE, NULL, AD_hash,
                                           AD_compare, free);
      if (! fts->fts_cycle.ht)
        return false;
    }
  else
    {
      fts->fts_cycle.state = malloc (sizeof *fts->fts_cycle.state);
      if (! fts->fts_cycle.state)
        return false;
      cycle_check_init (fts->fts_cycle.state);
    }

  return true;
}

/* Enter a directory during a file tree walk.  */

static bool
enter_dir (FTSOBJ *fts, FTSENTRY *ent)
{
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      struct Active_dir *ad = malloc (sizeof *ad);
      if (!ad)
        return false;

      struct STRUCT_STAT const *st = ent->fts_statp;
      ad->dev = st->st_dev;
      ad->ino = st->st_ino;
      ad->fts_ent = ent;

      /* See if we've already encountered this directory.
         This can happen when following symlinks as well as
         with a corrupted directory hierarchy. */
      struct Active_dir *ad_from_table = hash_insert (fts->fts_cycle.ht, ad);

      if (ad_from_table != ad)
        {
          free (ad);
          if (!ad_from_table)
            return false;

          /* There was an entry with matching dev/inode already in the table.
             Record the fact that we've found a cycle.  */
          ent->fts_cycle = ad_from_table->fts_ent;
          ent->fts_info = FTS_DC;
        }
    }
  else
    {
      if (cycle_check (fts->fts_cycle.state, ent->fts_statp))
        {
          /* FIXME: setting fts_cycle like this isn't proper.
             To do what the documentation requires, we'd have to
             go around the cycle again and find the right entry.
             But no callers in coreutils use the fts_cycle member. */
          ent->fts_cycle = ent;
          ent->fts_info = FTS_DC;
        }
    }

  return true;
}

/* Leave a directory during a file tree walk.  */

static void
leave_dir (FTSOBJ *fts, FTSENTRY *ent)
{
  struct STRUCT_STAT const *st = ent->fts_statp;
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      struct Active_dir obj;
      obj.dev = st->st_dev;
      obj.ino = st->st_ino;
      void *found = hash_remove (fts->fts_cycle.ht, &obj);
      if (!found)
        abort ();
      free (found);
    }
  else
    {
      FTSENTRY *parent = ent->fts_parent;
      if (parent != NULL && 0 <= parent->fts_level)
        CYCLE_CHECK_REFLECT_CHDIR_UP (fts->fts_cycle.state,
                                      *(parent->fts_statp), *st);
    }
}

/* Free any memory used for cycle detection.  */

static void
free_dir (FTSOBJ *sp)
{
  if (sp->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      if (sp->fts_cycle.ht)
        hash_free (sp->fts_cycle.ht);
    }
  else
    free (sp->fts_cycle.state);
}
