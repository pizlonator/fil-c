/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "aubinator_error_decode_lib.h"
#include "decoder/intel_decoder.h"

void
read_xe_data_file(FILE *file,
                  enum intel_batch_decode_flags batch_flags,
                  const char *spec_xml_path,
                  bool option_dump_kernels,
                  bool option_print_all_bb,
                  enum decode_color option_color);
