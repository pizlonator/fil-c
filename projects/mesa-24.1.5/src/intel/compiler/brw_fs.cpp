/*
 * Copyright © 2010 Intel Corporation
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

/** @file brw_fs.cpp
 *
 * This file drives the GLSL IR -> LIR translation, contains the
 * optimizations on the LIR, and drives the generation of native code
 * from the LIR.
 */

#include "brw_eu.h"
#include "brw_fs.h"
#include "brw_fs_builder.h"
#include "brw_fs_live_variables.h"
#include "brw_nir.h"
#include "brw_cfg.h"
#include "brw_private.h"
#include "intel_nir.h"
#include "shader_enums.h"
#include "dev/intel_debug.h"
#include "dev/intel_wa.h"
#include "compiler/glsl_types.h"
#include "compiler/nir/nir_builder.h"
#include "util/u_math.h"

#include <memory>

using namespace brw;

static void
initialize_sources(fs_inst *inst, const fs_reg src[], uint8_t num_sources);

void
fs_inst::init(enum opcode opcode, uint8_t exec_size, const fs_reg &dst,
              const fs_reg *src, unsigned sources)
{
   memset((void*)this, 0, sizeof(*this));

   initialize_sources(this, src, sources);

   for (unsigned i = 0; i < sources; i++)
      this->src[i] = src[i];

   this->opcode = opcode;
   this->dst = dst;
   this->exec_size = exec_size;

   assert(dst.file != IMM && dst.file != UNIFORM);

   assert(this->exec_size != 0);

   this->conditional_mod = BRW_CONDITIONAL_NONE;

   /* This will be the case for almost all instructions. */
   switch (dst.file) {
   case VGRF:
   case ARF:
   case FIXED_GRF:
   case ATTR:
      this->size_written = dst.component_size(exec_size);
      break;
   case BAD_FILE:
      this->size_written = 0;
      break;
   case IMM:
   case UNIFORM:
      unreachable("Invalid destination register file");
   }

   this->writes_accumulator = false;
}

fs_inst::fs_inst()
{
   init(BRW_OPCODE_NOP, 8, dst, NULL, 0);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_size)
{
   init(opcode, exec_size, reg_undef, NULL, 0);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_size, const fs_reg &dst)
{
   init(opcode, exec_size, dst, NULL, 0);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_size, const fs_reg &dst,
                 const fs_reg &src0)
{
   const fs_reg src[1] = { src0 };
   init(opcode, exec_size, dst, src, 1);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_size, const fs_reg &dst,
                 const fs_reg &src0, const fs_reg &src1)
{
   const fs_reg src[2] = { src0, src1 };
   init(opcode, exec_size, dst, src, 2);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_size, const fs_reg &dst,
                 const fs_reg &src0, const fs_reg &src1, const fs_reg &src2)
{
   const fs_reg src[3] = { src0, src1, src2 };
   init(opcode, exec_size, dst, src, 3);
}

fs_inst::fs_inst(enum opcode opcode, uint8_t exec_width, const fs_reg &dst,
                 const fs_reg src[], unsigned sources)
{
   init(opcode, exec_width, dst, src, sources);
}

fs_inst::fs_inst(const fs_inst &that)
{
   memcpy((void*)this, &that, sizeof(that));
   initialize_sources(this, that.src, that.sources);
}

fs_inst::~fs_inst()
{
   if (this->src != this->builtin_src)
      delete[] this->src;
}

static void
initialize_sources(fs_inst *inst, const fs_reg src[], uint8_t num_sources)
{
   if (num_sources > ARRAY_SIZE(inst->builtin_src))
      inst->src = new fs_reg[num_sources];
   else
      inst->src = inst->builtin_src;

   for (unsigned i = 0; i < num_sources; i++)
      inst->src[i] = src[i];

   inst->sources = num_sources;
}

void
fs_inst::resize_sources(uint8_t num_sources)
{
   if (this->sources == num_sources)
      return;

   fs_reg *old_src = this->src;
   fs_reg *new_src;

   const unsigned builtin_size = ARRAY_SIZE(this->builtin_src);

   if (old_src == this->builtin_src) {
      if (num_sources > builtin_size) {
         new_src = new fs_reg[num_sources];
         for (unsigned i = 0; i < this->sources; i++)
            new_src[i] = old_src[i];

      } else {
         new_src = old_src;
      }
   } else {
      if (num_sources <= builtin_size) {
         new_src = this->builtin_src;
         assert(this->sources > num_sources);
         for (unsigned i = 0; i < num_sources; i++)
            new_src[i] = old_src[i];

      } else if (num_sources < this->sources) {
         new_src = old_src;

      } else {
         new_src = new fs_reg[num_sources];
         for (unsigned i = 0; i < num_sources; i++)
            new_src[i] = old_src[i];
      }

      if (old_src != new_src)
         delete[] old_src;
   }

   this->sources = num_sources;
   this->src = new_src;
}

void
fs_visitor::VARYING_PULL_CONSTANT_LOAD(const fs_builder &bld,
                                       const fs_reg &dst,
                                       const fs_reg &surface,
                                       const fs_reg &surface_handle,
                                       const fs_reg &varying_offset,
                                       uint32_t const_offset,
                                       uint8_t alignment,
                                       unsigned components)
{
   assert(components <= 4);

   /* We have our constant surface use a pitch of 4 bytes, so our index can
    * be any component of a vector, and then we load 4 contiguous
    * components starting from that.  TODO: Support loading fewer than 4.
    */
   fs_reg total_offset = bld.vgrf(BRW_REGISTER_TYPE_UD);
   bld.ADD(total_offset, varying_offset, brw_imm_ud(const_offset));

   /* The pull load message will load a vec4 (16 bytes). If we are loading
    * a double this means we are only loading 2 elements worth of data.
    * We also want to use a 32-bit data type for the dst of the load operation
    * so other parts of the driver don't get confused about the size of the
    * result.
    */
   fs_reg vec4_result = bld.vgrf(BRW_REGISTER_TYPE_F, 4);

   fs_reg srcs[PULL_VARYING_CONSTANT_SRCS];
   srcs[PULL_VARYING_CONSTANT_SRC_SURFACE]        = surface;
   srcs[PULL_VARYING_CONSTANT_SRC_SURFACE_HANDLE] = surface_handle;
   srcs[PULL_VARYING_CONSTANT_SRC_OFFSET]         = total_offset;
   srcs[PULL_VARYING_CONSTANT_SRC_ALIGNMENT]      = brw_imm_ud(alignment);

   fs_inst *inst = bld.emit(FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL,
                            vec4_result, srcs, PULL_VARYING_CONSTANT_SRCS);
   inst->size_written = 4 * vec4_result.component_size(inst->exec_size);

   shuffle_from_32bit_read(bld, dst, vec4_result, 0, components);
}

bool
fs_inst::is_send_from_grf() const
{
   switch (opcode) {
   case SHADER_OPCODE_SEND:
   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
   case SHADER_OPCODE_INTERLOCK:
   case SHADER_OPCODE_MEMORY_FENCE:
   case SHADER_OPCODE_BARRIER:
      return true;
   case FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
      return src[1].file == VGRF;
   default:
      return false;
   }
}

bool
fs_inst::is_control_source(unsigned arg) const
{
   switch (opcode) {
   case FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
      return arg == 0;

   case SHADER_OPCODE_BROADCAST:
   case SHADER_OPCODE_SHUFFLE:
   case SHADER_OPCODE_QUAD_SWIZZLE:
   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return arg == 1;

   case SHADER_OPCODE_MOV_INDIRECT:
   case SHADER_OPCODE_CLUSTER_BROADCAST:
      return arg == 1 || arg == 2;

   case SHADER_OPCODE_SEND:
      return arg == 0 || arg == 1;

   default:
      return false;
   }
}

bool
fs_inst::is_payload(unsigned arg) const
{
   switch (opcode) {
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case SHADER_OPCODE_INTERLOCK:
   case SHADER_OPCODE_MEMORY_FENCE:
   case SHADER_OPCODE_BARRIER:
      return arg == 0;

   case SHADER_OPCODE_SEND:
      return arg == 2 || arg == 3;

   default:
      return false;
   }
}

/**
 * Returns true if this instruction's sources and destinations cannot
 * safely be the same register.
 *
 * In most cases, a register can be written over safely by the same
 * instruction that is its last use.  For a single instruction, the
 * sources are dereferenced before writing of the destination starts
 * (naturally).
 *
 * However, there are a few cases where this can be problematic:
 *
 * - Virtual opcodes that translate to multiple instructions in the
 *   code generator: if src == dst and one instruction writes the
 *   destination before a later instruction reads the source, then
 *   src will have been clobbered.
 *
 * - SIMD16 compressed instructions with certain regioning (see below).
 *
 * The register allocator uses this information to set up conflicts between
 * GRF sources and the destination.
 */
bool
fs_inst::has_source_and_destination_hazard() const
{
   switch (opcode) {
   case FS_OPCODE_PACK_HALF_2x16_SPLIT:
      /* Multiple partial writes to the destination */
      return true;
   case SHADER_OPCODE_SHUFFLE:
      /* This instruction returns an arbitrary channel from the source and
       * gets split into smaller instructions in the generator.  It's possible
       * that one of the instructions will read from a channel corresponding
       * to an earlier instruction.
       */
   case SHADER_OPCODE_SEL_EXEC:
      /* This is implemented as
       *
       * mov(16)      g4<1>D      0D            { align1 WE_all 1H };
       * mov(16)      g4<1>D      g5<8,8,1>D    { align1 1H }
       *
       * Because the source is only read in the second instruction, the first
       * may stomp all over it.
       */
      return true;
   case SHADER_OPCODE_QUAD_SWIZZLE:
      switch (src[1].ud) {
      case BRW_SWIZZLE_XXXX:
      case BRW_SWIZZLE_YYYY:
      case BRW_SWIZZLE_ZZZZ:
      case BRW_SWIZZLE_WWWW:
      case BRW_SWIZZLE_XXZZ:
      case BRW_SWIZZLE_YYWW:
      case BRW_SWIZZLE_XYXY:
      case BRW_SWIZZLE_ZWZW:
         /* These can be implemented as a single Align1 region on all
          * platforms, so there's never a hazard between source and
          * destination.  C.f. fs_generator::generate_quad_swizzle().
          */
         return false;
      default:
         return !is_uniform(src[0]);
      }
   case BRW_OPCODE_DPAS:
      /* This is overly conservative. The actual hazard is more complicated to
       * describe. When the repeat count is N, the single instruction behaves
       * like N instructions with a repeat count of one, but the destination
       * and source registers are incremented (in somewhat complex ways) for
       * each instruction.
       *
       * This means the source and destination register is actually a range of
       * registers. The hazard exists of an earlier iteration would write a
       * register that should be read by a later iteration.
       *
       * There may be some advantage to properly modeling this, but for now,
       * be overly conservative.
       */
      return rcount > 1;
   default:
      /* The SIMD16 compressed instruction
       *
       * add(16)      g4<1>F      g4<8,8,1>F   g6<8,8,1>F
       *
       * is actually decoded in hardware as:
       *
       * add(8)       g4<1>F      g4<8,8,1>F   g6<8,8,1>F
       * add(8)       g5<1>F      g5<8,8,1>F   g7<8,8,1>F
       *
       * Which is safe.  However, if we have uniform accesses
       * happening, we get into trouble:
       *
       * add(8)       g4<1>F      g4<0,1,0>F   g6<8,8,1>F
       * add(8)       g5<1>F      g4<0,1,0>F   g7<8,8,1>F
       *
       * Now our destination for the first instruction overwrote the
       * second instruction's src0, and we get garbage for those 8
       * pixels.  There's a similar issue for the pre-gfx6
       * pixel_x/pixel_y, which are registers of 16-bit values and thus
       * would get stomped by the first decode as well.
       */
      if (exec_size == 16) {
         for (int i = 0; i < sources; i++) {
            if (src[i].file == VGRF && (src[i].stride == 0 ||
                                        src[i].type == BRW_REGISTER_TYPE_UW ||
                                        src[i].type == BRW_REGISTER_TYPE_W ||
                                        src[i].type == BRW_REGISTER_TYPE_UB ||
                                        src[i].type == BRW_REGISTER_TYPE_B)) {
               return true;
            }
         }
      }
      return false;
   }
}

bool
fs_inst::can_do_source_mods(const struct intel_device_info *devinfo) const
{
   if (is_send_from_grf())
      return false;

   /* From Wa_1604601757:
    *
    * "When multiplying a DW and any lower precision integer, source modifier
    *  is not supported."
    */
   if (devinfo->ver >= 12 && (opcode == BRW_OPCODE_MUL ||
                              opcode == BRW_OPCODE_MAD)) {
      const brw_reg_type exec_type = get_exec_type(this);
      const unsigned min_type_sz = opcode == BRW_OPCODE_MAD ?
         MIN2(type_sz(src[1].type), type_sz(src[2].type)) :
         MIN2(type_sz(src[0].type), type_sz(src[1].type));

      if (brw_reg_type_is_integer(exec_type) &&
          type_sz(exec_type) >= 4 &&
          type_sz(exec_type) != min_type_sz)
         return false;
   }

   switch (opcode) {
   case BRW_OPCODE_ADDC:
   case BRW_OPCODE_BFE:
   case BRW_OPCODE_BFI1:
   case BRW_OPCODE_BFI2:
   case BRW_OPCODE_BFREV:
   case BRW_OPCODE_CBIT:
   case BRW_OPCODE_FBH:
   case BRW_OPCODE_FBL:
   case BRW_OPCODE_ROL:
   case BRW_OPCODE_ROR:
   case BRW_OPCODE_SUBB:
   case BRW_OPCODE_DP4A:
   case BRW_OPCODE_DPAS:
   case SHADER_OPCODE_BROADCAST:
   case SHADER_OPCODE_CLUSTER_BROADCAST:
   case SHADER_OPCODE_MOV_INDIRECT:
   case SHADER_OPCODE_SHUFFLE:
   case SHADER_OPCODE_INT_QUOTIENT:
   case SHADER_OPCODE_INT_REMAINDER:
      return false;
   default:
      return true;
   }
}

bool
fs_inst::can_do_cmod() const
{
   switch (opcode) {
   case BRW_OPCODE_ADD:
   case BRW_OPCODE_ADD3:
   case BRW_OPCODE_ADDC:
   case BRW_OPCODE_AND:
   case BRW_OPCODE_ASR:
   case BRW_OPCODE_AVG:
   case BRW_OPCODE_CMP:
   case BRW_OPCODE_CMPN:
   case BRW_OPCODE_DP2:
   case BRW_OPCODE_DP3:
   case BRW_OPCODE_DP4:
   case BRW_OPCODE_DPH:
   case BRW_OPCODE_FRC:
   case BRW_OPCODE_LINE:
   case BRW_OPCODE_LRP:
   case BRW_OPCODE_LZD:
   case BRW_OPCODE_MAC:
   case BRW_OPCODE_MACH:
   case BRW_OPCODE_MAD:
   case BRW_OPCODE_MOV:
   case BRW_OPCODE_MUL:
   case BRW_OPCODE_NOT:
   case BRW_OPCODE_OR:
   case BRW_OPCODE_PLN:
   case BRW_OPCODE_RNDD:
   case BRW_OPCODE_RNDE:
   case BRW_OPCODE_RNDU:
   case BRW_OPCODE_RNDZ:
   case BRW_OPCODE_SAD2:
   case BRW_OPCODE_SADA2:
   case BRW_OPCODE_SHL:
   case BRW_OPCODE_SHR:
   case BRW_OPCODE_SUBB:
   case BRW_OPCODE_XOR:
      break;
   default:
      return false;
   }

   /* The accumulator result appears to get used for the conditional modifier
    * generation.  When negating a UD value, there is a 33rd bit generated for
    * the sign in the accumulator value, so now you can't check, for example,
    * equality with a 32-bit value.  See piglit fs-op-neg-uvec4.
    */
   for (unsigned i = 0; i < sources; i++) {
      if (brw_reg_type_is_unsigned_integer(src[i].type) && src[i].negate)
         return false;
   }

   return true;
}

bool
fs_inst::can_change_types() const
{
   return dst.type == src[0].type &&
          !src[0].abs && !src[0].negate && !saturate && src[0].file != ATTR &&
          (opcode == BRW_OPCODE_MOV ||
           (opcode == SHADER_OPCODE_LOAD_PAYLOAD && sources == 1) ||
           (opcode == BRW_OPCODE_SEL &&
            dst.type == src[1].type &&
            predicate != BRW_PREDICATE_NONE &&
            !src[1].abs && !src[1].negate && src[1].file != ATTR));
}

void
fs_reg::init()
{
   memset((void*)this, 0, sizeof(*this));
   type = BRW_REGISTER_TYPE_UD;
   stride = 1;
}

/** Generic unset register constructor. */
fs_reg::fs_reg()
{
   init();
   this->file = BAD_FILE;
}

fs_reg::fs_reg(struct ::brw_reg reg) :
   brw_reg(reg)
{
   this->offset = 0;
   this->stride = 1;
   if (this->file == IMM &&
       (this->type != BRW_REGISTER_TYPE_V &&
        this->type != BRW_REGISTER_TYPE_UV &&
        this->type != BRW_REGISTER_TYPE_VF)) {
      this->stride = 0;
   }
}

bool
fs_reg::equals(const fs_reg &r) const
{
   return brw_regs_equal(this, &r) &&
          offset == r.offset &&
          stride == r.stride;
}

bool
fs_reg::negative_equals(const fs_reg &r) const
{
   return brw_regs_negative_equal(this, &r) &&
          offset == r.offset &&
          stride == r.stride;
}

bool
fs_reg::is_contiguous() const
{
   switch (file) {
   case ARF:
   case FIXED_GRF:
      return hstride == BRW_HORIZONTAL_STRIDE_1 &&
             vstride == width + hstride;
   case VGRF:
   case ATTR:
      return stride == 1;
   case UNIFORM:
   case IMM:
   case BAD_FILE:
      return true;
   }

   unreachable("Invalid register file");
}

unsigned
fs_reg::component_size(unsigned width) const
{
   if (file == ARF || file == FIXED_GRF) {
      const unsigned w = MIN2(width, 1u << this->width);
      const unsigned h = width >> this->width;
      const unsigned vs = vstride ? 1 << (vstride - 1) : 0;
      const unsigned hs = hstride ? 1 << (hstride - 1) : 0;
      assert(w > 0);
      return ((MAX2(1, h) - 1) * vs + (w - 1) * hs + 1) * type_sz(type);
   } else {
      return MAX2(width * stride, 1) * type_sz(type);
   }
}

void
fs_visitor::vfail(const char *format, va_list va)
{
   char *msg;

   if (failed)
      return;

   failed = true;

   msg = ralloc_vasprintf(mem_ctx, format, va);
   msg = ralloc_asprintf(mem_ctx, "SIMD%d %s compile failed: %s\n",
         dispatch_width, _mesa_shader_stage_to_abbrev(stage), msg);

   this->fail_msg = msg;

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "%s",  msg);
   }
}

void
fs_visitor::fail(const char *format, ...)
{
   va_list va;

   va_start(va, format);
   vfail(format, va);
   va_end(va);
}

