/*
 * Copyright © 2008 Keith Packard
 * Copyright © 2014 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk_disasm.h"
#include "elk_disasm_info.h"
#include "elk_eu_defines.h"
#include "elk_eu.h"
#include "elk_inst.h"
#include "elk_isa_info.h"
#include "elk_reg.h"
#include "elk_shader.h"
#include "util/half_float.h"

bool
elk_has_jip(const struct intel_device_info *devinfo, enum elk_opcode opcode)
{
   if (devinfo->ver < 6)
      return false;

   return opcode == ELK_OPCODE_IF ||
          opcode == ELK_OPCODE_ELSE ||
          opcode == ELK_OPCODE_ENDIF ||
          opcode == ELK_OPCODE_WHILE ||
          opcode == ELK_OPCODE_BREAK ||
          opcode == ELK_OPCODE_CONTINUE ||
          opcode == ELK_OPCODE_HALT;
}

bool
elk_has_uip(const struct intel_device_info *devinfo, enum elk_opcode opcode)
{
   if (devinfo->ver < 6)
      return false;

   return (devinfo->ver >= 7 && opcode == ELK_OPCODE_IF) ||
          (devinfo->ver >= 8 && opcode == ELK_OPCODE_ELSE) ||
          opcode == ELK_OPCODE_BREAK ||
          opcode == ELK_OPCODE_CONTINUE ||
          opcode == ELK_OPCODE_HALT;
}

static bool
has_branch_ctrl(const struct intel_device_info *devinfo, enum elk_opcode opcode)
{
   if (devinfo->ver < 8)
      return false;

   return opcode == ELK_OPCODE_IF ||
          opcode == ELK_OPCODE_ELSE;
          /* opcode == ELK_OPCODE_GOTO; */
}

static bool
is_logic_instruction(unsigned opcode)
{
   return opcode == ELK_OPCODE_AND ||
          opcode == ELK_OPCODE_NOT ||
          opcode == ELK_OPCODE_OR ||
          opcode == ELK_OPCODE_XOR;
}

static bool
is_send(unsigned opcode)
{
   return opcode == ELK_OPCODE_SEND ||
          opcode == ELK_OPCODE_SENDC;
}

const char *const elk_conditional_modifier[16] = {
   [ELK_CONDITIONAL_NONE] = "",
   [ELK_CONDITIONAL_Z]    = ".z",
   [ELK_CONDITIONAL_NZ]   = ".nz",
   [ELK_CONDITIONAL_G]    = ".g",
   [ELK_CONDITIONAL_GE]   = ".ge",
   [ELK_CONDITIONAL_L]    = ".l",
   [ELK_CONDITIONAL_LE]   = ".le",
   [ELK_CONDITIONAL_R]    = ".r",
   [ELK_CONDITIONAL_O]    = ".o",
   [ELK_CONDITIONAL_U]    = ".u",
};

static const char *const m_negate[2] = {
   [0] = "",
   [1] = "-",
};

static const char *const _abs[2] = {
   [0] = "",
   [1] = "(abs)",
};

static const char *const m_bitnot[2] = { "", "~" };

static const char *const vert_stride[16] = {
   [0] = "0",
   [1] = "1",
   [2] = "2",
   [3] = "4",
   [4] = "8",
   [5] = "16",
   [6] = "32",
   [15] = "VxH",
};

static const char *const width[8] = {
   [0] = "1",
   [1] = "2",
   [2] = "4",
   [3] = "8",
   [4] = "16",
};

static const char *const horiz_stride[4] = {
   [0] = "0",
   [1] = "1",
   [2] = "2",
   [3] = "4"
};

static const char *const chan_sel[4] = {
   [0] = "x",
   [1] = "y",
   [2] = "z",
   [3] = "w",
};

static const char *const debug_ctrl[2] = {
   [0] = "",
   [1] = ".breakpoint"
};

static const char *const saturate[2] = {
   [0] = "",
   [1] = ".sat"
};

static const char *const cmpt_ctrl[2] = {
   [0] = "",
   [1] = "compacted"
};

static const char *const accwr[2] = {
   [0] = "",
   [1] = "AccWrEnable"
};

static const char *const branch_ctrl[2] = {
   [0] = "",
   [1] = "BranchCtrl"
};

static const char *const wectrl[2] = {
   [0] = "",
   [1] = "WE_all"
};

static const char *const exec_size[8] = {
   [0] = "1",
   [1] = "2",
   [2] = "4",
   [3] = "8",
   [4] = "16",
   [5] = "32"
};

static const char *const pred_inv[2] = {
   [0] = "+",
   [1] = "-"
};

const char *const elk_pred_ctrl_align16[16] = {
   [1] = "",
   [2] = ".x",
   [3] = ".y",
   [4] = ".z",
   [5] = ".w",
   [6] = ".any4h",
   [7] = ".all4h",
};

static const char *const pred_ctrl_align1[16] = {
   [ELK_PREDICATE_NORMAL]        = "",
   [ELK_PREDICATE_ALIGN1_ANYV]   = ".anyv",
   [ELK_PREDICATE_ALIGN1_ALLV]   = ".allv",
   [ELK_PREDICATE_ALIGN1_ANY2H]  = ".any2h",
   [ELK_PREDICATE_ALIGN1_ALL2H]  = ".all2h",
   [ELK_PREDICATE_ALIGN1_ANY4H]  = ".any4h",
   [ELK_PREDICATE_ALIGN1_ALL4H]  = ".all4h",
   [ELK_PREDICATE_ALIGN1_ANY8H]  = ".any8h",
   [ELK_PREDICATE_ALIGN1_ALL8H]  = ".all8h",
   [ELK_PREDICATE_ALIGN1_ANY16H] = ".any16h",
   [ELK_PREDICATE_ALIGN1_ALL16H] = ".all16h",
   [ELK_PREDICATE_ALIGN1_ANY32H] = ".any32h",
   [ELK_PREDICATE_ALIGN1_ALL32H] = ".all32h",
};

static const char *const thread_ctrl[4] = {
   [ELK_THREAD_NORMAL] = "",
   [ELK_THREAD_ATOMIC] = "atomic",
   [ELK_THREAD_SWITCH] = "switch",
};

static const char *const compr_ctrl[4] = {
   [0] = "",
   [1] = "sechalf",
   [2] = "compr",
   [3] = "compr4",
};

static const char *const dep_ctrl[4] = {
   [0] = "",
   [1] = "NoDDClr",
   [2] = "NoDDChk",
   [3] = "NoDDClr,NoDDChk",
};

static const char *const mask_ctrl[4] = {
   [0] = "",
   [1] = "nomask",
};

static const char *const access_mode[2] = {
   [0] = "align1",
   [1] = "align16",
};

static const char *const reg_file[4] = {
   [0] = "A",
   [1] = "g",
   [2] = "m",
   [3] = "imm",
};

static const char *const writemask[16] = {
   [0x0] = ".",
   [0x1] = ".x",
   [0x2] = ".y",
   [0x3] = ".xy",
   [0x4] = ".z",
   [0x5] = ".xz",
   [0x6] = ".yz",
   [0x7] = ".xyz",
   [0x8] = ".w",
   [0x9] = ".xw",
   [0xa] = ".yw",
   [0xb] = ".xyw",
   [0xc] = ".zw",
   [0xd] = ".xzw",
   [0xe] = ".yzw",
   [0xf] = "",
};

static const char *const end_of_thread[2] = {
   [0] = "",
   [1] = "EOT"
};

/* SFIDs on Gfx4-5 */
static const char *const gfx4_sfid[16] = {
   [ELK_SFID_NULL]            = "null",
   [ELK_SFID_MATH]            = "math",
   [ELK_SFID_SAMPLER]         = "sampler",
   [ELK_SFID_MESSAGE_GATEWAY] = "gateway",
   [ELK_SFID_DATAPORT_READ]   = "read",
   [ELK_SFID_DATAPORT_WRITE]  = "write",
   [ELK_SFID_URB]             = "urb",
   [ELK_SFID_THREAD_SPAWNER]  = "thread_spawner",
   [ELK_SFID_VME]             = "vme",
};

static const char *const gfx6_sfid[16] = {
   [ELK_SFID_NULL]                     = "null",
   [ELK_SFID_MATH]                     = "math",
   [ELK_SFID_SAMPLER]                  = "sampler",
   [ELK_SFID_MESSAGE_GATEWAY]          = "gateway",
   [ELK_SFID_URB]                      = "urb",
   [ELK_SFID_THREAD_SPAWNER]           = "thread_spawner",
   [GFX6_SFID_DATAPORT_SAMPLER_CACHE]  = "dp_sampler",
   [GFX6_SFID_DATAPORT_RENDER_CACHE]   = "render",
   [GFX6_SFID_DATAPORT_CONSTANT_CACHE] = "const",
   [GFX7_SFID_DATAPORT_DATA_CACHE]     = "data",
   [GFX7_SFID_PIXEL_INTERPOLATOR]      = "pixel interp",
   [HSW_SFID_DATAPORT_DATA_CACHE_1]    = "dp data 1",
   [HSW_SFID_CRE]                      = "cre",
};

