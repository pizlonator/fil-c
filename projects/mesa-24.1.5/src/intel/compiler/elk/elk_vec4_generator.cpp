/* Copyright © 2011 Intel Corporation
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

#include "elk_vec4.h"
#include "elk_cfg.h"
#include "elk_eu.h"
#include "elk_disasm_info.h"
#include "dev/intel_debug.h"
#include "util/mesa-sha1.h"

using namespace elk;

static void
generate_math1_gfx4(struct elk_codegen *p,
                    vec4_instruction *inst,
                    struct elk_reg dst,
                    struct elk_reg src)
{
   elk_gfx4_math(p,
	     dst,
	     elk_math_function(inst->opcode),
	     inst->base_mrf,
	     src,
	     ELK_MATH_PRECISION_FULL);
}

static void
check_gfx6_math_src_arg(struct elk_reg src)
{
   /* Source swizzles are ignored. */
   assert(!src.abs);
   assert(!src.negate);
   assert(src.swizzle == ELK_SWIZZLE_XYZW);
}

static void
generate_math_gfx6(struct elk_codegen *p,
                   vec4_instruction *inst,
                   struct elk_reg dst,
                   struct elk_reg src0,
                   struct elk_reg src1)
{
   /* Can't do writemask because math can't be align16. */
   assert(dst.writemask == WRITEMASK_XYZW);
   /* Source swizzles are ignored. */
   check_gfx6_math_src_arg(src0);
   if (src1.file == ELK_GENERAL_REGISTER_FILE)
      check_gfx6_math_src_arg(src1);

   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_gfx6_math(p, dst, elk_math_function(inst->opcode), src0, src1);
   elk_set_default_access_mode(p, ELK_ALIGN_16);
}

static void
generate_math2_gfx4(struct elk_codegen *p,
                    vec4_instruction *inst,
                    struct elk_reg dst,
                    struct elk_reg src0,
                    struct elk_reg src1)
{
   /* From the Ironlake PRM, Volume 4, Part 1, Section 6.1.13
    * "Message Payload":
    *
    * "Operand0[7].  For the INT DIV functions, this operand is the
    *  denominator."
    *  ...
    * "Operand1[7].  For the INT DIV functions, this operand is the
    *  numerator."
    */
   bool is_int_div = inst->opcode != ELK_SHADER_OPCODE_POW;
   struct elk_reg &op0 = is_int_div ? src1 : src0;
   struct elk_reg &op1 = is_int_div ? src0 : src1;

   elk_push_insn_state(p);
   elk_set_default_saturate(p, false);
   elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
   elk_set_default_flag_reg(p, 0, 0);
   elk_MOV(p, retype(elk_message_reg(inst->base_mrf + 1), op1.type), op1);
   elk_pop_insn_state(p);

   elk_gfx4_math(p,
	     dst,
	     elk_math_function(inst->opcode),
	     inst->base_mrf,
	     op0,
	     ELK_MATH_PRECISION_FULL);
}

static void
generate_tex(struct elk_codegen *p,
             struct elk_vue_prog_data *prog_data,
             gl_shader_stage stage,
             vec4_instruction *inst,
             struct elk_reg dst,
             struct elk_reg src,
             struct elk_reg surface_index,
             struct elk_reg sampler_index)
{
   const struct intel_device_info *devinfo = p->devinfo;
   int msg_type = -1;

   if (devinfo->ver >= 5) {
      switch (inst->opcode) {
      case ELK_SHADER_OPCODE_TEX:
      case ELK_SHADER_OPCODE_TXL:
	 if (inst->shadow_compare) {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE;
	 } else {
	    msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LOD;
	 }
	 break;
      case ELK_SHADER_OPCODE_TXD:
         if (inst->shadow_compare) {
            /* Gfx7.5+.  Otherwise, lowered by elk_lower_texture_gradients(). */
            assert(devinfo->verx10 == 75);
            msg_type = HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE;
         } else {
            msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_DERIVS;
         }
	 break;
      case ELK_SHADER_OPCODE_TXF:
	 msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
	 break;
      case ELK_SHADER_OPCODE_TXF_CMS:
         if (devinfo->ver >= 7)
            msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_LD2DMS;
         else
            msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
         break;
      case ELK_SHADER_OPCODE_TXF_MCS:
         assert(devinfo->ver >= 7);
         msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_LD_MCS;
         break;
      case ELK_SHADER_OPCODE_TXS:
	 msg_type = GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO;
	 break;
      case ELK_SHADER_OPCODE_TG4:
         if (inst->shadow_compare) {
            msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C;
         } else {
            msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4;
         }
         break;
      case ELK_SHADER_OPCODE_TG4_OFFSET:
         if (inst->shadow_compare) {
            msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C;
         } else {
            msg_type = GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO;
         }
         break;
      case ELK_SHADER_OPCODE_SAMPLEINFO:
         msg_type = GFX6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO;
         break;
      default:
	 unreachable("should not get here: invalid vec4 texture opcode");
      }
   } else {
      switch (inst->opcode) {
      case ELK_SHADER_OPCODE_TEX:
      case ELK_SHADER_OPCODE_TXL:
	 if (inst->shadow_compare) {
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD_COMPARE;
	    assert(inst->mlen == 3);
	 } else {
	    msg_type = ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD;
	    assert(inst->mlen == 2);
	 }
	 break;
      case ELK_SHADER_OPCODE_TXD:
	 /* There is no sample_d_c message; comparisons are done manually. */
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_GRADIENTS;
	 assert(inst->mlen == 4);
	 break;
      case ELK_SHADER_OPCODE_TXF:
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD4X2_LD;
	 assert(inst->mlen == 2);
	 break;
      case ELK_SHADER_OPCODE_TXS:
	 msg_type = ELK_SAMPLER_MESSAGE_SIMD4X2_RESINFO;
	 assert(inst->mlen == 2);
	 break;
      default:
	 unreachable("should not get here: invalid vec4 texture opcode");
      }
   }

   assert(msg_type != -1);

   assert(sampler_index.type == ELK_REGISTER_TYPE_UD);

   /* Load the message header if present.  If there's a texture offset, we need
    * to set it up explicitly and load the offset bitfield.  Otherwise, we can
    * use an implied move from g0 to the first message register.
    */
   if (inst->header_size != 0) {
      if (devinfo->ver < 6 && !inst->offset) {
         /* Set up an implied move from g0 to the MRF. */
         src = elk_vec8_grf(0, 0);
      } else {
         struct elk_reg header =
            retype(elk_message_reg(inst->base_mrf), ELK_REGISTER_TYPE_UD);
         uint32_t dw2 = 0;

         /* Explicitly set up the message header by copying g0 to the MRF. */
         elk_push_insn_state(p);
         elk_set_default_mask_control(p, ELK_MASK_DISABLE);
         elk_MOV(p, header, retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         if (inst->offset)
            /* Set the texel offset bits in DWord 2. */
            dw2 = inst->offset;

         /* The VS, DS, and FS stages have the g0.2 payload delivered as 0,
          * so header0.2 is 0 when g0 is copied.  The HS and GS stages do
          * not, so we must set to to 0 to avoid setting undesirable bits
          * in the message header.
          */
         if (dw2 ||
             stage == MESA_SHADER_TESS_CTRL ||
             stage == MESA_SHADER_GEOMETRY) {
            elk_MOV(p, get_element_ud(header, 2), elk_imm_ud(dw2));
         }

         elk_adjust_sampler_state_pointer(p, header, sampler_index);
         elk_pop_insn_state(p);
      }
   }

   uint32_t return_format;

   switch (dst.type) {
   case ELK_REGISTER_TYPE_D:
      return_format = ELK_SAMPLER_RETURN_FORMAT_SINT32;
      break;
   case ELK_REGISTER_TYPE_UD:
      return_format = ELK_SAMPLER_RETURN_FORMAT_UINT32;
      break;
   default:
      return_format = ELK_SAMPLER_RETURN_FORMAT_FLOAT32;
      break;
   }

   /* Stomp the resinfo output type to UINT32.  On gens 4-5, the output type
    * is set as part of the message descriptor.  On gfx4, the PRM seems to
    * allow UINT32 and FLOAT32 (i965 PRM, Vol. 4 Section 4.8.1.1), but on
    * later gens UINT32 is required.  Once you hit Sandy Bridge, the bit is
    * gone from the message descriptor entirely and you just get UINT32 all
    * the time regasrdless.  Since we can really only do non-UINT32 on gfx4,
    * just stomp it to UINT32 all the time.
    */
   if (inst->opcode == ELK_SHADER_OPCODE_TXS)
      return_format = ELK_SAMPLER_RETURN_FORMAT_UINT32;

   if (surface_index.file == ELK_IMMEDIATE_VALUE &&
       sampler_index.file == ELK_IMMEDIATE_VALUE) {
      uint32_t surface = surface_index.ud;
      uint32_t sampler = sampler_index.ud;

      elk_SAMPLE(p,
                 dst,
                 inst->base_mrf,
                 src,
                 surface,
                 sampler % 16,
                 msg_type,
                 1, /* response length */
                 inst->mlen,
                 inst->header_size != 0,
                 ELK_SAMPLER_SIMD_MODE_SIMD4X2,
                 return_format);
   } else {
      /* Non-constant sampler index. */

      struct elk_reg addr = vec1(retype(elk_address_reg(0), ELK_REGISTER_TYPE_UD));
      struct elk_reg surface_reg = vec1(retype(surface_index, ELK_REGISTER_TYPE_UD));
      struct elk_reg sampler_reg = vec1(retype(sampler_index, ELK_REGISTER_TYPE_UD));

      elk_push_insn_state(p);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_access_mode(p, ELK_ALIGN_1);

      if (elk_regs_equal(&surface_reg, &sampler_reg)) {
         elk_MUL(p, addr, sampler_reg, elk_imm_uw(0x101));
      } else {
         if (sampler_reg.file == ELK_IMMEDIATE_VALUE) {
            elk_OR(p, addr, surface_reg, elk_imm_ud(sampler_reg.ud << 8));
         } else {
            elk_SHL(p, addr, sampler_reg, elk_imm_ud(8));
            elk_OR(p, addr, addr, surface_reg);
         }
      }
      elk_AND(p, addr, addr, elk_imm_ud(0xfff));

      elk_pop_insn_state(p);

      if (inst->base_mrf != -1)
         elk_gfx6_resolve_implied_move(p, &src, inst->base_mrf);

      /* dst = send(offset, a0.0 | <descriptor>) */
      elk_send_indirect_message(
         p, ELK_SFID_SAMPLER, dst, src, addr,
         elk_message_desc(devinfo, inst->mlen, 1, inst->header_size) |
         elk_sampler_desc(devinfo,
                          0 /* surface */,
                          0 /* sampler */,
                          msg_type,
                          ELK_SAMPLER_SIMD_MODE_SIMD4X2,
                          return_format),
         false /* EOT */);

      /* visitor knows more than we do about the surface limit required,
       * so has already done marking.
       */
   }
}

static void
generate_vs_urb_write(struct elk_codegen *p, vec4_instruction *inst)
{
   elk_urb_WRITE(p,
		 elk_null_reg(), /* dest */
		 inst->base_mrf, /* starting mrf reg nr */
		 elk_vec8_grf(0, 0), /* src */
                 inst->urb_write_flags,
		 inst->mlen,
		 0,		/* response len */
		 inst->offset,	/* urb destination offset */
		 ELK_URB_SWIZZLE_INTERLEAVE);
}

static void
generate_gs_urb_write(struct elk_codegen *p, vec4_instruction *inst)
{
   struct elk_reg src = elk_message_reg(inst->base_mrf);
   elk_urb_WRITE(p,
                 elk_null_reg(), /* dest */
                 inst->base_mrf, /* starting mrf reg nr */
                 src,
                 inst->urb_write_flags,
                 inst->mlen,
                 0,             /* response len */
                 inst->offset,  /* urb destination offset */
                 ELK_URB_SWIZZLE_INTERLEAVE);
}

static void
generate_gs_urb_write_allocate(struct elk_codegen *p, vec4_instruction *inst)
{
   struct elk_reg src = elk_message_reg(inst->base_mrf);

   /* We pass the temporary passed in src0 as the writeback register */
   elk_urb_WRITE(p,
                 inst->src[0].as_elk_reg(), /* dest */
                 inst->base_mrf, /* starting mrf reg nr */
                 src,
                 ELK_URB_WRITE_ALLOCATE_COMPLETE,
                 inst->mlen,
                 1, /* response len */
                 inst->offset,  /* urb destination offset */
                 ELK_URB_SWIZZLE_INTERLEAVE);

   /* Now put allocated urb handle in dst.0 */
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, get_element_ud(inst->dst.as_elk_reg(), 0),
           get_element_ud(inst->src[0].as_elk_reg(), 0));
   elk_pop_insn_state(p);
}

