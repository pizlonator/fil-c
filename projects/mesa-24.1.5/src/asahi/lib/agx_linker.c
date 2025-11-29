/*
 * Copyright 2024 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "agx_linker.h"
#include <stddef.h>
#include <stdint.h>
#include "util/ralloc.h"
#include "agx_compile.h"
#include "agx_device.h"
#include "agx_pack.h"
#include "agx_scratch.h"

/*
 * When sample shading is used with a non-monolithic fragment shader, we
 * fast-link a program with the following structure:
 *
 *    Fragment prolog;
 *
 *    for (u16 sample_bit = 1; sample_bit < (1 << # of samples); ++sample_bit) {
 *       API fragment shader;
 *       Fragment epilog;
 *    }
 *
 * This means the prolog runs per-pixel but the fragment shader and epilog run
 * per-sample. To do this, we need to generate the loop on the fly. The
 * following binary sequences form the relevant loop.
 */

/* clang-format off */
static const uint8_t sample_loop_header[] = {
   /* mov_imm r0, 0x10000, 0b0 */
   0x62, 0x01, 0x00, 0x00, 0x01, 0x00,
};

#define STOP                                                                   \
   /* stop */                                                                  \
   0x88, 0x00,                                                                 \
                                                                               \
   /* trap */                                                                  \
   0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00,                             \
   0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00,

static const uint8_t stop[] = {STOP};

static const uint8_t sample_loop_footer[] = {
   /* iadd r0h, 0, r0h, lsl 1 */
   0x0e, 0x02, 0x00, 0x10, 0x84, 0x00, 0x00, 0x00,

   /* while_icmp r0l, ult, r0h, 0, 1 */
   0x52, 0x2c, 0x41, 0x00, 0x00, 0x00,

   /* jmp_exec_any */
   0x00, 0xc0, 0x00, 0x00, 0x00, 0x00,

   /* pop_exec r0l, 1 */
   0x52, 0x0e, 0x00, 0x00, 0x00, 0x00,

   STOP
};

/* Offset in sample_loop_footer to the jmp_exec_any's target */
#define SAMPLE_LOOP_FOOTER_JMP_PATCH_OFFS (16)

/* Offset of the jmp_exec_any, for calculating the PC offsets */
#define SAMPLE_LOOP_FOOTER_JMP_OFFS (14)

/* Offset in sample_loop_footer to the while_icmp's sample count immediate. Bit
 * position in the byte given by the shift.
 */
#define SAMPLE_LOOP_FOOTER_COUNT_PATCH_OFFS (11)
#define SAMPLE_LOOP_FOOTER_COUNT_SHIFT (4)
/* clang-format on */

struct agx_linked_shader *
agx_fast_link(void *memctx, struct agx_device *dev, bool fragment,
              struct agx_shader_part *main, struct agx_shader_part *prolog,
              struct agx_shader_part *epilog, unsigned nr_samples_shaded)
{
   struct agx_linked_shader *linked = rzalloc(memctx, struct agx_linked_shader);

   size_t size = 0;
   unsigned nr_gprs = 0, scratch_size = 0;
   bool reads_tib = false, writes_sample_mask = false,
        disable_tri_merging = false, tag_write_disable = true;

   if (nr_samples_shaded) {
      size += sizeof(sample_loop_header);

      if (nr_samples_shaded > 1)
         size += sizeof(sample_loop_footer);
      else
         size += sizeof(stop);
   }

   struct agx_shader_part *parts[] = {prolog, main, epilog};

   for (unsigned i = 0; i < ARRAY_SIZE(parts); ++i) {
      struct agx_shader_part *part = parts[i];
      if (!part)
         continue;

      assert(part->info.main_offset == 0);
      size += part->info.main_size;

      nr_gprs = MAX2(nr_gprs, part->info.nr_gprs);
      scratch_size = MAX2(scratch_size, part->info.scratch_size);
      reads_tib |= part->info.reads_tib;
      writes_sample_mask |= part->info.writes_sample_mask;
      disable_tri_merging |= part->info.disable_tri_merging;
      linked->uses_base_param |= part->info.uses_base_param;
      tag_write_disable &= part->info.tag_write_disable;
   }

   assert(size > 0 && "must stop");

   linked->bo = agx_bo_create(dev, size, AGX_BO_EXEC | AGX_BO_LOW_VA,
                              "Linked executable");

   size_t offset = 0;

   /* FS prolog happens per-pixel, outside the sample loop */
   if (prolog) {
      size_t sz = prolog->info.main_size;
      memcpy((uint8_t *)linked->bo->ptr.cpu + offset, prolog->binary, sz);
      offset += sz;
   }

   if (nr_samples_shaded) {
      memcpy((uint8_t *)linked->bo->ptr.cpu + offset, sample_loop_header,
             sizeof(sample_loop_header));
      offset += sizeof(sample_loop_header);
   }

