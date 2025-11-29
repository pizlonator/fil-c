/*
 * Copyright © 2020 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */
#include <llvm/Config/llvm-config.h>

#include "helpers.h"
#include "test_isel-spirv.h"

using namespace aco;

BEGIN_TEST(isel.interp.simple)
   QoShaderModuleCreateInfo vs = qoShaderModuleCreateInfoGLSL(VERTEX,
      layout(location = 0) in vec4 in_color;
      layout(location = 0) out vec4 out_color;
      void main() { out_color = in_color;
      }
   );
   QoShaderModuleCreateInfo fs = qoShaderModuleCreateInfoGLSL(FRAGMENT,
      layout(location = 0) in vec4 in_color;
      layout(location = 0) out vec4 out_color;
      void main() {
         //>> v1: %b_tmp = v_interp_p1_f32 %bx, %pm:m0 attr0.z
         //! v1: %b = v_interp_p2_f32 %by, %pm:m0, (kill)%b_tmp attr0.z
         //! v1: %a_tmp = v_interp_p1_f32 %bx, %pm:m0 attr0.w
         //! v1: %a = v_interp_p2_f32 %by, %pm:m0, (kill)%a_tmp attr0.w
         //! v1: %r_tmp = v_interp_p1_f32 %bx, %pm:m0 attr0.x
         //! v1: %r = v_interp_p2_f32 %by, %pm:m0, (kill)%r_tmp attr0.x
         //! v1: %g_tmp = v_interp_p1_f32 (kill)%bx, %pm:m0 attr0.y
         //! v1: %g = v_interp_p2_f32 (kill)%by, (kill)%pm:m0, (kill)%g_tmp attr0.y
         //! exp (kill)%r, (kill)%g, (kill)%b, (kill)%a mrt0
         out_color = in_color;
      }
   );

   PipelineBuilder pbld(get_vk_device(GFX9));
   pbld.add_vsfs(vs, fs);
   pbld.print_ir(VK_SHADER_STAGE_FRAGMENT_BIT, "ACO IR");
END_TEST

BEGIN_TEST(isel.compute.simple)
   for (unsigned i = GFX7; i <= GFX8; i++) {
      if (!set_variant((amd_gfx_level)i))
         continue;

      QoShaderModuleCreateInfo cs = qoShaderModuleCreateInfoGLSL(COMPUTE,
         layout(local_size_x=1) in;
         layout(binding=0) buffer Buf {
            uint res;
         };
         void main() {
            //>> v1: %data = p_parallelcopy 42
            //! buffer_store_dword (kill)%_, v1: undef, 0, (kill)%data disable_wqm storage:buffer
            res = 42;
         }
      );

      PipelineBuilder pbld(get_vk_device((amd_gfx_level)i));
      pbld.add_cs(cs);
      pbld.print_ir(VK_SHADER_STAGE_COMPUTE_BIT, "ACO IR", true);
   }
END_TEST

BEGIN_TEST(isel.gs.no_outputs)
   for (unsigned i = GFX8; i <= GFX10; i++) {
      if (!set_variant((amd_gfx_level)i))
         continue;

      QoShaderModuleCreateInfo vs = qoShaderModuleCreateInfoGLSL(VERTEX,
         void main() {}
      );

      QoShaderModuleCreateInfo gs = qoShaderModuleCreateInfoGLSL(GEOMETRY,
         layout(points) in;
         layout(points, max_vertices = 1) out;

         void main() {
            EmitVertex();
            EndPrimitive();
         }
      );

      PipelineBuilder pbld(get_vk_device((amd_gfx_level)i));
      pbld.add_stage(VK_SHADER_STAGE_VERTEX_BIT, vs);
      pbld.add_stage(VK_SHADER_STAGE_GEOMETRY_BIT, gs);
      pbld.create_pipeline();

      //! success
      fprintf(output, "success\n");
   }
END_TEST

