/* Copyright (C) 2003-2024 Free Software Foundation, Inc.
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

/* Activate all C11 atomic builtins.

   Note:
   E.g. in nptl/pthread_key_delete.c if compiled with GCCs 6 and before,
   an extra stack-frame is generated and the old value is stored on stack
   before cs instruction but it never loads this value from stack.
   An unreleased GCC 7 omit those stack operations.

   E.g. in nptl/pthread_once.c the condition code of cs instruction is
   evaluated by a sequence of ipm, sra, compare and jump instructions instead
   of one conditional jump instruction.  This also occurs with an unreleased
   GCC 7.

   The atomic_fetch_abc_def C11 builtins are now using load-and-abc instructions
   on z196 zarch and higher cpus instead of a loop with compare-and-swap
   instruction.  */
#define USE_ATOMIC_COMPILER_BUILTINS 1

#ifdef __s390x__
# define __HAVE_64B_ATOMICS 1
#else
# define __HAVE_64B_ATOMICS 0
#endif

#define ATOMIC_EXCHANGE_USES_CAS 1

/* Implement some of the non-C11 atomic macros from include/atomic.h
   with help of the C11 atomic builtins.  The other non-C11 atomic macros
   are using the macros defined here.  */

/* Atomically store NEWVAL in *MEM if *MEM is equal to OLDVAL.
   Return the old *MEM value.  */
#define atomic_compare_and_exchange_val_acq(mem, newval, oldval)	\
  ({ __atomic_check_size((mem));					\
    typeof ((__typeof (*(mem))) *(mem)) __atg1_oldval = (oldval);	\
    __atomic_compare_exchange_n (mem, (void *) &__atg1_oldval,		\
				 newval, 1, __ATOMIC_ACQUIRE,		\
				 __ATOMIC_RELAXED);			\
    __atg1_oldval; })
#define atomic_compare_and_exchange_val_rel(mem, newval, oldval)	\
  ({ __atomic_check_size((mem));					\
    typeof ((__typeof (*(mem))) *(mem)) __atg1_2_oldval = (oldval);	\
    __atomic_compare_exchange_n (mem, (void *) &__atg1_2_oldval,	\
				 newval, 1, __ATOMIC_RELEASE,		\
				 __ATOMIC_RELAXED);			\
    __atg1_2_oldval; })

/* Atomically store NEWVAL in *MEM if *MEM is equal to OLDVAL.
   Return zero if *MEM was changed or non-zero if no exchange happened.  */
#define atomic_compare_and_exchange_bool_acq(mem, newval, oldval)	\
  ({ __atomic_check_size((mem));					\
    typeof ((__typeof (*(mem))) *(mem)) __atg2_oldval = (oldval);	\
    !__atomic_compare_exchange_n (mem, (void *) &__atg2_oldval, newval,	\
				  1, __ATOMIC_ACQUIRE,			\
				  __ATOMIC_RELAXED); })
#define catomic_compare_and_exchange_bool_acq(mem, newval, oldval)	\
  atomic_compare_and_exchange_bool_acq (mem, newval, oldval)

/* Store NEWVALUE in *MEM and return the old value.  */
#define atomic_exchange_acq(mem, newvalue)				\
  ({ __atomic_check_size((mem));					\
    __atomic_exchange_n (mem, newvalue, __ATOMIC_ACQUIRE); })
#define atomic_exchange_rel(mem, newvalue)				\
  ({ __atomic_check_size((mem));					\
    __atomic_exchange_n (mem, newvalue, __ATOMIC_RELEASE); })

/* Add VALUE to *MEM and return the old value of *MEM.  */
/* The gcc builtin uses load-and-add instruction on z196 zarch and higher cpus
   instead of a loop with compare-and-swap instruction.  */
# define atomic_exchange_and_add_acq(mem, operand)			\
  ({ __atomic_check_size((mem));					\
  __atomic_fetch_add ((mem), (operand), __ATOMIC_ACQUIRE); })
# define atomic_exchange_and_add_rel(mem, operand)			\
  ({ __atomic_check_size((mem));					\
  __atomic_fetch_add ((mem), (operand), __ATOMIC_RELEASE); })
#define catomic_exchange_and_add(mem, value)	\
  atomic_exchange_and_add (mem, value)

/* Atomically *mem |= mask and return the old value of *mem.  */
/* The gcc builtin uses load-and-or instruction on z196 zarch and higher cpus
   instead of a loop with compare-and-swap instruction.  */
#define atomic_or_val(mem, operand)					\
  ({ __atomic_check_size((mem));					\
  __atomic_fetch_or ((mem), (operand), __ATOMIC_ACQUIRE); })
/* Atomically *mem |= mask.  */
#define atomic_or(mem, mask)			\
  do {						\
    atomic_or_val (mem, mask);			\
  } while (0)
#define catomic_or(mem, mask)			\
  atomic_or (mem, mask)

/* Atomically *mem |= 1 << bit and return true if the bit was set in old value
   of *mem.  */
/* The load-and-or instruction is used on z196 zarch and higher cpus
   instead of a loop with compare-and-swap instruction.  */
#define atomic_bit_test_set(mem, bit)					\
  ({ __typeof (*(mem)) __atg14_old;					\
    __typeof (mem) __atg14_memp = (mem);				\
    __typeof (*(mem)) __atg14_mask = ((__typeof (*(mem))) 1 << (bit));	\
    __atg14_old = atomic_or_val (__atg14_memp, __atg14_mask);		\
    __atg14_old & __atg14_mask; })

/* Atomically *mem &= mask and return the old value of *mem.  */
/* The gcc builtin uses load-and-and instruction on z196 zarch and higher cpus
   instead of a loop with compare-and-swap instruction.  */
#define atomic_and_val(mem, operand)					\
  ({ __atomic_check_size((mem));					\
  __atomic_fetch_and ((mem), (operand), __ATOMIC_ACQUIRE); })
/* Atomically *mem &= mask.  */
#define atomic_and(mem, mask)			\
  do {						\
    atomic_and_val (mem, mask);			\
  } while (0)
#define catomic_and(mem, mask)			\
  atomic_and(mem, mask)
