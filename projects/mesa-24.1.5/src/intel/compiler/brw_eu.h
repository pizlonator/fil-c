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


#ifndef BRW_EU_H
#define BRW_EU_H

#include <stdbool.h>
#include <stdio.h>
#include "brw_inst.h"
#include "brw_compiler.h"
#include "brw_eu_defines.h"
#include "brw_isa_info.h"
#include "brw_reg.h"

#include "intel_wa.h"
#include "util/bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

struct disasm_info;

#define BRW_EU_MAX_INSN_STACK 5

struct brw_insn_state {
   /* One of BRW_EXECUTE_* */
   unsigned exec_size:3;

   /* Group in units of channels */
   unsigned group:5;

   /* One of BRW_MASK_* */
   unsigned mask_control:1;

   /* Scheduling info for Gfx12+ */
   struct tgl_swsb swsb;

   bool saturate:1;

   /* One of BRW_ALIGN_* */
   unsigned access_mode:1;

   /* One of BRW_PREDICATE_* */
   enum brw_predicate predicate:4;

   bool pred_inv:1;

   /* Flag subreg.  Bottom bit is subreg, top bit is reg */
   unsigned flag_subreg:2;

   bool acc_wr_control:1;
};


/* A helper for accessing the last instruction emitted.  This makes it easy
 * to set various bits on an instruction without having to create temporary
 * variable and assign the emitted instruction to those.
 */
#define brw_last_inst (&p->store[p->nr_insn - 1])

struct brw_codegen {
   brw_inst *store;
   int store_size;
   unsigned nr_insn;
   unsigned int next_insn_offset;

   void *mem_ctx;

   /* Allow clients to push/pop instruction state:
    */
   struct brw_insn_state stack[BRW_EU_MAX_INSN_STACK];
   struct brw_insn_state *current;

   const struct brw_isa_info *isa;
   const struct intel_device_info *devinfo;

   /* Control flow stacks:
    * - if_stack contains IF and ELSE instructions which must be patched
    *   (and popped) once the matching ENDIF instruction is encountered.
    *
    *   Just store the instruction pointer(an index).
    */
   int *if_stack;
   int if_stack_depth;
   int if_stack_array_size;

   /**
    * loop_stack contains the instruction pointers of the starts of loops which
    * must be patched (and popped) once the matching WHILE instruction is
    * encountered.
    */
   int *loop_stack;
   int loop_stack_depth;
   int loop_stack_array_size;

   struct brw_shader_reloc *relocs;
   int num_relocs;
   int reloc_array_size;
};

struct brw_label {
   int offset;
   int number;
   struct brw_label *next;
};

void brw_pop_insn_state( struct brw_codegen *p );
void brw_push_insn_state( struct brw_codegen *p );
unsigned brw_get_default_exec_size(struct brw_codegen *p);
unsigned brw_get_default_group(struct brw_codegen *p);
unsigned brw_get_default_access_mode(struct brw_codegen *p);
struct tgl_swsb brw_get_default_swsb(struct brw_codegen *p);
void brw_set_default_exec_size(struct brw_codegen *p, unsigned value);
void brw_set_default_mask_control( struct brw_codegen *p, unsigned value );
void brw_set_default_saturate( struct brw_codegen *p, bool enable );
void brw_set_default_access_mode( struct brw_codegen *p, unsigned access_mode );
void brw_inst_set_group(const struct intel_device_info *devinfo,
                        brw_inst *inst, unsigned group);
void brw_set_default_group(struct brw_codegen *p, unsigned group);
void brw_set_default_predicate_control(struct brw_codegen *p, enum brw_predicate pc);
void brw_set_default_predicate_inverse(struct brw_codegen *p, bool predicate_inverse);
void brw_set_default_flag_reg(struct brw_codegen *p, int reg, int subreg);
void brw_set_default_acc_write_control(struct brw_codegen *p, unsigned value);
void brw_set_default_swsb(struct brw_codegen *p, struct tgl_swsb value);

void brw_init_codegen(const struct brw_isa_info *isa,
                      struct brw_codegen *p, void *mem_ctx);
bool brw_has_jip(const struct intel_device_info *devinfo, enum opcode opcode);
bool brw_has_uip(const struct intel_device_info *devinfo, enum opcode opcode);
const struct brw_shader_reloc *brw_get_shader_relocs(struct brw_codegen *p,
                                                     unsigned *num_relocs);
const unsigned *brw_get_program( struct brw_codegen *p, unsigned *sz );

bool brw_should_dump_shader_bin(void);
void brw_dump_shader_bin(void *assembly, int start_offset, int end_offset,
                         const char *identifier);

bool brw_try_override_assembly(struct brw_codegen *p, int start_offset,
                               const char *identifier);

void brw_realign(struct brw_codegen *p, unsigned alignment);
int brw_append_data(struct brw_codegen *p, void *data,
                    unsigned size, unsigned alignment);
brw_inst *brw_next_insn(struct brw_codegen *p, unsigned opcode);
void brw_add_reloc(struct brw_codegen *p, uint32_t id,
                   enum brw_shader_reloc_type type,
                   uint32_t offset, uint32_t delta);
void brw_set_dest(struct brw_codegen *p, brw_inst *insn, struct brw_reg dest);
void brw_set_src0(struct brw_codegen *p, brw_inst *insn, struct brw_reg reg);

/* Helpers for regular instructions:
 */
#define ALU1(OP)				\
brw_inst *brw_##OP(struct brw_codegen *p,	\
	      struct brw_reg dest,		\
	      struct brw_reg src0);

#define ALU2(OP)				\
brw_inst *brw_##OP(struct brw_codegen *p,	\
	      struct brw_reg dest,		\
	      struct brw_reg src0,		\
	      struct brw_reg src1);

#define ALU3(OP)				\
brw_inst *brw_##OP(struct brw_codegen *p,	\
	      struct brw_reg dest,		\
	      struct brw_reg src0,		\
	      struct brw_reg src1,		\
	      struct brw_reg src2);

