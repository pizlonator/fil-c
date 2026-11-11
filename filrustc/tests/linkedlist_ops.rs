use std::collections::LinkedList;

fn main() {
    let mut a: LinkedList<String> = LinkedList::new();
    a.push_back("one".to_string());
    a.push_back("two".to_string());
    a.push_front("zero".to_string());
    println!("len = {} front = {} back = {}", a.len(), a.front().unwrap(), a.back().unwrap());

    let mut b: LinkedList<String> = LinkedList::new();
    b.push_back("bx".to_string());
    b.push_front("ax".to_string());
    a.append(&mut b);
    println!("after append len(a) = {} len(b) = {}", a.len(), b.len());
    println!("a = {}", a.iter().cloned().collect::<Vec<_>>().join("|"));

    let tail: LinkedList<String> = a.split_off(2);
    println!("split a = {} tail = {}", a.iter().cloned().collect::<Vec<_>>().join(","), tail.iter().cloned().collect::<Vec<_>>().join(","));

    let mut c: LinkedList<String> = tail.clone();
    println!("tail cloned len = {} a len = {}", c.len(), a.len());

    let mut d = LinkedList::new();
    for i in 0..4 { d.push_back(Box::new(format!("item{}", i))); }
    println!("boxed list = {}", d.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));
    d.pop_front();
    d.pop_back();
    println!("after pops = {}", d.iter().map(|b| b.as_str()).collect::<Vec<_>>().join(","));
}
