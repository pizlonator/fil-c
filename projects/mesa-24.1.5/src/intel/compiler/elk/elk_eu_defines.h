/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics to
 develop this 3D driver.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */

#ifndef ELK_EU_DEFINES_H
#define ELK_EU_DEFINES_H

#include <stdint.h>
#include <stdlib.h>
#include "util/macros.h"
#include "dev/intel_device_info.h"
#include "elk_eu_opcodes.h"

/* The following hunk, up-to "Execution Unit" is used by both the
 * intel/compiler and i965 codebase. */

#define INTEL_MASK(high, low) (((1u<<((high)-(low)+1))-1)<<(low))
/* Using the GNU statement expression extension */
#define SET_FIELD(value, field)                                         \
   ({                                                                   \
      uint32_t fieldval = (uint32_t)(value) << field ## _SHIFT;         \
      assert((fieldval & ~ field ## _MASK) == 0);                       \
      fieldval & field ## _MASK;                                        \
   })

#define SET_BITS(value, high, low)                                      \
   ({                                                                   \
      const uint32_t fieldval = (uint32_t)(value) << (low);             \
      assert((fieldval & ~INTEL_MASK(high, low)) == 0);                 \
      fieldval & INTEL_MASK(high, low);                                 \
   })

#define GET_BITS(data, high, low) ((data & INTEL_MASK((high), (low))) >> (low))
#define GET_FIELD(word, field) (((word)  & field ## _MASK) >> field ## _SHIFT)

/* Bitfields for the URB_WRITE message, DW2 of message header: */
#define URB_WRITE_PRIM_END		0x1
#define URB_WRITE_PRIM_START		0x2
#define URB_WRITE_PRIM_TYPE_SHIFT	2

#define ELK_SPRITE_POINT_ENABLE  16

# define GFX7_GS_CONTROL_DATA_FORMAT_GSCTL_CUT		0
# define GFX7_GS_CONTROL_DATA_FORMAT_GSCTL_SID		1

/* Execution Unit (EU) defines
 */

#define ELK_ALIGN_1   0
#define ELK_ALIGN_16  1

#define ELK_ADDRESS_DIRECT                        0
#define ELK_ADDRESS_REGISTER_INDIRECT_REGISTER    1

#define ELK_CHANNEL_X     0
#define ELK_CHANNEL_Y     1
#define ELK_CHANNEL_Z     2
#define ELK_CHANNEL_W     3

enum elk_compression {
   ELK_COMPRESSION_NONE       = 0,
   ELK_COMPRESSION_2NDHALF    = 1,
   ELK_COMPRESSION_COMPRESSED = 2,
};

#define GFX6_COMPRESSION_1Q		0
#define GFX6_COMPRESSION_2Q		1
#define GFX6_COMPRESSION_3Q		2
#define GFX6_COMPRESSION_4Q		3
#define GFX6_COMPRESSION_1H		0
#define GFX6_COMPRESSION_2H		2

enum ENUM_PACKED elk_conditional_mod {
   ELK_CONDITIONAL_NONE = 0,
   ELK_CONDITIONAL_Z    = 1,
   ELK_CONDITIONAL_NZ   = 2,
   ELK_CONDITIONAL_EQ   = 1,	/* Z */
   ELK_CONDITIONAL_NEQ  = 2,	/* NZ */
   ELK_CONDITIONAL_G    = 3,
   ELK_CONDITIONAL_GE   = 4,
   ELK_CONDITIONAL_L    = 5,
   ELK_CONDITIONAL_LE   = 6,
   ELK_CONDITIONAL_R    = 7,    /* Gen <= 5 */
   ELK_CONDITIONAL_O    = 8,
   ELK_CONDITIONAL_U    = 9,
};

#define ELK_DEBUG_NONE        0
#define ELK_DEBUG_BREAKPOINT  1

#define ELK_DEPENDENCY_NORMAL         0
#define ELK_DEPENDENCY_NOTCLEARED     1
#define ELK_DEPENDENCY_NOTCHECKED     2
#define ELK_DEPENDENCY_DISABLE        3

enum ENUM_PACKED elk_execution_size {
   ELK_EXECUTE_1  = 0,
   ELK_EXECUTE_2  = 1,
   ELK_EXECUTE_4  = 2,
   ELK_EXECUTE_8  = 3,
   ELK_EXECUTE_16 = 4,
   ELK_EXECUTE_32 = 5,
};

enum ENUM_PACKED elk_horizontal_stride {
   ELK_HORIZONTAL_STRIDE_0 = 0,
   ELK_HORIZONTAL_STRIDE_1 = 1,
   ELK_HORIZONTAL_STRIDE_2 = 2,
   ELK_HORIZONTAL_STRIDE_4 = 3,
};

enum ENUM_PACKED gfx10_align1_3src_src_horizontal_stride {
   ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0 = 0,
   ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1 = 1,
   ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2 = 2,
   ELK_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4 = 3,
};

enum ENUM_PACKED gfx10_align1_3src_dst_horizontal_stride {
   ELK_ALIGN1_3SRC_DST_HORIZONTAL_STRIDE_1 = 0,
   ELK_ALIGN1_3SRC_DST_HORIZONTAL_STRIDE_2 = 1,
};

#define ELK_INSTRUCTION_NORMAL    0
#define ELK_INSTRUCTION_SATURATE  1

#define ELK_MASK_ENABLE   0
#define ELK_MASK_DISABLE  1

/** @{
 *
 * Gfx6 has replaced "mask enable/disable" with WECtrl, which is
 * effectively the same but much simpler to think about.  Now, there
 * are two contributors ANDed together to whether channels are
 * executed: The predication on the instruction, and the channel write
 * enable.
 */
/**
 * This is the default value.  It means that a channel's write enable is set
 * if the per-channel IP is pointing at this instruction.
 */
#define ELK_WE_NORMAL		0
/**
 * This is used like ELK_MASK_DISABLE, and causes all channels to have
 * their write enable set.  Note that predication still contributes to
 * whether the channel actually gets written.
 */
#define ELK_WE_ALL		1
/** @} */

enum elk_urb_write_flags {
   ELK_URB_WRITE_NO_FLAGS = 0,

   /**
    * Causes a new URB entry to be allocated, and its address stored in the
    * destination register (gen < 7).
    */
   ELK_URB_WRITE_ALLOCATE = 0x1,

   /**
    * Causes the current URB entry to be deallocated (gen < 7).
    */
   ELK_URB_WRITE_UNUSED = 0x2,

   /**
    * Causes the thread to terminate.
    */
   ELK_URB_WRITE_EOT = 0x4,

   /**
    * Indicates that the given URB entry is complete, and may be sent further
    * down the 3D pipeline (gen < 7).
    */
   ELK_URB_WRITE_COMPLETE = 0x8,

   /**
    * Indicates that an additional offset (which may be different for the two
    * vec4 slots) is stored in the message header (gen == 7).
    */
   ELK_URB_WRITE_PER_SLOT_OFFSET = 0x10,

   /**
    * Indicates that the channel masks in the URB_WRITE message header should
    * not be overridden to 0xff (gen == 7).
    */
   ELK_URB_WRITE_USE_CHANNEL_MASKS = 0x20,

   /**
    * Indicates that the data should be sent to the URB using the
    * URB_WRITE_OWORD message rather than URB_WRITE_HWORD (gen == 7).  This
    * causes offsets to be interpreted as multiples of an OWORD instead of an
    * HWORD, and only allows one OWORD to be written.
    */
   ELK_URB_WRITE_OWORD = 0x40,

   /**
    * Convenient combination of flags: end the thread while simultaneously
    * marking the given URB entry as complete.
    */
   ELK_URB_WRITE_EOT_COMPLETE = ELK_URB_WRITE_EOT | ELK_URB_WRITE_COMPLETE,

   /**
    * Convenient combination of flags: mark the given URB entry as complete
    * and simultaneously allocate a new one.
    */
   ELK_URB_WRITE_ALLOCATE_COMPLETE =
      ELK_URB_WRITE_ALLOCATE | ELK_URB_WRITE_COMPLETE,
};

enum fb_write_logical_srcs {
   FB_WRITE_LOGICAL_SRC_COLOR0,      /* REQUIRED */
   FB_WRITE_LOGICAL_SRC_COLOR1,      /* for dual source blend messages */
   FB_WRITE_LOGICAL_SRC_SRC0_ALPHA,
   FB_WRITE_LOGICAL_SRC_SRC_DEPTH,   /* gl_FragDepth */
   FB_WRITE_LOGICAL_SRC_DST_DEPTH,   /* GFX4-5: passthrough from thread */
   FB_WRITE_LOGICAL_SRC_OMASK,       /* Sample Mask (gl_SampleMask) */
   FB_WRITE_LOGICAL_SRC_COMPONENTS,  /* REQUIRED */
   FB_WRITE_LOGICAL_NUM_SRCS
};

enum tex_logical_srcs {
   /** Texture coordinates */
   TEX_LOGICAL_SRC_COORDINATE,
   /** Shadow comparator */
   TEX_LOGICAL_SRC_SHADOW_C,
   /** dPdx if the operation takes explicit derivatives, otherwise LOD value */
   TEX_LOGICAL_SRC_LOD,
   /** dPdy if the operation takes explicit derivatives */
   TEX_LOGICAL_SRC_LOD2,
   /** Min LOD */
   TEX_LOGICAL_SRC_MIN_LOD,
   /** Sample index */
   TEX_LOGICAL_SRC_SAMPLE_INDEX,
   /** MCS data */
   TEX_LOGICAL_SRC_MCS,
   /** REQUIRED: Texture surface index */
   TEX_LOGICAL_SRC_SURFACE,
   /** Texture sampler index */
   TEX_LOGICAL_SRC_SAMPLER,
   /** Texture surface bindless handle */
   TEX_LOGICAL_SRC_SURFACE_HANDLE,
   /** Texture sampler bindless handle */
   TEX_LOGICAL_SRC_SAMPLER_HANDLE,
   /** Texel offset for gathers */
   TEX_LOGICAL_SRC_TG4_OFFSET,
   /** REQUIRED: Number of coordinate components (as UD immediate) */
   TEX_LOGICAL_SRC_COORD_COMPONENTS,
   /** REQUIRED: Number of derivative components (as UD immediate) */
   TEX_LOGICAL_SRC_GRAD_COMPONENTS,
   /** REQUIRED: request residency (as UD immediate) */
   TEX_LOGICAL_SRC_RESIDENCY,

   TEX_LOGICAL_NUM_SRCS,
};

enum pull_uniform_constant_srcs {
   /** Surface binding table index */
   PULL_UNIFORM_CONSTANT_SRC_SURFACE,
   /** Surface bindless handle */
   PULL_UNIFORM_CONSTANT_SRC_SURFACE_HANDLE,
   /** Surface offset */
   PULL_UNIFORM_CONSTANT_SRC_OFFSET,
   /** Pull size */
   PULL_UNIFORM_CONSTANT_SRC_SIZE,

   PULL_UNIFORM_CONSTANT_SRCS,
};

enum pull_varying_constant_srcs {
   /** Surface binding table index */
   PULL_VARYING_CONSTANT_SRC_SURFACE,
   /** Surface bindless handle */
   PULL_VARYING_CONSTANT_SRC_SURFACE_HANDLE,
   /** Surface offset */
   PULL_VARYING_CONSTANT_SRC_OFFSET,
   /** Pull alignment */
   PULL_VARYING_CONSTANT_SRC_ALIGNMENT,

   PULL_VARYING_CONSTANT_SRCS,
};

enum get_buffer_size_srcs {
   /** Surface binding table index */
   GET_BUFFER_SIZE_SRC_SURFACE,
   /** Surface bindless handle */
   GET_BUFFER_SIZE_SRC_SURFACE_HANDLE,
   /** LOD */
   GET_BUFFER_SIZE_SRC_LOD,

   GET_BUFFER_SIZE_SRCS
};

enum surface_logical_srcs {
   /** Surface binding table index */
   SURFACE_LOGICAL_SRC_SURFACE,
   /** Surface bindless handle */
   SURFACE_LOGICAL_SRC_SURFACE_HANDLE,
   /** Surface address; could be multi-dimensional for typed opcodes */
   SURFACE_LOGICAL_SRC_ADDRESS,
   /** Data to be written or used in an atomic op */
   SURFACE_LOGICAL_SRC_DATA,
   /** Surface number of dimensions.  Affects the size of ADDRESS */
   SURFACE_LOGICAL_SRC_IMM_DIMS,
   /** Per-opcode immediate argument.  For atomics, this is the atomic opcode */
   SURFACE_LOGICAL_SRC_IMM_ARG,
   /**
    * Some instructions with side-effects should not be predicated on
    * sample mask, e.g. lowered stores to scratch.
    */
   SURFACE_LOGICAL_SRC_ALLOW_SAMPLE_MASK,

   SURFACE_LOGICAL_NUM_SRCS
};

enum a64_logical_srcs {
   /** Address the A64 message operates on */
   A64_LOGICAL_ADDRESS,
   /** Source for the operation (unused of LOAD ops) */
   A64_LOGICAL_SRC,
   /** Per-opcode immediate argument. Number of dwords, bit size, or atomic op. */
   A64_LOGICAL_ARG,
   /**
    * Some instructions do want to run on helper lanes (like ray queries).
    */
   A64_LOGICAL_ENABLE_HELPERS,

   A64_LOGICAL_NUM_SRCS
};

enum urb_logical_srcs {
   URB_LOGICAL_SRC_HANDLE,
   URB_LOGICAL_SRC_PER_SLOT_OFFSETS,
   URB_LOGICAL_SRC_CHANNEL_MASK,
   /** Data to be written.  BAD_FILE for reads. */
   URB_LOGICAL_SRC_DATA,
   URB_LOGICAL_SRC_COMPONENTS,
   URB_LOGICAL_NUM_SRCS
};

enum interpolator_logical_srcs {
   /** Interpolation offset */
   INTERP_SRC_OFFSET,
   /** Message data  */
   INTERP_SRC_MSG_DESC,
   /** Flag register for dynamic mode */
   INTERP_SRC_DYNAMIC_MODE,

   INTERP_NUM_SRCS
};


#ifdef __cplusplus
/**
 * Allow elk_urb_write_flags enums to be ORed together.
 */
inline elk_urb_write_flags
operator|(elk_urb_write_flags x, elk_urb_write_flags y)
{
   return static_cast<elk_urb_write_flags>(static_cast<int>(x) |
                                           static_cast<int>(y));
}
#endif

enum ENUM_PACKED elk_predicate {
   ELK_PREDICATE_NONE                =  0,
   ELK_PREDICATE_NORMAL              =  1,
   ELK_PREDICATE_ALIGN1_ANYV         =  2,
   ELK_PREDICATE_ALIGN1_ALLV         =  3,
   ELK_PREDICATE_ALIGN1_ANY2H        =  4,
   ELK_PREDICATE_ALIGN1_ALL2H        =  5,
   ELK_PREDICATE_ALIGN1_ANY4H        =  6,
   ELK_PREDICATE_ALIGN1_ALL4H        =  7,
   ELK_PREDICATE_ALIGN1_ANY8H        =  8,
   ELK_PREDICATE_ALIGN1_ALL8H        =  9,
   ELK_PREDICATE_ALIGN1_ANY16H       = 10,
   ELK_PREDICATE_ALIGN1_ALL16H       = 11,
   ELK_PREDICATE_ALIGN1_ANY32H       = 12,
   ELK_PREDICATE_ALIGN1_ALL32H       = 13,
   ELK_PREDICATE_ALIGN16_REPLICATE_X =  2,
   ELK_PREDICATE_ALIGN16_REPLICATE_Y =  3,
   ELK_PREDICATE_ALIGN16_REPLICATE_Z =  4,
   ELK_PREDICATE_ALIGN16_REPLICATE_W =  5,
   ELK_PREDICATE_ALIGN16_ANY4H       =  6,
   ELK_PREDICATE_ALIGN16_ALL4H       =  7,
   XE2_PREDICATE_ANY = 2,
   XE2_PREDICATE_ALL = 3
};

enum ENUM_PACKED elk_reg_file {
   ELK_ARCHITECTURE_REGISTER_FILE = 0,
   ELK_GENERAL_REGISTER_FILE      = 1,
   ELK_MESSAGE_REGISTER_FILE      = 2,
   ELK_IMMEDIATE_VALUE            = 3,

   ARF = ELK_ARCHITECTURE_REGISTER_FILE,
   FIXED_GRF = ELK_GENERAL_REGISTER_FILE,
   MRF = ELK_MESSAGE_REGISTER_FILE,
   IMM = ELK_IMMEDIATE_VALUE,

   /* These are not hardware values */
   VGRF,
   ATTR,
   UNIFORM, /* prog_data->params[reg] */
   BAD_FILE,
};

enum ENUM_PACKED gfx10_align1_3src_reg_file {
   ELK_ALIGN1_3SRC_GENERAL_REGISTER_FILE = 0,
   ELK_ALIGN1_3SRC_IMMEDIATE_VALUE       = 1, /* src0, src2 */
   ELK_ALIGN1_3SRC_ACCUMULATOR           = 1, /* dest, src1 */
};

/* CNL adds Align1 support for 3-src instructions. Bit 35 of the instruction
 * word is "Execution Datatype" which controls whether the instruction operates
 * on float or integer types. The register arguments have fields that offer
 * more fine control their respective types.
 */
enum ENUM_PACKED gfx10_align1_3src_exec_type {
   ELK_ALIGN1_3SRC_EXEC_TYPE_INT   = 0,
   ELK_ALIGN1_3SRC_EXEC_TYPE_FLOAT = 1,
};

#define ELK_ARF_NULL                  0x00
#define ELK_ARF_ADDRESS               0x10
#define ELK_ARF_ACCUMULATOR           0x20
#define ELK_ARF_FLAG                  0x30
#define ELK_ARF_MASK                  0x40
#define ELK_ARF_MASK_STACK            0x50
#define ELK_ARF_MASK_STACK_DEPTH      0x60
#define ELK_ARF_STATE                 0x70
#define ELK_ARF_CONTROL               0x80
#define ELK_ARF_NOTIFICATION_COUNT    0x90
#define ELK_ARF_IP                    0xA0
#define ELK_ARF_TDR                   0xB0
#define ELK_ARF_TIMESTAMP             0xC0

#define ELK_MRF_COMPR4			(1 << 7)

#define ELK_AMASK   0
#define ELK_IMASK   1
#define ELK_LMASK   2
#define ELK_CMASK   3



#define ELK_THREAD_NORMAL     0
#define ELK_THREAD_ATOMIC     1
#define ELK_THREAD_SWITCH     2

enum ENUM_PACKED elk_vertical_stride {
   ELK_VERTICAL_STRIDE_0               = 0,
   ELK_VERTICAL_STRIDE_1               = 1,
   ELK_VERTICAL_STRIDE_2               = 2,
   ELK_VERTICAL_STRIDE_4               = 3,
   ELK_VERTICAL_STRIDE_8               = 4,
   ELK_VERTICAL_STRIDE_16              = 5,
   ELK_VERTICAL_STRIDE_32              = 6,
   ELK_VERTICAL_STRIDE_ONE_DIMENSIONAL = 0xF,
};

enum ENUM_PACKED gfx10_align1_3src_vertical_stride {
   ELK_ALIGN1_3SRC_VERTICAL_STRIDE_0 = 0,
   ELK_ALIGN1_3SRC_VERTICAL_STRIDE_1 = 1,
   ELK_ALIGN1_3SRC_VERTICAL_STRIDE_2 = 1,
   ELK_ALIGN1_3SRC_VERTICAL_STRIDE_4 = 2,
   ELK_ALIGN1_3SRC_VERTICAL_STRIDE_8 = 3,
};

enum ENUM_PACKED elk_width {
   ELK_WIDTH_1  = 0,
   ELK_WIDTH_2  = 1,
   ELK_WIDTH_4  = 2,
   ELK_WIDTH_8  = 3,
   ELK_WIDTH_16 = 4,
};

/**
 * Message target: Shared Function ID for where to SEND a message.
 *
 * These are enumerated in the ISA reference under "send - Send Message".
 * In particular, see the following tables:
 * - G45 PRM, Volume 4, Table 14-15 "Message Descriptor Definition"
 * - Sandybridge PRM, Volume 4 Part 2, Table 8-16 "Extended Message Descriptor"
 * - Ivybridge PRM, Volume 1 Part 1, section 3.2.7 "GPE Function IDs"
 */
enum elk_message_target {
   ELK_SFID_NULL                     = 0,
   ELK_SFID_MATH                     = 1, /* Only valid on Gfx4-5 */
   ELK_SFID_SAMPLER                  = 2,
   ELK_SFID_MESSAGE_GATEWAY          = 3,
   ELK_SFID_DATAPORT_READ            = 4,
   ELK_SFID_DATAPORT_WRITE           = 5,
   ELK_SFID_URB                      = 6,
   ELK_SFID_THREAD_SPAWNER           = 7,
   ELK_SFID_VME                      = 8,

   GFX6_SFID_DATAPORT_SAMPLER_CACHE  = 4,
   GFX6_SFID_DATAPORT_RENDER_CACHE   = 5,
   GFX6_SFID_DATAPORT_CONSTANT_CACHE = 9,

   GFX7_SFID_DATAPORT_DATA_CACHE     = 10,
   GFX7_SFID_PIXEL_INTERPOLATOR      = 11,
   HSW_SFID_DATAPORT_DATA_CACHE_1    = 12,
   HSW_SFID_CRE                      = 13,
};

#define GFX7_MESSAGE_TARGET_DP_DATA_CACHE     10

#define ELK_SAMPLER_RETURN_FORMAT_FLOAT32     0
#define ELK_SAMPLER_RETURN_FORMAT_UINT32      2
#define ELK_SAMPLER_RETURN_FORMAT_SINT32      3

#define GFX8_SAMPLER_RETURN_FORMAT_32BITS    0
#define GFX8_SAMPLER_RETURN_FORMAT_16BITS    1

#define ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE              0
#define ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE             0
#define ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_BIAS        0
#define ELK_SAMPLER_MESSAGE_SIMD8_KILLPIX             1
#define ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD        1
#define ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_LOD         1
#define ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_GRADIENTS  2
#define ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_GRADIENTS    2
#define ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_COMPARE    0
#define ELK_SAMPLER_MESSAGE_SIMD16_SAMPLE_COMPARE     2
#define ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_BIAS_COMPARE 0
#define ELK_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD_COMPARE 1
#define ELK_SAMPLER_MESSAGE_SIMD8_SAMPLE_LOD_COMPARE  1
#define ELK_SAMPLER_MESSAGE_SIMD4X2_RESINFO           2
#define ELK_SAMPLER_MESSAGE_SIMD16_RESINFO            2
#define ELK_SAMPLER_MESSAGE_SIMD4X2_LD                3
#define ELK_SAMPLER_MESSAGE_SIMD8_LD                  3
#define ELK_SAMPLER_MESSAGE_SIMD16_LD                 3

#define GFX5_SAMPLER_MESSAGE_SAMPLE              0
#define GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS         1
#define GFX5_SAMPLER_MESSAGE_SAMPLE_LOD          2
#define GFX5_SAMPLER_MESSAGE_SAMPLE_COMPARE      3
#define GFX5_SAMPLER_MESSAGE_SAMPLE_DERIVS       4
#define GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE 5
#define GFX5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE  6
#define GFX5_SAMPLER_MESSAGE_SAMPLE_LD           7
#define GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4      8
#define GFX5_SAMPLER_MESSAGE_LOD                 9
#define GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO      10
#define GFX6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO   11
#define GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C    16
#define GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO   17
#define GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C 18
#define HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE 20
#define GFX7_SAMPLER_MESSAGE_SAMPLE_LD_MCS       29
#define GFX7_SAMPLER_MESSAGE_SAMPLE_LD2DMS       30
#define GFX7_SAMPLER_MESSAGE_SAMPLE_LD2DSS       31

/* for GFX5 only */
#define ELK_SAMPLER_SIMD_MODE_SIMD4X2                   0
#define ELK_SAMPLER_SIMD_MODE_SIMD8                     1
#define ELK_SAMPLER_SIMD_MODE_SIMD16                    2
#define ELK_SAMPLER_SIMD_MODE_SIMD32_64                 3

#define ELK_DATAPORT_OWORD_BLOCK_1_OWORDLOW   0
#define ELK_DATAPORT_OWORD_BLOCK_1_OWORDHIGH  1
#define ELK_DATAPORT_OWORD_BLOCK_2_OWORDS     2
#define ELK_DATAPORT_OWORD_BLOCK_4_OWORDS     3
#define ELK_DATAPORT_OWORD_BLOCK_8_OWORDS     4
#define ELK_DATAPORT_OWORD_BLOCK_OWORDS(n)              \
   ((n) == 1 ? ELK_DATAPORT_OWORD_BLOCK_1_OWORDLOW :    \
    (n) == 2 ? ELK_DATAPORT_OWORD_BLOCK_2_OWORDS :      \
    (n) == 4 ? ELK_DATAPORT_OWORD_BLOCK_4_OWORDS :      \
    (n) == 8 ? ELK_DATAPORT_OWORD_BLOCK_8_OWORDS :      \
    (abort(), ~0))
#define ELK_DATAPORT_OWORD_BLOCK_DWORDS(n)              \
   ((n) == 4 ? ELK_DATAPORT_OWORD_BLOCK_1_OWORDLOW :    \
    (n) == 8 ? ELK_DATAPORT_OWORD_BLOCK_2_OWORDS :      \
    (n) == 16 ? ELK_DATAPORT_OWORD_BLOCK_4_OWORDS :     \
    (n) == 32 ? ELK_DATAPORT_OWORD_BLOCK_8_OWORDS :     \
    (abort(), ~0))

#define ELK_DATAPORT_OWORD_DUAL_BLOCK_1OWORD     0
#define ELK_DATAPORT_OWORD_DUAL_BLOCK_4OWORDS    2

#define ELK_DATAPORT_DWORD_SCATTERED_BLOCK_8DWORDS   2
#define ELK_DATAPORT_DWORD_SCATTERED_BLOCK_16DWORDS  3

/* This one stays the same across generations. */
#define ELK_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ          0
/* GFX4 */
#define ELK_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     1
#define ELK_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          2
#define ELK_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      3
/* G45, GFX5 */
#define G45_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ	    1
#define G45_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     2
#define G45_DATAPORT_READ_MESSAGE_AVC_LOOP_FILTER_READ	    3
#define G45_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          4
#define G45_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      6
/* GFX6 */
#define GFX6_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ	    1
#define GFX6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     2
#define GFX6_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          4
#define GFX6_DATAPORT_READ_MESSAGE_OWORD_UNALIGN_BLOCK_READ  5
#define GFX6_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      6

#define ELK_DATAPORT_READ_TARGET_DATA_CACHE      0
#define ELK_DATAPORT_READ_TARGET_RENDER_CACHE    1
#define ELK_DATAPORT_READ_TARGET_SAMPLER_CACHE   2

#define ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE                0
#define ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED     1
#define ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN01         2
#define ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN23         3
#define ELK_DATAPORT_RENDER_TARGET_WRITE_SIMD8_SINGLE_SOURCE_SUBSPAN01       4

#define ELK_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE                0
#define ELK_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE           1
#define ELK_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE                2
#define ELK_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE            3
#define ELK_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE              4
#define ELK_DATAPORT_WRITE_MESSAGE_STREAMED_VERTEX_BUFFER_WRITE     5
#define ELK_DATAPORT_WRITE_MESSAGE_FLUSH_RENDER_CACHE               7

/* GFX6 */
#define GFX6_DATAPORT_WRITE_MESSAGE_DWORD_ATOMIC_WRITE              7
#define GFX6_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE               8
#define GFX6_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE          9
#define GFX6_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE               10
#define GFX6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE           11
#define GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE             12
#define GFX6_DATAPORT_WRITE_MESSAGE_STREAMED_VB_WRITE               13
#define GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_UNORM_WRITE       14

/* GFX7 */
#define GFX7_DATAPORT_RC_MEDIA_BLOCK_READ                           4
#define GFX7_DATAPORT_RC_TYPED_SURFACE_READ                         5
#define GFX7_DATAPORT_RC_TYPED_ATOMIC_OP                            6
#define GFX7_DATAPORT_RC_MEMORY_FENCE                               7
#define GFX7_DATAPORT_RC_MEDIA_BLOCK_WRITE                          10
#define GFX7_DATAPORT_RC_RENDER_TARGET_WRITE                        12
#define GFX7_DATAPORT_RC_TYPED_SURFACE_WRITE                        13
#define GFX7_DATAPORT_DC_OWORD_BLOCK_READ                           0
#define GFX7_DATAPORT_DC_UNALIGNED_OWORD_BLOCK_READ                 1
#define GFX7_DATAPORT_DC_OWORD_DUAL_BLOCK_READ                      2
#define GFX7_DATAPORT_DC_DWORD_SCATTERED_READ                       3
#define GFX7_DATAPORT_DC_BYTE_SCATTERED_READ                        4
#define GFX7_DATAPORT_DC_UNTYPED_SURFACE_READ                       5
#define GFX7_DATAPORT_DC_UNTYPED_ATOMIC_OP                          6
#define GFX7_DATAPORT_DC_MEMORY_FENCE                               7
#define GFX7_DATAPORT_DC_OWORD_BLOCK_WRITE                          8
#define GFX7_DATAPORT_DC_OWORD_DUAL_BLOCK_WRITE                     10
#define GFX7_DATAPORT_DC_DWORD_SCATTERED_WRITE                      11
#define GFX7_DATAPORT_DC_BYTE_SCATTERED_WRITE                       12
#define GFX7_DATAPORT_DC_UNTYPED_SURFACE_WRITE                      13

#define GFX7_DATAPORT_SCRATCH_READ                            ((1 << 18) | \
                                                               (0 << 17))
#define GFX7_DATAPORT_SCRATCH_WRITE                           ((1 << 18) | \
                                                               (1 << 17))
