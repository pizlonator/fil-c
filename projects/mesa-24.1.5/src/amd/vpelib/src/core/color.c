/* Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */

#include "vpe_assert.h"
#include <string.h>
#include "color.h"
#include "color_gamma.h"
#include "color_cs.h"
#include "vpe_priv.h"
#include "color_gamut.h"
#include "common.h"
#include "custom_float.h"
#include "color_test_values.h"
#include "color_pwl.h"
#include "3dlut_builder.h"
#include "shaper_builder.h"

static void color_check_input_cm_update(struct vpe_priv *vpe_priv, struct stream_ctx *stream_ctx,
    const struct vpe_color_space *vcs, const struct vpe_color_adjust *adjustments,
    bool enable_3dlut);

static void color_check_output_cm_update(
    struct vpe_priv *vpe_priv, const struct vpe_color_space *vcs);

static bool color_update_regamma_tf(struct vpe_priv *vpe_priv,
    enum color_transfer_func output_transfer_function, struct fixed31_32 x_scale,
    struct fixed31_32 y_scale, struct fixed31_32 y_bias, bool can_bypass,
    struct transfer_func *output_tf);

static bool color_update_degamma_tf(struct vpe_priv *vpe_priv,
    enum color_transfer_func color_input_tf, struct fixed31_32 x_scale,
    struct fixed31_32 y_scale, struct fixed31_32 y_bias, bool can_bypass,
    struct transfer_func *input_tf);

static bool color_update_input_cs(struct vpe_priv *vpe_priv, enum color_space in_cs,
    const struct vpe_color_adjust *adjustments, struct vpe_csc_matrix *input_cs,
    struct vpe_color_adjust *stream_clr_adjustments, struct fixed31_32 *matrix_scaling_factor);

static bool is_ycbcr(enum color_space in_cs);

static void get_shaper_norm_factor(struct vpe_tonemap_params *tm_params,
    struct stream_ctx *stream_ctx, uint32_t *shaper_norm_factor)
{
    if (tm_params->shaper_tf == VPE_TF_PQ_NORMALIZED) {
        if (tm_params->input_pq_norm_factor == 0) {
            *shaper_norm_factor = stream_ctx->stream.hdr_metadata.max_mastering;
        } else {
            *shaper_norm_factor = tm_params->input_pq_norm_factor;
        }
    } else {
        *shaper_norm_factor = HDR_PEAK_WHITE;
    }
}

static bool is_ycbcr(enum color_space in_cs)
{
    if ((in_cs == COLOR_SPACE_YCBCR601) || (in_cs == COLOR_SPACE_YCBCR601_LIMITED) ||
        (in_cs == COLOR_SPACE_YCBCR709) || (in_cs == COLOR_SPACE_YCBCR709_LIMITED) ||
        (in_cs == COLOR_SPACE_2020_YCBCR) || (in_cs == COLOR_SPACE_2020_YCBCR_LIMITED)) {
        return true;
    }
    return false;
}

static void color_check_output_cm_update(
    struct vpe_priv *vpe_priv, const struct vpe_color_space *vcs)
{
    enum color_space         cs;
    enum color_transfer_func tf;

    vpe_color_get_color_space_and_tf(vcs, &cs, &tf);

    if (cs == COLOR_SPACE_UNKNOWN || tf == TRANSFER_FUNC_UNKNOWN)
        VPE_ASSERT(0);

    if (cs != vpe_priv->output_ctx.cs) {
        vpe_priv->output_ctx.dirty_bits.color_space = 1;
        vpe_priv->output_ctx.cs                     = cs;
    } else {
        vpe_priv->output_ctx.dirty_bits.color_space = 0;
    }

    if (tf != vpe_priv->output_ctx.tf) {
        vpe_priv->output_ctx.dirty_bits.transfer_function = 1;
        vpe_priv->output_ctx.tf                           = tf;
    } else {
        vpe_priv->output_ctx.dirty_bits.transfer_function = 0;
    }
}

static void color_check_input_cm_update(struct vpe_priv *vpe_priv, struct stream_ctx *stream_ctx,
    const struct vpe_color_space *vcs, const struct vpe_color_adjust *adjustments,
    bool enable_3dlut)
{
    enum color_space         cs;
    enum color_transfer_func tf;

    vpe_color_get_color_space_and_tf(vcs, &cs, &tf);
    /*
     * Bias and Scale already does full->limited range conversion.
     * Hence, the ICSC matrix should always be full range
     */
    vpe_convert_full_range_color_enum(&cs);

    if (cs == COLOR_SPACE_UNKNOWN && tf == TRANSFER_FUNC_UNKNOWN)
        VPE_ASSERT(0);

    if (cs != stream_ctx->cs || enable_3dlut != stream_ctx->enable_3dlut) {
        stream_ctx->dirty_bits.color_space = 1;
        stream_ctx->cs                     = cs;
    } else {
        stream_ctx->dirty_bits.color_space = 0;
        if (adjustments) {
            if (vpe_color_different_color_adjusts(
                    adjustments, &stream_ctx->color_adjustments)) // the new stream has different
                                                                  // color adjustments params
                stream_ctx->dirty_bits.color_space = 1;
        }
    }
    // if the new transfer function is different than the old one or the scaling factor is not one
    //  any new stream will start with a transfer function which is not scaled
    if (tf != stream_ctx->tf || enable_3dlut != stream_ctx->enable_3dlut) {
        stream_ctx->dirty_bits.transfer_function = 1;
        stream_ctx->tf                           = tf;
    } else {
        stream_ctx->dirty_bits.transfer_function = 0;
    }

    stream_ctx->enable_3dlut = enable_3dlut;
}

