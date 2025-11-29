/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gtksecurememory.c - API for allocating memory that is non-pageable

   Copyright  2007 Stefan Walter
   Copyright  2020 GNOME Foundation

   SPDX-License-Identifier: LGPL-2.0-or-later

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stef@memberwebs.com>
*/

/*
 * IMPORTANT: This is pure vanila standard C, no glib. We need this
 * because certain consumers of this protocol need to be built
 * without linking in any special libraries. ie: the PKCS#11 module.
 */

#include "config.h"

#include "gtksecurememoryprivate.h"

#include <sys/types.h>

#if defined(HAVE_SYS_MMAN_H)
#include <sys/mman.h>
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <assert.h>

#ifdef WITH_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#endif

#define DEBUG_SECURE_MEMORY 0

#if DEBUG_SECURE_MEMORY
#define DEBUG_ALLOC(msg, n) 	fprintf(stderr, "%s %lu bytes\n", msg, n);
#else
#define DEBUG_ALLOC(msg, n)
#endif

#define DEFAULT_BLOCK_SIZE 16384

#define DO_LOCK() \
	GTK_SECURE_GLOBALS.lock ();

#define DO_UNLOCK() \
	GTK_SECURE_GLOBALS.unlock ();

typedef struct {
    void       (* lock)        (void);
    void       (* unlock)      (void);
    void *     (* fallback_alloc) (void *pointer,
                                   size_t length);
    void       (* fallback_free) (void *pointer);
    void *        pool_data;
    const char *  pool_version;
} GtkSecureGlob;

#include <glib.h>

#ifdef G_OS_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <dpapi.h> /* for CryptProtectMemory() */
#endif

#define GTK_SECURE_POOL_VER_STR             "1.0"

static int show_warning = 1;
static int gtk_secure_warnings = 1;
static GMutex memory_mutex;

static void
gtk_memory_lock (void)
{
  g_mutex_lock (&memory_mutex);
}

static void
gtk_memory_unlock (void)
{
  g_mutex_unlock (&memory_mutex);
}

static GtkSecureGlob GTK_SECURE_GLOBALS = {
  .lock = gtk_memory_lock,
  .unlock = gtk_memory_unlock,
  .fallback_alloc = g_realloc,
  .fallback_free = g_free,
  .pool_data = NULL,
  .pool_version = GTK_SECURE_POOL_VER_STR,
};

/*
 * We allocate all memory in units of sizeof(void*). This
 * is our definition of 'word'.
 */
typedef void* word_t;

/* The amount of extra words we can allocate */
#define WASTE   4

/*
 * Track allocated memory or a free block. This structure is not stored
 * in the secure memory area. It is allocated from a pool of other
 * memory. See meta_pool_xxx ().
 */
typedef struct _Cell {
	word_t *words;          /* Pointer to secure memory */
	size_t n_words;         /* Amount of secure memory in words */
	size_t requested;       /* Amount actually requested by app, in bytes, 0 if unused */
	const char *tag;        /* Tag which describes the allocation */
	struct _Cell *next;     /* Next in memory ring */
	struct _Cell *prev;     /* Previous in memory ring */
} Cell;

/*
 * A block of secure memory. This structure is the header in that block.
 */
typedef struct _Block {
	word_t *words;              /* Actual memory hangs off here */
	size_t n_words;             /* Number of words in block */
	size_t n_used;              /* Number of used allocations */
	struct _Cell* used_cells;   /* Ring of used allocations */
	struct _Cell* unused_cells; /* Ring of unused allocations */
	struct _Block *next;        /* Next block in list */
} Block;

/* -----------------------------------------------------------------------------
 * UNUSED STACK
 */

static inline void
unused_push (void **stack, void *ptr)
{
	g_assert (ptr);
	g_assert (stack);
	*((void**)ptr) = *stack;
	*stack = ptr;
}

static inline void*
unused_pop (void **stack)
{
	void *ptr;
	g_assert (stack);
	ptr = *stack;
	*stack = *(void**)ptr;
	return ptr;

}

static inline void*
unused_peek (void **stack)
{
	g_assert (stack);
	return *stack;
}

/* -----------------------------------------------------------------------------
 * POOL META DATA ALLOCATION
 *
 * A pool for memory meta data. We allocate fixed size blocks. There are actually
 * two different structures stored in this pool: Cell and Block. Cell is allocated
 * way more often, and is bigger so we just allocate that size for both.
 */

/* Pool allocates this data type */
typedef union _Item {
	Cell cell;
	Block block;
} Item;

typedef struct _Pool {
	struct _Pool *next;    /* Next pool in list */
	size_t length;         /* Length in bytes of the pool */
	size_t used;           /* Number of cells used in pool */
	void *unused;          /* Unused stack of unused stuff */
	size_t n_items;        /* Total number of items in pool */
	Item items[1];         /* Actual items hang off here */
} Pool;

