/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "intel/common/intel_engine.h"

enum decode_color {
   DECODE_COLOR_AUTO,
   DECODE_COLOR_ALWAYS,
   DECODE_COLOR_NEVER
};

int ring_name_to_class(const char *ring_name, enum intel_engine_class *class);

void
dump_shader_binary(void *user_data, const char *short_name,
                   uint64_t address, const void *data,
                   unsigned data_length);