/**
 * Mark this program as impossible to compile with dispatch width greater
 * than n.
 *
 * During the SIMD8 compile (which happens first), we can detect and flag
 * things that are unsupported in SIMD16+ mode, so the compiler can skip the
 * SIMD16+ compile altogether.
 *
 * During a compile of dispatch width greater than n (if one happens anyway),
 * this just calls fail().
 */
void
fs_visitor::limit_dispatch_width(unsigned n, const char *msg)
{
   if (dispatch_width > n) {
      fail("%s", msg);
   } else {
      max_dispatch_width = MIN2(max_dispatch_width, n);
      brw_shader_perf_log(compiler, log_data,
                          "Shader dispatch width limited to SIMD%d: %s\n",
                          n, msg);
   }
}

/**
 * Returns true if the instruction has a flag that means it won't
 * update an entire destination register.
 *
 * For example, dead code elimination and live variable analysis want to know
 * when a write to a variable screens off any preceding values that were in
 * it.
 */
bool
fs_inst::is_partial_write() const
{
   if (this->predicate && !this->predicate_trivial &&
       this->opcode != BRW_OPCODE_SEL)
      return true;

   if (this->dst.offset % REG_SIZE != 0)
      return true;

   /* SEND instructions always write whole registers */
   if (this->opcode == SHADER_OPCODE_SEND)
      return false;

   /* Special case UNDEF since a lot of places in the backend do things like this :
    *
    *  fs_builder ubld = bld.exec_all().group(1, 0);
    *  fs_reg tmp = ubld.vgrf(BRW_REGISTER_TYPE_UD);
    *  ubld.UNDEF(tmp); <- partial write, even if the whole register is concerned
    */
   if (this->opcode == SHADER_OPCODE_UNDEF) {
      assert(this->dst.is_contiguous());
      return this->size_written < 32;
   }

   return this->exec_size * type_sz(this->dst.type) < 32 ||
          !this->dst.is_contiguous();
}

unsigned
fs_inst::components_read(unsigned i) const
{
   /* Return zero if the source is not present. */
   if (src[i].file == BAD_FILE)
      return 0;

   switch (opcode) {
   case BRW_OPCODE_PLN:
      return i == 0 ? 1 : 2;

   case FS_OPCODE_PIXEL_X:
   case FS_OPCODE_PIXEL_Y:
      assert(i < 2);
      if (i == 0)
         return 2;
      else
         return 1;

   case FS_OPCODE_FB_WRITE_LOGICAL:
      assert(src[FB_WRITE_LOGICAL_SRC_COMPONENTS].file == IMM);
      /* First/second FB write color. */
      if (i < 2)
         return src[FB_WRITE_LOGICAL_SRC_COMPONENTS].ud;
      else
         return 1;

   case SHADER_OPCODE_TEX_LOGICAL:
   case SHADER_OPCODE_TXD_LOGICAL:
   case SHADER_OPCODE_TXF_LOGICAL:
   case SHADER_OPCODE_TXL_LOGICAL:
   case SHADER_OPCODE_TXS_LOGICAL:
   case SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
   case FS_OPCODE_TXB_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
   case SHADER_OPCODE_TXF_MCS_LOGICAL:
   case SHADER_OPCODE_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
   case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
      assert(src[TEX_LOGICAL_SRC_COORD_COMPONENTS].file == IMM &&
             src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].file == IMM &&
             src[TEX_LOGICAL_SRC_RESIDENCY].file == IMM);
      /* Texture coordinates. */
      if (i == TEX_LOGICAL_SRC_COORDINATE)
         return src[TEX_LOGICAL_SRC_COORD_COMPONENTS].ud;
      /* Texture derivatives. */
      else if ((i == TEX_LOGICAL_SRC_LOD || i == TEX_LOGICAL_SRC_LOD2) &&
               opcode == SHADER_OPCODE_TXD_LOGICAL)
         return src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].ud;
      /* Texture offset. */
      else if (i == TEX_LOGICAL_SRC_TG4_OFFSET)
         return 2;
      /* MCS */
      else if (i == TEX_LOGICAL_SRC_MCS) {
         if (opcode == SHADER_OPCODE_TXF_CMS_W_LOGICAL)
            return 2;
         else if (opcode == SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL)
            return 4;
         else
            return 1;
      } else
         return 1;

   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM);
      /* Surface coordinates. */
      if (i == SURFACE_LOGICAL_SRC_ADDRESS)
         return src[SURFACE_LOGICAL_SRC_IMM_DIMS].ud;
      /* Surface operation source (ignored for reads). */
      else if (i == SURFACE_LOGICAL_SRC_DATA)
         return 0;
      else
         return 1;

   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM &&
             src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      /* Surface coordinates. */
      if (i == SURFACE_LOGICAL_SRC_ADDRESS)
         return src[SURFACE_LOGICAL_SRC_IMM_DIMS].ud;
      /* Surface operation source. */
      else if (i == SURFACE_LOGICAL_SRC_DATA)
         return src[SURFACE_LOGICAL_SRC_IMM_ARG].ud;
      else
         return 1;

   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
   case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
   case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return 1;

   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      if (i == A64_LOGICAL_SRC) { /* data to write */
         const unsigned comps = src[A64_LOGICAL_ARG].ud / exec_size;
         assert(comps > 0);
         return comps;
      } else {
         return 1;
      }

   case SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      return 1;

   case SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      if (i == SURFACE_LOGICAL_SRC_DATA) {
         const unsigned comps = src[SURFACE_LOGICAL_SRC_IMM_ARG].ud / exec_size;
         assert(comps > 0);
         return comps;
      } else {
         return 1;
      }

   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return i == A64_LOGICAL_SRC ? src[A64_LOGICAL_ARG].ud : 1;

   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return i == A64_LOGICAL_SRC ?
             lsc_op_num_data_values(src[A64_LOGICAL_ARG].ud) : 1;

   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      /* Scattered logical opcodes use the following params:
       * src[0] Surface coordinates
       * src[1] Surface operation source (ignored for reads)
       * src[2] Surface
       * src[3] IMM with always 1 dimension.
       * src[4] IMM with arg bitsize for scattered read/write 8, 16, 32
       */
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM &&
             src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      return i == SURFACE_LOGICAL_SRC_DATA ? 0 : 1;

   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM &&
             src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      return 1;

   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL: {
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM &&
             src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      const unsigned op = src[SURFACE_LOGICAL_SRC_IMM_ARG].ud;
      /* Surface coordinates. */
      if (i == SURFACE_LOGICAL_SRC_ADDRESS)
         return src[SURFACE_LOGICAL_SRC_IMM_DIMS].ud;
      /* Surface operation source. */
      else if (i == SURFACE_LOGICAL_SRC_DATA)
         return lsc_op_num_data_values(op);
      else
         return 1;
   }
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return (i == 0 ? 2 : 1);

   case SHADER_OPCODE_URB_WRITE_LOGICAL:
      assert(src[URB_LOGICAL_SRC_COMPONENTS].file == IMM);

      if (i == URB_LOGICAL_SRC_DATA)
         return src[URB_LOGICAL_SRC_COMPONENTS].ud;
      else
         return 1;

   case BRW_OPCODE_DPAS:
      unreachable("Do not use components_read() for DPAS.");

   default:
      return 1;
   }
}

unsigned
fs_inst::size_read(int arg) const
{
   switch (opcode) {
   case SHADER_OPCODE_SEND:
      if (arg == 2) {
         return mlen * REG_SIZE;
      } else if (arg == 3) {
         return ex_mlen * REG_SIZE;
      }
      break;

   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      if (arg == 0)
         return mlen * REG_SIZE;
      break;

   case BRW_OPCODE_PLN:
      if (arg == 0)
         return 16;
      break;

   case SHADER_OPCODE_LOAD_PAYLOAD:
      if (arg < this->header_size)
         return retype(src[arg], BRW_REGISTER_TYPE_UD).component_size(8);
      break;

   case SHADER_OPCODE_BARRIER:
      return REG_SIZE;

   case SHADER_OPCODE_MOV_INDIRECT:
      if (arg == 0) {
         assert(src[2].file == IMM);
         return src[2].ud;
      }
      break;

   case BRW_OPCODE_DPAS:
      switch (arg) {
      case 0:
         if (src[0].type == BRW_REGISTER_TYPE_HF) {
            return rcount * REG_SIZE / 2;
         } else {
            return rcount * REG_SIZE;
         }
      case 1:
         return sdepth * REG_SIZE;
      case 2:
         /* This is simpler than the formula described in the Bspec, but it
          * covers all of the cases that we support on DG2.
          */
         return rcount * REG_SIZE;
      default:
         unreachable("Invalid source number.");
      }
      break;

   default:
      break;
   }

   switch (src[arg].file) {
   case UNIFORM:
   case IMM:
      return components_read(arg) * type_sz(src[arg].type);
   case BAD_FILE:
   case ARF:
   case FIXED_GRF:
   case VGRF:
   case ATTR:
      return components_read(arg) * src[arg].component_size(exec_size);
   }
   return 0;
}

namespace {
   unsigned
   predicate_width(const intel_device_info *devinfo, brw_predicate predicate)
   {
      if (devinfo->ver >= 20) {
         return 1;
      } else {
         switch (predicate) {
         case BRW_PREDICATE_NONE:            return 1;
         case BRW_PREDICATE_NORMAL:          return 1;
         case BRW_PREDICATE_ALIGN1_ANY2H:    return 2;
         case BRW_PREDICATE_ALIGN1_ALL2H:    return 2;
         case BRW_PREDICATE_ALIGN1_ANY4H:    return 4;
         case BRW_PREDICATE_ALIGN1_ALL4H:    return 4;
         case BRW_PREDICATE_ALIGN1_ANY8H:    return 8;
         case BRW_PREDICATE_ALIGN1_ALL8H:    return 8;
         case BRW_PREDICATE_ALIGN1_ANY16H:   return 16;
         case BRW_PREDICATE_ALIGN1_ALL16H:   return 16;
         case BRW_PREDICATE_ALIGN1_ANY32H:   return 32;
         case BRW_PREDICATE_ALIGN1_ALL32H:   return 32;
         default: unreachable("Unsupported predicate");
         }
      }
   }
}

unsigned
fs_inst::flags_read(const intel_device_info *devinfo) const
{
   if (devinfo->ver < 20 && (predicate == BRW_PREDICATE_ALIGN1_ANYV ||
                             predicate == BRW_PREDICATE_ALIGN1_ALLV)) {
      /* The vertical predication modes combine corresponding bits from
       * f0.0 and f1.0 on Gfx7+.
       */
      const unsigned shift = 4;
      return brw_fs_flag_mask(this, 1) << shift | brw_fs_flag_mask(this, 1);
   } else if (predicate) {
      return brw_fs_flag_mask(this, predicate_width(devinfo, predicate));
   } else {
      unsigned mask = 0;
      for (int i = 0; i < sources; i++) {
         mask |= brw_fs_flag_mask(src[i], size_read(i));
      }
      return mask;
   }
}

unsigned
fs_inst::flags_written(const intel_device_info *devinfo) const
{
   if (conditional_mod && (opcode != BRW_OPCODE_SEL &&
                           opcode != BRW_OPCODE_CSEL &&
                           opcode != BRW_OPCODE_IF &&
                           opcode != BRW_OPCODE_WHILE)) {
      return brw_fs_flag_mask(this, 1);
   } else if (opcode == FS_OPCODE_LOAD_LIVE_CHANNELS) {
      return brw_fs_flag_mask(this, 32);
   } else {
      return brw_fs_flag_mask(dst, size_written);
   }
}

bool
fs_inst::has_sampler_residency() const
{
   switch (opcode) {
   case SHADER_OPCODE_TEX_LOGICAL:
   case FS_OPCODE_TXB_LOGICAL:
   case SHADER_OPCODE_TXL_LOGICAL:
   case SHADER_OPCODE_TXD_LOGICAL:
   case SHADER_OPCODE_TXF_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case SHADER_OPCODE_TXS_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case SHADER_OPCODE_TG4_LOGICAL:
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
      assert(src[TEX_LOGICAL_SRC_RESIDENCY].file == IMM);
      return src[TEX_LOGICAL_SRC_RESIDENCY].ud != 0;
   default:
      return false;
   }
}

fs_reg::fs_reg(enum brw_reg_file file, unsigned nr)
{
   init();
   this->file = file;
   this->nr = nr;
   this->type = BRW_REGISTER_TYPE_F;
   this->stride = (file == UNIFORM ? 0 : 1);
}

fs_reg::fs_reg(enum brw_reg_file file, unsigned nr, enum brw_reg_type type)
{
   init();
   this->file = file;
   this->nr = nr;
   this->type = type;
   this->stride = (file == UNIFORM ? 0 : 1);
}

/* For SIMD16, we need to follow from the uniform setup of SIMD8 dispatch.
 * This brings in those uniform definitions
 */
void
fs_visitor::import_uniforms(fs_visitor *v)
{
   this->push_constant_loc = v->push_constant_loc;
   this->uniforms = v->uniforms;
}

enum brw_barycentric_mode
brw_barycentric_mode(const struct brw_wm_prog_key *key,
                     nir_intrinsic_instr *intr)
{
   const glsl_interp_mode mode =
      (enum glsl_interp_mode) nir_intrinsic_interp_mode(intr);

   /* Barycentric modes don't make sense for flat inputs. */
   assert(mode != INTERP_MODE_FLAT);

   unsigned bary;
   switch (intr->intrinsic) {
   case nir_intrinsic_load_barycentric_pixel:
   case nir_intrinsic_load_barycentric_at_offset:
      /* When per sample interpolation is dynamic, assume sample
       * interpolation. We'll dynamically remap things so that the FS thread
       * payload is not affected.
       */
      bary = key->persample_interp == BRW_SOMETIMES ?
             BRW_BARYCENTRIC_PERSPECTIVE_SAMPLE :
             BRW_BARYCENTRIC_PERSPECTIVE_PIXEL;
      break;
   case nir_intrinsic_load_barycentric_centroid:
      bary = BRW_BARYCENTRIC_PERSPECTIVE_CENTROID;
      break;
   case nir_intrinsic_load_barycentric_sample:
   case nir_intrinsic_load_barycentric_at_sample:
      bary = BRW_BARYCENTRIC_PERSPECTIVE_SAMPLE;
      break;
   default:
      unreachable("invalid intrinsic");
   }

   if (mode == INTERP_MODE_NOPERSPECTIVE)
      bary += 3;

   return (enum brw_barycentric_mode) bary;
}

/**
 * Turn one of the two CENTROID barycentric modes into PIXEL mode.
 */
static enum brw_barycentric_mode
centroid_to_pixel(enum brw_barycentric_mode bary)
{
   assert(bary == BRW_BARYCENTRIC_PERSPECTIVE_CENTROID ||
          bary == BRW_BARYCENTRIC_NONPERSPECTIVE_CENTROID);
   return (enum brw_barycentric_mode) ((unsigned) bary - 1);
}

/**
 * Walk backwards from the end of the program looking for a URB write that
 * isn't in control flow, and mark it with EOT.
 *
 * Return true if successful or false if a separate EOT write is needed.
 */
bool
fs_visitor::mark_last_urb_write_with_eot()
{
   foreach_in_list_reverse(fs_inst, prev, &this->instructions) {
      if (prev->opcode == SHADER_OPCODE_URB_WRITE_LOGICAL) {
         prev->eot = true;

         /* Delete now dead instructions. */
         foreach_in_list_reverse_safe(exec_node, dead, &this->instructions) {
            if (dead == prev)
               break;
            dead->remove();
         }
         return true;
      } else if (prev->is_control_flow() || prev->has_side_effects()) {
         break;
      }
   }

   return false;
}

void
fs_visitor::emit_gs_thread_end()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   struct brw_gs_prog_data *gs_prog_data = brw_gs_prog_data(prog_data);

   if (gs_compile->control_data_header_size_bits > 0) {
      emit_gs_control_data_bits(this->final_gs_vertex_count);
   }

   const fs_builder abld = fs_builder(this).at_end().annotate("thread end");
   fs_inst *inst;

   if (gs_prog_data->static_vertex_count != -1) {
      /* Try and tag the last URB write with EOT instead of emitting a whole
       * separate write just to finish the thread.
       */
      if (mark_last_urb_write_with_eot())
         return;

      fs_reg srcs[URB_LOGICAL_NUM_SRCS];
      srcs[URB_LOGICAL_SRC_HANDLE] = gs_payload().urb_handles;
      srcs[URB_LOGICAL_SRC_COMPONENTS] = brw_imm_ud(0);
      inst = abld.emit(SHADER_OPCODE_URB_WRITE_LOGICAL, reg_undef,
                       srcs, ARRAY_SIZE(srcs));
   } else {
      fs_reg srcs[URB_LOGICAL_NUM_SRCS];
      srcs[URB_LOGICAL_SRC_HANDLE] = gs_payload().urb_handles;
      srcs[URB_LOGICAL_SRC_DATA] = this->final_gs_vertex_count;
      srcs[URB_LOGICAL_SRC_COMPONENTS] = brw_imm_ud(1);
      inst = abld.emit(SHADER_OPCODE_URB_WRITE_LOGICAL, reg_undef,
                       srcs, ARRAY_SIZE(srcs));
   }
   inst->eot = true;
   inst->offset = 0;
}

static unsigned
round_components_to_whole_registers(const intel_device_info *devinfo,
                                    unsigned c)
{
   return DIV_ROUND_UP(c, 8 * reg_unit(devinfo)) * reg_unit(devinfo);
}

