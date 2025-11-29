/*
 * Copyright © 2023 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mme_fermi.h"
#include "mme_tu104.h"


int main(int argc, char **argv) {
  const char *arch_name;
  const char *file_name;
  FILE *file;
  long file_size;
  uint32_t *data;

  if (argc != 3) {
    fprintf(stderr, "Usage: nv_mme_dump file.bin "
                    "<FERMI|TURING>\n");
    return 1;
  }

  file_name = argv[1];
  arch_name = argv[2];

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

   if (!strcmp(arch_name, "FERMI")) {
      mme_fermi_dump(stdout, data, file_size);
   } else {
      mme_tu104_dump(stdout, data, file_size);
   }

   free(data);

  return 0;
}