#define GFX7_DATAPORT_SCRATCH_NUM_REGS_SHIFT                        12

#define GFX7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET     0
#define GFX7_PIXEL_INTERPOLATOR_LOC_SAMPLE            1
#define GFX7_PIXEL_INTERPOLATOR_LOC_CENTROID          2
#define GFX7_PIXEL_INTERPOLATOR_LOC_PER_SLOT_OFFSET   3

/* HSW */
#define HSW_DATAPORT_DC_PORT0_OWORD_BLOCK_READ                      0
#define HSW_DATAPORT_DC_PORT0_UNALIGNED_OWORD_BLOCK_READ            1
#define HSW_DATAPORT_DC_PORT0_OWORD_DUAL_BLOCK_READ                 2
#define HSW_DATAPORT_DC_PORT0_DWORD_SCATTERED_READ                  3
#define HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_READ                   4
#define HSW_DATAPORT_DC_PORT0_MEMORY_FENCE                          7
#define HSW_DATAPORT_DC_PORT0_OWORD_BLOCK_WRITE                     8
#define HSW_DATAPORT_DC_PORT0_OWORD_DUAL_BLOCK_WRITE                10
#define HSW_DATAPORT_DC_PORT0_DWORD_SCATTERED_WRITE                 11
#define HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_WRITE                  12

