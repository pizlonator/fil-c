/* Test that IRELATIVE resolvers may call PLT functions with LD_BIND_NOW=1.
   Copyright (C) 2019-2026 Free Software Foundation, Inc.
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

/* Same as tst-ifunc-plt, but the test is run with LD_BIND_NOW=1 so the
   library is processed eagerly.  Under eager binding the JMP_SLOT entries
   in .rela.plt are resolved by elf_machine_rel during relocation; this
   exercises the eager path of the deferred IRELATIVE processing.  */

#include "tst-ifunc-plt.c"