static void
generate_gs_thread_end(struct elk_codegen *p, vec4_instruction *inst)
{
   struct elk_reg src = elk_message_reg(inst->base_mrf);
   elk_urb_WRITE(p,
                 elk_null_reg(), /* dest */
                 inst->base_mrf, /* starting mrf reg nr */
                 src,
                 ELK_URB_WRITE_EOT | inst->urb_write_flags,
                 inst->mlen,
                 0,              /* response len */
                 0,              /* urb destination offset */
                 ELK_URB_SWIZZLE_INTERLEAVE);
}

static void
generate_gs_set_write_offset(struct elk_codegen *p,
                             struct elk_reg dst,
                             struct elk_reg src0,
                             struct elk_reg src1)
{
   /* From p22 of volume 4 part 2 of the Ivy Bridge PRM (2.4.3.1 Message
    * Header: M0.3):
    *
    *     Slot 0 Offset. This field, after adding to the Global Offset field
    *     in the message descriptor, specifies the offset (in 256-bit units)
    *     from the start of the URB entry, as referenced by URB Handle 0, at
    *     which the data will be accessed.
    *
    * Similar text describes DWORD M0.4, which is slot 1 offset.
    *
    * Therefore, we want to multiply DWORDs 0 and 4 of src0 (the x components
    * of the register for geometry shader invocations 0 and 1) by the
    * immediate value in src1, and store the result in DWORDs 3 and 4 of dst.
    *
    * We can do this with the following EU instruction:
    *
    *     mul(2) dst.3<1>UD src0<8;2,4>UD src1<...>UW   { Align1 WE_all }
    */
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   assert(p->devinfo->ver >= 7 &&
          src1.file == ELK_IMMEDIATE_VALUE &&
          src1.type == ELK_REGISTER_TYPE_UD &&
          src1.ud <= USHRT_MAX);
   if (src0.file == ELK_IMMEDIATE_VALUE) {
      elk_MOV(p, suboffset(stride(dst, 2, 2, 1), 3),
              elk_imm_ud(src0.ud * src1.ud));
   } else {
      if (src1.file == ELK_IMMEDIATE_VALUE) {
         src1 = elk_imm_uw(src1.ud);
      }
      elk_MUL(p, suboffset(stride(dst, 2, 2, 1), 3), stride(src0, 8, 2, 4),
              retype(src1, ELK_REGISTER_TYPE_UW));
   }
   elk_pop_insn_state(p);
}

static void
generate_gs_set_vertex_count(struct elk_codegen *p,
                             struct elk_reg dst,
                             struct elk_reg src)
{
   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   /* If we think of the src and dst registers as composed of 8 DWORDs each,
    * we want to pick up the contents of DWORDs 0 and 4 from src, truncate
    * them to WORDs, and then pack them into DWORD 2 of dst.
    *
    * It's easier to get the EU to do this if we think of the src and dst
    * registers as composed of 16 WORDS each; then, we want to pick up the
    * contents of WORDs 0 and 8 from src, and pack them into WORDs 4 and 5
    * of dst.
    *
    * We can do that by the following EU instruction:
    *
    *     mov (2) dst.4<1>:uw src<8;1,0>:uw   { Align1, Q1, NoMask }
    */
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_MOV(p,
           suboffset(stride(retype(dst, ELK_REGISTER_TYPE_UW), 2, 2, 1), 4),
           stride(retype(src, ELK_REGISTER_TYPE_UW), 8, 1, 0));

   elk_pop_insn_state(p);
}

static void
generate_gs_svb_write(struct elk_codegen *p,
                      vec4_instruction *inst,
                      struct elk_reg dst,
                      struct elk_reg src0,
                      struct elk_reg src1)
{
   int binding = inst->sol_binding;
   bool final_write = inst->sol_final_write;

   elk_push_insn_state(p);
   elk_set_default_exec_size(p, ELK_EXECUTE_4);
   /* Copy Vertex data into M0.x */
   elk_MOV(p, stride(dst, 4, 4, 1),
           stride(retype(src0, ELK_REGISTER_TYPE_UD), 4, 4, 1));
   elk_pop_insn_state(p);

   elk_push_insn_state(p);
   /* Send SVB Write */
   elk_svb_write(p,
                 final_write ? src1 : elk_null_reg(), /* dest == src1 */
                 1, /* msg_reg_nr */
                 dst, /* src0 == previous dst */
                 ELK_GFX6_SOL_BINDING_START + binding, /* binding_table_index */
                 final_write); /* send_commit_msg */

   /* Finally, wait for the write commit to occur so that we can proceed to
    * other things safely.
    *
    * From the Sandybridge PRM, Volume 4, Part 1, Section 3.3:
    *
    *   The write commit does not modify the destination register, but
    *   merely clears the dependency associated with the destination
    *   register. Thus, a simple “mov” instruction using the register as a
    *   source is sufficient to wait for the write commit to occur.
    */
   if (final_write) {
      elk_MOV(p, src1, src1);
   }
   elk_pop_insn_state(p);
}

static void
generate_gs_svb_set_destination_index(struct elk_codegen *p,
                                      vec4_instruction *inst,
                                      struct elk_reg dst,
                                      struct elk_reg src)
{
   int vertex = inst->sol_vertex;
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, get_element_ud(dst, 5), get_element_ud(src, vertex));
   elk_pop_insn_state(p);
}

static void
generate_gs_set_dword_2(struct elk_codegen *p,
                        struct elk_reg dst,
                        struct elk_reg src)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, suboffset(vec1(dst), 2), suboffset(vec1(src), 0));
   elk_pop_insn_state(p);
}

static void
generate_gs_prepare_channel_masks(struct elk_codegen *p,
                                  struct elk_reg dst)
{
   /* We want to left shift just DWORD 4 (the x component belonging to the
    * second geometry shader invocation) by 4 bits.  So generate the
    * instruction:
    *
    *     shl(1) dst.4<1>UD dst.4<0,1,0>UD 4UD { align1 WE_all }
    */
   dst = suboffset(vec1(dst), 4);
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_SHL(p, dst, dst, elk_imm_ud(4));
   elk_pop_insn_state(p);
}

static void
generate_gs_set_channel_masks(struct elk_codegen *p,
                              struct elk_reg dst,
                              struct elk_reg src)
{
   /* From p21 of volume 4 part 2 of the Ivy Bridge PRM (2.4.3.1 Message
    * Header: M0.5):
    *
    *     15 Vertex 1 DATA [3] / Vertex 0 DATA[7] Channel Mask
    *
    *        When Swizzle Control = URB_INTERLEAVED this bit controls Vertex 1
    *        DATA[3], when Swizzle Control = URB_NOSWIZZLE this bit controls
    *        Vertex 0 DATA[7].  This bit is ANDed with the corresponding
    *        channel enable to determine the final channel enable.  For the
    *        URB_READ_OWORD & URB_READ_HWORD messages, when final channel
    *        enable is 1 it indicates that Vertex 1 DATA [3] will be included
    *        in the writeback message.  For the URB_WRITE_OWORD &
    *        URB_WRITE_HWORD messages, when final channel enable is 1 it
    *        indicates that Vertex 1 DATA [3] will be written to the surface.
    *
    *        0: Vertex 1 DATA [3] / Vertex 0 DATA[7] channel not included
    *        1: Vertex DATA [3] / Vertex 0 DATA[7] channel included
    *
    *     14 Vertex 1 DATA [2] Channel Mask
    *     13 Vertex 1 DATA [1] Channel Mask
    *     12 Vertex 1 DATA [0] Channel Mask
    *     11 Vertex 0 DATA [3] Channel Mask
    *     10 Vertex 0 DATA [2] Channel Mask
    *      9 Vertex 0 DATA [1] Channel Mask
    *      8 Vertex 0 DATA [0] Channel Mask
    *
    * (This is from a section of the PRM that is agnostic to the particular
    * type of shader being executed, so "Vertex 0" and "Vertex 1" refer to
    * geometry shader invocations 0 and 1, respectively).  Since we have the
    * enable flags for geometry shader invocation 0 in bits 3:0 of DWORD 0,
    * and the enable flags for geometry shader invocation 1 in bits 7:0 of
    * DWORD 4, we just need to OR them together and store the result in bits
    * 15:8 of DWORD 5.
    *
    * It's easier to get the EU to do this if we think of the src and dst
    * registers as composed of 32 bytes each; then, we want to pick up the
    * contents of bytes 0 and 16 from src, OR them together, and store them in
    * byte 21.
    *
    * We can do that by the following EU instruction:
    *
    *     or(1) dst.21<1>UB src<0,1,0>UB src.16<0,1,0>UB { align1 WE_all }
    *
    * Note: this relies on the source register having zeros in (a) bits 7:4 of
    * DWORD 0 and (b) bits 3:0 of DWORD 4.  We can rely on (b) because the
    * source register was prepared by ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS (which
    * shifts DWORD 4 left by 4 bits), and we can rely on (a) because prior to
    * the execution of ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS, DWORDs 0 and 4 need to
    * contain valid channel mask values (which are in the range 0x0-0xf).
    */
   dst = retype(dst, ELK_REGISTER_TYPE_UB);
   src = retype(src, ELK_REGISTER_TYPE_UB);
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_OR(p, suboffset(vec1(dst), 21), vec1(src), suboffset(vec1(src), 16));
   elk_pop_insn_state(p);
}