void
fs_visitor::assign_curb_setup()
{
   unsigned uniform_push_length =
      round_components_to_whole_registers(devinfo, prog_data->nr_params);

   unsigned ubo_push_length = 0;
   unsigned ubo_push_start[4];
   for (int i = 0; i < 4; i++) {
      ubo_push_start[i] = 8 * (ubo_push_length + uniform_push_length);
      ubo_push_length += prog_data->ubo_ranges[i].length;

      assert(ubo_push_start[i] % (8 * reg_unit(devinfo)) == 0);
      assert(ubo_push_length % (1 * reg_unit(devinfo)) == 0);
   }

   prog_data->curb_read_length = uniform_push_length + ubo_push_length;
   if (stage == MESA_SHADER_FRAGMENT &&
       ((struct brw_wm_prog_key *)key)->null_push_constant_tbimr_workaround)
      prog_data->curb_read_length = MAX2(1, prog_data->curb_read_length);

   uint64_t used = 0;
   bool is_compute = gl_shader_stage_is_compute(stage);

   if (is_compute && brw_cs_prog_data(prog_data)->uses_inline_data) {
      /* With COMPUTE_WALKER, we can push up to one register worth of data via
       * the inline data parameter in the COMPUTE_WALKER command itself.
       *
       * TODO: Support inline data and push at the same time.
       */
      assert(devinfo->verx10 >= 125);
      assert(uniform_push_length <= reg_unit(devinfo));
   } else if (is_compute && devinfo->verx10 >= 125) {
      assert(devinfo->has_lsc);
      fs_builder ubld = fs_builder(this, 1).exec_all().at(
         cfg->first_block(), cfg->first_block()->start());

      /* The base offset for our push data is passed in as R0.0[31:6]. We have
       * to mask off the bottom 6 bits.
       */
      fs_reg base_addr = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.AND(base_addr,
               retype(brw_vec1_grf(0, 0), BRW_REGISTER_TYPE_UD),
               brw_imm_ud(INTEL_MASK(31, 6)));

      /* On Gfx12-HP we load constants at the start of the program using A32
       * stateless messages.
       */
      for (unsigned i = 0; i < uniform_push_length;) {
         /* Limit ourselves to LSC HW limit of 8 GRFs (256bytes D32V64). */
         unsigned num_regs = MIN2(uniform_push_length - i, 8);
         assert(num_regs > 0);
         num_regs = 1 << util_logbase2(num_regs);

         fs_reg addr;

         /* This pass occurs after all of the optimization passes, so don't
          * emit an 'ADD addr, base_addr, 0' instruction.
          */
         if (i != 0) {
            addr = ubld.vgrf(BRW_REGISTER_TYPE_UD);
            ubld.ADD(addr, base_addr, brw_imm_ud(i * REG_SIZE));
         } else {
            addr = base_addr;
         }

         fs_reg srcs[4] = {
            brw_imm_ud(0), /* desc */
            brw_imm_ud(0), /* ex_desc */
            addr,          /* payload */
            fs_reg(),      /* payload2 */
         };

         fs_reg dest = retype(brw_vec8_grf(payload().num_regs + i, 0),
                              BRW_REGISTER_TYPE_UD);
         fs_inst *send = ubld.emit(SHADER_OPCODE_SEND, dest, srcs, 4);

         send->sfid = GFX12_SFID_UGM;
         send->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD,
                                   LSC_ADDR_SURFTYPE_FLAT,
                                   LSC_ADDR_SIZE_A32,
                                   LSC_DATA_SIZE_D32,
                                   num_regs * 8 /* num_channels */,
                                   true /* transpose */,
                                   LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
         send->header_size = 0;
         send->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, 1);
         send->size_written =
            lsc_msg_dest_len(devinfo, LSC_DATA_SIZE_D32, num_regs * 8) * REG_SIZE;
         send->send_is_volatile = true;

         i += num_regs;
      }

      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);
   }

   /* Map the offsets in the UNIFORM file to fixed HW regs. */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      for (unsigned int i = 0; i < inst->sources; i++) {
	 if (inst->src[i].file == UNIFORM) {
            int uniform_nr = inst->src[i].nr + inst->src[i].offset / 4;
            int constant_nr;
            if (inst->src[i].nr >= UBO_START) {
               /* constant_nr is in 32-bit units, the rest are in bytes */
               constant_nr = ubo_push_start[inst->src[i].nr - UBO_START] +
                             inst->src[i].offset / 4;
            } else if (uniform_nr >= 0 && uniform_nr < (int) uniforms) {
               constant_nr = push_constant_loc[uniform_nr];
            } else {
               /* Section 5.11 of the OpenGL 4.1 spec says:
                * "Out-of-bounds reads return undefined values, which include
                *  values from other variables of the active program or zero."
                * Just return the first push constant.
                */
               constant_nr = 0;
            }

            assert(constant_nr / 8 < 64);
            used |= BITFIELD64_BIT(constant_nr / 8);

	    struct brw_reg brw_reg = brw_vec1_grf(payload().num_regs +
						  constant_nr / 8,
						  constant_nr % 8);
            brw_reg.abs = inst->src[i].abs;
            brw_reg.negate = inst->src[i].negate;

            assert(inst->src[i].stride == 0);
            inst->src[i] = byte_offset(
               retype(brw_reg, inst->src[i].type),
               inst->src[i].offset % 4);
	 }
      }
   }

   uint64_t want_zero = used & prog_data->zero_push_reg;
   if (want_zero) {
      fs_builder ubld = fs_builder(this, 8).exec_all().at(
         cfg->first_block(), cfg->first_block()->start());

      /* push_reg_mask_param is in 32-bit units */
      unsigned mask_param = prog_data->push_reg_mask_param;
      struct brw_reg mask = brw_vec1_grf(payload().num_regs + mask_param / 8,
                                                              mask_param % 8);

      fs_reg b32;
      for (unsigned i = 0; i < 64; i++) {
         if (i % 16 == 0 && (want_zero & BITFIELD64_RANGE(i, 16))) {
            fs_reg shifted = ubld.vgrf(BRW_REGISTER_TYPE_W, 2);
            ubld.SHL(horiz_offset(shifted, 8),
                     byte_offset(retype(mask, BRW_REGISTER_TYPE_W), i / 8),
                     brw_imm_v(0x01234567));
            ubld.SHL(shifted, horiz_offset(shifted, 8), brw_imm_w(8));

            fs_builder ubld16 = ubld.group(16, 0);
            b32 = ubld16.vgrf(BRW_REGISTER_TYPE_D);
            ubld16.group(16, 0).ASR(b32, shifted, brw_imm_w(15));
         }

         if (want_zero & BITFIELD64_BIT(i)) {
            assert(i < prog_data->curb_read_length);
            struct brw_reg push_reg =
               retype(brw_vec8_grf(payload().num_regs + i, 0),
                      BRW_REGISTER_TYPE_D);

            ubld.AND(push_reg, push_reg, component(b32, i % 16));
         }
      }

      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);
   }

   /* This may be updated in assign_urb_setup or assign_vs_urb_setup. */
   this->first_non_payload_grf = payload().num_regs + prog_data->curb_read_length;
}

/*
 * Build up an array of indices into the urb_setup array that
 * references the active entries of the urb_setup array.
 * Used to accelerate walking the active entries of the urb_setup array
 * on each upload.
 */
void
brw_compute_urb_setup_index(struct brw_wm_prog_data *wm_prog_data)
{
   /* TODO(mesh): Review usage of this in the context of Mesh, we may want to
    * skip per-primitive attributes here.
    */

   /* Make sure uint8_t is sufficient */
   STATIC_ASSERT(VARYING_SLOT_MAX <= 0xff);
   uint8_t index = 0;
   for (uint8_t attr = 0; attr < VARYING_SLOT_MAX; attr++) {
      if (wm_prog_data->urb_setup[attr] >= 0) {
         wm_prog_data->urb_setup_attribs[index++] = attr;
      }
   }
   wm_prog_data->urb_setup_attribs_count = index;
}

static void
calculate_urb_setup(const struct intel_device_info *devinfo,
                    const struct brw_wm_prog_key *key,
                    struct brw_wm_prog_data *prog_data,
                    const nir_shader *nir,
                    const struct brw_mue_map *mue_map)
{
   memset(prog_data->urb_setup, -1, sizeof(prog_data->urb_setup));
   memset(prog_data->urb_setup_channel, 0, sizeof(prog_data->urb_setup_channel));

   int urb_next = 0; /* in vec4s */

   const uint64_t inputs_read =
      nir->info.inputs_read & ~nir->info.per_primitive_inputs;

   /* Figure out where each of the incoming setup attributes lands. */
   if (key->mesh_input != BRW_NEVER) {
      /* Per-Primitive Attributes are laid out by Hardware before the regular
       * attributes, so order them like this to make easy later to map setup
       * into real HW registers.
       */
      if (nir->info.per_primitive_inputs) {
         uint64_t per_prim_inputs_read =
               nir->info.inputs_read & nir->info.per_primitive_inputs;

         /* In Mesh, PRIMITIVE_SHADING_RATE, VIEWPORT and LAYER slots
          * are always at the beginning, because they come from MUE
          * Primitive Header, not Per-Primitive Attributes.
          */
         const uint64_t primitive_header_bits = VARYING_BIT_VIEWPORT |
                                                VARYING_BIT_LAYER |
                                                VARYING_BIT_PRIMITIVE_SHADING_RATE;

         if (mue_map) {
            unsigned per_prim_start_dw = mue_map->per_primitive_start_dw;
            unsigned per_prim_size_dw = mue_map->per_primitive_pitch_dw;

            bool reads_header = (per_prim_inputs_read & primitive_header_bits) != 0;

            if (reads_header || mue_map->user_data_in_primitive_header) {
               /* Primitive Shading Rate, Layer and Viewport live in the same
                * 4-dwords slot (psr is dword 0, layer is dword 1, and viewport
                * is dword 2).
                */
               if (per_prim_inputs_read & VARYING_BIT_PRIMITIVE_SHADING_RATE)
                  prog_data->urb_setup[VARYING_SLOT_PRIMITIVE_SHADING_RATE] = 0;

               if (per_prim_inputs_read & VARYING_BIT_LAYER)
                  prog_data->urb_setup[VARYING_SLOT_LAYER] = 0;

               if (per_prim_inputs_read & VARYING_BIT_VIEWPORT)
                  prog_data->urb_setup[VARYING_SLOT_VIEWPORT] = 0;

               per_prim_inputs_read &= ~primitive_header_bits;
            } else {
               /* If fs doesn't need primitive header, then it won't be made
                * available through SBE_MESH, so we have to skip them when
                * calculating offset from start of per-prim data.
                */
               per_prim_start_dw += mue_map->per_primitive_header_size_dw;
               per_prim_size_dw -= mue_map->per_primitive_header_size_dw;
            }

            u_foreach_bit64(i, per_prim_inputs_read) {
               int start = mue_map->start_dw[i];

               assert(start >= 0);
               assert(mue_map->len_dw[i] > 0);

               assert(unsigned(start) >= per_prim_start_dw);
               unsigned pos_dw = unsigned(start) - per_prim_start_dw;

               prog_data->urb_setup[i] = urb_next + pos_dw / 4;
               prog_data->urb_setup_channel[i] = pos_dw % 4;
            }

            urb_next = per_prim_size_dw / 4;
         } else {
            /* With no MUE map, we never read the primitive header, and
             * per-primitive attributes won't be packed either, so just lay
             * them in varying order.
             */
            per_prim_inputs_read &= ~primitive_header_bits;

            for (unsigned i = 0; i < VARYING_SLOT_MAX; i++) {
               if (per_prim_inputs_read & BITFIELD64_BIT(i)) {
                  prog_data->urb_setup[i] = urb_next++;
               }
            }

            /* The actual setup attributes later must be aligned to a full GRF. */
            urb_next = ALIGN(urb_next, 2);
         }

         prog_data->num_per_primitive_inputs = urb_next;
      }

      const uint64_t clip_dist_bits = VARYING_BIT_CLIP_DIST0 |
                                      VARYING_BIT_CLIP_DIST1;

      uint64_t unique_fs_attrs = inputs_read & BRW_FS_VARYING_INPUT_MASK;

      if (inputs_read & clip_dist_bits) {
         assert(!mue_map || mue_map->per_vertex_header_size_dw > 8);
         unique_fs_attrs &= ~clip_dist_bits;
      }

      if (mue_map) {
         unsigned per_vertex_start_dw = mue_map->per_vertex_start_dw;
         unsigned per_vertex_size_dw = mue_map->per_vertex_pitch_dw;

         /* Per-Vertex header is available to fragment shader only if there's
          * user data there.
          */
         if (!mue_map->user_data_in_vertex_header) {
            per_vertex_start_dw += 8;
            per_vertex_size_dw -= 8;
         }

         /* In Mesh, CLIP_DIST slots are always at the beginning, because
          * they come from MUE Vertex Header, not Per-Vertex Attributes.
          */
         if (inputs_read & clip_dist_bits) {
            prog_data->urb_setup[VARYING_SLOT_CLIP_DIST0] = urb_next;
            prog_data->urb_setup[VARYING_SLOT_CLIP_DIST1] = urb_next + 1;
         } else if (mue_map && mue_map->per_vertex_header_size_dw > 8) {
            /* Clip distances are in MUE, but we are not reading them in FS. */
            per_vertex_start_dw += 8;
            per_vertex_size_dw -= 8;
         }

         /* Per-Vertex attributes are laid out ordered.  Because we always link
          * Mesh and Fragment shaders, the which slots are written and read by
          * each of them will match. */
         u_foreach_bit64(i, unique_fs_attrs) {
            int start = mue_map->start_dw[i];

            assert(start >= 0);
            assert(mue_map->len_dw[i] > 0);

            assert(unsigned(start) >= per_vertex_start_dw);
            unsigned pos_dw = unsigned(start) - per_vertex_start_dw;

            prog_data->urb_setup[i] = urb_next + pos_dw / 4;
            prog_data->urb_setup_channel[i] = pos_dw % 4;
         }

         urb_next += per_vertex_size_dw / 4;
      } else {
         /* If we don't have an MUE map, just lay down the inputs the FS reads
          * in varying order, as we do for the legacy pipeline.
          */
         if (inputs_read & clip_dist_bits) {
            prog_data->urb_setup[VARYING_SLOT_CLIP_DIST0] = urb_next++;
            prog_data->urb_setup[VARYING_SLOT_CLIP_DIST1] = urb_next++;
         }

         for (unsigned int i = 0; i < VARYING_SLOT_MAX; i++) {
            if (unique_fs_attrs & BITFIELD64_BIT(i))
               prog_data->urb_setup[i] = urb_next++;
         }
      }
   } else {
      assert(!nir->info.per_primitive_inputs);

      uint64_t vue_header_bits =
         VARYING_BIT_PSIZ | VARYING_BIT_LAYER | VARYING_BIT_VIEWPORT;

      uint64_t unique_fs_attrs = inputs_read & BRW_FS_VARYING_INPUT_MASK;

      /* VUE header fields all live in the same URB slot, so we pass them
       * as a single FS input attribute.  We want to only count them once.
       */
      if (inputs_read & vue_header_bits) {
         unique_fs_attrs &= ~vue_header_bits;
         unique_fs_attrs |= VARYING_BIT_PSIZ;
      }

      if (util_bitcount64(unique_fs_attrs) <= 16) {
         /* The SF/SBE pipeline stage can do arbitrary rearrangement of the
          * first 16 varying inputs, so we can put them wherever we want.
          * Just put them in order.
          *
          * This is useful because it means that (a) inputs not used by the
          * fragment shader won't take up valuable register space, and (b) we
          * won't have to recompile the fragment shader if it gets paired with
          * a different vertex (or geometry) shader.
          *
          * VUE header fields share the same FS input attribute.
          */
         if (inputs_read & vue_header_bits) {
            if (inputs_read & VARYING_BIT_PSIZ)
               prog_data->urb_setup[VARYING_SLOT_PSIZ] = urb_next;
            if (inputs_read & VARYING_BIT_LAYER)
               prog_data->urb_setup[VARYING_SLOT_LAYER] = urb_next;
            if (inputs_read & VARYING_BIT_VIEWPORT)
               prog_data->urb_setup[VARYING_SLOT_VIEWPORT] = urb_next;

            urb_next++;
         }

         for (unsigned int i = 0; i < VARYING_SLOT_MAX; i++) {
            if (inputs_read & BRW_FS_VARYING_INPUT_MASK & ~vue_header_bits &
                BITFIELD64_BIT(i)) {
               prog_data->urb_setup[i] = urb_next++;
            }
         }
      } else {
         /* We have enough input varyings that the SF/SBE pipeline stage can't
          * arbitrarily rearrange them to suit our whim; we have to put them
          * in an order that matches the output of the previous pipeline stage
          * (geometry or vertex shader).
          */

         /* Re-compute the VUE map here in the case that the one coming from
          * geometry has more than one position slot (used for Primitive
          * Replication).
          */
         struct intel_vue_map prev_stage_vue_map;
         brw_compute_vue_map(devinfo, &prev_stage_vue_map,
                             key->input_slots_valid,
                             nir->info.separate_shader, 1);

         int first_slot =
            brw_compute_first_urb_slot_required(inputs_read,
                                                &prev_stage_vue_map);

         assert(prev_stage_vue_map.num_slots <= first_slot + 32);
         for (int slot = first_slot; slot < prev_stage_vue_map.num_slots;
              slot++) {
            int varying = prev_stage_vue_map.slot_to_varying[slot];
            if (varying != BRW_VARYING_SLOT_PAD &&
                (inputs_read & BRW_FS_VARYING_INPUT_MASK &
                 BITFIELD64_BIT(varying))) {
               prog_data->urb_setup[varying] = slot - first_slot;
            }
         }
         urb_next = prev_stage_vue_map.num_slots - first_slot;
      }
   }

   prog_data->num_varying_inputs = urb_next - prog_data->num_per_primitive_inputs;
   prog_data->inputs = inputs_read;

   brw_compute_urb_setup_index(prog_data);
}

