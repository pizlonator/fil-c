/*
 * Copyright (C) 2021 Icecream95
 * Copyright (C) 2019 Google LLC
 * Copyright (C) 2024 Collabora, Ltd.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "drm-shim/drm_shim.h"
#include "drm-uapi/panfrost_drm.h"
#include "drm-uapi/panthor_drm.h"

#include "util/os_mman.h"
#include "util/u_math.h"

/* Default GPU ID if PAN_GPU_ID is not set. This defaults to Mali-G52. */
#define PAN_GPU_ID_DEFAULT (0x7212)

bool drm_shim_driver_prefers_first_render_node = true;

static uint64_t
pan_get_gpu_id(void)
{
   char *override_version = getenv("PAN_GPU_ID");

   if (override_version)
      return strtol(override_version, NULL, 16);

   return PAN_GPU_ID_DEFAULT;
}

static int
pan_ioctl_noop(int fd, unsigned long request, void *arg)
{
   return 0;
}

static int
panfrost_ioctl_get_param(int fd, unsigned long request, void *arg)
{
   struct drm_panfrost_get_param *gp = arg;

   switch (gp->param) {
   case DRM_PANFROST_PARAM_GPU_PROD_ID: {
      gp->value = pan_get_gpu_id();
      return 0;
   }

   case DRM_PANFROST_PARAM_SHADER_PRESENT:
      /* Assume an MP4 GPU */
      gp->value = 0xF;
      return 0;
   case DRM_PANFROST_PARAM_TILER_FEATURES:
      gp->value = 0x809;
      return 0;
   case DRM_PANFROST_PARAM_TEXTURE_FEATURES0:
   case DRM_PANFROST_PARAM_TEXTURE_FEATURES1:
   case DRM_PANFROST_PARAM_TEXTURE_FEATURES2:
   case DRM_PANFROST_PARAM_TEXTURE_FEATURES3:
      /* Allow all compressed textures */
      gp->value = ~0;
      return 0;
   case DRM_PANFROST_PARAM_GPU_REVISION:
   case DRM_PANFROST_PARAM_THREAD_TLS_ALLOC:
   case DRM_PANFROST_PARAM_AFBC_FEATURES:
   case DRM_PANFROST_PARAM_THREAD_FEATURES:
   case DRM_PANFROST_PARAM_MEM_FEATURES:
      /* lazy default, but works for the purposes of drm_shim */
      gp->value = 0x0;
      return 0;
   case DRM_PANFROST_PARAM_MMU_FEATURES:
      /* default for most hardware so far */
      gp->value = 0x00280030;
      return 0;
   case DRM_PANFROST_PARAM_MAX_THREADS:
   case DRM_PANFROST_PARAM_THREAD_MAX_WORKGROUP_SZ:
      gp->value = 256;
      return 0;
   default:
      fprintf(stderr, "Unknown DRM_IOCTL_PANFROST_GET_PARAM %d\n", gp->param);
      return -1;
   }
}

static int
panfrost_ioctl_create_bo(int fd, unsigned long request, void *arg)
{
   struct drm_panfrost_create_bo *create = arg;

   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct shim_bo *bo = calloc(1, sizeof(*bo));
   size_t size = ALIGN(create->size, 4096);

   drm_shim_bo_init(bo, size);

   create->handle = drm_shim_bo_get_handle(shim_fd, bo);
   create->offset = bo->mem_addr;

   drm_shim_bo_put(bo);

   return 0;
}

static int
panfrost_ioctl_mmap_bo(int fd, unsigned long request, void *arg)
{
   struct drm_panfrost_mmap_bo *mmap_bo = arg;

   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, mmap_bo->handle);

   mmap_bo->offset = drm_shim_bo_get_mmap_offset(shim_fd, bo);

   return 0;
}

static int
panfrost_ioctl_madvise(int fd, unsigned long request, void *arg)
{
   struct drm_panfrost_madvise *madvise = arg;

   madvise->retained = 1;

   return 0;
}

static ioctl_fn_t panfrost_driver_ioctls[] = {
   [DRM_PANFROST_SUBMIT] = pan_ioctl_noop,
   [DRM_PANFROST_WAIT_BO] = pan_ioctl_noop,
   [DRM_PANFROST_CREATE_BO] = panfrost_ioctl_create_bo,
   [DRM_PANFROST_MMAP_BO] = panfrost_ioctl_mmap_bo,
   [DRM_PANFROST_GET_PARAM] = panfrost_ioctl_get_param,
   [DRM_PANFROST_GET_BO_OFFSET] = pan_ioctl_noop,
   [DRM_PANFROST_PERFCNT_ENABLE] = pan_ioctl_noop,
   [DRM_PANFROST_PERFCNT_DUMP] = pan_ioctl_noop,
   [DRM_PANFROST_MADVISE] = panfrost_ioctl_madvise,
};

