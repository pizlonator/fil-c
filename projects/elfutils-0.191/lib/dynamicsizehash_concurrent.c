/* Copyright (C) 2000-2019 Red Hat, Inc.
   This file is part of elfutils.
   Written by Srdan Milakovic <sm108@rice.edu>, 2019.
   Derived from Ulrich Drepper <drepper@redhat.com>, 2000.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <stdlib.h>
#include <system.h>
#include <pthread.h>

/* Before including this file the following macros must be defined:

   NAME      name of the hash table structure.
   TYPE      data type of the hash table entries
 */


static size_t
lookup (NAME *htab, HASHTYPE hval)
{
  /* First hash function: simply take the modulus but prevent zero.  Small values
      can skip the division, which helps performance when this is common.  */
  size_t idx = 1 + (hval < htab->size ? hval : hval % htab->size);

  HASHTYPE hash;

  hash = atomic_load_explicit(&htab->table[idx].hashval,
                              memory_order_acquire);
  if (hash == hval)
    return idx;
  else if (hash == 0)
    return 0;

  /* Second hash function as suggested in [Knuth].  */
  HASHTYPE second_hash = 1 + hval % (htab->size - 2);

  for(;;)
    {
      if (idx <= second_hash)
          idx = htab->size + idx - second_hash;
      else
          idx -= second_hash;

      hash = atomic_load_explicit(&htab->table[idx].hashval,
                                  memory_order_acquire);
      if (hash == hval)
	return idx;
      else if (hash == 0)
	return 0;
    }
}

static int
insert_helper (NAME *htab, HASHTYPE hval, TYPE val)
{
  /* First hash function: simply take the modulus but prevent zero.  Small values
      can skip the division, which helps performance when this is common.  */
  size_t idx = 1 + (hval < htab->size ? hval : hval % htab->size);

  TYPE val_ptr;
  HASHTYPE hash;

  hash = atomic_load_explicit(&htab->table[idx].hashval,
                              memory_order_acquire);
  if (hash == hval)
    return -1;
  else if (hash == 0)
    {
      val_ptr = NULL;
      atomic_compare_exchange_strong_explicit(&htab->table[idx].val_ptr,
                                              (uintptr_t *) &val_ptr,
                                              (uintptr_t) val,
                                              memory_order_acquire,
                                              memory_order_acquire);

      if (val_ptr == NULL)
        {
          atomic_store_explicit(&htab->table[idx].hashval, hval,
                                memory_order_release);
          return 0;
        }
      else
        {
          do
            {
              hash = atomic_load_explicit(&htab->table[idx].hashval,
                                          memory_order_acquire);
            }
          while (hash == 0);
          if (hash == hval)
            return -1;
        }
    }

  /* Second hash function as suggested in [Knuth].  */
  HASHTYPE second_hash = 1 + hval % (htab->size - 2);

  for(;;)
    {
      if (idx <= second_hash)
          idx = htab->size + idx - second_hash;
      else
          idx -= second_hash;

      hash = atomic_load_explicit(&htab->table[idx].hashval,
                                  memory_order_acquire);
      if (hash == hval)
        return -1;
      else if (hash == 0)
        {
          val_ptr = NULL;
          atomic_compare_exchange_strong_explicit(&htab->table[idx].val_ptr,
                                                  (uintptr_t *) &val_ptr,
                                                  (uintptr_t) val,
                                                  memory_order_acquire,
                                                  memory_order_acquire);

          if (val_ptr == NULL)
            {
              atomic_store_explicit(&htab->table[idx].hashval, hval,
                                    memory_order_release);
              return 0;
            }
          else
            {
              do
                {
                  hash = atomic_load_explicit(&htab->table[idx].hashval,
                                              memory_order_acquire);
                }
              while (hash == 0);
              if (hash == hval)
                return -1;
            }
        }
    }
}

#define NO_RESIZING 0u
#define ALLOCATING_MEMORY 1u
#define MOVING_DATA 3u
#define CLEANING 2u

#define STATE_BITS 2u
#define STATE_INCREMENT (1u << STATE_BITS)
#define STATE_MASK (STATE_INCREMENT - 1)
#define GET_STATE(A) ((A) & STATE_MASK)

#define IS_NO_RESIZE_OR_CLEANING(A) (((A) & 0x1u) == 0)

#define GET_ACTIVE_WORKERS(A) ((A) >> STATE_BITS)

#define INITIALIZATION_BLOCK_SIZE 256
#define MOVE_BLOCK_SIZE 256
#define CEIL(A, B) (((A) + (B) - 1) / (B))

/* Initializes records and copies the data from the old table.
   It can share work with other threads.  Only the coordinator
   will pass blocking as 1, other worker threads pass 0.  */
