/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "error2hangdump_lib.h"

#include "common/intel_hang_dump.h"

void
write_header(FILE *f)
{
   struct intel_hang_dump_block_header header = {
      .base = {
         .type = INTEL_HANG_DUMP_BLOCK_TYPE_HEADER,
      },
      .magic   = INTEL_HANG_DUMP_MAGIC,
      .version = INTEL_HANG_DUMP_VERSION,
   };

   fwrite(&header, sizeof(header), 1, f);
}

void
write_buffer(FILE *f,
             uint64_t offset,
             const void *data,
             uint64_t size,
             const char *name)
{
   struct intel_hang_dump_block_bo header = {
      .base = {
         .type = INTEL_HANG_DUMP_BLOCK_TYPE_BO,
      },
      .offset  = offset,
      .size    = size,
   };
   snprintf(header.name, sizeof(header.name), "%s", name);

   fwrite(&header, sizeof(header), 1, f);
   fwrite(data, size, 1, f);
}

void
write_hw_image_buffer(FILE *f, const void *data, uint64_t size)
{
   struct intel_hang_dump_block_hw_image header = {
      .base = {
         .type = INTEL_HANG_DUMP_BLOCK_TYPE_HW_IMAGE,
      },
      .size    = size,
   };

   fwrite(&header, sizeof(header), 1, f);
   fwrite(data, size, 1, f);
}

void
write_exec(FILE *f, uint64_t offset)
{
   struct intel_hang_dump_block_exec header = {
      .base = {
         .type = INTEL_HANG_DUMP_BLOCK_TYPE_EXEC,
      },
      .offset  = offset,
   };

   fwrite(&header, sizeof(header), 1, f);
}
