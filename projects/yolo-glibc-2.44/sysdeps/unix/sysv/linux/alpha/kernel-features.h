/* Set flags signalling availability of kernel features based on given
   kernel version number.
   Copyright (C) 2010-2026 Free Software Foundation, Inc.
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

#ifndef _KERNEL_FEATURES_H
#define _KERNEL_FEATURES_H 1

#include_next <kernel-features.h>

/* Support for statfs64 was added in 5.1.  */
#if __LINUX_KERNEL_VERSION < 0x050100
# undef __ASSUME_STATFS64
# define __ASSUME_STATFS64 0
#endif

#define __ASSUME_RECV_SYSCALL	1
#define __ASSUME_SEND_SYSCALL	1

/* Support for the renameat2 syscall was added in 3.17.  */
#if __LINUX_KERNEL_VERSION < 0x031100
# undef __ASSUME_RENAMEAT2
#endif

/* Support for the execveat syscall was added in 4.2.  */
#if __LINUX_KERNEL_VERSION < 0x040200
# undef __ASSUME_EXECVEAT
#endif

/* Support for copy_file_range, statx was added in kernel 4.13.  */
#if __LINUX_KERNEL_VERSION < 0x040D00
# undef __ASSUME_MLOCK2
# undef __ASSUME_STATX
#endif

/* Alpha requires old sysvipc even being a 64-bit architecture.  */
#undef __ASSUME_SYSVIPC_DEFAULT_IPC_64

/* Alpha does not provide clone3.  */
#undef __ASSUME_CLONE3
#define __ASSUME_CLONE3 0

/* Alpha did not provide BOTHER, CIBAUD or the termios2 ioctls until
   kernel 4.20.  Even though struct __kernel_termios and struct
   termios2 are the same on Alpha, Calling the legacy TCSETS* ioctls
   with BOTHER set triggers a bug in these old kernels, so only use
   the legacy TCSETS* ioctl numbers if neither BOTHER nor split speed is
   needed; that way the code will fail gracefully. */
#if __LINUX_KERNEL_VERSION < 0x041400
# undef  __ASSUME_TERMIOS2
# define __ASSUME_TERMIOS2 0
#endif

#endif /* _KERNEL_FEATURES_H */