static void resize_helper(NAME *htab, int blocking)
{
  size_t num_old_blocks = CEIL(htab->old_size, MOVE_BLOCK_SIZE);
  size_t num_new_blocks = CEIL(htab->size, INITIALIZATION_BLOCK_SIZE);

  size_t my_block;
  size_t num_finished_blocks = 0;

  while ((my_block = atomic_fetch_add_explicit(&htab->next_init_block, 1,
                                                memory_order_acquire))
                                                    < num_new_blocks)
    {
      size_t record_it = my_block * INITIALIZATION_BLOCK_SIZE;
      size_t record_end = (my_block + 1) * INITIALIZATION_BLOCK_SIZE;
      if (record_end > htab->size)
          record_end = htab->size;

      while (record_it++ != record_end)
        {
          atomic_init(&htab->table[record_it].hashval, (uintptr_t) NULL);
          atomic_init(&htab->table[record_it].val_ptr, (uintptr_t) NULL);
        }

      num_finished_blocks++;
    }

  atomic_fetch_add_explicit(&htab->num_initialized_blocks,
                            num_finished_blocks, memory_order_release);
  while (atomic_load_explicit(&htab->num_initialized_blocks,
                              memory_order_acquire) != num_new_blocks);

  /* All block are initialized, start moving */
  num_finished_blocks = 0;
  while ((my_block = atomic_fetch_add_explicit(&htab->next_move_block, 1,
                                                memory_order_acquire))
                                                    < num_old_blocks)
    {
      size_t record_it = my_block * MOVE_BLOCK_SIZE;
      size_t record_end = (my_block + 1) * MOVE_BLOCK_SIZE;
      if (record_end > htab->old_size)
          record_end = htab->old_size;

      while (record_it++ != record_end)
        {
          TYPE val_ptr = (TYPE) atomic_load_explicit(
              &htab->old_table[record_it].val_ptr,
              memory_order_acquire);
          if (val_ptr == NULL)
              continue;

          HASHTYPE hashval = atomic_load_explicit(
              &htab->old_table[record_it].hashval,
              memory_order_acquire);
          assert(hashval);

          insert_helper(htab, hashval, val_ptr);
        }

      num_finished_blocks++;
    }

  atomic_fetch_add_explicit(&htab->num_moved_blocks, num_finished_blocks,
                            memory_order_release);

  /* The coordinating thread will block here waiting for all blocks to
     be moved.  */
  if (blocking)
      while (atomic_load_explicit(&htab->num_moved_blocks,
                                  memory_order_acquire) != num_old_blocks);
}

/* Called by the main thread holding the htab->resize_rwl lock to
   coordinate the moving of hash table data. Allocates the new hash
   table and frees the old one when moving all data is done.  */
static void
resize_coordinator(NAME *htab)
{
  htab->old_size = htab->size;
  htab->old_table = htab->table;

  htab->size = next_prime(htab->size * 2);
  htab->table = malloc((1 + htab->size) * sizeof(htab->table[0]));
  assert(htab->table);

  /* Change state from ALLOCATING_MEMORY to MOVING_DATA */
  atomic_fetch_xor_explicit(&htab->resizing_state,
                            ALLOCATING_MEMORY ^ MOVING_DATA,
                            memory_order_release);

  resize_helper(htab, 1);

  /* Change state from MOVING_DATA to CLEANING */
  size_t resize_state = atomic_fetch_xor_explicit(&htab->resizing_state,
                                                  MOVING_DATA ^ CLEANING,
                                                  memory_order_acq_rel);
  while (GET_ACTIVE_WORKERS(resize_state) != 0)
      resize_state = atomic_load_explicit(&htab->resizing_state,
                                          memory_order_acquire);

  /* There are no more active workers */
  atomic_store_explicit(&htab->next_init_block, 0, memory_order_relaxed);
  atomic_store_explicit(&htab->num_initialized_blocks, 0,
                        memory_order_relaxed);

  atomic_store_explicit(&htab->next_move_block, 0, memory_order_relaxed);
  atomic_store_explicit(&htab->num_moved_blocks, 0, memory_order_relaxed);

  free(htab->old_table);

  /* Change state to NO_RESIZING */
  atomic_fetch_xor_explicit(&htab->resizing_state, CLEANING ^ NO_RESIZING,
                            memory_order_relaxed);

}

/* Called by any thread that wants to do an insert or find operation
   but notices it cannot get the htab->resize_rwl lock because another
   thread is resizing the hash table.  Try to help out by moving table
   data if still necessary.  */
static void
resize_worker(NAME *htab)
{
  size_t resize_state = atomic_load_explicit(&htab->resizing_state,
                                              memory_order_acquire);

  /* If the resize has finished */
  if (IS_NO_RESIZE_OR_CLEANING(resize_state))
      return;

  /* Register as worker and check if the resize has finished in the meantime*/
  resize_state = atomic_fetch_add_explicit(&htab->resizing_state,
                                            STATE_INCREMENT,
                                            memory_order_acquire);
  if (IS_NO_RESIZE_OR_CLEANING(resize_state))
    {
      atomic_fetch_sub_explicit(&htab->resizing_state, STATE_INCREMENT,
                                memory_order_relaxed);
      return;
    }

  /* Wait while the new table is being allocated. */
  while (GET_STATE(resize_state) == ALLOCATING_MEMORY)
      resize_state = atomic_load_explicit(&htab->resizing_state,
                                          memory_order_acquire);

  /* Check if the resize is done */
  assert(GET_STATE(resize_state) != NO_RESIZING);
  if (GET_STATE(resize_state) == CLEANING)
    {
      atomic_fetch_sub_explicit(&htab->resizing_state, STATE_INCREMENT,
                                memory_order_relaxed);
      return;
    }

  resize_helper(htab, 0);

  /* Deregister worker */
  atomic_fetch_sub_explicit(&htab->resizing_state, STATE_INCREMENT,
                            memory_order_release);
}


