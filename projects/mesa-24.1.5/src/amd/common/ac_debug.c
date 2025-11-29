/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ac_debug.h"
#include "sid.h"
#include "sid_tables.h"

#include "util/u_string.h"

#include <inttypes.h>

const struct si_reg *ac_find_register(enum amd_gfx_level gfx_level, enum radeon_family family,
                                      unsigned offset)
{
   const struct si_reg *table;
   unsigned table_size;

   switch (gfx_level) {
   case GFX11_5:
      table = gfx115_reg_table;
      table_size = ARRAY_SIZE(gfx115_reg_table);
      break;
   case GFX11:
      table = gfx11_reg_table;
      table_size = ARRAY_SIZE(gfx11_reg_table);
      break;
   case GFX10_3:
      table = gfx103_reg_table;
      table_size = ARRAY_SIZE(gfx103_reg_table);
      break;
   case GFX10:
      table = gfx10_reg_table;
      table_size = ARRAY_SIZE(gfx10_reg_table);
      break;
   case GFX9:
      if (family == CHIP_GFX940) {
         table = gfx940_reg_table;
         table_size = ARRAY_SIZE(gfx940_reg_table);
         break;
      }
      table = gfx9_reg_table;
      table_size = ARRAY_SIZE(gfx9_reg_table);
      break;
   case GFX8:
      if (family == CHIP_STONEY) {
         table = gfx81_reg_table;
         table_size = ARRAY_SIZE(gfx81_reg_table);
         break;
      }
      table = gfx8_reg_table;
      table_size = ARRAY_SIZE(gfx8_reg_table);
      break;
   case GFX7:
      table = gfx7_reg_table;
      table_size = ARRAY_SIZE(gfx7_reg_table);
      break;
   case GFX6:
      table = gfx6_reg_table;
      table_size = ARRAY_SIZE(gfx6_reg_table);
      break;
   default:
      return NULL;
   }

   for (unsigned i = 0; i < table_size; i++) {
      const struct si_reg *reg = &table[i];

      if (reg->offset == offset)
         return reg;
   }

   return NULL;
}

const char *ac_get_register_name(enum amd_gfx_level gfx_level, enum radeon_family family,
                                 unsigned offset)
{
   const struct si_reg *reg = ac_find_register(gfx_level, family, offset);

   return reg ? sid_strings + reg->name_offset : "(no name)";
}

bool ac_register_exists(enum amd_gfx_level gfx_level, enum radeon_family family,
                        unsigned offset)
{
   return ac_find_register(gfx_level, family, offset) != NULL;
}

/**
 * Parse dmesg and return TRUE if a VM fault has been detected.
 *
 * \param gfx_level		gfx level
 * \param old_dmesg_timestamp	previous dmesg timestamp parsed at init time
 * \param out_addr		detected VM fault addr
 */
