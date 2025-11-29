/*
 * Copyright © 2023 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef AXE_1_16M_H
#define AXE_1_16M_H

#include <stdbool.h>

#include "pvr_device_info.h"

static const struct pvr_device_ident pvr_device_ident_33_V_11_3 = {
   .device_id = 0x33011003,
   .series_name = "A-Series",
   .public_name = "AXE-1-16M",
};

static const struct pvr_device_features pvr_device_features_33_V_11_3 = {
   .has_common_store_size_in_dwords = true,
   .has_compute = true,
   .has_ipf_creq_pf = true,
   .has_isp_max_tiles_in_flight = true,
   .has_isp_samples_per_pixel = true,
   .has_max_instances_per_pds_task = true,
   .has_max_multisample = true,
   .has_max_partitions = true,
   .has_max_usc_tasks = true,
   .has_num_clusters = true,
   .has_num_raster_pipes = true,
   .has_num_user_clip_planes = true,
   .has_pbe2_in_xe = true,
   .has_pbe_filterable_f16 = true,
   .has_pbe_yuv = true,
   .has_roguexe = true,
   .has_screen_size8K = true,
   .has_simple_internal_parameter_format = true,
   .has_simple_internal_parameter_format_v2 = true,
   .has_simple_parameter_format_version = true,
   .has_slc_cache_line_size_bits = true,
   .has_tile_size_16x16 = true,
   .has_tile_size_x = true,
   .has_tile_size_y = true,
   .has_tpu_border_colour_enhanced = true,
   .has_tpu_extended_integer_lookup = true,
   .has_tpu_image_state_v2 = true,
   .has_usc_f16sop_u8 = true,
   .has_usc_min_output_registers_per_pix = true,
   .has_usc_pixel_partition_mask = true,
   .has_usc_slots = true,
   .has_uvs_banks = true,
   .has_uvs_pba_entries = true,
   .has_uvs_vtx_entries = true,
   .has_vdm_cam_size = true,
   .has_vdm_degenerate_culling = true,

   .common_store_size_in_dwords = 512U * 4U * 4U,
   .isp_max_tiles_in_flight = 1U,
   .isp_samples_per_pixel = 1U,
   .max_instances_per_pds_task = 32U,
   .max_multisample = 4U,
   .max_partitions = 4U,
   .max_usc_tasks = 24U,
   .num_clusters = 1U,
   .num_raster_pipes = 1U,
   .num_user_clip_planes = 8U,
   .simple_parameter_format_version = 2U,
   .slc_cache_line_size_bits = 512U,
   .tile_size_x = 16U,
   .tile_size_y = 16U,
   .usc_min_output_registers_per_pix = 1U,
   .usc_slots = 14U,
   .uvs_banks = 2U,
   .uvs_pba_entries = 320U,
   .uvs_vtx_entries = 288U,
   .vdm_cam_size = 32U,

   .has_s8xe = true,
};

static const struct pvr_device_enhancements pvr_device_enhancements_33_15_11_3 = {
   .has_ern35421 = true,
   .has_ern38748 = true,
   .has_ern42307 = true,
   .has_ern45493 = true,
};

static const struct pvr_device_quirks pvr_device_quirks_33_15_11_3 = {
   .has_brn70165 = true,
};

#endif /* AXE_1_16M_H */
