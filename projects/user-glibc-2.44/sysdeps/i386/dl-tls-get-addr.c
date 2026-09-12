/* Ifunc selector for ___tls_get_addr.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifdef SHARED
# define ___tls_get_addr __redirect____tls_get_addr
# include <dl-tls.h>
# undef ___tls_get_addr
# undef __tls_get_addr

# define SYMBOL_NAME ___tls_get_addr
# include <init-arch.h>

extern __typeof (REDIRECT_NAME) OPTIMIZE (fnsave) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (fxsave) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (xsave) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (xsavec) attribute_hidden;

static inline void *
IFUNC_SELECTOR (void)
{
  const struct cpu_features* cpu_features = __get_cpu_features ();

  if (cpu_features->xsave_state_size != 0)
    {
      if (CPU_FEATURE_USABLE_P (cpu_features, XSAVEC))
	return OPTIMIZE (xsavec);
      else
	return OPTIMIZE (xsave);
    }
  else if (CPU_FEATURE_USABLE_P (cpu_features, FXSR))
    return OPTIMIZE (fxsave);
  return OPTIMIZE (fnsave);
}

libc_ifunc_redirected (__redirect____tls_get_addr, ___tls_get_addr,
		       IFUNC_SELECTOR ());

/* The special thing about the x86 TLS ABI is that we have two
   variants of the __tls_get_addr function with different calling
   conventions.  The GNU version, which we are mostly concerned here,
   takes the parameter in a register.  The name is changed by adding
   an additional underscore at the beginning.  The Sun version uses
   the normal calling convention.  */

rtld_hidden_proto (___tls_get_addr)
rtld_hidden_def (___tls_get_addr)

void *
__tls_get_addr (tls_index *ti)
{
  return ___tls_get_addr (ti);
}
#endif
