// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

extern crate bitview;
extern crate nvidia_headers;
extern crate paste;

use nak_bindings::*;
use nvidia_headers::classes::{cla0c0, clc0c0, clc3c0, clc6c0};

use bitview::*;
use paste::paste;

type QMDBitView<'a> = BitMutView<'a, [u32]>;

trait QMD {
    const GLOBAL_SIZE_OFFSET: usize;

    fn new() -> Self;
    fn set_barrier_count(&mut self, barrier_count: u8);
    fn set_cbuf(&mut self, idx: u8, addr: u64, size: u32);
    fn set_global_size(&mut self, width: u32, height: u32, depth: u32);
    fn set_local_size(&mut self, width: u16, height: u16, depth: u16);
    fn set_prog_addr(&mut self, addr: u64);
    fn set_register_count(&mut self, register_count: u8);
    fn set_slm_size(&mut self, slm_size: u32);
    fn set_smem_size(&mut self, smem_size: u32, smem_max: u32);
}

macro_rules! set_enum {
    ($bv:expr, $cls:ident, $strct:ident, $field:ident, $enum:ident) => {
        $bv.set_field(
            paste! {$cls::[<$strct _ $field>]},
            paste! {$cls::[<$strct _ $field _ $enum>]},
        )
    };
}

macro_rules! set_field {
    ($bv:expr, $cls:ident, $strct:ident, $field:ident, $val:expr) => {
        $bv.set_field(paste! {$cls::[<$strct _ $field>]}, $val)
    };
}

macro_rules! set_array {
    ($bv:expr, $cls:ident, $strct:ident, $f:ident, $i:expr, $x:expr) => {
        $bv.set_field(paste! {$cls::[<$strct _ $f>]}($i), $x)
    };
}

macro_rules! qmd_init {
    ($bv: expr, $c:ident, $s:ident, $mjv:expr, $mnv:expr) => {
        set_field!($bv, $c, $s, QMD_MAJOR_VERSION, $mjv);
        set_field!($bv, $c, $s, QMD_VERSION, $mnv);

        set_enum!($bv, $c, $s, API_VISIBLE_CALL_LIMIT, NO_CHECK);
        set_enum!($bv, $c, $s, SAMPLER_INDEX, INDEPENDENTLY);
    };
}

macro_rules! qmd_impl_common {
    ($c:ident, $s:ident) => {
        fn set_barrier_count(&mut self, barrier_count: u8) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            set_field!(bv, $c, $s, BARRIER_COUNT, barrier_count);
        }

        const GLOBAL_SIZE_OFFSET: usize = {
            let w = paste! {$c::[<$s _CTA_RASTER_WIDTH>]};
            assert!(w.end == w.start + 32);
            w.start / 8
        };

        fn set_global_size(&mut self, width: u32, height: u32, depth: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            set_field!(bv, $c, $s, CTA_RASTER_WIDTH, width);
            set_field!(bv, $c, $s, CTA_RASTER_HEIGHT, height);
            set_field!(bv, $c, $s, CTA_RASTER_DEPTH, depth);
        }

        fn set_local_size(&mut self, width: u16, height: u16, depth: u16) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            set_field!(bv, $c, $s, CTA_THREAD_DIMENSION0, width);
            set_field!(bv, $c, $s, CTA_THREAD_DIMENSION1, height);
            set_field!(bv, $c, $s, CTA_THREAD_DIMENSION2, depth);
        }

        fn set_slm_size(&mut self, slm_size: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            let slm_size = slm_size.next_multiple_of(0x10);
            set_field!(bv, $c, $s, SHADER_LOCAL_MEMORY_HIGH_SIZE, 0);
            set_field!(bv, $c, $s, SHADER_LOCAL_MEMORY_LOW_SIZE, slm_size);
        }
    };
}

const SIZE_SHIFT: u8 = 0;
const SIZE_SHIFTED4_SHIFT: u8 = 4;