static bool color_update_regamma_tf(struct vpe_priv *vpe_priv,
    enum color_transfer_func output_transfer_function, struct fixed31_32 x_scale,
    struct fixed31_32 y_scale, struct fixed31_32 y_bias, bool can_bypass,
    struct transfer_func* output_tf)
{
    struct pwl_params *params      = NULL;
   
    if (can_bypass) {
        output_tf->type = TF_TYPE_BYPASS;
        return true;
    }

    output_tf->sdr_ref_white_level = 80;
    output_tf->cm_gamma_type = CM_REGAM;
    output_tf->type = TF_TYPE_DISTRIBUTED_POINTS;
    output_tf->start_base = y_bias;

    switch (output_transfer_function) {
    case TRANSFER_FUNC_SRGB:
    case TRANSFER_FUNC_BT709:
    case TRANSFER_FUNC_BT1886:
    case TRANSFER_FUNC_PQ2084:
    case TRANSFER_FUNC_LINEAR_0_125:
    case TRANSFER_FUNC_LINEAR_0_1:
        output_tf->tf = output_transfer_function;
        break;
    default:
        VPE_ASSERT(0);
        break;
    }

    if (!vpe_priv->init.debug.force_tf_calculation &&
        x_scale.value == vpe_fixpt_one.value &&
        y_scale.value == vpe_fixpt_one.value &&
        y_bias.value == vpe_fixpt_zero.value)
        vpe_priv->resource.get_tf_pwl_params(output_tf, &params, CM_REGAM);

    if (params)
        output_tf->use_pre_calculated_table = true;
    else
        output_tf->use_pre_calculated_table = false;

    if (!output_tf->use_pre_calculated_table)
        vpe_color_calculate_regamma_params(vpe_priv,
            x_scale,
            y_scale,
            &vpe_priv->cal_buffer,
            output_tf);

    return true;
}

static bool color_update_degamma_tf(struct vpe_priv *vpe_priv,
    enum color_transfer_func color_input_tf, struct fixed31_32 x_scale,
    struct fixed31_32 y_scale, struct fixed31_32 y_bias, bool can_bypass,
    struct transfer_func *input_tf)
{
    bool               ret    = true;
    struct pwl_params *params = NULL;

    if (can_bypass) {
        input_tf->type = TF_TYPE_BYPASS;
        return true;
    }

    input_tf->cm_gamma_type = CM_DEGAM;
    input_tf->type = TF_TYPE_DISTRIBUTED_POINTS;
    input_tf->start_base = y_bias;

    switch (color_input_tf) {
    case TRANSFER_FUNC_SRGB:
    case TRANSFER_FUNC_BT709:
    case TRANSFER_FUNC_BT1886:
    case TRANSFER_FUNC_PQ2084:
    case TRANSFER_FUNC_NORMALIZED_PQ:
    case TRANSFER_FUNC_LINEAR_0_1:
    case TRANSFER_FUNC_LINEAR_0_125:
        input_tf->tf = color_input_tf;
        break;
    default:
        VPE_ASSERT(0);
        break;
    }

    if (!vpe_priv->init.debug.force_tf_calculation &&
        x_scale.value == vpe_fixpt_one.value       &&
        y_scale.value == vpe_fixpt_one.value       &&
        y_bias.value  == vpe_fixpt_zero.value)
        vpe_priv->resource.get_tf_pwl_params(input_tf, &params, CM_DEGAM);

    if (params)
        input_tf->use_pre_calculated_table = true;
    else
        input_tf->use_pre_calculated_table = false;

    if (!input_tf->use_pre_calculated_table) {
        vpe_color_calculate_degamma_params(vpe_priv,
            x_scale,
            y_scale,
            input_tf);
    }

    return ret;
}

static enum vpe_status vpe_allocate_cm_memory(struct vpe_priv *vpe_priv, const struct vpe_build_param *param) {

    struct stream_ctx  *stream_ctx;
    struct output_ctx  *output_ctx;
    enum vpe_status     status = VPE_STATUS_OK;

    for (uint32_t stream_idx = 0; stream_idx < param->num_streams; stream_idx++) {
        stream_ctx = &vpe_priv->stream_ctx[stream_idx];

        if (!stream_ctx->input_cs) {
            stream_ctx->input_cs =
                (struct vpe_csc_matrix *)vpe_zalloc(sizeof(struct vpe_csc_matrix));
            if (!stream_ctx->input_cs) {
                vpe_log("err: out of memory for input cs!");
                return VPE_STATUS_NO_MEMORY;
            }
        }

        if (!stream_ctx->input_tf) {
            stream_ctx->input_tf =
                (struct transfer_func *)vpe_zalloc(sizeof(struct transfer_func));
            if (!stream_ctx->input_tf) {
                vpe_log("err: out of memory for input tf!");
                return VPE_STATUS_NO_MEMORY;
            }
        }

        if (!stream_ctx->bias_scale) {
            stream_ctx->bias_scale =
                (struct bias_and_scale *)vpe_zalloc(sizeof(struct bias_and_scale));
            if (!stream_ctx->bias_scale) {
                vpe_log("err: out of memory for bias and scale!");
                return VPE_STATUS_NO_MEMORY;
            }
        }

        if (!stream_ctx->gamut_remap) {
            stream_ctx->gamut_remap = vpe_zalloc(sizeof(struct colorspace_transform));
            if (!stream_ctx->gamut_remap) {
                vpe_log("err: out of memory for gamut_remap!");
                return VPE_STATUS_NO_MEMORY;
            }
        }
        if (!stream_ctx->blend_tf) {
            stream_ctx->blend_tf = vpe_zalloc(sizeof(struct transfer_func));
            if (!stream_ctx->blend_tf) {
                vpe_log("err: out of memory for blend tf!");
                return VPE_STATUS_NO_MEMORY;
            }
        }
    }

