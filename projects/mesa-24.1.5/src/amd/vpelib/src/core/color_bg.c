#include <string.h>
#include <math.h>
#include "color_bg.h"
#include "vpe_priv.h"

struct csc_vector {
    float x;
    float y;
    float z;
};

struct csc_table {
    struct csc_vector rgb_offset; // RGB offset
    struct csc_vector red_coef;   // RED coefficient
    struct csc_vector green_coef; // GREEN coefficient
    struct csc_vector blue_coef;  // BLUE coefficient
};


const double bt_709_rgb_xyz_matrix[] = {
    0.135676572958501,   0.117645247657296,   0.059378179384203,
    0.069958232931727,   0.235290495314592,   0.023751271753681,
    0.006359839357430,   0.039215082552432,   0.312725078090138
};

const double bt_601_rgb_xyz_matrix[] = {
    0.129468377303939,   0.120169907240092,   0.063061715455969,
    0.069871822671967,   0.230648692928563,   0.028479484399470,
    0.006165160823997,   0.036826261896157,   0.315308577279846
};

const double bt_2020_rgb_xyz_matrix[] = {
    0.209559197891125,   0.047578961279863,   0.055561840829013,
    0.086428369751707,   0.223061365529709,   0.019510264718585,
    0.000000000000000,   0.009235916013150,   0.349064083986850
};

const double bt_709_xyz_rgb_matrix[] = {
    9.850972467794900,    -4.672897196261683,    -1.515534225814599,
   -2.946029289607537,     5.702028879962675,     0.126307165371354,
    0.169088388136759,    -0.619990756501448,     3.212679374598414
};

const double bt_601_xyz_rgb_matrix[] = {
    10.656544932293809,   -5.288117709127149,    -1.653672548215019,
   -3.249384680406732,     6.011485965740993,     0.106904010143450,
    0.171144655726832,    -0.598710197023623,     3.191344462670923
};

const double bt_2020_xyz_rgb_matrix[] = {
    5.217784765870115,    -1.081066212086299,    -0.770110277731489,
   -2.026396206177778,     4.913316828677627,     0.047928710680581,
    0.053616587979668,    -0.130001864005497,     2.863535322904176
};


static struct csc_table bgcolor_to_rgbfull_table[COLOR_SPACE_MAX] = {
    [COLOR_SPACE_YCBCR601] =
        {
            {0.0f, -0.5f, -0.5f},
            {1.0f, 0.0f, 1.402f},
            {1.0f, -0.344136286f, -0.714136286f},
            {1.0f, 1.772f, 0.0f},
        },
    [COLOR_SPACE_YCBCR709] =
        {
            {0.0f, -0.5f, -0.5f},
            {1.0f, 0.0f, 1.5748f},
            {1.0f, -0.187324273f, -0.468124273f},
            {1.0f, 1.8556f, 0.0f},
        },
    [COLOR_SPACE_YCBCR601_LIMITED] =
        {
            {-0.0625f, -0.5f, -0.5f},
            {1.164383562f, 0.0f, 1.596026786f},
            {1.164383562f, -0.39176229f, -0.812967647f},
            {1.164383562f, 2.017232143f, 0.0f},
        },
    [COLOR_SPACE_YCBCR709_LIMITED] =
        {
            {-0.0625f, -0.5f, -0.5f},
            {1.164383562f, 0.0f, 1.792741071f},
            {1.164383562f, -0.213248614f, -0.532909329f},
            {1.164383562f, 2.112401786f, 0.0f},
        },
    [COLOR_SPACE_2020_YCBCR] =
        {
            {0.0f, -512.f / 1023.f, -512.f / 1023.f},
            {1.0f, 0.0f, 1.4746f},
            {1.0f, -0.164553127f, -0.571353127f},
            {1.0f, 1.8814f, 0.0f},
        },
    [COLOR_SPACE_2020_YCBCR_LIMITED] =
        {
            {-0.0625f, -0.5f, -0.5f},
            {1.167808219f, 0.0f, 1.683611384f},
            {1.167808219f, -0.187877063f, -0.652337331f},
            {1.167808219f, 2.148071652f, 0.0f},
        },
    [COLOR_SPACE_SRGB_LIMITED] =
        {
            {-0.0626221f, -0.0626221f, -0.0626221f},
            {1.167783652f, 0.0f, 0.0f},
            {0.0f, 1.167783652f, 0.0f},
            {0.0f, 0.0, 1.167783652f},
        },
    [COLOR_SPACE_2020_RGB_LIMITEDRANGE] = {
        {-0.0626221f, -0.0626221f, -0.0626221f},
        {1.167783652f, 0.0f, 0.0f},
        {0.0f, 1.167783652f, 0.0f},
        {0.0f, 0.0, 1.167783652f},
    }};

