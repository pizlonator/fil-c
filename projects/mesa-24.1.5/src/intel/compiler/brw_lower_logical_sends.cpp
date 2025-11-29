/*
 * Copyright © 2010, 2022 Intel Corporation
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

/**
 * @file brw_lower_logical_sends.cpp
 */

#include "brw_eu.h"
#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

static void
lower_urb_read_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   const bool per_slot_present =
      inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS].file != BAD_FILE;

   assert(inst->size_written % REG_SIZE == 0);
   assert(inst->header_size == 0);

   fs_reg payload_sources[2];
   unsigned header_size = 0;
   payload_sources[header_size++] = inst->src[URB_LOGICAL_SRC_HANDLE];
   if (per_slot_present)
      payload_sources[header_size++] = inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS];

   fs_reg payload = fs_reg(VGRF, bld.shader->alloc.allocate(header_size),
                           BRW_REGISTER_TYPE_F);
   bld.LOAD_PAYLOAD(payload, payload_sources, header_size, header_size);

   inst->opcode = SHADER_OPCODE_SEND;
   inst->header_size = header_size;

   inst->sfid = BRW_SFID_URB;
   inst->desc = brw_urb_desc(devinfo,
                             GFX8_URB_OPCODE_SIMD8_READ,
                             per_slot_present,
                             false,
                             inst->offset);

   inst->mlen = header_size;
   inst->ex_desc = 0;
   inst->ex_mlen = 0;
   inst->send_is_volatile = true;

   inst->resize_sources(4);

   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = payload;
   inst->src[3] = brw_null_reg();
}

static void
lower_urb_read_logical_send_xe2(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   assert(devinfo->has_lsc);

   assert(inst->size_written % (REG_SIZE * reg_unit(devinfo)) == 0);
   assert(inst->header_size == 0);

   /* Get the logical send arguments. */
   const fs_reg handle = inst->src[URB_LOGICAL_SRC_HANDLE];

   /* Calculate the total number of components of the payload. */
   const unsigned dst_comps = inst->size_written / (REG_SIZE * reg_unit(devinfo));

   fs_reg payload = bld.vgrf(BRW_REGISTER_TYPE_UD);

   bld.MOV(payload, handle);

   /* The low 24-bits of the URB handle is a byte offset into the URB area.
    * Add the (OWord) offset of the write to this value.
    */
   if (inst->offset) {
      bld.ADD(payload, payload, brw_imm_ud(inst->offset * 16));
      inst->offset = 0;
   }

   fs_reg offsets = inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS];
   if (offsets.file != BAD_FILE) {
      bld.ADD(payload, payload, offsets);
   }

   inst->sfid = BRW_SFID_URB;

   assert((dst_comps >= 1 && dst_comps <= 4) || dst_comps == 8);

   inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD,
                             LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A32,
                             LSC_DATA_SIZE_D32, dst_comps /* num_channels */,
                             false /* transpose */,
                             LSC_CACHE(devinfo, STORE, L1UC_L3UC));

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, inst->exec_size);
   inst->ex_mlen = 0;
   inst->header_size = 0;
   inst->send_has_side_effects = true;
   inst->send_is_volatile = false;

   inst->resize_sources(4);

   inst->src[0] = brw_imm_ud(0);
   inst->src[1] = brw_imm_ud(0);

   inst->src[2] = payload;
   inst->src[3] = brw_null_reg();
}

static void
lower_urb_write_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   const bool per_slot_present =
      inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS].file != BAD_FILE;
   const bool channel_mask_present =
      inst->src[URB_LOGICAL_SRC_CHANNEL_MASK].file != BAD_FILE;

   assert(inst->header_size == 0);

   const unsigned length = 1 + per_slot_present + channel_mask_present +
                           inst->components_read(URB_LOGICAL_SRC_DATA);

   fs_reg *payload_sources = new fs_reg[length];
   fs_reg payload = fs_reg(VGRF, bld.shader->alloc.allocate(length),
                           BRW_REGISTER_TYPE_F);

   unsigned header_size = 0;
   payload_sources[header_size++] = inst->src[URB_LOGICAL_SRC_HANDLE];
   if (per_slot_present)
      payload_sources[header_size++] = inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS];

   if (channel_mask_present)
      payload_sources[header_size++] = inst->src[URB_LOGICAL_SRC_CHANNEL_MASK];

   for (unsigned i = header_size, j = 0; i < length; i++, j++)
      payload_sources[i] = offset(inst->src[URB_LOGICAL_SRC_DATA], bld, j);

   bld.LOAD_PAYLOAD(payload, payload_sources, length, header_size);

   delete [] payload_sources;

   inst->opcode = SHADER_OPCODE_SEND;
   inst->header_size = header_size;
   inst->dst = brw_null_reg();

   inst->sfid = BRW_SFID_URB;
   inst->desc = brw_urb_desc(devinfo,
                             GFX8_URB_OPCODE_SIMD8_WRITE,
                             per_slot_present,
                             channel_mask_present,
                             inst->offset);

   inst->mlen = length;
   inst->ex_desc = 0;
   inst->ex_mlen = 0;
   inst->send_has_side_effects = true;

   inst->resize_sources(4);

   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = payload;
   inst->src[3] = brw_null_reg();
}

static void
lower_urb_write_logical_send_xe2(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   assert(devinfo->has_lsc);

   /* Get the logical send arguments. */
   const fs_reg handle = inst->src[URB_LOGICAL_SRC_HANDLE];
   const fs_reg src = inst->components_read(URB_LOGICAL_SRC_DATA) ?
      inst->src[URB_LOGICAL_SRC_DATA] : fs_reg(brw_imm_ud(0));
   assert(type_sz(src.type) == 4);

   /* Calculate the total number of components of the payload. */
   const unsigned src_comps = MAX2(1, inst->components_read(URB_LOGICAL_SRC_DATA));
   const unsigned src_sz = type_sz(src.type);

   fs_reg payload = bld.vgrf(BRW_REGISTER_TYPE_UD);

   bld.MOV(payload, handle);

   /* The low 24-bits of the URB handle is a byte offset into the URB area.
    * Add the (OWord) offset of the write to this value.
    */
   if (inst->offset) {
      bld.ADD(payload, payload, brw_imm_ud(inst->offset * 16));
      inst->offset = 0;
   }

   fs_reg offsets = inst->src[URB_LOGICAL_SRC_PER_SLOT_OFFSETS];
   if (offsets.file != BAD_FILE) {
      bld.ADD(payload, payload, offsets);
   }

   const fs_reg cmask = inst->src[URB_LOGICAL_SRC_CHANNEL_MASK];
   unsigned mask = 0;

   if (cmask.file != BAD_FILE) {
      assert(cmask.file == IMM);
      assert(cmask.type == BRW_REGISTER_TYPE_UD);
      mask = cmask.ud >> 16;
   }

   fs_reg payload2 = bld.move_to_vgrf(src, src_comps);
   const unsigned ex_mlen = (src_comps * src_sz * inst->exec_size) / REG_SIZE;

   inst->sfid = BRW_SFID_URB;

   enum lsc_opcode op = mask ? LSC_OP_STORE_CMASK : LSC_OP_STORE;
   inst->desc = lsc_msg_desc_wcmask(devinfo, op,
                             LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A32,
                             LSC_DATA_SIZE_D32, src_comps /* num_channels */,
                             false /* transpose */,
                             LSC_CACHE(devinfo, STORE, L1UC_L3UC),
                             mask);


   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, inst->exec_size);
   inst->ex_mlen = ex_mlen;
   inst->header_size = 0;
   inst->send_has_side_effects = true;
   inst->send_is_volatile = false;

   inst->resize_sources(4);

   inst->src[0] = brw_imm_ud(0);
   inst->src[1] = brw_imm_ud(0);

   inst->src[2] = payload;
   inst->src[3] = payload2;
}

static void
setup_color_payload(const fs_builder &bld, const brw_wm_prog_key *key,
                    fs_reg *dst, fs_reg color, unsigned components)
{
   if (key->clamp_fragment_color) {
      fs_reg tmp = bld.vgrf(BRW_REGISTER_TYPE_F, 4);
      assert(color.type == BRW_REGISTER_TYPE_F);

      for (unsigned i = 0; i < components; i++)
         set_saturate(true,
                      bld.MOV(offset(tmp, bld, i), offset(color, bld, i)));

      color = tmp;
   }

   for (unsigned i = 0; i < components; i++)
      dst[i] = offset(color, bld, i);
}

static void
lower_fb_write_logical_send(const fs_builder &bld, fs_inst *inst,
                            const struct brw_wm_prog_data *prog_data,
                            const brw_wm_prog_key *key,
                            const fs_thread_payload &fs_payload)
{
   assert(inst->src[FB_WRITE_LOGICAL_SRC_COMPONENTS].file == IMM);
   const intel_device_info *devinfo = bld.shader->devinfo;
   const fs_reg color0 = inst->src[FB_WRITE_LOGICAL_SRC_COLOR0];
   const fs_reg color1 = inst->src[FB_WRITE_LOGICAL_SRC_COLOR1];
   const fs_reg src0_alpha = inst->src[FB_WRITE_LOGICAL_SRC_SRC0_ALPHA];
   const fs_reg src_depth = inst->src[FB_WRITE_LOGICAL_SRC_SRC_DEPTH];
   const fs_reg dst_depth = inst->src[FB_WRITE_LOGICAL_SRC_DST_DEPTH];
   const fs_reg src_stencil = inst->src[FB_WRITE_LOGICAL_SRC_SRC_STENCIL];
   fs_reg sample_mask = inst->src[FB_WRITE_LOGICAL_SRC_OMASK];
   const unsigned components =
      inst->src[FB_WRITE_LOGICAL_SRC_COMPONENTS].ud;

   assert(inst->target != 0 || src0_alpha.file == BAD_FILE);

   fs_reg sources[15];
   int header_size = 2, payload_header_size;
   unsigned length = 0;

   if (devinfo->ver < 11 &&
      (color1.file != BAD_FILE || key->nr_color_regions > 1)) {
      assert(devinfo->ver < 20);

      /* From the Sandy Bridge PRM, volume 4, page 198:
       *
       *     "Dispatched Pixel Enables. One bit per pixel indicating
       *      which pixels were originally enabled when the thread was
       *      dispatched. This field is only required for the end-of-
       *      thread message and on all dual-source messages."
       */
      const fs_builder ubld = bld.exec_all().group(8, 0);

      fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD, 2);
      if (bld.group() < 16) {
         /* The header starts off as g0 and g1 for the first half */
         ubld.group(16, 0).MOV(header, retype(brw_vec8_grf(0, 0),
                                              BRW_REGISTER_TYPE_UD));
      } else {
         /* The header starts off as g0 and g2 for the second half */
         assert(bld.group() < 32);
         const fs_reg header_sources[2] = {
            retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD),
            retype(brw_vec8_grf(2, 0), BRW_REGISTER_TYPE_UD),
         };
         ubld.LOAD_PAYLOAD(header, header_sources, 2, 0);

         /* Gfx12 will require additional fix-ups if we ever hit this path. */
         assert(devinfo->ver < 12);
      }

      uint32_t g00_bits = 0;

      /* Set "Source0 Alpha Present to RenderTarget" bit in message
       * header.
       */
      if (src0_alpha.file != BAD_FILE)
         g00_bits |= 1 << 11;

      /* Set computes stencil to render target */
      if (prog_data->computed_stencil)
         g00_bits |= 1 << 14;

      if (g00_bits) {
         /* OR extra bits into g0.0 */
         ubld.group(1, 0).OR(component(header, 0),
                             retype(brw_vec1_grf(0, 0),
                                    BRW_REGISTER_TYPE_UD),
                             brw_imm_ud(g00_bits));
      }

      /* Set the render target index for choosing BLEND_STATE. */
      if (inst->target > 0) {
         ubld.group(1, 0).MOV(component(header, 2), brw_imm_ud(inst->target));
      }

      if (prog_data->uses_kill) {
         ubld.group(1, 0).MOV(retype(component(header, 15),
                                     BRW_REGISTER_TYPE_UW),
                              brw_sample_mask_reg(bld));
      }

      assert(length == 0);
      sources[0] = header;
      sources[1] = horiz_offset(header, 8);
      length = 2;
   }
   assert(length == 0 || length == 2);
   header_size = length;

   if (fs_payload.aa_dest_stencil_reg[0]) {
      assert(inst->group < 16);
      sources[length] = fs_reg(VGRF, bld.shader->alloc.allocate(1));
      bld.group(8, 0).exec_all().annotate("FB write stencil/AA alpha")
         .MOV(sources[length],
              fs_reg(brw_vec8_grf(fs_payload.aa_dest_stencil_reg[0], 0)));
      length++;
   }

   if (src0_alpha.file != BAD_FILE) {
      for (unsigned i = 0; i < bld.dispatch_width() / 8; i++) {
         const fs_builder &ubld = bld.exec_all().group(8, i)
                                    .annotate("FB write src0 alpha");
         const fs_reg tmp = ubld.vgrf(BRW_REGISTER_TYPE_F);
         ubld.MOV(tmp, horiz_offset(src0_alpha, i * 8));
         setup_color_payload(ubld, key, &sources[length], tmp, 1);
         length++;
      }
   }

   if (sample_mask.file != BAD_FILE) {
      const fs_reg tmp(VGRF, bld.shader->alloc.allocate(reg_unit(devinfo)),
                       BRW_REGISTER_TYPE_UD);

      /* Hand over gl_SampleMask.  Only the lower 16 bits of each channel are
       * relevant.  Since it's unsigned single words one vgrf is always
       * 16-wide, but only the lower or higher 8 channels will be used by the
       * hardware when doing a SIMD8 write depending on whether we have
       * selected the subspans for the first or second half respectively.
       */
      assert(sample_mask.file != BAD_FILE && type_sz(sample_mask.type) == 4);
      sample_mask.type = BRW_REGISTER_TYPE_UW;
      sample_mask.stride *= 2;

      bld.exec_all().annotate("FB write oMask")
         .MOV(horiz_offset(retype(tmp, BRW_REGISTER_TYPE_UW),
                           inst->group % (16 * reg_unit(devinfo))),
              sample_mask);

      for (unsigned i = 0; i < reg_unit(devinfo); i++)
         sources[length++] = byte_offset(tmp, REG_SIZE * i);
   }

   payload_header_size = length;

   setup_color_payload(bld, key, &sources[length], color0, components);
   length += 4;

   if (color1.file != BAD_FILE) {
      setup_color_payload(bld, key, &sources[length], color1, components);
      length += 4;
   }

   if (src_depth.file != BAD_FILE) {
      sources[length] = src_depth;
      length++;
   }

   if (dst_depth.file != BAD_FILE) {
      sources[length] = dst_depth;
      length++;
   }

   if (src_stencil.file != BAD_FILE) {
      assert(bld.dispatch_width() == 8 * reg_unit(devinfo));

      /* XXX: src_stencil is only available on gfx9+. dst_depth is never
       * available on gfx9+. As such it's impossible to have both enabled at the
       * same time and therefore length cannot overrun the array.
       */
      assert(length < 15 * reg_unit(devinfo));

      sources[length] = bld.vgrf(BRW_REGISTER_TYPE_UD);
      bld.exec_all().annotate("FB write OS")
         .MOV(retype(sources[length], BRW_REGISTER_TYPE_UB),
              subscript(src_stencil, BRW_REGISTER_TYPE_UB, 0));
      length++;
   }

   /* Send from the GRF */
   fs_reg payload = fs_reg(VGRF, -1, BRW_REGISTER_TYPE_F);
   fs_inst *load = bld.LOAD_PAYLOAD(payload, sources, length, payload_header_size);
   payload.nr = bld.shader->alloc.allocate(regs_written(load));
   load->dst = payload;

   uint32_t msg_ctl = brw_fb_write_msg_control(inst, prog_data);

   /* XXX - Bit 13 Per-sample PS enable */
   inst->desc =
      (inst->group / 16) << 11 | /* rt slot group */
      brw_fb_write_desc(devinfo, inst->target, msg_ctl, inst->last_rt,
                        0 /* coarse_rt_write */);

   fs_reg desc = brw_imm_ud(0);
   if (prog_data->coarse_pixel_dispatch == BRW_ALWAYS) {
      inst->desc |= (1 << 18);
   } else if (prog_data->coarse_pixel_dispatch == BRW_SOMETIMES) {
      STATIC_ASSERT(INTEL_MSAA_FLAG_COARSE_RT_WRITES == (1 << 18));
      const fs_builder &ubld = bld.exec_all().group(8, 0);
      desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.AND(desc, dynamic_msaa_flags(prog_data),
               brw_imm_ud(INTEL_MSAA_FLAG_COARSE_RT_WRITES));
      desc = component(desc, 0);
   }

   uint32_t ex_desc = 0;
   if (devinfo->ver >= 20) {
      ex_desc = inst->target << 21 |
                (key->nr_color_regions == 0) << 20 |
                (src0_alpha.file != BAD_FILE) << 15 |
                (src_stencil.file != BAD_FILE) << 14 |
                (src_depth.file != BAD_FILE) << 13 |
                (sample_mask.file != BAD_FILE) << 12;
   } else if (devinfo->ver >= 11) {
      /* Set the "Render Target Index" and "Src0 Alpha Present" fields
       * in the extended message descriptor, in lieu of using a header.
       */
      ex_desc = inst->target << 12 | (src0_alpha.file != BAD_FILE) << 15;

      if (key->nr_color_regions == 0)
         ex_desc |= 1 << 20; /* Null Render Target */
   }
   inst->ex_desc = ex_desc;

   inst->opcode = SHADER_OPCODE_SEND;
   inst->resize_sources(3);
   inst->sfid = GFX6_SFID_DATAPORT_RENDER_CACHE;
   inst->src[0] = desc;
   inst->src[1] = brw_imm_ud(0);
   inst->src[2] = payload;
   inst->mlen = regs_written(load);
   inst->ex_mlen = 0;
   inst->header_size = header_size;
   inst->check_tdr = true;
   inst->send_has_side_effects = true;
}