void
fs_visitor::assign_urb_setup()
{
   assert(stage == MESA_SHADER_FRAGMENT);
   struct brw_wm_prog_data *prog_data = brw_wm_prog_data(this->prog_data);

   int urb_start = payload().num_regs + prog_data->base.curb_read_length;

   /* Offset all the urb_setup[] index by the actual position of the
    * setup regs, now that the location of the constants has been chosen.
    */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == ATTR) {
            /* ATTR fs_reg::nr in the FS is in units of logical scalar
             * inputs each of which consumes 16B on Gfx4-Gfx12.  In
             * single polygon mode this leads to the following layout
             * of the vertex setup plane parameters in the ATTR
             * register file:
             *
             *  fs_reg::nr   Input   Comp0  Comp1  Comp2  Comp3
             *      0       Attr0.x  a1-a0  a2-a0   N/A    a0
             *      1       Attr0.y  a1-a0  a2-a0   N/A    a0
             *      2       Attr0.z  a1-a0  a2-a0   N/A    a0
             *      3       Attr0.w  a1-a0  a2-a0   N/A    a0
             *      4       Attr1.x  a1-a0  a2-a0   N/A    a0
             *     ...
             *
             * In multipolygon mode that no longer works since
             * different channels may be processing polygons with
             * different plane parameters, so each parameter above is
             * represented as a dispatch_width-wide vector:
             *
             *  fs_reg::nr     fs_reg::offset    Input      Comp0     ...    CompN
             *      0                 0          Attr0.x  a1[0]-a0[0] ... a1[N]-a0[N]
             *      0        4 * dispatch_width  Attr0.x  a2[0]-a0[0] ... a2[N]-a0[N]
             *      0        8 * dispatch_width  Attr0.x     N/A      ...     N/A
             *      0       12 * dispatch_width  Attr0.x    a0[0]     ...    a0[N]
             *      1                 0          Attr0.y  a1[0]-a0[0] ... a1[N]-a0[N]
             *     ...
             *
             * Note that many of the components on a single row above
             * are likely to be replicated multiple times (if, say, a
             * single SIMD thread is only processing 2 different
             * polygons), so plane parameters aren't actually stored
             * in GRF memory with that layout to avoid wasting space.
             * Instead we compose ATTR register regions with a 2D
             * region that walks through the parameters of each
             * polygon with the correct stride, reading the parameter
             * corresponding to each channel directly from the PS
             * thread payload.
             *
             * The latter layout corresponds to a param_width equal to
             * dispatch_width, while the former (scalar parameter)
             * layout has a param_width of 1.
             *
             * Gfx20+ represent plane parameters in a format similar
             * to the above, except the parameters are packed in 12B
             * and ordered like "a0, a1-a0, a2-a0" instead of the
             * above vec4 representation with a missing component.
             */
            const unsigned param_width = (max_polygons > 1 ? dispatch_width : 1);

            /* Size of a single scalar component of a plane parameter
             * in bytes.
             */
            const unsigned chan_sz = 4;
            struct brw_reg reg;
            assert(max_polygons > 0);

            /* Calculate the base register on the thread payload of
             * either the block of vertex setup data or the block of
             * per-primitive constant data depending on whether we're
             * accessing a primitive or vertex input.  Also calculate
             * the index of the input within that block.
             */
            const bool per_prim = inst->src[i].nr < prog_data->num_per_primitive_inputs;
            const unsigned base = urb_start +
               (per_prim ? 0 :
                ALIGN(prog_data->num_per_primitive_inputs / 2,
                      reg_unit(devinfo)) * max_polygons);
            const unsigned idx = per_prim ? inst->src[i].nr :
               inst->src[i].nr - prog_data->num_per_primitive_inputs;

            /* Translate the offset within the param_width-wide
             * representation described above into an offset and a
             * grf, which contains the plane parameters for the first
             * polygon processed by the thread.
             */
            if (devinfo->ver >= 20 && !per_prim) {
               /* Gfx20+ is able to pack 5 logical input components
                * per 64B register for vertex setup data.
                */
               const unsigned grf = base + idx / 5 * 2 * max_polygons;
               assert(inst->src[i].offset / param_width < 12);
               const unsigned delta = idx % 5 * 12 +
                  inst->src[i].offset / (param_width * chan_sz) * chan_sz +
                  inst->src[i].offset % chan_sz;
               reg = byte_offset(retype(brw_vec8_grf(grf, 0), inst->src[i].type),
                                 delta);
            } else {
               /* Earlier platforms and per-primitive block pack 2 logical
                * input components per 32B register.
                */
               const unsigned grf = base + idx / 2 * max_polygons;
               assert(inst->src[i].offset / param_width < REG_SIZE / 2);
               const unsigned delta = (idx % 2) * (REG_SIZE / 2) +
                  inst->src[i].offset / (param_width * chan_sz) * chan_sz +
                  inst->src[i].offset % chan_sz;
               reg = byte_offset(retype(brw_vec8_grf(grf, 0), inst->src[i].type),
                                 delta);
            }

            if (max_polygons > 1) {
               assert(devinfo->ver >= 12);
               /* Misaligned channel strides that would lead to
                * cross-channel access in the representation above are
                * disallowed.
                */
               assert(inst->src[i].stride * type_sz(inst->src[i].type) == chan_sz);

               /* Number of channels processing the same polygon. */
               const unsigned poly_width = dispatch_width / max_polygons;
               assert(dispatch_width % max_polygons == 0);

               /* Accessing a subset of channels of a parameter vector
                * starting from "chan" is necessary to handle
                * SIMD-lowered instructions though.
                */
               const unsigned chan = inst->src[i].offset %
                  (param_width * chan_sz) / chan_sz;
               assert(chan < dispatch_width);
               assert(chan % poly_width == 0);
               const unsigned reg_size = reg_unit(devinfo) * REG_SIZE;
               reg = byte_offset(reg, chan / poly_width * reg_size);

               if (inst->exec_size > poly_width) {
                  /* Accessing the parameters for multiple polygons.
                   * Corresponding parameters for different polygons
                   * are stored a GRF apart on the thread payload, so
                   * use that as vertical stride.
                   */
                  const unsigned vstride = reg_size / type_sz(inst->src[i].type);
                  assert(vstride <= 32);
                  assert(chan % poly_width == 0);
                  reg = stride(reg, vstride, poly_width, 0);
               } else {
                  /* Accessing one parameter for a single polygon --
                   * Translate to a scalar region.
                   */
                  assert(chan % poly_width + inst->exec_size <= poly_width);
                  reg = stride(reg, 0, 1, 0);
               }

            } else {
               const unsigned width = inst->src[i].stride == 0 ?
                  1 : MIN2(inst->exec_size, 8);
               reg = stride(reg, width * inst->src[i].stride,
                            width, inst->src[i].stride);
            }

            reg.abs = inst->src[i].abs;
            reg.negate = inst->src[i].negate;
            inst->src[i] = reg;
         }
      }
   }

   /* Each attribute is 4 setup channels, each of which is half a reg,
    * but they may be replicated multiple times for multipolygon
    * dispatch.
    */
   this->first_non_payload_grf += prog_data->num_varying_inputs * 2 * max_polygons;

   /* Unlike regular attributes, per-primitive attributes have all 4 channels
    * in the same slot, so each GRF can store two slots.
    */
   assert(prog_data->num_per_primitive_inputs % 2 == 0);
   this->first_non_payload_grf += prog_data->num_per_primitive_inputs / 2 * max_polygons;
}

void
fs_visitor::convert_attr_sources_to_hw_regs(fs_inst *inst)
{
   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].file == ATTR) {
         assert(inst->src[i].nr == 0);
         int grf = payload().num_regs +
                   prog_data->curb_read_length +
                   inst->src[i].offset / REG_SIZE;

         /* As explained at brw_reg_from_fs_reg, From the Haswell PRM:
          *
          * VertStride must be used to cross GRF register boundaries. This
          * rule implies that elements within a 'Width' cannot cross GRF
          * boundaries.
          *
          * So, for registers that are large enough, we have to split the exec
          * size in two and trust the compression state to sort it out.
          */
         unsigned total_size = inst->exec_size *
                               inst->src[i].stride *
                               type_sz(inst->src[i].type);

         assert(total_size <= 2 * REG_SIZE);
         const unsigned exec_size =
            (total_size <= REG_SIZE) ? inst->exec_size : inst->exec_size / 2;

         unsigned width = inst->src[i].stride == 0 ? 1 : exec_size;
         struct brw_reg reg =
            stride(byte_offset(retype(brw_vec8_grf(grf, 0), inst->src[i].type),
                               inst->src[i].offset % REG_SIZE),
                   exec_size * inst->src[i].stride,
                   width, inst->src[i].stride);
         reg.abs = inst->src[i].abs;
         reg.negate = inst->src[i].negate;

         inst->src[i] = reg;
      }
   }
}

void
fs_visitor::assign_vs_urb_setup()
{
   struct brw_vs_prog_data *vs_prog_data = brw_vs_prog_data(prog_data);

   assert(stage == MESA_SHADER_VERTEX);

   /* Each attribute is 4 regs. */
   this->first_non_payload_grf += 4 * vs_prog_data->nr_attribute_slots;

   assert(vs_prog_data->base.urb_read_length <= 15);

   /* Rewrite all ATTR file references to the hw grf that they land in. */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
fs_visitor::assign_tcs_urb_setup()
{
   assert(stage == MESA_SHADER_TESS_CTRL);

   /* Rewrite all ATTR file references to HW_REGs. */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
fs_visitor::assign_tes_urb_setup()
{
   assert(stage == MESA_SHADER_TESS_EVAL);

   struct brw_vue_prog_data *vue_prog_data = brw_vue_prog_data(prog_data);

   first_non_payload_grf += 8 * vue_prog_data->urb_read_length;

   /* Rewrite all ATTR file references to HW_REGs. */
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
fs_visitor::assign_gs_urb_setup()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   struct brw_vue_prog_data *vue_prog_data = brw_vue_prog_data(prog_data);

   first_non_payload_grf +=
      8 * vue_prog_data->urb_read_length * nir->info.gs.vertices_in;

   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      /* Rewrite all ATTR file references to GRFs. */
      convert_attr_sources_to_hw_regs(inst);
   }
}

int
brw_get_subgroup_id_param_index(const intel_device_info *devinfo,
                                const brw_stage_prog_data *prog_data)
{
   if (prog_data->nr_params == 0)
      return -1;

   if (devinfo->verx10 >= 125)
      return -1;

   /* The local thread id is always the last parameter in the list */
   uint32_t last_param = prog_data->param[prog_data->nr_params - 1];
   if (last_param == BRW_PARAM_BUILTIN_SUBGROUP_ID)
      return prog_data->nr_params - 1;

   return -1;
}

/**
 * Assign UNIFORM file registers to either push constants or pull constants.
 *
 * We allow a fragment shader to have more than the specified minimum
 * maximum number of fragment shader uniform components (64).  If
 * there are too many of these, they'd fill up all of register space.
 * So, this will push some of them out to the pull constant buffer and
 * update the program to load them.
 */
void
fs_visitor::assign_constant_locations()
{
   /* Only the first compile gets to decide on locations. */
   if (push_constant_loc)
      return;

   push_constant_loc = ralloc_array(mem_ctx, int, uniforms);
   for (unsigned u = 0; u < uniforms; u++)
      push_constant_loc[u] = u;

   /* Now that we know how many regular uniforms we'll push, reduce the
    * UBO push ranges so we don't exceed the 3DSTATE_CONSTANT limits.
    *
    * If changing this value, note the limitation about total_regs in
    * brw_curbe.c/crocus_state.c
    */
   const unsigned max_push_length = 64;
   unsigned push_length =
      round_components_to_whole_registers(devinfo, prog_data->nr_params);
   for (int i = 0; i < 4; i++) {
      struct brw_ubo_range *range = &prog_data->ubo_ranges[i];

      if (push_length + range->length > max_push_length)
         range->length = max_push_length - push_length;

      push_length += range->length;

      assert(push_length % (1 * reg_unit(devinfo)) == 0);

   }
   assert(push_length <= max_push_length);
}

bool
fs_visitor::get_pull_locs(const fs_reg &src,
                          unsigned *out_surf_index,
                          unsigned *out_pull_index)
{
   assert(src.file == UNIFORM);

   if (src.nr < UBO_START)
      return false;

   const struct brw_ubo_range *range =
      &prog_data->ubo_ranges[src.nr - UBO_START];

   /* If this access is in our (reduced) range, use the push data. */
   if (src.offset / 32 < range->length)
      return false;

   *out_surf_index = range->block;
   *out_pull_index = (32 * range->start + src.offset) / 4;

   prog_data->has_ubo_pull = true;

   return true;
}

/**
 * Once we've generated code, try to convert normal FS_OPCODE_FB_WRITE
 * instructions to FS_OPCODE_REP_FB_WRITE.
 */
void
fs_visitor::emit_repclear_shader()
{
   brw_wm_prog_key *key = (brw_wm_prog_key*) this->key;
   fs_inst *write = NULL;

   assert(devinfo->ver < 20);
   assert(uniforms == 0);
   assume(key->nr_color_regions > 0);

   fs_reg color_output = retype(brw_vec4_grf(127, 0), BRW_REGISTER_TYPE_UD);
   fs_reg header = retype(brw_vec8_grf(125, 0), BRW_REGISTER_TYPE_UD);

   /* We pass the clear color as a flat input.  Copy it to the output. */
   fs_reg color_input =
      brw_reg(BRW_GENERAL_REGISTER_FILE, 2, 3, 0, 0, BRW_REGISTER_TYPE_UD,
              BRW_VERTICAL_STRIDE_8, BRW_WIDTH_2, BRW_HORIZONTAL_STRIDE_4,
              BRW_SWIZZLE_XYZW, WRITEMASK_XYZW);

   const fs_builder bld = fs_builder(this).at_end();
   bld.exec_all().group(4, 0).MOV(color_output, color_input);

   if (key->nr_color_regions > 1) {
      /* Copy g0..g1 as the message header */
      bld.exec_all().group(16, 0)
         .MOV(header, retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD));
   }

   for (int i = 0; i < key->nr_color_regions; ++i) {
      if (i > 0)
         bld.exec_all().group(1, 0).MOV(component(header, 2), brw_imm_ud(i));

      write = bld.emit(SHADER_OPCODE_SEND);
      write->resize_sources(3);
      write->sfid = GFX6_SFID_DATAPORT_RENDER_CACHE;
      write->src[0] = brw_imm_ud(0);
      write->src[1] = brw_imm_ud(0);
      write->src[2] = i == 0 ? color_output : header;
      write->check_tdr = true;
      write->send_has_side_effects = true;
      write->desc = brw_fb_write_desc(devinfo, i,
         BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED,
         i == key->nr_color_regions - 1, false);

      /* We can use a headerless message for the first render target */
      write->header_size = i == 0 ? 0 : 2;
      write->mlen = 1 + write->header_size;
   }
   write->eot = true;
   write->last_rt = true;

   calculate_cfg();

   this->first_non_payload_grf = payload().num_regs;

   brw_fs_lower_scoreboard(*this);
}

/**
 * Get the mask of SIMD channels enabled during dispatch and not yet disabled
 * by discard.  Due to the layout of the sample mask in the fragment shader
 * thread payload, \p bld is required to have a dispatch_width() not greater
 * than 16 for fragment shaders.
 */
fs_reg
brw_sample_mask_reg(const fs_builder &bld)
{
   const fs_visitor &s = *bld.shader;

   if (s.stage != MESA_SHADER_FRAGMENT) {
      return brw_imm_ud(0xffffffff);
   } else if (brw_wm_prog_data(s.prog_data)->uses_kill) {
      assert(bld.dispatch_width() <= 16);
      return brw_flag_subreg(sample_mask_flag_subreg(s) + bld.group() / 16);
   } else {
      assert(bld.dispatch_width() <= 16);
      assert(s.devinfo->ver < 20);
      return retype(brw_vec1_grf((bld.group() >= 16 ? 2 : 1), 7),
                    BRW_REGISTER_TYPE_UW);
   }
}

uint32_t
brw_fb_write_msg_control(const fs_inst *inst,
                         const struct brw_wm_prog_data *prog_data)
{
   uint32_t mctl;

   if (prog_data->dual_src_blend) {
      assert(inst->exec_size < 32);

      if (inst->group % 16 == 0)
         mctl = BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN01;
      else if (inst->group % 16 == 8)
         mctl = BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN23;
      else
         unreachable("Invalid dual-source FB write instruction group");
   } else {
      assert(inst->group == 0 || (inst->group == 16 && inst->exec_size == 16));

      if (inst->exec_size == 16)
         mctl = BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE;
      else if (inst->exec_size == 8)
         mctl = BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_SINGLE_SOURCE_SUBSPAN01;
      else if (inst->exec_size == 32)
         mctl = XE2_DATAPORT_RENDER_TARGET_WRITE_SIMD32_SINGLE_SOURCE;
      else
         unreachable("Invalid FB write execution size");
   }

   return mctl;
}

 /**
 * Predicate the specified instruction on the sample mask.
 */
void
brw_emit_predicate_on_sample_mask(const fs_builder &bld, fs_inst *inst)
{
   assert(bld.shader->stage == MESA_SHADER_FRAGMENT &&
          bld.group() == inst->group &&
          bld.dispatch_width() == inst->exec_size);

   const fs_visitor &s = *bld.shader;
   const fs_reg sample_mask = brw_sample_mask_reg(bld);
   const unsigned subreg = sample_mask_flag_subreg(s);

   if (brw_wm_prog_data(s.prog_data)->uses_kill) {
      assert(sample_mask.file == ARF &&
             sample_mask.nr == brw_flag_subreg(subreg).nr &&
             sample_mask.subnr == brw_flag_subreg(
                subreg + inst->group / 16).subnr);
   } else {
      bld.group(1, 0).exec_all()
         .MOV(brw_flag_subreg(subreg + inst->group / 16), sample_mask);
   }

   if (inst->predicate) {
      assert(inst->predicate == BRW_PREDICATE_NORMAL);
      assert(!inst->predicate_inverse);
      assert(inst->flag_subreg == 0);
      assert(s.devinfo->ver < 20);
      /* Combine the sample mask with the existing predicate by using a
       * vertical predication mode.
       */
      inst->predicate = BRW_PREDICATE_ALIGN1_ALLV;
   } else {
      inst->flag_subreg = subreg;
      inst->predicate = BRW_PREDICATE_NORMAL;
      inst->predicate_inverse = false;
   }
}

void
fs_visitor::dump_instructions_to_file(FILE *file) const
{
   if (cfg && grf_used == 0) {
      const register_pressure &rp = regpressure_analysis.require();
      unsigned ip = 0, max_pressure = 0;
      unsigned cf_count = 0;
      foreach_block_and_inst(block, fs_inst, inst, cfg) {
         if (inst->is_control_flow_end())
            cf_count -= 1;

         max_pressure = MAX2(max_pressure, rp.regs_live_at_ip[ip]);
         fprintf(file, "{%3d} %4d: ", rp.regs_live_at_ip[ip], ip);
         for (unsigned i = 0; i < cf_count; i++)
            fprintf(file, "  ");
         dump_instruction(inst, file);
         ip++;

         if (inst->is_control_flow_begin())
            cf_count += 1;
      }
      fprintf(file, "Maximum %3d registers live at once.\n", max_pressure);
   } else if (cfg && exec_list_is_empty(&instructions)) {
      unsigned ip = 0;
      foreach_block_and_inst(block, fs_inst, inst, cfg) {
         fprintf(file, "%4d: ", ip);
         dump_instruction(inst, file);
         ip++;
      }
   } else {
      int ip = 0;
      foreach_in_list(fs_inst, inst, &instructions) {
         fprintf(file, "%4d: ", ip++);
         dump_instruction(inst, file);
      }
   }
}

void
fs_visitor::dump_instructions(const char *name) const
{
   FILE *file = stderr;
   if (name && __normal_user()) {
      file = fopen(name, "w");
      if (!file)
         file = stderr;
   }

   dump_instructions_to_file(file);

   if (file != stderr) {
      fclose(file);
   }
}

