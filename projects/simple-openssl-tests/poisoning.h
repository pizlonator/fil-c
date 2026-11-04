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
 * --poison-key, ...), the ALLOCATE_* function calls poison_range() on what
 * malloc() returned.  Without the command line option, poison_range() is not
 * called for that buffer.
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
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zegarmistrz.h>

/* Buffer kinds.  One bit per kind; --poison-* options set bits in
 * g_poison_mask. */
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

/* Which buffer kinds the user asked to poison (bitmask of POISON_*). */
static unsigned g_poison_mask = POISON_NONE;

/* Which zegarmistrz poisoning variants to taint poisoned ranges with
 * (bitmask of ZEGARMISTRZ_POISON_*).  Zero means "do not poison at all" --
 * poison options never change what a program computes unless the user both
 * selects buffer kinds AND selects at least one variant. */
static unsigned g_poison_variants = 0;

/* The single poisoning hook.  When running under the zegarmistrz emulator,
 * poisons [ptr, ptr+size) with the selected variant bits.  Outside the
 * emulator (and when no variant is selected) this is a no-op, so poison
 * options never change what a program computes.  Only CANNOT_BRANCH,
 * CANNOT_INDEX, and CANNOT_VAR_TIME_OP are supported -- zegarmistrz's
 * CANNOT_LOAD/CANNOT_STORE, CLEAR_*_ON_STORE, and CLEAR_VAR_TIME_* bits are
 * deliberately not exposed. */
static void poison_range(void *ptr, size_t size)
{
    if (!g_poison_variants)
        return;
    zegarmistrz_poison_range(ptr, size, g_poison_variants);
}

/* malloc() plus conditional poisoning.  Called only by the ALLOCATE_*
 * functions defined at the top of each test program. */
static void *poison_alloc(unsigned kind, size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL) {
        fprintf(stderr, "fatal: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    if (g_poison_mask & kind)
        poison_range(p, size);
    return p;
}

static void poison_print_usage(FILE *out)
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
 * spec), which clear the selection.  Unknown tokens exit with status 2, matching the
 * poison-kind error style. */
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
static void poison_parse_args(int argc, char **argv)
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

#endif /* POISONING_H */
