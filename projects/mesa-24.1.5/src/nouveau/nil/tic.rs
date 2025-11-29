// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

#![allow(unused_macros)]

extern crate bitview;
extern crate nvidia_headers;

use bitview::*;
use nil_rs_bindings::*;
use nvidia_headers::classes::cl9097::tex as cl9097;
use nvidia_headers::classes::cl9097::FERMI_A;
use nvidia_headers::classes::clb097::tex as clb097;
use nvidia_headers::classes::clb097::MAXWELL_A;
use nvidia_headers::classes::clc097::tex as clc097;
use nvidia_headers::classes::clc097::PASCAL_A;
use paste::paste;
use std::ops::Range;

use crate::extent::{units, Extent4D};
use crate::format::Format;
use crate::image::Image;
use crate::image::ImageDim;
use crate::image::SampleLayout;
use crate::image::View;
use crate::image::ViewType;

macro_rules! set_enum {
    ($th:expr, $cls:ident, $field:ident, $enum:ident) => {
        paste! {
            $th.set_field($cls::$field, $cls::[<$field _ $enum>])
        }
    };
}

trait SetUFixed {
    fn set_ufixed(&mut self, range: Range<usize>, val: f32);
}

const FIXED_FRAC_BITS: u32 = 8;

impl<T: SetFieldU64> SetUFixed for T {
    fn set_ufixed(&mut self, range: Range<usize>, val: f32) {
        assert!(range.len() >= FIXED_FRAC_BITS as usize);
        let scaled = val * ((1 << FIXED_FRAC_BITS) as f32);
        let scaled_max = ((1 << range.len()) - 1) as f32;
        let u_val = scaled.clamp(0.0, scaled_max).round() as u32;
        self.set_field(range, u_val);
    }
}

fn nv9097_th_v2_source(
    fmt: &nil_tic_format,
    swizzle: pipe_swizzle,
    is_int: bool,
) -> u32 {
    match swizzle {
        PIPE_SWIZZLE_X => fmt.src_x(),
        PIPE_SWIZZLE_Y => fmt.src_y(),
        PIPE_SWIZZLE_Z => fmt.src_z(),
        PIPE_SWIZZLE_W => fmt.src_w(),
        PIPE_SWIZZLE_0 => cl9097::TEXHEADV2_X_SOURCE_IN_ZERO,
        PIPE_SWIZZLE_1 => {
            if is_int {
                cl9097::TEXHEADV2_X_SOURCE_IN_ONE_INT
            } else {
                cl9097::TEXHEADV2_X_SOURCE_IN_ONE_FLOAT
            }
        }
        other => panic!("Invalid component swizzle {}", other),
    }
}

fn nvb097_th_bl_source(
    fmt: &nil_tic_format,
    swizzle: pipe_swizzle,
    is_int: bool,
) -> u32 {
    match swizzle {
        PIPE_SWIZZLE_X => fmt.src_x(),
        PIPE_SWIZZLE_Y => fmt.src_y(),
        PIPE_SWIZZLE_Z => fmt.src_z(),
        PIPE_SWIZZLE_W => fmt.src_w(),
        PIPE_SWIZZLE_0 => clb097::TEXHEADV2_X_SOURCE_IN_ZERO,
        PIPE_SWIZZLE_1 => {
            if is_int {
                clb097::TEXHEADV2_X_SOURCE_IN_ONE_INT
            } else {
                clb097::TEXHEADV2_X_SOURCE_IN_ONE_FLOAT
            }
        }
        other => panic!("Invalid component swizzle {}", other),
    }
}

type THBitView<'a> = BitMutView<'a, [u32; 8]>;

