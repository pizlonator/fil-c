/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "error_decode_lib.h"

const char *
ascii85_decode_char(const char *in, uint32_t *v)
{
   *v = 0;

   if (*in == 'z') {
      in++;
   } else {
      *v += in[0] - 33; *v *= 85;
      *v += in[1] - 33; *v *= 85;
      *v += in[2] - 33; *v *= 85;
      *v += in[3] - 33; *v *= 85;
      *v += in[4] - 33;
      in += 5;
   }

   return in;
}
