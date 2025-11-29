/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_CUBETEXTURE9_H_
#define _NINE_CUBETEXTURE9_H_

#include "basetexture9.h"
#include "nine_memory_helper.h"
#include "surface9.h"

struct NineCubeTexture9
{
    struct NineBaseTexture9 base;
    struct NineSurface9 **surfaces;
    struct pipe_box dirty_rect[6]; /* covers all mip levels */
    struct nine_allocation *managed_buffer;
};
static inline struct NineCubeTexture9 *
NineCubeTexture9( void *data )
{
    return (struct NineCubeTexture9 *)data;
}

HRESULT
NineCubeTexture9_new( struct NineDevice9 *pDevice,
                      UINT EdgeLength, UINT Levels,
                      DWORD Usage,
                      D3DFORMAT Format,
                      D3DPOOL Pool,
                      struct NineCubeTexture9 **ppOut,
                      HANDLE *pSharedHandle );

HRESULT NINE_WINAPI
NineCubeTexture9_GetLevelDesc( struct NineCubeTexture9 *This,
                               UINT Level,
                               D3DSURFACE_DESC *pDesc );

HRESULT NINE_WINAPI
NineCubeTexture9_GetCubeMapSurface( struct NineCubeTexture9 *This,
                                    D3DCUBEMAP_FACES FaceType,
                                    UINT Level,
                                    IDirect3DSurface9 **ppCubeMapSurface );

HRESULT NINE_WINAPI
NineCubeTexture9_LockRect( struct NineCubeTexture9 *This,
                           D3DCUBEMAP_FACES FaceType,
                           UINT Level,
                           D3DLOCKED_RECT *pLockedRect,
                           const RECT *pRect,
                           DWORD Flags );

HRESULT NINE_WINAPI
NineCubeTexture9_UnlockRect( struct NineCubeTexture9 *This,
                             D3DCUBEMAP_FACES FaceType,
                             UINT Level );

HRESULT NINE_WINAPI
NineCubeTexture9_AddDirtyRect( struct NineCubeTexture9 *This,
                               D3DCUBEMAP_FACES FaceType,
                               const RECT *pDirtyRect );

#endif /* _NINE_CUBETEXTURE9_H_ */
