/* glibc-hwcaps subdirectory test.  s390x version.
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

#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <sys/auxv.h>
#include <sys/param.h>

extern int marker2 (void);
extern int marker3 (void);
extern int marker4 (void);
extern int marker5 (void);
extern int marker6 (void);

/* Return the arch level, 10 for the baseline libmarkermod*.so's.  */
static int
compute_level (void)
{
  const char *platform = (const char *) getauxval (AT_PLATFORM);
  const unsigned long int hwcap = getauxval (AT_HWCAP);
  const int latest_level = 15;

  /* The arch* versions refer to the edition of the Principles of
     Operation, and they are off by two when compared with the recent
     product names.  (The code below should not be considered an
     accurate mapping to Principles of Operation editions for earlier
     AT_PLATFORM strings).  */
  if ((strcmp (platform, "z900") == 0)
      || (strcmp (platform, "z990") == 0)
      || (strcmp (platform, "z9-109") == 0)
      || (strcmp (platform, "z10") == 0)
      || (strcmp (platform, "z196") == 0)
      || (strcmp (platform, "zEC12") == 0))
    {
      if ((hwcap & HWCAP_S390_VX) == 0)
	{
	  /* As vector-support was introduced with the newer z13
	     architecture, we are really on one of the tested older
	     architectures.  */
	  return 10;
	}
      else
	{
	  /* According to AT_PLATFORM we are on an older architecture
	     without vector-support, but according to HWCAPs vector
	     registers are supported.  This means we are running on a
	     new architecture which is not yet known by the kernel.
	     Thus the default AT_PLATFORM string is used, which is the
	     oldest supported one.  For this test, assume we are on
	     the latest known architecture.  See
	     <kernel>/arch/s390/kernel/processor.c:setup_elf_platform().
	  */
	  return latest_level;
	}
    }

  /* If we are running on z13 or newer and the kernel was booted with novx,
     then AT_PLATFORM is z13 or newer, but _dl_hwcaps_subdirs_active will
     return zero and the _dl_hwcaps_subdirs are not searched.  */
  if ((hwcap & HWCAP_S390_VX) == 0)
    return 10;

  if (strcmp (platform, "z13") == 0)
    return 11;
  if (strcmp (platform, "z14") == 0)
    return 12;
  if (strcmp (platform, "z15") == 0)
    return 13;
  if (strcmp (platform, "z16") == 0)
    return 14;
  if (strcmp (platform, "z17") == 0)
    return latest_level;

  printf ("warning: unrecognized AT_PLATFORM value: %s\n", platform);
  /* Assume that the new platform supports the latest known architecture.  */
  return latest_level;
}

static int
do_test (void)
{
  int level = compute_level ();
  printf ("info: detected architecture level: arch%d\n", level);
  TEST_COMPARE (marker2 (), MIN (level - 9, 2));
  TEST_COMPARE (marker3 (), MIN (level - 9, 3));
  TEST_COMPARE (marker4 (), MIN (level - 9, 4));
  TEST_COMPARE (marker5 (), MIN (level - 9, 5));
  TEST_COMPARE (marker6 (), MIN (level - 9, 6));
  return 0;
}

#include <support/test-driver.c>
