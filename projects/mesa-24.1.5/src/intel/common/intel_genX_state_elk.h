/*
 * Copyright (c) 2022 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INTEL_GENX_STATE_ELK_H
#define INTEL_GENX_STATE_ELK_H

#ifndef GFX_VERx10
#error This file should only be included by genX files.
#endif

#if GFX_VER > 8
#error "ELK doesn't support Gfx > 8."
#endif

#include <stdbool.h>

#include "dev/intel_device_info.h"
#include "genxml/gen_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#if GFX_VER >= 7

static inline void
intel_set_ps_dispatch_state(struct GENX(3DSTATE_PS) *ps,
                            const struct intel_device_info *devinfo,
                            const struct elk_wm_prog_data *prog_data,
                            unsigned rasterization_samples,
                            enum intel_msaa_flags msaa_flags)
{
   assert(rasterization_samples != 0);

   bool enable_8  = prog_data->dispatch_8;
   bool enable_16 = prog_data->dispatch_16;
   bool enable_32 = prog_data->dispatch_32;

   /* SKL PRMs, Volume 2a: Command Reference: Instructions:
    *    3DSTATE_PS_BODY::8 Pixel Dispatch Enable:
    *
    *    "When Render Target Fast Clear Enable is ENABLED or Render Target
    *     Resolve Type = RESOLVE_PARTIAL or RESOLVE_FULL, this bit must be
    *     DISABLED."
    */
#if GFX_VER >= 8
   /* BDW has the same wording as SKL, except some of the fields mentioned
    * don't exist...
    */
   if (ps->RenderTargetFastClearEnable ||
       ps->RenderTargetResolveEnable)
      enable_8 = false;
#endif

   const bool is_persample_dispatch =
      elk_wm_prog_data_is_persample(prog_data, msaa_flags);

   if (is_persample_dispatch) {
      /* Starting with SandyBridge (where we first get MSAA), the different
       * pixel dispatch combinations are grouped into classifications A
       * through F (SNB PRM Vol. 2 Part 1 Section 7.7.1).  On most hardware
       * generations, the only configurations supporting persample dispatch
       * are those in which only one dispatch width is enabled.
       */
      if (enable_32 || enable_16)
         enable_8 = false;
      if (enable_32)
         enable_16 = false;
   }

   assert(enable_8 || enable_16 || enable_32);

   ps->_8PixelDispatchEnable = enable_8;
   ps->_16PixelDispatchEnable = enable_16;
   ps->_32PixelDispatchEnable = enable_32;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* INTEL_GENX_STATE_ELK_H */