static double clip_double(double x)
{
    if (x < 0.0)
        return 0.0;
    else if (x > 1.0)
        return 1.0;
    else
        return x;
}

static float clip_float(float x)
{
    if (x < 0.0f)
        return 0.0f;
    else if (x > 1.0f)
        return 1.0f;
    else
        return x;
}

static void color_multiply_matrices_double(double *mResult, double *M1,
    double *M2, unsigned int Rows1, unsigned int Cols1, unsigned int Cols2)
{
    unsigned int i, j, k;

    for (i = 0; i < Rows1; i++) {
        for (j = 0; j < Cols2; j++) {
            mResult[(i * Cols2) + j] = 0.0;
            for (k = 0; k < Cols1; k++)
                mResult[(i * Cols2) + j] = mResult[(i * Cols2) + j] +
                    M1[(i * Cols1) + k] * M2[(k * Cols2) + j];
        }
    }
}

static void set_gamut_remap_matrix(double* res, enum color_space src_cs, enum color_space dst_cs) {

    double rgb_to_xyz[9] = { 0.0 };
    double xyz_to_rgb[9] = { 0.0 };

    switch (src_cs)
    {
    case COLOR_SPACE_SRGB:
    case COLOR_SPACE_SRGB_LIMITED:
    case COLOR_SPACE_MSREF_SCRGB:
    case COLOR_SPACE_YCBCR709_LIMITED:
    case COLOR_SPACE_YCBCR709:
    case COLOR_SPACE_JFIF:
        memcpy(rgb_to_xyz, bt_709_rgb_xyz_matrix, 9 * sizeof(double));
        break;
    case COLOR_SPACE_YCBCR601:
    case COLOR_SPACE_YCBCR601_LIMITED:
        memcpy(rgb_to_xyz, bt_601_rgb_xyz_matrix, 9 * sizeof(double));
        break;
    case COLOR_SPACE_2020_RGB_FULLRANGE:
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
    case COLOR_SPACE_2020_YCBCR:
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        memcpy(rgb_to_xyz, bt_2020_rgb_xyz_matrix, 9 * sizeof(double));
        break;
    default:
        VPE_ASSERT(0);
        break;
    }

    switch (dst_cs)
    {
    case COLOR_SPACE_SRGB:
    case COLOR_SPACE_SRGB_LIMITED:
    case COLOR_SPACE_MSREF_SCRGB:
    case COLOR_SPACE_YCBCR709_LIMITED:
    case COLOR_SPACE_YCBCR709:
    case COLOR_SPACE_JFIF:
        memcpy(xyz_to_rgb, bt_709_xyz_rgb_matrix, 9 * sizeof(double));
        break;
    case COLOR_SPACE_YCBCR601:
    case COLOR_SPACE_YCBCR601_LIMITED:
        memcpy(xyz_to_rgb, bt_601_xyz_rgb_matrix, 9 * sizeof(double));
        break;
    case COLOR_SPACE_2020_RGB_FULLRANGE:
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
    case COLOR_SPACE_2020_YCBCR:
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        memcpy(xyz_to_rgb, bt_2020_xyz_rgb_matrix, 9 * sizeof(double));
        break;
    default:
        VPE_ASSERT(0);
        break;
    }

    color_multiply_matrices_double(res, xyz_to_rgb, rgb_to_xyz, 3, 3, 3);

}

