/*
 * Copyright © 2008 Ryan Lortie
 * Copyright © 2010 Codethink Limited
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gbitlock.h"

#include <glib/gmacros.h>
#include <glib/gmessages.h>
#include <glib/gatomic.h>
#include <glib/gslist.h>
#include <glib/gthread.h>
#include <glib/gslice.h>

#include "gthreadprivate.h"

#include <stdfil.h>

#ifdef G_BIT_LOCK_FORCE_FUTEX_EMULATION
#undef HAVE_FUTEX
#undef HAVE_FUTEX_TIME64
#endif

#ifndef HAVE_FUTEX
static GMutex g_futex_mutex;
static GSList *g_futex_address_list = NULL;
#endif

#if defined(HAVE_FUTEX) || defined(HAVE_FUTEX_TIME64)
/*
 * We have headers for futex(2) on the build machine.  This does not
 * imply that every system that ever runs the resulting glib will have
 * kernel support for futex, but you'd have to have a pretty old
 * kernel in order for that not to be the case.
 *
 * If anyone actually gets bit by this, please file a bug. :)
 */

/* < private >
 * g_futex_wait:
 * @address: a pointer to an integer
 * @value: the value that should be at @address
 *
 * Atomically checks that the value stored at @address is equal to
 * @value and then blocks.  If the value stored at @address is not
 * equal to @value then this function returns immediately.
 *
 * To unblock, call g_futex_wake() on @address.
 *
 * This call may spuriously unblock (for example, in response to the
 * process receiving a signal) but this is not guaranteed.  Unlike the
 * Linux system call of a similar name, there is no guarantee that a
 * waiting process will unblock due to a g_futex_wake() call in a
 * separate process.
 */
static void
g_futex_wait (const gint *address,
              gint        value)
{
  g_futex_simple (address, (gsize) FUTEX_WAIT_PRIVATE, (gsize) value, NULL);
}

/* < private >
 * g_futex_wake:
 * @address: a pointer to an integer
 *
 * Nominally, wakes one thread that is blocked in g_futex_wait() on
 * @address (if any thread is currently waiting).
 *
 * As mentioned in the documentation for g_futex_wait(), spurious
 * wakeups may occur.  As such, this call may result in more than one
 * thread being woken up.
 */
static void
g_futex_wake (const gint *address)
{
  g_futex_simple (address, (gsize) FUTEX_WAKE_PRIVATE, (gsize) 1, NULL);
}

#else

/* emulate futex(2) */
typedef struct
{
  const gint *address;
  gint ref_count;
  GCond wait_queue;
} WaitAddress;

static WaitAddress *
g_futex_find_address (const gint *address)
{
  GSList *node;

  for (node = g_futex_address_list; node; node = node->next)
    {
      WaitAddress *waiter = node->data;

      if (waiter->address == address)
        return waiter;
    }

  return NULL;
}

static void
g_futex_wait (const gint *address,
              gint        value)
{
  g_mutex_lock (&g_futex_mutex);
  if G_LIKELY (g_atomic_int_get (address) == value)
    {
      WaitAddress *waiter;

      if ((waiter = g_futex_find_address (address)) == NULL)
        {
          waiter = g_slice_new (WaitAddress);
          waiter->address = address;
          g_cond_init (&waiter->wait_queue);
          waiter->ref_count = 0;
          g_futex_address_list =
            g_slist_prepend (g_futex_address_list, waiter);
        }

      waiter->ref_count++;
      g_cond_wait (&waiter->wait_queue, &g_futex_mutex);

      if (!--waiter->ref_count)
        {
          g_futex_address_list =
            g_slist_remove (g_futex_address_list, waiter);
          g_cond_clear (&waiter->wait_queue);
          g_slice_free (WaitAddress, waiter);
        }
    }
  g_mutex_unlock (&g_futex_mutex);
}

static void
g_futex_wake (const gint *address)
{
  WaitAddress *waiter;

  /* need to lock here for two reasons:
   *   1) need to acquire/release lock to ensure waiter is not in
   *      the process of registering a wait
   *   2) need to -stay- locked until the end to ensure a wake()
   *      in another thread doesn't cause 'waiter' to stop existing
   */
  g_mutex_lock (&g_futex_mutex);
  if ((waiter = g_futex_find_address (address)))
    g_cond_signal (&waiter->wait_queue);
  g_mutex_unlock (&g_futex_mutex);
}
#endif

#define CONTENTION_CLASSES 11
static gint g_bit_lock_contended[CONTENTION_CLASSES];  /* (atomic) */

G_ALWAYS_INLINE static inline guint
bit_lock_contended_class (gpointer address)
{
  return ((gsize) address) % G_N_ELEMENTS (g_bit_lock_contended);
}

