use std::collections::BTreeSet;

fn main() {
    let a: BTreeSet<String> = ["apple", "banana", "cherry"].iter().map(|s| s.to_string()).collect();
    let b: BTreeSet<String> = ["banana", "date", "cherry"].iter().map(|s| s.to_string()).collect();

    let union: Vec<String> = a.union(&b).cloned().collect();
    println!("union = {}", union.join(","));
    let inter: Vec<String> = a.intersection(&b).cloned().collect();
    println!("intersection = {}", inter.join(","));
    let diff: Vec<String> = a.difference(&b).cloned().collect();
    println!("difference = {}", diff.join(","));
    let sym: Vec<String> = a.symmetric_difference(&b).cloned().collect();
    println!("symmetric = {}", sym.join(","));

    let mut s: BTreeSet<Box<String>> = BTreeSet::new();
    s.insert(Box::new("held".to_string()));
    s.insert(Box::new("moved".to_string()));
    println!("boxed set = {}", s.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));
    let replaced = s.replace(Box::new("moved2".to_string()));
    println!("replace none = {}", replaced.is_none());
    let replaced2 = s.replace(Box::new("moved2".to_string()));
    println!("replace old = {}", replaced2.unwrap());
    let removed = s.remove(&Box::new("moved2".to_string()));
    println!("removed = {} len = {}", removed, s.len());

    let words: BTreeSet<String> =
        "the quick brown fox the lazy dog the end".split_whitespace().map(|w| w.to_string()).collect();
    println!("unique words = {}", words.len());
    let ranged: Vec<String> = words.range(String::new().."c".to_string()).cloned().collect();
    println!("range <c = {}", ranged.join(","));

    let nums: BTreeSet<i64> = [5, 3, 9, 1, 5].into_iter().collect();
    println!("nums = {:?}", nums.iter().collect::<Vec<_>>());
    println!("subset = {} superset = {}", nums.is_subset(&[1, 3, 5, 9].into_iter().collect::<BTreeSet<i64>>()), nums.is_superset(&[1, 5].into_iter().collect::<BTreeSet<i64>>()));
}
