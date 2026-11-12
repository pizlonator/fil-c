// Mirror of filc/tests/unioncarrierfail (C, negative): a pointer-bearing
// union passed BY VALUE across an extern "C" call boundary loses the
// pointer's capability in the ABI carrier staging -- the documented
// known-issue #10 gap (a pointer-bearing 8-byte aggregate is classified as
// one Int eightbyte and crosses the boundary as an i64 register, which under
// GIMSO carries no capability).  Losing the capability gives a null-object
// error; bypassing bounds would allow the read -- either way the Fil-C
// runtime must catch this read before it prints anything (the C original
// stages a one-past-the-end pointer precisely so that a *preserved*
// capability would also trap at upper).
#[repr(C)]
union Hidden {
    bits: u64,
    pointers: [*mut i32; 1],
}

extern "C" fn bounce(value: Hidden) -> Hidden {
    value
}

fn main() {
    let p = Box::into_raw(Box::new(41i32));
    // one-past-the-end pointer, exactly like the C test
    let value = Hidden { pointers: [p.wrapping_add(4)] };
    let value = bounce(value);
    println!("{}", unsafe { *value.pointers[0] });
    println!("nono");
}
