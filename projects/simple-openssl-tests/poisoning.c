/*
 * poisoning.c -- implementation of the poisoning.h API, shared by every
 * program in projects/simple-openssl-tests.
 *
 * Everything that used to be static inline in poisoning.h lives here, so
 * that the tests link one common poisoning.o instead of each compiling
 * their own private copy of the poisoning code (see the Makefile).
 * poison_range() is private to this file: tests poison buffers exclusively
 * through poison_alloc().
 *
 * poison_alloc() records every allocation it hands out -- poisoned or not,
 * along with whether the poison was actually applied -- in a resizable
 * table.  That table powers the three maintenance APIs:
 *
 * - unpoison_all() clears the poison bits on every tracked allocation.
 *   This also clears taint that propagated into those buffers downstream of
 *   an originally-poisoned buffer (e.g. a ciphertext derived from a poisoned
 *   key), which is what lets the tests run memcpy/memcmp comparisons
 *   without fatally faulting under the emulator.
 * - repoison_all() restores the poison on exactly those tracked allocations
 *   that poison_alloc() poisoned when they were made.  It deliberately does
 *   not poison allocations that were never poisoned: re-poisoning clean
 *   data would newly taint it and change what the emulator faults on.
 * - free_poison_allocations() unpoisons every tracked allocation, frees
 *   them, and empties the table.  It is safe to call any number of times:
 *   after it returns, poison_alloc() can hand out fresh allocations (the
 *   table keeps its capacity).  Tests call it at the end of every test
 *   function (or loop body) that allocated poison allocations, so the
 *   allocations die as early as possible; buffers allocated directly in
 *   main are freed once, at the end of main.
 */

#include "poisoning.h"

#include <zegarmistrz.h>

/* Which buffer kinds the user asked to poison (bitmask of POISON_*). */
static unsigned g_poison_mask = POISON_NONE;

/* Which zegarmistrz poisoning variants to taint poisoned ranges with
 * (bitmask of ZEGARMISTRZ_POISON_*).  Zero means "do not poison at all" --
 * poison options never change what a program computes unless the user both
 * selects buffer kinds AND selects at least one variant. */
static unsigned g_poison_variants = 0;

/* One tracked poison_alloc() allocation. */
typedef struct {
    void *ptr;      /* the malloc() result */
    size_t size;    /* the size the caller asked for */
    int poisoned;   /* nonzero if poison_range() was applied to it */
} poison_allocation;

/* Every allocation poison_alloc() has handed out, poisoned or not. */
static poison_allocation *g_poison_allocations = NULL;
static size_t g_poison_allocation_count = 0;
static size_t g_poison_allocation_cap = 0;

static void track_poison_allocation(void *ptr, size_t size, int poisoned)
{
    if (g_poison_allocation_count == g_poison_allocation_cap) {
        size_t cap = g_poison_allocation_cap ? g_poison_allocation_cap * 2 : 64;
        poison_allocation *allocations =
            realloc(g_poison_allocations, cap * sizeof(poison_allocation));
        if (allocations == NULL) {
            fprintf(stderr,
                    "fatal: out of memory tracking poison allocations\n");
            exit(1);
        }
        g_poison_allocations = allocations;
        g_poison_allocation_cap = cap;
    }
    g_poison_allocations[g_poison_allocation_count].ptr = ptr;
    g_poison_allocations[g_poison_allocation_count].size = size;
    g_poison_allocations[g_poison_allocation_count].poisoned = poisoned;
    g_poison_allocation_count++;
}

/* The single poisoning hook.  When running under the zegarmistrz emulator,
 * poisons [ptr, ptr+size) with the selected variant bits.  Outside the
 * emulator (and when no variant is selected) this is a no-op, so poison
 * options never change what a program computes.  Only CANNOT_BRANCH,
 * CANNOT_INDEX, and CANNOT_VAR_TIME_OP are supported -- zegarmistrz's
 * CANNOT_LOAD/CANNOT_STORE, CLEAR_*_ON_STORE, and CLEAR_VAR_TIME_* bits are
 * deliberately not exposed.  Private to this file. */
static void poison_range(void *ptr, size_t size)
{
    if (!g_poison_variants)
        return;
    zegarmistrz_poison_range(ptr, size, g_poison_variants);
}