static const char *const gfx7_gateway_subfuncid[8] = {
   [ELK_MESSAGE_GATEWAY_SFID_OPEN_GATEWAY] = "open",
   [ELK_MESSAGE_GATEWAY_SFID_CLOSE_GATEWAY] = "close",
   [ELK_MESSAGE_GATEWAY_SFID_FORWARD_MSG] = "forward msg",
   [ELK_MESSAGE_GATEWAY_SFID_GET_TIMESTAMP] = "get timestamp",
   [ELK_MESSAGE_GATEWAY_SFID_BARRIER_MSG] = "barrier msg",
   [ELK_MESSAGE_GATEWAY_SFID_UPDATE_GATEWAY_STATE] = "update state",
   [ELK_MESSAGE_GATEWAY_SFID_MMIO_READ_WRITE] = "mmio read/write",
};

static const char *const gfx4_dp_read_port_msg_type[4] = {
   [0b00] = "OWord Block Read",
   [0b01] = "OWord Dual Block Read",
   [0b10] = "Media Block Read",
   [0b11] = "DWord Scattered Read",
};

static const char *const g45_dp_read_port_msg_type[8] = {
   [0b000] = "OWord Block Read",
   [0b010] = "OWord Dual Block Read",
   [0b100] = "Media Block Read",
   [0b110] = "DWord Scattered Read",
   [0b001] = "Render Target UNORM Read",
   [0b011] = "AVC Loop Filter Read",
};

static const char *const dp_write_port_msg_type[8] = {
   [0b000] = "OWord block write",
   [0b001] = "OWord dual block write",
   [0b010] = "media block write",
   [0b011] = "DWord scattered write",
   [0b100] = "RT write",
   [0b101] = "streamed VB write",
   [0b110] = "RT UNORM write", /* G45+ */
   [0b111] = "flush render cache",
};

static const char *const dp_rc_msg_type_gfx6[16] = {
   [ELK_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ] = "OWORD block read",
   [GFX6_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ] = "RT UNORM read",
   [GFX6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ] = "OWORD dual block read",
   [GFX6_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ] = "media block read",
   [GFX6_DATAPORT_READ_MESSAGE_OWORD_UNALIGN_BLOCK_READ] =
      "OWORD unaligned block read",
   [GFX6_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ] = "DWORD scattered read",
   [GFX6_DATAPORT_WRITE_MESSAGE_DWORD_ATOMIC_WRITE] = "DWORD atomic write",
   [GFX6_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE] = "OWORD block write",
   [GFX6_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE] =
      "OWORD dual block write",
   [GFX6_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE] = "media block write",
   [GFX6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE] =
      "DWORD scattered write",
   [GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE] = "RT write",
   [GFX6_DATAPORT_WRITE_MESSAGE_STREAMED_VB_WRITE] = "streamed VB write",
   [GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_UNORM_WRITE] = "RT UNORM write",
};

static const char *const dp_rc_msg_type_gfx7[16] = {
   [GFX7_DATAPORT_RC_MEDIA_BLOCK_READ] = "media block read",
   [GFX7_DATAPORT_RC_TYPED_SURFACE_READ] = "typed surface read",
   [GFX7_DATAPORT_RC_TYPED_ATOMIC_OP] = "typed atomic op",
   [GFX7_DATAPORT_RC_MEMORY_FENCE] = "memory fence",
   [GFX7_DATAPORT_RC_MEDIA_BLOCK_WRITE] = "media block write",
   [GFX7_DATAPORT_RC_RENDER_TARGET_WRITE] = "RT write",
   [GFX7_DATAPORT_RC_TYPED_SURFACE_WRITE] = "typed surface write"
};

static const char *const *
dp_rc_msg_type(const struct intel_device_info *devinfo)
{
   return (devinfo->ver >= 7 ? dp_rc_msg_type_gfx7 :
           devinfo->ver >= 6 ? dp_rc_msg_type_gfx6 :
           dp_write_port_msg_type);
}

static const char *const m_rt_write_subtype[] = {
   [0b000] = "SIMD16",
   [0b001] = "SIMD16/RepData",
   [0b010] = "SIMD8/DualSrcLow",
   [0b011] = "SIMD8/DualSrcHigh",
   [0b100] = "SIMD8",
   [0b101] = "SIMD8/ImageWrite",   /* Gfx6+ */
   [0b111] = "SIMD16/RepData-111", /* no idea how this is different than 1 */
};

static const char *const dp_dc0_msg_type_gfx7[16] = {
   [GFX7_DATAPORT_DC_OWORD_BLOCK_READ] = "DC OWORD block read",
   [GFX7_DATAPORT_DC_UNALIGNED_OWORD_BLOCK_READ] =
      "DC unaligned OWORD block read",
   [GFX7_DATAPORT_DC_OWORD_DUAL_BLOCK_READ] = "DC OWORD dual block read",
   [GFX7_DATAPORT_DC_DWORD_SCATTERED_READ] = "DC DWORD scattered read",
   [GFX7_DATAPORT_DC_BYTE_SCATTERED_READ] = "DC byte scattered read",
   [GFX7_DATAPORT_DC_UNTYPED_SURFACE_READ] = "DC untyped surface read",
   [GFX7_DATAPORT_DC_UNTYPED_ATOMIC_OP] = "DC untyped atomic",
   [GFX7_DATAPORT_DC_MEMORY_FENCE] = "DC mfence",
   [GFX7_DATAPORT_DC_OWORD_BLOCK_WRITE] = "DC OWORD block write",
   [GFX7_DATAPORT_DC_OWORD_DUAL_BLOCK_WRITE] = "DC OWORD dual block write",
   [GFX7_DATAPORT_DC_DWORD_SCATTERED_WRITE] = "DC DWORD scatterd write",
   [GFX7_DATAPORT_DC_BYTE_SCATTERED_WRITE] = "DC byte scattered write",
   [GFX7_DATAPORT_DC_UNTYPED_SURFACE_WRITE] = "DC untyped surface write",
};

static const char *const dp_oword_block_rw[8] = {
      [ELK_DATAPORT_OWORD_BLOCK_1_OWORDLOW]  = "1-low",
      [ELK_DATAPORT_OWORD_BLOCK_1_OWORDHIGH] = "1-high",
      [ELK_DATAPORT_OWORD_BLOCK_2_OWORDS]    = "2",
      [ELK_DATAPORT_OWORD_BLOCK_4_OWORDS]    = "4",
      [ELK_DATAPORT_OWORD_BLOCK_8_OWORDS]    = "8",
};

static const char *const dp_dc1_msg_type_hsw[32] = {
   [HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ] = "untyped surface read",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP] = "DC untyped atomic op",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2] =
      "DC untyped 4x2 atomic op",
   [HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_READ] = "DC media block read",
   [HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ] = "DC typed surface read",
   [HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP] = "DC typed atomic",
   [HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2] = "DC typed 4x2 atomic op",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE] = "DC untyped surface write",
   [HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_WRITE] = "DC media block write",
   [HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP] = "DC atomic counter op",
   [HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2] =
      "DC 4x2 atomic counter op",
   [HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE] = "DC typed surface write",
   [GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_READ] = "DC A64 untyped surface read",
   [GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_OP] = "DC A64 untyped atomic op",
   [GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_READ] = "DC A64 oword block read",
   [GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_WRITE] = "DC A64 oword block write",
   [GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_WRITE] = "DC A64 untyped surface write",
   [GFX8_DATAPORT_DC_PORT1_A64_SCATTERED_WRITE] = "DC A64 scattered write",
};

static const char *const aop[16] = {
   [ELK_AOP_AND]    = "and",
   [ELK_AOP_OR]     = "or",
   [ELK_AOP_XOR]    = "xor",
   [ELK_AOP_MOV]    = "mov",
   [ELK_AOP_INC]    = "inc",
   [ELK_AOP_DEC]    = "dec",
   [ELK_AOP_ADD]    = "add",
   [ELK_AOP_SUB]    = "sub",
   [ELK_AOP_REVSUB] = "revsub",
   [ELK_AOP_IMAX]   = "imax",
   [ELK_AOP_IMIN]   = "imin",
   [ELK_AOP_UMAX]   = "umax",
   [ELK_AOP_UMIN]   = "umin",
   [ELK_AOP_CMPWR]  = "cmpwr",
   [ELK_AOP_PREDEC] = "predec",
};

static const char * const pixel_interpolator_msg_types[4] = {
    [GFX7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET] = "per_message_offset",
    [GFX7_PIXEL_INTERPOLATOR_LOC_SAMPLE] = "sample_position",
    [GFX7_PIXEL_INTERPOLATOR_LOC_CENTROID] = "centroid",
    [GFX7_PIXEL_INTERPOLATOR_LOC_PER_SLOT_OFFSET] = "per_slot_offset",
};

static const char *const math_function[16] = {
   [ELK_MATH_FUNCTION_INV]    = "inv",
   [ELK_MATH_FUNCTION_LOG]    = "log",
   [ELK_MATH_FUNCTION_EXP]    = "exp",
   [ELK_MATH_FUNCTION_SQRT]   = "sqrt",
   [ELK_MATH_FUNCTION_RSQ]    = "rsq",
   [ELK_MATH_FUNCTION_SIN]    = "sin",
   [ELK_MATH_FUNCTION_COS]    = "cos",
   [ELK_MATH_FUNCTION_SINCOS] = "sincos",
   [ELK_MATH_FUNCTION_FDIV]   = "fdiv",
   [ELK_MATH_FUNCTION_POW]    = "pow",
   [ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER] = "intdivmod",
   [ELK_MATH_FUNCTION_INT_DIV_QUOTIENT]  = "intdiv",
   [ELK_MATH_FUNCTION_INT_DIV_REMAINDER] = "intmod",
   [GFX8_MATH_FUNCTION_INVM]  = "invm",
   [GFX8_MATH_FUNCTION_RSQRTM] = "rsqrtm",
};

