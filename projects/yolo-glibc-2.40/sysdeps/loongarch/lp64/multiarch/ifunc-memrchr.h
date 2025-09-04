/* Common definition for memrchr implementation.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <ldsodefs.h>
#include <ifunc-init.h>

#if !defined __loongarch_soft_float
extern __typeof (REDIRECT_NAME) OPTIMIZE (lasx) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (lsx) attribute_hidden;
#endif
extern __typeof (REDIRECT_NAME) OPTIMIZE (generic) attribute_hidden;

static inline void *
IFUNC_SELECTOR (void)
{
#if !defined __loongarch_soft_float
  if (SUPPORT_LASX)
    return OPTIMIZE (lasx);
  else if (SUPPORT_LSX)
    return OPTIMIZE (lsx);
  else
#endif
    return OPTIMIZE (generic);
}
