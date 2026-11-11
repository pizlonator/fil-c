use std::collections::HashSet;

fn sorted(mut v: Vec<String>) -> String {
    v.sort();
    v.join(",")
}

fn main() {
    let mut a: HashSet<String> = ["apple", "banana", "cherry"].iter().map(|s| s.to_string()).collect();
    let b: HashSet<String> = ["banana", "date"].iter().map(|s| s.to_string()).collect();
    println!("len = {} has = {}", a.len(), a.contains("banana"));
    let union: Vec<String> = a.union(&b).cloned().collect();
    println!("union = {}", sorted(union));
    let inter: Vec<String> = a.intersection(&b).cloned().collect();
    println!("intersection = {}", sorted(inter));
    let diff: Vec<String> = a.difference(&b).cloned().collect();
    println!("difference = {}", sorted(diff));
    let sym: Vec<String> = a.symmetric_difference(&b).cloned().collect();
    println!("symmetric = {}", sorted(sym));

    a.retain(|s| s.len() != 5);
    let retained: Vec<String> = a.iter().cloned().collect();
    println!("retained = {}", sorted(retained));

    let mut c: HashSet<Box<String>> = HashSet::new();
    for i in 0..6 {
        c.insert(Box::new(format!("bx{}", i)));
    }
    println!("boxed len = {}", c.len());
    let drained: Vec<String> = c.drain().map(|b| b.as_str().to_string()).collect();
    println!("drained = {} empty = {}", sorted(drained), c.is_empty());

    let mut d: HashSet<i64> = HashSet::new();
    for i in 0..50 { d.insert(i * 7 % 23); }
    println!("mod set len = {}", d.len());
    let taken = d.take(&7);
    println!("take 7 = {:?} still has = {}", taken, d.contains(&7));
    let words: Vec<String> =
        "one two three four five six seven eight nine ten".split_whitespace().map(|w| w.to_string()).collect();
    let wset: HashSet<String> = words.iter().cloned().collect();
    println!("dedup = {} -> {}", words.len(), wset.len());
}
