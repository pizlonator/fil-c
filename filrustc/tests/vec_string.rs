use std::collections::HashMap;
use std::fmt::Write;

fn main() {
    let mut v: Vec<String> = Vec::new();
    v.push("filc".to_string());
    v.push("rust".to_string());
    v.push("pizlonated".to_string());
    v.sort();
    let joined: Vec<String> = v.iter().map(|s| s.to_uppercase()).collect();
    println!("{}", joined.join("+"));

    let mut m: HashMap<String, i32> = HashMap::new();
    m.insert("one".to_string(), 1);
    m.insert("two".to_string(), 2);
    let total: i32 = m.values().sum();
    let mut out = String::new();
    write!(&mut out, "total = {} ({} entries)", total, m.len()).unwrap();
    println!("{}", out);

    let squares: Vec<i32> = (0..5).map(|i| i * i).collect();
    println!("squares = {:?}", squares);
}