BEGIN_TEST(isel.gs.no_verts)
   for (unsigned i = GFX8; i <= GFX10; i++) {
      if (!set_variant((amd_gfx_level)i))
         continue;

      QoShaderModuleCreateInfo vs = qoShaderModuleCreateInfoGLSL(VERTEX,
         void main() {}
      );

      QoShaderModuleCreateInfo gs = qoShaderModuleCreateInfoGLSL(GEOMETRY,
         layout(points) in;
         layout(points, max_vertices = 0) out;

         void main() {}
      );

      PipelineBuilder pbld(get_vk_device((amd_gfx_level)i));
      pbld.add_stage(VK_SHADER_STAGE_VERTEX_BIT, vs);
      pbld.add_stage(VK_SHADER_STAGE_GEOMETRY_BIT, gs);
      pbld.create_pipeline();

      //! success
      fprintf(output, "success\n");
   }
END_TEST

BEGIN_TEST(isel.sparse.clause)
   for (unsigned i = GFX10_3; i <= GFX10_3; i++) {
      if (!set_variant((amd_gfx_level)i))
         continue;

      QoShaderModuleCreateInfo cs = qoShaderModuleCreateInfoGLSL(COMPUTE,
         QO_EXTENSION GL_ARB_sparse_texture2 : require
         layout(local_size_x=1) in;
         layout(binding=0) uniform sampler2D tex;
         layout(binding=1) buffer Buf {
            vec4 res[4];
            uint code[4];
         };
         void main() {
            //>> v5: (noCSE)%zero0 = p_create_vector 0, 0, 0, 0, 0
            //>> v5: %_ = image_sample_lz_o %_, %_, (kill)%zero0, (kill)%_, %_, %_ dmask:xyzw 2d tfe
            //>> v5: (noCSE)%zero1 = p_create_vector 0, 0, 0, 0, 0
            //>> v5: %_ = image_sample_lz_o %_, %_, (kill)%zero1, (kill)%_, %_, %_ dmask:xyzw 2d tfe
            //>> v5: (noCSE)%zero2 = p_create_vector 0, 0, 0, 0, 0
            //>> v5: %_ = image_sample_lz_o %_, %_, (kill)%zero2, (kill)%_, %_, %_ dmask:xyzw 2d tfe
            //>> v5: (noCSE)%zero3 = p_create_vector 0, 0, 0, 0, 0
            //>> v5: %_ = image_sample_lz_o (kill)%_, (kill)%_, (kill)%zero3, (kill)%_, (kill)%_, (kill)%_ dmask:xyzw 2d tfe
            //>> s_clause 0x3
            //! image_sample_lz_o v[#_:#_], [v#_, v#_, v#_], @s256(img), @s128(samp) dmask:0xf dim:SQ_RSRC_IMG_2D tfe
            //! image_sample_lz_o v[#_:#_], [v#_, v#_, v#_], @s256(img), @s128(samp) dmask:0xf dim:SQ_RSRC_IMG_2D tfe
            //! image_sample_lz_o v[#_:#_], [v#_, v#_, v#_], @s256(img), @s128(samp) dmask:0xf dim:SQ_RSRC_IMG_2D tfe
            //! image_sample_lz_o v[#_:#_], [v#_, v#_, v#_], @s256(img), @s128(samp) dmask:0xf dim:SQ_RSRC_IMG_2D tfe
            code[0] = sparseTextureOffsetARB(tex, vec2(0.5), ivec2(1, 0), res[0]);
            code[1] = sparseTextureOffsetARB(tex, vec2(0.5), ivec2(2, 0), res[1]);
            code[2] = sparseTextureOffsetARB(tex, vec2(0.5), ivec2(3, 0), res[2]);
            code[3] = sparseTextureOffsetARB(tex, vec2(0.5), ivec2(4, 0), res[3]);
         }
      );

      PipelineBuilder pbld(get_vk_device((amd_gfx_level)i));
      pbld.add_cs(cs);
      pbld.print_ir(VK_SHADER_STAGE_COMPUTE_BIT, "ACO IR", true);
      pbld.print_ir(VK_SHADER_STAGE_COMPUTE_BIT, "Assembly", true);
   }
END_TEST