#define HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ                  1
#define HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP                     2
#define HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2             3
#define HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_READ                      4
#define HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ                    5
#define HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP                       6
#define HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2               7
#define HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE                 9
#define HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_WRITE                     10
#define HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP                     11
#define HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2             12
#define HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE                   13
#define GFX9_DATAPORT_DC_PORT1_A64_SCATTERED_READ                   0x10
#define GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_READ             0x11
#define GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_OP                0x12
#define GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_READ                 0x14
#define GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_WRITE                0x15
#define GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_WRITE            0x19
#define GFX8_DATAPORT_DC_PORT1_A64_SCATTERED_WRITE                  0x1a

/* A64 scattered message subtype */
#define GFX8_A64_SCATTERED_SUBTYPE_BYTE                             0
#define GFX8_A64_SCATTERED_SUBTYPE_DWORD                            1
#define GFX8_A64_SCATTERED_SUBTYPE_QWORD                            2
#define GFX8_A64_SCATTERED_SUBTYPE_HWORD                            3

/* Dataport special binding table indices: */
#define ELK_BTI_STATELESS                255
#define GFX7_BTI_SLM                     254

#define HSW_BTI_STATELESS_LOCALLY_COHERENT 255
#define HSW_BTI_STATELESS_NON_COHERENT 253
#define HSW_BTI_STATELESS_GLOBALLY_COHERENT 252
#define HSW_BTI_STATELESS_LLC_COHERENT 251
#define HSW_BTI_STATELESS_L3_UNCACHED 250