static int
check_pool_version (void)
{
        if (GTK_SECURE_GLOBALS.pool_version == NULL ||
	    strcmp (GTK_SECURE_GLOBALS.pool_version, GTK_SECURE_POOL_VER_STR) != 0) {
                return 0;
        }

        return 1;
}

static void *
pool_alloc (void)
{
        if (!check_pool_version ()) {
		if (show_warning && gtk_secure_warnings)
			fprintf (stderr, "the secure memory pool version does not match the code '%s' != '%s'\n",
			         GTK_SECURE_GLOBALS.pool_version ? GTK_SECURE_GLOBALS.pool_version : "(null)",
			         GTK_SECURE_POOL_VER_STR);
		show_warning = 0;
		return NULL;
	}

#if defined(HAVE_MMAP) && defined(HAVE_GETPAGESIZE)
	/* A pool with an available item */
        Pool *pool = NULL;

	for (pool = GTK_SECURE_GLOBALS.pool_data; pool != NULL; pool = pool->next) {
		if (unused_peek (&pool->unused))
			break;
	}

        void *item = NULL;

	/* Create a new pool */
	if (pool == NULL) {
		size_t len = getpagesize () * 2;
		void *pages = mmap (0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (pages == MAP_FAILED)
			return NULL;

		/* Fill in the block header, and include in block list */
		pool = pages;
		pool->next = GTK_SECURE_GLOBALS.pool_data;
		GTK_SECURE_GLOBALS.pool_data = pool;
		pool->length = len;
		pool->used = 0;
		pool->unused = NULL;

		/* Fill block with unused items */
		pool->n_items = (len - sizeof (Pool)) / sizeof (Item);
		for (size_t i = 0; i < pool->n_items; ++i)
			unused_push (&pool->unused, pool->items + i);

#ifdef WITH_VALGRIND
		VALGRIND_CREATE_MEMPOOL(pool, 0, 0);
#endif
	}

	++pool->used;
	g_assert (unused_peek (&pool->unused));
	item = unused_pop (&pool->unused);

#ifdef WITH_VALGRIND
	VALGRIND_MEMPOOL_ALLOC (pool, item, sizeof (Item));
#endif

	return memset (item, 0, sizeof (Item));
#else /* HAVE_MMAP */
        return NULL;
#endif
}

static void
pool_free (void* item)
{
#ifdef HAVE_MMAP
	Pool *pool, **at;
	char *ptr, *beg, *end;

	ptr = item;

	/* Find which block this one belongs to */
	for (at = (Pool **)&GTK_SECURE_GLOBALS.pool_data, pool = *at;
	     pool != NULL; at = &pool->next, pool = *at) {
		beg = (char*)pool->items;
		end = (char*)pool + pool->length - sizeof (Item);
		if (ptr >= beg && ptr <= end) {
			g_assert ((ptr - beg) % sizeof (Item) == 0);
			break;
		}
	}

	/* Otherwise invalid meta */
	g_assert (at);
	g_assert (pool);
	g_assert (pool->used > 0);

	/* No more meta cells used in this block, remove from list, destroy */
	if (pool->used == 1) {
		*at = pool->next;

#ifdef WITH_VALGRIND
		VALGRIND_DESTROY_MEMPOOL (pool);
#endif

		munmap (pool, pool->length);
		return;
	}

#ifdef WITH_VALGRIND
	VALGRIND_MEMPOOL_FREE (pool, item);
	VALGRIND_MAKE_MEM_UNDEFINED (item, sizeof (Item));
#endif

	--pool->used;
	memset (item, 0xCD, sizeof (Item));
	unused_push (&pool->unused, item);
#endif /* HAVE_MMAP */
}

#ifndef G_DISABLE_ASSERT

static int
pool_valid (void* item)
{
	Pool *pool;
	char *ptr, *beg, *end;

	ptr = item;

	/* Find which block this one belongs to */
	for (pool = GTK_SECURE_GLOBALS.pool_data; pool; pool = pool->next) {
		beg = (char*)pool->items;
		end = (char*)pool + pool->length - sizeof (Item);
		if (ptr >= beg && ptr <= end)
			return (pool->used && (ptr - beg) % sizeof (Item) == 0);
	}

	return 0;
}

#endif /* G_DISABLE_g_assert */

/* -----------------------------------------------------------------------------
 * SEC ALLOCATION
 *
 * Each memory cell begins and ends with a pointer to its metadata. These are also
 * used as guards or red zones. Since they're treated as redzones by valgrind we
 * have to jump through a few hoops before reading and/or writing them.
 */

static inline size_t
sec_size_to_words (size_t length)
{
	return (length % sizeof (void*) ? 1 : 0) + (length / sizeof (void*));
}

static inline void
sec_write_guards (Cell *cell)
{
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_UNDEFINED (cell->words, sizeof (word_t));
	VALGRIND_MAKE_MEM_UNDEFINED (cell->words + cell->n_words - 1, sizeof (word_t));
#endif

	((void**)cell->words)[0] = (void*)cell;
	((void**)cell->words)[cell->n_words - 1] = (void*)cell;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (cell->words, sizeof (word_t));
	VALGRIND_MAKE_MEM_NOACCESS (cell->words + cell->n_words - 1, sizeof (word_t));
#endif
}

static inline void
sec_check_guards (Cell *cell)
{
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (cell->words, sizeof (word_t));
	VALGRIND_MAKE_MEM_DEFINED (cell->words + cell->n_words - 1, sizeof (word_t));
#endif

	g_assert(((void**)cell->words)[0] == (void*)cell);
	g_assert(((void**)cell->words)[cell->n_words - 1] == (void*)cell);

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (cell->words, sizeof (word_t));
	VALGRIND_MAKE_MEM_NOACCESS (cell->words + cell->n_words - 1, sizeof (word_t));
#endif
}

static void
sec_insert_cell_ring (Cell **ring, Cell *cell)
{
	g_assert (ring);
	g_assert (cell);
	g_assert (cell != *ring);
	g_assert (cell->next == NULL);
	g_assert (cell->prev == NULL);

	/* Insert back into the mix of available memory */
	if (*ring) {
		cell->next = (*ring)->next;
		cell->prev = *ring;
		cell->next->prev = cell;
		cell->prev->next = cell;
	} else {
		cell->next = cell;
		cell->prev = cell;
	}

	*ring = cell;
	g_assert (cell->next->prev == cell);
	g_assert (cell->prev->next == cell);
}

static void
sec_remove_cell_ring (Cell **ring, Cell *cell)
{
	g_assert (ring);
	g_assert (*ring);
	g_assert (cell->next);
	g_assert (cell->prev);

	g_assert (cell->next->prev == cell);
	g_assert (cell->prev->next == cell);

	if (cell == *ring) {
		/* The last meta? */
		if (cell->next == cell) {
			g_assert (cell->prev == cell);
			*ring = NULL;

		/* Just pointing to this meta */
		} else {
			g_assert (cell->prev != cell);
			*ring = cell->next;
		}
	}

	cell->next->prev = cell->prev;
	cell->prev->next = cell->next;
	cell->next = cell->prev = NULL;

	g_assert (*ring != cell);
}

static inline void*
sec_cell_to_memory (Cell *cell)
{
	return cell->words + 1;
}

static inline int
sec_is_valid_word (Block *block, word_t *word)
{
	return (word >= block->words && word < block->words + block->n_words);
}

static inline void
sec_clear_undefined (void *memory,
                     size_t from,
                     size_t to)
{
	char *ptr = memory;
	g_assert (from <= to);
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_UNDEFINED (ptr + from, to - from);
#endif
	memset (ptr + from, 0, to - from);
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_UNDEFINED (ptr + from, to - from);
#endif
}
static inline void
sec_clear_noaccess (void *memory, size_t from, size_t to)
{
	char *ptr = memory;
	g_assert (from <= to);
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_UNDEFINED (ptr + from, to - from);
#endif
	memset (ptr + from, 0, to - from);
#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (ptr + from, to - from);
#endif
}

static Cell*
sec_neighbor_before (Block *block, Cell *cell)
{
	word_t *word;

	g_assert (cell);
	g_assert (block);

	word = cell->words - 1;
	if (!sec_is_valid_word (block, word))
		return NULL;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (word, sizeof (word_t));
#endif

	cell = *word;
	sec_check_guards (cell);

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (word, sizeof (word_t));
#endif

	return cell;
}

static Cell*
sec_neighbor_after (Block *block, Cell *cell)
{
	word_t *word;

	g_assert (cell);
	g_assert (block);

	word = cell->words + cell->n_words;
	if (!sec_is_valid_word (block, word))
		return NULL;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (word, sizeof (word_t));
#endif

	cell = *word;
	sec_check_guards (cell);

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (word, sizeof (word_t));
#endif

	return cell;
}

static void*
sec_alloc (Block *block,
           const char *tag,
           size_t length)
{
	Cell *cell, *other;
	size_t n_words;
	void *memory;

	g_assert (block);
	g_assert (length);
	g_assert (tag);

	if (!block->unused_cells)
		return NULL;

	/*
	 * Each memory allocation is aligned to a pointer size, and
	 * then, sandwidched between two pointers to its meta data.
	 * These pointers also act as guards.
	 *
	 * We allocate memory in units of sizeof (void*)
	 */

	n_words = sec_size_to_words (length) + 2;

	/* Look for a cell of at least our required size */
	cell = block->unused_cells;
	while (cell->n_words < n_words) {
		cell = cell->next;
		if (cell == block->unused_cells) {
			cell = NULL;
			break;
		}
	}

	if (!cell)
		return NULL;

	g_assert (cell->tag == NULL);
	g_assert (cell->requested == 0);
	g_assert (cell->prev);
	g_assert (cell->words);
	sec_check_guards (cell);

	/* Steal from the cell if it's too long */
	if (cell->n_words > n_words + WASTE) {
		other = pool_alloc ();
		if (!other)
			return NULL;
		other->n_words = n_words;
		other->words = cell->words;
		cell->n_words -= n_words;
		cell->words += n_words;

		sec_write_guards (other);
		sec_write_guards (cell);

		cell = other;
	}

	if (cell->next)
		sec_remove_cell_ring (&block->unused_cells, cell);

	++block->n_used;
	cell->tag = tag;
	cell->requested = length;
	sec_insert_cell_ring (&block->used_cells, cell);
	memory = sec_cell_to_memory (cell);

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_UNDEFINED (memory, length);
#endif

	return memset (memory, 0, length);
}

static void*
sec_free (Block *block, void *memory)
{
	Cell *cell, *other;
	word_t *word;

	g_assert (block);
	g_assert (memory);

	word = memory;
	--word;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (word, sizeof (word_t));
#endif

	/* Lookup the meta for this memory block (using guard pointer) */
	g_assert (sec_is_valid_word (block, word));
	g_assert (pool_valid (*word));
	cell = *word;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (cell->words, cell->n_words * sizeof (word_t));
#endif

	sec_check_guards (cell);
	sec_clear_noaccess (memory, 0, cell->requested);

	sec_check_guards (cell);
	g_assert (cell->requested > 0);
	g_assert (cell->tag != NULL);

	/* Remove from the used cell ring */
	sec_remove_cell_ring (&block->used_cells, cell);

	/* Find previous unallocated neighbor, and merge if possible */
	other = sec_neighbor_before (block, cell);
	if (other && other->requested == 0) {
		g_assert (other->tag == NULL);
		g_assert (other->next && other->prev);
		other->n_words += cell->n_words;
		sec_write_guards (other);
		pool_free (cell);
		cell = other;
	}

	/* Find next unallocated neighbor, and merge if possible */
	other = sec_neighbor_after (block, cell);
	if (other && other->requested == 0) {
		g_assert (other->tag == NULL);
		g_assert (other->next && other->prev);
		other->n_words += cell->n_words;
		other->words = cell->words;
		if (cell->next)
			sec_remove_cell_ring (&block->unused_cells, cell);
		sec_write_guards (other);
		pool_free (cell);
		cell = other;
	}

	/* Add to the unused list if not already there */
	if (!cell->next)
		sec_insert_cell_ring (&block->unused_cells, cell);

	cell->tag = NULL;
	cell->requested = 0;
	--block->n_used;
	return NULL;
}

static void
memcpy_with_vbits (void *dest,
                   void *src,
                   size_t length)
{
#ifdef WITH_VALGRIND
	int vbits_setup = 0;
	void *vbits = NULL;

	if (RUNNING_ON_VALGRIND) {
		vbits = malloc (length);
		if (vbits != NULL)
			vbits_setup = VALGRIND_GET_VBITS (src, vbits, length);
		VALGRIND_MAKE_MEM_DEFINED (src, length);
	}
#endif

	memcpy (dest, src, length);

#ifdef WITH_VALGRIND
	if (vbits_setup == 1) {
		VALGRIND_SET_VBITS (dest, vbits, length);
		VALGRIND_SET_VBITS (src, vbits, length);
	}
	free (vbits);
#endif
}

static void*
sec_realloc (Block *block,
             const char *tag,
             void *memory,
             size_t length)
{
	Cell *cell, *other;
	word_t *word;
	size_t n_words;
	size_t valid;
	void *alloc;

	/* Standard realloc behavior, should have been handled elsewhere */
	g_assert (memory != NULL);
	g_assert (length > 0);
	g_assert (tag != NULL);

	/* Dig out where the meta should be */
	word = memory;
	--word;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (word, sizeof (word_t));
#endif

	g_assert (sec_is_valid_word (block, word));
	g_assert (pool_valid (*word));
	cell = *word;

	/* Validate that it's actually for real */
	sec_check_guards (cell);
	g_assert (cell->requested > 0);
	g_assert (cell->tag != NULL);

	/* The amount of valid data */
	valid = cell->requested;

	/* How many words we actually want */
	n_words = sec_size_to_words (length) + 2;

	/* Less memory is required than is in the cell */
	if (n_words <= cell->n_words) {

		/* TODO: No shrinking behavior yet */
		cell->requested = length;
		alloc = sec_cell_to_memory (cell);

		/*
		 * Even though we may be reusing the same cell, that doesn't
		 * mean that the allocation is shrinking. It could have shrunk
		 * and is now expanding back some.
		 */
		if (length < valid)
			sec_clear_undefined (alloc, length, valid);

		return alloc;
	}

	/* Need braaaaaiiiiiinsss... */
	while (cell->n_words < n_words) {

		/* See if we have a neighbor who can give us some memory */
		other = sec_neighbor_after (block, cell);
		if (!other || other->requested != 0)
			break;

		/* Eat the whole neighbor if not too big */
		if (n_words - cell->n_words + WASTE >= other->n_words) {
			cell->n_words += other->n_words;
			sec_write_guards (cell);
			sec_remove_cell_ring (&block->unused_cells, other);
			pool_free (other);

		/* Steal from the neighbor */
		} else {
			other->words += n_words - cell->n_words;
			other->n_words -= n_words - cell->n_words;
			sec_write_guards (other);
			cell->n_words = n_words;
			sec_write_guards (cell);
		}
	}

	if (cell->n_words >= n_words) {
		cell->requested = length;
		cell->tag = tag;
		alloc = sec_cell_to_memory (cell);
		sec_clear_undefined (alloc, valid, length);
		return alloc;
	}

	/* That didn't work, try alloc/free */
	alloc = sec_alloc (block, tag, length);
	if (alloc) {
		memcpy_with_vbits (alloc, memory, valid);
		sec_free (block, memory);
	}

	return alloc;
}


static size_t
sec_allocated (Block *block, void *memory)
{
	Cell *cell;
	word_t *word;

	g_assert (block);
	g_assert (memory);

	word = memory;
	--word;

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (word, sizeof (word_t));
#endif

	/* Lookup the meta for this memory block (using guard pointer) */
	g_assert (sec_is_valid_word (block, word));
	g_assert (pool_valid (*word));
	cell = *word;

	sec_check_guards (cell);
	g_assert (cell->requested > 0);
	g_assert (cell->tag != NULL);

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_NOACCESS (word, sizeof (word_t));
#endif

	return cell->requested;
}

static void
sec_validate (Block *block)
{
	Cell *cell;
	word_t *word, *last;

#ifdef WITH_VALGRIND
	if (RUNNING_ON_VALGRIND)
		return;
#endif

	word = block->words;
	last = word + block->n_words;

	for (;;) {
		g_assert (word < last);

		g_assert (sec_is_valid_word (block, word));
		g_assert (pool_valid (*word));
		cell = *word;

		/* Validate that it's actually for real */
		sec_check_guards (cell);

		/* Is it an allocated block? */
		if (cell->requested > 0) {
			g_assert (cell->tag != NULL);
			g_assert (cell->next != NULL);
			g_assert (cell->prev != NULL);
			g_assert (cell->next->prev == cell);
			g_assert (cell->prev->next == cell);
			g_assert (cell->requested <= (cell->n_words - 2) * sizeof (word_t));

		/* An unused block */
		} else {
			g_assert (cell->tag == NULL);
			g_assert (cell->next != NULL);
			g_assert (cell->prev != NULL);
			g_assert (cell->next->prev == cell);
			g_assert (cell->prev->next == cell);
		}

		word += cell->n_words;
		if (word == last)
			break;
	}
}

/* -----------------------------------------------------------------------------
 * LOCKED MEMORY
 */

static void*
sec_acquire_pages (size_t *sz,
                   const char *during_tag)
{
	g_assert (sz);
	g_assert (*sz);
	g_assert (during_tag);

#if defined(HAVE_MLOCK) && defined(HAVE_MMAP) && defined(HAVE_GETPAGESIZE)
	/* Make sure sz is a multiple of the page size */
	unsigned long pgsize = getpagesize ();

	*sz = (*sz + pgsize -1) & ~(pgsize - 1);

	void *pages = mmap (0, *sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (pages == MAP_FAILED) {
		if (show_warning && gtk_secure_warnings)
			fprintf (stderr, "couldn't map %lu bytes of memory (%s): %s\n",
			         (unsigned long)*sz, during_tag, strerror (errno));
		show_warning = 0;
		return NULL;
	}

	if (mlock (pages, *sz) < 0) {
		if (show_warning && gtk_secure_warnings && errno != EPERM) {
			fprintf (stderr, "couldn't lock %lu bytes of memory (%s): %s\n",
			         (unsigned long)*sz, during_tag, strerror (errno));
			show_warning = 0;
		}
		munmap (pages, *sz);
		return NULL;
	}

	DEBUG_ALLOC ("gtk-secure-memory: new block ", *sz);

#if defined(HAVE_MADVISE) && defined(MADV_DONTDUMP)
	if (madvise (pages, *sz, MADV_DONTDUMP) < 0) {
		if (show_warning && gtk_secure_warnings) {
			/*
			 * Not fatal - this was added in Linux 3.4 and older
			 * kernels will legitimately fail this at runtime
			 */
			fprintf (stderr, "couldn't MADV_DONTDUMP %lu bytes of memory (%s): %s\n",
				 (unsigned long)*sz, during_tag, strerror (errno));
		}
	}
#endif

	show_warning = 1;
	return pages;

#elif defined G_OS_WIN32
	/* Make sure sz is a multiple of CRYPTPROTECTMEMORY_BLOCK_SIZE in wincrypt.h */
	*sz = (*sz + CRYPTPROTECTMEMORY_BLOCK_SIZE - 1) & ~(CRYPTPROTECTMEMORY_BLOCK_SIZE - 1);

	void *data = (void *) LocalAlloc (LPTR, *sz);

	if (data == NULL) {
		if (show_warning && gtk_secure_warnings)
			fprintf (stderr, "couldn't allocate %lu bytes of memory (%s): %#010lX\n",
			         (unsigned long)*sz, during_tag, GetLastError ());
		show_warning = 0;
		return NULL;
	}

	if (!CryptProtectMemory (data, *sz, CRYPTPROTECTMEMORY_SAME_PROCESS)) {
		if (show_warning && gtk_secure_warnings)
			fprintf (stderr, "couldn't encrypt %lu bytes of memory (%s): %#010lX\n",
			         (unsigned long)*sz, during_tag, GetLastError ());
		show_warning = 0;
		return NULL;
	}

	DEBUG_ALLOC ("gtk-secure-memory: new block ", *sz);

	show_warning = 1;
	return data;
	
#else
	if (show_warning && gtk_secure_warnings)
		fprintf (stderr, "your system does not support private memory");
	show_warning = 0;
	return NULL;
#endif

}

static void
sec_release_pages (void *pages, size_t sz)
{
	g_assert (pages);

#if defined(HAVE_MLOCK) && defined(HAVE_GETPAGESIZE)
	g_assert (sz % getpagesize () == 0);

	if (munlock (pages, sz) < 0 && gtk_secure_warnings)
		fprintf (stderr, "couldn't unlock private memory: %s\n", strerror (errno));

	if (munmap (pages, sz) < 0 && gtk_secure_warnings)
		fprintf (stderr, "couldn't unmap private anonymous memory: %s\n", strerror (errno));

	DEBUG_ALLOC ("gtk-secure-memory: freed block ", sz);

#elif defined G_OS_WIN32
	g_assert (sz % CRYPTPROTECTMEMORY_BLOCK_SIZE == 0);

	if (!CryptUnprotectMemory (pages, sz, CRYPTPROTECTMEMORY_SAME_PROCESS))
		fprintf (stderr, "couldn't decrypt private memory: %#010lX\n", GetLastError ());

	if (LocalFree (pages) != NULL)
		fprintf (stderr, "couldn't free private anonymous memory: %#010lX\n", GetLastError ());

	DEBUG_ALLOC ("gtk-secure-memory: freed block ", sz);
#else
	g_assert (FALSE);
#endif
}

/* -----------------------------------------------------------------------------
 * MANAGE DIFFERENT BLOCKS
 */

static Block *all_blocks = NULL;

static Block*
sec_block_create (size_t size,
                  const char *during_tag)
{
	Block *block;
	Cell *cell;

	g_assert (during_tag);

	/* We can force all all memory to be malloced */
	if (getenv ("SECMEM_FORCE_FALLBACK"))
		return NULL;

	block = pool_alloc ();
	if (!block)
		return NULL;

	cell = pool_alloc ();
	if (!cell) {
		pool_free (block);
		return NULL;
	}

	/* The size above is a minimum, we're free to go bigger */
	if (size < DEFAULT_BLOCK_SIZE)
		size = DEFAULT_BLOCK_SIZE;

	block->words = sec_acquire_pages (&size, during_tag);
	block->n_words = size / sizeof (word_t);
	if (!block->words) {
		pool_free (block);
		pool_free (cell);
		return NULL;
	}

#ifdef WITH_VALGRIND
	VALGRIND_MAKE_MEM_DEFINED (block->words, size);
#endif

	/* The first cell to allocate from */
	cell->words = block->words;
	cell->n_words = block->n_words;
	cell->requested = 0;
	sec_write_guards (cell);
	sec_insert_cell_ring (&block->unused_cells, cell);

	block->next = all_blocks;
	all_blocks = block;

	return block;
}

static void
sec_block_destroy (Block *block)
{
	Block *bl, **at;
	Cell *cell;

	g_assert (block);
	g_assert (block->words);
	g_assert (block->n_used == 0);

	/* Remove from the list */
	for (at = &all_blocks, bl = *at; bl; at = &bl->next, bl = *at) {
		if (bl == block) {
			*at = block->next;
			break;
		}
	}

	/* Must have been found */
	g_assert (bl == block);
	g_assert (block->used_cells == NULL);

	/* Release all the meta data cells */
	while (block->unused_cells) {
		cell = block->unused_cells;
		sec_remove_cell_ring (&block->unused_cells, cell);
		pool_free (cell);
	}

	/* Release all pages of secure memory */
	sec_release_pages (block->words, block->n_words * sizeof (word_t));

	pool_free (block);
}

/* ------------------------------------------------------------------------
 * PUBLIC FUNCTIONALITY
 */

void*
gtk_secure_alloc_full (const char *tag,
                       size_t length,
                       int flags)
{
	Block *block;
	void *memory = NULL;

	if (tag == NULL)
		tag = "?";

	if (length > 0xFFFFFFFF / 2) {
		if (gtk_secure_warnings)
			fprintf (stderr, "tried to allocate an insane amount of memory: %lu\n",
			         (unsigned long)length);
		return NULL;
	}

	/* Can't allocate zero bytes */
	if (length == 0)
		return NULL;

	DO_LOCK ();

	for (block = all_blocks; block; block = block->next) {
		memory = sec_alloc (block, tag, length);
		if (memory)
			break;
	}

	/* None of the current blocks have space, allocate new */
	if (!memory) {
		block = sec_block_create (length, tag);
		if (block)
			memory = sec_alloc (block, tag, length);
	}

#ifdef WITH_VALGRIND
	if (memory != NULL)
		VALGRIND_MALLOCLIKE_BLOCK (memory, length, sizeof (void*), 1);
#endif

	DO_UNLOCK ();

	if (!memory && (flags & GTK_SECURE_USE_FALLBACK) && GTK_SECURE_GLOBALS.fallback_alloc != NULL) {
		memory = GTK_SECURE_GLOBALS.fallback_alloc (NULL, length);
		if (memory) /* Our returned memory is always zeroed */
			memset (memory, 0, length);
	}

	if (!memory)
		errno = ENOMEM;

	return memory;
}

void*
gtk_secure_realloc_full (const char *tag,
                         void *memory,
                         size_t length,
                         int flags)
{
	Block *block = NULL;
	size_t previous = 0;
	int donew = 0;
	void *alloc = NULL;

	if (tag == NULL)
		tag = "?";

	if (length > 0xFFFFFFFF / 2) {
		if (gtk_secure_warnings)
			fprintf (stderr, "tried to allocate an excessive amount of memory: %lu\n",
			         (unsigned long)length);
		return NULL;
	}

	if (memory == NULL)
		return gtk_secure_alloc_full (tag, length, flags);
	if (!length) {
		gtk_secure_free_full (memory, flags);
		return NULL;
	}

	DO_LOCK ();

	/* Find out where it belongs to */
	for (block = all_blocks; block; block = block->next) {
		if (sec_is_valid_word (block, memory)) {
			previous = sec_allocated (block, memory);

#ifdef WITH_VALGRIND
			/* Let valgrind think we are unallocating so that it'll validate */
			VALGRIND_FREELIKE_BLOCK (memory, sizeof (word_t));
#endif

			alloc = sec_realloc (block, tag, memory, length);

#ifdef WITH_VALGRIND
			/* Now tell valgrind about either the new block or old one */
			VALGRIND_MALLOCLIKE_BLOCK (alloc ? alloc : memory,
			                           alloc ? length : previous,
			                           sizeof (word_t), 1);
#endif
			break;
		}
	}

	/* If it didn't work we may need to allocate a new block */
	if (block && !alloc)
		donew = 1;

	if (block && block->n_used == 0)
		sec_block_destroy (block);

	DO_UNLOCK ();

	if (!block) {
		if ((flags & GTK_SECURE_USE_FALLBACK) && GTK_SECURE_GLOBALS.fallback_alloc) {
			/*
			 * In this case we can't zero the returned memory,
			 * because we don't know what the block size was.
			 */
			return GTK_SECURE_GLOBALS.fallback_alloc (memory, length);
		} else {
			if (gtk_secure_warnings)
				fprintf (stderr, "memory does not belong to secure memory pool: 0x%08" G_GUINTPTR_FORMAT "x\n",
				         (guintptr) memory);
			g_assert (0 && "memory does does not belong to secure memory pool");
			return NULL;
		}
	}

	if (donew) {
		alloc = gtk_secure_alloc_full (tag, length, flags);
		if (alloc) {
			memcpy_with_vbits (alloc, memory, previous);
			gtk_secure_free_full (memory, flags);
		}
	}

	if (!alloc)
		errno = ENOMEM;

	return alloc;
}

void
gtk_secure_free (void *memory)
{
	gtk_secure_free_full (memory, GTK_SECURE_USE_FALLBACK);
}

void
gtk_secure_free_full (void *memory, int flags)
{
	Block *block = NULL;

	if (memory == NULL)
		return;

	DO_LOCK ();

	/* Find out where it belongs to */
	for (block = all_blocks; block; block = block->next) {
		if (sec_is_valid_word (block, memory))
			break;
	}

#ifdef WITH_VALGRIND
	/* We like valgrind's warnings, so give it a first whack at checking for errors */
	if (block != NULL || !(flags & GTK_SECURE_USE_FALLBACK))
		VALGRIND_FREELIKE_BLOCK (memory, sizeof (word_t));
#endif

	if (block != NULL) {
		sec_free (block, memory);
		if (block->n_used == 0)
			sec_block_destroy (block);
	}

	DO_UNLOCK ();

	if (!block) {
		if ((flags & GTK_SECURE_USE_FALLBACK) && GTK_SECURE_GLOBALS.fallback_free) {
			GTK_SECURE_GLOBALS.fallback_free (memory);
		} else {
			if (gtk_secure_warnings)
				fprintf (stderr, "memory does not belong to secure memory pool: 0x%08" G_GUINTPTR_FORMAT "x\n",
				         (guintptr) memory);
			g_assert (0 && "memory does does not belong to secure memory pool");
		}
	}
}

int
gtk_secure_check (const void *memory)
{
	Block *block = NULL;

	DO_LOCK ();

	/* Find out where it belongs to */
	for (block = all_blocks; block; block = block->next) {
		if (sec_is_valid_word (block, (word_t*)memory))
			break;
	}

	DO_UNLOCK ();

	return block == NULL ? 0 : 1;
}

void
gtk_secure_validate (void)
{
	Block *block = NULL;

	DO_LOCK ();

	for (block = all_blocks; block; block = block->next)
		sec_validate (block);

	DO_UNLOCK ();
}


static gtk_secure_rec *
records_for_ring (Cell *cell_ring,
                  gtk_secure_rec *records,
                  unsigned int *count,
                  unsigned int *total)
{
	gtk_secure_rec *new_rec;
	unsigned int allocated = *count;
	Cell *cell;

	cell = cell_ring;
	do {
		if (*count >= allocated) {
			new_rec = realloc (records, sizeof (gtk_secure_rec) * (allocated + 32));
			if (new_rec == NULL) {
				*count = 0;
				free (records);
				return NULL;
			} else {
				records = new_rec;
				allocated += 32;
			}
		}

		if (cell != NULL) {
			records[*count].request_length = cell->requested;
			records[*count].block_length = cell->n_words * sizeof (word_t);
			records[*count].tag = cell->tag;
			(*count)++;
			(*total) += cell->n_words;
			cell = cell->next;
		}
	} while (cell != NULL && cell != cell_ring);

	return records;
}

gtk_secure_rec *
gtk_secure_records (unsigned int *count)
{
	gtk_secure_rec *records = NULL;
	Block *block = NULL;
	unsigned int total;

	*count = 0;

	DO_LOCK ();

	for (block = all_blocks; block != NULL; block = block->next) {
		total = 0;

		records = records_for_ring (block->unused_cells, records, count, &total);
		if (records == NULL)
			break;
		records = records_for_ring (block->used_cells, records, count, &total);
		if (records == NULL)
			break;

		/* Make sure this actually accounts for all memory */
		g_assert (total == block->n_words);
	}

	DO_UNLOCK ();

	return records;
}

char*
gtk_secure_strdup_full (const char *tag,
                        const char *str,
                        int options)
{
	size_t len;
	char *res;

	if (!str)
		return NULL;

	len = strlen (str) + 1;
	res = (char *)gtk_secure_alloc_full (tag, len, options);
	strcpy (res, str);
	return res;
}

char *
gtk_secure_strndup_full (const char *tag,
                         const char *str,
                         size_t length,
                         int options)
{
	size_t len;
	char *res;
	const char *end;

	if (!str)
		return NULL;

	end = memchr (str, '\0', length);
	if (end != NULL)
		length = (end - str);
	len = length + 1;
	res = (char *)gtk_secure_alloc_full (tag, len, options);
	memcpy (res, str, len);
	return res;
}

void
gtk_secure_clear (void *p, size_t length)
{
	volatile char *vp;

	if (p == NULL)
		return;

        vp = (volatile char*)p;
        while (length) {
	        *vp = 0xAA;
	        vp++;
	        length--;
	}
}

void
gtk_secure_strclear (char *str)
{
	if (!str)
		return;
	gtk_secure_clear ((unsigned char*)str, strlen (str));
}

void
gtk_secure_strfree (char *str)
{
	/*
	 * If we're using unpageable 'secure' memory, then the free call
	 * should zero out the memory, but because on certain platforms
	 * we may be using normal memory, zero it out here just in case.
	 */

	gtk_secure_strclear (str);
	gtk_secure_free_full (str, GTK_SECURE_USE_FALLBACK);
}