static void
generate_gs_get_instance_id(struct elk_codegen *p,
                            struct elk_reg dst)
{
   /* We want to right shift R0.0 & R0.1 by GFX7_GS_PAYLOAD_INSTANCE_ID_SHIFT
    * and store into dst.0 & dst.4. So generate the instruction:
    *
    *     shr(8) dst<1> R0<1,4,0> GFX7_GS_PAYLOAD_INSTANCE_ID_SHIFT { align1 WE_normal 1Q }
    */
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   dst = retype(dst, ELK_REGISTER_TYPE_UD);
   struct elk_reg r0(retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));
   elk_SHR(p, dst, stride(r0, 1, 4, 0),
           elk_imm_ud(GFX7_GS_PAYLOAD_INSTANCE_ID_SHIFT));
   elk_pop_insn_state(p);
}

static void
generate_gs_ff_sync_set_primitives(struct elk_codegen *p,
                                   struct elk_reg dst,
                                   struct elk_reg src0,
                                   struct elk_reg src1,
                                   struct elk_reg src2)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   /* Save src0 data in 16:31 bits of dst.0 */
   elk_AND(p, suboffset(vec1(dst), 0), suboffset(vec1(src0), 0),
           elk_imm_ud(0xffffu));
   elk_SHL(p, suboffset(vec1(dst), 0), suboffset(vec1(dst), 0), elk_imm_ud(16));
   /* Save src1 data in 0:15 bits of dst.0 */
   elk_AND(p, suboffset(vec1(src2), 0), suboffset(vec1(src1), 0),
           elk_imm_ud(0xffffu));
   elk_OR(p, suboffset(vec1(dst), 0),
          suboffset(vec1(dst), 0),
          suboffset(vec1(src2), 0));
   elk_pop_insn_state(p);
}

static void
generate_gs_ff_sync(struct elk_codegen *p,
                    vec4_instruction *inst,
                    struct elk_reg dst,
                    struct elk_reg src0,
                    struct elk_reg src1)
{
   /* This opcode uses an implied MRF register for:
    *  - the header of the ff_sync message. And as such it is expected to be
    *    initialized to r0 before calling here.
    *  - the destination where we will write the allocated URB handle.
    */
   struct elk_reg header =
      retype(elk_message_reg(inst->base_mrf), ELK_REGISTER_TYPE_UD);

   /* Overwrite dword 0 of the header (SO vertices to write) and
    * dword 1 (number of primitives written).
    */
   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_MOV(p, get_element_ud(header, 0), get_element_ud(src1, 0));
   elk_MOV(p, get_element_ud(header, 1), get_element_ud(src0, 0));
   elk_pop_insn_state(p);

   /* Allocate URB handle in dst */
   elk_ff_sync(p,
               dst,
               0,
               header,
               1, /* allocate */
               1, /* response length */
               0 /* eot */);

   /* Now put allocated urb handle in header.0 */
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, get_element_ud(header, 0), get_element_ud(dst, 0));

   /* src1 is not an immediate when we use transform feedback */
   if (src1.file != ELK_IMMEDIATE_VALUE) {
      elk_set_default_exec_size(p, ELK_EXECUTE_4);
      elk_MOV(p, elk_vec4_grf(src1.nr, 0), elk_vec4_grf(dst.nr, 1));
   }

   elk_pop_insn_state(p);
}

static void
generate_gs_set_primitive_id(struct elk_codegen *p, struct elk_reg dst)
{
   /* In gfx6, PrimitiveID is delivered in R0.1 of the payload */
   struct elk_reg src = elk_vec8_grf(0, 0);
   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_MOV(p, get_element_ud(dst, 0), get_element_ud(src, 1));
   elk_pop_insn_state(p);
}

static void
generate_tcs_get_instance_id(struct elk_codegen *p, struct elk_reg dst)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const bool ivb = devinfo->platform == INTEL_PLATFORM_IVB ||
                    devinfo->platform == INTEL_PLATFORM_BYT;

   /* "Instance Count" comes as part of the payload in r0.2 bits 23:17.
    *
    * Since we operate in SIMD4x2 mode, we need run half as many threads
    * as necessary.  So we assign (2i + 1, 2i) as the thread counts.  We
    * shift right by one less to accomplish the multiplication by two.
    */
   dst = retype(dst, ELK_REGISTER_TYPE_UD);
   struct elk_reg r0(retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);

   const int mask = ivb ? INTEL_MASK(22, 16) : INTEL_MASK(23, 17);
   const int shift = ivb ? 16 : 17;

   elk_AND(p, get_element_ud(dst, 0), get_element_ud(r0, 2), elk_imm_ud(mask));
   elk_SHR(p, get_element_ud(dst, 0), get_element_ud(dst, 0),
           elk_imm_ud(shift - 1));
   elk_ADD(p, get_element_ud(dst, 4), get_element_ud(dst, 0), elk_imm_ud(1));

   elk_pop_insn_state(p);
}

static void
generate_tcs_urb_write(struct elk_codegen *p,
                       vec4_instruction *inst,
                       struct elk_reg urb_header)
{
   const struct intel_device_info *devinfo = p->devinfo;

   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_set_dest(p, send, elk_null_reg());
   elk_set_src0(p, send, urb_header);
   elk_set_desc(p, send, elk_message_desc(devinfo, inst->mlen, 0, true));

   elk_inst_set_sfid(devinfo, send, ELK_SFID_URB);
   elk_inst_set_urb_opcode(devinfo, send, ELK_URB_OPCODE_WRITE_OWORD);
   elk_inst_set_urb_global_offset(devinfo, send, inst->offset);
   if (inst->urb_write_flags & ELK_URB_WRITE_EOT) {
      elk_inst_set_eot(devinfo, send, 1);
   } else {
      elk_inst_set_urb_per_slot_offset(devinfo, send, 1);
      elk_inst_set_urb_swizzle_control(devinfo, send, ELK_URB_SWIZZLE_INTERLEAVE);
   }

   /* what happens to swizzles? */
}


static void
generate_tcs_input_urb_offsets(struct elk_codegen *p,
                               struct elk_reg dst,
                               struct elk_reg vertex,
                               struct elk_reg offset)
{
   /* Generates an URB read/write message header for HS/DS operation.
    * Inputs are a vertex index, and a byte offset from the beginning of
    * the vertex. */

   /* If `vertex` is not an immediate, we clobber a0.0 */

   assert(vertex.file == ELK_IMMEDIATE_VALUE || vertex.file == ELK_GENERAL_REGISTER_FILE);
   assert(vertex.type == ELK_REGISTER_TYPE_UD || vertex.type == ELK_REGISTER_TYPE_D);

   assert(dst.file == ELK_GENERAL_REGISTER_FILE);

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, dst, elk_imm_ud(0));

   /* m0.5 bits 8-15 are channel enables */
   elk_MOV(p, get_element_ud(dst, 5), elk_imm_ud(0xff00));

   /* m0.0-0.1: URB handles */
   if (vertex.file == ELK_IMMEDIATE_VALUE) {
      uint32_t vertex_index = vertex.ud;
      struct elk_reg index_reg = elk_vec1_grf(
            1 + (vertex_index >> 3), vertex_index & 7);

      elk_MOV(p, vec2(get_element_ud(dst, 0)),
              retype(index_reg, ELK_REGISTER_TYPE_UD));
   } else {
      /* Use indirect addressing.  ICP Handles are DWords (single channels
       * of a register) and start at g1.0.
       *
       * In order to start our region at g1.0, we add 8 to the vertex index,
       * effectively skipping over the 8 channels in g0.0.  This gives us a
       * DWord offset to the ICP Handle.
       *
       * Indirect addressing works in terms of bytes, so we then multiply
       * the DWord offset by 4 (by shifting left by 2).
       */
      struct elk_reg addr = elk_address_reg(0);

      /* bottom half: m0.0 = g[1.0 + vertex.0]UD */
      elk_ADD(p, addr, retype(get_element_ud(vertex, 0), ELK_REGISTER_TYPE_UW),
              elk_imm_uw(0x8));
      elk_SHL(p, addr, addr, elk_imm_uw(2));
      elk_MOV(p, get_element_ud(dst, 0), deref_1ud(elk_indirect(0, 0), 0));

      /* top half: m0.1 = g[1.0 + vertex.4]UD */
      elk_ADD(p, addr, retype(get_element_ud(vertex, 4), ELK_REGISTER_TYPE_UW),
              elk_imm_uw(0x8));
      elk_SHL(p, addr, addr, elk_imm_uw(2));
      elk_MOV(p, get_element_ud(dst, 1), deref_1ud(elk_indirect(0, 0), 0));
   }

   /* m0.3-0.4: 128bit-granular offsets into the URB from the handles */
   if (offset.file != ARF)
      elk_MOV(p, vec2(get_element_ud(dst, 3)), stride(offset, 4, 1, 0));

   elk_pop_insn_state(p);
}


static void
generate_tcs_output_urb_offsets(struct elk_codegen *p,
                                struct elk_reg dst,
                                struct elk_reg write_mask,
                                struct elk_reg offset)
{
   /* Generates an URB read/write message header for HS/DS operation, for the patch URB entry. */
   assert(dst.file == ELK_GENERAL_REGISTER_FILE || dst.file == ELK_MESSAGE_REGISTER_FILE);

   assert(write_mask.file == ELK_IMMEDIATE_VALUE);
   assert(write_mask.type == ELK_REGISTER_TYPE_UD);

