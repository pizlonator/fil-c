/* -*- c++ -*- */
/*
 * Copyright © 2021 Intel Corporation
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

#ifndef ELK_PRIVATE_H
#define ELK_PRIVATE_H

#include "elk_compiler.h"

#include <variant>

unsigned elk_required_dispatch_width(const struct shader_info *info);

static constexpr int SIMD_COUNT = 3;

struct elk_simd_selection_state {
   const struct intel_device_info *devinfo;

   std::variant<struct elk_cs_prog_data *,
                struct elk_bs_prog_data *> prog_data;

   unsigned required_width;

   const char *error[SIMD_COUNT];

   bool compiled[SIMD_COUNT];
   bool spilled[SIMD_COUNT];
};

inline int elk_simd_first_compiled(const elk_simd_selection_state &state)
{
   for (int i = 0; i < SIMD_COUNT; i++) {
      if (state.compiled[i])
         return i;
   }
   return -1;
}

inline bool elk_simd_any_compiled(const elk_simd_selection_state &state)
{
   return elk_simd_first_compiled(state) >= 0;
}

bool elk_simd_should_compile(elk_simd_selection_state &state, unsigned simd);

void elk_simd_mark_compiled(elk_simd_selection_state &state, unsigned simd, bool spilled);

int elk_simd_select(const elk_simd_selection_state &state);

int elk_simd_select_for_workgroup_size(const struct intel_device_info *devinfo,
                                       const struct elk_cs_prog_data *prog_data,
                                       const unsigned *sizes);

bool elk_should_print_shader(const nir_shader *shader, uint64_t debug_flag);

#endif // ELK_PRIVATE_H
