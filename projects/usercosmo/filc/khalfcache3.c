/* Pizlonated definition of cosmo's kHalfCache3 for the usercosmo flavor.
 *
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cosmo defines kHalfCache3 (the half-size of the L3 cache, used as the
 * non-temporal-store threshold in libc/intrin/memset.c and memmove.c) in
 * libc/nexgen32e/khalfcache3.S.  That file is GNU-as assembly that uses
 * cosmo's own assembler macros (.initbss/.init.start/.endobj) and an
 * inline cpuid probe; the pizlonated libc cannot compile it - Sarcasm
 * rejects the macro expansions ("unexpected character '\"' in displacement
 * expression"), and even if it didn't, the yolo object would still not
 * provide the `pizlonated_kHalfCache3` getter that pizlonated references
 * of the global resolve to (pizlonated code never touches raw yolo data).
 *
 * This file provides the pizlonated definition as plain C: compiling
 *
 *     long kHalfCache3 = 4 * 1024 * 1024;
 *
 * with the Fil-C compiler emits exactly the `pizlonated_kHalfCache3`
 * getter that memset.c.o/memmove.c.o reference (plus the backing data
 * object, which only the getter ever exposes).  The value is cosmo's own
 * fallback: the x86_64 .S uses 0x00400000 (4 MB) whenever its cpuid cache
 * probe does not find a level-3 descriptor, and the non-x86_64 branch of
 * the same file hard-codes 4 MB as well.  kHalfCache3 only steers the
 * memset/memmove fast paths between regular and non-temporal stores, so
 * the constant is always safe; it merely skips a per-machine heuristic.
 *
 * The yolo side keeps its own (runtime-probed) copy of kHalfCache3 in
 * libyolocosmo.a for yolo code; the two copies are separate pizlonated vs
 * yolo objects, which is how every other global in this sandwich works.
 */

#include "libc/nexgen32e/nexgen32e.h"

long kHalfCache3 = 4 * 1024 * 1024;