macro_rules! qmd_impl_set_cbuf {
    ($c:ident, $s:ident, $size_field:ident) => {
        fn set_cbuf(&mut self, idx: u8, addr: u64, size: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            let idx = idx.into();

            let addr_lo = addr as u32;
            let addr_hi = (addr >> 32) as u32;
            set_array!(bv, $c, $s, CONSTANT_BUFFER_ADDR_LOWER, idx, addr_lo);
            set_array!(bv, $c, $s, CONSTANT_BUFFER_ADDR_UPPER, idx, addr_hi);

            paste! {
                let shift = [<$size_field _SHIFT>];
                let range = $c::[<$s _CONSTANT_BUFFER_ $size_field>](idx);
                assert!(((size >> shift) << shift) == size);
                bv.set_field(range, size >> shift);
            }

            set_array!(bv, $c, $s, CONSTANT_BUFFER_VALID, idx, true);
        }
    };
}

macro_rules! qmd_impl_set_prog_addr_32 {
    ($c:ident, $s:ident) => {
        fn set_prog_addr(&mut self, addr: u64) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            set_field!(bv, $c, $s, PROGRAM_OFFSET, addr);
        }
    };
}

macro_rules! qmd_impl_set_prog_addr_64 {
    ($c:ident, $s:ident) => {
        fn set_prog_addr(&mut self, addr: u64) {
            let mut bv = QMDBitView::new(&mut self.qmd);

            let addr_lo = addr as u32;
            let addr_hi = (addr >> 32) as u32;
            set_field!(bv, $c, $s, PROGRAM_ADDRESS_LOWER, addr_lo);
            set_field!(bv, $c, $s, PROGRAM_ADDRESS_UPPER, addr_hi);
        }
    };
}

macro_rules! qmd_impl_set_register_count {
    ($c:ident, $s:ident, $field:ident) => {
        fn set_register_count(&mut self, register_count: u8) {
            let mut bv = QMDBitView::new(&mut self.qmd);
            set_field!(bv, $c, $s, $field, register_count);
        }
    };
}

mod qmd_0_6 {
    use crate::qmd::*;
    use nvidia_headers::classes::cla0c0::qmd as cla0c0;

    #[repr(transparent)]
    pub struct Qmd0_6 {
        qmd: [u32; 64],
    }

    impl QMD for Qmd0_6 {
        fn new() -> Self {
            let mut qmd = [0; 64];
            let mut bv = QMDBitView::new(&mut qmd);
            qmd_init!(bv, cla0c0, QMDV00_06, 0, 6);
            set_field!(bv, cla0c0, QMDV00_06, SASS_VERSION, 0x30);
            Self { qmd }
        }

        qmd_impl_common!(cla0c0, QMDV00_06);
        qmd_impl_set_cbuf!(cla0c0, QMDV00_06, SIZE);
        qmd_impl_set_prog_addr_32!(cla0c0, QMDV00_06);
        qmd_impl_set_register_count!(cla0c0, QMDV00_06, REGISTER_COUNT);

        fn set_smem_size(&mut self, smem_size: u32, _smem_max: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);

            let smem_size = smem_size.next_multiple_of(0x100);
            set_field!(bv, cla0c0, QMDV00_06, SHARED_MEMORY_SIZE, smem_size);

            let l1_config = if smem_size <= (16 << 10) {
                cla0c0::QMDV00_06_L1_CONFIGURATION_DIRECTLY_ADDRESSABLE_MEMORY_SIZE_16KB
            } else if smem_size <= (32 << 10) {
                cla0c0::QMDV00_06_L1_CONFIGURATION_DIRECTLY_ADDRESSABLE_MEMORY_SIZE_32KB
            } else if smem_size <= (48 << 10) {
                cla0c0::QMDV00_06_L1_CONFIGURATION_DIRECTLY_ADDRESSABLE_MEMORY_SIZE_48KB
            } else {
                panic!("Iclalid shared memory size");
            };
            set_field!(bv, cla0c0, QMDV00_06, L1_CONFIGURATION, l1_config);
        }
    }
}
use qmd_0_6::Qmd0_6;

mod qmd_2_1 {
    use crate::qmd::*;
    use nvidia_headers::classes::clc0c0::qmd as clc0c0;

    #[repr(transparent)]
    pub struct Qmd2_1 {
        qmd: [u32; 64],
    }

    impl QMD for Qmd2_1 {
        fn new() -> Self {
            let mut qmd = [0; 64];
            let mut bv = QMDBitView::new(&mut qmd);
            qmd_init!(bv, clc0c0, QMDV02_01, 2, 1);
            set_field!(bv, clc0c0, QMDV02_01, SM_GLOBAL_CACHING_ENABLE, true);
            Self { qmd }
        }

