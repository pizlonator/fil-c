/*
 * Copyright © 2018 Google
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ACO_INTERFACE_H
#define ACO_INTERFACE_H

#include "aco_shader_info.h"

#include "nir.h"

#include "amd_family.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Special launch size to indicate this dispatch is a 1D dispatch converted into a 2D one */
#define ACO_RT_CONVERTED_2D_LAUNCH_SIZE -1u

struct ac_shader_config;
struct aco_shader_info;
struct aco_vs_prolog_info;
struct aco_ps_epilog_info;
struct radeon_info;

struct aco_compiler_statistic_info {
   char name[32];
   char desc[64];
};

typedef void(aco_callback)(void** priv_ptr, const struct ac_shader_config* config,
                           const char* llvm_ir_str, unsigned llvm_ir_size, const char* disasm_str,
                           unsigned disasm_size, uint32_t* statistics, uint32_t stats_size,
                           uint32_t exec_size, const uint32_t* code, uint32_t code_dw,
                           const struct aco_symbol* symbols, unsigned num_symbols);

typedef void(aco_shader_part_callback)(void** priv_ptr, uint32_t num_sgprs, uint32_t num_vgprs,
                                       const uint32_t* code, uint32_t code_size,
                                       const char* disasm_str, uint32_t disasm_size);

extern const struct aco_compiler_statistic_info* aco_statistic_infos;

void aco_compile_shader(const struct aco_compiler_options* options,
                        const struct aco_shader_info* info, unsigned shader_count,
                        struct nir_shader* const* shaders, const struct ac_shader_args* args,
                        aco_callback* build_binary, void** binary);

void aco_compile_rt_prolog(const struct aco_compiler_options* options,
                           const struct aco_shader_info* info, const struct ac_shader_args* in_args,
                           const struct ac_shader_args* out_args, aco_callback* build_prolog,
                           void** binary);

void aco_compile_vs_prolog(const struct aco_compiler_options* options,
                           const struct aco_shader_info* info,
                           const struct aco_vs_prolog_info* prolog_info,
                           const struct ac_shader_args* args,
                           aco_shader_part_callback* build_prolog, void** binary);

void aco_compile_ps_epilog(const struct aco_compiler_options* options,
                           const struct aco_shader_info* info,
                           const struct aco_ps_epilog_info* epilog_info,
                           const struct ac_shader_args* args,
                           aco_shader_part_callback* build_epilog, void** binary);

void aco_compile_ps_prolog(const struct aco_compiler_options* options,
                           const struct aco_shader_info* info,
                           const struct aco_ps_prolog_info* pinfo,
                           const struct ac_shader_args* args,
                           aco_shader_part_callback* build_prolog, void** binary);

uint64_t aco_get_codegen_flags();

bool aco_is_gpu_supported(const struct radeon_info* info);

bool aco_nir_op_supports_packed_math_16bit(const nir_alu_instr* alu);

void aco_print_asm(const struct radeon_info *info, unsigned wave_size,
                   uint32_t *binary, unsigned num_dw);

#ifdef __cplusplus
}
#endif

#endif
