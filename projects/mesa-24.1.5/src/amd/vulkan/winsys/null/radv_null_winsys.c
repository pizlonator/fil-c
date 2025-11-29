/*
 * Copyright © 2020 Valve Corporation
 *
 * based on amdgpu winsys.
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */
#include "radv_null_winsys_public.h"

#include "util/u_string.h"
#include "radv_null_bo.h"
#include "radv_null_cs.h"
#include "vk_sync_dummy.h"

/* Hardcode some GPU info that are needed for the driver or for some tools. */
static const struct {
   uint32_t pci_id;
   uint32_t num_render_backends;
   bool has_dedicated_vram;
} pci_ids[] = {
   /* clang-format off */
   [CHIP_TAHITI] = {0x6780, 8, true},
   [CHIP_PITCAIRN] = {0x6800, 8, true},
   [CHIP_VERDE] = {0x6820, 4, true},
   [CHIP_OLAND] = {0x6060, 2, true},
   [CHIP_HAINAN] = {0x6660, 2, true},
   [CHIP_BONAIRE] = {0x6640, 4, true},
   [CHIP_KAVERI] = {0x1304, 2, false},
   [CHIP_KABINI] = {0x9830, 2, false},
   [CHIP_HAWAII] = {0x67A0, 16, true},
   [CHIP_TONGA] = {0x6920, 8, true},
   [CHIP_ICELAND] = {0x6900, 2, true},
   [CHIP_CARRIZO] = {0x9870, 2, false},
   [CHIP_FIJI] = {0x7300, 16, true},
   [CHIP_STONEY] = {0x98E4, 2, false},
   [CHIP_POLARIS10] = {0x67C0, 8, true},
   [CHIP_POLARIS11] = {0x67E0, 4, true},
   [CHIP_POLARIS12] = {0x6980, 4, true},
   [CHIP_VEGAM] = {0x694C, 4, true},
   [CHIP_VEGA10] = {0x6860, 16, true},
   [CHIP_VEGA12] = {0x69A0, 8, true},
   [CHIP_VEGA20] = {0x66A0, 16, true},
   [CHIP_RAVEN] = {0x15DD, 2, false},
   [CHIP_RENOIR] = {0x1636, 2, false},
   [CHIP_MI100] = {0x738C, 2, true},
   [CHIP_NAVI10] = {0x7310, 16, true},
   [CHIP_NAVI12] = {0x7360, 8, true},
   [CHIP_NAVI14] = {0x7340, 8, true},
   [CHIP_NAVI21] = {0x73A0, 16, true},
   [CHIP_VANGOGH] = {0x163F, 8, false},
   [CHIP_NAVI22] = {0x73C0, 8, true},
   [CHIP_NAVI23] = {0x73E0, 8, true},
   [CHIP_NAVI31] = {0x744C, 24, true},
   /* clang-format on */
};

