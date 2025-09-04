/* Sort array of link maps according to dependencies.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <ldsodefs.h>
#include <elf/dl-tunables.h>

/* Note: this is the older, "original" sorting algorithm, being used as
   default up to 2.35.

   Sort array MAPS according to dependencies of the contained objects.
   If FOR_FINI is true, this is called for finishing an object.  */
static void
_dl_sort_maps_original (struct link_map **maps, unsigned int nmaps,
			bool force_first, bool for_fini)
{
  /* Allows caller to do the common optimization of skipping the first map,
     usually the main binary.  */
  maps += force_first;
  nmaps -= force_first;

  /* A list of one element need not be sorted.  */
  if (nmaps <= 1)
    return;

  unsigned int i = 0;
  uint16_t seen[nmaps];
  memset (seen, 0, nmaps * sizeof (seen[0]));
  while (1)
    {
      /* Keep track of which object we looked at this round.  */
      ++seen[i];
      struct link_map *thisp = maps[i];

      if (__glibc_unlikely (for_fini))
	{
	  /* Do not handle ld.so in secondary namespaces and objects which
	     are not removed.  */
	  if (thisp != thisp->l_real || thisp->l_idx == -1)
	    goto skip;
	}

      /* Find the last object in the list for which the current one is
	 a dependency and move the current object behind the object
	 with the dependency.  */
      unsigned int k = nmaps - 1;
      while (k > i)
	{
	  struct link_map **runp = maps[k]->l_initfini;
	  if (runp != NULL)
	    /* Look through the dependencies of the object.  */
	    while (*runp != NULL)
	      if (__glibc_unlikely (*runp++ == thisp))
		{
		move:
		  /* Move the current object to the back past the last
		     object with it as the dependency.  */
		  memmove (&maps[i], &maps[i + 1],
			   (k - i) * sizeof (maps[0]));
		  maps[k] = thisp;

		  if (seen[i + 1] > nmaps - i)
		    {
		      ++i;
		      goto next_clear;
		    }

		  uint16_t this_seen = seen[i];
		  memmove (&seen[i], &seen[i + 1], (k - i) * sizeof (seen[0]));
		  seen[k] = this_seen;

		  goto next;
		}

	  if (__glibc_unlikely (for_fini && maps[k]->l_reldeps != NULL))
	    {
	      unsigned int m = maps[k]->l_reldeps->act;
	      struct link_map **relmaps = &maps[k]->l_reldeps->list[0];

	      /* Look through the relocation dependencies of the object.  */
	      while (m-- > 0)
		if (__glibc_unlikely (relmaps[m] == thisp))
		  {
		    /* If a cycle exists with a link time dependency,
		       preserve the latter.  */
		    struct link_map **runp = thisp->l_initfini;
		    if (runp != NULL)
		      while (*runp != NULL)
			if (__glibc_unlikely (*runp++ == maps[k]))
			  goto ignore;
		    goto move;
		  }
	    ignore:;
	    }

	  --k;
	}

    skip:
      if (++i == nmaps)
	break;
    next_clear:
      memset (&seen[i], 0, (nmaps - i) * sizeof (seen[0]));

    next:;
    }
}

/* We use a recursive function due to its better clarity and ease of
   implementation, as well as faster execution speed. We already use
   alloca() for list allocation during the breadth-first search of
   dependencies in _dl_map_object_deps(), and this should be on the
   same order of worst-case stack usage.

   Note: the '*rpo' parameter is supposed to point to one past the
   last element of the array where we save the sort results, and is
   decremented before storing the current map at each level.  */

static void
dfs_traversal (struct link_map ***rpo, struct link_map *map,
	       bool *do_reldeps)
{
  /* _dl_map_object_deps ignores l_faked objects when calculating the
     number of maps before calling _dl_sort_maps, ignore them as well.  */
  if (map->l_visited || map->l_faked)
    return;

  map->l_visited = 1;

  if (map->l_initfini)
    {
      for (int i = 0; map->l_initfini[i] != NULL; i++)
	{
	  struct link_map *dep = map->l_initfini[i];
	  if (dep->l_visited == 0
	      && dep->l_main_map == 0)
	    dfs_traversal (rpo, dep, do_reldeps);
	}
    }

  if (__glibc_unlikely (do_reldeps != NULL && map->l_reldeps != NULL))
    {
      /* Indicate that we encountered relocation dependencies during
	 traversal.  */
      *do_reldeps = true;

      for (int m = map->l_reldeps->act - 1; m >= 0; m--)
	{
	  struct link_map *dep = map->l_reldeps->list[m];
	  if (dep->l_visited == 0
	      && dep->l_main_map == 0)
	    dfs_traversal (rpo, dep, do_reldeps);
	}
    }

  *rpo -= 1;
  **rpo = map;
}

/* Topologically sort array MAPS according to dependencies of the contained
   objects.  */

