/**************************************************************************
 *
 * Copyright 2009, VMware, Inc.
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
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Author: Keith Whitwell <keithw@vmware.com>
 * Author: Jakob Bornecrantz <wallbraker@gmail.com>
 */

#include "dri_screen.h"
#include "dri_context.h"
#include "dri_helpers.h"

#include "util/u_inlines.h"
#include "pipe/p_screen.h"
#include "util/format/u_formats.h"
#include "pipe-loader/pipe_loader.h"
#include "frontend/drm_driver.h"

#include "util/u_debug.h"
#include "util/u_driconf.h"
#include "util/format/u_format_s3tc.h"

#include "state_tracker/st_context.h"

#define MSAA_VISUAL_MAX_SAMPLES 32

#undef false

const __DRIconfigOptionsExtension gallium_config_options = {
   .base = { __DRI_CONFIG_OPTIONS, 2 },
   .getXml = pipe_loader_get_driinfo_xml
};

#define false 0

void
dri_init_options(struct dri_screen *screen)
{
   pipe_loader_config_options(screen->dev);

   struct st_config_options *options = &screen->options;
   const struct driOptionCache *optionCache = &screen->dev->option_cache;

   u_driconf_fill_st_options(options, optionCache);
}

static unsigned
dri_loader_get_cap(struct dri_screen *screen, enum dri_loader_cap cap)
{
   const __DRIdri2LoaderExtension *dri2_loader = screen->dri2.loader;
   const __DRIimageLoaderExtension *image_loader = screen->image.loader;

   if (dri2_loader && dri2_loader->base.version >= 4 &&
       dri2_loader->getCapability)
      return dri2_loader->getCapability(screen->loaderPrivate, cap);

   if (image_loader && image_loader->base.version >= 2 &&
       image_loader->getCapability)
      return image_loader->getCapability(screen->loaderPrivate, cap);

   return 0;
}

/**
 * Creates a set of \c struct gl_config that a driver will expose.
 *
 * A set of \c struct gl_config will be created based on the supplied
 * parameters.  The number of modes processed will be 2 *
 * \c num_depth_stencil_bits * \c num_db_modes.
 *
 * For the most part, data is just copied from \c depth_bits, \c stencil_bits,
 * \c db_modes, and \c visType into each \c struct gl_config element.
 * However, the meanings of \c fb_format and \c fb_type require further
 * explanation.  The \c fb_format specifies which color components are in
 * each pixel and what the default order is.  For example, \c GL_RGB specifies
 * that red, green, blue are available and red is in the "most significant"
 * position and blue is in the "least significant".  The \c fb_type specifies
 * the bit sizes of each component and the actual ordering.  For example, if
 * \c GL_UNSIGNED_SHORT_5_6_5_REV is specified with \c GL_RGB, bits [15:11]
 * are the blue value, bits [10:5] are the green value, and bits [4:0] are
 * the red value.
 *
 * One sublte issue is the combination of \c GL_RGB  or \c GL_BGR and either
 * of the \c GL_UNSIGNED_INT_8_8_8_8 modes.  The resulting mask values in the
 * \c struct gl_config structure is \b identical to the \c GL_RGBA or
 * \c GL_BGRA case, except the \c alphaMask is zero.  This means that, as
 * far as this routine is concerned, \c GL_RGB with \c GL_UNSIGNED_INT_8_8_8_8
 * still uses 32-bits.
 *
 * If in doubt, look at the tables used in the function.
 *
 * \param ptr_to_modes  Pointer to a pointer to a linked list of
 *                      \c struct gl_config.  Upon completion, a pointer to
 *                      the next element to be process will be stored here.
 *                      If the function fails and returns \c GL_FALSE, this
 *                      value will be unmodified, but some elements in the
 *                      linked list may be modified.
 * \param format        Mesa mesa_format enum describing the pixel format
 * \param zs_formats    Array of depth/stencil formats to expose
 * \param num_zs_formats Number of entries in \c depth_stencil_formats.
 * \param db_modes      Array of buffer swap modes.
 * \param num_db_modes  Number of entries in \c db_modes.
 * \param msaa_samples  Array of msaa sample count. 0 represents a visual
 *                      without a multisample buffer.
 * \param num_msaa_modes Number of entries in \c msaa_samples.
 * \param enable_accum  Add an accum buffer to the configs
 * \param color_depth_match Whether the color depth must match the zs depth
 *                          This forces 32-bit color to have 24-bit depth, and
 *                          16-bit color to have 16-bit depth.
 *
 * \returns
 * Pointer to any array of pointers to the \c __DRIconfig structures created
 * for the specified formats.  If there is an error, \c NULL is returned.
 * Currently the only cause of failure is a bad parameter (i.e., unsupported
 * \c format).
 */
