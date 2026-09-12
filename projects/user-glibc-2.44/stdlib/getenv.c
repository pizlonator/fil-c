/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <atomic.h>
#include <setenv.h>
#include <string.h>
#include <unistd.h>

char *
getenv (const char *name)
{
  while (true)
    {
      /* Used to deal with concurrent unsetenv.  */
      environ_counter start_counter = atomic_load_acquire (&__environ_counter);

      /* We use relaxed MO for loading the string pointers because we
	 assume the strings themselves are immutable and that loads
	 through the string pointers carry a dependency.  (This
	 depends on the the release MO store to __environ in
	 __add_to_environ.)  Objects pointed to by pointers stored in
	 the __environ array are never modified or deallocated (except
	 perhaps if putenv is used, but then synchronization is the
	 responsibility of the applications).  The backing store for
	 __environ is allocated zeroed.  In summary, we can assume
	 that the pointers we observe are either valid or null, and
	 that only initialized string contents is visible.  */
      char **start_environ = atomic_load_relaxed (&__environ);
      if (start_environ == NULL || name[0] == '\0')
	return NULL;

      size_t len = strlen (name);
      for (char **ep = start_environ; ; ++ep)
	{
	  char *entry = atomic_load_relaxed (ep);
	  if (entry == NULL)
	    break;

	  /* If there is a match, return that value.  It was valid at
	     one point, so we can return it.  */
	  if (name[0] == entry[0]
	      && strncmp (name, entry, len) == 0 && entry[len] == '=')
	    return entry + len + 1;
	}

      /* The variable was not found.  This might be a false negative
	 because unsetenv has shuffled around entries.  Check if it is
	 necessary to retry.  */

      /* See Hans Boehm, Can Seqlocks Get Along with Programming Language
	 Memory Models?, Section 4.  This is necessary so that loads in
	 the loop above are not ordered past the counter check below.  */
      atomic_thread_fence_acquire ();

      if (atomic_load_acquire (&__environ_counter) == start_counter)
	  /* If we reach this point and there was a concurrent
	     unsetenv call which removed the key we tried to find, the
	     NULL return value is valid.  We can also try again, not
	     find the value, and then return NULL (assuming there are
	     no further concurrent unsetenv calls).

	     However, if getenv is called to find a value that is
	     present originally and not removed by any of the
	     concurrent unsetenv calls, we must not return NULL here.

	     If the counter did not change, there was at most one
	     write to the array in unsetenv while the scanning loop
	     above was running.  This means that there are at most two
	     different versions of the array to consider.  For the
	     sake of argument, we assume that each load can make an
	     independent choice which version to use.  An arbitrary
	     number of unsetenv and setenv calls may have happened
	     since start of getenv.  Lets write E[0], E[1], ... for
	     the original environment elements, a(0) < (1) < ... for a
	     sequence of increasing integers that are the indices of
	     the environment variables remaining after the removals, and
	     N[0], N[1], ... for the new variables added by setenv or
	     putenv.  Then at the start of the last unsetenv call, the
	     environment contains

	       E[a(0)], E[a(1)], ..., N[0], N[1], ...

	     (the N[0], N[1], .... are optional.)  Let's assume that
	     we are looking for the value E[j].  Then one of the
	     a(i) == j (otherwise we may return NULL here because
	     of a unsetenv for the value we are looking for).  In the
	     discussion below it will become clear that the N[k] do
	     not actually matter.

	     The two versions of array we can choose from differ only
	     in one element, say E[a(i)].  There are two cases:

	     Case (A): E[a(i)] is an element being removed by unsetenv
	     (the target of the first write).  We can see the original
	     version:

	     ..., E[a(i-1)], E[a(i)],   E[a(i+1)], ..., N[0], ...
                             -------
	     And the overwritten version:

	     ..., E[a(i-1)], E[a(i+1)], E[a(i+1)], ..., N[0], ...
                             ---------

             (The valueE[a(i+1)] can be the terminating NULL.)
	     As discussed, we are not considering the removal of the
	     variable being searched for, so a(i) != j, and the
	     variable getenv is looking for is available in either
	     version, and we would have found it above.

	     Case (B): E[a(i)] is an element that has already been
	     moved forward and is now itself being overwritten with
	     its sucessor value E[a(i+1)].  The two versions of the
	     array look like this:

	     ..., E[a(i-1)], E[a(i)], E[a(i)],   E[a(i+1)], ..., N[0], ...
                                      -------
	     And with the overwrite in place:

	     ..., E[a(i-1)], E[a(i)], E[a(i+1)], E[a(i+1)], ..., N[0], ...
                                      ---------

             The key observation here is that even in the second
             version with the overwrite present, the scanning loop
             will still encounter the overwritten value E[a(i)] in the
             previous array element.  This means that as long as the
             E[j] is still present among the initial E[a(...)] (as we
             assumed because there is no concurrent unsetenv for
             E[j]), we encounter it while scanning here in getenv.

	     In summary, if there was at most one write, a negative
	     result is a true negative, and we can return NULL.  This
	     is different from the seqlock paper, which retries if
	     there was any write at all.  It avoids the need for a
	     second, unwritten copy for async-signal-safety.  */
	return NULL;
      /* If there was one more write, retry.  This will never happen
	 in a signal handler that interrupted unsetenv because the
	 suspended unsetenv call cannot change the counter value.  */
    }
}
libc_hidden_def (getenv)
