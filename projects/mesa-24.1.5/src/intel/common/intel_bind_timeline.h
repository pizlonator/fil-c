/*
 * Copyright 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/simple_mtx.h"

struct intel_bind_timeline {
   simple_mtx_t mutex;
   uint32_t syncobj;
   uint64_t point;
};

bool intel_bind_timeline_init(struct intel_bind_timeline *intel_bind_timeline, int fd);
void intel_bind_timeline_finish(struct intel_bind_timeline *bind_timeline, int fd);

uint64_t intel_bind_timeline_bind_begin(struct intel_bind_timeline *bind_timeline);
void intel_bind_timeline_bind_end(struct intel_bind_timeline *bind_timeline);
uint32_t intel_bind_timeline_get_syncobj(struct intel_bind_timeline *bind_timeline);
uint64_t intel_bind_timeline_get_last_point(struct intel_bind_timeline *bind_timeline);