    output_ctx = &vpe_priv->output_ctx;
    if (!output_ctx->output_tf) {
        output_ctx->output_tf =
            (struct transfer_func *)vpe_zalloc(sizeof(struct transfer_func));
        if (!output_ctx->output_tf) {
            vpe_log("err: out of memory for output tf!");
            return VPE_STATUS_NO_MEMORY;
        }
    }

    return VPE_STATUS_OK;
}

static enum color_space color_get_icsc_cs(enum color_space ics)
{
    switch (ics) {
    case COLOR_SPACE_SRGB:
    case COLOR_SPACE_SRGB_LIMITED:
    case COLOR_SPACE_MSREF_SCRGB:
    case COLOR_SPACE_2020_RGB_FULLRANGE:
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
        return COLOR_SPACE_SRGB;
    case COLOR_SPACE_JFIF:
    case COLOR_SPACE_YCBCR601:
    case COLOR_SPACE_YCBCR601_LIMITED:
        return COLOR_SPACE_YCBCR601;
    case COLOR_SPACE_YCBCR709:
    case COLOR_SPACE_YCBCR709_LIMITED:
        return COLOR_SPACE_YCBCR709;
    case COLOR_SPACE_2020_YCBCR:
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        return COLOR_SPACE_2020_YCBCR;
    default:
        return COLOR_SPACE_UNKNOWN;
    }
}

// return true is bypass can be done
static bool color_update_input_cs(struct vpe_priv *vpe_priv, enum color_space in_cs,
    const struct vpe_color_adjust *adjustments, struct vpe_csc_matrix *input_cs,
    struct vpe_color_adjust *stream_clr_adjustments, struct fixed31_32 *matrix_scaling_factor)
{
    int  i, j;
    bool use_adjustments = false;
    int  arr_size        = sizeof(vpe_input_csc_matrix_fixed) / sizeof(struct vpe_csc_matrix);

    input_cs->cs    = COLOR_SPACE_UNKNOWN;
    use_adjustments = vpe_use_csc_adjust(adjustments);
    in_cs           = color_get_icsc_cs(in_cs);

    for (i = 0; i < arr_size; i++)
        if (vpe_input_csc_matrix_fixed[i].cs == in_cs) {
            input_cs->cs = vpe_input_csc_matrix_fixed[i].cs;
            for (j = 0; j < 12; j++)
                input_cs->regval[j] = vpe_input_csc_matrix_fixed[i].regval[j];
            break;
        }

    if (i == arr_size) {
        vpe_log("err: unknown cs not handled!");
        return false;
    }

    if (use_adjustments && is_ycbcr(in_cs)) { // shader supports only yuv input for color
                                              // adjustments
        vpe_log("Apply color adjustments (contrast, saturation, hue, brightness)");
        if (!vpe_color_calculate_input_cs(
                vpe_priv, in_cs, adjustments, input_cs, matrix_scaling_factor))
            return false;
        *stream_clr_adjustments = *adjustments;
    }

    return true;
}

/* This function generates software points for the blnd gam programming block.
   The logic for the blndgam/ogam programming sequence is a function of:
   1. Output Range (Studio Full)
   2. 3DLUT usage
   3. Output format (HDR SDR)

   SDR Out or studio range out
      TM Case
         BLNDGAM : NL -> NL*S + B
         OGAM    : Bypass
      Non TM Case
         BLNDGAM : L -> NL*S + B
         OGAM    : Bypass
   Full range HDR Out
      TM Case
         BLNDGAM : NL -> L
         OGAM    : L -> NL
      Non TM Case
         BLNDGAM : Bypass
         OGAM    : L -> NL

*/

static enum vpe_status vpe_update_blnd_gamma(
    struct vpe_priv                 *vpe_priv,
    const struct vpe_build_param    *param,
    const struct vpe_tonemap_params *tm_params,
    struct transfer_func            *blnd_tf)
{

    struct output_ctx *output_ctx;
    struct vpe_color_space   tm_out_cs;
    struct fixed31_32        x_scale       = vpe_fixpt_one;
    struct fixed31_32        y_scale       = vpe_fixpt_one;
    struct fixed31_32        y_bias        = vpe_fixpt_zero;
    bool                     is_studio     = false;
    bool                     can_bypass    = false;
    bool                     lut3d_enabled = false;
    enum color_space         cs            = COLOR_SPACE_2020_RGB_FULLRANGE;
    enum color_transfer_func tf            = TRANSFER_FUNC_LINEAR_0_1;
    enum vpe_status          status        = VPE_STATUS_OK;

