/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
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
 * \file hash.h
 * A table managing OpenGL object IDs.
 */

#ifndef HASH_H
#define HASH_H

#include <stdbool.h>
#include <stdint.h>
#include "util/glheader.h"

#include "c11/threads.h"
#include "util/simple_mtx.h"
#include "util/sparse_array.h"
#include "util/u_idalloc.h"

/**
 * The not-really-hash-table data structure. It pretends to be a hash table,
 * but it uses util_idalloc to keep track of GL object IDs and
 * util_sparse_array for storing entries. Lookups only access the array.
 */
struct _mesa_HashTable {
   struct util_sparse_array array;
   /* Used when name reuse is enabled */
   struct util_idalloc id_alloc;
   simple_mtx_t Mutex;
   GLuint MaxKey;                        /**< highest key inserted so far */
   bool alloc_via_idalloc;
};

void
_mesa_InitHashTable(struct _mesa_HashTable *table);

void
_mesa_DeinitHashTable(struct _mesa_HashTable *table,
                      void (*free_callback)(void *data, void *userData),
                      void *userData);

void
_mesa_HashInsert(struct _mesa_HashTable *table, GLuint key, void *data);

void
_mesa_HashRemove(struct _mesa_HashTable *table, GLuint key);

void
_mesa_HashInsertLocked(struct _mesa_HashTable *table, GLuint key, void *data);

void
_mesa_HashRemoveLocked(struct _mesa_HashTable *table, GLuint key);

void
_mesa_HashWalk(struct _mesa_HashTable *table,
               void (*callback)(void *data, void *userData),
               void *userData);

void
_mesa_HashWalkLocked(struct _mesa_HashTable *table,
                     void (*callback)(void *data, void *userData),
                     void *userData);

GLuint
_mesa_HashFindFreeKeyBlock(struct _mesa_HashTable *table, GLuint numKeys);

bool
_mesa_HashFindFreeKeys(struct _mesa_HashTable *table, GLuint* keys,
                       GLuint numKeys);

void
_mesa_HashEnableNameReuse(struct _mesa_HashTable *table);

/* Inline functions. */

/**
 * Lock the hash table mutex.
 *
 * This function should be used when multiple objects need
 * to be looked up in the hash table, to avoid having to lock
 * and unlock the mutex each time.
 *
 * \param table the hash table.
 */
static inline void
_mesa_HashLockMutex(struct _mesa_HashTable *table)
{
   simple_mtx_lock(&table->Mutex);
}

/**
 * Unlock the hash table mutex.
 *
 * \param table the hash table.
 */
static inline void
_mesa_HashUnlockMutex(struct _mesa_HashTable *table)
{
   simple_mtx_unlock(&table->Mutex);
}

static inline void
_mesa_HashLockMaybeLocked(struct _mesa_HashTable *table, bool locked)
{
   if (!locked)
      _mesa_HashLockMutex(table);
}

static inline void
_mesa_HashUnlockMaybeLocked(struct _mesa_HashTable *table, bool locked)
{
   if (!locked)
      _mesa_HashUnlockMutex(table);
}

/**
 * Lookup an entry in the hash table without locking the mutex.
 *
 * The hash table mutex must be locked manually by calling
 * _mesa_HashLockMutex() before calling this function.
 *
 * \return pointer to user's data or NULL if key not in table
 */
static inline void *
_mesa_HashLookupLocked(struct _mesa_HashTable *table, GLuint key)
{
   assert(key);
   return *(void**)util_sparse_array_get(&table->array, key);
}

/**
 * Lookup an entry in the hash table.
 *
 * \return pointer to user's data or NULL if key not in table
 */
static inline void *
_mesa_HashLookup(struct _mesa_HashTable *table, GLuint key)
{
   _mesa_HashLockMutex(table);
   void *res = _mesa_HashLookupLocked(table, key);
   _mesa_HashUnlockMutex(table);
   return res;
}

static inline void *
_mesa_HashLookupMaybeLocked(struct _mesa_HashTable *table, GLuint key,
                            bool locked)
{
   if (locked)
      return _mesa_HashLookupLocked(table, key);
   else
      return _mesa_HashLookup(table, key);
}

#endif
