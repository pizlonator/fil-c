/* SPDX-License-Identifier: GPL-2.0 */

/* Fil-C: the x86-64 asm bench memcpy variants are not built (nor referenced)
 * under Fil-C; see mem-memcpy-x86-64-asm.S. */
#if defined(HAVE_ARCH_X86_64_SUPPORT) && !defined(__PIZLONATOR_WAS_HERE__)

#define MEMCPY_FN(fn, name, desc)		\
	void *fn(void *, const void *, size_t);

#include "mem-memcpy-x86-64-asm-def.h"

#undef MEMCPY_FN

#endif

