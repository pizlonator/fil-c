/*
 * Copyright © 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef ELK_EU_OPCODES_H
#define ELK_EU_OPCODES_H

#ifdef __cplusplus
extern "C" {
#endif

enum elk_opcode {
   /* These are the actual hardware instructions. */
   ELK_OPCODE_ILLEGAL,
   ELK_OPCODE_MOV,
   ELK_OPCODE_SEL,
   ELK_OPCODE_MOVI, /**< G45+ */
   ELK_OPCODE_NOT,
   ELK_OPCODE_AND,
   ELK_OPCODE_OR,
   ELK_OPCODE_XOR,
   ELK_OPCODE_SHR,
   ELK_OPCODE_SHL,
   ELK_OPCODE_DIM, /**< Gfx7.5 only */
   ELK_OPCODE_SMOV, /**< Gfx8+ */
   ELK_OPCODE_ASR,
   ELK_OPCODE_CMP,
   ELK_OPCODE_CMPN,
   ELK_OPCODE_CSEL, /**< Gfx8+ */
   ELK_OPCODE_F32TO16, /**< Gfx7 only */
   ELK_OPCODE_F16TO32, /**< Gfx7 only */
   ELK_OPCODE_BFREV, /**< Gfx7+ */
   ELK_OPCODE_BFE, /**< Gfx7+ */
   ELK_OPCODE_BFI1, /**< Gfx7+ */
   ELK_OPCODE_BFI2, /**< Gfx7+ */
   ELK_OPCODE_JMPI,
   ELK_OPCODE_BRD, /**< Gfx7+ */
   ELK_OPCODE_IF,
   ELK_OPCODE_IFF, /**< Pre-Gfx6 */
   ELK_OPCODE_BRC, /**< Gfx7+ */
   ELK_OPCODE_ELSE,
   ELK_OPCODE_ENDIF,
   ELK_OPCODE_DO, /**< Pre-Gfx6 */
   ELK_OPCODE_CASE, /**< Gfx6 only */
   ELK_OPCODE_WHILE,
   ELK_OPCODE_BREAK,
   ELK_OPCODE_CONTINUE,
   ELK_OPCODE_HALT,
   ELK_OPCODE_CALLA, /**< Gfx7.5+ */
   ELK_OPCODE_MSAVE, /**< Pre-Gfx6 */
   ELK_OPCODE_CALL, /**< Gfx6+ */
   ELK_OPCODE_MREST, /**< Pre-Gfx6 */
   ELK_OPCODE_RET, /**< Gfx6+ */
   ELK_OPCODE_PUSH, /**< Pre-Gfx6 */
   ELK_OPCODE_FORK, /**< Gfx6 only */
   ELK_OPCODE_GOTO, /**< Gfx8+ */
   ELK_OPCODE_POP, /**< Pre-Gfx6 */
   ELK_OPCODE_WAIT,
   ELK_OPCODE_SEND,
   ELK_OPCODE_SENDC,
   ELK_OPCODE_MATH, /**< Gfx6+ */
   ELK_OPCODE_ADD,
   ELK_OPCODE_MUL,
   ELK_OPCODE_AVG,
   ELK_OPCODE_FRC,
   ELK_OPCODE_RNDU,
   ELK_OPCODE_RNDD,
   ELK_OPCODE_RNDE,
   ELK_OPCODE_RNDZ,
   ELK_OPCODE_MAC,
   ELK_OPCODE_MACH,
   ELK_OPCODE_LZD,
   ELK_OPCODE_FBH, /**< Gfx7+ */
   ELK_OPCODE_FBL, /**< Gfx7+ */
   ELK_OPCODE_CBIT, /**< Gfx7+ */
   ELK_OPCODE_ADDC, /**< Gfx7+ */
   ELK_OPCODE_SUBB, /**< Gfx7+ */
   ELK_OPCODE_SAD2,
   ELK_OPCODE_SADA2,
   ELK_OPCODE_DP4,
   ELK_OPCODE_DPH,
   ELK_OPCODE_DP3,
   ELK_OPCODE_DP2,
   ELK_OPCODE_LINE,
   ELK_OPCODE_PLN, /**< G45+ */
   ELK_OPCODE_MAD, /**< Gfx6+ */
   ELK_OPCODE_LRP, /**< Gfx6+ */
   ELK_OPCODE_MADM, /**< Gfx8+ */
   ELK_OPCODE_NENOP, /**< G45 only */
   ELK_OPCODE_NOP,

   NUM_ELK_OPCODES,

   /* These are compiler backend opcodes that get translated into other
    * instructions.
    */
   ELK_FS_OPCODE_FB_WRITE = NUM_ELK_OPCODES,

   /**
    * Same as ELK_FS_OPCODE_FB_WRITE but expects its arguments separately as
    * individual sources instead of as a single payload blob. The
    * position/ordering of the arguments are defined by the enum
    * fb_write_logical_srcs.
    */
   ELK_FS_OPCODE_FB_WRITE_LOGICAL,

   ELK_FS_OPCODE_REP_FB_WRITE,

   ELK_SHADER_OPCODE_RCP,
   ELK_SHADER_OPCODE_RSQ,
   ELK_SHADER_OPCODE_SQRT,
   ELK_SHADER_OPCODE_EXP2,
   ELK_SHADER_OPCODE_LOG2,
   ELK_SHADER_OPCODE_POW,
   ELK_SHADER_OPCODE_INT_QUOTIENT,
   ELK_SHADER_OPCODE_INT_REMAINDER,
   ELK_SHADER_OPCODE_SIN,
   ELK_SHADER_OPCODE_COS,

   /**
    * A generic "send" opcode.  The first source is the descriptor and
    * the second source is the message payload.
    */
   ELK_SHADER_OPCODE_SEND,

   /**
    * An "undefined" write which does nothing but indicates to liveness that
    * we don't care about any values in the register which predate this
    * instruction.  Used to prevent partial writes from causing issues with
    * live ranges.
    */
   ELK_SHADER_OPCODE_UNDEF,

   /**
    * Texture sampling opcodes.
    *
    * LOGICAL opcodes are eventually translated to the matching non-LOGICAL
    * opcode but instead of taking a single payload blob they expect their
    * arguments separately as individual sources. The position/ordering of the
    * arguments are defined by the enum tex_logical_srcs.
    */
   ELK_SHADER_OPCODE_TEX,
   ELK_SHADER_OPCODE_TEX_LOGICAL,
   ELK_SHADER_OPCODE_TXD,
   ELK_SHADER_OPCODE_TXD_LOGICAL,
   ELK_SHADER_OPCODE_TXF,
   ELK_SHADER_OPCODE_TXF_LOGICAL,
   ELK_SHADER_OPCODE_TXF_LZ,
   ELK_SHADER_OPCODE_TXL,
   ELK_SHADER_OPCODE_TXL_LOGICAL,
   ELK_SHADER_OPCODE_TXL_LZ,
   ELK_SHADER_OPCODE_TXS,
   ELK_SHADER_OPCODE_TXS_LOGICAL,
   ELK_FS_OPCODE_TXB,
   ELK_FS_OPCODE_TXB_LOGICAL,
   ELK_SHADER_OPCODE_TXF_CMS,
   ELK_SHADER_OPCODE_TXF_CMS_LOGICAL,
   ELK_SHADER_OPCODE_TXF_CMS_W,
   ELK_SHADER_OPCODE_TXF_CMS_W_LOGICAL,
   ELK_SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL,
   ELK_SHADER_OPCODE_TXF_UMS,
   ELK_SHADER_OPCODE_TXF_UMS_LOGICAL,
   ELK_SHADER_OPCODE_TXF_MCS,
   ELK_SHADER_OPCODE_TXF_MCS_LOGICAL,
   ELK_SHADER_OPCODE_LOD,
   ELK_SHADER_OPCODE_LOD_LOGICAL,
   ELK_SHADER_OPCODE_TG4,
   ELK_SHADER_OPCODE_TG4_LOGICAL,
   ELK_SHADER_OPCODE_TG4_OFFSET,
   ELK_SHADER_OPCODE_TG4_OFFSET_LOGICAL,
   ELK_SHADER_OPCODE_SAMPLEINFO,
   ELK_SHADER_OPCODE_SAMPLEINFO_LOGICAL,

   ELK_SHADER_OPCODE_IMAGE_SIZE_LOGICAL,

   /**
    * Combines multiple sources of size 1 into a larger virtual GRF.
    * For example, parameters for a send-from-GRF message.  Or, updating
    * channels of a size 4 VGRF used to store vec4s such as texturing results.
    *
    * This will be lowered into MOVs from each source to consecutive offsets
    * of the destination VGRF.
    *
    * src[0] may be BAD_FILE.  If so, the lowering pass skips emitting the MOV,
    * but still reserves the first channel of the destination VGRF.  This can be
    * used to reserve space for, say, a message header set up by the generators.
    */
   ELK_SHADER_OPCODE_LOAD_PAYLOAD,

   /**
    * Packs a number of sources into a single value. Unlike LOAD_PAYLOAD, this
    * acts intra-channel, obtaining the final value for each channel by
    * combining the sources values for the same channel, the first source
    * occupying the lowest bits and the last source occupying the highest
    * bits.
    */
   ELK_FS_OPCODE_PACK,

   /**
    * Typed and untyped surface access opcodes.
    *
    * LOGICAL opcodes are eventually translated to the matching non-LOGICAL
    * opcode but instead of taking a single payload blob they expect their
    * arguments separately as individual sources:
    *
    * Source 0: [required] Surface coordinates.
    * Source 1: [optional] Operation source.
    * Source 2: [required] Surface index.
    * Source 3: [required] Number of coordinate components (as UD immediate).
    * Source 4: [required] Opcode-specific control immediate, same as source 2
    *                      of the matching non-LOGICAL opcode.
    */
   ELK_VEC4_OPCODE_UNTYPED_ATOMIC,
   ELK_SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL,
   ELK_VEC4_OPCODE_UNTYPED_SURFACE_READ,
   ELK_SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL,
   ELK_VEC4_OPCODE_UNTYPED_SURFACE_WRITE,
   ELK_SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL,

   ELK_SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL,
   ELK_SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL,

   /**
    * Untyped A64 surface access opcodes.
    *
    * Source 0: 64-bit address
    * Source 1: Operational source
    * Source 2: [required] Opcode-specific control immediate, same as source 2
    *                      of the matching non-LOGICAL opcode.
    */
   ELK_SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL,
   ELK_SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL,
   ELK_SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL,
   ELK_SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL,
   ELK_SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL,
   ELK_SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL,
   ELK_SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL,
   ELK_SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL,

   ELK_SHADER_OPCODE_TYPED_ATOMIC_LOGICAL,
   ELK_SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL,
   ELK_SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL,

   ELK_SHADER_OPCODE_RND_MODE,
   ELK_SHADER_OPCODE_FLOAT_CONTROL_MODE,

   /**
    * Byte scattered write/read opcodes.
    *
    * LOGICAL opcodes are eventually translated to the matching non-LOGICAL
    * opcode, but instead of taking a single payload blog they expect their
    * arguments separately as individual sources, like untyped write/read.
    */
   ELK_SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL,
   ELK_SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL,
   ELK_SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL,
   ELK_SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL,

   /**
    * Memory fence messages.
    *
    * Source 0: Must be register g0, used as header.
    * Source 1: Immediate bool to indicate whether control is returned to the
    *           thread only after the fence has been honored.
    * Source 2: Immediate byte indicating which memory to fence.  Zero means
    *           global memory; GFX7_BTI_SLM means SLM (for Gfx11+ only).
    *
    * Vec4 backend only uses Source 0.
    */
   ELK_SHADER_OPCODE_MEMORY_FENCE,

   /**
    * Scheduling-only fence.
    *
    * Sources can be used to force a stall until the registers in those are
    * available.  This might generate MOVs or SYNC_NOPs (Gfx12+).
    */
   ELK_FS_OPCODE_SCHEDULING_FENCE,

   ELK_SHADER_OPCODE_GFX4_SCRATCH_READ,
   ELK_SHADER_OPCODE_GFX4_SCRATCH_WRITE,
   ELK_SHADER_OPCODE_GFX7_SCRATCH_READ,

   ELK_SHADER_OPCODE_SCRATCH_HEADER,

   /**
    * Gfx8+ SIMD8 URB messages.
    */
   ELK_SHADER_OPCODE_URB_READ_LOGICAL,
   ELK_SHADER_OPCODE_URB_WRITE_LOGICAL,

   /**
    * Return the index of the first enabled live channel and assign it to
    * to the first component of the destination.  Frequently used as input
    * for the BROADCAST pseudo-opcode.
    */
   ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL,

   /**
    * Return the index of the last enabled live channel and assign it to
    * the first component of the destination.
    */
   ELK_SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL,

   /**
    * Return the current execution mask in the specified flag subregister.
    * Can be CSE'ed more easily than a plain MOV from the ce0 ARF register.
    */
   ELK_FS_OPCODE_LOAD_LIVE_CHANNELS,

   /**
    * Pick the channel from its first source register given by the index
    * specified as second source.  Useful for variable indexing of surfaces.
    *
    * Note that because the result of this instruction is by definition
    * uniform and it can always be splatted to multiple channels using a
    * scalar regioning mode, only the first channel of the destination region
    * is guaranteed to be updated, which implies that BROADCAST instructions
    * should usually be marked force_writemask_all.
    */
   ELK_SHADER_OPCODE_BROADCAST,

   /* Pick the channel from its first source register given by the index
    * specified as second source.
    *
    * This is similar to the BROADCAST instruction except that it takes a
    * dynamic index and potentially puts a different value in each output
    * channel.
    */
   ELK_SHADER_OPCODE_SHUFFLE,

   /* Select between src0 and src1 based on channel enables.
    *
    * This instruction copies src0 into the enabled channels of the
    * destination and copies src1 into the disabled channels.
    */
   ELK_SHADER_OPCODE_SEL_EXEC,

   /* This turns into an align16 mov from src0 to dst with a swizzle
    * provided as an immediate in src1.
    */
   ELK_SHADER_OPCODE_QUAD_SWIZZLE,

   /* Take every Nth element in src0 and broadcast it to the group of N
    * channels in which it lives in the destination.  The offset within the
    * cluster is given by src1 and the cluster size is given by src2.
    */
   ELK_SHADER_OPCODE_CLUSTER_BROADCAST,

   ELK_SHADER_OPCODE_GET_BUFFER_SIZE,

   ELK_SHADER_OPCODE_INTERLOCK,

   /** Target for a HALT
    *
    * All HALT instructions in a shader must target the same jump point and
    * that point is denoted by a HALT_TARGET instruction.
    */
   ELK_SHADER_OPCODE_HALT_TARGET,

   ELK_VEC4_OPCODE_MOV_BYTES,
   ELK_VEC4_OPCODE_PACK_BYTES,
   ELK_VEC4_OPCODE_UNPACK_UNIFORM,
   ELK_VEC4_OPCODE_DOUBLE_TO_F32,
   ELK_VEC4_OPCODE_DOUBLE_TO_D32,
   ELK_VEC4_OPCODE_DOUBLE_TO_U32,
   ELK_VEC4_OPCODE_TO_DOUBLE,
   ELK_VEC4_OPCODE_PICK_LOW_32BIT,
   ELK_VEC4_OPCODE_PICK_HIGH_32BIT,
   ELK_VEC4_OPCODE_SET_LOW_32BIT,
   ELK_VEC4_OPCODE_SET_HIGH_32BIT,
   ELK_VEC4_OPCODE_MOV_FOR_SCRATCH,
   ELK_VEC4_OPCODE_ZERO_OOB_PUSH_REGS,

   ELK_FS_OPCODE_DDX_COARSE,
   ELK_FS_OPCODE_DDX_FINE,
   /**
    * Compute dFdy(), dFdyCoarse(), or dFdyFine().
    */
   ELK_FS_OPCODE_DDY_COARSE,
   ELK_FS_OPCODE_DDY_FINE,
   ELK_FS_OPCODE_LINTERP,
   ELK_FS_OPCODE_PIXEL_X,
   ELK_FS_OPCODE_PIXEL_Y,
   ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD,
   ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GFX4,
   ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL,
   ELK_FS_OPCODE_SET_SAMPLE_ID,
   ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT,
   ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE,
   ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET,
   ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET,

   ELK_VEC4_VS_OPCODE_URB_WRITE,
   ELK_VS_OPCODE_PULL_CONSTANT_LOAD,
   ELK_VS_OPCODE_PULL_CONSTANT_LOAD_GFX7,

   ELK_VS_OPCODE_UNPACK_FLAGS_SIMD4X2,

   /**
    * Write geometry shader output data to the URB.
    *
    * Unlike ELK_VEC4_VS_OPCODE_URB_WRITE, this opcode doesn't do an implied move from
    * R0 to the first MRF.  This allows the geometry shader to override the
    * "Slot {0,1} Offset" fields in the message header.
    */
   ELK_VEC4_GS_OPCODE_URB_WRITE,

   /**
    * Write geometry shader output data to the URB and request a new URB
    * handle (gfx6).
    *
    * This opcode doesn't do an implied move from R0 to the first MRF.
    */
   ELK_VEC4_GS_OPCODE_URB_WRITE_ALLOCATE,

   /**
    * Terminate the geometry shader thread by doing an empty URB write.
    *
    * This opcode doesn't do an implied move from R0 to the first MRF.  This
    * allows the geometry shader to override the "GS Number of Output Vertices
    * for Slot {0,1}" fields in the message header.
    */
   ELK_GS_OPCODE_THREAD_END,

   /**
    * Set the "Slot {0,1} Offset" fields of a URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src0.x indicates which portion of the URB should be written to (e.g. a
    *   vertex number)
    *
    * - src1 is an immediate multiplier which will be applied to src0
    *   (e.g. the size of a single vertex in the URB).
    *
    * Note: the hardware will apply this offset *in addition to* the offset in
    * vec4_instruction::offset.
    */
   ELK_GS_OPCODE_SET_WRITE_OFFSET,

   /**
    * Set the "GS Number of Output Vertices for Slot {0,1}" fields of a
    * URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src0.x is the vertex count.  The upper 16 bits will be ignored.
    */
   ELK_GS_OPCODE_SET_VERTEX_COUNT,

   /**
    * Set DWORD 2 of dst to the value in src.
    */
   ELK_GS_OPCODE_SET_DWORD_2,

   /**
    * Prepare the dst register for storage in the "Channel Mask" fields of a
    * URB_WRITE message header.
    *
    * DWORD 4 of dst is shifted left by 4 bits, so that later,
    * ELK_GS_OPCODE_SET_CHANNEL_MASKS can OR DWORDs 0 and 4 together to form the
    * final channel mask.
    *
    * Note: since ELK_GS_OPCODE_SET_CHANNEL_MASKS ORs DWORDs 0 and 4 together to
    * form the final channel mask, DWORDs 0 and 4 of the dst register must not
    * have any extraneous bits set prior to execution of this opcode (that is,
    * they should be in the range 0x0 to 0xf).
    */
   ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS,

   /**
    * Set the "Channel Mask" fields of a URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src.x is the channel mask, as prepared by
    *   ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS.  DWORDs 0 and 4 are OR'ed together to
    *   form the final channel mask.
    */
   ELK_GS_OPCODE_SET_CHANNEL_MASKS,

   /**
    * Get the "Instance ID" fields from the payload.
    *
    * - dst is the GRF for gl_InvocationID.
    */
   ELK_GS_OPCODE_GET_INSTANCE_ID,

   /**
    * Send a FF_SYNC message to allocate initial URB handles (gfx6).
    *
    * - dst will be used as the writeback register for the FF_SYNC operation.
    *
    * - src0 is the number of primitives written.
    *
    * - src1 is the value to hold in M0.0: number of SO vertices to write
    *   and number of SO primitives needed. Its value will be overwritten
    *   with the SVBI values if transform feedback is enabled.
    *
    * Note: This opcode uses an implicit MRF register for the ff_sync message
    * header, so the caller is expected to set inst->base_mrf and initialize
    * that MRF register to r0. This opcode will also write to this MRF register
    * to include the allocated URB handle so it can then be reused directly as
    * the header in the URB write operation we are allocating the handle for.
    */
   ELK_GS_OPCODE_FF_SYNC,

   /**
    * Move r0.1 (which holds PrimitiveID information in gfx6) to a separate
    * register.
    *
    * - dst is the GRF where PrimitiveID information will be moved.
    */
   ELK_GS_OPCODE_SET_PRIMITIVE_ID,

   /**
    * Write transform feedback data to the SVB by sending a SVB WRITE message.
    * Used in gfx6.
    *
    * - dst is the MRF register containing the message header.
    *
    * - src0 is the register where the vertex data is going to be copied from.
    *
    * - src1 is the destination register when write commit occurs.
    */
   ELK_GS_OPCODE_SVB_WRITE,

   /**
    * Set destination index in the SVB write message payload (M0.5). Used
    * in gfx6 for transform feedback.
    *
    * - dst is the header to save the destination indices for SVB WRITE.
    * - src is the register that holds the destination indices value.
    */
   ELK_GS_OPCODE_SVB_SET_DST_INDEX,

   /**
    * Prepare Mx.0 subregister for being used in the FF_SYNC message header.
    * Used in gfx6 for transform feedback.
    *
    * - dst will hold the register with the final Mx.0 value.
    *
    * - src0 has the number of vertices emitted in SO (NumSOVertsToWrite)
    *
    * - src1 has the number of needed primitives for SO (NumSOPrimsNeeded)
    *
    * - src2 is the value to hold in M0: number of SO vertices to write
    *   and number of SO primitives needed.
    */
   ELK_GS_OPCODE_FF_SYNC_SET_PRIMITIVES,

   /**
    * Terminate the compute shader.
    */
   ELK_CS_OPCODE_CS_TERMINATE,

   /**
    * GLSL barrier()
    */
   ELK_SHADER_OPCODE_BARRIER,

   /**
    * Calculate the high 32-bits of a 32x32 multiply.
    */
   ELK_SHADER_OPCODE_MULH,

   /** Signed subtraction with saturation. */
   ELK_SHADER_OPCODE_ISUB_SAT,

   /** Unsigned subtraction with saturation. */
   ELK_SHADER_OPCODE_USUB_SAT,

   /**
    * A MOV that uses VxH indirect addressing.
    *
    * Source 0: A register to start from (HW_REG).
    * Source 1: An indirect offset (in bytes, UD GRF).
    * Source 2: The length of the region that could be accessed (in bytes,
    *           UD immediate).
    */
   ELK_SHADER_OPCODE_MOV_INDIRECT,

   /** Fills out a relocatable immediate */
   ELK_SHADER_OPCODE_MOV_RELOC_IMM,

   ELK_VEC4_OPCODE_URB_READ,
   ELK_TCS_OPCODE_GET_INSTANCE_ID,
   ELK_VEC4_TCS_OPCODE_URB_WRITE,
   ELK_VEC4_TCS_OPCODE_SET_INPUT_URB_OFFSETS,
   ELK_VEC4_TCS_OPCODE_SET_OUTPUT_URB_OFFSETS,
   ELK_TCS_OPCODE_GET_PRIMITIVE_ID,
   ELK_TCS_OPCODE_CREATE_BARRIER_HEADER,
   ELK_TCS_OPCODE_SRC0_010_IS_ZERO,
   ELK_TCS_OPCODE_RELEASE_INPUT,
   ELK_TCS_OPCODE_THREAD_END,

   ELK_TES_OPCODE_GET_PRIMITIVE_ID,
   ELK_TES_OPCODE_CREATE_INPUT_READ_HEADER,
   ELK_TES_OPCODE_ADD_INDIRECT_URB_OFFSET,

   ELK_SHADER_OPCODE_READ_SR_REG,
};


#ifdef __cplusplus
}
#endif

#endif

