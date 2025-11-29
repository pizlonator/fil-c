/* Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom thesrc/core/color.c
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

#include "geometric_scaling.h"

/*
 * Geometric scaling is to support multiple pass resizing to achieve larger resize ratio.
 * User will need to set stream.flags.geometric_scaling = 1
 * With the gs flag set to true, VPE will disable following feature
 * 1. gamma remapping, the input tf will be used for output tf.
 * 2. gamut remapping, the input primiaries/range will be used for output.
 * 3. tone mapping, tone mapping will be disabled.
 * 4. blending, blending will be disabled.
 *
 */

void geometric_scaling_feature_skip(struct vpe_priv *vpe_priv, const struct vpe_build_param *param)
{
    /* copy input cs to output for skiping gamut and gamma conversion */
    vpe_priv->output_ctx.surface.cs.primaries = param->streams[0].surface_info.cs.primaries;
    vpe_priv->output_ctx.surface.cs.range = param->streams[0].surface_info.cs.range;
    vpe_priv->output_ctx.surface.cs.tf = param->streams[0].surface_info.cs.tf;

    /* skip tone mapping */
    vpe_priv->stream_ctx[0].stream.tm_params.UID = 0;
    vpe_priv->stream_ctx[0].stream.tm_params.enable_3dlut = false;

    /* disable blending */
    vpe_priv->stream_ctx[0].stream.blend_info.blending = false;

}