static __DRIconfig **
driCreateConfigs(enum pipe_format format,
                 enum pipe_format *zs_formats, unsigned num_zs_formats,
                 const bool *db_modes, unsigned num_db_modes,
                 const uint8_t * msaa_samples, unsigned num_msaa_modes,
                 GLboolean enable_accum, GLboolean color_depth_match)
{
   uint32_t masks[4];
   int shifts[4];
   int color_bits[4];
   __DRIconfig **configs, **c;
   struct gl_config *modes;
   unsigned i, j, k, h;
   unsigned num_modes;
   unsigned num_accum_bits = (enable_accum) ? 2 : 1;
   bool is_srgb;
   bool is_float;

   is_srgb = util_format_is_srgb(format);
   is_float = util_format_is_float(format);

   for (i = 0; i < 4; i++) {
      color_bits[i] =
         util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, i);

      if (color_bits[i] > 0) {
         shifts[i] =
            util_format_get_component_shift(format, UTIL_FORMAT_COLORSPACE_RGB, i);
      } else {
         shifts[i] = -1;
      }

      if (is_float || color_bits[i] == 0)
         masks[i] = 0;
      else
         masks[i] = ((1 << color_bits[i]) - 1) << shifts[i];
   }

   num_modes = num_zs_formats * num_db_modes * num_accum_bits * num_msaa_modes;
   configs = calloc(num_modes + 1, sizeof *configs);
   if (configs == NULL)
       return NULL;

    c = configs;
    for ( k = 0 ; k < num_zs_formats ; k++ ) {
        unsigned depth_bits, stencil_bits;

        if (zs_formats[k] != PIPE_FORMAT_NONE) {
           depth_bits =
              util_format_get_component_bits(zs_formats[k],
                                          UTIL_FORMAT_COLORSPACE_ZS, 0);
           stencil_bits =
              util_format_get_component_bits(zs_formats[k],
                                          UTIL_FORMAT_COLORSPACE_ZS, 1);
        } else {
           depth_bits = 0;
           stencil_bits = 0;
        }

        for ( i = 0 ; i < num_db_modes ; i++ ) {
            for ( h = 0 ; h < num_msaa_modes; h++ ) {
                for ( j = 0 ; j < num_accum_bits ; j++ ) {
                    if (color_depth_match &&
                        (depth_bits || stencil_bits)) {
                        /* Depth can really only be 0, 16, 24, or 32. A 32-bit
                         * color format still matches 24-bit depth, as there
                         * is an implicit 8-bit stencil. So really we just
                         * need to make sure that color/depth are both 16 or
                         * both non-16.
                         */
                        if ((depth_bits + stencil_bits == 16) !=
                            (color_bits[0] + color_bits[1] +
                             color_bits[2] + color_bits[3] == 16))
                            continue;
                    }

                    *c = malloc (sizeof **c);
                    modes = &(*c)->modes;
                    c++;

                    memset(modes, 0, sizeof *modes);
                    modes->color_format = format;
                    modes->zs_format = zs_formats[k];
                    if (j > 0)
                       modes->accum_format = PIPE_FORMAT_R16G16B16A16_SNORM;
                    else
                       modes->accum_format = PIPE_FORMAT_NONE;

                    modes->floatMode  = is_float;
                    modes->redBits    = color_bits[0];
                    modes->redShift   = shifts[0];
                    modes->redMask    = masks[0];
                    modes->greenBits  = color_bits[1];
                    modes->greenShift = shifts[1];
                    modes->greenMask  = masks[1];
                    modes->blueBits   = color_bits[2];
                    modes->blueShift  = shifts[2];
                    modes->blueMask   = masks[2];
                    modes->alphaBits  = color_bits[3];
                    modes->alphaMask  = masks[3];
                    modes->alphaShift = shifts[3];
                    modes->rgbBits   = modes->redBits + modes->greenBits
                            + modes->blueBits + modes->alphaBits;

                    modes->accumRedBits   = 16 * j;
                    modes->accumGreenBits = 16 * j;
                    modes->accumBlueBits  = 16 * j;
                    modes->accumAlphaBits = 16 * j;

                    modes->stencilBits = stencil_bits;
                    modes->depthBits = depth_bits;

                    modes->doubleBufferMode = db_modes[i];

                    modes->samples = msaa_samples[h];

                    modes->sRGBCapable = is_srgb;
                }
            }
        }
    }
    *c = NULL;

    return configs;
}

