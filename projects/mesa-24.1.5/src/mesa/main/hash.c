/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 * Copyright (C) 2024 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file hash.c
 * Generic hash table.
 *
 * Used for display lists, texture objects, vertex/fragment programs,
 * buffer objects, etc.  The hash functions are thread-safe.
 *
 * \note key=0 is illegal.
 *
 * \author Brian Paul
 */

#include "errors.h"
#include "util/glheader.h"
#include "hash.h"
#include "util/hash_table.h"
#include "util/u_memory.h"

/**
 * Initialize a hash table.
 */
void
_mesa_InitHashTable(struct _mesa_HashTable *table)
{
   memset(table, 0, sizeof(*table));
   util_sparse_array_init(&table->array, sizeof(void*), 1024);
   util_idalloc_init(&table->id_alloc, 8);
   /* Mark ID = 0 as used, so that we don't return it. */
   util_idalloc_reserve(&table->id_alloc, 0);
   simple_mtx_init(&table->Mutex, mtx_plain);
}

/**
 * Delete a hash table.
 * Frees each entry on the hash table and then the hash table structure itself.
 * Note that the caller should have already traversed the table and deleted
 * the objects in the table (i.e. We don't free the entries' data pointer).
 *
 * Invoke the given callback function for each table entry if not NULL.
 *
 * \param table the hash table to delete.
 * \param table  the hash table to delete
 * \param free_callback  the callback function
 * \param userData  arbitrary pointer to pass along to the callback
 *                  (this is typically a struct gl_context pointer)
 */
void
_mesa_DeinitHashTable(struct _mesa_HashTable *table,
                      void (*free_callback)(void *data, void *userData),
                      void *userData)
{
   if (free_callback) {
      util_idalloc_foreach_no_zero_safe(&table->id_alloc, id) {
         free_callback(*(void**)util_sparse_array_get(&table->array, id),
                       userData);
      }
   }

   util_idalloc_fini(&table->id_alloc);
   util_sparse_array_finish(&table->array);
   simple_mtx_destroy(&table->Mutex);
}

void
_mesa_HashEnableNameReuse(struct _mesa_HashTable *table)
{
   _mesa_HashLockMutex(table);
   table->alloc_via_idalloc = true;
   _mesa_HashUnlockMutex(table);
}

/**
 * Insert a key/pointer pair into the hash table without locking the mutex.
 * If an entry with this key already exists we'll replace the existing entry.
 *
 * The hash table mutex must be locked manually by calling
 * _mesa_HashLockMutex() before calling this function.
 *
 * \param table the hash table.
 * \param key the key (not zero).
 * \param data pointer to user data.
 */
void
_mesa_HashInsertLocked(struct _mesa_HashTable *table, GLuint key, void *data)
{
   assert(key);

   if (key > table->MaxKey)
      table->MaxKey = key;

   *(void**)util_sparse_array_get(&table->array, key) = data;

   util_idalloc_reserve(&table->id_alloc, key);
}

/**
 * Insert a key/pointer pair into the hash table.
 * If an entry with this key already exists we'll replace the existing entry.
 *
 * \param table the hash table.
 * \param key the key (not zero).
 * \param data pointer to user data.
 */
void
_mesa_HashInsert(struct _mesa_HashTable *table, GLuint key, void *data)
{
   _mesa_HashLockMutex(table);
   _mesa_HashInsertLocked(table, key, data);
   _mesa_HashUnlockMutex(table);
}

/**
 * Remove an entry from the hash table.
 * 
 * \param table the hash table.
 * \param key key of entry to remove.
 *
 * While holding the hash table's lock, searches the entry with the matching
 * key and unlinks it.
 */
void
_mesa_HashRemoveLocked(struct _mesa_HashTable *table, GLuint key)
{
   assert(key);
   *(void**)util_sparse_array_get(&table->array, key) = NULL;

   util_idalloc_free(&table->id_alloc, key);
}

void
_mesa_HashRemove(struct _mesa_HashTable *table, GLuint key)
{
   _mesa_HashLockMutex(table);
   _mesa_HashRemoveLocked(table, key);
   _mesa_HashUnlockMutex(table);
}

/**
 * Walk over all entries in a hash table, calling callback function for each.
 * \param table  the hash table to walk
 * \param callback  the callback function
 * \param userData  arbitrary pointer to pass along to the callback
 *                  (this is typically a struct gl_context pointer)
 */
void
_mesa_HashWalkLocked(struct _mesa_HashTable *table,
                     void (*callback)(void *data, void *userData),
                     void *userData)
{
   assert(callback);

   util_idalloc_foreach_no_zero_safe(&table->id_alloc, id) {
      callback(*(void**)util_sparse_array_get(&table->array, id), userData);
   }
}

void
_mesa_HashWalk(struct _mesa_HashTable *table,
               void (*callback)(void *data, void *userData),
               void *userData)
{
   _mesa_HashLockMutex(table);
   _mesa_HashWalkLocked(table, callback, userData);
   _mesa_HashUnlockMutex(table);
}

/**
 * Find a block of adjacent unused hash keys.
 * 
 * \param table the hash table.
 * \param numKeys number of keys needed.
 * 
 * \return Starting key of free block or 0 if failure.
 *
 * If there are enough free keys between the maximum key existing in the table
 * (_mesa_HashTable::MaxKey) and the maximum key possible, then simply return
 * the adjacent key. Otherwise do a full search for a free key block in the
 * allowable key range.
 */
GLuint
_mesa_HashFindFreeKeyBlock(struct _mesa_HashTable *table, GLuint numKeys)
{
   const GLuint maxKey = ~((GLuint) 0) - 1;
   if (table->alloc_via_idalloc) {
      return util_idalloc_alloc_range(&table->id_alloc, numKeys);
   } else if (maxKey - numKeys > table->MaxKey) {
      /* the quick solution */
      return table->MaxKey + 1;
   }
   else {
      /* the slow solution */
      GLuint freeCount = 0;
      GLuint freeStart = 1;
      GLuint key;
      for (key = 1; key != maxKey; key++) {
	 if (_mesa_HashLookupLocked(table, key)) {
	    /* darn, this key is already in use */
	    freeCount = 0;
	    freeStart = key+1;
	 }
	 else {
	    /* this key not in use, check if we've found enough */
	    freeCount++;
	    if (freeCount == numKeys) {
	       return freeStart;
	    }
	 }
      }
      /* cannot allocate a block of numKeys consecutive keys */
      return 0;
   }
}

bool
_mesa_HashFindFreeKeys(struct _mesa_HashTable *table, GLuint* keys, GLuint numKeys)
{
   if (!table->alloc_via_idalloc) {
      GLuint first = _mesa_HashFindFreeKeyBlock(table, numKeys);
      for (int i = 0; i < numKeys; i++) {
         keys[i] = first + i;
      }
      return first != 0;
   }

   for (int i = 0; i < numKeys; i++) {
      keys[i] = util_idalloc_alloc(&table->id_alloc);
   }

   return true;
}
