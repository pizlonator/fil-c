/* Thread-local storage handling in the ELF dynamic linker.  i386 version.
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

#include <sysdep.h>
#include <tls.h>
#include <cpu-features-offsets.h>
#include <features-offsets.h>

	.text
#ifdef SHARED
# define USE_FNSAVE
# define MINIMUM_ALIGNMENT	4
# define STATE_SAVE_ALIGNMENT	4
# define ___tls_get_addr	_____tls_get_addr_fnsave
# include "tls_get_addr.h"
# undef ___tls_get_addr
# undef MINIMUM_ALIGNMENT
# undef USE_FNSAVE

# define MINIMUM_ALIGNMENT	16

# define USE_FXSAVE
# define STATE_SAVE_ALIGNMENT	16
# define ___tls_get_addr	_____tls_get_addr_fxsave
# include "tls_get_addr.h"
# undef ___tls_get_addr
# undef USE_FXSAVE

# define USE_XSAVE
# define STATE_SAVE_ALIGNMENT	64
# define ___tls_get_addr	_____tls_get_addr_xsave
# include "tls_get_addr.h"
# undef ___tls_get_addr
# undef USE_XSAVE

# define USE_XSAVEC
# define STATE_SAVE_ALIGNMENT	64
# define ___tls_get_addr	_____tls_get_addr_xsavec
# include "tls_get_addr.h"
# undef ___tls_get_addr
# undef USE_XSAVEC
#endif /* SHARED */
