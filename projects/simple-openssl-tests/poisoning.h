#ifndef POISONING_H
#define POISONING_H

/*
 * poisoning.h -- common support header for every program in
 * projects/simple-openssl-tests.
 *
 * Every key, input, output, IV, AAD, tag, salt, and other working buffer that
 * a test program hands to OpenSSL is allocated with malloc() inside a
 * carefully labeled ALLOCATE_* function at the very top of the program (e.g.
 * ALLOCATE_INPUT, ALLOCATE_OUTPUT, ALLOCATE_KEY).  When the user asks for a
 * given buffer kind to be poisoned on the command line (--poison-input,
 * --poison-key, ...), the ALLOCATE_* function calls poison_alloc(), which
 * poisons what malloc() returned by way of poison_range() and records the
 * allocation.  Without the command line option, poison_range() is not called
 * for that buffer.
 *
 * The implementation lives in poisoning.c; this header only declares the API
 * and every test binary links the shared poisoning.o (see the Makefile).
 * poison_range() is private to poisoning.c: tests poison buffers exclusively
 * through poison_alloc().
 *
 * poison_range() wires into the zegarmistrz poisoning API (<zegarmistrz.h>,
 * header-only): when running under the zegarmistrz emulator, it poisons the
 * buffer with the selected variant bits (--poison-variants=
 * cannot-branch,cannot-index,cannot-var-time-op, or the POISON_VARIANTS
 * environment variable; see poison_print_usage).  Outside the emulator --
 * and whenever no variant is selected -- zegarmistrz's entry points are
 * no-ops, so poison options never change what a program computes.  Only the
 * CANNOT_BRANCH, CANNOT_INDEX, and CANNOT_VAR_TIME_OP variants are exposed
 * here; zegarmistrz's CANNOT_LOAD/CANNOT_STORE, CLEAR_*_ON_STORE, and
 * CLEAR_VAR_TIME_OP_ON_STORE bits are deliberately not offered (loads and
 * stores must keep working, and clear-on-store would make the poisoning of
 * long-lived key material evaporate on first use).
 *
 * All test programs start their main() with:
 *
 *     poison_parse_args(argc, argv);
 *
 * which consumes only --poison-* options (see poison_print_usage for the
 * list) and rejects everything else, because the tests are self-contained
 * and take no other input.
 *
 * poison_alloc() tracks every allocation it hands out -- poisoned or not --
 * in a table private to poisoning.c, which powers three APIs the tests use
 * around their comparisons:
 *
 * - unpoison_all() clears the poison on every tracked allocation, which also
 *   clears any taint that propagated into those buffers downstream of an
 *   originally-poisoned buffer (e.g. a ciphertext derived from a poisoned
 *   key).  memcpy/memcmp comparisons must run on unpoisoned data: the
 *   emulator faults when a branch depends on a tainted value, and memcmp
 *   branches internally.  So every comparison block in a test runs
 *   unpoison_all() before the memcpy/memcmp and repoison_all() after it.
 * - repoison_all() restores the poison on exactly those tracked allocations
 *   that poison_alloc() poisoned when they were made.
 * - free_poison_allocations() unpoisons every tracked allocation, frees them
 *   all, and empties the table.  It is safe to call any number of times:
 *   after it returns, poison_alloc() can hand out fresh allocations (the
 *   table keeps its capacity).  Tests call it at the end of every test
 *   function (or loop body) that allocated poison allocations, so the
 *   allocations die as early as possible; buffers allocated directly in
 *   main are freed once, at the end of main.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buffer kinds.  One bit per kind; --poison-* options set bits in
 * poison_alloc's poison-kind mask. */
enum {
    POISON_NONE   = 0,
    POISON_INPUT  = 1u << 0,
    POISON_OUTPUT = 1u << 1,
    POISON_KEY    = 1u << 2,
    POISON_IV     = 1u << 3,
    POISON_AAD    = 1u << 4,
    POISON_TAG    = 1u << 5,
    POISON_SALT   = 1u << 6,
    POISON_OTHER  = 1u << 7,
    POISON_ALL    = 0xffu
};

/* malloc() plus conditional poisoning.  Called only by the ALLOCATE_*
 * functions defined at the top of each test program.  Every allocation is
 * recorded so that unpoison_all(), repoison_all(), and
 * free_poison_allocations() can find it. */
void *poison_alloc(unsigned kind, size_t size);

/* Every test program calls this first thing in main(). */
void poison_parse_args(int argc, char **argv);

/* Prints the --poison-* option help. */
void poison_print_usage(FILE *out);

/* Comparison discipline.  unpoison_all() clears the poison on every
 * allocation poison_alloc() handed out; repoison_all() restores the poison
 * on exactly those that were poisoned when they were made.  Tests bracket
 * every memcpy/memcmp comparison block with the two of them. */
void unpoison_all(void);
void repoison_all(void);

/* Unpoisons every tracked allocation, frees them all, and empties the
 * table.  Safe to call any number of times: after it returns, poison_alloc()
 * can hand out fresh allocations (the table keeps its capacity).  Tests call
 * this at the end of every test function (or loop body) that allocated
 * poison allocations; buffers allocated directly in main are freed once, at
 * the end of main. */
void free_poison_allocations(void);

#endif /* POISONING_H */