#if (defined (i386) || defined (__amd64__))
  #if G_GNUC_CHECK_VERSION(4, 5)
    #define USE_ASM_GOTO 1
  #endif
#endif

/**
 * g_bit_lock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Sets the indicated @lock_bit in @address.  If the bit is already
 * set, this call will block until g_bit_unlock() unsets the
 * corresponding bit.
 *
 * Attempting to lock on two different bits within the same integer is
 * not supported and will very probably cause deadlocks.
 *
 * The value of the bit that is set is (1u << @bit).  If @bit is not
 * between 0 and 31 then the result is undefined.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably. While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Since: 2.24
 **/
void
g_bit_lock (volatile gint *address,
            gint           lock_bit)
{
  gint *address_nonvolatile = (gint *) address;

#ifdef USE_ASM_GOTO
 retry:
  __asm__ volatile goto ("lock bts %1, (%0)\n"
                         "jc %l[contended]"
                         : /* no output */
                         : "r" (address), "r" (lock_bit)
                         : "cc", "memory"
                         : contended);
  return;

 contended:
  {
    guint mask = 1u << lock_bit;
    guint v;

    v = (guint) g_atomic_int_get (address_nonvolatile);
    if (v & mask)
      {
        guint class = bit_lock_contended_class (address_nonvolatile);

        g_atomic_int_add (&g_bit_lock_contended[class], +1);
        g_futex_wait (address_nonvolatile, v);
        g_atomic_int_add (&g_bit_lock_contended[class], -1);
      }
  }
  goto retry;
#else
  guint mask = 1u << lock_bit;
  guint v;

 retry:
  v = g_atomic_int_or (address_nonvolatile, mask);
  if (v & mask)
    /* already locked */
    {
      guint class = bit_lock_contended_class (address_nonvolatile);

      g_atomic_int_add (&g_bit_lock_contended[class], +1);
      g_futex_wait (address_nonvolatile, v);
      g_atomic_int_add (&g_bit_lock_contended[class], -1);

      goto retry;
    }
#endif
}

/**
 * g_bit_trylock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Sets the indicated @lock_bit in @address, returning %TRUE if
 * successful.  If the bit is already set, returns %FALSE immediately.
 *
 * Attempting to lock on two different bits within the same integer is
 * not supported.
 *
 * The value of the bit that is set is (1u << @bit).  If @bit is not
 * between 0 and 31 then the result is undefined.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably. While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Returns: %TRUE if the lock was acquired
 *
 * Since: 2.24
 **/
gboolean
g_bit_trylock (volatile gint *address,
               gint           lock_bit)
{
#ifdef USE_ASM_GOTO
  gboolean result;

  __asm__ volatile ("lock bts %2, (%1)\n"
                    "setnc %%al\n"
                    "movzx %%al, %0"
                    : "=r" (result)
                    : "r" (address), "r" (lock_bit)
                    : "cc", "memory");

  return result;
#else
  gint *address_nonvolatile = (gint *) address;
  guint mask = 1u << lock_bit;
  guint v;

  v = g_atomic_int_or (address_nonvolatile, mask);

  return ~v & mask;
#endif
}

/**
 * g_bit_unlock:
 * @address: a pointer to an integer
 * @lock_bit: a bit value between 0 and 31
 *
 * Clears the indicated @lock_bit in @address.  If another thread is
 * currently blocked in g_bit_lock() on this same bit then it will be
 * woken up.
 *
 * This function accesses @address atomically.  All other accesses to
 * @address must be atomic in order for this function to work
 * reliably. While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Since: 2.24
 **/
void
g_bit_unlock (volatile gint *address,
              gint           lock_bit)
{
  gint *address_nonvolatile = (gint *) address;

#ifdef USE_ASM_GOTO
  __asm__ volatile ("lock btr %1, (%0)"
                    : /* no output */
                    : "r" (address), "r" (lock_bit)
                    : "cc", "memory");
#else
  guint mask = 1u << lock_bit;

  g_atomic_int_and (address_nonvolatile, ~mask);
#endif

  /* Warning: unlocking may allow another thread to proceed and destroy the
   * memory that @address points to. We thus must not dereference it anymore.
   */

  {
    guint class = bit_lock_contended_class (address_nonvolatile);

    if (g_atomic_int_get (&g_bit_lock_contended[class]))
      g_futex_wake (address_nonvolatile);
  }
}


