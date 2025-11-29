/*
 * Copyright © 2017 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef INTEL_DECODER_PRIVATE_H
#define INTEL_DECODER_PRIVATE_H

#include "intel_decoder.h"

void intel_batch_decode_ctx_init(struct intel_batch_decode_ctx *ctx,
                                 const struct intel_device_info *devinfo,
                                 FILE *fp, enum intel_batch_decode_flags flags,
                                 const char *xml_path,
                                 struct intel_batch_decode_bo (*get_bo)(void *,
                                                                        bool,
                                                                        uint64_t),
                                 unsigned (*get_state_size)(void *, uint64_t,
                                                            uint64_t),
                                 void *user_data);

struct intel_batch_decode_bo
ctx_get_bo(struct intel_batch_decode_ctx *ctx, bool ppgtt, uint64_t addr);

#endif /* INTEL_DECODER_PRIVATE_H */
