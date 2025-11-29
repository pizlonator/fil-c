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

#include <sys/stat.h>
#include <fcntl.h>

#include "elk_disasm.h"
#include "elk_eu_defines.h"
#include "elk_eu.h"
#include "elk_shader.h"
#include "../intel_gfx_ver_enum.h"
#include "dev/intel_debug.h"

#include "util/u_debug.h"
#include "util/ralloc.h"

/* Returns a conditional modifier that negates the condition. */
enum elk_conditional_mod
elk_negate_cmod(enum elk_conditional_mod cmod)
{
   switch (cmod) {
   case ELK_CONDITIONAL_Z:
      return ELK_CONDITIONAL_NZ;
   case ELK_CONDITIONAL_NZ:
      return ELK_CONDITIONAL_Z;
   case ELK_CONDITIONAL_G:
      return ELK_CONDITIONAL_LE;
   case ELK_CONDITIONAL_GE:
      return ELK_CONDITIONAL_L;
   case ELK_CONDITIONAL_L:
      return ELK_CONDITIONAL_GE;
   case ELK_CONDITIONAL_LE:
      return ELK_CONDITIONAL_G;
   default:
      unreachable("Can't negate this cmod");
   }
}

/* Returns the corresponding conditional mod for swapping src0 and
 * src1 in e.g. CMP.
 */
enum elk_conditional_mod
elk_swap_cmod(enum elk_conditional_mod cmod)
{
   switch (cmod) {
   case ELK_CONDITIONAL_Z:
   case ELK_CONDITIONAL_NZ:
      return cmod;
   case ELK_CONDITIONAL_G:
      return ELK_CONDITIONAL_L;
   case ELK_CONDITIONAL_GE:
      return ELK_CONDITIONAL_LE;
   case ELK_CONDITIONAL_L:
      return ELK_CONDITIONAL_G;
   case ELK_CONDITIONAL_LE:
      return ELK_CONDITIONAL_GE;
   default:
      return ELK_CONDITIONAL_NONE;
   }
}

/**
 * Get the least significant bit offset of the i+1-th component of immediate
 * type \p type.  For \p i equal to the two's complement of j, return the
 * offset of the j-th component starting from the end of the vector.  For
 * scalar register types return zero.
 */
static unsigned
imm_shift(enum elk_reg_type type, unsigned i)
{
   assert(type != ELK_REGISTER_TYPE_UV && type != ELK_REGISTER_TYPE_V &&
          "Not implemented.");

   if (type == ELK_REGISTER_TYPE_VF)
      return 8 * (i & 3);
   else
      return 0;
}

/**
 * Swizzle an arbitrary immediate \p x of the given type according to the
 * permutation specified as \p swz.
 */
uint32_t
elk_swizzle_immediate(enum elk_reg_type type, uint32_t x, unsigned swz)
{
   if (imm_shift(type, 1)) {
      const unsigned n = 32 / imm_shift(type, 1);
      uint32_t y = 0;

      for (unsigned i = 0; i < n; i++) {
         /* Shift the specified component all the way to the right and left to
          * discard any undesired L/MSBs, then shift it right into component i.
          */
         y |= x >> imm_shift(type, (i & ~3) + ELK_GET_SWZ(swz, i & 3))
                << imm_shift(type, ~0u)
                >> imm_shift(type, ~0u - i);
      }

      return y;
   } else {
      return x;
   }
}

unsigned
elk_get_default_exec_size(struct elk_codegen *p)
{
   return p->current->exec_size;
}

unsigned
elk_get_default_group(struct elk_codegen *p)
{
   return p->current->group;
}

unsigned
elk_get_default_access_mode(struct elk_codegen *p)
{
   return p->current->access_mode;
}

void
elk_set_default_exec_size(struct elk_codegen *p, unsigned value)
{
   p->current->exec_size = value;
}

