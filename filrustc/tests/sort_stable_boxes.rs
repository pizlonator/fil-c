// slice::sort (driftsort) of 8-byte pointer elements: pushes elements through
// the small-sort's element-typed scratch AND the quicksort/stable_partition's
// byte-typed scratch ([MaybeUninit<u8>; 4096]) with per-element constant-size
// copies.  Every surviving element must still be usable (capability intact).
fn main() {
    let mut v: Vec<Box<String>> = Vec::new();
    for i in 0..60u32 {
        let n = ((i * 37) % 61).to_string();
        v.push(Box::new(n));
    }
    v.sort();
    let joined: Vec<&str> = v.iter().map(|b| b.as_str()).collect();
    println!("sorted = {}", joined.join(" "));
    let joined_text = joined.join(" ");
    drop(joined);

    // sort_by on the same data, descending, then re-sort ascending
    v.sort_by(|a, b| b.cmp(a));
    println!("desc = {}", v.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(" "));
    v.sort();
    let asc_text = v.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(" ");
    println!("check = {}", asc_text == joined_text);

    // small-sort path (<= 32 elements) with 8-byte elements
    let mut small: Vec<Box<u32>> = (0..12u32).map(|i| Box::new((i * 5) % 11)).collect();
    small.sort();
    println!("small = {}", small.iter().map(|b| b.to_string()).collect::<Vec<_>>().join(","));

    // 24-byte elements (String) through the same machinery
    let mut strs: Vec<String> = (0..40).map(|i| format!("s{:02}", (i * 7) % 43)).collect();
    strs.sort();
    println!("strs = {}", strs.join(" "));
}
