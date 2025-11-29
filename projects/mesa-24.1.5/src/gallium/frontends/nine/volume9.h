/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_VOLUME9_H_
#define _NINE_VOLUME9_H_

#include "iunknown.h"

#include "pipe/p_state.h"
#include "util/u_inlines.h"

struct hash_table;

struct NineDevice9;

struct NineVolume9
{
    struct NineUnknown base;

    struct pipe_resource *resource;
    unsigned level;
    unsigned level_actual;

    uint8_t *data; /* system memory backing */
    uint8_t *data_internal; /* for conversions */

    D3DVOLUME_DESC desc;
    struct pipe_resource info;
    enum pipe_format format_internal;
    unsigned stride;
    unsigned stride_internal;
    unsigned layer_stride;
    unsigned layer_stride_internal;

    struct pipe_transfer *transfer;
    unsigned lock_count;

    unsigned pending_uploads_counter; /* pending uploads */
};
static inline struct NineVolume9 *
NineVolume9( void *data )
{
    return (struct NineVolume9 *)data;
}

HRESULT
NineVolume9_new( struct NineDevice9 *pDevice,
                 struct NineUnknown *pContainer,
                 struct pipe_resource *pResource,
                 unsigned Level,
                 D3DVOLUME_DESC *pDesc,
                 struct NineVolume9 **ppOut );

/*** Nine private ***/

static inline void
NineVolume9_SetResource( struct NineVolume9 *This,
                         struct pipe_resource *resource, unsigned level )
{
    This->level = level;
    pipe_resource_reference(&This->resource, resource);
}

void
NineVolume9_AddDirtyRegion( struct NineVolume9 *This,
                            const struct pipe_box *box );

void
NineVolume9_CopyMemToDefault( struct NineVolume9 *This,
                              struct NineVolume9 *From,
                              unsigned dstx, unsigned dsty, unsigned dstz,
                              struct pipe_box *pSrcBox );

HRESULT
NineVolume9_UploadSelf( struct NineVolume9 *This,
                        const struct pipe_box *damaged );


/*** Direct3D public ***/

HRESULT NINE_WINAPI
NineVolume9_GetContainer( struct NineVolume9 *This,
                          REFIID riid,
                          void **ppContainer );

HRESULT NINE_WINAPI
NineVolume9_GetDesc( struct NineVolume9 *This,
                     D3DVOLUME_DESC *pDesc );

HRESULT NINE_WINAPI
NineVolume9_LockBox( struct NineVolume9 *This,
                     D3DLOCKED_BOX *pLockedVolume,
                     const D3DBOX *pBox,
                     DWORD Flags );

HRESULT NINE_WINAPI
NineVolume9_UnlockBox( struct NineVolume9 *This );

#endif /* _NINE_VOLUME9_H_ */