    is_studio = (param->dst_surface.cs.range == VPE_COLOR_RANGE_STUDIO);
    output_ctx = &vpe_priv->output_ctx;
    lut3d_enabled = tm_params->UID != 0 || tm_params->enable_3dlut;

    if (is_studio) {

        if (vpe_is_rgb8(param->dst_surface.format)) {
            y_scale = STUDIO_RANGE_SCALE_8_BIT;
            y_bias = STUDIO_RANGE_FOOT_ROOM_8_BIT;
        }
        else {

            y_scale = STUDIO_RANGE_SCALE_10_BIT;
            y_bias = STUDIO_RANGE_FOOT_ROOM_10_BIT;
        }
    }

    //If SDR out -> Blend should be NL
    //If studio out -> No choice but to blend in NL
    if (!vpe_is_HDR(output_ctx->tf) || is_studio) {
        if (lut3d_enabled) {
            tf = TRANSFER_FUNC_LINEAR_0_1;
        }
        else {
            tf = output_ctx->tf;
        }

        color_update_regamma_tf(vpe_priv,
            tf,
            x_scale,
            y_scale,
            y_bias,
            can_bypass,
            blnd_tf);
    }
    else {

        if (lut3d_enabled) {
            vpe_color_build_tm_cs(tm_params, param->dst_surface, &tm_out_cs);
            vpe_color_get_color_space_and_tf(&tm_out_cs, &cs, &tf);
        }
        else {
            can_bypass = true;
        }

        color_update_degamma_tf(vpe_priv,
            tf,
            x_scale,
            y_scale,
            y_bias,
            can_bypass,
            blnd_tf);
    }
    return status;
}

/* This function generates software points for the ogam gamma programming block.
   The logic for the blndgam/ogam programming sequence is a function of:
   1. Output Range (Studio Full)
   2. 3DLUT usage
   3. Output format (HDR SDR)

   SDR Out or studio range out
      TM Case
         BLNDGAM : NL -> NL*S + B
         OGAM    : Bypass
      Non TM Case
         BLNDGAM : L -> NL*S + B
         OGAM    : Bypass
   Full range HDR Out
      TM Case
         BLNDGAM : NL -> L
         OGAM    : L -> NL
      Non TM Case
         BLNDGAM : Bypass
         OGAM    : L -> NL

*/
static enum vpe_status vpe_update_output_gamma(
    struct vpe_priv              *vpe_priv,
    const struct vpe_build_param *param,
    struct transfer_func         *output_tf)
{
    bool               can_bypass = false;
    struct output_ctx *output_ctx = &vpe_priv->output_ctx;
    bool               is_studio  = (param->dst_surface.cs.range == VPE_COLOR_RANGE_STUDIO);
    enum vpe_status    status     = VPE_STATUS_OK;

    if (vpe_is_HDR(output_ctx->tf) && !is_studio)
        can_bypass = false; //Blending is done in linear light so ogam needs to handle the regam
    else
        can_bypass = true;

    color_update_regamma_tf(vpe_priv,
        output_ctx->tf,
        vpe_fixpt_one,
        vpe_fixpt_one,
        vpe_fixpt_zero,
        can_bypass,
        output_tf);

    return status;
}

bool vpe_use_csc_adjust(const struct vpe_color_adjust *adjustments)
{
    float epsilon = 0.001f; // steps are 1.0f or 0.01f, so should be plenty

    // see vpe_types.h and vpe_color_adjust definition for VpBlt ranges

    // default brightness = 0
    if (adjustments->brightness > epsilon || adjustments->brightness < -epsilon)
        return true;

    // default contrast = 1
    if (adjustments->contrast > 1 + epsilon || adjustments->contrast < 1 - epsilon)
        return true;

    // default saturation = 1
    if (adjustments->saturation > 1 + epsilon || adjustments->saturation < 1 - epsilon)
        return true;

    // default hue = 0
    if (adjustments->hue > epsilon || adjustments->hue < -epsilon)
        return true;

    return false;
}

/*                     Bias and Scale reference table
    Encoding Bpp    Format      Data Range    Expansion Bias         Scale
    aRGB     32bpp  8888        Full          Zero      0            256/255
                    8888        Limited       Zero      -16/256      256/(235-16)
                    2101010     Full          Zero      0            1024/1023
                    2101010     Limited       Zero      -64/1024     1024/(940-64)
                    2101010     XR bias       Zero      -384/1024    1024/510  // not used
             64bpp  fixed 10bpc Full          Zero      0            1024/1023 // do we have these?
                    10 bpc      limited       zero      -64/1024     1024/(940-64)
                    12 bpc      Full          Zero      0            4096/4095
                    12 bpc      Limited       Zero     -256/4096     4096/(3760-256)
    aCrYCb   32bpp  8888        Full          Zero      0            256/255
                    8888        Limited       Zero      Y:-16/256    Y:256/(235-16)
                                                        C:-128/256   C:256/(240-16) // See notes
   below 2101010     Full          Zero      0            1024/1023 2101010     Limited       Zero
   Y:-64/1024   Y:1024/(940-64) C:-512/1024  C:1024(960-64) 64bpp  fixed 10bpc Full          Zero 0
   1024/1023 10 bpc      Limited       Zero      Y:-64/1024   Y:1024/(940-64) C:-512/1024
   C:1024(960-64) // See notes below 12 bpc      Full          Zero      0            4096/4095 12
   bpc      Limited       Zero      Y:-256/4096  Y:4096/(3760-256) C:-2048/4096 C:4096/(3840-256) //
   See notes below

    The bias_c we use here in the function are diff with the above table from hw team
    because the table is to run with CSC matrix which expect chroma
    from -0.5~+0.5.
    However the csc matrix we use in ICSC is expecting chroma value
    from 0.0~1.0.
    Hence we need a bias for chroma to output a range from 0.0~1.0 instead.
    So we use the same value as luma (Y) which expects range from 0~1.0 already.
                                                        */
