/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "etna_hwdb.h"

#include "etna_core_info.h"
#include "hwdb.h"

/* clang-format off */
#define ETNA_FEATURE(member, feature)                                                        \
   if (db->member)                                                                           \
      etna_core_enable_feature(info, ETNA_FEATURE_##feature)
/* clang-format on */

bool
etna_query_feature_db(struct etna_core_info *info)
{
   gcsFEATURE_DATABASE *db = gcQueryFeatureDB(info->model, info->revision, info->product_id,
                                              info->eco_id, info->customer_id);

   if (!db)
      return false;

   if (db->NNCoreCount)
      info->type = ETNA_CORE_NPU;
   else
      info->type = ETNA_CORE_GPU;

   /* Features: */
   ETNA_FEATURE(REG_FastClear, FAST_CLEAR);
   ETNA_FEATURE(REG_Pipe3D, PIPE_3D);
   ETNA_FEATURE(REG_FE20BitIndex, 32_BIT_INDICES);
   ETNA_FEATURE(REG_MSAA, MSAA);
   ETNA_FEATURE(REG_DXTTextureCompression, DXT_TEXTURE_COMPRESSION);
   ETNA_FEATURE(REG_ETC1TextureCompression, ETC1_TEXTURE_COMPRESSION);
   ETNA_FEATURE(REG_NoEZ, NO_EARLY_Z);

   ETNA_FEATURE(REG_MC20, MC20);
   ETNA_FEATURE(REG_Render8K, RENDERTARGET_8K);
   ETNA_FEATURE(REG_Texture8K, TEXTURE_8K);
   ETNA_FEATURE(REG_ExtraShaderInstructions0, HAS_SIGN_FLOOR_CEIL);
   ETNA_FEATURE(REG_ExtraShaderInstructions1, HAS_SQRT_TRIG);
   ETNA_FEATURE(REG_TileStatus2Bits, 2BITPERTILE);
   ETNA_FEATURE(REG_SuperTiled32x32, SUPER_TILED);

   ETNA_FEATURE(REG_CorrectAutoDisable1, AUTO_DISABLE);
   ETNA_FEATURE(REG_TextureHorizontalAlignmentSelect, TEXTURE_HALIGN);
   ETNA_FEATURE(REG_MMU, MMU_VERSION);
   ETNA_FEATURE(REG_HalfFloatPipe, HALF_FLOAT);
   ETNA_FEATURE(REG_WideLine, WIDE_LINE);
   ETNA_FEATURE(REG_Halti0, HALTI0);
   ETNA_FEATURE(REG_NonPowerOfTwo, NON_POWER_OF_TWO);
   ETNA_FEATURE(REG_LinearTextureSupport, LINEAR_TEXTURE_SUPPORT);

   ETNA_FEATURE(REG_LinearPE, LINEAR_PE);
   ETNA_FEATURE(REG_SuperTiledTexture, SUPERTILED_TEXTURE);
   ETNA_FEATURE(REG_LogicOp, LOGIC_OP);
   ETNA_FEATURE(REG_Halti1, HALTI1);
   ETNA_FEATURE(REG_SeamlessCubeMap, SEAMLESS_CUBE_MAP);
   ETNA_FEATURE(REG_LineLoop, LINE_LOOP);
   ETNA_FEATURE(REG_TextureTileStatus, TEXTURE_TILED_READ);
   ETNA_FEATURE(REG_BugFixes8, BUG_FIXES8);

   ETNA_FEATURE(REG_BugFixes15, PE_DITHER_FIX);
   ETNA_FEATURE(REG_InstructionCache, INSTRUCTION_CACHE);
   ETNA_FEATURE(REG_ExtraShaderInstructions2, HAS_FAST_TRANSCENDENTALS);

   ETNA_FEATURE(REG_SmallMSAA, SMALL_MSAA);
   ETNA_FEATURE(REG_BugFixes18, BUG_FIXES18);
   ETNA_FEATURE(REG_TXEnhancements4, TEXTURE_ASTC);
   ETNA_FEATURE(REG_PEEnhancements3, SINGLE_BUFFER);
   ETNA_FEATURE(REG_Halti2, HALTI2);

   ETNA_FEATURE(REG_BltEngine, BLT_ENGINE);
   ETNA_FEATURE(REG_Halti3, HALTI3);
   ETNA_FEATURE(REG_Halti4, HALTI4);
   ETNA_FEATURE(REG_Halti5, HALTI5);
   ETNA_FEATURE(REG_RAWriteDepth, RA_WRITE_DEPTH);

   ETNA_FEATURE(CACHE128B256BPERLINE, CACHE128B256BPERLINE);
   ETNA_FEATURE(NEW_GPIPE, NEW_GPIPE);
   ETNA_FEATURE(NO_ASTC, NO_ASTC);
   ETNA_FEATURE(V4Compression, V4_COMPRESSION);

   ETNA_FEATURE(RS_NEW_BASEADDR, RS_NEW_BASEADDR);
   ETNA_FEATURE(PE_NO_ALPHA_TEST, PE_NO_ALPHA_TEST);

   ETNA_FEATURE(SH_NO_ONECONST_LIMIT, SH_NO_ONECONST_LIMIT);

   ETNA_FEATURE(DEC400, DEC400);

   ETNA_FEATURE(VIP_V7, VIP_V7);
   ETNA_FEATURE(NN_XYDP0, NN_XYDP0);

   /* Limits: */
   if (info->type == ETNA_CORE_GPU) {
      info->gpu.max_instructions = db->InstructionCount;
      info->gpu.vertex_output_buffer_size = db->VertexOutputBufferSize;
      info->gpu.vertex_cache_size = db->VertexCacheSize;
      info->gpu.shader_core_count = db->NumShaderCores;
      info->gpu.stream_count = db->Streams;
      info->gpu.max_registers = db->TempRegisters;
      info->gpu.pixel_pipes = db->NumPixelPipes;
      info->gpu.max_varyings = db->VaryingCount;
      info->gpu.num_constants = db->NumberOfConstants;
   } else {
      info->npu.nn_core_count = db->NNCoreCount;
      info->npu.nn_mad_per_core = db->NNMadPerCore;
      info->npu.tp_core_count = db->TPEngine_CoreCount;
      info->npu.on_chip_sram_size = db->VIP_SRAM_SIZE;
      info->npu.axi_sram_size = db->AXI_SRAM_SIZE;
      info->npu.nn_zrl_bits = db->NN_ZRL_BITS;
   }

   return true;
}
