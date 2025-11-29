/*
 * Copyright © 2018 Red Hat.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef RADV_SHADER_HELPER_H
#define RADV_SHADER_HELPER_H

#include "ac_llvm_util.h"

#ifdef __cplusplus
extern "C" {
#endif

bool radv_init_llvm_compiler(struct ac_llvm_compiler *info, enum radeon_family family,
                             enum ac_target_machine_options tm_options, unsigned wave_size);

bool radv_compile_to_elf(struct ac_llvm_compiler *info, LLVMModuleRef module, char **pelf_buffer, size_t *pelf_size);

#ifdef __cplusplus
}
#endif

#endif /* RADV_LLVM_HELPER_H */
