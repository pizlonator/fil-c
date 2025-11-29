/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_IUNKNOWN_H_
#define _NINE_IUNKNOWN_H_

#include "util/compiler.h"

#include "util/u_atomic.h"
#include "util/u_memory.h"

#include "guid.h"
#include "nine_flags.h"
#include "nine_debug.h"
#include "nine_quirk.h"

#include "d3d9.h"

struct Nine9;
struct NineDevice9;

struct NineUnknown
{
    /* pointer to vtable (can be overridden outside gallium nine) */
    void *vtable;
    /* pointer to internal vtable  */
    void *vtable_internal;

    int32_t refs; /* external reference count */
    int32_t bind; /* internal bind count */
    int32_t has_bind_or_refs; /* 0 if no ref, 1 if bind or ref, 2 if both */
    bool forward; /* whether to forward references to the container */

    /* container: for surfaces and volumes only.
     * Can be a texture, a volume texture or a swapchain.
     * forward is set to false for the swapchain case.
     * If forward is set, refs are passed to the container if forward is set
     * and the container has bind increased if the object has non null bind. */
    struct NineUnknown *container;
    struct NineDevice9 *device;    /* referenced if (refs) */

    const GUID **guids; /* for QueryInterface */

    /* for [GS]etPrivateData/FreePrivateData */
    struct hash_table *pdata;

    void (*dtor)(void *data); /* top-level dtor */
};
static inline struct NineUnknown *
NineUnknown( void *data )
{
    return (struct NineUnknown *)data;
}

/* Use this instead of a shitload of arguments: */
struct NineUnknownParams
{
    void *vtable;
    const GUID **guids;
    void (*dtor)(void *data);
    struct NineUnknown *container;
    struct NineDevice9 *device;
    bool start_with_bind_not_ref;
};

HRESULT
NineUnknown_ctor( struct NineUnknown *This,
                  struct NineUnknownParams *pParams );

void
NineUnknown_dtor( struct NineUnknown *This );

/*** Direct3D public methods ***/

HRESULT NINE_WINAPI
NineUnknown_QueryInterface( struct NineUnknown *This,
                            REFIID riid,
                            void **ppvObject );

ULONG NINE_WINAPI
NineUnknown_AddRef( struct NineUnknown *This );

ULONG NINE_WINAPI
NineUnknown_Release( struct NineUnknown *This );

ULONG NINE_WINAPI
NineUnknown_ReleaseWithDtorLock( struct NineUnknown *This );

HRESULT NINE_WINAPI
NineUnknown_GetDevice( struct NineUnknown *This,
                       IDirect3DDevice9 **ppDevice );

HRESULT NINE_WINAPI
NineUnknown_SetPrivateData( struct NineUnknown *This,
                            REFGUID refguid,
                            const void *pData,
                            DWORD SizeOfData,
                            DWORD Flags );

HRESULT NINE_WINAPI
NineUnknown_GetPrivateData( struct NineUnknown *This,
                            REFGUID refguid,
                            void *pData,
                            DWORD *pSizeOfData );

HRESULT NINE_WINAPI
NineUnknown_FreePrivateData( struct NineUnknown *This,
                             REFGUID refguid );

/*** Nine private methods ***/

static inline void
NineUnknown_Destroy( struct NineUnknown *This )
{
    assert(!(This->refs | This->bind) && !This->has_bind_or_refs);
    This->dtor(This);
}

static inline UINT
NineUnknown_Bind( struct NineUnknown *This )
{
    UINT b = p_atomic_inc_return(&This->bind);
    assert(b);

    if (b == 1)
        p_atomic_inc(&This->has_bind_or_refs);
    if (b == 1 && This->forward)
        NineUnknown_Bind(This->container);

    return b;
}

static inline UINT
NineUnknown_Unbind( struct NineUnknown *This )
{
    UINT b = p_atomic_dec_return(&This->bind);
    UINT b_or_ref = 1;

    if (b == 0)
        b_or_ref = p_atomic_dec_return(&This->has_bind_or_refs);
    if (b == 0 && This->forward)
        NineUnknown_Unbind(This->container);
    else if (b_or_ref == 0 && !This->container)
        This->dtor(This);

    return b;
}

static inline void
NineUnknown_ConvertRefToBind( struct NineUnknown *This )
{
    NineUnknown_Bind(This);
    NineUnknown_Release(This);
}

/* Detach from container. */
static inline void
NineUnknown_Detach( struct NineUnknown *This )
{
    assert(This->container && !This->forward);

    This->container = NULL;
    if (!(This->has_bind_or_refs))
        This->dtor(This);
}

#endif /* _NINE_IUNKNOWN_H_ */
