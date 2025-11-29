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


#ifndef ELK_EU_H
#define ELK_EU_H

#include <stdbool.h>
#include <stdio.h>
#include "elk_inst.h"
#include "elk_compiler.h"
#include "elk_eu_defines.h"
#include "elk_isa_info.h"
#include "elk_reg.h"

#include "util/bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

struct elk_disasm_info;

#define ELK_EU_MAX_INSN_STACK 5

struct elk_insn_state {
   /* One of ELK_EXECUTE_* */
   unsigned exec_size:3;

   /* Group in units of channels */
   unsigned group:5;

   /* Compression control on gfx4-5 */
   bool compressed:1;

   /* One of ELK_MASK_* */
   unsigned mask_control:1;

   bool saturate:1;

   /* One of ELK_ALIGN_* */
   unsigned access_mode:1;

   /* One of ELK_PREDICATE_* */
   enum elk_predicate predicate:4;

   bool pred_inv:1;

   /* Flag subreg.  Bottom bit is subreg, top bit is reg */
   unsigned flag_subreg:2;

   bool acc_wr_control:1;
};


/* A helper for accessing the last instruction emitted.  This makes it easy
 * to set various bits on an instruction without having to create temporary
 * variable and assign the emitted instruction to those.
 */
#define elk_last_inst (&p->store[p->nr_insn - 1])

struct elk_codegen {
   elk_inst *store;
   int store_size;
   unsigned nr_insn;
   unsigned int next_insn_offset;

   void *mem_ctx;

   /* Allow clients to push/pop instruction state:
    */
   struct elk_insn_state stack[ELK_EU_MAX_INSN_STACK];
   struct elk_insn_state *current;

   /** Whether or not the user wants automatic exec sizes
    *
    * If true, codegen will try to automatically infer the exec size of an
    * instruction from the width of the destination register.  If false, it
    * will take whatever is set by elk_set_default_exec_size verbatim.
    *
    * This is set to true by default in elk_init_codegen.
    */
   bool automatic_exec_sizes;

   bool single_program_flow;
   const struct elk_isa_info *isa;
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
   /**
    * pre-gfx6, the BREAK and CONT instructions had to tell how many IF/ENDIF
    * blocks they were popping out of, to fix up the mask stack.  This tracks
    * the IF/ENDIF nesting in each current nested loop level.
    */
   int *if_depth_in_loop;
   int loop_stack_depth;
   int loop_stack_array_size;

   struct elk_shader_reloc *relocs;
   int num_relocs;
   int reloc_array_size;
};

struct elk_label {
   int offset;
   int number;
   struct elk_label *next;
};

void elk_pop_insn_state( struct elk_codegen *p );
void elk_push_insn_state( struct elk_codegen *p );
unsigned elk_get_default_exec_size(struct elk_codegen *p);
unsigned elk_get_default_group(struct elk_codegen *p);
unsigned elk_get_default_access_mode(struct elk_codegen *p);
void elk_set_default_exec_size(struct elk_codegen *p, unsigned value);
void elk_set_default_mask_control( struct elk_codegen *p, unsigned value );
void elk_set_default_saturate( struct elk_codegen *p, bool enable );
void elk_set_default_access_mode( struct elk_codegen *p, unsigned access_mode );
void elk_inst_set_compression(const struct intel_device_info *devinfo,
                              elk_inst *inst, bool on);
void elk_set_default_compression(struct elk_codegen *p, bool on);
void elk_inst_set_group(const struct intel_device_info *devinfo,
                        elk_inst *inst, unsigned group);
void elk_set_default_group(struct elk_codegen *p, unsigned group);
void elk_set_default_compression_control(struct elk_codegen *p, enum elk_compression c);
void elk_set_default_predicate_control(struct elk_codegen *p, enum elk_predicate pc);
void elk_set_default_predicate_inverse(struct elk_codegen *p, bool predicate_inverse);
void elk_set_default_flag_reg(struct elk_codegen *p, int reg, int subreg);
void elk_set_default_acc_write_control(struct elk_codegen *p, unsigned value);

void elk_init_codegen(const struct elk_isa_info *isa,
                      struct elk_codegen *p, void *mem_ctx);
bool elk_has_jip(const struct intel_device_info *devinfo, enum elk_opcode opcode);
bool elk_has_uip(const struct intel_device_info *devinfo, enum elk_opcode opcode);
const struct elk_shader_reloc *elk_get_shader_relocs(struct elk_codegen *p,
                                                     unsigned *num_relocs);
const unsigned *elk_get_program( struct elk_codegen *p, unsigned *sz );

bool elk_should_dump_shader_bin(void);
void elk_dump_shader_bin(void *assembly, int start_offset, int end_offset,
                         const char *identifier);

bool elk_try_override_assembly(struct elk_codegen *p, int start_offset,
                               const char *identifier);

void elk_realign(struct elk_codegen *p, unsigned alignment);
int elk_append_data(struct elk_codegen *p, void *data,
                    unsigned size, unsigned alignment);
elk_inst *elk_next_insn(struct elk_codegen *p, unsigned opcode);
void elk_add_reloc(struct elk_codegen *p, uint32_t id,
                   enum elk_shader_reloc_type type,
                   uint32_t offset, uint32_t delta);
void elk_set_dest(struct elk_codegen *p, elk_inst *insn, struct elk_reg dest);
void elk_set_src0(struct elk_codegen *p, elk_inst *insn, struct elk_reg reg);

void elk_gfx6_resolve_implied_move(struct elk_codegen *p,
			       struct elk_reg *src,
			       unsigned msg_reg_nr);

/* Helpers for regular instructions:
 */
#define ALU1(OP)				\
elk_inst *elk_##OP(struct elk_codegen *p,	\
	      struct elk_reg dest,		\
	      struct elk_reg src0);

#define ALU2(OP)				\
elk_inst *elk_##OP(struct elk_codegen *p,	\
	      struct elk_reg dest,		\
	      struct elk_reg src0,		\
	      struct elk_reg src1);

#define ALU3(OP)				\
elk_inst *elk_##OP(struct elk_codegen *p,	\
	      struct elk_reg dest,		\
	      struct elk_reg src0,		\
	      struct elk_reg src1,		\
	      struct elk_reg src2);

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
   return 1;
}


