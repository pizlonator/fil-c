/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "device9video.h"

#define DBG_CHANNEL DBG_DEVICEVIDEO

HRESULT NINE_WINAPI
NineDevice9Video_GetContentProtectionCaps( struct NineDevice9Video *This,
                                           const GUID *pCryptoType,
                                           const GUID *pDecodeProfile,
                                           D3DCONTENTPROTECTIONCAPS *pCaps )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineDevice9Video_CreateAuthenticatedChannel( struct NineDevice9Video *This,
                                             D3DAUTHENTICATEDCHANNELTYPE ChannelType,
                                             IDirect3DAuthenticatedChannel9 **ppAuthenticatedChannel,
                                             HANDLE *pChannelHandle )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineDevice9Video_CreateCryptoSession( struct NineDevice9Video *This,
                                      const GUID *pCryptoType,
                                      const GUID *pDecodeProfile,
                                      IDirect3DCryptoSession9 **ppCryptoSession,
                                      HANDLE *pCryptoHandle )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DDevice9VideoVtbl NineDevice9Video_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineDevice9Video_GetContentProtectionCaps,
    (void *)NineDevice9Video_CreateAuthenticatedChannel,
    (void *)NineDevice9Video_CreateCryptoSession
};