int
#define INIT(name) _INIT (name)
#define _INIT(name) \
  name##_init
INIT(NAME) (NAME *htab, size_t init_size)
{
  /* We need the size to be a prime.  */
  init_size = next_prime (init_size);

  /* Initialize the data structure.  */
  htab->size = init_size;
  atomic_init(&htab->filled, 0);
  atomic_init(&htab->resizing_state, 0);

  atomic_init(&htab->next_init_block, 0);
  atomic_init(&htab->num_initialized_blocks, 0);

  atomic_init(&htab->next_move_block, 0);
  atomic_init(&htab->num_moved_blocks, 0);

  pthread_rwlock_init(&htab->resize_rwl, NULL);

  htab->table = malloc ((init_size + 1) * sizeof (htab->table[0]));
  if (htab->table == NULL)
      return -1;

  for (size_t i = 0; i <= init_size; i++)
    {
      atomic_init(&htab->table[i].hashval, (uintptr_t) NULL);
      atomic_init(&htab->table[i].val_ptr, (uintptr_t) NULL);
    }

  return 0;
}


int
#define FREE(name) _FREE (name)
#define _FREE(name) \
name##_free
FREE(NAME) (NAME *htab)
{
  pthread_rwlock_destroy(&htab->resize_rwl);
  free (htab->table);
  return 0;
}


int
#define INSERT(name) _INSERT (name)
#define _INSERT(name) \
name##_insert
INSERT(NAME) (NAME *htab, HASHTYPE hval, TYPE data)
{
  int incremented = 0;

  for(;;)
    {
      /* If we cannot get the resize_rwl lock someone is resizing
	 hash table, try to help out by moving table data.  */
      while (pthread_rwlock_tryrdlock(&htab->resize_rwl) != 0)
          resize_worker(htab);

      size_t filled;
      if (!incremented)
        {
          filled = atomic_fetch_add_explicit(&htab->filled, 1,
                                              memory_order_acquire);
          incremented = 1;
        }
      else
        {
          filled = atomic_load_explicit(&htab->filled,
                                        memory_order_acquire);
        }


      if (100 * filled > 90 * htab->size)
        {
          /* Table is filled more than 90%.  Resize the table.  */

          size_t resizing_state = atomic_load_explicit(&htab->resizing_state,
                                                        memory_order_acquire);
          if (resizing_state == 0 &&
              atomic_compare_exchange_strong_explicit(&htab->resizing_state,
                                                      &resizing_state,
                                                      ALLOCATING_MEMORY,
                                                      memory_order_acquire,
                                                      memory_order_acquire))
            {
              /* Main resizing thread, will coordinate moving data.  */
              pthread_rwlock_unlock(&htab->resize_rwl);

              pthread_rwlock_wrlock(&htab->resize_rwl);
              resize_coordinator(htab);
              pthread_rwlock_unlock(&htab->resize_rwl);

            }
          else
            {
              /* Worker thread, will help moving data.  */
              pthread_rwlock_unlock(&htab->resize_rwl);
              resize_worker(htab);
            }
        }
      else
        {
          /* Lock acquired, no need for resize*/
          break;
        }
    }

  int ret_val = insert_helper(htab, hval, data);
  if (ret_val == -1)
      atomic_fetch_sub_explicit(&htab->filled, 1, memory_order_relaxed);
  pthread_rwlock_unlock(&htab->resize_rwl);
  return ret_val;
}



TYPE
#define FIND(name) _FIND (name)
#define _FIND(name) \
  name##_find
FIND(NAME) (NAME *htab, HASHTYPE hval)
{
  /* If we cannot get the resize_rwl lock someone is resizing
     the hash table, try to help out by moving table data.  */
  while (pthread_rwlock_tryrdlock(&htab->resize_rwl) != 0)
    resize_worker(htab);

  size_t idx;

  /* Make the hash data nonzero.  */
  hval = hval ?: 1;
  idx = lookup(htab, hval);

  if (idx == 0)
    {
      pthread_rwlock_unlock(&htab->resize_rwl);
      return NULL;
    }

  /* get a copy before unlocking the lock */
  TYPE ret_val = (TYPE) atomic_load_explicit(&htab->table[idx].val_ptr,
                                             memory_order_relaxed);

  pthread_rwlock_unlock(&htab->resize_rwl);
  return ret_val;
}
