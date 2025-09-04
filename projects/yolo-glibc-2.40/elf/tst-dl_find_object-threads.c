/* _dl_find_object test with parallelism.
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

#include <array_length.h>
#include <dlfcn.h>
#include <elf/dl-find_object.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>
#include <support/xunistd.h>

/* Computes the expected _dl_find_object result directly from the
   map.  */
static void
from_map (struct link_map *l, struct dl_find_object *expected)
{
  struct dl_find_object_internal internal;
  _dl_find_object_from_map (l, &internal);
  _dl_find_object_to_external (&internal, expected);
}

/* Returns the soname for the test object NUMBER.  */
static char *
soname (int number)
{
  return xasprintf ("tst-dl_find_object-mod%d.so", number);
}

/* Returns the data symbol name for the test object NUMBER.  */
static char *
symbol (int number)
{
  return xasprintf ("mod%d_data", number);
}

struct verify_data
{
  char *soname;
  void *address;                /* Address in the shared object.  */
  struct dl_find_object dlfo;
  pthread_t thr;
};

/* Compare _dl_find_object result at ADDRESS with *EXPECTED.  */
static void
check (void *address, struct dl_find_object *expected, int line)
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

/* Request process termination after 0.3 seconds.  */
static bool exit_requested;
static void *
exit_thread (void *ignored)
{
  usleep (300 * 1000);
  __atomic_store_n (&exit_requested, true,  __ATOMIC_RELAXED);
  return NULL;
}

static void *
verify_thread (void *closure)
{
  struct verify_data *data = closure;

  while (!__atomic_load_n (&exit_requested, __ATOMIC_RELAXED))
    {
      check (data->address, &data->dlfo, __LINE__);
      check (data->dlfo.dlfo_map_start, &data->dlfo, __LINE__);
      check (data->dlfo.dlfo_map_end - 1, &data->dlfo, __LINE__);
    }

  return NULL;
}

/* Sets up the verification data, dlopen'ing shared object NUMBER, and
   launches a verification thread.  */
static void
start_verify (int number, struct verify_data *data)
{
  data->soname = soname (number);
  struct link_map *l = xdlopen (data->soname, RTLD_NOW);
  from_map (l, &data->dlfo);
  TEST_VERIFY_EXIT (data->dlfo.dlfo_link_map == l);
  char *sym = symbol (number);
  data->address = xdlsym (data->dlfo.dlfo_link_map, sym);
  free (sym);
  data->thr = xpthread_create (NULL, verify_thread, data);
}


static int
do_test (void)
{
  struct verify_data data_mod2;
  struct verify_data data_mod4;
  struct verify_data data_mod7;

  /* Load the modules with gaps.  */
  {
    void *mod1 = xdlopen ("tst-dl_find_object-mod1.so", RTLD_NOW);
    start_verify (2, &data_mod2);
    void *mod3 = xdlopen ("tst-dl_find_object-mod3.so", RTLD_NOW);
    start_verify (4, &data_mod4);
    void *mod5 = xdlopen ("tst-dl_find_object-mod5.so", RTLD_NOW);
    void *mod6 = xdlopen ("tst-dl_find_object-mod6.so", RTLD_NOW);
    start_verify (7, &data_mod7);
    xdlclose (mod6);
    xdlclose (mod5);
    xdlclose (mod3);
    xdlclose (mod1);
  }

  /* Objects that continuously opened and closed.  */
  struct temp_object
  {
    char *soname;
    char *symbol;
    struct link_map *link_map;
    void *address;
  } temp_objects[] =
    {
      { soname (1), symbol (1), },
      { soname (3), symbol (3), },
      { soname (5), symbol (5), },
      { soname (6), symbol (6), },
      { soname (8), symbol (8), },
      { soname (9), symbol (9), },
    };

  pthread_t exit_thr = xpthread_create (NULL, exit_thread, NULL);

  struct drand48_data state;
  srand48_r (1, &state);
  while (!__atomic_load_n (&exit_requested, __ATOMIC_RELAXED))
    {
      long int idx;
      lrand48_r (&state, &idx);
      idx %= array_length (temp_objects);
      if (temp_objects[idx].link_map == NULL)
        {
          temp_objects[idx].link_map = xdlopen (temp_objects[idx].soname,
                                                RTLD_NOW);
          temp_objects[idx].address = xdlsym (temp_objects[idx].link_map,
                                              temp_objects[idx].symbol);
        }
      else
        {
          xdlclose (temp_objects[idx].link_map);
          temp_objects[idx].link_map = NULL;
          struct dl_find_object dlfo;
          int ret = _dl_find_object (temp_objects[idx].address, &dlfo);
          if (ret != -1)
            {
              TEST_VERIFY_EXIT (ret == 0);
              support_record_failure ();
              printf ("%s: error: %s EH found after dlclose, link map %p\n",
                      __FILE__, temp_objects[idx].soname, dlfo.dlfo_link_map);
            }
        }
    }

  xpthread_join (data_mod2.thr);
  xpthread_join (data_mod4.thr);
  xpthread_join (data_mod7.thr);
  xpthread_join (exit_thr);

  for (size_t i = 0; i < array_length (temp_objects); ++i)
    {
      free (temp_objects[i].soname);
      free (temp_objects[i].symbol);
      if (temp_objects[i].link_map != NULL)
        xdlclose (temp_objects[i].link_map);
    }

  free (data_mod2.soname);
  free (data_mod4.soname);
  xdlclose (data_mod4.dlfo.dlfo_link_map);
  free (data_mod7.soname);
  xdlclose (data_mod7.dlfo.dlfo_link_map);

  return 0;
}

#include <support/test-driver.c>
