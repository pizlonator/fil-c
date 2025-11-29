/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_STATEBLOCK9_H_
#define _NINE_STATEBLOCK9_H_

#include "iunknown.h"

#include "nine_state.h"

enum nine_stateblock_type
{
   NINESBT_ALL,
   NINESBT_VERTEXSTATE,
   NINESBT_PIXELSTATE,
   NINESBT_CUSTOM
};

struct NineStateBlock9
{
    struct NineUnknown base;

    struct nine_state state;

    enum nine_stateblock_type type;
};
static inline struct NineStateBlock9 *
NineStateBlock9( void *data )
{
    return (struct NineStateBlock9 *)data;
}

HRESULT
NineStateBlock9_new( struct NineDevice9 *,
                     struct NineStateBlock9 **ppOut,
                     enum nine_stateblock_type);

HRESULT
NineStateBlock9_ctor( struct NineStateBlock9 *,
                      struct NineUnknownParams *pParams,
                      enum nine_stateblock_type type );

void
NineStateBlock9_dtor( struct NineStateBlock9 * );

HRESULT NINE_WINAPI
NineStateBlock9_Capture( struct NineStateBlock9 *This );

HRESULT NINE_WINAPI
NineStateBlock9_Apply( struct NineStateBlock9 *This );

#endif /* _NINE_STATEBLOCK9_H_ */