ALU1(MOV)
ALU2(SEL)
ALU1(NOT)
ALU2(AND)
ALU2(OR)
ALU2(XOR)
ALU2(SHR)
ALU2(SHL)
ALU1(DIM)
ALU2(ASR)
ALU2(ROL)
ALU2(ROR)
ALU3(CSEL)
ALU1(F32TO16)
ALU1(F16TO32)
ALU2(ADD)
ALU3(ADD3)
ALU2(AVG)
ALU2(MUL)
ALU1(FRC)
ALU1(RNDD)
ALU1(RNDE)
ALU1(RNDU)
ALU1(RNDZ)
ALU2(MAC)
ALU2(MACH)
ALU1(LZD)
ALU2(DP4)
ALU2(DPH)
ALU2(DP3)
ALU2(DP2)
ALU3(DP4A)
ALU2(LINE)
ALU2(PLN)
ALU3(MAD)
ALU3(LRP)
ALU1(BFREV)
ALU3(BFE)
ALU2(BFI1)
ALU3(BFI2)
ALU1(FBH)
ALU1(FBL)
ALU1(CBIT)
ALU2(ADDC)
ALU2(SUBB)

#undef ALU1
#undef ALU2
#undef ALU3

static inline unsigned
reg_unit(const struct intel_device_info *devinfo)
{
   return devinfo->ver >= 20 ? 2 : 1;
}


/* Helpers for SEND instruction:
 */

/**
 * Construct a message descriptor immediate with the specified common
 * descriptor controls.
 */
static inline uint32_t
brw_message_desc(const struct intel_device_info *devinfo,
                 unsigned msg_length,
                 unsigned response_length,
                 bool header_present)
{
   assert(msg_length % reg_unit(devinfo) == 0);
   assert(response_length % reg_unit(devinfo) == 0);
   return (SET_BITS(msg_length / reg_unit(devinfo), 28, 25) |
           SET_BITS(response_length / reg_unit(devinfo), 24, 20) |
           SET_BITS(header_present, 19, 19));
}

static inline unsigned
brw_message_desc_mlen(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 28, 25) * reg_unit(devinfo);
}

static inline unsigned
brw_message_desc_rlen(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 24, 20) * reg_unit(devinfo);
}

static inline bool
brw_message_desc_header_present(ASSERTED
                                const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return GET_BITS(desc, 19, 19);
}

static inline unsigned
brw_message_ex_desc(const struct intel_device_info *devinfo,
                    unsigned ex_msg_length)
{
   assert(ex_msg_length % reg_unit(devinfo) == 0);
   return devinfo->ver >= 20 ?
      SET_BITS(ex_msg_length / reg_unit(devinfo), 10, 6) :
      SET_BITS(ex_msg_length / reg_unit(devinfo), 9, 6);
}

static inline unsigned
brw_message_ex_desc_ex_mlen(const struct intel_device_info *devinfo,
                            uint32_t ex_desc)
{
   return devinfo->ver >= 20 ?
      GET_BITS(ex_desc, 10, 6) * reg_unit(devinfo) :
      GET_BITS(ex_desc, 9, 6) * reg_unit(devinfo);
}

static inline uint32_t
brw_urb_desc(const struct intel_device_info *devinfo,
             unsigned msg_type,
             bool per_slot_offset_present,
             bool channel_mask_present,
             unsigned global_offset)
{
   return (SET_BITS(per_slot_offset_present, 17, 17) |
           SET_BITS(channel_mask_present, 15, 15) |
           SET_BITS(global_offset, 14, 4) |
           SET_BITS(msg_type, 3, 0));
}

static inline uint32_t
brw_urb_desc_msg_type(ASSERTED const struct intel_device_info *devinfo,
                      uint32_t desc)
{
   return GET_BITS(desc, 3, 0);
}

static inline uint32_t
brw_urb_fence_desc(const struct intel_device_info *devinfo)
{
   assert(devinfo->has_lsc);
   return brw_urb_desc(devinfo, GFX125_URB_OPCODE_FENCE, false, false, 0);
}

/**
 * Construct a message descriptor immediate with the specified sampler
 * function controls.
 */
static inline uint32_t
brw_sampler_desc(const struct intel_device_info *devinfo,
                 unsigned binding_table_index,
                 unsigned sampler,
                 unsigned msg_type,
                 unsigned simd_mode,
                 unsigned return_format)
{
   const unsigned desc = (SET_BITS(binding_table_index, 7, 0) |
                          SET_BITS(sampler, 11, 8));

   /* From GFX20 Bspec: Shared Functions - Message Descriptor -
    * Sampling Engine:
    *
    *    Message Type[5]  31  This bit represents the upper bit of message type
    *                         6-bit encoding (c.f. [16:12]). This bit is set
    *                         for messages with programmable offsets.
    */
   if (devinfo->ver >= 20)
      return desc | SET_BITS(msg_type & 0x1F, 16, 12) |
             SET_BITS(simd_mode & 0x3, 18, 17) |
             SET_BITS(simd_mode >> 2, 29, 29) |
             SET_BITS(return_format, 30, 30) |
             SET_BITS(msg_type >> 5, 31, 31);

   /* From the CHV Bspec: Shared Functions - Message Descriptor -
    * Sampling Engine:
    *
    *   SIMD Mode[2]  29    This field is the upper bit of the 3-bit
    *                       SIMD Mode field.
    */
   return desc | SET_BITS(msg_type, 16, 12) |
          SET_BITS(simd_mode & 0x3, 18, 17) |
          SET_BITS(simd_mode >> 2, 29, 29) |
          SET_BITS(return_format, 30, 30);
}

static inline unsigned
brw_sampler_desc_binding_table_index(UNUSED
                                     const struct intel_device_info *devinfo,
                                     uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline unsigned
brw_sampler_desc_sampler(UNUSED const struct intel_device_info *devinfo,
                         uint32_t desc)
{
   return GET_BITS(desc, 11, 8);
}

static inline unsigned
brw_sampler_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   if (devinfo->ver >= 20)
      return GET_BITS(desc, 31, 31) << 5 | GET_BITS(desc, 16, 12);
   else
      return GET_BITS(desc, 16, 12);
}

static inline unsigned
brw_sampler_desc_simd_mode(const struct intel_device_info *devinfo,
                           uint32_t desc)
{
   return GET_BITS(desc, 18, 17) | GET_BITS(desc, 29, 29) << 2;
}