   elk_push_insn_state(p);

   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, dst, elk_imm_ud(0));

   unsigned mask = write_mask.ud;

   /* m0.5 bits 15:12 and 11:8 are channel enables */
   elk_MOV(p, get_element_ud(dst, 5), elk_imm_ud((mask << 8) | (mask << 12)));

   /* HS patch URB handle is delivered in r0.0 */
   struct elk_reg urb_handle = elk_vec1_grf(0, 0);

   /* m0.0-0.1: URB handles */
   elk_MOV(p, vec2(get_element_ud(dst, 0)),
           retype(urb_handle, ELK_REGISTER_TYPE_UD));

   /* m0.3-0.4: 128bit-granular offsets into the URB from the handles */
   if (offset.file != ARF)
      elk_MOV(p, vec2(get_element_ud(dst, 3)), stride(offset, 4, 1, 0));

   elk_pop_insn_state(p);
}

static void
generate_tes_create_input_read_header(struct elk_codegen *p,
                                      struct elk_reg dst)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   /* Initialize the register to 0 */
   elk_MOV(p, dst, elk_imm_ud(0));

   /* Enable all the channels in m0.5 bits 15:8 */
   elk_MOV(p, get_element_ud(dst, 5), elk_imm_ud(0xff00));

   /* Copy g1.3 (the patch URB handle) to m0.0 and m0.1.  For safety,
    * mask out irrelevant "Reserved" bits, as they're not marked MBZ.
    */
   elk_AND(p, vec2(get_element_ud(dst, 0)),
           retype(elk_vec1_grf(1, 3), ELK_REGISTER_TYPE_UD),
           elk_imm_ud(0x1fff));
   elk_pop_insn_state(p);
}

static void
generate_tes_add_indirect_urb_offset(struct elk_codegen *p,
                                     struct elk_reg dst,
                                     struct elk_reg header,
                                     struct elk_reg offset)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   elk_MOV(p, dst, header);

   /* Uniforms will have a stride <0;4,1>, and we need to convert to <0;1,0>.
    * Other values get <4;1,0>.
    */
   struct elk_reg restrided_offset;
   if (offset.vstride == ELK_VERTICAL_STRIDE_0 &&
       offset.width == ELK_WIDTH_4 &&
       offset.hstride == ELK_HORIZONTAL_STRIDE_1) {
      restrided_offset = stride(offset, 0, 1, 0);
   } else {
      restrided_offset = stride(offset, 4, 1, 0);
   }

   /* m0.3-0.4: 128-bit-granular offsets into the URB from the handles */
   elk_MOV(p, vec2(get_element_ud(dst, 3)), restrided_offset);

   elk_pop_insn_state(p);
}

static void
generate_vec4_urb_read(struct elk_codegen *p,
                       vec4_instruction *inst,
                       struct elk_reg dst,
                       struct elk_reg header)
{
   const struct intel_device_info *devinfo = p->devinfo;

   assert(header.file == ELK_GENERAL_REGISTER_FILE);
   assert(header.type == ELK_REGISTER_TYPE_UD);

   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_set_dest(p, send, dst);
   elk_set_src0(p, send, header);

   elk_set_desc(p, send, elk_message_desc(devinfo, 1, 1, true));

   elk_inst_set_sfid(devinfo, send, ELK_SFID_URB);
   elk_inst_set_urb_opcode(devinfo, send, ELK_URB_OPCODE_READ_OWORD);
   elk_inst_set_urb_swizzle_control(devinfo, send, ELK_URB_SWIZZLE_INTERLEAVE);
   elk_inst_set_urb_per_slot_offset(devinfo, send, 1);

   elk_inst_set_urb_global_offset(devinfo, send, inst->offset);
}

static void
generate_tcs_release_input(struct elk_codegen *p,
                           struct elk_reg header,
                           struct elk_reg vertex,
                           struct elk_reg is_unpaired)
{
   const struct intel_device_info *devinfo = p->devinfo;

   assert(vertex.file == ELK_IMMEDIATE_VALUE);
   assert(vertex.type == ELK_REGISTER_TYPE_UD);

   /* m0.0-0.1: URB handles */
   struct elk_reg urb_handles =
      retype(elk_vec2_grf(1 + (vertex.ud >> 3), vertex.ud & 7),
             ELK_REGISTER_TYPE_UD);

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, header, elk_imm_ud(0));
   elk_MOV(p, vec2(get_element_ud(header, 0)), urb_handles);
   elk_pop_insn_state(p);

   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_set_dest(p, send, elk_null_reg());
   elk_set_src0(p, send, header);
   elk_set_desc(p, send, elk_message_desc(devinfo, 1, 0, true));

   elk_inst_set_sfid(devinfo, send, ELK_SFID_URB);
   elk_inst_set_urb_opcode(devinfo, send, ELK_URB_OPCODE_READ_OWORD);
   elk_inst_set_urb_complete(devinfo, send, 1);
   elk_inst_set_urb_swizzle_control(devinfo, send, is_unpaired.ud ?
                                    ELK_URB_SWIZZLE_NONE :
                                    ELK_URB_SWIZZLE_INTERLEAVE);
}

static void
generate_tcs_thread_end(struct elk_codegen *p, vec4_instruction *inst)
{
   struct elk_reg header = elk_message_reg(inst->base_mrf);

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_MOV(p, header, elk_imm_ud(0));
   elk_MOV(p, get_element_ud(header, 5), elk_imm_ud(WRITEMASK_X << 8));
   elk_MOV(p, get_element_ud(header, 0),
           retype(elk_vec1_grf(0, 0), ELK_REGISTER_TYPE_UD));
   elk_MOV(p, elk_message_reg(inst->base_mrf + 1), elk_imm_ud(0u));
   elk_pop_insn_state(p);

   elk_urb_WRITE(p,
                 elk_null_reg(), /* dest */
                 inst->base_mrf, /* starting mrf reg nr */
                 header,
                 ELK_URB_WRITE_EOT | ELK_URB_WRITE_OWORD |
                 ELK_URB_WRITE_USE_CHANNEL_MASKS,
                 inst->mlen,
                 0,              /* response len */
                 0,              /* urb destination offset */
                 0);
}

static void
generate_tes_get_primitive_id(struct elk_codegen *p, struct elk_reg dst)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_MOV(p, dst, retype(elk_vec1_grf(1, 7), ELK_REGISTER_TYPE_D));
   elk_pop_insn_state(p);
}

static void
generate_tcs_get_primitive_id(struct elk_codegen *p, struct elk_reg dst)
{
   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_MOV(p, dst, retype(elk_vec1_grf(0, 1), ELK_REGISTER_TYPE_UD));
   elk_pop_insn_state(p);
}

static void
generate_tcs_create_barrier_header(struct elk_codegen *p,
                                   struct elk_vue_prog_data *prog_data,
                                   struct elk_reg dst)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const bool ivb = devinfo->platform == INTEL_PLATFORM_IVB ||
                    devinfo->platform == INTEL_PLATFORM_BYT;
   struct elk_reg m0_2 = get_element_ud(dst, 2);
   unsigned instances = ((struct elk_tcs_prog_data *) prog_data)->instances;

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   /* Zero the message header */
   elk_MOV(p, retype(dst, ELK_REGISTER_TYPE_UD), elk_imm_ud(0u));

   /* Copy "Barrier ID" from r0.2, bits 16:13 (Gfx7.5+) or 15:12 (Gfx7) */
   elk_AND(p, m0_2,
           retype(elk_vec1_grf(0, 2), ELK_REGISTER_TYPE_UD),
           elk_imm_ud(ivb ? INTEL_MASK(15, 12) : INTEL_MASK(16, 13)));

   /* Shift it up to bits 27:24. */
   elk_SHL(p, m0_2, get_element_ud(dst, 2), elk_imm_ud(ivb ? 12 : 11));

   /* Set the Barrier Count and the enable bit */
   elk_OR(p, m0_2, m0_2, elk_imm_ud(instances << 9 | (1 << 15)));

   elk_pop_insn_state(p);
}

static void
generate_oword_dual_block_offsets(struct elk_codegen *p,
                                  struct elk_reg m1,
                                  struct elk_reg index)
{
   int second_vertex_offset;

   if (p->devinfo->ver >= 6)
      second_vertex_offset = 1;
   else
      second_vertex_offset = 16;

   m1 = retype(m1, ELK_REGISTER_TYPE_D);

   /* Set up M1 (message payload).  Only the block offsets in M1.0 and
    * M1.4 are used, and the rest are ignored.
    */
   struct elk_reg m1_0 = suboffset(vec1(m1), 0);
   struct elk_reg m1_4 = suboffset(vec1(m1), 4);
   struct elk_reg index_0 = suboffset(vec1(index), 0);
   struct elk_reg index_4 = suboffset(vec1(index), 4);

   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_set_default_access_mode(p, ELK_ALIGN_1);

   elk_MOV(p, m1_0, index_0);

   if (index.file == ELK_IMMEDIATE_VALUE) {
      index_4.ud += second_vertex_offset;
      elk_MOV(p, m1_4, index_4);
   } else {
      elk_ADD(p, m1_4, index_4, elk_imm_d(second_vertex_offset));
   }

   elk_pop_insn_state(p);
}

static void
generate_unpack_flags(struct elk_codegen *p,
                      struct elk_reg dst)
{
   elk_push_insn_state(p);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);
   elk_set_default_access_mode(p, ELK_ALIGN_1);

   struct elk_reg flags = elk_flag_reg(0, 0);
   struct elk_reg dst_0 = suboffset(vec1(dst), 0);
   struct elk_reg dst_4 = suboffset(vec1(dst), 4);

   elk_AND(p, dst_0, flags, elk_imm_ud(0x0f));
   elk_AND(p, dst_4, flags, elk_imm_ud(0xf0));
   elk_SHR(p, dst_4, dst_4, elk_imm_ud(4));

   elk_pop_insn_state(p);
}

static void
generate_scratch_read(struct elk_codegen *p,
                      vec4_instruction *inst,
                      struct elk_reg dst,
                      struct elk_reg index)
{
   const struct intel_device_info *devinfo = p->devinfo;
   struct elk_reg header = elk_vec8_grf(0, 0);

   elk_gfx6_resolve_implied_move(p, &header, inst->base_mrf);

   generate_oword_dual_block_offsets(p, elk_message_reg(inst->base_mrf + 1),
				     index);

   uint32_t msg_type;

   if (devinfo->ver >= 6)
      msg_type = GFX6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;
   else if (devinfo->verx10 >= 45)
      msg_type = G45_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;
   else
      msg_type = ELK_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;