/* We emulate pointer-sized futex(2) because the kernel API only
 * supports integers.
 *
 * We assume that the 'interesting' part is always the lower order bits.
 * This assumption holds because pointer bitlocks are restricted to
 * using the low order bits of the pointer as the lock.
 *
 * On 32 bits, there is nothing to do since the pointer size is equal to
 * the integer size.  On little endian the lower-order bits don't move,
 * so do nothing.  Only on 64bit big endian do we need to do a bit of
 * pointer arithmetic: the low order bits are shifted by 4 bytes.  We
 * have a helper function that always does the right thing here.
 *
 * Since we always consider the low-order bits of the integer value, a
 * simple cast from (gsize) to (guint) always takes care of that.
 *
 * After that, pointer-sized futex becomes as simple as:
 *
 *   g_futex_wait (g_futex_int_address (address), (guint) value);
 *
 * and
 *
 *   g_futex_wake (g_futex_int_address (int_address));
 */
static const gint *
g_futex_int_address (const void *address)
{
  const gint *int_address = address;

  /* this implementation makes these (reasonable) assumptions: */
  G_STATIC_ASSERT (G_BYTE_ORDER == G_LITTLE_ENDIAN ||
      (G_BYTE_ORDER == G_BIG_ENDIAN &&
       sizeof (int) == 4 &&
       (sizeof (gpointer) == 4 || sizeof (gpointer) == 8)));

#if G_BYTE_ORDER == G_BIG_ENDIAN && GLIB_SIZEOF_VOID_P == 8
  int_address++;
#endif

  return int_address;
}

G_ALWAYS_INLINE static inline gpointer
pointer_bit_lock_mask_ptr (gpointer ptr, guint lock_bit, gboolean set, guintptr preserve_mask, gpointer preserve_ptr)
{
  guintptr x_ptr;
  guintptr x_preserve_ptr;
  guintptr lock_mask;

  x_ptr = (guintptr) ptr;

  if (preserve_mask != 0)
    {
      x_preserve_ptr = (guintptr) preserve_ptr;
      x_ptr = (x_preserve_ptr & preserve_mask) | (x_ptr & ~preserve_mask);
    }

  if (lock_bit == G_MAXUINT)
      return (gpointer) zmkptr (ptr, x_ptr);

  lock_mask = (guintptr) (1u << lock_bit);
  if (set)
    return (gpointer) zmkptr (ptr, (x_ptr | lock_mask));
  else
    return (gpointer) zmkptr (ptr, (x_ptr & ~lock_mask));
}

/**
 * g_pointer_bit_lock_and_get:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 * @out_ptr: (out) (optional): returns the set pointer atomically.
 *   This is the value after setting the lock, it thus always has the
 *   lock bit set, while previously @address had the lockbit unset.
 *   You may also use g_pointer_bit_lock_mask_ptr() to clear the lock bit.
 *
 * This is equivalent to g_bit_lock, but working on pointers (or other
 * pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * Since: 2.80
 **/
void
(g_pointer_bit_lock_and_get) (gpointer address,
                              guint lock_bit,
                              gpointer *out_ptr)
{
  guint class = bit_lock_contended_class (address);
  guintptr mask;
  gpointer v;

  g_return_if_fail (lock_bit < 32);

  mask = 1u << lock_bit;

retry:
  v = g_atomic_pointer_or ((gpointer *) address, mask);
  if ((guintptr) v & mask)
    /* already locked */
    {
      g_atomic_int_add (&g_bit_lock_contended[class], +1);
      g_futex_wait (g_futex_int_address (address), (guint) v);
      g_atomic_int_add (&g_bit_lock_contended[class], -1);
      goto retry;
    }

  if (out_ptr)
      *out_ptr = (gpointer) ((guintptr) v | mask);
}

/**
 * g_pointer_bit_lock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_lock, but working on pointers (or other
 * pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Since: 2.30
 **/
void
(g_pointer_bit_lock) (volatile void *address,
                      gint lock_bit)
{
  g_pointer_bit_lock_and_get ((gpointer *) address, (guint) lock_bit, NULL);
}

/**
 * g_pointer_bit_trylock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_trylock(), but working on pointers (or
 * other pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Returns: %TRUE if the lock was acquired
 *
 * Since: 2.30
 **/
gboolean
(g_pointer_bit_trylock) (volatile void *address,
                         gint           lock_bit)
{
  g_return_val_if_fail (lock_bit < 32, FALSE);

  {
    void *address_nonvolatile = (void *) address;
    gpointer *pointer_address = address_nonvolatile;
    gsize mask = 1u << lock_bit;
    gpointer v;

    g_return_val_if_fail (lock_bit < 32, FALSE);

    v = g_atomic_pointer_or (pointer_address, mask);

    return (~(gsize) v & mask) != 0;
  }
}

/**
 * g_pointer_bit_unlock:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 *
 * This is equivalent to g_bit_unlock, but working on pointers (or other
 * pointer-sized values).
 *
 * For portability reasons, you may only lock on the bottom 32 bits of
 * the pointer.
 *
 * While @address has a `volatile` qualifier, this is a historical
 * artifact and the argument passed to it should not be `volatile`.
 *
 * Since: 2.30
 **/