static inline unsigned
brw_sampler_desc_return_format(ASSERTED const struct intel_device_info *devinfo,
                               uint32_t desc)
{
   return GET_BITS(desc, 30, 30);
}

/**
 * Construct a message descriptor for the dataport
 */
static inline uint32_t
brw_dp_desc(const struct intel_device_info *devinfo,
            unsigned binding_table_index,
            unsigned msg_type,
            unsigned msg_control)
{
   return SET_BITS(binding_table_index, 7, 0) |
          SET_BITS(msg_control, 13, 8) |
          SET_BITS(msg_type, 18, 14);
}

static inline unsigned
brw_dp_desc_binding_table_index(UNUSED const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline unsigned
brw_dp_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 18, 14);
}

static inline unsigned
brw_dp_desc_msg_control(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 13, 8);
}

/**
 * Construct a message descriptor immediate with the specified dataport read
 * function controls.
 */
static inline uint32_t
brw_dp_read_desc(const struct intel_device_info *devinfo,
                 unsigned binding_table_index,
                 unsigned msg_control,
                 unsigned msg_type,
                 unsigned target_cache)
{
   return brw_dp_desc(devinfo, binding_table_index, msg_type, msg_control);
}

static inline unsigned
brw_dp_read_desc_msg_type(const struct intel_device_info *devinfo,
                          uint32_t desc)
{
   return brw_dp_desc_msg_type(devinfo, desc);
}

static inline unsigned
brw_dp_read_desc_msg_control(const struct intel_device_info *devinfo,
                             uint32_t desc)
{
   return brw_dp_desc_msg_control(devinfo, desc);
}

/**
 * Construct a message descriptor immediate with the specified dataport write
 * function controls.
 */
static inline uint32_t
brw_dp_write_desc(const struct intel_device_info *devinfo,
                  unsigned binding_table_index,
                  unsigned msg_control,
                  unsigned msg_type,
                  unsigned send_commit_msg)
{
   assert(!send_commit_msg);
   return brw_dp_desc(devinfo, binding_table_index, msg_type, msg_control) |
          SET_BITS(send_commit_msg, 17, 17);
}

static inline unsigned
brw_dp_write_desc_msg_type(const struct intel_device_info *devinfo,
                           uint32_t desc)
{
   return brw_dp_desc_msg_type(devinfo, desc);
}

static inline unsigned
brw_dp_write_desc_msg_control(const struct intel_device_info *devinfo,
                              uint32_t desc)
{
   return brw_dp_desc_msg_control(devinfo, desc);
}

/**
 * Construct a message descriptor immediate with the specified dataport
 * surface function controls.
 */
static inline uint32_t
brw_dp_surface_desc(const struct intel_device_info *devinfo,
                    unsigned msg_type,
                    unsigned msg_control)
{
   /* We'll OR in the binding table index later */
   return brw_dp_desc(devinfo, 0, msg_type, msg_control);
}

