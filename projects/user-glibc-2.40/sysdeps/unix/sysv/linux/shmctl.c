/* Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <sys/shm.h>
#include <stdarg.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <shlib-compat.h>
#include <errno.h>
#include <linux/posix_types.h>  /* For __kernel_mode_t.  */
#include <pizlonated_syscalls.h>

/* POSIX states ipc_perm mode should have type of mode_t.  */
_Static_assert (sizeof ((struct shmid_ds){0}.shm_perm.mode)
		== sizeof (mode_t),
		"sizeof (shmid_ds.shm_perm.mode) != sizeof (mode_t)");

/* Provide operations to control over shared memory segments.  */
int
__shmctl64 (int shmid, int cmd, struct __shmid64_ds *buf)
{
  return zsys_shmctl (shmid, cmd, buf);
}
#if __TIMESIZE != 64
libc_hidden_def (__shmctl64)

static void
shmid_to_shmid64 (struct __shmid64_ds *shm64, const struct shmid_ds *shm)
{
  shm64->shm_perm   = shm->shm_perm;
  shm64->shm_segsz  = shm->shm_segsz;
  shm64->shm_atime  = shm->shm_atime
		      | ((__time64_t) shm->__shm_atime_high << 32);
  shm64->shm_dtime  = shm->shm_dtime
		      | ((__time64_t) shm->__shm_dtime_high << 32);
  shm64->shm_ctime  = shm->shm_ctime
		      | ((__time64_t) shm->__shm_ctime_high << 32);
  shm64->shm_cpid   = shm->shm_cpid;
  shm64->shm_lpid   = shm->shm_lpid;
  shm64->shm_nattch = shm->shm_nattch;
}

static void
shmid64_to_shmid (struct shmid_ds *shm, const struct __shmid64_ds *shm64)
{
  shm->shm_perm         = shm64->shm_perm;
  shm->shm_segsz        = shm64->shm_segsz;
  shm->shm_atime        = shm64->shm_atime;
  shm->__shm_atime_high = 0;
  shm->shm_dtime        = shm64->shm_dtime;
  shm->__shm_dtime_high = 0;
  shm->shm_ctime        = shm64->shm_ctime;
  shm->__shm_ctime_high = 0;
  shm->shm_cpid         = shm64->shm_cpid;
  shm->shm_lpid         = shm64->shm_lpid;
  shm->shm_nattch       = shm64->shm_nattch;
}

int
__shmctl (int shmid, int cmd, struct shmid_ds *buf)
{
  struct __shmid64_ds shmid64, *buf64 = NULL;
  if (buf != NULL)
    {
      /* This is a Linux extension where kernel expects either a
	 'struct shminfo' (IPC_INFO) or 'struct shm_info' (SHM_INFO).  */
      if (cmd == IPC_INFO || cmd == SHM_INFO)
	buf64 = (struct __shmid64_ds *) buf;
      else
	{
	  shmid_to_shmid64 (&shmid64, buf);
	  buf64 = &shmid64;
	}
    }

  int ret = __shmctl64 (shmid, cmd, buf64);
  if (ret < 0)
    return ret;

  switch (cmd)
    {
      case IPC_STAT:
      case SHM_STAT:
      case SHM_STAT_ANY:
	shmid64_to_shmid (buf, buf64);
    }

  return ret;
}
#endif

#ifndef DEFAULT_VERSION
# ifndef __ASSUME_SYSVIPC_BROKEN_MODE_T
#  define DEFAULT_VERSION GLIBC_2_2
# else
#  define DEFAULT_VERSION GLIBC_2_31
# endif
#endif

versioned_symbol (libc, __shmctl, shmctl, DEFAULT_VERSION);

#if defined __ASSUME_SYSVIPC_BROKEN_MODE_T \
    && SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_31)
int
attribute_compat_text_section
__shmctl_mode16 (int shmid, int cmd, struct shmid_ds *buf)
{
  return shmctl_syscall (shmid, cmd, (shmctl_arg_t *) buf);
}
compat_symbol (libc, __shmctl_mode16, shmctl, GLIBC_2_2);
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_2)
struct __old_shmid_ds
{
  struct __old_ipc_perm shm_perm;	/* operation permission struct */
  int shm_segsz;			/* size of segment in bytes */
  __time_t shm_atime;			/* time of last shmat() */
  __time_t shm_dtime;			/* time of last shmdt() */
  __time_t shm_ctime;			/* time of last change by shmctl() */
  __ipc_pid_t shm_cpid;			/* pid of creator */
  __ipc_pid_t shm_lpid;			/* pid of last shmop */
  unsigned short int shm_nattch;	/* number of current attaches */
  unsigned short int __shm_npages;	/* size of segment (pages) */
  unsigned long int *__shm_pages;	/* array of ptrs to frames -> SHMMAX */
  struct vm_area_struct *__attaches;	/* descriptors for attaches */
};

int
attribute_compat_text_section
__old_shmctl (int shmid, int cmd, struct __old_shmid_ds *buf)
{
#if defined __ASSUME_DIRECT_SYSVIPC_SYSCALLS \
    && !defined __ASSUME_SYSVIPC_DEFAULT_IPC_64
  /* For architecture that have wire-up shmctl but also have __IPC_64 to a
     value different than default (0x0), it means the compat symbol used the
     __NR_ipc syscall.  */
  return INLINE_SYSCALL_CALL (shmctl, shmid, cmd, buf);
#else
  return INLINE_SYSCALL_CALL (ipc, IPCOP_shmctl, shmid, cmd, 0, buf);
#endif
}
compat_symbol (libc, __old_shmctl, shmctl, GLIBC_2_0);
#endif