fn nv9097_set_th_v2_0<'a>(
    th: &mut THBitView<'a>,
    format: &Format,
    swizzle: [nil_rs_bindings::pipe_swizzle; 4],
) {
    let fmt = &format.info().tic;
    let is_int = format.is_integer();
    let source = [
        nv9097_th_v2_source(fmt, swizzle[0], is_int),
        nv9097_th_v2_source(fmt, swizzle[1], is_int),
        nv9097_th_v2_source(fmt, swizzle[2], is_int),
        nv9097_th_v2_source(fmt, swizzle[3], is_int),
    ];

    th.set_field(cl9097::TEXHEADV2_COMPONENT_SIZES, fmt.comp_sizes());
    th.set_field(cl9097::TEXHEADV2_R_DATA_TYPE, fmt.type_r());
    th.set_field(cl9097::TEXHEADV2_G_DATA_TYPE, fmt.type_g());
    th.set_field(cl9097::TEXHEADV2_B_DATA_TYPE, fmt.type_b());
    th.set_field(cl9097::TEXHEADV2_A_DATA_TYPE, fmt.type_a());
    th.set_field(cl9097::TEXHEADV2_X_SOURCE, source[0]);
    th.set_field(cl9097::TEXHEADV2_Y_SOURCE, source[1]);
    th.set_field(cl9097::TEXHEADV2_Z_SOURCE, source[2]);
    th.set_field(cl9097::TEXHEADV2_W_SOURCE, source[3]);
}

fn nvb097_set_th_bl_0<'a>(
    th: &mut THBitView<'a>,
    format: &Format,
    swizzle: [nil_rs_bindings::pipe_swizzle; 4],
) {
    let fmt = &format.info().tic;
    let is_int = format.is_integer();
    let source = [
        nvb097_th_bl_source(fmt, swizzle[0], is_int),
        nvb097_th_bl_source(fmt, swizzle[1], is_int),
        nvb097_th_bl_source(fmt, swizzle[2], is_int),
        nvb097_th_bl_source(fmt, swizzle[3], is_int),
    ];

    th.set_field(clb097::TEXHEAD_BL_COMPONENTS, fmt.comp_sizes());
    th.set_field(clb097::TEXHEAD_BL_R_DATA_TYPE, fmt.type_r());
    th.set_field(clb097::TEXHEAD_BL_G_DATA_TYPE, fmt.type_g());
    th.set_field(clb097::TEXHEAD_BL_B_DATA_TYPE, fmt.type_b());
    th.set_field(clb097::TEXHEAD_BL_A_DATA_TYPE, fmt.type_a());
    th.set_field(clb097::TEXHEAD_BL_X_SOURCE, source[0]);
    th.set_field(clb097::TEXHEAD_BL_Y_SOURCE, source[1]);
    th.set_field(clb097::TEXHEAD_BL_Z_SOURCE, source[2]);
    th.set_field(clb097::TEXHEAD_BL_W_SOURCE, source[3]);
}

fn pipe_to_nv_texture_type(ty: ViewType) -> u32 {
    match ty {
        ViewType::_1D => clb097::TEXHEAD_BL_TEXTURE_TYPE_ONE_D,
        ViewType::_2D => clb097::TEXHEAD_BL_TEXTURE_TYPE_TWO_D,
        ViewType::_3D | ViewType::_3DSliced => {
            clb097::TEXHEAD_BL_TEXTURE_TYPE_THREE_D
        }
        ViewType::Cube => clb097::TEXHEAD_BL_TEXTURE_TYPE_CUBEMAP,
        ViewType::_1DArray => clb097::TEXHEAD_BL_TEXTURE_TYPE_ONE_D_ARRAY,
        ViewType::_2DArray => clb097::TEXHEAD_BL_TEXTURE_TYPE_TWO_D_ARRAY,
        ViewType::CubeArray => clb097::TEXHEAD_BL_TEXTURE_TYPE_CUBEMAP_ARRAY,
    }
}

fn nil_rs_to_nv9097_multi_sample_count(sample_layout: SampleLayout) -> u32 {
    match sample_layout {
        SampleLayout::_1x1 => cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT_MODE_1X1,
        SampleLayout::_2x1 => cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT_MODE_2X1,
        SampleLayout::_2x2 => cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT_MODE_2X2,
        SampleLayout::_4x2 => cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT_MODE_4X2,
        SampleLayout::_4x4 => cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT_MODE_4X4,
        SampleLayout::Invalid => panic!("Invalid sample layout"),
    }
}

