/*
 * Copyright (c) 2023-2026 Epic Games, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
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

void filc_start_program(int argc, char** argv,
                        void (*pizlonated___libc_start_main)(),
                        void (*pizlonated_main)());

#ifdef __cplusplus
}
#endif

int main(int argc, char** argv)
{
#if USE_LIBC
    filc_start_program(argc, argv, pizlonated___libc_start_main, pizlonated_main);
#else /* USE_LIBC -> so !USE_LIBC */
    filc_start_program(argc, argv, NULL, pizlonated_main);
#endif /* USE_LIBC -> so end of !USE_LIBC */

    abort();
    return 1;
}

