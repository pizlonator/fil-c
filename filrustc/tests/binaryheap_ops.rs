use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn main() {
    let mut heap: BinaryHeap<Box<String>> = BinaryHeap::new();
    for w in ["mid", "zed", "alpha", "beta", "gamma", "alpha"] {
        heap.push(Box::new(w.to_string()));
    }
    println!("len = {} peek = {}", heap.len(), heap.peek().unwrap());
    let mut popped: Vec<String> = Vec::new();
    while let Some(b) = heap.pop() {
        popped.push(*b);
    }
    println!("popped = {}", popped.join(","));

    let mut nums: BinaryHeap<i64> = [5, 1, 9, 3, 7].into_iter().collect();
    let sorted = nums.clone().into_sorted_vec();
    println!("into_sorted = {:?}", sorted);
    {
        let mut top = nums.peek_mut().unwrap();
        *top = 0;
    }
    let after = nums.into_sorted_vec();
    println!("after peek_mut 9->0 = {:?}", after);

    let mut rev: BinaryHeap<Reverse<String>> = BinaryHeap::new();
    for w in ["b", "d", "a", "c"] {
        rev.push(Reverse(w.to_string()));
    }
    let ascending: Vec<String> = rev.into_sorted_vec().into_iter().map(|r| r.0).collect();
    println!("min-heap = {}", ascending.join(","));

    let mut pairs: BinaryHeap<(u64, Box<String>)> = BinaryHeap::new();
    pairs.push((1, Box::new("one".to_string())));
    pairs.push((3, Box::new("three".to_string())));
    pairs.push((2, Box::new("two".to_string())));
    let drained: Vec<(u64, String)> = pairs.drain().map(|(k, b)| (k, *b)).collect();
    println!("drained = {:?}", drained);
    println!("drained empty = {}", pairs.is_empty());
}
