/*
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef LOADER_DRI_HELPER_H
#define LOADER_DRI_HELPER_H

#include <stdbool.h>
#include <sys/types.h>

#include <GL/gl.h> /* dri_interface needs GL types */
#include <GL/internal/dri_interface.h>
#include <c11/threads.h>
#include "util/format/u_formats.h"

#ifdef HAVE_X11_PLATFORM
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>

struct loader_crtc_info {
   xcb_randr_crtc_t id;
   xcb_timestamp_t timestamp;

   int16_t x, y;
   uint16_t width, height;

   unsigned refresh_numerator;
   unsigned refresh_denominator;
};

struct loader_screen_resources {
   mtx_t mtx;

   xcb_connection_t *conn;
   xcb_screen_t *screen;

   xcb_timestamp_t config_timestamp;

   /* Number of CRTCs with an active mode set */
   unsigned num_crtcs;
   struct loader_crtc_info *crtcs;
};
#endif


/**
 * These formats correspond to the similarly named MESA_FORMAT_*
 * tokens, except in the native endian of the CPU.  For example, on
 * little endian __DRI_IMAGE_FORMAT_XRGB8888 corresponds to
 * MESA_FORMAT_XRGB8888, but MESA_FORMAT_XRGB8888_REV on big endian.
 *
 * __DRI_IMAGE_FORMAT_NONE is for images that aren't directly usable
 * by the driver (YUV planar formats) but serve as a base image for
 * creating sub-images for the different planes within the image.
 *
 * R8, GR88 and NONE should not be used with createImageFromName or
 * createImage, and are returned by query from sub images created with
 * createImageFromNames (NONE, see above) and fromPlane (R8 & GR88).
 */
#define __DRI_IMAGE_FORMAT_RGB565       PIPE_FORMAT_B5G6R5_UNORM
#define __DRI_IMAGE_FORMAT_XRGB8888     PIPE_FORMAT_BGRX8888_UNORM
#define __DRI_IMAGE_FORMAT_ARGB8888     PIPE_FORMAT_BGRA8888_UNORM
#define __DRI_IMAGE_FORMAT_ABGR8888     PIPE_FORMAT_RGBA8888_UNORM
#define __DRI_IMAGE_FORMAT_XBGR8888     PIPE_FORMAT_RGBX8888_UNORM
#define __DRI_IMAGE_FORMAT_R8           PIPE_FORMAT_R8_UNORM
#define __DRI_IMAGE_FORMAT_GR88         PIPE_FORMAT_RG88_UNORM
#define __DRI_IMAGE_FORMAT_NONE         PIPE_FORMAT_NONE
#define __DRI_IMAGE_FORMAT_XRGB2101010  PIPE_FORMAT_B10G10R10X2_UNORM
#define __DRI_IMAGE_FORMAT_ARGB2101010  PIPE_FORMAT_B10G10R10A2_UNORM
#define __DRI_IMAGE_FORMAT_SARGB8       PIPE_FORMAT_BGRA8888_SRGB
#define __DRI_IMAGE_FORMAT_ARGB1555     PIPE_FORMAT_B5G5R5A1_UNORM
#define __DRI_IMAGE_FORMAT_R16          PIPE_FORMAT_R16_UNORM
#define __DRI_IMAGE_FORMAT_GR1616       PIPE_FORMAT_RG1616_UNORM
#define __DRI_IMAGE_FORMAT_XBGR2101010  PIPE_FORMAT_R10G10B10X2_UNORM
#define __DRI_IMAGE_FORMAT_ABGR2101010  PIPE_FORMAT_R10G10B10A2_UNORM
#define __DRI_IMAGE_FORMAT_SABGR8       PIPE_FORMAT_RGBA8888_SRGB
#define __DRI_IMAGE_FORMAT_XBGR16161616F PIPE_FORMAT_R16G16B16X16_FLOAT
#define __DRI_IMAGE_FORMAT_ABGR16161616F PIPE_FORMAT_R16G16B16A16_FLOAT
#define __DRI_IMAGE_FORMAT_SXRGB8       PIPE_FORMAT_BGRX8888_SRGB
#define __DRI_IMAGE_FORMAT_ABGR16161616 PIPE_FORMAT_R16G16B16X16_UNORM
#define __DRI_IMAGE_FORMAT_XBGR16161616 PIPE_FORMAT_R16G16B16A16_UNORM
#define __DRI_IMAGE_FORMAT_ARGB4444	PIPE_FORMAT_B4G4R4A4_UNORM
#define __DRI_IMAGE_FORMAT_XRGB4444	PIPE_FORMAT_B4G4R4X4_UNORM
#define __DRI_IMAGE_FORMAT_ABGR4444	PIPE_FORMAT_R4G4B4A4_UNORM
#define __DRI_IMAGE_FORMAT_XBGR4444	PIPE_FORMAT_R4G4B4X4_UNORM
#define __DRI_IMAGE_FORMAT_XRGB1555	PIPE_FORMAT_B5G5R5X1_UNORM
#define __DRI_IMAGE_FORMAT_ABGR1555	PIPE_FORMAT_R5G5B5A1_UNORM
#define __DRI_IMAGE_FORMAT_XBGR1555	PIPE_FORMAT_R5G5B5X1_UNORM

__DRIimage *loader_dri_create_image(__DRIscreen *screen,
                                    const __DRIimageExtension *image,
                                    uint32_t width, uint32_t height,
                                    uint32_t dri_format, uint32_t dri_usage,
                                    const uint64_t *modifiers,
                                    unsigned int modifiers_count,
                                    void *loaderPrivate);

int dri_get_initial_swap_interval(__DRIscreen *driScreen,
                                  const __DRI2configQueryExtension *config);

bool dri_valid_swap_interval(__DRIscreen *driScreen,
                             const __DRI2configQueryExtension *config, int interval);

int
loader_image_format_to_fourcc(int format);

#ifdef HAVE_X11_PLATFORM
void
loader_init_screen_resources(struct loader_screen_resources *res,
                             xcb_connection_t *conn,
                             xcb_screen_t *screen);
bool
loader_update_screen_resources(struct loader_screen_resources *res);

void
loader_destroy_screen_resources(struct loader_screen_resources *res);
#endif

#endif /* LOADER_DRI_HELPER_H */