static void
lower_fb_read_logical_send(const fs_builder &bld, fs_inst *inst,
                           const struct brw_wm_prog_data *wm_prog_data)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   const fs_builder &ubld = bld.exec_all().group(8, 0);
   const unsigned length = 2;
   const fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD, length);

   assert(devinfo->ver >= 9 && devinfo->ver < 20);

   if (bld.group() < 16) {
      ubld.group(16, 0).MOV(header, retype(brw_vec8_grf(0, 0),
                                           BRW_REGISTER_TYPE_UD));
   } else {
      assert(bld.group() < 32);
      const fs_reg header_sources[] = {
         retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD),
         retype(brw_vec8_grf(2, 0), BRW_REGISTER_TYPE_UD)
      };
      ubld.LOAD_PAYLOAD(header, header_sources, ARRAY_SIZE(header_sources), 0);

      if (devinfo->ver >= 12) {
         /* On Gfx12 the Viewport and Render Target Array Index fields (AKA
          * Poly 0 Info) are provided in r1.1 instead of r0.0, and the render
          * target message header format was updated accordingly -- However
          * the updated format only works for the lower 16 channels in a
          * SIMD32 thread, since the higher 16 channels want the subspan data
          * from r2 instead of r1, so we need to copy over the contents of
          * r1.1 in order to fix things up.
          */
         ubld.group(1, 0).MOV(component(header, 9),
                              retype(brw_vec1_grf(1, 1), BRW_REGISTER_TYPE_UD));
      }
   }

   /* BSpec 12470 (Gfx8-11), BSpec 47842 (Gfx12+) :
    *
    *   "Must be zero for Render Target Read message."
    *
    * For bits :
    *   - 14 : Stencil Present to Render Target
    *   - 13 : Source Depth Present to Render Target
    *   - 12 : oMask to Render Target
    *   - 11 : Source0 Alpha Present to Render Target
    */
   ubld.group(1, 0).AND(component(header, 0),
                        component(header, 0),
                        brw_imm_ud(~INTEL_MASK(14, 11)));

   inst->resize_sources(4);
   inst->opcode = SHADER_OPCODE_SEND;
   inst->src[0] = brw_imm_ud(0);
   inst->src[1] = brw_imm_ud(0);
   inst->src[2] = header;
   inst->src[3] = fs_reg();
   inst->mlen = length;
   inst->header_size = length;
   inst->sfid = GFX6_SFID_DATAPORT_RENDER_CACHE;
   inst->check_tdr = true;
   inst->desc =
      (inst->group / 16) << 11 | /* rt slot group */
      brw_fb_read_desc(devinfo, inst->target,
                       0 /* msg_control */, inst->exec_size,
                       wm_prog_data->persample_dispatch);
}

static bool
is_high_sampler(const struct intel_device_info *devinfo, const fs_reg &sampler)
{
   return sampler.file != IMM || sampler.ud >= 16;
}

static unsigned
sampler_msg_type(const intel_device_info *devinfo,
                 opcode opcode, bool shadow_compare,
                 bool lod_is_zero, bool has_min_lod)
{
   switch (opcode) {
   case SHADER_OPCODE_TEX_LOGICAL:
      if (devinfo->ver >= 20 && has_min_lod) {
         return shadow_compare ? XE2_SAMPLER_MESSAGE_SAMPLE_COMPARE_MLOD :
                                 XE2_SAMPLER_MESSAGE_SAMPLE_MLOD;
      } else {
         return shadow_compare ? GFX5_SAMPLER_MESSAGE_SAMPLE_COMPARE :
                                 GFX5_SAMPLER_MESSAGE_SAMPLE;
      }
   case FS_OPCODE_TXB_LOGICAL:
      return shadow_compare ? GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE :
                              GFX5_SAMPLER_MESSAGE_SAMPLE_BIAS;
   case SHADER_OPCODE_TXL_LOGICAL:
      assert(!has_min_lod);
      if (lod_is_zero) {
         return shadow_compare ? GFX9_SAMPLER_MESSAGE_SAMPLE_C_LZ :
                                 GFX9_SAMPLER_MESSAGE_SAMPLE_LZ;
      }
      return shadow_compare ? GFX5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE :
                              GFX5_SAMPLER_MESSAGE_SAMPLE_LOD;
   case SHADER_OPCODE_TXS_LOGICAL:
   case SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
      assert(!has_min_lod);
      return GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO;
   case SHADER_OPCODE_TXD_LOGICAL:
      return shadow_compare ? HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE :
                              GFX5_SAMPLER_MESSAGE_SAMPLE_DERIVS;
   case SHADER_OPCODE_TXF_LOGICAL:
      assert(!has_min_lod);
      return lod_is_zero ? GFX9_SAMPLER_MESSAGE_SAMPLE_LD_LZ :
                           GFX5_SAMPLER_MESSAGE_SAMPLE_LD;
   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      assert(!has_min_lod);
      return GFX9_SAMPLER_MESSAGE_SAMPLE_LD2DMS_W;
   case SHADER_OPCODE_TXF_MCS_LOGICAL:
      assert(!has_min_lod);
      return GFX7_SAMPLER_MESSAGE_SAMPLE_LD_MCS;
   case SHADER_OPCODE_LOD_LOGICAL:
      assert(!has_min_lod);
      return GFX5_SAMPLER_MESSAGE_LOD;
   case SHADER_OPCODE_TG4_LOGICAL:
      assert(!has_min_lod);
      return shadow_compare ? GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C :
                              GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4;
      break;
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
      assert(!has_min_lod);
      return shadow_compare ? GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C :
                              GFX7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO;
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
      assert(!has_min_lod);
      assert(devinfo->ver >= 20);
      return shadow_compare ? XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_L_C:
                              XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_L;
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
      assert(!has_min_lod);
      assert(devinfo->ver >= 20);
      return XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_B;
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
      assert(!has_min_lod);
      assert(devinfo->ver >= 20);
      return XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_B;
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
      assert(!has_min_lod);
      assert(devinfo->ver >= 20);
      return shadow_compare ? XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_L_C :
                              XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_L;
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
      assert(!has_min_lod);
      assert(devinfo->ver >= 20);
      return shadow_compare ? XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_I_C :
                              XE2_SAMPLER_MESSAGE_SAMPLE_GATHER4_I;
  case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
      assert(!has_min_lod);
      return GFX6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO;
   default:
      unreachable("not reached");
   }
}

/**
 * Emit a LOAD_PAYLOAD instruction while ensuring the sources are aligned to
 * the given requested_alignment_sz.
 */
static fs_inst *
emit_load_payload_with_padding(const fs_builder &bld, const fs_reg &dst,
                               const fs_reg *src, unsigned sources,
                               unsigned header_size,
                               unsigned requested_alignment_sz)
{
   unsigned length = 0;
   unsigned num_srcs =
      sources * DIV_ROUND_UP(requested_alignment_sz, bld.dispatch_width());
   fs_reg *src_comps = new fs_reg[num_srcs];

   for (unsigned i = 0; i < header_size; i++)
      src_comps[length++] = src[i];

   for (unsigned i = header_size; i < sources; i++) {
      unsigned src_sz =
         retype(dst, src[i].type).component_size(bld.dispatch_width());
      const enum brw_reg_type padding_payload_type =
         brw_reg_type_from_bit_size(type_sz(src[i].type) * 8,
                                    BRW_REGISTER_TYPE_UD);

      src_comps[length++] = src[i];

      /* Expand the real sources if component of requested payload type is
       * larger than real source component.
       */
      if (src_sz < requested_alignment_sz) {
         for (unsigned j = 0; j < (requested_alignment_sz / src_sz) - 1; j++) {
            src_comps[length++] = retype(fs_reg(), padding_payload_type);
         }
      }
   }

   fs_inst *inst = bld.LOAD_PAYLOAD(dst, src_comps, length, header_size);
   delete[] src_comps;

   return inst;
}

static bool
shader_opcode_needs_header(opcode op)
{
   switch (op) {
   case SHADER_OPCODE_TG4_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
      return true;
   default:
      break;
   }

   return false;
}

