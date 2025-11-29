/*
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_DEFINES_H
#define R300_DEFINES_H

#include "pipe/p_defines.h"

#define R300_MAX_TEXTURE_LEVELS         13
#define R300_MAX_DRAW_VBO_SIZE          (1024 * 1024)

#define R300_RESOURCE_FLAG_TRANSFER     (PIPE_RESOURCE_FLAG_DRV_PRIV << 0)
#define R300_RESOURCE_FORCE_MICROTILING (PIPE_RESOURCE_FLAG_DRV_PRIV << 1)

#define R300_INVALID_FORMAT 0xffff

#endif
