/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_SWAPCHAIN9EX_H_
#define _NINE_SWAPCHAIN9EX_H_

#include "swapchain9.h"

struct NineSwapChain9Ex
{
    struct NineSwapChain9 base;
};
static inline struct NineSwapChain9Ex *
NineSwapChain9Ex( void *data )
{
    return (struct NineSwapChain9Ex *)data;
}

HRESULT
NineSwapChain9Ex_new( struct NineDevice9 *pDevice,
                      BOOL implicit,
                      ID3DPresent *pPresent,
                      D3DPRESENT_PARAMETERS *pPresentationParameters,
                      struct d3dadapter9_context *pCTX,
                      HWND hFocusWindow,
                      D3DDISPLAYMODEEX *mode,
                      struct NineSwapChain9Ex **ppOut );

HRESULT NINE_WINAPI
NineSwapChain9Ex_GetLastPresentCount( struct NineSwapChain9Ex *This,
                                      UINT *pLastPresentCount );

HRESULT NINE_WINAPI
NineSwapChain9Ex_GetPresentStats( struct NineSwapChain9Ex *This,
                                  D3DPRESENTSTATS *pPresentationStatistics );

HRESULT NINE_WINAPI
NineSwapChain9Ex_GetDisplayModeEx( struct NineSwapChain9Ex *This,
                                   D3DDISPLAYMODEEX *pMode,
                                   D3DDISPLAYROTATION *pRotation );

#endif /* _NINE_SWAPCHAIN9EX_H_ */
