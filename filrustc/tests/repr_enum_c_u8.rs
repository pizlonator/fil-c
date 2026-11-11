// `#[repr(C, u8)]` pointer-maybe enums -- the standard C-FFI tagged-enum
// pattern: the ICE + runtime guard for the explicit-int-repr arm of the
// Fil-C enum-layout fix (change #24).
//
// ICE guard: this shape FAILS TO COMPILE (rustc exits 101 with the
// rustc_abi layout-calculator assert "layout decided on a larger
// discriminant type (I64) than typeck (I8)") if the Fil-C discriminant
// bump is applied to explicit reprs on the first layout computation, so
// any return of that bug is caught here at compile time.
//
// Runtime guard: the enum is pointer-maybe, so its alloca gets the
// all-pointer struct typing and the FIRST payload byte of every variant
// must sit on a word boundary (the Tagged payloads do, at 8, because repr(C)
// grows the tag prefix to the field alignment; Mixed's u32 payload lands at
// 4 mod 8 on the first computation and the recompute path must widen Mixed's
// tag; later fields of a multi-field variant may stay at 4 mod 8, which is
// safe because they are only touched with integer-typed accesses).
// The program RUNS both variants of both enums through by-value moves and
// prints the payloads: the integer-only variant payloads at 4 mod 8 must
// not pizlonator-panic at runtime.
use std::mem::{replace, swap};

// The C-FFI tagged-enum pattern itself.
#[repr(C, u8)]
#[derive(Debug, PartialEq)]
enum Tagged {
    Num(u64),
    Ptr(Box<String>),
}

// The same repr with a payload that is NOT word-aligned on the first
// layout computation (a 1-byte tag leaves the u32 at offset 4): this is
// the shape that must go through the recompute (word-sized tag).
#[repr(C, u8)]
#[derive(Debug, PartialEq)]
enum Mixed {
    Small(u32),
    Big(Box<String>),
}

#[inline(never)]
fn make_tagged(n: u64, s: &str) -> Tagged {
    if n == 0 {
        Tagged::Ptr(Box::new(s.to_string()))
    } else {
        Tagged::Num(n)
    }
}

#[inline(never)]
fn describe_tagged(t: Tagged) -> String {
    match t {
        Tagged::Num(n) => format!("Num({})", n),
        Tagged::Ptr(b) => format!("Ptr({})", b),
    }
}

#[inline(never)]
fn describe_mixed(m: Mixed) -> String {
    match m {
        Mixed::Small(v) => format!("Small({})", v),
        Mixed::Big(b) => format!("Big({})", b),
    }
}

fn main() {
    // Both Tagged variants, by value through a call (sret move) and a match.
    println!(
        "n = {} p = {}",
        describe_tagged(make_tagged(0xABCD, "ignored")),
        describe_tagged(make_tagged(0, "ffi-box"))
    );

    // Vec<Tagged>: whole enums move through the vector's memory.
    let mut v = Vec::new();
    v.push(make_tagged(1, "v1"));
    v.push(make_tagged(0, "v2"));
    v.push(make_tagged(0xFFFF_FFFF_FFFF_FFFF, "v3"));
    let out: Vec<String> = v.into_iter().map(describe_tagged).collect();
    println!("vec = {}", out.join(", "));

    // mem::swap and mem::replace on Tagged.
    let mut x = make_tagged(0, "swap-ptr");
    let mut y = make_tagged(77, "swap-num");
    swap(&mut x, &mut y);
    let swapped = (describe_tagged(x), describe_tagged(y));
    println!("swapped x = {} y = {}", swapped.0, swapped.1);

    let mut x = make_tagged(0, "replace-me");
    let prev = replace(&mut x, make_tagged(0, "replaced"));
    println!("replaced prev = {} now = {}", describe_tagged(prev), describe_tagged(x));

    // Both Mixed variants through the same moves (Small's payload is the
    // 4-mod-8 one that must go through the recompute).
    println!(
        "m1 = {} m2 = {}",
        describe_mixed(Mixed::Small(0xC0FFEE)),
        describe_mixed(Mixed::Big(Box::new("mixed-box".to_string())))
    );

    let mut m3 = Mixed::Small(5);
    let mut m2 = Mixed::Big(Box::new("mixed-swap".to_string()));
    swap(&mut m3, &mut m2);
    let mixed_swap = (describe_mixed(m3), describe_mixed(m2));
    println!("mixed swap = {} {}", mixed_swap.0, mixed_swap.1);

    let mut m3 = Mixed::Small(6);
    let mprev = replace(&mut m3, Mixed::Big(Box::new("m-repl".to_string())));
    println!("mixed replace = {} now = {}", describe_mixed(mprev), describe_mixed(m3));

    let mv: Vec<Mixed> = vec![Mixed::Small(9), Mixed::Big(Box::new("mv".to_string()))];
    let mout: Vec<String> = mv.into_iter().map(describe_mixed).collect();
    println!("mixed vec = {}", mout.join(", "));

    println!("eq = {}", Tagged::Num(5) == Tagged::Num(5) && Mixed::Small(5) != Mixed::Small(6));
    println!("debug = {:?} {:?}", make_tagged(3, "dbg"), Mixed::Small(44));
}