   size_t sample_loop_begin = offset;

   /* Main shader and epilog happen in the sample loop, so start from i=1 */
   for (unsigned i = 1; i < ARRAY_SIZE(parts); ++i) {
      struct agx_shader_part *part = parts[i];
      if (!part)
         continue;

      size_t sz = part->info.main_size;
      memcpy((uint8_t *)linked->bo->ptr.cpu + offset, part->binary, sz);
      offset += sz;
   }

   if (nr_samples_shaded > 1) {
      assert(sample_loop_footer[SAMPLE_LOOP_FOOTER_COUNT_PATCH_OFFS] == 0);

      /* Make a stack copy of the footer so we can efficiently patch it */
      uint8_t footer[sizeof(sample_loop_footer)];
      memcpy(footer, sample_loop_footer, sizeof(footer));

      /* Patch in sample end */
      uint8_t end = (1u << nr_samples_shaded) - 1;
      footer[SAMPLE_LOOP_FOOTER_COUNT_PATCH_OFFS] =
         end << SAMPLE_LOOP_FOOTER_COUNT_SHIFT;

      /* Patch in the branch target */
      int32_t loop_size = offset - sample_loop_begin;
      int32_t branch_offs = -(SAMPLE_LOOP_FOOTER_JMP_OFFS + loop_size);
      int32_t *target = (int32_t *)(footer + SAMPLE_LOOP_FOOTER_JMP_PATCH_OFFS);
      *target = branch_offs;

      /* Copy in the patched footer */
      memcpy((uint8_t *)linked->bo->ptr.cpu + offset, footer, sizeof(footer));
      offset += sizeof(footer);
   } else if (nr_samples_shaded) {
      /* Just end after the first sample, no need to loop for a single sample */
      memcpy((uint8_t *)linked->bo->ptr.cpu + offset, stop, sizeof(stop));
      offset += sizeof(stop);
   }

   assert(offset == size);

   agx_pack(&linked->shader, USC_SHADER, cfg) {
      cfg.code = linked->bo->ptr.gpu;
      cfg.unk_2 = fragment ? 2 : 3;

      if (fragment)
         cfg.loads_varyings = linked->cf.nr_bindings > 0;
   }

   agx_pack(&linked->regs, USC_REGISTERS, cfg) {
      cfg.register_count = nr_gprs;
      cfg.unk_1 = fragment;
      cfg.spill_size = scratch_size ? agx_scratch_get_bucket(scratch_size) : 0;
   }

   if (fragment) {
      agx_pack(&linked->fragment_props, USC_FRAGMENT_PROPERTIES, cfg) {
         cfg.early_z_testing = !writes_sample_mask;
         cfg.unk_4 = 0x2;
         cfg.unk_5 = 0x0;
      }

      agx_pack(&linked->fragment_control, FRAGMENT_CONTROL, cfg) {
         cfg.tag_write_disable = tag_write_disable;
         cfg.disable_tri_merging = disable_tri_merging;

         if (reads_tib && writes_sample_mask)
            cfg.pass_type = AGX_PASS_TYPE_TRANSLUCENT_PUNCH_THROUGH;
         else if (reads_tib)
            cfg.pass_type = AGX_PASS_TYPE_TRANSLUCENT;
         else if (writes_sample_mask)
            cfg.pass_type = AGX_PASS_TYPE_PUNCH_THROUGH;
         else
            cfg.pass_type = AGX_PASS_TYPE_OPAQUE;
      }

      /* Merge the CF binding lists from the prolog to handle cull distance */
      memcpy(&linked->cf, &main->info.varyings.fs,
             sizeof(struct agx_varyings_fs));

      struct agx_varyings_fs *prolog_vary =
         prolog ? &prolog->info.varyings.fs : NULL;

      if (prolog_vary && prolog_vary->nr_bindings) {
         assert(!prolog_vary->reads_z);
         linked->cf.nr_cf = MAX2(linked->cf.nr_cf, prolog_vary->nr_cf);

         assert(linked->cf.nr_bindings + prolog_vary->nr_bindings <=
                   ARRAY_SIZE(linked->cf.bindings) &&
                "bounded by # of coeff registers");

         memcpy(linked->cf.bindings + linked->cf.nr_bindings,
                prolog_vary->bindings,
                sizeof(struct agx_cf_binding) * prolog_vary->nr_bindings);

         linked->cf.nr_bindings += prolog_vary->nr_bindings;
      }

      agx_pack(&linked->osel, OUTPUT_SELECT, cfg) {
         cfg.varyings = linked->cf.nr_bindings > 0;
         cfg.frag_coord_z = linked->cf.reads_z;
      }
   }

   return linked;
}