BEGIN_TEST(isel.discard_early_exit.mrtz)
   QoShaderModuleCreateInfo vs = qoShaderModuleCreateInfoGLSL(VERTEX,
      void main() {}
   );
   QoShaderModuleCreateInfo fs = qoShaderModuleCreateInfoGLSL(FRAGMENT,
      void main() {
         if (gl_FragCoord.w > 0.5)
            discard;
         gl_FragDepth = 1.0 / gl_FragCoord.z;
      }
   );

   /* On GFX11, the discard early exit must use mrtz if the shader exports only depth. */
   //>> exp mrtz v0, off, off, off done     ; $_ $_
   //! s_nop 0                              ; $_
   //! s_sendmsg sendmsg(MSG_DEALLOC_VGPRS) ; $_
   //! s_endpgm                             ; $_
   //! BB1:
   //! exp mrtz off, off, off, off done     ; $_ $_
   //! s_nop 0                              ; $_
   //! s_sendmsg sendmsg(MSG_DEALLOC_VGPRS) ; $_
   //! s_endpgm                             ; $_

   PipelineBuilder pbld(get_vk_device(GFX11));
   pbld.add_vsfs(vs, fs);
   pbld.print_ir(VK_SHADER_STAGE_FRAGMENT_BIT, "Assembly");
END_TEST

BEGIN_TEST(isel.discard_early_exit.mrt0)
   QoShaderModuleCreateInfo vs = qoShaderModuleCreateInfoGLSL(VERTEX,
      void main() {}
   );
   QoShaderModuleCreateInfo fs = qoShaderModuleCreateInfoGLSL(FRAGMENT,
      layout(location = 0) out vec4 out_color;
      void main() {
         if (gl_FragCoord.w > 0.5)
            discard;
         out_color = vec4(1.0 / gl_FragCoord.z);
      }
   );

   /* On GFX11, the discard early exit must use mrt0 if the shader exports color. */
   //>> exp mrt0 v0, v0, v0, v0 done        ; $_ $_
   //! s_nop 0                              ; $_
   //! s_sendmsg sendmsg(MSG_DEALLOC_VGPRS) ; $_
   //! s_endpgm                             ; $_
   //! BB1:
   //! exp mrt0 off, off, off, off done     ; $_ $_
   //! s_nop 0                              ; $_
   //! s_sendmsg sendmsg(MSG_DEALLOC_VGPRS) ; $_
   //! s_endpgm                             ; $_

   PipelineBuilder pbld(get_vk_device(GFX11));
   pbld.add_vsfs(vs, fs);
   pbld.print_ir(VK_SHADER_STAGE_FRAGMENT_BIT, "Assembly");
END_TEST

BEGIN_TEST(isel.s_bfe_mask_bits)
   QoShaderModuleCreateInfo cs = qoShaderModuleCreateInfoGLSL(COMPUTE,
      layout(local_size_x=1) in;
      layout(binding=0) buffer Buf {
         int res;
      };
      void main() {
         //>> s1: %bits, s1: (kill)%_:scc = s_and_b32 (kill)%_, 31
         //! s1: %src1 = s_pack_ll_b32_b16 0, (kill)%bits
         //! s1: %_, s1: (kill)%_:scc = s_bfe_i32 0xdeadbeef, (kill)%src1
         res = bitfieldExtract(0xdeadbeef, 0, res & 0x1f);
      }
   );

   PipelineBuilder pbld(get_vk_device(GFX10_3));
   pbld.add_cs(cs);
   pbld.print_ir(VK_SHADER_STAGE_COMPUTE_BIT, "ACO IR", true);
END_TEST

/**
 * loop {
 *   if (uniform) {
 *     break;
 *   } else {
 *     break;
 *   }
 *   // unreachable continue
 * }
 */
