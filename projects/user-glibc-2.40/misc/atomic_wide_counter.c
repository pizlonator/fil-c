/* Monotonically increasing wide counters (at least 62 bits).
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#include <atomic_wide_counter.h>

#if !__HAVE_64B_ATOMICS

/* Values we add or xor are less than or equal to 1<<31, so we only
   have to make overflow-and-addition atomic wrt. to concurrent load
   operations and xor operations.  To do that, we split each counter
   into two 32b values of which we reserve the MSB of each to
   represent an overflow from the lower-order half to the higher-order
   half.

   In the common case, the state is (higher-order / lower-order half, and . is
   basically concatenation of the bits):
   0.h     / 0.l  = h.l

   When we add a value of x that overflows (i.e., 0.l + x == 1.L), we run the
   following steps S1-S4 (the values these represent are on the right-hand
   side):
   S1:  0.h     / 1.L == (h+1).L
   S2:  1.(h+1) / 1.L == (h+1).L
   S3:  1.(h+1) / 0.L == (h+1).L
   S4:  0.(h+1) / 0.L == (h+1).L
   If the LSB of the higher-order half is set, readers will ignore the
   overflow bit in the lower-order half.

   To get an atomic snapshot in load operations, we exploit that the
   higher-order half is monotonically increasing; if we load a value V from
   it, then read the lower-order half, and then read the higher-order half
   again and see the same value V, we know that both halves have existed in
   the sequence of values the full counter had.  This is similar to the
   validated reads in the time-based STMs in GCC's libitm (e.g.,
   method_ml_wt).

   One benefit of this scheme is that this makes load operations
   obstruction-free because unlike if we would just lock the counter, readers
   can almost always interpret a snapshot of each halves.  Readers can be
   forced to read a new snapshot when the read is concurrent with an overflow.
   However, overflows will happen infrequently, so load operations are
   practically lock-free.  */

uint64_t
__atomic_wide_counter_fetch_add_relaxed (__atomic_wide_counter *c,
                                         unsigned int op)
{
  /* S1. Note that this is an atomic read-modify-write so it extends the
     release sequence of release MO store at S3.  */
  unsigned int l = atomic_fetch_add_relaxed (&c->__value32.__low, op);
  unsigned int h = atomic_load_relaxed (&c->__value32.__high);
  uint64_t result = ((uint64_t) h << 31) | l;
  l += op;
  if ((l >> 31) > 0)
    {
      /* Overflow.  Need to increment higher-order half.  Note that all
         add operations are ordered in happens-before.  */
      h++;
      /* S2. Release MO to synchronize with the loads of the higher-order half
         in the load operation.  See __atomic_wide_counter_load_relaxed.  */
      atomic_store_release (&c->__value32.__high,
                            h | ((unsigned int) 1 << 31));
      l ^= (unsigned int) 1 << 31;
      /* S3.  See __atomic_wide_counter_load_relaxed.  */
      atomic_store_release (&c->__value32.__low, l);
      /* S4.  Likewise.  */
      atomic_store_release (&c->__value32.__high, h);
    }
  return result;
}

uint64_t
__atomic_wide_counter_load_relaxed (__atomic_wide_counter *c)
{
  unsigned int h, l, h2;
  do
    {
      /* This load and the second one below to the same location read from the
         stores in the overflow handling of the add operation or the
         initializing stores (which is a simple special case because
         initialization always completely happens before further use).
         Because no two stores to the higher-order half write the same value,
         the loop ensures that if we continue to use the snapshot, this load
         and the second one read from the same store operation.  All candidate
         store operations have release MO.
         If we read from S2 in the first load, then we will see the value of
         S1 on the next load (because we synchronize with S2), or a value
         later in modification order.  We correctly ignore the lower-half's
         overflow bit in this case.  If we read from S4, then we will see the
         value of S3 in the next load (or a later value), which does not have
         the overflow bit set anymore.
          */
      h = atomic_load_acquire (&c->__value32.__high);
      /* This will read from the release sequence of S3 (i.e, either the S3
         store or the read-modify-writes at S1 following S3 in modification
         order).  Thus, the read synchronizes with S3, and the following load
         of the higher-order half will read from the matching S2 (or a later
         value).
         Thus, if we read a lower-half value here that already overflowed and
         belongs to an increased higher-order half value, we will see the
         latter and h and h2 will not be equal.  */
      l = atomic_load_acquire (&c->__value32.__low);
      /* See above.  */
      h2 = atomic_load_relaxed (&c->__value32.__high);
    }
  while (h != h2);
  if (((l >> 31) > 0) && ((h >> 31) > 0))
    l ^= (unsigned int) 1 << 31;
  return ((uint64_t) (h & ~((unsigned int) 1 << 31)) << 31) + l;
}

#endif /* !__HAVE_64B_ATOMICS */