static const char *
brw_instruction_name(const struct brw_isa_info *isa, enum opcode op)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   switch (op) {
   case 0 ... NUM_BRW_OPCODES - 1:
      /* The DO instruction doesn't exist on Gfx9+, but we use it to mark the
       * start of a loop in the IR.
       */
      if (op == BRW_OPCODE_DO)
         return "do";

      /* DPAS instructions may transiently exist on platforms that do not
       * support DPAS. They will eventually be lowered, but in the meantime it
       * must be possible to query the instruction name.
       */
      if (devinfo->verx10 < 125 && op == BRW_OPCODE_DPAS)
         return "dpas";

      assert(brw_opcode_desc(isa, op)->name);
      return brw_opcode_desc(isa, op)->name;
   case FS_OPCODE_FB_WRITE_LOGICAL:
      return "fb_write_logical";
   case FS_OPCODE_FB_READ_LOGICAL:
      return "fb_read_logical";

   case SHADER_OPCODE_RCP:
      return "rcp";
   case SHADER_OPCODE_RSQ:
      return "rsq";
   case SHADER_OPCODE_SQRT:
      return "sqrt";
   case SHADER_OPCODE_EXP2:
      return "exp2";
   case SHADER_OPCODE_LOG2:
      return "log2";
   case SHADER_OPCODE_POW:
      return "pow";
   case SHADER_OPCODE_INT_QUOTIENT:
      return "int_quot";
   case SHADER_OPCODE_INT_REMAINDER:
      return "int_rem";
   case SHADER_OPCODE_SIN:
      return "sin";
   case SHADER_OPCODE_COS:
      return "cos";

   case SHADER_OPCODE_SEND:
      return "send";

   case SHADER_OPCODE_UNDEF:
      return "undef";

   case SHADER_OPCODE_TEX_LOGICAL:
      return "tex_logical";
   case SHADER_OPCODE_TXD_LOGICAL:
      return "txd_logical";
   case SHADER_OPCODE_TXF_LOGICAL:
      return "txf_logical";
   case SHADER_OPCODE_TXL_LOGICAL:
      return "txl_logical";
   case SHADER_OPCODE_TXS_LOGICAL:
      return "txs_logical";
   case FS_OPCODE_TXB_LOGICAL:
      return "txb_logical";
   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
      return "txf_cms_w_logical";
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      return "txf_cms_w_gfx12_logical";
   case SHADER_OPCODE_TXF_MCS_LOGICAL:
      return "txf_mcs_logical";
   case SHADER_OPCODE_LOD_LOGICAL:
      return "lod_logical";
   case SHADER_OPCODE_TG4_LOGICAL:
      return "tg4_logical";
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
      return "tg4_offset_logical";
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
      return "tg4_offset_lod_logical";
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
      return "tg4_offset_bias_logical";
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
      return "tg4_b_logical";
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
      return "tg4_l_logical";
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
      return "tg4_i_logical";
   case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
      return "sampleinfo_logical";

   case SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
      return "image_size_logical";

   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
      return "untyped_atomic_logical";
   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
      return "untyped_surface_read_logical";
   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
      return "untyped_surface_write_logical";
   case SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      return "unaligned_oword_block_read_logical";
   case SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL:
      return "oword_block_write_logical";
   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
      return "a64_untyped_read_logical";
   case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
      return "a64_oword_block_read_logical";
   case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      return "a64_unaligned_oword_block_read_logical";
   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      return "a64_oword_block_write_logical";
   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      return "a64_untyped_write_logical";
   case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      return "a64_byte_scattered_read_logical";
   case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
      return "a64_byte_scattered_write_logical";
   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      return "a64_untyped_atomic_logical";
   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
      return "typed_atomic_logical";
   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
      return "typed_surface_read_logical";
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      return "typed_surface_write_logical";
   case SHADER_OPCODE_MEMORY_FENCE:
      return "memory_fence";
   case FS_OPCODE_SCHEDULING_FENCE:
      return "scheduling_fence";
   case SHADER_OPCODE_INTERLOCK:
      /* For an interlock we actually issue a memory fence via sendc. */
      return "interlock";

   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
      return "byte_scattered_read_logical";
   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
      return "byte_scattered_write_logical";
   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      return "dword_scattered_read_logical";
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      return "dword_scattered_write_logical";

   case SHADER_OPCODE_LOAD_PAYLOAD:
      return "load_payload";
   case FS_OPCODE_PACK:
      return "pack";

   case SHADER_OPCODE_SCRATCH_HEADER:
      return "scratch_header";

   case SHADER_OPCODE_URB_WRITE_LOGICAL:
      return "urb_write_logical";
   case SHADER_OPCODE_URB_READ_LOGICAL:
      return "urb_read_logical";

   case SHADER_OPCODE_FIND_LIVE_CHANNEL:
      return "find_live_channel";
   case SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL:
      return "find_last_live_channel";
   case SHADER_OPCODE_LOAD_LIVE_CHANNELS:
      return "load_live_channels";
   case FS_OPCODE_LOAD_LIVE_CHANNELS:
      return "fs_load_live_channels";

   case SHADER_OPCODE_BROADCAST:
      return "broadcast";
   case SHADER_OPCODE_SHUFFLE:
      return "shuffle";
   case SHADER_OPCODE_SEL_EXEC:
      return "sel_exec";
   case SHADER_OPCODE_QUAD_SWIZZLE:
      return "quad_swizzle";
   case SHADER_OPCODE_CLUSTER_BROADCAST:
      return "cluster_broadcast";

   case SHADER_OPCODE_GET_BUFFER_SIZE:
      return "get_buffer_size";

   case FS_OPCODE_DDX_COARSE:
      return "ddx_coarse";
   case FS_OPCODE_DDX_FINE:
      return "ddx_fine";
   case FS_OPCODE_DDY_COARSE:
      return "ddy_coarse";
   case FS_OPCODE_DDY_FINE:
      return "ddy_fine";

   case FS_OPCODE_PIXEL_X:
      return "pixel_x";
   case FS_OPCODE_PIXEL_Y:
      return "pixel_y";

   case FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
      return "uniform_pull_const";
   case FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL:
      return "varying_pull_const_logical";

   case FS_OPCODE_PACK_HALF_2x16_SPLIT:
      return "pack_half_2x16_split";

   case SHADER_OPCODE_HALT_TARGET:
      return "halt_target";

   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      return "interp_sample";
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      return "interp_shared_offset";
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return "interp_per_slot_offset";

   case SHADER_OPCODE_BARRIER:
      return "barrier";
   case SHADER_OPCODE_MULH:
      return "mulh";
   case SHADER_OPCODE_ISUB_SAT:
      return "isub_sat";
   case SHADER_OPCODE_USUB_SAT:
      return "usub_sat";
   case SHADER_OPCODE_MOV_INDIRECT:
      return "mov_indirect";
   case SHADER_OPCODE_MOV_RELOC_IMM:
      return "mov_reloc_imm";

   case RT_OPCODE_TRACE_RAY_LOGICAL:
      return "rt_trace_ray_logical";

   case SHADER_OPCODE_RND_MODE:
      return "rnd_mode";
   case SHADER_OPCODE_FLOAT_CONTROL_MODE:
      return "float_control_mode";
   case SHADER_OPCODE_BTD_SPAWN_LOGICAL:
      return "btd_spawn_logical";
   case SHADER_OPCODE_BTD_RETIRE_LOGICAL:
      return "btd_retire_logical";
   case SHADER_OPCODE_READ_ARCH_REG:
      return "read_arch_reg";
   }

   unreachable("not reached");
}


void
fs_visitor::dump_instruction_to_file(const fs_inst *inst, FILE *file) const
{
   if (inst->predicate) {
      fprintf(file, "(%cf%d.%d) ",
              inst->predicate_inverse ? '-' : '+',
              inst->flag_subreg / 2,
              inst->flag_subreg % 2);
   }

   fprintf(file, "%s", brw_instruction_name(&compiler->isa, inst->opcode));
   if (inst->saturate)
      fprintf(file, ".sat");
   if (inst->conditional_mod) {
      fprintf(file, "%s", conditional_modifier[inst->conditional_mod]);
      if (!inst->predicate &&
          (inst->opcode != BRW_OPCODE_SEL &&
           inst->opcode != BRW_OPCODE_CSEL &&
           inst->opcode != BRW_OPCODE_IF &&
           inst->opcode != BRW_OPCODE_WHILE)) {
         fprintf(file, ".f%d.%d", inst->flag_subreg / 2,
                 inst->flag_subreg % 2);
      }
   }
   fprintf(file, "(%d) ", inst->exec_size);

   if (inst->mlen) {
      fprintf(file, "(mlen: %d) ", inst->mlen);
   }

   if (inst->ex_mlen) {
      fprintf(file, "(ex_mlen: %d) ", inst->ex_mlen);
   }

   if (inst->eot) {
      fprintf(file, "(EOT) ");
   }

   switch (inst->dst.file) {
   case VGRF:
      fprintf(file, "vgrf%d", inst->dst.nr);
      break;
   case FIXED_GRF:
      fprintf(file, "g%d", inst->dst.nr);
      if (inst->dst.subnr != 0)
         fprintf(file, ".%d", inst->dst.subnr / type_sz(inst->dst.type));
      break;
   case BAD_FILE:
      fprintf(file, "(null)");
      break;
   case UNIFORM:
      fprintf(file, "***u%d***", inst->dst.nr);
      break;
   case ATTR:
      fprintf(file, "***attr%d***", inst->dst.nr);
      break;
   case ARF:
      switch (inst->dst.nr & 0xF0) {
      case BRW_ARF_NULL:
         fprintf(file, "null");
         break;
      case BRW_ARF_ADDRESS:
         fprintf(file, "a0.%d", inst->dst.subnr);
         break;
      case BRW_ARF_ACCUMULATOR:
         if (inst->dst.subnr == 0)
            fprintf(file, "acc%d", inst->dst.nr & 0x0F);
         else
            fprintf(file, "acc%d.%d", inst->dst.nr & 0x0F, inst->dst.subnr);

         break;
      case BRW_ARF_FLAG:
         fprintf(file, "f%d.%d", inst->dst.nr & 0xf, inst->dst.subnr);
         break;
      default:
         fprintf(file, "arf%d.%d", inst->dst.nr & 0xf, inst->dst.subnr);
         break;
      }
      break;
   case IMM:
      unreachable("not reached");
   }

   if (inst->dst.offset ||
       (inst->dst.file == VGRF &&
        alloc.sizes[inst->dst.nr] * REG_SIZE != inst->size_written)) {
      const unsigned reg_size = (inst->dst.file == UNIFORM ? 4 : REG_SIZE);
      fprintf(file, "+%d.%d", inst->dst.offset / reg_size,
              inst->dst.offset % reg_size);
   }

   if (inst->dst.stride != 1)
      fprintf(file, "<%u>", inst->dst.stride);
   fprintf(file, ":%s, ", brw_reg_type_to_letters(inst->dst.type));

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].negate)
         fprintf(file, "-");
      if (inst->src[i].abs)
         fprintf(file, "|");
      switch (inst->src[i].file) {
      case VGRF:
         fprintf(file, "vgrf%d", inst->src[i].nr);
         break;
      case FIXED_GRF:
         fprintf(file, "g%d", inst->src[i].nr);
         break;
      case ATTR:
         fprintf(file, "attr%d", inst->src[i].nr);
         break;
      case UNIFORM:
         fprintf(file, "u%d", inst->src[i].nr);
         break;
      case BAD_FILE:
         fprintf(file, "(null)");
         break;
      case IMM:
         switch (inst->src[i].type) {
         case BRW_REGISTER_TYPE_HF:
            fprintf(file, "%-ghf", _mesa_half_to_float(inst->src[i].ud & 0xffff));
            break;
         case BRW_REGISTER_TYPE_F:
            fprintf(file, "%-gf", inst->src[i].f);
            break;
         case BRW_REGISTER_TYPE_DF:
            fprintf(file, "%fdf", inst->src[i].df);
            break;
         case BRW_REGISTER_TYPE_W:
         case BRW_REGISTER_TYPE_D:
            fprintf(file, "%dd", inst->src[i].d);
            break;
         case BRW_REGISTER_TYPE_UW:
         case BRW_REGISTER_TYPE_UD:
            fprintf(file, "%uu", inst->src[i].ud);
            break;
         case BRW_REGISTER_TYPE_Q:
            fprintf(file, "%" PRId64 "q", inst->src[i].d64);
            break;
         case BRW_REGISTER_TYPE_UQ:
            fprintf(file, "%" PRIu64 "uq", inst->src[i].u64);
            break;
         case BRW_REGISTER_TYPE_VF:
            fprintf(file, "[%-gF, %-gF, %-gF, %-gF]",
                    brw_vf_to_float((inst->src[i].ud >>  0) & 0xff),
                    brw_vf_to_float((inst->src[i].ud >>  8) & 0xff),
                    brw_vf_to_float((inst->src[i].ud >> 16) & 0xff),
                    brw_vf_to_float((inst->src[i].ud >> 24) & 0xff));
            break;
         case BRW_REGISTER_TYPE_V:
         case BRW_REGISTER_TYPE_UV:
            fprintf(file, "%08x%s", inst->src[i].ud,
                    inst->src[i].type == BRW_REGISTER_TYPE_V ? "V" : "UV");
            break;
         default:
            fprintf(file, "???");
            break;
         }
         break;
      case ARF:
         switch (inst->src[i].nr & 0xF0) {
         case BRW_ARF_NULL:
            fprintf(file, "null");
            break;
         case BRW_ARF_ADDRESS:
            fprintf(file, "a0.%d", inst->src[i].subnr);
            break;
         case BRW_ARF_ACCUMULATOR:
            if (inst->src[i].subnr == 0)
               fprintf(file, "acc%d", inst->src[i].nr & 0x0F);
            else
               fprintf(file, "acc%d.%d", inst->src[i].nr & 0x0F, inst->src[i].subnr);

            break;
         case BRW_ARF_FLAG:
            fprintf(file, "f%d.%d", inst->src[i].nr & 0xf, inst->src[i].subnr);
            break;
         default:
            fprintf(file, "arf%d.%d", inst->src[i].nr & 0xf, inst->src[i].subnr);
            break;
         }
         break;
      }

      if (inst->src[i].file == FIXED_GRF && inst->src[i].subnr != 0) {
         assert(inst->src[i].offset == 0);

         fprintf(file, ".%d", inst->src[i].subnr / type_sz(inst->src[i].type));
      } else if (inst->src[i].offset ||
          (inst->src[i].file == VGRF &&
           alloc.sizes[inst->src[i].nr] * REG_SIZE != inst->size_read(i))) {
         const unsigned reg_size = (inst->src[i].file == UNIFORM ? 4 : REG_SIZE);
         fprintf(file, "+%d.%d", inst->src[i].offset / reg_size,
                 inst->src[i].offset % reg_size);
      }

      if (inst->src[i].abs)
         fprintf(file, "|");

      if (inst->src[i].file != IMM) {
         unsigned stride;
         if (inst->src[i].file == ARF || inst->src[i].file == FIXED_GRF) {
            unsigned hstride = inst->src[i].hstride;
            stride = (hstride == 0 ? 0 : (1 << (hstride - 1)));
         } else {
            stride = inst->src[i].stride;
         }
         if (stride != 1)
            fprintf(file, "<%u>", stride);

         fprintf(file, ":%s", brw_reg_type_to_letters(inst->src[i].type));
      }

      if (i < inst->sources - 1 && inst->src[i + 1].file != BAD_FILE)
         fprintf(file, ", ");
   }

   fprintf(file, " ");

   if (inst->force_writemask_all)
      fprintf(file, "NoMask ");

   if (inst->exec_size != dispatch_width)
      fprintf(file, "group%d ", inst->group);

   fprintf(file, "\n");
}

brw::register_pressure::register_pressure(const fs_visitor *v)
{
   const fs_live_variables &live = v->live_analysis.require();
   const unsigned num_instructions = v->cfg->num_blocks ?
      v->cfg->blocks[v->cfg->num_blocks - 1]->end_ip + 1 : 0;

   regs_live_at_ip = new unsigned[num_instructions]();

   for (unsigned reg = 0; reg < v->alloc.count; reg++) {
      for (int ip = live.vgrf_start[reg]; ip <= live.vgrf_end[reg]; ip++)
         regs_live_at_ip[ip] += v->alloc.sizes[reg];
   }

   const unsigned payload_count = v->first_non_payload_grf;

   int *payload_last_use_ip = new int[payload_count];
   v->calculate_payload_ranges(payload_count, payload_last_use_ip);

   for (unsigned reg = 0; reg < payload_count; reg++) {
      for (int ip = 0; ip < payload_last_use_ip[reg]; ip++)
         ++regs_live_at_ip[ip];
   }

   delete[] payload_last_use_ip;
}

brw::register_pressure::~register_pressure()
{
   delete[] regs_live_at_ip;
}

void
fs_visitor::invalidate_analysis(brw::analysis_dependency_class c)
{
   live_analysis.invalidate(c);
   regpressure_analysis.invalidate(c);
   idom_analysis.invalidate(c);
}

void
fs_visitor::debug_optimizer(const nir_shader *nir,
                            const char *pass_name,
                            int iteration, int pass_num) const
{
   if (!brw_should_print_shader(nir, DEBUG_OPTIMIZER))
      return;

   char *filename;
   int ret = asprintf(&filename, "%s/%s%d-%s-%02d-%02d-%s",
                      debug_get_option("INTEL_SHADER_OPTIMIZER_PATH", "./"),
                      _mesa_shader_stage_to_abbrev(stage), dispatch_width, nir->info.name,
                      iteration, pass_num, pass_name);
   if (ret == -1)
      return;
   dump_instructions(filename);
   free(filename);
}

uint32_t
fs_visitor::compute_max_register_pressure()
{
   const register_pressure &rp = regpressure_analysis.require();
   uint32_t ip = 0, max_pressure = 0;
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      max_pressure = MAX2(max_pressure, rp.regs_live_at_ip[ip]);
      ip++;
   }
   return max_pressure;
}

static fs_inst **
save_instruction_order(const struct cfg_t *cfg)
{
   /* Before we schedule anything, stash off the instruction order as an array
    * of fs_inst *.  This way, we can reset it between scheduling passes to
    * prevent dependencies between the different scheduling modes.
    */
   int num_insts = cfg->last_block()->end_ip + 1;
   fs_inst **inst_arr = new fs_inst * [num_insts];

   int ip = 0;
   foreach_block_and_inst(block, fs_inst, inst, cfg) {
      assert(ip >= block->start_ip && ip <= block->end_ip);
      inst_arr[ip++] = inst;
   }
   assert(ip == num_insts);

   return inst_arr;
}

static void
restore_instruction_order(struct cfg_t *cfg, fs_inst **inst_arr)
{
   ASSERTED int num_insts = cfg->last_block()->end_ip + 1;

   int ip = 0;
   foreach_block (block, cfg) {
      block->instructions.make_empty();

      assert(ip == block->start_ip);
      for (; ip <= block->end_ip; ip++)
         block->instructions.push_tail(inst_arr[ip]);
   }
   assert(ip == num_insts);
}

/* Per-thread scratch space is a power-of-two multiple of 1KB. */
static inline unsigned
brw_get_scratch_size(int size)
{
   return MAX2(1024, util_next_power_of_two(size));
}