fn nil_rs_to_nvb097_multi_sample_count(sample_layout: SampleLayout) -> u32 {
    match sample_layout {
        SampleLayout::_1x1 => clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_1X1,
        SampleLayout::_2x1 => clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1,
        SampleLayout::_2x2 => clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2,
        SampleLayout::_4x2 => clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2,
        SampleLayout::_4x4 => clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X4,
        SampleLayout::Invalid => panic!("Invalid sample layout"),
    }
}

fn nil_rs_max_mip_level(image: &Image, view: &View) -> u32 {
    if view.view_type != ViewType::_3D
        && view.array_len == 1
        && view.base_level == 0
        && view.num_levels == 1
    {
        // The Unnormalized coordinates bit in the sampler gets ignored if the
        // referenced image has more than one miplevel.  Fortunately, Vulkan has
        // restrictions requiring the view to be a single-layer single-LOD view
        // in order to use nonnormalizedCoordinates = VK_TRUE in the sampler.
        // From the Vulkan 1.3.255 spec:
        //
        //  "When unnormalizedCoordinates is VK_TRUE, images the sampler is
        //  used with in the shader have the following requirements:
        //
        //      - The viewType must be either VK_IMAGE_VIEW_TYPE_1D or
        //        VK_IMAGE_VIEW_TYPE_2D.
        //      - The image view must have a single layer and a single mip
        //        level."
        //
        // Under these conditions, the view is simply LOD 0 of a single array
        // slice so we don't need to care about aray stride between slices so
        // it's safe to set the number of miplevels to 0 regardless of how many
        // the image actually has.
        0
    } else {
        image.num_levels - 1
    }
}

fn normalize_extent(image: &Image, view: &View) -> Extent4D<units::Pixels> {
    let mut extent = image.extent_px;
    match view.view_type {
        ViewType::_1D
        | ViewType::_2D
        | ViewType::_1DArray
        | ViewType::_2DArray => {
            assert!(image.extent_px.depth == 1);
            extent.depth = view.array_len;
        }
        ViewType::_3D => {
            assert!(image.dim == ImageDim::_3D);
            extent.depth = image.extent_px.depth;
        }
        ViewType::Cube | ViewType::CubeArray => {
            assert!(image.dim == ImageDim::_2D);
            assert!(view.array_len % 6 == 0);
            extent.depth = view.array_len / 6;
        }
        ViewType::_3DSliced => {
            assert!(image.dim == ImageDim::_3D);
            extent.depth = view.array_len;
        }
    }
    extent.array_len = 0;

    extent
}

