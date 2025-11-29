/*
 * Copyright 2021 Alyssa Rosenzweig
 * Copyright 2020 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "util/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
enum agx_compiler_dbg {
   /* bit 0 unused */
   AGX_DBG_SHADERS     = BITFIELD_BIT(1),
   AGX_DBG_SHADERDB    = BITFIELD_BIT(2),
   AGX_DBG_VERBOSE     = BITFIELD_BIT(3),
   AGX_DBG_INTERNAL    = BITFIELD_BIT(4),
   AGX_DBG_NOVALIDATE  = BITFIELD_BIT(5),
   AGX_DBG_NOOPT       = BITFIELD_BIT(6),
   AGX_DBG_WAIT        = BITFIELD_BIT(7),
   AGX_DBG_NOPREAMBLE  = BITFIELD_BIT(8),
   AGX_DBG_DEMAND      = BITFIELD_BIT(9),
   AGX_DBG_NOSCHED     = BITFIELD_BIT(10),
   AGX_DBG_SPILL       = BITFIELD_BIT(11),
   AGX_DBG_NOPROMOTE   = BITFIELD_BIT(12),
};
/* clang-format on */

uint64_t agx_get_compiler_debug(void);

#ifdef __cplusplus
} /* extern C */
#endif
