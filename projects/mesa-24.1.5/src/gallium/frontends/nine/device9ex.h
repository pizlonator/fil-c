/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_DEVICE9EX_H_
#define _NINE_DEVICE9EX_H_

#include "device9.h"

struct NineDevice9Ex
{
    struct NineDevice9 base;
};
static inline struct NineDevice9Ex *
NineDevice9Ex( void *data )
{
    return (struct NineDevice9Ex *)data;
}

HRESULT
NineDevice9Ex_new( struct pipe_screen *pScreen,
                   D3DDEVICE_CREATION_PARAMETERS *pCreationParameters,
                   D3DCAPS9 *pCaps,
                   D3DPRESENT_PARAMETERS *pPresentationParameters,
                   D3DDISPLAYMODEEX *pFullscreenDisplayMode,
                   IDirect3D9Ex *pD3D9Ex,
                   ID3DPresentGroup *pPresentationGroup,
                   struct d3dadapter9_context *pCTX,
                   struct NineDevice9Ex **ppOut,
                   int minorVersionNum );

HRESULT NINE_WINAPI
NineDevice9Ex_SetConvolutionMonoKernel( struct NineDevice9Ex *This,
                                        UINT width,
                                        UINT height,
                                        float *rows,
                                        float *columns );

HRESULT NINE_WINAPI
NineDevice9Ex_ComposeRects( struct NineDevice9Ex *This,
                            IDirect3DSurface9 *pSrc,
                            IDirect3DSurface9 *pDst,
                            IDirect3DVertexBuffer9 *pSrcRectDescs,
                            UINT NumRects,
                            IDirect3DVertexBuffer9 *pDstRectDescs,
                            D3DCOMPOSERECTSOP Operation,
                            int Xoffset,
                            int Yoffset );

HRESULT NINE_WINAPI
NineDevice9Ex_PresentEx( struct NineDevice9Ex *This,
                         const RECT *pSourceRect,
                         const RECT *pDestRect,
                         HWND hDestWindowOverride,
                         const RGNDATA *pDirtyRegion,
                         DWORD dwFlags );

HRESULT NINE_WINAPI
NineDevice9Ex_Present( struct NineDevice9Ex *This,
                     const RECT *pSourceRect,
                     const RECT *pDestRect,
                     HWND hDestWindowOverride,
                     const RGNDATA *pDirtyRegion );

HRESULT NINE_WINAPI
NineDevice9Ex_GetGPUThreadPriority( struct NineDevice9Ex *This,
                                    INT *pPriority );

HRESULT NINE_WINAPI
NineDevice9Ex_SetGPUThreadPriority( struct NineDevice9Ex *This,
                                    INT Priority );

HRESULT NINE_WINAPI
NineDevice9Ex_WaitForVBlank( struct NineDevice9Ex *This,
                             UINT iSwapChain );

HRESULT NINE_WINAPI
NineDevice9Ex_CheckResourceResidency( struct NineDevice9Ex *This,
                                      IDirect3DResource9 **pResourceArray,
                                      UINT32 NumResources );

HRESULT NINE_WINAPI
NineDevice9Ex_SetMaximumFrameLatency( struct NineDevice9Ex *This,
                                      UINT MaxLatency );

HRESULT NINE_WINAPI
NineDevice9Ex_GetMaximumFrameLatency( struct NineDevice9Ex *This,
                                      UINT *pMaxLatency );

HRESULT NINE_WINAPI
NineDevice9Ex_CheckDeviceState( struct NineDevice9Ex *This,
                                HWND hDestinationWindow );

HRESULT NINE_WINAPI
NineDevice9Ex_CreateRenderTargetEx( struct NineDevice9Ex *This,
                                    UINT Width,
                                    UINT Height,
                                    D3DFORMAT Format,
                                    D3DMULTISAMPLE_TYPE MultiSample,
                                    DWORD MultisampleQuality,
                                    BOOL Lockable,
                                    IDirect3DSurface9 **ppSurface,
                                    HANDLE *pSharedHandle,
                                    DWORD Usage );

HRESULT NINE_WINAPI
NineDevice9Ex_CreateOffscreenPlainSurfaceEx( struct NineDevice9Ex *This,
                                             UINT Width,
                                             UINT Height,
                                             D3DFORMAT Format,
                                             D3DPOOL Pool,
                                             IDirect3DSurface9 **ppSurface,
                                             HANDLE *pSharedHandle,
                                             DWORD Usage );

HRESULT NINE_WINAPI
NineDevice9Ex_CreateDepthStencilSurfaceEx( struct NineDevice9Ex *This,
                                           UINT Width,
                                           UINT Height,
                                           D3DFORMAT Format,
                                           D3DMULTISAMPLE_TYPE MultiSample,
                                           DWORD MultisampleQuality,
                                           BOOL Discard,
                                           IDirect3DSurface9 **ppSurface,
                                           HANDLE *pSharedHandle,
                                           DWORD Usage );

HRESULT NINE_WINAPI
NineDevice9Ex_ResetEx( struct NineDevice9Ex *This,
                       D3DPRESENT_PARAMETERS *pPresentationParameters,
                       D3DDISPLAYMODEEX *pFullscreenDisplayMode );

HRESULT NINE_WINAPI
NineDevice9Ex_Reset( struct NineDevice9Ex *This,
                     D3DPRESENT_PARAMETERS *pPresentationParameters );

HRESULT NINE_WINAPI
NineDevice9Ex_GetDisplayModeEx( struct NineDevice9Ex *This,
                                UINT iSwapChain,
                                D3DDISPLAYMODEEX *pMode,
                                D3DDISPLAYROTATION *pRotation );

HRESULT NINE_WINAPI
NineDevice9Ex_TestCooperativeLevel( struct NineDevice9Ex *This );

#endif /* _NINE_DEVICE9EX_H_ */
