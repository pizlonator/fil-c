/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_NINEEXOVERLAYEXTENSION_H_
#define _NINE_NINEEXOVERLAYEXTENSION_H_

#include "iunknown.h"

struct Nine9ExOverlayExtension
{
    struct NineUnknown base;
};
static inline struct Nine9ExOverlayExtension *
Nine9ExOverlayExtension( void *data )
{
    return (struct Nine9ExOverlayExtension *)data;
}

HRESULT NINE_WINAPI
Nine9ExOverlayExtension_CheckDeviceOverlayType( struct Nine9ExOverlayExtension *This,
                                                UINT Adapter,
                                                D3DDEVTYPE DevType,
                                                UINT OverlayWidth,
                                                UINT OverlayHeight,
                                                D3DFORMAT OverlayFormat,
                                                D3DDISPLAYMODEEX *pDisplayMode,
                                                D3DDISPLAYROTATION DisplayRotation,
                                                D3DOVERLAYCAPS *pOverlayCaps );

#endif /* _NINE_NINEEXOVERLAYEXTENSION_H_ */
