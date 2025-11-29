/*
 * Copyright © 2023 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */

#include "nv_device_info.h"
#include "nv_push.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cl902d.h"
#include "cla097.h"
#include "clb097.h"
#include "clc097.h"
#include "clc397.h"
#include "clc597.h"
#include "clc697.h"
#include "clc997.h"

#include "cla0c0.h"
#include "clb0c0.h"
#include "clc0c0.h"
#include "clc3c0.h"
#include "clc5c0.h"
#include "clc6c0.h"
#include "clc7c0.h"

#define KEPLER_DMA_COPY_A 0x0000a0b5
#define MAXWELL_DMA_COPY_A 0x0000b0b5
#define PASCAL_DMA_COPY_A 0x0000c0b5
#define VOLTA_DMA_COPY_A 0x0000c3b5
#define TURING_DMA_COPY_A 0x0000c5b5
#define AMPERE_DMA_COPY_A 0x0000c6b5
#define KEPLER_INLINE_TO_MEMORY_A 0xa040
#define KEPLER_INLINE_TO_MEMORY_B 0xa140

static struct nv_device_info get_fake_device_info(const char *arch_name) {
  struct nv_device_info info;

  memset(&info, 0, sizeof(info));

  if (!strcmp(arch_name, "KEPLER")) {
    info.cls_eng3d = KEPLER_A;
    info.cls_copy = KEPLER_DMA_COPY_A;
    info.cls_m2mf = KEPLER_INLINE_TO_MEMORY_A;
  } else if (!strcmp(arch_name, "MAXWELL")) {
    info.cls_eng3d = MAXWELL_A;
    info.cls_compute = MAXWELL_COMPUTE_A;
    info.cls_copy = MAXWELL_DMA_COPY_A;
  } else if (!strcmp(arch_name, "PASCAL")) {
    info.cls_eng3d = PASCAL_A;
    info.cls_compute = PASCAL_COMPUTE_A;
    info.cls_copy = PASCAL_DMA_COPY_A;
  } else if (!strcmp(arch_name, "VOLTA")) {
    info.cls_eng3d = VOLTA_A;
    info.cls_compute = VOLTA_COMPUTE_A;
    info.cls_copy = VOLTA_DMA_COPY_A;
  } else if (!strcmp(arch_name, "TURING")) {
    info.cls_eng3d = TURING_A;
    info.cls_compute = TURING_COMPUTE_A;
    info.cls_copy = TURING_DMA_COPY_A;
  } else if (!strcmp(arch_name, "AMPERE")) {
    info.cls_eng3d = AMPERE_A;
    info.cls_compute = AMPERE_COMPUTE_B;
    info.cls_copy = AMPERE_DMA_COPY_A;
  } else if (!strcmp(arch_name, "ADA")) {
    info.cls_eng3d = ADA_A;
    info.cls_copy = AMPERE_DMA_COPY_A;
  } else {
    fprintf(stderr, "Unknown architecture \"%s\", defaulting to Turing",
            arch_name);
    info.cls_eng3d = TURING_A;
    info.cls_compute = TURING_COMPUTE_A;
    info.cls_copy = TURING_DMA_COPY_A;
  }

  info.cls_eng2d = FERMI_TWOD_A;
  info.cls_m2mf = KEPLER_INLINE_TO_MEMORY_B;

  return info;
}

int main(int argc, char **argv) {
  const char *arch_name;
  const char *file_name;
  FILE *file;
  long file_size;
  uint32_t *data;
  struct nv_device_info device_info;
  struct nv_push pushbuf;

  if (argc != 3) {
    fprintf(stderr, "Usage: nv_push_dump file.bin "
                    "<KEPLER|MAXWELL|VOLTA|TURING|AMPERE|ADA>\n");
    return 1;
  }

  file_name = argv[1];
  arch_name = argv[2];

  device_info = get_fake_device_info(arch_name);

  file = fopen(file_name, "r");

  if (file == NULL) {
    fprintf(stderr, "couldn't open file \"%s\"\n", file_name);
    return 1;
  }

  fseek(file, 0L, SEEK_END);
  file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);

  if (file_size % 4 != 0) {
    fclose(file);

    fprintf(stderr, "invalid file, data isn't aligned to 4 bytes\n");
    return 1;
  }

  data = malloc(file_size);

  if (data == NULL) {
    fclose(file);

    fprintf(stderr, "memory allocation failed\n");
    return 1;
  }

  fread(data, file_size, 1, file);
  fclose(file);

  nv_push_init(&pushbuf, data, file_size / 4);
  pushbuf.last_size = pushbuf.limit - 1;
  pushbuf.end = pushbuf.limit;

  vk_push_print(stdout, &pushbuf, &device_info);

  free(data);

  return 0;
}