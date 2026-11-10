// mem::swap family on SMALL pointer-containing aggregates (<= 24 bytes):
// the size regime where upstream std's byte-chunk swap would have dropped
// capabilities (fixed by change #22's pointer-typed chunks).  Everything
// here must print deterministic output at every opt level.
use std::mem::swap;

#[inline(never)]
fn make(id: u32, name: &str) -> Rec {
    Rec { name: name.to_string(), id }
}

#[derive(Clone)]
struct Rec {
    name: String,
    id: u32,
}

impl std::fmt::Display for Rec {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.name, self.id)
    }
}

#[inline(never)]
fn dump(recs: &[Rec]) -> String {
    recs.iter().map(|r| r.to_string()).collect::<Vec<_>>().join(" ")
}

fn main() {
    // swap two 24-byte structs held in locals
    let mut a = make(1, "alpha");
    let mut b = make(2, "beta");
    swap(&mut a, &mut b);
    println!("pair: {} {}", a, b);

    // swap whole arrays of them (stack allocas, 24-byte elements)
    let mut arr = [make(3, "c"), make(4, "d"), make(5, "e"), make(6, "f")];
    {
        let (l, r) = arr.split_at_mut(3);
        swap(&mut l[0], &mut r[0]);
    }
    let (l, r) = arr.split_at_mut(2);
    swap(&mut l[1], &mut r[0]);
    println!("array: {}", dump(&arr));

    // swap through a Vec (heap-backed)
    let mut v = vec![make(7, "g"), make(8, "h")];
    {
        let (l, r) = v.split_at_mut(1);
        swap(&mut l[0], &mut r[0]);
    }
    println!("vec: {}", dump(&v));

    // swap 16-byte Option<&str> (fat-pointer-free niche enum) and 16-byte
    // (String, u32) tuples
    let s1 = "one".to_string();
    let s2 = "two".to_string();
    let mut o1 = Some(&s1[..]);
    let mut o2 = Some(&s2[..]);
    swap(&mut o1, &mut o2);
    println!("opts: {:?} {:?}", o1, o2);

    let mut t1 = (s1.clone(), 11u32);
    let mut t2 = (s2.clone(), 22u32);
    swap(&mut t1, &mut t2);
    println!("tuples: ({}, {}) ({}, {})", t1.0, t1.1, t2.0, t2.1);

    // repeated swapping must not corrupt anything
    let mut x = make(9, "x");
    let mut y = make(10, "y");
    for i in 0..7 {
        swap(&mut x, &mut y);
        if i % 2 == 0 {
            println!("loop: {} {}", x, y);
        }
    }
}