static void
lower_sampler_logical_send(const fs_builder &bld, fs_inst *inst,
                           const fs_reg &coordinate,
                           const fs_reg &shadow_c,
                           fs_reg lod, const fs_reg &lod2,
                           const fs_reg &min_lod,
                           const fs_reg &sample_index,
                           const fs_reg &mcs,
                           const fs_reg &surface,
                           const fs_reg &sampler,
                           const fs_reg &surface_handle,
                           const fs_reg &sampler_handle,
                           const fs_reg &tg4_offset,
                           unsigned payload_type_bit_size,
                           unsigned coord_components,
                           unsigned grad_components,
                           bool residency)
{
   const brw_compiler *compiler = bld.shader->compiler;
   const intel_device_info *devinfo = bld.shader->devinfo;
   const enum brw_reg_type payload_type =
      brw_reg_type_from_bit_size(payload_type_bit_size, BRW_REGISTER_TYPE_F);
   const enum brw_reg_type payload_unsigned_type =
      brw_reg_type_from_bit_size(payload_type_bit_size, BRW_REGISTER_TYPE_UD);
   const enum brw_reg_type payload_signed_type =
      brw_reg_type_from_bit_size(payload_type_bit_size, BRW_REGISTER_TYPE_D);
   unsigned reg_width = bld.dispatch_width() / 8;
   unsigned header_size = 0, length = 0;
   opcode op = inst->opcode;
   fs_reg sources[1 + MAX_SAMPLER_MESSAGE_SIZE];
   for (unsigned i = 0; i < ARRAY_SIZE(sources); i++)
      sources[i] = bld.vgrf(payload_type);

   /* We must have exactly one of surface/sampler and surface/sampler_handle */
   assert((surface.file == BAD_FILE) != (surface_handle.file == BAD_FILE));
   assert((sampler.file == BAD_FILE) != (sampler_handle.file == BAD_FILE));

   if (shader_opcode_needs_header(op) || inst->offset != 0 || inst->eot ||
       sampler_handle.file != BAD_FILE ||
       is_high_sampler(devinfo, sampler) ||
       residency) {
      /* For general texture offsets (no txf workaround), we need a header to
       * put them in.
       *
       * TG4 needs to place its channel select in the header, for interaction
       * with ARB_texture_swizzle.  The sampler index is only 4-bits, so for
       * larger sampler numbers we need to offset the Sampler State Pointer in
       * the header.
       */
      fs_reg header = retype(sources[0], BRW_REGISTER_TYPE_UD);
      for (header_size = 0; header_size < reg_unit(devinfo); header_size++)
         sources[length++] = byte_offset(header, REG_SIZE * header_size);

      /* If we're requesting fewer than four channels worth of response,
       * and we have an explicit header, we need to set up the sampler
       * writemask.  It's reversed from normal: 1 means "don't write".
       */
      unsigned reg_count = regs_written(inst) - reg_unit(devinfo) * residency;
      if (!inst->eot && reg_count < 4 * reg_width) {
         assert(reg_count % reg_width == 0);
         unsigned mask = ~((1 << (reg_count / reg_width)) - 1) & 0xf;
         inst->offset |= mask << 12;
      }

      if (residency)
         inst->offset |= 1 << 23; /* g0.2 bit23 : Pixel Null Mask Enable */

      /* Build the actual header */
      const fs_builder ubld = bld.exec_all().group(8 * reg_unit(devinfo), 0);
      const fs_builder ubld1 = ubld.group(1, 0);
      if (devinfo->ver >= 11)
         ubld.MOV(header, brw_imm_ud(0));
      else
         ubld.MOV(header, retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD));
      if (inst->offset) {
         ubld1.MOV(component(header, 2), brw_imm_ud(inst->offset));
      } else if (devinfo->ver < 11 &&
                 bld.shader->stage != MESA_SHADER_VERTEX &&
                 bld.shader->stage != MESA_SHADER_FRAGMENT) {
         /* The vertex and fragment stages have g0.2 set to 0, so
          * header0.2 is 0 when g0 is copied. Other stages may not, so we
          * must set it to 0 to avoid setting undesirable bits in the
          * message.
          */
         ubld1.MOV(component(header, 2), brw_imm_ud(0));
      }

      if (sampler_handle.file != BAD_FILE) {
         /* Bindless sampler handles aren't relative to the sampler state
          * pointer passed into the shader through SAMPLER_STATE_POINTERS_*.
          * Instead, it's an absolute pointer relative to dynamic state base
          * address.
          *
          * Sampler states are 16 bytes each and the pointer we give here has
          * to be 32-byte aligned.  In order to avoid more indirect messages
          * than required, we assume that all bindless sampler states are
          * 32-byte aligned.  This sacrifices a bit of general state base
          * address space but means we can do something more efficient in the
          * shader.
          */
         if (compiler->use_bindless_sampler_offset) {
            assert(devinfo->ver >= 11);
            ubld1.OR(component(header, 3), sampler_handle, brw_imm_ud(1));
         } else {
            ubld1.MOV(component(header, 3), sampler_handle);
         }
      } else if (is_high_sampler(devinfo, sampler)) {
         fs_reg sampler_state_ptr =
            retype(brw_vec1_grf(0, 3), BRW_REGISTER_TYPE_UD);

         /* Gfx11+ sampler message headers include bits in 4:0 which conflict
          * with the ones included in g0.3 bits 4:0.  Mask them out.
          */
         if (devinfo->ver >= 11) {
            sampler_state_ptr = ubld1.vgrf(BRW_REGISTER_TYPE_UD);
            ubld1.AND(sampler_state_ptr,
                      retype(brw_vec1_grf(0, 3), BRW_REGISTER_TYPE_UD),
                      brw_imm_ud(INTEL_MASK(31, 5)));
         }

         if (sampler.file == BRW_IMMEDIATE_VALUE) {
            assert(sampler.ud >= 16);
            const int sampler_state_size = 16; /* 16 bytes */

            ubld1.ADD(component(header, 3), sampler_state_ptr,
                      brw_imm_ud(16 * (sampler.ud / 16) * sampler_state_size));
         } else {
            fs_reg tmp = ubld1.vgrf(BRW_REGISTER_TYPE_UD);
            ubld1.AND(tmp, sampler, brw_imm_ud(0x0f0));
            ubld1.SHL(tmp, tmp, brw_imm_ud(4));
            ubld1.ADD(component(header, 3), sampler_state_ptr, tmp);
         }
      } else if (devinfo->ver >= 11) {
         /* Gfx11+ sampler message headers include bits in 4:0 which conflict
          * with the ones included in g0.3 bits 4:0.  Mask them out.
          */
         ubld1.AND(component(header, 3),
                   retype(brw_vec1_grf(0, 3), BRW_REGISTER_TYPE_UD),
                   brw_imm_ud(INTEL_MASK(31, 5)));
      }
   }

   const bool lod_is_zero = lod.is_zero();

   /* On Xe2 and newer platforms, min_lod is the first parameter specifically
    * so that a bunch of other, possibly unused, parameters don't need to also
    * be included.
    */
   const unsigned msg_type =
      sampler_msg_type(devinfo, op, inst->shadow_compare, lod_is_zero,
                       min_lod.file != BAD_FILE);

   const bool min_lod_is_first = devinfo->ver >= 20 &&
      (msg_type == XE2_SAMPLER_MESSAGE_SAMPLE_MLOD ||
       msg_type == XE2_SAMPLER_MESSAGE_SAMPLE_COMPARE_MLOD);

   if (min_lod_is_first) {
      assert(min_lod.file != BAD_FILE);
      bld.MOV(sources[length++], min_lod);
   }

   if (shadow_c.file != BAD_FILE) {
      bld.MOV(sources[length], shadow_c);
      length++;
   }

   bool coordinate_done = false;

   /* Set up the LOD info */
   switch (op) {
   case SHADER_OPCODE_TXL_LOGICAL:
      if (lod_is_zero)
         break;
      FALLTHROUGH;
   case FS_OPCODE_TXB_LOGICAL:
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
      bld.MOV(sources[length], lod);
      length++;
      break;
   case SHADER_OPCODE_TXD_LOGICAL:
      /* TXD should have been lowered in SIMD16 mode (in SIMD32 mode in
       * Xe2+).
       */
      assert(bld.dispatch_width() == (8 * reg_unit(devinfo)));

      /* Load dPdx and the coordinate together:
       * [hdr], [ref], x, dPdx.x, dPdy.x, y, dPdx.y, dPdy.y, z, dPdx.z, dPdy.z
       */
      for (unsigned i = 0; i < coord_components; i++) {
         bld.MOV(sources[length++], offset(coordinate, bld, i));

         /* For cube map array, the coordinate is (u,v,r,ai) but there are
          * only derivatives for (u, v, r).
          */
         if (i < grad_components) {
            bld.MOV(sources[length++], offset(lod, bld, i));
            bld.MOV(sources[length++], offset(lod2, bld, i));
         }
      }

      coordinate_done = true;
      break;
   case SHADER_OPCODE_TXS_LOGICAL:
      sources[length] = retype(sources[length], payload_unsigned_type);
      bld.MOV(sources[length++], lod);
      break;
   case SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
      /* We need an LOD; just use 0 */
      sources[length] = retype(sources[length], payload_unsigned_type);
      bld.MOV(sources[length++], brw_imm_ud(0));
      break;
   case SHADER_OPCODE_TXF_LOGICAL:
       /* On Gfx9 the parameters are intermixed they are u, v, lod, r. */
      sources[length] = retype(sources[length], payload_signed_type);
      bld.MOV(sources[length++], coordinate);

      if (coord_components >= 2) {
         sources[length] = retype(sources[length], payload_signed_type);
         bld.MOV(sources[length], offset(coordinate, bld, 1));
      } else {
         sources[length] = brw_imm_d(0);
      }
      length++;

      if (!lod_is_zero) {
         sources[length] = retype(sources[length], payload_signed_type);
         bld.MOV(sources[length++], lod);
      }

      for (unsigned i = 2; i < coord_components; i++) {
         sources[length] = retype(sources[length], payload_signed_type);
         bld.MOV(sources[length++], offset(coordinate, bld, i));
      }

      coordinate_done = true;
      break;

   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      sources[length] = retype(sources[length], payload_unsigned_type);
      bld.MOV(sources[length++], sample_index);

      /* Data from the multisample control surface. */
      for (unsigned i = 0; i < 2; ++i) {
         /* Sampler always writes 4/8 register worth of data but for ld_mcs
          * only valid data is in first two register. So with 16-bit
          * payload, we need to split 2-32bit register into 4-16-bit
          * payload.
          *
          * From the Gfx12HP BSpec: Render Engine - 3D and GPGPU Programs -
          * Shared Functions - 3D Sampler - Messages - Message Format:
          *
          *    ld2dms_w   si  mcs0 mcs1 mcs2  mcs3  u  v  r
          */
         if (op == SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL) {
            fs_reg tmp = offset(mcs, bld, i);
            sources[length] = retype(sources[length], payload_unsigned_type);
            bld.MOV(sources[length++],
                    mcs.file == IMM ? mcs :
                    subscript(tmp, payload_unsigned_type, 0));

            sources[length] = retype(sources[length], payload_unsigned_type);
            bld.MOV(sources[length++],
                    mcs.file == IMM ? mcs :
                    subscript(tmp, payload_unsigned_type, 1));
         } else {
            sources[length] = retype(sources[length], payload_unsigned_type);
            bld.MOV(sources[length++],
                    mcs.file == IMM ? mcs : offset(mcs, bld, i));
         }
      }
      FALLTHROUGH;

   case SHADER_OPCODE_TXF_MCS_LOGICAL:
      /* There is no offsetting for this message; just copy in the integer
       * texture coordinates.
       */
      for (unsigned i = 0; i < coord_components; i++) {
         sources[length] = retype(sources[length], payload_signed_type);
         bld.MOV(sources[length++], offset(coordinate, bld, i));
      }

      coordinate_done = true;
      break;
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
      /* More crazy intermixing */
      for (unsigned i = 0; i < 2; i++) /* u, v */
         bld.MOV(sources[length++], offset(coordinate, bld, i));

      for (unsigned i = 0; i < 2; i++) { /* offu, offv */
         sources[length] = retype(sources[length], payload_signed_type);
         bld.MOV(sources[length++], offset(tg4_offset, bld, i));
      }

      if (coord_components == 3) /* r if present */
         bld.MOV(sources[length++], offset(coordinate, bld, 2));

      coordinate_done = true;
      break;
   default:
      break;
   }

   /* Set up the coordinate (except for cases where it was done above) */
   if (!coordinate_done) {
      for (unsigned i = 0; i < coord_components; i++)
         bld.MOV(retype(sources[length++], payload_type),
                 offset(coordinate, bld, i));
   }

   if (min_lod.file != BAD_FILE && !min_lod_is_first) {
      /* Account for all of the missing coordinate sources */
      if (op == FS_OPCODE_TXB_LOGICAL && devinfo->ver >= 20 &&
          inst->has_packed_lod_ai_src) {
         /* Bspec 64985:
          *
          * For sample_b sampler message format:
          *
          * SIMD16H/SIMD32H
          * Param Number   0     1  2  3  4  5
          * Param          BIAS  U  V  R  Ai MLOD
          *
          * SIMD16/SIMD32
          * Param Number   0        1  2  3  4
          * Param          BIAS_AI  U  V  R  MLOD
          */
         length += 3 - coord_components;
      } else if (op == SHADER_OPCODE_TXD_LOGICAL && devinfo->verx10 >= 125) {
         /* On DG2 and newer platforms, sample_d can only be used with 1D and
          * 2D surfaces, so the maximum number of gradient components is 2.
          * In spite of this limitation, the Bspec lists a mysterious R
          * component before the min_lod, so the maximum coordinate components
          * is 3.
          *
          * See bspec 45942, "Enable new message layout for cube array"
          */
         length += 3 - coord_components;
         length += (2 - grad_components) * 2;
      } else {
         length += 4 - coord_components;
         if (op == SHADER_OPCODE_TXD_LOGICAL)
            length += (3 - grad_components) * 2;
      }

      bld.MOV(sources[length++], min_lod);

      /* Wa_14014595444: Populate MLOD as parameter 5 (twice). */
       if (devinfo->verx10 == 125 && op == FS_OPCODE_TXB_LOGICAL &&
          !inst->shadow_compare)
         bld.MOV(sources[length++], min_lod);
   }

   const fs_reg src_payload =
      fs_reg(VGRF, bld.shader->alloc.allocate(length * reg_width),
                                              BRW_REGISTER_TYPE_F);
   /* In case of 16-bit payload each component takes one full register in
    * both SIMD8H and SIMD16H modes. In both cases one reg can hold 16
    * elements. In SIMD8H case hardware simply expects the components to be
    * padded (i.e., aligned on reg boundary).
    */
   fs_inst *load_payload_inst =
      emit_load_payload_with_padding(bld, src_payload, sources, length,
                                     header_size, REG_SIZE * reg_unit(devinfo));
   unsigned mlen = load_payload_inst->size_written / REG_SIZE;
   unsigned simd_mode = 0;
   if (devinfo->ver < 20) {
      if (payload_type_bit_size == 16) {
         assert(devinfo->ver >= 11);
         simd_mode = inst->exec_size <= 8 ? GFX10_SAMPLER_SIMD_MODE_SIMD8H :
            GFX10_SAMPLER_SIMD_MODE_SIMD16H;
      } else {
         simd_mode = inst->exec_size <= 8 ? BRW_SAMPLER_SIMD_MODE_SIMD8 :
            BRW_SAMPLER_SIMD_MODE_SIMD16;
      }
   } else {
      if (payload_type_bit_size == 16) {
         simd_mode = inst->exec_size <= 16 ? XE2_SAMPLER_SIMD_MODE_SIMD16H :
            XE2_SAMPLER_SIMD_MODE_SIMD32H;
      } else {
         simd_mode = inst->exec_size <= 16 ? XE2_SAMPLER_SIMD_MODE_SIMD16 :
            XE2_SAMPLER_SIMD_MODE_SIMD32;
      }
   }

   /* Generate the SEND. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = mlen;
   inst->header_size = header_size;
   inst->sfid = BRW_SFID_SAMPLER;
   if (surface.file == IMM &&
       (sampler.file == IMM || sampler_handle.file != BAD_FILE)) {
      inst->desc = brw_sampler_desc(devinfo, surface.ud,
                                    sampler.file == IMM ? sampler.ud % 16 : 0,
                                    msg_type,
                                    simd_mode,
                                    0 /* return_format unused on gfx7+ */);
      inst->src[0] = brw_imm_ud(0);
      inst->src[1] = brw_imm_ud(0);
   } else if (surface_handle.file != BAD_FILE) {
      /* Bindless surface */
      inst->desc = brw_sampler_desc(devinfo,
                                    GFX9_BTI_BINDLESS,
                                    sampler.file == IMM ? sampler.ud % 16 : 0,
                                    msg_type,
                                    simd_mode,
                                    0 /* return_format unused on gfx7+ */);

      /* For bindless samplers, the entire address is included in the message
       * header so we can leave the portion in the message descriptor 0.
       */
      if (sampler_handle.file != BAD_FILE || sampler.file == IMM) {
         inst->src[0] = brw_imm_ud(0);
      } else {
         const fs_builder ubld = bld.group(1, 0).exec_all();
         fs_reg desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         ubld.SHL(desc, sampler, brw_imm_ud(8));
         inst->src[0] = component(desc, 0);
      }

      /* We assume that the driver provided the handle in the top 20 bits so
       * we can use the surface handle directly as the extended descriptor.
       */
      inst->src[1] = retype(surface_handle, BRW_REGISTER_TYPE_UD);
      inst->send_ex_bso = compiler->extended_bindless_surface_offset;
   } else {
      /* Immediate portion of the descriptor */
      inst->desc = brw_sampler_desc(devinfo,
                                    0, /* surface */
                                    0, /* sampler */
                                    msg_type,
                                    simd_mode,
                                    0 /* return_format unused on gfx7+ */);
      const fs_builder ubld = bld.group(1, 0).exec_all();
      fs_reg desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      if (surface.equals(sampler)) {
         /* This case is common in GL */
         ubld.MUL(desc, surface, brw_imm_ud(0x101));
      } else {
         if (sampler_handle.file != BAD_FILE) {
            ubld.MOV(desc, surface);
         } else if (sampler.file == IMM) {
            ubld.OR(desc, surface, brw_imm_ud(sampler.ud << 8));
         } else {
            ubld.SHL(desc, sampler, brw_imm_ud(8));
            ubld.OR(desc, desc, surface);
         }
      }
      ubld.AND(desc, desc, brw_imm_ud(0xfff));

      inst->src[0] = component(desc, 0);
      inst->src[1] = brw_imm_ud(0); /* ex_desc */
   }

   inst->ex_desc = 0;

   inst->src[2] = src_payload;
   inst->resize_sources(3);

   if (inst->eot) {
      /* EOT sampler messages don't make sense to split because it would
       * involve ending half of the thread early.
       */
      assert(inst->group == 0);
      /* We need to use SENDC for EOT sampler messages */
      inst->check_tdr = true;
      inst->send_has_side_effects = true;
   }

   /* Message length > MAX_SAMPLER_MESSAGE_SIZE disallowed by hardware. */
   assert(inst->mlen <= MAX_SAMPLER_MESSAGE_SIZE * reg_unit(devinfo));
}