fn nv9097_fill_tic(
    image: &Image,
    view: &View,
    base_address: u64,
    desc_out: &mut [u32; 8],
) {
    assert!(image.format.el_size_B() == view.format.el_size_B());
    assert!(view.base_level + view.num_levels <= image.num_levels);
    assert!(
        view.base_array_layer + view.array_len <= image.extent_px.array_len
    );

    *desc_out = [0u32; 8];
    let mut th = BitMutView::new(desc_out);

    th.set_field(cl9097::TEXHEADV2_USE_TEXTURE_HEADER_VERSION2, true);
    nv9097_set_th_v2_0(&mut th, &view.format, view.swizzle);

    // There's no base layer field in the texture header
    let layer_address = base_address
        + u64::from(view.base_array_layer)
        + u64::from(image.array_stride_B);

    th.set_field(cl9097::TEXHEADV2_OFFSET_LOWER, layer_address as u32);
    th.set_field(cl9097::TEXHEADV2_OFFSET_UPPER, (layer_address >> 32) as u32);

    let tiling = &image.levels[0].tiling;

    if tiling.is_tiled {
        set_enum!(th, cl9097, TEXHEADV2_MEMORY_LAYOUT, BLOCKLINEAR);

        assert!(tiling.gob_height_is_8);
        assert!(tiling.x_log2 == 0);
        set_enum!(th, cl9097, TEXHEADV2_GOBS_PER_BLOCK_WIDTH, ONE_GOB);
        th.set_field(cl9097::TEXHEADV2_GOBS_PER_BLOCK_HEIGHT, tiling.y_log2);
        th.set_field(cl9097::TEXHEADV2_GOBS_PER_BLOCK_DEPTH, tiling.z_log2);

        let nv_tex_type = pipe_to_nv_texture_type(view.view_type);
        th.set_field(cl9097::TEXHEADV2_TEXTURE_TYPE, nv_tex_type);
    } else {
        set_enum!(th, cl9097, TEXHEADV2_MEMORY_LAYOUT, PITCH);

        let pitch = image.levels[0].row_stride_B;
        th.set_field(cl9097::TEXHEADV2_PITCH, pitch);

        assert!(
            view.view_type == ViewType::_2D
                || view.view_type == ViewType::_2DArray
        );
        assert!(image.sample_layout == SampleLayout::_1x1);
        assert!(view.num_levels == 1);
        set_enum!(th, cl9097, TEXHEADV2_TEXTURE_TYPE, TWO_D_NO_MIPMAP);
    }

    set_enum!(th, cl9097, TEXHEADV2_LOD_ANISO_QUALITY, LOD_QUALITY_HIGH);
    set_enum!(th, cl9097, TEXHEADV2_LOD_ISO_QUALITY, LOD_QUALITY_HIGH);
    set_enum!(
        th,
        cl9097,
        TEXHEADV2_ANISO_COARSE_SPREAD_MODIFIER,
        SPREAD_MODIFIER_NONE
    );

    let extent = normalize_extent(image, view);
    th.set_field(cl9097::TEXHEADV2_WIDTH, extent.width);
    th.set_field(cl9097::TEXHEADV2_HEIGHT, extent.height);
    th.set_field(cl9097::TEXHEADV2_DEPTH, extent.depth);

    let max_mip_level = nil_rs_max_mip_level(image, view);
    th.set_field(cl9097::TEXHEADV2_MAX_MIP_LEVEL, max_mip_level);

    th.set_field(cl9097::TEXHEADV2_S_R_G_B_CONVERSION, view.format.is_srgb());

    set_enum!(th, cl9097, TEXHEADV2_BORDER_SOURCE, BORDER_COLOR);

    // In the sampler, the two options for FLOAT_COORD_NORMALIZATION are:
    //
    // - FORCE_UNNORMALIZED_COORDS
    // - USE_HEADER_SETTING
    //
    // So we set it to normalized in the header and let the sampler select that
    // or force non-normalized.
    th.set_field(cl9097::TEXHEADV2_NORMALIZED_COORDS, true);

    set_enum!(
        th,
        cl9097,
        TEXHEADV2_ANISO_FINE_SPREAD_FUNC,
        SPREAD_FUNC_TWO
    );
    set_enum!(
        th,
        cl9097,
        TEXHEADV2_ANISO_COARSE_SPREAD_FUNC,
        SPREAD_FUNC_ONE
    );

    th.set_field(cl9097::TEXHEADV2_RES_VIEW_MIN_MIP_LEVEL, view.base_level);
    th.set_field(
        cl9097::TEXHEADV2_RES_VIEW_MAX_MIP_LEVEL,
        view.num_levels + view.base_level - 1,
    );

    let msc = nil_rs_to_nv9097_multi_sample_count(image.sample_layout);
    th.set_field(cl9097::TEXHEADV2_MULTI_SAMPLE_COUNT, msc);

    let min_lod_clamp = view.min_lod_clamp - (view.base_level as f32);
    th.set_ufixed(cl9097::TEXHEADV2_MIN_LOD_CLAMP, min_lod_clamp);
}

