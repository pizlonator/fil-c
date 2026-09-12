/* Macros for ucontext routines.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _LINUX_LOONGARCH_UCONTEXT_MACROS_H
#define _LINUX_LOONGARCH_UCONTEXT_MACROS_H

#include <sysdep.h>
#include <sys/asm.h>
#include "ucontext_i.h"

/* mcontext_t.__gregs is unsigned long long on la32 and la64,
   use 8 instead of SZREG.  */
#define SAVE_INT_REG(name, num, base) \
  REG_S name, base, ((num) * 8 + MCONTEXT_GREGS)

#define RESTORE_INT_REG(name, num, base) \
  REG_L name, base, ((num) * 8 + MCONTEXT_GREGS)

#endif /* _LINUX_LOONGARCH_UCONTEXT_MACROS_H */