static bool bg_csc(struct vpe_color *bg_color, enum color_space cs)
{
    struct csc_table *entry             = &bgcolor_to_rgbfull_table[cs];
    float             csc_final[3]      = {0};
    float             csc_mm[3][4]      = {0};
    bool              output_is_clipped = false;

    memcpy(&csc_mm[0][0], &entry->red_coef, sizeof(struct csc_vector));
    memcpy(&csc_mm[1][0], &entry->green_coef, sizeof(struct csc_vector));
    memcpy(&csc_mm[2][0], &entry->blue_coef, sizeof(struct csc_vector));

    csc_mm[0][3] = entry->rgb_offset.x * csc_mm[0][0] + entry->rgb_offset.y * csc_mm[0][1] +
                   entry->rgb_offset.z * csc_mm[0][2];

    csc_mm[1][3] = entry->rgb_offset.x * csc_mm[1][0] + entry->rgb_offset.y * csc_mm[1][1] +
                   entry->rgb_offset.z * csc_mm[1][2];

    csc_mm[2][3] = entry->rgb_offset.x * csc_mm[2][0] + entry->rgb_offset.y * csc_mm[2][1] +
                   entry->rgb_offset.z * csc_mm[2][2];

    csc_final[0] = csc_mm[0][0] * bg_color->ycbcra.y + csc_mm[0][1] * bg_color->ycbcra.cb +
                   csc_mm[0][2] * bg_color->ycbcra.cr + csc_mm[0][3];

    csc_final[1] = csc_mm[1][0] * bg_color->ycbcra.y + csc_mm[1][1] * bg_color->ycbcra.cb +
                   csc_mm[1][2] * bg_color->ycbcra.cr + csc_mm[1][3];

    csc_final[2] = csc_mm[2][0] * bg_color->ycbcra.y + csc_mm[2][1] * bg_color->ycbcra.cb +
                   csc_mm[2][2] * bg_color->ycbcra.cr + csc_mm[2][3];

    // switch to RGB components
    bg_color->rgba.a = bg_color->ycbcra.a;
    bg_color->rgba.r = clip_float(csc_final[0]);
    bg_color->rgba.g = clip_float(csc_final[1]);
    bg_color->rgba.b = clip_float(csc_final[2]);
    if ((bg_color->rgba.r != csc_final[0]) || (bg_color->rgba.g != csc_final[1]) ||
        (bg_color->rgba.b != csc_final[2])) {
        output_is_clipped = true;
    }
    bg_color->is_ycbcr = false;
    return output_is_clipped;
}

static inline bool is_global_bg_blend_applied(struct stream_ctx *stream_ctx) {

    return (stream_ctx->stream.blend_info.blending)  &&
        (stream_ctx->stream.blend_info.global_alpha) &&
        (stream_ctx->stream.blend_info.global_alpha_value != 1.0);
}

/*
    In order to support background color fill correctly, we need to do studio -> full range conversion
    before the blend block. However, there is also a requirement for HDR output to be blended in linear space.
    Hence, if we have PQ out and studio range, we need to make sure no blenidng will occur. Othewise the job
    is invalid.

*/
static enum vpe_status is_valid_blend(const struct vpe_priv *vpe_priv, struct vpe_color *bg_color) {

    enum vpe_status status = VPE_STATUS_OK;
    const struct vpe_color_space *vcs = &vpe_priv->output_ctx.surface.cs;
    struct stream_ctx *stream_ctx = vpe_priv->stream_ctx;  //Only need to check the first stream.

    if ((vcs->range == VPE_COLOR_RANGE_STUDIO) &&
        (vcs->tf == VPE_TF_PQ) &&
        ((stream_ctx->stream.surface_info.cs.encoding == VPE_PIXEL_ENCODING_RGB) ||
            is_global_bg_blend_applied(stream_ctx)))
        status = VPE_STATUS_BG_COLOR_OUT_OF_RANGE;

