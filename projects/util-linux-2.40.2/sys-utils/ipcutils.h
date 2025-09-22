/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_IPCUTILS_H
#define UTIL_LINUX_IPCUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>

/*
 * SHM_DEST and SHM_LOCKED are defined in kernel headers, but inside
 * #ifdef __KERNEL__ ... #endif
 */
#ifndef SHM_DEST
  /* shm_mode upper byte flags */
# define SHM_DEST	01000	/* segment will be destroyed on last detach */
# define SHM_LOCKED	02000	/* segment will not be swapped */
#endif

/* For older kernels the same holds for the defines below */
#ifndef MSG_STAT
# define MSG_STAT	11
# define MSG_INFO	12
#endif

#ifndef SHM_STAT
# define SHM_STAT	13
# define SHM_INFO	14
struct shm_info {
	int used_ids;
	unsigned long shm_tot;		/* total allocated shm */
	unsigned long shm_rss;		/* total resident shm */
	unsigned long shm_swp;		/* total swapped shm */
	unsigned long swap_attempts;
	unsigned long swap_successes;
};
#endif

#ifndef SEM_STAT
# define SEM_STAT	18
# define SEM_INFO	19
#endif

/* Some versions of libc only define IPC_INFO when __USE_GNU is defined. */
#ifndef IPC_INFO
# define IPC_INFO	3
#endif

/*
 *  * The last arg of semctl is a union semun, but where is it defined? X/OPEN
 *   * tells us to define it ourselves, but until recently Linux include files
 *    * would also define it.
 *     */
#ifndef HAVE_UNION_SEMUN
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

/*
 * X/OPEN (Jan 1987) does not define fields key, seq in struct ipc_perm;
 *	glibc-1.09 has no support for sysv ipc.
 *	glibc 2 uses __key, __seq
 */
#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

/* Size printing in ipcs is using these. */
enum {
	IPC_UNIT_DEFAULT,
	IPC_UNIT_BYTES,
	IPC_UNIT_KB,
	IPC_UNIT_HUMAN
};

struct ipc_limits {
	uint64_t	shmmni;		/* max number of segments */
	uint64_t	shmmax;		/* max segment size */
	uint64_t	shmall;		/* max total shared memory */
	uint64_t	shmmin;		/* min segment size */

	int		semmni;		/* max number of arrays */
	int		semmsl;		/* max semaphores per array */
	int		semmns;		/* max semaphores system wide */
	int		semopm;		/* max ops per semop call */
	unsigned int	semvmx;		/* semaphore max value (constant) */

	int		msgmni;		/* max queues system wide */
	uint64_t	msgmax;		/* max size of message */
	int		msgmnb;		/* default max size of queue */
};

extern int ipc_msg_get_limits(struct ipc_limits *lim);
extern int ipc_sem_get_limits(struct ipc_limits *lim);
extern int ipc_shm_get_limits(struct ipc_limits *lim);

struct ipc_stat {
	int		id;
	key_t		key;
	uid_t		uid;    /* current uid */
	gid_t		gid;    /* current gid */
	uid_t		cuid;    /* creator uid */
	gid_t		cgid;    /* creator gid */
	unsigned int	mode;
};

extern void ipc_print_perms(FILE *f, struct ipc_stat *is);
extern void ipc_print_size(int unit, char *msg, uint64_t size, const char *end, int width);

/* See 'struct shmid_kernel' in kernel sources
 */
struct shm_data {
	struct ipc_stat	shm_perm;

	uint64_t	shm_nattch;
	uint64_t	shm_segsz;
	int64_t		shm_atim;	/* __kernel_time_t is signed long */
	int64_t		shm_dtim;
	int64_t		shm_ctim;
	pid_t		shm_cprid;
	pid_t		shm_lprid;
	uint64_t	shm_rss;
	uint64_t	shm_swp;

	struct shm_data  *next;
};

extern int ipc_shm_get_info(int id, struct shm_data **shmds);
extern void ipc_shm_free_info(struct shm_data *shmds);

/* See 'struct sem_array' in kernel sources
 */
struct sem_elem {
	int	semval;
	int	ncount;		/* processes waiting on increase semval */
	int	zcount;		/* processes waiting on semval set to zero */
	pid_t	pid;		/* process last executed semop(2) call */
};
struct sem_data {
	struct ipc_stat sem_perm;

	int64_t		sem_ctime;
	int64_t		sem_otime;
	uint64_t	sem_nsems;

	struct sem_elem	*elements;
	struct sem_data *next;
};

extern int ipc_sem_get_info(int id, struct sem_data **semds);
extern void ipc_sem_free_info(struct sem_data *semds);

/* See 'struct msg_queue' in kernel sources
 */
struct msg_data {
	struct ipc_stat msg_perm;

	int64_t		q_stime;
	int64_t		q_rtime;
	int64_t		q_ctime;
	uint64_t	q_cbytes;
	uint64_t	q_qnum;
	uint64_t	q_qbytes;
	pid_t		q_lspid;
	pid_t		q_lrpid;

	struct msg_data *next;
};

extern int ipc_msg_get_info(int id, struct msg_data **msgds);
extern void ipc_msg_free_info(struct msg_data *msgds);

#endif /* UTIL_LINUX_IPCUTILS_H */
