// BinaryHeap<Box<String>>: sift-up/down moves elements through the
// element-typed Hole scratch; pops must yield every value intact.
use std::collections::BinaryHeap;

fn main() {
    let mut heap: BinaryHeap<Box<String>> = BinaryHeap::new();
    for i in 0..10u32 {
        heap.push(Box::new(format!("h{}", (i * 7) % 10)));
    }
    println!("len = {}", heap.len());
    let mut popped: Vec<String> = Vec::new();
    while let Some(b) = heap.pop() {
        popped.push((*b).clone());
    }
    println!("popped = {}", popped.join(","));

    // heap of boxed numbers with into_sorted_vec
    let nh: BinaryHeap<Box<u32>> = (0..8u32).map(|i| Box::new((i * 5) % 9)).collect();
    let sorted = nh.into_sorted_vec();
    println!("sorted = {}", sorted.iter().map(|b| b.to_string()).collect::<Vec<_>>().join(","));

    // peek + peek_mut (mutable access into the heap's storage)
    let mut mh: BinaryHeap<Box<String>> = Vec::new().into();
    mh.push(Box::new("mutable".to_string()));
    mh.push(Box::new("aaa".to_string()));
    if let Some(mut top) = mh.peek_mut() {
        *top = Box::new("MUTATED".to_string());
    }
    println!("top = {}", mh.peek().unwrap());
}