static bool build_scale_and_bias(struct bias_and_scale *bias_and_scale,
    const struct vpe_color_space *vcs, enum vpe_surface_pixel_format format)
{
    struct fixed31_32 scale               = vpe_fixpt_one;                  // RGB or Y
    struct fixed31_32 scale_c             = vpe_fixpt_one;                  // Cb/Cr
    struct fixed31_32 bias                = vpe_fixpt_zero;                 // RGB or Y
    struct fixed31_32 bias_c              = vpe_fixpt_from_fraction(-1, 2); // Cb/Cr
    bool              is_chroma_different = false;

    struct custom_float_format fmt;
    fmt.exponenta_bits = 6;
    fmt.mantissa_bits  = 12;
    fmt.sign           = true;

    if (vpe_is_rgb8(format)) {
        if (vcs->range == VPE_COLOR_RANGE_FULL) {
            scale = vpe_fixpt_from_fraction(256, 255);
        } else if (vcs->range == VPE_COLOR_RANGE_STUDIO) {
            scale = vpe_fixpt_from_fraction(256, 235 - 16);
            bias  = vpe_fixpt_from_fraction(-16, 256);
        } // else report error? here just go with default (1.0, 0.0)
    } else if (vpe_is_rgb10(format)) {
        if (vcs->range == VPE_COLOR_RANGE_FULL) {
            scale = vpe_fixpt_from_fraction(1024, 1023);
        } else if (vcs->range == VPE_COLOR_RANGE_STUDIO) {
            scale = vpe_fixpt_from_fraction(1024, 940 - 64);
            bias  = vpe_fixpt_from_fraction(-64, 1024);
        } // else report error? here just go with default (1.0, 0.0)
    } else if (vpe_is_yuv420_8(format) || vpe_is_yuv444_8(format)) {
        if (vcs->range == VPE_COLOR_RANGE_FULL) {
            scale = vpe_fixpt_from_fraction(256, 255);
        } else if (vcs->range == VPE_COLOR_RANGE_STUDIO) {
            scale   = vpe_fixpt_from_fraction(256, 235 - 16);
            bias    = vpe_fixpt_from_fraction(-16, 256);
            scale_c = vpe_fixpt_from_fraction(256, 240 - 16);
            bias_c  = vpe_fixpt_from_fraction(-16, 256); // See notes in function comment
            is_chroma_different = true;
        } // else report error? not sure if default is right
    } else if (vpe_is_yuv420_10(format) || vpe_is_yuv444_10(format)) {
        if (vcs->range == VPE_COLOR_RANGE_FULL) {
            scale = vpe_fixpt_from_fraction(1024, 1023);
        } else if (vcs->range == VPE_COLOR_RANGE_STUDIO) {
            scale   = vpe_fixpt_from_fraction(1024, 940 - 64);
            bias    = vpe_fixpt_from_fraction(-64, 1024);
            scale_c = vpe_fixpt_from_fraction(1024, 960 - 64);
            bias_c  = vpe_fixpt_from_fraction(-64, 1024); // See notes in function comment
            is_chroma_different = true;
        } // else report error? not sure if default is right
    }

    vpe_convert_to_custom_float_format(scale, &fmt, &bias_and_scale->scale_green);
    vpe_convert_to_custom_float_format(bias, &fmt, &bias_and_scale->bias_green);

    // see definition of scale/bias and scale_c/bias_c
    // RGB formats only have scale/bias since all color channels are the same
    // YCbCr have scale/bias for Y (in HW maps to G) and scale_c/bias_c for CrCb (mapping to R,B)
    if (!is_chroma_different) {
        bias_and_scale->scale_red  = bias_and_scale->scale_green;
        bias_and_scale->scale_blue = bias_and_scale->scale_green;
        bias_and_scale->bias_red   = bias_and_scale->bias_green;
        bias_and_scale->bias_blue  = bias_and_scale->bias_green;
    } else {
        vpe_convert_to_custom_float_format(scale_c, &fmt, &bias_and_scale->scale_red);
        vpe_convert_to_custom_float_format(bias_c, &fmt, &bias_and_scale->bias_red);
        bias_and_scale->scale_blue = bias_and_scale->scale_red;
        bias_and_scale->bias_blue  = bias_and_scale->bias_red;
    }

    return true;
}

