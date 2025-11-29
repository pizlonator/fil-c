/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_DEVICE9VIDEO_H_
#define _NINE_DEVICE9VIDEO_H_

#include "iunknown.h"

struct NineDevice9Video
{
    struct NineUnknown base;
};
static inline struct NineDevice9Video *
NineDevice9Video( void *data )
{
    return (struct NineDevice9Video *)data;
}

HRESULT NINE_WINAPI
NineDevice9Video_GetContentProtectionCaps( struct NineDevice9Video *This,
                                           const GUID *pCryptoType,
                                           const GUID *pDecodeProfile,
                                           D3DCONTENTPROTECTIONCAPS *pCaps );

HRESULT NINE_WINAPI
NineDevice9Video_CreateAuthenticatedChannel( struct NineDevice9Video *This,
                                             D3DAUTHENTICATEDCHANNELTYPE ChannelType,
                                             IDirect3DAuthenticatedChannel9 **ppAuthenticatedChannel,
                                             HANDLE *pChannelHandle );

HRESULT NINE_WINAPI
NineDevice9Video_CreateCryptoSession( struct NineDevice9Video *This,
                                      const GUID *pCryptoType,
                                      const GUID *pDecodeProfile,
                                      IDirect3DCryptoSession9 **ppCryptoSession,
                                      HANDLE *pCryptoHandle );

#endif /* _NINE_DEVICE9VIDEO_H_ */
