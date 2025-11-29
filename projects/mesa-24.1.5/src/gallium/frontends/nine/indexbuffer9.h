/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_INDEXBUFFER9_H_
#define _NINE_INDEXBUFFER9_H_

#include "resource9.h"
#include "buffer9.h"
#include "pipe/p_state.h"

struct pipe_screen;
struct pipe_context;
struct pipe_transfer;
struct NineDevice9;

struct NineIndexBuffer9
{
    struct NineBuffer9 base;

    /* g3d stuff */
    unsigned index_size;

    D3DINDEXBUFFER_DESC desc;
};
static inline struct NineIndexBuffer9 *
NineIndexBuffer9( void *data )
{
    return (struct NineIndexBuffer9 *)data;
}

HRESULT
NineIndexBuffer9_new( struct NineDevice9 *pDevice,
                      D3DINDEXBUFFER_DESC *pDesc,
                      struct NineIndexBuffer9 **ppOut );

HRESULT
NineIndexBuffer9_ctor( struct NineIndexBuffer9 *This,
                       struct NineUnknownParams *pParams,
                       D3DINDEXBUFFER_DESC *pDesc );

void
NineIndexBuffer9_dtor( struct NineIndexBuffer9 *This );

/*** Nine private ***/

struct pipe_resource *
NineIndexBuffer9_GetBuffer( struct NineIndexBuffer9 *This,
                            unsigned *offset );

/*** Direct3D public ***/

HRESULT NINE_WINAPI
NineIndexBuffer9_Lock( struct NineIndexBuffer9 *This,
                       UINT OffsetToLock,
                       UINT SizeToLock,
                       void **ppbData,
                       DWORD Flags );

HRESULT NINE_WINAPI
NineIndexBuffer9_Unlock( struct NineIndexBuffer9 *This );

HRESULT NINE_WINAPI
NineIndexBuffer9_GetDesc( struct NineIndexBuffer9 *This,
                          D3DINDEXBUFFER_DESC *pDesc );

#endif /* _NINE_INDEXBUFFER9_H_ */
