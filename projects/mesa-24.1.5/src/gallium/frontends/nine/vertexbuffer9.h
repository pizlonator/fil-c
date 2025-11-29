/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_VERTEXBUFFER9_H_
#define _NINE_VERTEXBUFFER9_H_
#include "resource9.h"
#include "buffer9.h"

struct pipe_screen;
struct pipe_context;
struct pipe_transfer;

struct NineVertexBuffer9
{
    struct NineBuffer9 base;

    /* G3D */
    struct pipe_context *pipe;
    D3DVERTEXBUFFER_DESC desc;
};
static inline struct NineVertexBuffer9 *
NineVertexBuffer9( void *data )
{
    return (struct NineVertexBuffer9 *)data;
}

HRESULT
NineVertexBuffer9_new( struct NineDevice9 *pDevice,
                       D3DVERTEXBUFFER_DESC *pDesc,
                       struct NineVertexBuffer9 **ppOut );

HRESULT
NineVertexBuffer9_ctor( struct NineVertexBuffer9 *This,
                        struct NineUnknownParams *pParams,
                        D3DVERTEXBUFFER_DESC *pDesc );

void
NineVertexBuffer9_dtor( struct NineVertexBuffer9 *This );
/*** Nine private ***/

struct pipe_resource *
NineVertexBuffer9_GetResource( struct NineVertexBuffer9 *This, unsigned *offset );

/*** Direct3D public ***/

HRESULT NINE_WINAPI
NineVertexBuffer9_Lock( struct NineVertexBuffer9 *This,
                        UINT OffsetToLock,
                        UINT SizeToLock,
                        void **ppbData,
                        DWORD Flags );

HRESULT NINE_WINAPI
NineVertexBuffer9_Unlock( struct NineVertexBuffer9 *This );

HRESULT NINE_WINAPI
NineVertexBuffer9_GetDesc( struct NineVertexBuffer9 *This,
                           D3DVERTEXBUFFER_DESC *pDesc );

#endif /* _NINE_VERTEXBUFFER9_H_ */
