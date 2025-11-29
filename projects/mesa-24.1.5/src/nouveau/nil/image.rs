// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

use crate::extent::{units, Extent4D};
use crate::format::Format;
use crate::modifiers::*;
use crate::tiling::Tiling;
use crate::Minify;

use nil_rs_bindings::*;
use nvidia_headers::classes::{cl9097, clc597};

pub const MAX_LEVELS: usize = 16;

pub type ImageUsageFlags = u8;
pub const IMAGE_USAGE_2D_VIEW_BIT: ImageUsageFlags = 1 << 0;
pub const IMAGE_USAGE_LINEAR_BIT: ImageUsageFlags = 1 << 1;
pub const IMAGE_USAGE_SPARSE_RESIDENCY_BIT: ImageUsageFlags = 1 << 2;

#[derive(Clone, Debug, Copy, PartialEq, Default)]
#[repr(u8)]
pub enum ImageDim {
    #[default]
    _1D = 1,
    _2D = 2,
    _3D = 3,
}

#[derive(Clone, Debug, Copy, PartialEq, Default)]
#[repr(u8)]
pub enum SampleLayout {
    _1x1 = 0,
    _2x1 = 1,
    _2x2 = 2,
    _4x2 = 3,
    _4x4 = 4,
    #[default]
    Invalid = 5,
}

impl SampleLayout {
    #[no_mangle]
    pub extern "C" fn nil_choose_sample_layout(samples: u32) -> SampleLayout {
        Self::choose_sample_layout(samples)
    }

    pub fn choose_sample_layout(samples: u32) -> SampleLayout {
        match samples {
            1 => SampleLayout::_1x1,
            2 => SampleLayout::_2x1,
            4 => SampleLayout::_2x2,
            8 => SampleLayout::_4x2,
            16 => SampleLayout::_4x4,
            _ => SampleLayout::Invalid,
        }
    }

    pub fn px_extent_sa(&self) -> Extent4D<units::Samples> {
        match self {
            SampleLayout::_1x1 => Extent4D::new(1, 1, 1, 1),
            SampleLayout::_2x1 => Extent4D::new(2, 1, 1, 1),
            SampleLayout::_2x2 => Extent4D::new(2, 2, 1, 1),
            SampleLayout::_4x2 => Extent4D::new(4, 2, 1, 1),
            SampleLayout::_4x4 => Extent4D::new(4, 4, 1, 1),
            SampleLayout::Invalid => panic!("Invalid sample layout"),
        }
    }

    #[no_mangle]
    pub extern "C" fn nil_px_extent_sa(self) -> Extent4D<units::Samples> {
        self.px_extent_sa()
    }
}

#[derive(Clone, Debug, Copy, PartialEq)]
#[repr(C)]
pub struct ImageInitInfo {
    pub dim: ImageDim,
    pub format: Format,
    pub extent_px: Extent4D<units::Pixels>,
    pub levels: u32,
    pub samples: u32,
    pub usage: ImageUsageFlags,
    pub modifier: u64,
}

/// Represents the data layout of a single slice (level + lod) of an image.
#[repr(C)]
#[derive(Clone, Debug, Copy, PartialEq, Default)]
pub struct ImageLevel {
    pub offset_B: u64,
    pub tiling: Tiling,
    pub row_stride_B: u32,
}

#[repr(C)]
#[derive(Clone, Debug, PartialEq)]
pub struct Image {
    pub dim: ImageDim,
    pub format: Format,
    pub extent_px: Extent4D<units::Pixels>,
    pub sample_layout: SampleLayout,
    pub num_levels: u32,
    pub mip_tail_first_lod: u32,
    pub levels: [ImageLevel; MAX_LEVELS],
    pub array_stride_B: u64,
    pub align_B: u32,
    pub size_B: u64,
    pub compressed: bool,
    pub tile_mode: u16,
    pub pte_kind: u8,
}

impl Image {
    #[no_mangle]
    pub extern "C" fn nil_image_new(
        dev: &nil_rs_bindings::nv_device_info,
        info: &ImageInitInfo,
    ) -> Self {
        Self::new(dev, info)
    }

