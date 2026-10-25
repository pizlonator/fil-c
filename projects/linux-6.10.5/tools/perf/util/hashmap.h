/* SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause) */

/*
 * Generic non-thread safe hash map implementation.
 *
 * Copyright (c) 2019 Facebook
 */
#ifndef __LIBBPF_HASHMAP_H
#define __LIBBPF_HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

static inline size_t hash_bits(size_t h, int bits)
{
	/* shuffle bits and return requested number of upper bits */
	if (bits == 0)
		return 0;

#if (__SIZEOF_SIZE_T__ == __SIZEOF_LONG_LONG__)
	/* LP64 case */
	return (h * 11400714819323198485llu) >> (__SIZEOF_LONG_LONG__ * 8 - bits);
#elif (__SIZEOF_SIZE_T__ <= __SIZEOF_LONG__)
	return (h * 2654435769lu) >> (__SIZEOF_LONG__ * 8 - bits);
#else
#	error "Unsupported size_t size"
#endif
}

/* generic C-string hashing function */
static inline size_t str_hash(const char *s)
{
	size_t h = 0;

	while (*s) {
		h = h * 31 + *s;
		s++;
	}
	return h;
}

/* Fil-C: keys and values are stored as real pointers, not long-sized
 * integers.  The original implementation smuggled pointers through longs
 * (hashmap_insert(map, (long)key, (long)value), entry->value = ...), which
 * strips Fil-C pointer capabilities: the pointer that comes back out of
 * the map has no object and dereferencing (or freeing) it panics.  Users
 * that key by an integer (e.g. tids) must smuggle the integer through
 * uintptr_t themselves; such keys are only ever hashed and compared, so
 * their lost capabilities do not matter.  Integer keys and values that
 * are never dereferenced or freed can do the same. */
typedef size_t (*hashmap_hash_fn)(const void *key, void *ctx);
typedef bool (*hashmap_equal_fn)(const void *key1, const void *key2, void *ctx);

/*
 * Hashmap interface is polymorphic: interface functions that operate on
 * keys and values are hidden behind auxiliary macros, e.g.
 * hashmap_insert <-> hashmap__insert.  The macros pass keys and values
 * through unchanged (they must already be pointers, or casts to pointer
 * types at the call site) and only cast the out parameters (old_key,
 * old_value, value) to the internal pointer-to-pointer types.
 * When iterating using hashmap__for_each_* forms, hashmap_entry->key
 * (same as ->pkey) and hashmap_entry->value (same as ->pvalue) are
 * plain pointers.
 */
struct hashmap_entry {
	union {
		const void *key;
		const void *pkey;
	};
	union {
		void *value;
		void *pvalue;
	};
	struct hashmap_entry *next;
};

struct hashmap {
	hashmap_hash_fn hash_fn;
	hashmap_equal_fn equal_fn;
	void *ctx;

	struct hashmap_entry **buckets;
	size_t cap;
	size_t cap_bits;
	size_t sz;
};

void hashmap__init(struct hashmap *map, hashmap_hash_fn hash_fn,
		   hashmap_equal_fn equal_fn, void *ctx);
struct hashmap *hashmap__new(hashmap_hash_fn hash_fn,
			     hashmap_equal_fn equal_fn,
			     void *ctx);
void hashmap__clear(struct hashmap *map);
void hashmap__free(struct hashmap *map);

size_t hashmap__size(const struct hashmap *map);
size_t hashmap__capacity(const struct hashmap *map);

/*
 * Hashmap insertion strategy:
 * - HASHMAP_ADD - only add key/value if key doesn't exist yet;
 * - HASHMAP_SET - add key/value pair if key doesn't exist yet; otherwise,
 *   update value;
 * - HASHMAP_UPDATE - update value, if key already exists; otherwise, do
 *   nothing and return -ENOENT;
 * - HASHMAP_APPEND - always add key/value pair, even if key already exists.
 *   This turns hashmap into a multimap by allowing multiple values to be
 *   associated with the same key. Most useful read API for such hashmap is
 *   hashmap__for_each_key_entry() iteration. If hashmap__find() is still
 *   used, it will return last inserted key/value entry (first in a bucket
 *   chain).
 */