    return status;
}

struct gamma_coefs {
    float a0;
    float a1;
    float a2;
    float a3;
    float user_gamma;
    float user_contrast;
    float user_brightness;
};

// srgb, 709, G24
static const int32_t numerator01[] = {31308, 180000, 0};
static const int32_t numerator02[] = {12920, 4500, 0};
static const int32_t numerator03[] = {55, 99, 0};
static const int32_t numerator04[] = {55, 99, 0};
static const int32_t numerator05[] = {2400, 2222, 2400};

static bool build_coefficients(struct gamma_coefs *coefficients, enum color_transfer_func type)
{
    uint32_t index = 0;
    bool     ret   = true;

    if (type == TRANSFER_FUNC_SRGB)
        index = 0;
    else if (type == TRANSFER_FUNC_BT709)
        index = 1;
    else if (type == TRANSFER_FUNC_BT1886)
        index = 2;
    else {
        ret = false;
        goto release;
    }

    coefficients->a0         = (float)numerator01[index] / 10000000.0f;
    coefficients->a1         = (float)numerator02[index] / 1000.0f;
    coefficients->a2         = (float)numerator03[index] / 1000.0f;
    coefficients->a3         = (float)numerator04[index] / 1000.0f;
    coefficients->user_gamma = (float)numerator05[index] / 1000.0f;

release:
    return ret;
}

static double translate_to_linear_space(
    double arg, double a0, double a1, double a2, double a3, double gamma)
{
    double linear;
    double base;

    a0 *= a1;
    if (arg <= -a0) {
        base   = (a2 - arg) / (1.0 + a3);
        linear = -pow(base, gamma);
    } else if ((-a0 <= arg) && (arg <= a0))
        linear = arg / a1;
    else {
        base   = (a2 + arg) / (1.0 + a3);
        linear = pow(base, gamma);
    }

    return linear;
}

// for 709 & sRGB
static void compute_degam(enum color_transfer_func tf, double inY, double *outX, bool clip)
{
    double             ret;
    struct gamma_coefs coefs = {0};

    build_coefficients(&coefs, tf);

    ret = translate_to_linear_space(inY, (double)coefs.a0, (double)coefs.a1, (double)coefs.a2,
        (double)coefs.a3, (double)coefs.user_gamma);

    if (clip) {
        ret = clip_double(ret);
    }
    *outX = ret;
}

static double get_maximum_fp(double a, double b)
{
    if (a > b)
        return a;
    return b;
}

static void compute_depq(double inY, double *outX, bool clip)
{
    double M1 = 0.159301758;
    double M2 = 78.84375;
    double C1 = 0.8359375;
    double C2 = 18.8515625;
    double C3 = 18.6875;

    double nPowM2;
    double base;
    double one      = 1.0;
    double zero     = 0.0;
    bool   negative = false;
    double ret;

    if (inY < zero) {
        inY      = -inY;
        negative = true;
    }
    nPowM2 = pow(inY, one / M2);
    base   = get_maximum_fp(nPowM2 - C1, zero) / (C2 - C3 * nPowM2);
    ret    = pow(base, one / M1);
    if (clip) {
        ret = clip_double(ret);
    }
    if (negative)
        ret = -ret;

    *outX = ret;
}

static bool is_limited_cs(enum color_space cs)
{
    bool is_limited = false;

    switch (cs)
    {
    case COLOR_SPACE_SRGB:
    case COLOR_SPACE_2020_RGB_FULLRANGE:
    case COLOR_SPACE_MSREF_SCRGB:
    case COLOR_SPACE_YCBCR601:
    case COLOR_SPACE_YCBCR709:
    case COLOR_SPACE_JFIF:
    case COLOR_SPACE_2020_YCBCR:
        is_limited = false;
        break;
    case COLOR_SPACE_SRGB_LIMITED:
    case COLOR_SPACE_YCBCR601_LIMITED:
    case COLOR_SPACE_YCBCR709_LIMITED:
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        is_limited = true;
        break;
    default:
        VPE_ASSERT(0);
        is_limited = false;
        break;
    }
    return is_limited;
}