void *poison_alloc(unsigned kind, size_t size)
{
    void *p = malloc(size ? size : 1);
    int poisoned;
    if (p == NULL) {
        fprintf(stderr, "fatal: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    poisoned = (g_poison_mask & kind) != 0;
    if (poisoned)
        poison_range(p, size);
    track_poison_allocation(p, size, poisoned);
    return p;
}

void unpoison_all(void)
{
    size_t i;
    if (!g_poison_variants)
        return;
    for (i = 0; i < g_poison_allocation_count; i++) {
        zegarmistrz_unpoison_range(g_poison_allocations[i].ptr,
                                   g_poison_allocations[i].size);
    }
}

void repoison_all(void)
{
    size_t i;
    if (!g_poison_variants)
        return;
    for (i = 0; i < g_poison_allocation_count; i++) {
        if (g_poison_allocations[i].poisoned)
            poison_range(g_poison_allocations[i].ptr,
                         g_poison_allocations[i].size);
    }
}

void free_poison_allocations(void)
{
    size_t i;
    for (i = 0; i < g_poison_allocation_count; i++) {
        zegarmistrz_unpoison_range(g_poison_allocations[i].ptr,
                                   g_poison_allocations[i].size);
        free(g_poison_allocations[i].ptr);
    }
    g_poison_allocation_count = 0;
}

/* (The --poison-* option parsing below is moved verbatim from poisoning.h;
 * only the static-ness of the two entry points changed.) */

void poison_print_usage(FILE *out)
{
    fprintf(out,
            "poison options:\n"
            "  --poison-input    poison the input buffer(s)\n"
            "  --poison-output   poison the output buffer(s)\n"
            "  --poison-key      poison key buffers\n"
            "  --poison-iv       poison IV/nonce buffers\n"
            "  --poison-aad      poison AAD buffers\n"
            "  --poison-tag      poison tag buffers\n"
            "  --poison-salt     poison salt/seed/info buffers\n"
            "  --poison-other    poison any other working buffer\n"
            "  --poison-all      poison every buffer kind\n"
            "  --poison-none     poison nothing (the default)\n"
            "  --poison=a,b,...  poison the listed kinds\n"
            "  --poison-variants=s  poison with the listed zegarmistrz\n"
            "                    variant(s): cannot-branch, cannot-index,\n"
            "                    cannot-var-time-op, or all/none; replaces\n"
            "                    the current variant selection\n"
            "  --poison-help     show this help\n"
            "environment:\n"
            "  POISON_VARIANTS   default --poison-variants= spec (a\n"
            "                    --poison-variants= option on the command\n"
            "                    line overrides it)\n"
            "notes:\n"
            "  Poisoning only takes effect under the zegarmistrz emulator;\n"
            "  elsewhere every poison option is a no-op, so poison options\n"
            "  never change what a program computes.\n");
}

static unsigned poison_kind_from_name(const char *name)
{
    if (strcmp(name, "input") == 0)  return POISON_INPUT;
    if (strcmp(name, "output") == 0) return POISON_OUTPUT;
    if (strcmp(name, "key") == 0)    return POISON_KEY;
    if (strcmp(name, "iv") == 0)     return POISON_IV;
    if (strcmp(name, "nonce") == 0)  return POISON_IV;
    if (strcmp(name, "aad") == 0)    return POISON_AAD;
    if (strcmp(name, "tag") == 0)    return POISON_TAG;
    if (strcmp(name, "salt") == 0)   return POISON_SALT;
    if (strcmp(name, "seed") == 0)   return POISON_SALT;
    if (strcmp(name, "info") == 0)   return POISON_SALT;
    if (strcmp(name, "other") == 0)  return POISON_OTHER;
    if (strcmp(name, "all") == 0)    return POISON_ALL;
    return POISON_NONE;
}

/* Parses a comma-separated zegarmistrz variant spec into *mask (replacing
 * whatever *mask held).  Accepted tokens: cannot-branch, cannot-index,
 * cannot-var-time-op, all (the three of them OR'd), none (and the empty
 * spec), which clear the selection.  Unknown tokens exit with status 2,
 * matching the poison-kind error style. */
static void poison_variants_from_spec(const char *spec, unsigned *mask)
{
    *mask = 0;
    if (spec == NULL)
        return;
    const char *p = spec;
    for (;;) {
        const char *comma = strchr(p, ',');
        char name[32];
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len == 0) {
            if (comma == NULL)
                break;
            p = comma + 1;
            continue;
        }
        if (len >= sizeof(name)) {
            fprintf(stderr, "bad poison variant in %s\n", spec);
            exit(2);
        }
        memcpy(name, p, len);
        name[len] = '\0';
        if (strcmp(name, "cannot-branch") == 0)
            *mask |= ZEGARMISTRZ_POISON_CANNOT_BRANCH;
        else if (strcmp(name, "cannot-index") == 0)
            *mask |= ZEGARMISTRZ_POISON_CANNOT_INDEX;
        else if (strcmp(name, "cannot-var-time-op") == 0)
            *mask |= ZEGARMISTRZ_POISON_CANNOT_VAR_TIME_OP;
        else if (strcmp(name, "all") == 0)
            *mask |= ZEGARMISTRZ_POISON_CANNOT_BRANCH
                   | ZEGARMISTRZ_POISON_CANNOT_INDEX
                   | ZEGARMISTRZ_POISON_CANNOT_VAR_TIME_OP;
        else if (strcmp(name, "none") != 0) {
            fprintf(stderr, "unknown poison variant: %s\n", name);
            exit(2);
        }
        if (comma == NULL)
            break;
        p = comma + 1;
    }
}

/* Handles one --poison-* argument.  Returns 1 if the argument was consumed. */
static int poison_handle_arg(const char *arg)
{
    if (strcmp(arg, "--poison-input") == 0)  { g_poison_mask |= POISON_INPUT;  return 1; }
    if (strcmp(arg, "--poison-output") == 0) { g_poison_mask |= POISON_OUTPUT; return 1; }
    if (strcmp(arg, "--poison-key") == 0)    { g_poison_mask |= POISON_KEY;    return 1; }
    if (strcmp(arg, "--poison-iv") == 0)     { g_poison_mask |= POISON_IV;     return 1; }
    if (strcmp(arg, "--poison-aad") == 0)    { g_poison_mask |= POISON_AAD;    return 1; }
    if (strcmp(arg, "--poison-tag") == 0)    { g_poison_mask |= POISON_TAG;    return 1; }
    if (strcmp(arg, "--poison-salt") == 0)   { g_poison_mask |= POISON_SALT;   return 1; }
    if (strcmp(arg, "--poison-other") == 0)  { g_poison_mask |= POISON_OTHER;  return 1; }
    if (strcmp(arg, "--poison-all") == 0)    { g_poison_mask |= POISON_ALL;    return 1; }
    if (strcmp(arg, "--poison-none") == 0)   { g_poison_mask  = POISON_NONE;   return 1; }
    if (strcmp(arg, "--poison-help") == 0)   { poison_print_usage(stdout);     exit(0); }
    if (strncmp(arg, "--poison-variants=", 18) == 0) {
        poison_variants_from_spec(arg + 18, &g_poison_variants);
        return 1;
    }
    if (strncmp(arg, "--poison=", 9) == 0) {
        const char *p = arg + 9;
        for (;;) {
            const char *comma = strchr(p, ',');
            char name[32];
            size_t len = comma ? (size_t)(comma - p) : strlen(p);
            unsigned bit;
            if (len == 0 || len >= sizeof(name)) {
                fprintf(stderr, "bad poison kind in %s\n", arg);
                exit(2);
            }
            memcpy(name, p, len);
            name[len] = '\0';
            bit = poison_kind_from_name(name);
            if (bit == POISON_NONE) {
                fprintf(stderr, "unknown poison kind: %s\n", name);
                exit(2);
            }
            g_poison_mask |= bit;
            if (comma == NULL)
                break;
            p = comma + 1;
        }
        return 1;
    }
    return 0;
}

/* Every test program calls this first thing in main(). */
void poison_parse_args(int argc, char **argv)
{
    /* POISON_VARIANTS provides a default variant selection so that poison
     * runs do not need per-test argument plumbing, e.g.
     * `POISON_VARIANTS=cannot-branch ./test_x --poison-key`.  An explicit
     * --poison-variants= on the command line (parsed below) overrides it. */
    const char *env = getenv("POISON_VARIANTS");
    if (env != NULL && env[0] != '\0')
        poison_variants_from_spec(env, &g_poison_variants);
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            poison_print_usage(stdout);
            exit(0);
        }
        if (poison_handle_arg(argv[i]))
            continue;
        fprintf(stderr, "usage: %s [--poison-...]\n", argv[0]);
        poison_print_usage(stderr);
        fprintf(stderr, "unknown argument: %s\n", argv[i]);
        exit(2);
    }
}