BEGIN_TEST(isel.cf.unreachable_continue.uniform_break)
   if (!setup_nir_cs(GFX11))
      return;

   //>> s1: %init0 = p_unit_test 0
   //>> v1: %init1 = p_unit_test 1
   nir_def *init0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
   nir_def *init1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=1);
   nir_phi_instr *phi[2];

   nir_loop *loop = nir_push_loop(nb);
   {
      //>> BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: uniform, loop-header, */
      //! v1: %_ = p_phi %init1
      //! s1: %_ = p_linear_phi %init0
      phi[0] = nir_phi_instr_create(nb->shader);
      phi[1] = nir_phi_instr_create(nb->shader);
      nir_def_init(&phi[0]->instr, &phi[0]->def, 1, 32);
      nir_def_init(&phi[1]->instr, &phi[1]->def, 1, 32);
      nir_phi_instr_add_src(phi[0], init0->parent_instr->block, init0);
      nir_phi_instr_add_src(phi[1], init1->parent_instr->block, init1);

      nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=4));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB3
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, */
         //>> BB4
         //! /* logical preds: BB3, / linear preds: BB3, / kind: uniform, break, */
         //! p_logical_start
         //! s1: %_ = p_unit_test 5
         //! p_logical_end
         nir_unit_test_uniform_amd(nb, 1, 32, .base=5);
         nir_jump(nb, nir_jump_break);
      }
      nir_pop_if(nb, NULL);

      nir_def *cont0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=2);
      nir_def *cont1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=3);

      nir_phi_instr_add_src(phi[0], nir_loop_last_block(loop), cont0);
      nir_phi_instr_add_src(phi[1], nir_loop_last_block(loop), cont1);
   }
   nir_pop_loop(nb, NULL);

   nb->cursor = nir_after_phis(nir_loop_first_block(loop));
   nir_builder_instr_insert(nb, &phi[0]->instr);
   nir_builder_instr_insert(nb, &phi[1]->instr);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (divergent) {
 *     break;
 *   } else {
 *     break;
 *   }
 *   // unreachable continue
 * }
 */
BEGIN_TEST(isel.cf.unreachable_continue.divergent_break)
   if (!setup_nir_cs(GFX11))
      return;

   //>> s1: %init0 = p_unit_test 0
   //>> v1: %init1 = p_unit_test 1
   nir_def *init0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
   nir_def *init1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=1);
   nir_phi_instr *phi[2];

   nir_loop *loop = nir_push_loop(nb);
   {
      //>> BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: loop-header, branch, */
      //! v1: %_ = p_phi %init1
      //! s1: %_ = p_linear_phi %init0
      phi[0] = nir_phi_instr_create(nb->shader);
      phi[1] = nir_phi_instr_create(nb->shader);
      nir_def_init(&phi[0]->instr, &phi[0]->def, 1, 32);
      nir_def_init(&phi[1]->instr, &phi[1]->def, 1, 32);
      nir_phi_instr_add_src(phi[0], init0->parent_instr->block, init0);
      nir_phi_instr_add_src(phi[1], init1->parent_instr->block, init1);

      nir_push_if(nb, nir_unit_test_divergent_amd(nb, 1, 1, .base=4));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB7
         //! /* logical preds: BB1, / linear preds: BB6, / kind: uniform, */
         //>> BB9
         //! /* logical preds: BB7, / linear preds: BB7, BB8, / kind: uniform, break, merge, */
         //! p_logical_start
         //! s1: %_ = p_unit_test 5
         //! p_logical_end
         nir_unit_test_uniform_amd(nb, 1, 32, .base=5);
         nir_jump(nb, nir_jump_break);
      }
      nir_pop_if(nb, NULL);

      nir_def *cont0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=2);
      nir_def *cont1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=3);

      nir_phi_instr_add_src(phi[0], nir_loop_last_block(loop), cont0);
      nir_phi_instr_add_src(phi[1], nir_loop_last_block(loop), cont1);
   }
   nir_pop_loop(nb, NULL);

   nb->cursor = nir_after_phis(nir_loop_first_block(loop));
   nir_builder_instr_insert(nb, &phi[0]->instr);
   nir_builder_instr_insert(nb, &phi[1]->instr);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (uniform) {
 *     continue;
 *   } else {
 *     continue;
 *   }
 *   // unreachable block
 *   break;
 * }
 */
