/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_AUDIT_ARCH_H
#define UTIL_LINUX_AUDIT_ARCH_H

#if __x86_64__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_X86_64
#elif __i386__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_I386
#elif __arm__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_ARM
#elif __aarch64__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_AARCH64
#elif __riscv
#    if __riscv_xlen == 32
#        define SECCOMP_ARCH_NATIVE AUDIT_ARCH_RISCV32
#    elif __riscv_xlen == 64
#        define SECCOMP_ARCH_NATIVE AUDIT_ARCH_RISCV64
#    endif
#elif __s390x__
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_S390X
#elif __s390__
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_S390
#elif __PPC64__
#    if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_PPC64
#    else
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_PPC64LE
#    endif
#elif __powerpc__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_PPC
#elif __mips__
#    if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_MIPS
#    else
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_MIPSEL
#    endif
#elif __arc__
#    if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_ARCV2BE
#    else
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_ARCV2
#    endif
#elif __sparc__
#    if __SIZEOF_POINTER__ == 4
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_SPARC
#    else
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_SPARC64
#    endif
#elif __loongarch__
#    if __SIZEOF_POINTER__ == 4
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_LOONGARCH32
#    else
# 	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_LOONGARCH64
#    endif
#elif __hppa__
#    if __SIZEOF_POINTER__ == 4
#	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_PARISC
#    else
#	 define SECCOMP_ARCH_NATIVE AUDIT_ARCH_PARISC64
#    endif
#elif __alpha__
#    define SECCOMP_ARCH_NATIVE AUDIT_ARCH_ALPHA
#else
#    error Unknown target architecture
#endif

#endif /* UTIL_LINUX_AUDIT_ARCH_H */