   const unsigned target_cache =
      devinfo->ver >= 7 ? GFX7_SFID_DATAPORT_DATA_CACHE :
      devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_RENDER_CACHE :
      ELK_SFID_DATAPORT_READ;

   /* Each of the 8 channel enables is considered for whether each
    * dword is written.
    */
   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_sfid(devinfo, send, target_cache);
   elk_set_dest(p, send, dst);
   elk_set_src0(p, send, header);
   if (devinfo->ver < 6)
      elk_inst_set_cond_modifier(devinfo, send, inst->base_mrf);
   elk_set_desc(p, send,
                elk_message_desc(devinfo, 2, 1, true) |
                elk_dp_read_desc(devinfo,
                                 elk_scratch_surface_idx(p),
                                 ELK_DATAPORT_OWORD_DUAL_BLOCK_1OWORD,
                                 msg_type, ELK_DATAPORT_READ_TARGET_RENDER_CACHE));
}

static void
generate_scratch_write(struct elk_codegen *p,
                       vec4_instruction *inst,
                       struct elk_reg dst,
                       struct elk_reg src,
                       struct elk_reg index)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned target_cache =
      (devinfo->ver >= 7 ? GFX7_SFID_DATAPORT_DATA_CACHE :
       devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_RENDER_CACHE :
       ELK_SFID_DATAPORT_WRITE);
   struct elk_reg header = elk_vec8_grf(0, 0);
   bool write_commit;

   /* If the instruction is predicated, we'll predicate the send, not
    * the header setup.
    */
   elk_push_insn_state(p);
   elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
   elk_set_default_flag_reg(p, 0, 0);

   elk_gfx6_resolve_implied_move(p, &header, inst->base_mrf);

   generate_oword_dual_block_offsets(p, elk_message_reg(inst->base_mrf + 1),
				     index);

   elk_MOV(p,
	   retype(elk_message_reg(inst->base_mrf + 2), ELK_REGISTER_TYPE_D),
	   retype(src, ELK_REGISTER_TYPE_D));

   elk_pop_insn_state(p);

   uint32_t msg_type;

   if (devinfo->ver >= 7)
      msg_type = GFX7_DATAPORT_DC_OWORD_DUAL_BLOCK_WRITE;
   else if (devinfo->ver == 6)
      msg_type = GFX6_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE;
   else
      msg_type = ELK_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE;

   elk_set_default_predicate_control(p, inst->predicate);

   /* Pre-gfx6, we have to specify write commits to ensure ordering
    * between reads and writes within a thread.  Afterwards, that's
    * guaranteed and write commits only matter for inter-thread
    * synchronization.
    */
   if (devinfo->ver >= 6) {
      write_commit = false;
   } else {
      /* The visitor set up our destination register to be g0.  This
       * means that when the next read comes along, we will end up
       * reading from g0 and causing a block on the write commit.  For
       * write-after-read, we are relying on the value of the previous
       * read being used (and thus blocking on completion) before our
       * write is executed.  This means we have to be careful in
       * instruction scheduling to not violate this assumption.
       */
      write_commit = true;
   }

   /* Each of the 8 channel enables is considered for whether each
    * dword is written.
    */
   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_sfid(p->devinfo, send, target_cache);
   elk_set_dest(p, send, dst);
   elk_set_src0(p, send, header);
   if (devinfo->ver < 6)
      elk_inst_set_cond_modifier(p->devinfo, send, inst->base_mrf);
   elk_set_desc(p, send,
                elk_message_desc(devinfo, 3, write_commit, true) |
                elk_dp_write_desc(devinfo,
                                  elk_scratch_surface_idx(p),
                                  ELK_DATAPORT_OWORD_DUAL_BLOCK_1OWORD,
                                  msg_type,
                                  write_commit));
}

static void
generate_pull_constant_load(struct elk_codegen *p,
                            vec4_instruction *inst,
                            struct elk_reg dst,
                            struct elk_reg index,
                            struct elk_reg offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const unsigned target_cache =
      (devinfo->ver >= 6 ? GFX6_SFID_DATAPORT_SAMPLER_CACHE :
       ELK_SFID_DATAPORT_READ);
   assert(index.file == ELK_IMMEDIATE_VALUE &&
	  index.type == ELK_REGISTER_TYPE_UD);
   uint32_t surf_index = index.ud;

   struct elk_reg header = elk_vec8_grf(0, 0);

   elk_gfx6_resolve_implied_move(p, &header, inst->base_mrf);

   if (devinfo->ver >= 6) {
      if (offset.file == ELK_IMMEDIATE_VALUE) {
         elk_MOV(p, retype(elk_message_reg(inst->base_mrf + 1),
                           ELK_REGISTER_TYPE_D),
                 elk_imm_d(offset.ud >> 4));
      } else {
         elk_SHR(p, retype(elk_message_reg(inst->base_mrf + 1),
                           ELK_REGISTER_TYPE_D),
                 offset, elk_imm_d(4));
      }
   } else {
      elk_MOV(p, retype(elk_message_reg(inst->base_mrf + 1),
                        ELK_REGISTER_TYPE_D),
              offset);
   }

   uint32_t msg_type;

   if (devinfo->ver >= 6)
      msg_type = GFX6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;
   else if (devinfo->verx10 >= 45)
      msg_type = G45_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;
   else
      msg_type = ELK_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ;

   /* Each of the 8 channel enables is considered for whether each
    * dword is written.
    */
   elk_inst *send = elk_next_insn(p, ELK_OPCODE_SEND);
   elk_inst_set_sfid(devinfo, send, target_cache);
   elk_set_dest(p, send, dst);
   elk_set_src0(p, send, header);
   if (devinfo->ver < 6)
      elk_inst_set_cond_modifier(p->devinfo, send, inst->base_mrf);
   elk_set_desc(p, send,
                elk_message_desc(devinfo, 2, 1, true) |
                elk_dp_read_desc(devinfo, surf_index,
                                 ELK_DATAPORT_OWORD_DUAL_BLOCK_1OWORD,
                                 msg_type,
                                 ELK_DATAPORT_READ_TARGET_DATA_CACHE));
}

static void
generate_get_buffer_size(struct elk_codegen *p,
                         vec4_instruction *inst,
                         struct elk_reg dst,
                         struct elk_reg src,
                         struct elk_reg surf_index)
{
   assert(p->devinfo->ver >= 7);
   assert(surf_index.type == ELK_REGISTER_TYPE_UD &&
          surf_index.file == ELK_IMMEDIATE_VALUE);

   elk_SAMPLE(p,
              dst,
              inst->base_mrf,
              src,
              surf_index.ud,
              0,
              GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO,
              1, /* response length */
              inst->mlen,
              inst->header_size > 0,
              ELK_SAMPLER_SIMD_MODE_SIMD4X2,
              ELK_SAMPLER_RETURN_FORMAT_SINT32);
}

static void
generate_pull_constant_load_gfx7(struct elk_codegen *p,
                                 vec4_instruction *inst,
                                 struct elk_reg dst,
                                 struct elk_reg surf_index,
                                 struct elk_reg offset)
{
   const struct intel_device_info *devinfo = p->devinfo;
   assert(surf_index.type == ELK_REGISTER_TYPE_UD);

   if (surf_index.file == ELK_IMMEDIATE_VALUE) {

      elk_inst *insn = elk_next_insn(p, ELK_OPCODE_SEND);
      elk_inst_set_sfid(devinfo, insn, ELK_SFID_SAMPLER);
      elk_set_dest(p, insn, dst);
      elk_set_src0(p, insn, offset);
      elk_set_desc(p, insn,
                   elk_message_desc(devinfo, inst->mlen, 1, inst->header_size) |
                   elk_sampler_desc(devinfo, surf_index.ud,
                                    0, /* LD message ignores sampler unit */
                                    GFX5_SAMPLER_MESSAGE_SAMPLE_LD,
                                    ELK_SAMPLER_SIMD_MODE_SIMD4X2, 0));
   } else {

      struct elk_reg addr = vec1(retype(elk_address_reg(0), ELK_REGISTER_TYPE_UD));

      elk_push_insn_state(p);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);
      elk_set_default_access_mode(p, ELK_ALIGN_1);

      /* a0.0 = surf_index & 0xff */
      elk_inst *insn_and = elk_next_insn(p, ELK_OPCODE_AND);
      elk_inst_set_exec_size(devinfo, insn_and, ELK_EXECUTE_1);
      elk_set_dest(p, insn_and, addr);
      elk_set_src0(p, insn_and, vec1(retype(surf_index, ELK_REGISTER_TYPE_UD)));
      elk_set_src1(p, insn_and, elk_imm_ud(0x0ff));

      elk_pop_insn_state(p);

      /* dst = send(offset, a0.0 | <descriptor>) */
      elk_send_indirect_message(
         p, ELK_SFID_SAMPLER, dst, offset, addr,
         elk_message_desc(devinfo, inst->mlen, 1, inst->header_size) |
         elk_sampler_desc(devinfo,
                          0 /* surface */,
                          0 /* sampler */,
                          GFX5_SAMPLER_MESSAGE_SAMPLE_LD,
                          ELK_SAMPLER_SIMD_MODE_SIMD4X2,
                          0),
         false /* EOT */);
   }
}

static void
generate_mov_indirect(struct elk_codegen *p,
                      vec4_instruction *,
                      struct elk_reg dst, struct elk_reg reg,
                      struct elk_reg indirect)
{
   assert(indirect.type == ELK_REGISTER_TYPE_UD);
   assert(p->devinfo->ver >= 6);

   unsigned imm_byte_offset = reg.nr * REG_SIZE + reg.subnr * (REG_SIZE / 2);

   /* This instruction acts in align1 mode */
   assert(dst.writemask == WRITEMASK_XYZW);

