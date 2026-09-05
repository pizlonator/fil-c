/*
 * Copyright (c) 2025-2026 Epic Games, Inc. All Rights Reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "filc_runtime.h"

#if PAS_ENABLE_FILC

#include "filc_native.h"
#include "bmalloc_heap.h"
#include "pas_string_stream.h"
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <string.h>

filc_ptr filc_native_zsys_dlopen(filc_thread* my_thread, filc_ptr filename_ptr, int flags)
{
    char* filename = filc_check_and_get_tmp_str_or_null(my_thread, filename_ptr);
    if (filc_dump_dlopens) {
        pas_log("[%d] dlopening %s as %s.\n",
                pas_getpid(), filename, (flags & RTLD_GLOBAL) ? "global" : "local");
    }
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

/* dl_iterate_phdr support.

   The user callback and every pointer we hand it inside `struct dl_phdr_info` (the `dlpi_name`
   string and the `dlpi_phdr` array) must be backed by a real Fil-C capability, otherwise Fil-C
   would trap when the callback dereferences them (which is exactly the panic the old
   `static_dl_iterate_phdr` stub produced). We cannot forge a capability over the host's ELF image
   directly (there is no unsafe escape hatch: GIMSO), so we copy the phdrs and the name into
   freshly-allocated Fil-C objects that carry valid capabilities, mirroring how zsys_dladdr and
   filc_native_zstack_scan build safe pointers for the values they pass to user code.

   Because the host dl_iterate_phdr holds the dynamic linker lock while it runs, we do not re-enter
   Fil-C (which may allocate, run the GC, or itself call into the loader) from inside its callback.
   Instead we collect the raw phdr data for every object natively, and only then build the Fil-C
   objects and invoke the user callback. */

typedef struct {
    unsigned long dlpi_addr;
    char* name;                    /* bmalloc'd, or NULL */
    void* phdr;                    /* bmalloc'd copy of phnum phdrs, or NULL */
    unsigned long long phdr_size;  /* bytes */
    unsigned phnum;
    unsigned long long dlpi_adds;
    unsigned long long dlpi_subs;
    size_t dlpi_tls_modid;
    void* dlpi_tls_data;
} filc_phdr_entry;

typedef struct {
    filc_phdr_entry* entries;
    size_t count;
    size_t capacity;
} filc_phdr_collect;

static int filc_phdr_collect_callback(struct dl_phdr_info* info, size_t size, void* data)
{
    filc_phdr_collect* collect = (filc_phdr_collect*)data;
    if (collect->count == collect->capacity) {
        size_t new_capacity = collect->capacity ? collect->capacity * 2 : 8;
        filc_phdr_entry* new_entries =
            (filc_phdr_entry*)bmalloc_allocate(new_capacity * sizeof(filc_phdr_entry));
        if (collect->entries) {
            memcpy(new_entries, collect->entries, collect->count * sizeof(filc_phdr_entry));
            bmalloc_deallocate(collect->entries);
        }
        collect->entries = new_entries;
        collect->capacity = new_capacity;
    }

    filc_phdr_entry* entry = collect->entries + collect->count++;
    entry->dlpi_addr = info->dlpi_addr;
    if (info->dlpi_name) {
        size_t len = strlen(info->dlpi_name) + 1;
        entry->name = (char*)bmalloc_allocate(len);
        memcpy(entry->name, info->dlpi_name, len);
    } else
        entry->name = NULL;
    entry->phnum = info->dlpi_phnum;
    entry->phdr_size = (unsigned long long)info->dlpi_phnum * sizeof(ElfW(Phdr));
    if (entry->phdr_size && info->dlpi_phdr) {
        entry->phdr = bmalloc_allocate(entry->phdr_size);
        memcpy(entry->phdr, info->dlpi_phdr, entry->phdr_size);
    } else {
        entry->phdr = NULL;
        entry->phdr_size = 0;
    }
    /* The dlpi_adds/dlpi_subs/dlpi_tls_* fields were added to the ABI after the original four, so
       only trust them when the host reported a struct large enough to contain them. */
    if (size >= offsetof(struct dl_phdr_info, dlpi_tls_data) + sizeof(void*)) {
        entry->dlpi_adds = info->dlpi_adds;
        entry->dlpi_subs = info->dlpi_subs;
        entry->dlpi_tls_modid = info->dlpi_tls_modid;
        entry->dlpi_tls_data = info->dlpi_tls_data;
    } else {
        entry->dlpi_adds = 0;
        entry->dlpi_subs = 0;
        entry->dlpi_tls_modid = 0;
        entry->dlpi_tls_data = NULL;
    }
    return 0; /* keep enumerating; we deliver to the user later */
}

