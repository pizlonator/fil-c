/* Wrapper for file descriptors that refers to a process functions.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _PIDFD_H

#include <fcntl.h>
#include <bits/types/siginfo_t.h>
#include <sys/ioctl.h>

#define PIDFD_NONBLOCK O_NONBLOCK
#define PIDFD_THREAD O_EXCL

#define PIDFD_SIGNAL_THREAD (1UL << 0)
#define PIDFD_SIGNAL_THREAD_GROUP (1UL << 1)
#define PIDFD_SIGNAL_PROCESS_GROUP (1UL << 2)

#define PIDFS_IOCTL_MAGIC 0xFF

#define PIDFD_GET_CGROUP_NAMESPACE            _IO(PIDFS_IOCTL_MAGIC, 1)
#define PIDFD_GET_IPC_NAMESPACE               _IO(PIDFS_IOCTL_MAGIC, 2)
#define PIDFD_GET_MNT_NAMESPACE               _IO(PIDFS_IOCTL_MAGIC, 3)
#define PIDFD_GET_NET_NAMESPACE               _IO(PIDFS_IOCTL_MAGIC, 4)
#define PIDFD_GET_PID_NAMESPACE               _IO(PIDFS_IOCTL_MAGIC, 5)
#define PIDFD_GET_PID_FOR_CHILDREN_NAMESPACE  _IO(PIDFS_IOCTL_MAGIC, 6)
#define PIDFD_GET_TIME_NAMESPACE              _IO(PIDFS_IOCTL_MAGIC, 7)
#define PIDFD_GET_TIME_FOR_CHILDREN_NAMESPACE _IO(PIDFS_IOCTL_MAGIC, 8)
#define PIDFD_GET_USER_NAMESPACE              _IO(PIDFS_IOCTL_MAGIC, 9)
#define PIDFD_GET_UTS_NAMESPACE               _IO(PIDFS_IOCTL_MAGIC, 10)

/* Sentinels to avoid allocating a file descriptor to refer to own process.  */
#define PIDFD_SELF_THREAD                     -10000
#define PIDFD_SELF_THREAD_GROUP               -10001
#define PIDFD_SELF                            PIDFD_SELF_THREAD
#define PIDFD_SELF_PROCESS                    PIDFD_SELF_THREAD_GROUP


/* Flags for pidfd_info.  */

/* Always returned, even if not requested */
#define PIDFD_INFO_PID                        (1UL << 0)
/* Always returned, even if not requested */
#define PIDFD_INFO_CREDS                      (1UL << 1)
/* Always returned if available, even if not requested */
#define PIDFD_INFO_CGROUPID                   (1UL << 2)
/* Only returned if requested. */
#define PIDFD_INFO_EXIT                       (1UL << 3)
/* Only returned if requested. */
#define PIDFD_INFO_COREDUMP                   (1UL << 4)
/* Want/got supported mask flags */
#define PIDFD_INFO_SUPPORTED_MASK             (1UL << 5)
/* Always returned if PIDFD_INFO_COREDUMP is requested. */
#define PIDFD_INFO_COREDUMP_SIGNAL            (1UL << 6)
/* Always returned if PIDFD_INFO_COREDUMP is requested. */
#define PIDFD_INFO_COREDUMP_CODE              (1UL << 7)


/* Value for coredump_mask in pidfd_info.  Only valid if PIDFD_INFO_COREDUMP
   is set in mask.  */

/* Did crash and... */
#define PIDFD_COREDUMPED                      (1U << 0)
/* coredumping generation was skipped. */
#define PIDFD_COREDUMP_SKIP                   (1U << 1)
/* coredump was done as the user. */
#define PIDFD_COREDUMP_USER                   (1U << 2)
/* coredump was done as root. */
#define PIDFD_COREDUMP_ROOT                   (1U << 3)

struct pidfd_info
{
  __uint64_t mask;
  __uint64_t cgroupid;
  __uint32_t pid;
  __uint32_t tgid;
  __uint32_t ppid;
  __uint32_t ruid;
  __uint32_t rgid;
  __uint32_t euid;
  __uint32_t egid;
  __uint32_t suid;
  __uint32_t sgid;
  __uint32_t fsuid;
  __uint32_t fsgid;
  __int32_t  exit_code;
  __uint32_t coredump_mask;
  __uint32_t coredump_signal;
  __uint32_t coredump_code;
  __uint32_t coredump_pad;
  __uint64_t supported_mask;
};

/* sizeof first published struct */
#define PIDFD_INFO_SIZE_VER0                  64
/* sizeof second published struct */
#define PIDFD_INFO_SIZE_VER1                  72
/* sizeof third published struct */
#define PIDFD_INFO_SIZE_VER2                  80
/* sizeof fourth published struct */
#define PIDFD_INFO_SIZE_VER3                  88

#define PIDFD_GET_INFO                        _IOWR(PIDFS_IOCTL_MAGIC, 11, struct pidfd_info)

/* Returns a file descriptor that refers to the process PID.  The
   close-on-exec is set on the file descriptor.  */
extern int pidfd_open (__pid_t __pid, unsigned int __flags) __THROW;

/* Duplicates an existing file descriptor TARGETFD in the process referred
   by the PIDFD file descriptor PIDFD.

   The FLAGS argument is reserved for future use, it must be specified
   as 0.  */
extern int pidfd_getfd (int __pidfd, int __targetfd,
			unsigned int __flags) __THROW;

/* Sends the signal SIG to the target process referred by the PIDFD.  If
   INFO points to a siginfo_t buffer, it will be populated.  */
extern int pidfd_send_signal (int __pidfd, int __sig, siginfo_t *__info,
			      unsigned int __flags) __THROW;

/* Query the process ID (PID) from process descriptor FD.  Return the PID
   or -1 in case of an error.  */
extern pid_t pidfd_getpid (int __fd) __THROW;

#endif /* _PIDFD_H  */
