/*
 * Copyright (c) 2017 Rob Clark <robdclark@gmail.com>
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/os_file.h"

#include "compiler/isaspec/isaspec.h"

#include "freedreno_pm4.h"

#include "afuc.h"
#include "afuc-isa.h"
#include "util.h"
#include "emu.h"

int gpuver;

/* non-verbose mode should output something suitable to feed back into
 * assembler.. verbose mode has additional output useful for debugging
 * (like unexpected bits that are set)
 */
static bool verbose = false;

/* emulator mode: */
static bool emulator = false;

#define printerr(fmt, ...) afuc_printc(AFUC_ERR, fmt, ##__VA_ARGS__)
#define printlbl(fmt, ...) afuc_printc(AFUC_LBL, fmt, ##__VA_ARGS__)

static const char *
getpm4(uint32_t id)
{
   return afuc_pm_id_name(id);
}

static void
print_gpu_reg(FILE *out, uint32_t regbase)
{
   if (regbase < 0x100)
      return;

   char *name = afuc_gpu_reg_name(regbase);
   if (name) {
      fprintf(out, "\t; %s", name);
      free(name);
   }
}

void
print_control_reg(uint32_t id)
{
   char *name = afuc_control_reg_name(id);
   if (name) {
      printf("@%s", name);
      free(name);
   } else {
      printf("0x%03x", id);
   }
}

void
print_sqe_reg(uint32_t id)
{
   char *name = afuc_sqe_reg_name(id);
   if (name) {
      printf("@%s", name);
      free(name);
   } else {
      printf("0x%03x", id);
   }
}

void
print_pipe_reg(uint32_t id)
{
   char *name = afuc_pipe_reg_name(id);
   if (name) {
      printf("|%s", name);
      free(name);
   } else {
      printf("0x%03x", id);
   }
}

struct decode_state {
   uint32_t immed;
   uint8_t shift;
   bool has_immed;
   bool dst_is_addr;
};

static void
field_print_cb(struct isa_print_state *state, const char *field_name, uint64_t val)
{
   if (!strcmp(field_name, "CONTROLREG")) {
      char *name = afuc_control_reg_name(val);
      if (name) {
         isa_print(state, "@%s", name);
         free(name);
      } else {
         isa_print(state, "0x%03x", (unsigned)val);
      }
   } else if (!strcmp(field_name, "SQEREG")) {
      char *name = afuc_sqe_reg_name(val);
      if (name) {
         isa_print(state, "%%%s", name);
         free(name);
      } else {
         isa_print(state, "0x%03x", (unsigned)val);
      }
   }
}

static void
pre_instr_cb(void *data, unsigned n, void *instr)
{
   struct decode_state *state = data;
   state->has_immed = state->dst_is_addr = false;
   state->shift = 0;

   if (verbose)
      printf("\t%04x: %08x  ", n, *(uint32_t *)instr);
}

static void
field_cb(void *data, const char *field_name, struct isa_decode_value *val)
{
   struct decode_state *state = data;

   if (!strcmp(field_name, "RIMMED")) {
      state->immed = val->num;
      state->has_immed = true;
   }

   if (!strcmp(field_name, "SHIFT")) {
      state->shift = val->num;
   }

   if (!strcmp(field_name, "DST")) {
      if (val->num == REG_ADDR)
         state->dst_is_addr = true;
   }
}

static void
post_instr_cb(void *data, unsigned n, void *instr)
{
   struct decode_state *state = data;

   if (state->has_immed) {
      uint32_t immed = state->immed << state->shift;
      if (state->dst_is_addr && state->shift >= 16) {
         immed &= ~0x40000; /* b18 disables auto-increment of address */
         if ((immed & 0x00ffffff) == 0) {
            printf("\t; ");
            print_pipe_reg(immed >> 24);
         }
      } else {
         print_gpu_reg(stdout, immed);
      }
   }
}

uint32_t jumptbl_offset = ~0;

