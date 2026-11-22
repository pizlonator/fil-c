/*
 * test_common.c -- implementation of the test_common.h API: the shared
 * boilerplate that every test program used to duplicate at the top of its
 * source.
 *
 * The labeled ALLOCATE_* wrappers live in test_common.h as one-line macros
 * over poison_alloc(); everything else -- the failure/skip reporting, the
 * hex helpers, the comparison discipline, and the main() plumbing -- lives
 * here.  Every test binary links this object (see the Makefile), next to
 * poisoning.o.
 *
 * Output contract (the whole suite's scripts depend on it):
 *
 *   - failure:  "FAIL <program>: <msg>" on stderr, exit status 1;
 *   - skip:     "SKIP <program>: <reason>" on stdout, exit status 0;
 *   - success:  "<program>: all N checks passed" on stdout, exit status 0.
 *
 * <program> is the basename of argv[0], recorded by test_parse_args() and
 * identical to the program name the sources used to hardcode.
 */

#include "test_common.h"

/* Direct includes: don't rely on poisoning.h transitively providing snprintf/strrchr/exit/memcmp/strlen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/provider.h>

/* The program name (argv[0] basename) recorded by test_parse_args().
 * Preinitialized so that even a test_failed() before main()'s first line
 * prints a well-formed FAIL line. */
static char g_prog_name[256] = "test";

unsigned g_checks = 0;

void test_parse_args(int argc, char **argv)
{
    poison_parse_args(argc, argv);
    if (argc > 0 && argv[0] != NULL) {
        const char *base = strrchr(argv[0], '/');

        snprintf(g_prog_name, sizeof(g_prog_name), "%s",
                 base != NULL ? base + 1 : argv[0]);
    }
}

const char *test_prog_name(void)
{
    return g_prog_name;
}

void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL %s: %s\n", test_prog_name(), msg);
    exit(1);
}

void test_skip(const char *reason)
{
    printf("SKIP %s: %s\n", test_prog_name(), reason);
    exit(0);
}

void test_report_pass(void)
{
    unpoison(&g_checks, sizeof(g_checks));
    printf("%s: all %u checks passed\n", test_prog_name(), g_checks);
}

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Length-and-content comparison of two working buffers.  memcmp() cannot be
 * run on poisoned data: it branches on the compared bytes, which fatally
 * faults under the emulator when the data is tainted (directly or via
 * downstream propagation).  Unpoison everything poison_alloc() handed out,
 * compare, then restore the poison. */
void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                  const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0) {
        unpoison_all();
        CHECK(memcmp(got, want, wantlen) == 0, what);
        repoison_all();
    }
}

OSSL_PROVIDER *test_load_provider(const char *name)
{
    OSSL_PROVIDER *p = OSSL_PROVIDER_load(NULL, name);

    CHECK(p != NULL, "OSSL_PROVIDER_load failed");
    return p;
}