static unsigned
get_sampler_msg_payload_type_bit_size(const intel_device_info *devinfo,
                                      const fs_inst *inst)
{
   assert(inst);
   const fs_reg *src = inst->src;
   unsigned src_type_size = 0;

   /* All sources need to have the same size, therefore seek the first valid
    * and take the size from there.
    */
   for (unsigned i = 0; i < TEX_LOGICAL_NUM_SRCS; i++) {
      if (src[i].file != BAD_FILE) {
         src_type_size = brw_reg_type_to_size(src[i].type);
         break;
      }
   }

   assert(src_type_size == 2 || src_type_size == 4);

#ifndef NDEBUG
   /* Make sure all sources agree. On gfx12 this doesn't hold when sampling
    * compressed multisampled surfaces. There the payload contains MCS data
    * which is already in 16-bits unlike the other parameters that need forced
    * conversion.
    */
   if (inst->opcode != SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL) {
      for (unsigned i = 0; i < TEX_LOGICAL_NUM_SRCS; i++) {
         assert(src[i].file == BAD_FILE ||
                brw_reg_type_to_size(src[i].type) == src_type_size);
      }
   }
#endif

   if (devinfo->verx10 < 125)
      return src_type_size * 8;

   /* Force conversion from 32-bit sources to 16-bit payload. From the XeHP Bspec:
    * 3D and GPGPU Programs - Shared Functions - 3D Sampler - Messages - Message
    * Format [GFX12:HAS:1209977870] *
    *
    *  ld2dms_w       SIMD8H and SIMD16H Only
    *  ld_mcs         SIMD8H and SIMD16H Only
    *  ld2dms         REMOVEDBY(GEN:HAS:1406788836)
    */
   if (inst->opcode == SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL ||
       inst->opcode == SHADER_OPCODE_TXF_MCS_LOGICAL ||
       (inst->opcode == FS_OPCODE_TXB_LOGICAL && !inst->has_packed_lod_ai_src &&
        devinfo->ver >= 20))
      src_type_size = 2;

   return src_type_size * 8;
}

static void
lower_sampler_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   const fs_reg coordinate = inst->src[TEX_LOGICAL_SRC_COORDINATE];
   const fs_reg shadow_c = inst->src[TEX_LOGICAL_SRC_SHADOW_C];
   const fs_reg lod = inst->src[TEX_LOGICAL_SRC_LOD];
   const fs_reg lod2 = inst->src[TEX_LOGICAL_SRC_LOD2];
   const fs_reg min_lod = inst->src[TEX_LOGICAL_SRC_MIN_LOD];
   const fs_reg sample_index = inst->src[TEX_LOGICAL_SRC_SAMPLE_INDEX];
   const fs_reg mcs = inst->src[TEX_LOGICAL_SRC_MCS];
   const fs_reg surface = inst->src[TEX_LOGICAL_SRC_SURFACE];
   const fs_reg sampler = inst->src[TEX_LOGICAL_SRC_SAMPLER];
   const fs_reg surface_handle = inst->src[TEX_LOGICAL_SRC_SURFACE_HANDLE];
   const fs_reg sampler_handle = inst->src[TEX_LOGICAL_SRC_SAMPLER_HANDLE];
   const fs_reg tg4_offset = inst->src[TEX_LOGICAL_SRC_TG4_OFFSET];
   assert(inst->src[TEX_LOGICAL_SRC_COORD_COMPONENTS].file == IMM);
   const unsigned coord_components = inst->src[TEX_LOGICAL_SRC_COORD_COMPONENTS].ud;
   assert(inst->src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].file == IMM);
   const unsigned grad_components = inst->src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].ud;
   assert(inst->src[TEX_LOGICAL_SRC_RESIDENCY].file == IMM);
   const bool residency = inst->src[TEX_LOGICAL_SRC_RESIDENCY].ud != 0;

   const unsigned msg_payload_type_bit_size =
      get_sampler_msg_payload_type_bit_size(devinfo, inst);

   /* 16-bit payloads are available only on gfx11+ */
   assert(msg_payload_type_bit_size != 16 || devinfo->ver >= 11);

   lower_sampler_logical_send(bld, inst, coordinate,
                              shadow_c, lod, lod2, min_lod,
                              sample_index,
                              mcs, surface, sampler,
                              surface_handle, sampler_handle,
                              tg4_offset,
                              msg_payload_type_bit_size,
                              coord_components, grad_components,
                              residency);
}

/**
 * Predicate the specified instruction on the vector mask.
 */
static void
emit_predicate_on_vector_mask(const fs_builder &bld, fs_inst *inst)
{
   assert(bld.shader->stage == MESA_SHADER_FRAGMENT &&
          bld.group() == inst->group &&
          bld.dispatch_width() == inst->exec_size);

   const fs_builder ubld = bld.exec_all().group(1, 0);

   const fs_visitor &s = *bld.shader;
   const fs_reg vector_mask = ubld.vgrf(BRW_REGISTER_TYPE_UW);
   ubld.UNDEF(vector_mask);
   ubld.emit(SHADER_OPCODE_READ_ARCH_REG, vector_mask, retype(brw_sr0_reg(3),
                                                              BRW_REGISTER_TYPE_UD));
   const unsigned subreg = sample_mask_flag_subreg(s);

   ubld.MOV(brw_flag_subreg(subreg + inst->group / 16), vector_mask);

   if (inst->predicate) {
      assert(inst->predicate == BRW_PREDICATE_NORMAL);
      assert(!inst->predicate_inverse);
      assert(inst->flag_subreg == 0);
      assert(s.devinfo->ver < 20);
      /* Combine the vector mask with the existing predicate by using a
       * vertical predication mode.
       */
      inst->predicate = BRW_PREDICATE_ALIGN1_ALLV;
   } else {
      inst->flag_subreg = subreg;
      inst->predicate = BRW_PREDICATE_NORMAL;
      inst->predicate_inverse = false;
   }
}

static void
setup_surface_descriptors(const fs_builder &bld, fs_inst *inst, uint32_t desc,
                          const fs_reg &surface, const fs_reg &surface_handle)
{
   const brw_compiler *compiler = bld.shader->compiler;

   /* We must have exactly one of surface and surface_handle */
   assert((surface.file == BAD_FILE) != (surface_handle.file == BAD_FILE));

   if (surface.file == IMM) {
      inst->desc = desc | (surface.ud & 0xff);
      inst->src[0] = brw_imm_ud(0);
      inst->src[1] = brw_imm_ud(0); /* ex_desc */
   } else if (surface_handle.file != BAD_FILE) {
      /* Bindless surface */
      inst->desc = desc | GFX9_BTI_BINDLESS;
      inst->src[0] = brw_imm_ud(0);

      /* We assume that the driver provided the handle in the top 20 bits so
       * we can use the surface handle directly as the extended descriptor.
       */
      inst->src[1] = retype(surface_handle, BRW_REGISTER_TYPE_UD);
      inst->send_ex_bso = compiler->extended_bindless_surface_offset;
   } else {
      inst->desc = desc;
      const fs_builder ubld = bld.exec_all().group(1, 0);
      fs_reg tmp = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.AND(tmp, surface, brw_imm_ud(0xff));
      inst->src[0] = component(tmp, 0);
      inst->src[1] = brw_imm_ud(0); /* ex_desc */
   }
}

static void
setup_lsc_surface_descriptors(const fs_builder &bld, fs_inst *inst,
                              uint32_t desc, const fs_reg &surface)
{
   const ASSERTED intel_device_info *devinfo = bld.shader->devinfo;
   const brw_compiler *compiler = bld.shader->compiler;

   inst->src[0] = brw_imm_ud(0); /* desc */

   enum lsc_addr_surface_type surf_type = lsc_msg_desc_addr_type(devinfo, desc);
   switch (surf_type) {
   case LSC_ADDR_SURFTYPE_BSS:
      inst->send_ex_bso = compiler->extended_bindless_surface_offset;
      /* fall-through */
   case LSC_ADDR_SURFTYPE_SS:
      assert(surface.file != BAD_FILE);
      /* We assume that the driver provided the handle in the top 20 bits so
       * we can use the surface handle directly as the extended descriptor.
       */
      inst->src[1] = retype(surface, BRW_REGISTER_TYPE_UD);
      break;

   case LSC_ADDR_SURFTYPE_BTI:
      assert(surface.file != BAD_FILE);
      if (surface.file == IMM) {
         inst->src[1] = brw_imm_ud(lsc_bti_ex_desc(devinfo, surface.ud));
      } else {
         const fs_builder ubld = bld.exec_all().group(1, 0);
         fs_reg tmp = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         ubld.SHL(tmp, surface, brw_imm_ud(24));
         inst->src[1] = component(tmp, 0);
      }
      break;

   case LSC_ADDR_SURFTYPE_FLAT:
      inst->src[1] = brw_imm_ud(0);
      break;

   default:
      unreachable("Invalid LSC surface address type");
   }
}

