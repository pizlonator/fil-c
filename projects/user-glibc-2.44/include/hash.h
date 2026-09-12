/* hash - hashing table processing.
   Copyright (C) 1998-1999, 2001, 2003, 2009-2026 Free Software Foundation,
   Inc.
   Written by Jim Meyering <meyering@ascend.com>, 1998.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* A generic hash table package.  */

/* Make sure USE_OBSTACK is defined to 1 if you want the allocator to use
   obstacks instead of malloc, and recompile 'hash.c' with same setting.  */

#ifndef HASH_H_
# define HASH_H_

/* This file uses _GL_ATTRIBUTE_DEALLOC, _GL_ATTRIBUTE_DEPRECATED,
   _GL_ATTRIBUTE_MALLOC, _GL_ATTRIBUTE_NODISCARD, _GL_ATTRIBUTE_PURE,
   _GL_ATTRIBUTE_RETURNS_NONNULL.  */
#if !_LIBC && !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

# include <stdbool.h>
# include <stdio.h>

# ifdef _LIBC
#  define _GL_ATTRIBUTE_NODISCARD          __attribute_warn_unused_result__
#  define _GL_ATTRIBUTE_MALLOC             __attribute_malloc__
#  define _GL_ATTRIBUTE_DEALLOC(__a, __b)  __attr_dealloc (__a, __b)
#  define _GL_ATTRIBUTE_RETURNS_NONNULL    __returns_nonnull
#  define GNULIB_HASHCODE_STRING1          0
#  define hash_get_n_buckets               __hash_get_n_buckets
#  define hash_get_n_buckets_used          __hash_get_n_buckets_used
#  define hash_get_n_entries               __hash_get_n_entries
#  define hash_get_max_bucket_length       __hash_get_max_bucket_length
#  define hash_table_ok                    __hash_table_ok
#  define hash_print_statistics            __hash_print_statistics
#  define hash_lookup                      __hash_lookup
#  define hash_get_first                   __hash_get_first
#  define hash_get_next                    __hash_get_next
#  define hash_get_entries                 __hash_get_entries
#  define hash_do_for_each                 __hash_do_for_each
#  define hash_reset_tuning                __hash_reset_tuning
#  define hash_free                        __hash_free
#  define hash_initialize                  __hash_initialize
#  define hash_xinitialize                 __hash_xinitialize
#  define hash_clear                       __hash_clear
#  define hash_rehash                      __hash_rehash
#  define hash_insert                      __hash_insert
#  define hash_xinsert                     __hash_xinsert
#  define hash_insert_if_absent            __hash_insert_if_absent
#  define hash_remove                      __hash_remove
# else
#  define attribute_hidden
# endif