fn nvb097_fill_tic(
    dev: &nil_rs_bindings::nv_device_info,
    image: &Image,
    view: &View,
    base_address: u64,
    desc_out: &mut [u32; 8],
) {
    assert!(image.format.el_size_B() == view.format.el_size_B());
    assert!(view.base_level + view.num_levels <= image.num_levels);

    *desc_out = [0u32; 8];
    let mut th = BitMutView::new(desc_out);

    nvb097_set_th_bl_0(&mut th, &view.format, view.swizzle);

    let tiling = &image.levels[0].tiling;

    // There's no base layer field in the texture header
    let mut layer_address = base_address;
    if view.view_type == ViewType::_3DSliced {
        assert!(view.num_levels == 1);
        assert!(
            view.base_array_layer + view.array_len <= image.extent_px.depth
        );

        layer_address +=
            image.level_z_offset_B(view.base_level, view.base_array_layer);
    } else {
        assert!(
            view.base_array_layer + view.array_len <= image.extent_px.array_len
        );
        layer_address +=
            u64::from(view.base_array_layer) * u64::from(image.array_stride_B);
    }

    if tiling.is_tiled {
        set_enum!(th, clb097, TEXHEAD_BL_HEADER_VERSION, SELECT_BLOCKLINEAR);

        let addr = BitView::new(&layer_address);
        assert!(addr.get_bit_range_u64(0..9) == 0);
        th.set_field(
            clb097::TEXHEAD_BL_ADDRESS_BITS31TO9,
            addr.get_bit_range_u64(9..32),
        );
        th.set_field(
            clb097::TEXHEAD_BL_ADDRESS_BITS47TO32,
            addr.get_bit_range_u64(32..48),
        );
        assert!(addr.get_bit_range_u64(48..64) == 0);

        assert!(tiling.gob_height_is_8);

        set_enum!(th, clb097, TEXHEAD_BL_GOBS_PER_BLOCK_WIDTH, ONE_GOB);
        th.set_field(clb097::TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT, tiling.y_log2);
        th.set_field(clb097::TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH, tiling.z_log2);
        th.set_field(clb097::TEXHEAD_BL_TILE_WIDTH_IN_GOBS, tiling.x_log2);

        let nv_text_type = pipe_to_nv_texture_type(view.view_type);
        th.set_field(clb097::TEXHEAD_BL_TEXTURE_TYPE, nv_text_type);
    } else {
        set_enum!(th, clb097, TEXHEAD_BL_HEADER_VERSION, SELECT_PITCH);

        let addr = BitView::new(&layer_address);
        assert!(addr.get_bit_range_u64(0..5) == 0);
        th.set_field(
            clb097::TEXHEAD_PITCH_ADDRESS_BITS31TO5,
            addr.get_bit_range_u64(5..32),
        );
        th.set_field(
            clb097::TEXHEAD_PITCH_ADDRESS_BITS47TO32,
            addr.get_bit_range_u64(32..48),
        );
        assert!(addr.get_bit_range_u64(48..64) == 0);

        let pitch = image.levels[0].row_stride_B;
        let pitch = BitView::new(&pitch);
        assert!(pitch.get_bit_range_u64(0..5) == 0);
        assert!(pitch.get_bit_range_u64(21..32) == 0);
        th.set_field(
            clb097::TEXHEAD_PITCH_PITCH_BITS20TO5,
            pitch.get_bit_range_u64(5..21),
        );

        assert!(
            view.view_type == ViewType::_2D
                || view.view_type == ViewType::_2DArray
        );
        assert!(image.sample_layout == SampleLayout::_1x1);
        assert!(view.num_levels == 1);
        set_enum!(th, clb097, TEXHEAD_PITCH_TEXTURE_TYPE, TWO_D_NO_MIPMAP);
    }

    th.set_field(clb097::TEXHEAD_BL_LOD_ANISO_QUALITY2, true);
    set_enum!(th, clb097, TEXHEAD_BL_LOD_ANISO_QUALITY, LOD_QUALITY_HIGH);
    set_enum!(th, clb097, TEXHEAD_BL_LOD_ISO_QUALITY, LOD_QUALITY_HIGH);
    set_enum!(
        th,
        clb097,
        TEXHEAD_BL_ANISO_COARSE_SPREAD_MODIFIER,
        SPREAD_MODIFIER_NONE
    );

    let extent = normalize_extent(image, view);
    th.set_field(clb097::TEXHEAD_BL_WIDTH_MINUS_ONE, extent.width - 1);

    if dev.cls_eng3d >= PASCAL_A {
        let height_1 = extent.height - 1;
        let depth_1 = extent.depth - 1;
        th.set_field(clc097::TEXHEAD_BL_HEIGHT_MINUS_ONE, height_1 & 0xffff);
        th.set_field(clc097::TEXHEAD_BL_HEIGHT_MINUS_ONE_BIT16, height_1 >> 16);
        th.set_field(clc097::TEXHEAD_BL_DEPTH_MINUS_ONE, depth_1 & 0x3fff);
        th.set_field(clc097::TEXHEAD_BL_DEPTH_MINUS_ONE_BIT14, depth_1 >> 14);
    } else {
        th.set_field(clb097::TEXHEAD_BL_HEIGHT_MINUS_ONE, extent.height - 1);
        th.set_field(clb097::TEXHEAD_BL_DEPTH_MINUS_ONE, extent.depth - 1);
    }

    let max_mip_level = nil_rs_max_mip_level(image, view);
    th.set_field(clb097::TEXHEAD_BL_MAX_MIP_LEVEL, max_mip_level);

    th.set_field(clb097::TEXHEAD_BL_S_R_G_B_CONVERSION, view.format.is_srgb());

    set_enum!(th, clb097, TEXHEAD_BL_SECTOR_PROMOTION, PROMOTE_TO_2_V);
    set_enum!(th, clb097, TEXHEAD_BL_BORDER_SIZE, BORDER_SAMPLER_COLOR);

    // In the sampler, the two options for FLOAT_COORD_NORMALIZATION are:
    //
    // - FORCE_UNNORMALIZED_COORDS
    // - USE_HEADER_SETTING
    //
    // So we set it to normalized in the header and let the sampler select that
    // or force non-normalized.

    th.set_field(clb097::TEXHEAD_BL_NORMALIZED_COORDS, true);
    set_enum!(
        th,
        clb097,
        TEXHEAD_BL_ANISO_FINE_SPREAD_FUNC,
        SPREAD_FUNC_TWO
    );
    set_enum!(
        th,
        clb097,
        TEXHEAD_BL_ANISO_COARSE_SPREAD_FUNC,
        SPREAD_FUNC_ONE
    );

    th.set_field(clb097::TEXHEAD_BL_RES_VIEW_MIN_MIP_LEVEL, view.base_level);

    let max_mip_level = view.num_levels + view.base_level - 1;
    th.set_field(clb097::TEXHEAD_BL_RES_VIEW_MAX_MIP_LEVEL, max_mip_level);

    let msc = nil_rs_to_nvb097_multi_sample_count(image.sample_layout);
    th.set_field(clb097::TEXHEAD_BL_MULTI_SAMPLE_COUNT, msc);

    let min_lod_clamp = view.min_lod_clamp - (view.base_level as f32);
    th.set_ufixed(clb097::TEXHEAD_BL_MIN_LOD_CLAMP, min_lod_clamp);
}

