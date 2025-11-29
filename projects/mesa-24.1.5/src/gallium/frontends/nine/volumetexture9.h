/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_VOLUMETEXTURE9_H_
#define _NINE_VOLUMETEXTURE9_H_

#include "basetexture9.h"
#include "volume9.h"

struct NineVolumeTexture9
{
    struct NineBaseTexture9 base;
    struct NineVolume9 **volumes;
    struct pipe_box dirty_box;
};
static inline struct NineVolumeTexture9 *
NineVolumeTexture9( void *data )
{
    return (struct NineVolumeTexture9 *)data;
}

HRESULT
NineVolumeTexture9_new( struct NineDevice9 *pDevice,
                        UINT Width, UINT Height, UINT Depth, UINT Levels,
                        DWORD Usage,
                        D3DFORMAT Format,
                        D3DPOOL Pool,
                        struct NineVolumeTexture9 **ppOut,
                        HANDLE *pSharedHandle );

HRESULT NINE_WINAPI
NineVolumeTexture9_GetLevelDesc( struct NineVolumeTexture9 *This,
                                 UINT Level,
                                 D3DVOLUME_DESC *pDesc );

HRESULT NINE_WINAPI
NineVolumeTexture9_GetVolumeLevel( struct NineVolumeTexture9 *This,
                                   UINT Level,
                                   IDirect3DVolume9 **ppVolumeLevel );

HRESULT NINE_WINAPI
NineVolumeTexture9_LockBox( struct NineVolumeTexture9 *This,
                            UINT Level,
                            D3DLOCKED_BOX *pLockedVolume,
                            const D3DBOX *pBox,
                            DWORD Flags );

HRESULT NINE_WINAPI
NineVolumeTexture9_UnlockBox( struct NineVolumeTexture9 *This,
                              UINT Level );

HRESULT NINE_WINAPI
NineVolumeTexture9_AddDirtyBox( struct NineVolumeTexture9 *This,
                                const D3DBOX *pDirtyBox );

#endif /* _NINE_VOLUMETEXTURE9_H_ */
