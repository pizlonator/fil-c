// thread::spawn joining a LARGE (>64 byte) return value: the spawn+join
// flight path is the exact shape the rustc-side typed-allocas fix guards
// (known-issues #1(a)); this stresses it with a big pointer-bearing payload.
use std::thread;

#[inline(never)]
fn make_big(id: u32) -> BigReturn {
    BigReturn {
        name: format!("ret-{}", id),
        first: Box::new(id as u64 * 1000),
        words: [id as u64, id as u64 + 1, id as u64 + 2, id as u64 + 3, id as u64 + 4],
        tail: Box::new((id as u64) * 2),
    }
}

struct BigReturn {
    name: String,
    first: Box<u64>,
    words: [u64; 5],
    tail: Box<u64>,
}

impl std::fmt::Display for BigReturn {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {:?} {}", self.name, self.first, self.words, self.tail)
    }
}

fn main() {
    println!("size = {}", std::mem::size_of::<BigReturn>());
    let mut handles = Vec::new();
    for id in 0..4u32 {
        handles.push(thread::spawn(move || {
            let big = make_big(id);
            big.to_string()
        }));
    }
    let mut results: Vec<String> = Vec::new();
    for h in handles {
        results.push(h.join().unwrap());
    }
    results.sort();
    for r in &results {
        println!("joined = {}", r);
    }
}
