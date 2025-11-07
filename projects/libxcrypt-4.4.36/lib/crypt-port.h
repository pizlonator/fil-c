/* Portability glue for libcrypt.

   Copyright 2007-2017 Thorsten Kukuk and Zack Weinberg
   Copyright 2018-2019 Björn Esser

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _CRYPT_PORT_H
#define _CRYPT_PORT_H 1

#ifndef HAVE_CONFIG_H
#error "Run configure before compiling; see INSTALL for instructions"
#endif

#include "config.h"

#undef NDEBUG
#include <assert.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* unistd.h may contain declarations of crypt, crypt_r, crypt_data,
   encrypt, and setkey; if present, they may be incompatible with our
   declarations.  Rename them out of the way with macros.  This needs
   to be done before we include crypt-symbol-vers.h, which defines
   macros with the same names for symbol-versioning purposes.  */
#ifdef HAVE_UNISTD_H
#define crypt unistd_crypt_is_incompatible
#define crypt_r unistd_crypt_r_is_incompatible
#define crypt_data unistd_crypt_data_is_incompatible
#define encrypt unistd_encrypt_is_incompatible
#define setkey unistd_setkey_is_incompatible
#include <unistd.h>
#undef crypt
#undef crypt_r
#undef crypt_data
#undef encrypt
#undef setkey
#endif

#ifndef HAVE_SYS_CDEFS_THROW
#define __THROW /* nothing */
#endif

/* Suppression of unused-argument warnings.  */
#if defined __GNUC__ && __GNUC__ >= 3
# define ARG_UNUSED(x) x __attribute__ ((__unused__))
#else
# define ARG_UNUSED(x) x
#endif

/* Functions that should not be inlined.  */
#if defined __GNUC__ && __GNUC__ >= 3
# define NO_INLINE __attribute__ ((__noinline__))
#else
# error "Don't know how to prevent function inlining"
#endif

/* C99 Static array indices in function parameter declarations.  Syntax
   such as:  void bar(int myArray[static 10]);  is allowed in C99, but
   not all compiler support it properly.  Define MIN_SIZE appropriately
   so headers using it can be compiled using any compiler.
   Use like this:  void bar(int myArray[MIN_SIZE(10)]);  */
#if (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L) && \
    ((defined __GNUC__ && __GNUC__ > 4) || defined __clang__)
#define MIN_SIZE(x) static (x)
#else
#define MIN_SIZE(x) (x)
#endif

/* Detect system endianness.  */
#if ENDIANNESS_IS_BIG
# define XCRYPT_USE_BIGENDIAN 1
#elif ENDIANNESS_IS_LITTLE
# define XCRYPT_USE_BIGENDIAN 0
#elif ENDIANNESS_IS_PDP
# error "Byte-order sensitive code in libxcrypt does not support PDP-endianness"
#else
# error "Unable to determine byte ordering"
#endif

/* static_assert shim.  */
#ifdef HAVE_STATIC_ASSERT_IN_ASSERT_H
/* nothing to do */
#elif defined HAVE__STATIC_ASSERT
# define static_assert(expr, message) _Static_assert(expr, message)
#else
/* This fallback is known to work with most C99-compliant compilers.
   See verify.h in gnulib for extensive discussion.  */
# define static_assert(expr, message) \
  extern int (*xcrypt_static_assert_fn (void)) \
  [!!sizeof (struct { int xcrypt_error_if_negative: (expr) ? 2 : -1; })]
#endif

/* max_align_t shim.  In the absence of official word from the
   compiler, we guess that one of long double, uintmax_t, void *, and
   void (*)(void) will have the maximum alignment.  This is probably
   not true in the presence of vector types, but we currently don't
   use vector types, and hopefully any compiler with extra-aligned
   vector types will provide max_align_t.  */
#ifndef HAVE_MAX_ALIGN_T
typedef union
{
  long double ld;
  uintmax_t ui;
  void *vp;
  void (*vpf)(void);
} max_align_t;
#endif

/* Several files expect the traditional definitions of these macros.
   (We don't trust sys/param.h to define them correctly.)  */
#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* Size of a declared array.  */
#define ARRAY_SIZE(a_)  (sizeof (a_) / sizeof ((a_)[0]))

/* Not all systems provide a library function usable for erasing
   memory containing sensitive data, and among those that do, there is
   no standard for what it should be called.  (Plain memset and bzero
   are not usable for this purpose, because the compiler may remove
   calls to these functions if it thinks the stores are dead.)

   All code in libxcrypt is standardized on explicit_bzero() as the
   name for the function that does this job; here, we map that name
   to whatever platform routine is available, or to our own fallback
   implementation.  */
