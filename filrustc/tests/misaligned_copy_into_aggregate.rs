// The direct known-issue #13 shape through real rustc codegen (the IR shape
// of filrustc/tests/sroa_misaligned_ptr_piece.ll): a pointer-maybe aggregate
// with a typed (all-pointer-struct) alloca receives a byte copy of a
// low-alignment (align-2) buffer at a non-word-aligned offset, and the copy
// crosses the aggregate's pointer word at [8, 16).  Before the Fil-C rustc
// escaped pointer-maybe allocas with `zhas_union` and moved pointer-maybe
// copies with `zmemmove_builtin` (see build_rustc-notes.md, known-issue #13),
// upstream SROA widened the crossing memcpy piece into `load ptr` /
// `store ptr` at 4-mod-8/2-mod-8 addresses and the pizlonator's
// word-alignment check panicked at run time ("alignment requirement of 8
// bytes not met ... ptr aligned to 8 bytes at offset 4").  With the
// clang-parity annotations the alloca escapes SROA, the copy stays a whole
// memcpy, and the data must come back out byte-exact.
use std::ptr;

#[repr(C)]
struct Carrier {
    tag: u32, // offset 0..4
    err: *mut u8, // offset 8..16: the pointer word
    data: [u8; 100], // offset 16..116
}

#[repr(C, align(2))]
struct LowAlign {
    family: u16,
    bytes: [u8; 108], // 110 bytes total, align 2, no pointers
}

#[inline(never)]
fn build_low_align(seed: u8) -> LowAlign {
    let mut l = LowAlign { family: 2, bytes: [0; 108] };
    for (i, b) in l.bytes.iter_mut().enumerate() {
        *b = seed.wrapping_add(i as u8);
    }
    l
}

fn main() {
    let mut c = Carrier { tag: 0, err: std::ptr::null_mut(), data: [0u8; 100] };
    // A real pointer stored into (and read out of) the pointer word: these
    // typed accesses are what give SROA the ptr-typed partition evidence.
    let err_ptr = Box::into_raw(Box::new(0x5150u64)) as *mut u8;
    c.err = err_ptr;
    let check = c.err;
    assert_eq!(check, err_ptr);

    let sun = build_low_align(1);
    // Copy 108 bytes of the align-2 buffer into the carrier at offset 4
    // (4 mod 8): the copy covers [4, 112), which straddles the pointer word
    // [8, 16).  The copy's bytes are integer data; the pointer is clobbered
    // (as in the C original) and must not be read afterwards.
    unsafe {
        ptr::copy_nonoverlapping(
            (&sun as *const LowAlign as *const u8).add(2),
            (&mut c as *mut Carrier as *mut u8).add(4),
            108,
        );
    }
    // data[i] (offset 16 + i) must equal the source byte that landed there:
    // source byte sun+2+j has value seed + j, and data[i] is at j = 12 + i.
    let mut ok = true;
    for i in 0..96usize {
        // the copy covered data[0..96)
        if c.data[i] != 1u8.wrapping_add((12 + i) as u8) {
            ok = false;
        }
    }
    for i in 96..100usize {
        // data[96..100] was beyond the copy and must still be zero
        if c.data[i] != 0 {
            ok = false;
        }
    }
    println!("{}", if ok { "ok" } else { "bad" });
    assert!(ok);
    drop(unsafe { Box::from_raw(err_ptr as *mut u64) });
}
