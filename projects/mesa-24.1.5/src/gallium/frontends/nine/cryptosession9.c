/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "cryptosession9.h"

#define DBG_CHANNEL DBG_CRYPTOSESSION

HRESULT NINE_WINAPI
NineCryptoSession9_GetCertificateSize( struct NineCryptoSession9 *This,
                                       UINT *pCertificateSize )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_GetCertificate( struct NineCryptoSession9 *This,
                                   UINT CertifacteSize,
                                   BYTE *ppCertificate )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_NegotiateKeyExchange( struct NineCryptoSession9 *This,
                                         UINT DataSize,
                                         void *pData )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_EncryptionBlt( struct NineCryptoSession9 *This,
                                  IDirect3DSurface9 *pSrcSurface,
                                  IDirect3DSurface9 *pDstSurface,
                                  UINT DstSurfaceSize,
                                  void *pIV )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_DecryptionBlt( struct NineCryptoSession9 *This,
                                  IDirect3DSurface9 *pSrcSurface,
                                  IDirect3DSurface9 *pDstSurface,
                                  UINT SrcSurfaceSize,
                                  D3DENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
                                  void *pContentKey,
                                  void *pIV )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_GetSurfacePitch( struct NineCryptoSession9 *This,
                                    IDirect3DSurface9 *pSrcSurface,
                                    UINT *pSurfacePitch )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_StartSessionKeyRefresh( struct NineCryptoSession9 *This,
                                           void *pRandomNumber,
                                           UINT RandomNumberSize )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_FinishSessionKeyRefresh( struct NineCryptoSession9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineCryptoSession9_GetEncryptionBltKey( struct NineCryptoSession9 *This,
                                        void *pReadbackKey,
                                        UINT KeySize )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DCryptoSession9Vtbl NineCryptoSession9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineCryptoSession9_GetCertificateSize,
    (void *)NineCryptoSession9_GetCertificate,
    (void *)NineCryptoSession9_NegotiateKeyExchange,
    (void *)NineCryptoSession9_EncryptionBlt,
    (void *)NineCryptoSession9_DecryptionBlt,
    (void *)NineCryptoSession9_GetSurfacePitch,
    (void *)NineCryptoSession9_StartSessionKeyRefresh,
    (void *)NineCryptoSession9_FinishSessionKeyRefresh,
    (void *)NineCryptoSession9_GetEncryptionBltKey
};
