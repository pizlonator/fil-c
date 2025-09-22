/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2004 Robert Love
 * Copyright (C) 2020-2021 Qais Yousef
 * Copyright (C) 2020-2021 Arm Ltd
 */
#ifndef UTIL_LINUX_SCHED_ATTR_H
#define UTIL_LINUX_SCHED_ATTR_H

/* the SCHED_BATCH is supported since Linux 2.6.16
 *  -- temporary workaround for people with old glibc headers
 */
#if defined (__linux__) && !defined(SCHED_BATCH)
# define SCHED_BATCH 3
#endif

/* the SCHED_IDLE is supported since Linux 2.6.23
 * commit id 0e6aca43e08a62a48d6770e9a159dbec167bf4c6
 * -- temporary workaround for people with old glibc headers
 */
#if defined (__linux__) && !defined(SCHED_IDLE)
# define SCHED_IDLE 5
#endif

/* flag by sched_getscheduler() */
#if defined(__linux__) && !defined(SCHED_RESET_ON_FORK)
# define SCHED_RESET_ON_FORK 0x40000000
#endif

/* flag by sched_getattr() */
#if defined(__linux__) && !defined(SCHED_FLAG_RESET_ON_FORK)
# define SCHED_FLAG_RESET_ON_FORK 0x01
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_RECLAIM)
# define SCHED_FLAG_RECLAIM 0x02
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_DL_OVERRUN)
# define SCHED_FLAG_DL_OVERRUN 0x04
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_KEEP_POLICY)
# define SCHED_FLAG_KEEP_POLICY 0x08
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_KEEP_PARAMS)
# define SCHED_FLAG_KEEP_PARAMS 0x10
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_UTIL_CLAMP_MIN)
# define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_UTIL_CLAMP_MAX)
# define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

/* usable kernel-headers, but old glibc-headers */
#if defined (__linux__) && !defined(SYS_sched_setattr) && defined(__NR_sched_setattr)
# define SYS_sched_setattr __NR_sched_setattr
#endif

#if defined (__linux__) && !defined(SYS_sched_getattr) && defined(__NR_sched_getattr)
# define SYS_sched_getattr __NR_sched_getattr
#endif

#if defined (__linux__) && !defined(HAVE_SCHED_SETATTR) && defined(SYS_sched_setattr)
# define HAVE_SCHED_SETATTR

struct sched_attr {
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	int32_t sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	uint32_t sched_priority;

	/* SCHED_DEADLINE (nsec) */
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;

	/* UTILIZATION CLAMPING */
	uint32_t sched_util_min;
	uint32_t sched_util_max;
};

static int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags)
{
	return syscall(SYS_sched_setattr, pid, attr, flags);
}

static int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags)
{
	return syscall(SYS_sched_getattr, pid, attr, size, flags);
}
#endif

/* the SCHED_DEADLINE is supported since Linux 3.14
 * commit id aab03e05e8f7e26f51dee792beddcb5cca9215a5
 * -- sched_setattr() is required for this policy!
 */
#if defined (__linux__) && !defined(SCHED_DEADLINE) && defined(HAVE_SCHED_SETATTR)
# define SCHED_DEADLINE 6
#endif

#endif /* UTIL_LINUX_SCHED_ATTR_H */
