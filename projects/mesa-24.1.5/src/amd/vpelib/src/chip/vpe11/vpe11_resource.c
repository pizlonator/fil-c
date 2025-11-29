/* Copyright 2023 Advanced Micro Devices, Inc.
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
#include <string.h>
#include "vpe_priv.h"
#include "common.h"
#include "vpe11_resource.h"
#include "vpe10_resource.h"
#include "vpe11_cmd_builder.h"
#include "vpe10_vpec.h"
#include "vpe10_cdc.h"
#include "vpe10_dpp.h"
#include "vpe10_mpc.h"
#include "vpe10_opp.h"
#include "vpe_command.h"
#include "vpe10_cm_common.h"
#include "vpe10_background.h"
#include "vpe10/inc/asic/bringup_vpe_6_1_0_offset.h"
#include "vpe10/inc/asic/bringup_vpe_6_1_0_sh_mask.h"
#include "vpe10/inc/asic/bringup_vpe_6_1_0_default.h"
#include "vpe10/inc/asic/vpe_1_0_offset.h"
#include "custom_fp16.h"
#include "custom_float.h"
#include "background.h"

#define LUT_NUM_ENTRIES   (17 * 17 * 17)
#define LUT_ENTRY_SIZE    (2)
#define LUT_NUM_COMPONENT (3)
#define LUT_BUFFER_SIZE   (LUT_NUM_ENTRIES * LUT_ENTRY_SIZE * LUT_NUM_COMPONENT)

// set field/register/bitfield name
#define SFRB(field_name, reg_name, post_fix) .field_name = reg_name##__##field_name##post_fix

#define BASE_INNER(seg_id) VPE_BASE__INST0_SEG##seg_id

#define BASE(seg_id) BASE_INNER(seg_id)

// set register with block id and default val, init lastWrittenVal as default while isWritten set to
// false
#define SRIDFVL(reg_name, block, id)                                                               \
    .reg_name = {BASE(reg##reg_name##_BASE_IDX) + reg##reg_name, reg##reg_name##_##DEFAULT,        \
        reg##reg_name##_##DEFAULT, false}

static struct vpe_caps caps = {
    .lut_size               = LUT_BUFFER_SIZE,
    .rotation_support       = 0,
    .h_mirror_support       = 1,
    .v_mirror_support       = 0,
    .is_apu                 = 1,
    .bg_color_check_support = 0,
    .resource_caps =
        {
            .num_dpp       = 1,
            .num_opp       = 1,
            .num_mpc_3dlut = 1,
            .num_queue     = 8,
        },
    .color_caps = {.dpp =
                       {
                           .pre_csc    = 1,
                           .luma_key   = 0,
                           .dgam_ram   = 0,
                           .post_csc   = 1,
                           .gamma_corr = 1,
                           .hw_3dlut   = 1,
                           .ogam_ram   = 1, /**< programmable gam in output -> gamma_corr */
                           .ocsc       = 0,
                           .dgam_rom_caps =
                               {
                                   .srgb     = 1,
                                   .bt2020   = 1,
                                   .gamma2_2 = 1,
                                   .pq       = 1,
                                   .hlg      = 1,
                               },
                       },
        .mpc =
            {
                .gamut_remap         = 1,
                .ogam_ram            = 1,
                .ocsc                = 1,
                .shared_3d_lut       = 1,
                .global_alpha        = 1,
                .top_bottom_blending = 0,
            }},
    .plane_caps =
        {
            .per_pixel_alpha = 1,
            .input_pixel_format_support =
                {
                    .argb_packed_32b = 1,
                    .nv12            = 1,
                    .fp16            = 0,
                    .p010            = 1, /**< planar 4:2:0 10-bit */
                    .p016            = 0, /**< planar 4:2:0 16-bit */
                    .ayuv            = 0, /**< packed 4:4:4 */
                    .yuy2            = 0  /**< packed 4:2:2 */
                },
            .output_pixel_format_support = {.argb_packed_32b = 1,
                .nv12                                        = 0,
                .fp16                                        = 1,
                .p010                                        = 0,
                .p016                                        = 0,
                .ayuv                                        = 0,
                .yuy2                                        = 0},
            .max_upscale_factor          = 64000,

            // 6:1 downscaling ratio: 1000/6 = 166.666
            .max_downscale_factor = 167,

            .pitch_alignment    = 256,
            .addr_alignment     = 256,
            .max_viewport_width = 1024,
        },
};