   if (indirect.file == ELK_IMMEDIATE_VALUE) {
      imm_byte_offset += indirect.ud;

      reg.nr = imm_byte_offset / REG_SIZE;
      reg.subnr = (imm_byte_offset / (REG_SIZE / 2)) % 2;
      unsigned shift = (imm_byte_offset / 4) % 4;
      reg.swizzle += ELK_SWIZZLE4(shift, shift, shift, shift);

      elk_MOV(p, dst, reg);
   } else {
      elk_push_insn_state(p);
      elk_set_default_access_mode(p, ELK_ALIGN_1);
      elk_set_default_mask_control(p, ELK_MASK_DISABLE);

      struct elk_reg addr = vec8(elk_address_reg(0));

      /* We need to move the indirect value into the address register.  In
       * order to make things make some sense, we want to respect at least the
       * X component of the swizzle.  In order to do that, we need to convert
       * the subnr (probably 0) to an align1 subnr and add in the swizzle.
       */
      assert(elk_is_single_value_swizzle(indirect.swizzle));
      indirect.subnr = (indirect.subnr * 4 + ELK_GET_SWZ(indirect.swizzle, 0));

      /* We then use a region of <8,4,0>:uw to pick off the first 2 bytes of
       * the indirect and splat it out to all four channels of the given half
       * of a0.
       */
      indirect.subnr *= 2;
      indirect = stride(retype(indirect, ELK_REGISTER_TYPE_UW), 8, 4, 0);
      elk_ADD(p, addr, indirect, elk_imm_uw(imm_byte_offset));

      /* Now we need to incorporate the swizzle from the source register */
      if (reg.swizzle != ELK_SWIZZLE_XXXX) {
         uint32_t uv_swiz = ELK_GET_SWZ(reg.swizzle, 0) << 2 |
                            ELK_GET_SWZ(reg.swizzle, 1) << 6 |
                            ELK_GET_SWZ(reg.swizzle, 2) << 10 |
                            ELK_GET_SWZ(reg.swizzle, 3) << 14;
         uv_swiz |= uv_swiz << 16;

         elk_ADD(p, addr, addr, elk_imm_uv(uv_swiz));
      }

      elk_MOV(p, dst, retype(elk_VxH_indirect(0, 0), reg.type));

      elk_pop_insn_state(p);
   }
}

static void
generate_zero_oob_push_regs(struct elk_codegen *p,
                            struct elk_stage_prog_data *prog_data,
                            struct elk_reg scratch,
                            struct elk_reg bit_mask_in)
{
   const uint64_t want_zero = prog_data->zero_push_reg;
   assert(want_zero);

   assert(bit_mask_in.file == ELK_GENERAL_REGISTER_FILE);
   assert(ELK_GET_SWZ(bit_mask_in.swizzle, 1) ==
          ELK_GET_SWZ(bit_mask_in.swizzle, 0) + 1);
   bit_mask_in.subnr += ELK_GET_SWZ(bit_mask_in.swizzle, 0) * 4;
   bit_mask_in.type = ELK_REGISTER_TYPE_W;

   /* Scratch should be 3 registers in the GRF */
   assert(scratch.file == ELK_GENERAL_REGISTER_FILE);
   scratch = vec8(scratch);
   struct elk_reg mask_w16 = retype(scratch, ELK_REGISTER_TYPE_W);
   struct elk_reg mask_d16 = retype(byte_offset(scratch, REG_SIZE),
                                    ELK_REGISTER_TYPE_D);

   elk_push_insn_state(p);
   elk_set_default_access_mode(p, ELK_ALIGN_1);
   elk_set_default_mask_control(p, ELK_MASK_DISABLE);

   for (unsigned i = 0; i < 64; i++) {
      if (i % 16 == 0 && (want_zero & BITFIELD64_RANGE(i, 16))) {
         elk_set_default_exec_size(p, ELK_EXECUTE_8);
         elk_SHL(p, suboffset(mask_w16, 8),
                    vec1(byte_offset(bit_mask_in, i / 8)),
                    elk_imm_v(0x01234567));
         elk_SHL(p, mask_w16, suboffset(mask_w16, 8), elk_imm_w(8));

         elk_set_default_exec_size(p, ELK_EXECUTE_16);
         elk_ASR(p, mask_d16, mask_w16, elk_imm_w(15));
      }

      if (want_zero & BITFIELD64_BIT(i)) {
         unsigned push_start = prog_data->dispatch_grf_start_reg;
         struct elk_reg push_reg =
            retype(elk_vec8_grf(push_start + i, 0), ELK_REGISTER_TYPE_D);

         elk_set_default_exec_size(p, ELK_EXECUTE_8);
         elk_AND(p, push_reg, push_reg, vec1(suboffset(mask_d16, i)));
      }
   }

   elk_pop_insn_state(p);
}

