// mem::replace / mem::take on pointer-containing values: these are load/store
// moves of whole aggregates (typed allocas), plus Vec::swap_remove and
// Option::take/replace on large payloads.
use std::mem::{replace, take};

#[inline(never)]
fn make(id: u32, tag: &str) -> Payload {
    Payload {
        name: tag.to_string(),
        ids: vec![id, id + 1, id + 2],
        ptr: Box::new(id as u64 * 10),
    }
}

#[derive(Clone)]
struct Payload {
    name: String,
    ids: Vec<u32>,
    ptr: Box<u64>,
}

impl std::fmt::Display for Payload {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}[{};{}]", self.name, self.ids.iter().map(|i| i.to_string()).collect::<Vec<_>>().join(","), self.ptr)
    }
}

fn main() {
    let mut p = make(1, "one");
    let old = replace(&mut p, make(2, "two"));
    println!("new = {}, old = {}", p, old);

    // mem::take on String and Vec
    let mut s = "held".to_string();
    let got = take(&mut s);
    println!("taken string = {:?}, s = {:?}", got, s);
    let mut v = vec!["x".to_string(), "y".to_string()];
    let gotv = take(&mut v);
    println!("taken vec len = {}, v len = {}", gotv.len(), v.len());

    // mem::replace on Option<large payload> (None is the default here)
    let mut opt = Some(make(3, "three"));
    let prev = opt.take();
    println!("opt now {:?}, prev = {:?}", opt.is_none(), prev.map(|x| x.to_string()).unwrap_or_default());

    // mem::replace with a specific new value
    let mut opt2 = Some(make(4, "four"));
    let prev2 = replace(&mut opt2, Some(make(5, "five")));
    println!("opt2 = {:?}, prev2 = {}", opt2.map(|x| x.to_string()).unwrap(), prev2.map(|x| x.to_string()).unwrap());

    // Vec::swap_remove moves elements both by swap and by memmove semantics
    let mut vv = vec![make(6, "six"), make(7, "seven"), make(8, "eight")];
    let removed = vv.swap_remove(0);
    println!("swap_remove -> {}, rest = {}", removed, vv.iter().map(|x| x.to_string()).collect::<Vec<_>>().join(" | "));

    // replace on a 24-byte enum payload
    enum E { A(String), B(Vec<Box<String>>) }
    let mut e = E::A("enum-a".to_string());
    let old_e = replace(&mut e, E::B(vec![Box::new("boxed".to_string())]));
    match e {
        E::B(v) => println!("e is B with {}", v.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(",")),
        E::A(_) => println!("e is A?!"),
    }
    match old_e {
        E::A(s) => println!("old e was A {}", s),
        E::B(_) => println!("old e was B?!"),
    }
}