BEGIN_TEST(isel.cf.unreachable_break.uniform_continue)
   if (!setup_nir_cs(GFX11))
      return;

   nir_def *val0;
   nir_def *val1;

   /* These are undefs. */
   //>> s3: %val1 = p_create_vector 0, 0, 0
   //>> s1: %val0 = p_parallelcopy 0

   nir_push_loop(nb);
   {
      //>> BB1
      //! /* logical preds: BB0, BB2, BB7, / linear preds: BB0, BB2, BB7, / kind: uniform, loop-header, */
      nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=2));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, continue, */
         nir_jump(nb, nir_jump_continue);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block, and a dummy break is inserted
          * before the continue so that the loop has an exit.
          */
         //>> BB3
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, */
         //>> BB4
         //! /* logical preds: BB3, / linear preds: BB3, / kind: uniform, */
         //! p_logical_start
         //! s1: %_ = p_unit_test 5
         //! s2: %zero = p_parallelcopy 0
         //! s2: %_, s1: %cond:scc = s_and_b64 %zero, %0:exec
         //! p_logical_end
         //! s2: %_ = p_cbranch_z %cond:scc
         //! BB5
         //! /* logical preds: BB4, / linear preds: BB4, / kind: uniform, break, */
         //>> BB6
         //! /* logical preds: BB4, / linear preds: BB4, / kind: uniform, */
         //>> BB7
         //! /* logical preds: BB6, / linear preds: BB6, / kind: uniform, continue, */
         nir_unit_test_uniform_amd(nb, 1, 32, .base=5);
         nir_jump(nb, nir_jump_continue);
      }
      nir_pop_if(nb, NULL);

      val0 = nir_imm_zero(nb, 1, 32);
      val1 = nir_load_local_invocation_id(nb);

      nir_jump(nb, nir_jump_break);
   }
   nir_pop_loop(nb, NULL);
   //>> BB8
   //! /* logical preds: BB5, / linear preds: BB5, / kind: uniform, top-level, loop-exit, */

   //>> p_unit_test 0, %val0
   //! p_unit_test 1, %val1
   nir_unit_test_amd(nb, val0, .base=0);
   nir_unit_test_amd(nb, val1, .base=1);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (uniform) {
 *     break;
 *   } else {
 *     if (divergent) {
 *       break;
 *     } else {
 *       break;
 *     }
 *   }
 *   // unreachable continue
 * }
 */
BEGIN_TEST(isel.cf.unreachable_continue.mixed_break)
   if (!setup_nir_cs(GFX11))
      return;

   //>> s1: %init0 = p_unit_test 0
   //>> v1: %init1 = p_unit_test 1
   nir_def *init0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
   nir_def *init1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=1);
   nir_phi_instr *phi[2];

   nir_loop *loop = nir_push_loop(nb);
   {
      //>> BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: uniform, loop-header, */
      //! v1: %_ = p_phi %init1
      //! s1: %_ = p_linear_phi %init0
      phi[0] = nir_phi_instr_create(nb->shader);
      phi[1] = nir_phi_instr_create(nb->shader);
      nir_def_init(&phi[0]->instr, &phi[0]->def, 1, 32);
      nir_def_init(&phi[1]->instr, &phi[1]->def, 1, 32);
      nir_phi_instr_add_src(phi[0], init0->parent_instr->block, init0);
      nir_phi_instr_add_src(phi[1], init1->parent_instr->block, init1);

      nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=4));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB3
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, */
         //>> BB4
         //! /* logical preds: BB3, / linear preds: BB3, / kind: branch, */
         //! p_logical_start
         //! s2: %cond = p_unit_test 5
         //! p_logical_end
         //! s2: %_ = p_cbranch_z %cond
         nir_push_if(nb, nir_unit_test_divergent_amd(nb, 1, 1, .base=5));
         {
            //>> BB5
            //! /* logical preds: BB4, / linear preds: BB4, / kind: break, */
            nir_jump(nb, nir_jump_break);
         }
         nir_push_else(nb, NULL);
         {
            /* The contents of this branch is moved to the merge block. */
            //>> BB10
            //! /* logical preds: BB4, / linear preds: BB9, / kind: uniform, */
            //>> BB12
            //! /* logical preds: BB10, / linear preds: BB10, BB11, / kind: uniform, break, merge, */
            //! p_logical_start
            //! s1: %_ = p_unit_test 6
            nir_unit_test_uniform_amd(nb, 1, 32, .base=6);
            nir_jump(nb, nir_jump_break);
         }
         nir_pop_if(nb, NULL);
      }
      nir_pop_if(nb, NULL);

      nir_def *cont0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=2);
      nir_def *cont1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=3);

      nir_phi_instr_add_src(phi[0], nir_loop_last_block(loop), cont0);
      nir_phi_instr_add_src(phi[1], nir_loop_last_block(loop), cont1);
   }
   nir_pop_loop(nb, NULL);
   //>> BB13
   //! /* logical preds: BB2, BB5, BB12, / linear preds: BB2, BB6, BB12, / kind: uniform, top-level, loop-exit, */

   nb->cursor = nir_after_phis(nir_loop_first_block(loop));
   nir_builder_instr_insert(nb, &phi[0]->instr);
   nir_builder_instr_insert(nb, &phi[1]->instr);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (uniform) {
 *     break;
 *   } else {
 *     if (uniform) {
 *       break;
 *     } else {
 *       if (divergent) {
 *         break;
 *       } else {
 *         break;
 *       }
 *     }
 *   }
 *   // unreachable continue
 * }
 */
