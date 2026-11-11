// sort_by_key / sort_unstable_by_key on pointer-maybe enums at -O: the
// reviewer's miscompile (round 3).
//
// The bug: `Vec<E>::sort_by_key(|e| match e { E::A(_) => 0, E::B(a, _) => *a })`
// with `#[repr(u64)] enum E { A(Box<String>), B(u32, u32) }` corrupted the
// moved elements' `Box<String>` capability at -O: the key-extraction closure
// was inlined into the sort machinery (insert_tail for the stable sort, the
// ipnsort partition for the unstable sort), and the Fil-C pre-pizlonation
// pipeline then collapsed the machinery's element save/restore memcpys into
// integer load/store pairs.  Integer-typed stores move only the payload under
// GIMSO, so the Box pointer's capability stayed behind in the source element's
// shadow; the subsequent drop (or any read) of the moved element then hit
// "filc safety error: cannot read pointer with null object" (exit 133/42).
// The fix keeps the element save/restore boundaries out of the comparator's
// body: #[inline(never)] on insert_tail, on the two unstable partition
// functions, and on their GapGuard/GapGuardRaw/CopyOnDrop drop impls.
//
// This test runs the reviewer's program EXTENDED across the reprs (u64, u8,
// Rust), both key-sorted APIs (stable and unstable), and 3- and 100-element
// Vecs, and prints the post-sort payloads (String contents + integer fields)
// so the manifest asserts the exact sorted order.  Every element has a unique
// key, so even the unstable sort's output is fully determined.  A capability
// drop anywhere in the move path fails at runtime with exit 42 here.
//
// (All keys are unique so that sort_unstable_by_key's output is deterministic:
// one A variant with key 0 and B(a, _) variants with a = 1..=99.)

fn key64(e: &E64) -> u32 {
    match e {
        E64::A(_) => 0,
        E64::B(a, _) => *a,
    }
}

#[repr(u64)]
enum E64 {
    A(Box<String>),
    B(u32, u32),
}

#[repr(u8)]
enum E8 {
    A(Box<String>),
    B(u32, u32),
}

#[repr(Rust)]
enum ER {
    A(Box<String>),
    B(u32, u32),
}

fn show64(v: &[E64]) -> String {
    let mut s = String::new();
    for e in v {
        match e {
            E64::A(b) => s.push_str(&format!("A({}),", &**b)),
            E64::B(a, c) => s.push_str(&format!("B({},{})/", a, c)),
        }
    }
    s
}

fn show8(v: &[E8]) -> String {
    let mut s = String::new();
    for e in v {
        match e {
            E8::A(b) => s.push_str(&format!("A({}),", &**b)),
            E8::B(a, c) => s.push_str(&format!("B({},{})/", a, c)),
        }
    }
    s
}

fn showr(v: &[ER]) -> String {
    let mut s = String::new();
    for e in v {
        match e {
            ER::A(b) => s.push_str(&format!("A({}),", &**b)),
            ER::B(a, c) => s.push_str(&format!("B({},{})/", a, c)),
        }
    }
    s
}

// The reviewer's exact 3-element vector: [B(1,2), A("x"), B(3,4)].
fn data3_64() -> Vec<E64> {
    vec![E64::B(1, 2), E64::A(Box::new("x".into())), E64::B(3, 4)]
}

// 100 elements, unique keys: one A (key 0) and B(i, i*7) for i = 1..=99,
// scrambled so the sorts have work to do.
fn data100_64() -> Vec<E64> {
    let mut v: Vec<E64> = vec![E64::A(Box::new("hundred".into()))];
    for i in 1..100u32 {
        v.push(E64::B(i, i * 7));
    }
    v.reverse();
    v
}

fn main() {
    // repr(u64) + sort_by_key + 3 elements (the reviewer's program).
    let mut v = data3_64();
    v.sort_by_key(key64);
    println!("sbk3   = {}", show64(&v));
    drop(v);

    // repr(u64) + sort_by_key + 100 elements.
    let mut v = data100_64();
    v.sort_by_key(key64);
    println!("sbk100 = {}", show64(&v));
    drop(v);

    // repr(u64) + sort_unstable_by_key + 3 elements.
    let mut v = data3_64();
    v.sort_unstable_by_key(key64);
    println!("subk3  = {}", show64(&v));
    drop(v);

    // repr(u64) + sort_unstable_by_key + 100 elements (the ipnsort partition
    // path).
    let mut v = data100_64();
    v.sort_unstable_by_key(key64);
    println!("subk100= {}", show64(&v));
    drop(v);

    // repr(u8): the explicit-int-repr layout (word tag from the change #24
    // recompute), 3 elements through both APIs.
    let mut v: Vec<E8> =
        vec![E8::B(1, 2), E8::A(Box::new("eight".into())), E8::B(3, 4)];
    v.sort_by_key(|e| match e {
        E8::A(_) => 0u32,
        E8::B(a, _) => *a,
    });
    println!("e8sbk  = {}", show8(&v));
    drop(v);
    let mut v: Vec<E8> =
        vec![E8::B(1, 2), E8::A(Box::new("eight".into())), E8::B(3, 4)];
    v.sort_unstable_by_key(|e| match e {
        E8::A(_) => 0u32,
        E8::B(a, _) => *a,
    });
    println!("e8subk = {}", show8(&v));
    drop(v);

    // repr(Rust): the repr the compiler picks itself, 3 elements through both
    // APIs.
    let mut v: Vec<ER> = vec![ER::B(1, 2), ER::A(Box::new("rust".into())), ER::B(3, 4)];
    v.sort_by_key(|e| match e {
        ER::A(_) => 0u32,
        ER::B(a, _) => *a,
    });
    println!("ersbk  = {}", showr(&v));
    drop(v);
    let mut v: Vec<ER> = vec![ER::B(1, 2), ER::A(Box::new("rust".into())), ER::B(3, 4)];
    v.sort_unstable_by_key(|e| match e {
        ER::A(_) => 0u32,
        ER::B(a, _) => *a,
    });
    println!("ersubk = {}", showr(&v));
    drop(v);

    println!("sort_by_key_enum OK");
}
