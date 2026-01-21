/* asm-common-i386.h  -  Common macros for i386 assembly
 *
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GCRY_ASM_COMMON_I386_H
#define GCRY_ASM_COMMON_I386_H

#include <config.h>

#ifdef HAVE_COMPATIBLE_GCC_I386_PLATFORM_AS
# define ELF(...) __VA_ARGS__
#else
# define ELF(...) /*_*/
#endif

#ifdef HAVE_COMPATIBLE_GCC_WIN32_PLATFORM_AS
# define SECTION_RODATA .section .rdata
#else
# define SECTION_RODATA .section .rodata
#endif

#ifdef HAVE_COMPATIBLE_GCC_WIN32_PLATFORM_AS
# define SYM_NAME(name) _##name
#else
# define SYM_NAME(name) name
#endif

#ifdef HAVE_COMPATIBLE_GCC_WIN32_PLATFORM_AS
# define DECL_GET_PC_THUNK(reg)
# define GET_DATA_POINTER(name, reg) leal name, %reg
#else
# define DECL_GET_PC_THUNK(reg) \
      .type __gcry_get_pc_thunk_##reg, @function; \
      .align 16; \
      __gcry_get_pc_thunk_##reg:; \
	CFI_STARTPROC(); \
	movl (%esp), %reg; \
	ret_spec_stop; \
	CFI_ENDPROC()
# define GET_DATA_POINTER(name, reg) \
	call __gcry_get_pc_thunk_##reg; \
	addl $_GLOBAL_OFFSET_TABLE_, %reg; \
	movl name##@GOT(%reg), %reg;
#endif

#ifdef HAVE_GCC_ASM_CFI_DIRECTIVES
/* CFI directives to emit DWARF stack unwinding information. */
# define CFI_STARTPROC()            .cfi_startproc
# define CFI_ENDPROC()              .cfi_endproc
# define CFI_REMEMBER_STATE()       .cfi_remember_state
# define CFI_RESTORE_STATE()        .cfi_restore_state
# define CFI_ADJUST_CFA_OFFSET(off) .cfi_adjust_cfa_offset off
# define CFI_REL_OFFSET(reg,off)    .cfi_rel_offset reg, off
# define CFI_DEF_CFA_REGISTER(reg)  .cfi_def_cfa_register reg
# define CFI_REGISTER(ro,rn)        .cfi_register ro, rn
# define CFI_RESTORE(reg)           .cfi_restore reg

# define CFI_PUSH(reg) \
	CFI_ADJUST_CFA_OFFSET(4); CFI_REL_OFFSET(reg, 0)
# define CFI_POP(reg) \
	CFI_ADJUST_CFA_OFFSET(-4); CFI_RESTORE(reg)
# define CFI_POP_TMP_REG() \
	CFI_ADJUST_CFA_OFFSET(-4);
# define CFI_LEAVE() \
	CFI_ADJUST_CFA_OFFSET(-4); CFI_DEF_CFA_REGISTER(%esp)

/* CFA expressions are used for pointing CFA and registers to
 * %rsp relative offsets. */
# define DW_REGNO_eax 0
# define DW_REGNO_edx 1
# define DW_REGNO_ecx 2
# define DW_REGNO_ebx 3
# define DW_REGNO_esi 4
# define DW_REGNO_edi 5
# define DW_REGNO_ebp 6
# define DW_REGNO_esp 7

# define DW_REGNO(reg) DW_REGNO_ ## reg

/* Fixed length encoding used for integers for now. */
# define DW_SLEB128_7BIT(value) \
	0x00|((value) & 0x7f)
# define DW_SLEB128_28BIT(value) \
	0x80|((value)&0x7f), \
	0x80|(((value)>>7)&0x7f), \
	0x80|(((value)>>14)&0x7f), \
	0x00|(((value)>>21)&0x7f)

# define CFI_CFA_ON_STACK(esp_offs,cfa_depth) \
	.cfi_escape \
	  0x0f, /* DW_CFA_def_cfa_expression */ \
	    DW_SLEB128_7BIT(11), /* length */ \
	  0x77, /* DW_OP_breg7, rsp + constant */ \
	    DW_SLEB128_28BIT(esp_offs), \
	  0x06, /* DW_OP_deref */ \
	  0x23, /* DW_OP_plus_constu */ \
	    DW_SLEB128_28BIT((cfa_depth)+4)

# define CFI_REG_ON_STACK(reg,esp_offs) \
	.cfi_escape \
	  0x10, /* DW_CFA_expression */ \
	    DW_SLEB128_7BIT(DW_REGNO(reg)), \
	    DW_SLEB128_7BIT(5), /* length */ \
	  0x77, /* DW_OP_breg7, rsp + constant */ \
	    DW_SLEB128_28BIT(esp_offs)

#else
# define CFI_STARTPROC()
# define CFI_ENDPROC()
# define CFI_REMEMBER_STATE()
# define CFI_RESTORE_STATE()
# define CFI_ADJUST_CFA_OFFSET(off)
# define CFI_REL_OFFSET(reg,off)
# define CFI_DEF_CFA_REGISTER(reg)
# define CFI_REGISTER(ro,rn)
# define CFI_RESTORE(reg)

# define CFI_PUSH(reg)
# define CFI_POP(reg)
# define CFI_POP_TMP_REG()
# define CFI_LEAVE()

# define CFI_CFA_ON_STACK(rsp_offs,cfa_depth)
# define CFI_REG_ON_STACK(reg,rsp_offs)
#endif

/* 'ret' instruction replacement for straight-line speculation mitigation. */
#define ret_spec_stop \
	ret; int3;

/* This prevents speculative execution on old AVX512 CPUs, to prevent
 * speculative execution to AVX512 code. The vpopcntb instruction is
 * available on newer CPUs that do not suffer from significant frequency
 * drop when 512-bit vectors are utilized. */
#define spec_stop_avx512 \
	vpxord %ymm7, %ymm7, %ymm7; \
	vpopcntb %xmm7, %xmm7; /* Supported only by newer AVX512 CPUs. */ \
	vpxord %ymm7, %ymm7, %ymm7;

#define spec_stop_avx512_intel_syntax \
	vpxord ymm7, ymm7, ymm7; \
	vpopcntb xmm7, xmm7; /* Supported only by newer AVX512 CPUs. */ \
	vpxord ymm7, ymm7, ymm7;

#endif /* GCRY_ASM_COMMON_AMD64_H */
