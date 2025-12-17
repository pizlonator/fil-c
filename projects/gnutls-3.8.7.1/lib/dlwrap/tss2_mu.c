/*
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tss2_mu.h"

#if defined(GNUTLS_TSS2_MU_ENABLE_DLOPEN) && GNUTLS_TSS2_MU_ENABLE_DLOPEN

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>

/* If TSS2_MU_LIBRARY_SONAME_UNUSED is defined, dlopen handle can be automatically
 * set; otherwise, the caller needs to call
 * gnutls_tss2_mu_ensure_library with soname determined at run time.
 */
#ifdef TSS2_MU_LIBRARY_SONAME_UNUSED

static void
ensure_library (void)
{
  if (gnutls_tss2_mu_ensure_library (TSS2_MU_LIBRARY_SONAME_UNUSED, RTLD_LAZY | RTLD_LOCAL) < 0)
    abort ();
}

#if defined(GNUTLS_TSS2_MU_ENABLE_PTHREAD) && GNUTLS_TSS2_MU_ENABLE_PTHREAD
#include <pthread.h>

static pthread_once_t dlopen_once = PTHREAD_ONCE_INIT;

#define ENSURE_LIBRARY pthread_once(&dlopen_once, ensure_library)

#else /* GNUTLS_TSS2_MU_ENABLE_PTHREAD */

#define ENSURE_LIBRARY do {	    \
    if (!gnutls_tss2_mu_dlhandle) \
      ensure_library();		    \
  } while (0)

#endif /* !GNUTLS_TSS2_MU_ENABLE_PTHREAD */

#else /* TSS2_MU_LIBRARY_SONAME_UNUSED */

#define ENSURE_LIBRARY do {} while (0)

#endif /* !TSS2_MU_LIBRARY_SONAME_UNUSED */

static void *gnutls_tss2_mu_dlhandle;

/* Define redirection symbols */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"

#if (2 <= __GNUC__ || (4 <= __clang_major__))
#define FUNC(ret, name, args, cargs)			\
  static __typeof__(name)(*gnutls_tss2_mu_sym_##name);
#else
#define FUNC(ret, name, args, cargs)		\
  static ret(*gnutls_tss2_mu_sym_##name)args;
#endif
#define VOID_FUNC FUNC
#include "tss2_mufuncs.h"
#undef VOID_FUNC
#undef FUNC

#pragma GCC diagnostic pop

/* Define redirection wrapper functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"

#define FUNC(ret, name, args, cargs)        \
ret gnutls_tss2_mu_func_##name args           \
{					    \
  ENSURE_LIBRARY;			    \
  assert (gnutls_tss2_mu_sym_##name);	    \
  return gnutls_tss2_mu_sym_##name cargs;	    \
}
#define VOID_FUNC(ret, name, args, cargs)   \
ret gnutls_tss2_mu_func_##name args           \
{					    \
  ENSURE_LIBRARY;			    \
  assert (gnutls_tss2_mu_sym_##name);	    \
  gnutls_tss2_mu_sym_##name cargs;		    \
}
#include "tss2_mufuncs.h"
#undef VOID_FUNC
#undef FUNC

#pragma GCC diagnostic pop

static int
ensure_symbol (const char *name, void **symp)
{
  if (!*symp)
    {
      void *sym = dlsym (gnutls_tss2_mu_dlhandle, name);
      if (!sym)
	return -EINVAL;
      *symp = sym;
    }
  return 0;
}

int
gnutls_tss2_mu_ensure_library (const char *soname, int flags)
{
  int err;

  if (!gnutls_tss2_mu_dlhandle)
    {
      gnutls_tss2_mu_dlhandle = dlopen (soname, flags);
      if (!gnutls_tss2_mu_dlhandle)
	return -EINVAL;
    }

#define ENSURE_SYMBOL(name)					\
  ensure_symbol(#name, (void **)&gnutls_tss2_mu_sym_##name)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"

#define FUNC(ret, name, args, cargs)		\
  err = ENSURE_SYMBOL(name);			\
  if (err < 0)					\
    {						\
      gnutls_tss2_mu_dlhandle = NULL;		\
      return err;				\
    }
#define VOID_FUNC FUNC
#include "tss2_mufuncs.h"
#undef VOID_FUNC
#undef FUNC

#pragma GCC diagnostic pop

#undef ENSURE_SYMBOL
  return 0;
}

void
gnutls_tss2_mu_unload_library (void)
{
  if (gnutls_tss2_mu_dlhandle)
    {
      dlclose (gnutls_tss2_mu_dlhandle);
      gnutls_tss2_mu_dlhandle = NULL;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"

#define FUNC(ret, name, args, cargs)		\
  gnutls_tss2_mu_sym_##name = NULL;
#define VOID_FUNC FUNC
#include "tss2_mufuncs.h"
#undef VOID_FUNC
#undef FUNC

#pragma GCC diagnostic pop
}

unsigned
gnutls_tss2_mu_is_usable (void)
{
  return gnutls_tss2_mu_dlhandle != NULL;
}

#else /* GNUTLS_TSS2_MU_ENABLE_DLOPEN */

int
gnutls_tss2_mu_ensure_library (const char *soname, int flags)
{
  (void) soname;
  (void) flags;
  return 0;
}

void
gnutls_tss2_mu_unload_library (void)
{
}

unsigned
gnutls_tss2_mu_is_usable (void)
{
  /* The library is linked at build time, thus always usable */
  return 1;
}

#endif /* !GNUTLS_TSS2_MU_ENABLE_DLOPEN */
