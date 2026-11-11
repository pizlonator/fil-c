// `#[repr(i16)]` pointer-maybe enums: the ICE + runtime guard for the
// explicit-int-repr arm of the Fil-C enum-layout fix (change #24).
//
// ICE guard: this shape FAILS TO COMPILE (rustc exits 101 with the
// rustc_abi layout-calculator assert "layout decided on a larger
// discriminant type (I64) than typeck (I8/I16)") if the Fil-C
// discriminant bump is applied to explicit reprs on the first layout
// computation, so any return of that bug is caught here at compile time.
//
// Runtime guard: Wide's payload (a String) is already word-aligned on the
// first computation (the 2-byte signed tag leaves it at 8) and Signed's
// u32 payload lands at 4 mod 8, so Signed must go through the recompute
// (word-sized tag over a NEGATIVE discriminant, exercising the signed-tag
// machinery).  The integer-only variant payloads at 4 mod 8 must not
// pizlonator-panic at runtime.
use std::mem::{replace, swap};

// The review's shape: a signed 2-byte tag and a String payload.
#[repr(i16)]
#[derive(Debug, PartialEq)]
enum Wide {
    A(String),
    B,
}

// A signed-int repr whose integer-only payload is NOT word-aligned on the
// first computation, plus an explicitly negative discriminant: this is the
// shape that must go through the recompute with the sign preserved.
#[repr(i16)]
#[derive(Debug, PartialEq)]
enum Signed {
    Small(u32) = 3,
    Big(Box<String>),
    Neg = -5,
}

#[inline(never)]
fn make_wide(s: &str) -> Wide {
    if s.is_empty() {
        Wide::B
    } else {
        Wide::A(s.to_string())
    }
}

#[inline(never)]
fn describe_wide(w: Wide) -> String {
    match w {
        Wide::A(s) => format!("A({})", s),
        Wide::B => "B".to_string(),
    }
}

#[inline(never)]
fn describe_signed(s: Signed) -> String {
    match s {
        Signed::Small(v) => format!("Small({})", v),
        Signed::Big(b) => format!("Big({})", b),
        Signed::Neg => "Neg".to_string(),
    }
}

fn main() {
    // Both Wide variants, by value through a call (sret move) and a match.
    println!("a = {} b = {}", describe_wide(make_wide("i16-string")), describe_wide(make_wide("")));

    // Vec<Wide>: whole enums move through the vector's memory.
    let mut v = Vec::new();
    v.push(make_wide("vec-a"));
    v.push(make_wide(""));
    v.push(make_wide("vec-c"));
    let out: Vec<String> = v.into_iter().map(describe_wide).collect();
    println!("vec = {}", out.join(", "));

    // mem::swap and mem::replace on Wide.
    let mut x = make_wide("swap-me");
    let mut y = make_wide("");
    swap(&mut x, &mut y);
    let swapped = (describe_wide(x), describe_wide(y));
    println!("swapped x = {} y = {}", swapped.0, swapped.1);

    let mut x = make_wide("replace-me");
    let prev = replace(&mut x, make_wide("replaced"));
    println!("replaced prev = {} now = {}", describe_wide(prev), describe_wide(x));

    // Signed: both payload variants and the negative-discriminant unit
    // variant, through the same by-value moves.
    println!(
        "s1 = {} s2 = {} s3 = {}",
        describe_signed(Signed::Small(0xCAFE_F00D)),
        describe_signed(Signed::Big(Box::new("signed-box".to_string()))),
        describe_signed(Signed::Neg)
    );
    let sv: Vec<Signed> =
        vec![Signed::Small(9), Signed::Neg, Signed::Big(Box::new("sv".to_string()))];
    let sout: Vec<String> = sv.into_iter().map(describe_signed).collect();
    println!("signed vec = {}", sout.join(", "));

    let mut s4 = Signed::Small(1);
    let mut s5 = Signed::Big(Box::new("signed-swap".to_string()));
    swap(&mut s4, &mut s5);
    let signed_swap = (describe_signed(s4), describe_signed(s5));
    println!("signed swap = {} {}", signed_swap.0, signed_swap.1);

    let mut s4 = Signed::Small(2);
    let sprev = replace(&mut s4, Signed::Neg);
    println!("signed replace = {} now = {}", describe_signed(sprev), describe_signed(s4));

    println!("eq = {}", Wide::B == Wide::B && Signed::Neg != Signed::Small(0xFFFF_FFFF));
    println!("debug = {:?} {:?}", make_wide("dbg"), Signed::Small(44));
}