    pub fn new(
        dev: &nil_rs_bindings::nv_device_info,
        info: &ImageInitInfo,
    ) -> Self {
        match info.dim {
            ImageDim::_1D => {
                assert!(info.extent_px.height == 1);
                assert!(info.extent_px.depth == 1);
                assert!(info.samples == 1);
            }
            ImageDim::_2D => {
                assert!(info.extent_px.depth == 1);
            }
            ImageDim::_3D => {
                assert!(info.extent_px.array_len == 1);
                assert!(info.samples == 1);
            }
        }

        let sample_layout = SampleLayout::choose_sample_layout(info.samples);

        let tiling = if info.modifier != DRM_FORMAT_MOD_INVALID {
            assert!((info.usage & IMAGE_USAGE_SPARSE_RESIDENCY_BIT) == 0);
            assert!(info.dim == ImageDim::_2D);
            assert!(sample_layout == SampleLayout::_1x1);
            if info.modifier == DRM_FORMAT_MOD_LINEAR {
                Tiling::default()
            } else {
                let bl_mod =
                    BlockLinearModifier::try_from(info.modifier).unwrap();

                // We don't support compression yet
                assert!(bl_mod.compression_type() == CompressionType::None);

                bl_mod
                    .tiling()
                    .clamp(info.extent_px.to_B(info.format, sample_layout))
            }
        } else if (info.usage & IMAGE_USAGE_SPARSE_RESIDENCY_BIT) != 0 {
            Tiling::sparse(info.format, info.dim)
        } else {
            Tiling::choose(
                info.extent_px,
                info.format,
                sample_layout,
                info.usage,
            )
        };

        let mut image = Self {
            dim: info.dim,
            format: info.format,
            extent_px: info.extent_px,
            sample_layout,
            num_levels: info.levels,
            levels: [ImageLevel::default(); MAX_LEVELS as usize],
            array_stride_B: 0,
            align_B: 0,
            size_B: 0,
            compressed: false,
            tile_mode: 0,
            pte_kind: 0,
            mip_tail_first_lod: 0,
        };

        if (info.usage & IMAGE_USAGE_SPARSE_RESIDENCY_BIT) != 0 {
            image.mip_tail_first_lod = info.levels;
        }

        let mut layer_size_B = 0;
        for level in 0..info.levels {
            let mut lvl_ext_B = image.level_extent_B(level);
            if tiling.is_tiled {
                let lvl_tiling = tiling.clamp(lvl_ext_B);

                if tiling != lvl_tiling {
                    image.mip_tail_first_lod =
                        std::cmp::min(image.mip_tail_first_lod, level);
                }

                // Align the size to tiles
                let lvl_tiling_ext_B = lvl_tiling.extent_B();
                lvl_ext_B = lvl_ext_B.align(&lvl_tiling_ext_B);

                image.levels[level as usize] = ImageLevel {
                    offset_B: layer_size_B,
                    tiling: lvl_tiling,
                    row_stride_B: lvl_ext_B.width,
                };
            } else {
                // Linear images need to be 2D
                assert!(image.dim == ImageDim::_2D);
                // NVIDIA can't do linear and mipmapping
                assert!(image.num_levels == 1);
                // NVIDIA can't do linear and multisampling
                assert!(image.sample_layout == SampleLayout::_1x1);

                image.levels[level as usize] = ImageLevel {
                    offset_B: layer_size_B,
                    tiling,
                    // Row stride needs to be aligned to 128B for render to work
                    row_stride_B: lvl_ext_B.width.next_multiple_of(128),
                };

                assert!(lvl_ext_B.depth == 1);
            }
            layer_size_B += image.level_size_B(level);
        }

        // We use the tiling for level 0 instead of the tiling selected above
        // because, in the case of sparse residency with small images, level 0 may
        // have a smaller tiling than what we tried to use. However, the level 0
        // tiling is the one we program in the hardware so that's the one we need
        // to use for array stride calculations and the like.

        let lvl0_tiling_size_B = image.levels[0].tiling.size_B();

        // The array stride has to be aligned to the size of a level 0 tile
        image.array_stride_B =
            layer_size_B.next_multiple_of(lvl0_tiling_size_B.into());

        image.size_B =
            image.array_stride_B * u64::from(image.extent_px.array_len);
        image.align_B = lvl0_tiling_size_B;

        // If the client requested sparse residency, we need a 64K alignment
        // or else sparse binding may fail.  This is true regardless of
        // whether or not we actually select a 64K tile format.
        if (info.usage & IMAGE_USAGE_SPARSE_RESIDENCY_BIT) != 0 {
            image.align_B = std::cmp::max(image.align_B, 1 << 16);
        }

        if image.levels[0].tiling.is_tiled {
            image.pte_kind = Self::choose_pte_kind(
                dev,
                info.format,
                info.samples,
                image.compressed,
            );

            if info.modifier != DRM_FORMAT_MOD_INVALID {
                let bl_mod =
                    BlockLinearModifier::try_from(info.modifier).unwrap();
                assert!(bl_mod.pte_kind() == image.pte_kind);
            }
        }

        if image.levels[0].tiling.is_tiled {
            image.tile_mode = u16::from(image.levels[0].tiling.y_log2) << 4
                | u16::from(image.levels[0].tiling.z_log2) << 8;

            image.align_B = std::cmp::max(image.align_B, 4096);
            if image.pte_kind >= 0xb && image.pte_kind <= 0xe {
                image.align_B = std::cmp::max(image.align_B, 1 << 16);
            }
        } else {
            // Linear images need to be aligned to 128B for render to work
            image.align_B = std::cmp::max(image.align_B, 128);
        }

        image.size_B = image.size_B.next_multiple_of(image.align_B.into());

        image
    }

