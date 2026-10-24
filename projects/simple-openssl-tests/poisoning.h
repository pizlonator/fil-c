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
 * poison_range() is a no-op.  It exists so that there is exactly one hook
 * where a real poisoning implementation (e.g. a Fil-C/GIMSO poisoning API)
 * can be installed later without touching any test program.
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

/* The poisoning hook.  No-op by design; see the comment at the top. */
static void poison_range(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
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
            "  --poison-help     show this help\n");
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
