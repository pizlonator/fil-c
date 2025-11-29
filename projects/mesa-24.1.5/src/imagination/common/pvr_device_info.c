/*
 * Copyright © 2022 Imagination Technologies Ltd.
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

/* TODO: This file is currently hand-maintained. However, the intention is to
 * auto-generate it in the future based on the hwdefs.
 */

#include <assert.h>
#include <errno.h>

#include "pvr_device_info.h"

#include "device_info/gx6250.h"
#include "device_info/axe-1-16m.h"
#include "device_info/bxs-4-64.h"

/**
 * Initialize PowerVR device information.
 *
 * \param info Device info structure to initialize.
 * \param bvnc Packed BVNC.
 * \return
 *  * 0 on success, or
 *  * -%ENODEV if the device is not supported.
 */
int pvr_device_info_init(struct pvr_device_info *info, uint64_t bvnc)
{
#define CASE_PACKED_BVNC_DEVICE_INFO(_b, _v, _n, _c)                          \
   case PVR_BVNC_PACK(_b, _v, _n, _c):                                        \
      info->ident = pvr_device_ident_##_b##_V_##_n##_##_c;                    \
      info->ident.b = _b;                                                     \
      info->ident.n = _n;                                                     \
      info->ident.v = _v;                                                     \
      info->ident.c = _c;                                                     \
      info->features = pvr_device_features_##_b##_V_##_n##_##_c;              \
      info->enhancements = pvr_device_enhancements_##_b##_##_v##_##_n##_##_c; \
      info->quirks = pvr_device_quirks_##_b##_##_v##_##_n##_##_c;             \
      return 0

   switch (bvnc) {
      CASE_PACKED_BVNC_DEVICE_INFO(4, 40, 2, 51);
      CASE_PACKED_BVNC_DEVICE_INFO(33, 15, 11, 3);
   }

#undef CASE_PACKED_BVNC_DEVICE_INFO

   assert(!"Unsupported Device");

   return -ENODEV;
}
