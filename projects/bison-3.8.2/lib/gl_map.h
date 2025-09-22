/* Abstract map data type.
   Copyright (C) 2006-2007, 2009-2021 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2018.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef _GL_MAP_H
#define _GL_MAP_H

#include <stdbool.h>
#include <stddef.h>

#ifndef _GL_INLINE_HEADER_BEGIN
 #error "Please include config.h first."
#endif
_GL_INLINE_HEADER_BEGIN
#ifndef GL_MAP_INLINE
# define GL_MAP_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* gl_map is an abstract map data type.  It can contain any number of
   (key, value) pairs, where
     - keys and values are objects ('void *' or 'const void *' pointers),
     - There are no (key, value1) and (key, value2) pairs with the same key
       (in the sense of a given comparator function).

   There are several implementations of this map datatype, optimized for
   different operations or for memory.  You can start using the simplest map
   implementation, GL_ARRAY_MAP, and switch to a different implementation
   later, when you realize which operations are performed the most frequently.
   The API of the different implementations is exactly the same; when switching
   to a different implementation, you only have to change the gl_map_create
   call.

   The implementations are:
     GL_ARRAY_MAP         a growable array
     GL_LINKEDHASH_MAP    a hash table with a linked list
     GL_HASH_MAP          a hash table

   The memory consumption is asymptotically the same: O(1) for every pair
   in the map.  When looking more closely at the average memory consumed
   for an object, GL_ARRAY_MAP is the most compact representation, then comes
   GL_HASH_MAP, and GL_LINKEDHASH_MAP needs the most memory.

   The guaranteed average performance of the operations is, for a map of
   n pairs:

   Operation                  ARRAY   LINKEDHASH
                                      HASH

   gl_map_size                 O(1)   O(1)
   gl_map_get                  O(n)   O(1)
   gl_map_put                  O(n)   O(1)
   gl_map_remove               O(n)   O(1)
   gl_map_search               O(n)   O(1)
   gl_map_iterator             O(1)   O(1)
   gl_map_iterator_next        O(1)   O(1)
 */

/* --------------------------- gl_map_t Data Type --------------------------- */

/* Type of function used to compare two keys.
   NULL denotes pointer comparison.  */
typedef bool (*gl_mapkey_equals_fn) (const void *key1, const void *key2);

/* Type of function used to compute a hash code.
   NULL denotes a function that depends only on the pointer itself.  */
typedef size_t (*gl_mapkey_hashcode_fn) (const void *key);

#ifndef _GL_MAP_DISPOSE_FNS_DEFINED

/* Type of function used to dispose a key once a (key, value) pair is removed
   from a map.  NULL denotes a no-op.  */
typedef void (*gl_mapkey_dispose_fn) (const void *key);

/* Type of function used to dispose a value once a (key, value) pair is removed
   from a map.  NULL denotes a no-op.  */
typedef void (*gl_mapvalue_dispose_fn) (const void *value);

# define _GL_MAP_DISPOSE_FNS_DEFINED 1
#endif

struct gl_map_impl;
/* Type representing an entire map.  */
typedef struct gl_map_impl * gl_map_t;

struct gl_map_implementation;
/* Type representing a map datatype implementation.  */
typedef const struct gl_map_implementation * gl_map_implementation_t;

#if 0 /* Unless otherwise specified, these are defined inline below.  */

/* Creates an empty map.
   IMPLEMENTATION is one of GL_ARRAY_MAP, GL_LINKEDHASH_MAP, GL_HASH_MAP.
   EQUALS_FN is a key comparison function or NULL.
   HASHCODE_FN is a key hash code function or NULL.
   KDISPOSE_FN is a key disposal function or NULL.
   VDISPOSE_FN is a value disposal function or NULL.  */
/* declared in gl_xmap.h */
extern gl_map_t gl_map_create_empty (gl_map_implementation_t implementation,
                                     gl_mapkey_equals_fn equals_fn,
                                     gl_mapkey_hashcode_fn hashcode_fn,
                                     gl_mapkey_dispose_fn kdispose_fn,
                                     gl_mapvalue_dispose_fn vdispose_fn)
  /*_GL_ATTRIBUTE_DEALLOC (gl_map_free, 1)*/
  _GL_ATTRIBUTE_RETURNS_NONNULL;
/* Likewise.  Returns NULL upon out-of-memory.  */
extern gl_map_t gl_map_nx_create_empty (gl_map_implementation_t implementation,
                                        gl_mapkey_equals_fn equals_fn,
                                        gl_mapkey_hashcode_fn hashcode_fn,
                                        gl_mapkey_dispose_fn kdispose_fn,
                                        gl_mapvalue_dispose_fn vdispose_fn)
  /*_GL_ATTRIBUTE_DEALLOC (gl_map_free, 1)*/;