static __DRIconfig **
driConcatConfigs(__DRIconfig **a, __DRIconfig **b)
{
    __DRIconfig **all;
    int i, j, index;

    if (a == NULL || a[0] == NULL)
       return b;
    else if (b == NULL || b[0] == NULL)
       return a;

    i = 0;
    while (a[i] != NULL)
        i++;
    j = 0;
    while (b[j] != NULL)
        j++;

    all = malloc((i + j + 1) * sizeof *all);
    index = 0;
    for (i = 0; a[i] != NULL; i++)
        all[index++] = a[i];
    for (j = 0; b[j] != NULL; j++)
        all[index++] = b[j];
    all[index++] = NULL;

    free(a);
    free(b);

    return all;
}


static const __DRIconfig **
dri_fill_in_modes(struct dri_screen *screen)
{
   /* The 32-bit RGBA format must not precede the 32-bit BGRA format.
    * Likewise for RGBX and BGRX.  Otherwise, the GLX client and the GLX
    * server may disagree on which format the GLXFBConfig represents,
    * resulting in swapped color channels.
    *
    * The problem, as of 2017-05-30:
    * When matching a GLXFBConfig to a __DRIconfig, GLX ignores the channel
    * order and chooses the first __DRIconfig with the expected channel
    * sizes. Specifically, GLX compares the GLXFBConfig's and __DRIconfig's
    * __DRI_ATTRIB_{CHANNEL}_SIZE but ignores __DRI_ATTRIB_{CHANNEL}_MASK.
    *
    * EGL does not suffer from this problem. It correctly compares the
    * channel masks when matching EGLConfig to __DRIconfig.
    */
   static const enum pipe_format pipe_formats[] = {
      PIPE_FORMAT_B10G10R10A2_UNORM,
      PIPE_FORMAT_B10G10R10X2_UNORM,
      PIPE_FORMAT_R10G10B10A2_UNORM,
      PIPE_FORMAT_R10G10B10X2_UNORM,
      PIPE_FORMAT_BGRA8888_UNORM,
      PIPE_FORMAT_BGRX8888_UNORM,
      PIPE_FORMAT_BGRA8888_SRGB,
      PIPE_FORMAT_BGRX8888_SRGB,
      PIPE_FORMAT_B5G6R5_UNORM,
      PIPE_FORMAT_R16G16B16A16_FLOAT,
      PIPE_FORMAT_R16G16B16X16_FLOAT,
      PIPE_FORMAT_RGBA8888_UNORM,
      PIPE_FORMAT_RGBX8888_UNORM,
      PIPE_FORMAT_RGBA8888_SRGB,
      PIPE_FORMAT_RGBX8888_SRGB,
      PIPE_FORMAT_B5G5R5A1_UNORM,
      PIPE_FORMAT_R5G5B5A1_UNORM,
      PIPE_FORMAT_B4G4R4A4_UNORM,
      PIPE_FORMAT_R4G4B4A4_UNORM,
   };
   __DRIconfig **configs = NULL;
   enum pipe_format zs_formats[5];
   unsigned num_zs_formats = 0;
   unsigned i;
   struct pipe_screen *p_screen = screen->base.screen;
   bool mixed_color_depth;
   bool allow_rgba_ordering;
   bool allow_rgb10;
   bool allow_fp16;

   static const bool db_modes[] = { false, true };

   if (!driQueryOptionb(&screen->dev->option_cache, "always_have_depth_buffer"))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_NONE;

   allow_rgba_ordering = dri_loader_get_cap(screen, DRI_LOADER_CAP_RGBA_ORDERING);
   allow_rgb10 = driQueryOptionb(&screen->dev->option_cache, "allow_rgb10_configs");
   allow_fp16 = dri_loader_get_cap(screen, DRI_LOADER_CAP_FP16);

#define HAS_ZS(fmt) \
   p_screen->is_format_supported(p_screen, PIPE_FORMAT_##fmt, \
                                 PIPE_TEXTURE_2D, 0, 0, \
                                 PIPE_BIND_DEPTH_STENCIL)

   if (HAS_ZS(Z16_UNORM))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_Z16_UNORM;

   if (HAS_ZS(Z24X8_UNORM))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_Z24X8_UNORM;
   else if (HAS_ZS(X8Z24_UNORM))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_X8Z24_UNORM;

   if (HAS_ZS(Z24_UNORM_S8_UINT))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_Z24_UNORM_S8_UINT;
   else if (HAS_ZS(S8_UINT_Z24_UNORM))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_S8_UINT_Z24_UNORM;

   if (HAS_ZS(Z32_UNORM))
      zs_formats[num_zs_formats++] = PIPE_FORMAT_Z32_UNORM;

#undef HAS_ZS

   mixed_color_depth =
      p_screen->get_param(p_screen, PIPE_CAP_MIXED_COLOR_DEPTH_BITS);

   /* Add configs. */
   for (unsigned f = 0; f < ARRAY_SIZE(pipe_formats); f++) {
      __DRIconfig **new_configs = NULL;
      unsigned num_msaa_modes = 0; /* includes a single-sample mode */
      uint8_t msaa_modes[MSAA_VISUAL_MAX_SAMPLES];

      /* Expose only BGRA ordering if the loader doesn't support RGBA ordering. */
      if (!allow_rgba_ordering &&
          (pipe_formats[f] == PIPE_FORMAT_RGBA8888_UNORM ||
           pipe_formats[f] == PIPE_FORMAT_RGBX8888_UNORM ||
           pipe_formats[f] == PIPE_FORMAT_RGBA8888_SRGB  ||
           pipe_formats[f] == PIPE_FORMAT_RGBX8888_SRGB  ||
           pipe_formats[f] == PIPE_FORMAT_R5G5B5A1_UNORM ||
           pipe_formats[f] == PIPE_FORMAT_R5G5B5X1_UNORM ||
           pipe_formats[f] == PIPE_FORMAT_R4G4B4A4_UNORM ||
           pipe_formats[f] == PIPE_FORMAT_R4G4B4X4_UNORM))
         continue;

      if (!allow_rgb10 &&
          util_format_get_component_bits(pipe_formats[f],
                                         UTIL_FORMAT_COLORSPACE_RGB, 0) == 10 &&
          util_format_get_component_bits(pipe_formats[f],
                                         UTIL_FORMAT_COLORSPACE_RGB, 1) == 10 &&
          util_format_get_component_bits(pipe_formats[f],
                                         UTIL_FORMAT_COLORSPACE_RGB, 2) == 10)
         continue;

      if (!allow_fp16 && util_format_is_float(pipe_formats[f]))
         continue;

      if (!p_screen->is_format_supported(p_screen, pipe_formats[f],
                                         PIPE_TEXTURE_2D, 0, 0,
                                         PIPE_BIND_RENDER_TARGET |
                                         PIPE_BIND_DISPLAY_TARGET))
         continue;

      for (i = 1; i <= MSAA_VISUAL_MAX_SAMPLES; i++) {
         int samples = i > 1 ? i : 0;

         if (p_screen->is_format_supported(p_screen, pipe_formats[f],
                                           PIPE_TEXTURE_2D, samples, samples,
                                           PIPE_BIND_RENDER_TARGET)) {
            msaa_modes[num_msaa_modes++] = samples;
         }
      }

      if (num_msaa_modes) {
         /* Single-sample configs with an accumulation buffer. */
         new_configs = driCreateConfigs(pipe_formats[f],
                                        zs_formats, num_zs_formats,
                                        db_modes, ARRAY_SIZE(db_modes),
                                        msaa_modes, 1,
                                        GL_TRUE, !mixed_color_depth);
         configs = driConcatConfigs(configs, new_configs);

         /* Multi-sample configs without an accumulation buffer. */
         if (num_msaa_modes > 1) {
            new_configs = driCreateConfigs(pipe_formats[f],
                                           zs_formats, num_zs_formats,
                                           db_modes, ARRAY_SIZE(db_modes),
                                           msaa_modes+1, num_msaa_modes-1,
                                           GL_FALSE, !mixed_color_depth);
            configs = driConcatConfigs(configs, new_configs);
         }
      }
   }

   if (configs == NULL) {
      debug_printf("%s: driCreateConfigs failed\n", __func__);
      return NULL;
   }

   return (const __DRIconfig **)configs;
}

