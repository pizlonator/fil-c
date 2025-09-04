/* Check the usability of <dlfcn.h> functions in audit modules.  Audit module.
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
#include <first-versions.h>
#include <gnu/lib-names.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>
#include <support/xdlfcn.h>

unsigned int
la_version (unsigned int current)
{
  /* Exercise various <dlfcn.h> functions.  */

  /* Check dlopen, dlsym, dlclose.   */
  void *handle = xdlopen (LIBM_SO, RTLD_LOCAL | RTLD_NOW);
  void *ptr = xdlsym (handle, "sincos");
  TEST_VERIFY (ptr != NULL);
  ptr = dlsym (handle, "SINCOS");
  TEST_VERIFY (ptr == NULL);
  const char *message = dlerror ();
  TEST_VERIFY (strstr (message, ": undefined symbol: SINCOS") != NULL);
  ptr = dlsym (handle, "SINCOS");
  TEST_VERIFY (ptr == NULL);
  xdlclose (handle);
  TEST_COMPARE_STRING (dlerror (), NULL);

  handle = xdlopen (LIBC_SO, RTLD_LOCAL | RTLD_NOW | RTLD_NOLOAD);

  /* Check dlvsym.  _exit is unlikely to gain another symbol
     version.  */
  TEST_VERIFY (xdlsym (handle, "_exit")
               == xdlvsym (handle, "_exit", FIRST_VERSION_libc__exit_STRING));

  /* Check dlinfo.  */
  {
    void *handle2 = NULL;
    TEST_COMPARE (dlinfo (handle, RTLD_DI_LINKMAP, &handle2), 0);
    TEST_VERIFY (handle2 == handle);
  }

  /* Check dladdr and dladdr1.  */
  Dl_info info = { };
  TEST_VERIFY (dladdr (&_exit, &info) != 0);
  if (strcmp (info.dli_sname, "_Exit") != 0) /* _Exit is an alias.  */
    TEST_COMPARE_STRING (info.dli_sname, "_exit");
  TEST_VERIFY (info.dli_saddr == &_exit);
  TEST_VERIFY (strstr (info.dli_fname, LIBC_SO));
  void *extra_info;
  memset (&info, 0, sizeof (info));
  TEST_VERIFY (dladdr1 (&_exit, &info, &extra_info, RTLD_DL_LINKMAP) != 0);
  TEST_VERIFY (extra_info == handle);

  /* Check _dl_find_object.  */
  struct dl_find_object dlfo;
  void *ret_addr = __builtin_extract_return_addr (__builtin_return_address (0));
  int ret_dl_find_object =_dl_find_object (ret_addr, &dlfo);
  TEST_COMPARE (ret_dl_find_object, 0);
  if (ret_dl_find_object == 0)
    {
      /* "ld.so" is seen with --enable-hardcoded-path-in-tests.  */
      if (strcmp (basename (dlfo.dlfo_link_map->l_name), "ld.so") != 0)
	TEST_COMPARE_STRING (basename (dlfo.dlfo_link_map->l_name), LD_SO);
    }
  ret_dl_find_object = _dl_find_object (dlsym (handle, "environ"), &dlfo);
  TEST_COMPARE (ret_dl_find_object, 0);
  if (ret_dl_find_object == 0)
    TEST_COMPARE_STRING (basename (dlfo.dlfo_link_map->l_name), LIBC_SO);
  TEST_COMPARE (_dl_find_object ((void *) 1, &dlfo), -1);
  TEST_COMPARE (_dl_find_object ((void *) -1, &dlfo), -1);

  /* Verify that dlmopen creates a new namespace.  */
  void *dlmopen_handle = xdlmopen (LM_ID_NEWLM, LIBC_SO, RTLD_NOW);
  TEST_VERIFY (dlmopen_handle != handle);
  memset (&info, 0, sizeof (info));
  extra_info = NULL;
  ptr = xdlsym (dlmopen_handle, "_exit");
  TEST_VERIFY (dladdr1 (ptr, &info, &extra_info, RTLD_DL_LINKMAP) != 0);
  TEST_VERIFY (extra_info == dlmopen_handle);
  xdlclose (dlmopen_handle);

  /* Terminate the process with an error state.  This does not happen
     automatically because the audit module state is not shared with
     the main program.  */
  if (support_record_failure_is_failed ())
    {
      fflush (stdout);
      fflush (stderr);
      _exit (1);
    }

  return LAV_CURRENT;
}

char *
la_objsearch (const char *name, uintptr_t *cookie, unsigned int flag)
{
  if (strcmp (name, "mapped to libc") == 0)
    return (char *) LIBC_SO;
  else
    return (char *) name;
}