/* Helpers for SEND instruction:
 */

/**
 * Construct a message descriptor immediate with the specified common
 * descriptor controls.
 */
static inline uint32_t
elk_message_desc(const struct intel_device_info *devinfo,
                 unsigned msg_length,
                 unsigned response_length,
                 bool header_present)
{
   if (devinfo->ver >= 5) {
      assert(msg_length % reg_unit(devinfo) == 0);
      assert(response_length % reg_unit(devinfo) == 0);
      return (SET_BITS(msg_length / reg_unit(devinfo), 28, 25) |
              SET_BITS(response_length / reg_unit(devinfo), 24, 20) |
              SET_BITS(header_present, 19, 19));
   } else {
      return (SET_BITS(msg_length, 23, 20) |
              SET_BITS(response_length, 19, 16));
   }
}

static inline unsigned
elk_message_desc_mlen(const struct intel_device_info *devinfo, uint32_t desc)
{
   if (devinfo->ver >= 5)
      return GET_BITS(desc, 28, 25) * reg_unit(devinfo);
   else
      return GET_BITS(desc, 23, 20);
}

static inline unsigned
elk_message_desc_rlen(const struct intel_device_info *devinfo, uint32_t desc)
{
   if (devinfo->ver >= 5)
      return GET_BITS(desc, 24, 20) * reg_unit(devinfo);
   else
      return GET_BITS(desc, 19, 16);
}

static inline bool
elk_message_desc_header_present(ASSERTED
                                const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   assert(devinfo->ver >= 5);
   return GET_BITS(desc, 19, 19);
}

static inline unsigned
elk_message_ex_desc(const struct intel_device_info *devinfo,
                    unsigned ex_msg_length)
{
   assert(ex_msg_length % reg_unit(devinfo) == 0);
   return SET_BITS(ex_msg_length / reg_unit(devinfo), 9, 6);
}

static inline unsigned
elk_message_ex_desc_ex_mlen(const struct intel_device_info *devinfo,
                            uint32_t ex_desc)
{
   return GET_BITS(ex_desc, 9, 6) * reg_unit(devinfo);
}

static inline uint32_t
elk_urb_desc(const struct intel_device_info *devinfo,
             unsigned msg_type,
             bool per_slot_offset_present,
             bool channel_mask_present,
             unsigned global_offset)
{
   if (devinfo->ver >= 8) {
      return (SET_BITS(per_slot_offset_present, 17, 17) |
              SET_BITS(channel_mask_present, 15, 15) |
              SET_BITS(global_offset, 14, 4) |
              SET_BITS(msg_type, 3, 0));
   } else if (devinfo->ver >= 7) {
      assert(!channel_mask_present);
      return (SET_BITS(per_slot_offset_present, 16, 16) |
              SET_BITS(global_offset, 13, 3) |
              SET_BITS(msg_type, 3, 0));
   } else {
      unreachable("unhandled URB write generation");
   }
}

static inline uint32_t
elk_urb_desc_msg_type(ASSERTED const struct intel_device_info *devinfo,
                      uint32_t desc)
{
   assert(devinfo->ver >= 7);
   return GET_BITS(desc, 3, 0);
}

/**
 * Construct a message descriptor immediate with the specified sampler
 * function controls.
 */
static inline uint32_t
elk_sampler_desc(const struct intel_device_info *devinfo,
                 unsigned binding_table_index,
                 unsigned sampler,
                 unsigned msg_type,
                 unsigned simd_mode,
                 unsigned return_format)
{
   const unsigned desc = (SET_BITS(binding_table_index, 7, 0) |
                          SET_BITS(sampler, 11, 8));

   /* From the CHV Bspec: Shared Functions - Message Descriptor -
    * Sampling Engine:
    *
    *   SIMD Mode[2]  29    This field is the upper bit of the 3-bit
    *                       SIMD Mode field.
    */
   if (devinfo->ver >= 8)
      return desc | SET_BITS(msg_type, 16, 12) |
             SET_BITS(simd_mode & 0x3, 18, 17) |
             SET_BITS(simd_mode >> 2, 29, 29) |
             SET_BITS(return_format, 30, 30);
   if (devinfo->ver >= 7)
      return (desc | SET_BITS(msg_type, 16, 12) |
              SET_BITS(simd_mode, 18, 17));
   else if (devinfo->ver >= 5)
      return (desc | SET_BITS(msg_type, 15, 12) |
              SET_BITS(simd_mode, 17, 16));
   else if (devinfo->verx10 >= 45)
      return desc | SET_BITS(msg_type, 15, 12);
   else
      return (desc | SET_BITS(return_format, 13, 12) |
              SET_BITS(msg_type, 15, 14));
}

static inline unsigned
elk_sampler_desc_binding_table_index(UNUSED
                                     const struct intel_device_info *devinfo,
                                     uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline unsigned
elk_sampler_desc_sampler(UNUSED const struct intel_device_info *devinfo,
                         uint32_t desc)
{
   return GET_BITS(desc, 11, 8);
}

static inline unsigned
elk_sampler_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   if (devinfo->ver >= 7)
      return GET_BITS(desc, 16, 12);
   else if (devinfo->verx10 >= 45)
      return GET_BITS(desc, 15, 12);
   else
      return GET_BITS(desc, 15, 14);
}

static inline unsigned
elk_sampler_desc_simd_mode(const struct intel_device_info *devinfo,
                           uint32_t desc)
{
   assert(devinfo->ver >= 5);
   if (devinfo->ver >= 8)
      return GET_BITS(desc, 18, 17) | GET_BITS(desc, 29, 29) << 2;
   else if (devinfo->ver >= 7)
      return GET_BITS(desc, 18, 17);
   else
      return GET_BITS(desc, 17, 16);
}

static  inline unsigned
elk_sampler_desc_return_format(ASSERTED const struct intel_device_info *devinfo,
                               uint32_t desc)
{
   assert(devinfo->verx10 == 40 || devinfo->ver >= 8);
   if (devinfo->ver >= 8)
      return GET_BITS(desc, 30, 30);
   else
      return GET_BITS(desc, 13, 12);
}

/**
 * Construct a message descriptor for the dataport
 */
