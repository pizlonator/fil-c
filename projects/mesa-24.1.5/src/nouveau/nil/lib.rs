// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

extern crate nil_rs_bindings;
extern crate nvidia_headers;

mod extent;
mod format;
mod image;
mod modifiers;
mod tic;
mod tiling;

pub trait ILog2Ceil {
    fn ilog2_ceil(self) -> Self;
}

impl ILog2Ceil for u32 {
    fn ilog2_ceil(self) -> Self {
        if self <= 1 {
            0
        } else {
            (self - 1).ilog2() + 1
        }
    }
}

pub trait Minify<Rhs> {
    // Required method
    fn minify(self, rhs: Rhs) -> Self;
}

impl Minify<u32> for u32 {
    fn minify(self, level: u32) -> u32 {
        std::cmp::max(1, self >> level)
    }
}