static void vpe_bg_degam(
    struct transfer_func *output_tf, struct vpe_color *bg_color) {

    double degam_r = (double)bg_color->rgba.r;
    double degam_g = (double)bg_color->rgba.g;
    double degam_b = (double)bg_color->rgba.b;

    // de-gam
    switch (output_tf->tf) {

    case TRANSFER_FUNC_PQ2084:
        compute_depq((double)bg_color->rgba.r, &degam_r, true);
        compute_depq((double)bg_color->rgba.g, &degam_g, true);
        compute_depq((double)bg_color->rgba.b, &degam_b, true);
        break;
    case TRANSFER_FUNC_SRGB:
    case TRANSFER_FUNC_BT709:
    case TRANSFER_FUNC_BT1886:
        compute_degam(output_tf->tf, (double)bg_color->rgba.r, &degam_r, true);
        compute_degam(output_tf->tf, (double)bg_color->rgba.g, &degam_g, true);
        compute_degam(output_tf->tf, (double)bg_color->rgba.b, &degam_b, true);
        break;
    case TRANSFER_FUNC_LINEAR_0_125:
    case TRANSFER_FUNC_LINEAR_0_1:
        break;
    default:
        VPE_ASSERT(0);
        break;
    }
    bg_color->rgba.r = (float)degam_r;
    bg_color->rgba.g = (float)degam_g;
    bg_color->rgba.b = (float)degam_b;

}

static void vpe_bg_inverse_gamut_remap(enum color_space output_cs,
    struct transfer_func *output_tf, struct vpe_color *bg_color)
{

        double bg_rgb[3] = { 0.0 };
        double final_bg_rgb[3] = { 0.0 };
        double matrix[9] = { 0.0 };
        bg_rgb[0] = (double)bg_color->rgba.r;
        bg_rgb[1] = (double)bg_color->rgba.g;
        bg_rgb[2] = (double)bg_color->rgba.b;

        switch (output_tf->tf) {
        case TRANSFER_FUNC_LINEAR_0_1:
        case TRANSFER_FUNC_LINEAR_0_125:
            /* Since linear output uses Bt709, and this conversion is only needed
             * when the tone mapping is enabled on (Bt2020) input, it is needed to
             * apply the reverse of Bt2020 -> Bt709 on the background color to
             * cancel out the effect of Bt2020 -> Bt709 on the background color.
             */
            set_gamut_remap_matrix(matrix, COLOR_SPACE_SRGB, COLOR_SPACE_2020_RGB_FULLRANGE);
            color_multiply_matrices_double(final_bg_rgb, matrix, bg_rgb, 3, 3, 1);

            bg_color->rgba.r = (float)clip_double(final_bg_rgb[0]);
            bg_color->rgba.g = (float)clip_double(final_bg_rgb[1]);
            bg_color->rgba.b = (float)clip_double(final_bg_rgb[2]);

            break;
        case TRANSFER_FUNC_PQ2084:
        case TRANSFER_FUNC_SRGB:
        case TRANSFER_FUNC_BT709:
        case TRANSFER_FUNC_BT1886:
            break;
        default:
            VPE_ASSERT(0);
            break;
        }

}