/* Returns the current number of pairs in a map.  */
extern size_t gl_map_size (gl_map_t map);

/* Searches whether a pair with the given key is already in the map.
   Returns the value if found, or NULL if not present in the map.  */
extern const void * gl_map_get (gl_map_t map, const void *key);

/* Searches whether a pair with the given key is already in the map.
   Returns true and sets *VALUEP to the value if found.
   Returns false if not present in the map.  */
extern bool gl_map_search (gl_map_t map, const void *key, const void **valuep);

/* Adds a pair to a map.
   Returns true if a pair with the given key was not already in the map and so
   this pair was added.
   Returns false if a pair with the given key was already in the map and only
   its value was replaced.  */
/* declared in gl_xmap.h */
extern bool gl_map_put (gl_map_t map, const void *key, const void *value);
/* Likewise.  Returns -1 upon out-of-memory.  */
_GL_ATTRIBUTE_NODISCARD
extern int gl_map_nx_put (gl_map_t map, const void *key, const void *value);

/* Adds a pair to a map and retrieves the previous value.
   Returns true if a pair with the given key was not already in the map and so
   this pair was added.
   Returns false and sets *OLDVALUEP to the previous value, if a pair with the
   given key was already in the map and only its value was replaced.  */
/* declared in gl_xmap.h */
extern bool gl_map_getput (gl_map_t map, const void *key, const void *value,
                           const void **oldvaluep);
/* Likewise.  Returns -1 upon out-of-memory.  */
_GL_ATTRIBUTE_NODISCARD
extern int gl_map_nx_getput (gl_map_t map, const void *key, const void *value,
                             const void **oldvaluep);

/* Removes a pair from a map.
   Returns true if the key was found and its pair removed.
   Returns false otherwise.  */
extern bool gl_map_remove (gl_map_t map, const void *key);

/* Removes a pair from a map and retrieves the previous value.
   Returns true and sets *OLDVALUEP to the previous value, if the key was found
   and its pair removed.
   Returns false otherwise.  */
extern bool gl_map_getremove (gl_map_t map, const void *key,
                              const void **oldvaluep);

/* Frees an entire map.
   (But this call does not free the keys and values of the pairs in the map.
   It only invokes the KDISPOSE_FN on each key and the VDISPOSE_FN on each value
   of the pairs in the map.)  */
extern void gl_map_free (gl_map_t map);

#endif /* End of inline and gl_xmap.h-defined functions.  */

/* ---------------------- gl_map_iterator_t Data Type ---------------------- */

/* Functions for iterating through a map.
   Note: Iterating through a map of type GL_HASH_MAP returns the pairs in an
   unpredictable order.  If you need a predictable order, use GL_LINKEDHASH_MAP
   instead of GL_HASH_MAP.  */

/* Type of an iterator that traverses a map.
   This is a fixed-size struct, so that creation of an iterator doesn't need
   memory allocation on the heap.  */
typedef struct
{
  /* For fast dispatch of gl_map_iterator_next.  */
  const struct gl_map_implementation *vtable;
  /* For detecting whether the last returned pair was removed.  */
  gl_map_t map;
  size_t count;
  /* Other, implementation-private fields.  */
  void *p; void *q;
  size_t i; size_t j;
} gl_map_iterator_t;

#if 0 /* These are defined inline below.  */

/* Creates an iterator traversing a map.
   The map's contents must not be modified while the iterator is in use,
   except for modifying the value of the last returned key or removing the
   last returned pair.  */
extern gl_map_iterator_t gl_map_iterator (gl_map_t map);

/* If there is a next pair, stores the next pair in *KEYP and *VALUEP, advances
   the iterator, and returns true.  Otherwise, returns false.  */
extern bool gl_map_iterator_next (gl_map_iterator_t *iterator,
                                  const void **keyp, const void **valuep);

/* Frees an iterator.  */
extern void gl_map_iterator_free (gl_map_iterator_t *iterator);

#endif /* End of inline functions.  */

/* ------------------------- Implementation Details ------------------------- */