void elk_set_default_predicate_control(struct elk_codegen *p, enum elk_predicate pc)
{
   p->current->predicate = pc;
}

void elk_set_default_predicate_inverse(struct elk_codegen *p, bool predicate_inverse)
{
   p->current->pred_inv = predicate_inverse;
}

void elk_set_default_flag_reg(struct elk_codegen *p, int reg, int subreg)
{
   assert(subreg < 2);
   p->current->flag_subreg = reg * 2 + subreg;
}

void elk_set_default_access_mode( struct elk_codegen *p, unsigned access_mode )
{
   p->current->access_mode = access_mode;
}

void
elk_set_default_compression_control(struct elk_codegen *p,
			    enum elk_compression compression_control)
{
   switch (compression_control) {
   case ELK_COMPRESSION_NONE:
      /* This is the "use the first set of bits of dmask/vmask/arf
       * according to execsize" option.
       */
      p->current->group = 0;
      break;
   case ELK_COMPRESSION_2NDHALF:
      /* For SIMD8, this is "use the second set of 8 bits." */
      p->current->group = 8;
      break;
   case ELK_COMPRESSION_COMPRESSED:
      /* For SIMD16 instruction compression, use the first set of 16 bits
       * since we don't do SIMD32 dispatch.
       */
      p->current->group = 0;
      break;
   default:
      unreachable("not reached");
   }

   if (p->devinfo->ver <= 6) {
      p->current->compressed =
         (compression_control == ELK_COMPRESSION_COMPRESSED);
   }
}

/**
 * Enable or disable instruction compression on the given instruction leaving
 * the currently selected channel enable group untouched.
 */
void
elk_inst_set_compression(const struct intel_device_info *devinfo,
                         elk_inst *inst, bool on)
{
   if (devinfo->ver >= 6) {
      /* No-op, the EU will figure out for us whether the instruction needs to
       * be compressed.
       */
   } else {
      /* The channel group and compression controls are non-orthogonal, there
       * are two possible representations for uncompressed instructions and we
       * may need to preserve the current one to avoid changing the selected
       * channel group inadvertently.
       */
      if (on)
         elk_inst_set_qtr_control(devinfo, inst, ELK_COMPRESSION_COMPRESSED);
      else if (elk_inst_qtr_control(devinfo, inst)
               == ELK_COMPRESSION_COMPRESSED)
         elk_inst_set_qtr_control(devinfo, inst, ELK_COMPRESSION_NONE);
   }
}

void
elk_set_default_compression(struct elk_codegen *p, bool on)
{
   p->current->compressed = on;
}

/**
 * Apply the range of channel enable signals given by
 * [group, group + exec_size) to the instruction passed as argument.
 */
void
elk_inst_set_group(const struct intel_device_info *devinfo,
                   elk_inst *inst, unsigned group)
{
   if (devinfo->ver >= 7) {
      assert(group % 4 == 0 && group < 32);
      elk_inst_set_qtr_control(devinfo, inst, group / 8);
      elk_inst_set_nib_control(devinfo, inst, (group / 4) % 2);

   } else if (devinfo->ver == 6) {
      assert(group % 8 == 0 && group < 32);
      elk_inst_set_qtr_control(devinfo, inst, group / 8);

   } else {
      assert(group % 8 == 0 && group < 16);
      /* The channel group and compression controls are non-orthogonal, there
       * are two possible representations for group zero and we may need to
       * preserve the current one to avoid changing the selected compression
       * enable inadvertently.
       */
      if (group == 8)
         elk_inst_set_qtr_control(devinfo, inst, ELK_COMPRESSION_2NDHALF);
      else if (elk_inst_qtr_control(devinfo, inst) == ELK_COMPRESSION_2NDHALF)
         elk_inst_set_qtr_control(devinfo, inst, ELK_COMPRESSION_NONE);
   }
}

void
elk_set_default_group(struct elk_codegen *p, unsigned group)
{
   p->current->group = group;
}

