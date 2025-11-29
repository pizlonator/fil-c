/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef ELK_DISASM_H
#define ELK_DISASM_H

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct elk_isa_info;
struct elk_inst;

const struct elk_label *elk_find_label(const struct elk_label *root, int offset);
void elk_create_label(struct elk_label **labels, int offset, void *mem_ctx);
int elk_disassemble_inst(FILE *file, const struct elk_isa_info *isa,
                         const struct elk_inst *inst, bool is_compacted,
                         int offset, const struct elk_label *root_label);
const struct
elk_label *elk_label_assembly(const struct elk_isa_info *isa,
                              const void *assembly, int start, int end,
                              void *mem_ctx);
void elk_disassemble_with_labels(const struct elk_isa_info *isa,
                                 const void *assembly, int start, int end, FILE *out);
void elk_disassemble(const struct elk_isa_info *isa,
                     const void *assembly, int start, int end,
                     const struct elk_label *root_label, FILE *out);
int elk_disassemble_find_end(const struct elk_isa_info *isa,
                             const void *assembly, int start);
void elk_disassemble_with_errors(const struct elk_isa_info *isa,
                                 const void *assembly, int start, FILE *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ELK_DISASM_H */