/**
 * Roughly the converse of dri_fill_in_modes.
 */
void
dri_fill_st_visual(struct st_visual *stvis,
                   const struct dri_screen *screen,
                   const struct gl_config *mode)
{
   memset(stvis, 0, sizeof(*stvis));

   if (!mode)
      return;

   assert(mode->color_format != PIPE_FORMAT_NONE);
   stvis->color_format = mode->color_format;
   stvis->accum_format = mode->accum_format;
   stvis->depth_stencil_format = mode->zs_format;

   if (mode->samples > 0) {
      if (debug_get_bool_option("DRI_NO_MSAA", false))
         stvis->samples = 0;
      else
         stvis->samples = mode->samples;
   }

   stvis->buffer_mask |= ST_ATTACHMENT_FRONT_LEFT_MASK;
   if (mode->doubleBufferMode) {
      stvis->buffer_mask |= ST_ATTACHMENT_BACK_LEFT_MASK;
   }
   if (mode->stereoMode) {
      stvis->buffer_mask |= ST_ATTACHMENT_FRONT_RIGHT_MASK;
      if (mode->doubleBufferMode)
         stvis->buffer_mask |= ST_ATTACHMENT_BACK_RIGHT_MASK;
   }

   if (mode->depthBits > 0 || mode->stencilBits > 0)
      stvis->buffer_mask |= ST_ATTACHMENT_DEPTH_STENCIL_MASK;
   /* let the gallium frontend allocate the accum buffer */
}

