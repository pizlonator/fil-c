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

/** @file elk_fs.cpp
 *
 * This file drives the GLSL IR -> LIR translation, contains the
 * optimizations on the LIR, and drives the generation of native code
 * from the LIR.
 */

#include "elk_eu.h"
#include "elk_fs.h"
#include "elk_fs_builder.h"
#include "elk_fs_live_variables.h"
#include "elk_nir.h"
#include "elk_vec4_gs_visitor.h"
#include "elk_cfg.h"
#include "elk_dead_control_flow.h"
#include "elk_private.h"
#include "../intel_nir.h"
#include "shader_enums.h"
#include "dev/intel_debug.h"
#include "dev/intel_wa.h"
#include "compiler/glsl_types.h"
#include "compiler/nir/nir_builder.h"
#include "util/u_math.h"

#include <memory>

using namespace elk;

static unsigned get_lowered_simd_width(const elk_fs_visitor *shader,
                                       const elk_fs_inst *inst);

void
elk_fs_inst::init(enum elk_opcode opcode, uint8_t exec_size, const elk_fs_reg &dst,
              const elk_fs_reg *src, unsigned sources)
{
   memset((void*)this, 0, sizeof(*this));

   this->src = new elk_fs_reg[MAX2(sources, 3)];
   for (unsigned i = 0; i < sources; i++)
      this->src[i] = src[i];

   this->opcode = opcode;
   this->dst = dst;
   this->sources = sources;
   this->exec_size = exec_size;
   this->base_mrf = -1;

   assert(dst.file != IMM && dst.file != UNIFORM);

   assert(this->exec_size != 0);

   this->conditional_mod = ELK_CONDITIONAL_NONE;

   /* This will be the case for almost all instructions. */
   switch (dst.file) {
   case VGRF:
   case ARF:
   case FIXED_GRF:
   case MRF:
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

elk_fs_inst::elk_fs_inst()
{
   init(ELK_OPCODE_NOP, 8, dst, NULL, 0);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_size)
{
   init(opcode, exec_size, reg_undef, NULL, 0);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_size, const elk_fs_reg &dst)
{
   init(opcode, exec_size, dst, NULL, 0);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_size, const elk_fs_reg &dst,
                 const elk_fs_reg &src0)
{
   const elk_fs_reg src[1] = { src0 };
   init(opcode, exec_size, dst, src, 1);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_size, const elk_fs_reg &dst,
                 const elk_fs_reg &src0, const elk_fs_reg &src1)
{
   const elk_fs_reg src[2] = { src0, src1 };
   init(opcode, exec_size, dst, src, 2);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_size, const elk_fs_reg &dst,
                 const elk_fs_reg &src0, const elk_fs_reg &src1, const elk_fs_reg &src2)
{
   const elk_fs_reg src[3] = { src0, src1, src2 };
   init(opcode, exec_size, dst, src, 3);
}

elk_fs_inst::elk_fs_inst(enum elk_opcode opcode, uint8_t exec_width, const elk_fs_reg &dst,
                 const elk_fs_reg src[], unsigned sources)
{
   init(opcode, exec_width, dst, src, sources);
}

elk_fs_inst::elk_fs_inst(const elk_fs_inst &that)
{
   memcpy((void*)this, &that, sizeof(that));

   this->src = new elk_fs_reg[MAX2(that.sources, 3)];

   for (unsigned i = 0; i < that.sources; i++)
      this->src[i] = that.src[i];
}

elk_fs_inst::~elk_fs_inst()
{
   delete[] this->src;
}

void
elk_fs_inst::resize_sources(uint8_t num_sources)
{
   if (this->sources != num_sources) {
      elk_fs_reg *src = new elk_fs_reg[MAX2(num_sources, 3)];

      for (unsigned i = 0; i < MIN2(this->sources, num_sources); ++i)
         src[i] = this->src[i];

      delete[] this->src;
      this->src = src;
      this->sources = num_sources;
   }
}

void
elk_fs_visitor::VARYING_PULL_CONSTANT_LOAD(const fs_builder &bld,
                                       const elk_fs_reg &dst,
                                       const elk_fs_reg &surface,
                                       const elk_fs_reg &surface_handle,
                                       const elk_fs_reg &varying_offset,
                                       uint32_t const_offset,
                                       uint8_t alignment,
                                       unsigned components)
{
   assert(components <= 4);

   /* We have our constant surface use a pitch of 4 bytes, so our index can
    * be any component of a vector, and then we load 4 contiguous
    * components starting from that.  TODO: Support loading fewer than 4.
    */
   elk_fs_reg total_offset = vgrf(glsl_uint_type());
   bld.ADD(total_offset, varying_offset, elk_imm_ud(const_offset));

   /* The pull load message will load a vec4 (16 bytes). If we are loading
    * a double this means we are only loading 2 elements worth of data.
    * We also want to use a 32-bit data type for the dst of the load operation
    * so other parts of the driver don't get confused about the size of the
    * result.
    */
   elk_fs_reg vec4_result = bld.vgrf(ELK_REGISTER_TYPE_F, 4);

   elk_fs_reg srcs[PULL_VARYING_CONSTANT_SRCS];
   srcs[PULL_VARYING_CONSTANT_SRC_SURFACE]        = surface;
   srcs[PULL_VARYING_CONSTANT_SRC_SURFACE_HANDLE] = surface_handle;
   srcs[PULL_VARYING_CONSTANT_SRC_OFFSET]         = total_offset;
   srcs[PULL_VARYING_CONSTANT_SRC_ALIGNMENT]      = elk_imm_ud(alignment);

   elk_fs_inst *inst = bld.emit(ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL,
                            vec4_result, srcs, PULL_VARYING_CONSTANT_SRCS);
   inst->size_written = 4 * vec4_result.component_size(inst->exec_size);

   elk_shuffle_from_32bit_read(bld, dst, vec4_result, 0, components);
}

/**
 * A helper for MOV generation for fixing up broken hardware SEND dependency
 * handling.
 */
void
elk_fs_visitor::DEP_RESOLVE_MOV(const fs_builder &bld, int grf)
{
   /* The caller always wants uncompressed to emit the minimal extra
    * dependencies, and to avoid having to deal with aligning its regs to 2.
    */
   const fs_builder ubld = bld.annotate("send dependency resolve")
                              .quarter(0);

   ubld.MOV(ubld.null_reg_f(), elk_fs_reg(VGRF, grf, ELK_REGISTER_TYPE_F));
}

bool
elk_fs_inst::is_send_from_grf() const
{
   switch (opcode) {
   case ELK_SHADER_OPCODE_SEND:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
   case ELK_SHADER_OPCODE_INTERLOCK:
   case ELK_SHADER_OPCODE_MEMORY_FENCE:
   case ELK_SHADER_OPCODE_BARRIER:
      return true;
   case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
      return src[1].file == VGRF;
   case ELK_FS_OPCODE_FB_WRITE:
      return src[0].file == VGRF;
   default:
      return false;
   }
}

bool
elk_fs_inst::is_control_source(unsigned arg) const
{
   switch (opcode) {
   case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
   case ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GFX4:
      return arg == 0;

   case ELK_SHADER_OPCODE_BROADCAST:
   case ELK_SHADER_OPCODE_SHUFFLE:
   case ELK_SHADER_OPCODE_QUAD_SWIZZLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return arg == 1;

   case ELK_SHADER_OPCODE_MOV_INDIRECT:
   case ELK_SHADER_OPCODE_CLUSTER_BROADCAST:
   case ELK_SHADER_OPCODE_TEX:
   case ELK_FS_OPCODE_TXB:
   case ELK_SHADER_OPCODE_TXD:
   case ELK_SHADER_OPCODE_TXF:
   case ELK_SHADER_OPCODE_TXF_LZ:
   case ELK_SHADER_OPCODE_TXF_CMS:
   case ELK_SHADER_OPCODE_TXF_CMS_W:
   case ELK_SHADER_OPCODE_TXF_UMS:
   case ELK_SHADER_OPCODE_TXF_MCS:
   case ELK_SHADER_OPCODE_TXL:
   case ELK_SHADER_OPCODE_TXL_LZ:
   case ELK_SHADER_OPCODE_TXS:
   case ELK_SHADER_OPCODE_LOD:
   case ELK_SHADER_OPCODE_TG4:
   case ELK_SHADER_OPCODE_TG4_OFFSET:
   case ELK_SHADER_OPCODE_SAMPLEINFO:
      return arg == 1 || arg == 2;

   case ELK_SHADER_OPCODE_SEND:
      return arg == 0;

   default:
      return false;
   }
}

bool
elk_fs_inst::is_payload(unsigned arg) const
{
   switch (opcode) {
   case ELK_FS_OPCODE_FB_WRITE:
   case ELK_VEC4_OPCODE_UNTYPED_ATOMIC:
   case ELK_VEC4_OPCODE_UNTYPED_SURFACE_READ:
   case ELK_VEC4_OPCODE_UNTYPED_SURFACE_WRITE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case ELK_SHADER_OPCODE_INTERLOCK:
   case ELK_SHADER_OPCODE_MEMORY_FENCE:
   case ELK_SHADER_OPCODE_BARRIER:
   case ELK_SHADER_OPCODE_TEX:
   case ELK_FS_OPCODE_TXB:
   case ELK_SHADER_OPCODE_TXD:
   case ELK_SHADER_OPCODE_TXF:
   case ELK_SHADER_OPCODE_TXF_LZ:
   case ELK_SHADER_OPCODE_TXF_CMS:
   case ELK_SHADER_OPCODE_TXF_CMS_W:
   case ELK_SHADER_OPCODE_TXF_UMS:
   case ELK_SHADER_OPCODE_TXF_MCS:
   case ELK_SHADER_OPCODE_TXL:
   case ELK_SHADER_OPCODE_TXL_LZ:
   case ELK_SHADER_OPCODE_TXS:
   case ELK_SHADER_OPCODE_LOD:
   case ELK_SHADER_OPCODE_TG4:
   case ELK_SHADER_OPCODE_TG4_OFFSET:
   case ELK_SHADER_OPCODE_SAMPLEINFO:
      return arg == 0;

   case ELK_SHADER_OPCODE_SEND:
      return arg == 1;

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
elk_fs_inst::has_source_and_destination_hazard() const
{
   switch (opcode) {
   case ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT:
      /* Multiple partial writes to the destination */
      return true;
   case ELK_SHADER_OPCODE_SHUFFLE:
      /* This instruction returns an arbitrary channel from the source and
       * gets split into smaller instructions in the generator.  It's possible
       * that one of the instructions will read from a channel corresponding
       * to an earlier instruction.
       */
   case ELK_SHADER_OPCODE_SEL_EXEC:
      /* This is implemented as
       *
       * mov(16)      g4<1>D      0D            { align1 WE_all 1H };
       * mov(16)      g4<1>D      g5<8,8,1>D    { align1 1H }
       *
       * Because the source is only read in the second instruction, the first
       * may stomp all over it.
       */
      return true;
   case ELK_SHADER_OPCODE_QUAD_SWIZZLE:
      switch (src[1].ud) {
      case ELK_SWIZZLE_XXXX:
      case ELK_SWIZZLE_YYYY:
      case ELK_SWIZZLE_ZZZZ:
      case ELK_SWIZZLE_WWWW:
      case ELK_SWIZZLE_XXZZ:
      case ELK_SWIZZLE_YYWW:
      case ELK_SWIZZLE_XYXY:
      case ELK_SWIZZLE_ZWZW:
         /* These can be implemented as a single Align1 region on all
          * platforms, so there's never a hazard between source and
          * destination.  C.f. elk_fs_generator::generate_quad_swizzle().
          */
         return false;
      default:
         return !is_uniform(src[0]);
      }
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
                                        src[i].type == ELK_REGISTER_TYPE_UW ||
                                        src[i].type == ELK_REGISTER_TYPE_W ||
                                        src[i].type == ELK_REGISTER_TYPE_UB ||
                                        src[i].type == ELK_REGISTER_TYPE_B)) {
               return true;
            }
         }
      }
      return false;
   }
}

bool
elk_fs_inst::can_do_source_mods(const struct intel_device_info *devinfo) const
{
   if (devinfo->ver == 6 && is_math())
      return false;

   if (is_send_from_grf())
      return false;

   return elk_backend_instruction::can_do_source_mods();
}

bool
elk_fs_inst::can_do_cmod()
{
   if (!elk_backend_instruction::can_do_cmod())
      return false;

   /* The accumulator result appears to get used for the conditional modifier
    * generation.  When negating a UD value, there is a 33rd bit generated for
    * the sign in the accumulator value, so now you can't check, for example,
    * equality with a 32-bit value.  See piglit fs-op-neg-uvec4.
    */
   for (unsigned i = 0; i < sources; i++) {
      if (elk_reg_type_is_unsigned_integer(src[i].type) && src[i].negate)
         return false;
   }

   return true;
}

bool
elk_fs_inst::can_change_types() const
{
   return dst.type == src[0].type &&
          !src[0].abs && !src[0].negate && !saturate && src[0].file != ATTR &&
          (opcode == ELK_OPCODE_MOV ||
           (opcode == ELK_OPCODE_SEL &&
            dst.type == src[1].type &&
            predicate != ELK_PREDICATE_NONE &&
            !src[1].abs && !src[1].negate && src[1].file != ATTR));
}

void
elk_fs_reg::init()
{
   memset((void*)this, 0, sizeof(*this));
   type = ELK_REGISTER_TYPE_UD;
   stride = 1;
}

/** Generic unset register constructor. */
elk_fs_reg::elk_fs_reg()
{
   init();
   this->file = BAD_FILE;
}

elk_fs_reg::elk_fs_reg(struct ::elk_reg reg) :
   elk_backend_reg(reg)
{
   this->offset = 0;
   this->stride = 1;
   if (this->file == IMM &&
       (this->type != ELK_REGISTER_TYPE_V &&
        this->type != ELK_REGISTER_TYPE_UV &&
        this->type != ELK_REGISTER_TYPE_VF)) {
      this->stride = 0;
   }
}

bool
elk_fs_reg::equals(const elk_fs_reg &r) const
{
   return (this->elk_backend_reg::equals(r) &&
           stride == r.stride);
}

bool
elk_fs_reg::negative_equals(const elk_fs_reg &r) const
{
   return (this->elk_backend_reg::negative_equals(r) &&
           stride == r.stride);
}