int filc_native_zsys_dl_iterate_phdr(filc_thread* my_thread, filc_ptr callback_ptr, filc_ptr data_ptr)
{
    filc_phdr_collect collect;
    collect.entries = NULL;
    collect.count = 0;
    collect.capacity = 0;

    filc_exit(my_thread);
    dl_iterate_phdr(filc_phdr_collect_callback, &collect);
    filc_enter(my_thread);

    int result = 0;
    size_t index;
    for (index = 0; index < collect.count; ++index) {
        filc_phdr_entry* entry = collect.entries + index;

        filc_ptr info_ptr = filc_ptr_create_with_object(
            my_thread, filc_allocate(my_thread, sizeof(struct dl_phdr_info)));
        struct dl_phdr_info* info = (struct dl_phdr_info*)filc_ptr_ptr(info_ptr);

        info->dlpi_addr = entry->dlpi_addr;
        filc_store_ptr_at(my_thread, info_ptr, &info->dlpi_name,
                          entry->name ? filc_strdup(my_thread, entry->name)
                                      : filc_ptr_forge_null());

        filc_ptr phdr_ptr;
        if (entry->phdr_size) {
            phdr_ptr = filc_ptr_create_with_object(
                my_thread, filc_allocate(my_thread, entry->phdr_size));
            memcpy(filc_ptr_ptr(phdr_ptr), entry->phdr, entry->phdr_size);
        } else
            phdr_ptr = filc_ptr_forge_null();
        filc_store_ptr_at(my_thread, info_ptr, &info->dlpi_phdr, phdr_ptr);

        info->dlpi_phnum = (ElfW(Half))entry->phnum;
        info->dlpi_adds = entry->dlpi_adds;
        info->dlpi_subs = entry->dlpi_subs;
        info->dlpi_tls_modid = entry->dlpi_tls_modid;
        /* dlpi_tls_data points into the host's TLS image, which is not a Fil-C object, so we can
           only hand back an address without a dereferenceable capability (as zsys_dladdr does for
           dli_fbase). Callers that just want module names/phdrs never touch it. */
        filc_store_ptr_at(my_thread, info_ptr, &info->dlpi_tls_data,
                          entry->dlpi_tls_data ? filc_ptr_forge_invalid(entry->dlpi_tls_data)
                                               : filc_ptr_forge_null());

        result = filc_call_user_int_ptr_size_ptr(
            my_thread, callback_ptr, info_ptr, sizeof(struct dl_phdr_info), data_ptr);
        if (result)
            break;
    }

    for (index = 0; index < collect.count; ++index) {
        if (collect.entries[index].name)
            bmalloc_deallocate(collect.entries[index].name);
        if (collect.entries[index].phdr)
            bmalloc_deallocate(collect.entries[index].phdr);
    }
    if (collect.entries)
        bmalloc_deallocate(collect.entries);

    return result;
}

/* dl_find_object support.

   dl_find_object(3) is glibc-only (>= 2.35). glibc forwards it to _dl_find_object, but this Fil-C
   toolchain links its runtime against musl (libpizlo -> libyoloc.so), which has no such symbol, so
   forwarding is impossible here. Instead we compute the answer from the loaded-object program
   headers, using the host dl_iterate_phdr the runtime already provides (yolomusl's, which walks the
   real link_map) -- i.e. exactly the data glibc's own _dl_find_object works from. We find the object
   whose PT_LOAD segments contain `address`, then report its overall mapping [dlfo_map_start,
   dlfo_map_end) and its PT_GNU_EH_FRAME (dlfo_eh_frame). This is the same source of truth as
   zsys_dl_iterate_phdr and works on both musl and glibc Fil-C builds.

   The result layout mirrors glibc's x86_64 `struct dl_find_object`. Fil-C is x86_64-only, where
   DLFO_STRUCT_HAS_EH_DBASE and DLFO_STRUCT_HAS_EH_COUNT are both 0, so the fields are dlfo_flags,
   dlfo_map_start, dlfo_map_end, dlfo_link_map, dlfo_eh_frame, dlfo_sframe and six reserved words
   (sizeof == 96); the usermusl user header carries the identical layout. The opaque address fields
   are handed back via filc_ptr_forge_invalid (like dladdr's dli_fbase) and the scalar dlfo_flags is
   written directly. */
