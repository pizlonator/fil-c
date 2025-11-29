/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include "authenticatedchannel9.h"

#define DBG_CHANNEL DBG_AUTHENTICATEDCHANNEL

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_GetCertificateSize( struct NineAuthenticatedChannel9 *This,
                                              UINT *pCertificateSize )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_GetCertificate( struct NineAuthenticatedChannel9 *This,
                                          UINT CertifacteSize,
                                          BYTE *ppCertificate )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_NegotiateKeyExchange( struct NineAuthenticatedChannel9 *This,
                                                UINT DataSize,
                                                void *pData )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_Query( struct NineAuthenticatedChannel9 *This,
                                 UINT InputSize,
                                 const void *pInput,
                                 UINT OutputSize,
                                 void *pOutput )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_Configure( struct NineAuthenticatedChannel9 *This,
                                     UINT InputSize,
                                     const void *pInput,
                                     D3DAUTHENTICATEDCHANNEL_CONFIGURE_OUTPUT *pOutput )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DAuthenticatedChannel9Vtbl NineAuthenticatedChannel9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineAuthenticatedChannel9_GetCertificateSize,
    (void *)NineAuthenticatedChannel9_GetCertificate,
    (void *)NineAuthenticatedChannel9_NegotiateKeyExchange,
    (void *)NineAuthenticatedChannel9_Query,
    (void *)NineAuthenticatedChannel9_Configure
};