static const char *const math_saturate[2] = {
   [0] = "",
   [1] = "sat"
};

static const char *const math_signed[2] = {
   [0] = "",
   [1] = "signed"
};

static const char *const math_scalar[2] = {
   [0] = "",
   [1] = "scalar"
};

static const char *const math_precision[2] = {
   [0] = "",
   [1] = "partial_precision"
};

static const char *const gfx5_urb_opcode[] = {
   [0] = "urb_write",
   [1] = "ff_sync",
};

static const char *const gfx7_urb_opcode[] = {
   [ELK_URB_OPCODE_WRITE_HWORD] = "write HWord",
   [ELK_URB_OPCODE_WRITE_OWORD] = "write OWord",
   [ELK_URB_OPCODE_READ_HWORD] = "read HWord",
   [ELK_URB_OPCODE_READ_OWORD] = "read OWord",
   [GFX7_URB_OPCODE_ATOMIC_MOV] = "atomic mov",  /* Gfx7+ */
   [GFX7_URB_OPCODE_ATOMIC_INC] = "atomic inc",  /* Gfx7+ */
   [GFX8_URB_OPCODE_ATOMIC_ADD] = "atomic add",  /* Gfx8+ */
   [GFX8_URB_OPCODE_SIMD8_WRITE] = "SIMD8 write", /* Gfx8+ */
   [GFX8_URB_OPCODE_SIMD8_READ] = "SIMD8 read",  /* Gfx8+ */
   /* [9-15] - reserved */
};

static const char *const urb_swizzle[4] = {
   [ELK_URB_SWIZZLE_NONE]       = "",
   [ELK_URB_SWIZZLE_INTERLEAVE] = "interleave",
   [ELK_URB_SWIZZLE_TRANSPOSE]  = "transpose",
};

static const char *const urb_allocate[2] = {
   [0] = "",
   [1] = "allocate"
};

static const char *const urb_used[2] = {
   [0] = "",
   [1] = "used"
};

static const char *const urb_complete[2] = {
   [0] = "",
   [1] = "complete"
};

static const char *const gfx5_sampler_msg_type[] = {
   [GFX5_SAMPLER_MESSAGE_SAMPLE]              = "sample",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS]         = "sample_b",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_LOD]          = "sample_l",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_COMPARE]      = "sample_c",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_DERIVS]       = "sample_d",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE] = "sample_b_c",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE]  = "sample_l_c",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_LD]           = "ld",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4]      = "gather4",
   [GFX5_SAMPLER_MESSAGE_LOD]                 = "lod",
   [GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO]      = "resinfo",
   [GFX6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO]   = "sampleinfo",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C]    = "gather4_c",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO]   = "gather4_po",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C] = "gather4_po_c",
   [HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE] = "sample_d_c",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_LD_MCS]       = "ld_mcs",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_LD2DMS]       = "ld2dms",
   [GFX7_SAMPLER_MESSAGE_SAMPLE_LD2DSS]       = "ld2dss",
};

static const char *const gfx5_sampler_simd_mode[7] = {
   [ELK_SAMPLER_SIMD_MODE_SIMD4X2]   = "SIMD4x2",
   [ELK_SAMPLER_SIMD_MODE_SIMD8]     = "SIMD8",
   [ELK_SAMPLER_SIMD_MODE_SIMD16]    = "SIMD16",
   [ELK_SAMPLER_SIMD_MODE_SIMD32_64] = "SIMD32/64",
};

static const char *const sampler_target_format[4] = {
   [0] = "F",
   [2] = "UD",
   [3] = "D"
};

static int column;

static int
string(FILE *file, const char *string)
{
   fputs(string, file);
   column += strlen(string);
   return 0;
}

static int
format(FILE *f, const char *format, ...) PRINTFLIKE(2, 3);

static int
format(FILE *f, const char *format, ...)
{
   char buf[1024];
   va_list args;
   va_start(args, format);

   vsnprintf(buf, sizeof(buf) - 1, format, args);
   va_end(args);
   string(f, buf);
   return 0;
}

static int
newline(FILE *f)
{
   putc('\n', f);
   column = 0;
   return 0;
}

static int
pad(FILE *f, int c)
{
   do
      string(f, " ");
   while (column < c);
   return 0;
}

static int
control(FILE *file, const char *name, const char *const ctrl[],
        unsigned id, int *space)
{
   if (!ctrl[id]) {
      fprintf(file, "*** invalid %s value %d ", name, id);
      return 1;
   }
   if (ctrl[id][0]) {
      if (space && *space)
         string(file, " ");
      string(file, ctrl[id]);
      if (space)
         *space = 1;
   }
   return 0;
}

static int
print_opcode(FILE *file, const struct elk_isa_info *isa,
             enum elk_opcode id)
{
   const struct elk_opcode_desc *desc = elk_opcode_desc(isa, id);
   if (!desc) {
      format(file, "*** invalid opcode value %d ", id);
      return 1;
   }
   string(file, desc->name);
   return 0;
}