# ifdef __cplusplus
extern "C" {
# endif

struct hash_tuning
  {
    /* This structure is mainly used for 'hash_initialize', see the block
       documentation of 'hash_reset_tuning' for more complete comments.  */

    float shrink_threshold;     /* ratio of used buckets to trigger a shrink */
    float shrink_factor;        /* ratio of new smaller size to original size */
    float growth_threshold;     /* ratio of used buckets to trigger a growth */
    float growth_factor;        /* ratio of new bigger size to original size */
    bool is_n_buckets;          /* if CANDIDATE really means table size */
  };

typedef struct hash_tuning Hash_tuning;

struct hash_table;

typedef struct hash_table Hash_table;

/*
 * Information and lookup.
 */

/* The following few functions provide information about the overall hash
   table organization: the number of entries, number of buckets and maximum
   length of buckets.  */

/* Return the number of buckets in the hash table.  The table size, the total
   number of buckets (used plus unused), or the maximum number of slots, are
   the same quantity.  */
extern size_t hash_get_n_buckets (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

/* Return the number of slots in use (non-empty buckets).  */
extern size_t hash_get_n_buckets_used (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

/* Return the number of active entries.  */
extern size_t hash_get_n_entries (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

/* Return the length of the longest chain (bucket).  */
extern size_t hash_get_max_bucket_length (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

/* Do a mild validation of a hash table, by traversing it and checking two
   statistics.  */
extern bool hash_table_ok (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

extern void hash_print_statistics (const Hash_table *table, FILE *stream)
       attribute_hidden;

/* If ENTRY matches an entry already in the hash table, return the
   entry from the table.  Otherwise, return NULL.  */
extern void *hash_lookup (const Hash_table *table, const void *entry)
       attribute_hidden;

/*
 * Walking.
 */

/* The functions in this page traverse the hash table and process the
   contained entries.  For the traversal to work properly, the hash table
   should not be resized nor modified while any particular entry is being
   processed.  In particular, entries should not be added, and an entry
   may be removed only if there is no shrink threshold and the entry being
   removed has already been passed to hash_get_next.  */

/* Return the first data in the table, or NULL if the table is empty.  */
extern void *hash_get_first (const Hash_table *table)
       _GL_ATTRIBUTE_PURE attribute_hidden;

/* Return the user data for the entry following ENTRY, where ENTRY has been
   returned by a previous call to either 'hash_get_first' or 'hash_get_next'.
   Return NULL if there are no more entries.  */
extern void *hash_get_next (const Hash_table *table, const void *entry)
       attribute_hidden;

/* Fill BUFFER with pointers to active user entries in the hash table, then
   return the number of pointers copied.  Do not copy more than BUFFER_SIZE
   pointers.  */
extern size_t hash_get_entries (const Hash_table *table, void **buffer,
                                size_t buffer_size)
       attribute_hidden;

typedef bool (*Hash_processor) (void *entry, void *processor_data);

/* Call a PROCESSOR function for each entry of a hash table, and return the
   number of entries for which the processor function returned success.  A
   pointer to some PROCESSOR_DATA which will be made available to each call to
   the processor function.  The PROCESSOR accepts two arguments: the first is
   the user entry being walked into, the second is the value of PROCESSOR_DATA
   as received.  The walking continue for as long as the PROCESSOR function
   returns nonzero.  When it returns zero, the walking is interrupted.  */
extern size_t hash_do_for_each (const Hash_table *table,
                                Hash_processor processor, void *processor_data)
       attribute_hidden;

/* Return a hash code of ENTRY, in the range 0..TABLE_SIZE-1.
   This hash code function must have the property that if the comparator of
   ENTRY1 and ENTRY2 returns true, the hasher returns the same value for ENTRY1
   and for ENTRY2.
   The hash code function typically computes an unsigned integer and at the end
   performs a % TABLE_SIZE modulo operation.  This modulo operation is performed
   as part of this hash code function, not by the caller, because in some cases
   the unsigned integer will be a 'size_t', in other cases an 'uintmax_t' or
   even larger.  */
typedef size_t (*Hash_hasher) (const void *entry, size_t table_size);

/* Compare two entries, ENTRY1 (being looked up or being inserted) and
   ENTRY2 (already in the table) for equality.  Return true for equal,
   false otherwise.  */
typedef bool (*Hash_comparator) (const void *entry1, const void *entry2);

/* This function is invoked when an ENTRY is removed from the hash table.  */
typedef void (*Hash_data_freer) (void *entry);

/*
 * Allocation and clean-up.
 */

extern void hash_reset_tuning (Hash_tuning *tuning)
       attribute_hidden;

/* Reclaim all storage associated with a hash table.  If a data_freer
   function has been supplied by the user when the hash table was created,
   this function applies it to the data of each entry before freeing that
   entry.  This function preserves errno, like 'free'.  */
extern void hash_free (Hash_table *table)
       attribute_hidden;

/* Allocate and return a new hash table, or NULL upon failure.  The initial
   number of buckets is automatically selected so as to _guarantee_ that you
   may insert at least CANDIDATE different user entries before any growth of
   the hash table size occurs.  So, if have a reasonably tight a-priori upper
   bound on the number of entries you intend to insert in the hash table, you
   may save some table memory and insertion time, by specifying it here.  If
   the IS_N_BUCKETS field of the TUNING structure is true, the CANDIDATE
   argument has its meaning changed to the wanted number of buckets.

   TUNING points to a structure of user-supplied values, in case some fine
   tuning is wanted over the default behavior of the hasher.  If TUNING is
   NULL, the default tuning parameters are used instead.  If TUNING is
   provided but the values requested are out of bounds or might cause
   rounding errors, return NULL.

   The user-supplied HASHER function, when not NULL, accepts two
   arguments ENTRY and TABLE_SIZE.  It computes, by hashing ENTRY contents, a
   slot number for that entry which should be in the range 0..TABLE_SIZE-1.
   This slot number is then returned.

   The user-supplied COMPARATOR function, when not NULL, accepts two
   arguments pointing to user data, it then returns true for a pair of entries
   that compare equal, or false otherwise.  This function is internally called
   on entries which are already known to hash to the same bucket index,
   but which are distinct pointers.

   The user-supplied DATA_FREER function, when not NULL, may be later called
   with the user data as an argument, just before the entry containing the
   data gets freed.  This happens from within 'hash_free' or 'hash_clear'.
   You should specify this function only if you want these functions to free
   all of your 'data' data.  This is typically the case when your data is
   simply an auxiliary struct that you have malloc'd to aggregate several
   values.

   Set errno on failure; otherwise errno is unspecified.  */
_GL_ATTRIBUTE_NODISCARD
extern Hash_table *hash_initialize (size_t candidate,
                                    const Hash_tuning *tuning,
                                    Hash_hasher hasher,
                                    Hash_comparator comparator,
                                    Hash_data_freer data_freer)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (hash_free, 1)
  attribute_hidden;

/* Like hash_initialize, but invokes xalloc_die instead of returning NULL.  */
/* This function is defined by module 'xhash'.  */
_GL_ATTRIBUTE_NODISCARD
extern Hash_table *hash_xinitialize (size_t candidate,
                                     const Hash_tuning *tuning,
                                     Hash_hasher hasher,
                                     Hash_comparator comparator,
                                     Hash_data_freer data_freer)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (hash_free, 1)
  _GL_ATTRIBUTE_RETURNS_NONNULL
  attribute_hidden;

/* Make all buckets empty, placing any chained entries on the free list.
   Apply the user-specified function data_freer (if any) to the data of any
   affected entries.  */
extern void hash_clear (Hash_table *table)
  attribute_hidden;;

/*
 * Insertion and deletion.
 */

/* For an already existing hash table, change the number of buckets through
   specifying CANDIDATE.  The contents of the hash table are preserved.  The
   new number of buckets is automatically selected so as to _guarantee_ that
   the table may receive at least CANDIDATE different user entries, including
   those already in the table, before any other growth of the hash table size
   occurs.  If TUNING->IS_N_BUCKETS is true, then CANDIDATE specifies the
   exact number of buckets desired.  Return true iff the rehash succeeded,
   false (setting errno) otherwise.  */
_GL_ATTRIBUTE_NODISCARD
extern bool hash_rehash (Hash_table *table, size_t candidate)
       attribute_hidden;

/* If ENTRY matches an entry already in the hash table, return the pointer
   to the entry from the table.  Otherwise, insert ENTRY and return ENTRY.
   Return NULL (setting errno) if the storage required for insertion
   cannot be allocated.  This implementation does not support
   duplicate entries or insertion of NULL.  */
_GL_ATTRIBUTE_NODISCARD
extern void *hash_insert (Hash_table *table, const void *entry)
       attribute_hidden;

/* Same as hash_insert, but invokes xalloc_die instead of returning NULL.  */
/* This function is defined by module 'xhash'.  */
extern void *hash_xinsert (Hash_table *table, const void *entry)
       attribute_hidden;

/* Insert ENTRY into hash TABLE if there is not already a matching entry.

   Return -1 (setting errno) upon memory allocation failure.
   Return 1 if insertion succeeded.
   Return 0 if there is already a matching entry in the table,
   and in that case, if MATCHED_ENT is non-NULL, set *MATCHED_ENT
   to that entry.

   This interface is easier to use than hash_insert when you must
   distinguish between the latter two cases.  More importantly,
   hash_insert is unusable for some types of ENTRY values.  When using
   hash_insert, the only way to distinguish those cases is to compare
   the return value and ENTRY.  That works only when you can have two
   different ENTRY values that point to data that compares "equal".  Thus,
   when the ENTRY value is a simple scalar, you must use
   hash_insert_if_absent.  ENTRY must not be NULL.  */
extern int hash_insert_if_absent (Hash_table *table, const void *entry,
                                  const void **matched_ent)
       attribute_hidden;

/* If ENTRY is already in the table, remove it and return the just-deleted
   data (the user may want to deallocate its storage).  If ENTRY is not in the
   table, don't modify the table and return NULL.  */
extern void *hash_remove (Hash_table *table, const void *entry)
       attribute_hidden;


# if GNULIB_HASHCODE_STRING1
/* Include declarations of module 'hashcode-string1'.  */
#  include "hashcode-string1.h"
# endif

# ifdef __cplusplus
}
# endif

#endif