void
(g_pointer_bit_unlock) (volatile void *address,
                        gint           lock_bit)
{
  void *address_nonvolatile = (void *) address;

  g_return_if_fail (lock_bit < 32);

  {
#ifdef USE_ASM_GOTO
    __asm__ volatile ("lock btr %1, (%0)"
                      : /* no output */
                      : "r" (address), "r" ((gsize) lock_bit)
                      : "cc", "memory");
#else
    gpointer *pointer_address = address_nonvolatile;
    gsize mask = 1u << lock_bit;

    g_atomic_pointer_and (pointer_address, ~mask);
#endif

    /* Warning: unlocking may allow another thread to proceed and destroy the
     * memory that @address points to. We thus must not dereference it anymore.
     */

    {
      guint class = bit_lock_contended_class (address_nonvolatile);

      if (g_atomic_int_get (&g_bit_lock_contended[class]))
        g_futex_wake (g_futex_int_address (address_nonvolatile));
    }
  }
}

/**
 * g_pointer_bit_lock_mask_ptr:
 * @ptr: (nullable): the pointer to mask
 * @lock_bit: the bit to set/clear. If set to `G_MAXUINT`, the
 *   lockbit is taken from @preserve_ptr or @ptr (depending on @preserve_mask).
 * @set: whether to set (lock) the bit or unset (unlock). This
 *   has no effect, if @lock_bit is set to `G_MAXUINT`.
 * @preserve_mask: if non-zero, a bit-mask for @preserve_ptr. The
 *   @preserve_mask bits from @preserve_ptr are set in the result.
 *   Note that the @lock_bit bit will be always set according to @set,
 *   regardless of @preserve_mask and @preserve_ptr (unless @lock_bit is
 *   `G_MAXUINT`).
 * @preserve_ptr: (nullable): if @preserve_mask is non-zero, the bits
 *   from this pointer are set in the result.
 *
 * This mangles @ptr as g_pointer_bit_lock() and g_pointer_bit_unlock()
 * do.
 *
 * Returns: the mangled pointer.
 *
 * Since: 2.80
 **/
gpointer
g_pointer_bit_lock_mask_ptr (gpointer ptr, guint lock_bit, gboolean set, guintptr preserve_mask, gpointer preserve_ptr)
{
  g_return_val_if_fail (lock_bit < 32u || lock_bit == G_MAXUINT, ptr);

  return pointer_bit_lock_mask_ptr (ptr, lock_bit, set, preserve_mask, preserve_ptr);
}

/**
 * g_pointer_bit_unlock_and_set:
 * @address: (not nullable): a pointer to a #gpointer-sized value
 * @lock_bit: a bit value between 0 and 31
 * @ptr: the new pointer value to set
 * @preserve_mask: if non-zero, those bits of the current pointer in @address
 *   are preserved.
 *   Note that the @lock_bit bit will be always set according to @set,
 *   regardless of @preserve_mask and the currently set value in @address.
 *
 * This is equivalent to g_pointer_bit_unlock() and atomically setting
 * the pointer value.
 *
 * Note that the lock bit will be cleared from the pointer. If the unlocked
 * pointer that was set is not identical to @ptr, an assertion fails. In other
 * words, @ptr must have @lock_bit unset. This also means, you usually can
 * only use this on the lowest bits.
 *
 * Since: 2.80
 **/
void (g_pointer_bit_unlock_and_set) (void *address,
                                     guint lock_bit,
                                     gpointer ptr,
                                     guintptr preserve_mask)
{
  gpointer *pointer_address = address;
  guint class = bit_lock_contended_class (address);
  gpointer ptr2;

  g_return_if_fail (lock_bit < 32u);

  if (preserve_mask != 0)
    {
      gpointer old_ptr = g_atomic_pointer_get ((gpointer *) address);

    again:
      ptr2 = pointer_bit_lock_mask_ptr (ptr, lock_bit, FALSE, preserve_mask, old_ptr);
      if (!g_atomic_pointer_compare_and_exchange_full (pointer_address, old_ptr, ptr2, &old_ptr))
        goto again;
    }
  else
    {
      ptr2 = pointer_bit_lock_mask_ptr (ptr, lock_bit, FALSE, 0, NULL);
      g_atomic_pointer_set (pointer_address, ptr2);
    }

  if (g_atomic_int_get (&g_bit_lock_contended[class]) > 0)
    g_futex_wake (g_futex_int_address (address));

  /* It makes no sense, if unlocking mangles the pointer. Assert against
   * that.
   *
   * Note that based on @preserve_mask, the pointer also gets mangled, which
   * can make sense for the caller. We don't assert for that. */
  g_return_if_fail (ptr == pointer_bit_lock_mask_ptr (ptr, lock_bit, FALSE, 0, NULL));
}
