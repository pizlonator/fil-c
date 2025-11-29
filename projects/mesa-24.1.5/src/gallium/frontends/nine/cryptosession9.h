/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_CRYPTOSESSION9_H_
#define _NINE_CRYPTOSESSION9_H_

#include "iunknown.h"

struct NineCryptoSession9
{
    struct NineUnknown base;
};
static inline struct NineCryptoSession9 *
NineCryptoSession9( void *data )
{
    return (struct NineCryptoSession9 *)data;
}

HRESULT NINE_WINAPI
NineCryptoSession9_GetCertificateSize( struct NineCryptoSession9 *This,
                                       UINT *pCertificateSize );

HRESULT NINE_WINAPI
NineCryptoSession9_GetCertificate( struct NineCryptoSession9 *This,
                                   UINT CertifacteSize,
                                   BYTE *ppCertificate );

HRESULT NINE_WINAPI
NineCryptoSession9_NegotiateKeyExchange( struct NineCryptoSession9 *This,
                                         UINT DataSize,
                                         void *pData );

HRESULT NINE_WINAPI
NineCryptoSession9_EncryptionBlt( struct NineCryptoSession9 *This,
                                  IDirect3DSurface9 *pSrcSurface,
                                  IDirect3DSurface9 *pDstSurface,
                                  UINT DstSurfaceSize,
                                  void *pIV );

HRESULT NINE_WINAPI
NineCryptoSession9_DecryptionBlt( struct NineCryptoSession9 *This,
                                  IDirect3DSurface9 *pSrcSurface,
                                  IDirect3DSurface9 *pDstSurface,
                                  UINT SrcSurfaceSize,
                                  D3DENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
                                  void *pContentKey,
                                  void *pIV );

HRESULT NINE_WINAPI
NineCryptoSession9_GetSurfacePitch( struct NineCryptoSession9 *This,
                                    IDirect3DSurface9 *pSrcSurface,
                                    UINT *pSurfacePitch );

HRESULT NINE_WINAPI
NineCryptoSession9_StartSessionKeyRefresh( struct NineCryptoSession9 *This,
                                           void *pRandomNumber,
                                           UINT RandomNumberSize );

HRESULT NINE_WINAPI
NineCryptoSession9_FinishSessionKeyRefresh( struct NineCryptoSession9 *This );

HRESULT NINE_WINAPI
NineCryptoSession9_GetEncryptionBltKey( struct NineCryptoSession9 *This,
                                        void *pReadbackKey,
                                        UINT KeySize );

#endif /* _NINE_CRYPTOSESSION9_H_ */
