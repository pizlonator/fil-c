/* test_common.h */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "poisoning.h"
#include <stddef.h>
#include <openssl/evp.h>

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
#define ALLOCATE_KEY(n)    poison_alloc(POISON_KEY, (n))
#define ALLOCATE_IV(n)     poison_alloc(POISON_IV, (n))
#define ALLOCATE_INPUT(n)  poison_alloc(POISON_INPUT, (n))
#define ALLOCATE_OUTPUT(n) poison_alloc(POISON_OUTPUT, (n))
#define ALLOCATE_AAD(n)    poison_alloc(POISON_AAD, (n))
#define ALLOCATE_TAG(n)    poison_alloc(POISON_TAG, (n))
#define ALLOCATE_SALT(n)   poison_alloc(POISON_SALT, (n))
#define ALLOCATE_OTHER(n)  poison_alloc(POISON_OTHER, (n))

extern unsigned g_checks;

/* Prints "FAIL <program>: <msg>" to stderr and exits 1; <program> is the
 * basename of argv[0] recorded by test_parse_args(). */
void test_failed(const char *msg);

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

int hexval(char c);

/* Copies a hex constant into a labeled working buffer (house rule: constants
 * live in the source as hex strings, but are copied into working buffers
 * before use).  Returns the byte length. */
size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap);

/* Length-and-content comparison of two working buffers, run under the
 * unpoison_all()/repoison_all() comparison discipline (memcmp cannot run on
 * poisoned data; see poisoning.h). */
void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                  const unsigned char *want, size_t wantlen);

/* Every test program calls this first thing in main(); it calls
 * poison_parse_args(argc, argv) and records the program name. */
void test_parse_args(int argc, char **argv);

/* The program name recorded by test_parse_args() (argv[0] basename). */
const char *test_prog_name(void);

/* Prints "SKIP <program>: <reason>" and exits 0; for whole-program version
 * gates. */
void test_skip(const char *reason);

/* Loads a provider by name, CHECK()ing success, and returns the handle. */
OSSL_PROVIDER *test_load_provider(const char *name);

/* Prints "<program>: all %u checks passed". */
void test_report_pass(void);

#endif /* TEST_COMMON_H */
