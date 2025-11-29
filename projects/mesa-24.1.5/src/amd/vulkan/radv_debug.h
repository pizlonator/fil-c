/*
 * Copyright © 2017 Google.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_DEBUG_H
#define RADV_DEBUG_H

#include "radv_device.h"
#include "radv_instance.h"
#include "radv_physical_device.h"

/* Please keep docs/envvars.rst up-to-date when you add/remove options. */
enum {
   RADV_DEBUG_NO_FAST_CLEARS = 1ull << 0,
   RADV_DEBUG_NO_DCC = 1ull << 1,
   RADV_DEBUG_DUMP_SHADERS = 1ull << 2,
   RADV_DEBUG_NO_CACHE = 1ull << 3,
   RADV_DEBUG_DUMP_SHADER_STATS = 1ull << 4,
   RADV_DEBUG_NO_HIZ = 1ull << 5,
   RADV_DEBUG_NO_COMPUTE_QUEUE = 1ull << 6,
   RADV_DEBUG_ALL_BOS = 1ull << 7,
   RADV_DEBUG_NO_IBS = 1ull << 8,
   RADV_DEBUG_DUMP_SPIRV = 1ull << 9,
   RADV_DEBUG_ZERO_VRAM = 1ull << 10,
   RADV_DEBUG_SYNC_SHADERS = 1ull << 11,
   RADV_DEBUG_PREOPTIR = 1ull << 12,
   RADV_DEBUG_NO_DYNAMIC_BOUNDS = 1ull << 13,
   RADV_DEBUG_INFO = 1ull << 14,
   RADV_DEBUG_STARTUP = 1ull << 15,
   RADV_DEBUG_CHECKIR = 1ull << 16,
   RADV_DEBUG_NOBINNING = 1ull << 17,
   RADV_DEBUG_NO_NGG = 1ull << 18,
   RADV_DEBUG_DUMP_META_SHADERS = 1ull << 19,
   RADV_DEBUG_DISCARD_TO_DEMOTE = 1ull << 20,
   RADV_DEBUG_LLVM = 1ull << 21,
   RADV_DEBUG_FORCE_COMPRESS = 1ull << 22,
   RADV_DEBUG_HANG = 1ull << 23,
   RADV_DEBUG_IMG = 1ull << 24,
   RADV_DEBUG_NO_UMR = 1ull << 25,
   RADV_DEBUG_INVARIANT_GEOM = 1ull << 26,
   RADV_DEBUG_NO_DISPLAY_DCC = 1ull << 27,
   RADV_DEBUG_NO_TC_COMPAT_CMASK = 1ull << 28,
   RADV_DEBUG_NO_VRS_FLAT_SHADING = 1ull << 29,
   RADV_DEBUG_NO_ATOC_DITHERING = 1ull << 30,
   RADV_DEBUG_NO_NGGC = 1ull << 31,
   RADV_DEBUG_DUMP_PROLOGS = 1ull << 32,
   RADV_DEBUG_NO_DMA_BLIT = 1ull << 33,
   RADV_DEBUG_SPLIT_FMA = 1ull << 34,
   RADV_DEBUG_DUMP_EPILOGS = 1ull << 35,
   RADV_DEBUG_NO_FMASK = 1ull << 36,
   RADV_DEBUG_SHADOW_REGS = 1ull << 37,
   RADV_DEBUG_EXTRA_MD = 1ull << 38,
   RADV_DEBUG_NO_GPL = 1ull << 39,
   RADV_DEBUG_VIDEO_ARRAY_PATH = 1ull << 40,
   RADV_DEBUG_NO_RT = 1ull << 41,
   RADV_DEBUG_NO_MESH_SHADER = 1ull << 42,
   RADV_DEBUG_NO_NGG_GS = 1ull << 43,
   RADV_DEBUG_NO_GS_FAST_LAUNCH_2 = 1ull << 44,
   RADV_DEBUG_NO_ESO = 1ull << 45,
   RADV_DEBUG_PSO_CACHE_STATS = 1ull << 46,
};

enum {
   RADV_PERFTEST_LOCAL_BOS = 1u << 0,
   RADV_PERFTEST_DCC_MSAA = 1u << 1,
   RADV_PERFTEST_BO_LIST = 1u << 2,
   RADV_PERFTEST_CS_WAVE_32 = 1u << 3,
   RADV_PERFTEST_PS_WAVE_32 = 1u << 4,
   RADV_PERFTEST_GE_WAVE_32 = 1u << 5,
   RADV_PERFTEST_NO_SAM = 1u << 6,
   RADV_PERFTEST_SAM = 1u << 7,
   RADV_PERFTEST_NGGC = 1u << 8,
   RADV_PERFTEST_EMULATE_RT = 1u << 9,
   RADV_PERFTEST_RT_WAVE_64 = 1u << 10,
   RADV_PERFTEST_VIDEO_DECODE = 1u << 11,
   RADV_PERFTEST_DMA_SHADERS = 1u << 12,
   RADV_PERFTEST_TRANSFER_QUEUE = 1u << 13,
   RADV_PERFTEST_NIR_CACHE = 1u << 14,
   RADV_PERFTEST_RT_WAVE_32 = 1u << 15,
   RADV_PERFTEST_VIDEO_ENCODE = 1u << 16,
};

bool radv_init_trace(struct radv_device *device);
void radv_finish_trace(struct radv_device *device);

void radv_check_gpu_hangs(struct radv_queue *queue, const struct radv_winsys_submit_info *submit_info);

void radv_print_spirv(const char *data, uint32_t size, FILE *fp);

void radv_dump_enabled_options(const struct radv_device *device, FILE *f);

bool radv_trap_handler_init(struct radv_device *device);
void radv_trap_handler_finish(struct radv_device *device);
void radv_check_trap_handler(struct radv_queue *queue);

bool radv_vm_fault_occurred(struct radv_device *device, struct radv_winsys_gpuvm_fault_info *fault_info);


ALWAYS_INLINE static bool
radv_device_fault_detection_enabled(const struct radv_device *device)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   return instance->debug_flags & RADV_DEBUG_HANG;
}

#endif /* RADV_DEBUG_H */