static void
lower_surface_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const brw_compiler *compiler = bld.shader->compiler;
   const intel_device_info *devinfo = bld.shader->devinfo;

   /* Get the logical send arguments. */
   const fs_reg addr = inst->src[SURFACE_LOGICAL_SRC_ADDRESS];
   const fs_reg src = inst->src[SURFACE_LOGICAL_SRC_DATA];
   const fs_reg surface = inst->src[SURFACE_LOGICAL_SRC_SURFACE];
   const fs_reg surface_handle = inst->src[SURFACE_LOGICAL_SRC_SURFACE_HANDLE];
   const UNUSED fs_reg dims = inst->src[SURFACE_LOGICAL_SRC_IMM_DIMS];
   const fs_reg arg = inst->src[SURFACE_LOGICAL_SRC_IMM_ARG];
   const fs_reg allow_sample_mask =
      inst->src[SURFACE_LOGICAL_SRC_ALLOW_SAMPLE_MASK];
   assert(arg.file == IMM);
   assert(allow_sample_mask.file == IMM);

   /* Calculate the total number of components of the payload. */
   const unsigned addr_sz = inst->components_read(SURFACE_LOGICAL_SRC_ADDRESS);
   const unsigned src_sz = inst->components_read(SURFACE_LOGICAL_SRC_DATA);

   const bool is_typed_access =
      inst->opcode == SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL ||
      inst->opcode == SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL ||
      inst->opcode == SHADER_OPCODE_TYPED_ATOMIC_LOGICAL;

   const bool is_surface_access = is_typed_access ||
      inst->opcode == SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL ||
      inst->opcode == SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL ||
      inst->opcode == SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL;

   const bool is_stateless =
      surface.file == IMM && (surface.ud == BRW_BTI_STATELESS ||
                              surface.ud == GFX8_BTI_STATELESS_NON_COHERENT);

   const bool has_side_effects = inst->has_side_effects();

   fs_reg sample_mask = allow_sample_mask.ud ? brw_sample_mask_reg(bld) :
                                               fs_reg(brw_imm_ud(0xffffffff));

   fs_reg header;
   if (is_stateless) {
      assert(!is_surface_access);
      fs_builder ubld = bld.exec_all().group(8, 0);
      header = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.emit(SHADER_OPCODE_SCRATCH_HEADER, header);
   }
   const unsigned header_sz = header.file != BAD_FILE ? 1 : 0;

   fs_reg payload, payload2;
   unsigned mlen, ex_mlen = 0;
   if (src.file == BAD_FILE || header.file == BAD_FILE) {
      /* We have split sends on gfx9 and above */
      if (header.file == BAD_FILE) {
         payload = bld.move_to_vgrf(addr, addr_sz);
         payload2 = bld.move_to_vgrf(src, src_sz);
         mlen = addr_sz * (inst->exec_size / 8);
         ex_mlen = src_sz * (inst->exec_size / 8);
      } else {
         assert(src.file == BAD_FILE);
         payload = header;
         payload2 = bld.move_to_vgrf(addr, addr_sz);
         mlen = header_sz;
         ex_mlen = addr_sz * (inst->exec_size / 8);
      }
   } else {
      /* Allocate space for the payload. */
      const unsigned sz = header_sz + addr_sz + src_sz;
      payload = bld.vgrf(BRW_REGISTER_TYPE_UD, sz);
      fs_reg *const components = new fs_reg[sz];
      unsigned n = 0;

      /* Construct the payload. */
      if (header.file != BAD_FILE)
         components[n++] = header;

      for (unsigned i = 0; i < addr_sz; i++)
         components[n++] = offset(addr, bld, i);

      for (unsigned i = 0; i < src_sz; i++)
         components[n++] = offset(src, bld, i);

      bld.LOAD_PAYLOAD(payload, components, sz, header_sz);
      mlen = header_sz + (addr_sz + src_sz) * inst->exec_size / 8;

      delete[] components;
   }

   /* Predicate the instruction on the sample mask if no header is
    * provided.
    */
   if ((header.file == BAD_FILE || !is_surface_access) &&
       sample_mask.file != BAD_FILE && sample_mask.file != IMM)
      brw_emit_predicate_on_sample_mask(bld, inst);

   uint32_t sfid;
   switch (inst->opcode) {
   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
      /* Byte scattered opcodes go through the normal data cache */
      sfid = GFX7_SFID_DATAPORT_DATA_CACHE;
      break;

   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      sfid = GFX7_SFID_DATAPORT_DATA_CACHE;
      break;

   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
      /* Untyped Surface messages go through the data cache but the SFID value
       * changed on Haswell.
       */
      sfid = HSW_SFID_DATAPORT_DATA_CACHE_1;
      break;

   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
      /* Typed surface messages go through the render cache on IVB and the
       * data cache on HSW+.
       */
      sfid = HSW_SFID_DATAPORT_DATA_CACHE_1;
      break;

   default:
      unreachable("Unsupported surface opcode");
   }

   uint32_t desc;
   switch (inst->opcode) {
   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
      desc = brw_dp_untyped_surface_rw_desc(devinfo, inst->exec_size,
                                            arg.ud, /* num_channels */
                                            false   /* write */);
      break;

   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
      desc = brw_dp_untyped_surface_rw_desc(devinfo, inst->exec_size,
                                            arg.ud, /* num_channels */
                                            true    /* write */);
      break;

   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
      desc = brw_dp_byte_scattered_rw_desc(devinfo, inst->exec_size,
                                           arg.ud, /* bit_size */
                                           false   /* write */);
      break;

   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
      desc = brw_dp_byte_scattered_rw_desc(devinfo, inst->exec_size,
                                           arg.ud, /* bit_size */
                                           true    /* write */);
      break;

   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      assert(arg.ud == 32); /* bit_size */
      desc = brw_dp_dword_scattered_rw_desc(devinfo, inst->exec_size,
                                            false  /* write */);
      break;

   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      assert(arg.ud == 32); /* bit_size */
      desc = brw_dp_dword_scattered_rw_desc(devinfo, inst->exec_size,
                                            true   /* write */);
      break;

   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
      if (lsc_opcode_is_atomic_float((enum lsc_opcode) arg.ud)) {
         desc = brw_dp_untyped_atomic_float_desc(devinfo, inst->exec_size,
                                                 lsc_op_to_legacy_atomic(arg.ud),
                                                 !inst->dst.is_null());
      } else {
         desc = brw_dp_untyped_atomic_desc(devinfo, inst->exec_size,
                                           lsc_op_to_legacy_atomic(arg.ud),
                                           !inst->dst.is_null());
      }
      break;

   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
      desc = brw_dp_typed_surface_rw_desc(devinfo, inst->exec_size, inst->group,
                                          arg.ud, /* num_channels */
                                          false   /* write */);
      break;

   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      desc = brw_dp_typed_surface_rw_desc(devinfo, inst->exec_size, inst->group,
                                          arg.ud, /* num_channels */
                                          true    /* write */);
      break;

   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
      desc = brw_dp_typed_atomic_desc(devinfo, inst->exec_size, inst->group,
                                      lsc_op_to_legacy_atomic(arg.ud),
                                      !inst->dst.is_null());
      break;

   default:
      unreachable("Unknown surface logical instruction");
   }

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = mlen;
   inst->ex_mlen = ex_mlen;
   inst->header_size = header_sz;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;
   inst->send_ex_bso = surface_handle.file != BAD_FILE &&
                       compiler->extended_bindless_surface_offset;

   /* Set up SFID and descriptors */
   inst->sfid = sfid;
   setup_surface_descriptors(bld, inst, desc, surface, surface_handle);

   inst->resize_sources(4);

   /* Finally, the payload */
   inst->src[2] = payload;
   inst->src[3] = payload2;
}

static enum lsc_data_size
lsc_bits_to_data_size(unsigned bit_size)
{
   switch (bit_size / 8) {
   case 1:  return LSC_DATA_SIZE_D8U32;
   case 2:  return LSC_DATA_SIZE_D16U32;
   case 4:  return LSC_DATA_SIZE_D32;
   case 8:  return LSC_DATA_SIZE_D64;
   default:
      unreachable("Unsupported data size.");
   }
}

static void
lower_lsc_surface_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const brw_compiler *compiler = bld.shader->compiler;
   const intel_device_info *devinfo = bld.shader->devinfo;
   assert(devinfo->has_lsc);

   /* Get the logical send arguments. */
   const fs_reg addr = inst->src[SURFACE_LOGICAL_SRC_ADDRESS];
   const fs_reg src = inst->src[SURFACE_LOGICAL_SRC_DATA];
   const fs_reg surface = inst->src[SURFACE_LOGICAL_SRC_SURFACE];
   const fs_reg surface_handle = inst->src[SURFACE_LOGICAL_SRC_SURFACE_HANDLE];
   const fs_reg dims = inst->src[SURFACE_LOGICAL_SRC_IMM_DIMS];
   const fs_reg arg = inst->src[SURFACE_LOGICAL_SRC_IMM_ARG];
   const fs_reg allow_sample_mask =
      inst->src[SURFACE_LOGICAL_SRC_ALLOW_SAMPLE_MASK];
   assert(arg.file == IMM);
   assert(allow_sample_mask.file == IMM);
   assert(dims.file == IMM);

   /* Calculate the total number of components of the payload. */
   const unsigned addr_sz = inst->components_read(SURFACE_LOGICAL_SRC_ADDRESS);
   const unsigned src_comps = inst->components_read(SURFACE_LOGICAL_SRC_DATA);
   const unsigned src_sz = type_sz(src.type);
   const unsigned dst_sz = type_sz(inst->dst.type);

   const bool has_side_effects = inst->has_side_effects();

   const bool is_typed_access =
      inst->opcode == SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL ||
      inst->opcode == SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL ||
      inst->opcode == SHADER_OPCODE_TYPED_ATOMIC_LOGICAL;

   unsigned num_components = 0;
   bool has_dest = false;

   unsigned ex_mlen = 0;
   fs_reg payload, payload2;
   payload = bld.move_to_vgrf(addr, addr_sz);
   if (src.file != BAD_FILE) {
      payload2 = bld.move_to_vgrf(src, src_comps);
      ex_mlen = (src_comps * src_sz * inst->exec_size) / REG_SIZE;
   }

   /* Predicate the instruction on the sample mask if needed */
   fs_reg sample_mask = allow_sample_mask.ud ? brw_sample_mask_reg(bld) :
                                               fs_reg(brw_imm_ud(0xffffffff));
   if (sample_mask.file != BAD_FILE && sample_mask.file != IMM)
      brw_emit_predicate_on_sample_mask(bld, inst);

   if (surface.file == IMM && surface.ud == GFX7_BTI_SLM)
      inst->sfid = GFX12_SFID_SLM;
   else if (is_typed_access)
      inst->sfid = GFX12_SFID_TGM;
   else
      inst->sfid = GFX12_SFID_UGM;

   /* Dimensions should always be 1 for UGM/UGML/SLM or
    * between 1 and 4 for TGM
    */
   assert(dims.ud == 1 ||
          (inst->sfid == GFX12_SFID_TGM &&
           dims.ud >= 1 && dims.ud <= 4));

   /* We should have exactly one of surface and surface_handle. For scratch
    * messages generated by brw_fs_nir.cpp we also allow a special value to
    * know what heap base we should use in STATE_BASE_ADDRESS (SS = Surface
    * State Offset, or BSS = Bindless Surface State Offset).
    */
   bool non_bindless = surface.file == IMM && surface.ud == GFX125_NON_BINDLESS;
   assert((surface.file == BAD_FILE) != (surface_handle.file == BAD_FILE) ||
          (non_bindless && surface_handle.file != BAD_FILE));

   enum lsc_addr_surface_type surf_type;
   if (surface_handle.file != BAD_FILE) {
      if (surface.file == BAD_FILE) {
         assert(!non_bindless);
         surf_type = LSC_ADDR_SURFTYPE_BSS;
      } else {
         assert(surface.file == IMM &&
                (surface.ud == 0 || surface.ud == GFX125_NON_BINDLESS));
         surf_type = non_bindless ? LSC_ADDR_SURFTYPE_SS : LSC_ADDR_SURFTYPE_BSS;
      }
   } else if (surface.file == IMM && surface.ud == GFX7_BTI_SLM)
      surf_type = LSC_ADDR_SURFTYPE_FLAT;
   else
      surf_type = LSC_ADDR_SURFTYPE_BTI;

   switch (inst->opcode) {
   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
      num_components = arg.ud;
      has_dest = true;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD_CMASK,
                                surf_type, LSC_ADDR_SIZE_A32,
                                LSC_DATA_SIZE_D32, num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
      num_components = arg.ud;
      has_dest = false;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_STORE_CMASK,
                                surf_type, LSC_ADDR_SIZE_A32,
                                LSC_DATA_SIZE_D32, num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL: {
      /* Bspec: Atomic instruction -> Cache section:
       *
       *    Atomic messages are always forced to "un-cacheable" in the L1
       *    cache.
       */
      enum lsc_opcode opcode = (enum lsc_opcode) arg.ud;

      num_components = 1;
      has_dest = !inst->dst.is_null();
      inst->desc = lsc_msg_desc(devinfo, opcode,
                                surf_type, LSC_ADDR_SIZE_A32,
                                lsc_bits_to_data_size(dst_sz * 8),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1UC_L3WB));
      break;
   }
   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      num_components = 1;
      has_dest = true;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD,
                                surf_type, LSC_ADDR_SIZE_A32,
                                lsc_bits_to_data_size(arg.ud),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
      num_components = 1;
      has_dest = false;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_STORE,
                                surf_type, LSC_ADDR_SIZE_A32,
                                lsc_bits_to_data_size(arg.ud),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1STATE_L3MOCS));
      break;
   default:
      unreachable("Unknown surface logical instruction");
   }

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, inst->exec_size * dims.ud);
   inst->ex_mlen = ex_mlen;
   inst->header_size = 0;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;
   inst->send_ex_bso = surf_type == LSC_ADDR_SURFTYPE_BSS &&
                       compiler->extended_bindless_surface_offset;
   inst->size_written = !has_dest ? 0 :
      lsc_msg_dest_len(devinfo, lsc_msg_desc_data_size(devinfo, inst->desc),
                       inst->exec_size * num_components) * REG_SIZE;

   inst->resize_sources(4);

   if (non_bindless) {
      inst->src[0] = brw_imm_ud(0);     /* desc */
      inst->src[1] = surface_handle;    /* ex_desc */
   } else {
      setup_lsc_surface_descriptors(bld, inst, inst->desc,
                                    surface.file != BAD_FILE ?
                                    surface : surface_handle);
   }

   /* Finally, the payload */
   inst->src[2] = payload;
   inst->src[3] = payload2;
}

