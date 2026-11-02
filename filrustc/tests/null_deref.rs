// The helper must be `#[inline(never)]` and the read must be volatile:
// otherwise rustc's own null-pointer check panics in Rust before the load
// ever reaches Fil-C's bounds checking.  With this shape, the load reaches
// the pizlonated code and Fil-C panics with "cannot read pointer with null
// object".
#[inline(never)]
fn nullp() -> *const i32 {
    std::ptr::null()
}

fn main() {
    let p = nullp();
    unsafe {
        println!("{}", p.read_volatile());
    }
}
