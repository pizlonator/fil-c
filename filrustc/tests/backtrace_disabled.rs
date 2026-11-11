// `std::backtrace::Backtrace`: the DISABLED capture path works under Fil-C
// (the stable surface a program hits whenever RUST_BACKTRACE is unset --
// which is exactly what the test harness runs with).  This test pins that
// surface: `Backtrace::capture()` returns a Disabled backtrace that formats
// as "disabled backtrace".
//
// The ENABLED machinery (RUST_BACKTRACE=1) is the documented intentional
// gap: a real capture calls `_Unwind_Backtrace`, which libpizlo does not
// provide (filc/src/unwind.c `zerror` -> "filc user error: Not
// implemented", exit 42 in the harness), and frame-walking the pizlonated
// stack is not supported.  See the library/backtrace check_greps in
// build_rustc_impl.sh for what IS stubbed (library enumeration from
// /proc/self/maps, no _Unwind_FindEnclosingFunction).
use std::backtrace::Backtrace;

#[inline(never)]
fn capture() -> Backtrace {
    Backtrace::capture()
}

fn main() {
    let bt = capture();
    println!("status = {:?}", bt.status());
    println!("disabled string = {}", format!("{}", bt) == "disabled backtrace");
}