static struct vpe_cap_funcs cap_funcs = {.get_dcc_compression_cap = vpe10_get_dcc_compression_cap};

enum vpe_status vpe11_construct_resource(struct vpe_priv *vpe_priv, struct resource *res)
{
    struct vpe *vpe = &vpe_priv->pub;

    vpe->caps      = &caps;
    vpe->cap_funcs = &cap_funcs;

    vpe10_construct_vpec(vpe_priv, &res->vpec);

    res->cdc[0] = vpe10_cdc_create(vpe_priv, 0);
    if (!res->cdc[0])
        goto err;

    res->dpp[0] = vpe10_dpp_create(vpe_priv, 0);
    if (!res->dpp[0])
        goto err;

    res->mpc[0] = vpe10_mpc_create(vpe_priv, 0);
    if (!res->mpc[0])
        goto err;

    res->opp[0] = vpe10_opp_create(vpe_priv, 0);
    if (!res->opp[0])
        goto err;

    vpe11_construct_cmd_builder(vpe_priv, &res->cmd_builder);
    vpe_priv->num_pipe = 1;

    res->internal_hdr_normalization = 1;

    res->check_input_color_space           = vpe10_check_input_color_space;
    res->check_output_color_space          = vpe10_check_output_color_space;
    res->check_h_mirror_support            = vpe10_check_h_mirror_support;
    res->calculate_segments                = vpe10_calculate_segments;
    res->set_num_segments                  = vpe11_set_num_segments;
    res->split_bg_gap                      = vpe10_split_bg_gap;
    res->calculate_dst_viewport_and_active = vpe10_calculate_dst_viewport_and_active;
    res->find_bg_gaps                      = vpe_find_bg_gaps;
    res->create_bg_segments                = vpe_create_bg_segments;
    res->populate_cmd_info                 = vpe10_populate_cmd_info;
    res->program_frontend                  = vpe10_program_frontend;
    res->program_backend                   = vpe10_program_backend;
    res->get_bufs_req                      = vpe10_get_bufs_req;
    res->get_tf_pwl_params                 = vpe10_cm_get_tf_pwl_params;

    return VPE_STATUS_OK;
err:
    vpe11_destroy_resource(vpe_priv, res);
    return VPE_STATUS_ERROR;
}

void vpe11_destroy_resource(struct vpe_priv *vpe_priv, struct resource *res)
{
    if (res->cdc[0] != NULL) {
        vpe_free(container_of(res->cdc[0], struct vpe10_cdc, base));
        res->cdc[0] = NULL;
    }

    if (res->dpp[0] != NULL) {
        vpe_free(container_of(res->dpp[0], struct vpe10_dpp, base));
        res->dpp[0] = NULL;
    }

    if (res->mpc[0] != NULL) {
        vpe_free(container_of(res->mpc[0], struct vpe10_mpc, base));
        res->mpc[0] = NULL;
    }

    if (res->opp[0] != NULL) {
        vpe_free(container_of(res->opp[0], struct vpe10_opp, base));
        res->opp[0] = NULL;
    }
}

enum vpe_status vpe11_set_num_segments(struct vpe_priv *vpe_priv, struct stream_ctx *stream_ctx,
    struct scaler_data *scl_data, struct vpe_rect *src_rect, struct vpe_rect *dst_rect,
    uint32_t *max_seg_width)
{

    uint16_t       num_segs;
    struct dpp    *dpp         = vpe_priv->resource.dpp[0];
    const uint32_t max_lb_size = dpp->funcs->get_line_buffer_size();

    *max_seg_width = min(*max_seg_width, max_lb_size / scl_data->taps.v_taps);

    num_segs = vpe_get_num_segments(vpe_priv, src_rect, dst_rect, *max_seg_width);
    if ((src_rect->width > (uint32_t)(vpe_priv->vpe_num_instance * VPE_MIN_VIEWPORT_SIZE)) &&
        (num_segs % vpe_priv->vpe_num_instance != 0)) {
        num_segs += (vpe_priv->vpe_num_instance - (num_segs % vpe_priv->vpe_num_instance));
    }

    stream_ctx->segment_ctx = vpe_alloc_segment_ctx(vpe_priv, num_segs);
    if (!stream_ctx->segment_ctx)
        return VPE_STATUS_NO_MEMORY;

    stream_ctx->num_segments = num_segs;

    return VPE_STATUS_OK;
}