#define INCLUDE_explicit_bzero 0
#if defined HAVE_EXPLICIT_BZERO
/* nothing to do */
#elif defined HAVE_EXPLICIT_MEMSET
#define explicit_bzero(s, len) explicit_memset(s, 0, len)
#elif defined HAVE_MEMSET_S
#define explicit_bzero(s, len) memset_s(s, len, 0, len)
#else
/* activate our fallback implementation */
#undef INCLUDE_explicit_bzero
#define INCLUDE_explicit_bzero 1
#define explicit_bzero _crypt_explicit_bzero
extern void explicit_bzero (void *, size_t);
#endif

/* Provide a safe way to copy strings with the guarantee src,
   including its terminating '\0', will fit d_size bytes.
   The trailing bytes of d_size will be filled with '\0'.
   dst and src must not be NULL.  Returns strlen (src).
   Note: dst and src are declared as void * instead of char *
   because some of the hashing methods want to call this
   function with unsigned char * arguments.  */
#define strcpy_or_abort _crypt_strcpy_or_abort
extern size_t strcpy_or_abort (void *dst, size_t d_size, const void *src);


/* Define ALIASNAME as a strong alias for NAME.  */
#define strong_alias(name, aliasname) _strong_alias(name, aliasname)

/* Darwin (Mach-O) doesn't support alias attributes or symbol versioning.
   It does, however, support symbol aliasing at the object file level.  */
#ifdef __APPLE__