void elk_set_default_mask_control( struct elk_codegen *p, unsigned value )
{
   p->current->mask_control = value;
}

void elk_set_default_saturate( struct elk_codegen *p, bool enable )
{
   p->current->saturate = enable;
}

void elk_set_default_acc_write_control(struct elk_codegen *p, unsigned value)
{
   p->current->acc_wr_control = value;
}

void elk_push_insn_state( struct elk_codegen *p )
{
   assert(p->current != &p->stack[ELK_EU_MAX_INSN_STACK-1]);
   *(p->current + 1) = *p->current;
   p->current++;
}

void elk_pop_insn_state( struct elk_codegen *p )
{
   assert(p->current != p->stack);
   p->current--;
}


/***********************************************************************
 */
void
elk_init_codegen(const struct elk_isa_info *isa,
                 struct elk_codegen *p, void *mem_ctx)
{
   memset(p, 0, sizeof(*p));

   p->isa = isa;
   p->devinfo = isa->devinfo;
   p->automatic_exec_sizes = true;
   /*
    * Set the initial instruction store array size to 1024, if found that
    * isn't enough, then it will double the store size at elk_next_insn()
    * until out of memory.
    */
   p->store_size = 1024;
   p->store = rzalloc_array(mem_ctx, elk_inst, p->store_size);
   p->nr_insn = 0;
   p->current = p->stack;
   memset(p->current, 0, sizeof(p->current[0]));

   p->mem_ctx = mem_ctx;

   /* Some defaults?
    */
   elk_set_default_exec_size(p, ELK_EXECUTE_8);
   elk_set_default_mask_control(p, ELK_MASK_ENABLE); /* what does this do? */
   elk_set_default_saturate(p, 0);
   elk_set_default_compression_control(p, ELK_COMPRESSION_NONE);

   /* Set up control flow stack */
   p->if_stack_depth = 0;
   p->if_stack_array_size = 16;
   p->if_stack = rzalloc_array(mem_ctx, int, p->if_stack_array_size);

   p->loop_stack_depth = 0;
   p->loop_stack_array_size = 16;
   p->loop_stack = rzalloc_array(mem_ctx, int, p->loop_stack_array_size);
   p->if_depth_in_loop = rzalloc_array(mem_ctx, int, p->loop_stack_array_size);
}


const unsigned *elk_get_program( struct elk_codegen *p,
			       unsigned *sz )
{
   *sz = p->next_insn_offset;
   return (const unsigned *)p->store;
}

const struct elk_shader_reloc *
elk_get_shader_relocs(struct elk_codegen *p, unsigned *num_relocs)
{
   *num_relocs = p->num_relocs;
   return p->relocs;
}

DEBUG_GET_ONCE_OPTION(shader_bin_dump_path, "INTEL_SHADER_BIN_DUMP_PATH", NULL);

bool elk_should_dump_shader_bin(void)
{
   return debug_get_option_shader_bin_dump_path() != NULL;
}

void elk_dump_shader_bin(void *assembly, int start_offset, int end_offset,
                         const char *identifier)
{
   char *name = ralloc_asprintf(NULL, "%s/%s.bin",
                                debug_get_option_shader_bin_dump_path(),
                                identifier);

   int fd = open(name, O_CREAT | O_WRONLY, 0777);
   ralloc_free(name);

   if (fd < 0)
      return;

   struct stat sb;
   if (fstat(fd, &sb) != 0 || (!S_ISREG(sb.st_mode))) {
      close(fd);
      return;
   }

   size_t to_write = end_offset - start_offset;
   void *write_ptr = assembly + start_offset;

   while (to_write) {
      ssize_t ret = write(fd, write_ptr, to_write);

      if (ret <= 0) {
         close(fd);
         return;
      }

      to_write -= ret;
      write_ptr += ret;
   }

   close(fd);
}