bool
elk_fs_reg::is_contiguous() const
{
   switch (file) {
   case ARF:
   case FIXED_GRF:
      return hstride == ELK_HORIZONTAL_STRIDE_1 &&
             vstride == width + hstride;
   case MRF:
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
elk_fs_reg::component_size(unsigned width) const
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
elk_fs_visitor::vfail(const char *format, va_list va)
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
elk_fs_visitor::fail(const char *format, ...)
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
elk_fs_visitor::limit_dispatch_width(unsigned n, const char *msg)
{
   if (dispatch_width > n) {
      fail("%s", msg);
   } else {
      max_dispatch_width = MIN2(max_dispatch_width, n);
      elk_shader_perf_log(compiler, log_data,
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
elk_fs_inst::is_partial_write() const
{
   if (this->predicate && !this->predicate_trivial &&
       this->opcode != ELK_OPCODE_SEL)
      return true;

   if (this->dst.offset % REG_SIZE != 0)
      return true;

   /* SEND instructions always write whole registers */
   if (this->opcode == ELK_SHADER_OPCODE_SEND)
      return false;

   /* Special case UNDEF since a lot of places in the backend do things like this :
    *
    *  fs_builder ubld = bld.exec_all().group(1, 0);
    *  elk_fs_reg tmp = ubld.vgrf(ELK_REGISTER_TYPE_UD);
    *  ubld.UNDEF(tmp); <- partial write, even if the whole register is concerned
    */
   if (this->opcode == ELK_SHADER_OPCODE_UNDEF) {
      assert(this->dst.is_contiguous());
      return this->size_written < 32;
   }

   return this->exec_size * type_sz(this->dst.type) < 32 ||
          !this->dst.is_contiguous();
}

unsigned
elk_fs_inst::components_read(unsigned i) const
{
   /* Return zero if the source is not present. */
   if (src[i].file == BAD_FILE)
      return 0;

   switch (opcode) {
   case ELK_FS_OPCODE_LINTERP:
      if (i == 0)
         return 2;
      else
         return 1;

   case ELK_FS_OPCODE_PIXEL_X:
   case ELK_FS_OPCODE_PIXEL_Y:
      assert(i < 2);
      if (i == 0)
         return 2;
      else
         return 1;

   case ELK_FS_OPCODE_FB_WRITE_LOGICAL:
      assert(src[FB_WRITE_LOGICAL_SRC_COMPONENTS].file == IMM);
      /* First/second FB write color. */
      if (i < 2)
         return src[FB_WRITE_LOGICAL_SRC_COMPONENTS].ud;
      else
         return 1;

   case ELK_SHADER_OPCODE_TEX_LOGICAL:
   case ELK_SHADER_OPCODE_TXD_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_LOGICAL:
   case ELK_SHADER_OPCODE_TXL_LOGICAL:
   case ELK_SHADER_OPCODE_TXS_LOGICAL:
   case ELK_SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
   case ELK_FS_OPCODE_TXB_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_UMS_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_MCS_LOGICAL:
   case ELK_SHADER_OPCODE_LOD_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case ELK_SHADER_OPCODE_SAMPLEINFO_LOGICAL:
      assert(src[TEX_LOGICAL_SRC_COORD_COMPONENTS].file == IMM &&
             src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].file == IMM &&
             src[TEX_LOGICAL_SRC_RESIDENCY].file == IMM);
      /* Texture coordinates. */
      if (i == TEX_LOGICAL_SRC_COORDINATE)
         return src[TEX_LOGICAL_SRC_COORD_COMPONENTS].ud;
      /* Texture derivatives. */
      else if ((i == TEX_LOGICAL_SRC_LOD || i == TEX_LOGICAL_SRC_LOD2) &&
               opcode == ELK_SHADER_OPCODE_TXD_LOGICAL)
         return src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].ud;
      /* Texture offset. */
      else if (i == TEX_LOGICAL_SRC_TG4_OFFSET)
         return 2;
      /* MCS */
      else if (i == TEX_LOGICAL_SRC_MCS) {
         if (opcode == ELK_SHADER_OPCODE_TXF_CMS_W_LOGICAL)
            return 2;
         else if (opcode == ELK_SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL)
            return 4;
         else
            return 1;
      } else
         return 1;

   case ELK_SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case ELK_SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM);
      /* Surface coordinates. */
      if (i == SURFACE_LOGICAL_SRC_ADDRESS)
         return src[SURFACE_LOGICAL_SRC_IMM_DIMS].ud;
      /* Surface operation source (ignored for reads). */
      else if (i == SURFACE_LOGICAL_SRC_DATA)
         return 0;
      else
         return 1;

   case ELK_SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
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

   case ELK_SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
   case ELK_SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
   case ELK_SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return 1;

   case ELK_SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      if (i == A64_LOGICAL_SRC) { /* data to write */
         const unsigned comps = src[A64_LOGICAL_ARG].ud / exec_size;
         assert(comps > 0);
         return comps;
      } else {
         return 1;
      }

   case ELK_SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      return 1;

   case ELK_SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      if (i == SURFACE_LOGICAL_SRC_DATA) {
         const unsigned comps = src[SURFACE_LOGICAL_SRC_IMM_ARG].ud / exec_size;
         assert(comps > 0);
         return comps;
      } else {
         return 1;
      }

   case ELK_SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return i == A64_LOGICAL_SRC ? src[A64_LOGICAL_ARG].ud : 1;

   case ELK_SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      assert(src[A64_LOGICAL_ARG].file == IMM);
      return i == A64_LOGICAL_SRC ?
             lsc_op_num_data_values(src[A64_LOGICAL_ARG].ud) : 1;

   case ELK_SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case ELK_SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
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

   case ELK_SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      assert(src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == IMM &&
             src[SURFACE_LOGICAL_SRC_IMM_ARG].file == IMM);
      return 1;

   case ELK_SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
   case ELK_SHADER_OPCODE_TYPED_ATOMIC_LOGICAL: {
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
   case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return (i == 0 ? 2 : 1);

   case ELK_SHADER_OPCODE_URB_WRITE_LOGICAL:
      assert(src[URB_LOGICAL_SRC_COMPONENTS].file == IMM);

      if (i == URB_LOGICAL_SRC_DATA)
         return src[URB_LOGICAL_SRC_COMPONENTS].ud;
      else
         return 1;

   default:
      return 1;
   }
}

unsigned
elk_fs_inst::size_read(int arg) const
{
   switch (opcode) {
   case ELK_SHADER_OPCODE_SEND:
      if (arg == 1) {
         return mlen * REG_SIZE;
      }
      break;

   case ELK_FS_OPCODE_FB_WRITE:
   case ELK_FS_OPCODE_REP_FB_WRITE:
      if (arg == 0) {
         if (base_mrf >= 0)
            return src[0].file == BAD_FILE ? 0 : 2 * REG_SIZE;
         else
            return mlen * REG_SIZE;
      }
      break;

   case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      if (arg == 0)
         return mlen * REG_SIZE;
      break;

   case ELK_FS_OPCODE_SET_SAMPLE_ID:
      if (arg == 1)
         return 1;
      break;

   case ELK_FS_OPCODE_LINTERP:
      if (arg == 1)
         return 16;
      break;

   case ELK_SHADER_OPCODE_LOAD_PAYLOAD:
      if (arg < this->header_size)
         return retype(src[arg], ELK_REGISTER_TYPE_UD).component_size(8);
      break;

   case ELK_CS_OPCODE_CS_TERMINATE:
   case ELK_SHADER_OPCODE_BARRIER:
      return REG_SIZE;

   case ELK_SHADER_OPCODE_MOV_INDIRECT:
      if (arg == 0) {
         assert(src[2].file == IMM);
         return src[2].ud;
      }
      break;

   case ELK_SHADER_OPCODE_TEX:
   case ELK_FS_OPCODE_TXB:
   case ELK_SHADER_OPCODE_TXD:
   case ELK_SHADER_OPCODE_TXF:
   case ELK_SHADER_OPCODE_TXF_LZ:
   case ELK_SHADER_OPCODE_TXF_CMS:
   case ELK_SHADER_OPCODE_TXF_CMS_W:
   case ELK_SHADER_OPCODE_TXF_UMS:
   case ELK_SHADER_OPCODE_TXF_MCS:
   case ELK_SHADER_OPCODE_TXL:
   case ELK_SHADER_OPCODE_TXL_LZ:
   case ELK_SHADER_OPCODE_TXS:
   case ELK_SHADER_OPCODE_LOD:
   case ELK_SHADER_OPCODE_TG4:
   case ELK_SHADER_OPCODE_TG4_OFFSET:
   case ELK_SHADER_OPCODE_SAMPLEINFO:
      if (arg == 0 && src[0].file == VGRF)
         return mlen * REG_SIZE;
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
   case MRF:
      unreachable("MRF registers are not allowed as sources");
   }
   return 0;
}

namespace {
   unsigned
   predicate_width(const intel_device_info *devinfo, elk_predicate predicate)
   {
      switch (predicate) {
      case ELK_PREDICATE_NONE:            return 1;
      case ELK_PREDICATE_NORMAL:          return 1;
      case ELK_PREDICATE_ALIGN1_ANY2H:    return 2;
      case ELK_PREDICATE_ALIGN1_ALL2H:    return 2;
      case ELK_PREDICATE_ALIGN1_ANY4H:    return 4;
      case ELK_PREDICATE_ALIGN1_ALL4H:    return 4;
      case ELK_PREDICATE_ALIGN1_ANY8H:    return 8;
      case ELK_PREDICATE_ALIGN1_ALL8H:    return 8;
      case ELK_PREDICATE_ALIGN1_ANY16H:   return 16;
      case ELK_PREDICATE_ALIGN1_ALL16H:   return 16;
      case ELK_PREDICATE_ALIGN1_ANY32H:   return 32;
      case ELK_PREDICATE_ALIGN1_ALL32H:   return 32;
      default: unreachable("Unsupported predicate");
      }
   }

   /* Return the subset of flag registers that an instruction could
    * potentially read or write based on the execution controls and flag
    * subregister number of the instruction.
    */
   unsigned
   flag_mask(const elk_fs_inst *inst, unsigned width)
   {
      assert(util_is_power_of_two_nonzero(width));
      const unsigned start = (inst->flag_subreg * 16 + inst->group) &
                             ~(width - 1);
      const unsigned end = start + ALIGN(inst->exec_size, width);
      return ((1 << DIV_ROUND_UP(end, 8)) - 1) & ~((1 << (start / 8)) - 1);
   }

   unsigned
   bit_mask(unsigned n)
   {
      return (n >= CHAR_BIT * sizeof(bit_mask(n)) ? ~0u : (1u << n) - 1);
   }

   unsigned
   flag_mask(const elk_fs_reg &r, unsigned sz)
   {
      if (r.file == ARF) {
         const unsigned start = (r.nr - ELK_ARF_FLAG) * 4 + r.subnr;
         const unsigned end = start + sz;
         return bit_mask(end) & ~bit_mask(start);
      } else {
         return 0;
      }
   }
}

unsigned
elk_fs_inst::flags_read(const intel_device_info *devinfo) const
{
   if (predicate == ELK_PREDICATE_ALIGN1_ANYV ||
       predicate == ELK_PREDICATE_ALIGN1_ALLV) {
      /* The vertical predication modes combine corresponding bits from
       * f0.0 and f1.0 on Gfx7+, and f0.0 and f0.1 on older hardware.
       */
      const unsigned shift = devinfo->ver >= 7 ? 4 : 2;
      return flag_mask(this, 1) << shift | flag_mask(this, 1);
   } else if (predicate) {
      return flag_mask(this, predicate_width(devinfo, predicate));
   } else {
      unsigned mask = 0;
      for (int i = 0; i < sources; i++) {
         mask |= flag_mask(src[i], size_read(i));
      }
      return mask;
   }
}

unsigned
elk_fs_inst::flags_written(const intel_device_info *devinfo) const
{
   /* On Gfx4 and Gfx5, sel.l (for min) and sel.ge (for max) are implemented
    * using a separate cmpn and sel instruction.  This lowering occurs in
    * fs_vistor::lower_minmax which is called very, very late.
    */
   if ((conditional_mod && ((opcode != ELK_OPCODE_SEL || devinfo->ver <= 5) &&
                            opcode != ELK_OPCODE_CSEL &&
                            opcode != ELK_OPCODE_IF &&
                            opcode != ELK_OPCODE_WHILE)) ||
       opcode == ELK_FS_OPCODE_FB_WRITE) {
      return flag_mask(this, 1);
   } else if (opcode == ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL ||
              opcode == ELK_SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL ||
              opcode == ELK_FS_OPCODE_LOAD_LIVE_CHANNELS) {
      return flag_mask(this, 32);
   } else {
      return flag_mask(dst, size_written);
   }
}

/**
 * Returns how many MRFs an FS opcode will write over.
 *
 * Note that this is not the 0 or 1 implied writes in an actual gen
 * instruction -- the FS opcodes often generate MOVs in addition.
 */
unsigned
elk_fs_inst::implied_mrf_writes() const
{
   if (mlen == 0)
      return 0;

   if (base_mrf == -1)
      return 0;

   switch (opcode) {
   case ELK_SHADER_OPCODE_RCP:
   case ELK_SHADER_OPCODE_RSQ:
   case ELK_SHADER_OPCODE_SQRT:
   case ELK_SHADER_OPCODE_EXP2:
   case ELK_SHADER_OPCODE_LOG2:
   case ELK_SHADER_OPCODE_SIN:
   case ELK_SHADER_OPCODE_COS:
      return 1 * exec_size / 8;
   case ELK_SHADER_OPCODE_POW:
   case ELK_SHADER_OPCODE_INT_QUOTIENT:
   case ELK_SHADER_OPCODE_INT_REMAINDER:
      return 2 * exec_size / 8;
   case ELK_SHADER_OPCODE_TEX:
   case ELK_FS_OPCODE_TXB:
   case ELK_SHADER_OPCODE_TXD:
   case ELK_SHADER_OPCODE_TXF:
   case ELK_SHADER_OPCODE_TXF_CMS:
   case ELK_SHADER_OPCODE_TXF_MCS:
   case ELK_SHADER_OPCODE_TG4:
   case ELK_SHADER_OPCODE_TG4_OFFSET:
   case ELK_SHADER_OPCODE_TXL:
   case ELK_SHADER_OPCODE_TXS:
   case ELK_SHADER_OPCODE_LOD:
   case ELK_SHADER_OPCODE_SAMPLEINFO:
      return 1;
   case ELK_FS_OPCODE_FB_WRITE:
   case ELK_FS_OPCODE_REP_FB_WRITE:
      return src[0].file == BAD_FILE ? 0 : 2;
   case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
   case ELK_SHADER_OPCODE_GFX4_SCRATCH_READ:
      return 1;
   case ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GFX4:
      return mlen;
   case ELK_SHADER_OPCODE_GFX4_SCRATCH_WRITE:
      return mlen;
   default:
      unreachable("not reached");
   }
}

bool
elk_fs_inst::has_sampler_residency() const
{
   switch (opcode) {
   case ELK_SHADER_OPCODE_TEX_LOGICAL:
   case ELK_FS_OPCODE_TXB_LOGICAL:
   case ELK_SHADER_OPCODE_TXL_LOGICAL:
   case ELK_SHADER_OPCODE_TXD_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_LOGICAL:
   case ELK_SHADER_OPCODE_TXS_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_LOGICAL:
      assert(src[TEX_LOGICAL_SRC_RESIDENCY].file == IMM);
      return src[TEX_LOGICAL_SRC_RESIDENCY].ud != 0;
   default:
      return false;
   }
}

elk_fs_reg
elk_fs_visitor::vgrf(const glsl_type *const type)
{
   int reg_width = dispatch_width / 8;
   return elk_fs_reg(VGRF,
                 alloc.allocate(glsl_count_dword_slots(type, false) * reg_width),
                 elk_type_for_base_type(type));
}

elk_fs_reg::elk_fs_reg(enum elk_reg_file file, unsigned nr)
{
   init();
   this->file = file;
   this->nr = nr;
   this->type = ELK_REGISTER_TYPE_F;
   this->stride = (file == UNIFORM ? 0 : 1);
}

elk_fs_reg::elk_fs_reg(enum elk_reg_file file, unsigned nr, enum elk_reg_type type)
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
elk_fs_visitor::import_uniforms(elk_fs_visitor *v)
{
   this->push_constant_loc = v->push_constant_loc;
   this->uniforms = v->uniforms;
}

enum elk_barycentric_mode
elk_barycentric_mode(nir_intrinsic_instr *intr)
{
   const glsl_interp_mode mode =
      (enum glsl_interp_mode) nir_intrinsic_interp_mode(intr);

   /* Barycentric modes don't make sense for flat inputs. */
   assert(mode != INTERP_MODE_FLAT);

   unsigned bary;
   switch (intr->intrinsic) {
   case nir_intrinsic_load_barycentric_pixel:
   case nir_intrinsic_load_barycentric_at_offset:
      bary = ELK_BARYCENTRIC_PERSPECTIVE_PIXEL;
      break;
   case nir_intrinsic_load_barycentric_centroid:
      bary = ELK_BARYCENTRIC_PERSPECTIVE_CENTROID;
      break;
   case nir_intrinsic_load_barycentric_sample:
   case nir_intrinsic_load_barycentric_at_sample:
      bary = ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE;
      break;
   default:
      unreachable("invalid intrinsic");
   }

   if (mode == INTERP_MODE_NOPERSPECTIVE)
      bary += 3;

   return (enum elk_barycentric_mode) bary;
}

/**
 * Turn one of the two CENTROID barycentric modes into PIXEL mode.
 */
static enum elk_barycentric_mode
centroid_to_pixel(enum elk_barycentric_mode bary)
{
   assert(bary == ELK_BARYCENTRIC_PERSPECTIVE_CENTROID ||
          bary == ELK_BARYCENTRIC_NONPERSPECTIVE_CENTROID);
   return (enum elk_barycentric_mode) ((unsigned) bary - 1);
}

/**
 * Walk backwards from the end of the program looking for a URB write that
 * isn't in control flow, and mark it with EOT.
 *
 * Return true if successful or false if a separate EOT write is needed.
 */
bool
elk_fs_visitor::mark_last_urb_write_with_eot()
{
   foreach_in_list_reverse(elk_fs_inst, prev, &this->instructions) {
      if (prev->opcode == ELK_SHADER_OPCODE_URB_WRITE_LOGICAL) {
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
elk_fs_visitor::emit_gs_thread_end()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   struct elk_gs_prog_data *gs_prog_data = elk_gs_prog_data(prog_data);

   if (gs_compile->control_data_header_size_bits > 0) {
      emit_gs_control_data_bits(this->final_gs_vertex_count);
   }

   const fs_builder abld = fs_builder(this).at_end().annotate("thread end");
   elk_fs_inst *inst;

   if (gs_prog_data->static_vertex_count != -1) {
      /* Try and tag the last URB write with EOT instead of emitting a whole
       * separate write just to finish the thread.
       */
      if (mark_last_urb_write_with_eot())
         return;

      elk_fs_reg srcs[URB_LOGICAL_NUM_SRCS];
      srcs[URB_LOGICAL_SRC_HANDLE] = gs_payload().urb_handles;
      srcs[URB_LOGICAL_SRC_COMPONENTS] = elk_imm_ud(0);
      inst = abld.emit(ELK_SHADER_OPCODE_URB_WRITE_LOGICAL, reg_undef,
                       srcs, ARRAY_SIZE(srcs));
   } else {
      elk_fs_reg srcs[URB_LOGICAL_NUM_SRCS];
      srcs[URB_LOGICAL_SRC_HANDLE] = gs_payload().urb_handles;
      srcs[URB_LOGICAL_SRC_DATA] = this->final_gs_vertex_count;
      srcs[URB_LOGICAL_SRC_COMPONENTS] = elk_imm_ud(1);
      inst = abld.emit(ELK_SHADER_OPCODE_URB_WRITE_LOGICAL, reg_undef,
                       srcs, ARRAY_SIZE(srcs));
   }
   inst->eot = true;
   inst->offset = 0;
}

void
elk_fs_visitor::assign_curb_setup()
{
   unsigned uniform_push_length = DIV_ROUND_UP(stage_prog_data->nr_params, 8);

   unsigned ubo_push_length = 0;
   unsigned ubo_push_start[4];
   for (int i = 0; i < 4; i++) {
      ubo_push_start[i] = 8 * (ubo_push_length + uniform_push_length);
      ubo_push_length += stage_prog_data->ubo_ranges[i].length;
   }

   prog_data->curb_read_length = uniform_push_length + ubo_push_length;

   uint64_t used = 0;

   /* Map the offsets in the UNIFORM file to fixed HW regs. */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
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

	    struct elk_reg elk_reg = elk_vec1_grf(payload().num_regs +
						  constant_nr / 8,
						  constant_nr % 8);
            elk_reg.abs = inst->src[i].abs;
            elk_reg.negate = inst->src[i].negate;

            assert(inst->src[i].stride == 0);
            inst->src[i] = byte_offset(
               retype(elk_reg, inst->src[i].type),
               inst->src[i].offset % 4);
	 }
      }
   }

   uint64_t want_zero = used & stage_prog_data->zero_push_reg;
   if (want_zero) {
      fs_builder ubld = fs_builder(this, 8).exec_all().at(
         cfg->first_block(), cfg->first_block()->start());

      /* push_reg_mask_param is in 32-bit units */
      unsigned mask_param = stage_prog_data->push_reg_mask_param;
      struct elk_reg mask = elk_vec1_grf(payload().num_regs + mask_param / 8,
                                                              mask_param % 8);

      elk_fs_reg b32;
      for (unsigned i = 0; i < 64; i++) {
         if (i % 16 == 0 && (want_zero & BITFIELD64_RANGE(i, 16))) {
            elk_fs_reg shifted = ubld.vgrf(ELK_REGISTER_TYPE_W, 2);
            ubld.SHL(horiz_offset(shifted, 8),
                     byte_offset(retype(mask, ELK_REGISTER_TYPE_W), i / 8),
                     elk_imm_v(0x01234567));
            ubld.SHL(shifted, horiz_offset(shifted, 8), elk_imm_w(8));

            fs_builder ubld16 = ubld.group(16, 0);
            b32 = ubld16.vgrf(ELK_REGISTER_TYPE_D);
            ubld16.group(16, 0).ASR(b32, shifted, elk_imm_w(15));
         }

         if (want_zero & BITFIELD64_BIT(i)) {
            assert(i < prog_data->curb_read_length);
            struct elk_reg push_reg =
               retype(elk_vec8_grf(payload().num_regs + i, 0),
                      ELK_REGISTER_TYPE_D);

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
elk_compute_urb_setup_index(struct elk_wm_prog_data *wm_prog_data)
{
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
                    const struct elk_wm_prog_key *key,
                    struct elk_wm_prog_data *prog_data,
                    const nir_shader *nir)
{
   memset(prog_data->urb_setup, -1, sizeof(prog_data->urb_setup));
   memset(prog_data->urb_setup_channel, 0, sizeof(prog_data->urb_setup_channel));

   int urb_next = 0; /* in vec4s */

   const uint64_t inputs_read =
      nir->info.inputs_read & ~nir->info.per_primitive_inputs;

   /* Figure out where each of the incoming setup attributes lands. */
   if (devinfo->ver >= 6) {
      assert(!nir->info.per_primitive_inputs);

      uint64_t vue_header_bits =
         VARYING_BIT_PSIZ | VARYING_BIT_LAYER | VARYING_BIT_VIEWPORT;

      uint64_t unique_fs_attrs = inputs_read & ELK_FS_VARYING_INPUT_MASK;

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
            if (inputs_read & ELK_FS_VARYING_INPUT_MASK & ~vue_header_bits &
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
         elk_compute_vue_map(devinfo, &prev_stage_vue_map,
                             key->input_slots_valid,
                             nir->info.separate_shader, 1);

         int first_slot =
            elk_compute_first_urb_slot_required(inputs_read,
                                                &prev_stage_vue_map);

         assert(prev_stage_vue_map.num_slots <= first_slot + 32);
         for (int slot = first_slot; slot < prev_stage_vue_map.num_slots;
              slot++) {
            int varying = prev_stage_vue_map.slot_to_varying[slot];
            if (varying != ELK_VARYING_SLOT_PAD &&
                (inputs_read & ELK_FS_VARYING_INPUT_MASK &
                 BITFIELD64_BIT(varying))) {
               prog_data->urb_setup[varying] = slot - first_slot;
            }
         }
         urb_next = prev_stage_vue_map.num_slots - first_slot;
      }
   } else {
      /* FINISHME: The sf doesn't map VS->FS inputs for us very well. */
      for (unsigned int i = 0; i < VARYING_SLOT_MAX; i++) {
         /* Point size is packed into the header, not as a general attribute */
         if (i == VARYING_SLOT_PSIZ)
            continue;

	 if (key->input_slots_valid & BITFIELD64_BIT(i)) {
	    /* The back color slot is skipped when the front color is
	     * also written to.  In addition, some slots can be
	     * written in the vertex shader and not read in the
	     * fragment shader.  So the register number must always be
	     * incremented, mapped or not.
	     */
	    if (_mesa_varying_slot_in_fs((gl_varying_slot) i))
	       prog_data->urb_setup[i] = urb_next;
            urb_next++;
	 }
      }

      /*
       * It's a FS only attribute, and we did interpolation for this attribute
       * in SF thread. So, count it here, too.
       *
       * See compile_sf_prog() for more info.
       */
      if (inputs_read & BITFIELD64_BIT(VARYING_SLOT_PNTC))
         prog_data->urb_setup[VARYING_SLOT_PNTC] = urb_next++;
   }

   prog_data->num_varying_inputs = urb_next - prog_data->num_per_primitive_inputs;
   prog_data->inputs = inputs_read;

   elk_compute_urb_setup_index(prog_data);
}

void
elk_fs_visitor::assign_urb_setup()
{
   assert(stage == MESA_SHADER_FRAGMENT);
   struct elk_wm_prog_data *prog_data = elk_wm_prog_data(this->prog_data);

   int urb_start = payload().num_regs + prog_data->base.curb_read_length;

   /* Offset all the urb_setup[] index by the actual position of the
    * setup regs, now that the location of the constants has been chosen.
    */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == ATTR) {
            /* ATTR elk_fs_reg::nr in the FS is in units of logical scalar
             * inputs each of which consumes 16B on Gfx4-Gfx12.  In
             * single polygon mode this leads to the following layout
             * of the vertex setup plane parameters in the ATTR
             * register file:
             *
             *  elk_fs_reg::nr   Input   Comp0  Comp1  Comp2  Comp3
             *      0       Attr0.x  a1-a0  a2-a0   N/A    a0
             *      1       Attr0.y  a1-a0  a2-a0   N/A    a0
             *      2       Attr0.z  a1-a0  a2-a0   N/A    a0
             *      3       Attr0.w  a1-a0  a2-a0   N/A    a0
             *      4       Attr1.x  a1-a0  a2-a0   N/A    a0
             *     ...
             */
            const unsigned param_width = 1;

            /* Size of a single scalar component of a plane parameter
             * in bytes.
             */
            const unsigned chan_sz = 4;
            struct elk_reg reg;

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
                      reg_unit(devinfo)));
            const unsigned idx = per_prim ? inst->src[i].nr :
               inst->src[i].nr - prog_data->num_per_primitive_inputs;

            /* Translate the offset within the param_width-wide
             * representation described above into an offset and a
             * grf, which contains the plane parameters for the first
             * polygon processed by the thread.
             *
             * Earlier platforms and per-primitive block pack 2 logical
             * input components per 32B register.
             */
            const unsigned grf = base + idx / 2;
            assert(inst->src[i].offset / param_width < REG_SIZE / 2);
            const unsigned delta = (idx % 2) * (REG_SIZE / 2) +
               inst->src[i].offset / (param_width * chan_sz) * chan_sz +
               inst->src[i].offset % chan_sz;
            reg = byte_offset(retype(elk_vec8_grf(grf, 0), inst->src[i].type),
                              delta);

            const unsigned width = inst->src[i].stride == 0 ?
               1 : MIN2(inst->exec_size, 8);
            reg = stride(reg, width * inst->src[i].stride,
                         width, inst->src[i].stride);

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
   this->first_non_payload_grf += prog_data->num_varying_inputs * 2;

   /* Unlike regular attributes, per-primitive attributes have all 4 channels
    * in the same slot, so each GRF can store two slots.
    */
   assert(prog_data->num_per_primitive_inputs % 2 == 0);
   this->first_non_payload_grf += prog_data->num_per_primitive_inputs / 2;
}

void
elk_fs_visitor::convert_attr_sources_to_hw_regs(elk_fs_inst *inst)
{
   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].file == ATTR) {
         assert(inst->src[i].nr == 0);
         int grf = payload().num_regs +
                   prog_data->curb_read_length +
                   inst->src[i].offset / REG_SIZE;

         /* As explained at elk_reg_from_fs_reg, From the Haswell PRM:
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
         struct elk_reg reg =
            stride(byte_offset(retype(elk_vec8_grf(grf, 0), inst->src[i].type),
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
elk_fs_visitor::assign_vs_urb_setup()
{
   struct elk_vs_prog_data *vs_prog_data = elk_vs_prog_data(prog_data);

   assert(stage == MESA_SHADER_VERTEX);

   /* Each attribute is 4 regs. */
   this->first_non_payload_grf += 4 * vs_prog_data->nr_attribute_slots;

   assert(vs_prog_data->base.urb_read_length <= 15);

   /* Rewrite all ATTR file references to the hw grf that they land in. */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
elk_fs_visitor::assign_tcs_urb_setup()
{
   assert(stage == MESA_SHADER_TESS_CTRL);

   /* Rewrite all ATTR file references to HW_REGs. */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
elk_fs_visitor::assign_tes_urb_setup()
{
   assert(stage == MESA_SHADER_TESS_EVAL);

   struct elk_vue_prog_data *vue_prog_data = elk_vue_prog_data(prog_data);

   first_non_payload_grf += 8 * vue_prog_data->urb_read_length;

   /* Rewrite all ATTR file references to HW_REGs. */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      convert_attr_sources_to_hw_regs(inst);
   }
}

void
elk_fs_visitor::assign_gs_urb_setup()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   struct elk_vue_prog_data *vue_prog_data = elk_vue_prog_data(prog_data);

   first_non_payload_grf +=
      8 * vue_prog_data->urb_read_length * nir->info.gs.vertices_in;

   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      /* Rewrite all ATTR file references to GRFs. */
      convert_attr_sources_to_hw_regs(inst);
   }
}


/**
 * Split large virtual GRFs into separate components if we can.
 *
 * This pass aggressively splits VGRFs into as small a chunks as possible,
 * down to single registers if it can.  If no VGRFs can be split, we return
 * false so this pass can safely be used inside an optimization loop.  We
 * want to split, because virtual GRFs are what we register allocate and
 * spill (due to contiguousness requirements for some instructions), and
 * they're what we naturally generate in the codegen process, but most
 * virtual GRFs don't actually need to be contiguous sets of GRFs.  If we
 * split, we'll end up with reduced live intervals and better dead code
 * elimination and coalescing.
 */
bool
elk_fs_visitor::split_virtual_grfs()
{
   /* Compact the register file so we eliminate dead vgrfs.  This
    * only defines split points for live registers, so if we have
    * too large dead registers they will hit assertions later.
    */
   compact_virtual_grfs();

   unsigned num_vars = this->alloc.count;

   /* Count the total number of registers */
   unsigned reg_count = 0;
   unsigned vgrf_to_reg[num_vars];
   for (unsigned i = 0; i < num_vars; i++) {
      vgrf_to_reg[i] = reg_count;
      reg_count += alloc.sizes[i];
   }

   /* An array of "split points".  For each register slot, this indicates
    * if this slot can be separated from the previous slot.  Every time an
    * instruction uses multiple elements of a register (as a source or
    * destination), we mark the used slots as inseparable.  Then we go
    * through and split the registers into the smallest pieces we can.
    */
   bool *split_points = new bool[reg_count];
   memset(split_points, 0, reg_count * sizeof(*split_points));

   /* Mark all used registers as fully splittable */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      if (inst->dst.file == VGRF) {
         unsigned reg = vgrf_to_reg[inst->dst.nr];
         for (unsigned j = 1; j < this->alloc.sizes[inst->dst.nr]; j++)
            split_points[reg + j] = true;
      }

      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF) {
            unsigned reg = vgrf_to_reg[inst->src[i].nr];
            for (unsigned j = 1; j < this->alloc.sizes[inst->src[i].nr]; j++)
               split_points[reg + j] = true;
         }
      }
   }

   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      /* We fix up undef instructions later */
      if (inst->opcode == ELK_SHADER_OPCODE_UNDEF) {
         assert(inst->dst.file == VGRF);
         continue;
      }

      if (inst->dst.file == VGRF) {
         unsigned reg = vgrf_to_reg[inst->dst.nr] + inst->dst.offset / REG_SIZE;
         for (unsigned j = 1; j < regs_written(inst); j++)
            split_points[reg + j] = false;
      }
      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF) {
            unsigned reg = vgrf_to_reg[inst->src[i].nr] + inst->src[i].offset / REG_SIZE;
            for (unsigned j = 1; j < regs_read(inst, i); j++)
               split_points[reg + j] = false;
         }
      }
   }

   /* Bitset of which registers have been split */
   bool *vgrf_has_split = new bool[num_vars];
   memset(vgrf_has_split, 0, num_vars * sizeof(*vgrf_has_split));

   unsigned *new_virtual_grf = new unsigned[reg_count];
   unsigned *new_reg_offset = new unsigned[reg_count];

   unsigned reg = 0;
   bool has_splits = false;
   for (unsigned i = 0; i < num_vars; i++) {
      /* The first one should always be 0 as a quick sanity check. */
      assert(split_points[reg] == false);

      /* j = 0 case */
      new_reg_offset[reg] = 0;
      reg++;
      unsigned offset = 1;

      /* j > 0 case */
      for (unsigned j = 1; j < alloc.sizes[i]; j++) {
         /* If this is a split point, reset the offset to 0 and allocate a
          * new virtual GRF for the previous offset many registers
          */
         if (split_points[reg]) {
            has_splits = true;
            vgrf_has_split[i] = true;
            assert(offset <= MAX_VGRF_SIZE(devinfo));
            unsigned grf = alloc.allocate(offset);
            for (unsigned k = reg - offset; k < reg; k++)
               new_virtual_grf[k] = grf;
            offset = 0;
         }
         new_reg_offset[reg] = offset;
         offset++;
         reg++;
      }

      /* The last one gets the original register number */
      assert(offset <= MAX_VGRF_SIZE(devinfo));
      alloc.sizes[i] = offset;
      for (unsigned k = reg - offset; k < reg; k++)
         new_virtual_grf[k] = i;
   }
   assert(reg == reg_count);

   bool progress;
   if (!has_splits) {
      progress = false;
      goto cleanup;
   }

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      if (inst->opcode == ELK_SHADER_OPCODE_UNDEF) {
         assert(inst->dst.file == VGRF);
         if (vgrf_has_split[inst->dst.nr]) {
            const fs_builder ibld(this, block, inst);
            assert(inst->size_written % REG_SIZE == 0);
            unsigned reg_offset = inst->dst.offset / REG_SIZE;
            unsigned size_written = 0;
            while (size_written < inst->size_written) {
               reg = vgrf_to_reg[inst->dst.nr] + reg_offset + size_written / REG_SIZE;
               elk_fs_inst *undef =
                  ibld.UNDEF(
                     byte_offset(elk_fs_reg(VGRF, new_virtual_grf[reg], inst->dst.type),
                                 new_reg_offset[reg] * REG_SIZE));
               undef->size_written =
                  MIN2(inst->size_written - size_written, undef->size_written);
               assert(undef->size_written % REG_SIZE == 0);
               size_written += undef->size_written;
            }
            inst->remove(block);
         } else {
            reg = vgrf_to_reg[inst->dst.nr];
            assert(new_reg_offset[reg] == 0);
            assert(new_virtual_grf[reg] == inst->dst.nr);
         }
         continue;
      }

      if (inst->dst.file == VGRF) {
         reg = vgrf_to_reg[inst->dst.nr] + inst->dst.offset / REG_SIZE;
         if (vgrf_has_split[inst->dst.nr]) {
            inst->dst.nr = new_virtual_grf[reg];
            inst->dst.offset = new_reg_offset[reg] * REG_SIZE +
                               inst->dst.offset % REG_SIZE;
            assert(new_reg_offset[reg] < alloc.sizes[new_virtual_grf[reg]]);
         } else {
            assert(new_reg_offset[reg] == inst->dst.offset / REG_SIZE);
            assert(new_virtual_grf[reg] == inst->dst.nr);
         }
      }
      for (unsigned i = 0; i < inst->sources; i++) {
	 if (inst->src[i].file != VGRF)
            continue;

         reg = vgrf_to_reg[inst->src[i].nr] + inst->src[i].offset / REG_SIZE;
         if (vgrf_has_split[inst->src[i].nr]) {
            inst->src[i].nr = new_virtual_grf[reg];
            inst->src[i].offset = new_reg_offset[reg] * REG_SIZE +
                                  inst->src[i].offset % REG_SIZE;
            assert(new_reg_offset[reg] < alloc.sizes[new_virtual_grf[reg]]);
         } else {
            assert(new_reg_offset[reg] == inst->src[i].offset / REG_SIZE);
            assert(new_virtual_grf[reg] == inst->src[i].nr);
         }
      }
   }
   invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL | DEPENDENCY_VARIABLES);

   progress = true;

cleanup:
   delete[] split_points;
   delete[] vgrf_has_split;
   delete[] new_virtual_grf;
   delete[] new_reg_offset;

   return progress;
}

/**
 * Remove unused virtual GRFs and compact the vgrf_* arrays.
 *
 * During code generation, we create tons of temporary variables, many of
 * which get immediately killed and are never used again.  Yet, in later
 * optimization and analysis passes, such as compute_live_intervals, we need
 * to loop over all the virtual GRFs.  Compacting them can save a lot of
 * overhead.
 */
bool
elk_fs_visitor::compact_virtual_grfs()
{
   bool progress = false;
   int *remap_table = new int[this->alloc.count];
   memset(remap_table, -1, this->alloc.count * sizeof(int));

   /* Mark which virtual GRFs are used. */
   foreach_block_and_inst(block, const elk_fs_inst, inst, cfg) {
      if (inst->dst.file == VGRF)
         remap_table[inst->dst.nr] = 0;

      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF)
            remap_table[inst->src[i].nr] = 0;
      }
   }

   /* Compact the GRF arrays. */
   int new_index = 0;
   for (unsigned i = 0; i < this->alloc.count; i++) {
      if (remap_table[i] == -1) {
         /* We just found an unused register.  This means that we are
          * actually going to compact something.
          */
         progress = true;
      } else {
         remap_table[i] = new_index;
         alloc.sizes[new_index] = alloc.sizes[i];
         invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL | DEPENDENCY_VARIABLES);
         ++new_index;
      }
   }

   this->alloc.count = new_index;

   /* Patch all the instructions to use the newly renumbered registers */
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      if (inst->dst.file == VGRF)
         inst->dst.nr = remap_table[inst->dst.nr];

      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF)
            inst->src[i].nr = remap_table[inst->src[i].nr];
      }
   }

   /* Patch all the references to delta_xy, since they're used in register
    * allocation.  If they're unused, switch them to BAD_FILE so we don't
    * think some random VGRF is delta_xy.
    */
   for (unsigned i = 0; i < ARRAY_SIZE(delta_xy); i++) {
      if (delta_xy[i].file == VGRF) {
         if (remap_table[delta_xy[i].nr] != -1) {
            delta_xy[i].nr = remap_table[delta_xy[i].nr];
         } else {
            delta_xy[i].file = BAD_FILE;
         }
      }
   }

   delete[] remap_table;

   return progress;
}