static bool
dri_get_egl_image(struct pipe_frontend_screen *fscreen,
                  void *egl_image,
                  struct st_egl_image *stimg)
{
   struct dri_screen *screen = (struct dri_screen *)fscreen;
   __DRIimage *img = NULL;
   const struct dri2_format_mapping *map;

   if (screen->lookup_egl_image_validated) {
      img = screen->lookup_egl_image_validated(screen, egl_image);
   } else if (screen->lookup_egl_image) {
      img = screen->lookup_egl_image(screen, egl_image);
   }

   if (!img)
      return false;

   stimg->texture = NULL;
   pipe_resource_reference(&stimg->texture, img->texture);
   map = dri2_get_mapping_by_fourcc(img->dri_fourcc);
   stimg->format = map ? map->pipe_format : img->texture->format;
   stimg->level = img->level;
   stimg->layer = img->layer;
   stimg->imported_dmabuf = img->imported_dmabuf;

   if (img->imported_dmabuf && map) {
      /* Guess sized internal format for dma-bufs. Could be used
       * by EXT_EGL_image_storage.
       */
      mesa_format mesa_format = driImageFormatToGLFormat(map->dri_format);
      stimg->internalformat = driGLFormatToSizedInternalGLFormat(mesa_format);
   } else {
      stimg->internalformat = img->internal_format;
   }