bool elk_try_override_assembly(struct elk_codegen *p, int start_offset,
                               const char *identifier)
{
   const char *read_path = getenv("INTEL_SHADER_ASM_READ_PATH");
   if (!read_path) {
      return false;
   }

   char *name = ralloc_asprintf(NULL, "%s/%s.bin", read_path, identifier);

   int fd = open(name, O_RDONLY);
   ralloc_free(name);

   if (fd == -1) {
      return false;
   }

   struct stat sb;
   if (fstat(fd, &sb) != 0 || (!S_ISREG(sb.st_mode))) {
      close(fd);
      return false;
   }

   p->nr_insn -= (p->next_insn_offset - start_offset) / sizeof(elk_inst);
   p->nr_insn += sb.st_size / sizeof(elk_inst);

   p->next_insn_offset = start_offset + sb.st_size;
   p->store_size = (start_offset + sb.st_size) / sizeof(elk_inst);
   p->store = (elk_inst *)reralloc_size(p->mem_ctx, p->store, p->next_insn_offset);
   assert(p->store);

   ssize_t ret = read(fd, (char *)p->store + start_offset, sb.st_size);
   close(fd);
   if (ret != sb.st_size) {
      return false;
   }

   ASSERTED bool valid =
      elk_validate_instructions(p->isa, p->store,
                                start_offset, p->next_insn_offset,
                                NULL);
   assert(valid);

   return true;
}

const struct elk_label *
elk_find_label(const struct elk_label *root, int offset)
{
   const struct elk_label *curr = root;

   if (curr != NULL)
   {
      do {
         if (curr->offset == offset)
            return curr;

         curr = curr->next;
      } while (curr != NULL);
   }

   return curr;
}

void
elk_create_label(struct elk_label **labels, int offset, void *mem_ctx)
{
   if (*labels != NULL) {
      struct elk_label *curr = *labels;
      struct elk_label *prev;

      do {
         prev = curr;

         if (curr->offset == offset)
            return;

         curr = curr->next;
      } while (curr != NULL);

      curr = ralloc(mem_ctx, struct elk_label);
      curr->offset = offset;
      curr->number = prev->number + 1;
      curr->next = NULL;
      prev->next = curr;
   } else {
      struct elk_label *root = ralloc(mem_ctx, struct elk_label);
      root->number = 0;
      root->offset = offset;
      root->next = NULL;
      *labels = root;
   }
}

const struct elk_label *
elk_label_assembly(const struct elk_isa_info *isa,
                   const void *assembly, int start, int end, void *mem_ctx)
{
   const struct intel_device_info *const devinfo = isa->devinfo;

   struct elk_label *root_label = NULL;

   int to_bytes_scale = sizeof(elk_inst) / elk_jump_scale(devinfo);

   for (int offset = start; offset < end;) {
      const elk_inst *inst = (const elk_inst *) ((const char *) assembly + offset);
      elk_inst uncompacted;

      bool is_compact = elk_inst_cmpt_control(devinfo, inst);

      if (is_compact) {
         elk_compact_inst *compacted = (elk_compact_inst *)inst;
         elk_uncompact_instruction(isa, &uncompacted, compacted);
         inst = &uncompacted;
      }

      if (elk_has_uip(devinfo, elk_inst_opcode(isa, inst))) {
         /* Instructions that have UIP also have JIP. */
         elk_create_label(&root_label,
            offset + elk_inst_uip(devinfo, inst) * to_bytes_scale, mem_ctx);
         elk_create_label(&root_label,
            offset + elk_inst_jip(devinfo, inst) * to_bytes_scale, mem_ctx);
      } else if (elk_has_jip(devinfo, elk_inst_opcode(isa, inst))) {
         int jip;
         if (devinfo->ver >= 7) {
            jip = elk_inst_jip(devinfo, inst);
         } else {
            jip = elk_inst_gfx6_jump_count(devinfo, inst);
         }

         elk_create_label(&root_label, offset + jip * to_bytes_scale, mem_ctx);
      }

      if (is_compact) {
         offset += sizeof(elk_compact_inst);
      } else {
         offset += sizeof(elk_inst);
      }
   }

   return root_label;
}