pub const IDENTITY_SWIZZLE: [nil_rs_bindings::pipe_swizzle; 4] = [
    nil_rs_bindings::PIPE_SWIZZLE_X,
    nil_rs_bindings::PIPE_SWIZZLE_Y,
    nil_rs_bindings::PIPE_SWIZZLE_Z,
    nil_rs_bindings::PIPE_SWIZZLE_W,
];

fn nv9097_nil_fill_buffer_tic(
    base_address: u64,
    format: Format,
    num_elements: u32,
    desc_out: &mut [u32; 8],
) {
    *desc_out = [0u32; 8];
    let mut th = BitMutView::new(desc_out);
    th.set_field(cl9097::TEXHEADV2_USE_TEXTURE_HEADER_VERSION2, true);

    assert!(format.supports_buffer());
    nv9097_set_th_v2_0(&mut th, &format, IDENTITY_SWIZZLE);

    th.set_field(cl9097::TEXHEADV2_OFFSET_LOWER, base_address as u32);
    th.set_field(cl9097::TEXHEADV2_OFFSET_UPPER, (base_address >> 32) as u32);

    set_enum!(th, cl9097, TEXHEADV2_MEMORY_LAYOUT, PITCH);

    th.set_field(cl9097::TEXHEADV2_WIDTH, num_elements);

    set_enum!(th, cl9097, TEXHEADV2_TEXTURE_TYPE, ONE_D_BUFFER);
}