enum vpe_status vpe_color_build_tm_cs(const struct vpe_tonemap_params *tm_params,
    struct vpe_surface_info surface_info, struct vpe_color_space *tm_out_cs)
{
    tm_out_cs->tf        = tm_params->lut_out_tf;
    tm_out_cs->primaries = tm_params->lut_out_gamut;
    tm_out_cs->encoding  = surface_info.cs.encoding;
    tm_out_cs->range     = VPE_COLOR_RANGE_FULL;     // surface_info.cs.range;
    tm_out_cs->cositing  = VPE_CHROMA_COSITING_NONE; // surface_info.cs.cositing;

    return VPE_STATUS_OK;
}

enum vpe_status vpe_color_update_color_space_and_tf(
    struct vpe_priv *vpe_priv, const struct vpe_build_param *param)
{
    uint32_t           stream_idx;
    struct stream_ctx *stream_ctx;
    struct fixed31_32  new_matrix_scaling_factor;
    struct output_ctx *output_ctx                = &vpe_priv->output_ctx;
    enum vpe_status    status                    = VPE_STATUS_OK;

    status = vpe_allocate_cm_memory(vpe_priv, param);
    if (status == VPE_STATUS_OK) {

        color_check_output_cm_update(vpe_priv, &vpe_priv->output_ctx.surface.cs);

        for (stream_idx = 0; stream_idx < param->num_streams; stream_idx++) {

            new_matrix_scaling_factor = vpe_fixpt_one;
            stream_ctx = &vpe_priv->stream_ctx[stream_idx];

            color_check_input_cm_update(vpe_priv, stream_ctx,
                &param->streams[stream_idx].surface_info.cs, &param->streams[stream_idx].color_adj,
                param->streams[stream_idx].tm_params.UID != 0 || param->streams[stream_idx].tm_params.enable_3dlut);

            build_scale_and_bias(stream_ctx->bias_scale,
                &param->streams[stream_idx].surface_info.cs,
                param->streams[stream_idx].surface_info.format);

            if (stream_ctx->dirty_bits.color_space) {
                if (!color_update_input_cs(vpe_priv, stream_ctx->cs,
                    &stream_ctx->stream.color_adj, stream_ctx->input_cs,
                    &stream_ctx->color_adjustments, &new_matrix_scaling_factor)) {
                    vpe_log("err: input cs not being programmed!");
                }
                else {
                    if ((vpe_priv->scale_yuv_matrix) && // the option to scale the matrix yuv to rgb is
                                                        // on
                        (new_matrix_scaling_factor.value !=
                            vpe_priv->stream_ctx->tf_scaling_factor.value)) {
                        vpe_priv->stream_ctx->tf_scaling_factor = new_matrix_scaling_factor;
                        stream_ctx->dirty_bits.transfer_function = 1; // force tf recalculation
                    }
                }
            }

            if (stream_ctx->dirty_bits.transfer_function) {
                color_update_degamma_tf(vpe_priv, stream_ctx->tf,
                    vpe_priv->stream_ctx->tf_scaling_factor,
                    vpe_fixpt_one,
                    vpe_fixpt_zero,
                    stream_ctx->stream.tm_params.UID != 0 || stream_ctx->stream.tm_params.enable_3dlut, // By Pass degamma if 3DLUT is enabled
                    stream_ctx->input_tf);
            }

            if (stream_ctx->dirty_bits.color_space || output_ctx->dirty_bits.color_space) {
                status = vpe_color_update_gamut(vpe_priv, stream_ctx->cs, output_ctx->cs,
                    stream_ctx->gamut_remap,
                    stream_ctx->stream.tm_params.UID != 0 || stream_ctx->stream.tm_params.enable_3dlut);
            }

            
            if (output_ctx->dirty_bits.transfer_function ||
                output_ctx->dirty_bits.color_space ||
                stream_ctx->update_3dlut) {
                vpe_update_blnd_gamma(vpe_priv, param, &stream_ctx->stream.tm_params, stream_ctx->blend_tf);
            }
        }

        if (status == VPE_STATUS_OK) {
            if (output_ctx->dirty_bits.transfer_function ||
                output_ctx->dirty_bits.color_space) {
                vpe_update_output_gamma(vpe_priv, param, output_ctx->output_tf);
            }
        }

    }
    return status;
}

enum vpe_status vpe_color_tm_update_hdr_mult(uint16_t shaper_in_exp_max, uint32_t peak_white,
    struct fixed31_32 *hdr_multiplier, bool enable3dlut)
{
    if (enable3dlut) {
        struct fixed31_32 shaper_in_gain;
        struct fixed31_32 pq_norm_gain;

        // HDRMULT = 2^shaper_in_exp_max*(1/PQ(x))
        shaper_in_gain = vpe_fixpt_from_int((long long)1 << shaper_in_exp_max);
        vpe_compute_pq(vpe_fixpt_from_fraction((long long)peak_white, 10000), &pq_norm_gain);

        *hdr_multiplier = vpe_fixpt_div(shaper_in_gain, pq_norm_gain);
    } else {
        *hdr_multiplier = vpe_fixpt_one;
    }

    return VPE_STATUS_OK;
}

enum vpe_status vpe_color_update_shaper(
    uint16_t shaper_in_exp_max, struct transfer_func *shaper_func, bool enable_3dlut)

