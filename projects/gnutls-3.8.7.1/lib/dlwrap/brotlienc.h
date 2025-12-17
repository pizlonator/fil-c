/*
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

#ifndef GNUTLS_LIB_DLWRAP_BROTLIENC_H_
#define GNUTLS_LIB_DLWRAP_BROTLIENC_H_

#include <brotli/encode.h>

#if defined(GNUTLS_BROTLIENC_ENABLE_DLOPEN) && GNUTLS_BROTLIENC_ENABLE_DLOPEN

#define FUNC(ret, name, args, cargs)		\
  ret gnutls_brotlienc_func_##name args;
#define VOID_FUNC FUNC
#include "brotliencfuncs.h"
#undef VOID_FUNC
#undef FUNC

#define GNUTLS_BROTLIENC_FUNC(name) gnutls_brotlienc_func_##name

#else

#define GNUTLS_BROTLIENC_FUNC(name) name

#endif /* GNUTLS_BROTLIENC_ENABLE_DLOPEN */

/* Ensure SONAME to be loaded with dlopen FLAGS, and all the necessary
 * symbols are resolved.
 *
 * Returns 0 on success; negative error code otherwise.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
int gnutls_brotlienc_ensure_library (const char *soname, int flags);

/* Unload library and reset symbols.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
void gnutls_brotlienc_unload_library (void);

/* Return 1 if the library is loaded and usable.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
unsigned gnutls_brotlienc_is_usable (void);

#endif /* GNUTLS_LIB_DLWRAP_BROTLIENC_H_ */
