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

#ifndef GX6250_H
#define GX6250_H

#include <stdbool.h>

#include "pvr_device_info.h"

static const struct pvr_device_ident pvr_device_ident_4_V_2_51 = {
   .device_id = 0x6250,
   .series_name = "Rogue",
   .public_name = "GX6250",
};

static const struct pvr_device_features pvr_device_features_4_V_2_51 = {
   .has_astc = true,
   .has_cluster_grouping = true,
   .has_common_store_size_in_dwords = true,
   .has_compute = true,
   .has_compute_morton_capable = true,
   .has_compute_overlap = true,
   .has_eight_output_registers = true,
   .has_fbcdc_algorithm = true,
   .has_gs_rta_support = true,
   .has_isp_max_tiles_in_flight = true,
   .has_isp_samples_per_pixel = true,
   .has_max_instances_per_pds_task = true,
   .has_max_multisample = true,
   .has_max_partitions = true,
   .has_max_usc_tasks = true,
   .has_num_clusters = true,
   .has_num_raster_pipes = true,
   .has_num_user_clip_planes = true,
   .has_pbe_filterable_f16 = true,
   .has_pbe_yuv = true,
   .has_slc_cache_line_size_bits = true,
   .has_slc_mcu_cache_controls = true,
   .has_tf_bicubic_filter = true,
   .has_tile_size_x = true,
   .has_tile_size_y = true,
   .has_tpu_array_textures = true,
   .has_tpu_extended_integer_lookup = true,
   .has_tpu_image_state_v2 = true,
   .has_usc_f16sop_u8 = true,
   .has_usc_min_output_registers_per_pix = true,
   .has_usc_slots = true,
   .has_uvs_banks = true,
   .has_uvs_pba_entries = true,
   .has_uvs_vtx_entries = true,
   .has_vdm_cam_size = true,
   .has_xt_top_infrastructure = true,
   .has_zls_subtile = true,

   .common_store_size_in_dwords = 1280U * 4U * 4U,
   .fbcdc_algorithm = 2,
   .isp_max_tiles_in_flight = 4U,
   .isp_samples_per_pixel = 2U,
   .max_instances_per_pds_task = 32U,
   .max_multisample = 8U,
   .max_partitions = 8U,
   .max_usc_tasks = 56U,
   .num_clusters = 2U,
   .num_raster_pipes = 1U,
   .num_user_clip_planes = 8U,
   .slc_cache_line_size_bits = 512U,
   .tile_size_x = 32U,
   .tile_size_y = 32U,
   .usc_min_output_registers_per_pix = 2U,
   .usc_slots = 32U,
   .uvs_banks = 8U,
   .uvs_pba_entries = 320U,
   .uvs_vtx_entries = 288U,
   .vdm_cam_size = 256U,

   .has_requires_fb_cdc_zls_setup = true,
};

static const struct pvr_device_enhancements pvr_device_enhancements_4_40_2_51 = {
   .has_ern35421 = true,
   .has_ern38020 = true,
   .has_ern38748 = true,
   .has_ern42064 = true,
   .has_ern42307 = true,
};

static const struct pvr_device_quirks pvr_device_quirks_4_40_2_51 = {
   .has_brn44079 = true,
   .has_brn47727 = true,
   .has_brn48492 = true,
   .has_brn48545 = true,
   .has_brn49032 = true,
   .has_brn49927 = true,
   .has_brn51025 = true,
   .has_brn51210 = true,
   .has_brn51764 = true,
   .has_brn52354 = true,
   .has_brn52942 = true,
   .has_brn58839 = true,
   .has_brn62269 = true,
   .has_brn66011 = true,
   .has_brn70165 = true,
};

#endif /* GX6250_H */
