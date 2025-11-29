/*
 * Copyright 2013 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "util/u_memory.h"

#include "d3dadapter/drm.h"
extern const struct D3DAdapter9DRM drm9_desc;

struct {
    const char *name;
    const void *desc;
} drivers[] = {
    { D3DADAPTER9DRM_NAME, &drm9_desc },
};

PUBLIC const void * WINAPI
D3DAdapter9GetProc( const char *name )
{
    int i;
    for (i = 0; i < ARRAY_SIZE(drivers); ++i) {
        if (strcmp(name, drivers[i].name) == 0) {
            return drivers[i].desc;
        }
    }
    return NULL;
}