/* Assume that instructions that don't match are raw data */
static void
no_match(FILE *out, const BITSET_WORD *bitset, size_t size)
{
   if (jumptbl_offset != ~0 && bitset[0] == afuc_nop_literal(jumptbl_offset, gpuver)) {
      fprintf(out, "[#jumptbl]\n");
   } else {
      fprintf(out, "[%08x]", bitset[0]);
      print_gpu_reg(out, bitset[0]);
      fprintf(out, "\n");
   }
}

static void
get_decode_options(struct isa_decode_options *options)
{
   *options = (struct isa_decode_options) {
      .gpu_id = gpuver,
      .branch_labels = true,
      .field_cb = field_cb,
      .field_print_cb = field_print_cb,
      .pre_instr_cb = pre_instr_cb,
      .post_instr_cb = post_instr_cb,
      .no_match_cb = no_match,
   };
}

static void
disasm_instr(struct isa_decode_options *options, uint32_t *instrs, unsigned pc)
{
   afuc_isa_disasm(&instrs[pc], 4, stdout, options);
}

static void
setup_packet_table(struct isa_decode_options *options,
                   uint32_t *jmptbl, uint32_t sizedwords)
{
   struct isa_entrypoint *entrypoints = malloc(sizedwords * sizeof(struct isa_entrypoint));

   for (unsigned i = 0; i < sizedwords; i++) {
      entrypoints[i].offset = jmptbl[i];
      unsigned n = i; // + CP_NOP;
      entrypoints[i].name = afuc_pm_id_name(n);
      if (!entrypoints[i].name) {
         char *name;
         asprintf(&name, "UNKN%d", n);
         entrypoints[i].name = name;
      }
   }

   options->entrypoints = entrypoints;
   options->entrypoint_count = sizedwords;
}

static uint32_t
find_jump_table(uint32_t *instrs, uint32_t sizedwords,
                uint32_t *jmptbl, uint32_t jmptbl_size)
{
   for (unsigned i = 0; i <= sizedwords - jmptbl_size; i++) {
      bool found = true;
      for (unsigned j = 0; j < jmptbl_size; j++) {
         if (instrs[i + j] != jmptbl[j]) {
            found = false;
            break;
         }
      }
      if (found)
         return i;
   }

   return ~0;
}

