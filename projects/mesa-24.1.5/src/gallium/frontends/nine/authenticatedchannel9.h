/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_AUTHENTICATEDCHANNEL9_H_
#define _NINE_AUTHENTICATEDCHANNEL9_H_

#include "iunknown.h"

struct NineAuthenticatedChannel9
{
    struct NineUnknown base;
};
static inline struct NineAuthenticatedChannel9 *
NineAuthenticatedChannel9( void *data )
{
    return (struct NineAuthenticatedChannel9 *)data;
}

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_GetCertificateSize( struct NineAuthenticatedChannel9 *This,
                                              UINT *pCertificateSize );

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_GetCertificate( struct NineAuthenticatedChannel9 *This,
                                          UINT CertifacteSize,
                                          BYTE *ppCertificate );

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_NegotiateKeyExchange( struct NineAuthenticatedChannel9 *This,
                                                UINT DataSize,
                                                void *pData );

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_Query( struct NineAuthenticatedChannel9 *This,
                                 UINT InputSize,
                                 const void *pInput,
                                 UINT OutputSize,
                                 void *pOutput );

HRESULT NINE_WINAPI
NineAuthenticatedChannel9_Configure( struct NineAuthenticatedChannel9 *This,
                                     UINT InputSize,
                                     const void *pInput,
                                     D3DAUTHENTICATEDCHANNEL_CONFIGURE_OUTPUT *pOutput );

#endif /* _NINE_AUTHENTICATEDCHANNEL9_H_ */
