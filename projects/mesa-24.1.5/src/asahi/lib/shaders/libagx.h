/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBAGX_H
#define LIBAGX_H

/* Define stdint types compatible between the CPU and GPU for shared headers */
#ifndef __OPENCL_VERSION__
#include <stdint.h>
#include "util/macros.h"
#define GLOBAL(type_)            uint64_t
#define AGX_STATIC_ASSERT(_COND) static_assert(_COND, #_COND)
#else
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#define PACKED        __attribute__((packed, aligned(4)))
#define GLOBAL(type_) global type_ *

typedef ulong uint64_t;
typedef uint uint32_t;
typedef ushort uint16_t;
typedef uint uint8_t;

typedef long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef int int8_t;

/* Define NIR intrinsics for CL */
uint32_t nir_interleave_agx(uint16_t x, uint16_t y);
void nir_doorbell_agx(uint8_t value);
void nir_stack_map_agx(uint16_t index, uint32_t address);
uint32_t nir_stack_unmap_agx(uint16_t index);
uint32_t nir_load_core_id_agx(void);
uint32_t nir_load_helper_op_id_agx(void);
uint32_t nir_load_helper_arg_lo_agx(void);
uint32_t nir_load_helper_arg_hi_agx(void);
uint32_t nir_fence_helper_exit_agx(void);

/* I have no idea why CL doesn't have this */
uint ballot(bool cond);

#define AGX_STATIC_ASSERT(_COND)                                               \
   typedef char static_assertion_##__line__[(_COND) ? 1 : -1]

#endif

#endif