        qmd_impl_common!(clc0c0, QMDV02_01);
        qmd_impl_set_cbuf!(clc0c0, QMDV02_01, SIZE_SHIFTED4);
        qmd_impl_set_prog_addr_32!(clc0c0, QMDV02_01);
        qmd_impl_set_register_count!(clc0c0, QMDV02_01, REGISTER_COUNT);

        fn set_smem_size(&mut self, smem_size: u32, _smem_max: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);

            let smem_size = smem_size.next_multiple_of(0x100);
            set_field!(bv, clc0c0, QMDV02_01, SHARED_MEMORY_SIZE, smem_size);
        }
    }
}
use qmd_2_1::Qmd2_1;

fn gv100_sm_config_smem_size(size: u32) -> u32 {
    let size = if size > 64 * 1024 {
        96 * 1024
    } else if size > 32 * 1024 {
        64 * 1024
    } else if size > 16 * 1024 {
        32 * 1024
    } else if size > 8 * 1024 {
        16 * 1024
    } else {
        8 * 1024
    };

    size / 4096 + 1
}

macro_rules! qmd_impl_set_smem_size_bounded {
    ($c:ident, $s:ident) => {
        fn set_smem_size(&mut self, smem_size: u32, smem_max: u32) {
            let mut bv = QMDBitView::new(&mut self.qmd);

            let smem_size = smem_size.next_multiple_of(0x100);
            set_field!(bv, $c, $s, SHARED_MEMORY_SIZE, smem_size);

            let max = gv100_sm_config_smem_size(smem_max);
            let min = gv100_sm_config_smem_size(smem_size.into());
            let target = gv100_sm_config_smem_size(smem_size.into());
            set_field!(bv, $c, $s, MIN_SM_CONFIG_SHARED_MEM_SIZE, min);
            set_field!(bv, $c, $s, MAX_SM_CONFIG_SHARED_MEM_SIZE, max);
            set_field!(bv, $c, $s, TARGET_SM_CONFIG_SHARED_MEM_SIZE, target);
        }
    };
}

mod qmd_2_2 {
    use crate::qmd::*;
    use nvidia_headers::classes::clc3c0::qmd as clc3c0;

    #[repr(transparent)]
    pub struct Qmd2_2 {
        qmd: [u32; 64],
    }

    impl QMD for Qmd2_2 {
        fn new() -> Self {
            let mut qmd = [0; 64];
            let mut bv = QMDBitView::new(&mut qmd);
            qmd_init!(bv, clc3c0, QMDV02_02, 2, 2);
            set_field!(bv, clc3c0, QMDV02_02, SM_GLOBAL_CACHING_ENABLE, true);
            Self { qmd }
        }

        qmd_impl_common!(clc3c0, QMDV02_02);
        qmd_impl_set_cbuf!(clc3c0, QMDV02_02, SIZE_SHIFTED4);
        qmd_impl_set_prog_addr_64!(clc3c0, QMDV02_02);
        qmd_impl_set_register_count!(clc3c0, QMDV02_02, REGISTER_COUNT_V);
        qmd_impl_set_smem_size_bounded!(clc3c0, QMDV02_02);
    }
}
use qmd_2_2::Qmd2_2;

mod qmd_3_0 {
    use crate::qmd::*;
    use nvidia_headers::classes::clc6c0::qmd as clc6c0;

    #[repr(transparent)]
    pub struct Qmd3_0 {
        qmd: [u32; 64],
    }

    impl QMD for Qmd3_0 {
        fn new() -> Self {
            let mut qmd = [0; 64];
            let mut bv = QMDBitView::new(&mut qmd);
            qmd_init!(bv, clc6c0, QMDV03_00, 3, 0);
            set_field!(bv, clc6c0, QMDV03_00, SM_GLOBAL_CACHING_ENABLE, true);
            Self { qmd }
        }

        qmd_impl_common!(clc6c0, QMDV03_00);
        qmd_impl_set_cbuf!(clc6c0, QMDV03_00, SIZE_SHIFTED4);
        qmd_impl_set_prog_addr_64!(clc6c0, QMDV03_00);
        qmd_impl_set_register_count!(clc6c0, QMDV03_00, REGISTER_COUNT_V);
        qmd_impl_set_smem_size_bounded!(clc6c0, QMDV03_00);
    }
}
use qmd_3_0::Qmd3_0;