int
elk_get_subgroup_id_param_index(const intel_device_info *devinfo,
                                const elk_stage_prog_data *prog_data)
{
   if (prog_data->nr_params == 0)
      return -1;

   /* The local thread id is always the last parameter in the list */
   uint32_t last_param = prog_data->param[prog_data->nr_params - 1];
   if (last_param == ELK_PARAM_BUILTIN_SUBGROUP_ID)
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
elk_fs_visitor::assign_constant_locations()
{
   /* Only the first compile gets to decide on locations. */
   if (push_constant_loc)
      return;

   push_constant_loc = ralloc_array(mem_ctx, int, uniforms);
   for (unsigned u = 0; u < uniforms; u++)
      push_constant_loc[u] = u;

   /* Now that we know how many regular uniforms we'll push, reduce the
    * UBO push ranges so we don't exceed the 3DSTATE_CONSTANT limits.
    */
   /* For gen4/5:
    * Only allow 16 registers (128 uniform components) as push constants.
    *
    * If changing this value, note the limitation about total_regs in
    * elk_curbe.c/crocus_state.c
    */
   const unsigned max_push_length = compiler->devinfo->ver < 6 ? 16 : 64;
   unsigned push_length = DIV_ROUND_UP(stage_prog_data->nr_params, 8);
   for (int i = 0; i < 4; i++) {
      struct elk_ubo_range *range = &prog_data->ubo_ranges[i];

      if (push_length + range->length > max_push_length)
         range->length = max_push_length - push_length;

      push_length += range->length;
   }
   assert(push_length <= max_push_length);
}

bool
elk_fs_visitor::get_pull_locs(const elk_fs_reg &src,
                          unsigned *out_surf_index,
                          unsigned *out_pull_index)
{
   assert(src.file == UNIFORM);

   if (src.nr < UBO_START)
      return false;

   const struct elk_ubo_range *range =
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
 * Replace UNIFORM register file access with either UNIFORM_PULL_CONSTANT_LOAD
 * or VARYING_PULL_CONSTANT_LOAD instructions which load values into VGRFs.
 */
bool
elk_fs_visitor::lower_constant_loads()
{
   unsigned index, pull_index;
   bool progress = false;

   foreach_block_and_inst_safe (block, elk_fs_inst, inst, cfg) {
      /* Set up the annotation tracking for new generated instructions. */
      const fs_builder ibld(this, block, inst);

      for (int i = 0; i < inst->sources; i++) {
	 if (inst->src[i].file != UNIFORM)
	    continue;

         /* We'll handle this case later */
         if (inst->opcode == ELK_SHADER_OPCODE_MOV_INDIRECT && i == 0)
            continue;

         if (!get_pull_locs(inst->src[i], &index, &pull_index))
	    continue;

         assert(inst->src[i].stride == 0);

         const unsigned block_sz = 64; /* Fetch one cacheline at a time. */
         const fs_builder ubld = ibld.exec_all().group(block_sz / 4, 0);
         const elk_fs_reg dst = ubld.vgrf(ELK_REGISTER_TYPE_UD);
         const unsigned base = pull_index * 4;

         elk_fs_reg srcs[PULL_UNIFORM_CONSTANT_SRCS];
         srcs[PULL_UNIFORM_CONSTANT_SRC_SURFACE] = elk_imm_ud(index);
         srcs[PULL_UNIFORM_CONSTANT_SRC_OFFSET]  = elk_imm_ud(base & ~(block_sz - 1));
         srcs[PULL_UNIFORM_CONSTANT_SRC_SIZE]    = elk_imm_ud(block_sz);


         ubld.emit(ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD, dst,
                   srcs, PULL_UNIFORM_CONSTANT_SRCS);

         /* Rewrite the instruction to use the temporary VGRF. */
         inst->src[i].file = VGRF;
         inst->src[i].nr = dst.nr;
         inst->src[i].offset = (base & (block_sz - 1)) +
                               inst->src[i].offset % 4;

         progress = true;
      }

      if (inst->opcode == ELK_SHADER_OPCODE_MOV_INDIRECT &&
          inst->src[0].file == UNIFORM) {

         if (!get_pull_locs(inst->src[0], &index, &pull_index))
            continue;

         VARYING_PULL_CONSTANT_LOAD(ibld, inst->dst,
                                    elk_imm_ud(index),
                                    elk_fs_reg() /* surface_handle */,
                                    inst->src[1],
                                    pull_index * 4, 4, 1);
         inst->remove(block);

         progress = true;
      }
   }
   invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

static uint64_t
src_as_uint(const elk_fs_reg &src)
{
   assert(src.file == IMM);

   switch (src.type) {
   case ELK_REGISTER_TYPE_W:
      return (uint64_t)(int16_t)(src.ud & 0xffff);

   case ELK_REGISTER_TYPE_UW:
      return (uint64_t)(uint16_t)(src.ud & 0xffff);

   case ELK_REGISTER_TYPE_D:
      return (uint64_t)src.d;

   case ELK_REGISTER_TYPE_UD:
      return (uint64_t)src.ud;

   case ELK_REGISTER_TYPE_Q:
      return src.d64;

   case ELK_REGISTER_TYPE_UQ:
      return src.u64;

   default:
      unreachable("Invalid integer type.");
   }
}

static elk_fs_reg
elk_imm_for_type(uint64_t value, enum elk_reg_type type)
{
   switch (type) {
   case ELK_REGISTER_TYPE_W:
      return elk_imm_w(value);

   case ELK_REGISTER_TYPE_UW:
      return elk_imm_uw(value);

   case ELK_REGISTER_TYPE_D:
      return elk_imm_d(value);

   case ELK_REGISTER_TYPE_UD:
      return elk_imm_ud(value);

   case ELK_REGISTER_TYPE_Q:
      return elk_imm_d(value);

   case ELK_REGISTER_TYPE_UQ:
      return elk_imm_uq(value);

   default:
      unreachable("Invalid integer type.");
   }
}

bool
elk_fs_visitor::opt_algebraic()
{
   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      switch (inst->opcode) {
      case ELK_OPCODE_MOV:
         if (!devinfo->has_64bit_float &&
             inst->dst.type == ELK_REGISTER_TYPE_DF) {
            assert(inst->dst.type == inst->src[0].type);
            assert(!inst->saturate);
            assert(!inst->src[0].abs);
            assert(!inst->src[0].negate);
            const elk::fs_builder ibld(this, block, inst);

            if (!inst->is_partial_write())
               ibld.emit_undef_for_dst(inst);

            ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_F, 1),
                     subscript(inst->src[0], ELK_REGISTER_TYPE_F, 1));
            ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_F, 0),
                     subscript(inst->src[0], ELK_REGISTER_TYPE_F, 0));

            inst->remove(block);
            progress = true;
         }

         if (!devinfo->has_64bit_int &&
             (inst->dst.type == ELK_REGISTER_TYPE_UQ ||
              inst->dst.type == ELK_REGISTER_TYPE_Q)) {
            assert(inst->dst.type == inst->src[0].type);
            assert(!inst->saturate);
            assert(!inst->src[0].abs);
            assert(!inst->src[0].negate);
            const elk::fs_builder ibld(this, block, inst);

            if (!inst->is_partial_write())
               ibld.emit_undef_for_dst(inst);

            ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 1),
                     subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 1));
            ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 0),
                     subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0));

            inst->remove(block);
            progress = true;
         }

         if ((inst->conditional_mod == ELK_CONDITIONAL_Z ||
              inst->conditional_mod == ELK_CONDITIONAL_NZ) &&
             inst->dst.is_null() &&
             (inst->src[0].abs || inst->src[0].negate)) {
            inst->src[0].abs = false;
            inst->src[0].negate = false;
            progress = true;
            break;
         }

         if (inst->src[0].file != IMM)
            break;

         if (inst->saturate) {
            /* Full mixed-type saturates don't happen.  However, we can end up
             * with things like:
             *
             *    mov.sat(8) g21<1>DF       -1F
             *
             * Other mixed-size-but-same-base-type cases may also be possible.
             */
            if (inst->dst.type != inst->src[0].type &&
                inst->dst.type != ELK_REGISTER_TYPE_DF &&
                inst->src[0].type != ELK_REGISTER_TYPE_F)
               assert(!"unimplemented: saturate mixed types");

            if (elk_saturate_immediate(inst->src[0].type,
                                       &inst->src[0].as_elk_reg())) {
               inst->saturate = false;
               progress = true;
            }
         }
         break;

      case ELK_OPCODE_MUL:
         if (inst->src[1].file != IMM)
            continue;

         if (elk_reg_type_is_floating_point(inst->src[1].type))
            break;

         /* From the BDW PRM, Vol 2a, "mul - Multiply":
          *
          *    "When multiplying integer datatypes, if src0 is DW and src1
          *    is W, irrespective of the destination datatype, the
          *    accumulator maintains full 48-bit precision."
          *    ...
          *    "When multiplying integer data types, if one of the sources
          *    is a DW, the resulting full precision data is stored in
          *    the accumulator."
          *
          * There are also similar notes in earlier PRMs.
          *
          * The MOV instruction can copy the bits of the source, but it
          * does not clear the higher bits of the accumulator. So, because
          * we might use the full accumulator in the MUL/MACH macro, we
          * shouldn't replace such MULs with MOVs.
          */
         if ((elk_reg_type_to_size(inst->src[0].type) == 4 ||
              elk_reg_type_to_size(inst->src[1].type) == 4) &&
             (inst->dst.is_accumulator() ||
              inst->writes_accumulator_implicitly(devinfo)))
            break;

         /* a * 1.0 = a */
         if (inst->src[1].is_one()) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         /* a * -1.0 = -a */
         if (inst->src[1].is_negative_one()) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0].negate = !inst->src[0].negate;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         break;
      case ELK_OPCODE_ADD:
         if (inst->src[1].file != IMM)
            continue;

         if (elk_reg_type_is_integer(inst->src[1].type) &&
             inst->src[1].is_zero()) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         if (inst->src[0].file == IMM) {
            assert(inst->src[0].type == ELK_REGISTER_TYPE_F);
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0].f += inst->src[1].f;
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }
         break;

      case ELK_OPCODE_AND:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            const uint64_t src0 = src_as_uint(inst->src[0]);
            const uint64_t src1 = src_as_uint(inst->src[1]);

            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = elk_imm_for_type(src0 & src1, inst->dst.type);
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         break;

      case ELK_OPCODE_OR:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            const uint64_t src0 = src_as_uint(inst->src[0]);
            const uint64_t src1 = src_as_uint(inst->src[1]);

            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[0] = elk_imm_for_type(src0 | src1, inst->dst.type);
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }

         if (inst->src[0].equals(inst->src[1]) ||
             inst->src[1].is_zero()) {
            /* On Gfx8+, the OR instruction can have a source modifier that
             * performs logical not on the operand.  Cases of 'OR r0, ~r1, 0'
             * or 'OR r0, ~r1, ~r1' should become a NOT instead of a MOV.
             */
            if (inst->src[0].negate) {
               inst->opcode = ELK_OPCODE_NOT;
               inst->sources = 1;
               inst->src[0].negate = false;
            } else {
               inst->opcode = ELK_OPCODE_MOV;
               inst->sources = 1;
            }
            inst->src[1] = reg_undef;
            progress = true;
            break;
         }
         break;
      case ELK_OPCODE_CMP:
         if ((inst->conditional_mod == ELK_CONDITIONAL_Z ||
              inst->conditional_mod == ELK_CONDITIONAL_NZ) &&
             inst->src[1].is_zero() &&
             (inst->src[0].abs || inst->src[0].negate)) {
            inst->src[0].abs = false;
            inst->src[0].negate = false;
            progress = true;
            break;
         }
         break;
      case ELK_OPCODE_SEL:
         if (!devinfo->has_64bit_float &&
             !devinfo->has_64bit_int &&
             (inst->dst.type == ELK_REGISTER_TYPE_DF ||
              inst->dst.type == ELK_REGISTER_TYPE_UQ ||
              inst->dst.type == ELK_REGISTER_TYPE_Q)) {
            assert(inst->dst.type == inst->src[0].type);
            assert(!inst->saturate);
            assert(!inst->src[0].abs && !inst->src[0].negate);
            assert(!inst->src[1].abs && !inst->src[1].negate);
            const elk::fs_builder ibld(this, block, inst);

            if (!inst->is_partial_write())
               ibld.emit_undef_for_dst(inst);

            set_predicate(inst->predicate,
                          ibld.SEL(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 0),
                                   subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0),
                                   subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 0)));
            set_predicate(inst->predicate,
                          ibld.SEL(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 1),
                                   subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 1),
                                   subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 1)));

            inst->remove(block);
            progress = true;
         }
         if (inst->src[0].equals(inst->src[1])) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->src[1] = reg_undef;
            inst->predicate = ELK_PREDICATE_NONE;
            inst->predicate_inverse = false;
            progress = true;
         } else if (inst->saturate && inst->src[1].file == IMM) {
            switch (inst->conditional_mod) {
            case ELK_CONDITIONAL_LE:
            case ELK_CONDITIONAL_L:
               switch (inst->src[1].type) {
               case ELK_REGISTER_TYPE_F:
                  if (inst->src[1].f >= 1.0f) {
                     inst->opcode = ELK_OPCODE_MOV;
                     inst->sources = 1;
                     inst->src[1] = reg_undef;
                     inst->conditional_mod = ELK_CONDITIONAL_NONE;
                     progress = true;
                  }
                  break;
               default:
                  break;
               }
               break;
            case ELK_CONDITIONAL_GE:
            case ELK_CONDITIONAL_G:
               switch (inst->src[1].type) {
               case ELK_REGISTER_TYPE_F:
                  if (inst->src[1].f <= 0.0f) {
                     inst->opcode = ELK_OPCODE_MOV;
                     inst->sources = 1;
                     inst->src[1] = reg_undef;
                     inst->conditional_mod = ELK_CONDITIONAL_NONE;
                     progress = true;
                  }
                  break;
               default:
                  break;
               }
            default:
               break;
            }
         }
         break;
      case ELK_OPCODE_MAD:
         if (inst->src[0].type != ELK_REGISTER_TYPE_F ||
             inst->src[1].type != ELK_REGISTER_TYPE_F ||
             inst->src[2].type != ELK_REGISTER_TYPE_F)
            break;
         if (inst->src[1].is_one()) {
            inst->opcode = ELK_OPCODE_ADD;
            inst->sources = 2;
            inst->src[1] = inst->src[2];
            inst->src[2] = reg_undef;
            progress = true;
         } else if (inst->src[2].is_one()) {
            inst->opcode = ELK_OPCODE_ADD;
            inst->sources = 2;
            inst->src[2] = reg_undef;
            progress = true;
         }
         break;
      case ELK_OPCODE_SHL:
         if (inst->src[0].file == IMM && inst->src[1].file == IMM) {
            /* It's not currently possible to generate this, and this constant
             * folding does not handle it.
             */
            assert(!inst->saturate);

            elk_fs_reg result;

            switch (type_sz(inst->src[0].type)) {
            case 2:
               result = elk_imm_uw(0x0ffff & (inst->src[0].ud << (inst->src[1].ud & 0x1f)));
               break;
            case 4:
               result = elk_imm_ud(inst->src[0].ud << (inst->src[1].ud & 0x1f));
               break;
            case 8:
               result = elk_imm_uq(inst->src[0].u64 << (inst->src[1].ud & 0x3f));
               break;
            default:
               /* Just in case a future platform re-enables B or UB types. */
               unreachable("Invalid source size.");
            }

            inst->opcode = ELK_OPCODE_MOV;
            inst->src[0] = retype(result, inst->dst.type);
            inst->src[1] = reg_undef;
            inst->sources = 1;

            progress = true;
         }
         break;

      case ELK_SHADER_OPCODE_BROADCAST:
         if (is_uniform(inst->src[0])) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;
         } else if (inst->src[1].file == IMM) {
            inst->opcode = ELK_OPCODE_MOV;
            /* It's possible that the selected component will be too large and
             * overflow the register.  This can happen if someone does a
             * readInvocation() from GLSL or SPIR-V and provides an OOB
             * invocationIndex.  If this happens and we some how manage
             * to constant fold it in and get here, then component() may cause
             * us to start reading outside of the VGRF which will lead to an
             * assert later.  Instead, just let it wrap around if it goes over
             * exec_size.
             */
            const unsigned comp = inst->src[1].ud & (inst->exec_size - 1);
            inst->src[0] = component(inst->src[0], comp);
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;
         }
         break;

      case ELK_SHADER_OPCODE_SHUFFLE:
         if (is_uniform(inst->src[0])) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->sources = 1;
            progress = true;
         } else if (inst->src[1].file == IMM) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->src[0] = component(inst->src[0],
                                     inst->src[1].ud);
            inst->sources = 1;
            progress = true;
         }
         break;

      default:
	 break;
      }

      /* Ensure that the correct source has the immediate value. 2-source
       * instructions must have the immediate in src[1]. On Gfx12 and later,
       * some 3-source instructions can have the immediate in src[0] or
       * src[2]. It's complicated, so don't mess with 3-source instructions
       * here.
       */
      if (progress && inst->sources == 2 && inst->is_commutative()) {
         if (inst->src[0].file == IMM) {
            elk_fs_reg tmp = inst->src[1];
            inst->src[1] = inst->src[0];
            inst->src[0] = tmp;
         }
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTION_DATA_FLOW |
                          DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}

static unsigned
load_payload_sources_read_for_size(elk_fs_inst *lp, unsigned size_read)
{
   assert(lp->opcode == ELK_SHADER_OPCODE_LOAD_PAYLOAD);
   assert(size_read >= lp->header_size * REG_SIZE);

   unsigned i;
   unsigned size = lp->header_size * REG_SIZE;
   for (i = lp->header_size; size < size_read && i < lp->sources; i++)
      size += lp->exec_size * type_sz(lp->src[i].type);

   /* Size read must cover exactly a subset of sources. */
   assert(size == size_read);
   return i;
}

/**
 * Optimize sample messages that have constant zero values for the trailing
 * parameters. We can just reduce the message length for these
 * instructions instead of reserving a register for it. Trailing parameters
 * that aren't sent default to zero anyway. This will cause the dead code
 * eliminator to remove the MOV instruction that would otherwise be emitted to
 * set up the zero value.
 */
