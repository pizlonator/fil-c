use std::collections::hash_map::DefaultHasher;
use std::collections::{BTreeMap, HashMap, HashSet};
use std::hash::{BuildHasher, BuildHasherDefault, Hash, Hasher};

#[derive(Debug, PartialEq, Eq, Hash, Clone)]
struct Key { name: String, id: u32 }

fn main() {
    // DefaultHasher determinism within a process
    let mut h1 = DefaultHasher::new();
    "hello".hash(&mut h1);
    let s1 = h1.finish();
    let mut h2 = DefaultHasher::new();
    "hello".hash(&mut h2);
    let s2 = h2.finish();
    println!("hash deterministic = {}", s1 == s2);
    let mut h3 = DefaultHasher::new();
    "world".hash(&mut h3);
    println!("different strings hash differently = {}", h3.finish() != s1);

    // hashing composite values
    let k = Key { name: "key".to_string(), id: 7 };
    let mut h4 = DefaultHasher::new();
    k.hash(&mut h4);
    let mut h5 = DefaultHasher::new();
    Key { name: "key".to_string(), id: 7 }.hash(&mut h5);
    println!("struct hash stable = {}", h4.finish() == h5.finish());

    // HashMap with custom Hash type
    let mut map: HashMap<Key, Vec<String>> = HashMap::new();
    map.insert(Key { name: "a".to_string(), id: 1 }, vec!["one".to_string()]);
    map.insert(Key { name: "a".to_string(), id: 2 }, vec!["two".to_string()]);
    println!("map len = {}", map.len());
    println!("get = {:?}", map.get(&Key { name: "a".to_string(), id: 1 }).unwrap());

    // HashSet of boxed keys
    let mut set: HashSet<Box<Key>> = HashSet::new();
    set.insert(Box::new(Key { name: "bx".to_string(), id: 9 }));
    set.insert(Box::new(Key { name: "bx".to_string(), id: 9 }));
    println!("set dedup = {}", set.len() == 1);

    // BuildHasherDefault with DefaultHasher
    let build = BuildHasherDefault::<DefaultHasher>::default();
    let mut map2: HashMap<String, u64, _> = HashMap::with_hasher(build);
    map2.insert("x".to_string(), 1);
    map2.insert("y".to_string(), 2);
    println!("with_hasher = {} {}", map2["x"], map2["y"]);
    let mut h = map2.hasher().build_hasher();
    "x".hash(&mut h);
    println!("build_hasher works = {}", h.finish() != 0);

    // hashing containers (BTreeMap iteration -> same hash in both orders)
    let mut m1 = BTreeMap::new();
    let mut m2 = BTreeMap::new();
    m1.insert(1, "a");
    m1.insert(2, "b");
    m2.insert(2, "b");
    m2.insert(1, "a");
    let mut hh1 = DefaultHasher::new();
    m1.iter().for_each(|(k, v)| { k.hash(&mut hh1); v.hash(&mut hh1); });
    let mut hh2 = DefaultHasher::new();
    m2.iter().for_each(|(k, v)| { k.hash(&mut hh2); v.hash(&mut hh2); });
    println!("order-independent hash = {}", hh1.finish() == hh2.finish());
}
