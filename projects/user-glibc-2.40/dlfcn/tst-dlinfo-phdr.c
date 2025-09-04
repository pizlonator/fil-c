/* Test for dlinfo (RTLD_DI_PHDR).
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>

#include <support/check.h>
#include <support/xdlfcn.h>

/* Used to verify that the program header array appears as expected
   among the dl_iterate_phdr callback invocations.  */

struct dlip_callback_args
{
  struct link_map *l;           /* l->l_addr is used to find the object.  */
  const ElfW(Phdr) *phdr;       /* Expected program header pointed.  */
  int phnum;                    /* Expected program header count.  */
  bool found;                   /* True if l->l_addr has been found.  */
};

static int
dlip_callback (struct dl_phdr_info *dlpi, size_t size, void *closure)
{
  TEST_COMPARE (sizeof (*dlpi), size);
  struct dlip_callback_args *args = closure;

  if (dlpi->dlpi_addr == args->l->l_addr)
    {
      TEST_VERIFY (!args->found);
      args->found = true;
      TEST_VERIFY (args->phdr == dlpi->dlpi_phdr);
      TEST_COMPARE (args->phnum, dlpi->dlpi_phnum);
    }

  return 0;
}

static int
do_test (void)
{
  /* Avoid a copy relocation.  */
  struct r_debug *debug = xdlsym (RTLD_DEFAULT, "_r_debug");
  struct link_map *l = (struct link_map *) debug->r_map;
  TEST_VERIFY_EXIT (l != NULL);

  do
    {
      printf ("info: checking link map %p (%p) for \"%s\"\n",
              l, l->l_phdr, l->l_name);

      /* Cause dlerror () to return an error message.  */
      dlsym (RTLD_DEFAULT, "does-not-exist");

      /* Use the extension that link maps are valid dlopen handles.  */
      const ElfW(Phdr) *phdr;
      int phnum = dlinfo (l, RTLD_DI_PHDR, &phdr);
      TEST_VERIFY (phnum >= 0);
      /* Verify that the error message has been cleared.  */
      TEST_COMPARE_STRING (dlerror (), NULL);

      TEST_VERIFY (phdr == l->l_phdr);
      TEST_COMPARE (phnum, l->l_phnum);

      /* Check that we can find PT_DYNAMIC among the array.  */
      {
        bool dynamic_found = false;
        for (int i = 0; i < phnum; ++i)
          if (phdr[i].p_type == PT_DYNAMIC)
            {
              dynamic_found = true;
              TEST_COMPARE ((ElfW(Addr)) l->l_ld, l->l_addr + phdr[i].p_vaddr);
            }
        TEST_VERIFY (dynamic_found);
      }

      /* Check that dl_iterate_phdr finds the link map with the same
         program headers.  */
      {
        struct dlip_callback_args args =
          {
            .l =  l,
            .phdr = phdr,
            .phnum = phnum,
            .found = false,
          };
        TEST_COMPARE (dl_iterate_phdr (dlip_callback, &args), 0);
        TEST_VERIFY (args.found);
      }

      if (l->l_prev == NULL)
        {
          /* This is the executable, so the information is also
             available via getauxval.  */
          TEST_COMPARE_STRING (l->l_name, "");
          TEST_VERIFY (phdr == (const ElfW(Phdr) *) getauxval (AT_PHDR));
          TEST_COMPARE (phnum, getauxval (AT_PHNUM));
        }

      l = l->l_next;
    }
  while (l != NULL);

  return 0;
}

#include <support/test-driver.c>