/* The hardware docs are a bit contradictory here.  On Haswell, where they
 * first added cache ability control, there were 5 different cache modes (see
 * HSW_BTI_STATELESS_* above).  On Broadwell, they reduced to two:
 *
 *  - IA-Coherent (BTI=255): Coherent within Gen and coherent within the
 *    entire IA cache memory hierarchy.
 *
 *  - Non-Coherent (BTI=253): Coherent within Gen, same cache type.
 *
 * Information about stateless cache coherency can be found in the "A32
 * Stateless" section of the "3D Media GPGPU" volume of the PRM for each
 * hardware generation.
 *
 * Unfortunately, the docs for MDC_STATELESS appear to have been copied and
 * pasted from Haswell and give the Haswell definitions for the BTI values of
 * 255 and 253 including a warning about accessing 253 surfaces from multiple
 * threads.  This seems to be a copy+paste error and the definitions from the
 * "A32 Stateless" section should be trusted instead.
 *
 * Note that because the DRM sets bit 4 of HDC_CHICKEN0 on BDW, CHV and at
 * least some pre-production steppings of SKL due to WaForceEnableNonCoherent,
 * HDC memory access may have been overridden by the kernel to be non-coherent
 * (matching the behavior of the same BTI on pre-Gfx8 hardware) and BTI 255
 * may actually be an alias for BTI 253.
 */
