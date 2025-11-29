/*
 * Copyright © 2015 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef ELK_NIR_PRIVATE_H
#define ELK_NIR_PRIVATE_H

#include "elk_reg.h"
#include "nir.h"

#ifdef __cplusplus
extern "C" {
#endif

enum elk_conditional_mod elk_cmod_for_nir_comparison(nir_op op);
enum elk_lsc_opcode elk_lsc_aop_for_nir_intrinsic(const nir_intrinsic_instr *atomic);
enum elk_reg_type elk_type_for_nir_type(const struct intel_device_info *devinfo,
                                        nir_alu_type type);

#ifdef __cplusplus
}
#endif

#endif