static inline uint32_t
elk_dp_desc(const struct intel_device_info *devinfo,
            unsigned binding_table_index,
            unsigned msg_type,
            unsigned msg_control)
{
   /* Prior to gfx6, things are too inconsistent; use the dp_read/write_desc
    * helpers instead.
    */
   assert(devinfo->ver >= 6);
   const unsigned desc = SET_BITS(binding_table_index, 7, 0);
   if (devinfo->ver >= 8) {
      return (desc | SET_BITS(msg_control, 13, 8) |
              SET_BITS(msg_type, 18, 14));
   } else if (devinfo->ver >= 7) {
      return (desc | SET_BITS(msg_control, 13, 8) |
              SET_BITS(msg_type, 17, 14));
   } else {
      return (desc | SET_BITS(msg_control, 12, 8) |
              SET_BITS(msg_type, 16, 13));
   }
}

static inline unsigned
elk_dp_desc_binding_table_index(UNUSED const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline unsigned
elk_dp_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   assert(devinfo->ver >= 6);
   if (devinfo->ver >= 8)
      return GET_BITS(desc, 18, 14);
   else if (devinfo->ver >= 7)
      return GET_BITS(desc, 17, 14);
   else
      return GET_BITS(desc, 16, 13);
}

static inline unsigned
elk_dp_desc_msg_control(const struct intel_device_info *devinfo, uint32_t desc)
{
   assert(devinfo->ver >= 6);
   if (devinfo->ver >= 7)
      return GET_BITS(desc, 13, 8);
   else
      return GET_BITS(desc, 12, 8);
}

/**
 * Construct a message descriptor immediate with the specified dataport read
 * function controls.
 */
static inline uint32_t
elk_dp_read_desc(const struct intel_device_info *devinfo,
                 unsigned binding_table_index,
                 unsigned msg_control,
                 unsigned msg_type,
                 unsigned target_cache)
{
   if (devinfo->ver >= 6)
      return elk_dp_desc(devinfo, binding_table_index, msg_type, msg_control);
   else if (devinfo->verx10 >= 45)
      return (SET_BITS(binding_table_index, 7, 0) |
              SET_BITS(msg_control, 10, 8) |
              SET_BITS(msg_type, 13, 11) |
              SET_BITS(target_cache, 15, 14));
   else
      return (SET_BITS(binding_table_index, 7, 0) |
              SET_BITS(msg_control, 11, 8) |
              SET_BITS(msg_type, 13, 12) |
              SET_BITS(target_cache, 15, 14));
}

static inline unsigned
elk_dp_read_desc_msg_type(const struct intel_device_info *devinfo,
                          uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_dp_desc_msg_type(devinfo, desc);
   else if (devinfo->verx10 >= 45)
      return GET_BITS(desc, 13, 11);
   else
      return GET_BITS(desc, 13, 12);
}

static inline unsigned
elk_dp_read_desc_msg_control(const struct intel_device_info *devinfo,
                             uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_dp_desc_msg_control(devinfo, desc);
   else if (devinfo->verx10 >= 45)
      return GET_BITS(desc, 10, 8);
   else
      return GET_BITS(desc, 11, 8);
}

/**
 * Construct a message descriptor immediate with the specified dataport write
 * function controls.
 */
static inline uint32_t
elk_dp_write_desc(const struct intel_device_info *devinfo,
                  unsigned binding_table_index,
                  unsigned msg_control,
                  unsigned msg_type,
                  unsigned send_commit_msg)
{
   assert(devinfo->ver <= 6 || !send_commit_msg);
   if (devinfo->ver >= 6) {
      return elk_dp_desc(devinfo, binding_table_index, msg_type, msg_control) |
             SET_BITS(send_commit_msg, 17, 17);
   } else {
      return (SET_BITS(binding_table_index, 7, 0) |
              SET_BITS(msg_control, 11, 8) |
              SET_BITS(msg_type, 14, 12) |
              SET_BITS(send_commit_msg, 15, 15));
   }
}

static inline unsigned
elk_dp_write_desc_msg_type(const struct intel_device_info *devinfo,
                           uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_dp_desc_msg_type(devinfo, desc);
   else
      return GET_BITS(desc, 14, 12);
}

static inline unsigned
elk_dp_write_desc_msg_control(const struct intel_device_info *devinfo,
                              uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_dp_desc_msg_control(devinfo, desc);
   else
      return GET_BITS(desc, 11, 8);
}

static inline bool
elk_dp_write_desc_write_commit(const struct intel_device_info *devinfo,
                               uint32_t desc)
{
   assert(devinfo->ver <= 6);
   if (devinfo->ver >= 6)
      return GET_BITS(desc, 17, 17);
   else
      return GET_BITS(desc, 15, 15);
}

/**
 * Construct a message descriptor immediate with the specified dataport
 * surface function controls.
 */
static inline uint32_t
elk_dp_surface_desc(const struct intel_device_info *devinfo,
                    unsigned msg_type,
                    unsigned msg_control)
{
   assert(devinfo->ver >= 7);
   /* We'll OR in the binding table index later */
   return elk_dp_desc(devinfo, 0, msg_type, msg_control);
}

