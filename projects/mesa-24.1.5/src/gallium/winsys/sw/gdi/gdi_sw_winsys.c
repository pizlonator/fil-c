/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 *
 **************************************************************************/

/**
 * @file
 * GDI software rasterizer support.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 */


#include <windows.h>

#include "util/format/u_formats.h"
#include "pipe/p_context.h"
#include "util/u_inlines.h"
#include "util/format/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "frontend/sw_winsys.h"
#include "gdi_sw_winsys.h"
#include "wgl/stw_gdishim.h"


struct gdi_sw_displaytarget
{
   enum pipe_format format;
   unsigned width;
   unsigned height;
   unsigned stride;

   unsigned size;

   void *data;

   BITMAPV5HEADER bmi;
};

struct gdi_sw_winsys {
   struct sw_winsys base;

   HDC (*acquire_hdc)(void *winsys_drawable_handle);
   void (*release_hdc)(void *winsys_drawable_handle, HDC hdc);
};

/** Cast wrapper */
static inline struct gdi_sw_displaytarget *
gdi_sw_displaytarget( struct sw_displaytarget *buf )
{
   return (struct gdi_sw_displaytarget *)buf;
}


/** Cast wrapper */
static inline struct gdi_sw_winsys *
gdi_sw_winsys( struct sw_winsys *buf )
{
   return (struct gdi_sw_winsys *)buf;
}


static bool
gdi_sw_is_displaytarget_format_supported( struct sw_winsys *ws,
                                                unsigned tex_usage,
                                                enum pipe_format format )
{
   switch(format) {
   case PIPE_FORMAT_B8G8R8X8_UNORM:
   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_B5G6R5_UNORM:
   case PIPE_FORMAT_B5G5R5A1_UNORM:
   case PIPE_FORMAT_B4G4R4A4_UNORM:
   case PIPE_FORMAT_R10G10B10A2_UNORM:
   case PIPE_FORMAT_R8G8B8X8_UNORM:
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      return true;

   /* TODO: Support other formats possible with BMPs, as described in 
    * http://msdn.microsoft.com/en-us/library/dd183376(VS.85).aspx */
      
   default:
      return false;
   }
}


static void *
gdi_sw_displaytarget_map(struct sw_winsys *ws,
                               struct sw_displaytarget *dt,
                               unsigned flags )
{
   struct gdi_sw_displaytarget *gdt = gdi_sw_displaytarget(dt);

   return gdt->data;
}


static void
gdi_sw_displaytarget_unmap(struct sw_winsys *ws,
                                 struct sw_displaytarget *dt )
{

}


static void
gdi_sw_displaytarget_destroy(struct sw_winsys *winsys,
                                   struct sw_displaytarget *dt)
{
   struct gdi_sw_displaytarget *gdt = gdi_sw_displaytarget(dt);

   align_free(gdt->data);
   FREE(gdt);
}


static struct sw_displaytarget *
gdi_sw_displaytarget_create(struct sw_winsys *winsys,
                                  unsigned tex_usage,
                                  enum pipe_format format,
                                  unsigned width, unsigned height,
                                  unsigned alignment,
                                  const void *front_private,
                                  unsigned *stride)
{
   struct gdi_sw_displaytarget *gdt;
   unsigned cpp;
   unsigned bpp;
   
   gdt = CALLOC_STRUCT(gdi_sw_displaytarget);
   if(!gdt)
      goto no_gdt;

   gdt->format = format;
   gdt->width = width;
   gdt->height = height;

   bpp = util_format_get_blocksizebits(format);
   cpp = util_format_get_blocksize(format);
   
   gdt->stride = align(width * cpp, alignment);
   gdt->size = gdt->stride * height;
   
   gdt->data = align_malloc(gdt->size, alignment);
   if(!gdt->data)
      goto no_data;

   gdt->bmi.bV5Size = sizeof(BITMAPV5HEADER);
   gdt->bmi.bV5Width = gdt->stride / cpp;
   gdt->bmi.bV5Height = -(long)height;
   gdt->bmi.bV5Planes = 1;
   gdt->bmi.bV5BitCount = bpp;
   gdt->bmi.bV5Compression = BI_RGB;
   gdt->bmi.bV5SizeImage = 0;
   gdt->bmi.bV5XPelsPerMeter = 0;
   gdt->bmi.bV5YPelsPerMeter = 0;
   gdt->bmi.bV5ClrUsed = 0;
   gdt->bmi.bV5ClrImportant = 0;

   if (format == PIPE_FORMAT_B5G6R5_UNORM) {
      gdt->bmi.bV5Compression = BI_BITFIELDS;
      gdt->bmi.bV5RedMask = 0xF800;
      gdt->bmi.bV5GreenMask = 0x07E0;
      gdt->bmi.bV5BlueMask = 0x001F;
   } else if (format == PIPE_FORMAT_B4G4R4A4_UNORM) {
      gdt->bmi.bV5Compression = BI_BITFIELDS;
      gdt->bmi.bV5RedMask = 0x0F00;
      gdt->bmi.bV5GreenMask = 0x00F0;
      gdt->bmi.bV5BlueMask = 0x000F;
   } else if (format == PIPE_FORMAT_R10G10B10A2_UNORM) {
      gdt->bmi.bV5Compression = BI_BITFIELDS;
      gdt->bmi.bV5RedMask = 0x000003FF;
      gdt->bmi.bV5GreenMask = 0x000FFC00;
      gdt->bmi.bV5BlueMask = 0x3FF00000;
   } else if (format == PIPE_FORMAT_R8G8B8X8_UNORM || format == PIPE_FORMAT_R8G8B8A8_UNORM) {
      gdt->bmi.bV5Compression = BI_BITFIELDS;
      gdt->bmi.bV5RedMask = 0x0000ff;
      gdt->bmi.bV5GreenMask = 0x00ff00;
      gdt->bmi.bV5BlueMask = 0xff0000;
   }

   *stride = gdt->stride;
   return (struct sw_displaytarget *)gdt;

no_data:
   FREE(gdt);
no_gdt:
   return NULL;
}


