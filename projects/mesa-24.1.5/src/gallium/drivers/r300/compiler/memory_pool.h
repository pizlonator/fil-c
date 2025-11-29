/*
 * Copyright 2009 Nicolai Hähnle <nhaehnle@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

struct memory_block;

/**
 * Provides a pool of memory that can quickly be allocated from, at the
 * cost of being unable to explicitly free one of the allocated blocks.
 * Instead, the entire pool can be freed at once.
 *
 * The idea is to allow one to quickly allocate a flexible amount of
 * memory during operations like shader compilation while avoiding
 * reference counting headaches.
 */
struct memory_pool {
	unsigned char * head;
	unsigned char * end;
	unsigned int total_allocated;
	struct memory_block * blocks;
};


void memory_pool_init(struct memory_pool * pool);
void memory_pool_destroy(struct memory_pool * pool);
void * memory_pool_malloc(struct memory_pool * pool, unsigned int bytes);


/**
 * Generic helper for growing an array that has separate size/count
 * and reserved counters to accommodate up to num new element.
 *
 *  type * Array;
 *  unsigned int Size;
 *  unsigned int Reserved;
 *
 * memory_pool_array_reserve(pool, type, Array, Size, Reserved, k);
 * assert(Size + k < Reserved);
 *
 * \note Size is not changed by this macro.
 *
 * \warning Array, Size, Reserved have to be lvalues and may be evaluated
 * several times.
 */
#define memory_pool_array_reserve(pool, type, array, size, reserved, num) do { \
	unsigned int _num = (num); \
	if ((size) + _num > (reserved)) { \
		unsigned int newreserve = (reserved) * 2; \
		type * newarray; \
		if (newreserve < _num) \
			newreserve = 4 * _num; /* arbitrary heuristic */ \
		newarray = memory_pool_malloc((pool), newreserve * sizeof(type)); \
		memcpy(newarray, (array), (size) * sizeof(type)); \
		(array) = newarray; \
		(reserved) = newreserve; \
	} \
} while(0)

#endif /* MEMORY_POOL_H */
