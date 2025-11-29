/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_GUID_H_
#define _NINE_GUID_H_

#include "util/compiler.h"

#include "d3d9types.h"

extern const GUID IID_ID3D9Adapter;

bool
GUID_equal( const GUID *a,
            const GUID *b );

char*
GUID_sprintf( char *guid_str,
              REFGUID id );

#endif /* _NINE_GUID_H_ */
