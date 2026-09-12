/* Restartable Sequences internal API.  Linux implementation.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.

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

#ifndef RSEQ_INTERNAL_H
#define RSEQ_INTERNAL_H

#include <sysdep.h>
#include <errno.h>
#include <kernel-features.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/rseq.h>
#include <ldsodefs.h>
#include <thread_pointer.h>
#include <rseq-access.h>

/* rseq area registered with the kernel.  Use a custom definition here to
   isolate from the system provided header which could lack some fields of the
   Extended ABI.

   This is only used to get the field offsets and sizes, it should never be
   used for direct object allocations.

   Access to fields of the Extended ABI beyond the 20 bytes of the original ABI
   (after 'flags') must be gated by a check of the feature size.  */
struct rseq_area
{
  /* Original ABI.  */
  uint32_t cpu_id_start;
  uint32_t cpu_id;
  uint64_t rseq_cs;
  uint32_t flags;
  /* Extended ABI.  */
  uint32_t node_id;
  uint32_t mm_cid;
  /* Flexible array member to discourage direct object allocations.  */
  char end[];
};

/* Minimum size of the rseq area allocation required by the syscall.  The
   actually used rseq feature size may be less (20 bytes initially).  */
#define RSEQ_AREA_SIZE_INITIAL 32

/* Minimum used feature size of the rseq area.  */
#define RSEQ_AREA_SIZE_INITIAL_USED 20

/* Maximum currently used feature size of the rseq area.  */
#define RSEQ_AREA_SIZE_MAX_USED 28

/* Minimum alignment of the rseq area.  */
#define RSEQ_MIN_ALIGN 32

/* Alignment requirement of the rseq area.
   Populated from the auxiliary vector with a minimum of '32'.
   In .data.relro but not yet write-protected.  */
extern size_t _rseq_align attribute_hidden;

/* Size of the active features in the rseq area.
   Populated from the auxiliary vector with a minimum of '20'.
   Set to '0' on registration failure of the main thread.
   In .data.relro but not yet write-protected.  */
extern unsigned int _rseq_size attribute_hidden;

/* Offset from the thread pointer to the rseq area, always set to allow
   checking the registration status by reading the 'cpu_id' field.
   In .data.relro but not yet write-protected.  */
extern ptrdiff_t _rseq_offset attribute_hidden;

/* We want to use rtld_hidden_proto in order to call the internal aliases
   of __rseq_size and __rseq_offset from ld.so.  This avoids dynamic symbol
   binding at run time for both variables.  */
rtld_hidden_proto (__rseq_size)
rtld_hidden_proto (__rseq_offset)

/* Returns a pointer to the current thread rseq area.  */
static inline struct rseq_area *
RSEQ_SELF (void)
{
  return (struct rseq_area *) ((char *) __thread_pointer () + __rseq_offset);
}

#ifdef RSEQ_SIG
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  if (do_rseq)
    {
      unsigned int size =  __rseq_size;

      /* The feature size can be smaller than the minimum rseq area size of 32
         bytes accepted by the syscall, if this is the case, bump the size of
         the registration to the minimum.  The 'extra TLS' block is always at
         least 32 bytes. */
      if (size < RSEQ_AREA_SIZE_INITIAL)
        size = RSEQ_AREA_SIZE_INITIAL;

      /* Initialize the whole rseq area to zero prior to registration.  */
      memset (RSEQ_SELF (), 0, size);

      /* Set the cpu_id field to RSEQ_CPU_ID_UNINITIALIZED, this is checked by
         the kernel at registration when CONFIG_DEBUG_RSEQ is enabled.  */
      RSEQ_SETMEM (cpu_id, RSEQ_CPU_ID_UNINITIALIZED);

      int ret = INTERNAL_SYSCALL_CALL (rseq, RSEQ_SELF (), size, 0, RSEQ_SIG);
      if (!INTERNAL_SYSCALL_ERROR_P (ret))
        return true;
    }
  /* When rseq is disabled by tunables or the registration fails, inform
     userspace by setting 'cpu_id' to RSEQ_CPU_ID_REGISTRATION_FAILED.  */
  RSEQ_SETMEM (cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#else /* RSEQ_SIG */
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  RSEQ_SETMEM (cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#endif /* RSEQ_SIG */

#endif /* rseq-internal.h */
