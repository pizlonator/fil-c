/* Base for test program with impossiblyh large PT_TLS segment.
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

/* The test actual binary is patched using scripts/tst-elf-edit.py
   --maximize-tls-size, and this introduces the expected test
   allocation failure due to an excessive PT_LS p_memsz value.

   Patching the binary is required because on some 64-bit targets, TLS
   relocations can only cover a 32-bit range, and glibc-internal TLS
   variables such as errno end up outside that range.  */

int
main (void)
{
  return 0;
}