static void
lower_lsc_block_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const brw_compiler *compiler = bld.shader->compiler;
   const intel_device_info *devinfo = bld.shader->devinfo;
   assert(devinfo->has_lsc);

   /* Get the logical send arguments. */
   const fs_reg addr = inst->src[SURFACE_LOGICAL_SRC_ADDRESS];
   const fs_reg src = inst->src[SURFACE_LOGICAL_SRC_DATA];
   const fs_reg surface = inst->src[SURFACE_LOGICAL_SRC_SURFACE];
   const fs_reg surface_handle = inst->src[SURFACE_LOGICAL_SRC_SURFACE_HANDLE];
   const fs_reg arg = inst->src[SURFACE_LOGICAL_SRC_IMM_ARG];
   assert(arg.file == IMM);
   assert(inst->src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == BAD_FILE);
   assert(inst->src[SURFACE_LOGICAL_SRC_ALLOW_SAMPLE_MASK].file == BAD_FILE);

   const bool is_stateless =
      surface.file == IMM && (surface.ud == BRW_BTI_STATELESS ||
                              surface.ud == GFX8_BTI_STATELESS_NON_COHERENT);

   const bool has_side_effects = inst->has_side_effects();

   const bool write = inst->opcode == SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL;

   fs_builder ubld = bld.exec_all().group(1, 0);
   fs_reg stateless_ex_desc;
   if (is_stateless) {
      stateless_ex_desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.AND(stateless_ex_desc,
               retype(brw_vec1_grf(0, 5), BRW_REGISTER_TYPE_UD),
               brw_imm_ud(INTEL_MASK(31, 10)));
   }

   fs_reg data;
   if (write) {
      const unsigned src_sz = inst->components_read(SURFACE_LOGICAL_SRC_DATA);
      data = retype(bld.move_to_vgrf(src, src_sz), BRW_REGISTER_TYPE_UD);
   }

   inst->opcode = SHADER_OPCODE_SEND;
   if (surface.file == IMM && surface.ud == GFX7_BTI_SLM)
      inst->sfid = GFX12_SFID_SLM;
   else
      inst->sfid = GFX12_SFID_UGM;
   const enum lsc_addr_surface_type surf_type =
      inst->sfid == GFX12_SFID_SLM ?
      LSC_ADDR_SURFTYPE_FLAT :
      surface.file == BAD_FILE ?
      LSC_ADDR_SURFTYPE_BSS : LSC_ADDR_SURFTYPE_BTI;
   inst->desc = lsc_msg_desc(devinfo,
                             write ? LSC_OP_STORE : LSC_OP_LOAD,
                             surf_type,
                             LSC_ADDR_SIZE_A32,
                             LSC_DATA_SIZE_D32,
                             arg.ud /* num_channels */,
                             true /* transpose */,
                             LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));

   inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, 1);
   inst->size_written = write ? 0 :
      lsc_msg_dest_len(devinfo, LSC_DATA_SIZE_D32, arg.ud) * REG_SIZE;
   inst->exec_size = 1;
   inst->ex_mlen = write ? DIV_ROUND_UP(arg.ud, 8) : 0;
   inst->header_size = 0;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;
   inst->send_ex_bso = surf_type == LSC_ADDR_SURFTYPE_BSS &&
                       compiler->extended_bindless_surface_offset;

   inst->resize_sources(4);

   if (stateless_ex_desc.file != BAD_FILE) {
      inst->src[0] = brw_imm_ud(0);     /* desc */
      inst->src[1] = stateless_ex_desc; /* ex_desc */
   } else {
      setup_lsc_surface_descriptors(bld, inst, inst->desc,
                                    surface.file != BAD_FILE ?
                                    surface : surface_handle);
   }
   inst->src[2] = addr;          /* payload */
   inst->src[3] = data;          /* payload2 */
}

static void
lower_surface_block_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;

   /* Get the logical send arguments. */
   const fs_reg addr = inst->src[SURFACE_LOGICAL_SRC_ADDRESS];
   const fs_reg src = inst->src[SURFACE_LOGICAL_SRC_DATA];
   const fs_reg surface = inst->src[SURFACE_LOGICAL_SRC_SURFACE];
   const fs_reg surface_handle = inst->src[SURFACE_LOGICAL_SRC_SURFACE_HANDLE];
   const fs_reg arg = inst->src[SURFACE_LOGICAL_SRC_IMM_ARG];
   assert(arg.file == IMM);
   assert(inst->src[SURFACE_LOGICAL_SRC_IMM_DIMS].file == BAD_FILE);
   assert(inst->src[SURFACE_LOGICAL_SRC_ALLOW_SAMPLE_MASK].file == BAD_FILE);

   const bool is_stateless =
      surface.file == IMM && (surface.ud == BRW_BTI_STATELESS ||
                              surface.ud == GFX8_BTI_STATELESS_NON_COHERENT);

   const bool has_side_effects = inst->has_side_effects();

   const bool align_16B =
      inst->opcode != SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL;

   const bool write = inst->opcode == SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL;

   /* The address is stored in the header.  See MH_A32_GO and MH_BTS_GO. */
   fs_builder ubld = bld.exec_all().group(8, 0);
   fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD);

   if (is_stateless)
      ubld.emit(SHADER_OPCODE_SCRATCH_HEADER, header);
   else
      ubld.MOV(header, brw_imm_d(0));

   /* Address in OWord units when aligned to OWords. */
   if (align_16B)
      ubld.group(1, 0).SHR(component(header, 2), addr, brw_imm_ud(4));
   else
      ubld.group(1, 0).MOV(component(header, 2), addr);

   fs_reg data;
   unsigned ex_mlen = 0;
   if (write) {
      const unsigned src_sz = inst->components_read(SURFACE_LOGICAL_SRC_DATA);
      data = retype(bld.move_to_vgrf(src, src_sz), BRW_REGISTER_TYPE_UD);
      ex_mlen = src_sz * type_sz(src.type) * inst->exec_size / REG_SIZE;
   }

   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = 1;
   inst->ex_mlen = ex_mlen;
   inst->header_size = 1;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;

   inst->sfid = GFX7_SFID_DATAPORT_DATA_CACHE;

   const uint32_t desc = brw_dp_oword_block_rw_desc(devinfo, align_16B,
                                                    arg.ud, write);
   setup_surface_descriptors(bld, inst, desc, surface, surface_handle);

   inst->resize_sources(4);

   inst->src[2] = header;
   inst->src[3] = data;
}

static fs_reg
emit_a64_oword_block_header(const fs_builder &bld, const fs_reg &addr)
{
   const fs_builder ubld = bld.exec_all().group(8, 0);

   assert(type_sz(addr.type) == 8 && addr.stride == 0);

   fs_reg expanded_addr = addr;
   if (addr.file == UNIFORM) {
      /* We can't do stride 1 with the UNIFORM file, it requires stride 0 */
      expanded_addr = ubld.vgrf(BRW_REGISTER_TYPE_UQ);
      expanded_addr.stride = 0;
      ubld.MOV(expanded_addr, retype(addr, BRW_REGISTER_TYPE_UQ));
   }

   fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD);
   ubld.MOV(header, brw_imm_ud(0));

   /* Use a 2-wide MOV to fill out the address */
   fs_reg addr_vec2 = expanded_addr;
   addr_vec2.type = BRW_REGISTER_TYPE_UD;
   addr_vec2.stride = 1;
   ubld.group(2, 0).MOV(header, addr_vec2);

   return header;
}

static void
emit_fragment_mask(const fs_builder &bld, fs_inst *inst)
{
   assert(inst->src[A64_LOGICAL_ENABLE_HELPERS].file == IMM);
   const bool enable_helpers = inst->src[A64_LOGICAL_ENABLE_HELPERS].ud;

   /* If we're a fragment shader, we have to predicate with the sample mask to
    * avoid helper invocations to avoid helper invocations in instructions
    * with side effects, unless they are explicitly required.
    *
    * There are also special cases when we actually want to run on helpers
    * (ray queries).
    */
   assert(bld.shader->stage == MESA_SHADER_FRAGMENT);
   if (enable_helpers)
      emit_predicate_on_vector_mask(bld, inst);
   else if (inst->has_side_effects())
      brw_emit_predicate_on_sample_mask(bld, inst);
}

static void
lower_lsc_a64_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;

   /* Get the logical send arguments. */
   const fs_reg addr = inst->src[A64_LOGICAL_ADDRESS];
   const fs_reg src = inst->src[A64_LOGICAL_SRC];
   const unsigned src_sz = type_sz(src.type);
   const unsigned dst_sz = type_sz(inst->dst.type);

   const unsigned src_comps = inst->components_read(1);
   assert(inst->src[A64_LOGICAL_ARG].file == IMM);
   const unsigned arg = inst->src[A64_LOGICAL_ARG].ud;
   const bool has_side_effects = inst->has_side_effects();

   fs_reg payload = retype(bld.move_to_vgrf(addr, 1), BRW_REGISTER_TYPE_UD);
   fs_reg payload2 = retype(bld.move_to_vgrf(src, src_comps),
                            BRW_REGISTER_TYPE_UD);
   unsigned ex_mlen = src_comps * src_sz * inst->exec_size / REG_SIZE;
   unsigned num_components = 0;
   bool has_dest = false;

   switch (inst->opcode) {
   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
      num_components = arg;
      has_dest = true;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD_CMASK,
                                LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A64,
                                LSC_DATA_SIZE_D32, num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      num_components = arg;
      has_dest = false;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_STORE_CMASK,
                                LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A64,
                                LSC_DATA_SIZE_D32, num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      num_components = 1;
      has_dest = true;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD,
                                LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A64,
                                lsc_bits_to_data_size(arg),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
      num_components = 1;
      has_dest = false;
      inst->desc = lsc_msg_desc(devinfo, LSC_OP_STORE,
                                LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A64,
                                lsc_bits_to_data_size(arg),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL: {
      /* Bspec: Atomic instruction -> Cache section:
       *
       *    Atomic messages are always forced to "un-cacheable" in the L1
       *    cache.
       */
      enum lsc_opcode opcode = (enum lsc_opcode) arg;
      num_components = 1;
      has_dest = !inst->dst.is_null();
      inst->desc = lsc_msg_desc(devinfo, opcode,
                                LSC_ADDR_SURFTYPE_FLAT, LSC_ADDR_SIZE_A64,
                                lsc_bits_to_data_size(dst_sz * 8),
                                num_components,
                                false /* transpose */,
                                LSC_CACHE(devinfo, STORE, L1UC_L3WB));
      break;
   }
   case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
   case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      num_components = arg;
      has_dest = true;
      inst->exec_size = 1;
      inst->desc = lsc_msg_desc(devinfo,
                                LSC_OP_LOAD,
                                LSC_ADDR_SURFTYPE_FLAT,
                                LSC_ADDR_SIZE_A64,
                                LSC_DATA_SIZE_D32,
                                num_components,
                                true /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      break;
   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      num_components = arg;
      has_dest = false;
      inst->exec_size = 1;
      inst->desc = lsc_msg_desc(devinfo,
                                LSC_OP_STORE,
                                LSC_ADDR_SURFTYPE_FLAT,
                                LSC_ADDR_SIZE_A64,
                                LSC_DATA_SIZE_D32,
                                num_components,
                                true /* transpose */,
                                LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));

      break;
   default:
      unreachable("Unknown A64 logical instruction");
   }

   if (bld.shader->stage == MESA_SHADER_FRAGMENT)
      emit_fragment_mask(bld, inst);

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A64, inst->exec_size);
   inst->ex_mlen = ex_mlen;
   inst->header_size = 0;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;

   inst->size_written = !has_dest ? 0 :
      lsc_msg_dest_len(devinfo, lsc_msg_desc_data_size(devinfo, inst->desc),
                       inst->exec_size * num_components) * REG_SIZE;

   /* Set up SFID and descriptors */
   inst->sfid = GFX12_SFID_UGM;
   inst->resize_sources(4);
   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = payload;
   inst->src[3] = payload2;
}

static void
lower_a64_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;

   const fs_reg addr = inst->src[A64_LOGICAL_ADDRESS];
   const fs_reg src = inst->src[A64_LOGICAL_SRC];
   const unsigned src_comps = inst->components_read(1);
   assert(inst->src[A64_LOGICAL_ARG].file == IMM);
   const unsigned arg = inst->src[A64_LOGICAL_ARG].ud;
   const bool has_side_effects = inst->has_side_effects();

   fs_reg payload, payload2;
   unsigned mlen, ex_mlen = 0, header_size = 0;
   if (inst->opcode == SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL ||
       inst->opcode == SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL ||
       inst->opcode == SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL) {

      /* OWORD messages only take a scalar address in a header */
      mlen = 1;
      header_size = 1;
      payload = emit_a64_oword_block_header(bld, addr);

      if (inst->opcode == SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL) {
         ex_mlen = src_comps * type_sz(src.type) * inst->exec_size / REG_SIZE;
         payload2 = retype(bld.move_to_vgrf(src, src_comps),
                           BRW_REGISTER_TYPE_UD);
      }
   } else {
      /* On Skylake and above, we have SENDS */
      mlen = 2 * (inst->exec_size / 8);
      ex_mlen = src_comps * type_sz(src.type) * inst->exec_size / REG_SIZE;
      payload = retype(bld.move_to_vgrf(addr, 1), BRW_REGISTER_TYPE_UD);
      payload2 = retype(bld.move_to_vgrf(src, src_comps),
                        BRW_REGISTER_TYPE_UD);
   }

   uint32_t desc;
   switch (inst->opcode) {
   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
      desc = brw_dp_a64_untyped_surface_rw_desc(devinfo, inst->exec_size,
                                                arg,   /* num_channels */
                                                false  /* write */);
      break;

   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      desc = brw_dp_a64_untyped_surface_rw_desc(devinfo, inst->exec_size,
                                                arg,   /* num_channels */
                                                true   /* write */);
      break;

   case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
      desc = brw_dp_a64_oword_block_rw_desc(devinfo,
                                            true,    /* align_16B */
                                            arg,     /* num_dwords */
                                            false    /* write */);
      break;

   case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      desc = brw_dp_a64_oword_block_rw_desc(devinfo,
                                            false,   /* align_16B */
                                            arg,     /* num_dwords */
                                            false    /* write */);
      break;

   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      desc = brw_dp_a64_oword_block_rw_desc(devinfo,
                                            true,    /* align_16B */
                                            arg,     /* num_dwords */
                                            true     /* write */);
      break;

   case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      desc = brw_dp_a64_byte_scattered_rw_desc(devinfo, inst->exec_size,
                                               arg,   /* bit_size */
                                               false  /* write */);
      break;

   case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
      desc = brw_dp_a64_byte_scattered_rw_desc(devinfo, inst->exec_size,
                                               arg,   /* bit_size */
                                               true   /* write */);
      break;

   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      if (lsc_opcode_is_atomic_float((enum lsc_opcode) arg)) {
         desc =
            brw_dp_a64_untyped_atomic_float_desc(devinfo, inst->exec_size,
                                                 type_sz(inst->dst.type) * 8,
                                                 lsc_op_to_legacy_atomic(arg),
                                                 !inst->dst.is_null());
      } else {
         desc = brw_dp_a64_untyped_atomic_desc(devinfo, inst->exec_size,
                                               type_sz(inst->dst.type) * 8,
                                               lsc_op_to_legacy_atomic(arg),
                                               !inst->dst.is_null());
      }
      break;

   default:
      unreachable("Unknown A64 logical instruction");
   }

   if (bld.shader->stage == MESA_SHADER_FRAGMENT)
      emit_fragment_mask(bld, inst);

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = mlen;
   inst->ex_mlen = ex_mlen;
   inst->header_size = header_size;
   inst->send_has_side_effects = has_side_effects;
   inst->send_is_volatile = !has_side_effects;

   /* Set up SFID and descriptors */
   inst->sfid = HSW_SFID_DATAPORT_DATA_CACHE_1;
   inst->desc = desc;
   inst->resize_sources(4);
   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = payload;
   inst->src[3] = payload2;
}

