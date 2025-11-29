/**************************************************************************
 *
 * Copyright 2010 Thomas Balling Sørensen & Orasanu Lucian.
 * Copyright 2014 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "vl/vl_winsys.h"
#include "va_private.h"

VAStatus
vlVaQueryDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int *num_attributes)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (ctx->max_display_attributes <= 0)
      return VA_STATUS_ERROR_UNIMPLEMENTED;

   if (!(attr_list && num_attributes))
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   *num_attributes = 0;

#if VA_CHECK_VERSION(1, 15, 0)
   attr_list->type = VADisplayPCIID;
   (*num_attributes)++;
#endif

   return vlVaGetDisplayAttributes(ctx, attr_list, *num_attributes);
}

VAStatus
vlVaGetDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int num_attributes)
{
   struct pipe_screen *pscreen;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (ctx->max_display_attributes <= 0)
      return VA_STATUS_ERROR_UNIMPLEMENTED;

   pscreen = VL_VA_PSCREEN(ctx);

   if (!pscreen)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (!attr_list)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   for (unsigned i = 0; i < num_attributes; i++) {
      switch (attr_list->type) {
#if VA_CHECK_VERSION(1, 15, 0)
      case VADisplayPCIID: {
         uint32_t vendor_id = pscreen->get_param(pscreen, PIPE_CAP_VENDOR_ID);
         uint32_t device_id = pscreen->get_param(pscreen, PIPE_CAP_DEVICE_ID);
         attr_list->min_value = attr_list->max_value = attr_list->value = (vendor_id << 16) | (device_id & 0xFFFF);
         attr_list->flags = VA_DISPLAY_ATTRIB_GETTABLE;
         break;
      }
#endif
      default:
         /* Only attributes returned with VA_DISPLAY_ATTRIB_GETTABLE set in the "flags" field
          * from vaQueryDisplayAttributes() can have their values retrieved.
          */
         break;
      }
      attr_list++;
   }

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaSetDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int num_attributes)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   return VA_STATUS_ERROR_UNIMPLEMENTED;
}