# define _strong_alias(name, aliasname)         \
  __asm__(".globl _" #aliasname);               \
  __asm__(".set _" #aliasname ", _" #name);     \
  extern __typeof(name) aliasname __THROW

# define symver_set(extstr, intname, version, mode)     \
  __asm__(".globl _" extstr);                           \
  __asm__(".set _" extstr ", _" #intname)

#elif defined _WIN32

/* .symver is only supported for ELF format, Windows uses COFF/PE */
# define symver_set(extstr, intname, version, mode)

#elif defined __GNUC__ && __GNUC__ >= 3

# define _strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __THROW __attribute__ ((alias (#name)))

/* Starting with GCC 10, we can use the symver attribute, which is also
   needed at the point we decide to enable link-time optimization.  */
# if defined HAVE_FUNC_ATTRIBUTE_SYMVER

/* Set the symbol version for EXTNAME, which uses INTNAME as its
   implementation.  */
# define symver_set(extstr, intname, version, mode) \
  extern __typeof (intname) intname __THROW \
    __attribute__((symver (extstr mode #version)))

/* Referencing specific _compatibility_ symbols still needs inline asm.  */
# define _symver_ref(extstr, intname, version) \
  __asm__ (".symver " #intname "," extstr "@" #version)

# else

/* Set the symbol version for EXTNAME, which uses INTNAME as its
   implementation.  */
# define symver_set(extstr, intname, version, mode) \
  __asm__ (".symver " #intname "," extstr mode #version)

# endif

#else
# error "Don't know how to do symbol versioning with this compiler"
#endif

/* A construct with the same syntactic role as the expansion of symver_set,
   but which does nothing.  */
#define symver_nop() __asm__ ("")

/* The macros for versioned symbols work differently in this library
   than they do in glibc.  They are mostly auto-generated
   (see build-aux/scripts/gen-crypt-symbol-vers-h)
   and we currently don't support compatibility symbols that need a different
   definition from the default version.

   Each definition of a public symbol should look like this:
   #if INCLUDE_foo
   int foo(arguments)
   {
     body
   }
   SYMVER_foo;
   #endif

   and the macros take care of the rest.  Normally, to call a public
   symbol you do nothing special.  The macro symver_ref() forces
   all uses of a particular name (in the file where it's used) to refer
   to a particular version of a public symbol, e.g. for testing.  */

#ifdef IN_LIBCRYPT

#include "crypt-symbol-vers.h"

#ifdef PIC

#define symver_compat(n, extstr, extname, intname, version) \
  strong_alias (intname, extname ## __ ## n); \
  symver_set (extstr, extname ## __ ## n, version, "@")

#define symver_compat0(extstr, intname, version) \
  symver_set (extstr, intname, version, "@")

#define symver_default(extstr, intname, version) \
  symver_set (extstr, intname, version, "@@")

#else

/* When not building the shared library, don't do any of this.  */
#define symver_compat(n, extstr, extname, intname, version) symver_nop ()
#define symver_compat0(extstr, intname, version) symver_nop ()
#define symver_default(extstr, intname, version) symver_nop ()

#endif
#endif

/* Tests may need to _refer_ to compatibility symbols, but should never need
   to _define_ them.  */
#define symver_ref(extstr, intname, version) \
  _symver_ref(extstr, intname, version)

/* Generic way for referencing specific _compatibility_ symbols.  */
#ifndef _symver_ref
#define _symver_ref(extstr, intname, version) \
  symver_set(extstr, intname, version, "@")
#endif

/* Define configuration macros used during compile-time by the
   GOST R 34.11-2012 "Streebog" hash function.  */
#if XCRYPT_USE_BIGENDIAN
#define __GOST3411_BIG_ENDIAN__ 1
#else
#define __GOST3411_LITTLE_ENDIAN__ 1
#endif

/* Get the set of hash algorithms to be included and some related
   definitions.  */
#include "crypt-hashes.h"

/* Rename all of the internal-but-global symbols with a _crypt_ prefix
   so that they do not interfere with other people's code when linking
   statically.  This list cannot be autogenerated, but is validated by
   test-symbols.sh.  */

#define ascii64                  _crypt_ascii64
#define get_random_bytes         _crypt_get_random_bytes
#define make_failure_token       _crypt_make_failure_token

#if INCLUDE_descrypt || INCLUDE_bsdicrypt || INCLUDE_bigcrypt
#define des_crypt_block          _crypt_des_crypt_block
#define des_set_key              _crypt_des_set_key
#define des_set_salt             _crypt_des_set_salt
#define comp_maskl               _crypt_comp_maskl
#define comp_maskr               _crypt_comp_maskr
#define fp_maskl                 _crypt_fp_maskl
#define fp_maskr                 _crypt_fp_maskr
#define ip_maskl                 _crypt_ip_maskl
#define ip_maskr                 _crypt_ip_maskr
#define key_perm_maskl           _crypt_key_perm_maskl
#define key_perm_maskr           _crypt_key_perm_maskr
#define m_sbox                   _crypt_m_sbox
#define psbox                    _crypt_psbox
#endif

#if INCLUDE_nt
#define MD4_Init   _crypt_MD4_Init
#define MD4_Update _crypt_MD4_Update
#define MD4_Final  _crypt_MD4_Final
#endif

#if INCLUDE_md5crypt || INCLUDE_sunmd5
#define MD5_Init   _crypt_MD5_Init
#define MD5_Update _crypt_MD5_Update
#define MD5_Final  _crypt_MD5_Final
#endif

#if INCLUDE_sha1crypt
#define hmac_sha1_process_data   _crypt_hmac_sha1_process_data
#define sha1_finish_ctx          _crypt_sha1_finish_ctx
#define sha1_init_ctx            _crypt_sha1_init_ctx
#define sha1_process_bytes       _crypt_sha1_process_bytes
#endif

#if INCLUDE_sha512crypt
#define libcperciva_SHA512_Init   _crypt_SHA512_Init
#define libcperciva_SHA512_Update _crypt_SHA512_Update
#define libcperciva_SHA512_Final  _crypt_SHA512_Final
#define libcperciva_SHA512_Buf    _crypt_SHA512_Buf
#endif

#if INCLUDE_md5crypt || INCLUDE_sha256crypt || INCLUDE_sha512crypt
#define gensalt_sha_rn           _crypt_gensalt_sha_rn
#endif

#if INCLUDE_yescrypt || INCLUDE_scrypt || INCLUDE_gost_yescrypt
#define PBKDF2_SHA256            _crypt_PBKDF2_SHA256
#define crypto_scrypt            _crypt_crypto_scrypt
#define yescrypt                 _crypt_yescrypt
#define yescrypt_decode64        _crypt_yescrypt_decode64
#define yescrypt_digest_shared   _crypt_yescrypt_digest_shared
#define yescrypt_encode64        _crypt_yescrypt_encode64
#define yescrypt_encode_params   _crypt_yescrypt_encode_params
#define yescrypt_encode_params_r _crypt_yescrypt_encode_params_r
#define yescrypt_free_local      _crypt_yescrypt_free_local
#define yescrypt_free_shared     _crypt_yescrypt_free_shared
#define yescrypt_init_local      _crypt_yescrypt_init_local
#define yescrypt_init_shared     _crypt_yescrypt_init_shared
#define yescrypt_kdf             _crypt_yescrypt_kdf
#define yescrypt_r               _crypt_yescrypt_r
#define yescrypt_reencrypt       _crypt_yescrypt_reencrypt

#define libcperciva_HMAC_SHA256_Init _crypt_HMAC_SHA256_Init
#define libcperciva_HMAC_SHA256_Update _crypt_HMAC_SHA256_Update
#define libcperciva_HMAC_SHA256_Final _crypt_HMAC_SHA256_Final
#define libcperciva_HMAC_SHA256_Buf _crypt_HMAC_SHA256_Buf
#endif

#if INCLUDE_sha256crypt || INCLUDE_scrypt || INCLUDE_yescrypt || \
    INCLUDE_gost_yescrypt
#define libcperciva_SHA256_Init  _crypt_SHA256_Init
#define libcperciva_SHA256_Update _crypt_SHA256_Update
#define libcperciva_SHA256_Final _crypt_SHA256_Final
#define libcperciva_SHA256_Buf   _crypt_SHA256_Buf
#endif

#if INCLUDE_gost_yescrypt
#define GOST34112012Init       _crypt_GOST34112012_Init
#define GOST34112012Update     _crypt_GOST34112012_Update
#define GOST34112012Final      _crypt_GOST34112012_Final
#define GOST34112012Cleanup    _crypt_GOST34112012_Cleanup
#define gost_hash256           _crypt_gost_hash256
#define gost_hmac256           _crypt_gost_hmac256

/* Those are not present, if gost-yescrypt is selected,
   but yescrypt is not. */
#if !INCLUDE_yescrypt
#define gensalt_yescrypt_rn _crypt_gensalt_yescrypt_rn
extern void gensalt_yescrypt_rn
(unsigned long, const uint8_t *, size_t, uint8_t *, size_t);
#endif
#endif

/* Those are not present, if des-big is selected, but des is not. */
#if INCLUDE_bigcrypt && !INCLUDE_descrypt
#define gensalt_descrypt_rn _crypt_gensalt_descrypt_rn
extern void gensalt_descrypt_rn
(unsigned long, const uint8_t *, size_t, uint8_t *, size_t);
#endif

/* Those are not present, if scrypt is selected, but yescrypt is not. */
#if INCLUDE_scrypt && !INCLUDE_yescrypt
#define crypt_yescrypt_rn _crypt_crypt_yescrypt_rn
extern void crypt_yescrypt_rn (const char *, size_t, const char *,
                               size_t, uint8_t *, size_t, void *, size_t);
#endif

/* We need a prototype for fcrypt for some tests.  */
#if ENABLE_OBSOLETE_API
extern char *fcrypt (const char *key, const char *setting);
#endif

/* Utility functions */

/* Fill BUF with BUFLEN bytes whose values are chosen uniformly at
   random, using a cryptographically strong RNG provided by the
   operating system.  BUFLEN may not be greater than 256.  Returns
   true if all BUFLEN bytes were successfully filled, false otherwise;
   sets errno when it returns false.  Can block.  */
extern bool get_random_bytes (void *buf, size_t buflen);

/* Generate a setting string in the format common to md5crypt,
   sha256crypt, and sha512crypt.  */
extern void gensalt_sha_rn (char tag, size_t maxsalt, unsigned long defcount,
                            unsigned long mincount, unsigned long maxcount,
                            unsigned long count,
                            const uint8_t *rbytes, size_t nrbytes,
                            uint8_t *output, size_t output_size);

/* For historical reasons, crypt and crypt_r are not expected ever
   to return 0, and for internal implementation reasons (see
   call_crypt_fn, in crypt.c), it is simpler if the individual
   algorithms' crypt and gensalt functions return nothing.

   This function generates a "failure token" in the output buffer,
   which is guaranteed not to be equal to any valid password hash or
   setting string, nor to the setting(+hash) string that was passed
   in; thus, a subsequent blind attempt to authenticate someone by
   comparing the output to a previously recorded hash string will
   fail, even if that string is itself one of these "failure tokens".

   We always call this function on the output buffer as the first
   step.  If the individual algorithm's crypt or gensalt function
   succeeds, it overwrites the failure token with real output;
   otherwise the token is left intact, and the API functions that
   _can_ return 0 on error notice it.  */
extern void
make_failure_token (const char *setting, char *output, int size);

/* The base-64 encoding table used by most hashing methods.
   (bcrypt uses a slightly different encoding.)  Size 65
   because it's used as a C string in a few places.  */
extern const unsigned char ascii64[65];

/* Same table gets used with other names in various places.  */
#define b64t   ((const char *) ascii64)
#define itoa64 ascii64

/* Calculate the size of a base64 encoding of N bytes:
   6 bits per output byte, rounded up.  */
#define BASE64_LEN(bytes) ((((bytes) * 8) + 5) / 6)

/* The "scratch" area passed to each of the individual hash functions is
   this big.  */
#define ALG_SPECIFIC_SIZE 8192

#include "crypt.h"

#endif /* crypt-port.h */
