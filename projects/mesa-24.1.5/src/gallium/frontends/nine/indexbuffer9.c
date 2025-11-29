/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "indexbuffer9.h"
#include "device9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"
#include "nine_dump.h"

#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "util/format/u_formats.h"
#include "util/box.h"

#define DBG_CHANNEL DBG_INDEXBUFFER

HRESULT
NineIndexBuffer9_ctor( struct NineIndexBuffer9 *This,
                       struct NineUnknownParams *pParams,
                       D3DINDEXBUFFER_DESC *pDesc )
{
    HRESULT hr;
    DBG("This=%p pParams=%p pDesc=%p Usage=%s\n",
         This, pParams, pDesc, nine_D3DUSAGE_to_str(pDesc->Usage));

    hr = NineBuffer9_ctor(&This->base, pParams, D3DRTYPE_INDEXBUFFER,
                          pDesc->Usage, pDesc->Size, pDesc->Pool);
    if (FAILED(hr))
        return hr;

    switch (pDesc->Format) {
    case D3DFMT_INDEX16: This->index_size = 2; break;
    case D3DFMT_INDEX32: This->index_size = 4; break;
    default:
        user_assert(!"Invalid index format.", D3DERR_INVALIDCALL);
        break;
    }

    pDesc->Type = D3DRTYPE_INDEXBUFFER;
    This->desc = *pDesc;

    return D3D_OK;
}

void
NineIndexBuffer9_dtor( struct NineIndexBuffer9 *This )
{
    NineBuffer9_dtor(&This->base);
}

struct pipe_resource *
NineIndexBuffer9_GetBuffer( struct NineIndexBuffer9 *This, unsigned *offset )
{
    /* The resource may change */
    return NineBuffer9_GetResource(&This->base, offset);
}

HRESULT NINE_WINAPI
NineIndexBuffer9_Lock( struct NineIndexBuffer9 *This,
                       UINT OffsetToLock,
                       UINT SizeToLock,
                       void **ppbData,
                       DWORD Flags )
{
    return NineBuffer9_Lock(&This->base, OffsetToLock, SizeToLock, ppbData, Flags);
}

HRESULT NINE_WINAPI
NineIndexBuffer9_Unlock( struct NineIndexBuffer9 *This )
{
    return NineBuffer9_Unlock(&This->base);
}

HRESULT NINE_WINAPI
NineIndexBuffer9_GetDesc( struct NineIndexBuffer9 *This,
                          D3DINDEXBUFFER_DESC *pDesc )
{
    user_assert(pDesc, E_POINTER);
    *pDesc = This->desc;
    return D3D_OK;
}

IDirect3DIndexBuffer9Vtbl NineIndexBuffer9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineUnknown_GetDevice, /* actually part of Resource9 iface */
    (void *)NineUnknown_SetPrivateData,
    (void *)NineUnknown_GetPrivateData,
    (void *)NineUnknown_FreePrivateData,
    (void *)NineResource9_SetPriority,
    (void *)NineResource9_GetPriority,
    (void *)NineResource9_PreLoad,
    (void *)NineResource9_GetType,
    (void *)NineIndexBuffer9_Lock,
    (void *)NineIndexBuffer9_Unlock,
    (void *)NineIndexBuffer9_GetDesc
};

static const GUID *NineIndexBuffer9_IIDs[] = {
    &IID_IDirect3DIndexBuffer9,
    &IID_IDirect3DResource9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineIndexBuffer9_new( struct NineDevice9 *pDevice,
                      D3DINDEXBUFFER_DESC *pDesc,
                      struct NineIndexBuffer9 **ppOut )
{
    NINE_DEVICE_CHILD_NEW(IndexBuffer9, ppOut, /* args */ pDevice, pDesc);
}
