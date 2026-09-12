/* Wait for process state.
   Copyright (C) 2020-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <array_length.h>
#include <intprops.h>

#include <support/process_state.h>
#include <support/xstdio.h>
#include <support/check.h>

static enum support_process_state
support_process_state_wait_common (FILE *fstatus,
				   enum support_process_state state)
{
#ifdef __linux__
  /* For Linux it does a polling check on /proc/<pid>/status checking on
     third field.  */

  /* It mimics the kernel states from fs/proc/array.c  */
  static const struct process_states
  {
    enum support_process_state s;
    char v;
  } process_states[] = {
    { support_process_state_running,      'R' },
    { support_process_state_sleeping,     'S' },
    { support_process_state_disk_sleep,   'D' },
    { support_process_state_stopped,      'T' },
    { support_process_state_tracing_stop, 't' },
    { support_process_state_dead,         'X' },
    { support_process_state_zombie,       'Z' },
    { support_process_state_parked,       'P' },
  };

  char *line = NULL;
  size_t linesiz = 0;

  for (;;)
    {
      char cur_state = CHAR_MAX;
      ssize_t r;
      while ((r = getline (&line, &linesiz, fstatus)) > 0)
	if (strncmp (line, "State:", strlen ("State:")) == 0)
	  {
	    TEST_COMPARE (sscanf (line, "%*s %c", &cur_state), 1);
	    break;
	  }
      /* The procfs file for the /proc/self/task/tid might be removed by the
	 kernel if the thread exits before the getline call.  In this case
	 returns that the thread is dead.  */
      if (r == -1 && errno == ESRCH)
	{
	  free (line);
	  fclose (fstatus);
	  return support_process_state_dead;
	}
      TEST_VERIFY (ferror (fstatus) == 0);
      /* Fallback to nanosleep for invalid state.  */
      if (cur_state == CHAR_MAX)
	break;

      for (size_t i = 0; i < array_length (process_states); ++i)
	if (state & process_states[i].s && cur_state == process_states[i].v)
	  {
	    free (line);
	    fclose (fstatus);
	    return process_states[i].s;
	  }

      rewind (fstatus);
      fflush (fstatus);

      if (nanosleep (&(struct timespec) { 0, 10000000 }, NULL) != 0)
	FAIL_EXIT1 ("nanosleep: %m");
    }

  free (line);
  fclose (fstatus);
  /* Fallback to nanosleep if an invalid state is found.  */
#endif
  nanosleep (&(struct timespec) { 1, 0 }, NULL);

  return support_process_state_invalid;
}

enum support_process_state
support_process_state_wait (pid_t pid, enum support_process_state state)
{
  FILE *fstatus = NULL;

#ifdef __linux__
  /* For Linux it does a polling check on /proc/<pid>/status checking on
     third field.  */

  char path[sizeof ("/proc/")
	    + INT_STRLEN_BOUND (pid_t)
	    + sizeof ("/status") + 1];
  snprintf (path, sizeof (path), "/proc/%i/status", pid);
  fstatus = xfopen (path, "r");
#endif

  return support_process_state_wait_common (fstatus, state);
}

enum support_process_state
support_thread_state_wait (pid_t tid, enum support_process_state state)
{
  FILE *fstatus = NULL;

#ifdef __linux__
  /* For Linux it does a polling check on /proc/<getpid()>/task/<tid>/status
     checking on third field.  */

  char path[sizeof ("/proc/")
	    + INT_STRLEN_BOUND (pid_t) + 1 /* <getpid()>/ */
	    + sizeof ("task/")
	    + INT_STRLEN_BOUND (pid_t) + 1 /* <tid>/ */
	    + sizeof ("/status") + 1];
  snprintf (path, sizeof (path), "/proc/%i/task/%i/status", getpid (), tid);
  fstatus = fopen (path, "r");
  /* The thread might already being terminated and there is no check whether
     tid is a valid descriptior.  */
  if (fstatus == NULL)
    return support_process_state_dead;
#endif

  return support_process_state_wait_common (fstatus, state);
}