static void
lower_lsc_varying_pull_constant_logical_send(const fs_builder &bld,
                                             fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   ASSERTED const brw_compiler *compiler = bld.shader->compiler;

   fs_reg surface        = inst->src[PULL_VARYING_CONSTANT_SRC_SURFACE];
   fs_reg surface_handle = inst->src[PULL_VARYING_CONSTANT_SRC_SURFACE_HANDLE];
   fs_reg offset_B       = inst->src[PULL_VARYING_CONSTANT_SRC_OFFSET];
   fs_reg alignment_B    = inst->src[PULL_VARYING_CONSTANT_SRC_ALIGNMENT];

   /* We are switching the instruction from an ALU-like instruction to a
    * send-from-grf instruction.  Since sends can't handle strides or
    * source modifiers, we have to make a copy of the offset source.
    */
   fs_reg ubo_offset = bld.move_to_vgrf(offset_B, 1);

   enum lsc_addr_surface_type surf_type =
      surface_handle.file == BAD_FILE ?
      LSC_ADDR_SURFTYPE_BTI : LSC_ADDR_SURFTYPE_BSS;

   assert(alignment_B.file == BRW_IMMEDIATE_VALUE);
   unsigned alignment = alignment_B.ud;

   inst->opcode = SHADER_OPCODE_SEND;
   inst->sfid = GFX12_SFID_UGM;
   inst->resize_sources(3);
   inst->send_ex_bso = surf_type == LSC_ADDR_SURFTYPE_BSS &&
                       compiler->extended_bindless_surface_offset;

   assert(!compiler->indirect_ubos_use_sampler);

   inst->src[0] = brw_imm_ud(0);
   inst->src[2] = ubo_offset; /* payload */

   if (alignment >= 4) {
      inst->desc =
         lsc_msg_desc(devinfo, LSC_OP_LOAD_CMASK,
                      surf_type, LSC_ADDR_SIZE_A32,
                      LSC_DATA_SIZE_D32,
                      4 /* num_channels */,
                      false /* transpose */,
                      LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, inst->exec_size);

      setup_lsc_surface_descriptors(bld, inst, inst->desc,
                                    surface.file != BAD_FILE ?
                                    surface : surface_handle);
   } else {
      inst->desc =
         lsc_msg_desc(devinfo, LSC_OP_LOAD,
                      surf_type, LSC_ADDR_SIZE_A32,
                      LSC_DATA_SIZE_D32,
                      1 /* num_channels */,
                      false /* transpose */,
                      LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));
      inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, inst->exec_size);

      setup_lsc_surface_descriptors(bld, inst, inst->desc,
                                    surface.file != BAD_FILE ?
                                    surface : surface_handle);

      /* The byte scattered messages can only read one dword at a time so
       * we have to duplicate the message 4 times to read the full vec4.
       * Hopefully, dead code will clean up the mess if some of them aren't
       * needed.
       */
      assert(inst->size_written == 16 * inst->exec_size);
      inst->size_written /= 4;
      for (unsigned c = 1; c < 4; c++) {
         /* Emit a copy of the instruction because we're about to modify
          * it.  Because this loop starts at 1, we will emit copies for the
          * first 3 and the final one will be the modified instruction.
          */
         bld.emit(*inst);

         /* Offset the source */
         inst->src[2] = bld.vgrf(BRW_REGISTER_TYPE_UD);
         bld.ADD(inst->src[2], ubo_offset, brw_imm_ud(c * 4));

         /* Offset the destination */
         inst->dst = offset(inst->dst, bld, 1);
      }
   }
}

static void
lower_varying_pull_constant_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   const brw_compiler *compiler = bld.shader->compiler;

   fs_reg surface = inst->src[PULL_VARYING_CONSTANT_SRC_SURFACE];
   fs_reg surface_handle = inst->src[PULL_VARYING_CONSTANT_SRC_SURFACE_HANDLE];
   fs_reg offset_B = inst->src[PULL_VARYING_CONSTANT_SRC_OFFSET];

   /* We are switching the instruction from an ALU-like instruction to a
    * send-from-grf instruction.  Since sends can't handle strides or
    * source modifiers, we have to make a copy of the offset source.
    */
   fs_reg ubo_offset = bld.vgrf(BRW_REGISTER_TYPE_UD);
   bld.MOV(ubo_offset, offset_B);

   assert(inst->src[PULL_VARYING_CONSTANT_SRC_ALIGNMENT].file == BRW_IMMEDIATE_VALUE);
   unsigned alignment = inst->src[PULL_VARYING_CONSTANT_SRC_ALIGNMENT].ud;

   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = inst->exec_size / 8;
   inst->resize_sources(3);

   /* src[0] & src[1] are filled by setup_surface_descriptors() */
   inst->src[2] = ubo_offset; /* payload */

   if (compiler->indirect_ubos_use_sampler) {
      const unsigned simd_mode =
         inst->exec_size <= 8 ? BRW_SAMPLER_SIMD_MODE_SIMD8 :
                                BRW_SAMPLER_SIMD_MODE_SIMD16;
      const uint32_t desc = brw_sampler_desc(devinfo, 0, 0,
                                             GFX5_SAMPLER_MESSAGE_SAMPLE_LD,
                                             simd_mode, 0);

      inst->sfid = BRW_SFID_SAMPLER;
      setup_surface_descriptors(bld, inst, desc, surface, surface_handle);
   } else if (alignment >= 4) {
      const uint32_t desc =
         brw_dp_untyped_surface_rw_desc(devinfo, inst->exec_size,
                                        4, /* num_channels */
                                        false   /* write */);

      inst->sfid = HSW_SFID_DATAPORT_DATA_CACHE_1;
      setup_surface_descriptors(bld, inst, desc, surface, surface_handle);
   } else {
      const uint32_t desc =
         brw_dp_byte_scattered_rw_desc(devinfo, inst->exec_size,
                                       32,     /* bit_size */
                                       false   /* write */);

      inst->sfid = GFX7_SFID_DATAPORT_DATA_CACHE;
      setup_surface_descriptors(bld, inst, desc, surface, surface_handle);

      /* The byte scattered messages can only read one dword at a time so
       * we have to duplicate the message 4 times to read the full vec4.
       * Hopefully, dead code will clean up the mess if some of them aren't
       * needed.
       */
      assert(inst->size_written == 16 * inst->exec_size);
      inst->size_written /= 4;
      for (unsigned c = 1; c < 4; c++) {
         /* Emit a copy of the instruction because we're about to modify
          * it.  Because this loop starts at 1, we will emit copies for the
          * first 3 and the final one will be the modified instruction.
          */
         bld.emit(*inst);

         /* Offset the source */
         inst->src[2] = bld.vgrf(BRW_REGISTER_TYPE_UD);
         bld.ADD(inst->src[2], ubo_offset, brw_imm_ud(c * 4));

         /* Offset the destination */
         inst->dst = offset(inst->dst, bld, 1);
      }
   }
}

static void
lower_interpolator_logical_send(const fs_builder &bld, fs_inst *inst,
                                const struct brw_wm_prog_key *wm_prog_key,
                                const struct brw_wm_prog_data *wm_prog_data)
{
   const intel_device_info *devinfo = bld.shader->devinfo;

   /* We have to send something */
   fs_reg payload = brw_vec8_grf(0, 0);
   unsigned mlen = 1;

   unsigned mode;
   switch (inst->opcode) {
   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      assert(inst->src[INTERP_SRC_OFFSET].file == BAD_FILE);
      mode = GFX7_PIXEL_INTERPOLATOR_LOC_SAMPLE;
      break;

   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      assert(inst->src[INTERP_SRC_OFFSET].file == BAD_FILE);
      mode = GFX7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET;
      break;

   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
      payload = inst->src[INTERP_SRC_OFFSET];
      mlen = 2 * inst->exec_size / 8;
      mode = GFX7_PIXEL_INTERPOLATOR_LOC_PER_SLOT_OFFSET;
      break;

   default:
      unreachable("Invalid interpolator instruction");
   }

   const bool dynamic_mode =
      inst->src[INTERP_SRC_DYNAMIC_MODE].file != BAD_FILE;

   fs_reg desc = inst->src[INTERP_SRC_MSG_DESC];
   uint32_t desc_imm =
      brw_pixel_interp_desc(devinfo,
                            /* Leave the mode at 0 if persample_dispatch is
                             * dynamic, it will be ORed in below.
                             */
                            dynamic_mode ? 0 : mode,
                            inst->pi_noperspective,
                            false /* coarse_pixel_rate */,
                            inst->exec_size, inst->group);

   if (wm_prog_data->coarse_pixel_dispatch == BRW_ALWAYS) {
      desc_imm |= (1 << 15);
   } else if (wm_prog_data->coarse_pixel_dispatch == BRW_SOMETIMES) {
      STATIC_ASSERT(INTEL_MSAA_FLAG_COARSE_PI_MSG == (1 << 15));
      fs_reg orig_desc = desc;
      const fs_builder &ubld = bld.exec_all().group(8, 0);
      desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.AND(desc, dynamic_msaa_flags(wm_prog_data),
               brw_imm_ud(INTEL_MSAA_FLAG_COARSE_PI_MSG));

      /* And, if it's AT_OFFSET, we might have a non-trivial descriptor */
      if (orig_desc.file == IMM) {
         desc_imm |= orig_desc.ud;
      } else {
         ubld.OR(desc, desc, orig_desc);
      }
   }

   /* If persample_dispatch is dynamic, select the interpolation mode
    * dynamically and OR into the descriptor to complete the static part
    * generated by brw_pixel_interp_desc().
    *
    * Why does this work? If you look at the SKL PRMs, Volume 7:
    * 3D-Media-GPGPU, Shared Functions Pixel Interpolater, you'll see that
    *
    *   - "Per Message Offset” Message Descriptor
    *   - “Sample Position Offset” Message Descriptor
    *
    * have different formats. Fortunately, a fragment shader dispatched at
    * pixel rate, will have gl_SampleID = 0 & gl_NumSamples = 1. So the value
    * we pack in “Sample Position Offset” will be a 0 and will cover the X/Y
    * components of "Per Message Offset”, which will give us the pixel offset 0x0.
    */
   if (dynamic_mode) {
      fs_reg orig_desc = desc;
      const fs_builder &ubld = bld.exec_all().group(8, 0);
      desc = ubld.vgrf(BRW_REGISTER_TYPE_UD);

      /* The predicate should have been built in brw_fs_nir.cpp when emitting
       * NIR code. This guarantees that we do not have incorrect interactions
       * with the flag register holding the predication result.
       */
      if (orig_desc.file == IMM) {
         /* Not using SEL here because we would generate an instruction with 2
          * immediate sources which is not supported by HW.
          */
         set_predicate_inv(BRW_PREDICATE_NORMAL, false,
                           ubld.MOV(desc, brw_imm_ud(orig_desc.ud |
                                                     GFX7_PIXEL_INTERPOLATOR_LOC_SAMPLE << 12)));
         set_predicate_inv(BRW_PREDICATE_NORMAL, true,
                           ubld.MOV(desc, brw_imm_ud(orig_desc.ud |
                                                     GFX7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET << 12)));
      } else {
         set_predicate_inv(BRW_PREDICATE_NORMAL, false,
                           ubld.OR(desc, orig_desc,
                                   brw_imm_ud(GFX7_PIXEL_INTERPOLATOR_LOC_SAMPLE << 12)));
         set_predicate_inv(BRW_PREDICATE_NORMAL, true,
                           ubld.OR(desc, orig_desc,
                                   brw_imm_ud(GFX7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET << 12)));
      }
   }

   inst->opcode = SHADER_OPCODE_SEND;
   inst->sfid = GFX7_SFID_PIXEL_INTERPOLATOR;
   inst->desc = desc_imm;
   inst->ex_desc = 0;
   inst->mlen = mlen;
   inst->ex_mlen = 0;
   inst->send_has_side_effects = false;
   inst->send_is_volatile = false;

   inst->resize_sources(3);
   inst->src[0] = component(desc, 0);
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = payload;
}

static void
lower_btd_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   fs_reg global_addr = inst->src[0];
   const fs_reg btd_record = inst->src[1];

   const unsigned unit = reg_unit(devinfo);
   const unsigned mlen = 2 * unit;
   const fs_builder ubld = bld.exec_all();
   fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD, 2 * unit);

   ubld.MOV(header, brw_imm_ud(0));
   switch (inst->opcode) {
   case SHADER_OPCODE_BTD_SPAWN_LOGICAL:
      assert(type_sz(global_addr.type) == 8 && global_addr.stride == 0);
      global_addr.type = BRW_REGISTER_TYPE_UD;
      global_addr.stride = 1;
      ubld.group(2, 0).MOV(header, global_addr);
      break;

   case SHADER_OPCODE_BTD_RETIRE_LOGICAL:
      /* The bottom bit is the Stack ID release bit */
      ubld.group(1, 0).MOV(header, brw_imm_ud(1));
      break;

   default:
      unreachable("Invalid BTD message");
   }

   /* Stack IDs are always in R1 regardless of whether we're coming from a
    * bindless shader or a regular compute shader.
    */
   fs_reg stack_ids = retype(offset(header, bld, 1), BRW_REGISTER_TYPE_UW);
   bld.exec_all().MOV(stack_ids, retype(brw_vec8_grf(1 * unit, 0),
                                        BRW_REGISTER_TYPE_UW));

   unsigned ex_mlen = 0;
   fs_reg payload;
   if (inst->opcode == SHADER_OPCODE_BTD_SPAWN_LOGICAL) {
      ex_mlen = 2 * (inst->exec_size / 8);
      payload = bld.move_to_vgrf(btd_record, 1);
   } else {
      assert(inst->opcode == SHADER_OPCODE_BTD_RETIRE_LOGICAL);
      /* All these messages take a BTD and things complain if we don't provide
       * one for RETIRE.  However, it shouldn't ever actually get used so fill
       * it with zero.
       */
      ex_mlen = 2 * (inst->exec_size / 8);
      payload = bld.move_to_vgrf(brw_imm_uq(0), 1);
   }

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = mlen;
   inst->ex_mlen = ex_mlen;
   inst->header_size = 0; /* HW docs require has_header = false */
   inst->send_has_side_effects = true;
   inst->send_is_volatile = false;

   /* Set up SFID and descriptors */
   inst->sfid = GEN_RT_SFID_BINDLESS_THREAD_DISPATCH;
   inst->desc = brw_btd_spawn_desc(devinfo, inst->exec_size,
                                   GEN_RT_BTD_MESSAGE_SPAWN);
   inst->resize_sources(4);
   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = header;
   inst->src[3] = payload;
}