void
fs_visitor::allocate_registers(bool allow_spilling)
{
   bool allocated;

   static const enum instruction_scheduler_mode pre_modes[] = {
      SCHEDULE_PRE,
      SCHEDULE_PRE_NON_LIFO,
      SCHEDULE_NONE,
      SCHEDULE_PRE_LIFO,
   };

   static const char *scheduler_mode_name[] = {
      [SCHEDULE_PRE] = "top-down",
      [SCHEDULE_PRE_NON_LIFO] = "non-lifo",
      [SCHEDULE_PRE_LIFO] = "lifo",
      [SCHEDULE_POST] = "post",
      [SCHEDULE_NONE] = "none",
   };

   uint32_t best_register_pressure = UINT32_MAX;
   enum instruction_scheduler_mode best_sched = SCHEDULE_NONE;

   brw_fs_opt_compact_virtual_grfs(*this);

   if (needs_register_pressure)
      shader_stats.max_register_pressure = compute_max_register_pressure();

   debug_optimizer(nir, "pre_register_allocate", 90, 90);

   bool spill_all = allow_spilling && INTEL_DEBUG(DEBUG_SPILL_FS);

   /* Before we schedule anything, stash off the instruction order as an array
    * of fs_inst *.  This way, we can reset it between scheduling passes to
    * prevent dependencies between the different scheduling modes.
    */
   fs_inst **orig_order = save_instruction_order(cfg);
   fs_inst **best_pressure_order = NULL;

   void *scheduler_ctx = ralloc_context(NULL);
   instruction_scheduler *sched = prepare_scheduler(scheduler_ctx);

   /* Try each scheduling heuristic to see if it can successfully register
    * allocate without spilling.  They should be ordered by decreasing
    * performance but increasing likelihood of allocating.
    */
   for (unsigned i = 0; i < ARRAY_SIZE(pre_modes); i++) {
      enum instruction_scheduler_mode sched_mode = pre_modes[i];

      schedule_instructions_pre_ra(sched, sched_mode);
      this->shader_stats.scheduler_mode = scheduler_mode_name[sched_mode];

      debug_optimizer(nir, shader_stats.scheduler_mode, 95, i);

      if (0) {
         assign_regs_trivial();
         allocated = true;
         break;
      }

      /* We should only spill registers on the last scheduling. */
      assert(!spilled_any_registers);

      allocated = assign_regs(false, spill_all);
      if (allocated)
         break;

      /* Save the maximum register pressure */
      uint32_t this_pressure = compute_max_register_pressure();

      if (0) {
         fprintf(stderr, "Scheduler mode \"%s\" spilled, max pressure = %u\n",
                 scheduler_mode_name[sched_mode], this_pressure);
      }

      if (this_pressure < best_register_pressure) {
         best_register_pressure = this_pressure;
         best_sched = sched_mode;
         delete[] best_pressure_order;
         best_pressure_order = save_instruction_order(cfg);
      }

      /* Reset back to the original order before trying the next mode */
      restore_instruction_order(cfg, orig_order);
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);
   }

   ralloc_free(scheduler_ctx);

   if (!allocated) {
      if (0) {
         fprintf(stderr, "Spilling - using lowest-pressure mode \"%s\"\n",
                 scheduler_mode_name[best_sched]);
      }
      restore_instruction_order(cfg, best_pressure_order);
      shader_stats.scheduler_mode = scheduler_mode_name[best_sched];

      allocated = assign_regs(allow_spilling, spill_all);
   }

   delete[] orig_order;
   delete[] best_pressure_order;

   if (!allocated) {
      fail("Failure to register allocate.  Reduce number of "
           "live scalar values to avoid this.");
   } else if (spilled_any_registers) {
      brw_shader_perf_log(compiler, log_data,
                          "%s shader triggered register spilling.  "
                          "Try reducing the number of live scalar "
                          "values to improve performance.\n",
                          _mesa_shader_stage_to_string(stage));
   }

   if (failed)
      return;

   debug_optimizer(nir, "post_ra_alloc", 96, 0);

   brw_fs_opt_bank_conflicts(*this);

   debug_optimizer(nir, "bank_conflict", 96, 1);

   schedule_instructions_post_ra();

   debug_optimizer(nir, "post_ra_alloc_scheduling", 96, 2);

   /* Lowering VGRF to FIXED_GRF is currently done as a separate pass instead
    * of part of assign_regs since both bank conflicts optimization and post
    * RA scheduling take advantage of distinguishing references to registers
    * that were allocated from references that were already fixed.
    *
    * TODO: Change the passes above, then move this lowering to be part of
    * assign_regs.
    */
   brw_fs_lower_vgrfs_to_fixed_grfs(*this);

   debug_optimizer(nir, "lowered_vgrfs_to_fixed_grfs", 96, 3);

   if (last_scratch > 0) {
      ASSERTED unsigned max_scratch_size = 2 * 1024 * 1024;

      /* Take the max of any previously compiled variant of the shader. In the
       * case of bindless shaders with return parts, this will also take the
       * max of all parts.
       */
      prog_data->total_scratch = MAX2(brw_get_scratch_size(last_scratch),
                                      prog_data->total_scratch);

      /* We currently only support up to 2MB of scratch space.  If we
       * need to support more eventually, the documentation suggests
       * that we could allocate a larger buffer, and partition it out
       * ourselves.  We'd just have to undo the hardware's address
       * calculation by subtracting (FFTID * Per Thread Scratch Space)
       * and then add FFTID * (Larger Per Thread Scratch Space).
       *
       * See 3D-Media-GPGPU Engine > Media GPGPU Pipeline >
       * Thread Group Tracking > Local Memory/Scratch Space.
       */
      assert(prog_data->total_scratch < max_scratch_size);
   }

   brw_fs_lower_scoreboard(*this);
}

bool
fs_visitor::run_vs()
{
   assert(stage == MESA_SHADER_VERTEX);

   payload_ = new vs_thread_payload(*this);

   nir_to_brw(this);

   if (failed)
      return false;

   emit_urb_writes();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();
   assign_vs_urb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

void
fs_visitor::set_tcs_invocation_id()
{
   struct brw_tcs_prog_data *tcs_prog_data = brw_tcs_prog_data(prog_data);
   struct brw_vue_prog_data *vue_prog_data = &tcs_prog_data->base;
   const fs_builder bld = fs_builder(this).at_end();

   const unsigned instance_id_mask =
      (devinfo->verx10 >= 125) ? INTEL_MASK(7, 0) :
      (devinfo->ver >= 11)     ? INTEL_MASK(22, 16) :
                                 INTEL_MASK(23, 17);
   const unsigned instance_id_shift =
      (devinfo->verx10 >= 125) ? 0 : (devinfo->ver >= 11) ? 16 : 17;

   /* Get instance number from g0.2 bits:
    *  * 7:0 on DG2+
    *  * 22:16 on gfx11+
    *  * 23:17 otherwise
    */
   fs_reg t = bld.vgrf(BRW_REGISTER_TYPE_UD);
   bld.AND(t, fs_reg(retype(brw_vec1_grf(0, 2), BRW_REGISTER_TYPE_UD)),
           brw_imm_ud(instance_id_mask));

   invocation_id = bld.vgrf(BRW_REGISTER_TYPE_UD);

   if (vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_MULTI_PATCH) {
      /* gl_InvocationID is just the thread number */
      bld.SHR(invocation_id, t, brw_imm_ud(instance_id_shift));
      return;
   }

   assert(vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH);

   fs_reg channels_uw = bld.vgrf(BRW_REGISTER_TYPE_UW);
   fs_reg channels_ud = bld.vgrf(BRW_REGISTER_TYPE_UD);
   bld.MOV(channels_uw, fs_reg(brw_imm_uv(0x76543210)));
   bld.MOV(channels_ud, channels_uw);

   if (tcs_prog_data->instances == 1) {
      invocation_id = channels_ud;
   } else {
      fs_reg instance_times_8 = bld.vgrf(BRW_REGISTER_TYPE_UD);
      bld.SHR(instance_times_8, t, brw_imm_ud(instance_id_shift - 3));
      bld.ADD(invocation_id, instance_times_8, channels_ud);
   }
}

void
fs_visitor::emit_tcs_thread_end()
{
   /* Try and tag the last URB write with EOT instead of emitting a whole
    * separate write just to finish the thread.  There isn't guaranteed to
    * be one, so this may not succeed.
    */
   if (mark_last_urb_write_with_eot())
      return;

   const fs_builder bld = fs_builder(this).at_end();

   /* Emit a URB write to end the thread.  On Broadwell, we use this to write
    * zero to the "TR DS Cache Disable" bit (we haven't implemented a fancy
    * algorithm to set it optimally).  On other platforms, we simply write
    * zero to a reserved/MBZ patch header DWord which has no consequence.
    */
   fs_reg srcs[URB_LOGICAL_NUM_SRCS];
   srcs[URB_LOGICAL_SRC_HANDLE] = tcs_payload().patch_urb_output;
   srcs[URB_LOGICAL_SRC_CHANNEL_MASK] = brw_imm_ud(WRITEMASK_X << 16);
   srcs[URB_LOGICAL_SRC_DATA] = brw_imm_ud(0);
   srcs[URB_LOGICAL_SRC_COMPONENTS] = brw_imm_ud(1);
   fs_inst *inst = bld.emit(SHADER_OPCODE_URB_WRITE_LOGICAL,
                            reg_undef, srcs, ARRAY_SIZE(srcs));
   inst->eot = true;
}

bool
fs_visitor::run_tcs()
{
   assert(stage == MESA_SHADER_TESS_CTRL);

   struct brw_vue_prog_data *vue_prog_data = brw_vue_prog_data(prog_data);
   const fs_builder bld = fs_builder(this).at_end();

   assert(vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH ||
          vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_MULTI_PATCH);

   payload_ = new tcs_thread_payload(*this);

   /* Initialize gl_InvocationID */
   set_tcs_invocation_id();

   const bool fix_dispatch_mask =
      vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH &&
      (nir->info.tess.tcs_vertices_out % 8) != 0;

   /* Fix the disptach mask */
   if (fix_dispatch_mask) {
      bld.CMP(bld.null_reg_ud(), invocation_id,
              brw_imm_ud(nir->info.tess.tcs_vertices_out), BRW_CONDITIONAL_L);
      bld.IF(BRW_PREDICATE_NORMAL);
   }

   nir_to_brw(this);

   if (fix_dispatch_mask) {
      bld.emit(BRW_OPCODE_ENDIF);
   }

   emit_tcs_thread_end();

   if (failed)
      return false;

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();
   assign_tcs_urb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

bool
fs_visitor::run_tes()
{
   assert(stage == MESA_SHADER_TESS_EVAL);

   payload_ = new tes_thread_payload(*this);

   nir_to_brw(this);

   if (failed)
      return false;

   emit_urb_writes();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();
   assign_tes_urb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

bool
fs_visitor::run_gs()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   payload_ = new gs_thread_payload(*this);

   const fs_builder bld = fs_builder(this).at_end();

   this->final_gs_vertex_count = bld.vgrf(BRW_REGISTER_TYPE_UD);

   if (gs_compile->control_data_header_size_bits > 0) {
      /* Create a VGRF to store accumulated control data bits. */
      this->control_data_bits = bld.vgrf(BRW_REGISTER_TYPE_UD);

      /* If we're outputting more than 32 control data bits, then EmitVertex()
       * will set control_data_bits to 0 after emitting the first vertex.
       * Otherwise, we need to initialize it to 0 here.
       */
      if (gs_compile->control_data_header_size_bits <= 32) {
         const fs_builder abld = bld.annotate("initialize control data bits");
         abld.MOV(this->control_data_bits, brw_imm_ud(0u));
      }
   }

   nir_to_brw(this);

   emit_gs_thread_end();

   if (failed)
      return false;

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();
   assign_gs_urb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

/* From the SKL PRM, Volume 16, Workarounds:
 *
 *   0877  3D   Pixel Shader Hang possible when pixel shader dispatched with
 *              only header phases (R0-R2)
 *
 *   WA: Enable a non-header phase (e.g. push constant) when dispatch would
 *       have been header only.
 *
 * Instead of enabling push constants one can alternatively enable one of the
 * inputs. Here one simply chooses "layer" which shouldn't impose much
 * overhead.
 */
static void
gfx9_ps_header_only_workaround(struct brw_wm_prog_data *wm_prog_data)
{
   if (wm_prog_data->num_varying_inputs)
      return;

   if (wm_prog_data->base.curb_read_length)
      return;

   wm_prog_data->urb_setup[VARYING_SLOT_LAYER] = 0;
   wm_prog_data->num_varying_inputs = 1;

   brw_compute_urb_setup_index(wm_prog_data);
}

bool
fs_visitor::run_fs(bool allow_spilling, bool do_rep_send)
{
   struct brw_wm_prog_data *wm_prog_data = brw_wm_prog_data(this->prog_data);
   brw_wm_prog_key *wm_key = (brw_wm_prog_key *) this->key;
   const fs_builder bld = fs_builder(this).at_end();

   assert(stage == MESA_SHADER_FRAGMENT);

   payload_ = new fs_thread_payload(*this, source_depth_to_render_target);

   if (nir->info.ray_queries > 0)
      limit_dispatch_width(16, "SIMD32 not supported with ray queries.\n");

   if (do_rep_send) {
      assert(dispatch_width == 16);
      emit_repclear_shader();
   } else {
      if (nir->info.inputs_read > 0 ||
          BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_FRAG_COORD) ||
          (nir->info.outputs_read > 0 && !wm_key->coherent_fb_fetch)) {
         emit_interpolation_setup();
      }

      /* We handle discards by keeping track of the still-live pixels in f0.1.
       * Initialize it with the dispatched pixels.
       */
      if (wm_prog_data->uses_kill) {
         const unsigned lower_width = MIN2(dispatch_width, 16);
         for (unsigned i = 0; i < dispatch_width / lower_width; i++) {
            /* According to the "PS Thread Payload for Normal
             * Dispatch" pages on the BSpec, the dispatch mask is
             * stored in R0.15/R1.15 on gfx20+ and in R1.7/R2.7 on
             * gfx6+.
             */
            const fs_reg dispatch_mask =
               devinfo->ver >= 20 ? xe2_vec1_grf(i, 15) :
                                    brw_vec1_grf(i + 1, 7);
            bld.exec_all().group(1, 0)
               .MOV(brw_sample_mask_reg(bld.group(lower_width, i)),
                    retype(dispatch_mask, BRW_REGISTER_TYPE_UW));
         }
      }

      if (nir->info.writes_memory)
         wm_prog_data->has_side_effects = true;

      nir_to_brw(this);

      if (failed)
	 return false;

      emit_fb_writes();

      calculate_cfg();

      brw_fs_optimize(*this);

      assign_curb_setup();

      if (devinfo->ver == 9)
         gfx9_ps_header_only_workaround(wm_prog_data);

      assign_urb_setup();

      brw_fs_lower_3src_null_dest(*this);
      brw_fs_workaround_memory_fence_before_eot(*this);
      brw_fs_workaround_emit_dummy_mov_instruction(*this);

      allocate_registers(allow_spilling);
   }

   return !failed;
}

bool
fs_visitor::run_cs(bool allow_spilling)
{
   assert(gl_shader_stage_is_compute(stage));
   const fs_builder bld = fs_builder(this).at_end();

   payload_ = new cs_thread_payload(*this);

   if (devinfo->platform == INTEL_PLATFORM_HSW && prog_data->total_shared > 0) {
      /* Move SLM index from g0.0[27:24] to sr0.1[11:8] */
      const fs_builder abld = bld.exec_all().group(1, 0);
      abld.MOV(retype(brw_sr0_reg(1), BRW_REGISTER_TYPE_UW),
               suboffset(retype(brw_vec1_grf(0, 0), BRW_REGISTER_TYPE_UW), 1));
   }

   nir_to_brw(this);

   if (failed)
      return false;

   emit_cs_terminate();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(allow_spilling);

   return !failed;
}

bool
fs_visitor::run_bs(bool allow_spilling)
{
   assert(stage >= MESA_SHADER_RAYGEN && stage <= MESA_SHADER_CALLABLE);

   payload_ = new bs_thread_payload(*this);

   nir_to_brw(this);

   if (failed)
      return false;

   /* TODO(RT): Perhaps rename this? */
   emit_cs_terminate();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(allow_spilling);

   return !failed;
}

bool
fs_visitor::run_task(bool allow_spilling)
{
   assert(stage == MESA_SHADER_TASK);

   payload_ = new task_mesh_thread_payload(*this);

   nir_to_brw(this);

   if (failed)
      return false;

   emit_urb_fence();

   emit_cs_terminate();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(allow_spilling);

   return !failed;
}

bool
fs_visitor::run_mesh(bool allow_spilling)
{
   assert(stage == MESA_SHADER_MESH);

   payload_ = new task_mesh_thread_payload(*this);

   nir_to_brw(this);

   if (failed)
      return false;

   emit_urb_fence();

   emit_cs_terminate();

   calculate_cfg();

   brw_fs_optimize(*this);

   assign_curb_setup();

   brw_fs_lower_3src_null_dest(*this);
   brw_fs_workaround_memory_fence_before_eot(*this);
   brw_fs_workaround_emit_dummy_mov_instruction(*this);

   allocate_registers(allow_spilling);

   return !failed;
}

static bool
is_used_in_not_interp_frag_coord(nir_def *def)
{
   nir_foreach_use_including_if(src, def) {
      if (nir_src_is_if(src))
         return true;

      if (nir_src_parent_instr(src)->type != nir_instr_type_intrinsic)
         return true;

      nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(nir_src_parent_instr(src));
      if (intrin->intrinsic != nir_intrinsic_load_frag_coord)
         return true;
   }

   return false;
}

/**
 * Return a bitfield where bit n is set if barycentric interpolation mode n
 * (see enum brw_barycentric_mode) is needed by the fragment shader.
 *
 * We examine the load_barycentric intrinsics rather than looking at input
 * variables so that we catch interpolateAtCentroid() messages too, which
 * also need the BRW_BARYCENTRIC_[NON]PERSPECTIVE_CENTROID mode set up.
 */
static unsigned
brw_compute_barycentric_interp_modes(const struct intel_device_info *devinfo,
                                     const struct brw_wm_prog_key *key,
                                     const nir_shader *shader)
{
   unsigned barycentric_interp_modes = 0;

   nir_foreach_function_impl(impl, shader) {
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            switch (intrin->intrinsic) {
            case nir_intrinsic_load_barycentric_pixel:
            case nir_intrinsic_load_barycentric_centroid:
            case nir_intrinsic_load_barycentric_sample:
            case nir_intrinsic_load_barycentric_at_sample:
            case nir_intrinsic_load_barycentric_at_offset:
               break;
            default:
               continue;
            }

            /* Ignore WPOS; it doesn't require interpolation. */
            if (!is_used_in_not_interp_frag_coord(&intrin->def))
               continue;

            nir_intrinsic_op bary_op = intrin->intrinsic;
            enum brw_barycentric_mode bary =
               brw_barycentric_mode(key, intrin);

            barycentric_interp_modes |= 1 << bary;

            if (devinfo->needs_unlit_centroid_workaround &&
                bary_op == nir_intrinsic_load_barycentric_centroid)
               barycentric_interp_modes |= 1 << centroid_to_pixel(bary);
         }
      }
   }

   return barycentric_interp_modes;
}

static void
brw_compute_flat_inputs(struct brw_wm_prog_data *prog_data,
                        const nir_shader *shader)
{
   prog_data->flat_inputs = 0;

   nir_foreach_shader_in_variable(var, shader) {
      /* flat shading */
      if (var->data.interpolation != INTERP_MODE_FLAT)
         continue;

      if (var->data.per_primitive)
         continue;

      unsigned slots = glsl_count_attribute_slots(var->type, false);
      for (unsigned s = 0; s < slots; s++) {
         int input_index = prog_data->urb_setup[var->data.location + s];

         if (input_index >= 0)
            prog_data->flat_inputs |= 1 << input_index;
      }
   }
}

static uint8_t
computed_depth_mode(const nir_shader *shader)
{
   if (shader->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_DEPTH)) {
      switch (shader->info.fs.depth_layout) {
      case FRAG_DEPTH_LAYOUT_NONE:
      case FRAG_DEPTH_LAYOUT_ANY:
         return BRW_PSCDEPTH_ON;
      case FRAG_DEPTH_LAYOUT_GREATER:
         return BRW_PSCDEPTH_ON_GE;
      case FRAG_DEPTH_LAYOUT_LESS:
         return BRW_PSCDEPTH_ON_LE;
      case FRAG_DEPTH_LAYOUT_UNCHANGED:
         /* We initially set this to OFF, but having the shader write the
          * depth means we allocate register space in the SEND message. The
          * difference between the SEND register count and the OFF state
          * programming makes the HW hang.
          *
          * Removing the depth writes also leads to test failures. So use
          * LesserThanOrEqual, which fits writing the same value
          * (unchanged/equal).
          *
          */
         return BRW_PSCDEPTH_ON_LE;
      }
   }
   return BRW_PSCDEPTH_OFF;
}