bool
elk_fs_visitor::opt_zero_samples()
{
   /* Implementation supports only SENDs, so applicable to Gfx7+ only. */
   assert(devinfo->ver >= 7);

   bool progress = false;

   foreach_block_and_inst(block, elk_fs_inst, send, cfg) {
      if (send->opcode != ELK_SHADER_OPCODE_SEND ||
          send->sfid != ELK_SFID_SAMPLER)
         continue;

      /* Wa_14012688258:
       *
       * Don't trim zeros at the end of payload for sample operations
       * in cube and cube arrays.
       */
      if (send->keep_payload_trailing_zeros)
         continue;

      elk_fs_inst *lp = (elk_fs_inst *) send->prev;

      if (lp->is_head_sentinel() || lp->opcode != ELK_SHADER_OPCODE_LOAD_PAYLOAD)
         continue;

      /* How much of the payload are actually read by this SEND. */
      const unsigned params =
         load_payload_sources_read_for_size(lp, send->mlen * REG_SIZE);

      /* We don't want to remove the message header or the first parameter.
       * Removing the first parameter is not allowed, see the Haswell PRM
       * volume 7, page 149:
       *
       *     "Parameter 0 is required except for the sampleinfo message, which
       *      has no parameter 0"
       */
      const unsigned first_param_idx = lp->header_size;
      unsigned zero_size = 0;
      for (unsigned i = params - 1; i > first_param_idx; i--) {
         if (lp->src[i].file != BAD_FILE && !lp->src[i].is_zero())
            break;
         zero_size += lp->exec_size * type_sz(lp->src[i].type) * lp->dst.stride;
      }

      const unsigned zero_len = zero_size / (reg_unit(devinfo) * REG_SIZE);
      if (zero_len > 0) {
         send->mlen -= zero_len;
         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}

/**
 * Remove redundant or useless halts.
 *
 * For example, we can eliminate halts in the following sequence:
 *
 * halt        (redundant with the next halt)
 * halt        (useless; jumps to the next instruction)
 * halt-target
 */
bool
elk_fs_visitor::opt_redundant_halt()
{
   bool progress = false;

   unsigned halt_count = 0;
   elk_fs_inst *halt_target = NULL;
   elk_bblock_t *halt_target_block = NULL;
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      if (inst->opcode == ELK_OPCODE_HALT)
         halt_count++;

      if (inst->opcode == ELK_SHADER_OPCODE_HALT_TARGET) {
         halt_target = inst;
         halt_target_block = block;
         break;
      }
   }

   if (!halt_target) {
      assert(halt_count == 0);
      return false;
   }

   /* Delete any HALTs immediately before the halt target. */
   for (elk_fs_inst *prev = (elk_fs_inst *) halt_target->prev;
        !prev->is_head_sentinel() && prev->opcode == ELK_OPCODE_HALT;
        prev = (elk_fs_inst *) halt_target->prev) {
      prev->remove(halt_target_block);
      halt_count--;
      progress = true;
   }

   if (halt_count == 0) {
      halt_target->remove(halt_target_block);
      progress = true;
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

/**
 * Compute a bitmask with GRF granularity with a bit set for each GRF starting
 * from \p r.offset which overlaps the region starting at \p s.offset and
 * spanning \p ds bytes.
 */
static inline unsigned
mask_relative_to(const elk_fs_reg &r, const elk_fs_reg &s, unsigned ds)
{
   const int rel_offset = reg_offset(s) - reg_offset(r);
   const int shift = rel_offset / REG_SIZE;
   const unsigned n = DIV_ROUND_UP(rel_offset % REG_SIZE + ds, REG_SIZE);
   assert(reg_space(r) == reg_space(s) &&
          shift >= 0 && shift < int(8 * sizeof(unsigned)));
   return ((1 << n) - 1) << shift;
}

bool
elk_fs_visitor::compute_to_mrf()
{
   bool progress = false;
   int next_ip = 0;

   /* No MRFs on Gen >= 7. */
   if (devinfo->ver >= 7)
      return false;

   const fs_live_variables &live = live_analysis.require();

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      int ip = next_ip;
      next_ip++;

      if (inst->opcode != ELK_OPCODE_MOV ||
	  inst->is_partial_write() ||
	  inst->dst.file != MRF || inst->src[0].file != VGRF ||
	  inst->dst.type != inst->src[0].type ||
	  inst->src[0].abs || inst->src[0].negate ||
          !inst->src[0].is_contiguous() ||
          inst->src[0].offset % REG_SIZE != 0)
	 continue;

      /* Can't compute-to-MRF this GRF if someone else was going to
       * read it later.
       */
      if (live.vgrf_end[inst->src[0].nr] > ip)
	 continue;

      /* Found a move of a GRF to a MRF.  Let's see if we can go rewrite the
       * things that computed the value of all GRFs of the source region.  The
       * regs_left bitset keeps track of the registers we haven't yet found a
       * generating instruction for.
       */
      unsigned regs_left = (1 << regs_read(inst, 0)) - 1;

      foreach_inst_in_block_reverse_starting_from(elk_fs_inst, scan_inst, inst) {
         if (regions_overlap(scan_inst->dst, scan_inst->size_written,
                             inst->src[0], inst->size_read(0))) {
	    /* Found the last thing to write our reg we want to turn
	     * into a compute-to-MRF.
	     */

	    /* If this one instruction didn't populate all the
	     * channels, bail.  We might be able to rewrite everything
	     * that writes that reg, but it would require smarter
	     * tracking.
	     */
	    if (scan_inst->is_partial_write())
	       break;

            /* Handling things not fully contained in the source of the copy
             * would need us to understand coalescing out more than one MOV at
             * a time.
             */
            if (!region_contained_in(scan_inst->dst, scan_inst->size_written,
                                     inst->src[0], inst->size_read(0)))
               break;

	    /* SEND instructions can't have MRF as a destination. */
	    if (scan_inst->mlen)
	       break;

	    if (devinfo->ver == 6) {
	       /* gfx6 math instructions must have the destination be
		* GRF, so no compute-to-MRF for them.
		*/
	       if (scan_inst->is_math()) {
		  break;
	       }
	    }

            /* Clear the bits for any registers this instruction overwrites. */
            regs_left &= ~mask_relative_to(
               inst->src[0], scan_inst->dst, scan_inst->size_written);
            if (!regs_left)
               break;
	 }

	 /* We don't handle control flow here.  Most computation of
	  * values that end up in MRFs are shortly before the MRF
	  * write anyway.
	  */
	 if (block->start() == scan_inst)
	    break;

	 /* You can't read from an MRF, so if someone else reads our
	  * MRF's source GRF that we wanted to rewrite, that stops us.
	  */
	 bool interfered = false;
	 for (int i = 0; i < scan_inst->sources; i++) {
            if (regions_overlap(scan_inst->src[i], scan_inst->size_read(i),
                                inst->src[0], inst->size_read(0))) {
	       interfered = true;
	    }
	 }
	 if (interfered)
	    break;

         if (regions_overlap(scan_inst->dst, scan_inst->size_written,
                             inst->dst, inst->size_written)) {
	    /* If somebody else writes our MRF here, we can't
	     * compute-to-MRF before that.
	     */
            break;
         }

         if (scan_inst->mlen > 0 && scan_inst->base_mrf != -1 &&
             regions_overlap(elk_fs_reg(MRF, scan_inst->base_mrf), scan_inst->mlen * REG_SIZE,
                             inst->dst, inst->size_written)) {
	    /* Found a SEND instruction, which means that there are
	     * live values in MRFs from base_mrf to base_mrf +
	     * scan_inst->mlen - 1.  Don't go pushing our MRF write up
	     * above it.
	     */
            break;
         }
      }

      if (regs_left)
         continue;

      /* Found all generating instructions of our MRF's source value, so it
       * should be safe to rewrite them to point to the MRF directly.
       */
      regs_left = (1 << regs_read(inst, 0)) - 1;

      foreach_inst_in_block_reverse_starting_from(elk_fs_inst, scan_inst, inst) {
         if (regions_overlap(scan_inst->dst, scan_inst->size_written,
                             inst->src[0], inst->size_read(0))) {
            /* Clear the bits for any registers this instruction overwrites. */
            regs_left &= ~mask_relative_to(
               inst->src[0], scan_inst->dst, scan_inst->size_written);

            const unsigned rel_offset = reg_offset(scan_inst->dst) -
                                        reg_offset(inst->src[0]);

            if (inst->dst.nr & ELK_MRF_COMPR4) {
               /* Apply the same address transformation done by the hardware
                * for COMPR4 MRF writes.
                */
               assert(rel_offset < 2 * REG_SIZE);
               scan_inst->dst.nr = inst->dst.nr + rel_offset / REG_SIZE * 4;

               /* Clear the COMPR4 bit if the generating instruction is not
                * compressed.
                */
               if (scan_inst->size_written < 2 * REG_SIZE)
                  scan_inst->dst.nr &= ~ELK_MRF_COMPR4;

            } else {
               /* Calculate the MRF number the result of this instruction is
                * ultimately written to.
                */
               scan_inst->dst.nr = inst->dst.nr + rel_offset / REG_SIZE;
            }

            scan_inst->dst.file = MRF;
            scan_inst->dst.offset = inst->dst.offset + rel_offset % REG_SIZE;
            scan_inst->saturate |= inst->saturate;
            if (!regs_left)
               break;
         }
      }

      assert(!regs_left);
      inst->remove(block);
      progress = true;
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

/**
 * Eliminate FIND_LIVE_CHANNEL instructions occurring outside any control
 * flow.  We could probably do better here with some form of divergence
 * analysis.
 */
bool
elk_fs_visitor::eliminate_find_live_channel()
{
   bool progress = false;
   unsigned depth = 0;

   if (!elk_stage_has_packed_dispatch(devinfo, stage, stage_prog_data)) {
      /* The optimization below assumes that channel zero is live on thread
       * dispatch, which may not be the case if the fixed function dispatches
       * threads sparsely.
       */
      return false;
   }

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      switch (inst->opcode) {
      case ELK_OPCODE_IF:
      case ELK_OPCODE_DO:
         depth++;
         break;

      case ELK_OPCODE_ENDIF:
      case ELK_OPCODE_WHILE:
         depth--;
         break;

      case ELK_OPCODE_HALT:
         /* This can potentially make control flow non-uniform until the end
          * of the program.
          */
         goto out;

      case ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL:
         if (depth == 0) {
            inst->opcode = ELK_OPCODE_MOV;
            inst->src[0] = elk_imm_ud(0u);
            inst->sources = 1;
            inst->force_writemask_all = true;
            progress = true;
         }
         break;

      default:
         break;
      }
   }

out:
   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL);

   return progress;
}

/**
 * Once we've generated code, try to convert normal ELK_FS_OPCODE_FB_WRITE
 * instructions to ELK_FS_OPCODE_REP_FB_WRITE.
 */
void
elk_fs_visitor::emit_repclear_shader()
{
   elk_wm_prog_key *key = (elk_wm_prog_key*) this->key;
   elk_fs_inst *write = NULL;

   assert(uniforms == 0);
   assume(key->nr_color_regions > 0);

   elk_fs_reg color_output, header;
   if (devinfo->ver >= 7) {
      color_output = retype(elk_vec4_grf(127, 0), ELK_REGISTER_TYPE_UD);
      header = retype(elk_vec8_grf(125, 0), ELK_REGISTER_TYPE_UD);
   } else {
      color_output = retype(elk_vec4_reg(MRF, 2, 0), ELK_REGISTER_TYPE_UD);
      header = retype(elk_vec8_reg(MRF, 0, 0), ELK_REGISTER_TYPE_UD);
   }

   /* We pass the clear color as a flat input.  Copy it to the output. */
   elk_fs_reg color_input =
      elk_reg(ELK_GENERAL_REGISTER_FILE, 2, 3, 0, 0, ELK_REGISTER_TYPE_UD,
              ELK_VERTICAL_STRIDE_8, ELK_WIDTH_2, ELK_HORIZONTAL_STRIDE_4,
              ELK_SWIZZLE_XYZW, WRITEMASK_XYZW);

   const fs_builder bld = fs_builder(this).at_end();
   bld.exec_all().group(4, 0).MOV(color_output, color_input);

   if (key->nr_color_regions > 1) {
      /* Copy g0..g1 as the message header */
      bld.exec_all().group(16, 0)
         .MOV(header, retype(elk_vec8_grf(0, 0), ELK_REGISTER_TYPE_UD));
   }

   for (int i = 0; i < key->nr_color_regions; ++i) {
      if (i > 0)
         bld.exec_all().group(1, 0).MOV(component(header, 2), elk_imm_ud(i));

      if (devinfo->ver >= 7) {
         write = bld.emit(ELK_SHADER_OPCODE_SEND);
         write->resize_sources(2);
         write->sfid = GFX6_SFID_DATAPORT_RENDER_CACHE;
         write->src[0] = elk_imm_ud(0);
         write->src[1] = i == 0 ? color_output : header;
         write->check_tdr = true;
         write->send_has_side_effects = true;
         write->desc = elk_fb_write_desc(devinfo, i,
            ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED,
            i == key->nr_color_regions - 1, false);
      } else {
         write = bld.emit(ELK_FS_OPCODE_REP_FB_WRITE);
         write->target = i;
         write->base_mrf = i == 0 ? color_output.nr : header.nr;
      }

      /* We can use a headerless message for the first render target */
      write->header_size = i == 0 ? 0 : 2;
      write->mlen = 1 + write->header_size;
   }
   write->eot = true;
   write->last_rt = true;

   calculate_cfg();

   this->first_non_payload_grf = payload().num_regs;
}

/**
 * Walks through basic blocks, looking for repeated MRF writes and
 * removing the later ones.
 */
bool
elk_fs_visitor::remove_duplicate_mrf_writes()
{
   elk_fs_inst *last_mrf_move[ELK_MAX_MRF(devinfo->ver)];
   bool progress = false;

   /* Need to update the MRF tracking for compressed instructions. */
   if (dispatch_width >= 16)
      return false;

   memset(last_mrf_move, 0, sizeof(last_mrf_move));

   foreach_block_and_inst_safe (block, elk_fs_inst, inst, cfg) {
      if (inst->is_control_flow()) {
	 memset(last_mrf_move, 0, sizeof(last_mrf_move));
      }

      if (inst->opcode == ELK_OPCODE_MOV &&
	  inst->dst.file == MRF) {
         elk_fs_inst *prev_inst = last_mrf_move[inst->dst.nr];
	 if (prev_inst && prev_inst->opcode == ELK_OPCODE_MOV &&
             inst->dst.equals(prev_inst->dst) &&
             inst->src[0].equals(prev_inst->src[0]) &&
             inst->saturate == prev_inst->saturate &&
             inst->predicate == prev_inst->predicate &&
             inst->conditional_mod == prev_inst->conditional_mod &&
             inst->exec_size == prev_inst->exec_size) {
	    inst->remove(block);
	    progress = true;
	    continue;
	 }
      }

      /* Clear out the last-write records for MRFs that were overwritten. */
      if (inst->dst.file == MRF) {
         last_mrf_move[inst->dst.nr] = NULL;
      }

      if (inst->mlen > 0 && inst->base_mrf != -1) {
	 /* Found a SEND instruction, which will include two or fewer
	  * implied MRF writes.  We could do better here.
	  */
	 for (unsigned i = 0; i < inst->implied_mrf_writes(); i++) {
	    last_mrf_move[inst->base_mrf + i] = NULL;
	 }
      }

      /* Clear out any MRF move records whose sources got overwritten. */
      for (unsigned i = 0; i < ARRAY_SIZE(last_mrf_move); i++) {
         if (last_mrf_move[i] &&
             regions_overlap(inst->dst, inst->size_written,
                             last_mrf_move[i]->src[0],
                             last_mrf_move[i]->size_read(0))) {
            last_mrf_move[i] = NULL;
         }
      }

      if (inst->opcode == ELK_OPCODE_MOV &&
	  inst->dst.file == MRF &&
	  inst->src[0].file != ARF &&
	  !inst->is_partial_write()) {
         last_mrf_move[inst->dst.nr] = inst;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

/**
 * Rounding modes for conversion instructions are included for each
 * conversion, but right now it is a state. So once it is set,
 * we don't need to call it again for subsequent calls.
 *
 * This is useful for vector/matrices conversions, as setting the
 * mode once is enough for the full vector/matrix
 */
bool
elk_fs_visitor::remove_extra_rounding_modes()
{
   bool progress = false;
   unsigned execution_mode = this->nir->info.float_controls_execution_mode;

   elk_rnd_mode base_mode = ELK_RND_MODE_UNSPECIFIED;
   if ((FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP16 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP32 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTE_FP64) &
       execution_mode)
      base_mode = ELK_RND_MODE_RTNE;
   if ((FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP16 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP32 |
        FLOAT_CONTROLS_ROUNDING_MODE_RTZ_FP64) &
       execution_mode)
      base_mode = ELK_RND_MODE_RTZ;

   foreach_block (block, cfg) {
      elk_rnd_mode prev_mode = base_mode;

      foreach_inst_in_block_safe (elk_fs_inst, inst, block) {
         if (inst->opcode == ELK_SHADER_OPCODE_RND_MODE) {
            assert(inst->src[0].file == ELK_IMMEDIATE_VALUE);
            const elk_rnd_mode mode = (elk_rnd_mode) inst->src[0].d;
            if (mode == prev_mode) {
               inst->remove(block);
               progress = true;
            } else {
               prev_mode = mode;
            }
         }
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

static void
clear_deps_for_inst_src(elk_fs_inst *inst, bool *deps, int first_grf, int grf_len)
{
   /* Clear the flag for registers that actually got read (as expected). */
   for (int i = 0; i < inst->sources; i++) {
      int grf;
      if (inst->src[i].file == VGRF || inst->src[i].file == FIXED_GRF) {
         grf = inst->src[i].nr;
      } else {
         continue;
      }

      if (grf >= first_grf &&
          grf < first_grf + grf_len) {
         deps[grf - first_grf] = false;
         if (inst->exec_size == 16)
            deps[grf - first_grf + 1] = false;
      }
   }
}

/**
 * Implements this workaround for the original 965:
 *
 *     "[DevBW, DevCL] Implementation Restrictions: As the hardware does not
 *      check for post destination dependencies on this instruction, software
 *      must ensure that there is no destination hazard for the case of ‘write
 *      followed by a posted write’ shown in the following example.
 *
 *      1. mov r3 0
 *      2. send r3.xy <rest of send instruction>
 *      3. mov r2 r3
 *
 *      Due to no post-destination dependency check on the ‘send’, the above
 *      code sequence could have two instructions (1 and 2) in flight at the
 *      same time that both consider ‘r3’ as the target of their final writes.
 */
void
elk_fs_visitor::insert_gfx4_pre_send_dependency_workarounds(elk_bblock_t *block,
                                                        elk_fs_inst *inst)
{
   int write_len = regs_written(inst);
   int first_write_grf = inst->dst.nr;
   bool needs_dep[ELK_MAX_MRF(devinfo->ver)];
   assert(write_len < (int)sizeof(needs_dep) - 1);

   memset(needs_dep, false, sizeof(needs_dep));
   memset(needs_dep, true, write_len);

   clear_deps_for_inst_src(inst, needs_dep, first_write_grf, write_len);

   /* Walk backwards looking for writes to registers we're writing which
    * aren't read since being written.  If we hit the start of the program,
    * we assume that there are no outstanding dependencies on entry to the
    * program.
    */
   foreach_inst_in_block_reverse_starting_from(elk_fs_inst, scan_inst, inst) {
      /* If we hit control flow, assume that there *are* outstanding
       * dependencies, and force their cleanup before our instruction.
       */
      if (block->start() == scan_inst && block->num != 0) {
         for (int i = 0; i < write_len; i++) {
            if (needs_dep[i])
               DEP_RESOLVE_MOV(fs_builder(this, block, inst),
                               first_write_grf + i);
         }
         return;
      }

      /* We insert our reads as late as possible on the assumption that any
       * instruction but a MOV that might have left us an outstanding
       * dependency has more latency than a MOV.
       */
      if (scan_inst->dst.file == VGRF) {
         for (unsigned i = 0; i < regs_written(scan_inst); i++) {
            int reg = scan_inst->dst.nr + i;

            if (reg >= first_write_grf &&
                reg < first_write_grf + write_len &&
                needs_dep[reg - first_write_grf]) {
               DEP_RESOLVE_MOV(fs_builder(this, block, inst), reg);
               needs_dep[reg - first_write_grf] = false;
               if (scan_inst->exec_size == 16)
                  needs_dep[reg - first_write_grf + 1] = false;
            }
         }
      }

      /* Clear the flag for registers that actually got read (as expected). */
      clear_deps_for_inst_src(scan_inst, needs_dep, first_write_grf, write_len);

      /* Continue the loop only if we haven't resolved all the dependencies */
      int i;
      for (i = 0; i < write_len; i++) {
         if (needs_dep[i])
            break;
      }
      if (i == write_len)
         return;
   }
}

/**
 * Implements this workaround for the original 965:
 *
 *     "[DevBW, DevCL] Errata: A destination register from a send can not be
 *      used as a destination register until after it has been sourced by an
 *      instruction with a different destination register.
 */
void
elk_fs_visitor::insert_gfx4_post_send_dependency_workarounds(elk_bblock_t *block, elk_fs_inst *inst)
{
   int write_len = regs_written(inst);
   unsigned first_write_grf = inst->dst.nr;
   bool needs_dep[ELK_MAX_MRF(devinfo->ver)];
   assert(write_len < (int)sizeof(needs_dep) - 1);

   memset(needs_dep, false, sizeof(needs_dep));
   memset(needs_dep, true, write_len);
   /* Walk forwards looking for writes to registers we're writing which aren't
    * read before being written.
    */
   foreach_inst_in_block_starting_from(elk_fs_inst, scan_inst, inst) {
      /* If we hit control flow, force resolve all remaining dependencies. */
      if (block->end() == scan_inst && block->num != cfg->num_blocks - 1) {
         for (int i = 0; i < write_len; i++) {
            if (needs_dep[i])
               DEP_RESOLVE_MOV(fs_builder(this, block, scan_inst),
                               first_write_grf + i);
         }
         return;
      }

      /* Clear the flag for registers that actually got read (as expected). */
      clear_deps_for_inst_src(scan_inst, needs_dep, first_write_grf, write_len);

      /* We insert our reads as late as possible since they're reading the
       * result of a SEND, which has massive latency.
       */
      if (scan_inst->dst.file == VGRF &&
          scan_inst->dst.nr >= first_write_grf &&
          scan_inst->dst.nr < first_write_grf + write_len &&
          needs_dep[scan_inst->dst.nr - first_write_grf]) {
         DEP_RESOLVE_MOV(fs_builder(this, block, scan_inst),
                         scan_inst->dst.nr);
         needs_dep[scan_inst->dst.nr - first_write_grf] = false;
      }

      /* Continue the loop only if we haven't resolved all the dependencies */
      int i;
      for (i = 0; i < write_len; i++) {
         if (needs_dep[i])
            break;
      }
      if (i == write_len)
         return;
   }
}

void
elk_fs_visitor::insert_gfx4_send_dependency_workarounds()
{
   if (devinfo->ver != 4 || devinfo->platform == INTEL_PLATFORM_G4X)
      return;

   bool progress = false;

   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      if (inst->mlen != 0 && inst->dst.file == VGRF) {
         insert_gfx4_pre_send_dependency_workarounds(block, inst);
         insert_gfx4_post_send_dependency_workarounds(block, inst);
         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);
}

bool
elk_fs_visitor::lower_load_payload()
{
   bool progress = false;

   foreach_block_and_inst_safe (block, elk_fs_inst, inst, cfg) {
      if (inst->opcode != ELK_SHADER_OPCODE_LOAD_PAYLOAD)
         continue;

      assert(inst->dst.file == MRF || inst->dst.file == VGRF);
      assert(inst->saturate == false);
      elk_fs_reg dst = inst->dst;

      /* Get rid of COMPR4.  We'll add it back in if we need it */
      if (dst.file == MRF)
         dst.nr = dst.nr & ~ELK_MRF_COMPR4;

      const fs_builder ibld(this, block, inst);
      const fs_builder ubld = ibld.exec_all();

      for (uint8_t i = 0; i < inst->header_size;) {
         /* Number of header GRFs to initialize at once with a single MOV
          * instruction.
          */
         const unsigned n =
            (i + 1 < inst->header_size && inst->src[i].stride == 1 &&
             inst->src[i + 1].equals(byte_offset(inst->src[i], REG_SIZE))) ?
            2 : 1;

         if (inst->src[i].file != BAD_FILE)
            ubld.group(8 * n, 0).MOV(retype(dst, ELK_REGISTER_TYPE_UD),
                                     retype(inst->src[i], ELK_REGISTER_TYPE_UD));

         dst = byte_offset(dst, n * REG_SIZE);
         i += n;
      }

      if (inst->dst.file == MRF && (inst->dst.nr & ELK_MRF_COMPR4) &&
          inst->exec_size > 8) {
         /* In this case, the payload portion of the LOAD_PAYLOAD isn't
          * a straightforward copy.  Instead, the result of the
          * LOAD_PAYLOAD is treated as interleaved and the first four
          * non-header sources are unpacked as:
          *
          * m + 0: r0
          * m + 1: g0
          * m + 2: b0
          * m + 3: a0
          * m + 4: r1
          * m + 5: g1
          * m + 6: b1
          * m + 7: a1
          *
          * This is used for gen <= 5 fb writes.
          */
         assert(inst->exec_size == 16);
         assert(inst->header_size + 4 <= inst->sources);
         for (uint8_t i = inst->header_size; i < inst->header_size + 4; i++) {
            if (inst->src[i].file != BAD_FILE) {
               if (devinfo->has_compr4) {
                  elk_fs_reg compr4_dst = retype(dst, inst->src[i].type);
                  compr4_dst.nr |= ELK_MRF_COMPR4;
                  ibld.MOV(compr4_dst, inst->src[i]);
               } else {
                  /* Platform doesn't have COMPR4.  We have to fake it */
                  elk_fs_reg mov_dst = retype(dst, inst->src[i].type);
                  ibld.quarter(0).MOV(mov_dst, quarter(inst->src[i], 0));
                  mov_dst.nr += 4;
                  ibld.quarter(1).MOV(mov_dst, quarter(inst->src[i], 1));
               }
            }

            dst.nr++;
         }

         /* The loop above only ever incremented us through the first set
          * of 4 registers.  However, thanks to the magic of COMPR4, we
          * actually wrote to the first 8 registers, so we need to take
          * that into account now.
          */
         dst.nr += 4;

         /* The COMPR4 code took care of the first 4 sources.  We'll let
          * the regular path handle any remaining sources.  Yes, we are
          * modifying the instruction but we're about to delete it so
          * this really doesn't hurt anything.
          */
         inst->header_size += 4;
      }

      for (uint8_t i = inst->header_size; i < inst->sources; i++) {
         dst.type = inst->src[i].type;
         if (inst->src[i].file != BAD_FILE) {
            ibld.MOV(dst, inst->src[i]);
         }
         dst = offset(dst, ibld, 1);
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

/**
 * Factor an unsigned 32-bit integer.
 *
 * Attempts to factor \c x into two values that are at most 0xFFFF.  If no
 * such factorization is possible, either because the value is too large or is
 * prime, both \c result_a and \c result_b will be zero.
 */
static void
factor_uint32(uint32_t x, unsigned *result_a, unsigned *result_b)
{
   /* This is necessary to prevent various opportunities for division by zero
    * below.
    */
   assert(x > 0xffff);

   /* This represents the actual expected constraints on the input.  Namely,
    * both the upper and lower words should be > 1.
    */
   assert(x >= 0x00020002);

   *result_a = 0;
   *result_b = 0;

   /* The value is too large to factor with the constraints. */
   if (x > (0xffffu * 0xffffu))
      return;

   /* A non-prime number will have the form p*q*d where p is some prime
    * number, q > 1, and 1 <= d <= q.  To meet the constraints of this
    * function, (p*d) < 0x10000.  This implies d <= floor(0xffff / p).
    * Furthermore, since q < 0x10000, d >= floor(x / (0xffff * p)).  Finally,
    * floor(x / (0xffff * p)) <= d <= floor(0xffff / p).
    *
    * The observation is finding the largest possible value of p reduces the
    * possible range of d.  After selecting p, all values of d in this range
    * are tested until a factorization is found.  The size of the range of
    * possible values of d sets an upper bound on the run time of the
    * function.
    */
   static const uint16_t primes[256] = {
         2,    3,    5,    7,   11,   13,   17,   19,
        23,   29,   31,   37,   41,   43,   47,   53,
        59,   61,   67,   71,   73,   79,   83,   89,
        97,  101,  103,  107,  109,  113,  127,  131,  /*  32 */
       137,  139,  149,  151,  157,  163,  167,  173,
       179,  181,  191,  193,  197,  199,  211,  223,
       227,  229,  233,  239,  241,  251,  257,  263,
       269,  271,  277,  281,  283,  293,  307,  311,  /*  64 */
       313,  317,  331,  337,  347,  349,  353,  359,
       367,  373,  379,  383,  389,  397,  401,  409,
       419,  421,  431,  433,  439,  443,  449,  457,
       461,  463,  467,  479,  487,  491,  499,  503,  /*  96 */
       509,  521,  523,  541,  547,  557,  563,  569,
       571,  577,  587,  593,  599,  601,  607,  613,
       617,  619,  631,  641,  643,  647,  653,  659,
       661,  673,  677,  683,  691,  701,  709,  719,   /* 128 */
       727,  733,  739,  743,  751,  757,  761,  769,
       773,  787,  797,  809,  811,  821,  823,  827,
       829,  839,  853,  857,  859,  863,  877,  881,
       883,  887,  907,  911,  919,  929,  937,  941,  /* 160 */
       947,  953,  967,  971,  977,  983,  991,  997,
      1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049,
      1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097,
      1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163,  /* 192 */
      1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
      1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283,
      1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321,
      1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423,  /* 224 */
      1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459,
      1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
      1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571,
      1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619,  /* 256 */
   };

   unsigned p;
   unsigned x_div_p;

   for (int i = ARRAY_SIZE(primes) - 1; i >= 0; i--) {
      p = primes[i];
      x_div_p = x / p;

      if ((x_div_p * p) == x)
         break;
   }

   /* A prime factor was not found. */
   if (x_div_p * p != x)
      return;

   /* Terminate early if d=1 is a solution. */
   if (x_div_p < 0x10000) {
      *result_a = x_div_p;
      *result_b = p;
      return;
   }

   /* Pick the maximum possible value for 'd'.  It's important that the loop
    * below execute while d <= max_d because max_d is a valid value.  Having
    * the wrong loop bound would cause 1627*1367*47 (0x063b0c83) to be
    * incorrectly reported as not being factorable.  The problem would occur
    * with any value that is a factor of two primes in the table and one prime
    * not in the table.
    */
   const unsigned max_d = 0xffff / p;

   /* Pick an initial value of 'd' that (combined with rejecting too large
    * values above) guarantees that 'q' will always be small enough.
    * DIV_ROUND_UP is used to prevent 'd' from being zero.
    */
   for (unsigned d = DIV_ROUND_UP(x_div_p, 0xffff); d <= max_d; d++) {
      unsigned q = x_div_p / d;

      if ((q * d) == x_div_p) {
         assert(p * d * q == x);
         assert((p * d) < 0x10000);

         *result_a = q;
         *result_b = p * d;
         break;
      }

      /* Since every value of 'd' is tried, as soon as 'd' is larger
       * than 'q', we're just re-testing combinations that have
       * already been tested.
       */
      if (d > q)
         break;
   }
}

void
elk_fs_visitor::lower_mul_dword_inst(elk_fs_inst *inst, elk_bblock_t *block)
{
   const fs_builder ibld(this, block, inst);

   /* It is correct to use inst->src[1].d in both end of the comparison.
    * Using .ud in the UINT16_MAX comparison would cause any negative value to
    * fail the check.
    */
   if (inst->src[1].file == IMM &&
       (inst->src[1].d >= INT16_MIN && inst->src[1].d <= UINT16_MAX)) {
      /* The MUL instruction isn't commutative. On Gen <= 6, only the low
       * 16-bits of src0 are read, and on Gen >= 7 only the low 16-bits of
       * src1 are used.
       *
       * If multiplying by an immediate value that fits in 16-bits, do a
       * single MUL instruction with that value in the proper location.
       */
      const bool ud = (inst->src[1].d >= 0);
      if (devinfo->ver < 7) {
         elk_fs_reg imm(VGRF, alloc.allocate(dispatch_width / 8), inst->dst.type);
         ibld.MOV(imm, inst->src[1]);
         ibld.MUL(inst->dst, imm, inst->src[0]);
      } else {
         ibld.MUL(inst->dst, inst->src[0],
                  ud ? elk_imm_uw(inst->src[1].ud)
                     : elk_imm_w(inst->src[1].d));
      }
   } else {
      /* Gen < 8 (and some Gfx8+ low-power parts like Cherryview) cannot
       * do 32-bit integer multiplication in one instruction, but instead
       * must do a sequence (which actually calculates a 64-bit result):
       *
       *    mul(8)  acc0<1>D   g3<8,8,1>D      g4<8,8,1>D
       *    mach(8) null       g3<8,8,1>D      g4<8,8,1>D
       *    mov(8)  g2<1>D     acc0<8,8,1>D
       *
       * But on Gen > 6, the ability to use second accumulator register
       * (acc1) for non-float data types was removed, preventing a simple
       * implementation in SIMD16. A 16-channel result can be calculated by
       * executing the three instructions twice in SIMD8, once with quarter
       * control of 1Q for the first eight channels and again with 2Q for
       * the second eight channels.
       *
       * Which accumulator register is implicitly accessed (by AccWrEnable
       * for instance) is determined by the quarter control. Unfortunately
       * Ivybridge (and presumably Baytrail) has a hardware bug in which an
       * implicit accumulator access by an instruction with 2Q will access
       * acc1 regardless of whether the data type is usable in acc1.
       *
       * Specifically, the 2Q mach(8) writes acc1 which does not exist for
       * integer data types.
       *
       * Since we only want the low 32-bits of the result, we can do two
       * 32-bit x 16-bit multiplies (like the mul and mach are doing), and
       * adjust the high result and add them (like the mach is doing):
       *
       *    mul(8)  g7<1>D     g3<8,8,1>D      g4.0<8,8,1>UW
       *    mul(8)  g8<1>D     g3<8,8,1>D      g4.1<8,8,1>UW
       *    shl(8)  g9<1>D     g8<8,8,1>D      16D
       *    add(8)  g2<1>D     g7<8,8,1>D      g8<8,8,1>D
       *
       * We avoid the shl instruction by realizing that we only want to add
       * the low 16-bits of the "high" result to the high 16-bits of the
       * "low" result and using proper regioning on the add:
       *
       *    mul(8)  g7<1>D     g3<8,8,1>D      g4.0<16,8,2>UW
       *    mul(8)  g8<1>D     g3<8,8,1>D      g4.1<16,8,2>UW
       *    add(8)  g7.1<2>UW  g7.1<16,8,2>UW  g8<16,8,2>UW
       *
       * Since it does not use the (single) accumulator register, we can
       * schedule multi-component multiplications much better.
       */

      bool needs_mov = false;
      elk_fs_reg orig_dst = inst->dst;

      /* Get a new VGRF for the "low" 32x16-bit multiplication result if
       * reusing the original destination is impossible due to hardware
       * restrictions, source/destination overlap, or it being the null
       * register.
       */
      elk_fs_reg low = inst->dst;
      if (orig_dst.is_null() || orig_dst.file == MRF ||
          regions_overlap(inst->dst, inst->size_written,
                          inst->src[0], inst->size_read(0)) ||
          regions_overlap(inst->dst, inst->size_written,
                          inst->src[1], inst->size_read(1)) ||
          inst->dst.stride >= 4) {
         needs_mov = true;
         low = elk_fs_reg(VGRF, alloc.allocate(regs_written(inst)),
                      inst->dst.type);
      }

      /* Get a new VGRF but keep the same stride as inst->dst */
      elk_fs_reg high(VGRF, alloc.allocate(regs_written(inst)), inst->dst.type);
      high.stride = inst->dst.stride;
      high.offset = inst->dst.offset % REG_SIZE;

      bool do_addition = true;
      if (devinfo->ver >= 7) {
         if (inst->src[1].abs)
            lower_src_modifiers(this, block, inst, 1);

         if (inst->src[1].file == IMM) {
            unsigned a;
            unsigned b;

            /* If the immeditate value can be factored into two values, A and
             * B, that each fit in 16-bits, the multiplication result can
             * instead be calculated as (src1 * (A * B)) = ((src1 * A) * B).
             * This saves an operation (the addition) and a temporary register
             * (high).
             *
             * Skip the optimization if either the high word or the low word
             * is 0 or 1.  In these conditions, at least one of the
             * multiplications generated by the straightforward method will be
             * eliminated anyway.
             */
            if (inst->src[1].ud > 0x0001ffff &&
                (inst->src[1].ud & 0xffff) > 1) {
               factor_uint32(inst->src[1].ud, &a, &b);

               if (a != 0) {
                  ibld.MUL(low, inst->src[0], elk_imm_uw(a));
                  ibld.MUL(low, low, elk_imm_uw(b));
                  do_addition = false;
               }
            }

            if (do_addition) {
               ibld.MUL(low, inst->src[0],
                        elk_imm_uw(inst->src[1].ud & 0xffff));
               ibld.MUL(high, inst->src[0],
                        elk_imm_uw(inst->src[1].ud >> 16));
            }
         } else {
            ibld.MUL(low, inst->src[0],
                     subscript(inst->src[1], ELK_REGISTER_TYPE_UW, 0));
            ibld.MUL(high, inst->src[0],
                     subscript(inst->src[1], ELK_REGISTER_TYPE_UW, 1));
         }
      } else {
         if (inst->src[0].abs)
            lower_src_modifiers(this, block, inst, 0);

         ibld.MUL(low, subscript(inst->src[0], ELK_REGISTER_TYPE_UW, 0),
                  inst->src[1]);
         ibld.MUL(high, subscript(inst->src[0], ELK_REGISTER_TYPE_UW, 1),
                  inst->src[1]);
      }

      if (do_addition) {
         ibld.ADD(subscript(low, ELK_REGISTER_TYPE_UW, 1),
                  subscript(low, ELK_REGISTER_TYPE_UW, 1),
                  subscript(high, ELK_REGISTER_TYPE_UW, 0));
      }

      if (needs_mov || inst->conditional_mod)
         set_condmod(inst->conditional_mod, ibld.MOV(orig_dst, low));
   }
}

void
elk_fs_visitor::lower_mul_qword_inst(elk_fs_inst *inst, elk_bblock_t *block)
{
   const fs_builder ibld(this, block, inst);

   /* Considering two 64-bit integers ab and cd where each letter        ab
    * corresponds to 32 bits, we get a 128-bit result WXYZ. We         * cd
    * only need to provide the YZ part of the result.               -------
    *                                                                    BD
    *  Only BD needs to be 64 bits. For AD and BC we only care       +  AD
    *  about the lower 32 bits (since they are part of the upper     +  BC
    *  32 bits of our result). AC is not needed since it starts      + AC
    *  on the 65th bit of the result.                               -------
    *                                                                  WXYZ
    */
   unsigned int q_regs = regs_written(inst);
   unsigned int d_regs = (q_regs + 1) / 2;

   elk_fs_reg bd(VGRF, alloc.allocate(q_regs), ELK_REGISTER_TYPE_UQ);
   elk_fs_reg ad(VGRF, alloc.allocate(d_regs), ELK_REGISTER_TYPE_UD);
   elk_fs_reg bc(VGRF, alloc.allocate(d_regs), ELK_REGISTER_TYPE_UD);

   /* Here we need the full 64 bit result for 32b * 32b. */
   if (devinfo->has_integer_dword_mul) {
      ibld.MUL(bd, subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0),
               subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 0));
   } else {
      elk_fs_reg bd_high(VGRF, alloc.allocate(d_regs), ELK_REGISTER_TYPE_UD);
      elk_fs_reg bd_low(VGRF, alloc.allocate(d_regs), ELK_REGISTER_TYPE_UD);
      const unsigned acc_width = reg_unit(devinfo) * 8;
      elk_fs_reg acc = suboffset(retype(elk_acc_reg(inst->exec_size), ELK_REGISTER_TYPE_UD),
                             inst->group % acc_width);

      elk_fs_inst *mul = ibld.MUL(acc,
                            subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0),
                            subscript(inst->src[1], ELK_REGISTER_TYPE_UW, 0));
      mul->writes_accumulator = true;

      ibld.MACH(bd_high, subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0),
                subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 0));
      ibld.MOV(bd_low, acc);

      ibld.UNDEF(bd);
      ibld.MOV(subscript(bd, ELK_REGISTER_TYPE_UD, 0), bd_low);
      ibld.MOV(subscript(bd, ELK_REGISTER_TYPE_UD, 1), bd_high);
   }

   ibld.MUL(ad, subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 1),
            subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 0));
   ibld.MUL(bc, subscript(inst->src[0], ELK_REGISTER_TYPE_UD, 0),
            subscript(inst->src[1], ELK_REGISTER_TYPE_UD, 1));

   ibld.ADD(ad, ad, bc);
   ibld.ADD(subscript(bd, ELK_REGISTER_TYPE_UD, 1),
            subscript(bd, ELK_REGISTER_TYPE_UD, 1), ad);

   if (devinfo->has_64bit_int) {
      ibld.MOV(inst->dst, bd);
   } else {
      if (!inst->is_partial_write())
         ibld.emit_undef_for_dst(inst);
      ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 0),
               subscript(bd, ELK_REGISTER_TYPE_UD, 0));
      ibld.MOV(subscript(inst->dst, ELK_REGISTER_TYPE_UD, 1),
               subscript(bd, ELK_REGISTER_TYPE_UD, 1));
   }
}

void
elk_fs_visitor::lower_mulh_inst(elk_fs_inst *inst, elk_bblock_t *block)
{
   const fs_builder ibld(this, block, inst);

   /* According to the BDW+ BSpec page for the "Multiply Accumulate
    * High" instruction:
    *
    *  "An added preliminary mov is required for source modification on
    *   src1:
    *      mov (8) r3.0<1>:d -r3<8;8,1>:d
    *      mul (8) acc0:d r2.0<8;8,1>:d r3.0<16;8,2>:uw
    *      mach (8) r5.0<1>:d r2.0<8;8,1>:d r3.0<8;8,1>:d"
    */
   if (devinfo->ver >= 8 && (inst->src[1].negate || inst->src[1].abs))
      lower_src_modifiers(this, block, inst, 1);

   /* Should have been lowered to 8-wide. */
   assert(inst->exec_size <= get_lowered_simd_width(this, inst));
   const unsigned acc_width = reg_unit(devinfo) * 8;
   const elk_fs_reg acc = suboffset(retype(elk_acc_reg(inst->exec_size), inst->dst.type),
                                inst->group % acc_width);
   elk_fs_inst *mul = ibld.MUL(acc, inst->src[0], inst->src[1]);
   elk_fs_inst *mach = ibld.MACH(inst->dst, inst->src[0], inst->src[1]);

   if (devinfo->ver >= 8) {
      /* Until Gfx8, integer multiplies read 32-bits from one source,
       * and 16-bits from the other, and relying on the MACH instruction
       * to generate the high bits of the result.
       *
       * On Gfx8, the multiply instruction does a full 32x32-bit
       * multiply, but in order to do a 64-bit multiply we can simulate
       * the previous behavior and then use a MACH instruction.
       */
      assert(mul->src[1].type == ELK_REGISTER_TYPE_D ||
             mul->src[1].type == ELK_REGISTER_TYPE_UD);
      mul->src[1].type = ELK_REGISTER_TYPE_UW;
      mul->src[1].stride *= 2;

      if (mul->src[1].file == IMM) {
         mul->src[1] = elk_imm_uw(mul->src[1].ud);
      }
   } else if (devinfo->verx10 == 70 &&
              inst->group > 0) {
      /* Among other things the quarter control bits influence which
       * accumulator register is used by the hardware for instructions
       * that access the accumulator implicitly (e.g. MACH).  A
       * second-half instruction would normally map to acc1, which
       * doesn't exist on Gfx7 and up (the hardware does emulate it for
       * floating-point instructions *only* by taking advantage of the
       * extra precision of acc0 not normally used for floating point
       * arithmetic).
       *
       * HSW and up are careful enough not to try to access an
       * accumulator register that doesn't exist, but on earlier Gfx7
       * hardware we need to make sure that the quarter control bits are
       * zero to avoid non-deterministic behaviour and emit an extra MOV
       * to get the result masked correctly according to the current
       * channel enables.
       */
      mach->group = 0;
      mach->force_writemask_all = true;
      mach->dst = ibld.vgrf(inst->dst.type);
      ibld.MOV(inst->dst, mach->dst);
   }
}

bool
elk_fs_visitor::lower_integer_multiplication()
{
   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      if (inst->opcode == ELK_OPCODE_MUL) {
         /* If the instruction is already in a form that does not need lowering,
          * return early.
          */
         if (devinfo->ver >= 7) {
            if (type_sz(inst->src[1].type) < 4 && type_sz(inst->src[0].type) <= 4)
               continue;
         } else {
            if (type_sz(inst->src[0].type) < 4 && type_sz(inst->src[1].type) <= 4)
               continue;
         }

         if ((inst->dst.type == ELK_REGISTER_TYPE_Q ||
              inst->dst.type == ELK_REGISTER_TYPE_UQ) &&
             (inst->src[0].type == ELK_REGISTER_TYPE_Q ||
              inst->src[0].type == ELK_REGISTER_TYPE_UQ) &&
             (inst->src[1].type == ELK_REGISTER_TYPE_Q ||
              inst->src[1].type == ELK_REGISTER_TYPE_UQ)) {
            lower_mul_qword_inst(inst, block);
            inst->remove(block);
            progress = true;
         } else if (!inst->dst.is_accumulator() &&
                    (inst->dst.type == ELK_REGISTER_TYPE_D ||
                     inst->dst.type == ELK_REGISTER_TYPE_UD) &&
                    !devinfo->has_integer_dword_mul) {
            lower_mul_dword_inst(inst, block);
            inst->remove(block);
            progress = true;
         }
      } else if (inst->opcode == ELK_SHADER_OPCODE_MULH) {
         lower_mulh_inst(inst, block);
         inst->remove(block);
         progress = true;
      }

   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

bool
elk_fs_visitor::lower_minmax()
{
   assert(devinfo->ver < 6);

   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      const fs_builder ibld(this, block, inst);

      if (inst->opcode == ELK_OPCODE_SEL &&
          inst->predicate == ELK_PREDICATE_NONE) {
         /* If src1 is an immediate value that is not NaN, then it can't be
          * NaN.  In that case, emit CMP because it is much better for cmod
          * propagation.  Likewise if src1 is not float.  Gfx4 and Gfx5 don't
          * support HF or DF, so it is not necessary to check for those.
          */
         if (inst->src[1].type != ELK_REGISTER_TYPE_F ||
             (inst->src[1].file == IMM && !isnan(inst->src[1].f))) {
            ibld.CMP(ibld.null_reg_d(), inst->src[0], inst->src[1],
                     inst->conditional_mod);
         } else {
            ibld.CMPN(ibld.null_reg_d(), inst->src[0], inst->src[1],
                      inst->conditional_mod);
         }
         inst->predicate = ELK_PREDICATE_NORMAL;
         inst->conditional_mod = ELK_CONDITIONAL_NONE;

         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

bool
elk_fs_visitor::lower_sub_sat()
{
   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      const fs_builder ibld(this, block, inst);

      if (inst->opcode == ELK_SHADER_OPCODE_USUB_SAT ||
          inst->opcode == ELK_SHADER_OPCODE_ISUB_SAT) {
         /* The fundamental problem is the hardware performs source negation
          * at the bit width of the source.  If the source is 0x80000000D, the
          * negation is 0x80000000D.  As a result, subtractSaturate(0,
          * 0x80000000) will produce 0x80000000 instead of 0x7fffffff.  There
          * are at least three ways to resolve this:
          *
          * 1. Use the accumulator for the negated source.  The accumulator is
          *    33 bits, so our source 0x80000000 is sign-extended to
          *    0x1800000000.  The negation of which is 0x080000000.  This
          *    doesn't help for 64-bit integers (which are already bigger than
          *    33 bits).  There are also only 8 accumulators, so SIMD16 or
          *    SIMD32 instructions would have to be split into multiple SIMD8
          *    instructions.
          *
          * 2. Use slightly different math.  For any n-bit value x, we know (x
          *    >> 1) != -(x >> 1).  We can use this fact to only do
          *    subtractions involving (x >> 1).  subtractSaturate(a, b) ==
          *    subtractSaturate(subtractSaturate(a, (b >> 1)), b - (b >> 1)).
          *
          * 3. For unsigned sources, it is sufficient to replace the
          *    subtractSaturate with (a > b) ? a - b : 0.
          *
          * It may also be possible to use the SUBB instruction.  This
          * implicitly writes the accumulator, so it could only be used in the
          * same situations as #1 above.  It is further limited by only
          * allowing UD sources.
          */
         if (inst->exec_size == 8 && inst->src[0].type != ELK_REGISTER_TYPE_Q &&
             inst->src[0].type != ELK_REGISTER_TYPE_UQ) {
            elk_fs_reg acc(ARF, ELK_ARF_ACCUMULATOR, inst->src[1].type);

            ibld.MOV(acc, inst->src[1]);
            elk_fs_inst *add = ibld.ADD(inst->dst, acc, inst->src[0]);
            add->saturate = true;
            add->src[0].negate = true;
         } else if (inst->opcode == ELK_SHADER_OPCODE_ISUB_SAT) {
            /* tmp = src1 >> 1;
             * dst = add.sat(add.sat(src0, -tmp), -(src1 - tmp));
             */
            elk_fs_reg tmp1 = ibld.vgrf(inst->src[0].type);
            elk_fs_reg tmp2 = ibld.vgrf(inst->src[0].type);
            elk_fs_reg tmp3 = ibld.vgrf(inst->src[0].type);
            elk_fs_inst *add;

            ibld.SHR(tmp1, inst->src[1], elk_imm_d(1));

            add = ibld.ADD(tmp2, inst->src[1], tmp1);
            add->src[1].negate = true;

            add = ibld.ADD(tmp3, inst->src[0], tmp1);
            add->src[1].negate = true;
            add->saturate = true;

            add = ibld.ADD(inst->dst, tmp3, tmp2);
            add->src[1].negate = true;
            add->saturate = true;
         } else {
            /* a > b ? a - b : 0 */
            ibld.CMP(ibld.null_reg_d(), inst->src[0], inst->src[1],
                     ELK_CONDITIONAL_G);

            elk_fs_inst *add = ibld.ADD(inst->dst, inst->src[0], inst->src[1]);
            add->src[1].negate = !add->src[1].negate;

            ibld.SEL(inst->dst, inst->dst, elk_imm_ud(0))
               ->predicate = ELK_PREDICATE_NORMAL;
         }

         inst->remove(block);
         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Get the mask of SIMD channels enabled during dispatch and not yet disabled
 * by discard.  Due to the layout of the sample mask in the fragment shader
 * thread payload, \p bld is required to have a dispatch_width() not greater
 * than 16 for fragment shaders.
 */
elk_fs_reg
elk_sample_mask_reg(const fs_builder &bld)
{
   const elk_fs_visitor &s = *bld.shader;

   if (s.stage != MESA_SHADER_FRAGMENT) {
      return elk_imm_ud(0xffffffff);
   } else if (elk_wm_prog_data(s.stage_prog_data)->uses_kill) {
      assert(bld.dispatch_width() <= 16);
      return elk_flag_subreg(sample_mask_flag_subreg(s) + bld.group() / 16);
   } else {
      assert(s.devinfo->ver >= 6 && bld.dispatch_width() <= 16);
      return retype(elk_vec1_grf((bld.group() >= 16 ? 2 : 1), 7),
                    ELK_REGISTER_TYPE_UW);
   }
}

uint32_t
elk_fb_write_msg_control(const elk_fs_inst *inst,
                         const struct elk_wm_prog_data *prog_data)
{
   uint32_t mctl;

   if (inst->opcode == ELK_FS_OPCODE_REP_FB_WRITE) {
      assert(inst->group == 0 && inst->exec_size == 16);
      mctl = ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED;
   } else if (prog_data->dual_src_blend) {
      assert(inst->exec_size == 8);

      if (inst->group % 16 == 0)
         mctl = ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN01;
      else if (inst->group % 16 == 8)
         mctl = ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN23;
      else
         unreachable("Invalid dual-source FB write instruction group");
   } else {
      assert(inst->group == 0 || (inst->group == 16 && inst->exec_size == 16));

      if (inst->exec_size == 16)
         mctl = ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE;
      else if (inst->exec_size == 8)
         mctl = ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_SINGLE_SOURCE_SUBSPAN01;
      else
         unreachable("Invalid FB write execution size");
   }

   return mctl;
}

 /**
 * Predicate the specified instruction on the sample mask.
 */
void
elk_emit_predicate_on_sample_mask(const fs_builder &bld, elk_fs_inst *inst)
{
   assert(bld.shader->stage == MESA_SHADER_FRAGMENT &&
          bld.group() == inst->group &&
          bld.dispatch_width() == inst->exec_size);

   const elk_fs_visitor &s = *bld.shader;
   const elk_fs_reg sample_mask = elk_sample_mask_reg(bld);
   const unsigned subreg = sample_mask_flag_subreg(s);

   if (elk_wm_prog_data(s.stage_prog_data)->uses_kill) {
      assert(sample_mask.file == ARF &&
             sample_mask.nr == elk_flag_subreg(subreg).nr &&
             sample_mask.subnr == elk_flag_subreg(
                subreg + inst->group / 16).subnr);
   } else {
      bld.group(1, 0).exec_all()
         .MOV(elk_flag_subreg(subreg + inst->group / 16), sample_mask);
   }

   if (inst->predicate) {
      assert(inst->predicate == ELK_PREDICATE_NORMAL);
      assert(!inst->predicate_inverse);
      assert(inst->flag_subreg == 0);
      /* Combine the sample mask with the existing predicate by using a
       * vertical predication mode.
       */
      inst->predicate = ELK_PREDICATE_ALIGN1_ALLV;
   } else {
      inst->flag_subreg = subreg;
      inst->predicate = ELK_PREDICATE_NORMAL;
      inst->predicate_inverse = false;
   }
}

static bool
is_mixed_float_with_fp32_dst(const elk_fs_inst *inst)
{
   /* This opcode sometimes uses :W type on the source even if the operand is
    * a :HF, because in gfx7 there is no support for :HF, and thus it uses :W.
    */
   if (inst->opcode == ELK_OPCODE_F16TO32)
      return true;

   if (inst->dst.type != ELK_REGISTER_TYPE_F)
      return false;

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].type == ELK_REGISTER_TYPE_HF)
         return true;
   }

   return false;
}

static bool
is_mixed_float_with_packed_fp16_dst(const elk_fs_inst *inst)
{
   /* This opcode sometimes uses :W type on the destination even if the
    * destination is a :HF, because in gfx7 there is no support for :HF, and
    * thus it uses :W.
    */
   if (inst->opcode == ELK_OPCODE_F32TO16 &&
       inst->dst.stride == 1)
      return true;

   if (inst->dst.type != ELK_REGISTER_TYPE_HF ||
       inst->dst.stride != 1)
      return false;

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].type == ELK_REGISTER_TYPE_F)
         return true;
   }

   return false;
}

/**
 * Get the closest allowed SIMD width for instruction \p inst accounting for
 * some common regioning and execution control restrictions that apply to FPU
 * instructions.  These restrictions don't necessarily have any relevance to
 * instructions not executed by the FPU pipeline like extended math, control
 * flow or send message instructions.
 *
 * For virtual opcodes it's really up to the instruction -- In some cases
 * (e.g. where a virtual instruction unrolls into a simple sequence of FPU
 * instructions) it may simplify virtual instruction lowering if we can
 * enforce FPU-like regioning restrictions already on the virtual instruction,
 * in other cases (e.g. virtual send-like instructions) this may be
 * excessively restrictive.
 */
static unsigned
get_fpu_lowered_simd_width(const elk_fs_visitor *shader,
                           const elk_fs_inst *inst)
{
   const struct elk_compiler *compiler = shader->compiler;
   const struct intel_device_info *devinfo = compiler->devinfo;

   /* Maximum execution size representable in the instruction controls. */
   unsigned max_width = MIN2(32, inst->exec_size);

   /* According to the PRMs:
    *  "A. In Direct Addressing mode, a source cannot span more than 2
    *      adjacent GRF registers.
    *   B. A destination cannot span more than 2 adjacent GRF registers."
    *
    * Look for the source or destination with the largest register region
    * which is the one that is going to limit the overall execution size of
    * the instruction due to this rule.
    */
   unsigned reg_count = DIV_ROUND_UP(inst->size_written, REG_SIZE);

   for (unsigned i = 0; i < inst->sources; i++)
      reg_count = MAX2(reg_count, DIV_ROUND_UP(inst->size_read(i), REG_SIZE));

   /* Calculate the maximum execution size of the instruction based on the
    * factor by which it goes over the hardware limit of 2 GRFs.
    */
   const unsigned max_reg_count = 2 * reg_unit(devinfo);
   if (reg_count > max_reg_count)
      max_width = MIN2(max_width, inst->exec_size / DIV_ROUND_UP(reg_count, max_reg_count));

   /* According to the IVB PRMs:
    *  "When destination spans two registers, the source MUST span two
    *   registers. The exception to the above rule:
    *
    *    - When source is scalar, the source registers are not incremented.
    *    - When source is packed integer Word and destination is packed
    *      integer DWord, the source register is not incremented but the
    *      source sub register is incremented."
    *
    * The hardware specs from Gfx4 to Gfx7.5 mention similar regioning
    * restrictions.  The code below intentionally doesn't check whether the
    * destination type is integer because empirically the hardware doesn't
    * seem to care what the actual type is as long as it's dword-aligned.
    *
    * HSW PRMs also add a note to the second exception:
    *  "When lower 8 channels are disabled, the sub register of source1
    *   operand is not incremented. If the lower 8 channels are expected
    *   to be disabled, say by predication, the instruction must be split
    *   into pair of simd8 operations."
    *
    * We can't reliably know if the channels won't be disabled due to,
    * for example, IMASK. So, play it safe and disallow packed-word exception
    * for src1.
    */
   if (devinfo->ver < 8) {
      for (unsigned i = 0; i < inst->sources; i++) {
         /* IVB implements DF scalars as <0;2,1> regions. */
         const bool is_scalar_exception = is_uniform(inst->src[i]) &&
            (devinfo->platform == INTEL_PLATFORM_HSW || type_sz(inst->src[i].type) != 8);
         const bool is_packed_word_exception = i != 1 &&
            type_sz(inst->dst.type) == 4 && inst->dst.stride == 1 &&
            type_sz(inst->src[i].type) == 2 && inst->src[i].stride == 1;

         /* We check size_read(i) against size_written instead of REG_SIZE
          * because we want to properly handle SIMD32.  In SIMD32, you can end
          * up with writes to 4 registers and a source that reads 2 registers
          * and we may still need to lower all the way to SIMD8 in that case.
          */
         if (inst->size_written > REG_SIZE &&
             inst->size_read(i) != 0 &&
             inst->size_read(i) < inst->size_written &&
             !is_scalar_exception && !is_packed_word_exception) {
            const unsigned reg_count = DIV_ROUND_UP(inst->size_written, REG_SIZE);
            max_width = MIN2(max_width, inst->exec_size / reg_count);
         }
      }
   }

   if (devinfo->ver < 6) {
      /* From the G45 PRM, Volume 4 Page 361:
       *
       *    "Operand Alignment Rule: With the exceptions listed below, a
       *     source/destination operand in general should be aligned to even
       *     256-bit physical register with a region size equal to two 256-bit
       *     physical registers."
       *
       * Normally we enforce this by allocating virtual registers to the
       * even-aligned class.  But we need to handle payload registers.
       */
      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == FIXED_GRF && (inst->src[i].nr & 1) &&
             inst->size_read(i) > REG_SIZE) {
            max_width = MIN2(max_width, 8);
         }
      }
   }

   /* From the IVB PRMs:
    *  "When an instruction is SIMD32, the low 16 bits of the execution mask
    *   are applied for both halves of the SIMD32 instruction. If different
    *   execution mask channels are required, split the instruction into two
    *   SIMD16 instructions."
    *
    * There is similar text in the HSW PRMs.  Gfx4-6 don't even implement
    * 32-wide control flow support in hardware and will behave similarly.
    */
   if (devinfo->ver < 8 && !inst->force_writemask_all)
      max_width = MIN2(max_width, 16);

   /* From the IVB PRMs (applies to HSW too):
    *  "Instructions with condition modifiers must not use SIMD32."
    *
    * From the BDW PRMs (applies to later hardware too):
    *  "Ternary instruction with condition modifiers must not use SIMD32."
    */
   if (inst->conditional_mod && (devinfo->ver < 8 ||
                                 inst->elk_is_3src(compiler)))
      max_width = MIN2(max_width, 16);

   /* From the IVB PRMs (applies to other devices that don't have the
    * intel_device_info::supports_simd16_3src flag set):
    *  "In Align16 access mode, SIMD16 is not allowed for DW operations and
    *   SIMD8 is not allowed for DF operations."
    */
   if (inst->elk_is_3src(compiler) && !devinfo->supports_simd16_3src)
      max_width = MIN2(max_width, inst->exec_size / reg_count);

   /* Pre-Gfx8 EUs are hardwired to use the QtrCtrl+1 (where QtrCtrl is
    * the 8-bit quarter of the execution mask signals specified in the
    * instruction control fields) for the second compressed half of any
    * single-precision instruction (for double-precision instructions
    * it's hardwired to use NibCtrl+1, at least on HSW), which means that
    * the EU will apply the wrong execution controls for the second
    * sequential GRF write if the number of channels per GRF is not exactly
    * eight in single-precision mode (or four in double-float mode).
    *
    * In this situation we calculate the maximum size of the split
    * instructions so they only ever write to a single register.
    */
   if (devinfo->ver < 8 && inst->size_written > REG_SIZE &&
       !inst->force_writemask_all) {
      const unsigned channels_per_grf = inst->exec_size /
         DIV_ROUND_UP(inst->size_written, REG_SIZE);
      const unsigned exec_type_size = get_exec_type_size(inst);
      assert(exec_type_size);

      /* The hardware shifts exactly 8 channels per compressed half of the
       * instruction in single-precision mode and exactly 4 in double-precision.
       */
      if (channels_per_grf != (exec_type_size == 8 ? 4 : 8))
         max_width = MIN2(max_width, channels_per_grf);

      /* Lower all non-force_writemask_all DF instructions to SIMD4 on IVB/BYT
       * because HW applies the same channel enable signals to both halves of
       * the compressed instruction which will be just wrong under
       * non-uniform control flow.
       */
      if (devinfo->verx10 == 70 &&
          (exec_type_size == 8 || type_sz(inst->dst.type) == 8))
         max_width = MIN2(max_width, 4);
   }

   /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
    * Float Operations:
    *
    *    "No SIMD16 in mixed mode when destination is f32. Instruction
    *     execution size must be no more than 8."
    *
    * FIXME: the simulator doesn't seem to complain if we don't do this and
    * empirical testing with existing CTS tests show that they pass just fine
    * without implementing this, however, since our interpretation of the PRM
    * is that conversion MOVs between HF and F are still mixed-float
    * instructions (and therefore subject to this restriction) we decided to
    * split them to be safe. Might be useful to do additional investigation to
    * lift the restriction if we can ensure that it is safe though, since these
    * conversions are common when half-float types are involved since many
    * instructions do not support HF types and conversions from/to F are
    * required.
    */
   if (is_mixed_float_with_fp32_dst(inst))
      max_width = MIN2(max_width, 8);

   /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
    * Float Operations:
    *
    *    "No SIMD16 in mixed mode when destination is packed f16 for both
    *     Align1 and Align16."
    */
   if (is_mixed_float_with_packed_fp16_dst(inst))
      max_width = MIN2(max_width, 8);

   /* Only power-of-two execution sizes are representable in the instruction
    * control fields.
    */
   return 1 << util_logbase2(max_width);
}