#define GFX8_BTI_STATELESS_IA_COHERENT   255
#define GFX8_BTI_STATELESS_NON_COHERENT  253

/* Dataport atomic operations for Untyped Atomic Integer Operation message
 * (and others).
 */
#define ELK_AOP_AND                   1
#define ELK_AOP_OR                    2
#define ELK_AOP_XOR                   3
#define ELK_AOP_MOV                   4
#define ELK_AOP_INC                   5
#define ELK_AOP_DEC                   6
#define ELK_AOP_ADD                   7
#define ELK_AOP_SUB                   8
#define ELK_AOP_REVSUB                9
#define ELK_AOP_IMAX                  10
#define ELK_AOP_IMIN                  11
#define ELK_AOP_UMAX                  12
#define ELK_AOP_UMIN                  13
#define ELK_AOP_CMPWR                 14
#define ELK_AOP_PREDEC                15

/* Dataport atomic operations for Untyped Atomic Float Operation message. */
#define ELK_AOP_FMAX                  1
#define ELK_AOP_FMIN                  2
#define ELK_AOP_FCMPWR                3
#define ELK_AOP_FADD                  4

#define ELK_MATH_FUNCTION_INV                              1
#define ELK_MATH_FUNCTION_LOG                              2
#define ELK_MATH_FUNCTION_EXP                              3
#define ELK_MATH_FUNCTION_SQRT                             4
#define ELK_MATH_FUNCTION_RSQ                              5
#define ELK_MATH_FUNCTION_SIN                              6
#define ELK_MATH_FUNCTION_COS                              7
#define ELK_MATH_FUNCTION_SINCOS                           8 /* gfx4, gfx5 */
#define ELK_MATH_FUNCTION_FDIV                             9 /* gfx6+ */
#define ELK_MATH_FUNCTION_POW                              10
#define ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER   11
#define ELK_MATH_FUNCTION_INT_DIV_QUOTIENT                 12
#define ELK_MATH_FUNCTION_INT_DIV_REMAINDER                13
#define GFX8_MATH_FUNCTION_INVM                            14
#define GFX8_MATH_FUNCTION_RSQRTM                          15