void
elk_disassemble_with_labels(const struct elk_isa_info *isa,
                            const void *assembly, int start, int end, FILE *out)
{
   void *mem_ctx = ralloc_context(NULL);
   const struct elk_label *root_label =
      elk_label_assembly(isa, assembly, start, end, mem_ctx);

   elk_disassemble(isa, assembly, start, end, root_label, out);

   ralloc_free(mem_ctx);
}

void
elk_disassemble(const struct elk_isa_info *isa,
                const void *assembly, int start, int end,
                const struct elk_label *root_label, FILE *out)
{
   const struct intel_device_info *devinfo = isa->devinfo;

   bool dump_hex = INTEL_DEBUG(DEBUG_HEX);

   for (int offset = start; offset < end;) {
      const elk_inst *insn = (const elk_inst *)((char *)assembly + offset);
      elk_inst uncompacted;

      if (root_label != NULL) {
        const struct elk_label *label = elk_find_label(root_label, offset);
        if (label != NULL) {
           fprintf(out, "\nLABEL%d:\n", label->number);
        }
      }

      bool compacted = elk_inst_cmpt_control(devinfo, insn);
      if (0)
         fprintf(out, "0x%08x: ", offset);

      if (compacted) {
         elk_compact_inst *compacted = (elk_compact_inst *)insn;
         if (dump_hex) {
            unsigned char * insn_ptr = ((unsigned char *)&insn[0]);
            const unsigned int blank_spaces = 24;
            for (int i = 0 ; i < 8; i = i + 4) {
               fprintf(out, "%02x %02x %02x %02x ",
                       insn_ptr[i],
                       insn_ptr[i + 1],
                       insn_ptr[i + 2],
                       insn_ptr[i + 3]);
            }
            /* Make compacted instructions hex value output vertically aligned
             * with uncompacted instructions hex value
             */
            fprintf(out, "%*c", blank_spaces, ' ');
         }

         elk_uncompact_instruction(isa, &uncompacted, compacted);
         insn = &uncompacted;
      } else {
         if (dump_hex) {
            unsigned char * insn_ptr = ((unsigned char *)&insn[0]);
            for (int i = 0 ; i < 16; i = i + 4) {
               fprintf(out, "%02x %02x %02x %02x ",
                       insn_ptr[i],
                       insn_ptr[i + 1],
                       insn_ptr[i + 2],
                       insn_ptr[i + 3]);
            }
         }
      }

      elk_disassemble_inst(out, isa, insn, compacted, offset, root_label);

      if (compacted) {
         offset += sizeof(elk_compact_inst);
      } else {
         offset += sizeof(elk_inst);
      }
   }
}