/**
 * Get the maximum allowed SIMD width for instruction \p inst accounting for
 * various payload size restrictions that apply to sampler message
 * instructions.
 *
 * This is only intended to provide a maximum theoretical bound for the
 * execution size of the message based on the number of argument components
 * alone, which in most cases will determine whether the SIMD8 or SIMD16
 * variant of the message can be used, though some messages may have
 * additional restrictions not accounted for here (e.g. pre-ILK hardware uses
 * the message length to determine the exact SIMD width and argument count,
 * which makes a number of sampler message combinations impossible to
 * represent).
 *
 * Note: Platforms with monolithic SIMD16 double the possible SIMD widths
 * change from (SIMD8, SIMD16) to (SIMD16, SIMD32).
 */
static unsigned
get_sampler_lowered_simd_width(const struct intel_device_info *devinfo,
                               const elk_fs_inst *inst)
{
   /* If we have a min_lod parameter on anything other than a simple sample
    * message, it will push it over 5 arguments and we have to fall back to
    * SIMD8.
    */
   if (inst->opcode != ELK_SHADER_OPCODE_TEX &&
       inst->components_read(TEX_LOGICAL_SRC_MIN_LOD))
      return 8;

   /* Calculate the number of coordinate components that have to be present
    * assuming that additional arguments follow the texel coordinates in the
    * message payload.  On IVB+ there is no need for padding, on ILK-SNB we
    * need to pad to four or three components depending on the message,
    * pre-ILK we need to pad to at most three components.
    */
   const unsigned req_coord_components =
      (devinfo->ver >= 7 ||
       !inst->components_read(TEX_LOGICAL_SRC_COORDINATE)) ? 0 :
      (devinfo->ver >= 5 && inst->opcode != ELK_SHADER_OPCODE_TXF_LOGICAL &&
                            inst->opcode != ELK_SHADER_OPCODE_TXF_CMS_LOGICAL) ? 4 :
      3;

   /* Calculate the total number of argument components that need to be passed
    * to the sampler unit.
    */
   const unsigned num_payload_components =
      MAX2(inst->components_read(TEX_LOGICAL_SRC_COORDINATE),
           req_coord_components) +
      inst->components_read(TEX_LOGICAL_SRC_SHADOW_C) +
      inst->components_read(TEX_LOGICAL_SRC_LOD) +
      inst->components_read(TEX_LOGICAL_SRC_LOD2) +
      inst->components_read(TEX_LOGICAL_SRC_SAMPLE_INDEX) +
      (inst->opcode == ELK_SHADER_OPCODE_TG4_OFFSET_LOGICAL ?
       inst->components_read(TEX_LOGICAL_SRC_TG4_OFFSET) : 0) +
      inst->components_read(TEX_LOGICAL_SRC_MCS);

   const unsigned simd_limit = reg_unit(devinfo) *
      (num_payload_components > MAX_SAMPLER_MESSAGE_SIZE / 2 ? 8 : 16);

   /* SIMD16 (SIMD32 on Xe2) messages with more than five arguments exceed the
    * maximum message size supported by the sampler, regardless of whether a
    * header is provided or not.
    */
   return MIN2(inst->exec_size, simd_limit);
}

