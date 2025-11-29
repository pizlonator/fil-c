/*
 * Copyright © 2023 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "isl/isl.h"

struct iris_bo;
struct iris_bufmgr;

bool iris_i915_bo_busy_gem(struct iris_bo *bo);
int iris_i915_bo_wait_gem(struct iris_bo *bo, int64_t timeout_ns);
int iris_i915_bo_get_tiling(struct iris_bo *bo, uint32_t *tiling);
int iris_i915_bo_set_tiling(struct iris_bo *bo, const struct isl_surf *surf);
uint64_t iris_i915_tiling_to_modifier(uint32_t tiling);

bool iris_i915_init_global_vm(struct iris_bufmgr *bufmgr, uint32_t *vm_id);