#define ELK_MATH_INTEGER_UNSIGNED     0
#define ELK_MATH_INTEGER_SIGNED       1

#define ELK_MATH_PRECISION_FULL        0
#define ELK_MATH_PRECISION_PARTIAL     1

#define ELK_MATH_SATURATE_NONE         0
#define ELK_MATH_SATURATE_SATURATE     1

#define ELK_MATH_DATA_VECTOR  0
#define ELK_MATH_DATA_SCALAR  1

#define ELK_URB_OPCODE_WRITE_HWORD  0
#define ELK_URB_OPCODE_WRITE_OWORD  1
#define ELK_URB_OPCODE_READ_HWORD   2
#define ELK_URB_OPCODE_READ_OWORD   3
#define GFX7_URB_OPCODE_ATOMIC_MOV  4
#define GFX7_URB_OPCODE_ATOMIC_INC  5
#define GFX8_URB_OPCODE_ATOMIC_ADD  6
#define GFX8_URB_OPCODE_SIMD8_WRITE 7
#define GFX8_URB_OPCODE_SIMD8_READ  8

#define ELK_URB_SWIZZLE_NONE          0
#define ELK_URB_SWIZZLE_INTERLEAVE    1
#define ELK_URB_SWIZZLE_TRANSPOSE     2

#define ELK_SCRATCH_SPACE_SIZE_1K     0
#define ELK_SCRATCH_SPACE_SIZE_2K     1
#define ELK_SCRATCH_SPACE_SIZE_4K     2
#define ELK_SCRATCH_SPACE_SIZE_8K     3
#define ELK_SCRATCH_SPACE_SIZE_16K    4
#define ELK_SCRATCH_SPACE_SIZE_32K    5
#define ELK_SCRATCH_SPACE_SIZE_64K    6
#define ELK_SCRATCH_SPACE_SIZE_128K   7
#define ELK_SCRATCH_SPACE_SIZE_256K   8
#define ELK_SCRATCH_SPACE_SIZE_512K   9
#define ELK_SCRATCH_SPACE_SIZE_1M     10
#define ELK_SCRATCH_SPACE_SIZE_2M     11

#define ELK_MESSAGE_GATEWAY_SFID_OPEN_GATEWAY         0
#define ELK_MESSAGE_GATEWAY_SFID_CLOSE_GATEWAY        1
#define ELK_MESSAGE_GATEWAY_SFID_FORWARD_MSG          2
#define ELK_MESSAGE_GATEWAY_SFID_GET_TIMESTAMP        3
#define ELK_MESSAGE_GATEWAY_SFID_BARRIER_MSG          4
#define ELK_MESSAGE_GATEWAY_SFID_UPDATE_GATEWAY_STATE 5
#define ELK_MESSAGE_GATEWAY_SFID_MMIO_READ_WRITE      6


/* Gfx7 "GS URB Entry Allocation Size" is a U9-1 field, so the maximum gs_size
 * is 2^9, or 512.  It's counted in multiples of 64 bytes.
 *
 * Identical for VS, DS, and HS.
 */
#define GFX7_MAX_GS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GFX7_MAX_DS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GFX7_MAX_HS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GFX7_MAX_VS_URB_ENTRY_SIZE_BYTES                (512*64)

#define ELK_GS_EDGE_INDICATOR_0			(1 << 8)
#define ELK_GS_EDGE_INDICATOR_1			(1 << 9)

/* Gfx6 "GS URB Entry Allocation Size" is defined as a number of 1024-bit
 * (128 bytes) URB rows and the maximum allowed value is 5 rows.
 */
#define GFX6_MAX_GS_URB_ENTRY_SIZE_BYTES                (5*128)

/* GS Thread Payload
 */

/* 3DSTATE_GS "Output Vertex Size" has an effective maximum of 62. It's
 * counted in multiples of 16 bytes.
 */
#define GFX7_MAX_GS_OUTPUT_VERTEX_SIZE_BYTES            (62*16)


/* R0 */
# define GFX7_GS_PAYLOAD_INSTANCE_ID_SHIFT		27

/* CR0.0[5:4] Floating-Point Rounding Modes
 *  Skylake PRM, Volume 7 Part 1, "Control Register", page 756
 */

#define ELK_CR0_RND_MODE_MASK     0x30
#define ELK_CR0_RND_MODE_SHIFT    4

enum ENUM_PACKED elk_rnd_mode {
   ELK_RND_MODE_RTNE = 0,  /* Round to Nearest or Even */
   ELK_RND_MODE_RU = 1,    /* Round Up, toward +inf */
   ELK_RND_MODE_RD = 2,    /* Round Down, toward -inf */
   ELK_RND_MODE_RTZ = 3,   /* Round Toward Zero */
   ELK_RND_MODE_UNSPECIFIED,  /* Unspecified rounding mode */
};

#define ELK_CR0_FP64_DENORM_PRESERVE (1 << 6)
#define ELK_CR0_FP32_DENORM_PRESERVE (1 << 7)
#define ELK_CR0_FP16_DENORM_PRESERVE (1 << 10)

#define ELK_CR0_FP_MODE_MASK (ELK_CR0_FP64_DENORM_PRESERVE | \
                              ELK_CR0_FP32_DENORM_PRESERVE | \
                              ELK_CR0_FP16_DENORM_PRESERVE | \
                              ELK_CR0_RND_MODE_MASK)

/* MDC_DS - Data Size Message Descriptor Control Field
 * Skylake PRM, Volume 2d, page 129
 *
 * Specifies the number of Bytes to be read or written per Dword used at
 * byte_scattered read/write and byte_scaled read/write messages.
 */
#define GFX7_BYTE_SCATTERED_DATA_ELEMENT_BYTE     0
#define GFX7_BYTE_SCATTERED_DATA_ELEMENT_WORD     1
#define GFX7_BYTE_SCATTERED_DATA_ELEMENT_DWORD    2

/* Starting with Xe-HPG, the old dataport was massively reworked dataport.
 * The new thing, called Load/Store Cache or LSC, has a significantly improved
 * interface.  Instead of bespoke messages for every case, there's basically
 * one or two messages with different bits to control things like address
 * size, how much data is read/written, etc.  It's way nicer but also means we
 * get to rewrite all our dataport encoding/decoding code.  This patch kicks
 * off the party with all of the new enums.
 */
