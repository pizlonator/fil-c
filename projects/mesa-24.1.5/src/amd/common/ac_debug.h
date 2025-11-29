/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AC_DEBUG_H
#define AC_DEBUG_H

#include "amd_family.h"
#include "ac_gpu_info.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define AC_ENCODE_TRACE_POINT(id) (0xcafe0000 | ((id)&0xffff))
#define AC_IS_TRACE_POINT(x)      (((x)&0xcafe0000) == 0xcafe0000)
#define AC_GET_TRACE_POINT_ID(x)  ((x)&0xffff)

#define AC_MAX_WAVES_PER_CHIP (64 * 40)

#ifdef __cplusplus
extern "C" {
#endif

struct si_reg;

struct ac_wave_info {
   unsigned se; /* shader engine */
   unsigned sh; /* shader array */
   unsigned cu; /* compute unit */
   unsigned simd;
   unsigned wave;
   uint32_t status;
   union {
      uint64_t pc; /* program counter */
      struct {
         uint32_t pc_lo;
         uint32_t pc_hi;
      };
   };
   uint32_t inst_dw0;
   uint32_t inst_dw1;
   union {
      uint64_t exec;
      struct {
         uint32_t exec_lo;
         uint32_t exec_hi;
      };
   };
   bool matched; /* whether the wave is used by a currently-bound shader */
};

struct ac_addr_info {
   void *cpu_addr;
   bool valid;
   bool use_after_free;
};

typedef void (*ac_debug_addr_callback)(void *data, uint64_t addr, struct ac_addr_info *info);

/* ac_debug.c */
const struct si_reg *ac_find_register(enum amd_gfx_level gfx_level, enum radeon_family family,
                                      unsigned offset);
const char *ac_get_register_name(enum amd_gfx_level gfx_level, enum radeon_family family,
                                 unsigned offset);
bool ac_register_exists(enum amd_gfx_level gfx_level, enum radeon_family family,
                        unsigned offset);
bool ac_vm_fault_occurred(enum amd_gfx_level gfx_level, uint64_t *old_dmesg_timestamp,
                         uint64_t *out_addr);
unsigned ac_get_wave_info(enum amd_gfx_level gfx_level, const struct radeon_info *info,
                          struct ac_wave_info waves[AC_MAX_WAVES_PER_CHIP]);
void ac_print_gpuvm_fault_status(FILE *output, enum amd_gfx_level gfx_level,
                                 uint32_t status);

/* ac_gather_context_rolls.c */
struct hash_table;
void ac_gather_context_rolls(FILE *f, uint32_t **ibs, uint32_t *ib_dw_sizes, unsigned num_ibs,
                             struct hash_table *annotations, const struct radeon_info *info);

/* ac_parse_ib.c */

struct ac_ib_parser {
   /* Arguments to ac_parse_ib.* */
   FILE *f;
   uint32_t *ib;
   unsigned num_dw;
   const int *trace_ids;
   unsigned trace_id_count;
   enum amd_gfx_level gfx_level;
   enum radeon_family family;
   enum amd_ip_type ip_type;
   ac_debug_addr_callback addr_callback;
   void *addr_callback_data;
   struct hash_table *annotations;

   /* Internal */
   unsigned cur_dw;
};

void ac_dump_reg(FILE *file, enum amd_gfx_level gfx_level, enum radeon_family family,
                 unsigned offset, uint32_t value, uint32_t field_mask);
void ac_parse_ib_chunk(struct ac_ib_parser *ib);
void ac_parse_ib(struct ac_ib_parser *ib, const char *name);

#ifdef __cplusplus
}
#endif

#endif