static void
disasm(struct emu *emu)
{
   uint32_t sizedwords = emu->sizedwords;
   uint32_t lpac_offset = 0, bv_offset = 0;

   EMU_GPU_REG(CP_SQE_INSTR_BASE);
   EMU_GPU_REG(CP_LPAC_SQE_INSTR_BASE);
   EMU_CONTROL_REG(BV_INSTR_BASE);
   EMU_CONTROL_REG(LPAC_INSTR_BASE);

   emu_init(emu);
   emu->processor = EMU_PROC_SQE;

   struct isa_decode_options options;
   struct decode_state state;
   get_decode_options(&options);
   options.cbdata = &state;

#ifdef BOOTSTRAP_DEBUG
   while (true) {
      disasm_instr(&options, emu->instrs, emu->gpr_regs.pc);
      emu_step(emu);
   }
#endif

   emu_run_bootstrap(emu);

   /* Figure out if we have BV/LPAC SQE appended: */
   if (gpuver >= 7) {
      bv_offset = emu_get_reg64(emu, &BV_INSTR_BASE) -
         emu_get_reg64(emu, &CP_SQE_INSTR_BASE);
      bv_offset /= 4;
      lpac_offset = emu_get_reg64(emu, &LPAC_INSTR_BASE) -
         emu_get_reg64(emu, &CP_SQE_INSTR_BASE);
      lpac_offset /= 4;
      sizedwords = MIN2(bv_offset, lpac_offset);
   } else {
      if (emu_get_reg64(emu, &CP_LPAC_SQE_INSTR_BASE)) {
         lpac_offset = emu_get_reg64(emu, &CP_LPAC_SQE_INSTR_BASE) -
               emu_get_reg64(emu, &CP_SQE_INSTR_BASE);
         lpac_offset /= 4;
         sizedwords = lpac_offset;
      }
   }

   setup_packet_table(&options, emu->jmptbl, ARRAY_SIZE(emu->jmptbl));

   jumptbl_offset = find_jump_table(emu->instrs, sizedwords, emu->jmptbl,
                                    ARRAY_SIZE(emu->jmptbl));

   /* TODO add option to emulate LPAC SQE instead: */
   if (emulator) {
      /* Start from clean slate: */
      emu_fini(emu);
      emu_init(emu);

      while (true) {
         disasm_instr(&options, emu->instrs, emu->gpr_regs.pc);
         emu_step(emu);
      }
   }

   /* print instructions: */
   afuc_isa_disasm(emu->instrs, MIN2(sizedwords, jumptbl_offset) * 4, stdout, &options);

   /* print jump table */
   if (jumptbl_offset != ~0) {
      if (gpuver >= 7) {
         /* The BV/LPAC microcode must be aligned to 32 bytes. On a7xx, by
          * convention the firmware aligns the jumptable preceding it instead
          * of the microcode itself, with nop instructions. Insert this
          * directive to make sure that it stays aligned when reassembling
          * even if the user modifies the BR microcode.
          */
         printf(".align 32\n");
      }
      printf("jumptbl:\n");
      printf(".jumptbl\n");

      if (jumptbl_offset + ARRAY_SIZE(emu->jmptbl) != sizedwords) {
         for (unsigned i = jumptbl_offset + ARRAY_SIZE(emu->jmptbl); i < sizedwords; i++)
            printf("[%08x]\n", emu->instrs[i]);
      }
   }

   if (bv_offset) {
      printf("\n.section BV\n");
      printf(";\n");
      printf("; BV microcode:\n");
      printf(";\n");

      emu_fini(emu);

      emu->processor = EMU_PROC_BV;
      emu->instrs += bv_offset;
      emu->sizedwords -= bv_offset;

      emu_init(emu);
      emu_run_bootstrap(emu);

      setup_packet_table(&options, emu->jmptbl, ARRAY_SIZE(emu->jmptbl));

      uint32_t sizedwords = lpac_offset - bv_offset;

      jumptbl_offset = find_jump_table(emu->instrs, sizedwords, emu->jmptbl,
                                       ARRAY_SIZE(emu->jmptbl));

      afuc_isa_disasm(emu->instrs, MIN2(sizedwords, jumptbl_offset) * 4, stdout, &options);

      if (jumptbl_offset != ~0) {
         printf(".align 32\n");
         printf("jumptbl:\n");
         printf(".jumptbl\n");
         if (jumptbl_offset + ARRAY_SIZE(emu->jmptbl) != sizedwords) {
            for (unsigned i = jumptbl_offset + ARRAY_SIZE(emu->jmptbl); i < sizedwords; i++)
               printf("[%08x]\n", emu->instrs[i]);
         }
      }

      emu->instrs -= bv_offset;
      emu->sizedwords += bv_offset;
   }

   if (lpac_offset) {
      printf("\n.section LPAC\n");
      printf(";\n");
      printf("; LPAC microcode:\n");
      printf(";\n");

      emu_fini(emu);

      emu->processor = EMU_PROC_LPAC;
      emu->instrs += lpac_offset;
      emu->sizedwords -= lpac_offset;

      emu_init(emu);
      emu_run_bootstrap(emu);

      setup_packet_table(&options, emu->jmptbl, ARRAY_SIZE(emu->jmptbl));

      jumptbl_offset = find_jump_table(emu->instrs, emu->sizedwords, emu->jmptbl,
                                       ARRAY_SIZE(emu->jmptbl));

      afuc_isa_disasm(emu->instrs, MIN2(emu->sizedwords, jumptbl_offset) * 4, stdout, &options);

      if (jumptbl_offset != ~0) {
         printf("jumptbl:\n");
         printf(".jumptbl\n");
         if (jumptbl_offset + ARRAY_SIZE(emu->jmptbl) != emu->sizedwords) {
            for (unsigned i = jumptbl_offset + ARRAY_SIZE(emu->jmptbl); i < emu->sizedwords; i++)
               printf("[%08x]\n", emu->instrs[i]);
         }
      }

      emu->instrs -= lpac_offset;
      emu->sizedwords += lpac_offset;
   }
}

