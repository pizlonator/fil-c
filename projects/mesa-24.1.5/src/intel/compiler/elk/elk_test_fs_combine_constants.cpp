/*
 * Copyright (c) 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include "elk_fs.h"
#include "elk_fs_builder.h"
#include "elk_cfg.h"

using namespace elk;

struct FSCombineConstantsTest : public ::testing::Test {
   FSCombineConstantsTest() {
      mem_ctx = ralloc_context(NULL);

      devinfo = {};
      devinfo.ver = 9;
      devinfo.verx10 = 90;

      compiler = {};
      compiler.devinfo = &devinfo;
      elk_init_isa_info(&compiler.isa, &devinfo);

      params = {};
      params.mem_ctx = mem_ctx;

      prog_data = {};
      nir_shader *nir =
         nir_shader_create(mem_ctx, MESA_SHADER_COMPUTE, NULL, NULL);

      shader = new elk_fs_visitor(&compiler, &params, NULL,
                              &prog_data.base, nir, 8, false, false);
   }

   ~FSCombineConstantsTest() override {
      delete shader;
      ralloc_free(mem_ctx);
      mem_ctx = NULL;
   }

   void *mem_ctx;
   elk_compiler compiler;
   elk_compile_params params;
   intel_device_info devinfo;
   struct elk_wm_prog_data prog_data;
   struct gl_shader_program *shader_prog;

   elk_fs_visitor *shader;

   bool opt_combine_constants(elk_fs_visitor *s) {
      const bool print = getenv("TEST_DEBUG");

      if (print) {
         fprintf(stderr, "= Before =\n");
         s->cfg->dump();
      }

      bool ret = s->opt_combine_constants();

      if (print) {
         fprintf(stderr, "\n= After =\n");
         s->cfg->dump();
      }

      return ret;
   }
};

static fs_builder
make_builder(elk_fs_visitor *s)
{
   return fs_builder(s, s->dispatch_width).at_end();
}

TEST_F(FSCombineConstantsTest, Simple)
{
   fs_builder bld = make_builder(shader);

   elk_fs_reg r = elk_vec8_grf(1, 0);
   elk_fs_reg imm_a = elk_imm_ud(1);
   elk_fs_reg imm_b = elk_imm_ud(2);

   bld.SEL(r, imm_a, imm_b);
   shader->calculate_cfg();

   bool progress = opt_combine_constants(shader);
   ASSERT_TRUE(progress);

   ASSERT_EQ(shader->cfg->num_blocks, 1);
   elk_bblock_t *block = cfg_first_block(shader->cfg);
   ASSERT_NE(block, nullptr);

   /* We can do better but for now sanity check that
    * there's a MOV and a SEL.
    */
   ASSERT_EQ(bblock_start(block)->opcode, ELK_OPCODE_MOV);
   ASSERT_EQ(bblock_end(block)->opcode, ELK_OPCODE_SEL);
}

TEST_F(FSCombineConstantsTest, DoContainingDo)
{
   fs_builder bld = make_builder(shader);

   elk_fs_reg r1 = elk_vec8_grf(1, 0);
   elk_fs_reg r2 = elk_vec8_grf(2, 0);
   elk_fs_reg imm_a = elk_imm_ud(1);
   elk_fs_reg imm_b = elk_imm_ud(2);

   bld.DO();
   bld.DO();
   bld.SEL(r1, imm_a, imm_b);
   bld.WHILE();
   bld.WHILE();
   bld.SEL(r2, imm_a, imm_b);
   shader->calculate_cfg();

   unsigned original_num_blocks = shader->cfg->num_blocks;

   bool progress = opt_combine_constants(shader);
   ASSERT_TRUE(progress);

   /* We can do better but for now sanity check there's
    * enough blocks, since the original issue motivating this
    * test is that the shader would be empty.
    */
   ASSERT_GE(shader->cfg->num_blocks, original_num_blocks);
   shader->validate();
}