static void
generate_code(struct elk_codegen *p,
              const struct elk_compiler *compiler,
              const struct elk_compile_params *params,
              const nir_shader *nir,
              struct elk_vue_prog_data *prog_data,
              const struct elk_cfg_t *cfg,
              const performance &perf,
              struct elk_compile_stats *stats,
              bool debug_enabled)
{
   const struct intel_device_info *devinfo = p->devinfo;
   const char *stage_abbrev = _mesa_shader_stage_to_abbrev(nir->info.stage);
   struct elk_disasm_info *elk_disasm_info = elk_disasm_initialize(p->isa, cfg);

   /* `send_count` explicitly does not include spills or fills, as we'd
    * like to use it as a metric for intentional memory access or other
    * shared function use.  Otherwise, subtle changes to scheduling or
    * register allocation could cause it to fluctuate wildly - and that
    * effect is already counted in spill/fill counts.
    */
   int spill_count = 0, fill_count = 0;
   int loop_count = 0, send_count = 0;

   foreach_block_and_inst (block, vec4_instruction, inst, cfg) {
      struct elk_reg src[3], dst;

      if (unlikely(debug_enabled))
         elk_disasm_annotate(elk_disasm_info, inst, p->next_insn_offset);

      for (unsigned int i = 0; i < 3; i++) {
         src[i] = inst->src[i].as_elk_reg();
      }
      dst = inst->dst.as_elk_reg();

      elk_set_default_predicate_control(p, inst->predicate);
      elk_set_default_predicate_inverse(p, inst->predicate_inverse);
      elk_set_default_flag_reg(p, inst->flag_subreg / 2, inst->flag_subreg % 2);
      elk_set_default_saturate(p, inst->saturate);
      elk_set_default_mask_control(p, inst->force_writemask_all);
      elk_set_default_acc_write_control(p, inst->writes_accumulator);

      assert(inst->group % inst->exec_size == 0);
      assert(inst->group % 4 == 0);

      /* There are some instructions where the destination is 64-bit
       * but we retype it to a smaller type. In that case, we cannot
       * double the exec_size.
       */
      const bool is_df = (get_exec_type_size(inst) == 8 ||
                          inst->dst.type == ELK_REGISTER_TYPE_DF) &&
                         inst->opcode != ELK_VEC4_OPCODE_PICK_LOW_32BIT &&
                         inst->opcode != ELK_VEC4_OPCODE_PICK_HIGH_32BIT &&
                         inst->opcode != ELK_VEC4_OPCODE_SET_LOW_32BIT &&
                         inst->opcode != ELK_VEC4_OPCODE_SET_HIGH_32BIT;

      unsigned exec_size = inst->exec_size;
      if (devinfo->verx10 == 70 && is_df)
         exec_size *= 2;

      elk_set_default_exec_size(p, cvt(exec_size) - 1);

      if (!inst->force_writemask_all)
         elk_set_default_group(p, inst->group);

      assert(inst->base_mrf + inst->mlen <= ELK_MAX_MRF(devinfo->ver));
      assert(inst->mlen <= ELK_MAX_MSG_LENGTH);

      unsigned pre_emit_nr_insn = p->nr_insn;

      switch (inst->opcode) {
      case ELK_VEC4_OPCODE_UNPACK_UNIFORM:
      case ELK_OPCODE_MOV:
      case ELK_VEC4_OPCODE_MOV_FOR_SCRATCH:
         elk_MOV(p, dst, src[0]);
         break;
      case ELK_OPCODE_ADD:
         elk_ADD(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_MUL:
         elk_MUL(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_MACH:
         elk_MACH(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_MAD:
         assert(devinfo->ver >= 6);
         elk_MAD(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_FRC:
         elk_FRC(p, dst, src[0]);
         break;
      case ELK_OPCODE_RNDD:
         elk_RNDD(p, dst, src[0]);
         break;
      case ELK_OPCODE_RNDE:
         elk_RNDE(p, dst, src[0]);
         break;
      case ELK_OPCODE_RNDZ:
         elk_RNDZ(p, dst, src[0]);
         break;

      case ELK_OPCODE_AND:
         elk_AND(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_OR:
         elk_OR(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_XOR:
         elk_XOR(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_NOT:
         elk_NOT(p, dst, src[0]);
         break;
      case ELK_OPCODE_ASR:
         elk_ASR(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_SHR:
         elk_SHR(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_SHL:
         elk_SHL(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_CMP:
         elk_CMP(p, dst, inst->conditional_mod, src[0], src[1]);
         break;
      case ELK_OPCODE_CMPN:
         elk_CMPN(p, dst, inst->conditional_mod, src[0], src[1]);
         break;
      case ELK_OPCODE_SEL:
         elk_SEL(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_DPH:
         elk_DPH(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_DP4:
         elk_DP4(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_DP3:
         elk_DP3(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_DP2:
         elk_DP2(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_F32TO16:
         assert(devinfo->ver >= 7);
         elk_F32TO16(p, dst, src[0]);
         break;

      case ELK_OPCODE_F16TO32:
         assert(devinfo->ver >= 7);
         elk_F16TO32(p, dst, src[0]);
         break;

      case ELK_OPCODE_LRP:
         assert(devinfo->ver >= 6);
         elk_LRP(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_BFREV:
         assert(devinfo->ver >= 7);
         elk_BFREV(p, retype(dst, ELK_REGISTER_TYPE_UD),
                   retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_FBH:
         assert(devinfo->ver >= 7);
         elk_FBH(p, retype(dst, src[0].type), src[0]);
         break;
      case ELK_OPCODE_FBL:
         assert(devinfo->ver >= 7);
         elk_FBL(p, retype(dst, ELK_REGISTER_TYPE_UD),
                 retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_LZD:
         elk_LZD(p, dst, src[0]);
         break;
      case ELK_OPCODE_CBIT:
         assert(devinfo->ver >= 7);
         elk_CBIT(p, retype(dst, ELK_REGISTER_TYPE_UD),
                  retype(src[0], ELK_REGISTER_TYPE_UD));
         break;
      case ELK_OPCODE_ADDC:
         assert(devinfo->ver >= 7);
         elk_ADDC(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_SUBB:
         assert(devinfo->ver >= 7);
         elk_SUBB(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_MAC:
         elk_MAC(p, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_BFE:
         assert(devinfo->ver >= 7);
         elk_BFE(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_BFI1:
         assert(devinfo->ver >= 7);
         elk_BFI1(p, dst, src[0], src[1]);
         break;
      case ELK_OPCODE_BFI2:
         assert(devinfo->ver >= 7);
         elk_BFI2(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_OPCODE_IF:
         if (!inst->src[0].is_null()) {
            /* The instruction has an embedded compare (only allowed on gfx6) */
            assert(devinfo->ver == 6);
            elk_gfx6_IF(p, inst->conditional_mod, src[0], src[1]);
         } else {
            elk_inst *if_inst = elk_IF(p, ELK_EXECUTE_8);
            elk_inst_set_pred_control(p->devinfo, if_inst, inst->predicate);
         }
         break;

      case ELK_OPCODE_ELSE:
         elk_ELSE(p);
         break;
      case ELK_OPCODE_ENDIF:
         elk_ENDIF(p);
         break;

      case ELK_OPCODE_DO:
         elk_DO(p, ELK_EXECUTE_8);
         break;

      case ELK_OPCODE_BREAK:
         elk_BREAK(p);
         elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
         break;
      case ELK_OPCODE_CONTINUE:
         elk_CONT(p);
         elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
         break;

      case ELK_OPCODE_WHILE:
         elk_WHILE(p);
         loop_count++;
         break;

      case ELK_SHADER_OPCODE_RCP:
      case ELK_SHADER_OPCODE_RSQ:
      case ELK_SHADER_OPCODE_SQRT:
      case ELK_SHADER_OPCODE_EXP2:
      case ELK_SHADER_OPCODE_LOG2:
      case ELK_SHADER_OPCODE_SIN:
      case ELK_SHADER_OPCODE_COS:
         assert(inst->conditional_mod == ELK_CONDITIONAL_NONE);
         if (devinfo->ver >= 7) {
            elk_gfx6_math(p, dst, elk_math_function(inst->opcode), src[0],
                      elk_null_reg());
         } else if (devinfo->ver == 6) {
            generate_math_gfx6(p, inst, dst, src[0], elk_null_reg());
         } else {
            generate_math1_gfx4(p, inst, dst, src[0]);
            send_count++;
         }
         break;

      case ELK_SHADER_OPCODE_POW:
      case ELK_SHADER_OPCODE_INT_QUOTIENT:
      case ELK_SHADER_OPCODE_INT_REMAINDER:
         assert(inst->conditional_mod == ELK_CONDITIONAL_NONE);
         if (devinfo->ver >= 7) {
            elk_gfx6_math(p, dst, elk_math_function(inst->opcode), src[0], src[1]);
         } else if (devinfo->ver == 6) {
            generate_math_gfx6(p, inst, dst, src[0], src[1]);
         } else {
            generate_math2_gfx4(p, inst, dst, src[0], src[1]);
            send_count++;
         }
         break;

      case ELK_SHADER_OPCODE_TEX:
      case ELK_SHADER_OPCODE_TXD:
      case ELK_SHADER_OPCODE_TXF:
      case ELK_SHADER_OPCODE_TXF_CMS:
      case ELK_SHADER_OPCODE_TXF_CMS_W:
      case ELK_SHADER_OPCODE_TXF_MCS:
      case ELK_SHADER_OPCODE_TXL:
      case ELK_SHADER_OPCODE_TXS:
      case ELK_SHADER_OPCODE_TG4:
      case ELK_SHADER_OPCODE_TG4_OFFSET:
      case ELK_SHADER_OPCODE_SAMPLEINFO:
         generate_tex(p, prog_data, nir->info.stage,
                      inst, dst, src[0], src[1], src[2]);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_GET_BUFFER_SIZE:
         generate_get_buffer_size(p, inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_VEC4_VS_OPCODE_URB_WRITE:
         generate_vs_urb_write(p, inst);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_GFX4_SCRATCH_READ:
         generate_scratch_read(p, inst, dst, src[0]);
         fill_count++;
         break;

      case ELK_SHADER_OPCODE_GFX4_SCRATCH_WRITE:
         generate_scratch_write(p, inst, dst, src[0], src[1]);
         spill_count++;
         break;

      case ELK_VS_OPCODE_PULL_CONSTANT_LOAD:
         generate_pull_constant_load(p, inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_VS_OPCODE_PULL_CONSTANT_LOAD_GFX7:
         generate_pull_constant_load_gfx7(p, inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_VEC4_GS_OPCODE_URB_WRITE:
         generate_gs_urb_write(p, inst);
         send_count++;
         break;

      case ELK_VEC4_GS_OPCODE_URB_WRITE_ALLOCATE:
         generate_gs_urb_write_allocate(p, inst);
         send_count++;
         break;

      case ELK_GS_OPCODE_SVB_WRITE:
         generate_gs_svb_write(p, inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_GS_OPCODE_SVB_SET_DST_INDEX:
         generate_gs_svb_set_destination_index(p, inst, dst, src[0]);
         break;

      case ELK_GS_OPCODE_THREAD_END:
         generate_gs_thread_end(p, inst);
         send_count++;
         break;

      case ELK_GS_OPCODE_SET_WRITE_OFFSET:
         generate_gs_set_write_offset(p, dst, src[0], src[1]);
         break;

      case ELK_GS_OPCODE_SET_VERTEX_COUNT:
         generate_gs_set_vertex_count(p, dst, src[0]);
         break;

      case ELK_GS_OPCODE_FF_SYNC:
         generate_gs_ff_sync(p, inst, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_GS_OPCODE_FF_SYNC_SET_PRIMITIVES:
         generate_gs_ff_sync_set_primitives(p, dst, src[0], src[1], src[2]);
         break;

      case ELK_GS_OPCODE_SET_PRIMITIVE_ID:
         generate_gs_set_primitive_id(p, dst);
         break;

      case ELK_GS_OPCODE_SET_DWORD_2:
         generate_gs_set_dword_2(p, dst, src[0]);
         break;

      case ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS:
         generate_gs_prepare_channel_masks(p, dst);
         break;

      case ELK_GS_OPCODE_SET_CHANNEL_MASKS:
         generate_gs_set_channel_masks(p, dst, src[0]);
         break;

      case ELK_GS_OPCODE_GET_INSTANCE_ID:
         generate_gs_get_instance_id(p, dst);
         break;

      case ELK_VEC4_OPCODE_UNTYPED_ATOMIC:
         assert(src[2].file == ELK_IMMEDIATE_VALUE);
         elk_untyped_atomic(p, dst, src[0], src[1], src[2].ud, inst->mlen,
                            !inst->dst.is_null(), inst->header_size);
         send_count++;
         break;

      case ELK_VEC4_OPCODE_UNTYPED_SURFACE_READ:
         assert(!inst->header_size);
         assert(src[2].file == ELK_IMMEDIATE_VALUE);
         elk_untyped_surface_read(p, dst, src[0], src[1], inst->mlen,
                                  src[2].ud);
         send_count++;
         break;

      case ELK_VEC4_OPCODE_UNTYPED_SURFACE_WRITE:
         assert(src[2].file == ELK_IMMEDIATE_VALUE);
         elk_untyped_surface_write(p, src[0], src[1], inst->mlen,
                                   src[2].ud, inst->header_size);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_MEMORY_FENCE:
         elk_memory_fence(p, dst, src[0], ELK_OPCODE_SEND,
                          elk_message_target(inst->sfid),
                          inst->desc,
                          /* commit_enable */ false,
                          /* bti */ 0);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL:
         elk_find_live_channel(p, dst, false);
         break;

      case ELK_SHADER_OPCODE_BROADCAST:
         assert(inst->force_writemask_all);
         elk_broadcast(p, dst, src[0], src[1]);
         break;

      case ELK_VS_OPCODE_UNPACK_FLAGS_SIMD4X2:
         generate_unpack_flags(p, dst);
         break;

      case ELK_VEC4_OPCODE_MOV_BYTES: {
         /* Moves the low byte from each channel, using an Align1 access mode
          * and a <4,1,0> source region.
          */
         assert(src[0].type == ELK_REGISTER_TYPE_UB ||
                src[0].type == ELK_REGISTER_TYPE_B);

         elk_set_default_access_mode(p, ELK_ALIGN_1);
         src[0].vstride = ELK_VERTICAL_STRIDE_4;
         src[0].width = ELK_WIDTH_1;
         src[0].hstride = ELK_HORIZONTAL_STRIDE_0;
         elk_MOV(p, dst, src[0]);
         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_DOUBLE_TO_F32:
      case ELK_VEC4_OPCODE_DOUBLE_TO_D32:
      case ELK_VEC4_OPCODE_DOUBLE_TO_U32: {
         assert(type_sz(src[0].type) == 8);
         assert(type_sz(dst.type) == 8);

         elk_reg_type dst_type;

         switch (inst->opcode) {
         case ELK_VEC4_OPCODE_DOUBLE_TO_F32:
            dst_type = ELK_REGISTER_TYPE_F;
            break;
         case ELK_VEC4_OPCODE_DOUBLE_TO_D32:
            dst_type = ELK_REGISTER_TYPE_D;
            break;
         case ELK_VEC4_OPCODE_DOUBLE_TO_U32:
            dst_type = ELK_REGISTER_TYPE_UD;
            break;
         default:
            unreachable("Not supported conversion");
         }
         dst = retype(dst, dst_type);

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         /* When converting from DF->F, we set destination's stride as 2 as an
          * alignment requirement. But in IVB/BYT, each DF implicitly writes
          * two floats, being the first one the converted value. So we don't
          * need to explicitly set stride 2, but 1.
          */
         struct elk_reg spread_dst;
         if (devinfo->verx10 == 70)
            spread_dst = stride(dst, 8, 4, 1);
         else
            spread_dst = stride(dst, 8, 4, 2);

         elk_MOV(p, spread_dst, src[0]);

         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_TO_DOUBLE: {
         assert(type_sz(src[0].type) == 4);
         assert(type_sz(dst.type) == 8);

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         elk_MOV(p, dst, src[0]);

         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_PICK_LOW_32BIT:
      case ELK_VEC4_OPCODE_PICK_HIGH_32BIT: {
         /* Stores the low/high 32-bit of each 64-bit element in src[0] into
          * dst using ALIGN1 mode and a <8,4,2>:UD region on the source.
          */
         assert(type_sz(src[0].type) == 8);
         assert(type_sz(dst.type) == 4);

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         dst = retype(dst, ELK_REGISTER_TYPE_UD);
         dst.hstride = ELK_HORIZONTAL_STRIDE_1;

         src[0] = retype(src[0], ELK_REGISTER_TYPE_UD);
         if (inst->opcode == ELK_VEC4_OPCODE_PICK_HIGH_32BIT)
            src[0] = suboffset(src[0], 1);
         src[0] = spread(src[0], 2);
         elk_MOV(p, dst, src[0]);

         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_SET_LOW_32BIT:
      case ELK_VEC4_OPCODE_SET_HIGH_32BIT: {
         /* Reads consecutive 32-bit elements from src[0] and writes
          * them to the low/high 32-bit of each 64-bit element in dst.
          */
         assert(type_sz(src[0].type) == 4);
         assert(type_sz(dst.type) == 8);

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         dst = retype(dst, ELK_REGISTER_TYPE_UD);
         if (inst->opcode == ELK_VEC4_OPCODE_SET_HIGH_32BIT)
            dst = suboffset(dst, 1);
         dst.hstride = ELK_HORIZONTAL_STRIDE_2;

         src[0] = retype(src[0], ELK_REGISTER_TYPE_UD);
         elk_MOV(p, dst, src[0]);

         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_PACK_BYTES: {
         /* Is effectively:
          *
          *   mov(8) dst<16,4,1>:UB src<4,1,0>:UB
          *
          * but destinations' only regioning is horizontal stride, so instead we
          * have to use two instructions:
          *
          *   mov(4) dst<1>:UB     src<4,1,0>:UB
          *   mov(4) dst.16<1>:UB  src.16<4,1,0>:UB
          *
          * where they pack the four bytes from the low and high four DW.
          */
         assert(util_is_power_of_two_nonzero(dst.writemask));
         unsigned offset = __builtin_ctz(dst.writemask);

         dst.type = ELK_REGISTER_TYPE_UB;

         elk_set_default_access_mode(p, ELK_ALIGN_1);

         src[0].type = ELK_REGISTER_TYPE_UB;
         src[0].vstride = ELK_VERTICAL_STRIDE_4;
         src[0].width = ELK_WIDTH_1;
         src[0].hstride = ELK_HORIZONTAL_STRIDE_0;
         dst.subnr = offset * 4;
         struct elk_inst *insn = elk_MOV(p, dst, src[0]);
         elk_inst_set_exec_size(p->devinfo, insn, ELK_EXECUTE_4);
         elk_inst_set_no_dd_clear(p->devinfo, insn, true);
         elk_inst_set_no_dd_check(p->devinfo, insn, inst->no_dd_check);

         src[0].subnr = 16;
         dst.subnr = 16 + offset * 4;
         insn = elk_MOV(p, dst, src[0]);
         elk_inst_set_exec_size(p->devinfo, insn, ELK_EXECUTE_4);
         elk_inst_set_no_dd_clear(p->devinfo, insn, inst->no_dd_clear);
         elk_inst_set_no_dd_check(p->devinfo, insn, true);

         elk_set_default_access_mode(p, ELK_ALIGN_16);
         break;
      }

      case ELK_VEC4_OPCODE_ZERO_OOB_PUSH_REGS:
         generate_zero_oob_push_regs(p, &prog_data->base, dst, src[0]);
         break;

      case ELK_VEC4_TCS_OPCODE_URB_WRITE:
         generate_tcs_urb_write(p, inst, src[0]);
         send_count++;
         break;

      case ELK_VEC4_OPCODE_URB_READ:
         generate_vec4_urb_read(p, inst, dst, src[0]);
         send_count++;
         break;

      case ELK_VEC4_TCS_OPCODE_SET_INPUT_URB_OFFSETS:
         generate_tcs_input_urb_offsets(p, dst, src[0], src[1]);
         break;

      case ELK_VEC4_TCS_OPCODE_SET_OUTPUT_URB_OFFSETS:
         generate_tcs_output_urb_offsets(p, dst, src[0], src[1]);
         break;

      case ELK_TCS_OPCODE_GET_INSTANCE_ID:
         generate_tcs_get_instance_id(p, dst);
         break;

      case ELK_TCS_OPCODE_GET_PRIMITIVE_ID:
         generate_tcs_get_primitive_id(p, dst);
         break;

      case ELK_TCS_OPCODE_CREATE_BARRIER_HEADER:
         generate_tcs_create_barrier_header(p, prog_data, dst);
         break;

      case ELK_TES_OPCODE_CREATE_INPUT_READ_HEADER:
         generate_tes_create_input_read_header(p, dst);
         break;

      case ELK_TES_OPCODE_ADD_INDIRECT_URB_OFFSET:
         generate_tes_add_indirect_urb_offset(p, dst, src[0], src[1]);
         break;

      case ELK_TES_OPCODE_GET_PRIMITIVE_ID:
         generate_tes_get_primitive_id(p, dst);
         break;

      case ELK_TCS_OPCODE_SRC0_010_IS_ZERO:
         /* If src_reg had stride like elk_fs_reg, we wouldn't need this. */
         elk_MOV(p, elk_null_reg(), stride(src[0], 0, 1, 0));
         break;

      case ELK_TCS_OPCODE_RELEASE_INPUT:
         generate_tcs_release_input(p, dst, src[0], src[1]);
         send_count++;
         break;

      case ELK_TCS_OPCODE_THREAD_END:
         generate_tcs_thread_end(p, inst);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_BARRIER:
         elk_barrier(p, src[0]);
         elk_WAIT(p);
         send_count++;
         break;

      case ELK_SHADER_OPCODE_MOV_INDIRECT:
         generate_mov_indirect(p, inst, dst, src[0], src[1]);
         break;

      case ELK_OPCODE_DIM:
         assert(devinfo->verx10 == 75);
         assert(src[0].type == ELK_REGISTER_TYPE_DF);
         assert(dst.type == ELK_REGISTER_TYPE_DF);
         elk_DIM(p, dst, retype(src[0], ELK_REGISTER_TYPE_F));
         break;

      case ELK_SHADER_OPCODE_RND_MODE: {
         assert(src[0].file == ELK_IMMEDIATE_VALUE);
         /*
          * Changes the floating point rounding mode updating the control
          * register field defined at cr0.0[5-6] bits.
          */
         enum elk_rnd_mode mode =
            (enum elk_rnd_mode) (src[0].d << ELK_CR0_RND_MODE_SHIFT);
         elk_float_controls_mode(p, mode, ELK_CR0_RND_MODE_MASK);
      }
         break;

      default:
         unreachable("Unsupported opcode");
      }

      if (inst->opcode == ELK_VEC4_OPCODE_PACK_BYTES) {
         /* Handled dependency hints in the generator. */

         assert(!inst->conditional_mod);
      } else if (inst->no_dd_clear || inst->no_dd_check || inst->conditional_mod) {
         assert(p->nr_insn == pre_emit_nr_insn + 1 ||
                !"conditional_mod, no_dd_check, or no_dd_clear set for IR "
                 "emitting more than 1 instruction");

         elk_inst *last = &p->store[pre_emit_nr_insn];

         if (inst->conditional_mod)
            elk_inst_set_cond_modifier(p->devinfo, last, inst->conditional_mod);
         elk_inst_set_no_dd_clear(p->devinfo, last, inst->no_dd_clear);
         elk_inst_set_no_dd_check(p->devinfo, last, inst->no_dd_check);
      }
   }

   elk_set_uip_jip(p, 0);

   /* end of program sentinel */
   elk_disasm_new_inst_group(elk_disasm_info, p->next_insn_offset);

#ifndef NDEBUG
   bool validated =
#else
   if (unlikely(debug_enabled))
#endif
      elk_validate_instructions(&compiler->isa, p->store,
                                0, p->next_insn_offset,
                                elk_disasm_info);

   int before_size = p->next_insn_offset;
   elk_compact_instructions(p, 0, elk_disasm_info);
   int after_size = p->next_insn_offset;

   bool dump_shader_bin = elk_should_dump_shader_bin();
   unsigned char sha1[21];
   char sha1buf[41];

   if (unlikely(debug_enabled || dump_shader_bin)) {
      _mesa_sha1_compute(p->store, p->next_insn_offset, sha1);
      _mesa_sha1_format(sha1buf, sha1);
   }

   if (unlikely(dump_shader_bin))
      elk_dump_shader_bin(p->store, 0, p->next_insn_offset, sha1buf);

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "Native code for %s %s shader %s (src_hash 0x%08x) (sha1 %s):\n",
            nir->info.label ? nir->info.label : "unnamed",
            _mesa_shader_stage_to_string(nir->info.stage), nir->info.name,
            params->source_hash, sha1buf);

      fprintf(stderr, "%s vec4 shader: %d instructions. %d loops. %u cycles. %d:%d "
                     "spills:fills, %u sends. Compacted %d to %d bytes (%.0f%%)\n",
            stage_abbrev, before_size / 16, loop_count, perf.latency,
            spill_count, fill_count, send_count, before_size, after_size,
            100.0f * (before_size - after_size) / before_size);

      /* overriding the shader makes elk_disasm_info invalid */
      if (!elk_try_override_assembly(p, 0, sha1buf)) {
         elk_dump_assembly(p->store, 0, p->next_insn_offset,
                       elk_disasm_info, perf.block_latency);
      } else {
         fprintf(stderr, "Successfully overrode shader with sha1 %s\n\n", sha1buf);
      }
   }
   ralloc_free(elk_disasm_info);
   assert(validated);

   elk_shader_debug_log(compiler, params->log_data,
                        "%s vec4 shader: %d inst, %d loops, %u cycles, "
                        "%d:%d spills:fills, %u sends, "
                        "compacted %d to %d bytes.\n",
                        stage_abbrev, before_size / 16,
                        loop_count, perf.latency, spill_count,
                        fill_count, send_count, before_size, after_size);
   if (stats) {
      stats->dispatch_width = 0;
      stats->max_dispatch_width = 0;
      stats->instructions = before_size / 16;
      stats->sends = send_count;
      stats->loops = loop_count;
      stats->cycles = perf.latency;
      stats->spills = spill_count;
      stats->fills = fill_count;
   }
}

extern "C" const unsigned *
elk_vec4_generate_assembly(const struct elk_compiler *compiler,
                           const struct elk_compile_params *params,
                           const nir_shader *nir,
                           struct elk_vue_prog_data *prog_data,
                           const struct elk_cfg_t *cfg,
                           const performance &perf,
                           bool debug_enabled)
{
   struct elk_codegen *p = rzalloc(params->mem_ctx, struct elk_codegen);
   elk_init_codegen(&compiler->isa, p, params->mem_ctx);
   elk_set_default_access_mode(p, ELK_ALIGN_16);

   generate_code(p, compiler, params,
                 nir, prog_data, cfg, perf,
                 params->stats, debug_enabled);

   assert(prog_data->base.const_data_size == 0);
   if (nir->constant_data_size > 0) {
      prog_data->base.const_data_size = nir->constant_data_size;
      prog_data->base.const_data_offset =
         elk_append_data(p, nir->constant_data, nir->constant_data_size, 32);
   }

   return elk_get_program(p, &prog_data->base.program_size);
}
