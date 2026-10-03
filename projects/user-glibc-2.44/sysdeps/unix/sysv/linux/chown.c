/* Change ownership of a file.  Linux version.
   Copyright (C) 2011-2026 Free Software Foundation, Inc.
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

#include <unistd.h>
#include <fcntl.h>
#include <sysdep.h>
#include <pizlonated_syscalls.h>

/* Change the owner and group of FILE.  */
int
__chown (const char *file, uid_t owner, gid_t group)
{
#ifdef __NR_chown
  return INLINE_SYSCALL_CALL (chown, file, owner, group);
#else
  /* Fil-C: aarch64 has no chown kernel syscall, so this fallback would use
     a raw svc inline asm, which Fil-C cannot make memory safe.  Route it
     through the zsys runtime instead.  x86_64 defines __NR_chown and uses
     the INLINE_SYSCALL_CALL above (in practice x86_64 does not even compile
     this file: the shared sysdeps/unix/sysv/linux/syscalls.list chown entry
     - there is no chown entry in the x86_64 syscalls.list - generates a
     zsys_chown stub via the ported sysdeps/unix/make-syscalls.sh zsys
     redirect, and that generated rule overrides the implicit rule for this
     file).  */
  return zsys_fchownat (AT_FDCWD, file, owner, group, 0);
#endif
}
libc_hidden_def (__chown)
weak_alias (__chown, chown)