static struct sw_displaytarget *
gdi_sw_displaytarget_from_handle(struct sw_winsys *winsys,
                                 const struct pipe_resource *templet,
                                 struct winsys_handle *whandle,
                                 unsigned *stride)
{
   assert(0);
   return NULL;
}


static bool
gdi_sw_displaytarget_get_handle(struct sw_winsys *winsys,
                                struct sw_displaytarget *dt,
                                struct winsys_handle *whandle)
{
   assert(0);
   return false;
}


void
gdi_sw_display( struct sw_winsys *winsys,
                struct sw_displaytarget *dt,
                HDC hDC )
{
    struct gdi_sw_displaytarget *gdt = gdi_sw_displaytarget(dt);

    StretchDIBits(hDC,
                  0, 0, gdt->width, gdt->height,
                  0, 0, gdt->width, gdt->height,
                  gdt->data, (BITMAPINFO *)&gdt->bmi, 0, SRCCOPY);
}

static void
gdi_sw_displaytarget_display(struct sw_winsys *_winsys, 
                             struct sw_displaytarget *dt,
                             void *context_private,
                             unsigned nboxes,
                             struct pipe_box *box)
{
    struct gdi_sw_winsys *winsys = gdi_sw_winsys(_winsys);
    HDC hDC = winsys->acquire_hdc(context_private);

    gdi_sw_display(_winsys, dt, hDC);
    winsys->release_hdc(context_private, hDC);
}


static void
gdi_sw_destroy(struct sw_winsys *winsys)
{
   FREE(winsys);
}

struct sw_winsys *
gdi_create_sw_winsys(
    HDC (*acquire_hdc)(void *winsys_drawable_handle),
    void (*release_hdc)(void *winsys_drawable_handle, HDC hdc))
{
   static struct gdi_sw_winsys *winsys;

   winsys = CALLOC_STRUCT(gdi_sw_winsys);
   if(!winsys)
      return NULL;

   winsys->acquire_hdc = acquire_hdc;
   winsys->release_hdc = release_hdc;

   winsys->base.destroy = gdi_sw_destroy;
   winsys->base.is_displaytarget_format_supported = gdi_sw_is_displaytarget_format_supported;
   winsys->base.displaytarget_create = gdi_sw_displaytarget_create;
   winsys->base.displaytarget_from_handle = gdi_sw_displaytarget_from_handle;
   winsys->base.displaytarget_get_handle = gdi_sw_displaytarget_get_handle;
   winsys->base.displaytarget_map = gdi_sw_displaytarget_map;
   winsys->base.displaytarget_unmap = gdi_sw_displaytarget_unmap;
   winsys->base.displaytarget_display = gdi_sw_displaytarget_display;
   winsys->base.displaytarget_destroy = gdi_sw_displaytarget_destroy;

   return &winsys->base;
}


HDC gdi_sw_acquire_hdc_by_value(void *context_private) {
   return (HDC)context_private;
};

void gdi_sw_release_hdc_by_value(void *context_private, HDC hdc) {
   // Nothing to do
};