static const struct elk_opcode_desc opcode_descs[] = {
   /* IR,                 HW,  name,      nsrc, ndst, gfx_vers */
   { ELK_OPCODE_ILLEGAL,  0,   "illegal", 0,    0,    GFX_ALL },
   { ELK_OPCODE_MOV,      1,   "mov",     1,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_MOV,      97,  "mov",     1,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_SEL,      2,   "sel",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SEL,      98,  "sel",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_MOVI,     3,   "movi",    2,    1,    GFX_GE(GFX45) & GFX_LT(GFX12) },
   { ELK_OPCODE_MOVI,     99,  "movi",    2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_NOT,      4,   "not",     1,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_NOT,      100, "not",     1,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_AND,      5,   "and",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_AND,      101, "and",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_OR,       6,   "or",      2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_OR,       102, "or",      2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_XOR,      7,   "xor",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_XOR,      103, "xor",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_SHR,      8,   "shr",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SHR,      104, "shr",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_SHL,      9,   "shl",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SHL,      105, "shl",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_DIM,      10,  "dim",     1,    1,    GFX75 },
   { ELK_OPCODE_SMOV,     10,  "smov",    0,    0,    GFX_GE(GFX8) & GFX_LT(GFX12) },
   { ELK_OPCODE_SMOV,     106, "smov",    0,    0,    GFX_GE(GFX12) },
   { ELK_OPCODE_ASR,      12,  "asr",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_ASR,      108, "asr",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_CMP,      16,  "cmp",     2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_CMP,      112, "cmp",     2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_CMPN,     17,  "cmpn",    2,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_CMPN,     113, "cmpn",    2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_CSEL,     18,  "csel",    3,    1,    GFX_GE(GFX8) & GFX_LT(GFX12) },
   { ELK_OPCODE_CSEL,     114, "csel",    3,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_F32TO16,  19,  "f32to16", 1,    1,    GFX7 | GFX75 },
   { ELK_OPCODE_F16TO32,  20,  "f16to32", 1,    1,    GFX7 | GFX75 },
   { ELK_OPCODE_BFREV,    23,  "bfrev",   1,    1,    GFX_GE(GFX7) & GFX_LT(GFX12) },
   { ELK_OPCODE_BFREV,    119, "bfrev",   1,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_BFE,      24,  "bfe",     3,    1,    GFX_GE(GFX7) & GFX_LT(GFX12) },
   { ELK_OPCODE_BFE,      120, "bfe",     3,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_BFI1,     25,  "bfi1",    2,    1,    GFX_GE(GFX7) & GFX_LT(GFX12) },
   { ELK_OPCODE_BFI1,     121, "bfi1",    2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_BFI2,     26,  "bfi2",    3,    1,    GFX_GE(GFX7) & GFX_LT(GFX12) },
   { ELK_OPCODE_BFI2,     122, "bfi2",    3,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_JMPI,     32,  "jmpi",    0,    0,    GFX_ALL },
   { ELK_OPCODE_BRD,      33,  "brd",     0,    0,    GFX_GE(GFX7) },
   { ELK_OPCODE_IF,       34,  "if",      0,    0,    GFX_ALL },
   { ELK_OPCODE_IFF,      35,  "iff",     0,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_BRC,      35,  "brc",     0,    0,    GFX_GE(GFX7) },
   { ELK_OPCODE_ELSE,     36,  "else",    0,    0,    GFX_ALL },
   { ELK_OPCODE_ENDIF,    37,  "endif",   0,    0,    GFX_ALL },
   { ELK_OPCODE_DO,       38,  "do",      0,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_CASE,     38,  "case",    0,    0,    GFX6 },
   { ELK_OPCODE_WHILE,    39,  "while",   0,    0,    GFX_ALL },
   { ELK_OPCODE_BREAK,    40,  "break",   0,    0,    GFX_ALL },
   { ELK_OPCODE_CONTINUE, 41,  "cont",    0,    0,    GFX_ALL },
   { ELK_OPCODE_HALT,     42,  "halt",    0,    0,    GFX_ALL },
   { ELK_OPCODE_CALLA,    43,  "calla",   0,    0,    GFX_GE(GFX75) },
   { ELK_OPCODE_MSAVE,    44,  "msave",   0,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_CALL,     44,  "call",    0,    0,    GFX_GE(GFX6) },
   { ELK_OPCODE_MREST,    45,  "mrest",   0,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_RET,      45,  "ret",     0,    0,    GFX_GE(GFX6) },
   { ELK_OPCODE_PUSH,     46,  "push",    0,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_FORK,     46,  "fork",    0,    0,    GFX6 },
   { ELK_OPCODE_GOTO,     46,  "goto",    0,    0,    GFX_GE(GFX8) },
   { ELK_OPCODE_POP,      47,  "pop",     2,    0,    GFX_LE(GFX5) },
   { ELK_OPCODE_WAIT,     48,  "wait",    0,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SEND,     49,  "send",    1,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SENDC,    50,  "sendc",   1,    1,    GFX_LT(GFX12) },
   { ELK_OPCODE_SEND,     49,  "send",    2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_SENDC,    50,  "sendc",   2,    1,    GFX_GE(GFX12) },
   { ELK_OPCODE_MATH,     56,  "math",    2,    1,    GFX_GE(GFX6) },
   { ELK_OPCODE_ADD,      64,  "add",     2,    1,    GFX_ALL },
   { ELK_OPCODE_MUL,      65,  "mul",     2,    1,    GFX_ALL },
   { ELK_OPCODE_AVG,      66,  "avg",     2,    1,    GFX_ALL },
   { ELK_OPCODE_FRC,      67,  "frc",     1,    1,    GFX_ALL },
   { ELK_OPCODE_RNDU,     68,  "rndu",    1,    1,    GFX_ALL },
   { ELK_OPCODE_RNDD,     69,  "rndd",    1,    1,    GFX_ALL },
   { ELK_OPCODE_RNDE,     70,  "rnde",    1,    1,    GFX_ALL },
   { ELK_OPCODE_RNDZ,     71,  "rndz",    1,    1,    GFX_ALL },
   { ELK_OPCODE_MAC,      72,  "mac",     2,    1,    GFX_ALL },
   { ELK_OPCODE_MACH,     73,  "mach",    2,    1,    GFX_ALL },
   { ELK_OPCODE_LZD,      74,  "lzd",     1,    1,    GFX_ALL },
   { ELK_OPCODE_FBH,      75,  "fbh",     1,    1,    GFX_GE(GFX7) },
   { ELK_OPCODE_FBL,      76,  "fbl",     1,    1,    GFX_GE(GFX7) },
   { ELK_OPCODE_CBIT,     77,  "cbit",    1,    1,    GFX_GE(GFX7) },
   { ELK_OPCODE_ADDC,     78,  "addc",    2,    1,    GFX_GE(GFX7) },
   { ELK_OPCODE_SUBB,     79,  "subb",    2,    1,    GFX_GE(GFX7) },
   { ELK_OPCODE_SAD2,     80,  "sad2",    2,    1,    GFX_ALL },
   { ELK_OPCODE_SADA2,    81,  "sada2",   2,    1,    GFX_ALL },
   { ELK_OPCODE_DP4,      84,  "dp4",     2,    1,    GFX_LT(GFX11) },
   { ELK_OPCODE_DPH,      85,  "dph",     2,    1,    GFX_LT(GFX11) },
   { ELK_OPCODE_DP3,      86,  "dp3",     2,    1,    GFX_LT(GFX11) },
   { ELK_OPCODE_DP2,      87,  "dp2",     2,    1,    GFX_LT(GFX11) },
   { ELK_OPCODE_LINE,     89,  "line",    2,    1,    GFX_LE(GFX10) },
   { ELK_OPCODE_PLN,      90,  "pln",     2,    1,    GFX_GE(GFX45) & GFX_LE(GFX10) },
   { ELK_OPCODE_MAD,      91,  "mad",     3,    1,    GFX_GE(GFX6) },
   { ELK_OPCODE_LRP,      92,  "lrp",     3,    1,    GFX_GE(GFX6) & GFX_LE(GFX10) },
   { ELK_OPCODE_MADM,     93,  "madm",    3,    1,    GFX_GE(GFX8) },
   { ELK_OPCODE_NENOP,    125, "nenop",   0,    0,    GFX45 },
   { ELK_OPCODE_NOP,      126, "nop",     0,    0,    GFX_LT(GFX12) },
   { ELK_OPCODE_NOP,      96,  "nop",     0,    0,    GFX_GE(GFX12) }
};

