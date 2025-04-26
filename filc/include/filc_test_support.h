/*
 * Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
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

#ifndef FILC_TEST_SUPPORT_H
#define FILC_TEST_SUPPORT_H

#include <stdfil.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what's up */
#endif

/* This header includes functions that are only useful for testing Fil-C. None of this is stable API
   and may change from one Fil-C release to another. */

/* This function is just for testing zptrtable and it only returns accurate data if
   zis_runtime_testing_enabled(). */
__SIZE_TYPE__ ztesting_get_num_ptrtables(void);

/* Returns true if running in the build of the runtime that has extra (super expensive) testing
   checks.

   This is here so that the test suite can assert that it runs with testing asserts enabled. */
filc_bool zis_runtime_testing_enabled(void);

/* Asks Fil-C to run additional pointer validation on this pointer. If memory safety holds, then
   these checks will succeed. If they don't, then it's a Fil-C bug, and we should fix it. It could
   be a real bug, or it could be a bug in the validation checks. They are designed to be hella strict
   and maybe I made them too strict.
   
   If you run with pizfix/lib_test in your library path, then this check happens in a bunch of
   random places anyway (and that's the main reason why the lib_test version is so slow). */
void zvalidate_ptr(void* ptr);

/* Tells if the GC is running in STW (stop the world) mode. The default is false. You can enable STW
   mode by setting the FUGC_STW=1 environment variable. */
filc_bool zgc_is_stw(void);

filc_bool zgc_is_scribbling(void);
filc_bool zgc_is_verifying(void);

__SIZE_TYPE__ znum_deferred_signals(void);

#ifdef __cplusplus
}
#endif

#endif /* FILC_TEST_SUPPORT_H */

