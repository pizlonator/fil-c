/* SPDX-License-Identifier: GPL-2.0 */
// memset_orig is being defined as SYM_L_LOCAL but we need it
#define SYM_FUNC_START_LOCAL(name)                      \
        SYM_START(name, SYM_L_GLOBAL, SYM_A_ALIGN)
#define memset MEMSET /* don't hide glibc's memset() */
#define altinstr_replacement text
#define globl p2align 4; .globl
/* Fil-C: sarcasm, the Fil-C assembler, cannot assemble the kernel's
 * memset_64.S (it uses ALTERNATIVE/_ASM_EXTABLE macros and functions
 * that are not annotated the way sarcasm requires).  When building with
 * Fil-C, contribute no asm memset variants; bench/mem-functions.c and
 * bench/mem-memset-arch.h skip them in that case as well.
 */
#if defined(__PIZLONATOR_WAS_HERE__)
.section .note.GNU-stack,"",@progbits
#else
#include "../../arch/x86/lib/memset_64.S"
.section .note.GNU-stack,"",@progbits
#endif