BEGIN_TEST(isel.cf.unreachable_continue.nested_mixed_break)
   if (!setup_nir_cs(GFX11))
      return;

   //>> s1: %init0 = p_unit_test 0
   //>> v1: %init1 = p_unit_test 1
   nir_def *init0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
   nir_def *init1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=1);
   nir_phi_instr *phi[2];

   nir_loop *loop = nir_push_loop(nb);
   {
      //>> BB1
      //! /* logical preds: BB0, / linear preds: BB0, / kind: uniform, loop-header, */
      //! v1: %_ = p_phi %init1
      //! s1: %_ = p_linear_phi %init0
      phi[0] = nir_phi_instr_create(nb->shader);
      phi[1] = nir_phi_instr_create(nb->shader);
      nir_def_init(&phi[0]->instr, &phi[0]->def, 1, 32);
      nir_def_init(&phi[1]->instr, &phi[1]->def, 1, 32);
      nir_phi_instr_add_src(phi[0], init0->parent_instr->block, init0);
      nir_phi_instr_add_src(phi[1], init1->parent_instr->block, init1);

      nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=4));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB3
         //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, */
         //>> BB4
         //! /* logical preds: BB3, / linear preds: BB3, / kind: uniform, */
         //! p_logical_start
         //! s2: %cond1 = p_unit_test 4
         //! s2: %_,  s1: %_:scc = s_and_b64 %cond1, %0:exec
         //! p_logical_end
         //! s2: %_ = p_cbranch_z %_:scc
         nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=4));
         {
            //>> BB5
            //! /* logical preds: BB4, / linear preds: BB4, / kind: uniform, break, */
            nir_jump(nb, nir_jump_break);
         }
         nir_push_else(nb, NULL);
         {
            /* The contents of this branch is moved to the merge block. */
            //>> BB6
            //! /* logical preds: BB4, / linear preds: BB4, / kind: uniform, */
            //>> BB7
            //! /* logical preds: BB6, / linear preds: BB6, / kind: branch, */
            //! p_logical_start
            //! s2: %cond2 = p_unit_test 5
            //! p_logical_end
            //! s2: %_ = p_cbranch_z %cond2
            nir_push_if(nb, nir_unit_test_divergent_amd(nb, 1, 1, .base=5));
            {
               //>> BB8
               //! /* logical preds: BB7, / linear preds: BB7, / kind: break, */
               nir_jump(nb, nir_jump_break);
            }
            nir_push_else(nb, NULL);
            {
               /* The contents of this branch is moved to the merge block. */
               //>> BB13
               //! /* logical preds: BB7, / linear preds: BB12, / kind: uniform, */
               //>> BB15
               //! /* logical preds: BB13, / linear preds: BB13, BB14, / kind: uniform, break, merge, */
               nir_jump(nb, nir_jump_break);
            }
            nir_pop_if(nb, NULL);
         }
         nir_pop_if(nb, NULL);
      }
      nir_pop_if(nb, NULL);

      nir_def *cont0 = nir_unit_test_uniform_amd(nb, 1, 32, .base=2);
      nir_def *cont1 = nir_unit_test_divergent_amd(nb, 1, 32, .base=3);

      nir_phi_instr_add_src(phi[0], nir_loop_last_block(loop), cont0);
      nir_phi_instr_add_src(phi[1], nir_loop_last_block(loop), cont1);
   }
   nir_pop_loop(nb, NULL);

   nb->cursor = nir_after_phis(nir_loop_first_block(loop));
   nir_builder_instr_insert(nb, &phi[0]->instr);
   nir_builder_instr_insert(nb, &phi[1]->instr);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   continue;
 * }
 */
