// `#[repr(u8)]` pointer-maybe enums: the ICE + runtime guard for the
// explicit-int-repr arm of the Fil-C enum-layout fix (change #24).
//
// ICE guard: this shape FAILS TO COMPILE (rustc exits 101 with the
// rustc_abi layout-calculator assert "layout decided on a larger
// discriminant type (I64) than typeck (I8)") if the Fil-C discriminant
// bump is applied to explicit reprs on the first layout computation, so
// any return of that bug is caught here at compile time.
//
// Runtime guard: with the fix in place the first layout computation keeps
// the user's 1-byte tag and lands the integer-only variant's payload at
// 4 mod 8 (the misalignment hazard); the recompute path must then widen
// the tag to a word so the FIRST payload byte of every variant starts
// word-aligned (later fields of a multi-field variant may stay at 4 mod 8;
// that is safe because they are only touched with integer-typed accesses).
// The program
// RUNS both variants through by-value moves (Vec growth, sret returns,
// mem::swap/mem::replace) and prints the payloads: the integer-only
// variant payloads at 4 mod 8 must not pizlonator-panic at runtime.
use std::mem::{replace, swap};

#[repr(u8)]
#[derive(Debug, PartialEq)]
enum E {
    A(Box<String>),
    B(u32),
}

// Return-by-value moves the whole enum through the sret slot.
#[inline(never)]
fn make(s: &str, v: u32) -> E {
    if v == 0 {
        E::A(Box::new(s.to_string()))
    } else {
        E::B(v)
    }
}

// Match-driven extraction, consuming the enum by value.
#[inline(never)]
fn describe(e: E) -> String {
    match e {
        E::A(boxed) => format!("A({})", boxed),
        E::B(v) => format!("B({})", v),
    }
}

fn main() {
    // Both variants, by value through a call (sret move) and a match.
    println!("a = {} b = {}", describe(make("hello", 0)), describe(make("ignored", 0xDEAD_BEEF)));

    // Vec<E>: whole enums move through the vector's memory (a growth
    // realloc memmoves entire elements), then come back out by value.
    let mut v = Vec::new();
    v.push(make("vec-a", 0));
    v.push(make("vec-b", 7));
    v.push(make("vec-c", 0));
    let out: Vec<String> = v.into_iter().map(describe).collect();
    println!("vec = {}", out.join(", "));

    // mem::swap between two locals of different variants.
    let mut x = make("swap-me", 0);
    let mut y = make("x", 11);
    swap(&mut x, &mut y);
    let swapped = (describe(x), describe(y));
    println!("swapped x = {} y = {}", swapped.0, swapped.1);

    // mem::replace across variants.
    let mut x = make("replace-me", 0);
    let prev = replace(&mut x, make("fresh", 22));
    println!("replaced prev = {} now = {}", describe(prev), describe(x));

    // Both variants again, matched in a loop over a moved array.
    for e in [make("iter-a", 0), make("iter-b", 33)] {
        match e {
            E::A(boxed) => println!("iter A len = {}", boxed.len()),
            E::B(v) => println!("iter B = {}", v),
        }
    }

    // PartialEq across variants (compares the discriminants + payloads).
    println!("eq = {} neq = {}", E::B(5) == E::B(5), E::B(5) != E::A(Box::new("5".into())));
    println!("debug = {:?}", make("dbg", 44));
}
