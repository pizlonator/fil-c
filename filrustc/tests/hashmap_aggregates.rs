// HashMap / BTreeMap with pointer-containing keys and values: insert, get,
// remove, entry API, iteration (printed in sorted order for determinism).
use std::collections::{BTreeMap, HashMap};

fn main() {
    let mut m: HashMap<String, Vec<Box<String>>> = HashMap::new();
    for i in 0..10u32 {
        let key = format!("key{}", i % 4);
        m.entry(key).or_default().push(Box::new(format!("v{}", i)));
    }
    println!("size = {}", m.len());

    // deterministic iteration: sort keys
    let mut keys: Vec<&String> = m.keys().collect();
    keys.sort();
    for k in keys {
        let vals = &m[k];
        let strs: Vec<&str> = vals.iter().map(|b| b.as_str()).collect();
        println!("{} -> {}", k, strs.join(","));
    }

    // get / contains / remove
    println!("has key3 = {}", m.contains_key("key3"));
    let removed = m.remove("key3");
    println!("removed key3 = {}", removed.is_some());
    println!("has key3 after = {}", m.contains_key("key3"));

    // entry with occupied vs vacant
    let one = m.entry("only".to_string()).or_insert_with(|| vec![Box::new("made".to_string())]);
    one.push(Box::new("added".to_string()));
    println!("only = {}", m["only"].iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));

    // BTreeMap<String, Box<Node>>: ordered iteration of pointer values
    struct Node { name: String, boxed: Box<u64> }
    let mut bt: BTreeMap<String, Box<Node>> = BTreeMap::new();
    for i in 0..8u64 {
        bt.insert(format!("n{}", (i * 3) % 8), Box::new(Node {
            name: format!("node{}", i),
            boxed: Box::new(i * 100),
        }));
    }
    for (k, v) in &bt {
        println!("bt[{}] = {}#{}", k, v.name, v.boxed);
    }
    let bt_removed = bt.remove("n6");
    println!("bt removed n6 = {}", bt_removed.map(|n| n.name).unwrap_or_default());

    // HashMap<&str, Box<dyn std::any::Any>> with downcast
    let mut any: HashMap<&str, Box<dyn std::any::Any>> = HashMap::new();
    any.insert("s", Box::new("string payload".to_string()));
    any.insert("n", Box::new(42u32));
    any.insert("big", Box::new(vec!["l1".to_string(), "l2".to_string()]));
    let s = any.get("s").unwrap().downcast_ref::<String>().unwrap();
    let n = any.get("n").unwrap().downcast_ref::<u32>().unwrap();
    let big = any.get("big").unwrap().downcast_ref::<Vec<String>>().unwrap();
    println!("any: {} {} {}", s, n, big.join("+"));
}