static void
disasm_raw(uint32_t *instrs, int sizedwords)
{
   struct isa_decode_options options;
   struct decode_state state;
   get_decode_options(&options);
   options.cbdata = &state;

   afuc_isa_disasm(instrs, sizedwords * 4, stdout, &options);
}

static void
disasm_legacy(uint32_t *buf, int sizedwords)
{
   uint32_t *instrs = buf;
   const int jmptbl_start = instrs[1] & 0xffff;
   uint32_t *jmptbl = &buf[jmptbl_start];
   int i;

   struct isa_decode_options options;
   struct decode_state state;
   get_decode_options(&options);
   options.cbdata = &state;

   /* parse jumptable: */
   setup_packet_table(&options, jmptbl, 0x80);

   /* print instructions: */
   afuc_isa_disasm(instrs, sizedwords * 4, stdout, &options);

   /* print jumptable: */
   if (verbose) {
      printf(";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n");
      printf("; JUMP TABLE\n");
      for (i = 0; i < 0x7f; i++) {
         int n = i; // + CP_NOP;
         uint32_t offset = jmptbl[i];
         const char *name = getpm4(n);
         printf("%3d %02x: ", n, n);
         printf("%04x", offset);
         if (name) {
            printf("   ; %s", name);
         } else {
            printf("   ; UNKN%d", n);
         }
         printf("\n");
      }
   }
}

static void
usage(void)
{
   fprintf(stderr, "Usage:\n"
                   "\tdisasm [-g GPUVER] [-v] [-c] [-r] filename.asm\n"
                   "\t\t-c - use colors\n"
                   "\t\t-e - emulator mode\n"
                   "\t\t-g - override GPU firmware id\n"
                   "\t\t-r - raw disasm, don't try to find jumptable\n"
                   "\t\t-v - verbose output\n"
           );
   exit(2);
}

int
main(int argc, char **argv)
{
   uint32_t *buf;
   char *file;
   bool colors = false;
   size_t sz;
   int c, ret;
   bool unit_test = false;
   bool raw = false;
   enum afuc_fwid fw_id = 0;

   /* Argument parsing: */
   while ((c = getopt(argc, argv, "ceg:rvu")) != -1) {
      switch (c) {
      case 'c':
         colors = true;
         break;
      case 'e':
         emulator = true;
         verbose  = true;
         break;
      case 'g':
         fw_id = strtol(optarg, NULL, 16);
         break;
      case 'r':
         raw = true;
         break;
      case 'v':
         verbose = true;
         break;
      case 'u':
         /* special "hidden" flag for unit tests, to avoid file paths (which
          * can differ from reference output)
          */
         unit_test = true;
         break;
      default:
         usage();
      }
   }

   if (optind >= argc) {
      fprintf(stderr, "no file specified!\n");
      usage();
   }

   file = argv[optind];

   buf = (uint32_t *)os_read_file(file, &sz);

   if (!fw_id)
      fw_id = afuc_get_fwid(buf[1]);

   ret = afuc_util_init(fw_id, &gpuver, colors);
   if (ret < 0) {
      usage();
   }

   /* a6xx is *mostly* a superset of a5xx, but some opcodes shuffle
    * around, and behavior of special regs is a bit different.  Right
    * now we only bother to support the a6xx variant.
    */
   if (emulator && (gpuver < 6 || gpuver > 7)) {
      fprintf(stderr, "Emulator only supported on a6xx-a7xx!\n");
      return 1;
   }

   printf("; a%dxx microcode\n", gpuver);

   if (!unit_test)
      printf("; Disassembling microcode: %s\n", file);
   printf("; Version: %08x\n\n", buf[1]);

   if (raw) {
      disasm_raw(buf, sz / 4);
   } else if (gpuver < 6) {
      disasm_legacy(&buf[1], sz / 4 - 1);
   } else {
      struct emu emu = {
            .instrs = &buf[1],
            .sizedwords = sz / 4 - 1,
            .fw_id = fw_id,
      };

      disasm(&emu);
   }

   return 0;
}
