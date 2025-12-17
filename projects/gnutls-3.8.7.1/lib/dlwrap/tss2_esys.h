/*
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

#ifndef GNUTLS_LIB_DLWRAP_TSS2_ESYS_H_
#define GNUTLS_LIB_DLWRAP_TSS2_ESYS_H_

#include <tss2/tss2_esys.h>

#if defined(GNUTLS_TSS2_ESYS_ENABLE_DLOPEN) && GNUTLS_TSS2_ESYS_ENABLE_DLOPEN

#define FUNC(ret, name, args, cargs)		\
  ret gnutls_tss2_esys_func_##name args;
#define VOID_FUNC FUNC
#include "tss2_esysfuncs.h"
#undef VOID_FUNC
#undef FUNC

#define GNUTLS_TSS2_ESYS_FUNC(name) gnutls_tss2_esys_func_##name

#else

#define GNUTLS_TSS2_ESYS_FUNC(name) name

#endif /* GNUTLS_TSS2_ESYS_ENABLE_DLOPEN */

/* Ensure SONAME to be loaded with dlopen FLAGS, and all the necessary
 * symbols are resolved.
 *
 * Returns 0 on success; negative error code otherwise.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
int gnutls_tss2_esys_ensure_library (const char *soname, int flags);

/* Unload library and reset symbols.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
void gnutls_tss2_esys_unload_library (void);

/* Return 1 if the library is loaded and usable.
 *
 * Note that this function is NOT thread-safe; when calling it from
 * multi-threaded programs, protect it with a locking mechanism.
 */
unsigned gnutls_tss2_esys_is_usable (void);

#endif /* GNUTLS_LIB_DLWRAP_TSS2_ESYS_H_ */
