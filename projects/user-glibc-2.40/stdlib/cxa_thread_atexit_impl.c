/* Register destructors for C++ TLS variables declared with thread_local.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

/* CONCURRENCY NOTES:

   This documents concurrency for the non-POD TLS destructor registration,
   calling and destruction.  The functions __cxa_thread_atexit_impl,
   _dl_close_worker and __call_tls_dtors are the three main routines that may
   run concurrently and access shared data.  The shared data in all possible
   combinations of all three functions are the link map list, a link map for a
   DSO and the link map member l_tls_dtor_count.

   __cxa_thread_atexit_impl acquires the dl_load_lock before accessing any
   shared state and hence multiple of its instances can safely execute
   concurrently.

   _dl_close_worker acquires the dl_load_lock before accessing any shared state
   as well and hence can concurrently execute multiple of its own instances as
   well as those of __cxa_thread_atexit_impl safely.  Not all accesses to
   l_tls_dtor_count are protected by the dl_load_lock, so we need to
   synchronize using atomics.

   __call_tls_dtors accesses the l_tls_dtor_count without taking the lock; it
   decrements the value by one.  It does not need the big lock because it does
   not access any other shared state except for the current DSO link map and
   its member l_tls_dtor_count.

   Correspondingly, _dl_close_worker loads l_tls_dtor_count and if it is zero,
   unloads the DSO, thus deallocating the current link map.  This is the goal
   of maintaining l_tls_dtor_count - to unload the DSO and free resources if
   there are no pending destructors to be called.

   We want to eliminate the inconsistent state where the DSO is unloaded in
   _dl_close_worker before it is used in __call_tls_dtors.  This could happen
   if __call_tls_dtors uses the link map after it sets l_tls_dtor_count to 0,
   since _dl_close_worker will conclude from the 0 l_tls_dtor_count value that
   it is safe to unload the DSO.  Hence, to ensure that this does not happen,
   the following conditions must be met:

   1. In _dl_close_worker, the l_tls_dtor_count load happens before the DSO is
      unloaded and its link map is freed
   2. The link map dereference in __call_tls_dtors happens before the
      l_tls_dtor_count dereference.

   To ensure this, the l_tls_dtor_count decrement in __call_tls_dtors should
   have release semantics and the load in _dl_close_worker should have acquire
   semantics.

   Concurrent executions of __call_tls_dtors should only ensure that the value
   is accessed atomically; no reordering constraints need to be considered.
   Likewise for the increment of l_tls_dtor_count in __cxa_thread_atexit_impl.

   There is still a possibility on concurrent execution of _dl_close_worker and
   __call_tls_dtors where _dl_close_worker reads the value of l_tls_dtor_count
   as 1, __call_tls_dtors decrements the value of l_tls_dtor_count but
   _dl_close_worker does not unload the DSO, having read the old value.  This
   is not very different from a case where __call_tls_dtors is called after
   _dl_close_worker on the DSO and hence is an accepted execution.  */

#include <stdio.h>
#include <stdlib.h>
#include <ldsodefs.h>
#include <pointer_guard.h>

void
__call_tls_dtors (void)
{
}
libc_hidden_def (__call_tls_dtors)
