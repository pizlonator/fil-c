// sort_by (comparator-based, driftsort) on pointer-maybe enums at -O: the
// round-3 sibling check for the sort_by_key miscompile.
//
// sort_by feeds the comparator closure `|a, b| compare(a, b) == Less` into the
// same driftsort machinery (insert_tail's small sorts, the stable quicksort's
// pivot copy, the merge), so a capability-dropping element move anywhere in
// that machinery breaks it exactly like sort_by_key.  The reviewer flagged
// sort_by-with-#[inline]-comparators as a possible same-class case; this test
// pins the comparator path with pointer-containing enum elements at 3 and 100
// elements, at both the debug level and -O, and asserts the exact sorted
// output (all comparison results are unique, so the order is fully
// determined).
//
// The pivot copy in the stable quicksort (`ManuallyDrop::new(ptr::read(...))`)
// is read-only -- it never writes the saved element back -- so the machinery
// only needs the element-move fixes (insert_tail et al.) to keep capabilities
// intact here; but if any of those boundaries is ever removed, the moved
// elements' Box capabilities die and this test panics with exit 42.

#[repr(u64)]
enum E {
    A(Box<String>),
    B(u32, u32),
}

fn key(e: &E) -> u32 {
    match e {
        E::A(_) => 0,
        E::B(a, _) => *a,
    }
}

fn show(v: &[E]) -> String {
    let mut s = String::new();
    for e in v {
        match e {
            E::A(b) => s.push_str(&format!("A({}),", &**b)),
            E::B(a, c) => s.push_str(&format!("B({},{})/", a, c)),
        }
    }
    s
}

fn data3() -> Vec<E> {
    vec![E::B(1, 2), E::A(Box::new("x".into())), E::B(3, 4)]
}

// 100 elements, unique keys (one A with key 0, B(i, i*7) for i = 1..=99),
// scrambled.
fn data100() -> Vec<E> {
    let mut v: Vec<E> = vec![E::A(Box::new("hundred".into()))];
    for i in 1..100u32 {
        v.push(E::B(i, i * 7));
    }
    v.reverse();
    v
}

fn main() {
    // The reviewer-shaped 3-element sort_by with a closure comparator whose
    // body is small enough to inline.
    let mut v = data3();
    v.sort_by(|a, b| key(a).cmp(&key(b)));
    println!("sby3   = {}", show(&v));
    drop(v);

    // 100 elements: driftsort's quicksort + merge paths.
    let mut v = data100();
    v.sort_by(|a, b| key(a).cmp(&key(b)));
    println!("sby100 = {}", show(&v));
    drop(v);

    // Descending re-sort of the already-sorted vector (driftsort's
    // already-sorted detection + reverse run handling), then back.
    let mut v = data100();
    v.sort_by(|a, b| key(b).cmp(&key(a)));
    println!("sbydesc= {}", show(&v));
    v.sort_by(|a, b| key(a).cmp(&key(b)));
    println!("sbyasc = {}", show(&v));
    drop(v);

    println!("sort_by_comparator OK");
}
