/* Check for hardware capabilities after HWCAP parsing.  S390 version.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#ifndef _DL_HWCAP_CHECK_H
#define _DL_HWCAP_CHECK_H

#include <gcc-macros.h>
#include <ldsodefs.h>

static inline void
dl_hwcap_check (void)
{
  /* Note: The s390x kernel won't introduce new HWCAP-Bits if there is
     no special handling needed in kernel itself.  Thus we have have
     to check the facility-list retrieved with the stfle instruction.
     We already have a common storage of this list in cpu-features.c.
     This dl-hwcap-check.h file is included in
     sysdeps/unix/sysv/linux/dl-sysdep.c, where also dl-machine.h and
     cpu-features.c is included.  Therefore we don't have a special
     include here.  */

#if defined GCCMACRO__ARCH__
# if GCCMACRO__ARCH__ >= 15
  init_cpu_features_no_tunables (&GLRO(dl_s390_cpu_features));
  if (!(S390_IS_ARCH15 (GLRO(dl_s390_cpu_features).stfle_orig)))
    _dl_fatal_printf ("\
Fatal glibc error: CPU lacks VXRS_EXT3/VXRS_PDE3/MIE4/Concurrent-functions \
support (z17 or later required)\n");
# elif GCCMACRO__ARCH__ >= 14
  if (!(GLRO(dl_hwcap) & HWCAP_S390_VXRS_PDE2))
    _dl_fatal_printf ("\
Fatal glibc error: CPU lacks VXRS_PDE2 support (z16 or later required)\n");
# elif GCCMACRO__ARCH__ >= 13
  if (!(GLRO(dl_hwcap) & HWCAP_S390_VXRS_EXT2))
    _dl_fatal_printf ("\
Fatal glibc error: CPU lacks VXRS_EXT2 support (z15 or later required)\n");
# elif GCCMACRO__ARCH__ >= 12
  if (!(GLRO(dl_hwcap) & HWCAP_S390_VXE))
    _dl_fatal_printf ("\
Fatal glibc error: CPU lacks VXE support (z14 or later required)\n");
# endif
#endif /* GCCMACRO__ARCH__ */
}

#endif /* _DL_HWCAP_CHECK_H */