static inline uint32_t
elk_dp_untyped_atomic_desc(const struct intel_device_info *devinfo,
                           unsigned exec_size, /**< 0 for SIMD4x2 */
                           unsigned atomic_op,
                           bool response_expected)
{
   assert(exec_size <= 8 || exec_size == 16);

   unsigned msg_type;
   if (devinfo->verx10 >= 75) {
      if (exec_size > 0) {
         msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP;
      } else {
         msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2;
      }
   } else {
      msg_type = GFX7_DATAPORT_DC_UNTYPED_ATOMIC_OP;
   }

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(0 < exec_size && exec_size <= 8, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline unsigned
elk_mdc_cmask(unsigned num_channels)
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
elk_dp_untyped_surface_rw_desc(const struct intel_device_info *devinfo,
                               unsigned exec_size, /**< 0 for SIMD4x2 */
                               unsigned num_channels,
                               bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   unsigned msg_type;
   if (write) {
      if (devinfo->verx10 >= 75) {
         msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE;
      } else {
         msg_type = GFX7_DATAPORT_DC_UNTYPED_SURFACE_WRITE;
      }
   } else {
      /* Read */
      if (devinfo->verx10 >= 75) {
         msg_type = HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ;
      } else {
         msg_type = GFX7_DATAPORT_DC_UNTYPED_SURFACE_READ;
      }
   }

   /* SIMD4x2 is only valid for read messages on IVB; use SIMD8 instead */
   if (write && devinfo->verx10 == 70 && exec_size == 0)
      exec_size = 8;

   /* See also MDC_SM3 in the SKL PRM Vol 2d. */
   const unsigned simd_mode = exec_size == 0 ? 0 : /* SIMD4x2 */
                              exec_size <= 8 ? 2 : 1;

   const unsigned msg_control =
      SET_BITS(elk_mdc_cmask(num_channels), 3, 0) |
      SET_BITS(simd_mode, 5, 4);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline unsigned
elk_mdc_ds(unsigned bit_size)
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
elk_dp_byte_scattered_rw_desc(const struct intel_device_info *devinfo,
                              unsigned exec_size,
                              unsigned bit_size,
                              bool write)
{
   assert(exec_size <= 8 || exec_size == 16);

   assert(devinfo->verx10 >= 75);
   const unsigned msg_type =
      write ? HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_WRITE :
              HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_READ;

   assert(exec_size > 0);
   const unsigned msg_control =
      SET_BITS(exec_size == 16, 0, 0) |
      SET_BITS(elk_mdc_ds(bit_size), 3, 2);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
elk_dp_dword_scattered_rw_desc(const struct intel_device_info *devinfo,
                               unsigned exec_size,
                               bool write)
{
   assert(exec_size == 8 || exec_size == 16);

   unsigned msg_type;
   if (write) {
      if (devinfo->ver >= 6) {
         msg_type = GFX6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE;
      } else {
         msg_type = ELK_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE;
      }
   } else {
      if (devinfo->ver >= 7) {
         msg_type = GFX7_DATAPORT_DC_DWORD_SCATTERED_READ;
      } else if (devinfo->verx10 >= 45) {
         msg_type = G45_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ;
      } else {
         msg_type = ELK_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ;
      }
   }

   const unsigned msg_control =
      SET_BITS(1, 1, 1) | /* Legacy SIMD Mode */
      SET_BITS(exec_size == 16, 0, 0);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
elk_dp_oword_block_rw_desc(const struct intel_device_info *devinfo,
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
      SET_BITS(ELK_DATAPORT_OWORD_BLOCK_DWORDS(num_dwords), 2, 0);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
elk_dp_a64_untyped_surface_rw_desc(const struct intel_device_info *devinfo,
                                   unsigned exec_size, /**< 0 for SIMD4x2 */
                                   unsigned num_channels,
                                   bool write)
{
   assert(exec_size <= 8 || exec_size == 16);
   assert(devinfo->ver >= 8);

   unsigned msg_type =
      write ? GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_WRITE :
              GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_SURFACE_READ;

   /* See also MDC_SM3 in the SKL PRM Vol 2d. */
   const unsigned simd_mode = exec_size == 0 ? 0 : /* SIMD4x2 */
                              exec_size <= 8 ? 2 : 1;

   const unsigned msg_control =
      SET_BITS(elk_mdc_cmask(num_channels), 3, 0) |
      SET_BITS(simd_mode, 5, 4);

   return elk_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
elk_dp_a64_oword_block_rw_desc(const struct intel_device_info *devinfo,
                               bool align_16B,
                               unsigned num_dwords,
                               bool write)
{
   /* Writes can only have addresses aligned by OWORDs (16 Bytes). */
   assert(!write || align_16B);

   unsigned msg_type =
      write ? GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_WRITE :
              GFX8_DATAPORT_DC_PORT1_A64_OWORD_BLOCK_READ;

   unsigned msg_control =
      SET_BITS(!align_16B, 4, 3) |
      SET_BITS(ELK_DATAPORT_OWORD_BLOCK_DWORDS(num_dwords), 2, 0);

   return elk_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

/**
 * Calculate the data size (see MDC_A64_DS in the "Structures" volume of the
 * Skylake PRM).
 */
static inline uint32_t
elk_mdc_a64_ds(unsigned elems)
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
elk_dp_a64_byte_scattered_rw_desc(const struct intel_device_info *devinfo,
                                  unsigned exec_size, /**< 0 for SIMD4x2 */
                                  unsigned bit_size,
                                  bool write)
{
   assert(exec_size <= 8 || exec_size == 16);
   assert(devinfo->ver >= 8);

   unsigned msg_type =
      write ? GFX8_DATAPORT_DC_PORT1_A64_SCATTERED_WRITE :
              GFX9_DATAPORT_DC_PORT1_A64_SCATTERED_READ;

   const unsigned msg_control =
      SET_BITS(GFX8_A64_SCATTERED_SUBTYPE_BYTE, 1, 0) |
      SET_BITS(elk_mdc_a64_ds(bit_size / 8), 3, 2) |
      SET_BITS(exec_size == 16, 4, 4);

   return elk_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
elk_dp_a64_untyped_atomic_desc(const struct intel_device_info *devinfo,
                               ASSERTED unsigned exec_size, /**< 0 for SIMD4x2 */
                               unsigned bit_size,
                               unsigned atomic_op,
                               bool response_expected)
{
   assert(exec_size == 8);
   assert(devinfo->ver >= 8);
   assert(bit_size == 32 || bit_size == 64);

   const unsigned msg_type = GFX8_DATAPORT_DC_PORT1_A64_UNTYPED_ATOMIC_OP;

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(bit_size == 64, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return elk_dp_desc(devinfo, GFX8_BTI_STATELESS_NON_COHERENT,
                      msg_type, msg_control);
}

static inline uint32_t
elk_dp_typed_atomic_desc(const struct intel_device_info *devinfo,
                         unsigned exec_size,
                         unsigned exec_group,
                         unsigned atomic_op,
                         bool response_expected)
{
   assert(exec_size > 0 || exec_group == 0);
   assert(exec_group % 8 == 0);

   unsigned msg_type;
   if (devinfo->verx10 >= 75) {
      if (exec_size == 0) {
         msg_type = HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2;
      } else {
         msg_type = HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP;
      }
   } else {
      /* SIMD4x2 typed surface R/W messages only exist on HSW+ */
      assert(exec_size > 0);
      msg_type = GFX7_DATAPORT_RC_TYPED_ATOMIC_OP;
   }

   const bool high_sample_mask = (exec_group / 8) % 2 == 1;

   const unsigned msg_control =
      SET_BITS(atomic_op, 3, 0) |
      SET_BITS(high_sample_mask, 4, 4) |
      SET_BITS(response_expected, 5, 5);

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
elk_dp_typed_surface_rw_desc(const struct intel_device_info *devinfo,
                             unsigned exec_size,
                             unsigned exec_group,
                             unsigned num_channels,
                             bool write)
{
   assert(exec_size > 0 || exec_group == 0);
   assert(exec_group % 8 == 0);

   /* Typed surface reads and writes don't support SIMD16 */
   assert(exec_size <= 8);

   unsigned msg_type;
   if (write) {
      if (devinfo->verx10 >= 75) {
         msg_type = HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE;
      } else {
         msg_type = GFX7_DATAPORT_RC_TYPED_SURFACE_WRITE;
      }
   } else {
      if (devinfo->verx10 >= 75) {
         msg_type = HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ;
      } else {
         msg_type = GFX7_DATAPORT_RC_TYPED_SURFACE_READ;
      }
   }

   /* See also MDC_SG3 in the SKL PRM Vol 2d. */
   unsigned msg_control;
   if (devinfo->verx10 >= 75) {
      /* See also MDC_SG3 in the SKL PRM Vol 2d. */
      const unsigned slot_group = exec_size == 0 ? 0 : /* SIMD4x2 */
                                  1 + ((exec_group / 8) % 2);

      msg_control =
         SET_BITS(elk_mdc_cmask(num_channels), 3, 0) |
         SET_BITS(slot_group, 5, 4);
   } else {
      /* SIMD4x2 typed surface R/W messages only exist on HSW+ */
      assert(exec_size > 0);
      const unsigned slot_group = ((exec_group / 8) % 2);

      msg_control =
         SET_BITS(elk_mdc_cmask(num_channels), 3, 0) |
         SET_BITS(slot_group, 5, 5);
   }

   return elk_dp_surface_desc(devinfo, msg_type, msg_control);
}

static inline uint32_t
elk_fb_desc(const struct intel_device_info *devinfo,
            unsigned binding_table_index,
            unsigned msg_type,
            unsigned msg_control)
{
   /* Prior to gen6, things are too inconsistent; use the fb_(read|write)_desc
    * helpers instead.
    */
   assert(devinfo->ver >= 6);
   const unsigned desc = SET_BITS(binding_table_index, 7, 0);
   if (devinfo->ver >= 7) {
      return (desc | SET_BITS(msg_control, 13, 8) |
              SET_BITS(msg_type, 17, 14));
   } else {
      return (desc | SET_BITS(msg_control, 12, 8) |
              SET_BITS(msg_type, 16, 13));
   }
}

static inline unsigned
elk_fb_desc_binding_table_index(UNUSED const struct intel_device_info *devinfo,
                                uint32_t desc)
{
   return GET_BITS(desc, 7, 0);
}

static inline uint32_t
elk_fb_desc_msg_control(const struct intel_device_info *devinfo, uint32_t desc)
{
   assert(devinfo->ver >= 6);
   if (devinfo->ver >= 7)
      return GET_BITS(desc, 13, 8);
   else
      return GET_BITS(desc, 12, 8);
}

static inline unsigned
elk_fb_desc_msg_type(const struct intel_device_info *devinfo, uint32_t desc)
{
   assert(devinfo->ver >= 6);
   if (devinfo->ver >= 7)
      return GET_BITS(desc, 17, 14);
   else
      return GET_BITS(desc, 16, 13);
}

static inline uint32_t
elk_fb_write_desc(const struct intel_device_info *devinfo,
                  unsigned binding_table_index,
                  unsigned msg_control,
                  bool last_render_target,
                  bool coarse_write)
{
   const unsigned msg_type =
      devinfo->ver >= 6 ?
      GFX6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE :
      ELK_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE;

   assert(!coarse_write);

   if (devinfo->ver >= 6) {
      return elk_fb_desc(devinfo, binding_table_index, msg_type, msg_control) |
             SET_BITS(last_render_target, 12, 12) |
             SET_BITS(coarse_write, 18, 18);
   } else {
      return (SET_BITS(binding_table_index, 7, 0) |
              SET_BITS(msg_control, 11, 8) |
              SET_BITS(last_render_target, 11, 11) |
              SET_BITS(msg_type, 14, 12));
   }
}

static inline unsigned
elk_fb_write_desc_msg_type(const struct intel_device_info *devinfo,
                           uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_fb_desc_msg_type(devinfo, desc);
   else
      return GET_BITS(desc, 14, 12);
}

static inline unsigned
elk_fb_write_desc_msg_control(const struct intel_device_info *devinfo,
                              uint32_t desc)
{
   if (devinfo->ver >= 6)
      return elk_fb_desc_msg_control(devinfo, desc);
   else
      return GET_BITS(desc, 11, 8);
}

static inline bool
elk_fb_write_desc_last_render_target(const struct intel_device_info *devinfo,
                                     uint32_t desc)
{
   if (devinfo->ver >= 6)
      return GET_BITS(desc, 12, 12);
   else
      return GET_BITS(desc, 11, 11);
}

static inline bool
elk_fb_write_desc_write_commit(const struct intel_device_info *devinfo,
                               uint32_t desc)
{
   assert(devinfo->ver <= 6);
   if (devinfo->ver >= 6)
      return GET_BITS(desc, 17, 17);
   else
      return GET_BITS(desc, 15, 15);
}

static inline bool
elk_lsc_opcode_has_cmask(enum elk_lsc_opcode opcode)
{
   return opcode == LSC_OP_LOAD_CMASK || opcode == LSC_OP_STORE_CMASK;
}

static inline bool
elk_lsc_opcode_has_transpose(enum elk_lsc_opcode opcode)
{
   return opcode == LSC_OP_LOAD || opcode == LSC_OP_STORE;
}

static inline bool
elk_lsc_opcode_is_store(enum elk_lsc_opcode opcode)
{
   return opcode == LSC_OP_STORE ||
          opcode == LSC_OP_STORE_CMASK;
}

static inline bool
elk_lsc_opcode_is_atomic(enum elk_lsc_opcode opcode)
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
elk_lsc_opcode_is_atomic_float(enum elk_lsc_opcode opcode)
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
   enum elk_lsc_opcode op = (enum elk_lsc_opcode) _op;

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
   enum elk_lsc_opcode op = (enum elk_lsc_opcode) _op;

   switch (op) {
   case LSC_OP_ATOMIC_INC:
      return ELK_AOP_INC;
   case LSC_OP_ATOMIC_DEC:
      return ELK_AOP_DEC;
   case LSC_OP_ATOMIC_STORE:
      return ELK_AOP_MOV;
   case LSC_OP_ATOMIC_ADD:
      return ELK_AOP_ADD;
   case LSC_OP_ATOMIC_SUB:
      return ELK_AOP_SUB;
   case LSC_OP_ATOMIC_MIN:
      return ELK_AOP_IMIN;
   case LSC_OP_ATOMIC_MAX:
      return ELK_AOP_IMAX;
   case LSC_OP_ATOMIC_UMIN:
      return ELK_AOP_UMIN;
   case LSC_OP_ATOMIC_UMAX:
      return ELK_AOP_UMAX;
   case LSC_OP_ATOMIC_CMPXCHG:
      return ELK_AOP_CMPWR;
   case LSC_OP_ATOMIC_FADD:
      return ELK_AOP_FADD;
   case LSC_OP_ATOMIC_FMIN:
      return ELK_AOP_FMIN;
   case LSC_OP_ATOMIC_FMAX:
      return ELK_AOP_FMAX;
   case LSC_OP_ATOMIC_FCMPXCHG:
      return ELK_AOP_FCMPWR;
   case LSC_OP_ATOMIC_AND:
      return ELK_AOP_AND;
   case LSC_OP_ATOMIC_OR:
      return ELK_AOP_OR;
   case LSC_OP_ATOMIC_XOR:
      return ELK_AOP_XOR;
   /* No LSC op maps to ELK_AOP_PREDEC */
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
lsc_msg_desc_wcmask(UNUSED const struct intel_device_info *devinfo,
             enum elk_lsc_opcode opcode, unsigned simd_size,
             enum lsc_addr_surface_type addr_type,
             enum lsc_addr_size addr_sz, unsigned num_coordinates,
             enum lsc_data_size data_sz, unsigned num_channels,
             bool transpose, unsigned cache_ctrl, bool has_dest, unsigned cmask)
{
   assert(devinfo->has_lsc);

   unsigned dest_length = !has_dest ? 0 :
      DIV_ROUND_UP(lsc_data_size_bytes(data_sz) * num_channels * simd_size,
                   reg_unit(devinfo) * REG_SIZE);

   unsigned src0_length =
      DIV_ROUND_UP(lsc_addr_size_bytes(addr_sz) * num_coordinates * simd_size,
                   reg_unit(devinfo) * REG_SIZE);

   assert(!transpose || elk_lsc_opcode_has_transpose(opcode));

   unsigned msg_desc =
      SET_BITS(opcode, 5, 0) |
      SET_BITS(addr_sz, 8, 7) |
      SET_BITS(data_sz, 11, 9) |
      SET_BITS(transpose, 15, 15) |
      SET_BITS(cache_ctrl, 19, 17) |
      SET_BITS(dest_length, 24, 20) |
      SET_BITS(src0_length, 28, 25) |
      SET_BITS(addr_type, 30, 29);

   if (elk_lsc_opcode_has_cmask(opcode))
      msg_desc |= SET_BITS(cmask ? cmask : lsc_cmask(num_channels), 15, 12);
   else
      msg_desc |= SET_BITS(lsc_vect_size(num_channels), 14, 12);

   return msg_desc;
}

static inline uint32_t
lsc_msg_desc(UNUSED const struct intel_device_info *devinfo,
             enum elk_lsc_opcode opcode, unsigned simd_size,
             enum lsc_addr_surface_type addr_type,
             enum lsc_addr_size addr_sz, unsigned num_coordinates,
             enum lsc_data_size data_sz, unsigned num_channels,
             bool transpose, unsigned cache_ctrl, bool has_dest)
{
   return lsc_msg_desc_wcmask(devinfo, opcode, simd_size, addr_type, addr_sz,
         num_coordinates, data_sz, num_channels, transpose, cache_ctrl,
         has_dest, 0);
}

static inline enum elk_lsc_opcode
lsc_msg_desc_opcode(UNUSED const struct intel_device_info *devinfo,
                    uint32_t desc)
{
   assert(devinfo->has_lsc);
   return (enum elk_lsc_opcode) GET_BITS(desc, 5, 0);
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
   assert(!elk_lsc_opcode_has_cmask(lsc_msg_desc_opcode(devinfo, desc)));
   return (enum lsc_vect_size) GET_BITS(desc, 14, 12);
}

static inline enum lsc_cmask
lsc_msg_desc_cmask(UNUSED const struct intel_device_info *devinfo,
                   uint32_t desc)
{
   assert(devinfo->has_lsc);
   assert(elk_lsc_opcode_has_cmask(lsc_msg_desc_opcode(devinfo, desc)));
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
   return GET_BITS(desc, 19, 17);
}

static inline unsigned
lsc_msg_desc_dest_len(const struct intel_device_info *devinfo,
                      uint32_t desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(desc, 24, 20) * reg_unit(devinfo);
}

static inline unsigned
lsc_msg_desc_src0_len(const struct intel_device_info *devinfo,
                      uint32_t desc)
{
   assert(devinfo->has_lsc);
   return GET_BITS(desc, 28, 25) * reg_unit(devinfo);
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
elk_mdc_sm2(unsigned exec_size)
{
   assert(exec_size == 8 || exec_size == 16);
   return exec_size > 8;
}

static inline uint32_t
elk_mdc_sm2_exec_size(uint32_t sm2)
{
   assert(sm2 <= 1);
   return 8 << sm2;
}

static inline uint32_t
elk_btd_spawn_msg_type(UNUSED const struct intel_device_info *devinfo,
                       uint32_t desc)
{
   return GET_BITS(desc, 17, 14);
}

static inline uint32_t
elk_btd_spawn_exec_size(UNUSED const struct intel_device_info *devinfo,
                        uint32_t desc)
{
   return elk_mdc_sm2_exec_size(GET_BITS(desc, 8, 8));
}

/**
 * Construct a message descriptor immediate with the specified pixel
 * interpolator function controls.
 */
static inline uint32_t
elk_pixel_interp_desc(UNUSED const struct intel_device_info *devinfo,
                      unsigned msg_type,
                      bool noperspective,
                      bool coarse_pixel_rate,
                      unsigned exec_size,
                      unsigned group)
{
   assert(exec_size == 8 || exec_size == 16);
   const bool simd_mode = exec_size == 16;
   const bool slot_group = group >= 16;

   assert(!coarse_pixel_rate);
   return (SET_BITS(slot_group, 11, 11) |
           SET_BITS(msg_type, 13, 12) |
           SET_BITS(!!noperspective, 14, 14) |
           SET_BITS(coarse_pixel_rate, 15, 15) |
           SET_BITS(simd_mode, 16, 16));
}

void elk_urb_WRITE(struct elk_codegen *p,
		   struct elk_reg dest,
		   unsigned msg_reg_nr,
		   struct elk_reg src0,
                   enum elk_urb_write_flags flags,
		   unsigned msg_length,
		   unsigned response_length,
		   unsigned offset,
		   unsigned swizzle);

/**
 * Send message to shared unit \p sfid with a possibly indirect descriptor \p
 * desc.  If \p desc is not an immediate it will be transparently loaded to an
 * address register using an OR instruction.
 */
void
elk_send_indirect_message(struct elk_codegen *p,
                          unsigned sfid,
                          struct elk_reg dst,
                          struct elk_reg payload,
                          struct elk_reg desc,
                          unsigned desc_imm,
                          bool eot);

void
elk_send_indirect_split_message(struct elk_codegen *p,
                                unsigned sfid,
                                struct elk_reg dst,
                                struct elk_reg payload0,
                                struct elk_reg payload1,
                                struct elk_reg desc,
                                unsigned desc_imm,
                                struct elk_reg ex_desc,
                                unsigned ex_desc_imm,
                                bool ex_desc_scratch,
                                bool ex_bso,
                                bool eot);

void elk_ff_sync(struct elk_codegen *p,
		   struct elk_reg dest,
		   unsigned msg_reg_nr,
		   struct elk_reg src0,
		   bool allocate,
		   unsigned response_length,
		   bool eot);

void elk_svb_write(struct elk_codegen *p,
                   struct elk_reg dest,
                   unsigned msg_reg_nr,
                   struct elk_reg src0,
                   unsigned binding_table_index,
                   bool   send_commit_msg);

elk_inst *elk_fb_WRITE(struct elk_codegen *p,
                       struct elk_reg payload,
                       struct elk_reg implied_header,
                       unsigned msg_control,
                       unsigned binding_table_index,
                       unsigned msg_length,
                       unsigned response_length,
                       bool eot,
                       bool last_render_target,
                       bool header_present);

void elk_SAMPLE(struct elk_codegen *p,
		struct elk_reg dest,
		unsigned msg_reg_nr,
		struct elk_reg src0,
		unsigned binding_table_index,
		unsigned sampler,
		unsigned msg_type,
		unsigned response_length,
		unsigned msg_length,
		unsigned header_present,
		unsigned simd_mode,
		unsigned return_format);

void elk_adjust_sampler_state_pointer(struct elk_codegen *p,
                                      struct elk_reg header,
                                      struct elk_reg sampler_index);

void elk_gfx4_math(struct elk_codegen *p,
	       struct elk_reg dest,
	       unsigned function,
	       unsigned msg_reg_nr,
	       struct elk_reg src,
	       unsigned precision );

void elk_gfx6_math(struct elk_codegen *p,
	       struct elk_reg dest,
	       unsigned function,
	       struct elk_reg src0,
	       struct elk_reg src1);

void elk_oword_block_read(struct elk_codegen *p,
			  struct elk_reg dest,
			  struct elk_reg mrf,
			  uint32_t offset,
			  uint32_t bind_table_index);

unsigned elk_scratch_surface_idx(const struct elk_codegen *p);

void elk_oword_block_read_scratch(struct elk_codegen *p,
				  struct elk_reg dest,
				  struct elk_reg mrf,
				  int num_regs,
				  unsigned offset);

void elk_oword_block_write_scratch(struct elk_codegen *p,
				   struct elk_reg mrf,
				   int num_regs,
				   unsigned offset);

void elk_gfx7_block_read_scratch(struct elk_codegen *p,
                             struct elk_reg dest,
                             int num_regs,
                             unsigned offset);

/**
 * Return the generation-specific jump distance scaling factor.
 *
 * Given the number of instructions to jump, we need to scale by
 * some number to obtain the actual jump distance to program in an
 * instruction.
 */
static inline unsigned
elk_jump_scale(const struct intel_device_info *devinfo)
{
   /* Broadwell measures jump targets in bytes. */
   if (devinfo->ver >= 8)
      return 16;

   /* Ironlake and later measure jump targets in 64-bit data chunks (in order
    * (to support compaction), so each 128-bit instruction requires 2 chunks.
    */
   if (devinfo->ver >= 5)
      return 2;

   /* Gfx4 simply uses the number of 128-bit instructions. */
   return 1;
}

void elk_barrier(struct elk_codegen *p, struct elk_reg src);

/* If/else/endif.  Works by manipulating the execution flags on each
 * channel.
 */
elk_inst *elk_IF(struct elk_codegen *p, unsigned execute_size);
elk_inst *elk_gfx6_IF(struct elk_codegen *p, enum elk_conditional_mod conditional,
                  struct elk_reg src0, struct elk_reg src1);

void elk_ELSE(struct elk_codegen *p);
void elk_ENDIF(struct elk_codegen *p);

/* DO/WHILE loops:
 */
elk_inst *elk_DO(struct elk_codegen *p, unsigned execute_size);

elk_inst *elk_WHILE(struct elk_codegen *p);

elk_inst *elk_BREAK(struct elk_codegen *p);
elk_inst *elk_CONT(struct elk_codegen *p);
elk_inst *elk_HALT(struct elk_codegen *p);

/* Forward jumps:
 */
void elk_land_fwd_jump(struct elk_codegen *p, int jmp_insn_idx);

elk_inst *elk_JMPI(struct elk_codegen *p, struct elk_reg index,
                   unsigned predicate_control);

void elk_NOP(struct elk_codegen *p);

void elk_WAIT(struct elk_codegen *p);

/* Special case: there is never a destination, execution size will be
 * taken from src0:
 */
void elk_CMP(struct elk_codegen *p,
	     struct elk_reg dest,
	     unsigned conditional,
	     struct elk_reg src0,
	     struct elk_reg src1);

void elk_CMPN(struct elk_codegen *p,
              struct elk_reg dest,
              unsigned conditional,
              struct elk_reg src0,
              struct elk_reg src1);

void
elk_untyped_atomic(struct elk_codegen *p,
                   struct elk_reg dst,
                   struct elk_reg payload,
                   struct elk_reg surface,
                   unsigned atomic_op,
                   unsigned msg_length,
                   bool response_expected,
                   bool header_present);

void
elk_untyped_surface_read(struct elk_codegen *p,
                         struct elk_reg dst,
                         struct elk_reg payload,
                         struct elk_reg surface,
                         unsigned msg_length,
                         unsigned num_channels);

void
elk_untyped_surface_write(struct elk_codegen *p,
                          struct elk_reg payload,
                          struct elk_reg surface,
                          unsigned msg_length,
                          unsigned num_channels,
                          bool header_present);

void
elk_memory_fence(struct elk_codegen *p,
                 struct elk_reg dst,
                 struct elk_reg src,
                 enum elk_opcode send_op,
                 enum elk_message_target sfid,
                 uint32_t desc,
                 bool commit_enable,
                 unsigned bti);

void
elk_pixel_interpolator_query(struct elk_codegen *p,
                             struct elk_reg dest,
                             struct elk_reg mrf,
                             bool noperspective,
                             bool coarse_pixel_rate,
                             unsigned mode,
                             struct elk_reg data,
                             unsigned msg_length,
                             unsigned response_length);

void
elk_find_live_channel(struct elk_codegen *p,
                      struct elk_reg dst,
                      bool last);

void
elk_broadcast(struct elk_codegen *p,
              struct elk_reg dst,
              struct elk_reg src,
              struct elk_reg idx);

void
elk_float_controls_mode(struct elk_codegen *p,
                        unsigned mode, unsigned mask);

void
elk_update_reloc_imm(const struct elk_isa_info *isa,
                     elk_inst *inst,
                     uint32_t value);

void
elk_MOV_reloc_imm(struct elk_codegen *p,
                  struct elk_reg dst,
                  enum elk_reg_type src_type,
                  uint32_t id);

unsigned
elk_num_sources_from_inst(const struct elk_isa_info *isa,
                          const elk_inst *inst);

/***********************************************************************
 * elk_eu_util.c:
 */

void elk_copy_indirect_to_indirect(struct elk_codegen *p,
				   struct elk_indirect dst_ptr,
				   struct elk_indirect src_ptr,
				   unsigned count);

void elk_copy_from_indirect(struct elk_codegen *p,
			    struct elk_reg dst,
			    struct elk_indirect ptr,
			    unsigned count);

void elk_copy4(struct elk_codegen *p,
	       struct elk_reg dst,
	       struct elk_reg src,
	       unsigned count);

void elk_copy8(struct elk_codegen *p,
	       struct elk_reg dst,
	       struct elk_reg src,
	       unsigned count);

void elk_math_invert( struct elk_codegen *p,
		      struct elk_reg dst,
		      struct elk_reg src);

void elk_set_src1(struct elk_codegen *p, elk_inst *insn, struct elk_reg reg);

void elk_set_desc_ex(struct elk_codegen *p, elk_inst *insn,
                     unsigned desc, unsigned ex_desc);

static inline void
elk_set_desc(struct elk_codegen *p, elk_inst *insn, unsigned desc)
{
   elk_set_desc_ex(p, insn, desc, 0);
}

void elk_set_uip_jip(struct elk_codegen *p, int start_offset);

enum elk_conditional_mod elk_negate_cmod(enum elk_conditional_mod cmod);
enum elk_conditional_mod elk_swap_cmod(enum elk_conditional_mod cmod);

/* elk_eu_compact.c */
void elk_compact_instructions(struct elk_codegen *p, int start_offset,
                              struct elk_disasm_info *disasm);
void elk_uncompact_instruction(const struct elk_isa_info *isa,
                               elk_inst *dst, elk_compact_inst *src);
bool elk_try_compact_instruction(const struct elk_isa_info *isa,
                                 elk_compact_inst *dst, const elk_inst *src);

void elk_debug_compact_uncompact(const struct elk_isa_info *isa,
                                 elk_inst *orig, elk_inst *uncompacted);

/* elk_eu_validate.c */
bool elk_validate_instruction(const struct elk_isa_info *isa,
                              const elk_inst *inst, int offset,
                              unsigned inst_size,
                              struct elk_disasm_info *disasm);
bool elk_validate_instructions(const struct elk_isa_info *isa,
                               const void *assembly, int start_offset, int end_offset,
                               struct elk_disasm_info *disasm);

static inline int
next_offset(const struct intel_device_info *devinfo, void *store, int offset)
{
   elk_inst *insn = (elk_inst *)((char *)store + offset);

   if (elk_inst_cmpt_control(devinfo, insn))
      return offset + 8;
   else
      return offset + 16;
}

/** Maximum SEND message length */
#define ELK_MAX_MSG_LENGTH 15

/** First MRF register used by pull loads */
#define FIRST_SPILL_MRF(gen) ((gen) == 6 ? 21 : 13)

/** First MRF register used by spills */
#define FIRST_PULL_LOAD_MRF(gen) ((gen) == 6 ? 16 : 13)

#ifdef __cplusplus
}
#endif

#endif
