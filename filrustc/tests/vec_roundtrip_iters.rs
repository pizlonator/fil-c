// Vec <-> iterator round-trips of pointer-containing elements: IntoIter,
// collect, rev, zip, enumerate, windows, chunks, chain, flat_map.
fn main() {
    let v: Vec<Box<String>> = (0..6).map(|i| Box::new(format!("it{}", i))).collect();
    let v2: Vec<Box<String>> = v.into_iter().collect();
    println!("roundtrip: {}", v2.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    let rev: Vec<String> = v2.iter().rev().map(|b| (**b).clone()).collect();
    println!("rev: {}", rev.join(","));

    let (front, back): (Box<String>, Vec<Box<String>>) = {
        let mut it = v2.into_iter();
        let f = it.next().unwrap();
        let rest: Vec<Box<String>> = it.collect();
        (f, rest)
    };
    println!("front = {} back = {}", front, back.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // zip two vecs of different pointer-bearing element types
    let nums: Vec<u32> = vec![1, 2, 3];
    let strs: Vec<String> = vec!["one".into(), "two".into(), "three".into()];
    let zipped: Vec<(u32, String)> = nums.into_iter().zip(strs).collect();
    println!("zip: {}", zipped.iter().map(|(n, s)| format!("{}={}", n, s)).collect::<Vec<_>>().join(" "));

    // windows and chunks over pointer elements
    let base: Vec<String> = (0..6).map(|i| format!("w{}", i)).collect();
    let wins: Vec<Vec<String>> = base.windows(3).map(|w| w.to_vec()).collect();
    println!("windows: {}", wins.iter().map(|w| w.join("+")).collect::<Vec<_>>().join(" | "));
    let chunks: Vec<Vec<String>> = base.chunks(4).map(|c| c.to_vec()).collect();
    println!("chunks: {}", chunks.iter().map(|c| c.join("/")).collect::<Vec<_>>().join(" | "));

    // chain + filter + map + collect with boxed payloads
    let a: Vec<Box<u32>> = vec![Box::new(1), Box::new(2)];
    let b: Vec<Box<u32>> = vec![Box::new(3), Box::new(4)];
    let chained: Vec<u32> = a.into_iter().chain(b).map(|bx| *bx * 10).collect();
    println!("chain: {}", chained.iter().map(|x| x.to_string()).collect::<Vec<_>>().join(","));

    // flat_map producing Strings from nested data
    let nested: Vec<Vec<String>> = vec![vec!["n1".into(), "n2".into()], vec!["n3".into()]];
    let flat: Vec<String> = nested.into_iter().flat_map(|inner| inner).collect();
    println!("flat: {}", flat.join(","));
}