fn fill_qmd<Q: QMD>(info: &nak_shader_info, qmd_info: &nak_qmd_info) -> Q {
    let cs_info = unsafe {
        assert!(info.stage == MESA_SHADER_COMPUTE);
        &info.__bindgen_anon_1.cs
    };

    let mut qmd = Q::new();

    qmd.set_barrier_count(info.num_barriers);
    qmd.set_global_size(
        qmd_info.global_size[0],
        qmd_info.global_size[1],
        qmd_info.global_size[2],
    );
    qmd.set_local_size(
        cs_info.local_size[0],
        cs_info.local_size[1],
        cs_info.local_size[2],
    );
    qmd.set_prog_addr(qmd_info.addr);
    qmd.set_register_count(info.num_gprs);
    qmd.set_slm_size(info.slm_size);

    assert!(qmd_info.smem_size >= cs_info.smem_size);
    assert!(qmd_info.smem_size <= qmd_info.smem_max);
    qmd.set_smem_size(qmd_info.smem_size.into(), qmd_info.smem_max.into());

    for i in 0..qmd_info.num_cbufs {
        let cb = &qmd_info.cbufs[usize::try_from(i).unwrap()];
        if cb.size > 0 {
            qmd.set_cbuf(cb.index.try_into().unwrap(), cb.addr, cb.size);
        }
    }

    qmd
}

#[no_mangle]
pub extern "C" fn nak_fill_qmd(
    dev: *const nv_device_info,
    info: *const nak_shader_info,
    qmd_info: *const nak_qmd_info,
    qmd_out: *mut ::std::os::raw::c_void,
    qmd_size: usize,
) {
    assert!(!dev.is_null());
    let dev = unsafe { &*dev };

    assert!(!info.is_null());
    let info = unsafe { &*info };

    assert!(!qmd_info.is_null());
    let qmd_info = unsafe { &*qmd_info };

    unsafe {
        if dev.cls_compute >= clc6c0::AMPERE_COMPUTE_A {
            let qmd_out = qmd_out as *mut Qmd3_0;
            assert!(qmd_size == std::mem::size_of_val(&*qmd_out));
            qmd_out.write(fill_qmd(info, qmd_info));
        } else if dev.cls_compute >= clc3c0::VOLTA_COMPUTE_A {
            let qmd_out = qmd_out as *mut Qmd2_2;
            assert!(qmd_size == std::mem::size_of_val(&*qmd_out));
            qmd_out.write(fill_qmd(info, qmd_info));
        } else if dev.cls_compute >= clc0c0::PASCAL_COMPUTE_A {
            let qmd_out = qmd_out as *mut Qmd2_1;
            assert!(qmd_size == std::mem::size_of_val(&*qmd_out));
            qmd_out.write(fill_qmd(info, qmd_info));
        } else if dev.cls_compute >= cla0c0::KEPLER_COMPUTE_A {
            let qmd_out = qmd_out as *mut Qmd0_6;
            assert!(qmd_size == std::mem::size_of_val(&*qmd_out));
            qmd_out.write(fill_qmd(info, qmd_info));
        } else {
            panic!("Unknown shader model");
        }
    }
}

#[no_mangle]
pub extern "C" fn nak_qmd_dispatch_size_offset(
    dev: *const nv_device_info,
) -> u32 {
    assert!(!dev.is_null());
    let dev = unsafe { &*dev };

    if dev.cls_compute >= clc6c0::AMPERE_COMPUTE_A {
        Qmd3_0::GLOBAL_SIZE_OFFSET.try_into().unwrap()
    } else if dev.cls_compute >= clc3c0::VOLTA_COMPUTE_A {
        Qmd2_2::GLOBAL_SIZE_OFFSET.try_into().unwrap()
    } else if dev.cls_compute >= clc0c0::PASCAL_COMPUTE_A {
        Qmd2_1::GLOBAL_SIZE_OFFSET.try_into().unwrap()
    } else if dev.cls_compute >= cla0c0::KEPLER_COMPUTE_A {
        Qmd0_6::GLOBAL_SIZE_OFFSET.try_into().unwrap()
    } else {
        panic!("Unsupported shader model");
    }
}
