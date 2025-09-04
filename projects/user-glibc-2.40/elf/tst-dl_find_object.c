/* Basic tests for _dl_find_object.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <dl-find_object.h>
#include <dlfcn.h>
#include <gnu/lib-names.h>
#include <ldsodefs.h>
#include <link.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xdlfcn.h>

/* Use data objects for testing, so that it is not necessary to decode
   function descriptors on architectures that have them.  */
static char main_program_data;

/* Computes the expected _dl_find_object result directly from the
   map.  */
static void
from_map (struct link_map *l, struct dl_find_object *expected)
{
  struct dl_find_object_internal internal;
  _dl_find_object_from_map (l, &internal);
  _dl_find_object_to_external (&internal, expected);
}

/* Compare _dl_find_object result at ADDRESS with *EXPECTED.  */
static void
check (void *address,
       struct dl_find_object *expected, int line)
{
  struct dl_find_object actual;
  int ret = _dl_find_object (address, &actual);
  if (expected == NULL)
    {
      if (ret != -1)
        {
          support_record_failure ();
          printf ("%s:%d: unexpected success for %p\n",
                  __FILE__, line, address);
        }
      return;
    }
  if (ret != 0)
    {
      support_record_failure ();
      printf ("%s:%d: unexpected failure for %p\n",
              __FILE__, line, address);
      return;
    }

  if (actual.dlfo_flags != expected->dlfo_flags)
    {
      support_record_failure ();
      printf ("%s:%d: error: %p: flags is %llu, expected %llu\n",
              __FILE__, line, address,
              actual.dlfo_flags, expected->dlfo_flags);
    }
  if (expected->dlfo_link_map->l_contiguous)
    {
      /* If the mappings are not contiguous, the actual and execpted
         mappings may differ, so this subtest will not work.  */
      if (actual.dlfo_flags != expected->dlfo_flags)
        {
          support_record_failure ();
          printf ("%s:%d: error: %p: map start is %p, expected %p\n",
                  __FILE__, line,
                  address, actual.dlfo_map_start, expected->dlfo_map_start);
        }
      if (actual.dlfo_map_end != expected->dlfo_map_end)
        {
          support_record_failure ();
          printf ("%s:%d: error: %p: map end is %p, expected %p\n",
                  __FILE__, line,
                  address, actual.dlfo_map_end, expected->dlfo_map_end);
        }
    }
  if (actual.dlfo_link_map != expected->dlfo_link_map)
    {
      support_record_failure ();
      printf ("%s:%d: error: %p: link map is %p, expected %p\n",
              __FILE__, line,
              address, actual.dlfo_link_map, expected->dlfo_link_map);
    }
  if (actual.dlfo_eh_frame != expected->dlfo_eh_frame)
    {
      support_record_failure ();
      printf ("%s:%d: error: %p: EH frame is %p, expected %p\n",
              __FILE__, line,
              address, actual.dlfo_eh_frame, expected->dlfo_eh_frame);
    }
#if DLFO_STRUCT_HAS_EH_DBASE
  if (actual.dlfo_eh_dbase != expected->dlfo_eh_dbase)
    {
      support_record_failure ();
      printf ("%s:%d: error: %p: data base is %p, expected %p\n",
              __FILE__, line,
              address, actual.dlfo_eh_dbase, expected->dlfo_eh_dbase);
    }
#endif
#if DLFO_STRUCT_HAS_EH_COUNT
  if (actual.dlfo_eh_count != expected->dlfo_eh_count)
    {
      support_record_failure ();
      printf ("%s:%d: error: %p: count is %d, expected %d\n",
              __FILE__, line,
              address, actual.dlfo_eh_count, expected->dlfo_eh_count);
    }
#endif
}

/* Check that unwind data for the main executable and the dynamic
   linker can be found.  */