    /// The size in bytes of an extent at a given level.
    fn level_extent_B(&self, level: u32) -> Extent4D<units::Bytes> {
        self.level_extent_px(level)
            .to_B(self.format, self.sample_layout)
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_extent_px(
        &self,
        level: u32,
    ) -> Extent4D<units::Pixels> {
        self.level_extent_px(level)
    }

    pub fn level_extent_px(&self, level: u32) -> Extent4D<units::Pixels> {
        assert!(level == 0 || self.sample_layout == SampleLayout::_1x1);
        self.extent_px.minify(level)
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_layer_offset_B(
        &self,
        level: u32,
        layer: u32,
    ) -> u64 {
        self.level_layer_offset_B(level, layer)
    }

    pub fn level_layer_offset_B(&self, level: u32, layer: u32) -> u64 {
        assert!(level < self.num_levels);
        assert!(layer < self.extent_px.array_len);
        self.levels[level as usize].offset_B
            + u64::from(layer) * self.array_stride_B
    }

    #[no_mangle]
    pub extern "C" fn nil_image_mip_tail_offset_B(&self) -> u64 {
        self.mip_tail_offset_B()
    }

    pub fn mip_tail_offset_B(&self) -> u64 {
        assert!(self.mip_tail_first_lod > 0);
        self.levels[self.mip_tail_first_lod as usize].offset_B
    }

    #[no_mangle]
    pub extern "C" fn nil_image_mip_tail_size_B(&self) -> u32 {
        self.mip_tail_size_B()
    }

    pub fn mip_tail_size_B(&self) -> u32 {
        (self.array_stride_B - self.mip_tail_offset_B())
            .try_into()
            .unwrap()
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_extent_sa(
        &self,
        level: u32,
    ) -> Extent4D<units::Samples> {
        self.level_extent_sa(level)
    }

    pub fn level_extent_sa(&self, level: u32) -> Extent4D<units::Samples> {
        self.level_extent_px(level).to_sa(self.sample_layout)
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_size_B(&self, level: u32) -> u64 {
        self.level_size_B(level)
    }

    pub fn level_size_B(&self, level: u32) -> u64 {
        assert!(level < self.num_levels);
        let lvl_ext_B = self.level_extent_B(level);
        let level = &self.levels[level as usize];

        if level.tiling.is_tiled {
            let lvl_tiling_ext_B = level.tiling.extent_B();
            lvl_ext_B.align(&lvl_tiling_ext_B).size_B().into()
        } else {
            assert!(lvl_ext_B.depth == 1);
            let row_stride = level.row_stride_B * lvl_ext_B.height;
            row_stride.into()
        }
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_depth_stride_B(&self, level: u32) -> u64 {
        self.level_depth_stride_B(level)
    }

    pub fn level_depth_stride_B(&self, level: u32) -> u64 {
        assert!(level < self.num_levels);

        let lvl_ext_B = self.level_extent_B(level);
        let level = &self.levels[level as usize];
        let lvl_tiling_ext_B = level.tiling.extent_B();
        let lvl_ext_B = lvl_ext_B.align(&lvl_tiling_ext_B);

        (lvl_ext_B.width * lvl_ext_B.height).into()
    }

    #[no_mangle]
    pub extern "C" fn nil_image_for_level(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        self.image_for_level(level, offset_in_bytes_out)
    }

    pub fn image_for_level(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        assert!(level < self.num_levels);
        let lvl_extent_px = self.level_extent_px(level);
        let lvl = self.levels[level as usize];
        let align_B = lvl.tiling.size_B();

        let mut size_B = self.size_B - lvl.offset_B;
        if (level + 1) < self.num_levels {
            // This assumes levels are sequential, tightly packed and that each
            // level has a higher alignment than the next one. All of this is
            // currently true.
            let next_lvl_offset_in_bytes =
                self.levels[level as usize + 1].offset_B;
            assert!(next_lvl_offset_in_bytes > lvl.offset_B);
            size_B -= next_lvl_offset_in_bytes - lvl.offset_B;
        }

        let mut levels: [ImageLevel; MAX_LEVELS as usize] = Default::default();
        levels[0] = lvl;

        *offset_in_bytes_out = lvl.offset_B;
        levels[0].offset_B = 0;

        Self {
            extent_px: lvl_extent_px,
            num_levels: 1,
            levels,
            align_B,
            size_B,
            mip_tail_first_lod: if level < self.mip_tail_first_lod {
                1
            } else {
                0
            },
            ..*self
        }
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_as_uncompressed(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        self.level_as_uncompressed(level, offset_in_bytes_out)
    }

    pub fn level_as_uncompressed(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        assert!(self.sample_layout == SampleLayout::_1x1);

        // Format is arbitrary. Pick one that has the right number of bits.
        let uc_format = match self.format.el_size_B() {
            4 => PIPE_FORMAT_R32_UINT,
            8 => PIPE_FORMAT_R32G32_UINT,
            16 => PIPE_FORMAT_R32G32B32A32_UINT,
            _ => panic!("No compressed PIPE_FORMAT with this size"),
        };

        let lvl_image = self.image_for_level(level, offset_in_bytes_out);
        let mut image_out = lvl_image.clone();

        image_out.format = uc_format.try_into().unwrap();
        image_out.extent_px = lvl_image
            .extent_px
            .to_el(lvl_image.format, lvl_image.sample_layout)
            .cast_units();

        image_out
    }

    #[no_mangle]
    pub extern "C" fn nil_image_3d_level_as_2d_array(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        self._3d_level_as_2d_array(level, offset_in_bytes_out)
    }

    pub fn _3d_level_as_2d_array(
        &self,
        level: u32,
        offset_in_bytes_out: &mut u64,
    ) -> Self {
        assert!(self.dim == ImageDim::_3D);
        assert!(self.extent_px.array_len == 1);
        assert!(self.sample_layout == SampleLayout::_1x1);

        let mut image_2d_out = self.image_for_level(level, offset_in_bytes_out);
        let lvl0 = &image_2d_out.levels[0];

        assert!(image_2d_out.num_levels == 1);
        assert!(!lvl0.tiling.is_tiled || lvl0.tiling.z_log2 == 0);

        let lvl_tiling_ext_B = lvl0.tiling.extent_B();
        let lvl_ext_B = image_2d_out.level_extent_B(0);
        let lvl_ext_B = lvl_ext_B.align(&lvl_tiling_ext_B);
        let z_stride = u64::from(lvl_ext_B.width * lvl_ext_B.height);

        image_2d_out.dim = ImageDim::_2D;
        image_2d_out.extent_px.array_len = image_2d_out.extent_px.depth;
        image_2d_out.extent_px.depth = 1;
        image_2d_out.array_stride_B = z_stride;

        image_2d_out
    }

    pub fn choose_pte_kind(
        dev: &nil_rs_bindings::nv_device_info,
        format: Format,
        samples: u32,
        compressed: bool,
    ) -> u8 {
        if dev.cls_eng3d >= clc597::TURING_A {
            Self::tu102_choose_pte_kind(format, compressed)
        } else if dev.cls_eng3d >= cl9097::FERMI_A {
            Self::nvc0_choose_pte_kind(format, samples, compressed)
        } else {
            panic!("Unsupported 3d engine class")
        }
    }

    fn tu102_choose_pte_kind(format: Format, compressed: bool) -> u8 {
        use nvidia_headers::hwref::tu102::mmu::*;
        match pipe_format::from(format) {
            PIPE_FORMAT_Z16_UNORM => {
                if compressed {
                    NV_MMU_PTE_KIND_Z16_COMPRESSIBLE_DISABLE_PLC
                } else {
                    NV_MMU_PTE_KIND_Z16
                }
            }
            PIPE_FORMAT_X8Z24_UNORM
            | PIPE_FORMAT_S8X24_UINT
            | PIPE_FORMAT_S8_UINT_Z24_UNORM => {
                if compressed {
                    NV_MMU_PTE_KIND_Z24S8_COMPRESSIBLE_DISABLE_PLC
                } else {
                    NV_MMU_PTE_KIND_Z24S8
                }
            }
            PIPE_FORMAT_X24S8_UINT
            | PIPE_FORMAT_Z24X8_UNORM
            | PIPE_FORMAT_Z24_UNORM_S8_UINT => {
                if compressed {
                    NV_MMU_PTE_KIND_S8Z24_COMPRESSIBLE_DISABLE_PLC
                } else {
                    NV_MMU_PTE_KIND_S8Z24
                }
            }
            PIPE_FORMAT_X32_S8X24_UINT | PIPE_FORMAT_Z32_FLOAT_S8X24_UINT => {
                if compressed {
                    NV_MMU_PTE_KIND_ZF32_X24S8_COMPRESSIBLE_DISABLE_PLC
                } else {
                    NV_MMU_PTE_KIND_ZF32_X24S8
                }
            }
            PIPE_FORMAT_Z32_FLOAT => NV_MMU_PTE_KIND_GENERIC_MEMORY,
            PIPE_FORMAT_S8_UINT => {
                if compressed {
                    NV_MMU_PTE_KIND_S8_COMPRESSIBLE_DISABLE_PLC
                } else {
                    NV_MMU_PTE_KIND_S8
                }
            }
            _ => NV_MMU_PTE_KIND_GENERIC_MEMORY,
        }
        .try_into()
        .unwrap()
    }

    fn nvc0_choose_pte_kind(
        format: Format,
        samples: u32,
        compressed: bool,
    ) -> u8 {
        use nvidia_headers::hwref::gp100::mmu::*;
        let ms = samples.ilog2();
        match pipe_format::from(format) {
            PIPE_FORMAT_Z16_UNORM => {
                if compressed {
                    NV_MMU_PTE_KIND_Z16_2C + ms
                } else {
                    NV_MMU_PTE_KIND_Z16
                }
            }
            PIPE_FORMAT_X8Z24_UNORM
            | PIPE_FORMAT_S8X24_UINT
            | PIPE_FORMAT_S8_UINT_Z24_UNORM => {
                if compressed {
                    NV_MMU_PTE_KIND_Z24S8_2CZ + ms
                } else {
                    NV_MMU_PTE_KIND_Z24S8
                }
            }
            PIPE_FORMAT_X24S8_UINT
            | PIPE_FORMAT_Z24X8_UNORM
            | PIPE_FORMAT_Z24_UNORM_S8_UINT => {
                if compressed {
                    NV_MMU_PTE_KIND_S8Z24_2CZ + ms
                } else {
                    NV_MMU_PTE_KIND_S8Z24
                }
            }
            PIPE_FORMAT_Z32_FLOAT => {
                if compressed {
                    NV_MMU_PTE_KIND_ZF32_2CZ + ms
                } else {
                    NV_MMU_PTE_KIND_ZF32
                }
            }
            PIPE_FORMAT_X32_S8X24_UINT | PIPE_FORMAT_Z32_FLOAT_S8X24_UINT => {
                if compressed {
                    NV_MMU_PTE_KIND_ZF32_X24S8_2CSZV + ms
                } else {
                    NV_MMU_PTE_KIND_ZF32_X24S8
                }
            }
            PIPE_FORMAT_S8_UINT => NV_MMU_PTE_KIND_S8,
            _ => {
                let blocksize_bits = format.el_size_B() * 8;
                match blocksize_bits {
                    128 => {
                        if compressed {
                            match samples {
                                1 => NV_MMU_PTE_KIND_C128_2C,
                                2 => NV_MMU_PTE_KIND_C128_MS2_2C,
                                4 => NV_MMU_PTE_KIND_C128_MS4_2C,
                                8 | 16 => NV_MMU_PTE_KIND_C128_MS8_MS16_2C,
                                _ => panic!("Unsupported sample count"),
                            }
                        } else {
                            NV_MMU_PTE_KIND_GENERIC_16BX2
                        }
                    }
                    64 => {
                        if compressed {
                            match samples {
                                1 => NV_MMU_PTE_KIND_C64_2C,
                                2 => NV_MMU_PTE_KIND_C64_MS2_2C,
                                4 => NV_MMU_PTE_KIND_C64_MS4_2C,
                                8 | 16 => NV_MMU_PTE_KIND_C64_MS8_MS16_2C,
                                _ => panic!("Unsupported sample count"),
                            }
                        } else {
                            NV_MMU_PTE_KIND_GENERIC_16BX2
                        }
                    }
                    32 => {
                        if compressed {
                            match samples {
                                1 => NV_MMU_PTE_KIND_C32_2C,
                                2 => NV_MMU_PTE_KIND_C32_MS2_2C,
                                4 => NV_MMU_PTE_KIND_C32_MS4_2C,
                                8 | 16 => NV_MMU_PTE_KIND_C32_MS8_MS16_2C,
                                _ => panic!("Unsupported sample count"),
                            }
                        } else {
                            NV_MMU_PTE_KIND_GENERIC_16BX2
                        }
                    }
                    16 | 8 => NV_MMU_PTE_KIND_GENERIC_16BX2,
                    _ => NV_MMU_PTE_KIND_PITCH,
                }
            }
        }
        .try_into()
        .unwrap()
    }

    #[no_mangle]
    pub extern "C" fn nil_msaa_image_as_sa(&self) -> Self {
        self.msaa_as_samples()
    }

    /// For a multisampled image, returns an image of samples
    ///
    /// The resulting image is supersampled with each pixel in the original
    /// consuming some number pixels in the supersampled images according to the
    /// original image's sample layout
    pub fn msaa_as_samples(&self) -> Self {
        assert!(self.dim == ImageDim::_2D);
        assert!(self.num_levels == 1);

        let extent_sa = self.extent_px.to_sa(self.sample_layout);
        let mut out = self.clone();
        out.extent_px = extent_sa.cast_units();
        out.sample_layout = SampleLayout::_1x1;
        out
    }

    #[no_mangle]
    pub extern "C" fn nil_image_level_z_offset_B(
        &self,
        level: u32,
        z: u32,
    ) -> u64 {
        self.level_z_offset_B(level, z)
    }

    pub fn level_z_offset_B(&self, level: u32, z: u32) -> u64 {
        assert!(level < self.num_levels);
        let lvl_extent_px = self.level_extent_px(level);
        assert!(z < lvl_extent_px.depth);

        let lvl_tiling = &self.levels[level as usize].tiling;
        let z_tl = z >> lvl_tiling.z_log2;
        let z_gob = z & ((1 << lvl_tiling.z_log2) - 1);

        let lvl_extent_tl =
            lvl_extent_px.to_tl(lvl_tiling, self.format, self.sample_layout);
        let offset_B = u64::from(
            lvl_extent_tl.width
                * lvl_extent_tl.height
                * z_tl
                * lvl_tiling.size_B(),
        );

        let tiling_extent_B = lvl_tiling.extent_B();
        let offset_B = offset_B
            + u64::from(tiling_extent_B.width * tiling_extent_B.height * z_gob);
        offset_B
    }
}

#[allow(dead_code)]
#[derive(Clone, Debug, Copy, PartialEq)]
#[repr(u8)]
pub enum ViewType {
    _1D,
    _2D,
    _3D,
    _3DSliced,
    Cube,
    _1DArray,
    _2DArray,
    CubeArray,
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq)]
pub struct View {
    pub view_type: ViewType,

    /// The format to use in the view
    ///
    /// This may differ from the format of the actual isl_surf but must have the
    /// same block size.
    pub format: Format,

    pub base_level: u32,
    pub num_levels: u32,

    /// Base array layer
    ///
    /// For cube maps, both base_array_layer and array_len should be specified in
    /// terms of 2-D layers and must be a multiple of 6.
    pub base_array_layer: u32,

    /// Array Length
    ///
    /// Indicates the number of array elements starting at  Base Array Layer.
    pub array_len: u32,

    pub swizzle: [nil_rs_bindings::pipe_swizzle; 4],

    // VK_EXT_image_view_min_lod
    pub min_lod_clamp: f32,
}
