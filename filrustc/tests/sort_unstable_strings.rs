// slice::sort_unstable (ipnsort): quicksort partitioning moves 24-byte
// String elements through in-place copies; 8-byte Box elements through the
// small-sort networks.  Contents must be intact after sorting.
fn main() {
    let mut v: Vec<String> = Vec::new();
    for i in 0..100u32 {
        v.push(format!("s{}", (i * 53) % 97));
    }
    v.sort_unstable();
    println!("first = {} last = {} len = {}", v[0], v[99], v.len());
    let sorted = {
        let mut c = v.clone();
        c.sort();
        c == v
    };
    println!("matches stable = {}", sorted);

    let mut w: Vec<Box<String>> = (0..50u32).map(|i| Box::new(format!("b{}", (i * 11) % 49))).collect();
    w.sort_unstable_by(|a, b| a.as_str().cmp(b.as_str()));
    let uniq: Vec<&str> = w.iter().map(|b| b.as_str()).collect();
    // dedup the printed view; sort_unstable may reorder equal elements only
    let mut seen = std::collections::BTreeSet::new();
    let mut out = String::new();
    for s in uniq {
        if seen.insert(s.to_string()) {
            out.push_str(s);
            out.push(' ');
        }
    }
    println!("boxed = {}", out.trim_end());

    // sort_unstable of structs containing pointers, by pointer-free key
    struct Rec { name: String, id: u32, extra: Box<u64> }
    let mut recs: Vec<Rec> = (0..30u32).map(|i| Rec {
        name: format!("n{}", (i * 13) % 29),
        id: i,
        extra: Box::new(i as u64),
    }).collect();
    recs.sort_unstable_by_key(|r| r.name.clone());
    let mut names = String::new();
    for r in &recs { names.push_str(&r.name); names.push(' '); }
    println!("recs = {}", names.trim_end());
    let ids_ascending = recs.windows(2).all(|p| p[0].id < p[1].id || p[0].name != p[1].name);
    println!("stable-ish = {}", ids_ascending);
}