void
elk_init_isa_info(struct elk_isa_info *isa,
                  const struct intel_device_info *devinfo)
{
   isa->devinfo = devinfo;

   enum gfx_ver ver = gfx_ver_from_devinfo(devinfo);

   memset(isa->ir_to_descs, 0, sizeof(isa->ir_to_descs));
   memset(isa->hw_to_descs, 0, sizeof(isa->hw_to_descs));

   for (unsigned i = 0; i < ARRAY_SIZE(opcode_descs); i++) {
      if (opcode_descs[i].gfx_vers & ver) {
         const unsigned e = opcode_descs[i].ir;
         const unsigned h = opcode_descs[i].hw;
         assert(e < ARRAY_SIZE(isa->ir_to_descs) && !isa->ir_to_descs[e]);
         assert(h < ARRAY_SIZE(isa->hw_to_descs) && !isa->hw_to_descs[h]);
         isa->ir_to_descs[e] = &opcode_descs[i];
         isa->hw_to_descs[h] = &opcode_descs[i];
      }
   }
}

/**
 * Return the matching opcode_desc for the specified IR opcode and hardware
 * generation, or NULL if the opcode is not supported by the device.
 */
const struct elk_opcode_desc *
elk_opcode_desc(const struct elk_isa_info *isa, enum elk_opcode op)
{
   return op < ARRAY_SIZE(isa->ir_to_descs) ? isa->ir_to_descs[op] : NULL;
}

