use std::collections::{BTreeMap, BTreeSet};
use std::collections::btree_map::Entry;

fn main() {
    let mut m: BTreeMap<String, Box<u64>> = BTreeMap::new();
    m.insert("k1".to_string(), Box::new(10));
    m.insert("k3".to_string(), Box::new(30));
    m.insert("k2".to_string(), Box::new(20));
    println!("first = {:?}", m.first_key_value().map(|(k, v)| (k.clone(), **v)));
    println!("last = {:?}", m.last_key_value().map(|(k, v)| (k.clone(), **v)));

    match m.entry("k2".to_string()) {
        Entry::Occupied(mut e) => { *e.get_mut() = Box::new(22); }
        Entry::Vacant(_) => panic!("expected occupied"),
    }
    match m.entry("k4".to_string()) {
        Entry::Vacant(e) => { e.insert(Box::new(40)); }
        Entry::Occupied(_) => panic!("expected vacant"),
    }
    println!("after entries = {}", m.iter().map(|(k, v)| format!("{}={}", k, v)).collect::<Vec<_>>().join(","));

    let range: Vec<(String, u64)> =
        m.range("k2".to_string().."k4".to_string()).map(|(k, v)| (k.clone(), **v)).collect();
    println!("range = {}", range.iter().map(|(k, v)| format!("{}={}", k, v)).collect::<Vec<_>>().join(","));

    let (k, v) = m.get_key_value("k3").unwrap();
    println!("get_key_value = {} {}", k, v);

    for v in m.values_mut() { **v += 1; }
    println!("values_mut = {}", m.values().map(|v| format!("{}", v)).collect::<Vec<_>>().join(","));

    let mut other: BTreeMap<String, Box<u64>> = BTreeMap::new();
    other.insert("k5".to_string(), Box::new(50));
    other.insert("k6".to_string(), Box::new(60));
    m.append(&mut other);
    println!("after append len = {} keys = {}", m.len(), m.keys().cloned().collect::<Vec<_>>().join(","));

    let right = m.split_off(&"k4".to_string());
    println!("left = {} right = {}",
        m.keys().cloned().collect::<Vec<_>>().join(","),
        right.keys().cloned().collect::<Vec<_>>().join(","));

    let pf = m.pop_first().unwrap();
    println!("pop_first = {} {}", pf.0, pf.1);

    let mut bs: BTreeSet<String> = BTreeSet::new();
    bs.insert("r1".to_string());
    bs.insert("r2".to_string());
    println!("bset = {} contains = {}", bs.iter().cloned().collect::<Vec<_>>().join(","), bs.contains("r1"));
    let taken = bs.take("r2");
    println!("taken = {:?} remaining = {}", taken.is_some(), bs.len());
}
