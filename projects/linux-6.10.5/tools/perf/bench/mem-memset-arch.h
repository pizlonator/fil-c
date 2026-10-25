/* SPDX-License-Identifier: GPL-2.0 */

/* Fil-C: the x86-64 asm bench memset variants are not built (nor referenced)
 * under Fil-C; see mem-memset-x86-64-asm.S. */
#if defined(HAVE_ARCH_X86_64_SUPPORT) && !defined(__PIZLONATOR_WAS_HERE__)

#define MEMSET_FN(fn, name, desc)		\
	void *fn(void *, int, size_t);

#include "mem-memset-x86-64-asm-def.h"

#undef MEMSET_FN

#endif

