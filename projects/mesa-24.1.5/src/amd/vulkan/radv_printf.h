/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_PRINTF_H
#define RADV_PRINTF_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <vulkan/vulkan_core.h>

#include "util/u_dynarray.h"

struct radv_device;
typedef struct nir_builder nir_builder;
typedef struct nir_shader nir_shader;
typedef struct nir_def nir_def;

struct radv_printf_data {
   uint32_t buffer_size;
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkDeviceAddress buffer_addr;
   void *data;
   struct util_dynarray formats;
};

struct radv_printf_format {
   char *string;
   uint32_t divergence_mask;
   uint8_t element_sizes[32];
};

struct radv_printf_buffer_header {
   uint32_t offset;
   uint32_t size;
};

VkResult radv_printf_data_init(struct radv_device *device);

void radv_printf_data_finish(struct radv_device *device);

void radv_build_printf(nir_builder *b, nir_def *cond, const char *format, ...);

void radv_dump_printf_data(struct radv_device *device, FILE *out);

void radv_device_associate_nir(struct radv_device *device, nir_shader *nir);

#endif /* RADV_PRINTF_H */
