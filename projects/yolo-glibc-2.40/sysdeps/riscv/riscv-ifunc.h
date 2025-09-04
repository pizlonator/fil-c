/* Common definition for ifunc resolvers.  Linux/RISC-V version.
   This file is part of the GNU C Library.
   Copyright (C) 2024 Free Software Foundation, Inc.

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

#include <sysdep.h>
#include <ifunc-init.h>
#include <sys/hwprobe.h>

#define INIT_ARCH()

#define riscv_libc_ifunc(name, expr)				\
  __ifunc_args (name, name, expr(hwcap, hwprobe), INIT_ARCH,	\
                uint64_t hwcap, __riscv_hwprobe_t hwprobe)
