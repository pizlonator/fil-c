/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_QUERY9_H_
#define _NINE_QUERY9_H_

#include "iunknown.h"

enum nine_query_state
{
    NINE_QUERY_STATE_FRESH = 0,
    NINE_QUERY_STATE_RUNNING,
    NINE_QUERY_STATE_ENDED,
};

struct NineQuery9
{
    struct NineUnknown base;
    struct pipe_query *pq;
    DWORD result_size;
    D3DQUERYTYPE type;
    enum nine_query_state state;
    bool instant; /* true if D3DISSUE_BEGIN is not needed / invalid */
    unsigned counter; /* Number of pending Begin/End (0 if internal multithreading off) */
};
static inline struct NineQuery9 *
NineQuery9( void *data )
{
    return (struct NineQuery9 *)data;
}

HRESULT
nine_is_query_supported(struct pipe_screen *screen, D3DQUERYTYPE);

HRESULT
NineQuery9_new( struct NineDevice9 *Device,
                struct NineQuery9 **ppOut,
                D3DQUERYTYPE);

HRESULT
NineQuery9_ctor( struct NineQuery9 *,
                 struct NineUnknownParams *pParams,
                 D3DQUERYTYPE Type );

void
NineQuery9_dtor( struct NineQuery9 * );

D3DQUERYTYPE NINE_WINAPI
NineQuery9_GetType( struct NineQuery9 *This );

DWORD NINE_WINAPI
NineQuery9_GetDataSize( struct NineQuery9 *This );

HRESULT NINE_WINAPI
NineQuery9_Issue( struct NineQuery9 *This,
                  DWORD dwIssueFlags );

HRESULT NINE_WINAPI
NineQuery9_GetData( struct NineQuery9 *This,
                    void *pData,
                    DWORD dwSize,
                    DWORD dwGetDataFlags );

#endif /* _NINE_QUERY9_H_ */
