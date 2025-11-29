/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "nineexoverlayextension.h"

#define DBG_CHANNEL DBG_OVERLAYEXTENSION

HRESULT NINE_WINAPI
Nine9ExOverlayExtension_CheckDeviceOverlayType( struct Nine9ExOverlayExtension *This,
                                                UINT Adapter,
                                                D3DDEVTYPE DevType,
                                                UINT OverlayWidth,
                                                UINT OverlayHeight,
                                                D3DFORMAT OverlayFormat,
                                                D3DDISPLAYMODEEX *pDisplayMode,
                                                D3DDISPLAYROTATION DisplayRotation,
                                                D3DOVERLAYCAPS *pOverlayCaps )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3D9ExOverlayExtensionVtbl Nine9ExOverlayExtension_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)Nine9ExOverlayExtension_CheckDeviceOverlayType
};
