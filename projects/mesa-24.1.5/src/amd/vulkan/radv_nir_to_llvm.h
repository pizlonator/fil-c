/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_NIR_TO_LLVM_H
#define RADV_NIR_TO_LLVM_H

struct radv_nir_compiler_options;
struct radv_shader_info;
struct nir_shader;
struct radv_shader_binary;
struct radv_shader_args;

void llvm_compile_shader(const struct radv_nir_compiler_options *options, const struct radv_shader_info *info,
                         unsigned shader_count, struct nir_shader *const *shaders, struct radv_shader_binary **binary,
                         const struct radv_shader_args *args);

#endif /* RADV_NIR_TO_LLVM_H */