   stimg->yuv_color_space = img->yuv_color_space;
   stimg->yuv_range = img->sample_range;

   return true;
}

static bool
dri_validate_egl_image(struct pipe_frontend_screen *fscreen,
                       void *egl_image)
{
   struct dri_screen *screen = (struct dri_screen *)fscreen;

   return screen->validate_egl_image(screen, egl_image);
}

static int
dri_get_param(struct pipe_frontend_screen *fscreen,
              enum st_manager_param param)
{
   return 0;
}

void
dri_release_screen(struct dri_screen * screen)
{
   st_screen_destroy(&screen->base);

   if (screen->base.screen) {
      screen->base.screen->destroy(screen->base.screen);
      screen->base.screen = NULL;
   }

   if (screen->dev) {
      pipe_loader_release(&screen->dev, 1);
      screen->dev = NULL;
   }

   mtx_destroy(&screen->opencl_func_mutex);
}

void
dri_destroy_screen(struct dri_screen *screen)
{
   dri_release_screen(screen);

   free(screen->options.force_gl_vendor);
   free(screen->options.force_gl_renderer);
   free(screen->options.mesa_extension_override);

   driDestroyOptionCache(&screen->optionCache);
   driDestroyOptionInfo(&screen->optionInfo);

   /* The caller in dri_util preserves the fd ownership */
   free(screen);
}

static void
dri_postprocessing_init(struct dri_screen *screen)
{
   unsigned i;

   for (i = 0; i < PP_FILTERS; i++) {
      screen->pp_enabled[i] = driQueryOptioni(&screen->dev->option_cache,
                                              pp_filters[i].name);
   }
}

static void
dri_set_background_context(struct st_context *st,
                           struct util_queue_monitoring *queue_info)
{
   struct dri_context *ctx = (struct dri_context *)st->frontend_context;
   const __DRIbackgroundCallableExtension *backgroundCallable =
      ctx->screen->dri2.backgroundCallable;

   if (backgroundCallable)
      backgroundCallable->setBackgroundContext(ctx->loaderPrivate);

   if (ctx->hud)
      hud_add_queue_for_monitoring(ctx->hud, queue_info);
}

const __DRIconfig **
dri_init_screen(struct dri_screen *screen,
                struct pipe_screen *pscreen)
{
   screen->base.screen = pscreen;
   screen->base.get_egl_image = dri_get_egl_image;
   screen->base.get_param = dri_get_param;
   screen->base.set_background_context = dri_set_background_context;

   if (screen->validate_egl_image)
      screen->base.validate_egl_image = dri_validate_egl_image;

   if (pscreen->get_param(pscreen, PIPE_CAP_NPOT_TEXTURES))
      screen->target = PIPE_TEXTURE_2D;
   else
      screen->target = PIPE_TEXTURE_RECT;

   dri_postprocessing_init(screen);

   st_api_query_versions(&screen->base,
                         &screen->options,
                         &screen->max_gl_core_version,
                         &screen->max_gl_compat_version,
                         &screen->max_gl_es1_version,
                         &screen->max_gl_es2_version);

   return dri_fill_in_modes(screen);
}

/* vim: set sw=3 ts=8 sts=3 expandtab: */