static int
panthor_ioctl_dev_query(int fd, unsigned long request, void *arg)
{
   struct drm_panthor_dev_query *dev_query = arg;

   switch (dev_query->type) {
   case DRM_PANTHOR_DEV_QUERY_GPU_INFO: {
      struct drm_panthor_gpu_info *gpu_info =
         (struct drm_panthor_gpu_info *)dev_query->pointer;

      gpu_info->gpu_id = pan_get_gpu_id() << 16;
      gpu_info->gpu_rev = 0;

      /* Dumped from a G610 */
      gpu_info->csf_id = 0x40a0412;
      gpu_info->l2_features = 0x7120306;
      gpu_info->tiler_features = 0x809;
      gpu_info->mem_features = 0x301;
      gpu_info->mmu_features = 0x2830;
      gpu_info->thread_features = 0x4010000;
      gpu_info->max_threads = 2048;
      gpu_info->thread_max_workgroup_size = 1024;
      gpu_info->thread_max_barrier_size = 1024;
      gpu_info->coherency_features = 0;
      gpu_info->texture_features[0] = 0xc1ffff9e;
      gpu_info->as_present = 0xff;
      gpu_info->shader_present = 0x50005;
      gpu_info->l2_present = 1;
      gpu_info->tiler_present = 1;
      return 0;
   }
   case DRM_PANTHOR_DEV_QUERY_CSIF_INFO: {
      struct drm_panthor_csif_info *csif_info =
         (struct drm_panthor_csif_info *)dev_query->pointer;

      /* Dumped from a G610 */
      csif_info->csg_slot_count = 8;
      csif_info->cs_slot_count = 8;
      csif_info->cs_reg_count = 96;
      csif_info->scoreboard_slot_count = 8;
      csif_info->unpreserved_cs_reg_count = 4;
      return 0;
   }
   default:
      fprintf(stderr, "Unknown DRM_IOCTL_PANTHOR_DEV_QUERY %d\n",
              dev_query->type);
      return -1;
   }

   return 0;
}

static int
panthor_ioctl_bo_create(int fd, unsigned long request, void *arg)
{
   struct drm_panthor_bo_create *bo_create = arg;

   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct shim_bo *bo = calloc(1, sizeof(*bo));
   size_t size = ALIGN(bo_create->size, 4096);

   drm_shim_bo_init(bo, size);

   bo_create->handle = drm_shim_bo_get_handle(shim_fd, bo);

   drm_shim_bo_put(bo);

   return 0;
}

static int
panthor_ioctl_bo_mmap_offset(int fd, unsigned long request, void *arg)
{
   struct drm_panthor_bo_mmap_offset *mmap_offset = arg;

   struct shim_fd *shim_fd = drm_shim_fd_lookup(fd);
   struct shim_bo *bo = drm_shim_bo_lookup(shim_fd, mmap_offset->handle);

   mmap_offset->offset = drm_shim_bo_get_mmap_offset(shim_fd, bo);

   return 0;
}

static ioctl_fn_t panthor_driver_ioctls[] = {
   [DRM_PANTHOR_DEV_QUERY] = panthor_ioctl_dev_query,
   [DRM_PANTHOR_VM_CREATE] = pan_ioctl_noop,
   [DRM_PANTHOR_VM_DESTROY] = pan_ioctl_noop,
   [DRM_PANTHOR_VM_BIND] = pan_ioctl_noop,
   [DRM_PANTHOR_VM_GET_STATE] = pan_ioctl_noop,
   [DRM_PANTHOR_BO_CREATE] = panthor_ioctl_bo_create,
   [DRM_PANTHOR_BO_MMAP_OFFSET] = panthor_ioctl_bo_mmap_offset,
   [DRM_PANTHOR_GROUP_CREATE] = pan_ioctl_noop,
   [DRM_PANTHOR_GROUP_DESTROY] = pan_ioctl_noop,
   [DRM_PANTHOR_GROUP_SUBMIT] = pan_ioctl_noop,
   [DRM_PANTHOR_GROUP_GET_STATE] = pan_ioctl_noop,
   [DRM_PANTHOR_TILER_HEAP_CREATE] = pan_ioctl_noop,
   [DRM_PANTHOR_TILER_HEAP_DESTROY] = pan_ioctl_noop,
};

static void *flush_id_mmap;

static void *
panthor_iomem_mmap(size_t size, int prot, int flags, off64_t offset)
{
   switch (offset) {
   case DRM_PANTHOR_USER_FLUSH_ID_MMIO_OFFSET:
      if (prot != PROT_READ || flags != MAP_SHARED || size != getpagesize())
         return MAP_FAILED;

      return flush_id_mmap;

   default:
      return MAP_FAILED;
   }
}

void
drm_shim_driver_init(void)
{
   uint64_t gpu_id = pan_get_gpu_id();
   bool is_csf_based = (gpu_id >> 12) > 9;

   shim_device.bus_type = DRM_BUS_PLATFORM;

   /* panfrost uses the DRM version to expose features, instead of getparam. */
   shim_device.version_major = 1;
   shim_device.version_minor = 1;
   shim_device.version_patchlevel = 0;

   if (is_csf_based) {
      shim_device.driver_name = "panthor";
      shim_device.driver_ioctls = panthor_driver_ioctls;
      shim_device.driver_ioctl_count = ARRAY_SIZE(panthor_driver_ioctls);

      flush_id_mmap = os_mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
      assert(flush_id_mmap != MAP_FAILED);
      memset(flush_id_mmap, 0, getpagesize());

      drm_shim_init_iomem_region(DRM_PANTHOR_USER_MMIO_OFFSET, getpagesize(),
                                 panthor_iomem_mmap);

      drm_shim_override_file("DRIVER=panthor\n"
                             "OF_FULLNAME=/soc/mali\n"
                             "OF_COMPATIBLE_0=arm,mali-valhall-csf\n"
                             "OF_COMPATIBLE_N=1\n",
                             "/sys/dev/char/%d:%d/device/uevent", DRM_MAJOR,
                             render_node_minor);
   } else {
      shim_device.driver_name = "panfrost";
      shim_device.driver_ioctls = panfrost_driver_ioctls;
      shim_device.driver_ioctl_count = ARRAY_SIZE(panfrost_driver_ioctls);

      drm_shim_override_file("DRIVER=panfrost\n"
                             "OF_FULLNAME=/soc/mali\n"
                             "OF_COMPATIBLE_0=arm,mali-t860\n"
                             "OF_COMPATIBLE_N=1\n",
                             "/sys/dev/char/%d:%d/device/uevent", DRM_MAJOR,
                             render_node_minor);
   }
}
