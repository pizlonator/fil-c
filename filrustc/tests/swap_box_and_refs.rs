// mem::swap of 8-byte pointer ELEMENTS (Box<T>, Option<&T>): the exact size
// class where a numeric-typed swap chunk would widen to a single i64 and drop
// the capability.  Guards std change #22 at the smallest element size.
use std::mem::swap;

#[inline(never)]
fn swap_slice(v: &mut [Box<String>]) {
    // reverse in place using pairwise swaps of 8-byte elements
    let n = v.len();
    for i in 0..n / 2 {
        let (l, r) = v.split_at_mut(n - 1 - i);
        swap(&mut l[i], &mut r[0]);
    }
}

fn main() {
    let mut v: Vec<Box<String>> = Vec::new();
    for i in 0..12 {
        v.push(Box::new(format!("e{:02}", i)));
    }
    swap_slice(&mut v);
    let out: Vec<&str> = v.iter().map(|b| b.as_str()).collect();
    println!("rev: {}", out.join(" "));

    // swap of Box<T> locals (the two 8-byte words are bare Box pointers)
    let mut a: Box<String> = Box::new("first".to_string());
    let mut b: Box<String> = Box::new("second".to_string());
    swap(&mut a, &mut b);
    println!("boxes: {} {}", a, b);

    // swap &mut T pairs through references: replace/swap over indirection
    let mut s1 = "ref-one".to_string();
    let mut s2 = "ref-two".to_string();
    {
        let r1 = &mut s1;
        let r2 = &mut s2;
        swap(r1, r2);
    }
    println!("refs: {} {}", s1, s2);

    // swap 8-byte Option<&str> elements inside an array
    let strs = ["p0", "p1", "p2", "p3"];
    let mut arr: [Option<&str>; 4] = [Some(strs[0]), Some(strs[1]), Some(strs[2]), Some(strs[3])];
    for i in 0..2 {
        let (l, r) = arr.split_at_mut(3 - i);
        swap(&mut l[i], &mut r[0]);
    }
    let outs: Vec<String> = arr.iter().map(|o| o.unwrap().to_string()).collect();
    println!("opts: {}", outs.join(" "));

    // swap of Box<dyn Fn> (16-byte fat pointer) via a Vec
    let mut closures: Vec<Box<dyn Fn() -> String>> = Vec::new();
    closures.push(Box::new(|| "cb-a".to_string()));
    closures.push(Box::new(|| "cb-b".to_string()));
    {
        let (l, r) = closures.split_at_mut(1);
        swap(&mut l[0], &mut r[0]);
    }
    println!("closures: {} {}", closures[0](), closures[1]());
}