bool ac_vm_fault_occurred(enum amd_gfx_level gfx_level, uint64_t *old_dmesg_timestamp,
                         uint64_t *out_addr)
{
#ifdef _WIN32
   return false;
#else
   char line[2000];
   unsigned sec, usec;
   int progress = 0;
   uint64_t dmesg_timestamp = 0;
   bool fault = false;

   FILE *p = popen("dmesg", "r");
   if (!p)
      return false;

   while (fgets(line, sizeof(line), p)) {
      char *msg, len;

      if (!line[0] || line[0] == '\n')
         continue;

      /* Get the timestamp. */
      if (sscanf(line, "[%u.%u]", &sec, &usec) != 2) {
         static bool hit = false;
         if (!hit) {
            fprintf(stderr, "%s: failed to parse line '%s'\n", __func__, line);
            hit = true;
         }
         continue;
      }
      dmesg_timestamp = sec * 1000000ull + usec;

      /* If just updating the timestamp. */
      if (!out_addr)
         continue;

      /* Process messages only if the timestamp is newer. */
      if (dmesg_timestamp <= *old_dmesg_timestamp)
         continue;

      /* Only process the first VM fault. */
      if (fault)
         continue;

      /* Remove trailing \n */
      len = strlen(line);
      if (len && line[len - 1] == '\n')
         line[len - 1] = 0;

      /* Get the message part. */
      msg = strchr(line, ']');
      if (!msg)
         continue;
      msg++;

      const char *header_line, *addr_line_prefix, *addr_line_format;

      if (gfx_level >= GFX9) {
         /* Match this:
          * ..: [gfxhub] VMC page fault (src_id:0 ring:158 vm_id:2 pas_id:0)
          * ..:   at page 0x0000000219f8f000 from 27
          * ..: VM_L2_PROTECTION_FAULT_STATUS:0x0020113C
          */
         header_line = "VMC page fault";
         addr_line_prefix = "   at page";
         addr_line_format = "%" PRIx64;
      } else {
         header_line = "GPU fault detected:";
         addr_line_prefix = "VM_CONTEXT1_PROTECTION_FAULT_ADDR";
         addr_line_format = "%" PRIX64;
      }

      switch (progress) {
      case 0:
         if (strstr(msg, header_line))
            progress = 1;
         break;
      case 1:
         msg = strstr(msg, addr_line_prefix);
         if (msg) {
            msg = strstr(msg, "0x");
            if (msg) {
               msg += 2;
               if (sscanf(msg, addr_line_format, out_addr) == 1)
                  fault = true;
            }
         }
         progress = 0;
         break;
      default:
         progress = 0;
      }
   }
   pclose(p);

   if (dmesg_timestamp > *old_dmesg_timestamp)
      *old_dmesg_timestamp = dmesg_timestamp;

   return fault;
#endif
}

static int compare_wave(const void *p1, const void *p2)
{
   struct ac_wave_info *w1 = (struct ac_wave_info *)p1;
   struct ac_wave_info *w2 = (struct ac_wave_info *)p2;

   /* Sort waves according to PC and then SE, SH, CU, etc. */
   if (w1->pc < w2->pc)
      return -1;
   if (w1->pc > w2->pc)
      return 1;
   if (w1->se < w2->se)
      return -1;
   if (w1->se > w2->se)
      return 1;
   if (w1->sh < w2->sh)
      return -1;
   if (w1->sh > w2->sh)
      return 1;
   if (w1->cu < w2->cu)
      return -1;
   if (w1->cu > w2->cu)
      return 1;
   if (w1->simd < w2->simd)
      return -1;
   if (w1->simd > w2->simd)
      return 1;
   if (w1->wave < w2->wave)
      return -1;
   if (w1->wave > w2->wave)
      return 1;

   return 0;
}

#define AC_UMR_REGISTERS_LINE "Main Registers"

static bool
ac_read_umr_register(char **_scan, const char *name, uint32_t *value)
{
   char *scan = *_scan;
   if (strncmp(scan, name, MIN2(strlen(scan), strlen(name))))
      return false;

   scan += strlen(name);
   scan += strlen(": ");

   *value = strtoul(scan, NULL, 16);
   *_scan = scan + 8;
   return true;
}

/* Return wave information. "waves" should be a large enough array. */
unsigned ac_get_wave_info(enum amd_gfx_level gfx_level, const struct radeon_info *info,
                          struct ac_wave_info waves[AC_MAX_WAVES_PER_CHIP])
{
#ifdef _WIN32
   return 0;
#else
   char line[2000], cmd[256];
   unsigned num_waves = 0;

   sprintf(cmd, "umr --by-pci %04x:%02x:%02x.%01x -O halt_waves -wa %s",
           info->pci.domain, info->pci.bus, info->pci.dev, info->pci.func,
           gfx_level >= GFX10 ? "gfx_0.0.0" : "gfx");

   FILE *p = popen(cmd, "r");
   if (!p)
      return 0;

