// Copyright © 2024 Valve Corporation
// SPDX-License-Identifier: MIT

use crate::format::Format;
use crate::image::Image;
use crate::tiling::Tiling;

use bitview::*;
use nvidia_headers::classes::{cl9097, clc597};

pub const MAX_DRM_FORMAT_MODS: usize = 7;

#[repr(u8)]
#[derive(Clone, Copy, Eq, PartialEq)]
pub enum GOBKindVersion {
    Fermi = 0,
    G80 = 1,
    Turing = 2,
}

impl TryFrom<u64> for GOBKindVersion {
    type Error = &'static str;

    fn try_from(gob_kind_version: u64) -> Result<Self, Self::Error> {
        match gob_kind_version {
            0 => Ok(GOBKindVersion::Fermi),
            1 => Ok(GOBKindVersion::G80),
            2 => Ok(GOBKindVersion::Turing),
            _ => Err("Invalid gob/kind version"),
        }
    }
}

impl GOBKindVersion {
    pub fn for_dev(dev: &nil_rs_bindings::nv_device_info) -> GOBKindVersion {
        if dev.cls_eng3d >= clc597::TURING_A {
            GOBKindVersion::Turing
        } else if dev.cls_eng3d >= cl9097::FERMI_A {
            GOBKindVersion::Fermi
        } else {
            GOBKindVersion::G80
        }
    }
}

#[repr(u8)]
#[derive(Clone, Copy, Eq, PartialEq)]
pub enum SectorLayout {
    TegraK1 = 0,
    Desktop = 1,
}

impl TryFrom<u64> for SectorLayout {
    type Error = &'static str;

    fn try_from(sector_layout: u64) -> Result<Self, Self::Error> {
        match sector_layout {
            0 => Ok(SectorLayout::TegraK1),
            1 => Ok(SectorLayout::Desktop),
            _ => Err("Invalid gob/kind version"),
        }
    }
}

impl SectorLayout {
    // For now, this always returns desktop, but will be different for Tegra
    pub fn for_dev(_dev: &nil_rs_bindings::nv_device_info) -> SectorLayout {
        SectorLayout::Desktop
    }
}

#[repr(u8)]
#[allow(dead_code)]
#[derive(Clone, Copy, Eq, PartialEq)]
pub enum CompressionType {
    None = 0,
    ROP3DOne = 1,
    ROP3DTwo = 2,
    CDEHorizontal = 3,
    CDEVertical = 4,
}

impl TryFrom<u64> for CompressionType {
    type Error = &'static str;

    fn try_from(compression_type: u64) -> Result<Self, Self::Error> {
        match compression_type {
            0 => Ok(CompressionType::None),
            1 => Ok(CompressionType::ROP3DOne),
            2 => Ok(CompressionType::ROP3DTwo),
            3 => Ok(CompressionType::CDEHorizontal),
            4 => Ok(CompressionType::CDEVertical),
            _ => Err("Invalid gob/kind version"),
        }
    }
}

pub const DRM_FORMAT_MOD_LINEAR: u64 = 0;
pub const DRM_FORMAT_MOD_INVALID: u64 = 0x00ffffff_ffffffff;

const DRM_FORMAT_MOD_VENDOR_NVIDIA: u8 = 0x03;

pub struct BlockLinearModifier {
    drm_modifier: u64,
}

impl TryFrom<u64> for BlockLinearModifier {
    type Error = &'static str;

    fn try_from(drm_modifier: u64) -> Result<Self, Self::Error> {
        let bv = BitView::new(&drm_modifier);
        let vendor: u8 = bv.get_bit_range_u64(56..64).try_into().unwrap();
        if vendor != DRM_FORMAT_MOD_VENDOR_NVIDIA {
            Err("modifier does not have DRM_FORMAT_MOD_VENDOR_NVIDIA")
        } else if !bv.get_bit(4) {
            Err("modifier is not block linear")
        } else if bv.get_bit_range_u64(5..12) != 0
            || bv.get_bit_range_u64(26..56) != 0
        {
            Err("unknown reserved bits")
        } else {
            Ok(BlockLinearModifier { drm_modifier })
        }
    }
}

impl BlockLinearModifier {
    pub fn block_linear_2d(
        compression_type: CompressionType,
        sector_layout: SectorLayout,
        gob_kind_version: GOBKindVersion,
        pte_kind: u8,
        height_log2: u8,
    ) -> BlockLinearModifier {
        let mut drm_modifier = 0_u64;
        let mut bv = BitMutView::new(&mut drm_modifier);
        bv.set_field(0..4, height_log2);
        bv.set_bit(4, true); // Must be 1, to indicate block-linear layout.
        bv.set_field(12..20, pte_kind);
        bv.set_field(20..22, gob_kind_version as u8);
        bv.set_field(22..23, sector_layout as u8);
        bv.set_field(23..26, compression_type as u8);
        bv.set_field(56..64, DRM_FORMAT_MOD_VENDOR_NVIDIA);
        BlockLinearModifier { drm_modifier }
    }

