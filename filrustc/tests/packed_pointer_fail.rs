// Mirror of filc/tests/unionpackedpointerfail (C, negative): a packed struct
// with a pointer at a non-8-byte-aligned offset cannot be represented under
// Fil-C's aligned-pointer model (pointer loads/stores go through
// capability-carrying moves that require word alignment), so the unaligned
// pointer access must be caught by the runtime.  This is also exactly why
// filc_ptr_struct_type refuses all-pointer-struct allocas for aggregates
// with sub-8-byte alignment (see build_rustc-notes.md, known-issue #10).
#[repr(packed)]
struct Packed {
    prefix: [u8; 7],
    pointer: *mut i32, // offset 7: not 8-byte-aligned
}

fn main() {
    let mut target = 41i32;
    let mut value = Packed { prefix: [0u8; 7], pointer: std::ptr::null_mut() };
    // SAFETY: this is the documented negative test -- the unaligned pointer
    // store must panic under Fil-C (the C original stores through the packed
    // member and panics on the same check).
    unsafe {
        std::ptr::addr_of_mut!(value.pointer).write_unaligned(&mut target as *mut i32);
    }
    // also read it back the same way; unreachable if the store panics
    let v = unsafe { std::ptr::addr_of!(value.pointer).read_unaligned() };
    println!("{}", unsafe { *v });
    println!("nono");
}