enum elk_lsc_opcode {
   LSC_OP_LOAD            = 0,
   LSC_OP_LOAD_CMASK      = 2,
   LSC_OP_STORE           = 4,
   LSC_OP_STORE_CMASK     = 6,
   LSC_OP_ATOMIC_INC      = 8,
   LSC_OP_ATOMIC_DEC      = 9,
   LSC_OP_ATOMIC_LOAD     = 10,
   LSC_OP_ATOMIC_STORE    = 11,
   LSC_OP_ATOMIC_ADD      = 12,
   LSC_OP_ATOMIC_SUB      = 13,
   LSC_OP_ATOMIC_MIN      = 14,
   LSC_OP_ATOMIC_MAX      = 15,
   LSC_OP_ATOMIC_UMIN     = 16,
   LSC_OP_ATOMIC_UMAX     = 17,
   LSC_OP_ATOMIC_CMPXCHG  = 18,
   LSC_OP_ATOMIC_FADD     = 19,
   LSC_OP_ATOMIC_FSUB     = 20,
   LSC_OP_ATOMIC_FMIN     = 21,
   LSC_OP_ATOMIC_FMAX     = 22,
   LSC_OP_ATOMIC_FCMPXCHG = 23,
   LSC_OP_ATOMIC_AND      = 24,
   LSC_OP_ATOMIC_OR       = 25,
   LSC_OP_ATOMIC_XOR      = 26,
   LSC_OP_FENCE           = 31
};

/*
 * Specifies the size of the dataport address payload in registers.
 */
enum ENUM_PACKED lsc_addr_reg_size {
   LSC_ADDR_REG_SIZE_1  = 1,
   LSC_ADDR_REG_SIZE_2  = 2,
   LSC_ADDR_REG_SIZE_3  = 3,
   LSC_ADDR_REG_SIZE_4  = 4,
   LSC_ADDR_REG_SIZE_6  = 6,
   LSC_ADDR_REG_SIZE_8  = 8,
};

/*
 * Specifies the size of the address payload item in a dataport message.
 */
enum ENUM_PACKED lsc_addr_size {
  LSC_ADDR_SIZE_A16 = 1,    /* 16-bit address offset */
  LSC_ADDR_SIZE_A32 = 2,    /* 32-bit address offset */
  LSC_ADDR_SIZE_A64 = 3,    /* 64-bit address offset */
};

/*
 * Specifies the type of the address payload item in a dataport message. The
 * address type specifies how the dataport message decodes the Extended
 * Descriptor for the surface attributes and address calculation.
 */
enum ENUM_PACKED lsc_addr_surface_type {
   LSC_ADDR_SURFTYPE_FLAT = 0, /* Flat */
   LSC_ADDR_SURFTYPE_BSS = 1,  /* Bindless surface state */
   LSC_ADDR_SURFTYPE_SS = 2,   /* Surface state */
   LSC_ADDR_SURFTYPE_BTI = 3,  /* Binding table index */
};

/*
 * Specifies the dataport message override to the default L1 and L3 memory
 * cache policies. Dataport L1 cache policies are uncached (UC), cached (C),
 * cache streaming (S) and invalidate-after-read (IAR). Dataport L3 cache
 * policies are uncached (UC) and cached (C).
 */
enum lsc_cache_load {
   /* No override. Use the non-pipelined state or surface state cache settings
    * for L1 and L3.
    */
   LSC_CACHE_LOAD_L1STATE_L3MOCS = 0,
   /* Override to L1 uncached and L3 uncached */
   LSC_CACHE_LOAD_L1UC_L3UC      = 1,
   /* Override to L1 uncached and L3 cached */
   LSC_CACHE_LOAD_L1UC_L3C       = 2,
   /* Override to L1 cached and L3 uncached */
   LSC_CACHE_LOAD_L1C_L3UC       = 3,
   /* Override to cache at both L1 and L3 */
   LSC_CACHE_LOAD_L1C_L3C        = 4,
   /* Override to L1 streaming load and L3 uncached */
   LSC_CACHE_LOAD_L1S_L3UC       = 5,
   /* Override to L1 streaming load and L3 cached */
   LSC_CACHE_LOAD_L1S_L3C        = 6,
   /* For load messages, override to L1 invalidate-after-read, and L3 cached. */
   LSC_CACHE_LOAD_L1IAR_L3C      = 7,
};

/*
 * Specifies the dataport message override to the default L1 and L3 memory
 * cache policies. Dataport L1 cache policies are uncached (UC), cached (C),
 * streaming (S) and invalidate-after-read (IAR). Dataport L3 cache policies
 * are uncached (UC), cached (C), cached-as-a-constand (CC) and
 * invalidate-after-read (IAR).
 */
enum PACKED xe2_lsc_cache_load {
   /* No override. Use the non-pipelined or surface state cache settings for L1
    * and L3.
    */
   XE2_LSC_CACHE_LOAD_L1STATE_L3MOCS = 0,
   /* Override to L1 uncached and L3 uncached */
   XE2_LSC_CACHE_LOAD_L1UC_L3UC = 2,
   /* Override to L1 uncached and L3 cached */
   XE2_LSC_CACHE_LOAD_L1UC_L3C = 4,
   /* Override to L1 uncached and L3 cached as a constant */
   XE2_LSC_CACHE_LOAD_L1UC_L3CC = 5,
   /* Override to L1 cached and L3 uncached */
   XE2_LSC_CACHE_LOAD_L1C_L3UC = 6,
   /* Override to L1 cached and L3 cached */
   XE2_LSC_CACHE_LOAD_L1C_L3C = 8,
   /* Override to L1 cached and L3 cached as a constant */
   XE2_LSC_CACHE_LOAD_L1C_L3CC = 9,
   /* Override to L1 cached as streaming load and L3 uncached */
   XE2_LSC_CACHE_LOAD_L1S_L3UC = 10,
   /* Override to L1 cached as streaming load and L3 cached */
   XE2_LSC_CACHE_LOAD_L1S_L3C = 12,
   /* Override to L1 and L3 invalidate after read */
   XE2_LSC_CACHE_LOAD_L1IAR_L3IAR = 14,

};

/*
 * Specifies the dataport message override to the default L1 and L3 memory
 * cache policies. Dataport L1 cache policies are uncached (UC), write-through
 * (WT), write-back (WB) and streaming (S). Dataport L3 cache policies are
 * uncached (UC) and cached (WB).
 */
enum ENUM_PACKED lsc_cache_store {
   /* No override. Use the non-pipelined or surface state cache settings for L1
    * and L3.
    */
   LSC_CACHE_STORE_L1STATE_L3MOCS = 0,
   /* Override to L1 uncached and L3 uncached */
   LSC_CACHE_STORE_L1UC_L3UC = 1,
   /* Override to L1 uncached and L3 cached */
   LSC_CACHE_STORE_L1UC_L3WB = 2,
   /* Override to L1 write-through and L3 uncached */
   LSC_CACHE_STORE_L1WT_L3UC = 3,
   /* Override to L1 write-through and L3 cached */
   LSC_CACHE_STORE_L1WT_L3WB = 4,
   /* Override to L1 streaming and L3 uncached */
   LSC_CACHE_STORE_L1S_L3UC = 5,
   /* Override to L1 streaming and L3 cached */
   LSC_CACHE_STORE_L1S_L3WB = 6,
   /* Override to L1 write-back, and L3 cached */
   LSC_CACHE_STORE_L1WB_L3WB = 7,

};

/*
 * Specifies the dataport message override to the default L1 and L3 memory
 * cache policies. Dataport L1 cache policies are uncached (UC), write-through
 * (WT), write-back (WB) and streaming (S). Dataport L3 cache policies are
 * uncached (UC) and cached (WB).
 */