/**
 * Get the closest native SIMD width supported by the hardware for instruction
 * \p inst.  The instruction will be left untouched by
 * elk_fs_visitor::lower_simd_width() if the returned value is equal to the
 * original execution size.
 */
static unsigned
get_lowered_simd_width(const elk_fs_visitor *shader, const elk_fs_inst *inst)
{
   const struct elk_compiler *compiler = shader->compiler;
   const struct intel_device_info *devinfo = compiler->devinfo;

   switch (inst->opcode) {
   case ELK_OPCODE_MOV:
   case ELK_OPCODE_SEL:
   case ELK_OPCODE_NOT:
   case ELK_OPCODE_AND:
   case ELK_OPCODE_OR:
   case ELK_OPCODE_XOR:
   case ELK_OPCODE_SHR:
   case ELK_OPCODE_SHL:
   case ELK_OPCODE_ASR:
   case ELK_OPCODE_CMPN:
   case ELK_OPCODE_CSEL:
   case ELK_OPCODE_F32TO16:
   case ELK_OPCODE_F16TO32:
   case ELK_OPCODE_BFREV:
   case ELK_OPCODE_BFE:
   case ELK_OPCODE_ADD:
   case ELK_OPCODE_MUL:
   case ELK_OPCODE_AVG:
   case ELK_OPCODE_FRC:
   case ELK_OPCODE_RNDU:
   case ELK_OPCODE_RNDD:
   case ELK_OPCODE_RNDE:
   case ELK_OPCODE_RNDZ:
   case ELK_OPCODE_LZD:
   case ELK_OPCODE_FBH:
   case ELK_OPCODE_FBL:
   case ELK_OPCODE_CBIT:
   case ELK_OPCODE_SAD2:
   case ELK_OPCODE_MAD:
   case ELK_OPCODE_LRP:
   case ELK_FS_OPCODE_PACK:
   case ELK_SHADER_OPCODE_SEL_EXEC:
   case ELK_SHADER_OPCODE_CLUSTER_BROADCAST:
   case ELK_SHADER_OPCODE_MOV_RELOC_IMM:
      return get_fpu_lowered_simd_width(shader, inst);

   case ELK_OPCODE_CMP: {
      /* The Ivybridge/BayTrail WaCMPInstFlagDepClearedEarly workaround says that
       * when the destination is a GRF the dependency-clear bit on the flag
       * register is cleared early.
       *
       * Suggested workarounds are to disable coissuing CMP instructions
       * or to split CMP(16) instructions into two CMP(8) instructions.
       *
       * We choose to split into CMP(8) instructions since disabling
       * coissuing would affect CMP instructions not otherwise affected by
       * the errata.
       */
      const unsigned max_width = (devinfo->verx10 == 70 &&
                                  !inst->dst.is_null() ? 8 : ~0);
      return MIN2(max_width, get_fpu_lowered_simd_width(shader, inst));
   }
   case ELK_OPCODE_BFI1:
   case ELK_OPCODE_BFI2:
      /* The Haswell WaForceSIMD8ForBFIInstruction workaround says that we
       * should
       *  "Force BFI instructions to be executed always in SIMD8."
       */
      return MIN2(devinfo->platform == INTEL_PLATFORM_HSW ? 8 : ~0u,
                  get_fpu_lowered_simd_width(shader, inst));

   case ELK_OPCODE_IF:
      assert(inst->src[0].file == BAD_FILE || inst->exec_size <= 16);
      return inst->exec_size;

   case ELK_SHADER_OPCODE_RCP:
   case ELK_SHADER_OPCODE_RSQ:
   case ELK_SHADER_OPCODE_SQRT:
   case ELK_SHADER_OPCODE_EXP2:
   case ELK_SHADER_OPCODE_LOG2:
   case ELK_SHADER_OPCODE_SIN:
   case ELK_SHADER_OPCODE_COS: {
      /* Unary extended math instructions are limited to SIMD8 on Gfx4 and
       * Gfx6. Extended Math Function is limited to SIMD8 with half-float.
       */
      if (devinfo->ver == 6 || devinfo->verx10 == 40)
         return MIN2(8, inst->exec_size);
      if (inst->dst.type == ELK_REGISTER_TYPE_HF)
         return MIN2(8, inst->exec_size);
      return MIN2(16, inst->exec_size);
   }

   case ELK_SHADER_OPCODE_POW: {
      /* SIMD16 is only allowed on Gfx7+. Extended Math Function is limited
       * to SIMD8 with half-float
       */
      if (devinfo->ver < 7)
         return MIN2(8, inst->exec_size);
      if (inst->dst.type == ELK_REGISTER_TYPE_HF)
         return MIN2(8, inst->exec_size);
      return MIN2(16, inst->exec_size);
   }

   case ELK_SHADER_OPCODE_USUB_SAT:
   case ELK_SHADER_OPCODE_ISUB_SAT:
      return get_fpu_lowered_simd_width(shader, inst);

   case ELK_SHADER_OPCODE_INT_QUOTIENT:
   case ELK_SHADER_OPCODE_INT_REMAINDER:
      /* Integer division is limited to SIMD8 on all generations. */
      return MIN2(8, inst->exec_size);

   case ELK_FS_OPCODE_LINTERP:
   case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
   case ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      return MIN2(16, inst->exec_size);

   case ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL:
      /* Pre-ILK hardware doesn't have a SIMD8 variant of the texel fetch
       * message used to implement varying pull constant loads, so expand it
       * to SIMD16.  An alternative with longer message payload length but
       * shorter return payload would be to use the SIMD8 sampler message that
       * takes (header, u, v, r) as parameters instead of (header, u).
       */
      return (devinfo->ver == 4 ? 16 : MIN2(16, inst->exec_size));

   case ELK_FS_OPCODE_DDX_COARSE:
   case ELK_FS_OPCODE_DDX_FINE:
   case ELK_FS_OPCODE_DDY_COARSE:
   case ELK_FS_OPCODE_DDY_FINE:
      /* The implementation of this virtual opcode may require emitting
       * compressed Align16 instructions, which are severely limited on some
       * generations.
       *
       * From the Ivy Bridge PRM, volume 4 part 3, section 3.3.9 (Register
       * Region Restrictions):
       *
       *  "In Align16 access mode, SIMD16 is not allowed for DW operations
       *   and SIMD8 is not allowed for DF operations."
       *
       * In this context, "DW operations" means "operations acting on 32-bit
       * values", so it includes operations on floats.
       *
       * Gfx4 has a similar restriction.  From the i965 PRM, section 11.5.3
       * (Instruction Compression -> Rules and Restrictions):
       *
       *  "A compressed instruction must be in Align1 access mode. Align16
       *   mode instructions cannot be compressed."
       *
       * Similar text exists in the g45 PRM.
       *
       * Empirically, compressed align16 instructions using odd register
       * numbers don't appear to work on Sandybridge either.
       */
      return (devinfo->ver == 4 || devinfo->ver == 6 ||
              (devinfo->verx10 == 70) ?
              MIN2(8, inst->exec_size) : MIN2(16, inst->exec_size));

   case ELK_SHADER_OPCODE_MULH:
      /* MULH is lowered to the MUL/MACH sequence using the accumulator, which
       * is 8-wide on Gfx7+.
       */
      return (devinfo->ver >= 7 ? 8 :
              get_fpu_lowered_simd_width(shader, inst));

   case ELK_FS_OPCODE_FB_WRITE_LOGICAL:
      /* Gfx6 doesn't support SIMD16 depth writes but we cannot handle them
       * here.
       */
      assert(devinfo->ver != 6 ||
             inst->src[FB_WRITE_LOGICAL_SRC_SRC_DEPTH].file == BAD_FILE ||
             inst->exec_size == 8);
      /* Dual-source FB writes are unsupported in SIMD16 mode. */
      return (inst->src[FB_WRITE_LOGICAL_SRC_COLOR1].file != BAD_FILE ?
              8 : MIN2(16, inst->exec_size));

   case ELK_SHADER_OPCODE_TEX_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_UMS_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_MCS_LOGICAL:
   case ELK_SHADER_OPCODE_LOD_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_LOGICAL:
   case ELK_SHADER_OPCODE_SAMPLEINFO_LOGICAL:
   case ELK_SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case ELK_SHADER_OPCODE_TG4_OFFSET_LOGICAL:
      return get_sampler_lowered_simd_width(devinfo, inst);

   /* On gfx12 parameters are fixed to 16-bit values and therefore they all
    * always fit regardless of the execution size.
    */
   case ELK_SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      return MIN2(16, inst->exec_size);

   case ELK_SHADER_OPCODE_TXD_LOGICAL:
      /* TXD is unsupported in SIMD16 mode previous to Xe2. SIMD32 is still
       * unsuppported on Xe2.
       */
      return 8;

   case ELK_SHADER_OPCODE_TXL_LOGICAL:
   case ELK_FS_OPCODE_TXB_LOGICAL:
      /* Only one execution size is representable pre-ILK depending on whether
       * the shadow reference argument is present.
       */
      if (devinfo->ver == 4)
         return inst->src[TEX_LOGICAL_SRC_SHADOW_C].file == BAD_FILE ? 16 : 8;
      else
         return get_sampler_lowered_simd_width(devinfo, inst);

   case ELK_SHADER_OPCODE_TXF_LOGICAL:
   case ELK_SHADER_OPCODE_TXS_LOGICAL:
      /* Gfx4 doesn't have SIMD8 variants for the RESINFO and LD-with-LOD
       * messages.  Use SIMD16 instead.
       */
      if (devinfo->ver == 4)
         return 16;
      else
         return get_sampler_lowered_simd_width(devinfo, inst);

   case ELK_SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
   case ELK_SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
   case ELK_SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      return 8;

   case ELK_SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
   case ELK_SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case ELK_SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case ELK_SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      return MIN2(16, inst->exec_size);

   case ELK_SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
   case ELK_SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
   case ELK_SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      return devinfo->ver <= 8 ? 8 : MIN2(16, inst->exec_size);

   case ELK_SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
   case ELK_SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
   case ELK_SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      assert(inst->exec_size <= 16);
      return inst->exec_size;

   case ELK_SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      return devinfo->has_lsc ? MIN2(16, inst->exec_size) : 8;

   case ELK_SHADER_OPCODE_URB_READ_LOGICAL:
   case ELK_SHADER_OPCODE_URB_WRITE_LOGICAL:
      return MIN2(8, inst->exec_size);

   case ELK_SHADER_OPCODE_QUAD_SWIZZLE: {
      const unsigned swiz = inst->src[1].ud;
      return (is_uniform(inst->src[0]) ?
                 get_fpu_lowered_simd_width(shader, inst) :
              type_sz(inst->src[0].type) == 4 ? 8 :
              swiz == ELK_SWIZZLE_XYXY || swiz == ELK_SWIZZLE_ZWZW ? 4 :
              get_fpu_lowered_simd_width(shader, inst));
   }
   case ELK_SHADER_OPCODE_MOV_INDIRECT: {
      /* From IVB and HSW PRMs:
       *
       * "2.When the destination requires two registers and the sources are
       *  indirect, the sources must use 1x1 regioning mode.
       *
       * In case of DF instructions in HSW/IVB, the exec_size is limited by
       * the EU decompression logic not handling VxH indirect addressing
       * correctly.
       */
      const unsigned max_size = (devinfo->ver >= 8 ? 2 : 1) * REG_SIZE;
      /* Prior to Broadwell, we only have 8 address subregisters. */
      return MIN3(devinfo->ver >= 8 ? 16 : 8,
                  max_size / (inst->dst.stride * type_sz(inst->dst.type)),
                  inst->exec_size);
   }

   case ELK_SHADER_OPCODE_LOAD_PAYLOAD: {
      const unsigned reg_count =
         DIV_ROUND_UP(inst->dst.component_size(inst->exec_size), REG_SIZE);

      if (reg_count > 2) {
         /* Only LOAD_PAYLOAD instructions with per-channel destination region
          * can be easily lowered (which excludes headers and heterogeneous
          * types).
          */
         assert(!inst->header_size);
         for (unsigned i = 0; i < inst->sources; i++)
            assert(type_sz(inst->dst.type) == type_sz(inst->src[i].type) ||
                   inst->src[i].file == BAD_FILE);

         return inst->exec_size / DIV_ROUND_UP(reg_count, 2);
      } else {
         return inst->exec_size;
      }
   }
   default:
      return inst->exec_size;
   }
}

/**
 * Return true if splitting out the group of channels of instruction \p inst
 * given by lbld.group() requires allocating a temporary for the i-th source
 * of the lowered instruction.
 */
static inline bool
needs_src_copy(const fs_builder &lbld, const elk_fs_inst *inst, unsigned i)
{
   return !(is_periodic(inst->src[i], lbld.dispatch_width()) ||
            (inst->components_read(i) == 1 &&
             lbld.dispatch_width() <= inst->exec_size)) ||
          (inst->flags_written(lbld.shader->devinfo) &
           flag_mask(inst->src[i], type_sz(inst->src[i].type)));
}

/**
 * Extract the data that would be consumed by the channel group given by
 * lbld.group() from the i-th source region of instruction \p inst and return
 * it as result in packed form.
 */
static elk_fs_reg
emit_unzip(const fs_builder &lbld, elk_fs_inst *inst, unsigned i)
{
   assert(lbld.group() >= inst->group);

   /* Specified channel group from the source region. */
   const elk_fs_reg src = horiz_offset(inst->src[i], lbld.group() - inst->group);

   if (needs_src_copy(lbld, inst, i)) {
      /* Builder of the right width to perform the copy avoiding uninitialized
       * data if the lowered execution size is greater than the original
       * execution size of the instruction.
       */
      const fs_builder cbld = lbld.group(MIN2(lbld.dispatch_width(),
                                              inst->exec_size), 0);
      const elk_fs_reg tmp = lbld.vgrf(inst->src[i].type, inst->components_read(i));

      for (unsigned k = 0; k < inst->components_read(i); ++k)
         cbld.MOV(offset(tmp, lbld, k), offset(src, inst->exec_size, k));

      return tmp;

   } else if (is_periodic(inst->src[i], lbld.dispatch_width())) {
      /* The source is invariant for all dispatch_width-wide groups of the
       * original region.
       */
      return inst->src[i];

   } else {
      /* We can just point the lowered instruction at the right channel group
       * from the original region.
       */
      return src;
   }
}

/**
 * Return true if splitting out the group of channels of instruction \p inst
 * given by lbld.group() requires allocating a temporary for the destination
 * of the lowered instruction and copying the data back to the original
 * destination region.
 */
static inline bool
needs_dst_copy(const fs_builder &lbld, const elk_fs_inst *inst)
{
   if (inst->dst.is_null())
      return false;

   /* If the instruction writes more than one component we'll have to shuffle
    * the results of multiple lowered instructions in order to make sure that
    * they end up arranged correctly in the original destination region.
    */
   if (inst->size_written > inst->dst.component_size(inst->exec_size))
      return true;

   /* If the lowered execution size is larger than the original the result of
    * the instruction won't fit in the original destination, so we'll have to
    * allocate a temporary in any case.
    */
   if (lbld.dispatch_width() > inst->exec_size)
      return true;

   for (unsigned i = 0; i < inst->sources; i++) {
      /* If we already made a copy of the source for other reasons there won't
       * be any overlap with the destination.
       */
      if (needs_src_copy(lbld, inst, i))
         continue;

      /* In order to keep the logic simple we emit a copy whenever the
       * destination region doesn't exactly match an overlapping source, which
       * may point at the source and destination not being aligned group by
       * group which could cause one of the lowered instructions to overwrite
       * the data read from the same source by other lowered instructions.
       */
      if (regions_overlap(inst->dst, inst->size_written,
                          inst->src[i], inst->size_read(i)) &&
          !inst->dst.equals(inst->src[i]))
        return true;
   }

   return false;
}

/**
 * Insert data from a packed temporary into the channel group given by
 * lbld.group() of the destination region of instruction \p inst and return
 * the temporary as result.  Any copy instructions that are required for
 * unzipping the previous value (in the case of partial writes) will be
 * inserted using \p lbld_before and any copy instructions required for
 * zipping up the destination of \p inst will be inserted using \p lbld_after.
 */
static elk_fs_reg
emit_zip(const fs_builder &lbld_before, const fs_builder &lbld_after,
         elk_fs_inst *inst)
{
   assert(lbld_before.dispatch_width() == lbld_after.dispatch_width());
   assert(lbld_before.group() == lbld_after.group());
   assert(lbld_after.group() >= inst->group);

   const struct intel_device_info *devinfo = lbld_before.shader->devinfo;

   /* Specified channel group from the destination region. */
   const elk_fs_reg dst = horiz_offset(inst->dst, lbld_after.group() - inst->group);

   if (!needs_dst_copy(lbld_after, inst)) {
      /* No need to allocate a temporary for the lowered instruction, just
       * take the right group of channels from the original region.
       */
      return dst;
   }

   /* Deal with the residency data part later */
   const unsigned residency_size = inst->has_sampler_residency() ?
      (reg_unit(devinfo) * REG_SIZE) : 0;
   const unsigned dst_size = (inst->size_written - residency_size) /
      inst->dst.component_size(inst->exec_size);

   const elk_fs_reg tmp = lbld_after.vgrf(inst->dst.type,
                                      dst_size + inst->has_sampler_residency());

   if (inst->predicate) {
      /* Handle predication by copying the original contents of the
       * destination into the temporary before emitting the lowered
       * instruction.
       */
      const fs_builder gbld_before =
         lbld_before.group(MIN2(lbld_before.dispatch_width(),
                                inst->exec_size), 0);
      for (unsigned k = 0; k < dst_size; ++k) {
         gbld_before.MOV(offset(tmp, lbld_before, k),
                         offset(dst, inst->exec_size, k));
      }
   }

   const fs_builder gbld_after =
      lbld_after.group(MIN2(lbld_after.dispatch_width(),
                            inst->exec_size), 0);
   for (unsigned k = 0; k < dst_size; ++k) {
      /* Use a builder of the right width to perform the copy avoiding
       * uninitialized data if the lowered execution size is greater than the
       * original execution size of the instruction.
       */
      gbld_after.MOV(offset(dst, inst->exec_size, k),
                     offset(tmp, lbld_after, k));
   }

   if (inst->has_sampler_residency()) {
      /* Sampler messages with residency need a special attention. In the
       * first lane of the last component are located the Pixel Null Mask
       * (bits 0:15) & some upper bits we need to discard (bits 16:31). We
       * have to build a single 32bit value for the SIMD32 message out of 2
       * SIMD16 16 bit values.
       */
      const fs_builder rbld = gbld_after.exec_all().group(1, 0);
      elk_fs_reg local_res_reg = component(
         retype(offset(tmp, lbld_before, dst_size),
                ELK_REGISTER_TYPE_UW), 0);
      elk_fs_reg final_res_reg =
         retype(byte_offset(inst->dst,
                            inst->size_written - residency_size +
                            gbld_after.group() / 8),
                ELK_REGISTER_TYPE_UW);
      rbld.MOV(final_res_reg, local_res_reg);
   }

   return tmp;
}

