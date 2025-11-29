/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>

void read_xe_data_file(FILE *dump_file, FILE *hang_dump_file, bool verbose);