/**
 * Move load_interpolated_input with simple (payload-based) barycentric modes
 * to the top of the program so we don't emit multiple PLNs for the same input.
 *
 * This works around CSE not being able to handle non-dominating cases
 * such as:
 *
 *    if (...) {
 *       interpolate input
 *    } else {
 *       interpolate the same exact input
 *    }
 *
 * This should be replaced by global value numbering someday.
 */
bool
brw_nir_move_interpolation_to_top(nir_shader *nir)
{
   bool progress = false;

   nir_foreach_function_impl(impl, nir) {
      nir_block *top = nir_start_block(impl);
      nir_cursor cursor = nir_before_instr(nir_block_first_instr(top));
      bool impl_progress = false;

      for (nir_block *block = nir_block_cf_tree_next(top);
           block != NULL;
           block = nir_block_cf_tree_next(block)) {

         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            if (intrin->intrinsic != nir_intrinsic_load_interpolated_input)
               continue;
            nir_intrinsic_instr *bary_intrinsic =
               nir_instr_as_intrinsic(intrin->src[0].ssa->parent_instr);
            nir_intrinsic_op op = bary_intrinsic->intrinsic;

            /* Leave interpolateAtSample/Offset() where they are. */
            if (op == nir_intrinsic_load_barycentric_at_sample ||
                op == nir_intrinsic_load_barycentric_at_offset)
               continue;

            nir_instr *move[3] = {
               &bary_intrinsic->instr,
               intrin->src[1].ssa->parent_instr,
               instr
            };

            for (unsigned i = 0; i < ARRAY_SIZE(move); i++) {
               if (move[i]->block != top) {
                  nir_instr_move(cursor, move[i]);
                  impl_progress = true;
               }
            }
         }
      }

      progress = progress || impl_progress;

      nir_metadata_preserve(impl, impl_progress ? (nir_metadata_block_index |
                                                      nir_metadata_dominance)
                                                   : nir_metadata_all);
   }

   return progress;
}

static void
brw_nir_populate_wm_prog_data(nir_shader *shader,
                              const struct intel_device_info *devinfo,
                              const struct brw_wm_prog_key *key,
                              struct brw_wm_prog_data *prog_data,
                              const struct brw_mue_map *mue_map)
{
   prog_data->uses_kill = shader->info.fs.uses_discard ||
                          shader->info.fs.uses_demote;
   prog_data->uses_omask = !key->ignore_sample_mask_out &&
      (shader->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_SAMPLE_MASK));
   prog_data->max_polygons = 1;
   prog_data->computed_depth_mode = computed_depth_mode(shader);
   prog_data->computed_stencil =
      shader->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_STENCIL);

   prog_data->sample_shading =
      shader->info.fs.uses_sample_shading ||
      shader->info.outputs_read;

   assert(key->multisample_fbo != BRW_NEVER ||
          key->persample_interp == BRW_NEVER);

   prog_data->persample_dispatch = key->persample_interp;
   if (prog_data->sample_shading)
      prog_data->persample_dispatch = BRW_ALWAYS;

   /* We can only persample dispatch if we have a multisample FBO */
   prog_data->persample_dispatch = MIN2(prog_data->persample_dispatch,
                                        key->multisample_fbo);

   /* Currently only the Vulkan API allows alpha_to_coverage to be dynamic. If
    * persample_dispatch & multisample_fbo are not dynamic, Anv should be able
    * to definitively tell whether alpha_to_coverage is on or off.
    */
   prog_data->alpha_to_coverage = key->alpha_to_coverage;
   assert(prog_data->alpha_to_coverage != BRW_SOMETIMES ||
          prog_data->persample_dispatch == BRW_SOMETIMES);

   prog_data->uses_sample_mask =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_SAMPLE_MASK_IN);

   /* From the Ivy Bridge PRM documentation for 3DSTATE_PS:
    *
    *    "MSDISPMODE_PERSAMPLE is required in order to select
    *    POSOFFSET_SAMPLE"
    *
    * So we can only really get sample positions if we are doing real
    * per-sample dispatch.  If we need gl_SamplePosition and we don't have
    * persample dispatch, we hard-code it to 0.5.
    */
   prog_data->uses_pos_offset =
      prog_data->persample_dispatch != BRW_NEVER &&
      (BITSET_TEST(shader->info.system_values_read,
                   SYSTEM_VALUE_SAMPLE_POS) ||
       BITSET_TEST(shader->info.system_values_read,
                   SYSTEM_VALUE_SAMPLE_POS_OR_CENTER));

   prog_data->early_fragment_tests = shader->info.fs.early_fragment_tests;
   prog_data->post_depth_coverage = shader->info.fs.post_depth_coverage;
   prog_data->inner_coverage = shader->info.fs.inner_coverage;

   prog_data->barycentric_interp_modes =
      brw_compute_barycentric_interp_modes(devinfo, key, shader);

   /* From the BDW PRM documentation for 3DSTATE_WM:
    *
    *    "MSDISPMODE_PERSAMPLE is required in order to select Perspective
    *     Sample or Non- perspective Sample barycentric coordinates."
    *
    * So cleanup any potentially set sample barycentric mode when not in per
    * sample dispatch.
    */
   if (prog_data->persample_dispatch == BRW_NEVER) {
      prog_data->barycentric_interp_modes &=
         ~BITFIELD_BIT(BRW_BARYCENTRIC_PERSPECTIVE_SAMPLE);
   }

   prog_data->uses_nonperspective_interp_modes |=
      (prog_data->barycentric_interp_modes &
      BRW_BARYCENTRIC_NONPERSPECTIVE_BITS) != 0;

   /* The current VK_EXT_graphics_pipeline_library specification requires
    * coarse to specified at compile time. But per sample interpolation can be
    * dynamic. So we should never be in a situation where coarse &
    * persample_interp are both respectively true & BRW_ALWAYS.
    *
    * Coarse will dynamically turned off when persample_interp is active.
    */
   assert(!key->coarse_pixel || key->persample_interp != BRW_ALWAYS);

   prog_data->coarse_pixel_dispatch =
      brw_sometimes_invert(prog_data->persample_dispatch);
   if (!key->coarse_pixel ||
       prog_data->uses_omask ||
       prog_data->sample_shading ||
       prog_data->uses_sample_mask ||
       (prog_data->computed_depth_mode != BRW_PSCDEPTH_OFF) ||
       prog_data->computed_stencil) {
      prog_data->coarse_pixel_dispatch = BRW_NEVER;
   }

   /* ICL PRMs, Volume 9: Render Engine, Shared Functions Pixel Interpolater,
    * Message Descriptor :
    *
    *    "Message Type. Specifies the type of message being sent when
    *     pixel-rate evaluation is requested :
    *
    *     Format = U2
    *       0: Per Message Offset (eval_snapped with immediate offset)
    *       1: Sample Position Offset (eval_sindex)
    *       2: Centroid Position Offset (eval_centroid)
    *       3: Per Slot Offset (eval_snapped with register offset)
    *
    *     Message Type. Specifies the type of message being sent when
    *     coarse-rate evaluation is requested :
    *
    *     Format = U2
    *       0: Coarse to Pixel Mapping Message (internal message)
    *       1: Reserved
    *       2: Coarse Centroid Position (eval_centroid)
    *       3: Per Slot Coarse Pixel Offset (eval_snapped with register offset)"
    *
    * The Sample Position Offset is marked as reserved for coarse rate
    * evaluation and leads to hangs if we try to use it. So disable coarse
    * pixel shading if we have any intrinsic that will result in a pixel
    * interpolater message at sample.
    */
   if (intel_nir_pulls_at_sample(shader))
      prog_data->coarse_pixel_dispatch = BRW_NEVER;

   /* We choose to always enable VMask prior to XeHP, as it would cause
    * us to lose out on the eliminate_find_live_channel() optimization.
    */
   prog_data->uses_vmask = devinfo->verx10 < 125 ||
                           shader->info.fs.needs_quad_helper_invocations ||
                           shader->info.uses_wide_subgroup_intrinsics ||
                           prog_data->coarse_pixel_dispatch != BRW_NEVER;

   prog_data->uses_src_w =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_FRAG_COORD);
   prog_data->uses_src_depth =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_FRAG_COORD) &&
      prog_data->coarse_pixel_dispatch != BRW_ALWAYS;
   prog_data->uses_depth_w_coefficients =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_FRAG_COORD) &&
      prog_data->coarse_pixel_dispatch != BRW_NEVER;

   calculate_urb_setup(devinfo, key, prog_data, shader, mue_map);
   brw_compute_flat_inputs(prog_data, shader);
}

const unsigned *
brw_compile_fs(const struct brw_compiler *compiler,
               struct brw_compile_fs_params *params)
{
   struct nir_shader *nir = params->base.nir;
   const struct brw_wm_prog_key *key = params->key;
   struct brw_wm_prog_data *prog_data = params->prog_data;
   bool allow_spilling = params->allow_spilling;
   const bool debug_enabled =
      brw_should_print_shader(nir, params->base.debug_flag ?
                                   params->base.debug_flag : DEBUG_WM);

   prog_data->base.stage = MESA_SHADER_FRAGMENT;
   prog_data->base.ray_queries = nir->info.ray_queries;
   prog_data->base.total_scratch = 0;

   const struct intel_device_info *devinfo = compiler->devinfo;
   const unsigned max_subgroup_size = 32;

   brw_nir_apply_key(nir, compiler, &key->base, max_subgroup_size);
   brw_nir_lower_fs_inputs(nir, devinfo, key);
   brw_nir_lower_fs_outputs(nir);

   /* From the SKL PRM, Volume 7, "Alpha Coverage":
    *  "If Pixel Shader outputs oMask, AlphaToCoverage is disabled in
    *   hardware, regardless of the state setting for this feature."
    */
   if (key->alpha_to_coverage != BRW_NEVER) {
      /* Run constant fold optimization in order to get the correct source
       * offset to determine render target 0 store instruction in
       * emit_alpha_to_coverage pass.
       */
      NIR_PASS(_, nir, nir_opt_constant_folding);
      NIR_PASS(_, nir, brw_nir_lower_alpha_to_coverage, key, prog_data);
   }

   NIR_PASS(_, nir, brw_nir_move_interpolation_to_top);
   brw_postprocess_nir(nir, compiler, debug_enabled,
                       key->base.robust_flags);

   brw_nir_populate_wm_prog_data(nir, compiler->devinfo, key, prog_data,
                                 params->mue_map);

   std::unique_ptr<fs_visitor> v8, v16, v32, vmulti;
   cfg_t *simd8_cfg = NULL, *simd16_cfg = NULL, *simd32_cfg = NULL,
      *multi_cfg = NULL;
   float throughput = 0;
   bool has_spilled = false;

   if (devinfo->ver < 20) {
      v8 = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                        prog_data, nir, 8, 1,
                                        params->base.stats != NULL,
                                        debug_enabled);
      if (!v8->run_fs(allow_spilling, false /* do_rep_send */)) {
         params->base.error_str = ralloc_strdup(params->base.mem_ctx,
                                                v8->fail_msg);
         return NULL;
      } else if (INTEL_SIMD(FS, 8)) {
         simd8_cfg = v8->cfg;

         assert(v8->payload().num_regs % reg_unit(devinfo) == 0);
         prog_data->base.dispatch_grf_start_reg = v8->payload().num_regs / reg_unit(devinfo);

         const performance &perf = v8->performance_analysis.require();
         throughput = MAX2(throughput, perf.throughput);
         has_spilled = v8->spilled_any_registers;
         allow_spilling = false;
      }
   }

   if (key->coarse_pixel && devinfo->ver < 20) {
      if (prog_data->dual_src_blend) {
         v8->limit_dispatch_width(8, "SIMD16 coarse pixel shading cannot"
                                  " use SIMD8 messages.\n");
      }
      v8->limit_dispatch_width(16, "SIMD32 not supported with coarse"
                               " pixel shading.\n");
   }

   if (!has_spilled &&
       (!v8 || v8->max_dispatch_width >= 16) &&
       (INTEL_SIMD(FS, 16) || params->use_rep_send)) {
      /* Try a SIMD16 compile */
      v16 = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                         prog_data, nir, 16, 1,
                                         params->base.stats != NULL,
                                         debug_enabled);
      if (v8)
         v16->import_uniforms(v8.get());
      if (!v16->run_fs(allow_spilling, params->use_rep_send)) {
         brw_shader_perf_log(compiler, params->base.log_data,
                             "SIMD16 shader failed to compile: %s\n",
                             v16->fail_msg);
      } else {
         simd16_cfg = v16->cfg;

         assert(v16->payload().num_regs % reg_unit(devinfo) == 0);
         prog_data->dispatch_grf_start_reg_16 = v16->payload().num_regs / reg_unit(devinfo);

         const performance &perf = v16->performance_analysis.require();
         throughput = MAX2(throughput, perf.throughput);
         has_spilled = v16->spilled_any_registers;
         allow_spilling = false;
      }
   }

   const bool simd16_failed = v16 && !simd16_cfg;

   /* Currently, the compiler only supports SIMD32 on SNB+ */
   if (!has_spilled &&
       (!v8 || v8->max_dispatch_width >= 32) &&
       (!v16 || v16->max_dispatch_width >= 32) && !params->use_rep_send &&
       !simd16_failed &&
       INTEL_SIMD(FS, 32)) {
      /* Try a SIMD32 compile */
      v32 = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                         prog_data, nir, 32, 1,
                                         params->base.stats != NULL,
                                         debug_enabled);
      if (v8)
         v32->import_uniforms(v8.get());
      else if (v16)
         v32->import_uniforms(v16.get());

      if (!v32->run_fs(allow_spilling, false)) {
         brw_shader_perf_log(compiler, params->base.log_data,
                             "SIMD32 shader failed to compile: %s\n",
                             v32->fail_msg);
      } else {
         const performance &perf = v32->performance_analysis.require();

         if (!INTEL_DEBUG(DEBUG_DO32) && throughput >= perf.throughput) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "SIMD32 shader inefficient\n");
         } else {
            simd32_cfg = v32->cfg;

            assert(v32->payload().num_regs % reg_unit(devinfo) == 0);
            prog_data->dispatch_grf_start_reg_32 = v32->payload().num_regs / reg_unit(devinfo);

            throughput = MAX2(throughput, perf.throughput);
         }
      }
   }

   if (devinfo->ver >= 12 && !has_spilled &&
       params->max_polygons >= 2 && !key->coarse_pixel) {
      fs_visitor *vbase = v8 ? v8.get() : v16 ? v16.get() : v32.get();
      assert(vbase);

      if (devinfo->ver >= 20 &&
          params->max_polygons >= 4 &&
          vbase->max_dispatch_width >= 32 &&
          4 * prog_data->num_varying_inputs <= MAX_VARYING &&
          INTEL_SIMD(FS, 4X8)) {
         /* Try a quad-SIMD8 compile */
         vmulti = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                               prog_data, nir, 32, 4,
                                               params->base.stats != NULL,
                                               debug_enabled);
         vmulti->import_uniforms(vbase);
         if (!vmulti->run_fs(false, params->use_rep_send)) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "Quad-SIMD8 shader failed to compile: %s\n",
                                vmulti->fail_msg);
         } else {
            multi_cfg = vmulti->cfg;
            assert(!vmulti->spilled_any_registers);
         }
      }

      if (!multi_cfg && devinfo->ver >= 20 &&
          vbase->max_dispatch_width >= 32 &&
          2 * prog_data->num_varying_inputs <= MAX_VARYING &&
          INTEL_SIMD(FS, 2X16)) {
         /* Try a dual-SIMD16 compile */
         vmulti = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                               prog_data, nir, 32, 2,
                                               params->base.stats != NULL,
                                               debug_enabled);
         vmulti->import_uniforms(vbase);
         if (!vmulti->run_fs(false, params->use_rep_send)) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "Dual-SIMD16 shader failed to compile: %s\n",
                                vmulti->fail_msg);
         } else {
            multi_cfg = vmulti->cfg;
            assert(!vmulti->spilled_any_registers);
         }
      }

      if (!multi_cfg && vbase->max_dispatch_width >= 16 &&
          2 * prog_data->num_varying_inputs <= MAX_VARYING &&
          INTEL_SIMD(FS, 2X8)) {
         /* Try a dual-SIMD8 compile */
         vmulti = std::make_unique<fs_visitor>(compiler, &params->base, key,
                                               prog_data, nir, 16, 2,
                                               params->base.stats != NULL,
                                               debug_enabled);
         vmulti->import_uniforms(vbase);
         if (!vmulti->run_fs(allow_spilling, params->use_rep_send)) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "Dual-SIMD8 shader failed to compile: %s\n",
                                vmulti->fail_msg);
         } else {
            multi_cfg = vmulti->cfg;
         }
      }

      if (multi_cfg) {
         assert(vmulti->payload().num_regs % reg_unit(devinfo) == 0);
         prog_data->base.dispatch_grf_start_reg = vmulti->payload().num_regs / reg_unit(devinfo);
      }
   }

   /* When the caller requests a repclear shader, they want SIMD16-only */
   if (params->use_rep_send)
      simd8_cfg = NULL;

   fs_generator g(compiler, &params->base, &prog_data->base,
                  MESA_SHADER_FRAGMENT);

   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(params->base.mem_ctx,
                                     "%s fragment shader %s",
                                     nir->info.label ?
                                        nir->info.label : "unnamed",
                                     nir->info.name));
   }

   struct brw_compile_stats *stats = params->base.stats;
   uint32_t max_dispatch_width = 0;

   if (multi_cfg) {
      prog_data->dispatch_multi = vmulti->dispatch_width;
      prog_data->max_polygons = vmulti->max_polygons;
      g.generate_code(multi_cfg, vmulti->dispatch_width, vmulti->shader_stats,
                      vmulti->performance_analysis.require(),
                      stats, vmulti->max_polygons);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = vmulti->dispatch_width;

   } else if (simd8_cfg) {
      prog_data->dispatch_8 = true;
      g.generate_code(simd8_cfg, 8, v8->shader_stats,
                      v8->performance_analysis.require(), stats, 1);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 8;
   }

   if (simd16_cfg) {
      prog_data->dispatch_16 = true;
      prog_data->prog_offset_16 = g.generate_code(
         simd16_cfg, 16, v16->shader_stats,
         v16->performance_analysis.require(), stats, 1);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 16;
   }

   if (simd32_cfg) {
      prog_data->dispatch_32 = true;
      prog_data->prog_offset_32 = g.generate_code(
         simd32_cfg, 32, v32->shader_stats,
         v32->performance_analysis.require(), stats, 1);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 32;
   }

   for (struct brw_compile_stats *s = params->base.stats; s != NULL && s != stats; s++)
      s->max_dispatch_width = max_dispatch_width;

   g.add_const_data(nir->constant_data, nir->constant_data_size);
   return g.get_assembly();
}

unsigned
brw_cs_push_const_total_size(const struct brw_cs_prog_data *cs_prog_data,
                             unsigned threads)
{
   assert(cs_prog_data->push.per_thread.size % REG_SIZE == 0);
   assert(cs_prog_data->push.cross_thread.size % REG_SIZE == 0);
   return cs_prog_data->push.per_thread.size * threads +
          cs_prog_data->push.cross_thread.size;
}

