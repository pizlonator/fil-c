/* AArch64 GCS functions.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.

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

#include <unistd.h>
#include <ldsodefs.h>

/* GCS is disabled.  */
#define GCS_POLICY_DISABLED 0

/* Enable GCS, abort if unmarked binary is found.  */
#define GCS_POLICY_ENFORCED 1

/* Optionally enable GCS if all startup dependencies are marked.  */
#define GCS_POLICY_OPTIONAL 2

/* Override binary marking and always enabled GCS.  */
#define GCS_POLICY_OVERRIDE 3

static void
fail (struct link_map *l, const char *program)
{
    if (program != NULL)
    {
      if (program[0] != '\0' && l->l_name[0] != '\0')
	/* A program's dependency is not GCS compatible.  */
	_dl_fatal_printf ("%s: %s: not GCS compatible\n", program, l->l_name);
      if (program[0] != '\0')
	/* The program itself is not GCS compatible.  */
	_dl_fatal_printf ("%s: not GCS compatible\n", program);
      /* For static binaries, program will be an empty string.  */
      _dl_fatal_printf ("error: not GCS compatible\n");
    }
  else
    /* If program is NULL, we are processing a dlopen operation.
       Note: the errno value is not available any more.  */
    _dl_signal_error (0, l->l_name, "dlopen", "not GCS compatible");
}

static void
warn (struct link_map *l, const char *program)
{
  if (l->l_name[0] != '\0')
    _dl_debug_printf ("security: not compatible with AArch64 GCS: %s\n",
		      l->l_name);
  else if (__glibc_likely (program != NULL))
    _dl_debug_printf ("security: not compatible with AArch64 GCS: %s\n",
		      program);
}

static void
unsupported (void)
{
  _dl_fatal_printf ("unsupported GCS policy\n");
}

/* This function is called only when binary markings are not
   ignored and GCS is supposed to be enabled.  This occurs
   for the GCS_POLICY_ENFORCED and GCS_POLICY_OPTIONAL policies.  */
static bool
check_gcs (struct link_map *l, const char *program, bool enforced,
	   int dlopen_mode)
{
#ifdef SHARED
  /* Ignore GCS marking on ld.so: its properties are not processed.  */
  if (is_rtld_link_map (l->l_real))
    return true;
#endif
  /* Binary is marked, all good.  */
  if (l->l_mach.gcs)
    return true;
  /* Extra logging requested, print path to failed binary.  */
  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_SECURITY))
    warn (l, program);
  /* Binary is not marked and loaded via dlopen: abort.  Also, do not
     fail is optional dlopen_mode is being used with audit modules without
     GCS support.  */
  if (program == NULL && (dlopen_mode & __RTLD_AUDIT) == 0)
    fail (l, program);
  /* Binary is not marked and we enforce GCS: abort.  */
  if (enforced)
    fail (l, program);
  /* Binary is not marked but GSC is optional: disable GCS.  */
  else
    {
      GL(dl_aarch64_gcs) = 0;
      return false;
    }
  __builtin_unreachable ();
}

/* Iterate over all dependencies and check GCS marking.
   This function is called only when binary markings are not
   ignored and GCS is supposed to be enabled.  This occurs
   for the GCS_POLICY_ENFORCED and GCS_POLICY_ENFORCED policies.
   We interrupt checking if GCS is optional and we already know
   it is going to be disabled. */
static void
check_gcs_depends (struct link_map *l, const char *program, bool enforced,
		   int dlopen_mode)
{
  if (check_gcs (l, program, enforced, dlopen_mode))
    for (unsigned int i = 0; i < l->l_searchlist.r_nlist; i++)
      if (!check_gcs (l->l_searchlist.r_list[i], program, enforced,
		      dlopen_mode))
	break;
}

/* Apply GCS policy for L and its dependencies.
   PROGRAM is NULL when this check is invoked for dl_open.  */
void
_dl_gcs_check (struct link_map *l, const char *program, int dlopen_mode)
{
  unsigned long policy = GL (dl_aarch64_gcs);
  switch (policy)
    {
    case GCS_POLICY_DISABLED:
    case GCS_POLICY_OVERRIDE:
      return;
    case GCS_POLICY_ENFORCED:
      check_gcs_depends (l, program, true, dlopen_mode);
      return;
    case GCS_POLICY_OPTIONAL:
      check_gcs_depends (l, program, false, dlopen_mode);
      return;
    default:
      /* All other policy values are not supported: abort.  */
      unsupported ();
    }
}

/* Used to report error when prctl system call to enabled GCS fails.  */
void _dl_gcs_enable_failed (int code)
{
  _dl_fatal_printf ("failed to enable GCS: %d\n", -code);
}

/* Used to report error when prctl system call to lock GCS fails.  */
void _dl_gcs_lock_failed (int code)
{
  _dl_fatal_printf ("failed to lock GCS: %d\n", -code);
}
