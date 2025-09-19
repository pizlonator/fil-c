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

#include <sys/msg.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <shlib-compat.h>
#include <errno.h>
#include <linux/posix_types.h>  /* For __kernel_mode_t.  */
#include <pizlonated_syscalls.h>

/* POSIX states ipc_perm mode should have type of mode_t.  */
_Static_assert (sizeof ((struct msqid_ds){0}.msg_perm.mode)
		== sizeof (mode_t),
		"sizeof (msqid_ds.msg_perm.mode) != sizeof (mode_t)");

int
__msgctl64 (int msqid, int cmd, struct __msqid64_ds *buf)
{
  return zsys_msgctl (msqid, cmd, buf);
}
#if __TIMESIZE != 64
libc_hidden_def (__msgctl64)

static void
msqid_to_msqid64 (struct __msqid64_ds *mq64, const struct msqid_ds *mq)
{
  mq64->msg_perm   = mq->msg_perm;
  mq64->msg_stime  = mq->msg_stime
		     | ((__time64_t) mq->__msg_stime_high << 32);
  mq64->msg_rtime  = mq->msg_rtime
		     | ((__time64_t) mq->__msg_rtime_high << 32);
  mq64->msg_ctime  = mq->msg_ctime
		     | ((__time64_t) mq->__msg_ctime_high << 32);
  mq64->msg_cbytes = mq->msg_cbytes;
  mq64->msg_qnum   = mq->msg_qnum;
  mq64->msg_qbytes = mq->msg_qbytes;
  mq64->msg_lspid  = mq->msg_lspid;
  mq64->msg_lrpid  = mq->msg_lrpid;
}

static void
msqid64_to_msqid (struct msqid_ds *mq, const struct __msqid64_ds *mq64)
{
  mq->msg_perm         = mq64->msg_perm;
  mq->msg_stime        = mq64->msg_stime;
  mq->__msg_stime_high = 0;
  mq->msg_rtime        = mq64->msg_rtime;
  mq->__msg_rtime_high = 0;
  mq->msg_ctime        = mq64->msg_ctime;
  mq->__msg_ctime_high = 0;
  mq->msg_cbytes       = mq64->msg_cbytes;
  mq->msg_qnum         = mq64->msg_qnum;
  mq->msg_qbytes       = mq64->msg_qbytes;
  mq->msg_lspid        = mq64->msg_lspid;
  mq->msg_lrpid        = mq64->msg_lrpid;
}

int
__msgctl (int msqid, int cmd, struct msqid_ds *buf)
{
  struct __msqid64_ds msqid64, *buf64 = NULL;
  if (buf != NULL)
    {
      /* This is a Linux extension where kernel returns a 'struct msginfo'
	 instead.  */
      if (cmd == IPC_INFO || cmd == MSG_INFO)
	buf64 = (struct __msqid64_ds *) buf;
      else
	{
	  msqid_to_msqid64 (&msqid64, buf);
	  buf64 = &msqid64;
	}
    }

  int ret = __msgctl64 (msqid, cmd, buf64);
  if (ret < 0)
    return ret;

  switch (cmd)
    {
    case IPC_STAT:
    case MSG_STAT:
    case MSG_STAT_ANY:
      msqid64_to_msqid (buf, buf64);
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
versioned_symbol (libc, __msgctl, msgctl, DEFAULT_VERSION);

#if defined __ASSUME_SYSVIPC_BROKEN_MODE_T \
    && SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_31)
int
attribute_compat_text_section
__msgctl_mode16 (int msqid, int cmd, struct msqid_ds *buf)
{
  return msgctl_syscall (msqid, cmd, (msgctl_arg_t *) buf);
}
compat_symbol (libc, __msgctl_mode16, msgctl, GLIBC_2_2);
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_2)
struct __old_msqid_ds
{
  struct __old_ipc_perm msg_perm;	/* structure describing operation permission */
  struct msg *__msg_first;		/* pointer to first message on queue */
  struct msg *__msg_last;		/* pointer to last message on queue */
  __time_t msg_stime;			/* time of last msgsnd command */
  __time_t msg_rtime;			/* time of last msgrcv command */
  __time_t msg_ctime;			/* time of last change */
  struct wait_queue *__wwait;		/* ??? */
  struct wait_queue *__rwait;		/* ??? */
  unsigned short int __msg_cbytes;	/* current number of bytes on queue */
  unsigned short int msg_qnum;		/* number of messages currently on queue */
  unsigned short int msg_qbytes;	/* max number of bytes allowed on queue */
  __ipc_pid_t msg_lspid;		/* pid of last msgsnd() */
  __ipc_pid_t msg_lrpid;		/* pid of last msgrcv() */
};

int
attribute_compat_text_section
__old_msgctl (int msqid, int cmd, struct __old_msqid_ds *buf)
{
#if defined __ASSUME_DIRECT_SYSVIPC_SYSCALLS \
    && !defined __ASSUME_SYSVIPC_DEFAULT_IPC_64
  /* For architecture that have wire-up msgctl but also have __IPC_64 to a
     value different than default (0x0) it means the compat symbol used the
     __NR_ipc syscall.  */
  return INLINE_SYSCALL_CALL (msgctl, msqid, cmd, buf);
#else
  return INLINE_SYSCALL_CALL (ipc, IPCOP_msgctl, msqid, cmd, 0, buf);
#endif
}
compat_symbol (libc, __old_msgctl, msgctl, GLIBC_2_0);
#endif
