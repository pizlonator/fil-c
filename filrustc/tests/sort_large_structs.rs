// slice::sort / sort_by of LARGE (> 32 byte) pointer-containing structs:
// driftsort's scratch is element-typed so the sorts must be capability-safe
// at every size; this exercises sizes beyond the stack-scratch threshold
// (MAX_STACK_ARRAY_SIZE = 4096 => <= 4096/96 bytes use the stack scratch).
use std::fmt::Write;

#[derive(Clone)]
struct Big {
    name: String,
    boxed: Box<String>,
    ptr_words: [*mut u8; 2], // unused raw pointers, but pointer-typed words
    more: [u64; 8],
}

impl Big {
    #[inline(never)]
    fn new(id: u32) -> Big {
        Big {
            name: format!("big{:02}", (id * 17) % 31),
            boxed: Box::new(format!("bx{}", id % 7)),
            ptr_words: [std::ptr::null_mut(), std::ptr::null_mut()],
            more: [id as u64; 8],
        }
    }
}

fn main() {
    let mut v: Vec<Big> = (0..80u32).map(Big::new).collect();
    v.sort_by(|a, b| a.name.cmp(&b.name).then(a.more[0].cmp(&b.more[0])));
    let mut out = String::new();
    for r in &v {
        write!(out, "{}:{};", r.name, r.boxed).unwrap();
    }
    println!("sorted = {}", out);

    let ascending = v.windows(2).all(|p| p[0].name <= p[1].name);
    println!("ascending = {}", ascending);

    // sort_by_key with a String key clone
    v.sort_by_key(|r| r.boxed.clone());
    let mut out = String::new();
    for r in &v {
        write!(out, "{};", r.boxed).unwrap();
    }
    println!("bykey = {}", out);

    // sort_unstable of the same big structs
    v.sort_unstable_by(|a, b| a.name.cmp(&b.name));
    println!("first = {} last = {}", v[0].name, v[79].name);
}
