/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/bitset.h"

enum etna_feature {
   ETNA_FEATURE_FAST_CLEAR,
   ETNA_FEATURE_PIPE_3D,
   ETNA_FEATURE_32_BIT_INDICES,
   ETNA_FEATURE_MSAA,
   ETNA_FEATURE_DXT_TEXTURE_COMPRESSION,
   ETNA_FEATURE_ETC1_TEXTURE_COMPRESSION,
   ETNA_FEATURE_NO_EARLY_Z,
   ETNA_FEATURE_MC20,
   ETNA_FEATURE_RENDERTARGET_8K,
   ETNA_FEATURE_TEXTURE_8K,
   ETNA_FEATURE_HAS_SIGN_FLOOR_CEIL,
   ETNA_FEATURE_HAS_SQRT_TRIG,
   ETNA_FEATURE_2BITPERTILE,
   ETNA_FEATURE_SUPER_TILED,
   ETNA_FEATURE_AUTO_DISABLE,
   ETNA_FEATURE_TEXTURE_HALIGN,
   ETNA_FEATURE_MMU_VERSION,
   ETNA_FEATURE_HALF_FLOAT,
   ETNA_FEATURE_WIDE_LINE,
   ETNA_FEATURE_HALTI0,
   ETNA_FEATURE_NON_POWER_OF_TWO,
   ETNA_FEATURE_LINEAR_TEXTURE_SUPPORT,
   ETNA_FEATURE_LINEAR_PE,
   ETNA_FEATURE_SUPERTILED_TEXTURE,
   ETNA_FEATURE_LOGIC_OP,
   ETNA_FEATURE_HALTI1,
   ETNA_FEATURE_SEAMLESS_CUBE_MAP,
   ETNA_FEATURE_LINE_LOOP,
   ETNA_FEATURE_TEXTURE_TILED_READ,
   ETNA_FEATURE_BUG_FIXES8,
   ETNA_FEATURE_PE_DITHER_FIX,
   ETNA_FEATURE_INSTRUCTION_CACHE,
   ETNA_FEATURE_HAS_FAST_TRANSCENDENTALS,
   ETNA_FEATURE_SMALL_MSAA,
   ETNA_FEATURE_BUG_FIXES18,
   ETNA_FEATURE_TEXTURE_ASTC,
   ETNA_FEATURE_SINGLE_BUFFER,
   ETNA_FEATURE_HALTI2,
   ETNA_FEATURE_BLT_ENGINE,
   ETNA_FEATURE_HALTI3,
   ETNA_FEATURE_HALTI4,
   ETNA_FEATURE_HALTI5,
   ETNA_FEATURE_RA_WRITE_DEPTH,
   ETNA_FEATURE_CACHE128B256BPERLINE,
   ETNA_FEATURE_NEW_GPIPE,
   ETNA_FEATURE_NO_ASTC,
   ETNA_FEATURE_V4_COMPRESSION,
   ETNA_FEATURE_RS_NEW_BASEADDR,
   ETNA_FEATURE_PE_NO_ALPHA_TEST,
   ETNA_FEATURE_SH_NO_ONECONST_LIMIT,
   ETNA_FEATURE_DEC400,
   ETNA_FEATURE_VIP_V7,
   ETNA_FEATURE_NN_XYDP0,
   ETNA_FEATURE_NUM,
};

enum etna_core_type {
   ETNA_CORE_NOT_SUPPORTED = 0,
   ETNA_CORE_GPU,
   ETNA_CORE_NPU,
};

struct etna_core_gpu_info {
   unsigned max_instructions;          /* vertex/fragment shader max instructions */
   unsigned vertex_output_buffer_size; /* size of vertex shader output buffer */
   unsigned vertex_cache_size;         /* size of a cached vertex (?) */
   unsigned shader_core_count;         /* number of shader cores */
   unsigned stream_count;              /* number of vertex streams */
   unsigned max_registers;             /* maximum number of registers */
   unsigned pixel_pipes;               /* available pixel pipes */
   unsigned max_varyings;              /* maximum number of varyings */
   unsigned num_constants;             /* number of constants */
};

struct etna_core_npu_info {
   unsigned nn_core_count;             /* number of NN cores */
   unsigned nn_mad_per_core;           /* number of MAD units per NN core */
   unsigned tp_core_count;             /* number of TP cores */
   unsigned on_chip_sram_size;         /* Size of on-chip SRAM */
   unsigned axi_sram_size;             /* Size of SRAM behind AXI */
   unsigned nn_zrl_bits;               /* Number of bits for zero run-length compression */
};

struct etna_core_info {
   uint32_t model;
   uint32_t revision;
   uint32_t product_id;
   uint32_t eco_id;
   uint32_t customer_id;

   enum etna_core_type type;

   union {
      struct etna_core_gpu_info gpu;
      struct etna_core_npu_info npu;
   };

   BITSET_DECLARE(feature, ETNA_FEATURE_NUM);
};

static inline bool
etna_core_has_feature(const struct etna_core_info *info, enum etna_feature feature)
{
   return BITSET_TEST(info->feature, feature);
}

static inline void
etna_core_disable_feature(struct etna_core_info *info, enum etna_feature feature)
{
   BITSET_CLEAR(info->feature, feature);
}

static inline void
etna_core_enable_feature(struct etna_core_info *info, enum etna_feature feature)
{
   BITSET_SET(info->feature, feature);
}
