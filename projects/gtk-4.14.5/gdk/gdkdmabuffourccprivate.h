#pragma once

#include "config.h"
#ifdef HAVE_DRM_FOURCC_H
#include <drm_fourcc.h>
#endif

#ifndef fourcc_code
#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
				 ((__u32)(c) << 16) | ((__u32)(d) << 24))
#endif

#ifndef DRM_FORMAT_C1
#define DRM_FORMAT_C1		fourcc_code('C', '1', ' ', ' ')
#endif
#ifndef DRM_FORMAT_C2
#define DRM_FORMAT_C2		fourcc_code('C', '2', ' ', ' ')
#endif
#ifndef DRM_FORMAT_C4
#define DRM_FORMAT_C4		fourcc_code('C', '4', ' ', ' ')
#endif
#ifndef DRM_FORMAT_C8
#define DRM_FORMAT_C8		fourcc_code('C', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_D1
#define DRM_FORMAT_D1		fourcc_code('D', '1', ' ', ' ')
#endif
#ifndef DRM_FORMAT_D2
#define DRM_FORMAT_D2		fourcc_code('D', '2', ' ', ' ')
#endif
#ifndef DRM_FORMAT_D4
#define DRM_FORMAT_D4		fourcc_code('D', '4', ' ', ' ')
#endif
#ifndef DRM_FORMAT_D8
#define DRM_FORMAT_D8		fourcc_code('D', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R1
#define DRM_FORMAT_R1		fourcc_code('R', '1', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R2
#define DRM_FORMAT_R2		fourcc_code('R', '2', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R4
#define DRM_FORMAT_R4		fourcc_code('R', '4', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8		fourcc_code('R', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_R10
#define DRM_FORMAT_R10		fourcc_code('R', '1', '0', ' ')
#endif
#ifndef DRM_FORMAT_R12
#define DRM_FORMAT_R12		fourcc_code('R', '1', '2', ' ')
#endif
#ifndef DRM_FORMAT_R16
#define DRM_FORMAT_R16		fourcc_code('R', '1', '6', ' ')
#endif
#ifndef DRM_FORMAT_RG88
#define DRM_FORMAT_RG88		fourcc_code('R', 'G', '8', '8')
#endif
#ifndef DRM_FORMAT_GR88
#define DRM_FORMAT_GR88		fourcc_code('G', 'R', '8', '8')
#endif
#ifndef DRM_FORMAT_RG1616
#define DRM_FORMAT_RG1616	fourcc_code('R', 'G', '3', '2')
#endif
#ifndef DRM_FORMAT_GR1616
#define DRM_FORMAT_GR1616	fourcc_code('G', 'R', '3', '2')
#endif
#ifndef DRM_FORMAT_RGB332
#define DRM_FORMAT_RGB332	fourcc_code('R', 'G', 'B', '8')
#endif
#ifndef DRM_FORMAT_BGR233
#define DRM_FORMAT_BGR233	fourcc_code('B', 'G', 'R', '8')
#endif
#ifndef DRM_FORMAT_XRGB4444
#define DRM_FORMAT_XRGB4444	fourcc_code('X', 'R', '1', '2')
#endif
#ifndef DRM_FORMAT_XBGR4444
#define DRM_FORMAT_XBGR4444	fourcc_code('X', 'B', '1', '2')
#endif
#ifndef DRM_FORMAT_RGBX4444
#define DRM_FORMAT_RGBX4444	fourcc_code('R', 'X', '1', '2')
#endif
#ifndef DRM_FORMAT_BGRX4444
#define DRM_FORMAT_BGRX4444	fourcc_code('B', 'X', '1', '2')
#endif
#ifndef DRM_FORMAT_ARGB4444
#define DRM_FORMAT_ARGB4444	fourcc_code('A', 'R', '1', '2')
#endif
#ifndef DRM_FORMAT_ABGR4444
#define DRM_FORMAT_ABGR4444	fourcc_code('A', 'B', '1', '2')
#endif
#ifndef DRM_FORMAT_RGBA4444
#define DRM_FORMAT_RGBA4444	fourcc_code('R', 'A', '1', '2')
#endif
#ifndef DRM_FORMAT_BGRA4444
#define DRM_FORMAT_BGRA4444	fourcc_code('B', 'A', '1', '2')
#endif
#ifndef DRM_FORMAT_XRGB1555
#define DRM_FORMAT_XRGB1555	fourcc_code('X', 'R', '1', '5')
#endif
#ifndef DRM_FORMAT_XBGR1555
#define DRM_FORMAT_XBGR1555	fourcc_code('X', 'B', '1', '5')
#endif
#ifndef DRM_FORMAT_RGBX5551
#define DRM_FORMAT_RGBX5551	fourcc_code('R', 'X', '1', '5')
#endif
#ifndef DRM_FORMAT_BGRX5551
#define DRM_FORMAT_BGRX5551	fourcc_code('B', 'X', '1', '5')
#endif
#ifndef DRM_FORMAT_ARGB1555
#define DRM_FORMAT_ARGB1555	fourcc_code('A', 'R', '1', '5')
#endif
#ifndef DRM_FORMAT_ABGR1555
#define DRM_FORMAT_ABGR1555	fourcc_code('A', 'B', '1', '5')
#endif
#ifndef DRM_FORMAT_RGBA5551
#define DRM_FORMAT_RGBA5551	fourcc_code('R', 'A', '1', '5')
#endif
#ifndef DRM_FORMAT_BGRA5551
#define DRM_FORMAT_BGRA5551	fourcc_code('B', 'A', '1', '5')
#endif
#ifndef DRM_FORMAT_RGB565
#define DRM_FORMAT_RGB565	fourcc_code('R', 'G', '1', '6')
#endif
#ifndef DRM_FORMAT_BGR565
#define DRM_FORMAT_BGR565	fourcc_code('B', 'G', '1', '6')
#endif
#ifndef DRM_FORMAT_RGB888
#define DRM_FORMAT_RGB888	fourcc_code('R', 'G', '2', '4')
#endif
#ifndef DRM_FORMAT_BGR888
#define DRM_FORMAT_BGR888	fourcc_code('B', 'G', '2', '4')
#endif
#ifndef DRM_FORMAT_XRGB8888
#define DRM_FORMAT_XRGB8888	fourcc_code('X', 'R', '2', '4')
#endif
#ifndef DRM_FORMAT_XBGR8888
#define DRM_FORMAT_XBGR8888	fourcc_code('X', 'B', '2', '4')
#endif
#ifndef DRM_FORMAT_RGBX8888
#define DRM_FORMAT_RGBX8888	fourcc_code('R', 'X', '2', '4')
#endif
#ifndef DRM_FORMAT_BGRX8888
#define DRM_FORMAT_BGRX8888	fourcc_code('B', 'X', '2', '4')
#endif
#ifndef DRM_FORMAT_ARGB8888
#define DRM_FORMAT_ARGB8888	fourcc_code('A', 'R', '2', '4')
#endif
#ifndef DRM_FORMAT_ABGR8888
#define DRM_FORMAT_ABGR8888	fourcc_code('A', 'B', '2', '4')
#endif
#ifndef DRM_FORMAT_RGBA8888
#define DRM_FORMAT_RGBA8888	fourcc_code('R', 'A', '2', '4')
#endif
#ifndef DRM_FORMAT_BGRA8888
#define DRM_FORMAT_BGRA8888	fourcc_code('B', 'A', '2', '4')
#endif
#ifndef DRM_FORMAT_XRGB2101010
#define DRM_FORMAT_XRGB2101010	fourcc_code('X', 'R', '3', '0')
#endif
#ifndef DRM_FORMAT_XBGR2101010
#define DRM_FORMAT_XBGR2101010	fourcc_code('X', 'B', '3', '0')
#endif
#ifndef DRM_FORMAT_RGBX1010102
#define DRM_FORMAT_RGBX1010102	fourcc_code('R', 'X', '3', '0')
#endif
#ifndef DRM_FORMAT_BGRX1010102
#define DRM_FORMAT_BGRX1010102	fourcc_code('B', 'X', '3', '0')
#endif
#ifndef DRM_FORMAT_ARGB2101010
#define DRM_FORMAT_ARGB2101010	fourcc_code('A', 'R', '3', '0')
#endif
#ifndef DRM_FORMAT_ABGR2101010
#define DRM_FORMAT_ABGR2101010	fourcc_code('A', 'B', '3', '0')
#endif
#ifndef DRM_FORMAT_RGBA1010102
#define DRM_FORMAT_RGBA1010102	fourcc_code('R', 'A', '3', '0')
#endif
#ifndef DRM_FORMAT_BGRA1010102
#define DRM_FORMAT_BGRA1010102	fourcc_code('B', 'A', '3', '0')
#endif
#ifndef DRM_FORMAT_XRGB16161616
#define DRM_FORMAT_XRGB16161616	fourcc_code('X', 'R', '4', '8')
#endif
#ifndef DRM_FORMAT_XBGR16161616
#define DRM_FORMAT_XBGR16161616	fourcc_code('X', 'B', '4', '8')
#endif
#ifndef DRM_FORMAT_ARGB16161616
#define DRM_FORMAT_ARGB16161616	fourcc_code('A', 'R', '4', '8')
#endif
#ifndef DRM_FORMAT_ABGR16161616
#define DRM_FORMAT_ABGR16161616	fourcc_code('A', 'B', '4', '8')
#endif
#ifndef DRM_FORMAT_XRGB16161616F
#define DRM_FORMAT_XRGB16161616F fourcc_code('X', 'R', '4', 'H')
#endif
#ifndef DRM_FORMAT_XBGR16161616F
#define DRM_FORMAT_XBGR16161616F fourcc_code('X', 'B', '4', 'H')
#endif
#ifndef DRM_FORMAT_ARGB16161616F
#define DRM_FORMAT_ARGB16161616F fourcc_code('A', 'R', '4', 'H')
#endif
#ifndef DRM_FORMAT_ABGR16161616F
#define DRM_FORMAT_ABGR16161616F fourcc_code('A', 'B', '4', 'H')
#endif
#ifndef DRM_FORMAT_AXBXGXRX106106106106
#define DRM_FORMAT_AXBXGXRX106106106106 fourcc_code('A', 'B', '1', '0')
#endif
#ifndef DRM_FORMAT_YUYV
#define DRM_FORMAT_YUYV		fourcc_code('Y', 'U', 'Y', 'V')
#endif
#ifndef DRM_FORMAT_YVYU
#define DRM_FORMAT_YVYU		fourcc_code('Y', 'V', 'Y', 'U')
#endif
#ifndef DRM_FORMAT_UYVY
#define DRM_FORMAT_UYVY		fourcc_code('U', 'Y', 'V', 'Y')
#endif
#ifndef DRM_FORMAT_VYUY
#define DRM_FORMAT_VYUY		fourcc_code('V', 'Y', 'U', 'Y')
#endif
#ifndef DRM_FORMAT_AYUV
#define DRM_FORMAT_AYUV		fourcc_code('A', 'Y', 'U', 'V')
#endif
#ifndef DRM_FORMAT_AVUY8888
#define DRM_FORMAT_AVUY8888	fourcc_code('A', 'V', 'U', 'Y')
#endif
#ifndef DRM_FORMAT_XYUV8888
#define DRM_FORMAT_XYUV8888	fourcc_code('X', 'Y', 'U', 'V')
#endif
#ifndef DRM_FORMAT_XVUY8888
#define DRM_FORMAT_XVUY8888	fourcc_code('X', 'V', 'U', 'Y')
#endif
#ifndef DRM_FORMAT_VUY888
#define DRM_FORMAT_VUY888	fourcc_code('V', 'U', '2', '4')
#endif
#ifndef DRM_FORMAT_VUY101010
#define DRM_FORMAT_VUY101010	fourcc_code('V', 'U', '3', '0')
#endif
#ifndef DRM_FORMAT_Y210
#define DRM_FORMAT_Y210         fourcc_code('Y', '2', '1', '0')
#endif
#ifndef DRM_FORMAT_Y212
#define DRM_FORMAT_Y212         fourcc_code('Y', '2', '1', '2')
#endif
#ifndef DRM_FORMAT_Y216
#define DRM_FORMAT_Y216         fourcc_code('Y', '2', '1', '6')
#endif
#ifndef DRM_FORMAT_Y410
#define DRM_FORMAT_Y410         fourcc_code('Y', '4', '1', '0')
#endif
#ifndef DRM_FORMAT_Y412
#define DRM_FORMAT_Y412         fourcc_code('Y', '4', '1', '2')
#endif
#ifndef DRM_FORMAT_Y416
#define DRM_FORMAT_Y416         fourcc_code('Y', '4', '1', '6')
#endif
#ifndef DRM_FORMAT_XVYU2101010
#define DRM_FORMAT_XVYU2101010	fourcc_code('X', 'V', '3', '0')
#endif
#ifndef DRM_FORMAT_XVYU12_16161616
#define DRM_FORMAT_XVYU12_16161616	fourcc_code('X', 'V', '3', '6')
#endif
#ifndef DRM_FORMAT_XVYU16161616
#define DRM_FORMAT_XVYU16161616	fourcc_code('X', 'V', '4', '8')
#endif
#ifndef DRM_FORMAT_Y0L0
#define DRM_FORMAT_Y0L0		fourcc_code('Y', '0', 'L', '0')
#endif
#ifndef DRM_FORMAT_X0L0
#define DRM_FORMAT_X0L0		fourcc_code('X', '0', 'L', '0')
#endif
#ifndef DRM_FORMAT_Y0L2
#define DRM_FORMAT_Y0L2		fourcc_code('Y', '0', 'L', '2')
#endif
#ifndef DRM_FORMAT_X0L2
#define DRM_FORMAT_X0L2		fourcc_code('X', '0', 'L', '2')
#endif
#ifndef DRM_FORMAT_YUV420_8BIT
#define DRM_FORMAT_YUV420_8BIT	fourcc_code('Y', 'U', '0', '8')
#endif
#ifndef DRM_FORMAT_YUV420_10BIT
#define DRM_FORMAT_YUV420_10BIT	fourcc_code('Y', 'U', '1', '0')
#endif
#ifndef DRM_FORMAT_XRGB8888_A8
#define DRM_FORMAT_XRGB8888_A8	fourcc_code('X', 'R', 'A', '8')
#endif
#ifndef DRM_FORMAT_XBGR8888_A8
#define DRM_FORMAT_XBGR8888_A8	fourcc_code('X', 'B', 'A', '8')
#endif
#ifndef DRM_FORMAT_RGBX8888_A8
#define DRM_FORMAT_RGBX8888_A8	fourcc_code('R', 'X', 'A', '8')
#endif
#ifndef DRM_FORMAT_BGRX8888_A8
#define DRM_FORMAT_BGRX8888_A8	fourcc_code('B', 'X', 'A', '8')
#endif
#ifndef DRM_FORMAT_RGB888_A8
#define DRM_FORMAT_RGB888_A8	fourcc_code('R', '8', 'A', '8')
#endif
#ifndef DRM_FORMAT_BGR888_A8
#define DRM_FORMAT_BGR888_A8	fourcc_code('B', '8', 'A', '8')
#endif
#ifndef DRM_FORMAT_RGB565_A8
#define DRM_FORMAT_RGB565_A8	fourcc_code('R', '5', 'A', '8')
#endif
#ifndef DRM_FORMAT_BGR565_A8
#define DRM_FORMAT_BGR565_A8	fourcc_code('B', '5', 'A', '8')
#endif
#ifndef DRM_FORMAT_NV12
#define DRM_FORMAT_NV12		fourcc_code('N', 'V', '1', '2')
#endif
#ifndef DRM_FORMAT_NV21
#define DRM_FORMAT_NV21		fourcc_code('N', 'V', '2', '1')
#endif
#ifndef DRM_FORMAT_NV16
#define DRM_FORMAT_NV16		fourcc_code('N', 'V', '1', '6')
#endif
#ifndef DRM_FORMAT_NV61
#define DRM_FORMAT_NV61		fourcc_code('N', 'V', '6', '1')
#endif
#ifndef DRM_FORMAT_NV24
#define DRM_FORMAT_NV24		fourcc_code('N', 'V', '2', '4')
#endif
#ifndef DRM_FORMAT_NV42
#define DRM_FORMAT_NV42		fourcc_code('N', 'V', '4', '2')
#endif
#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15		fourcc_code('N', 'V', '1', '5')
#endif
#ifndef DRM_FORMAT_P210
#define DRM_FORMAT_P210		fourcc_code('P', '2', '1', '0')
#endif
#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0')
#endif
#ifndef DRM_FORMAT_P012
#define DRM_FORMAT_P012		fourcc_code('P', '0', '1', '2')
#endif
#ifndef DRM_FORMAT_P016
#define DRM_FORMAT_P016		fourcc_code('P', '0', '1', '6')
#endif
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030		fourcc_code('P', '0', '3', '0')
#endif
#ifndef DRM_FORMAT_Q410
#define DRM_FORMAT_Q410		fourcc_code('Q', '4', '1', '0')
#endif
#ifndef DRM_FORMAT_Q401
#define DRM_FORMAT_Q401		fourcc_code('Q', '4', '0', '1')
#endif
#ifndef DRM_FORMAT_YUV410
#define DRM_FORMAT_YUV410	fourcc_code('Y', 'U', 'V', '9')
#endif
#ifndef DRM_FORMAT_YVU410
#define DRM_FORMAT_YVU410	fourcc_code('Y', 'V', 'U', '9')
#endif
#ifndef DRM_FORMAT_YUV411
#define DRM_FORMAT_YUV411	fourcc_code('Y', 'U', '1', '1')
#endif
#ifndef DRM_FORMAT_YVU411
#define DRM_FORMAT_YVU411	fourcc_code('Y', 'V', '1', '1')
#endif
#ifndef DRM_FORMAT_YUV420
#define DRM_FORMAT_YUV420	fourcc_code('Y', 'U', '1', '2')
#endif
#ifndef DRM_FORMAT_YVU420
#define DRM_FORMAT_YVU420	fourcc_code('Y', 'V', '1', '2')
#endif
#ifndef DRM_FORMAT_YUV422
#define DRM_FORMAT_YUV422	fourcc_code('Y', 'U', '1', '6')
#endif
#ifndef DRM_FORMAT_YVU422
#define DRM_FORMAT_YVU422	fourcc_code('Y', 'V', '1', '6')
#endif
#ifndef DRM_FORMAT_YUV444
#define DRM_FORMAT_YUV444	fourcc_code('Y', 'U', '2', '4')
#endif
#ifndef DRM_FORMAT_YVU444
#define DRM_FORMAT_YVU444	fourcc_code('Y', 'V', '2', '4')
#endif