static void inverse_output_csc(enum color_space output_cs, struct vpe_color* bg_color)
{
    enum color_space bgcolor_cs = COLOR_SPACE_YCBCR709;

    switch (output_cs) {
        // output is ycbr cs, follow output's setting
    case COLOR_SPACE_YCBCR601:
    case COLOR_SPACE_YCBCR709:
    case COLOR_SPACE_YCBCR601_LIMITED:
    case COLOR_SPACE_YCBCR709_LIMITED:
    case COLOR_SPACE_2020_YCBCR:
    case COLOR_SPACE_2020_YCBCR_LIMITED:
        bgcolor_cs = output_cs;
        break;
        // output is RGB cs, follow output's range
        // but need yuv to rgb csc
    case COLOR_SPACE_SRGB_LIMITED:
        bgcolor_cs = COLOR_SPACE_YCBCR709_LIMITED;
        break;
    case COLOR_SPACE_2020_RGB_LIMITEDRANGE:
        bgcolor_cs = COLOR_SPACE_2020_YCBCR_LIMITED;
        break;
    case COLOR_SPACE_SRGB:
    case COLOR_SPACE_MSREF_SCRGB:
        bgcolor_cs = COLOR_SPACE_YCBCR709;
        break;
    case COLOR_SPACE_2020_RGB_FULLRANGE:
        bgcolor_cs = COLOR_SPACE_2020_YCBCR;
        break;
    default:
        // should revise the newly added CS
        // and set corresponding bgcolor_cs accordingly
        VPE_ASSERT(0);
        bgcolor_cs = COLOR_SPACE_YCBCR709;
        break;
    }

    // input is [0-0xffff]
    // convert bg color to RGB full range for use inside pipe
    bg_csc(bg_color, bgcolor_cs);
}

// To understand the logic for background color conversion,
// please refer to vpe_update_output_gamma_sequence in color.c
void vpe_bg_color_convert(
    enum color_space output_cs, struct transfer_func *output_tf, struct vpe_color *bg_color, bool enable_3dlut)
{
    // inverse OCSC
    if (bg_color->is_ycbcr)
        inverse_output_csc(output_cs, bg_color);

    if (output_tf->type != TF_TYPE_BYPASS) {
        // inverse degam
        if (output_tf->tf == TRANSFER_FUNC_PQ2084 && !is_limited_cs(output_cs))
            vpe_bg_degam(output_tf, bg_color);
        // inverse gamut remap
        if (enable_3dlut)
            vpe_bg_inverse_gamut_remap(output_cs, output_tf, bg_color);
    }
    // for TF_TYPE_BYPASS, bg color should be programmed to mpc as linear
}

enum vpe_status vpe_bg_color_outside_cs_gamut(
    const struct vpe_priv *vpe_priv, struct vpe_color *bg_color)
{
    enum color_space         cs;
    enum color_transfer_func tf;
    struct vpe_color         bg_color_copy = *bg_color;
    const struct vpe_color_space *vcs      = &vpe_priv->output_ctx.surface.cs;

    vpe_color_get_color_space_and_tf(vcs, &cs, &tf);

    if ((bg_color->is_ycbcr)) {
        // using the bg_color_copy instead as bg_csc will modify it
        // we should not do modification in checking stage
        // otherwise validate_cached_param() will fail
        if (bg_csc(&bg_color_copy, cs)) {
            return VPE_STATUS_BG_COLOR_OUT_OF_RANGE;
        }
    }
    return VPE_STATUS_OK;
}

static inline bool is_target_rect_equal_to_dest_rect(const struct vpe_priv *vpe_priv)
{
    const struct vpe_rect *target_rect = &vpe_priv->output_ctx.target_rect;
    const struct vpe_rect *dst_rect = &vpe_priv->stream_ctx[0].stream.scaling_info.dst_rect;

    return (target_rect->height == dst_rect ->height) && (target_rect->width  == dst_rect ->width) &&
           (target_rect->x == dst_rect ->x) && (target_rect->y == dst_rect ->y);
}

// These two checks are only necessary for VPE1.0 and contain a lot of quirks to work around VPE 1.0
// limitations.
enum vpe_status vpe_is_valid_bg_color(const struct vpe_priv *vpe_priv, struct vpe_color *bg_color) {

    enum vpe_status status = VPE_STATUS_OK;

    /* no need for background filling as for target rect equal to dest rect */
    if (is_target_rect_equal_to_dest_rect(vpe_priv)) {
        return VPE_STATUS_OK;
    }

    status = is_valid_blend(vpe_priv, bg_color);

    if (status == VPE_STATUS_OK)
        status = vpe_bg_color_outside_cs_gamut(vpe_priv, bg_color);

    return status;
}
