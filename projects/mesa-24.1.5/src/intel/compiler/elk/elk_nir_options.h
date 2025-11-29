/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef ELK_NIR_OPTIONS_H
#define ELK_NIR_OPTIONS_H

#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct nir_shader_compiler_options elk_scalar_nir_options;
extern const struct nir_shader_compiler_options elk_vector_nir_options;

#ifdef __cplusplus
}
#endif

#endif /* ELK_NIR_OPTIONS_H */