{
    if (!enable_3dlut) {
        shaper_func->type = TF_TYPE_BYPASS;
        return VPE_STATUS_OK;
    }
    struct vpe_shaper_setup_in shaper_in;

    shaper_in.shaper_in_max      = 1 << 16;
    shaper_in.use_const_hdr_mult = false; // can't be true. Fix is required.

    shaper_func->type = TF_TYPE_HWPWL;
    shaper_func->tf   = TRANSFER_FUNC_LINEAR_0_1;
    return vpe_build_shaper(&shaper_in, &shaper_func->pwl);
}

enum vpe_status vpe_color_update_movable_cm(
    struct vpe_priv *vpe_priv, const struct vpe_build_param *param)
{
    enum vpe_status ret = VPE_STATUS_OK;

    uint32_t           stream_idx;
    struct stream_ctx *stream_ctx;
    struct output_ctx *output_ctx = &vpe_priv->output_ctx;

    for (stream_idx = 0; stream_idx < param->num_streams; stream_idx++) {
        stream_ctx = &vpe_priv->stream_ctx[stream_idx];

        bool enable_3dlut = stream_ctx->stream.tm_params.UID != 0 || stream_ctx->stream.tm_params.enable_3dlut;

        if (stream_ctx->update_3dlut || stream_ctx->UID_3DLUT != stream_ctx->stream.tm_params.UID) {

            uint32_t                 shaper_norm_factor;
            struct vpe_color_space   tm_out_cs;
            enum color_space         out_lut_cs;
            enum color_transfer_func tf;

            if (!stream_ctx->in_shaper_func) {
                stream_ctx->in_shaper_func = vpe_zalloc(sizeof(struct transfer_func));
                if (!stream_ctx->in_shaper_func) {
                    vpe_log("err: out of memory for shaper tf!");
                    ret = VPE_STATUS_NO_MEMORY;
                    goto exit;
                }
            }

            if (!stream_ctx->blend_tf) {
                stream_ctx->blend_tf = vpe_zalloc(sizeof(struct transfer_func));
                if (!stream_ctx->blend_tf) {
                    vpe_log("err: out of memory for blend/post1d tf!");
                    ret = VPE_STATUS_NO_MEMORY;
                    goto exit;
                }
            }

            if (!stream_ctx->lut3d_func) {
                stream_ctx->lut3d_func = vpe_zalloc(sizeof(struct vpe_3dlut));
                if (!stream_ctx->lut3d_func) {
                    vpe_log("err: out of memory for 3d lut!");
                    ret = VPE_STATUS_NO_MEMORY;
                    goto exit;
                }
            }

            if (!output_ctx->gamut_remap) {
                output_ctx->gamut_remap = vpe_zalloc(sizeof(struct colorspace_transform));
                if (!output_ctx->gamut_remap) {
                    vpe_log("err: out of memory for post blend gamut remap!");
                    ret = VPE_STATUS_NO_MEMORY;
                    goto exit;
                }
            }

            //Blendgam is updated by output vpe_update_output_gamma_sequence

            get_shaper_norm_factor(
                &param->streams[stream_idx].tm_params, stream_ctx, &shaper_norm_factor);

            vpe_color_tm_update_hdr_mult(SHAPER_EXP_MAX_IN, shaper_norm_factor,
                &stream_ctx->lut3d_func->hdr_multiplier, enable_3dlut);

            vpe_color_update_shaper(SHAPER_EXP_MAX_IN, stream_ctx->in_shaper_func, enable_3dlut);

            vpe_color_build_tm_cs(&stream_ctx->stream.tm_params, vpe_priv->output_ctx.surface, &tm_out_cs);

            vpe_color_get_color_space_and_tf(&tm_out_cs, &out_lut_cs, &tf);

            vpe_color_update_gamut(vpe_priv, out_lut_cs, vpe_priv->output_ctx.cs,
                output_ctx->gamut_remap, !enable_3dlut);

            vpe_convert_to_tetrahedral(vpe_priv, param->streams[stream_idx].tm_params.lut_data,
                stream_ctx->lut3d_func, enable_3dlut);

            stream_ctx->update_3dlut = false;
            stream_ctx->UID_3DLUT = param->streams[stream_idx].tm_params.UID;
        }
    }
exit:
    return ret;
}

