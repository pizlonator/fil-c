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

#include <sys/sem.h>
#include <stdarg.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <shlib-compat.h>
#include <linux/posix_types.h>             /* For __kernel_mode_t.  */
#include <pizlonated_syscalls.h>

int
__semctl64 (int semid, int semnum, int cmd, ...)
{
  return *(int *) zcall (zsys_semctl, zargs ());
}
#if __TIMESIZE != 64
libc_hidden_def (__semctl64)


/* The 64-bit time_t semid_ds version might have a different layout and
   internal field alignment.  */

static void
semid_to_semid64 (struct __semid64_ds *ds64, const struct semid_ds *ds)
{
  ds64->sem_perm  = ds->sem_perm;
  ds64->sem_otime = ds->sem_otime
		    | ((__time64_t) ds->__sem_otime_high << 32);
  ds64->sem_ctime = ds->sem_ctime
		    | ((__time64_t) ds->__sem_ctime_high << 32);
  ds64->sem_nsems = ds->sem_nsems;
}

static void
semid64_to_semid (struct semid_ds *ds, const struct __semid64_ds *ds64)
{
  ds->sem_perm         = ds64->sem_perm;
  ds->sem_otime        = ds64->sem_otime;
  ds->__sem_otime_high = 0;
  ds->sem_ctime        = ds64->sem_ctime;
  ds->__sem_ctime_high = 0;
  ds->sem_nsems        = ds64->sem_nsems;
}

static union semun64
semun_to_semun64 (int cmd, union semun semun, struct __semid64_ds *semid64)
{
  union semun64 r = { 0 };
  switch (cmd)
    {
    case SETVAL:
      r.val = semun.val;
      break;
    case GETALL:
    case SETALL:
      r.array = semun.array;
      break;
    case SEM_STAT:
    case SEM_STAT_ANY:
    case IPC_STAT:
    case IPC_SET:
      r.buf = semid64;
      semid_to_semid64 (r.buf, semun.buf);
      break;
    case IPC_INFO:
    case SEM_INFO:
      r.__buf = semun.__buf;
      break;
    }
  return r;
}

int
__semctl (int semid, int semnum, int cmd, ...)
{
  union semun arg = { 0 };

  va_list ap;

  /* Get the argument only if required.  */
  switch (cmd)
    {
    case SETVAL:        /* arg.val */
    case GETALL:        /* arg.array */
    case SETALL:
    case IPC_STAT:      /* arg.buf */
    case IPC_SET:
    case SEM_STAT:
    case SEM_STAT_ANY:
    case IPC_INFO:      /* arg.__buf */
    case SEM_INFO:
      va_start (ap, cmd);
      arg = va_arg (ap, union semun);
      va_end (ap);
      break;
    /* __semctl64 handles non-supported commands.  */
    }

  struct __semid64_ds semid64;
  union semun64 arg64 = semun_to_semun64 (cmd, arg, &semid64);

  int ret = __semctl64 (semid, semnum, cmd, arg64);
  if (ret < 0)
    return ret;

  switch (cmd)
    {
    case IPC_STAT:
    case SEM_STAT:
    case SEM_STAT_ANY:
      semid64_to_semid (arg.buf, arg64.buf);
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
versioned_symbol (libc, __semctl, semctl, DEFAULT_VERSION);

#if defined __ASSUME_SYSVIPC_BROKEN_MODE_T \
    && SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_31)
int
attribute_compat_text_section
__semctl_mode16 (int semid, int semnum, int cmd, ...)
{
  semctl_arg_t arg = { 0 };
  va_list ap;

  /* Get the argument only if required.  */
  switch (cmd)
    {
    case SETVAL:        /* arg.val */
    case GETALL:        /* arg.array */
    case SETALL:
    case IPC_STAT:      /* arg.buf */
    case IPC_SET:
    case SEM_STAT:
    case SEM_STAT_ANY:
    case IPC_INFO:      /* arg.__buf */
    case SEM_INFO:
      va_start (ap, cmd);
      arg = va_arg (ap, semctl_arg_t);
      va_end (ap);
      break;
    }

  return semctl_syscall (semid, semnum, cmd, arg);
}
compat_symbol (libc, __semctl_mode16, semctl, GLIBC_2_2);
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_2)
/* Since semctl use a variadic argument for semid_ds there is not need to
   define and tie the compatibility symbol to the old 'union semun'
   definition.  */
int
attribute_compat_text_section
__old_semctl (int semid, int semnum, int cmd, ...)
{
  union semun arg = { 0 };
  va_list ap;

  /* Get the argument only if required.  */
  switch (cmd)
    {
    case SETVAL:        /* arg.val */
    case GETALL:        /* arg.array */
    case SETALL:
    case IPC_STAT:      /* arg.buf */
    case IPC_SET:
    case SEM_STAT:
    case SEM_STAT_ANY:
    case IPC_INFO:      /* arg.__buf */
    case SEM_INFO:
      va_start (ap, cmd);
      arg = va_arg (ap, union semun);
      va_end (ap);
      break;
    }

#if defined __ASSUME_DIRECT_SYSVIPC_SYSCALLS \
    && !defined __ASSUME_SYSVIPC_DEFAULT_IPC_64
 /* For architectures that have wire-up semctl but also have __IPC_64 to a
    value different than default (0x0) it means the compat symbol used the
    __NR_ipc syscall.  */
  return INLINE_SYSCALL_CALL (semctl, semid, semnum, cmd, arg.array);
# else
  return INLINE_SYSCALL_CALL (ipc, IPCOP_semctl, semid, semnum, cmd,
			      SEMCTL_ARG_ADDRESS (arg));
# endif
}
compat_symbol (libc, __old_semctl, semctl, GLIBC_2_0);
#endif
