/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include "etnaviv_ml.h"

void
etna_ml_lower_convolution(struct etna_ml_subgraph *subgraph,
                          const struct pipe_ml_operation *poperation,
                          struct etna_operation *operation);

void
etna_ml_lower_add(struct etna_ml_subgraph *subgraph,
                  const struct pipe_ml_operation *poperation,
                  struct etna_operation *operation);

void
etna_ml_compile_operation_nn(struct etna_ml_subgraph *subgraph,
                             const struct etna_operation *operation,
                             struct etna_vip_instruction *instruction);

void
etna_ml_emit_operation_nn(struct etna_ml_subgraph *subgraph,
                          struct etna_vip_instruction *operation,
                          unsigned idx);