fn nvb097_nil_fill_buffer_tic(
    base_address: u64,
    format: Format,
    num_elements: u32,
    desc_out: &mut [u32; 8],
) {
    *desc_out = [0u32; 8];
    let mut th = BitMutView::new(desc_out);

    assert!(format.supports_buffer());
    nvb097_set_th_bl_0(&mut th, &format, IDENTITY_SWIZZLE);

    th.set_field(clb097::TEXHEAD_1D_ADDRESS_BITS31TO0, base_address as u32);
    th.set_field(clb097::TEXHEAD_1D_ADDRESS_BITS47TO32, base_address >> 32);

    set_enum!(th, clb097, TEXHEAD_1D_HEADER_VERSION, SELECT_ONE_D_BUFFER);

    th.set_field(
        clb097::TEXHEAD_1D_WIDTH_MINUS_ONE_BITS15TO0,
        (num_elements - 1) & 0xffff,
    );
    th.set_field(
        clb097::TEXHEAD_1D_WIDTH_MINUS_ONE_BITS31TO16,
        (num_elements - 1) >> 16,
    );

    set_enum!(th, clb097, TEXHEAD_1D_TEXTURE_TYPE, ONE_D_BUFFER);

    // TODO: Do we need this?
    set_enum!(th, clb097, TEXHEAD_1D_SECTOR_PROMOTION, PROMOTE_TO_2_V);
}

impl Image {
    #[no_mangle]
    pub extern "C" fn nil_image_fill_tic(
        &self,
        dev: &nil_rs_bindings::nv_device_info,
        view: &View,
        base_address: u64,
        desc_out: &mut [u32; 8],
    ) {
        self.fill_tic(dev, view, base_address, desc_out);
    }

    pub fn fill_tic(
        &self,
        dev: &nil_rs_bindings::nv_device_info,
        view: &View,
        base_address: u64,
        desc_out: &mut [u32; 8],
    ) {
        if dev.cls_eng3d >= MAXWELL_A {
            nvb097_fill_tic(dev, self, view, base_address, desc_out);
        } else if dev.cls_eng3d >= FERMI_A {
            nv9097_fill_tic(self, view, base_address, desc_out);
        } else {
            panic!("Tesla and older not supported");
        }
    }
}

#[no_mangle]
pub extern "C" fn nil_buffer_fill_tic(
    dev: &nil_rs_bindings::nv_device_info,
    base_address: u64,
    format: Format,
    num_elements: u32,
    desc_out: &mut [u32; 8],
) {
    fill_buffer_tic(dev, base_address, format, num_elements, desc_out);
}

pub fn fill_buffer_tic(
    dev: &nil_rs_bindings::nv_device_info,
    base_address: u64,
    format: Format,
    num_elements: u32,
    desc_out: &mut [u32; 8],
) {
    if dev.cls_eng3d >= MAXWELL_A {
        nvb097_nil_fill_buffer_tic(base_address, format, num_elements, desc_out)
    } else if dev.cls_eng3d >= FERMI_A {
        nv9097_nil_fill_buffer_tic(base_address, format, num_elements, desc_out)
    } else {
        panic!("Tesla and older not supported");
    }
}