void vpe_color_get_color_space_and_tf(
    const struct vpe_color_space *vcs, enum color_space *cs, enum color_transfer_func *tf)
{
    enum vpe_color_range colorRange = vcs->range;

    *cs = COLOR_SPACE_UNKNOWN;
    *tf = TRANSFER_FUNC_UNKNOWN;

    switch (vcs->tf) {
    case VPE_TF_G22:
        *tf = TRANSFER_FUNC_SRGB;
        break;
    case VPE_TF_G24:
        *tf = TRANSFER_FUNC_BT1886;
        break;
    case VPE_TF_PQ:
        *tf = TRANSFER_FUNC_PQ2084;
        break;
    case VPE_TF_PQ_NORMALIZED:
        *tf = TRANSFER_FUNC_NORMALIZED_PQ;
        break;
    case VPE_TF_G10:
        *tf = TRANSFER_FUNC_LINEAR_0_125;
        break;
    case VPE_TF_SRGB:
        *tf = TRANSFER_FUNC_SRGB;
        break;
    case VPE_TF_BT709:
        *tf = TRANSFER_FUNC_BT709;
        break;
    case VPE_TF_HLG:
        *tf = TRANSFER_FUNC_HLG;
        break;
    default:
        break;
    }

    if (vcs->encoding == VPE_PIXEL_ENCODING_YCbCr) {
        switch (vcs->tf) {
        case VPE_TF_G22:
            *tf = TRANSFER_FUNC_BT709;
            break;
        default:
            break;
        }

        switch (vcs->primaries) {
        case VPE_PRIMARIES_BT601:
            *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_YCBCR601
                                                     : COLOR_SPACE_YCBCR601_LIMITED;
            break;
        case VPE_PRIMARIES_BT709:
            *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_YCBCR709
                                                     : COLOR_SPACE_YCBCR709_LIMITED;
            break;
        case VPE_PRIMARIES_BT2020:
            *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_2020_YCBCR
                                                     : COLOR_SPACE_2020_YCBCR_LIMITED;
            break;
        case VPE_PRIMARIES_JFIF:
            *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_JFIF : COLOR_SPACE_UNKNOWN;
            break;
        default:
            break;
        }
    } else {
        switch (vcs->primaries) {
        case VPE_PRIMARIES_BT709:
            if (vcs->tf == VPE_TF_G10) {
                *cs = COLOR_SPACE_MSREF_SCRGB;
            } else {
                *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_SRGB
                                                         : COLOR_SPACE_SRGB_LIMITED;
            }
            break;
        case VPE_PRIMARIES_BT2020:
            *cs = colorRange == VPE_COLOR_RANGE_FULL ? COLOR_SPACE_2020_RGB_FULLRANGE
                                                     : COLOR_SPACE_2020_RGB_LIMITEDRANGE;
            break;
        default:
            break;
        }
    }
}

bool vpe_is_rgb_equal(const struct pwl_result_data *rgb, uint32_t num)
{
    uint32_t i;
    bool     ret = true;

    for (i = 0; i < num; i++) {
        if (rgb[i].red_reg != rgb[i].green_reg || rgb[i].blue_reg != rgb[i].red_reg ||
            rgb[i].blue_reg != rgb[i].green_reg) {
            ret = false;
            break;
        }
    }
    return ret;
}

void vpe_convert_full_range_color_enum(enum color_space *cs)
{
    switch (*cs) {
    case COLOR_SPACE_YCBCR601_LIMITED:
        *cs = COLOR_SPACE_YCBCR601;
        break;
    case COLOR_SPACE_YCBCR709_LIMITED:
        *cs = COLOR_SPACE_YCBCR709;
        break;
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        *cs = COLOR_SPACE_2020_YCBCR;
        break;
    case COLOR_SPACE_SRGB_LIMITED:
        *cs = COLOR_SPACE_SRGB;
        break;
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
        *cs = COLOR_SPACE_2020_RGB_FULLRANGE;
        break;
    default:
        break;
    }
}

bool vpe_is_HDR(enum color_transfer_func tf)
{

    return (tf == TRANSFER_FUNC_PQ2084 || tf == TRANSFER_FUNC_LINEAR_0_125 || tf == TRANSFER_FUNC_HLG);
}

/*
 *
 * Pixel processing in VPE can be divided int two main paths. Tone maping cases and non tone mapping
 * cases. The gain factor supplied by the below function is only applied in the non-tone mapping
 * path.
 *
 * The gain is used to scale the white point in SDR<->HDR conversions.
 * 
 * The policy is as follows:
 * HDR -> SDR (None tone mapping case): Map max input pixel value indicated by HDR meta data to
 * value of 1. SDR-> HDR : Map nominal value of 1 to display brightness indicated by metadata.
 *
 * Table outlining handling for full combination can be found in VPE Wolfpack
 */
enum vpe_status vpe_color_update_whitepoint(
    const struct vpe_priv *vpe_priv, const struct vpe_build_param *param)
{

    struct stream_ctx            *stream       = vpe_priv->stream_ctx;
    const struct output_ctx      *output_ctx   = &vpe_priv->output_ctx;
    const struct vpe_color_space *vpe_cs       = &stream->stream.surface_info.cs;
    bool                          output_isHDR = vpe_is_HDR(vpe_priv->output_ctx.tf);
    bool                          input_isHDR  = false;
    bool                          isYCbCr      = false;
    bool                          isG24        = false;

    for (unsigned int stream_index = 0; stream_index < vpe_priv->num_streams; stream_index++) {

        input_isHDR = vpe_is_HDR(stream->tf);
        isYCbCr     = (vpe_cs->encoding == VPE_PIXEL_ENCODING_YCbCr);
        isG24       = (vpe_cs->tf == VPE_TF_G24);

        if (!input_isHDR && output_isHDR) {
            int sdrWhiteLevel        = (isYCbCr || isG24) ? SDR_VIDEO_WHITE_POINT : SDR_WHITE_POINT;
            stream->white_point_gain = vpe_fixpt_from_fraction(sdrWhiteLevel, 10000);
        } else if (input_isHDR && !output_isHDR) {

            stream->white_point_gain = stream->stream.hdr_metadata.max_mastering != 0
                                           ? vpe_fixpt_from_fraction(HDR_PEAK_WHITE,
                                                 stream->stream.hdr_metadata.max_mastering)
                                           : vpe_fixpt_one;
        } else {
            stream->white_point_gain = vpe_fixpt_one;
        }
        stream++;
    }
    return VPE_STATUS_OK;
}