    pub fn height_log2(&self) -> u8 {
        let bv = BitView::new(&self.drm_modifier);
        bv.get_bit_range_u64(0..4).try_into().unwrap()
    }

    pub fn pte_kind(&self) -> u8 {
        let bv = BitView::new(&self.drm_modifier);
        bv.get_bit_range_u64(12..20).try_into().unwrap()
    }

    pub fn gob_kind_version(&self) -> GOBKindVersion {
        let bv = BitView::new(&self.drm_modifier);
        bv.get_bit_range_u64(20..22).try_into().unwrap()
    }

    pub fn sector_layout(&self) -> SectorLayout {
        let bv = BitView::new(&self.drm_modifier);
        bv.get_bit_range_u64(22..23).try_into().unwrap()
    }

    pub fn compression_type(&self) -> CompressionType {
        let bv = BitView::new(&self.drm_modifier);
        bv.get_bit_range_u64(23..26).try_into().unwrap()
    }

    pub fn tiling(&self) -> Tiling {
        Tiling {
            is_tiled: true,
            gob_height_is_8: self.gob_kind_version() != GOBKindVersion::G80,
            x_log2: 0,
            y_log2: self.height_log2(),
            z_log2: 0,
        }
    }
}

#[no_mangle]
pub extern "C" fn nil_drm_format_mods_for_format(
    dev: &nil_rs_bindings::nv_device_info,
    format: Format,
    mod_count: &mut usize,
    mods: &mut [u64; MAX_DRM_FORMAT_MODS],
) {
    drm_format_mods_for_format(dev, format, mod_count, mods)
}

pub fn drm_format_mods_for_format(
    dev: &nil_rs_bindings::nv_device_info,
    format: Format,
    mod_count: &mut usize,
    mods: &mut [u64; MAX_DRM_FORMAT_MODS],
) {
    let max_mod_count = *mod_count;
    *mod_count = 0;

    if format.is_depth_or_stencil() {
        return;
    }

    if !format.supports_color_targets(dev) {
        return;
    }

    let compression_type = CompressionType::None;
    let sector_layout = SectorLayout::for_dev(dev);
    let gob_kind_version = GOBKindVersion::for_dev(dev);
    let pte_kind = Image::choose_pte_kind(dev, format, 1, false);

    // We assume bigger tiling is better
    for i in 0..6 {
        let height_log2 = 5 - i;

        let bl_mod = BlockLinearModifier::block_linear_2d(
            compression_type,
            sector_layout,
            gob_kind_version,
            pte_kind,
            height_log2,
        );

        assert!(*mod_count < max_mod_count);
        mods[*mod_count] = bl_mod.drm_modifier;
        *mod_count += 1;
    }

    assert!(*mod_count < max_mod_count);
    mods[*mod_count] = DRM_FORMAT_MOD_LINEAR;
    *mod_count += 1;
}

pub fn drm_format_mod_is_supported(
    dev: &nil_rs_bindings::nv_device_info,
    format: Format,
    modifier: u64,
) -> bool {
    if modifier == DRM_FORMAT_MOD_LINEAR {
        return true;
    }

    let Ok(bl_mod) = BlockLinearModifier::try_from(modifier) else {
        return false;
    };

    if bl_mod.height_log2() > 5 {
        return false;
    }

    if bl_mod.gob_kind_version() != GOBKindVersion::for_dev(dev) {
        return false;
    }

    if bl_mod.sector_layout() != SectorLayout::for_dev(dev) {
        return false;
    }

    if bl_mod.compression_type() != CompressionType::None {
        return false;
    }

    let pte_kind = Image::choose_pte_kind(dev, format, 1, false);
    if bl_mod.pte_kind() != pte_kind {
        return false;
    }

    return true;
}

fn score_drm_format_mod(modifier: u64) -> u32 {
    if modifier == DRM_FORMAT_MOD_LINEAR {
        return 1;
    }

    let bl_mod = BlockLinearModifier::try_from(modifier).unwrap();

    // Assume bigger Y-tiling is better
    let mut score = 10 + u32::from(bl_mod.height_log2());

    if bl_mod.compression_type() != CompressionType::None {
        score += 10;
    }

    score
}

pub fn select_best_drm_format_mod(
    dev: &nil_rs_bindings::nv_device_info,
    format: Format,
    modifiers: &[u64],
) -> u64 {
    let mut best = DRM_FORMAT_MOD_INVALID;
    let mut best_score = 0;

    for &modifier in modifiers {
        if !drm_format_mod_is_supported(dev, format, modifier) {
            continue;
        }

        let score = score_drm_format_mod(modifier);
        if score > best_score {
            best = modifier;
            best_score = score;
        }
    }

    return best;
}

#[no_mangle]
pub extern "C" fn nil_select_best_drm_format_mod(
    dev: &nil_rs_bindings::nv_device_info,
    format: Format,
    modifier_count: usize,
    modifiers: *const u64,
) -> u64 {
    let modifiers =
        unsafe { std::slice::from_raw_parts(modifiers, modifier_count) };
    select_best_drm_format_mod(dev, format, modifiers)
}