static int
reg(FILE *file, unsigned _reg_file, unsigned _reg_nr)
{
   int err = 0;

   /* Clear the Compr4 instruction compression bit. */
   if (_reg_file == ELK_MESSAGE_REGISTER_FILE)
      _reg_nr &= ~ELK_MRF_COMPR4;

   if (_reg_file == ELK_ARCHITECTURE_REGISTER_FILE) {
      switch (_reg_nr & 0xf0) {
      case ELK_ARF_NULL:
         string(file, "null");
         break;
      case ELK_ARF_ADDRESS:
         format(file, "a%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_ACCUMULATOR:
         format(file, "acc%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_FLAG:
         format(file, "f%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_MASK:
         format(file, "mask%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_MASK_STACK:
         format(file, "ms%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_MASK_STACK_DEPTH:
         format(file, "msd%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_STATE:
         format(file, "sr%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_CONTROL:
         format(file, "cr%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_NOTIFICATION_COUNT:
         format(file, "n%d", _reg_nr & 0x0f);
         break;
      case ELK_ARF_IP:
         string(file, "ip");
         return -1;
         break;
      case ELK_ARF_TDR:
         format(file, "tdr0");
         return -1;
      case ELK_ARF_TIMESTAMP:
         format(file, "tm%d", _reg_nr & 0x0f);
         break;
      default:
         format(file, "ARF%d", _reg_nr);
         break;
      }
   } else {
      err |= control(file, "src reg file", reg_file, _reg_file, NULL);
      format(file, "%d", _reg_nr);
   }
   return err;
}

static int
dest(FILE *file, const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   enum elk_reg_type type = elk_inst_dst_type(devinfo, inst);
   unsigned elem_size = elk_reg_type_to_size(type);
   int err = 0;

   if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
      if (elk_inst_dst_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         err |= reg(file, elk_inst_dst_reg_file(devinfo, inst),
                    elk_inst_dst_da_reg_nr(devinfo, inst));
         if (err == -1)
            return 0;
         if (elk_inst_dst_da1_subreg_nr(devinfo, inst))
            format(file, ".%"PRIu64, elk_inst_dst_da1_subreg_nr(devinfo, inst) /
                   elem_size);
         string(file, "<");
         err |= control(file, "horiz stride", horiz_stride,
                        elk_inst_dst_hstride(devinfo, inst), NULL);
         string(file, ">");
         string(file, elk_reg_type_to_letters(type));
      } else {
         string(file, "g[a0");
         if (elk_inst_dst_ia_subreg_nr(devinfo, inst))
            format(file, ".%"PRIu64, elk_inst_dst_ia_subreg_nr(devinfo, inst) /
                   elem_size);
         if (elk_inst_dst_ia1_addr_imm(devinfo, inst))
            format(file, " %d", elk_inst_dst_ia1_addr_imm(devinfo, inst));
         string(file, "]<");
         err |= control(file, "horiz stride", horiz_stride,
                        elk_inst_dst_hstride(devinfo, inst), NULL);
         string(file, ">");
         string(file, elk_reg_type_to_letters(type));
      }
   } else {
      if (elk_inst_dst_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         err |= reg(file, elk_inst_dst_reg_file(devinfo, inst),
                    elk_inst_dst_da_reg_nr(devinfo, inst));
         if (err == -1)
            return 0;
         if (elk_inst_dst_da16_subreg_nr(devinfo, inst))
            format(file, ".%u", 16 / elem_size);
         string(file, "<1>");
         err |= control(file, "writemask", writemask,
                        elk_inst_da16_writemask(devinfo, inst), NULL);
         string(file, elk_reg_type_to_letters(type));
      } else {
         err = 1;
         string(file, "Indirect align16 address mode not supported");
      }
   }

   return 0;
}

static int
dest_3src(FILE *file, const struct intel_device_info *devinfo,
          const elk_inst *inst)
{
   bool is_align1 = elk_inst_3src_access_mode(devinfo, inst) == ELK_ALIGN_1;
   int err = 0;
   uint32_t reg_file;
   unsigned subreg_nr;
   enum elk_reg_type type;

   if (is_align1)
      return 0;

   if (devinfo->ver == 6 && elk_inst_3src_a16_dst_reg_file(devinfo, inst))
      reg_file = ELK_MESSAGE_REGISTER_FILE;
   else
      reg_file = ELK_GENERAL_REGISTER_FILE;

   err |= reg(file, reg_file, elk_inst_3src_dst_reg_nr(devinfo, inst));
   if (err == -1)
      return 0;

   type = elk_inst_3src_a16_dst_type(devinfo, inst);
   subreg_nr = elk_inst_3src_a16_dst_subreg_nr(devinfo, inst) * 4;
   subreg_nr /= elk_reg_type_to_size(type);

   if (subreg_nr)
      format(file, ".%u", subreg_nr);
   string(file, "<1>");

   if (!is_align1) {
      err |= control(file, "writemask", writemask,
                     elk_inst_3src_a16_dst_writemask(devinfo, inst), NULL);
   }
   string(file, elk_reg_type_to_letters(type));

   return 0;
}

static int
src_align1_region(FILE *file,
                  unsigned _vert_stride, unsigned _width,
                  unsigned _horiz_stride)
{
   int err = 0;
   string(file, "<");
   err |= control(file, "vert stride", vert_stride, _vert_stride, NULL);
   string(file, ",");
   err |= control(file, "width", width, _width, NULL);
   string(file, ",");
   err |= control(file, "horiz_stride", horiz_stride, _horiz_stride, NULL);
   string(file, ">");
   return err;
}

static int
src_da1(FILE *file,
        const struct intel_device_info *devinfo,
        unsigned opcode,
        enum elk_reg_type type, unsigned _reg_file,
        unsigned _vert_stride, unsigned _width, unsigned _horiz_stride,
        unsigned reg_num, unsigned sub_reg_num, unsigned __abs,
        unsigned _negate)
{
   int err = 0;

   if (devinfo->ver >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   err |= reg(file, _reg_file, reg_num);
   if (err == -1)
      return 0;
   if (sub_reg_num) {
      unsigned elem_size = elk_reg_type_to_size(type);
      format(file, ".%d", sub_reg_num / elem_size);   /* use formal style like spec */
   }
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static int
src_ia1(FILE *file,
        const struct intel_device_info *devinfo,
        unsigned opcode,
        enum elk_reg_type type,
        int _addr_imm,
        unsigned _addr_subreg_nr,
        unsigned _negate,
        unsigned __abs,
        unsigned _horiz_stride, unsigned _width, unsigned _vert_stride)
{
   int err = 0;

   if (devinfo->ver >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   string(file, "g[a0");
   if (_addr_subreg_nr)
      format(file, ".%d", _addr_subreg_nr);
   if (_addr_imm)
      format(file, " %d", _addr_imm);
   string(file, "]");
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static int
src_swizzle(FILE *file, unsigned swiz)
{
   unsigned x = ELK_GET_SWZ(swiz, ELK_CHANNEL_X);
   unsigned y = ELK_GET_SWZ(swiz, ELK_CHANNEL_Y);
   unsigned z = ELK_GET_SWZ(swiz, ELK_CHANNEL_Z);
   unsigned w = ELK_GET_SWZ(swiz, ELK_CHANNEL_W);
   int err = 0;

   if (x == y && x == z && x == w) {
      string(file, ".");
      err |= control(file, "channel select", chan_sel, x, NULL);
   } else if (swiz != ELK_SWIZZLE_XYZW) {
      string(file, ".");
      err |= control(file, "channel select", chan_sel, x, NULL);
      err |= control(file, "channel select", chan_sel, y, NULL);
      err |= control(file, "channel select", chan_sel, z, NULL);
      err |= control(file, "channel select", chan_sel, w, NULL);
   }
   return err;
}

static int
src_da16(FILE *file,
         const struct intel_device_info *devinfo,
         unsigned opcode,
         enum elk_reg_type type,
         unsigned _reg_file,
         unsigned _vert_stride,
         unsigned _reg_nr,
         unsigned _subreg_nr,
         unsigned __abs,
         unsigned _negate,
         unsigned swz_x, unsigned swz_y, unsigned swz_z, unsigned swz_w)
{
   int err = 0;

   if (devinfo->ver >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   err |= reg(file, _reg_file, _reg_nr);
   if (err == -1)
      return 0;
   if (_subreg_nr) {
      unsigned elem_size = elk_reg_type_to_size(type);

      /* bit4 for subreg number byte addressing. Make this same meaning as
         in da1 case, so output looks consistent. */
      format(file, ".%d", 16 / elem_size);
   }
   string(file, "<");
   err |= control(file, "vert stride", vert_stride, _vert_stride, NULL);
   string(file, ">");
   err |= src_swizzle(file, ELK_SWIZZLE4(swz_x, swz_y, swz_z, swz_w));
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static enum elk_vertical_stride
vstride_from_align1_3src_vstride(const struct intel_device_info *devinfo,
                                 enum gfx10_align1_3src_vertical_stride vstride)
{
   switch (vstride) {
   case ELK_ALIGN1_3SRC_VERTICAL_STRIDE_0: return ELK_VERTICAL_STRIDE_0;
   case ELK_ALIGN1_3SRC_VERTICAL_STRIDE_2: return ELK_VERTICAL_STRIDE_2;
   case ELK_ALIGN1_3SRC_VERTICAL_STRIDE_4: return ELK_VERTICAL_STRIDE_4;
   case ELK_ALIGN1_3SRC_VERTICAL_STRIDE_8: return ELK_VERTICAL_STRIDE_8;
   default:
      unreachable("not reached");
   }
}

static enum elk_horizontal_stride
hstride_from_align1_3src_hstride(enum gfx10_align1_3src_src_horizontal_stride hstride)
{
   switch (hstride) {
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0: return ELK_HORIZONTAL_STRIDE_0;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1: return ELK_HORIZONTAL_STRIDE_1;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2: return ELK_HORIZONTAL_STRIDE_2;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4: return ELK_HORIZONTAL_STRIDE_4;
   default:
      unreachable("not reached");
   }
}

static enum elk_vertical_stride
vstride_from_align1_3src_hstride(enum gfx10_align1_3src_src_horizontal_stride hstride)
{
   switch (hstride) {
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0: return ELK_VERTICAL_STRIDE_0;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1: return ELK_VERTICAL_STRIDE_1;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2: return ELK_VERTICAL_STRIDE_2;
   case ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4: return ELK_VERTICAL_STRIDE_4;
   default:
      unreachable("not reached");
   }
}

/* From "GFX10 Regioning Rules for Align1 Ternary Operations" in the
 * "Register Region Restrictions" documentation
 */
static enum elk_width
implied_width(enum elk_vertical_stride _vert_stride,
              enum elk_horizontal_stride _horiz_stride)
{
   /* "1. Width is 1 when Vertical and Horizontal Strides are both zero." */
   if (_vert_stride == ELK_VERTICAL_STRIDE_0 &&
       _horiz_stride == ELK_HORIZONTAL_STRIDE_0) {
      return ELK_WIDTH_1;

   /* "2. Width is equal to vertical stride when Horizontal Stride is zero." */
   } else if (_horiz_stride == ELK_HORIZONTAL_STRIDE_0) {
      switch (_vert_stride) {
      case ELK_VERTICAL_STRIDE_1: return ELK_WIDTH_1;
      case ELK_VERTICAL_STRIDE_2: return ELK_WIDTH_2;
      case ELK_VERTICAL_STRIDE_4: return ELK_WIDTH_4;
      case ELK_VERTICAL_STRIDE_8: return ELK_WIDTH_8;
      case ELK_VERTICAL_STRIDE_0:
      default:
         unreachable("not reached");
      }

   } else {
      /* FINISHME: Implement these: */

      /* "3. Width is equal to Vertical Stride/Horizontal Stride when both
       *     Strides are non-zero.
       *
       *  4. Vertical Stride must not be zero if Horizontal Stride is non-zero.
       *     This implies Vertical Stride is always greater than Horizontal
       *     Stride."
       *
       * Given these statements and the knowledge that the stride and width
       * values are encoded in logarithmic form, we can perform the division
       * by just subtracting.
       */
      return _vert_stride - _horiz_stride;
   }
}

static int
src0_3src(FILE *file, const struct intel_device_info *devinfo,
          const elk_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum elk_reg_file _file;
   enum elk_reg_type type;
   enum elk_vertical_stride _vert_stride;
   enum elk_width _width;
   enum elk_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = elk_inst_3src_access_mode(devinfo, inst) == ELK_ALIGN_1;

   if (is_align1)
      return 0;

   _file = ELK_GENERAL_REGISTER_FILE;
   reg_nr = elk_inst_3src_src0_reg_nr(devinfo, inst);
   subreg_nr = elk_inst_3src_a16_src0_subreg_nr(devinfo, inst) * 4;
   type = elk_inst_3src_a16_src_type(devinfo, inst);

   if (elk_inst_3src_a16_src0_rep_ctrl(devinfo, inst)) {
      _vert_stride = ELK_VERTICAL_STRIDE_0;
      _width = ELK_WIDTH_1;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_0;
   } else {
      _vert_stride = ELK_VERTICAL_STRIDE_4;
      _width = ELK_WIDTH_4;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_1;
   }

   is_scalar_region = _vert_stride == ELK_VERTICAL_STRIDE_0 &&
                      _width == ELK_WIDTH_1 &&
                      _horiz_stride == ELK_HORIZONTAL_STRIDE_0;

   subreg_nr /= elk_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  elk_inst_3src_src0_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, elk_inst_3src_src0_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, elk_inst_3src_a16_src0_swizzle(devinfo, inst));
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static int
src1_3src(FILE *file, const struct intel_device_info *devinfo,
          const elk_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum elk_reg_file _file;
   enum elk_reg_type type;
   enum elk_vertical_stride _vert_stride;
   enum elk_width _width;
   enum elk_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = elk_inst_3src_access_mode(devinfo, inst) == ELK_ALIGN_1;

   if (is_align1)
      return 0;

   _file = ELK_GENERAL_REGISTER_FILE;
   reg_nr = elk_inst_3src_src1_reg_nr(devinfo, inst);
   subreg_nr = elk_inst_3src_a16_src1_subreg_nr(devinfo, inst) * 4;
   type = elk_inst_3src_a16_src_type(devinfo, inst);

   if (elk_inst_3src_a16_src1_rep_ctrl(devinfo, inst)) {
      _vert_stride = ELK_VERTICAL_STRIDE_0;
      _width = ELK_WIDTH_1;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_0;
   } else {
      _vert_stride = ELK_VERTICAL_STRIDE_4;
      _width = ELK_WIDTH_4;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_1;
   }

   is_scalar_region = _vert_stride == ELK_VERTICAL_STRIDE_0 &&
                      _width == ELK_WIDTH_1 &&
                      _horiz_stride == ELK_HORIZONTAL_STRIDE_0;

   subreg_nr /= elk_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  elk_inst_3src_src1_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, elk_inst_3src_src1_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, elk_inst_3src_a16_src1_swizzle(devinfo, inst));
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static int
src2_3src(FILE *file, const struct intel_device_info *devinfo,
          const elk_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum elk_reg_file _file;
   enum elk_reg_type type;
   enum elk_vertical_stride _vert_stride;
   enum elk_width _width;
   enum elk_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = elk_inst_3src_access_mode(devinfo, inst) == ELK_ALIGN_1;

   if (is_align1)
      return 0;

   _file = ELK_GENERAL_REGISTER_FILE;
   reg_nr = elk_inst_3src_src2_reg_nr(devinfo, inst);
   subreg_nr = elk_inst_3src_a16_src2_subreg_nr(devinfo, inst) * 4;
   type = elk_inst_3src_a16_src_type(devinfo, inst);

   if (elk_inst_3src_a16_src2_rep_ctrl(devinfo, inst)) {
      _vert_stride = ELK_VERTICAL_STRIDE_0;
      _width = ELK_WIDTH_1;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_0;
   } else {
      _vert_stride = ELK_VERTICAL_STRIDE_4;
      _width = ELK_WIDTH_4;
      _horiz_stride = ELK_HORIZONTAL_STRIDE_1;
   }

   is_scalar_region = _vert_stride == ELK_VERTICAL_STRIDE_0 &&
                      _width == ELK_WIDTH_1 &&
                      _horiz_stride == ELK_HORIZONTAL_STRIDE_0;

   subreg_nr /= elk_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  elk_inst_3src_src2_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, elk_inst_3src_src2_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, elk_inst_3src_a16_src2_swizzle(devinfo, inst));
   string(file, elk_reg_type_to_letters(type));
   return err;
}

static int
imm(FILE *file, const struct elk_isa_info *isa, enum elk_reg_type type,
    const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   switch (type) {
   case ELK_REGISTER_TYPE_UQ:
      format(file, "0x%016"PRIx64"UQ", elk_inst_imm_uq(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_Q:
      format(file, "0x%016"PRIx64"Q", elk_inst_imm_uq(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_UD:
      format(file, "0x%08xUD", elk_inst_imm_ud(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_D:
      format(file, "%dD", elk_inst_imm_d(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_UW:
      format(file, "0x%04xUW", (uint16_t) elk_inst_imm_ud(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_W:
      format(file, "%dW", (int16_t) elk_inst_imm_d(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_UV:
      format(file, "0x%08xUV", elk_inst_imm_ud(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_VF:
      format(file, "0x%"PRIx64"VF", elk_inst_bits(inst, 127, 96));
      pad(file, 48);
      format(file, "/* [%-gF, %-gF, %-gF, %-gF]VF */",
             elk_vf_to_float(elk_inst_imm_ud(devinfo, inst)),
             elk_vf_to_float(elk_inst_imm_ud(devinfo, inst) >> 8),
             elk_vf_to_float(elk_inst_imm_ud(devinfo, inst) >> 16),
             elk_vf_to_float(elk_inst_imm_ud(devinfo, inst) >> 24));
      break;
   case ELK_REGISTER_TYPE_V:
      format(file, "0x%08xV", elk_inst_imm_ud(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_F:
      /* The DIM instruction's src0 uses an F type but contains a
       * 64-bit immediate
       */
      if (elk_inst_opcode(isa, inst) == ELK_OPCODE_DIM) {
         format(file, "0x%"PRIx64"F", elk_inst_bits(inst, 127, 64));
         pad(file, 48);
         format(file, "/* %-gF */", elk_inst_imm_df(devinfo, inst));
      } else {
         format(file, "0x%"PRIx64"F", elk_inst_bits(inst, 127, 96));
         pad(file, 48);
         format(file, " /* %-gF */", elk_inst_imm_f(devinfo, inst));
      }
      break;
   case ELK_REGISTER_TYPE_DF:
      format(file, "0x%016"PRIx64"DF", elk_inst_imm_uq(devinfo, inst));
      pad(file, 48);
      format(file, "/* %-gDF */", elk_inst_imm_df(devinfo, inst));
      break;
   case ELK_REGISTER_TYPE_HF:
      format(file, "0x%04xHF",
             (uint16_t) elk_inst_imm_ud(devinfo, inst));
      pad(file, 48);
      format(file, "/* %-gHF */",
             _mesa_half_to_float((uint16_t) elk_inst_imm_ud(devinfo, inst)));
      break;
   case ELK_REGISTER_TYPE_NF:
   case ELK_REGISTER_TYPE_UB:
   case ELK_REGISTER_TYPE_B:
      format(file, "*** invalid immediate type %d ", type);
   }
   return 0;
}

static int
src_send_desc_ia(FILE *file,
                 const struct intel_device_info *devinfo,
                 unsigned _addr_subreg_nr)
{
   string(file, "a0");
   if (_addr_subreg_nr)
      format(file, ".%d", _addr_subreg_nr);
   format(file, "<0>UD");

   return 0;
}

static int
src0(FILE *file, const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   if (elk_inst_src0_reg_file(devinfo, inst) == ELK_IMMEDIATE_VALUE) {
      return imm(file, isa, elk_inst_src0_type(devinfo, inst), inst);
   } else if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
      if (elk_inst_src0_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         return src_da1(file,
                        devinfo,
                        elk_inst_opcode(isa, inst),
                        elk_inst_src0_type(devinfo, inst),
                        elk_inst_src0_reg_file(devinfo, inst),
                        elk_inst_src0_vstride(devinfo, inst),
                        elk_inst_src0_width(devinfo, inst),
                        elk_inst_src0_hstride(devinfo, inst),
                        elk_inst_src0_da_reg_nr(devinfo, inst),
                        elk_inst_src0_da1_subreg_nr(devinfo, inst),
                        elk_inst_src0_abs(devinfo, inst),
                        elk_inst_src0_negate(devinfo, inst));
      } else {
         return src_ia1(file,
                        devinfo,
                        elk_inst_opcode(isa, inst),
                        elk_inst_src0_type(devinfo, inst),
                        elk_inst_src0_ia1_addr_imm(devinfo, inst),
                        elk_inst_src0_ia_subreg_nr(devinfo, inst),
                        elk_inst_src0_negate(devinfo, inst),
                        elk_inst_src0_abs(devinfo, inst),
                        elk_inst_src0_hstride(devinfo, inst),
                        elk_inst_src0_width(devinfo, inst),
                        elk_inst_src0_vstride(devinfo, inst));
      }
   } else {
      if (elk_inst_src0_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         return src_da16(file,
                         devinfo,
                         elk_inst_opcode(isa, inst),
                         elk_inst_src0_type(devinfo, inst),
                         elk_inst_src0_reg_file(devinfo, inst),
                         elk_inst_src0_vstride(devinfo, inst),
                         elk_inst_src0_da_reg_nr(devinfo, inst),
                         elk_inst_src0_da16_subreg_nr(devinfo, inst),
                         elk_inst_src0_abs(devinfo, inst),
                         elk_inst_src0_negate(devinfo, inst),
                         elk_inst_src0_da16_swiz_x(devinfo, inst),
                         elk_inst_src0_da16_swiz_y(devinfo, inst),
                         elk_inst_src0_da16_swiz_z(devinfo, inst),
                         elk_inst_src0_da16_swiz_w(devinfo, inst));
      } else {
         string(file, "Indirect align16 address mode not supported");
         return 1;
      }
   }
}

static int
src1(FILE *file, const struct elk_isa_info *isa, const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   if (elk_inst_src1_reg_file(devinfo, inst) == ELK_IMMEDIATE_VALUE) {
      return imm(file, isa, elk_inst_src1_type(devinfo, inst), inst);
   } else if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
      if (elk_inst_src1_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         return src_da1(file,
                        devinfo,
                        elk_inst_opcode(isa, inst),
                        elk_inst_src1_type(devinfo, inst),
                        elk_inst_src1_reg_file(devinfo, inst),
                        elk_inst_src1_vstride(devinfo, inst),
                        elk_inst_src1_width(devinfo, inst),
                        elk_inst_src1_hstride(devinfo, inst),
                        elk_inst_src1_da_reg_nr(devinfo, inst),
                        elk_inst_src1_da1_subreg_nr(devinfo, inst),
                        elk_inst_src1_abs(devinfo, inst),
                        elk_inst_src1_negate(devinfo, inst));
      } else {
         return src_ia1(file,
                        devinfo,
                        elk_inst_opcode(isa, inst),
                        elk_inst_src1_type(devinfo, inst),
                        elk_inst_src1_ia1_addr_imm(devinfo, inst),
                        elk_inst_src1_ia_subreg_nr(devinfo, inst),
                        elk_inst_src1_negate(devinfo, inst),
                        elk_inst_src1_abs(devinfo, inst),
                        elk_inst_src1_hstride(devinfo, inst),
                        elk_inst_src1_width(devinfo, inst),
                        elk_inst_src1_vstride(devinfo, inst));
      }
   } else {
      if (elk_inst_src1_address_mode(devinfo, inst) == ELK_ADDRESS_DIRECT) {
         return src_da16(file,
                         devinfo,
                         elk_inst_opcode(isa, inst),
                         elk_inst_src1_type(devinfo, inst),
                         elk_inst_src1_reg_file(devinfo, inst),
                         elk_inst_src1_vstride(devinfo, inst),
                         elk_inst_src1_da_reg_nr(devinfo, inst),
                         elk_inst_src1_da16_subreg_nr(devinfo, inst),
                         elk_inst_src1_abs(devinfo, inst),
                         elk_inst_src1_negate(devinfo, inst),
                         elk_inst_src1_da16_swiz_x(devinfo, inst),
                         elk_inst_src1_da16_swiz_y(devinfo, inst),
                         elk_inst_src1_da16_swiz_z(devinfo, inst),
                         elk_inst_src1_da16_swiz_w(devinfo, inst));
      } else {
         string(file, "Indirect align16 address mode not supported");
         return 1;
      }
   }
}

static int
qtr_ctrl(FILE *file, const struct intel_device_info *devinfo,
         const elk_inst *inst)
{
   int qtr_ctl = elk_inst_qtr_control(devinfo, inst);
   int exec_size = 1 << elk_inst_exec_size(devinfo, inst);
   const unsigned nib_ctl =
      devinfo->ver < 7 ? 0 : elk_inst_nib_control(devinfo, inst);

   if (exec_size < 8 || nib_ctl) {
      format(file, " %dN", qtr_ctl * 2 + nib_ctl + 1);
   } else if (exec_size == 8) {
      switch (qtr_ctl) {
      case 0:
         string(file, " 1Q");
         break;
      case 1:
         string(file, " 2Q");
         break;
      case 2:
         string(file, " 3Q");
         break;
      case 3:
         string(file, " 4Q");
         break;
      }
   } else if (exec_size == 16) {
      if (qtr_ctl < 2)
         string(file, " 1H");
      else
         string(file, " 2H");
   }
   return 0;
}

static bool
inst_has_type(const struct elk_isa_info *isa,
              const elk_inst *inst,
              enum elk_reg_type type)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   const unsigned num_sources = elk_num_sources_from_inst(isa, inst);

   if (elk_inst_dst_type(devinfo, inst) == type)
      return true;

   if (num_sources >= 3) {
      return elk_inst_3src_a16_src_type(devinfo, inst) == type;
   } else if (num_sources == 2) {
      return elk_inst_src0_type(devinfo, inst) == type ||
             elk_inst_src1_type(devinfo, inst) == type;
   } else {
      return elk_inst_src0_type(devinfo, inst) == type;
   }
}

#if MESA_DEBUG
static __attribute__((__unused__)) int
elk_disassemble_imm(const struct elk_isa_info *isa,
                    uint32_t dw3, uint32_t dw2, uint32_t dw1, uint32_t dw0)
{
   elk_inst inst;
   inst.data[0] = (((uint64_t) dw1) << 32) | ((uint64_t) dw0);
   inst.data[1] = (((uint64_t) dw3) << 32) | ((uint64_t) dw2);
   return elk_disassemble_inst(stderr, isa, &inst, false, 0, NULL);
}
#endif

static void
write_label(FILE *file, const struct intel_device_info *devinfo,
            const struct elk_label *root_label,
            int offset, int jump)
{
   if (root_label != NULL) {
      int to_bytes_scale = sizeof(elk_inst) / elk_jump_scale(devinfo);
      const struct elk_label *label =
         elk_find_label(root_label, offset + jump * to_bytes_scale);
      if (label != NULL) {
         format(file, " LABEL%d", label->number);
      }
   }
}

static void
lsc_disassemble_ex_desc(const struct intel_device_info *devinfo,
                        uint32_t imm_desc,
                        uint32_t imm_ex_desc,
                        FILE *file)
{
   const unsigned addr_type = lsc_msg_desc_addr_type(devinfo, imm_desc);
   switch (addr_type) {
   case LSC_ADDR_SURFTYPE_FLAT:
      format(file, " base_offset %u ",
             lsc_flat_ex_desc_base_offset(devinfo, imm_ex_desc));
      break;
   case LSC_ADDR_SURFTYPE_BSS:
   case LSC_ADDR_SURFTYPE_SS:
      format(file, " surface_state_index %u ",
             lsc_bss_ex_desc_index(devinfo, imm_ex_desc));
      break;
   case LSC_ADDR_SURFTYPE_BTI:
      format(file, " BTI %u ",
             lsc_bti_ex_desc_index(devinfo, imm_ex_desc));
      format(file, " base_offset %u ",
             lsc_bti_ex_desc_base_offset(devinfo, imm_ex_desc));
      break;
   default:
      format(file, "unsupported address surface type %d", addr_type);
      break;
   }
}

int
elk_disassemble_inst(FILE *file, const struct elk_isa_info *isa,
                     const elk_inst *inst, bool is_compacted,
                     int offset, const struct elk_label *root_label)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   int err = 0;
   int space = 0;

   const enum elk_opcode opcode = elk_inst_opcode(isa, inst);
   const struct elk_opcode_desc *desc = elk_opcode_desc(isa, opcode);

   if (elk_inst_pred_control(devinfo, inst)) {
      string(file, "(");
      err |= control(file, "predicate inverse", pred_inv,
                     elk_inst_pred_inv(devinfo, inst), NULL);
      format(file, "f%"PRIu64".%"PRIu64,
             devinfo->ver >= 7 ? elk_inst_flag_reg_nr(devinfo, inst) : 0,
             elk_inst_flag_subreg_nr(devinfo, inst));
      if (elk_inst_access_mode(devinfo, inst) == ELK_ALIGN_1) {
         err |= control(file, "predicate control align1", pred_ctrl_align1,
                        elk_inst_pred_control(devinfo, inst), NULL);
      } else {
         err |= control(file, "predicate control align16", elk_pred_ctrl_align16,
                        elk_inst_pred_control(devinfo, inst), NULL);
      }
      string(file, ") ");
   }

   err |= print_opcode(file, isa, opcode);

   if (!is_send(opcode))
      err |= control(file, "saturate", saturate, elk_inst_saturate(devinfo, inst),
                     NULL);

   err |= control(file, "debug control", debug_ctrl,
                  elk_inst_debug_control(devinfo, inst), NULL);

   if (opcode == ELK_OPCODE_MATH) {
      string(file, " ");
      err |= control(file, "function", math_function,
                     elk_inst_math_function(devinfo, inst), NULL);

   } else if (!is_send(opcode)) {
      err |= control(file, "conditional modifier", elk_conditional_modifier,
                     elk_inst_cond_modifier(devinfo, inst), NULL);

      /* If we're using the conditional modifier, print which flags reg is
       * used for it.  Note that on gfx6+, the embedded-condition SEL and
       * control flow doesn't update flags.
       */
      if (elk_inst_cond_modifier(devinfo, inst) &&
          (devinfo->ver < 6 || (opcode != ELK_OPCODE_SEL &&
                                opcode != ELK_OPCODE_CSEL &&
                                opcode != ELK_OPCODE_IF &&
                                opcode != ELK_OPCODE_WHILE))) {
         format(file, ".f%"PRIu64".%"PRIu64,
                devinfo->ver >= 7 ? elk_inst_flag_reg_nr(devinfo, inst) : 0,
                elk_inst_flag_subreg_nr(devinfo, inst));
      }
   }

   if (opcode != ELK_OPCODE_NOP && opcode != ELK_OPCODE_NENOP) {
      string(file, "(");
      err |= control(file, "execution size", exec_size,
                     elk_inst_exec_size(devinfo, inst), NULL);
      string(file, ")");
   }

   if (opcode == ELK_OPCODE_SEND && devinfo->ver < 6)
      format(file, " %"PRIu64, elk_inst_base_mrf(devinfo, inst));

   if (elk_has_uip(devinfo, opcode)) {
      /* Instructions that have UIP also have JIP. */
      pad(file, 16);
      string(file, "JIP: ");
      write_label(file, devinfo, root_label, offset, elk_inst_jip(devinfo, inst));

      pad(file, 38);
      string(file, "UIP: ");
      write_label(file, devinfo, root_label, offset, elk_inst_uip(devinfo, inst));
   } else if (elk_has_jip(devinfo, opcode)) {
      int jip;
      if (devinfo->ver >= 7) {
         jip = elk_inst_jip(devinfo, inst);
      } else {
         jip = elk_inst_gfx6_jump_count(devinfo, inst);
      }

      pad(file, 16);
      string(file, "JIP: ");
      write_label(file, devinfo, root_label, offset, jip);
   } else if (devinfo->ver < 6 && (opcode == ELK_OPCODE_BREAK ||
                                   opcode == ELK_OPCODE_CONTINUE ||
                                   opcode == ELK_OPCODE_ELSE)) {
      pad(file, 16);
      format(file, "Jump: %d", elk_inst_gfx4_jump_count(devinfo, inst));
      pad(file, 32);
      format(file, "Pop: %"PRIu64, elk_inst_gfx4_pop_count(devinfo, inst));
   } else if (devinfo->ver < 6 && (opcode == ELK_OPCODE_IF ||
                                   opcode == ELK_OPCODE_IFF ||
                                   opcode == ELK_OPCODE_HALT ||
                                   opcode == ELK_OPCODE_WHILE)) {
      pad(file, 16);
      format(file, "Jump: %d", elk_inst_gfx4_jump_count(devinfo, inst));
   } else if (devinfo->ver < 6 && opcode == ELK_OPCODE_ENDIF) {
      pad(file, 16);
      format(file, "Pop: %"PRIu64, elk_inst_gfx4_pop_count(devinfo, inst));
   } else if (opcode == ELK_OPCODE_JMPI) {
      pad(file, 16);
      err |= src1(file, isa, inst);
   } else if (desc && desc->nsrc == 3) {
      pad(file, 16);
      err |= dest_3src(file, devinfo, inst);

      pad(file, 32);
      err |= src0_3src(file, devinfo, inst);

      pad(file, 48);
      err |= src1_3src(file, devinfo, inst);

      pad(file, 64);
      err |= src2_3src(file, devinfo, inst);
   } else if (desc) {
      if (desc->ndst > 0) {
         pad(file, 16);
         err |= dest(file, isa, inst);
      }

      if (desc->nsrc > 0) {
         pad(file, 32);
         err |= src0(file, isa, inst);
      }

      if (desc->nsrc > 1) {
         pad(file, 48);
         err |= src1(file, isa, inst);
      }
   }

   if (is_send(opcode)) {
      enum elk_message_target sfid = elk_inst_sfid(devinfo, inst);

      bool has_imm_desc = false, has_imm_ex_desc = false;
      uint32_t imm_desc = 0, imm_ex_desc = 0;
      {
         if (elk_inst_src1_reg_file(devinfo, inst) != ELK_IMMEDIATE_VALUE) {
            /* show the indirect descriptor source */
            pad(file, 48);
            err |= src1(file, isa, inst);
            pad(file, 64);
         } else {
            has_imm_desc = true;
            imm_desc = elk_inst_send_desc(devinfo, inst);
            pad(file, 48);
         }

         /* Print message descriptor as immediate source */
         fprintf(file, "0x%08"PRIx64, inst->data[1] >> 32);
      }

      newline(file);
      pad(file, 16);
      space = 0;

      fprintf(file, "            ");
      err |= control(file, "SFID", devinfo->ver >= 6 ? gfx6_sfid : gfx4_sfid,
                     sfid, &space);
      string(file, " MsgDesc:");

      if (!has_imm_desc) {
         format(file, " indirect");
      } else {
         bool unsupported = false;
         switch (sfid) {
         case ELK_SFID_MATH:
            err |= control(file, "math function", math_function,
                           elk_inst_math_msg_function(devinfo, inst), &space);
            err |= control(file, "math saturate", math_saturate,
                           elk_inst_math_msg_saturate(devinfo, inst), &space);
            err |= control(file, "math signed", math_signed,
                           elk_inst_math_msg_signed_int(devinfo, inst), &space);
            err |= control(file, "math scalar", math_scalar,
                           elk_inst_math_msg_data_type(devinfo, inst), &space);
            err |= control(file, "math precision", math_precision,
                           elk_inst_math_msg_precision(devinfo, inst), &space);
            break;
         case ELK_SFID_SAMPLER:
            if (devinfo->ver >= 5) {
               err |= control(file, "sampler message", gfx5_sampler_msg_type,
                              elk_sampler_desc_msg_type(devinfo, imm_desc),
                              &space);
               err |= control(file, "sampler simd mode", gfx5_sampler_simd_mode,
                              elk_sampler_desc_simd_mode(devinfo, imm_desc),
                              &space);
               if (devinfo->ver >= 8 &&
                   elk_sampler_desc_return_format(devinfo, imm_desc)) {
                  string(file, " HP");
               }
               format(file, " Surface = %u Sampler = %u",
                      elk_sampler_desc_binding_table_index(devinfo, imm_desc),
                      elk_sampler_desc_sampler(devinfo, imm_desc));
            } else {
               format(file, " (bti %u, sampler %u, msg_type %u, ",
                      elk_sampler_desc_binding_table_index(devinfo, imm_desc),
                      elk_sampler_desc_sampler(devinfo, imm_desc),
                      elk_sampler_desc_msg_type(devinfo, imm_desc));
               if (devinfo->verx10 != 45) {
                  err |= control(file, "sampler target format",
                                 sampler_target_format,
                                 elk_sampler_desc_return_format(devinfo, imm_desc),
                                 NULL);
               }
               string(file, ")");
            }
            break;
         case GFX6_SFID_DATAPORT_SAMPLER_CACHE:
         case GFX6_SFID_DATAPORT_CONSTANT_CACHE:
            /* aka ELK_SFID_DATAPORT_READ on Gfx4-5 */
            if (devinfo->ver >= 6) {
               format(file, " (bti %u, msg_ctrl %u, msg_type %u, write_commit %u)",
                      elk_dp_desc_binding_table_index(devinfo, imm_desc),
                      elk_dp_desc_msg_control(devinfo, imm_desc),
                      elk_dp_desc_msg_type(devinfo, imm_desc),
                      devinfo->ver >= 7 ? 0u :
                      elk_dp_write_desc_write_commit(devinfo, imm_desc));
            } else {
               bool is_965 = devinfo->verx10 == 40;
               err |= control(file, "DP read message type",
                              is_965 ? gfx4_dp_read_port_msg_type :
                                       g45_dp_read_port_msg_type,
                              elk_dp_read_desc_msg_type(devinfo, imm_desc),
                              &space);

               format(file, " MsgCtrl = 0x%u",
                      elk_dp_read_desc_msg_control(devinfo, imm_desc));

               format(file, " Surface = %u",
                      elk_dp_desc_binding_table_index(devinfo, imm_desc));
            }
            break;

         case GFX6_SFID_DATAPORT_RENDER_CACHE: {
            /* aka ELK_SFID_DATAPORT_WRITE on Gfx4-5 */
            unsigned msg_type = elk_fb_write_desc_msg_type(devinfo, imm_desc);

            err |= control(file, "DP rc message type",
                           dp_rc_msg_type(devinfo), msg_type, &space);

            bool is_rt_write = msg_type ==
               (devinfo->ver >= 6 ? GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE
                                  : ELK_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE);

            if (is_rt_write) {
               err |= control(file, "RT message type", m_rt_write_subtype,
                              elk_inst_rt_message_type(devinfo, inst), &space);
               if (devinfo->ver >= 6 && elk_inst_rt_slot_group(devinfo, inst))
                  string(file, " Hi");
               if (elk_fb_write_desc_last_render_target(devinfo, imm_desc))
                  string(file, " LastRT");
               if (devinfo->ver < 7 &&
                   elk_fb_write_desc_write_commit(devinfo, imm_desc))
                  string(file, " WriteCommit");
            } else {
               format(file, " MsgCtrl = 0x%u",
                      elk_fb_write_desc_msg_control(devinfo, imm_desc));
            }

            format(file, " Surface = %u",
                   elk_fb_desc_binding_table_index(devinfo, imm_desc));
            break;
         }

         case ELK_SFID_URB: {
            unsigned urb_opcode = elk_inst_urb_opcode(devinfo, inst);

            format(file, " offset %"PRIu64, elk_inst_urb_global_offset(devinfo, inst));

            space = 1;

            err |= control(file, "urb opcode",
                           devinfo->ver >= 7 ? gfx7_urb_opcode
                           : gfx5_urb_opcode,
                           urb_opcode, &space);

            if (devinfo->ver >= 7 &&
                elk_inst_urb_per_slot_offset(devinfo, inst)) {
               string(file, " per-slot");
            }

            if (urb_opcode == GFX8_URB_OPCODE_SIMD8_WRITE ||
                urb_opcode == GFX8_URB_OPCODE_SIMD8_READ) {
               if (elk_inst_urb_channel_mask_present(devinfo, inst))
                  string(file, " masked");
            } else {
               err |= control(file, "urb swizzle", urb_swizzle,
                              elk_inst_urb_swizzle_control(devinfo, inst),
                              &space);
            }

            if (devinfo->ver < 7) {
               err |= control(file, "urb allocate", urb_allocate,
                              elk_inst_urb_allocate(devinfo, inst), &space);
               err |= control(file, "urb used", urb_used,
                              elk_inst_urb_used(devinfo, inst), &space);
            }
            if (devinfo->ver < 8) {
               err |= control(file, "urb complete", urb_complete,
                              elk_inst_urb_complete(devinfo, inst), &space);
            }
            break;
         }
         case ELK_SFID_THREAD_SPAWNER:
            break;

         case ELK_SFID_MESSAGE_GATEWAY:
            format(file, " (%s)",
                   gfx7_gateway_subfuncid[elk_inst_gateway_subfuncid(devinfo, inst)]);
            break;

         case GFX7_SFID_DATAPORT_DATA_CACHE:
            if (devinfo->ver >= 7) {
               format(file, " (");
               space = 0;

               err |= control(file, "DP DC0 message type",
                              dp_dc0_msg_type_gfx7,
                              elk_dp_desc_msg_type(devinfo, imm_desc), &space);

               format(file, ", bti %u, ",
                      elk_dp_desc_binding_table_index(devinfo, imm_desc));

               switch (elk_inst_dp_msg_type(devinfo, inst)) {
               case GFX7_DATAPORT_DC_UNTYPED_ATOMIC_OP:
                  control(file, "atomic op", aop,
                          elk_dp_desc_msg_control(devinfo, imm_desc) & 0xf,
                          &space);
                  break;
               case GFX7_DATAPORT_DC_OWORD_BLOCK_READ:
               case GFX7_DATAPORT_DC_OWORD_BLOCK_WRITE: {
                  unsigned msg_ctrl = elk_dp_desc_msg_control(devinfo, imm_desc);
                  assert(dp_oword_block_rw[msg_ctrl & 7]);
                  format(file, "owords = %s, aligned = %d",
                        dp_oword_block_rw[msg_ctrl & 7], (msg_ctrl >> 3) & 3);
                  break;
               }
               default:
                  format(file, "%u",
                         elk_dp_desc_msg_control(devinfo, imm_desc));
               }
               format(file, ")");
            } else {
               unsupported = true;
            }
            break;

         case HSW_SFID_DATAPORT_DATA_CACHE_1: {
            if (devinfo->ver >= 7) {
               format(file, " (");
               space = 0;

               unsigned msg_ctrl = elk_dp_desc_msg_control(devinfo, imm_desc);

               err |= control(file, "DP DC1 message type",
                              dp_dc1_msg_type_hsw,
                              elk_dp_desc_msg_type(devinfo, imm_desc), &space);

               format(file, ", Surface = %u, ",
                      elk_dp_desc_binding_table_index(devinfo, imm_desc));

               switch (elk_inst_dp_msg_type(devinfo, inst)) {
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP:
               case HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP:
                  format(file, "SIMD%d,", (msg_ctrl & (1 << 4)) ? 8 : 16);
                  FALLTHROUGH;
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2:
               case GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_OP:
                  control(file, "atomic op", aop, msg_ctrl & 0xf, &space);
                  break;
               case HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ:
               case HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE:
               case HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ:
               case HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE:
               case GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_WRITE:
               case GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_READ: {
                  static const char *simd_modes[] = { "4x2", "16", "8" };
                  format(file, "SIMD%s, Mask = 0x%x",
                         simd_modes[msg_ctrl >> 4], msg_ctrl & 0xf);
                  break;
               }
               case GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_WRITE:
               case GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_READ:
                  assert(dp_oword_block_rw[msg_ctrl & 7]);
                  format(file, "owords = %s, aligned = %d",
                        dp_oword_block_rw[msg_ctrl & 7], (msg_ctrl >> 3) & 3);
                  break;
               default:
                  format(file, "0x%x", msg_ctrl);
               }
               format(file, ")");
            } else {
               unsupported = true;
            }
            break;
         }

         case GFX7_SFID_PIXEL_INTERPOLATOR:
            if (devinfo->ver >= 7) {
               format(file, " (%s, %s, 0x%02"PRIx64")",
                      elk_inst_pi_nopersp(devinfo, inst) ? "linear" : "persp",
                      pixel_interpolator_msg_types[elk_inst_pi_message_type(devinfo, inst)],
                      elk_inst_pi_message_data(devinfo, inst));
            } else {
               unsupported = true;
            }
            break;

         default:
            unsupported = true;
            break;
         }

         if (unsupported)
            format(file, "unsupported shared function ID %d", sfid);

         if (space)
            string(file, " ");
      }
      if (has_imm_desc)
         format(file, " mlen %u", elk_message_desc_mlen(devinfo, imm_desc));
      if (has_imm_ex_desc) {
         format(file, " ex_mlen %u",
                elk_message_ex_desc_ex_mlen(devinfo, imm_ex_desc));
      }
      if (has_imm_desc)
         format(file, " rlen %u", elk_message_desc_rlen(devinfo, imm_desc));
   }
   pad(file, 64);
   if (opcode != ELK_OPCODE_NOP && opcode != ELK_OPCODE_NENOP) {
      string(file, "{");
      space = 1;
      err |= control(file, "access mode", access_mode,
                     elk_inst_access_mode(devinfo, inst), &space);
      if (devinfo->ver >= 6) {
         err |= control(file, "write enable control", wectrl,
                        elk_inst_mask_control(devinfo, inst), &space);
      } else {
         err |= control(file, "mask control", mask_ctrl,
                        elk_inst_mask_control(devinfo, inst), &space);
      }

      err |= control(file, "dependency control", dep_ctrl,
                     ((elk_inst_no_dd_check(devinfo, inst) << 1) |
                      elk_inst_no_dd_clear(devinfo, inst)), &space);

      if (devinfo->ver >= 6)
         err |= qtr_ctrl(file, devinfo, inst);
      else {
         if (elk_inst_qtr_control(devinfo, inst) == ELK_COMPRESSION_COMPRESSED &&
             desc && desc->ndst > 0 &&
             elk_inst_dst_reg_file(devinfo, inst) == ELK_MESSAGE_REGISTER_FILE &&
             elk_inst_dst_da_reg_nr(devinfo, inst) & ELK_MRF_COMPR4) {
            format(file, " compr4");
         } else {
            err |= control(file, "compression control", compr_ctrl,
                           elk_inst_qtr_control(devinfo, inst), &space);
         }
      }

      err |= control(file, "compaction", cmpt_ctrl, is_compacted, &space);
      err |= control(file, "thread control", thread_ctrl,
                     elk_inst_thread_control(devinfo, inst),
                     &space);
      if (has_branch_ctrl(devinfo, opcode)) {
         err |= control(file, "branch ctrl", branch_ctrl,
                        elk_inst_branch_control(devinfo, inst), &space);
      } else if (devinfo->ver >= 6) {
         err |= control(file, "acc write control", accwr,
                        elk_inst_acc_wr_control(devinfo, inst), &space);
      }
      if (is_send(opcode))
         err |= control(file, "end of thread", end_of_thread,
                        elk_inst_eot(devinfo, inst), &space);
      if (space)
         string(file, " ");
      string(file, "}");
   }
   string(file, ";");
   newline(file);
   return err;
}

int
elk_disassemble_find_end(const struct elk_isa_info *isa,
                         const void *assembly, int start)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   int offset = start;

   /* This loop exits when send-with-EOT or when opcode is 0 */
   while (true) {
      const elk_inst *insn = assembly + offset;

      if (elk_inst_cmpt_control(devinfo, insn)) {
         offset += 8;
      } else {
         offset += 16;
      }

      /* Simplistic, but efficient way to terminate disasm */
      uint32_t opcode = elk_inst_opcode(isa, insn);
      if (opcode == 0 || (is_send(opcode) && elk_inst_eot(devinfo, insn))) {
         break;
      }
   }

   return offset;
}

void
elk_disassemble_with_errors(const struct elk_isa_info *isa,
                            const void *assembly, int start, FILE *out)
{
   int end = elk_disassemble_find_end(isa, assembly, start);

   /* Make a dummy disasm structure that elk_validate_instructions
    * can work from.
    */
   struct elk_disasm_info *elk_disasm_info = elk_disasm_initialize(isa, NULL);
   elk_disasm_new_inst_group(elk_disasm_info, start);
   elk_disasm_new_inst_group(elk_disasm_info, end);

   elk_validate_instructions(isa, assembly, start, end, elk_disasm_info);

   void *mem_ctx = ralloc_context(NULL);
   const struct elk_label *root_label =
      elk_label_assembly(isa, assembly, start, end, mem_ctx);

   foreach_list_typed(struct inst_group, group, link,
                      &elk_disasm_info->group_list) {
      struct exec_node *next_node = exec_node_get_next(&group->link);
      if (exec_node_is_tail_sentinel(next_node))
         break;

      struct inst_group *next =
         exec_node_data(struct inst_group, next_node, link);

      int start_offset = group->offset;
      int end_offset = next->offset;

      elk_disassemble(isa, assembly, start_offset, end_offset,
                      root_label, out);

      if (group->error) {
         fputs(group->error, out);
      }
   }

   ralloc_free(mem_ctx);
   ralloc_free(elk_disasm_info);
}
