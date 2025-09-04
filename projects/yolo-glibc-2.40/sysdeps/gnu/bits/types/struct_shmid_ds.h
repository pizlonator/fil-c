/* Generic implementation of the shared memory struct shmid_ds for GNU.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
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

#ifndef _SYS_SHM_H
# error "Never include <bits/types/struct_shmid_ds.h> directly; use <sys/shm.h> instead."
#endif

#include <bits/types/time_t.h>

/* Data structure describing a shared memory segment.  */
struct shmid_ds
  {
    struct ipc_perm shm_perm;		/* operation permission struct */
    size_t shm_segsz;			/* size of segment in bytes */
    __time_t shm_atime;			/* time of last shmat() */
    __time_t shm_dtime;			/* time of last shmdt() */
    __time_t shm_ctime;			/* time of last change by shmctl() */
    __pid_t shm_cpid;			/* pid of creator */
    __pid_t shm_lpid;			/* pid of last shmop */
    shmatt_t shm_nattch;		/* number of current attaches */
    unsigned short int __shm_npages;	/* size of segment (pages) */
    unsigned long int *__shm_pages;	/* array of ptrs to frames -> SHMMAX */
    struct __vm_area_struct *__attaches;	/* descriptors for attaches */
  };