struct gl_map_implementation
{
  /* gl_map_t functions.  */
  gl_map_t (*nx_create_empty) (gl_map_implementation_t implementation,
                               gl_mapkey_equals_fn equals_fn,
                               gl_mapkey_hashcode_fn hashcode_fn,
                               gl_mapkey_dispose_fn kdispose_fn,
                               gl_mapvalue_dispose_fn vdispose_fn);
  size_t (*size) (gl_map_t map);
  bool (*search) (gl_map_t map, const void *key, const void **valuep);
  int (*nx_getput) (gl_map_t map, const void *key, const void *value,
                    const void **oldvaluep);
  bool (*getremove) (gl_map_t map, const void *key, const void **oldvaluep);
  void (*map_free) (gl_map_t map);
  /* gl_map_iterator_t functions.  */
  gl_map_iterator_t (*iterator) (gl_map_t map);
  bool (*iterator_next) (gl_map_iterator_t *iterator,
                         const void **keyp, const void **valuep);
  void (*iterator_free) (gl_map_iterator_t *iterator);
};

struct gl_map_impl_base
{
  const struct gl_map_implementation *vtable;
  gl_mapkey_equals_fn equals_fn;
  gl_mapkey_dispose_fn kdispose_fn;
  gl_mapvalue_dispose_fn vdispose_fn;
};

/* Define most functions of this file as accesses to the
   struct gl_map_implementation.  */

GL_MAP_INLINE
/*_GL_ATTRIBUTE_DEALLOC (gl_map_free, 1)*/
gl_map_t
gl_map_nx_create_empty (gl_map_implementation_t implementation,
                        gl_mapkey_equals_fn equals_fn,
                        gl_mapkey_hashcode_fn hashcode_fn,
                        gl_mapkey_dispose_fn kdispose_fn,
                        gl_mapvalue_dispose_fn vdispose_fn)
{
  return implementation->nx_create_empty (implementation,
                                          equals_fn, hashcode_fn,
                                          kdispose_fn, vdispose_fn);
}

GL_MAP_INLINE size_t
gl_map_size (gl_map_t map)
{
  return ((const struct gl_map_impl_base *) map)->vtable->size (map);
}

GL_MAP_INLINE bool
gl_map_search (gl_map_t map, const void *key, const void **valuep)
{
  return ((const struct gl_map_impl_base *) map)->vtable
         ->search (map, key, valuep);
}

_GL_ATTRIBUTE_NODISCARD GL_MAP_INLINE int
gl_map_nx_getput (gl_map_t map, const void *key, const void *value,
                   const void **oldvaluep)
{
  return ((const struct gl_map_impl_base *) map)->vtable
         ->nx_getput (map, key, value, oldvaluep);
}

GL_MAP_INLINE bool
gl_map_getremove (gl_map_t map, const void *key, const void **oldvaluep)
{
  return ((const struct gl_map_impl_base *) map)->vtable
         ->getremove (map, key, oldvaluep);
}

GL_MAP_INLINE void
gl_map_free (gl_map_t map)
{
  ((const struct gl_map_impl_base *) map)->vtable->map_free (map);
}

GL_MAP_INLINE gl_map_iterator_t
gl_map_iterator (gl_map_t map)
{
  return ((const struct gl_map_impl_base *) map)->vtable->iterator (map);
}

GL_MAP_INLINE bool
gl_map_iterator_next (gl_map_iterator_t *iterator,
                      const void **keyp, const void **valuep)
{
  return iterator->vtable->iterator_next (iterator, keyp, valuep);
}

GL_MAP_INLINE void
gl_map_iterator_free (gl_map_iterator_t *iterator)
{
  iterator->vtable->iterator_free (iterator);
}

/* Define the convenience functions, that is, the functions that are independent
   of the implementation.  */

GL_MAP_INLINE const void *
gl_map_get (gl_map_t map, const void *key)
{
  const void *value = NULL;
  gl_map_search (map, key, &value);
  return value;
}

_GL_ATTRIBUTE_NODISCARD GL_MAP_INLINE int
gl_map_nx_put (gl_map_t map, const void *key, const void *value)
{
  const void *oldvalue;
  int result = gl_map_nx_getput (map, key, value, &oldvalue);
  if (result == 0)
    {
      gl_mapvalue_dispose_fn vdispose_fn =
        ((const struct gl_map_impl_base *) map)->vdispose_fn;
      if (vdispose_fn != NULL)
        vdispose_fn (oldvalue);
    }
  return result;
}

GL_MAP_INLINE bool
gl_map_remove (gl_map_t map, const void *key)
{
  const void *oldvalue;
  bool result = gl_map_getremove (map, key, &oldvalue);
  if (result)
    {
      gl_mapvalue_dispose_fn vdispose_fn =
        ((const struct gl_map_impl_base *) map)->vdispose_fn;
      if (vdispose_fn != NULL)
        vdispose_fn (oldvalue);
    }
  return result;
}

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif /* _GL_MAP_H */
