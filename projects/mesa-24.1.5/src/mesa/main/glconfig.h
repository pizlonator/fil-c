#ifndef __GL_CONFIG_H__
#define __GL_CONFIG_H__

#include "util/glheader.h"
#include "util/format/u_format.h"

/**
 * Framebuffer configuration (aka visual / pixelformat)
 * Note: some of these fields should be boolean, but it appears that
 * code in drivers/dri/common/util.c requires int-sized fields.
 */
struct gl_config
{
   /* if color_format is not PIPE_FORMAT_NONE, then all the properties of the
    * gl_config must match the pipe_formats; if it is PIPE_FORMAT_NONE then
    * the config properties are the only ones which can be trusted */
   enum pipe_format color_format;
   enum pipe_format zs_format;
   enum pipe_format accum_format;

   GLboolean floatMode;
   GLuint doubleBufferMode;
   GLuint stereoMode;

   GLint redBits, greenBits, blueBits, alphaBits;	/* bits per comp */
   GLuint redMask, greenMask, blueMask, alphaMask;
   GLint redShift, greenShift, blueShift, alphaShift;
   GLint rgbBits;		/* total bits for rgb */

   GLint accumRedBits, accumGreenBits, accumBlueBits, accumAlphaBits;
   GLint depthBits;
   GLint stencilBits;

   /* ARB_multisample / SGIS_multisample */
   GLuint samples;

   /* EXT_framebuffer_sRGB */
   GLint sRGBCapable;
};


#endif