struct filc_dl_find_object {
    unsigned long long dlfo_flags;
    void* dlfo_map_start;
    void* dlfo_map_end;
    void* dlfo_link_map;
    void* dlfo_eh_frame;
    void* dlfo_sframe;
    unsigned long long __dlfo_reserved[6];
};

typedef struct {
    size_t address;   /* target address being resolved */
    int found;
    size_t map_start;
    size_t map_end;
    size_t link_map;  /* best-effort opaque handle: the object's load base */
    size_t eh_frame;  /* PT_GNU_EH_FRAME segment, or 0 */
} filc_dlfo_search;

static int filc_dlfo_callback(struct dl_phdr_info* info, size_t size, void* data)
{
    PAS_UNUSED_PARAM(size);
    filc_dlfo_search* search = (filc_dlfo_search*)data;
    size_t base = (size_t)info->dlpi_addr;
    size_t lo = (size_t)-1;
    size_t hi = 0;
    size_t eh_frame = 0;
    int contains = 0;
    unsigned i;
    for (i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* phdr = info->dlpi_phdr + i;
        if (phdr->p_type == PT_LOAD) {
            size_t seg_lo = base + phdr->p_vaddr;
            size_t seg_hi = seg_lo + phdr->p_memsz;
            if (seg_lo < lo)
                lo = seg_lo;
            if (seg_hi > hi)
                hi = seg_hi;
            if (search->address >= seg_lo && search->address < seg_hi)
                contains = 1;
        } else if (phdr->p_type == PT_GNU_EH_FRAME)
            eh_frame = base + phdr->p_vaddr;
    }
    if (!contains)
        return 0;
    search->found = 1;
    search->map_start = lo;
    search->map_end = hi;
    search->link_map = base;
    search->eh_frame = eh_frame;
    return 1; /* stop: found the containing object */
}

int filc_native_zsys_dl_find_object(filc_thread* my_thread, filc_ptr address_ptr, filc_ptr result_ptr)
{
    filc_dlfo_search search;
    pas_zero_memory(&search, sizeof(search));
    search.address = (size_t)filc_ptr_ptr(address_ptr);

    filc_exit(my_thread);
    dl_iterate_phdr(filc_dlfo_callback, &search);
    filc_enter(my_thread);

    if (!search.found)
        return -1;

    filc_check_write(result_ptr, sizeof(struct filc_dl_find_object));
    struct filc_dl_find_object* result = (struct filc_dl_find_object*)filc_ptr_ptr(result_ptr);
    result->dlfo_flags = 0;
    filc_store_ptr_at(my_thread, result_ptr, &result->dlfo_map_start,
                      filc_ptr_forge_invalid((void*)search.map_start));
    filc_store_ptr_at(my_thread, result_ptr, &result->dlfo_map_end,
                      filc_ptr_forge_invalid((void*)search.map_end));
    filc_store_ptr_at(my_thread, result_ptr, &result->dlfo_link_map,
                      filc_ptr_forge_invalid((void*)search.link_map));
    filc_store_ptr_at(my_thread, result_ptr, &result->dlfo_eh_frame,
                      search.eh_frame ? filc_ptr_forge_invalid((void*)search.eh_frame)
                                      : filc_ptr_forge_null());
    filc_store_ptr_at(my_thread, result_ptr, &result->dlfo_sframe,
                      filc_ptr_forge_null());
    /* __dlfo_reserved stays zero (fresh Fil-C allocation). */
    return 0;
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

