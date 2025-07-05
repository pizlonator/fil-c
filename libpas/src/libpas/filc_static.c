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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include <dlfcn.h>

filc_ptr filc_native_zsys_dlopen(filc_thread* my_thread, filc_ptr filename_ptr, int flags)
{
    PAS_UNUSED_PARAM(my_thread);
    PAS_UNUSED_PARAM(flags);
    char* filename = filc_check_and_get_tmp_str_or_null(my_thread, filename_ptr);
    filc_set_dlerror("dlopen not supported in static build", filename);
    return filc_ptr_forge_null();
}

filc_ptr filc_native_zsys_dlsym(filc_thread* my_thread, filc_ptr handle_ptr, filc_ptr symbol_ptr)
{
    PAS_UNUSED_PARAM(my_thread);
    PAS_UNUSED_PARAM(handle_ptr);
    PAS_UNUSED_PARAM(symbol_ptr);
    filc_safety_panic(NULL, "cannot do dlsym in static build");
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