enum hashmap_insert_strategy {
	HASHMAP_ADD,
	HASHMAP_SET,
	HASHMAP_UPDATE,
	HASHMAP_APPEND,
};

/* Fil-C: the old hashmap_cast_ptr() helper (which cast out parameters to
 * long * with a _Static_assert on the pointee size) is gone; the macros
 * above cast out parameters to pointer-to-pointer types instead. */

/*
 * hashmap__insert() adds key/value entry w/ various semantics, depending on
 * provided strategy value. If a given key/value pair replaced already
 * existing key/value pair, both old key and old value will be returned
 * through old_key and old_value to allow calling code do proper memory
 * management.
 */
int hashmap_insert(struct hashmap *map, const void *key, void *value,
		   enum hashmap_insert_strategy strategy,
		   const void **old_key, void **old_value);

#define hashmap__insert(map, key, value, strategy, old_key, old_value) \
	hashmap_insert((map), (key), (value), (strategy),	       \
		       (const void **)(old_key), (void **)(old_value))

#define hashmap__add(map, key, value) \
	hashmap__insert((map), (key), (value), HASHMAP_ADD, NULL, NULL)

#define hashmap__set(map, key, value, old_key, old_value) \
	hashmap__insert((map), (key), (value), HASHMAP_SET, (old_key), (old_value))

#define hashmap__update(map, key, value, old_key, old_value) \
	hashmap__insert((map), (key), (value), HASHMAP_UPDATE, (old_key), (old_value))

#define hashmap__append(map, key, value) \
	hashmap__insert((map), (key), (value), HASHMAP_APPEND, NULL, NULL)

bool hashmap_delete(struct hashmap *map, const void *key, const void **old_key, void **old_value);

#define hashmap__delete(map, key, old_key, old_value)		       \
	hashmap_delete((map), (key),				       \
		       (const void **)(old_key),		       \
		       (void **)(old_value))

bool hashmap_find(const struct hashmap *map, const void *key, void **value);

#define hashmap__find(map, key, value) \
	hashmap_find((map), (key), (void **)(value))

/*
 * hashmap__for_each_entry - iterate over all entries in hashmap
 * @map: hashmap to iterate
 * @cur: struct hashmap_entry * used as a loop cursor
 * @bkt: integer used as a bucket loop cursor
 */
#define hashmap__for_each_entry(map, cur, bkt)				    \
	for (bkt = 0; bkt < map->cap; bkt++)				    \
		for (cur = map->buckets[bkt]; cur; cur = cur->next)

/*
 * hashmap__for_each_entry_safe - iterate over all entries in hashmap, safe
 * against removals
 * @map: hashmap to iterate
 * @cur: struct hashmap_entry * used as a loop cursor
 * @tmp: struct hashmap_entry * used as a temporary next cursor storage
 * @bkt: integer used as a bucket loop cursor
 */
#define hashmap__for_each_entry_safe(map, cur, tmp, bkt)		    \
	for (bkt = 0; bkt < map->cap; bkt++)				    \
		for (cur = map->buckets[bkt];				    \
		     cur && ({tmp = cur->next; true; });		    \
		     cur = tmp)

/*
 * hashmap__for_each_key_entry - iterate over entries associated with given key
 * @map: hashmap to iterate
 * @cur: struct hashmap_entry * used as a loop cursor
 * @key: key to iterate entries for
 */
#define hashmap__for_each_key_entry(map, cur, _key)			    \
	for (cur = map->buckets						    \
		     ? map->buckets[hash_bits(map->hash_fn((_key), map->ctx), map->cap_bits)] \
		     : NULL;						    \
	     cur;							    \
	     cur = cur->next)						    \
		if (map->equal_fn(cur->key, (_key), map->ctx))

#define hashmap__for_each_key_entry_safe(map, cur, tmp, _key)		    \
	for (cur = map->buckets						    \
		     ? map->buckets[hash_bits(map->hash_fn((_key), map->ctx), map->cap_bits)] \
		     : NULL;						    \
	     cur && ({ tmp = cur->next; true; });			    \
	     cur = tmp)							    \
		if (map->equal_fn(cur->key, (_key), map->ctx))

#endif /* __LIBBPF_HASHMAP_H */