   while (fgets(line, sizeof(line), p)) {
      if (strncmp(line, AC_UMR_REGISTERS_LINE, strlen(AC_UMR_REGISTERS_LINE)))
         continue;

      assert(num_waves < AC_MAX_WAVES_PER_CHIP);
      struct ac_wave_info *w = &waves[num_waves];
      memset(w, 0, sizeof(struct ac_wave_info));
      num_waves++;

      while (fgets(line, sizeof(line), p)) {
         if (strlen(line) < 2)
            break;

         char *scan = line;
         while (scan < line + strlen(line)) {
            if (strncmp(scan, "ix", MIN2(strlen(scan), strlen("ix")))) {
               scan++;
               continue;
            }

            scan += strlen("ix");

            bool progress = false;

            progress |= ac_read_umr_register(&scan, "SQ_WAVE_STATUS", &w->status);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_PC_LO", &w->pc_lo);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_PC_HI", &w->pc_hi);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_EXEC_LO", &w->exec_lo);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_EXEC_HI", &w->exec_hi);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_INST_DW0", &w->inst_dw0);
            progress |= ac_read_umr_register(&scan, "SQ_WAVE_INST_DW1", &w->inst_dw1);

            uint32_t wave;
            if (ac_read_umr_register(&scan, "SQ_WAVE_HW_ID", &wave)) {
               w->se = G_000050_SE_ID(wave);
               w->sh = G_000050_SH_ID(wave);
               w->cu = G_000050_CU_ID(wave);
               w->simd = G_000050_SIMD_ID(wave);
               w->wave = G_000050_WAVE_ID(wave);

               progress = true;
            }

            if (ac_read_umr_register(&scan, "SQ_WAVE_HW_ID1", &wave)) {
               w->se = G_00045C_SE_ID(wave);
               w->sh = G_00045C_SA_ID(wave);
               w->cu = G_00045C_WGP_ID(wave);
               w->simd = G_00045C_SIMD_ID(wave);
               w->wave = G_00045C_WAVE_ID(wave);

               progress = true;
            }

            /* Skip registers we do not handle. */
            if (!progress) {
               while (scan < line + strlen(line)) {
                  if (*scan == '|') {
                     progress = true;
                     break;
                  }
                  scan++;
               }
            }

            if (!progress)
               break;
         }
      }
   }

   qsort(waves, num_waves, sizeof(struct ac_wave_info), compare_wave);

   pclose(p);
   return num_waves;
#endif
}

/* List of GFXHUB clients from AMDGPU source code. */
static const char *const gfx10_gfxhub_client_ids[] = {
   "CB/DB",
   "Reserved",
   "GE1",
   "GE2",
   "CPF",
   "CPC",
   "CPG",
   "RLC",
   "TCP",
   "SQC (inst)",
   "SQC (data)",
   "SQG",
   "Reserved",
   "SDMA0",
   "SDMA1",
   "GCR",
   "SDMA2",
   "SDMA3",
};

static const char *
ac_get_gfx10_gfxhub_client(unsigned cid)
{
   if (cid >= ARRAY_SIZE(gfx10_gfxhub_client_ids))
      return "UNKNOWN";
   return gfx10_gfxhub_client_ids[cid];
}

void ac_print_gpuvm_fault_status(FILE *output, enum amd_gfx_level gfx_level,
                                 uint32_t status)
{
   if (gfx_level >= GFX10) {
      const uint8_t cid = G_00A130_CID(status);

      fprintf(output, "GCVM_L2_PROTECTION_FAULT_STATUS: 0x%x\n", status);
      fprintf(output, "\t CLIENT_ID: (%s) 0x%x\n", ac_get_gfx10_gfxhub_client(cid), cid);
      fprintf(output, "\t MORE_FAULTS: %d\n", G_00A130_MORE_FAULTS(status));
      fprintf(output, "\t WALKER_ERROR: %d\n", G_00A130_WALKER_ERROR(status));
      fprintf(output, "\t PERMISSION_FAULTS: %d\n", G_00A130_PERMISSION_FAULTS(status));
      fprintf(output, "\t MAPPING_ERROR: %d\n", G_00A130_MAPPING_ERROR(status));
      fprintf(output, "\t RW: %d\n", G_00A130_RW(status));
   } else {
      fprintf(output, "VM_CONTEXT1_PROTECTION_FAULT_STATUS: 0x%x\n", status);
   }
}
