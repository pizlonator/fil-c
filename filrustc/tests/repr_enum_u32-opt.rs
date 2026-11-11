// `#[repr(u32)]` pointer-maybe enums: the ICE + runtime guard for the
// explicit-int-repr arm of the Fil-C enum-layout fix (change #24).
//
// ICE guard: this shape FAILS TO COMPILE (rustc exits 101 with the
// rustc_abi layout-calculator assert "layout decided on a larger
// discriminant type (I64) than typeck (I8/I16/I32)") if the Fil-C
// discriminant bump is applied to explicit reprs on the first layout
// computation, so any return of that bug is caught here at compile time.
//
// Runtime guard: the Vec payload of E is already word-aligned on the
// first computation (the 4-byte tag leaves it at 8), Large checks a
// near-u32::MAX discriminant, and Mixed's u32 payload lands at 4 mod 8,
// so Mixed must go through the recompute (word-sized tag).  The
// integer-only variant payloads at 4 mod 8 must not pizlonator-panic at
// runtime.
use std::mem::{replace, swap};

#[repr(u32)]
#[derive(Debug, PartialEq)]
enum E {
    A(Vec<u8>),
    B,
}

// A 4-byte tag whose integer-only payload is NOT word-aligned on the
// first computation: this is the shape that must go through the recompute.
#[repr(u32)]
#[derive(Debug, PartialEq)]
enum Mixed {
    Small(u32),
    Big(Box<String>),
}

// A discriminant value near the top of the repr's range.
#[repr(u32)]
#[derive(Debug, PartialEq)]
enum Large {
    Hi = 4_000_000_000,
    Lo = 1,
    Unit,
}

#[inline(never)]
fn make_e(bytes: &[u8]) -> E {
    if bytes.is_empty() {
        E::B
    } else {
        E::A(bytes.to_vec())
    }
}

#[inline(never)]
fn describe_e(e: E) -> String {
    match e {
        E::A(v) => format!("A({:?})", v),
        E::B => "B".to_string(),
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
    // Both E variants, by value through a call (sret move) and a match.
    println!("a = {} b = {}", describe_e(make_e(&[1, 2, 3, 4])), describe_e(make_e(&[])));

    // Vec<E>: whole enums move through the vector's memory.
    let mut v = Vec::new();
    v.push(make_e(&[9, 8]));
    v.push(make_e(&[]));
    v.push(make_e(&[7, 6, 5, 4, 3, 2, 1, 0]));
    let out: Vec<String> = v.into_iter().map(describe_e).collect();
    println!("vec = {}", out.join(", "));

    // mem::swap and mem::replace on E.
    let mut x = make_e(&[42]);
    let mut y = make_e(&[]);
    swap(&mut x, &mut y);
    let swapped = (describe_e(x), describe_e(y));
    println!("swapped x = {} y = {}", swapped.0, swapped.1);

    let mut x = make_e(&[100]);
    let prev = replace(&mut x, make_e(&[200]));
    println!("replaced prev = {} now = {}", describe_e(prev), describe_e(x));

    // Mixed: both payload variants through the same by-value moves
    // (Small's payload is the 4-mod-8 one that must go through the
    // recompute).
    println!(
        "m1 = {} m2 = {}",
        describe_mixed(Mixed::Small(0xFACE_B00C)),
        describe_mixed(Mixed::Big(Box::new("u32-box".to_string())))
    );

    let mut m3 = Mixed::Small(5);
    let mut m2 = Mixed::Big(Box::new("u32-swap".to_string()));
    swap(&mut m3, &mut m2);
    let mixed_swap = (describe_mixed(m3), describe_mixed(m2));
    println!("mixed swap = {} {}", mixed_swap.0, mixed_swap.1);

    let mut m3 = Mixed::Small(6);
    let mprev = replace(&mut m3, Mixed::Big(Box::new("m-repl".to_string())));
    println!("mixed replace = {} now = {}", describe_mixed(mprev), describe_mixed(m3));

    let mv: Vec<Mixed> = vec![Mixed::Small(9), Mixed::Big(Box::new("mv".to_string()))];
    let mout: Vec<String> = mv.into_iter().map(describe_mixed).collect();
    println!("mixed vec = {}", mout.join(", "));

    // Large discriminants still compare and match correctly.
    println!(
        "large = {} {} {}",
        Large::Hi as u32,
        Large::Lo as u32,
        match Large::Unit { Large::Hi => "hi", Large::Lo => "lo", Large::Unit => "unit" }
    );
    println!("eq = {}", E::B == E::B && Mixed::Small(5) != Mixed::Small(6) && Large::Hi != Large::Lo);
    println!("debug = {:?} {:?} {:?}", make_e(&[1]), Mixed::Small(44), Large::Hi);
}
