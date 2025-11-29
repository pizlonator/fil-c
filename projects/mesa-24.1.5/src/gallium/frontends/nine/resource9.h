/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_RESOURCE9_H_
#define _NINE_RESOURCE9_H_

#include "iunknown.h"
#include "pipe/p_state.h"

struct pipe_screen;
struct hash_table;
struct NineDevice9;

struct NineResource9
{
    struct NineUnknown base;

    struct pipe_resource *resource; /* device resource */

    D3DRESOURCETYPE type;
    D3DPOOL pool;
    DWORD priority;
    DWORD usage;

    struct pipe_resource info; /* resource configuration */

    long long size;
};
static inline struct NineResource9 *
NineResource9( void *data )
{
    return (struct NineResource9 *)data;
}

HRESULT
NineResource9_ctor( struct NineResource9 *This,
                    struct NineUnknownParams *pParams,
                    struct pipe_resource *initResource,
                    BOOL Allocate,
                    D3DRESOURCETYPE Type,
                    D3DPOOL Pool,
                    DWORD Usage);

void
NineResource9_dtor( struct NineResource9 *This );

/*** Nine private methods ***/

struct pipe_resource *
NineResource9_GetResource( struct NineResource9 *This );

D3DPOOL
NineResource9_GetPool( struct NineResource9 *This );

/*** Direct3D public methods ***/

DWORD NINE_WINAPI
NineResource9_SetPriority( struct NineResource9 *This,
                           DWORD PriorityNew );

DWORD NINE_WINAPI
NineResource9_GetPriority( struct NineResource9 *This );

void NINE_WINAPI
NineResource9_PreLoad( struct NineResource9 *This );

D3DRESOURCETYPE NINE_WINAPI
NineResource9_GetType( struct NineResource9 *This );

#endif /* _NINE_RESOURCE9_H_ */