static void
lower_trace_ray_logical_send(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   /* The emit_uniformize() in brw_fs_nir.cpp will generate an horizontal
    * stride of 0. Below we're doing a MOV() in SIMD2. Since we can't use UQ/Q
    * types in on Gfx12.5, we need to tweak the stride with a value of 1 dword
    * so that the MOV operates on 2 components rather than twice the same
    * component.
    */
   fs_reg globals_addr = retype(inst->src[RT_LOGICAL_SRC_GLOBALS], BRW_REGISTER_TYPE_UD);
   globals_addr.stride = 1;
   const fs_reg bvh_level =
      inst->src[RT_LOGICAL_SRC_BVH_LEVEL].file == BRW_IMMEDIATE_VALUE ?
      inst->src[RT_LOGICAL_SRC_BVH_LEVEL] :
      bld.move_to_vgrf(inst->src[RT_LOGICAL_SRC_BVH_LEVEL],
                       inst->components_read(RT_LOGICAL_SRC_BVH_LEVEL));
   const fs_reg trace_ray_control =
      inst->src[RT_LOGICAL_SRC_TRACE_RAY_CONTROL].file == BRW_IMMEDIATE_VALUE ?
      inst->src[RT_LOGICAL_SRC_TRACE_RAY_CONTROL] :
      bld.move_to_vgrf(inst->src[RT_LOGICAL_SRC_TRACE_RAY_CONTROL],
                       inst->components_read(RT_LOGICAL_SRC_TRACE_RAY_CONTROL));
   const fs_reg synchronous_src = inst->src[RT_LOGICAL_SRC_SYNCHRONOUS];
   assert(synchronous_src.file == BRW_IMMEDIATE_VALUE);
   const bool synchronous = synchronous_src.ud;

   const unsigned unit = reg_unit(devinfo);
   const unsigned mlen = unit;
   const fs_builder ubld = bld.exec_all();
   fs_reg header = ubld.vgrf(BRW_REGISTER_TYPE_UD);
   ubld.MOV(header, brw_imm_ud(0));
   ubld.group(2, 0).MOV(header, globals_addr);
   if (synchronous)
      ubld.group(1, 0).MOV(byte_offset(header, 16), brw_imm_ud(synchronous));

   const unsigned ex_mlen = inst->exec_size / 8;
   fs_reg payload = bld.vgrf(BRW_REGISTER_TYPE_UD);
   if (bvh_level.file == BRW_IMMEDIATE_VALUE &&
       trace_ray_control.file == BRW_IMMEDIATE_VALUE) {
      bld.MOV(payload, brw_imm_ud(SET_BITS(trace_ray_control.ud, 9, 8) |
                                  (bvh_level.ud & 0x7)));
   } else {
      bld.SHL(payload, trace_ray_control, brw_imm_ud(8));
      bld.OR(payload, payload, bvh_level);
   }

   /* When doing synchronous traversal, the HW implicitly computes the
    * stack_id using the following formula :
    *
    *    EUID[3:0] & THREAD_ID[2:0] & SIMD_LANE_ID[3:0]
    *
    * Only in the asynchronous case we need to set the stack_id given from the
    * payload register.
    */
   if (!synchronous) {
      bld.AND(subscript(payload, BRW_REGISTER_TYPE_UW, 1),
              retype(brw_vec8_grf(1 * unit, 0), BRW_REGISTER_TYPE_UW),
              brw_imm_uw(0x7ff));
   }

   /* Update the original instruction. */
   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = mlen;
   inst->ex_mlen = ex_mlen;
   inst->header_size = 0; /* HW docs require has_header = false */
   inst->send_has_side_effects = true;
   inst->send_is_volatile = false;

   /* Set up SFID and descriptors */
   inst->sfid = GEN_RT_SFID_RAY_TRACE_ACCELERATOR;
   inst->desc = brw_rt_trace_ray_desc(devinfo, inst->exec_size);
   inst->resize_sources(4);
   inst->src[0] = brw_imm_ud(0); /* desc */
   inst->src[1] = brw_imm_ud(0); /* ex_desc */
   inst->src[2] = header;
   inst->src[3] = payload;
}

static void
lower_get_buffer_size(const fs_builder &bld, fs_inst *inst)
{
   const intel_device_info *devinfo = bld.shader->devinfo;
   /* Since we can only execute this instruction on uniform bti/surface
    * handles, brw_fs_nir.cpp should already have limited this to SIMD8.
    */
   assert(inst->exec_size == (devinfo->ver < 20 ? 8 : 16));

   fs_reg surface = inst->src[GET_BUFFER_SIZE_SRC_SURFACE];
   fs_reg surface_handle = inst->src[GET_BUFFER_SIZE_SRC_SURFACE_HANDLE];
   fs_reg lod = inst->src[GET_BUFFER_SIZE_SRC_LOD];

   inst->opcode = SHADER_OPCODE_SEND;
   inst->mlen = inst->exec_size / 8;
   inst->resize_sources(3);
   inst->ex_mlen = 0;
   inst->ex_desc = 0;

   /* src[0] & src[1] are filled by setup_surface_descriptors() */
   inst->src[2] = lod;

   const uint32_t return_format = GFX8_SAMPLER_RETURN_FORMAT_32BITS;

   const uint32_t desc = brw_sampler_desc(devinfo, 0, 0,
                                          GFX5_SAMPLER_MESSAGE_SAMPLE_RESINFO,
                                          BRW_SAMPLER_SIMD_MODE_SIMD8,
                                          return_format);

   inst->dst = retype(inst->dst, BRW_REGISTER_TYPE_UW);
   inst->sfid = BRW_SFID_SAMPLER;
   setup_surface_descriptors(bld, inst, desc, surface, surface_handle);
}

bool
brw_fs_lower_logical_sends(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      const fs_builder ibld(&s, block, inst);

      switch (inst->opcode) {
      case FS_OPCODE_FB_WRITE_LOGICAL:
         assert(s.stage == MESA_SHADER_FRAGMENT);
         lower_fb_write_logical_send(ibld, inst,
                                     brw_wm_prog_data(s.prog_data),
                                     (const brw_wm_prog_key *)s.key,
                                     s.fs_payload());
         break;

      case FS_OPCODE_FB_READ_LOGICAL:
         lower_fb_read_logical_send(ibld, inst, brw_wm_prog_data(s.prog_data));
         break;

      case SHADER_OPCODE_TEX_LOGICAL:
      case SHADER_OPCODE_TXD_LOGICAL:
      case SHADER_OPCODE_TXF_LOGICAL:
      case SHADER_OPCODE_TXL_LOGICAL:
      case SHADER_OPCODE_TXS_LOGICAL:
      case SHADER_OPCODE_IMAGE_SIZE_LOGICAL:
      case FS_OPCODE_TXB_LOGICAL:
      case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
      case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      case SHADER_OPCODE_TXF_MCS_LOGICAL:
      case SHADER_OPCODE_LOD_LOGICAL:
      case SHADER_OPCODE_TG4_LOGICAL:
      case SHADER_OPCODE_TG4_BIAS_LOGICAL:
      case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
      case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
      case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
      case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
      case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
      case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
         lower_sampler_logical_send(ibld, inst);
         break;

      case SHADER_OPCODE_GET_BUFFER_SIZE:
         lower_get_buffer_size(ibld, inst);
         break;

      case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
      case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
      case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
      case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
      case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
      case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
      case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
         if (devinfo->has_lsc)
            lower_lsc_surface_logical_send(ibld, inst);
         else
            lower_surface_logical_send(ibld, inst);
         break;

      case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
      case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
         devinfo->ver >= 20 && devinfo->has_lsc ?
            lower_lsc_surface_logical_send(ibld, inst) :
            lower_surface_logical_send(ibld, inst);
         break;

      case SHADER_OPCODE_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      case SHADER_OPCODE_OWORD_BLOCK_WRITE_LOGICAL:
         if (devinfo->has_lsc) {
            lower_lsc_block_logical_send(ibld, inst);
            break;
         }
         lower_surface_block_logical_send(ibld, inst);
         break;

      case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
      case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
      case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
      case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
      case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
      case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
         if (devinfo->has_lsc) {
            lower_lsc_a64_logical_send(ibld, inst);
            break;
         }
         lower_a64_logical_send(ibld, inst);
         break;

      case FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL:
         if (devinfo->has_lsc && !s.compiler->indirect_ubos_use_sampler)
            lower_lsc_varying_pull_constant_logical_send(ibld, inst);
         else
            lower_varying_pull_constant_logical_send(ibld, inst);
         break;

      case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
         lower_interpolator_logical_send(ibld, inst,
                                         (const brw_wm_prog_key *)s.key,
                                         brw_wm_prog_data(s.prog_data));
         break;

      case SHADER_OPCODE_BTD_SPAWN_LOGICAL:
      case SHADER_OPCODE_BTD_RETIRE_LOGICAL:
         lower_btd_logical_send(ibld, inst);
         break;

      case RT_OPCODE_TRACE_RAY_LOGICAL:
         lower_trace_ray_logical_send(ibld, inst);
         break;

      case SHADER_OPCODE_URB_READ_LOGICAL:
         if (devinfo->ver < 20)
            lower_urb_read_logical_send(ibld, inst);
         else
            lower_urb_read_logical_send_xe2(ibld, inst);
         break;

      case SHADER_OPCODE_URB_WRITE_LOGICAL:
         if (devinfo->ver < 20)
            lower_urb_write_logical_send(ibld, inst);
         else
            lower_urb_write_logical_send_xe2(ibld, inst);

         break;

      default:
         continue;
      }

      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Turns the generic expression-style uniform pull constant load instruction
 * into a hardware-specific series of instructions for loading a pull
 * constant.
 *
 * The expression style allows the CSE pass before this to optimize out
 * repeated loads from the same offset, and gives the pre-register-allocation
 * scheduling full flexibility, while the conversion to native instructions
 * allows the post-register-allocation scheduler the best information
 * possible.
 *
 * Note that execution masking for setting up pull constant loads is special:
 * the channels that need to be written are unrelated to the current execution
 * mask, since a later instruction will use one of the result channels as a
 * source operand for all 8 or 16 of its channels.
 */
bool
brw_fs_lower_uniform_pull_constant_loads(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;
   bool progress = false;

   foreach_block_and_inst (block, fs_inst, inst, s.cfg) {
      if (inst->opcode != FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD)
         continue;

      const fs_reg surface = inst->src[PULL_UNIFORM_CONSTANT_SRC_SURFACE];
      const fs_reg surface_handle = inst->src[PULL_UNIFORM_CONSTANT_SRC_SURFACE_HANDLE];
      const fs_reg offset_B = inst->src[PULL_UNIFORM_CONSTANT_SRC_OFFSET];
      const fs_reg size_B = inst->src[PULL_UNIFORM_CONSTANT_SRC_SIZE];
      assert(surface.file == BAD_FILE || surface_handle.file == BAD_FILE);
      assert(offset_B.file == IMM);
      assert(size_B.file == IMM);

      if (devinfo->has_lsc) {
         const fs_builder ubld =
            fs_builder(&s, block, inst).group(8, 0).exec_all();

         const fs_reg payload = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         ubld.MOV(payload, offset_B);

         inst->sfid = GFX12_SFID_UGM;
         inst->desc = lsc_msg_desc(devinfo, LSC_OP_LOAD,
                                   surface_handle.file == BAD_FILE ?
                                   LSC_ADDR_SURFTYPE_BTI :
                                   LSC_ADDR_SURFTYPE_BSS,
                                   LSC_ADDR_SIZE_A32,
                                   LSC_DATA_SIZE_D32,
                                   inst->size_written / 4,
                                   true /* transpose */,
                                   LSC_CACHE(devinfo, LOAD, L1STATE_L3MOCS));

         /* Update the original instruction. */
         inst->opcode = SHADER_OPCODE_SEND;
         inst->mlen = lsc_msg_addr_len(devinfo, LSC_ADDR_SIZE_A32, 1);
         inst->send_ex_bso = surface_handle.file != BAD_FILE &&
                             s.compiler->extended_bindless_surface_offset;
         inst->ex_mlen = 0;
         inst->header_size = 0;
         inst->send_has_side_effects = false;
         inst->send_is_volatile = true;
         inst->exec_size = 1;

         /* Finally, the payload */

         inst->resize_sources(3);
         setup_lsc_surface_descriptors(ubld, inst, inst->desc,
                                       surface.file != BAD_FILE ?
                                       surface : surface_handle);
         inst->src[2] = payload;

         s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);
      } else {
         const fs_builder ubld = fs_builder(&s, block, inst).exec_all();
         fs_reg header = fs_builder(&s, 8).exec_all().vgrf(BRW_REGISTER_TYPE_UD);

         ubld.group(8, 0).MOV(header,
                              retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD));
         ubld.group(1, 0).MOV(component(header, 2),
                              brw_imm_ud(offset_B.ud / 16));

         inst->sfid = GFX6_SFID_DATAPORT_CONSTANT_CACHE;
         inst->opcode = SHADER_OPCODE_SEND;
         inst->header_size = 1;
         inst->mlen = 1;

         uint32_t desc =
            brw_dp_oword_block_rw_desc(devinfo, true /* align_16B */,
                                       size_B.ud / 4, false /* write */);

         inst->resize_sources(4);

         setup_surface_descriptors(ubld, inst, desc, surface, surface_handle);

         inst->src[2] = header;
         inst->src[3] = fs_reg(); /* unused for reads */

         s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);
      }

      progress = true;
   }

   return progress;
}