static void
radv_null_winsys_query_info(struct radeon_winsys *rws, struct radeon_info *gpu_info)
{
   const char *family = getenv("RADV_FORCE_FAMILY");
   unsigned i;

   gpu_info->gfx_level = CLASS_UNKNOWN;
   gpu_info->family = CHIP_UNKNOWN;

   for (i = CHIP_TAHITI; i < CHIP_LAST; i++) {
      if (!strcasecmp(family, ac_get_family_name(i))) {
         /* Override family and gfx_level. */
         gpu_info->family = i;
         gpu_info->name = ac_get_family_name(i);

         if (gpu_info->family >= CHIP_NAVI31)
            gpu_info->gfx_level = GFX11;
         else if (i >= CHIP_NAVI21)
            gpu_info->gfx_level = GFX10_3;
         else if (i >= CHIP_NAVI10)
            gpu_info->gfx_level = GFX10;
         else if (i >= CHIP_VEGA10)
            gpu_info->gfx_level = GFX9;
         else if (i >= CHIP_TONGA)
            gpu_info->gfx_level = GFX8;
         else if (i >= CHIP_BONAIRE)
            gpu_info->gfx_level = GFX7;
         else
            gpu_info->gfx_level = GFX6;
      }
   }

   if (gpu_info->family == CHIP_UNKNOWN) {
      fprintf(stderr, "radv: Unknown family: %s\n", family);
      abort();
   }

   gpu_info->pci_id = pci_ids[gpu_info->family].pci_id;
   gpu_info->max_se = 4;
   gpu_info->num_se = 4;
   if (gpu_info->gfx_level >= GFX10_3)
      gpu_info->max_waves_per_simd = 16;
   else if (gpu_info->gfx_level >= GFX10)
      gpu_info->max_waves_per_simd = 20;
   else if (gpu_info->family >= CHIP_POLARIS10 && gpu_info->family <= CHIP_VEGAM)
      gpu_info->max_waves_per_simd = 8;
   else
      gpu_info->max_waves_per_simd = 10;

   if (gpu_info->gfx_level >= GFX10)
      gpu_info->num_physical_sgprs_per_simd = 128 * gpu_info->max_waves_per_simd;
   else if (gpu_info->gfx_level >= GFX8)
      gpu_info->num_physical_sgprs_per_simd = 800;
   else
      gpu_info->num_physical_sgprs_per_simd = 512;

   gpu_info->has_3d_cube_border_color_mipmap = true;
   gpu_info->has_image_opcodes = true;

   if (gpu_info->family == CHIP_NAVI31 || gpu_info->family == CHIP_NAVI32)
      gpu_info->num_physical_wave64_vgprs_per_simd = 768;
   else if (gpu_info->gfx_level >= GFX10)
      gpu_info->num_physical_wave64_vgprs_per_simd = 512;
   else
      gpu_info->num_physical_wave64_vgprs_per_simd = 256;
   gpu_info->num_simd_per_compute_unit = gpu_info->gfx_level >= GFX10 ? 2 : 4;
   gpu_info->lds_size_per_workgroup = gpu_info->gfx_level >= GFX10  ? 128 * 1024
                                      : gpu_info->gfx_level >= GFX7 ? 64 * 1024
                                                                    : 32 * 1024;
   gpu_info->lds_encode_granularity = gpu_info->gfx_level >= GFX7 ? 128 * 4 : 64 * 4;
   gpu_info->lds_alloc_granularity = gpu_info->gfx_level >= GFX10_3 ? 256 * 4 : gpu_info->lds_encode_granularity;
   gpu_info->max_render_backends = pci_ids[gpu_info->family].num_render_backends;

   gpu_info->has_dedicated_vram = pci_ids[gpu_info->family].has_dedicated_vram;
   gpu_info->has_packed_math_16bit = gpu_info->gfx_level >= GFX9;

   gpu_info->has_image_load_dcc_bug = gpu_info->family == CHIP_NAVI23 || gpu_info->family == CHIP_VANGOGH;

   gpu_info->has_accelerated_dot_product =
      gpu_info->family == CHIP_VEGA20 || (gpu_info->family >= CHIP_MI100 && gpu_info->family != CHIP_NAVI10);

   gpu_info->address32_hi = gpu_info->gfx_level >= GFX9 ? 0xffff8000u : 0x0;

   gpu_info->has_rbplus = gpu_info->family == CHIP_STONEY || gpu_info->gfx_level >= GFX9;
   gpu_info->rbplus_allowed =
      gpu_info->has_rbplus &&
      (gpu_info->family == CHIP_STONEY || gpu_info->family == CHIP_VEGA12 || gpu_info->family == CHIP_RAVEN ||
       gpu_info->family == CHIP_RAVEN2 || gpu_info->family == CHIP_RENOIR || gpu_info->gfx_level >= GFX10_3);

   gpu_info->has_scheduled_fence_dependency = true;
   gpu_info->has_gang_submit = true;
}

static const char *
radv_null_winsys_get_chip_name(struct radeon_winsys *rws)
{
   return "Null hardware";
}

static void
radv_null_winsys_destroy(struct radeon_winsys *rws)
{
   FREE(rws);
}

static int
radv_null_winsys_get_fd(struct radeon_winsys *rws)
{
   return -1;
}

static const struct vk_sync_type *const *
radv_null_winsys_get_sync_types(struct radeon_winsys *rws)
{
   return radv_null_winsys(rws)->sync_types;
}

struct radeon_winsys *
radv_null_winsys_create()
{
   struct radv_null_winsys *ws;

   ws = calloc(1, sizeof(struct radv_null_winsys));
   if (!ws)
      return NULL;

   ws->base.destroy = radv_null_winsys_destroy;
   ws->base.query_info = radv_null_winsys_query_info;
   ws->base.get_fd = radv_null_winsys_get_fd;
   ws->base.get_sync_types = radv_null_winsys_get_sync_types;
   ws->base.get_chip_name = radv_null_winsys_get_chip_name;
   radv_null_bo_init_functions(ws);
   radv_null_cs_init_functions(ws);

   ws->sync_types[0] = &vk_sync_dummy_type;
   ws->sync_types[1] = NULL;
   return &ws->base;
}
