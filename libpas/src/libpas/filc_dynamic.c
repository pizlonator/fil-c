/*
 * Copyright (c) 2025-2026 Epic Games, Inc. All Rights Reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include "filc_native.h"
#include "pas_string_stream.h"
#include <dlfcn.h>

filc_ptr filc_native_zsys_dlopen(filc_thread* my_thread, filc_ptr filename_ptr, int flags)
{
    char* filename = filc_check_and_get_tmp_str_or_null(my_thread, filename_ptr);
    filc_exit(my_thread);
    void* handle = dlopen(filename, flags);
    filc_enter(my_thread);
    if (!handle) {
        filc_set_dlerror(dlerror(), filename);
        return filc_ptr_forge_null();
    }
    return filc_ptr_create_with_special_object_and_manual_tracking(
        filc_allocate_special_with_existing_payload(my_thread, handle, FILC_SPECIAL_TYPE_DL_HANDLE));
}

filc_ptr filc_native_zsys_dlsym(filc_thread* my_thread, filc_ptr handle_ptr, filc_ptr symbol_ptr)
{
    if (filc_ptr_ptr(handle_ptr))
        filc_check_access_special(handle_ptr, FILC_SPECIAL_TYPE_DL_HANDLE);
    void* handle = filc_ptr_ptr(handle_ptr);
    char* symbol = filc_check_and_get_tmp_str(my_thread, symbol_ptr);
    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);
    pas_string_stream stream;
    pas_string_stream_construct(&stream, &allocation_config);
    pas_string_stream_printf(&stream, "pizlonated_%s", symbol);
    filc_exit(my_thread);
    pizlonated_getter raw_symbol =
        (pizlonated_getter)dlsym(handle, pas_string_stream_get_string(&stream));
    filc_enter(my_thread);
    pas_string_stream_destruct(&stream);
    if (!raw_symbol) {
        filc_set_dlerror(dlerror(), symbol);
        return filc_ptr_forge_null();
    }
    return raw_symbol(my_thread, NULL);
}

filc_ptr filc_native_zsys_dlvsym(filc_thread* my_thread, filc_ptr handle_ptr, filc_ptr symbol_ptr,
                                 filc_ptr version_ptr)
{
#if PAS_GLIBC
    filc_check_access_special(handle_ptr, FILC_SPECIAL_TYPE_DL_HANDLE);
    void* handle = filc_ptr_ptr(handle_ptr);
    char* symbol = filc_check_and_get_tmp_str(my_thread, symbol_ptr);
    char* version = filc_check_and_get_tmp_str(my_thread, version_ptr);
    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);
    pas_string_stream stream;
    pas_string_stream_construct(&stream, &allocation_config);
    pas_string_stream_printf(&stream, "pizlonated_%s", symbol);
    filc_exit(my_thread);
    pizlonated_getter raw_symbol =
        (pizlonated_getter)dlvsym(handle, pas_string_stream_get_string(&stream), version);
    filc_enter(my_thread);
    pas_string_stream_destruct(&stream);
    if (!raw_symbol) {
        filc_set_dlerror(dlerror(), symbol);
        return filc_ptr_forge_null();
    }
    return raw_symbol(my_thread, NULL);
#else
    PAS_UNUSED_PARAM(my_thread);
    PAS_UNUSED_PARAM(handle_ptr);
    PAS_UNUSED_PARAM(symbol_ptr);
    PAS_UNUSED_PARAM(version_ptr);
    filc_internal_panic(NULL, "dlvsym not supported.");
#endif
}

int filc_native_zsys_dladdr(filc_thread* my_thread, filc_ptr addr_ptr, filc_ptr info_ptr)
{
    filc_exit(my_thread);
    Dl_info my_info;
    int result = dladdr(filc_ptr_ptr(addr_ptr), &my_info);
    filc_enter(my_thread);
    if (!result) {
        filc_set_dlerror(dlerror(), NULL);
        return 0;
    }
    filc_check_write(info_ptr, sizeof(Dl_info));
    Dl_info* info = (Dl_info*)filc_ptr_ptr(info_ptr);
    filc_store_ptr_at(my_thread, info_ptr, &info->dli_fname,
                      filc_strdup(my_thread, my_info.dli_fname));
    filc_store_ptr_at(my_thread, info_ptr, &info->dli_fbase,
                      filc_ptr_forge_invalid(my_info.dli_fbase));
    /* FIXME: Currently, we are not able to provide information about the symbol itself. And,
       currently the users of this API don't care (gstreamer wants the fname). */
    filc_store_ptr_at(my_thread, info_ptr, &info->dli_sname,
                      filc_ptr_forge_null());
    filc_store_ptr_at(my_thread, info_ptr, &info->dli_saddr,
                      filc_ptr_forge_null());
    return result;
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

