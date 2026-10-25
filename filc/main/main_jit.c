/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
  
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
  
   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* JIT-enabled program entry point.
  
   This is the twin of main.c: it is built into filc_jit_crt.o
   (-DUSE_LIBC=1) and filc_jit_mincrt.o (-DUSE_LIBC=0) by the
   libpas/Makefile crt rules, and it calls filc_start_program_with_jit
   (libpas/src/libpas/filc_start_program.c) instead of
   filc_start_program. That sets the filc_allow_jit global (declared in
   libpas/src/libpas/filc_runtime.h, defined in
   libpas/src/libpas/filc_llvm_jit.c) so the process may use the LLVM JIT
   API wrappers, which panic when the flag is false.
   The with_jit entry point refuses to run (message + abort) in
   secure-execution (setuid/setgid) processes. The clang driver selects
   these CRT objects when passed -enable-filc-jit / --filc-enable-jit.
 */

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_LIBC
#error "Must define USE_LIBC"
#endif

#if USE_LIBC
void pizlonated___libc_start_main();
#endif /* USE_LIBC */
void pizlonated_main();

void filc_start_program_with_jit(int argc, char** argv,
                                 void (*pizlonated___libc_start_main)(),
                                 void (*pizlonated_main)());

#ifdef __cplusplus
}
#endif

int main(int argc, char** argv)
{
#if USE_LIBC
    filc_start_program_with_jit(argc, argv, pizlonated___libc_start_main, pizlonated_main);
#else /* USE_LIBC -> so !USE_LIBC */
    filc_start_program_with_jit(argc, argv, NULL, pizlonated_main);
#endif /* USE_LIBC -> so end of !USE_LIBC */

    abort();
    return 1;
}