enum PACKED xe2_lsc_cache_store {
   /* No override. Use the non-pipelined or surface state cache settings for L1
    * and L3.
    */
   XE2_LSC_CACHE_STORE_L1STATE_L3MOCS = 0,
   /* Override to L1 uncached and L3 uncached */
   XE2_LSC_CACHE_STORE_L1UC_L3UC = 2,
   /* Override to L1 uncached and L3 cached */
   XE2_LSC_CACHE_STORE_L1UC_L3WB = 4,
   /* Override to L1 write-through and L3 uncached */
   XE2_LSC_CACHE_STORE_L1WT_L3UC = 6,
   /* Override to L1 write-through and L3 cached */
   XE2_LSC_CACHE_STORE_L1WT_L3WB = 8,
   /* Override to L1 streaming and L3 uncached */
   XE2_LSC_CACHE_STORE_L1S_L3UC = 10,
   /* Override to L1 streaming and L3 cached */
   XE2_LSC_CACHE_STORE_L1S_L3WB = 12,
   /* Override to L1 write-back and L3 cached */
   XE2_LSC_CACHE_STORE_L1WB_L3WB = 14,

};

#define LSC_CACHE(devinfo, l_or_s, cc)                                  \
   ((devinfo)->ver < 20 ? (unsigned)LSC_CACHE_ ## l_or_s ## _ ## cc :   \
                          (unsigned)XE2_LSC_CACHE_ ## l_or_s ## _ ## cc)

/*
 * Specifies which components of the data payload 4-element vector (X,Y,Z,W) is
 * packed into the register payload.
 */
enum ENUM_PACKED lsc_cmask {
   LSC_CMASK_X = 0x1,
   LSC_CMASK_Y = 0x2,
   LSC_CMASK_XY = 0x3,
   LSC_CMASK_Z = 0x4,
   LSC_CMASK_XZ = 0x5,
   LSC_CMASK_YZ = 0x6,
   LSC_CMASK_XYZ = 0x7,
   LSC_CMASK_W = 0x8,
   LSC_CMASK_XW = 0x9,
   LSC_CMASK_YW = 0xa,
   LSC_CMASK_XYW = 0xb,
   LSC_CMASK_ZW = 0xc,
   LSC_CMASK_XZW = 0xd,
   LSC_CMASK_YZW = 0xe,
   LSC_CMASK_XYZW = 0xf,
};

/*
 * Specifies the size of the data payload item in a dataport message.
 */
enum ENUM_PACKED lsc_data_size {
   /* 8-bit scalar data value in memory, packed into a 8-bit data value in
    * register.
    */
   LSC_DATA_SIZE_D8 = 0,
   /* 16-bit scalar data value in memory, packed into a 16-bit data value in
    * register.
    */
   LSC_DATA_SIZE_D16 = 1,
   /* 32-bit scalar data value in memory, packed into 32-bit data value in
    * register.
    */
   LSC_DATA_SIZE_D32 = 2,
   /* 64-bit scalar data value in memory, packed into 64-bit data value in
    * register.
    */
   LSC_DATA_SIZE_D64 = 3,
   /* 8-bit scalar data value in memory, packed into 32-bit unsigned data value
    * in register.
    */
   LSC_DATA_SIZE_D8U32 = 4,
   /* 16-bit scalar data value in memory, packed into 32-bit unsigned data
    * value in register.
    */
   LSC_DATA_SIZE_D16U32 = 5,
   /* 16-bit scalar BigFloat data value in memory, packed into 32-bit float
    * value in register.
    */
   LSC_DATA_SIZE_D16BF32 = 6,
};

/*
 *  Enum specifies the scope of the fence.
 */
enum ENUM_PACKED lsc_fence_scope {
   /* Wait until all previous memory transactions from this thread are observed
    * within the local thread-group.
    */
   LSC_FENCE_THREADGROUP = 0,
   /* Wait until all previous memory transactions from this thread are observed
    * within the local sub-slice.
    */
   LSC_FENCE_LOCAL = 1,
   /* Wait until all previous memory transactions from this thread are observed
    * in the local tile.
    */
   LSC_FENCE_TILE = 2,
   /* Wait until all previous memory transactions from this thread are observed
    * in the local GPU.
    */
   LSC_FENCE_GPU = 3,
   /* Wait until all previous memory transactions from this thread are observed
    * across all GPUs in the system.
    */
   LSC_FENCE_ALL_GPU = 4,
   /* Wait until all previous memory transactions from this thread are observed
    * at the "system" level.
    */
   LSC_FENCE_SYSTEM_RELEASE = 5,
   /* For GPUs that do not follow PCIe Write ordering for downstream writes
    * targeting device memory, a fence message with scope=System_Acquire will
    * commit to device memory all downstream and peer writes that have reached
    * the device.
    */
   LSC_FENCE_SYSTEM_ACQUIRE = 6,
};

/*
 * Specifies the type of cache flush operation to perform after a fence is
 * complete.
 */
enum ENUM_PACKED lsc_flush_type {
   LSC_FLUSH_TYPE_NONE = 0,
   /*
    * For a R/W cache, evict dirty lines (M to I state) and invalidate clean
    * lines. For a RO cache, invalidate clean lines.
    */
   LSC_FLUSH_TYPE_EVICT = 1,
   /*
    * For both R/W and RO cache, invalidate clean lines in the cache.
    */
   LSC_FLUSH_TYPE_INVALIDATE = 2,
   /*
    * For a R/W cache, invalidate dirty lines (M to I state), without
    * write-back to next level. This opcode does nothing for a RO cache.
    */
   LSC_FLUSH_TYPE_DISCARD = 3,
   /*
    * For a R/W cache, write-back dirty lines to the next level, but kept in
    * the cache as "clean" (M to V state). This opcode does nothing for a RO
    * cache.
    */
   LSC_FLUSH_TYPE_CLEAN = 4,
   /*
    * Flush "RW" section of the L3 cache, but leave L1 and L2 caches untouched.
    */
   LSC_FLUSH_TYPE_L3ONLY = 5,
   /*
    * HW maps this flush type internally to NONE.
    */
   LSC_FLUSH_TYPE_NONE_6 = 6,

};

enum ENUM_PACKED lsc_backup_fence_routing {
   /* Normal routing: UGM fence is routed to UGM pipeline. */
   LSC_NORMAL_ROUTING,
   /* Route UGM fence to LSC unit. */
   LSC_ROUTE_TO_LSC,
};

/*
 * Specifies the size of the vector in a dataport message.
 */
enum ENUM_PACKED lsc_vect_size {
   LSC_VECT_SIZE_V1 = 0,    /* vector length 1 */
   LSC_VECT_SIZE_V2 = 1,    /* vector length 2 */
   LSC_VECT_SIZE_V3 = 2,    /* Vector length 3 */
   LSC_VECT_SIZE_V4 = 3,    /* Vector length 4 */
   LSC_VECT_SIZE_V8 = 4,    /* Vector length 8 */
   LSC_VECT_SIZE_V16 = 5,   /* Vector length 16 */
   LSC_VECT_SIZE_V32 = 6,   /* Vector length 32 */
   LSC_VECT_SIZE_V64 = 7,   /* Vector length 64 */
};

#define LSC_ONE_ADDR_REG   1

#endif /* ELK_EU_DEFINES_H */
