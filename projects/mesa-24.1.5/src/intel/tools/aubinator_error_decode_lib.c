/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "aubinator_error_decode_lib.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "util/macros.h"

int ring_name_to_class(const char *ring_name, enum intel_engine_class *class)
{
   static const char *class_names[] = {
      [INTEL_ENGINE_CLASS_RENDER] = "rcs",
      [INTEL_ENGINE_CLASS_COMPUTE] = "ccs",
      [INTEL_ENGINE_CLASS_COPY] = "bcs",
      [INTEL_ENGINE_CLASS_VIDEO] = "vcs",
      [INTEL_ENGINE_CLASS_VIDEO_ENHANCE] = "vecs",
   };
   for (size_t i = 0; i < ARRAY_SIZE(class_names); i++) {
      if (strncmp(ring_name, class_names[i], strlen(class_names[i])))
         continue;

      *class = i;
      return atoi(ring_name + strlen(class_names[i]));
   }

   static const struct {
      const char *name;
      unsigned int class;
      int instance;
   } legacy_names[] = {
      { "render", INTEL_ENGINE_CLASS_RENDER, 0 },
      { "blt", INTEL_ENGINE_CLASS_COPY, 0 },
      { "bsd", INTEL_ENGINE_CLASS_VIDEO, 0 },
      { "bsd2", INTEL_ENGINE_CLASS_VIDEO, 1 },
      { "vebox", INTEL_ENGINE_CLASS_VIDEO_ENHANCE, 0 },
   };
   for (size_t i = 0; i < ARRAY_SIZE(legacy_names); i++) {
      if (strcmp(ring_name, legacy_names[i].name))
         continue;

      *class = legacy_names[i].class;
      return legacy_names[i].instance;
   }

   return -1;
}

void
dump_shader_binary(void *user_data, const char *short_name,
                   uint64_t address, const void *data,
                   unsigned data_length)
{
   char filename[128];
   snprintf(filename, sizeof(filename), "%s_0x%016"PRIx64".bin",
            short_name, address);

   FILE *f = fopen(filename, "w");
   if (f == NULL) {
      fprintf(stderr, "Unable to open %s\n", filename);
      return;
   }
   fwrite(data, data_length, 1, f);
   fclose(f);
}