BEGIN_TEST(isel.cf.unreachable_loop_exit)
   if (!setup_nir_cs(GFX11))
      return;

   nir_push_loop(nb);
   {
      /* A dummy break is inserted before the continue so that the loop has an exit. */
      //>> BB1
      //! /* logical preds: BB0, BB4, / linear preds: BB0, BB4, / kind: uniform, loop-header, */
      //>> s1: %_ = p_unit_test 0
      //>> s2: %zero = p_parallelcopy 0
      //>> s2: %_,  s1: %cond:scc = s_and_b64 %zero, %0:exec
      //>> s2: %_ = p_cbranch_z %cond:scc
      //! BB2
      //! /* logical preds: BB1, / linear preds: BB1, / kind: uniform, break, */
      //>> BB4
      //! /* logical preds: BB3, / linear preds: BB3, / kind: uniform, continue, */
      nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
      nir_jump(nb, nir_jump_continue);
   }
   nir_pop_loop(nb, NULL);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (divergent) {
 *     break;
 *   } else {
 *     val = uniform;
 *   }
 *   use(val);
 * }
 */
BEGIN_TEST(isel.cf.divergent_if_branch_use)
   if (!setup_nir_cs(GFX11))
      return;

   nir_push_loop(nb);
   {
      nir_def *val;
      nir_push_if(nb, nir_unit_test_divergent_amd(nb, 1, 1, .base=2));
      {
         //>> BB2
         //! /* logical preds: BB1, / linear preds: BB1, / kind: break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB9
         //! /* logical preds: BB7, / linear preds: BB7, BB8, / kind: uniform, continue, merge, */
         //! p_logical_start
         //! s1: %val = p_unit_test 0
         val = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
      }
      nir_pop_if(nb, NULL);

      //! p_unit_test 1, %val
      nir_unit_test_amd(nb, val, .base=1);
   }
   nir_pop_loop(nb, NULL);

   finish_isel_test();
END_TEST

/**
 * loop {
 *   if (divergent) {
 *     continue;
 *   }
 *   if (uniform) {
 *     break;
 *   } else {
 *     val = uniform;
 *   }
 *   use(val);
 * }
 */
BEGIN_TEST(isel.cf.uniform_if_branch_use)
   if (!setup_nir_cs(GFX11))
      return;

   nir_push_loop(nb);
   {
      nir_push_if(nb, nir_unit_test_divergent_amd(nb, 1, 1, .base=3));
      {
         nir_jump(nb, nir_jump_continue);
      }
      nir_pop_if(nb, NULL);

      //>> s2: %cond = p_unit_test 2
      //! s2: %_,  s1: %_:scc = s_and_b64 %cond, %0:exec
      //! p_logical_end
      //! s2: %_ = p_cbranch_z %_:scc
      nir_def *val;
      nir_push_if(nb, nir_unit_test_uniform_amd(nb, 1, 1, .base=2));
      {
         //>> BB10
         //! /* logical preds: BB9, / linear preds: BB9, / kind: break, */
         nir_jump(nb, nir_jump_break);
      }
      nir_push_else(nb, NULL);
      {
         /* The contents of this branch is moved to the merge block. */
         //>> BB14
         //! /* logical preds: BB13, / linear preds: BB12, BB13, / kind: uniform, continue, */
         //! p_logical_start
         //! s1: %val = p_unit_test 0
         val = nir_unit_test_uniform_amd(nb, 1, 32, .base=0);
      }
      nir_pop_if(nb, NULL);

      //! p_unit_test 1, %val
      nir_unit_test_amd(nb, val, .base=1);
   }
   nir_pop_loop(nb, NULL);

   finish_isel_test();
END_TEST