/**
 * Return the matching opcode_desc for the specified HW opcode and hardware
 * generation, or NULL if the opcode is not supported by the device.
 */
const struct elk_opcode_desc *
elk_opcode_desc_from_hw(const struct elk_isa_info *isa, unsigned hw)
{
   return hw < ARRAY_SIZE(isa->hw_to_descs) ? isa->hw_to_descs[hw] : NULL;
}

unsigned
elk_num_sources_from_inst(const struct elk_isa_info *isa,
                          const elk_inst *inst)
{
   const struct intel_device_info *devinfo = isa->devinfo;
   const struct elk_opcode_desc *desc =
      elk_opcode_desc(isa, elk_inst_opcode(isa, inst));
   unsigned math_function;

   if (elk_inst_opcode(isa, inst) == ELK_OPCODE_MATH) {
      math_function = elk_inst_math_function(devinfo, inst);
   } else if (devinfo->ver < 6 &&
              elk_inst_opcode(isa, inst) == ELK_OPCODE_SEND) {
      if (elk_inst_sfid(devinfo, inst) == ELK_SFID_MATH) {
         /* src1 must be a descriptor (including the information to determine
          * that the SEND is doing an extended math operation), but src0 can
          * actually be null since it serves as the source of the implicit GRF
          * to MRF move.
          *
          * If we stop using that functionality, we'll have to revisit this.
          */
         return 2;
      } else {
         /* Send instructions are allowed to have null sources since they use
          * the base_mrf field to specify which message register source.
          */
         return 0;
      }
   } else {
      assert(desc->nsrc < 4);
      return desc->nsrc;
   }

   switch (math_function) {
   case ELK_MATH_FUNCTION_INV:
   case ELK_MATH_FUNCTION_LOG:
   case ELK_MATH_FUNCTION_EXP:
   case ELK_MATH_FUNCTION_SQRT:
   case ELK_MATH_FUNCTION_RSQ:
   case ELK_MATH_FUNCTION_SIN:
   case ELK_MATH_FUNCTION_COS:
   case ELK_MATH_FUNCTION_SINCOS:
   case GFX8_MATH_FUNCTION_INVM:
   case GFX8_MATH_FUNCTION_RSQRTM:
      return 1;
   case ELK_MATH_FUNCTION_FDIV:
   case ELK_MATH_FUNCTION_POW:
   case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER:
   case ELK_MATH_FUNCTION_INT_DIV_QUOTIENT:
   case ELK_MATH_FUNCTION_INT_DIV_REMAINDER:
      return 2;
   default:
      unreachable("not reached");
   }
}
