use std::collections::VecDeque;

fn main() {
    let mut dq: VecDeque<i32> = VecDeque::new();
    dq.push_back(1);
    dq.push_back(2);
    dq.push_front(0);
    println!("front = {} back = {}", dq.front().unwrap(), dq.back().unwrap());
    dq.insert(1, 100);
    println!("inserted/removed = {}", dq.remove(1).unwrap());
    println!("len = {}", dq.len());

    let mut names: VecDeque<String> =
        ["alpha", "beta", "gamma", "delta"].iter().map(|s| s.to_string()).collect();
    let popped = names.pop_front().unwrap();
    println!("popped = {}", popped);
    names.push_front(popped.clone());
    for s in names.iter_mut() {
        s.push('!');
    }
    println!("iter_mut = {}", names.iter().cloned().collect::<Vec<_>>().join(","));
    names.rotate_left(2);
    println!("rotated = {}", names.iter().cloned().collect::<Vec<_>>().join(","));
    let contig = names.make_contiguous();
    println!("contig = {} len = {}", contig[0], contig.len());

    let mut boxed: VecDeque<Box<String>> = VecDeque::with_capacity(8);
    boxed.push_back(Box::new("held".to_string()));
    boxed.shrink_to_fit();
    boxed.extend(vec![Box::new("bee".to_string()), Box::new("cee".to_string())]);
    println!("boxed = {} {} contains = {}", boxed[0], boxed.get(1).unwrap(), boxed.contains(&Box::new("bee".to_string())));
    let old = boxed.swap_remove_back(0).unwrap();
    println!("swap_remove = {} len = {}", old, boxed.len());
    let mut iter = boxed.into_iter();
    println!("into_iter = {} {}", iter.next().unwrap(), iter.next_back().unwrap());

    let mut nums: VecDeque<u64> = (0..5).collect();
    nums.resize(7, 99);
    println!("resized = {:?}", nums.into_iter().collect::<Vec<_>>());
}
