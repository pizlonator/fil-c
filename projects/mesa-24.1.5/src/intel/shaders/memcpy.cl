/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

void
genX(libanv_memcpy)(global void *dst_base,
                    global void *src_base,
                    uint num_dwords,
                    uint dword_offset)
{
   global void *dst = dst_base + 4 * dword_offset;
   global void *src = src_base + 4 * dword_offset;

   if (dword_offset + 4 <= num_dwords) {
      *(global uint4 *)(dst) = *(global uint4 *)(src);
   } else if (dword_offset + 3 <= num_dwords) {
      *(global uint3 *)(dst) = *(global uint3 *)(src);
   } else if (dword_offset + 2 <= num_dwords) {
      *(global uint2 *)(dst) = *(global uint2 *)(src);
   } else if (dword_offset + 1 <= num_dwords) {
      *(global uint *)(dst) = *(global uint *)(src);
   }
}