static inline uint32_t
brw_dp_untyped_atomic_desc(const struct intel_device_info *devinfo,
                           unsigned exec_size, /**< 0 for SIMD4x2 */
                           unsigned atomic_op,
                           bool response_expected)
{
   assert(exec_size <= 8 || exec_size == 16);

   unsigned msg_type;
   if (exec_size > 0) {
      msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP;
   } else {
      msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2;
   }

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(0 < exec_size && exec_size <= 8, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_dp_untyped_atomic_float_desc(const struct intel_device_info *devinfo,
                                 unsigned exec_size,
                                 unsigned atomic_op,
                                 bool response_expected)
{
   assert(exec_size <= 8 || exec_size == 16);

   assert(exec_size > 0);
   const unsigned msg_type = GFX9_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_FLOAT_OP;

   const unsigned msg_control =
      SET_BITS(atomic_op, 1, 0) |
      SET_BITS(exec_size <= 8, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline unsigned
brw_mdc_cmask(unsigned num_channels)
{
   /* See also MDC_CMASK in the SKL PRM Vol 2d. */
   return 0xf & (0xf << num_channels);
}

static inline unsigned
lsc_cmask(unsigned num_channels)
{
   assert(num_channels > 0 && num_channels <= 4);
   return BITSET_MASK(num_channels);
}

static inline uint32_t
brw_dp_untyped_surface_rw_desc(const struct intel_device_info *devinfo,
                               unsigned exec_size, /**< 0 for SIMD4x2 */
                               unsigned num_channels,
                               bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   const unsigned msg_type =
      write ? HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE :
              HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ;

   /* See also MDC_SM3 in the SKL PRM Vol 2d. */
   const unsigned simd_mode = exec_size == 0 ? 0 : /* SIMD4x2 */
                              exec_size <= 8 ? 2 : 1;

   const unsigned msg_control =
      SET_BITS(brw_mdc_cmask(num_channels), 3, 0) |
      SET_BITS(simd_mode, 5, 4);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline unsigned
brw_mdc_ds(unsigned bit_size)
{
   switch (bit_size) {
   case 8:
      return GFX7_BYTE_SCATTERED_DATA_ELEMENT_BYTE;
   case 16:
      return GFX7_BYTE_SCATTERED_DATA_ELEMENT_WORD;
   case 32:
      return GFX7_BYTE_SCATTERED_DATA_ELEMENT_DWORD;
   default:
      unreachable("Unsupported bit_size for byte scattered messages");
   }
}

static inline uint32_t
brw_dp_byte_scattered_rw_desc(const struct intel_device_info *devinfo,
                              unsigned exec_size,
                              unsigned bit_size,
                              bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   const unsigned msg_type =
      write ? HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_WRITE :
              HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_READ;

   assert(exec_size > 0);
   const unsigned msg_control =
      SET_BITS(exec_size == 16, 0, 0) |
      SET_BITS(brw_mdc_ds(bit_size), 3, 2);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_dp_dword_scattered_rw_desc(const struct intel_device_info *devinfo,
                               unsigned exec_size,
                               bool write)
{
   assert(exec_size == 8 || exec_size == 16);

   const unsigned msg_type =
      write ? GFX6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE :
              GFX7_DATAPORT_DC_DWORD_SCATTERED_READ;

   const unsigned msg_control =
      SET_BITS(1, 1, 1) | /* Legacy SIMD Mode */
      SET_BITS(exec_size == 16, 0, 0);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_dp_oword_block_rw_desc(const struct intel_device_info *devinfo,
                           bool align_16B,
                           unsigned num_dwords,
                           bool write)
{
   /* Writes can only have addresses aligned by OWORDs (16 Bytes). */
   assert(!write || align_16B);

   const unsigned msg_type =
      write ?     GFX7_DATAPORT_DC_OWORD_BLOCK_WRITE :
      align_16B ? GFX7_DATAPORT_DC_OWORD_BLOCK_READ :
                  GFX7_DATAPORT_DC_UNALIGNED_OWORD_BLOCK_READ;

   const unsigned msg_control =
      SET_BITS(BRW_DATAPORT_OWORD_BLOCK_DWORDS(num_dwords), 2, 0);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_dp_a64_untyped_surface_rw_desc(const struct intel_device_info *devinfo,
                                   unsigned exec_size, /**< 0 for SIMD4x2 */
                                   unsigned num_channels,
                                   bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   unsigned msg_type =
      write ? GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_WRITE :
              GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_READ;

   /* See also MDC_SM3 in the SKL PRM Vol 2d. */
   const unsigned simd_mode = exec_size == 0 ? 0 : /* SIMD4x2 */
                              exec_size <= 8 ? 2 : 1;

   const unsigned msg_control =
      SET_BITS(brw_mdc_cmask(num_channels), 3, 0) |
      SET_BITS(simd_mode, 5, 4);

   return brw_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
brw_dp_a64_oword_block_rw_desc(const struct intel_device_info *devinfo,
                               bool align_16B,
                               unsigned num_dwords,
                               bool write)
{
   /* Writes can only have addresses aligned by OWORDs (16 Bytes). */
   assert(!write || align_16B);

   unsigned msg_type =
      write ? GFX9_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_WRITE :
              GFX9_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_READ;

   unsigned msg_control =
      SET_BITS(!align_16B, 4, 3) |
      SET_BITS(BRW_DATAPORT_OWORD_BLOCK_DWORDS(num_dwords), 2, 0);

   return brw_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

/**
 * Calculate the data size (see MDC_A64_DS in the "Structures" volume of the
 * Skylake PRM).
 */
static inline uint32_t
brw_mdc_a64_ds(unsigned elems)
{
   switch (elems) {
   case 1:  return 0;
   case 2:  return 1;
   case 4:  return 2;
   case 8:  return 3;
   default:
      unreachable("Unsupported elmeent count for A64 scattered message");
   }
}

static inline uint32_t
brw_dp_a64_byte_scattered_rw_desc(const struct intel_device_info *devinfo,
                                  unsigned exec_size, /**< 0 for SIMD4x2 */
                                  unsigned bit_size,
                                  bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   unsigned msg_type =
      write ? GFX8_DATAPORT_DC_PORT1_A64_SCATTERED_WRITE :
              GFX9_DATAPORT_DC_PORT1_A64_SCATTERED_READ;

   const unsigned msg_control =
      SET_BITS(GFX8_A64_SCATTERED_SUBTYPE_BYTE, 1, 0) |
      SET_BITS(brw_mdc_a64_ds(bit_size / 8), 3, 2) |
      SET_BITS(exec_size == 16, 4, 4);

   return brw_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
brw_dp_a64_untyped_atomic_desc(const struct intel_device_info *devinfo,
                               ASSERTED unsigned exec_size, /**< 0 for SIMD4x2 */
                               unsigned bit_size,
                               unsigned atomic_op,
                               bool response_expected)
{
   assert(exec_size == 8);
   assert(bit_size == 16 || bit_size == 32 || bit_size == 64);
   assert(devinfo->ver >= 12 || bit_size >= 32);

   const unsigned msg_type = bit_size == 16 ?
      GFX12_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_HALF_INT_OP :
      GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_OP;

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(bit_size == 64, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return brw_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
brw_dp_a64_untyped_atomic_float_desc(const struct intel_device_info *devinfo,
                                     ASSERTED unsigned exec_size,
                                     unsigned bit_size,
                                     unsigned atomic_op,
                                     bool response_expected)
{
   assert(exec_size == 8);
   assert(bit_size == 16 || bit_size == 32);
   assert(devinfo->ver >= 12 || bit_size == 32);

   assert(exec_size > 0);
   const unsigned msg_type = bit_size == 32 ?
      GFX9_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_FLOAT_OP :
      GFX12_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_HALF_FLOAT_OP;

   const unsigned msg_control =
      SET_BITS(atomic_op, 1, 0) |
      SET_BITS(response_expected, 5, 5);

   return brw_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
brw_dp_typed_atomic_desc(const struct intel_device_info *devinfo,
                         unsigned exec_size,
                         unsigned exec_group,
                         unsigned atomic_op,
                         bool response_expected)
{
   assert(exec_size > 0 || exec_group == 0);
   assert(exec_group % 8 == 0);

   const unsigned msg_type =
      exec_size == 0 ? HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2 :
                       HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP;

   const bool high_sample_mask = (exec_group / 8) % 2 == 1;

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(high_sample_mask, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_dp_typed_surface_rw_desc(const struct intel_device_info *devinfo,
                             unsigned exec_size,
                             unsigned exec_group,
                             unsigned num_channels,
                             bool write)
{
   assert(exec_size > 0 || exec_group == 0);
   assert(exec_group % 8 == 0);

   /* Typed surface reads and writes don't support SIMD16 */
   assert(exec_size <= 8);

   const unsigned msg_type =
      write ? HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE :
              HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ;

   /* See also MDC_SG3 in the SKL PRM Vol 2d. */
   const unsigned slot_group = exec_size == 0 ? 0 : /* SIMD4x2 */
                               1 + ((exec_group / 8) % 2);

   const unsigned msg_control =
      SET_BITS(brw_mdc_cmask(num_channels), 3, 0) |
      SET_BITS(slot_group, 5, 4);

   return brw_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
brw_fb_desc(const struct intel_device_info *devinfo,
            unsigned binding_table_index,
            unsigned msg_type,
            unsigned msg_control)
{
   return SET_BITS(binding_table_index, 7, 0) |
          SET_BITS(msg_control, 13, 8) |
          SET_BITS(msg_type, 17, 14);
}

static inline unsigned
brw_fb_desc_binding_table_index(UNUSED const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline uint32_t
brw_fb_desc_msg_control(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 13, 8);
}

static inline unsigned
brw_fb_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   return GET_BITS(desc, 17, 14);
}

static inline uint32_t
brw_fb_read_desc(const struct intel_device_info *devinfo,
                 unsigned binding_table_index,
                 unsigned msg_control,
                 unsigned exec_size,
                 bool per_sample)
{
   assert(exec_size == 8 || exec_size == 16);

   return brw_fb_desc(devinfo, binding_table_index,
                      GFX9_DATAPORT_RC_RENDER_TARGET_READ, msg_control) |
          SET_BITS(per_sample, 13, 13) |
          SET_BITS(exec_size == 8, 8, 8) /* Render Target Message Subtype */;
}

static inline uint32_t
brw_fb_write_desc(const struct intel_device_info *devinfo,
                  unsigned binding_table_index,
                  unsigned msg_control,
                  bool last_render_target,
                  bool coarse_write)
{
   const unsigned msg_type = GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE;

   assert(devinfo->ver >= 10 || !coarse_write);

   return brw_fb_desc(devinfo, binding_table_index, msg_type, msg_control) |
          SET_BITS(last_render_target, 12, 12) |
          SET_BITS(coarse_write, 18, 18);
}

static inline bool
brw_fb_write_desc_last_render_target(const struct intel_device_info *devinfo,
                                     uint32_t desc)
{
   return GET_BITS(desc, 12, 12);
}

static inline bool
brw_fb_write_desc_coarse_write(const struct intel_device_info *devinfo,
                               uint32_t desc)
{
   assert(devinfo->ver >= 10);
   return GET_BITS(desc, 18, 18);
}

static inline bool
lsc_opcode_has_cmask(enum lsc_opcode opcode)
{
   return opcode == LSC_OP_LOAD_CMASK || opcode == LSC_OP_STORE_CMASK;
}

static inline bool
lsc_opcode_has_transpose(enum lsc_opcode opcode)
{
   return opcode == LSC_OP_LOAD || opcode == LSC_OP_STORE;
}

static inline bool
lsc_opcode_is_store(enum lsc_opcode opcode)
{
   return opcode == LSC_OP_STORE ||
          opcode == LSC_OP_STORE_CMASK;
}

static inline bool
lsc_opcode_is_atomic(enum lsc_opcode opcode)
{
   switch (opcode) {
   case LSC_OP_ATOMIC_INC:
   case LSC_OP_ATOMIC_DEC:
   case LSC_OP_ATOMIC_LOAD:
   case LSC_OP_ATOMIC_STORE:
   case LSC_OP_ATOMIC_ADD:
   case LSC_OP_ATOMIC_SUB:
   case LSC_OP_ATOMIC_MIN:
   case LSC_OP_ATOMIC_MAX:
   case LSC_OP_ATOMIC_UMIN:
   case LSC_OP_ATOMIC_UMAX:
   case LSC_OP_ATOMIC_CMPXCHG:
   case LSC_OP_ATOMIC_FADD:
   case LSC_OP_ATOMIC_FSUB:
   case LSC_OP_ATOMIC_FMIN:
   case LSC_OP_ATOMIC_FMAX:
   case LSC_OP_ATOMIC_FCMPXCHG:
   case LSC_OP_ATOMIC_AND:
   case LSC_OP_ATOMIC_OR:
   case LSC_OP_ATOMIC_XOR:
      return true;

   default:
      return false;
   }
}

static inline bool
lsc_opcode_is_atomic_float(enum lsc_opcode opcode)
{
   switch (opcode) {
   case LSC_OP_ATOMIC_FADD:
   case LSC_OP_ATOMIC_FSUB:
   case LSC_OP_ATOMIC_FMIN:
   case LSC_OP_ATOMIC_FMAX:
   case LSC_OP_ATOMIC_FCMPXCHG:
      return true;

   default:
      return false;
   }
}

static inline unsigned
lsc_op_num_data_values(unsigned _op)
{
   enum lsc_opcode op = (enum lsc_opcode) _op;

   switch (op) {
   case LSC_OP_ATOMIC_CMPXCHG:
   case LSC_OP_ATOMIC_FCMPXCHG:
      return 2;
   case LSC_OP_ATOMIC_INC:
   case LSC_OP_ATOMIC_DEC:
   case LSC_OP_LOAD:
   case LSC_OP_LOAD_CMASK:
   case LSC_OP_FENCE:
      /* XXX: actually check docs */
      return 0;
   default:
      return 1;
   }
}

static inline unsigned
lsc_op_to_legacy_atomic(unsigned _op)
{
   enum lsc_opcode op = (enum lsc_opcode) _op;

   switch (op) {
   case LSC_OP_ATOMIC_INC:
      return BRW_AOP_INC;
   case LSC_OP_ATOMIC_DEC:
      return BRW_AOP_DEC;
   case LSC_OP_ATOMIC_STORE:
      return BRW_AOP_MOV;
   case LSC_OP_ATOMIC_ADD:
      return BRW_AOP_ADD;
   case LSC_OP_ATOMIC_SUB:
      return BRW_AOP_SUB;
   case LSC_OP_ATOMIC_MIN:
      return BRW_AOP_IMIN;
   case LSC_OP_ATOMIC_MAX:
      return BRW_AOP_IMAX;
   case LSC_OP_ATOMIC_UMIN:
      return BRW_AOP_UMIN;
   case LSC_OP_ATOMIC_UMAX:
      return BRW_AOP_UMAX;
   case LSC_OP_ATOMIC_CMPXCHG:
      return BRW_AOP_CMPWR;
   case LSC_OP_ATOMIC_FADD:
      return BRW_AOP_FADD;
   case LSC_OP_ATOMIC_FMIN:
      return BRW_AOP_FMIN;
   case LSC_OP_ATOMIC_FMAX:
      return BRW_AOP_FMAX;
   case LSC_OP_ATOMIC_FCMPXCHG:
      return BRW_AOP_FCMPWR;
   case LSC_OP_ATOMIC_AND:
      return BRW_AOP_AND;
   case LSC_OP_ATOMIC_OR:
      return BRW_AOP_OR;
   case LSC_OP_ATOMIC_XOR:
      return BRW_AOP_XOR;
   /* No LSC op maps to BRW_AOP_PREDEC */
   case LSC_OP_ATOMIC_LOAD:
   case LSC_OP_ATOMIC_FSUB:
      unreachable("no corresponding legacy atomic operation");
   case LSC_OP_LOAD:
   case LSC_OP_LOAD_CMASK:
   case LSC_OP_STORE:
   case LSC_OP_STORE_CMASK:
   case LSC_OP_FENCE:
      unreachable("not an atomic op");
   }

   unreachable("invalid LSC op");
}

static inline uint32_t
lsc_data_size_bytes(enum lsc_data_size data_size)
{
   switch (data_size) {
   case LSC_DATA_SIZE_D8:
      return 1;
   case LSC_DATA_SIZE_D16:
      return 2;
   case LSC_DATA_SIZE_D32:
   case LSC_DATA_SIZE_D8U32:
   case LSC_DATA_SIZE_D16U32:
   case LSC_DATA_SIZE_D16BF32:
      return 4;
   case LSC_DATA_SIZE_D64:
      return 8;
   default:
      unreachable("Unsupported data payload size.");
   }
}

static inline uint32_t
lsc_addr_size_bytes(enum lsc_addr_size addr_size)
{
   switch (addr_size) {
   case LSC_ADDR_SIZE_A16: return 2;
   case LSC_ADDR_SIZE_A32: return 4;
   case LSC_ADDR_SIZE_A64: return 8;
   default:
      unreachable("Unsupported address size.");
   }
}

static inline uint32_t
lsc_vector_length(enum lsc_vect_size vect_size)
{
   switch (vect_size) {
   case LSC_VECT_SIZE_V1: return 1;
   case LSC_VECT_SIZE_V2: return 2;
   case LSC_VECT_SIZE_V3: return 3;
   case LSC_VECT_SIZE_V4: return 4;
   case LSC_VECT_SIZE_V8: return 8;
   case LSC_VECT_SIZE_V16: return 16;
   case LSC_VECT_SIZE_V32: return 32;
   case LSC_VECT_SIZE_V64: return 64;
   default:
      unreachable("Unsupported size of vector");
   }
}

static inline enum lsc_vect_size
lsc_vect_size(unsigned vect_size)
{
   switch(vect_size) {
   case 1:  return LSC_VECT_SIZE_V1;
   case 2:  return LSC_VECT_SIZE_V2;
   case 3:  return LSC_VECT_SIZE_V3;
   case 4:  return LSC_VECT_SIZE_V4;
   case 8:  return LSC_VECT_SIZE_V8;
   case 16: return LSC_VECT_SIZE_V16;
   case 32: return LSC_VECT_SIZE_V32;
   case 64: return LSC_VECT_SIZE_V64;
   default:
      unreachable("Unsupported vector size for dataport");
   }
}

static inline uint32_t
lsc_msg_desc_wcmask(const struct intel_device_info *devinfo,
             enum lsc_opcode opcode,
             enum lsc_addr_surface_type addr_type,
             enum lsc_addr_size addr_sz,
             enum lsc_data_size data_sz, unsigned num_channels,
             bool transpose, unsigned cache_ctrl, unsigned cmask)
{
   assert(devinfo->has_lsc);
   assert(!transpose || lsc_opcode_has_transpose(opcode));

   unsigned msg_desc =
      SET_BITS(opcode, 5, 0) |
      SET_BITS(addr_sz, 8, 7) |
      SET_BITS(data_sz, 11, 9) |
      SET_BITS(transpose, 15, 15) |
      (devinfo->ver >= 20 ? SET_BITS(cache_ctrl, 19, 16) :
                            SET_BITS(cache_ctrl, 19, 17)) |
      SET_BITS(addr_type, 30, 29);

   if (lsc_opcode_has_cmask(opcode))
      msg_desc |= SET_BITS(cmask ? cmask : lsc_cmask(num_channels), 15, 12);
   else
      msg_desc |= SET_BITS(lsc_vect_size(num_channels), 14, 12);

   return msg_desc;
}

static inline uint32_t
lsc_msg_desc(UNUSED const struct intel_device_info *devinfo,
             enum lsc_opcode opcode,
             enum lsc_addr_surface_type addr_type,
             enum lsc_addr_size addr_sz,
             enum lsc_data_size data_sz, unsigned num_channels,
             bool transpose, unsigned cache_ctrl)
{
   return lsc_msg_desc_wcmask(devinfo, opcode, addr_type, addr_sz,
                              data_sz, num_channels, transpose, cache_ctrl, 0);
}

static inline enum lsc_opcode
lsc_msg_desc_opcode(UNUSED const struct intel_device_info *devinfo,
                    uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_opcode) GET_BITS(desc, 5, 0);
}

static inline enum lsc_addr_size
lsc_msg_desc_addr_size(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_addr_size) GET_BITS(desc, 8, 7);
}

static inline enum lsc_data_size
lsc_msg_desc_data_size(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_data_size) GET_BITS(desc, 11, 9);
}

static inline enum lsc_vect_size
lsc_msg_desc_vect_size(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   assert(devinfo->has_lsc);
   assert(!lsc_opcode_has_cmask(lsc_msg_desc_opcode(devinfo, desc)));
   return (enum lsc_vect_size) GET_BITS(desc, 14, 12);
}

static inline enum lsc_cmask
lsc_msg_desc_cmask(UNUSED const struct intel_device_info *devinfo,
                   uint32_t desc)
{
   assert(devinfo->has_lsc);
   assert(lsc_opcode_has_cmask(lsc_msg_desc_opcode(devinfo, desc)));
   return (enum lsc_cmask) GET_BITS(desc, 15, 12);
}

static inline bool
lsc_msg_desc_transpose(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(desc, 15, 15);
}

static inline unsigned
lsc_msg_desc_cache_ctrl(UNUSED const struct intel_device_info *devinfo,
                        uint32_t desc)
{
   assert(devinfo->has_lsc);
   return devinfo->ver >= 20 ? GET_BITS(desc, 19, 16) : GET_BITS(desc, 19, 17);
}

static inline unsigned
lsc_msg_dest_len(const struct intel_device_info *devinfo,
                 enum lsc_data_size data_sz, unsigned n)
{
   return DIV_ROUND_UP(lsc_data_size_bytes(data_sz) * n,
                       reg_unit(devinfo) * REG_SIZE) * reg_unit(devinfo);
}

static inline unsigned
lsc_msg_addr_len(const struct intel_device_info *devinfo,
                 enum lsc_addr_size addr_sz, unsigned n)
{
   return DIV_ROUND_UP(lsc_addr_size_bytes(addr_sz) * n,
                       reg_unit(devinfo) * REG_SIZE) * reg_unit(devinfo);
}

static inline enum lsc_addr_surface_type
lsc_msg_desc_addr_type(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_addr_surface_type) GET_BITS(desc, 30, 29);
}

static inline uint32_t
lsc_fence_msg_desc(UNUSED const struct intel_device_info *devinfo,
                   enum lsc_fence_scope scope,
                   enum lsc_flush_type flush_type,
                   bool route_to_lsc)
{
   assert(devinfo->has_lsc);

#if INTEL_NEEDS_WA_22017182272
   assert(flush_type != LSC_FLUSH_TYPE_DISCARD);
#endif

   return SET_BITS(LSC_OP_FENCE, 5, 0) |
          SET_BITS(LSC_ADDR_SIZE_A32, 8, 7) |
          SET_BITS(scope, 11, 9) |
          SET_BITS(flush_type, 14, 12) |
          SET_BITS(route_to_lsc, 18, 18) |
          SET_BITS(LSC_ADDR_SURFTYPE_FLAT, 30, 29);
}

static inline enum lsc_fence_scope
lsc_fence_msg_desc_scope(UNUSED const struct intel_device_info *devinfo,
                         uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_fence_scope) GET_BITS(desc, 11, 9);
}

static inline enum lsc_flush_type
lsc_fence_msg_desc_flush_type(UNUSED const struct intel_device_info *devinfo,
                              uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_flush_type) GET_BITS(desc, 14, 12);
}

static inline enum lsc_backup_fence_routing
lsc_fence_msg_desc_backup_routing(UNUSED const struct intel_device_info *devinfo,
                                  uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum lsc_backup_fence_routing) GET_BITS(desc, 18, 18);
}

static inline uint32_t
lsc_bti_ex_desc(const struct intel_device_info *devinfo, unsigned bti)
{
   assert(devinfo->has_lsc);
   return SET_BITS(bti, 31, 24) |
          SET_BITS(0, 23, 12);  /* base offset */
}

static inline unsigned
lsc_bti_ex_desc_base_offset(const struct intel_device_info *devinfo,
                            uint32_t ex_desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(ex_desc, 23, 12);
}

static inline unsigned
lsc_bti_ex_desc_index(const struct intel_device_info *devinfo,
                      uint32_t ex_desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(ex_desc, 31, 24);
}

static inline unsigned
lsc_flat_ex_desc_base_offset(const struct intel_device_info *devinfo,
                             uint32_t ex_desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(ex_desc, 31, 12);
}

static inline uint32_t
lsc_bss_ex_desc(const struct intel_device_info *devinfo,
                unsigned surface_state_index)
{
   assert(devinfo->has_lsc);
   return SET_BITS(surface_state_index, 31, 6);
}

static inline unsigned
lsc_bss_ex_desc_index(const struct intel_device_info *devinfo,
                      uint32_t ex_desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(ex_desc, 31, 6);
}

static inline uint32_t
brw_mdc_sm2(unsigned exec_size)
{
   assert(exec_size == 8 || exec_size == 16);
   return exec_size > 8;
}

static inline uint32_t
brw_mdc_sm2_exec_size(uint32_t sm2)
{
   assert(sm2 <= 1);
   return 8 << sm2;
}

static inline uint32_t
brw_btd_spawn_desc(ASSERTED const struct intel_device_info *devinfo,
                   unsigned exec_size, unsigned msg_type)
{
   assert(devinfo->has_ray_tracing);
   assert(devinfo->ver < 20 || exec_size == 16);

   return SET_BITS(0, 19, 19) | /* No header */
          SET_BITS(msg_type, 17, 14) |
          SET_BITS(brw_mdc_sm2(exec_size), 8, 8);
}

static inline uint32_t
brw_btd_spawn_msg_type(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   return GET_BITS(desc, 17, 14);
}

static inline uint32_t
brw_btd_spawn_exec_size(UNUSED const struct intel_device_info *devinfo,
                        uint32_t desc)
{
   return brw_mdc_sm2_exec_size(GET_BITS(desc, 8, 8));
}

static inline uint32_t
brw_rt_trace_ray_desc(ASSERTED const struct intel_device_info *devinfo,
                      unsigned exec_size)
{
   assert(devinfo->has_ray_tracing);
   assert(devinfo->ver < 20 || exec_size == 16);

   return SET_BITS(0, 19, 19) | /* No header */
          SET_BITS(0, 17, 14) | /* Message type */
          SET_BITS(brw_mdc_sm2(exec_size), 8, 8);
}

static inline uint32_t
brw_rt_trace_ray_desc_exec_size(UNUSED const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return brw_mdc_sm2_exec_size(GET_BITS(desc, 8, 8));
}

/**
 * Construct a message descriptor immediate with the specified pixel
 * interpolator function controls.
 */
static inline uint32_t
brw_pixel_interp_desc(UNUSED const struct intel_device_info *devinfo,
                      unsigned msg_type,
                      bool noperspective,
                      bool coarse_pixel_rate,
                      unsigned exec_size,
                      unsigned group)
{
   assert(exec_size == 8 || exec_size == 16);
   const bool simd_mode = exec_size == 16;
   const bool slot_group = group >= 16;

   assert(devinfo->ver >= 10 || !coarse_pixel_rate);
   return (SET_BITS(slot_group, 11, 11) |
           SET_BITS(msg_type, 13, 12) |
           SET_BITS(!!noperspective, 14, 14) |
           SET_BITS(coarse_pixel_rate, 15, 15) |
           SET_BITS(simd_mode, 16, 16));
}

/**
 * Send message to shared unit \p sfid with a possibly indirect descriptor \p
 * desc.  If \p desc is not an immediate it will be transparently loaded to an
 * address register using an OR instruction.
 */
void
brw_send_indirect_message(struct brw_codegen *p,
                          unsigned sfid,
                          struct brw_reg dst,
                          struct brw_reg payload,
                          struct brw_reg desc,
                          unsigned desc_imm,
                          bool eot);

void
brw_send_indirect_split_message(struct brw_codegen *p,
                                unsigned sfid,
                                struct brw_reg dst,
                                struct brw_reg payload0,
                                struct brw_reg payload1,
                                struct brw_reg desc,
                                unsigned desc_imm,
                                struct brw_reg ex_desc,
                                unsigned ex_desc_imm,
                                bool ex_desc_scratch,
                                bool ex_bso,
                                bool eot);

void gfx6_math(struct brw_codegen *p,
	       struct brw_reg dest,
	       unsigned function,
	       struct brw_reg src0,
	       struct brw_reg src1);

/**
 * Return the generation-specific jump distance scaling factor.
 *
 * Given the number of instructions to jump, we need to scale by
 * some number to obtain the actual jump distance to program in an
 * instruction.
 */
static inline unsigned
brw_jump_scale(const struct intel_device_info *devinfo)
{
   /* Broadwell measures jump targets in bytes. */
   return 16;
}

void brw_barrier(struct brw_codegen *p, struct brw_reg src);

/* If/else/endif.  Works by manipulating the execution flags on each
 * channel.
 */
brw_inst *brw_IF(struct brw_codegen *p, unsigned execute_size);

void brw_ELSE(struct brw_codegen *p);
void brw_ENDIF(struct brw_codegen *p);

/* DO/WHILE loops:
 */
brw_inst *brw_DO(struct brw_codegen *p, unsigned execute_size);

brw_inst *brw_WHILE(struct brw_codegen *p);

brw_inst *brw_BREAK(struct brw_codegen *p);
brw_inst *brw_CONT(struct brw_codegen *p);
brw_inst *brw_HALT(struct brw_codegen *p);

/* Forward jumps:
 */
brw_inst *brw_JMPI(struct brw_codegen *p, struct brw_reg index,
                   unsigned predicate_control);

void brw_NOP(struct brw_codegen *p);

void brw_WAIT(struct brw_codegen *p);

void brw_SYNC(struct brw_codegen *p, enum tgl_sync_function func);

/* Special case: there is never a destination, execution size will be
 * taken from src0:
 */
void brw_CMP(struct brw_codegen *p,
	     struct brw_reg dest,
	     unsigned conditional,
	     struct brw_reg src0,
	     struct brw_reg src1);

void brw_CMPN(struct brw_codegen *p,
              struct brw_reg dest,
              unsigned conditional,
              struct brw_reg src0,
              struct brw_reg src1);

brw_inst *brw_DPAS(struct brw_codegen *p, enum gfx12_systolic_depth sdepth,
                   unsigned rcount, struct brw_reg dest, struct brw_reg src0,
                   struct brw_reg src1, struct brw_reg src2);

void
brw_memory_fence(struct brw_codegen *p,
                 struct brw_reg dst,
                 struct brw_reg src,
                 enum opcode send_op,
                 enum brw_message_target sfid,
                 uint32_t desc,
                 bool commit_enable,
                 unsigned bti);

void
brw_broadcast(struct brw_codegen *p,
              struct brw_reg dst,
              struct brw_reg src,
              struct brw_reg idx);

void
brw_float_controls_mode(struct brw_codegen *p,
                        unsigned mode, unsigned mask);

void
brw_update_reloc_imm(const struct brw_isa_info *isa,
                     brw_inst *inst,
                     uint32_t value);

void
brw_MOV_reloc_imm(struct brw_codegen *p,
                  struct brw_reg dst,
                  enum brw_reg_type src_type,
                  uint32_t id);

unsigned
brw_num_sources_from_inst(const struct brw_isa_info *isa,
                          const brw_inst *inst);

void brw_set_src1(struct brw_codegen *p, brw_inst *insn, struct brw_reg reg);

void brw_set_desc_ex(struct brw_codegen *p, brw_inst *insn,
                     unsigned desc, unsigned ex_desc);

static inline void
brw_set_desc(struct brw_codegen *p, brw_inst *insn, unsigned desc)
{
   brw_set_desc_ex(p, insn, desc, 0);
}

void brw_set_uip_jip(struct brw_codegen *p, int start_offset);

enum brw_conditional_mod brw_negate_cmod(enum brw_conditional_mod cmod);
enum brw_conditional_mod brw_swap_cmod(enum brw_conditional_mod cmod);

/* brw_eu_compact.c */
void brw_compact_instructions(struct brw_codegen *p, int start_offset,
                              struct disasm_info *disasm);
void brw_uncompact_instruction(const struct brw_isa_info *isa,
                               brw_inst *dst, brw_compact_inst *src);
bool brw_try_compact_instruction(const struct brw_isa_info *isa,
                                 brw_compact_inst *dst, const brw_inst *src);

void brw_debug_compact_uncompact(const struct brw_isa_info *isa,
                                 brw_inst *orig, brw_inst *uncompacted);

/* brw_eu_validate.c */
bool brw_validate_instruction(const struct brw_isa_info *isa,
                              const brw_inst *inst, int offset,
                              unsigned inst_size,
                              struct disasm_info *disasm);
bool brw_validate_instructions(const struct brw_isa_info *isa,
                               const void *assembly, int start_offset, int end_offset,
                               struct disasm_info *disasm);

static inline int
next_offset(const struct intel_device_info *devinfo, void *store, int offset)
{
   brw_inst *insn = (brw_inst *)((char *)store + offset);

   if (brw_inst_cmpt_control(devinfo, insn))
      return offset + 8;
   else
      return offset + 16;
}

/** Maximum SEND message length */
#define BRW_MAX_MSG_LENGTH 15

#ifdef __cplusplus
}
#endif

#endif