static void
fill_push_const_block_info(struct brw_push_const_block *block, unsigned dwords)
{
   block->dwords = dwords;
   block->regs = DIV_ROUND_UP(dwords, 8);
   block->size = block->regs * 32;
}

static void
cs_fill_push_const_info(const struct intel_device_info *devinfo,
                        struct brw_cs_prog_data *cs_prog_data)
{
   const struct brw_stage_prog_data *prog_data = &cs_prog_data->base;
   int subgroup_id_index = brw_get_subgroup_id_param_index(devinfo, prog_data);

   /* The thread ID should be stored in the last param dword */
   assert(subgroup_id_index == -1 ||
          subgroup_id_index == (int)prog_data->nr_params - 1);

   unsigned cross_thread_dwords, per_thread_dwords;
   if (subgroup_id_index >= 0) {
      /* Fill all but the last register with cross-thread payload */
      cross_thread_dwords = 8 * (subgroup_id_index / 8);
      per_thread_dwords = prog_data->nr_params - cross_thread_dwords;
      assert(per_thread_dwords > 0 && per_thread_dwords <= 8);
   } else {
      /* Fill all data using cross-thread payload */
      cross_thread_dwords = prog_data->nr_params;
      per_thread_dwords = 0u;
   }

   fill_push_const_block_info(&cs_prog_data->push.cross_thread, cross_thread_dwords);
   fill_push_const_block_info(&cs_prog_data->push.per_thread, per_thread_dwords);

   assert(cs_prog_data->push.cross_thread.dwords % 8 == 0 ||
          cs_prog_data->push.per_thread.size == 0);
   assert(cs_prog_data->push.cross_thread.dwords +
          cs_prog_data->push.per_thread.dwords ==
             prog_data->nr_params);
}

static bool
filter_simd(const nir_instr *instr, const void * /* options */)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   switch (nir_instr_as_intrinsic(instr)->intrinsic) {
   case nir_intrinsic_load_simd_width_intel:
   case nir_intrinsic_load_subgroup_id:
      return true;

   default:
      return false;
   }
}

static nir_def *
lower_simd(nir_builder *b, nir_instr *instr, void *options)
{
   uintptr_t simd_width = (uintptr_t)options;

   switch (nir_instr_as_intrinsic(instr)->intrinsic) {
   case nir_intrinsic_load_simd_width_intel:
      return nir_imm_int(b, simd_width);

   case nir_intrinsic_load_subgroup_id:
      /* If the whole workgroup fits in one thread, we can lower subgroup_id
       * to a constant zero.
       */
      if (!b->shader->info.workgroup_size_variable) {
         unsigned local_workgroup_size = b->shader->info.workgroup_size[0] *
                                         b->shader->info.workgroup_size[1] *
                                         b->shader->info.workgroup_size[2];
         if (local_workgroup_size <= simd_width)
            return nir_imm_int(b, 0);
      }
      return NULL;

   default:
      return NULL;
   }
}

bool
brw_nir_lower_simd(nir_shader *nir, unsigned dispatch_width)
{
   return nir_shader_lower_instructions(nir, filter_simd, lower_simd,
                                 (void *)(uintptr_t)dispatch_width);
}

const unsigned *
brw_compile_cs(const struct brw_compiler *compiler,
               struct brw_compile_cs_params *params)
{
   const nir_shader *nir = params->base.nir;
   const struct brw_cs_prog_key *key = params->key;
   struct brw_cs_prog_data *prog_data = params->prog_data;

   const bool debug_enabled =
      brw_should_print_shader(nir, params->base.debug_flag ?
                                   params->base.debug_flag : DEBUG_CS);

   prog_data->base.stage = MESA_SHADER_COMPUTE;
   prog_data->base.total_shared = nir->info.shared_size;
   prog_data->base.ray_queries = nir->info.ray_queries;
   prog_data->base.total_scratch = 0;

   if (!nir->info.workgroup_size_variable) {
      prog_data->local_size[0] = nir->info.workgroup_size[0];
      prog_data->local_size[1] = nir->info.workgroup_size[1];
      prog_data->local_size[2] = nir->info.workgroup_size[2];
   }

   brw_simd_selection_state simd_state{
      .devinfo = compiler->devinfo,
      .prog_data = prog_data,
      .required_width = brw_required_dispatch_width(&nir->info),
   };

   std::unique_ptr<fs_visitor> v[3];

   for (unsigned simd = 0; simd < 3; simd++) {
      if (!brw_simd_should_compile(simd_state, simd))
         continue;

      const unsigned dispatch_width = 8u << simd;

      nir_shader *shader = nir_shader_clone(params->base.mem_ctx, nir);
      brw_nir_apply_key(shader, compiler, &key->base,
                        dispatch_width);

      NIR_PASS(_, shader, brw_nir_lower_simd, dispatch_width);

      /* Clean up after the local index and ID calculations. */
      NIR_PASS(_, shader, nir_opt_constant_folding);
      NIR_PASS(_, shader, nir_opt_dce);

      brw_postprocess_nir(shader, compiler, debug_enabled,
                          key->base.robust_flags);

      v[simd] = std::make_unique<fs_visitor>(compiler, &params->base,
                                             &key->base,
                                             &prog_data->base,
                                             shader, dispatch_width,
                                             params->base.stats != NULL,
                                             debug_enabled);

      const int first = brw_simd_first_compiled(simd_state);
      if (first >= 0)
         v[simd]->import_uniforms(v[first].get());

      const bool allow_spilling = first < 0 || nir->info.workgroup_size_variable;

      if (v[simd]->run_cs(allow_spilling)) {
         cs_fill_push_const_info(compiler->devinfo, prog_data);

         brw_simd_mark_compiled(simd_state, simd, v[simd]->spilled_any_registers);
      } else {
         simd_state.error[simd] = ralloc_strdup(params->base.mem_ctx, v[simd]->fail_msg);
         if (simd > 0) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "SIMD%u shader failed to compile: %s\n",
                                dispatch_width, v[simd]->fail_msg);
         }
      }
   }

   const int selected_simd = brw_simd_select(simd_state);
   if (selected_simd < 0) {
      params->base.error_str =
         ralloc_asprintf(params->base.mem_ctx,
                         "Can't compile shader: "
                         "SIMD8 '%s', SIMD16 '%s' and SIMD32 '%s'.\n",
                         simd_state.error[0], simd_state.error[1],
                         simd_state.error[2]);
      return NULL;
   }

   assert(selected_simd < 3);

   if (!nir->info.workgroup_size_variable)
      prog_data->prog_mask = 1 << selected_simd;

   fs_generator g(compiler, &params->base, &prog_data->base,
                  MESA_SHADER_COMPUTE);
   if (unlikely(debug_enabled)) {
      char *name = ralloc_asprintf(params->base.mem_ctx,
                                   "%s compute shader %s",
                                   nir->info.label ?
                                   nir->info.label : "unnamed",
                                   nir->info.name);
      g.enable_debug(name);
   }

   uint32_t max_dispatch_width = 8u << (util_last_bit(prog_data->prog_mask) - 1);

   struct brw_compile_stats *stats = params->base.stats;
   for (unsigned simd = 0; simd < 3; simd++) {
      if (prog_data->prog_mask & (1u << simd)) {
         assert(v[simd]);
         prog_data->prog_offset[simd] =
            g.generate_code(v[simd]->cfg, 8u << simd, v[simd]->shader_stats,
                            v[simd]->performance_analysis.require(), stats);
         if (stats)
            stats->max_dispatch_width = max_dispatch_width;
         stats = stats ? stats + 1 : NULL;
         max_dispatch_width = 8u << simd;
      }
   }

   g.add_const_data(nir->constant_data, nir->constant_data_size);

   return g.get_assembly();
}

struct intel_cs_dispatch_info
brw_cs_get_dispatch_info(const struct intel_device_info *devinfo,
                         const struct brw_cs_prog_data *prog_data,
                         const unsigned *override_local_size)
{
   struct intel_cs_dispatch_info info = {};

   const unsigned *sizes =
      override_local_size ? override_local_size :
                            prog_data->local_size;

   const int simd = brw_simd_select_for_workgroup_size(devinfo, prog_data, sizes);
   assert(simd >= 0 && simd < 3);

   info.group_size = sizes[0] * sizes[1] * sizes[2];
   info.simd_size = 8u << simd;
   info.threads = DIV_ROUND_UP(info.group_size, info.simd_size);

   const uint32_t remainder = info.group_size & (info.simd_size - 1);
   if (remainder > 0)
      info.right_mask = ~0u >> (32 - remainder);
   else
      info.right_mask = ~0u >> (32 - info.simd_size);

   return info;
}

static uint8_t
compile_single_bs(const struct brw_compiler *compiler,
                  struct brw_compile_bs_params *params,
                  const struct brw_bs_prog_key *key,
                  struct brw_bs_prog_data *prog_data,
                  nir_shader *shader,
                  fs_generator *g,
                  struct brw_compile_stats *stats,
                  int *prog_offset)
{
   const bool debug_enabled = brw_should_print_shader(shader, DEBUG_RT);

   prog_data->base.stage = shader->info.stage;
   prog_data->max_stack_size = MAX2(prog_data->max_stack_size,
                                    shader->scratch_size);

   const unsigned max_dispatch_width = 16;
   brw_nir_apply_key(shader, compiler, &key->base, max_dispatch_width);
   brw_postprocess_nir(shader, compiler, debug_enabled,
                       key->base.robust_flags);

   brw_simd_selection_state simd_state{
      .devinfo = compiler->devinfo,
      .prog_data = prog_data,

      /* Since divergence is a lot more likely in RT than compute, it makes
       * sense to limit ourselves to the smallest available SIMD for now.
       */
      .required_width = compiler->devinfo->ver >= 20 ? 16u : 8u,
   };

   std::unique_ptr<fs_visitor> v[2];

   for (unsigned simd = 0; simd < ARRAY_SIZE(v); simd++) {
      if (!brw_simd_should_compile(simd_state, simd))
         continue;

      const unsigned dispatch_width = 8u << simd;

      if (dispatch_width == 8 && compiler->devinfo->ver >= 20)
         continue;

      v[simd] = std::make_unique<fs_visitor>(compiler, &params->base,
                                             &key->base,
                                             &prog_data->base, shader,
                                             dispatch_width,
                                             stats != NULL,
                                             debug_enabled);

      const bool allow_spilling = !brw_simd_any_compiled(simd_state);
      if (v[simd]->run_bs(allow_spilling)) {
         brw_simd_mark_compiled(simd_state, simd, v[simd]->spilled_any_registers);
      } else {
         simd_state.error[simd] = ralloc_strdup(params->base.mem_ctx,
                                                v[simd]->fail_msg);
         if (simd > 0) {
            brw_shader_perf_log(compiler, params->base.log_data,
                                "SIMD%u shader failed to compile: %s",
                                dispatch_width, v[simd]->fail_msg);
         }
      }
   }

   const int selected_simd = brw_simd_select(simd_state);
   if (selected_simd < 0) {
      params->base.error_str =
         ralloc_asprintf(params->base.mem_ctx,
                         "Can't compile shader: "
                         "SIMD8 '%s' and SIMD16 '%s'.\n",
                         simd_state.error[0], simd_state.error[1]);
      return 0;
   }

   assert(selected_simd < int(ARRAY_SIZE(v)));
   fs_visitor *selected = v[selected_simd].get();
   assert(selected);

   const unsigned dispatch_width = selected->dispatch_width;

   int offset = g->generate_code(selected->cfg, dispatch_width, selected->shader_stats,
                                 selected->performance_analysis.require(), stats);
   if (prog_offset)
      *prog_offset = offset;
   else
      assert(offset == 0);

   return dispatch_width;
}

uint64_t
brw_bsr(const struct intel_device_info *devinfo,
        uint32_t offset, uint8_t simd_size, uint8_t local_arg_offset)
{
   assert(offset % 64 == 0);
   assert(simd_size == 8 || simd_size == 16);
   assert(local_arg_offset % 8 == 0);

   return offset |
          SET_BITS(simd_size == 8, 4, 4) |
          SET_BITS(local_arg_offset / 8, 2, 0);
}

const unsigned *
brw_compile_bs(const struct brw_compiler *compiler,
               struct brw_compile_bs_params *params)
{
   nir_shader *shader = params->base.nir;
   struct brw_bs_prog_data *prog_data = params->prog_data;
   unsigned num_resume_shaders = params->num_resume_shaders;
   nir_shader **resume_shaders = params->resume_shaders;
   const bool debug_enabled = brw_should_print_shader(shader, DEBUG_RT);

   prog_data->base.stage = shader->info.stage;
   prog_data->base.ray_queries = shader->info.ray_queries;
   prog_data->base.total_scratch = 0;

   prog_data->max_stack_size = 0;
   prog_data->num_resume_shaders = num_resume_shaders;

   fs_generator g(compiler, &params->base, &prog_data->base,
                  shader->info.stage);
   if (unlikely(debug_enabled)) {
      char *name = ralloc_asprintf(params->base.mem_ctx,
                                   "%s %s shader %s",
                                   shader->info.label ?
                                      shader->info.label : "unnamed",
                                   gl_shader_stage_name(shader->info.stage),
                                   shader->info.name);
      g.enable_debug(name);
   }

   prog_data->simd_size =
      compile_single_bs(compiler, params, params->key, prog_data,
                        shader, &g, params->base.stats, NULL);
   if (prog_data->simd_size == 0)
      return NULL;

   uint64_t *resume_sbt = ralloc_array(params->base.mem_ctx,
                                       uint64_t, num_resume_shaders);
   for (unsigned i = 0; i < num_resume_shaders; i++) {
      if (INTEL_DEBUG(DEBUG_RT)) {
         char *name = ralloc_asprintf(params->base.mem_ctx,
                                      "%s %s resume(%u) shader %s",
                                      shader->info.label ?
                                         shader->info.label : "unnamed",
                                      gl_shader_stage_name(shader->info.stage),
                                      i, shader->info.name);
         g.enable_debug(name);
      }

      /* TODO: Figure out shader stats etc. for resume shaders */
      int offset = 0;
      uint8_t simd_size =
         compile_single_bs(compiler, params, params->key,
                           prog_data, resume_shaders[i], &g, NULL, &offset);
      if (simd_size == 0)
         return NULL;

      assert(offset > 0);
      resume_sbt[i] = brw_bsr(compiler->devinfo, offset, simd_size, 0);
   }

   /* We only have one constant data so we want to make sure they're all the
    * same.
    */
   for (unsigned i = 0; i < num_resume_shaders; i++) {
      assert(resume_shaders[i]->constant_data_size ==
             shader->constant_data_size);
      assert(memcmp(resume_shaders[i]->constant_data,
                    shader->constant_data,
                    shader->constant_data_size) == 0);
   }

   g.add_const_data(shader->constant_data, shader->constant_data_size);
   g.add_resume_sbt(num_resume_shaders, resume_sbt);

   return g.get_assembly();
}

/**
 * Test the dispatch mask packing assumptions of
 * brw_stage_has_packed_dispatch().  Call this from e.g. the top of
 * fs_visitor::emit_nir_code() to cause a GPU hang if any shader invocation is
 * executed with an unexpected dispatch mask.
 */
static UNUSED void
brw_fs_test_dispatch_packing(const fs_builder &bld)
{
   const fs_visitor *shader = bld.shader;
   const gl_shader_stage stage = shader->stage;
   const bool uses_vmask =
      stage == MESA_SHADER_FRAGMENT &&
      brw_wm_prog_data(shader->prog_data)->uses_vmask;

   if (brw_stage_has_packed_dispatch(shader->devinfo, stage,
                                     shader->max_polygons,
                                     shader->prog_data)) {
      const fs_builder ubld = bld.exec_all().group(1, 0);
      const fs_reg tmp = component(bld.vgrf(BRW_REGISTER_TYPE_UD), 0);
      const fs_reg mask = uses_vmask ? brw_vmask_reg() : brw_dmask_reg();

      ubld.ADD(tmp, mask, brw_imm_ud(1));
      ubld.AND(tmp, mask, tmp);

      /* This will loop forever if the dispatch mask doesn't have the expected
       * form '2^n-1', in which case tmp will be non-zero.
       */
      bld.emit(BRW_OPCODE_DO);
      bld.CMP(bld.null_reg_ud(), tmp, brw_imm_ud(0), BRW_CONDITIONAL_NZ);
      set_predicate(BRW_PREDICATE_NORMAL, bld.emit(BRW_OPCODE_WHILE));
   }
}

unsigned
fs_visitor::workgroup_size() const
{
   assert(gl_shader_stage_uses_workgroup(stage));
   const struct brw_cs_prog_data *cs = brw_cs_prog_data(prog_data);
   return cs->local_size[0] * cs->local_size[1] * cs->local_size[2];
}

bool brw_should_print_shader(const nir_shader *shader, uint64_t debug_flag)
{
   return INTEL_DEBUG(debug_flag) && (!shader->info.internal || NIR_DEBUG(PRINT_INTERNAL));
}

namespace brw {
   fs_reg
   fetch_payload_reg(const brw::fs_builder &bld, uint8_t regs[2],
                     brw_reg_type type, unsigned n)
   {
      if (!regs[0])
         return fs_reg();

      if (bld.dispatch_width() > 16) {
         const fs_reg tmp = bld.vgrf(type, n);
         const brw::fs_builder hbld = bld.exec_all().group(16, 0);
         const unsigned m = bld.dispatch_width() / hbld.dispatch_width();
         fs_reg *const components = new fs_reg[m * n];

         for (unsigned c = 0; c < n; c++) {
            for (unsigned g = 0; g < m; g++)
               components[c * m + g] =
                  offset(retype(brw_vec8_grf(regs[g], 0), type), hbld, c);
         }

         hbld.LOAD_PAYLOAD(tmp, components, m * n, 0);

         delete[] components;
         return tmp;

      } else {
         return fs_reg(retype(brw_vec8_grf(regs[0], 0), type));
      }
   }

   fs_reg
   fetch_barycentric_reg(const brw::fs_builder &bld, uint8_t regs[2])
   {
      if (!regs[0])
         return fs_reg();
      else if (bld.shader->devinfo->ver >= 20)
         return fetch_payload_reg(bld, regs, BRW_REGISTER_TYPE_F, 2);

      const fs_reg tmp = bld.vgrf(BRW_REGISTER_TYPE_F, 2);
      const brw::fs_builder hbld = bld.exec_all().group(8, 0);
      const unsigned m = bld.dispatch_width() / hbld.dispatch_width();
      fs_reg *const components = new fs_reg[2 * m];

      for (unsigned c = 0; c < 2; c++) {
         for (unsigned g = 0; g < m; g++)
            components[c * m + g] = offset(brw_vec8_grf(regs[g / 2], 0),
                                           hbld, c + 2 * (g % 2));
      }

      hbld.LOAD_PAYLOAD(tmp, components, 2 * m, 0);

      delete[] components;
      return tmp;
   }

   void
   check_dynamic_msaa_flag(const fs_builder &bld,
                           const struct brw_wm_prog_data *wm_prog_data,
                           enum intel_msaa_flags flag)
   {
      fs_inst *inst = bld.AND(bld.null_reg_ud(),
                              dynamic_msaa_flags(wm_prog_data),
                              brw_imm_ud(flag));
      inst->conditional_mod = BRW_CONDITIONAL_NZ;
   }
}