static void
check_initial (void)
{
#ifndef FOR_STATIC
  /* Avoid direct reference, which could lead to copy relocations.  */
  struct r_debug *debug = xdlsym (NULL, "_r_debug");
  TEST_VERIFY_EXIT (debug != NULL);
  char **tzname = xdlsym (NULL, "tzname");

  /* The main executable has an unnamed link map.  */
  struct link_map *main_map = (struct link_map *) debug->r_map;
  TEST_COMPARE_STRING (main_map->l_name, "");

  /* The link map of the dynamic linker.  */
  struct link_map *rtld_map = xdlopen (LD_SO, RTLD_LAZY | RTLD_NOLOAD);
  TEST_VERIFY_EXIT (rtld_map != NULL);

  /* The link map of libc.so.  */
  struct link_map *libc_map = xdlopen (LIBC_SO, RTLD_LAZY | RTLD_NOLOAD);
  TEST_VERIFY_EXIT (libc_map != NULL);

  struct dl_find_object expected;

  /* Data in the main program.  */
  from_map (main_map, &expected);
  check (&main_program_data, &expected, __LINE__);
  /* Corner cases for the mapping.  */
  check ((void *) main_map->l_map_start, &expected, __LINE__);
  check ((void *) (main_map->l_map_end - 1), &expected, __LINE__);

  /* Data in the dynamic loader.  */
  from_map (rtld_map, &expected);
  check (debug, &expected, __LINE__);
  check ((void *) rtld_map->l_map_start, &expected, __LINE__);
  check ((void *) (rtld_map->l_map_end - 1), &expected, __LINE__);

  /* Data in libc.  */
  from_map (libc_map, &expected);
  check (tzname, &expected, __LINE__);
  check ((void *) libc_map->l_map_start, &expected, __LINE__);
  check ((void *) (libc_map->l_map_end - 1), &expected, __LINE__);
#endif
}

static int
do_test (void)
{
  {
    struct dl_find_object dlfo = { };
    int ret = _dl_find_object (&main_program_data, &dlfo);
    printf ("info: main program unwind data: %p (%d)\n",
            dlfo.dlfo_eh_frame, ret);
    TEST_COMPARE (ret, 0);
    TEST_VERIFY (dlfo.dlfo_eh_frame != NULL);
  }

  check_initial ();

  /* dlopen-based test.  First an object that can be dlclosed.  */
  struct link_map *mod1 = xdlopen ("tst-dl_find_object-mod1.so", RTLD_NOW);
  void *mod1_data = xdlsym (mod1, "mod1_data");
  void *map_start = (void *) mod1->l_map_start;
  void *map_end = (void *) (mod1->l_map_end - 1);
  check_initial ();

  struct dl_find_object expected;
  from_map (mod1, &expected);
  check (mod1_data, &expected, __LINE__);
  check (map_start, &expected, __LINE__);
  check (map_end, &expected, __LINE__);

  /* Unloading must make the data unavailable.  */
  xdlclose (mod1);
  check_initial ();
  check (mod1_data, NULL, __LINE__);
  check (map_start, NULL, __LINE__);
  check (map_end, NULL, __LINE__);

  /* Now try a NODELETE load.  */
  struct link_map *mod2 = xdlopen ("tst-dl_find_object-mod2.so", RTLD_NOW);
  void *mod2_data = xdlsym (mod2, "mod2_data");
  map_start = (void *) mod2->l_map_start;
  map_end = (void *) (mod2->l_map_end - 1);
  check_initial ();
  from_map (mod2, &expected);
  check (mod2_data, &expected, __LINE__);
  check (map_start, &expected, __LINE__);
  check (map_end, &expected, __LINE__);
  dlclose (mod2);               /* Does nothing due to NODELETE.  */
  check_initial ();
  check (mod2_data, &expected, __LINE__);
  check (map_start, &expected, __LINE__);
  check (map_end, &expected, __LINE__);

  /* Now load again the first module.  */
  mod1 = xdlopen ("tst-dl_find_object-mod1.so", RTLD_NOW);
  mod1_data = xdlsym (mod1, "mod1_data");
  map_start = (void *) mod1->l_map_start;
  map_end = (void *) (mod1->l_map_end - 1);
  check_initial ();
  from_map (mod1, &expected);
  check (mod1_data, &expected, __LINE__);
  check (map_start, &expected, __LINE__);
  check (map_end, &expected, __LINE__);

  /* Check that _dl_find_object works from a shared object (mostly for
     static dlopen).  */
  __typeof (_dl_find_object) *find_object
    = *(void **) xdlsym (mod2, "find_object");
  struct dl_find_object actual;
  TEST_COMPARE (find_object (&main_program_data, &actual), 0);
  check (&main_program_data, &actual, __LINE__); /* Reversed check.  */

  return 0;
}

#include <support/test-driver.c>