static void
_dl_sort_maps_dfs (struct link_map **maps, unsigned int nmaps,
		   bool force_first, bool for_fini)
{
  struct link_map *first_map = maps[0];
  for (int i = nmaps - 1; i >= 0; i--)
    maps[i]->l_visited = 0;

  /* We apply DFS traversal for each of maps[i] until the whole total order
     is found and we're at the start of the Reverse-Postorder (RPO) sequence,
     which is a topological sort.

     We go from maps[nmaps - 1] backwards towards maps[0] at this level.
     Due to the breadth-first search (BFS) ordering we receive, going
     backwards usually gives a more shallow depth-first recursion depth,
     adding more stack usage safety. Also, combined with the natural
     processing order of l_initfini[] at each node during DFS, this maintains
     an ordering closer to the original link ordering in the sorting results
     under most simpler cases.

     Another reason we order the top level backwards, it that maps[0] is
     usually exactly the main object of which we're in the midst of
     _dl_map_object_deps() processing, and maps[0]->l_initfini[] is still
     blank. If we start the traversal from maps[0], since having no
     dependencies yet filled in, maps[0] will always be immediately
     incorrectly placed at the last place in the order (first in reverse).
     Adjusting the order so that maps[0] is last traversed naturally avoids
     this problem.

     To summarize, just passing in the full list, and iterating from back
     to front makes things much more straightforward.  */

  /* Array to hold RPO sorting results, before we copy back to maps[].  */
  struct link_map *rpo[nmaps];

  /* The 'head' position during each DFS iteration. Note that we start at
     one past the last element due to first-decrement-then-store (see the
     bottom of above dfs_traversal() routine).  */
  struct link_map **rpo_head = &rpo[nmaps];

  bool do_reldeps = false;
  bool *do_reldeps_ref = (for_fini ? &do_reldeps : NULL);

  for (int i = nmaps - 1; i >= 0; i--)
    {
      dfs_traversal (&rpo_head, maps[i], do_reldeps_ref);

      /* We can break early if all objects are already placed.  */
      if (rpo_head == rpo)
	goto end;
    }
  assert (rpo_head == rpo);

 end:
  /* Here we may do a second pass of sorting, using only l_initfini[]
     static dependency links. This is avoided if !FOR_FINI or if we didn't
     find any reldeps in the first DFS traversal.

     The reason we do this is: while it is unspecified how circular
     dependencies should be handled, the presumed reasonable behavior is to
     have destructors to respect static dependency links as much as possible,
     overriding reldeps if needed. And the first sorting pass, which takes
     l_initfini/l_reldeps links equally, may not preserve this priority.

     Hence we do a 2nd sorting pass, taking only DT_NEEDED links into account
     (see how the do_reldeps argument to dfs_traversal() is NULL below).  */
  if (do_reldeps)
    {
      for (int i = nmaps - 1; i >= 0; i--)
	rpo[i]->l_visited = 0;

      struct link_map **maps_head = &maps[nmaps];
      for (int i = nmaps - 1; i >= 0; i--)
	{
	  dfs_traversal (&maps_head, rpo[i], NULL);

	  /* We can break early if all objects are already placed.
	     The below memcpy is not needed in the do_reldeps case here,
	     since we wrote back to maps[] during DFS traversal.  */
	  if (maps_head == maps)
	    break;
	}
      assert (maps_head == maps);
    }
  else
    memcpy (maps, rpo, sizeof (struct link_map *) * nmaps);

  /* Skipping the first object at maps[0] is not valid in general,
     since traversing along object dependency-links may "find" that
     first object even when it is not included in the initial order
     (e.g., a dlopen'ed shared object can have circular dependencies
     linked back to itself).  In such a case, traversing N-1 objects
     will create a N-object result, and raise problems.  Instead,
     force the object back into first place after sorting.  This naive
     approach may introduce further dependency ordering violations
     compared to rotating the cycle until the first map is again in
     the first position, but as there is a cycle, at least one
     violation is already present.  */
  if (force_first && maps[0] != first_map)
    {
      int i;
      for (i = 0; maps[i] != first_map; ++i)
	;
      assert (i < nmaps);
      memmove (&maps[1], maps, i * sizeof (maps[0]));
      maps[0] = first_map;
    }
}

void
_dl_sort_maps_init (void)
{
  int32_t algorithm = TUNABLE_GET (glibc, rtld, dynamic_sort, int32_t, NULL);
  GLRO(dl_dso_sort_algo) = algorithm == 1 ? dso_sort_algorithm_original
					  : dso_sort_algorithm_dfs;
}

void
_dl_sort_maps (struct link_map **maps, unsigned int nmaps,
	       bool force_first, bool for_fini)
{
  /* It can be tempting to use a static function pointer to store and call
     the current selected sorting algorithm routine, but experimentation
     shows that current processors still do not handle indirect branches
     that efficiently, plus a static function pointer will involve
     PTR_MANGLE/DEMANGLE, further impairing performance of small, common
     input cases. A simple if-case with direct function calls appears to
     be the fastest.  */
  if (__glibc_likely (GLRO(dl_dso_sort_algo) == dso_sort_algorithm_original))
    _dl_sort_maps_original (maps, nmaps, force_first, for_fini);
  else
    _dl_sort_maps_dfs (maps, nmaps, force_first, for_fini);
}
