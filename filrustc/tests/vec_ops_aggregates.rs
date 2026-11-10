// Vec operations that move pointer-containing elements around: insert,
// remove, drain, splice, append, extend_from_slice, truncate + shrink.
fn main() {
    let mut v: Vec<String> = vec!["a".into(), "b".into(), "c".into()];
    v.insert(1, "ins".into());
    println!("after insert: {}", v.join(","));

    let removed = v.remove(3);
    println!("removed {} -> {}", removed, v.join(","));

    let drained: Vec<String> = v.drain(1..3).collect();
    println!("drained: {} rest: {}", drained.join(","), v.join(","));

    v.extend_from_slice(&["ext1".into(), "ext2".into()]);
    println!("after extend: {}", v.join(","));

    let mut other = vec!["app1".into(), "app2".into()];
    v.append(&mut other);
    println!("after append: {} (other empty: {})", v.join(","), other.is_empty());

    let mut spliced = vec!["x".into(), "y".into(), "z".into()];
    let extracted: Vec<String> = spliced.splice(1..2, ["s1".into(), "s2".into()]).collect();
    println!("spliced: {} extracted: {}", spliced.join(","), extracted.join(","));

    // Box elements: insert/remove around 8-byte pointer words
    let mut bv: Vec<Box<String>> = vec![Box::new("B0".into()), Box::new("B1".into()), Box::new("B2".into())];
    bv.insert(2, Box::new("mid".into()));
    let gone = bv.remove(0);
    println!("box vec: {} gone = {}", bv.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","), gone);

    // truncate + shrink_to_fit; surviving elements stay usable
    bv.truncate(2);
    bv.shrink_to_fit();
    println!("truncated: {} cap>0: {}", bv.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","), bv.capacity() > 0);

    // with_capacity + push loop
    let mut cv: Vec<Box<String>> = Vec::with_capacity(4);
    for i in 0..10 { cv.push(Box::new(format!("c{}", i))); }
    println!("cv: {}", cv.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // retain keeps only some pointer-containing elements
    cv.retain(|b| !b.ends_with("8") && !b.ends_with("9"));
    println!("retained: {}", cv.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));
}