bool
elk_fs_visitor::lower_simd_width()
{
   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      const unsigned lower_width = get_lowered_simd_width(this, inst);

      if (lower_width != inst->exec_size) {
         /* Builder matching the original instruction.  We may also need to
          * emit an instruction of width larger than the original, set the
          * execution size of the builder to the highest of both for now so
          * we're sure that both cases can be handled.
          */
         const unsigned max_width = MAX2(inst->exec_size, lower_width);

         const fs_builder bld =
            fs_builder(this, MAX2(max_width, dispatch_width)).at_end();
         const fs_builder ibld = bld.at(block, inst)
                                    .exec_all(inst->force_writemask_all)
                                    .group(max_width, inst->group / max_width);

         /* Split the copies in chunks of the execution width of either the
          * original or the lowered instruction, whichever is lower.
          */
         const unsigned n = DIV_ROUND_UP(inst->exec_size, lower_width);
         const unsigned residency_size = inst->has_sampler_residency() ?
            (reg_unit(devinfo) * REG_SIZE) : 0;
         const unsigned dst_size =
            (inst->size_written - residency_size) /
            inst->dst.component_size(inst->exec_size);

         assert(!inst->writes_accumulator && !inst->mlen);

         /* Inserting the zip, unzip, and duplicated instructions in all of
          * the right spots is somewhat tricky.  All of the unzip and any
          * instructions from the zip which unzip the destination prior to
          * writing need to happen before all of the per-group instructions
          * and the zip instructions need to happen after.  In order to sort
          * this all out, we insert the unzip instructions before \p inst,
          * insert the per-group instructions after \p inst (i.e. before
          * inst->next), and insert the zip instructions before the
          * instruction after \p inst.  Since we are inserting instructions
          * after \p inst, inst->next is a moving target and we need to save
          * it off here so that we insert the zip instructions in the right
          * place.
          *
          * Since we're inserting split instructions after after_inst, the
          * instructions will end up in the reverse order that we insert them.
          * However, certain render target writes require that the low group
          * instructions come before the high group.  From the Ivy Bridge PRM
          * Vol. 4, Pt. 1, Section 3.9.11:
          *
          *    "If multiple SIMD8 Dual Source messages are delivered by the
          *    pixel shader thread, each SIMD8_DUALSRC_LO message must be
          *    issued before the SIMD8_DUALSRC_HI message with the same Slot
          *    Group Select setting."
          *
          * And, from Section 3.9.11.1 of the same PRM:
          *
          *    "When SIMD32 or SIMD16 PS threads send render target writes
          *    with multiple SIMD8 and SIMD16 messages, the following must
          *    hold:
          *
          *    All the slots (as described above) must have a corresponding
          *    render target write irrespective of the slot's validity. A slot
          *    is considered valid when at least one sample is enabled. For
          *    example, a SIMD16 PS thread must send two SIMD8 render target
          *    writes to cover all the slots.
          *
          *    PS thread must send SIMD render target write messages with
          *    increasing slot numbers. For example, SIMD16 thread has
          *    Slot[15:0] and if two SIMD8 render target writes are used, the
          *    first SIMD8 render target write must send Slot[7:0] and the
          *    next one must send Slot[15:8]."
          *
          * In order to make low group instructions come before high group
          * instructions (this is required for some render target writes), we
          * split from the highest group to lowest.
          */
         exec_node *const after_inst = inst->next;
         for (int i = n - 1; i >= 0; i--) {
            /* Emit a copy of the original instruction with the lowered width.
             * If the EOT flag was set throw it away except for the last
             * instruction to avoid killing the thread prematurely.
             */
            elk_fs_inst split_inst = *inst;
            split_inst.exec_size = lower_width;
            split_inst.eot = inst->eot && i == int(n - 1);

            /* Select the correct channel enables for the i-th group, then
             * transform the sources and destination and emit the lowered
             * instruction.
             */
            const fs_builder lbld = ibld.group(lower_width, i);

            for (unsigned j = 0; j < inst->sources; j++)
               split_inst.src[j] = emit_unzip(lbld.at(block, inst), inst, j);

            split_inst.dst = emit_zip(lbld.at(block, inst),
                                      lbld.at(block, after_inst), inst);
            split_inst.size_written =
               split_inst.dst.component_size(lower_width) * dst_size +
               residency_size;

            lbld.at(block, inst->next).emit(split_inst);
         }

         inst->remove(block);
         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Transform barycentric vectors into the interleaved form expected by the PLN
 * instruction and returned by the Gfx7+ PI shared function.
 *
 * For channels 0-15 in SIMD16 mode they are expected to be laid out as
 * follows in the register file:
 *
 *    rN+0: X[0-7]
 *    rN+1: Y[0-7]
 *    rN+2: X[8-15]
 *    rN+3: Y[8-15]
 *
 * There is no need to handle SIMD32 here -- This is expected to be run after
 * SIMD lowering, since SIMD lowering relies on vectors having the standard
 * component layout.
 */
bool
elk_fs_visitor::lower_barycentrics()
{
   const bool has_interleaved_layout = devinfo->has_pln ||
                                       devinfo->ver >= 7;
   bool progress = false;

   if (stage != MESA_SHADER_FRAGMENT || !has_interleaved_layout)
      return false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      if (inst->exec_size < 16)
         continue;

      const fs_builder ibld(this, block, inst);
      const fs_builder ubld = ibld.exec_all().group(8, 0);

      switch (inst->opcode) {
      case ELK_FS_OPCODE_LINTERP : {
         assert(inst->exec_size == 16);
         const elk_fs_reg tmp = ibld.vgrf(inst->src[0].type, 2);
         elk_fs_reg srcs[4];

         for (unsigned i = 0; i < ARRAY_SIZE(srcs); i++)
            srcs[i] = horiz_offset(offset(inst->src[0], ibld, i % 2),
                                   8 * (i / 2));

         ubld.LOAD_PAYLOAD(tmp, srcs, ARRAY_SIZE(srcs), ARRAY_SIZE(srcs));

         inst->src[0] = tmp;
         progress = true;
         break;
      }
      case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET: {
         assert(inst->exec_size == 16);
         const elk_fs_reg tmp = ibld.vgrf(inst->dst.type, 2);

         for (unsigned i = 0; i < 2; i++) {
            for (unsigned g = 0; g < inst->exec_size / 8; g++) {
               elk_fs_inst *mov = ibld.at(block, inst->next).group(8, g)
                                  .MOV(horiz_offset(offset(inst->dst, ibld, i),
                                                    8 * g),
                                       offset(tmp, ubld, 2 * g + i));
               mov->predicate = inst->predicate;
               mov->predicate_inverse = inst->predicate_inverse;
               mov->flag_subreg = inst->flag_subreg;
            }
         }

         inst->dst = tmp;
         progress = true;
         break;
      }
      default:
         break;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

bool
elk_fs_visitor::lower_find_live_channel()
{
   bool progress = false;

   if (devinfo->ver < 8)
      return false;

   bool packed_dispatch =
      elk_stage_has_packed_dispatch(devinfo, stage, stage_prog_data);
   bool vmask =
      stage == MESA_SHADER_FRAGMENT &&
      elk_wm_prog_data(stage_prog_data)->uses_vmask;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      if (inst->opcode != ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL &&
          inst->opcode != ELK_SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL)
         continue;

      bool first = inst->opcode == ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL;

      /* Getting the first active channel index is easy on Gfx8: Just find
       * the first bit set in the execution mask.  The register exists on
       * HSW already but it reads back as all ones when the current
       * instruction has execution masking disabled, so it's kind of
       * useless there.
       */
      elk_fs_reg exec_mask(retype(elk_mask_reg(0), ELK_REGISTER_TYPE_UD));

      const fs_builder ibld(this, block, inst);
      if (!inst->is_partial_write())
         ibld.emit_undef_for_dst(inst);

      const fs_builder ubld = fs_builder(this, block, inst).exec_all().group(1, 0);

      /* ce0 doesn't consider the thread dispatch mask (DMask or VMask),
       * so combine the execution and dispatch masks to obtain the true mask.
       *
       * If we're looking for the first live channel, and we have packed
       * dispatch, we can skip this step, as we know all dispatched channels
       * will appear at the front of the mask.
       */
      if (!(first && packed_dispatch)) {
         elk_fs_reg mask = ubld.vgrf(ELK_REGISTER_TYPE_UD);
         ubld.UNDEF(mask);
         ubld.emit(ELK_SHADER_OPCODE_READ_SR_REG, mask, elk_imm_ud(vmask ? 3 : 2));

         /* Quarter control has the effect of magically shifting the value of
          * ce0 so you'll get the first/last active channel relative to the
          * specified quarter control as result.
          */
         if (inst->group > 0)
            ubld.SHR(mask, mask, elk_imm_ud(ALIGN(inst->group, 8)));

         ubld.AND(mask, exec_mask, mask);
         exec_mask = mask;
      }

      if (first) {
         ubld.FBL(inst->dst, exec_mask);
      } else {
         elk_fs_reg tmp = ubld.vgrf(ELK_REGISTER_TYPE_UD, 1);
         ubld.UNDEF(tmp);
         ubld.LZD(tmp, exec_mask);
         ubld.ADD(inst->dst, negate(tmp), elk_imm_uw(31));
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

void
elk_fs_visitor::dump_instructions_to_file(FILE *file) const
{
   if (cfg) {
      const register_pressure &rp = regpressure_analysis.require();
      unsigned ip = 0, max_pressure = 0;
      unsigned cf_count = 0;
      foreach_block_and_inst(block, elk_backend_instruction, inst, cfg) {
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
   } else {
      int ip = 0;
      foreach_in_list(elk_backend_instruction, inst, &instructions) {
         fprintf(file, "%4d: ", ip++);
         dump_instruction(inst, file);
      }
   }
}

void
elk_fs_visitor::dump_instruction_to_file(const elk_backend_instruction *be_inst, FILE *file) const
{
   const elk_fs_inst *inst = (const elk_fs_inst *)be_inst;

   if (inst->predicate) {
      fprintf(file, "(%cf%d.%d) ",
              inst->predicate_inverse ? '-' : '+',
              inst->flag_subreg / 2,
              inst->flag_subreg % 2);
   }

   fprintf(file, "%s", elk_instruction_name(&compiler->isa, inst->opcode));
   if (inst->saturate)
      fprintf(file, ".sat");
   if (inst->conditional_mod) {
      fprintf(file, "%s", elk_conditional_modifier[inst->conditional_mod]);
      if (!inst->predicate &&
          (devinfo->ver < 5 || (inst->opcode != ELK_OPCODE_SEL &&
                                inst->opcode != ELK_OPCODE_CSEL &&
                                inst->opcode != ELK_OPCODE_IF &&
                                inst->opcode != ELK_OPCODE_WHILE))) {
         fprintf(file, ".f%d.%d", inst->flag_subreg / 2,
                 inst->flag_subreg % 2);
      }
   }
   fprintf(file, "(%d) ", inst->exec_size);

   if (inst->mlen) {
      fprintf(file, "(mlen: %d) ", inst->mlen);
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
      break;
   case MRF:
      fprintf(file, "m%d", inst->dst.nr);
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
      switch (inst->dst.nr) {
      case ELK_ARF_NULL:
         fprintf(file, "null");
         break;
      case ELK_ARF_ADDRESS:
         fprintf(file, "a0.%d", inst->dst.subnr);
         break;
      case ELK_ARF_ACCUMULATOR:
         fprintf(file, "acc%d", inst->dst.subnr);
         break;
      case ELK_ARF_FLAG:
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
   fprintf(file, ":%s, ", elk_reg_type_to_letters(inst->dst.type));

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
      case MRF:
         fprintf(file, "***m%d***", inst->src[i].nr);
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
         case ELK_REGISTER_TYPE_HF:
            fprintf(file, "%-ghf", _mesa_half_to_float(inst->src[i].ud & 0xffff));
            break;
         case ELK_REGISTER_TYPE_F:
            fprintf(file, "%-gf", inst->src[i].f);
            break;
         case ELK_REGISTER_TYPE_DF:
            fprintf(file, "%fdf", inst->src[i].df);
            break;
         case ELK_REGISTER_TYPE_W:
         case ELK_REGISTER_TYPE_D:
            fprintf(file, "%dd", inst->src[i].d);
            break;
         case ELK_REGISTER_TYPE_UW:
         case ELK_REGISTER_TYPE_UD:
            fprintf(file, "%uu", inst->src[i].ud);
            break;
         case ELK_REGISTER_TYPE_Q:
            fprintf(file, "%" PRId64 "q", inst->src[i].d64);
            break;
         case ELK_REGISTER_TYPE_UQ:
            fprintf(file, "%" PRIu64 "uq", inst->src[i].u64);
            break;
         case ELK_REGISTER_TYPE_VF:
            fprintf(file, "[%-gF, %-gF, %-gF, %-gF]",
                    elk_vf_to_float((inst->src[i].ud >>  0) & 0xff),
                    elk_vf_to_float((inst->src[i].ud >>  8) & 0xff),
                    elk_vf_to_float((inst->src[i].ud >> 16) & 0xff),
                    elk_vf_to_float((inst->src[i].ud >> 24) & 0xff));
            break;
         case ELK_REGISTER_TYPE_V:
         case ELK_REGISTER_TYPE_UV:
            fprintf(file, "%08x%s", inst->src[i].ud,
                    inst->src[i].type == ELK_REGISTER_TYPE_V ? "V" : "UV");
            break;
         default:
            fprintf(file, "???");
            break;
         }
         break;
      case ARF:
         switch (inst->src[i].nr) {
         case ELK_ARF_NULL:
            fprintf(file, "null");
            break;
         case ELK_ARF_ADDRESS:
            fprintf(file, "a0.%d", inst->src[i].subnr);
            break;
         case ELK_ARF_ACCUMULATOR:
            fprintf(file, "acc%d", inst->src[i].subnr);
            break;
         case ELK_ARF_FLAG:
            fprintf(file, "f%d.%d", inst->src[i].nr & 0xf, inst->src[i].subnr);
            break;
         default:
            fprintf(file, "arf%d.%d", inst->src[i].nr & 0xf, inst->src[i].subnr);
            break;
         }
         break;
      }

      if (inst->src[i].offset ||
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

         fprintf(file, ":%s", elk_reg_type_to_letters(inst->src[i].type));
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

elk::register_pressure::register_pressure(const elk_fs_visitor *v)
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

elk::register_pressure::~register_pressure()
{
   delete[] regs_live_at_ip;
}

void
elk_fs_visitor::invalidate_analysis(elk::analysis_dependency_class c)
{
   elk_backend_shader::invalidate_analysis(c);
   live_analysis.invalidate(c);
   regpressure_analysis.invalidate(c);
}

void
elk_fs_visitor::debug_optimizer(const nir_shader *nir,
                            const char *pass_name,
                            int iteration, int pass_num) const
{
   if (!elk_should_print_shader(nir, DEBUG_OPTIMIZER))
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

void
elk_fs_visitor::optimize()
{
   debug_optimizer(nir, "start", 0, 0);

   /* Start by validating the shader we currently have. */
   validate();

   bool progress = false;
   int iteration = 0;
   int pass_num = 0;

#define OPT(pass, args...) ({                                           \
      pass_num++;                                                       \
      bool this_progress = pass(args);                                  \
                                                                        \
      if (this_progress)                                                \
         debug_optimizer(nir, #pass, iteration, pass_num);              \
                                                                        \
      validate();                                                       \
                                                                        \
      progress = progress || this_progress;                             \
      this_progress;                                                    \
   })

   assign_constant_locations();
   OPT(lower_constant_loads);

   validate();

   OPT(split_virtual_grfs);

   /* Before anything else, eliminate dead code.  The results of some NIR
    * instructions may effectively be calculated twice.  Once when the
    * instruction is encountered, and again when the user of that result is
    * encountered.  Wipe those away before algebraic optimizations and
    * especially copy propagation can mix things up.
    */
   OPT(dead_code_eliminate);

   OPT(remove_extra_rounding_modes);

   do {
      progress = false;
      pass_num = 0;
      iteration++;

      OPT(remove_duplicate_mrf_writes);

      OPT(opt_algebraic);
      OPT(opt_cse);
      OPT(opt_copy_propagation);
      OPT(elk_opt_predicated_break, this);
      OPT(opt_cmod_propagation);
      OPT(dead_code_eliminate);
      OPT(opt_peephole_sel);
      OPT(elk_dead_control_flow_eliminate, this);
      OPT(opt_saturate_propagation);
      OPT(register_coalesce);
      OPT(compute_to_mrf);
      OPT(eliminate_find_live_channel);

      OPT(compact_virtual_grfs);
   } while (progress);

   progress = false;
   pass_num = 0;

   if (OPT(lower_pack)) {
      OPT(register_coalesce);
      OPT(dead_code_eliminate);
   }

   OPT(lower_simd_width);
   OPT(lower_barycentrics);
   OPT(lower_logical_sends);

   /* After logical SEND lowering. */

   if (OPT(opt_copy_propagation))
      OPT(opt_algebraic);

   /* Identify trailing zeros LOAD_PAYLOAD of sampler messages.
    * Do this before splitting SENDs.
    */
   if (devinfo->ver >= 7) {
      if (OPT(opt_zero_samples) && OPT(opt_copy_propagation))
         OPT(opt_algebraic);
   }

   if (progress) {
      if (OPT(opt_copy_propagation))
         OPT(opt_algebraic);

      /* Run after logical send lowering to give it a chance to CSE the
       * LOAD_PAYLOAD instructions created to construct the payloads of
       * e.g. texturing messages in cases where it wasn't possible to CSE the
       * whole logical instruction.
       */
      OPT(opt_cse);
      OPT(register_coalesce);
      OPT(compute_to_mrf);
      OPT(dead_code_eliminate);
      OPT(remove_duplicate_mrf_writes);
      OPT(opt_peephole_sel);
   }

   OPT(opt_redundant_halt);

   if (OPT(lower_load_payload)) {
      OPT(split_virtual_grfs);

      /* Lower 64 bit MOVs generated by payload lowering. */
      if (!devinfo->has_64bit_float || !devinfo->has_64bit_int)
         OPT(opt_algebraic);

      OPT(register_coalesce);
      OPT(lower_simd_width);
      OPT(compute_to_mrf);
      OPT(dead_code_eliminate);
   }

   OPT(opt_combine_constants);
   if (OPT(lower_integer_multiplication)) {
      /* If lower_integer_multiplication made progress, it may have produced
       * some 32x32-bit MULs in the process of lowering 64-bit MULs.  Run it
       * one more time to clean those up if they exist.
       */
      OPT(lower_integer_multiplication);
   }
   OPT(lower_sub_sat);

   if (devinfo->ver <= 5 && OPT(lower_minmax)) {
      OPT(opt_cmod_propagation);
      OPT(opt_cse);
      if (OPT(opt_copy_propagation))
         OPT(opt_algebraic);
      OPT(dead_code_eliminate);
   }

   progress = false;
   OPT(lower_regioning);
   if (progress) {
      if (OPT(opt_copy_propagation))
         OPT(opt_algebraic);
      OPT(dead_code_eliminate);
      OPT(lower_simd_width);
   }

   OPT(lower_uniform_pull_constant_loads);

   OPT(lower_find_live_channel);

   validate();
}

/**
 * Three source instruction must have a GRF/MRF destination register.
 * ARF NULL is not allowed.  Fix that up by allocating a temporary GRF.
 */
void
elk_fs_visitor::fixup_3src_null_dest()
{
   bool progress = false;

   foreach_block_and_inst_safe (block, elk_fs_inst, inst, cfg) {
      if (inst->elk_is_3src(compiler) && inst->dst.is_null()) {
         inst->dst = elk_fs_reg(VGRF, alloc.allocate(dispatch_width / 8),
                            inst->dst.type);
         progress = true;
      }
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL |
                          DEPENDENCY_VARIABLES);
}

uint32_t
elk_fs_visitor::compute_max_register_pressure()
{
   const register_pressure &rp = regpressure_analysis.require();
   uint32_t ip = 0, max_pressure = 0;
   foreach_block_and_inst(block, elk_backend_instruction, inst, cfg) {
      max_pressure = MAX2(max_pressure, rp.regs_live_at_ip[ip]);
      ip++;
   }
   return max_pressure;
}

static elk_fs_inst **
save_instruction_order(const struct elk_cfg_t *cfg)
{
   /* Before we schedule anything, stash off the instruction order as an array
    * of elk_fs_inst *.  This way, we can reset it between scheduling passes to
    * prevent dependencies between the different scheduling modes.
    */
   int num_insts = cfg->last_block()->end_ip + 1;
   elk_fs_inst **inst_arr = new elk_fs_inst * [num_insts];

   int ip = 0;
   foreach_block_and_inst(block, elk_fs_inst, inst, cfg) {
      assert(ip >= block->start_ip && ip <= block->end_ip);
      inst_arr[ip++] = inst;
   }
   assert(ip == num_insts);

   return inst_arr;
}

static void
restore_instruction_order(struct elk_cfg_t *cfg, elk_fs_inst **inst_arr)
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

void
elk_fs_visitor::allocate_registers(bool allow_spilling)
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

   compact_virtual_grfs();

   if (needs_register_pressure)
      shader_stats.max_register_pressure = compute_max_register_pressure();

   debug_optimizer(nir, "pre_register_allocate", 90, 90);

   bool spill_all = allow_spilling && INTEL_DEBUG(DEBUG_SPILL_FS);

   /* Before we schedule anything, stash off the instruction order as an array
    * of elk_fs_inst *.  This way, we can reset it between scheduling passes to
    * prevent dependencies between the different scheduling modes.
    */
   elk_fs_inst **orig_order = save_instruction_order(cfg);
   elk_fs_inst **best_pressure_order = NULL;

   void *scheduler_ctx = ralloc_context(NULL);
   elk_fs_instruction_scheduler *sched = prepare_scheduler(scheduler_ctx);

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
      elk_shader_perf_log(compiler, log_data,
                          "%s shader triggered register spilling.  "
                          "Try reducing the number of live scalar "
                          "values to improve performance.\n",
                          _mesa_shader_stage_to_string(stage));
   }

   /* This must come after all optimization and register allocation, since
    * it inserts dead code that happens to have side effects, and it does
    * so based on the actual physical registers in use.
    */
   insert_gfx4_send_dependency_workarounds();

   if (failed)
      return;

   opt_bank_conflicts();

   schedule_instructions_post_ra();

   if (last_scratch > 0) {
      ASSERTED unsigned max_scratch_size = 2 * 1024 * 1024;

      /* Take the max of any previously compiled variant of the shader. In the
       * case of bindless shaders with return parts, this will also take the
       * max of all parts.
       */
      prog_data->total_scratch = MAX2(elk_get_scratch_size(last_scratch),
                                      prog_data->total_scratch);

      if (gl_shader_stage_is_compute(stage)) {
         if (devinfo->platform == INTEL_PLATFORM_HSW) {
            /* According to the MEDIA_VFE_STATE's "Per Thread Scratch Space"
             * field documentation, Haswell supports a minimum of 2kB of
             * scratch space for compute shaders, unlike every other stage
             * and platform.
             */
            prog_data->total_scratch = MAX2(prog_data->total_scratch, 2048);
         } else if (devinfo->ver <= 7) {
            /* According to the MEDIA_VFE_STATE's "Per Thread Scratch Space"
             * field documentation, platforms prior to Haswell measure scratch
             * size linearly with a range of [1kB, 12kB] and 1kB granularity.
             */
            prog_data->total_scratch = ALIGN(last_scratch, 1024);
            max_scratch_size = 12 * 1024;
         }
      }

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
}

bool
elk_fs_visitor::run_vs()
{
   assert(stage == MESA_SHADER_VERTEX);

   payload_ = new elk_vs_thread_payload(*this);

   nir_to_elk(this);

   if (failed)
      return false;

   emit_urb_writes();

   calculate_cfg();

   optimize();

   assign_curb_setup();
   assign_vs_urb_setup();

   fixup_3src_null_dest();

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

void
elk_fs_visitor::set_tcs_invocation_id()
{
   struct elk_tcs_prog_data *tcs_prog_data = elk_tcs_prog_data(prog_data);
   struct elk_vue_prog_data *vue_prog_data = &tcs_prog_data->base;
   const fs_builder bld = fs_builder(this).at_end();

   const unsigned instance_id_mask = INTEL_MASK(23, 17);
   const unsigned instance_id_shift = 17;

   elk_fs_reg t = bld.vgrf(ELK_REGISTER_TYPE_UD);
   bld.AND(t, elk_fs_reg(retype(elk_vec1_grf(0, 2), ELK_REGISTER_TYPE_UD)),
           elk_imm_ud(instance_id_mask));

   invocation_id = bld.vgrf(ELK_REGISTER_TYPE_UD);

   if (vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_MULTI_PATCH) {
      /* gl_InvocationID is just the thread number */
      bld.SHR(invocation_id, t, elk_imm_ud(instance_id_shift));
      return;
   }

   assert(vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH);

   elk_fs_reg channels_uw = bld.vgrf(ELK_REGISTER_TYPE_UW);
   elk_fs_reg channels_ud = bld.vgrf(ELK_REGISTER_TYPE_UD);
   bld.MOV(channels_uw, elk_fs_reg(elk_imm_uv(0x76543210)));
   bld.MOV(channels_ud, channels_uw);

   if (tcs_prog_data->instances == 1) {
      invocation_id = channels_ud;
   } else {
      elk_fs_reg instance_times_8 = bld.vgrf(ELK_REGISTER_TYPE_UD);
      bld.SHR(instance_times_8, t, elk_imm_ud(instance_id_shift - 3));
      bld.ADD(invocation_id, instance_times_8, channels_ud);
   }
}

void
elk_fs_visitor::emit_tcs_thread_end()
{
   /* Try and tag the last URB write with EOT instead of emitting a whole
    * separate write just to finish the thread.  There isn't guaranteed to
    * be one, so this may not succeed.
    */
   if (devinfo->ver != 8 && mark_last_urb_write_with_eot())
      return;

   const fs_builder bld = fs_builder(this).at_end();

   /* Emit a URB write to end the thread.  On Broadwell, we use this to write
    * zero to the "TR DS Cache Disable" bit (we haven't implemented a fancy
    * algorithm to set it optimally).  On other platforms, we simply write
    * zero to a reserved/MBZ patch header DWord which has no consequence.
    */
   elk_fs_reg srcs[URB_LOGICAL_NUM_SRCS];
   srcs[URB_LOGICAL_SRC_HANDLE] = tcs_payload().patch_urb_output;
   srcs[URB_LOGICAL_SRC_CHANNEL_MASK] = elk_imm_ud(WRITEMASK_X << 16);
   srcs[URB_LOGICAL_SRC_DATA] = elk_imm_ud(0);
   srcs[URB_LOGICAL_SRC_COMPONENTS] = elk_imm_ud(1);
   elk_fs_inst *inst = bld.emit(ELK_SHADER_OPCODE_URB_WRITE_LOGICAL,
                            reg_undef, srcs, ARRAY_SIZE(srcs));
   inst->eot = true;
}

bool
elk_fs_visitor::run_tcs()
{
   assert(stage == MESA_SHADER_TESS_CTRL);

   struct elk_vue_prog_data *vue_prog_data = elk_vue_prog_data(prog_data);
   const fs_builder bld = fs_builder(this).at_end();

   assert(vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH ||
          vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_MULTI_PATCH);

   payload_ = new elk_tcs_thread_payload(*this);

   /* Initialize gl_InvocationID */
   set_tcs_invocation_id();

   const bool fix_dispatch_mask =
      vue_prog_data->dispatch_mode == INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH &&
      (nir->info.tess.tcs_vertices_out % 8) != 0;

   /* Fix the disptach mask */
   if (fix_dispatch_mask) {
      bld.CMP(bld.null_reg_ud(), invocation_id,
              elk_imm_ud(nir->info.tess.tcs_vertices_out), ELK_CONDITIONAL_L);
      bld.IF(ELK_PREDICATE_NORMAL);
   }

   nir_to_elk(this);

   if (fix_dispatch_mask) {
      bld.emit(ELK_OPCODE_ENDIF);
   }

   emit_tcs_thread_end();

   if (failed)
      return false;

   calculate_cfg();

   optimize();

   assign_curb_setup();
   assign_tcs_urb_setup();

   fixup_3src_null_dest();

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

bool
elk_fs_visitor::run_tes()
{
   assert(stage == MESA_SHADER_TESS_EVAL);

   payload_ = new elk_tes_thread_payload(*this);

   nir_to_elk(this);

   if (failed)
      return false;

   emit_urb_writes();

   calculate_cfg();

   optimize();

   assign_curb_setup();
   assign_tes_urb_setup();

   fixup_3src_null_dest();

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

bool
elk_fs_visitor::run_gs()
{
   assert(stage == MESA_SHADER_GEOMETRY);

   payload_ = new elk_gs_thread_payload(*this);

   this->final_gs_vertex_count = vgrf(glsl_uint_type());

   if (gs_compile->control_data_header_size_bits > 0) {
      /* Create a VGRF to store accumulated control data bits. */
      this->control_data_bits = vgrf(glsl_uint_type());

      /* If we're outputting more than 32 control data bits, then EmitVertex()
       * will set control_data_bits to 0 after emitting the first vertex.
       * Otherwise, we need to initialize it to 0 here.
       */
      if (gs_compile->control_data_header_size_bits <= 32) {
         const fs_builder bld = fs_builder(this).at_end();
         const fs_builder abld = bld.annotate("initialize control data bits");
         abld.MOV(this->control_data_bits, elk_imm_ud(0u));
      }
   }

   nir_to_elk(this);

   emit_gs_thread_end();

   if (failed)
      return false;

   calculate_cfg();

   optimize();

   assign_curb_setup();
   assign_gs_urb_setup();

   fixup_3src_null_dest();

   allocate_registers(true /* allow_spilling */);

   return !failed;
}

bool
elk_fs_visitor::run_fs(bool allow_spilling, bool do_rep_send)
{
   struct elk_wm_prog_data *wm_prog_data = elk_wm_prog_data(this->prog_data);
   elk_wm_prog_key *wm_key = (elk_wm_prog_key *) this->key;
   const fs_builder bld = fs_builder(this).at_end();

   assert(stage == MESA_SHADER_FRAGMENT);

   payload_ = new elk_fs_thread_payload(*this, source_depth_to_render_target,
                                    runtime_check_aads_emit);

   if (do_rep_send) {
      assert(dispatch_width == 16);
      emit_repclear_shader();
   } else {
      if (nir->info.inputs_read > 0 ||
          BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_FRAG_COORD) ||
          (nir->info.outputs_read > 0 && !wm_key->coherent_fb_fetch)) {
         if (devinfo->ver < 6)
            emit_interpolation_setup_gfx4();
         else
            emit_interpolation_setup_gfx6();
      }

      /* We handle discards by keeping track of the still-live pixels in f0.1.
       * Initialize it with the dispatched pixels.
       */
      if (wm_prog_data->uses_kill) {
         const unsigned lower_width = MIN2(dispatch_width, 16);
         for (unsigned i = 0; i < dispatch_width / lower_width; i++) {
            /* According to the "PS Thread Payload for Normal
             * Dispatch" pages on the BSpec, the dispatch mask is
             * stored in R1.7/R2.7 on gfx6+.
             */
            const elk_fs_reg dispatch_mask =
               devinfo->ver >= 6 ? elk_vec1_grf(i + 1, 7) :
               elk_vec1_grf(0, 0);
            bld.exec_all().group(1, 0)
               .MOV(elk_sample_mask_reg(bld.group(lower_width, i)),
                    retype(dispatch_mask, ELK_REGISTER_TYPE_UW));
         }
      }

      if (nir->info.writes_memory)
         wm_prog_data->has_side_effects = true;

      nir_to_elk(this);

      if (failed)
	 return false;

      if (wm_key->emit_alpha_test)
         emit_alpha_test();

      emit_fb_writes();

      calculate_cfg();

      optimize();

      assign_curb_setup();

      assign_urb_setup();

      fixup_3src_null_dest();

      allocate_registers(allow_spilling);
   }

   return !failed;
}

bool
elk_fs_visitor::run_cs(bool allow_spilling)
{
   assert(gl_shader_stage_is_compute(stage));
   assert(devinfo->ver >= 7);
   const fs_builder bld = fs_builder(this).at_end();

   payload_ = new elk_cs_thread_payload(*this);

   if (devinfo->platform == INTEL_PLATFORM_HSW && prog_data->total_shared > 0) {
      /* Move SLM index from g0.0[27:24] to sr0.1[11:8] */
      const fs_builder abld = bld.exec_all().group(1, 0);
      abld.MOV(retype(elk_sr0_reg(1), ELK_REGISTER_TYPE_UW),
               suboffset(retype(elk_vec1_grf(0, 0), ELK_REGISTER_TYPE_UW), 1));
   }

   nir_to_elk(this);

   if (failed)
      return false;

   emit_cs_terminate();

   calculate_cfg();

   optimize();

   assign_curb_setup();

   fixup_3src_null_dest();

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
 * (see enum elk_barycentric_mode) is needed by the fragment shader.
 *
 * We examine the load_barycentric intrinsics rather than looking at input
 * variables so that we catch interpolateAtCentroid() messages too, which
 * also need the ELK_BARYCENTRIC_[NON]PERSPECTIVE_CENTROID mode set up.
 */
static unsigned
elk_compute_barycentric_interp_modes(const struct intel_device_info *devinfo,
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
            enum elk_barycentric_mode bary =
               elk_barycentric_mode(intrin);

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
elk_compute_flat_inputs(struct elk_wm_prog_data *prog_data,
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
         return ELK_PSCDEPTH_ON;
      case FRAG_DEPTH_LAYOUT_GREATER:
         return ELK_PSCDEPTH_ON_GE;
      case FRAG_DEPTH_LAYOUT_LESS:
         return ELK_PSCDEPTH_ON_LE;
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
         return ELK_PSCDEPTH_ON_LE;
      }
   }
   return ELK_PSCDEPTH_OFF;
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
elk_nir_move_interpolation_to_top(nir_shader *nir)
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
elk_nir_populate_wm_prog_data(nir_shader *shader,
                              const struct intel_device_info *devinfo,
                              const struct elk_wm_prog_key *key,
                              struct elk_wm_prog_data *prog_data)
{
   /* key->alpha_test_func means simulating alpha testing via discards,
    * so the shader definitely kills pixels.
    */
   prog_data->uses_kill = shader->info.fs.uses_discard ||
                          shader->info.fs.uses_demote ||
                          key->emit_alpha_test;
   prog_data->uses_omask = !key->ignore_sample_mask_out &&
      (shader->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_SAMPLE_MASK));
   prog_data->color_outputs_written = key->color_outputs_valid;
   prog_data->computed_depth_mode = computed_depth_mode(shader);
   prog_data->computed_stencil =
      shader->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_STENCIL);

   prog_data->sample_shading =
      shader->info.fs.uses_sample_shading ||
      shader->info.outputs_read;

   assert(key->multisample_fbo != ELK_NEVER ||
          key->persample_interp == ELK_NEVER);

   prog_data->persample_dispatch = key->persample_interp;
   if (prog_data->sample_shading)
      prog_data->persample_dispatch = ELK_ALWAYS;

   /* We can only persample dispatch if we have a multisample FBO */
   prog_data->persample_dispatch = MIN2(prog_data->persample_dispatch,
                                        key->multisample_fbo);

   /* Currently only the Vulkan API allows alpha_to_coverage to be dynamic. If
    * persample_dispatch & multisample_fbo are not dynamic, Anv should be able
    * to definitively tell whether alpha_to_coverage is on or off.
    */
   prog_data->alpha_to_coverage = key->alpha_to_coverage;
   assert(prog_data->alpha_to_coverage != ELK_SOMETIMES ||
          prog_data->persample_dispatch == ELK_SOMETIMES);

   if (devinfo->ver >= 6) {
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
         prog_data->persample_dispatch != ELK_NEVER &&
         (BITSET_TEST(shader->info.system_values_read,
                      SYSTEM_VALUE_SAMPLE_POS) ||
          BITSET_TEST(shader->info.system_values_read,
                      SYSTEM_VALUE_SAMPLE_POS_OR_CENTER));
   }

   prog_data->early_fragment_tests = shader->info.fs.early_fragment_tests;
   prog_data->post_depth_coverage = shader->info.fs.post_depth_coverage;
   prog_data->inner_coverage = shader->info.fs.inner_coverage;

   prog_data->barycentric_interp_modes =
      elk_compute_barycentric_interp_modes(devinfo, shader);

   /* From the BDW PRM documentation for 3DSTATE_WM:
    *
    *    "MSDISPMODE_PERSAMPLE is required in order to select Perspective
    *     Sample or Non- perspective Sample barycentric coordinates."
    *
    * So cleanup any potentially set sample barycentric mode when not in per
    * sample dispatch.
    */
   if (prog_data->persample_dispatch == ELK_NEVER) {
      prog_data->barycentric_interp_modes &=
         ~BITFIELD_BIT(ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE);
   }

   prog_data->uses_nonperspective_interp_modes |=
      (prog_data->barycentric_interp_modes &
      ELK_BARYCENTRIC_NONPERSPECTIVE_BITS) != 0;

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
   intel_nir_pulls_at_sample(shader);

   /* We choose to always enable VMask prior to XeHP, as it would cause
    * us to lose out on the eliminate_find_live_channel() optimization.
    */
   prog_data->uses_vmask = true;

   prog_data->uses_src_w =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_FRAG_COORD);
   prog_data->uses_src_depth =
      BITSET_TEST(shader->info.system_values_read, SYSTEM_VALUE_FRAG_COORD);

   calculate_urb_setup(devinfo, key, prog_data, shader);
   elk_compute_flat_inputs(prog_data, shader);
}

/**
 * Pre-gfx6, the register file of the EUs was shared between threads,
 * and each thread used some subset allocated on a 16-register block
 * granularity.  The unit states wanted these block counts.
 */
static inline int
elk_register_blocks(int reg_count)
{
   return ALIGN(reg_count, 16) / 16 - 1;
}

const unsigned *
elk_compile_fs(const struct elk_compiler *compiler,
               struct elk_compile_fs_params *params)
{
   struct nir_shader *nir = params->base.nir;
   const struct elk_wm_prog_key *key = params->key;
   struct elk_wm_prog_data *prog_data = params->prog_data;
   bool allow_spilling = params->allow_spilling;
   const bool debug_enabled =
      elk_should_print_shader(nir, params->base.debug_flag ?
                                   params->base.debug_flag : DEBUG_WM);

   prog_data->base.stage = MESA_SHADER_FRAGMENT;
   prog_data->base.total_scratch = 0;

   const struct intel_device_info *devinfo = compiler->devinfo;
   const unsigned max_subgroup_size = compiler->devinfo->ver >= 6 ? 32 : 16;

   elk_nir_apply_key(nir, compiler, &key->base, max_subgroup_size);
   elk_nir_lower_fs_inputs(nir, devinfo, key);
   elk_nir_lower_fs_outputs(nir);

   if (devinfo->ver < 6)
      elk_setup_vue_interpolation(params->vue_map, nir, prog_data);

   /* From the SKL PRM, Volume 7, "Alpha Coverage":
    *  "If Pixel Shader outputs oMask, AlphaToCoverage is disabled in
    *   hardware, regardless of the state setting for this feature."
    */
   if (devinfo->ver > 6 && key->alpha_to_coverage != ELK_NEVER) {
      /* Run constant fold optimization in order to get the correct source
       * offset to determine render target 0 store instruction in
       * emit_alpha_to_coverage pass.
       */
      NIR_PASS(_, nir, nir_opt_constant_folding);
      NIR_PASS(_, nir, elk_nir_lower_alpha_to_coverage, key, prog_data);
   }

   NIR_PASS(_, nir, elk_nir_move_interpolation_to_top);
   elk_postprocess_nir(nir, compiler, debug_enabled,
                       key->base.robust_flags);

   elk_nir_populate_wm_prog_data(nir, compiler->devinfo, key, prog_data);

   std::unique_ptr<elk_fs_visitor> v8, v16, v32, vmulti;
   elk_cfg_t *simd8_cfg = NULL, *simd16_cfg = NULL, *simd32_cfg = NULL;
   float throughput = 0;
   bool has_spilled = false;

   v8 = std::make_unique<elk_fs_visitor>(compiler, &params->base, key,
                                     prog_data, nir, 8,
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

      prog_data->reg_blocks_8 = elk_register_blocks(v8->grf_used);
      const performance &perf = v8->performance_analysis.require();
      throughput = MAX2(throughput, perf.throughput);
      has_spilled = v8->spilled_any_registers;
      allow_spilling = false;
   }

   /* Limit dispatch width to simd8 with dual source blending on gfx8.
    * See: https://gitlab.freedesktop.org/mesa/mesa/-/issues/1917
    */
   if (devinfo->ver == 8 && prog_data->dual_src_blend &&
       INTEL_SIMD(FS, 8)) {
      assert(!params->use_rep_send);
      v8->limit_dispatch_width(8, "gfx8 workaround: "
                               "using SIMD8 when dual src blending.\n");
   }

   if (!has_spilled &&
       (!v8 || v8->max_dispatch_width >= 16) &&
       (INTEL_SIMD(FS, 16) || params->use_rep_send)) {
      /* Try a SIMD16 compile */
      v16 = std::make_unique<elk_fs_visitor>(compiler, &params->base, key,
                                         prog_data, nir, 16,
                                         params->base.stats != NULL,
                                         debug_enabled);
      if (v8)
         v16->import_uniforms(v8.get());
      if (!v16->run_fs(allow_spilling, params->use_rep_send)) {
         elk_shader_perf_log(compiler, params->base.log_data,
                             "SIMD16 shader failed to compile: %s\n",
                             v16->fail_msg);
      } else {
         simd16_cfg = v16->cfg;

         assert(v16->payload().num_regs % reg_unit(devinfo) == 0);
         prog_data->dispatch_grf_start_reg_16 = v16->payload().num_regs / reg_unit(devinfo);

         prog_data->reg_blocks_16 = elk_register_blocks(v16->grf_used);
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
       devinfo->ver >= 6 && !simd16_failed &&
       INTEL_SIMD(FS, 32)) {
      /* Try a SIMD32 compile */
      v32 = std::make_unique<elk_fs_visitor>(compiler, &params->base, key,
                                         prog_data, nir, 32,
                                         params->base.stats != NULL,
                                         debug_enabled);
      if (v8)
         v32->import_uniforms(v8.get());
      else if (v16)
         v32->import_uniforms(v16.get());

      if (!v32->run_fs(allow_spilling, false)) {
         elk_shader_perf_log(compiler, params->base.log_data,
                             "SIMD32 shader failed to compile: %s\n",
                             v32->fail_msg);
      } else {
         const performance &perf = v32->performance_analysis.require();

         if (!INTEL_DEBUG(DEBUG_DO32) && throughput >= perf.throughput) {
            elk_shader_perf_log(compiler, params->base.log_data,
                                "SIMD32 shader inefficient\n");
         } else {
            simd32_cfg = v32->cfg;

            assert(v32->payload().num_regs % reg_unit(devinfo) == 0);
            prog_data->dispatch_grf_start_reg_32 = v32->payload().num_regs / reg_unit(devinfo);

            prog_data->reg_blocks_32 = elk_register_blocks(v32->grf_used);
            throughput = MAX2(throughput, perf.throughput);
         }
      }
   }

   /* When the caller requests a repclear shader, they want SIMD16-only */
   if (params->use_rep_send)
      simd8_cfg = NULL;

   /* Prior to Iron Lake, the PS had a single shader offset with a jump table
    * at the top to select the shader.  We've never implemented that.
    * Instead, we just give them exactly one shader and we pick the widest one
    * available.
    */
   if (compiler->devinfo->ver < 5) {
      if (simd32_cfg || simd16_cfg)
         simd8_cfg = NULL;
      if (simd32_cfg)
         simd16_cfg = NULL;
   }

   /* If computed depth is enabled SNB only allows SIMD8. */
   if (compiler->devinfo->ver == 6 &&
       prog_data->computed_depth_mode != ELK_PSCDEPTH_OFF)
      assert(simd16_cfg == NULL && simd32_cfg == NULL);

   if (compiler->devinfo->ver <= 5 && !simd8_cfg) {
      /* Iron lake and earlier only have one Dispatch GRF start field.  Make
       * the data available in the base prog data struct for convenience.
       */
      if (simd16_cfg) {
         prog_data->base.dispatch_grf_start_reg =
            prog_data->dispatch_grf_start_reg_16;
      } else if (simd32_cfg) {
         prog_data->base.dispatch_grf_start_reg =
            prog_data->dispatch_grf_start_reg_32;
      }
   }

   elk_fs_generator g(compiler, &params->base, &prog_data->base,
                  v8 && v8->runtime_check_aads_emit, MESA_SHADER_FRAGMENT);

   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(params->base.mem_ctx,
                                     "%s fragment shader %s",
                                     nir->info.label ?
                                        nir->info.label : "unnamed",
                                     nir->info.name));
   }

   struct elk_compile_stats *stats = params->base.stats;
   uint32_t max_dispatch_width = 0;

   if (simd8_cfg) {
      prog_data->dispatch_8 = true;
      g.generate_code(simd8_cfg, 8, v8->shader_stats,
                      v8->performance_analysis.require(), stats);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 8;
   }

   if (simd16_cfg) {
      prog_data->dispatch_16 = true;
      prog_data->prog_offset_16 = g.generate_code(
         simd16_cfg, 16, v16->shader_stats,
         v16->performance_analysis.require(), stats);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 16;
   }

   if (simd32_cfg) {
      prog_data->dispatch_32 = true;
      prog_data->prog_offset_32 = g.generate_code(
         simd32_cfg, 32, v32->shader_stats,
         v32->performance_analysis.require(), stats);
      stats = stats ? stats + 1 : NULL;
      max_dispatch_width = 32;
   }

   for (struct elk_compile_stats *s = params->base.stats; s != NULL && s != stats; s++)
      s->max_dispatch_width = max_dispatch_width;

   g.add_const_data(nir->constant_data, nir->constant_data_size);
   return g.get_assembly();
}

unsigned
elk_cs_push_const_total_size(const struct elk_cs_prog_data *cs_prog_data,
                             unsigned threads)
{
   assert(cs_prog_data->push.per_thread.size % REG_SIZE == 0);
   assert(cs_prog_data->push.cross_thread.size % REG_SIZE == 0);
   return cs_prog_data->push.per_thread.size * threads +
          cs_prog_data->push.cross_thread.size;
}

static void
fill_push_const_block_info(struct elk_push_const_block *block, unsigned dwords)
{
   block->dwords = dwords;
   block->regs = DIV_ROUND_UP(dwords, 8);
   block->size = block->regs * 32;
}

static void
cs_fill_push_const_info(const struct intel_device_info *devinfo,
                        struct elk_cs_prog_data *cs_prog_data)
{
   const struct elk_stage_prog_data *prog_data = &cs_prog_data->base;
   int subgroup_id_index = elk_get_subgroup_id_param_index(devinfo, prog_data);
   bool cross_thread_supported = devinfo->verx10 >= 75;

   /* The thread ID should be stored in the last param dword */
   assert(subgroup_id_index == -1 ||
          subgroup_id_index == (int)prog_data->nr_params - 1);

   unsigned cross_thread_dwords, per_thread_dwords;
   if (!cross_thread_supported) {
      cross_thread_dwords = 0u;
      per_thread_dwords = prog_data->nr_params;
   } else if (subgroup_id_index >= 0) {
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
elk_nir_lower_simd(nir_shader *nir, unsigned dispatch_width)
{
   return nir_shader_lower_instructions(nir, filter_simd, lower_simd,
                                 (void *)(uintptr_t)dispatch_width);
}

const unsigned *
elk_compile_cs(const struct elk_compiler *compiler,
               struct elk_compile_cs_params *params)
{
   const nir_shader *nir = params->base.nir;
   const struct elk_cs_prog_key *key = params->key;
   struct elk_cs_prog_data *prog_data = params->prog_data;

   const bool debug_enabled =
      elk_should_print_shader(nir, params->base.debug_flag ?
                                   params->base.debug_flag : DEBUG_CS);

   prog_data->base.stage = MESA_SHADER_COMPUTE;
   prog_data->base.total_shared = nir->info.shared_size;
   prog_data->base.total_scratch = 0;

   if (!nir->info.workgroup_size_variable) {
      prog_data->local_size[0] = nir->info.workgroup_size[0];
      prog_data->local_size[1] = nir->info.workgroup_size[1];
      prog_data->local_size[2] = nir->info.workgroup_size[2];
   }

   elk_simd_selection_state simd_state{
      .devinfo = compiler->devinfo,
      .prog_data = prog_data,
      .required_width = elk_required_dispatch_width(&nir->info),
   };

   std::unique_ptr<elk_fs_visitor> v[3];

   for (unsigned simd = 0; simd < 3; simd++) {
      if (!elk_simd_should_compile(simd_state, simd))
         continue;

      const unsigned dispatch_width = 8u << simd;

      nir_shader *shader = nir_shader_clone(params->base.mem_ctx, nir);
      elk_nir_apply_key(shader, compiler, &key->base,
                        dispatch_width);

      NIR_PASS(_, shader, elk_nir_lower_simd, dispatch_width);

      /* Clean up after the local index and ID calculations. */
      NIR_PASS(_, shader, nir_opt_constant_folding);
      NIR_PASS(_, shader, nir_opt_dce);

      elk_postprocess_nir(shader, compiler, debug_enabled,
                          key->base.robust_flags);

      v[simd] = std::make_unique<elk_fs_visitor>(compiler, &params->base,
                                             &key->base,
                                             &prog_data->base,
                                             shader, dispatch_width,
                                             params->base.stats != NULL,
                                             debug_enabled);

      const int first = elk_simd_first_compiled(simd_state);
      if (first >= 0)
         v[simd]->import_uniforms(v[first].get());

      const bool allow_spilling = first < 0 || nir->info.workgroup_size_variable;

      if (v[simd]->run_cs(allow_spilling)) {
         cs_fill_push_const_info(compiler->devinfo, prog_data);

         elk_simd_mark_compiled(simd_state, simd, v[simd]->spilled_any_registers);
      } else {
         simd_state.error[simd] = ralloc_strdup(params->base.mem_ctx, v[simd]->fail_msg);
         if (simd > 0) {
            elk_shader_perf_log(compiler, params->base.log_data,
                                "SIMD%u shader failed to compile: %s\n",
                                dispatch_width, v[simd]->fail_msg);
         }
      }
   }

   const int selected_simd = elk_simd_select(simd_state);
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
   elk_fs_visitor *selected = v[selected_simd].get();

   if (!nir->info.workgroup_size_variable)
      prog_data->prog_mask = 1 << selected_simd;

   elk_fs_generator g(compiler, &params->base, &prog_data->base,
                  selected->runtime_check_aads_emit, MESA_SHADER_COMPUTE);
   if (unlikely(debug_enabled)) {
      char *name = ralloc_asprintf(params->base.mem_ctx,
                                   "%s compute shader %s",
                                   nir->info.label ?
                                   nir->info.label : "unnamed",
                                   nir->info.name);
      g.enable_debug(name);
   }

   uint32_t max_dispatch_width = 8u << (util_last_bit(prog_data->prog_mask) - 1);

   struct elk_compile_stats *stats = params->base.stats;
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
elk_cs_get_dispatch_info(const struct intel_device_info *devinfo,
                         const struct elk_cs_prog_data *prog_data,
                         const unsigned *override_local_size)
{
   struct intel_cs_dispatch_info info = {};

   const unsigned *sizes =
      override_local_size ? override_local_size :
                            prog_data->local_size;

   const int simd = elk_simd_select_for_workgroup_size(devinfo, prog_data, sizes);
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

uint64_t
elk_bsr(const struct intel_device_info *devinfo,
        uint32_t offset, uint8_t simd_size, uint8_t local_arg_offset)
{
   assert(offset % 64 == 0);
   assert(simd_size == 8 || simd_size == 16);
   assert(local_arg_offset % 8 == 0);

   return offset |
          SET_BITS(simd_size == 8, 4, 4) |
          SET_BITS(local_arg_offset / 8, 2, 0);
}

/**
 * Test the dispatch mask packing assumptions of
 * elk_stage_has_packed_dispatch().  Call this from e.g. the top of
 * elk_fs_visitor::emit_nir_code() to cause a GPU hang if any shader invocation is
 * executed with an unexpected dispatch mask.
 */
static UNUSED void
elk_fs_test_dispatch_packing(const fs_builder &bld)
{
   const elk_fs_visitor *shader = static_cast<const elk_fs_visitor *>(bld.shader);
   const gl_shader_stage stage = shader->stage;
   const bool uses_vmask =
      stage == MESA_SHADER_FRAGMENT &&
      elk_wm_prog_data(shader->stage_prog_data)->uses_vmask;

   if (elk_stage_has_packed_dispatch(shader->devinfo, stage,
                                     shader->stage_prog_data)) {
      const fs_builder ubld = bld.exec_all().group(1, 0);
      const elk_fs_reg tmp = component(bld.vgrf(ELK_REGISTER_TYPE_UD), 0);
      const elk_fs_reg mask = uses_vmask ? elk_vmask_reg() : elk_dmask_reg();

      ubld.ADD(tmp, mask, elk_imm_ud(1));
      ubld.AND(tmp, mask, tmp);

      /* This will loop forever if the dispatch mask doesn't have the expected
       * form '2^n-1', in which case tmp will be non-zero.
       */
      bld.emit(ELK_OPCODE_DO);
      bld.CMP(bld.null_reg_ud(), tmp, elk_imm_ud(0), ELK_CONDITIONAL_NZ);
      set_predicate(ELK_PREDICATE_NORMAL, bld.emit(ELK_OPCODE_WHILE));
   }
}

unsigned
elk_fs_visitor::workgroup_size() const
{
   assert(gl_shader_stage_uses_workgroup(stage));
   const struct elk_cs_prog_data *cs = elk_cs_prog_data(prog_data);
   return cs->local_size[0] * cs->local_size[1] * cs->local_size[2];
}

bool elk_should_print_shader(const nir_shader *shader, uint64_t debug_flag)
{
   return INTEL_DEBUG(debug_flag) && (!shader->info.internal || NIR_DEBUG(PRINT_INTERNAL));
}

namespace elk {
   elk_fs_reg
   fetch_payload_reg(const elk::fs_builder &bld, uint8_t regs[2],
                     elk_reg_type type, unsigned n)
   {
      if (!regs[0])
         return elk_fs_reg();

      if (bld.dispatch_width() > 16) {
         const elk_fs_reg tmp = bld.vgrf(type, n);
         const elk::fs_builder hbld = bld.exec_all().group(16, 0);
         const unsigned m = bld.dispatch_width() / hbld.dispatch_width();
         elk_fs_reg *const components = new elk_fs_reg[m * n];

         for (unsigned c = 0; c < n; c++) {
            for (unsigned g = 0; g < m; g++)
               components[c * m + g] =
                  offset(retype(elk_vec8_grf(regs[g], 0), type), hbld, c);
         }

         hbld.LOAD_PAYLOAD(tmp, components, m * n, 0);

         delete[] components;
         return tmp;

      } else {
         return elk_fs_reg(retype(elk_vec8_grf(regs[0], 0), type));
      }
   }

   elk_fs_reg
   fetch_barycentric_reg(const elk::fs_builder &bld, uint8_t regs[2])
   {
      if (!regs[0])
         return elk_fs_reg();

      const elk_fs_reg tmp = bld.vgrf(ELK_REGISTER_TYPE_F, 2);
      const elk::fs_builder hbld = bld.exec_all().group(8, 0);
      const unsigned m = bld.dispatch_width() / hbld.dispatch_width();
      elk_fs_reg *const components = new elk_fs_reg[2 * m];

      for (unsigned c = 0; c < 2; c++) {
         for (unsigned g = 0; g < m; g++)
            components[c * m + g] = offset(elk_vec8_grf(regs[g / 2], 0),
                                           hbld, c + 2 * (g % 2));
      }

      hbld.LOAD_PAYLOAD(tmp, components, 2 * m, 0);

      delete[] components;
      return tmp;
   }

   void
   check_dynamic_msaa_flag(const fs_builder &bld,
                           const struct elk_wm_prog_data *wm_prog_data,
                           enum intel_msaa_flags flag)
   {
      elk_fs_inst *inst = bld.AND(bld.null_reg_ud(),
                              dynamic_msaa_flags(wm_prog_data),
                              elk_imm_ud(flag));
      inst->conditional_mod = ELK_CONDITIONAL_NZ;
   }
}
